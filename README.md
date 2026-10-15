# Stemboks

A Stembox-style stem DJ player with a native Qt 6 interface and the Mixxx
engine. Playback, sync, time stretching, effects, audio routing, recording,
and controllers run through upstream Mixxx.

Mixxx is pinned to 2.6 commit `527211c16adede822c262998dcb81089f81c87bf`.
This development snapshot includes native four-stem playback. The original
Stembox's exact Mixxx ancestry has not been identified. See
[the integration design](docs/mixxx-migration.md) and
[reverse-engineering evidence](docs/rea/).

## Build

CMake fetches the complete Mixxx source and Microsoft GSL from archives with
pinned SHA-256 hashes. The first build is larger than the previous custom
engine. Use CMake 3.24 or later and a C++20 compiler.

On macOS with Homebrew:

```sh
brew install cmake ninja qtbase qtdeclarative qtsvg qt5compat qtshadertools \
  qttranslations ffmpeg hidapi libusb portaudio portmidi chromaprint \
  libebur128 fftw sound-touch taglib rubberband libsndfile protobuf googletest
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --target stemboks -j3
open build/stemboks.app
```

Use a fresh build directory when upgrading from the previous implementation.
Optional ONNX Runtime enables in-app stem separation.
To use the same pinned CPU runtime as CI, run
`python3 tools/fetch_onnxruntime.py` with Python 3.12 or later and pass the
printed directory as `-DONNXRUNTIME_DIR=...` when configuring. This also
bundles the runtime and its licenses.

On Ubuntu 24.04, x86-64 or ARM64:

```sh
sudo apt install cmake ninja-build pkg-config qt6-base-dev qt6-base-private-dev \
  qt6-declarative-dev qt6-shadertools-dev qt6-5compat-dev qt6-svg-dev \
  libqt6opengl6-dev libqt6sql6-sqlite libavformat-dev libavcodec-dev \
  libavutil-dev libswresample-dev libchromaprint-dev libebur128-dev \
  libfftw3-dev libflac-dev libgtest-dev libhidapi-dev libmp3lame-dev \
  libportmidi-dev libprotobuf-dev protobuf-compiler librubberband-dev \
  libsndfile1-dev libsoundtouch-dev libsqlite3-dev libtag1-dev \
  libudev-dev libusb-1.0-0-dev libgl1-mesa-dev portaudio19-dev
cmake -S . -B build -G Ninja
cmake --build build --target stemboks -j3
./build/stemboks
```

Windows uses MSVC 2022 or later and Mixxx's prebuilt release dependencies.
The Windows job in [.github/workflows/build.yml](.github/workflows/build.yml)
contains the complete configure and package procedure. CI builds macOS,
Windows x64, Linux x86-64, and Linux ARM64.
The CI packages include ONNX Runtime 1.22. The macOS package requires macOS
13.3 or later; the Windows package requires Windows 10 build 19041 or later.
Linux packages use Ubuntu 24.04 system libraries.

## Use

Open **MENU → Controls → Configure sound and controllers** to select audio
outputs, headphone channels, latency, and controller mappings in Mixxx's
native preferences. The packaged resources include Mixxx's mappings and
Stemboks compatibility presets for the seven Pioneer DDJ models.

The existing interface retains four decks, hotcues, beat loops, beatjump,
slip, quantize, sync, keylock, key shift, stem pads, sampler, library search,
USB browsing, tag list, mix recording, and the second screen.

Native `.stem.mp4` and `.stem.m4a` tracks load directly. Existing separated
sidecars remain supported as `<track>.stems/drums.wav`, `bass.wav`,
`other.wav`, and `vocals.wav`. The first load prepares a cached lossless stem
container for Mixxx. **BROWSE → STEMS MANAGER** separates new tracks when
ONNX Runtime and an HTDemucs model are available.

The product library and appearance settings remain in the existing Stemboks
data directory. Mixxx engine settings and its track database use the isolated
`mixxx-engine` subdirectory. Installed Stembox and Mixxx data are untouched.

## License

GPL-3.0-or-later. Mixxx source and notices remain in the pinned upstream
archive. Packaged applications include its resources and license. Open Sans
uses SIL OFL 1.1. Other dependencies retain their upstream licenses.
