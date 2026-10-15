#!/usr/bin/env python3
"""Convert shipped SBX presets to native Mixxx MIDI mappings."""

import argparse
import json
import math
from pathlib import Path
import re
import shlex
import xml.etree.ElementTree as ET


def numeric(value):
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("Non-finite binding range")
    return int(result) if result.is_integer() else result


def parse_mapping(path):
    metadata, inputs, outputs = {}, [], []
    header = False
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        try:
            tokens = shlex.split(line, comments=True)
            if not tokens:
                continue
            kind = tokens[0]
            if not header:
                if tokens != ["SBX_CONTROLLER_MAPPING", "6"]:
                    raise ValueError("Expected SBX_CONTROLLER_MAPPING 6")
                header = True
                continue
            if kind in ("id", "name"):
                metadata[kind] = tokens[1]
                continue
            if kind == "device":
                if tokens[1] != "midi":
                    raise ValueError("Only MIDI presets are supported")
                metadata["device"] = tokens[4]
                continue
            if kind not in ("input", "output", "modifier", "effect-selection"):
                raise ValueError(f"Unsupported record: {kind}")
            binding = {"kind": kind, "control": tokens[2]}
            if kind == "output":
                if len(tokens) != 16 or tokens[3] != "midi" or tokens[5] not in ("scale", "threshold"):
                    raise ValueError("Unsupported output byte template")
                if tokens[9:11] != ["3", "literal"] or tokens[12] != "literal" or tokens[14] != "value7":
                    raise ValueError("Unsupported output byte expressions")
                binding.update(status=int(tokens[11], 0), number=int(tokens[13], 0),
                               minimum=numeric(tokens[6]), maximum=numeric(tokens[7]),
                               threshold=tokens[5] == "threshold")
                if tokens[4] != "none":
                    binding["modifier"] = tokens[4]
                outputs.append(binding)
                continue
            source_index = next(i for i, token in enumerate(tokens) if token.startswith("midi-"))
            source = tokens[source_index]
            channel, number = map(lambda value: int(value, 0), tokens[source_index + 1:source_index + 3])
            status_type = {"midi-note": 0x90, "midi-cc": 0xB0,
                           "midi-pitchbend": 0xE0, "midi-pitch-bend": 0xE0}[source]
            if not 0 <= channel < 16 or not 0 <= number < 128:
                raise ValueError("MIDI channel or address out of range")
            binding.update(status=status_type | channel, number=0 if status_type == 0xE0 else number)
            if "modifier" in tokens:
                binding["modifier"] = tokens[tokens.index("modifier") + 1]
            if kind == "modifier":
                if tokens[3] != "held":
                    raise ValueError("Unsupported modifier mode")
            elif kind == "effect-selection":
                binding.update(effect=tokens[3], offOnRelease="off-on-release" in tokens)
            else:
                if tokens[5] != "linear":
                    raise ValueError("Unsupported transform")
                action, mode = tokens[3:5]
                mode = {"toggle": "toggle", "increment": "relative", "trigger": "trigger"}.get(action, mode)
                if mode == "binary-offset":
                    mode = "relative"
                if action not in ("write", "toggle", "increment", "trigger", "write-literal"):
                    raise ValueError(f"Unsupported action: {action}")
                if mode not in ("absolute", "relative", "toggle", "momentary", "trigger"):
                    raise ValueError(f"Unsupported mode: {mode}")
                start = 7 if len(tokens) >= 28 else 6
                if start == 7 and not int(tokens[6]):
                    continue
                in_min, in_max, out_min, out_max = map(numeric, tokens[start:start + 4])
                if in_min == in_max:
                    raise ValueError("Zero input range")
                if mode == "relative" and start == 7:
                    out_max = out_min + (out_max - out_min) * numeric(tokens[11])
                if action == "write-literal":
                    if start != 7:
                        raise ValueError("Missing literal value")
                    mode, out_max = "trigger", numeric(tokens[19])
                binding.update(mode=mode, inMin=in_min, inMax=in_max, outMin=out_min, outMax=out_max)
                if "twos-complement" in line:
                    binding["twosComplement"] = True
            inputs.append(binding)
        except (ValueError, IndexError, KeyError, StopIteration) as error:
            raise ValueError(f"{path}:{line_number}: {error}") from error
    if not header or not all(key in metadata for key in ("id", "name", "device")):
        raise ValueError(f"{path}: Incomplete mapping identity")
    return metadata, inputs, outputs


def write_mapping(source, destination):
    metadata, inputs, outputs = parse_mapping(source)
    prefix = re.sub(r"[^A-Za-z0-9]", "", source.stem)
    script_name = source.stem + "-script.js"
    script = f"// Generated by tools/convert_mappings.py from res/mappings/{source.name}.\n"
    script += f"var {prefix} = StemboksSBX.create(\n[\n"
    script += ",\n".join("    " + json.dumps(binding, separators=(",", ":")) for binding in inputs)
    script += "\n],\n[\n"
    script += ",\n".join("    " + json.dumps(binding, separators=(",", ":")) for binding in outputs)
    (destination / script_name).write_text(script + "\n]);\n")

    root = ET.Element("MixxxControllerPreset", schemaVersion="1", mixxxVersion="2.6")
    info = ET.SubElement(root, "info")
    ET.SubElement(info, "name").text = metadata["name"]
    ET.SubElement(info, "author").text = "Stemboks"
    ET.SubElement(info, "description").text = "Stemboks controls and pad pages through the native Mixxx controller engine."
    controller = ET.SubElement(root, "controller", id=metadata["device"])
    files = ET.SubElement(controller, "scriptfiles")
    ET.SubElement(files, "file", filename="Stemboks-sbx-common.js", functionprefix="")
    ET.SubElement(files, "file", filename=script_name, functionprefix=prefix)
    controls = ET.SubElement(controller, "controls")
    addresses = set()
    for binding in inputs:
        status, number = binding["status"], binding["number"]
        addresses.add((status, number))
        if status & 0xF0 == 0x90:
            addresses.add((status - 0x10, number))
        elif status & 0xF0 == 0xE0:
            addresses.update((status, lsb) for lsb in range(128))
    for status, number in sorted(addresses):
        control = ET.SubElement(controls, "control")
        ET.SubElement(control, "group").text = "[Stemboks]"
        ET.SubElement(control, "key").text = prefix + ".input"
        ET.SubElement(control, "status").text = f"0x{status:02X}"
        ET.SubElement(control, "midino").text = f"0x{number:02X}"
        ET.SubElement(ET.SubElement(control, "options"), "Script-Binding")
    ET.SubElement(controller, "outputs")
    ET.indent(root, space="    ")
    ET.ElementTree(root).write(destination / (source.stem + ".midi.xml"), encoding="utf-8", xml_declaration=True)
    return len(inputs), len(outputs)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=root / "res/mappings")
    parser.add_argument("--destination", type=Path, default=root / "res/controllers")
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    sources = sorted(args.source.glob("*.mapping"))
    if not sources:
        parser.error("No SBX mapping files found")
    total = [0, 0]
    for source in sources:
        total = [sum(values) for values in zip(total, write_mapping(source, args.destination))]
    print(f"Converted {len(sources)} presets, {total[0]} input records and {total[1]} feedback records")


if __name__ == "__main__":
    main()
