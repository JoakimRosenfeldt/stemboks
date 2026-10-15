#!/usr/bin/env python3
"""Fetch the pinned CPU ONNX Runtime SDK and expose it to GitHub Actions."""

import argparse
import hashlib
import os
from pathlib import Path
import platform
import tarfile
import tempfile
import urllib.request
import zipfile

VERSION = "1.22.0"
SHA256 = {
    "linux-x64": "8344d55f93d5bc5021ce342db50f62079daf39aaafb5d311a451846228be49b3",
    "linux-aarch64": "bb76395092d150b52c7092dc6b8f2fe4d80f0f3bf0416d2f269193e347e24702",
    "osx-arm64": "cab6dcbd77e7ec775390e7b73a8939d45fec3379b017c7cb74f5b204c1a1cc07",
    "osx-x86_64": "e4ec94a7696de74fb1b12846569aa94e499958af6ffa186022cfde16c9d617f0",
    "win-x64": "174c616efc0271194488642a72f1a514e01487da4dfe84c49296d66e40ebe0da",
}


def host_platform():
    machine = platform.machine().lower()
    system = platform.system()
    if machine in ("arm64", "aarch64"):
        architecture = {"Darwin": "osx-arm64", "Linux": "linux-aarch64"}
    elif machine in ("amd64", "x86_64"):
        architecture = {"Darwin": "osx-x86_64", "Linux": "linux-x64", "Windows": "win-x64"}
    else:
        architecture = {}
    if system not in architecture:
        raise ValueError(f"Unsupported ONNX Runtime host: {system} {machine}")
    return architecture[system]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=SHA256)
    parser.add_argument("--destination", type=Path,
                        default=Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir())) / "stemboks-onnxruntime")
    args = parser.parse_args()
    target = args.platform or host_platform()
    name = f"onnxruntime-{target}-{VERSION}"
    extension = "zip" if target.startswith("win-") else "tgz"
    url = f"https://github.com/microsoft/onnxruntime/releases/download/v{VERSION}/{name}.{extension}"
    args.destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.destination) as temporary:
        archive = Path(temporary) / f"{name}.{extension}"
        with urllib.request.urlopen(url, timeout=60) as response, archive.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
        with archive.open("rb") as downloaded:
            actual = hashlib.file_digest(downloaded, "sha256").hexdigest()
        if actual != SHA256[target]:
            raise ValueError(f"ONNX Runtime checksum mismatch: expected {SHA256[target]}, got {actual}")
        if extension == "zip":
            with zipfile.ZipFile(archive) as package:
                package.extractall(args.destination)
        else:
            with tarfile.open(archive) as package:
                package.extractall(args.destination, filter="data")
    sdk = (args.destination / name).resolve()
    if not (sdk / "include/onnxruntime_cxx_api.h").is_file() or not (sdk / "LICENSE").is_file():
        raise ValueError("ONNX Runtime SDK is incomplete")
    if github_env := os.environ.get("GITHUB_ENV"):
        with open(github_env, "a", encoding="utf-8") as environment:
            environment.write(f"ONNXRUNTIME_DIR={sdk}\n")
    print(f"ONNXRUNTIME_DIR={sdk}")


if __name__ == "__main__":
    main()
