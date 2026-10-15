#!/usr/bin/env python3
"""Check mapping controls against literals and constructed engine/UI paths."""
from pathlib import Path
import re
import shlex
import sys

root = Path(__file__).resolve().parents[2]
sources = "\n".join(p.read_text() for folder in ("engine", "ui") for p in (root / "src" / folder).glob("*.cpp"))
# Discard comments so documentation cannot make an unused binding pass.
sources = re.sub(r"//[^\n]*|/\*.*?\*/", "", sources, flags=re.S)
literals = set(re.findall(r'"([^"\n]*)"', sources))


def consumed(control):
    if control in literals:
        return True
    if match := re.fullmatch(r"deck\.([1-4])\.(.+)", control):
        tail = match[2]
        if tail in literals:
            return True
        if re.fullmatch(r"eq\.(high|mid|low)", tail):
            return "eq." in literals
        if match := re.fullmatch(r"hotcue\.([1-8])\.(activate|set|clear|present)", tail):
            return "deck.%1.hotcue.%2." in literals and match[2] in literals
        if match := re.fullmatch(r"pads\.cell\.([1-8])\.press", tail):
            return "pads.cell." in literals and ".press" in literals
    if match := re.fullmatch(r"sampler\.([1-8])\.(play|gain)", control):
        return ("sampler.%1." in literals and match[2] in literals) or f"sampler.%1.{match[2]}" in literals
    if match := re.fullmatch(r"fx\.unit\.([12])\.(enabled|mix|parameter|name)", control):
        return "fx.unit.%1." in literals and match[2] in literals
    return False


unknown = []
count = 0
for path in sorted((root / "res/mappings").glob("*.mapping")):
    for line_no, line in enumerate(path.read_text().splitlines(), 1):
        tokens = shlex.split(line, comments=True)
        if not tokens or tokens[0] not in ("input", "output", "effect-selection"):
            continue
        count += 1
        if not consumed(tokens[2]):
            unknown.append(f"{path.name}:{line_no}: {tokens[2]}")
        if tokens[0] == "input" and tokens[2].endswith(".pads.mode"):
            if tokens[3:5] != ["write", "absolute"] or tokens[9] != tokens[10] or not 0 <= float(tokens[9]) <= 6 or not float(tokens[9]).is_integer():
                unknown.append(f"{path.name}:{line_no}: invalid pad-mode binding")
print("\n".join(unknown) if unknown else "Leftovers: 0 unknown controls")
print(f"Checked {count} bindings across 7 mappings")
sys.exit(bool(unknown))
