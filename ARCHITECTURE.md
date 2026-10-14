# Stemboks — clone architecture

Goal: a clean-room reimplementation of Stembox 1.8.9's observed behavior
(see docs/rea/), same UI shape, same feature surface, same controllers,
running on macOS, Windows, Linux x86-64 and Linux ARM64.

Stembox itself is a Qt 6 Widgets app built on a Mixxx-derived "substrat":
a dotted control registry (`deck.1.transport.playing`, `mixer.crossfader.position`, …)
that UI, controllers and engine all read/write. We mirror that design:

```
            ┌──────────── ControlRegistry (dotted name -> double/string) ───────────┐
UI widgets ─┤                                                                        ├─ AudioEngine (RT)
Controllers─┤  deck.{1..4}.*  mixer.*  fx.unit.{1,2}.*  sampler.*  library.*  app.*  ├─ Recorder
Keyboard  ──┘                                                                        └─ Analysis/Stems workers
```

## Choices

| Area | Original | Stemboks |
|---|---|---|
| UI toolkit | Qt 6 Widgets + QSS | Qt 6 Widgets + QSS (same object names/colors) |
| Decode/encode | FFmpeg 7/8 | FFmpeg (avformat/avcodec/swresample) |
| Audio out | Mixxx SoundManager (CoreAudio/ASIO/ALSA…) | miniaudio (CoreAudio/WASAPI/ALSA/PulseAudio), main + headphone devices |
| Time-stretch | SoundTouch + Signalsmith | built-in WSOLA (keylock + key shift) |
| Library DB | SQLite (QtSql) | SQLite (QtSql), same concepts: tracks/playlists/history |
| Settings | usersettings.json (`groupes`) | same JSON shape, dir `stemboks` |
| Stems | HTDemucs ONNX (downloaded model) | ONNX Runtime if found at build time + user model path; playback of pre-separated stems always works |
| Controllers | MIDI + HID, `.mapping` v6 text files | RtMidi + hidapi, same `.mapping` v6 parser, mappings for the 7 Pioneer DDJs |
| Waveform cache | `cache/ondes/*.onde` | same layout under our data dir |

## Layout

- `src/control/` — registry + settings
- `src/engine/` — decoder, deck player, mixer/FX DSP, analysis (BPM/key), recorder
- `src/library/` — SQLite library, scanning, playlists, history, tag list
- `src/stems/` — stem discovery/playback model, separation queue
- `src/controllers/` — .mapping parser, MIDI/HID backends
- `src/ui/` — main window (Large/Split), CDJ deck bars, waveforms, library
  screens (SOURCE/browse/search/tag list), preferences, stems manager, second screen
- `res/` — QSS theme, icons, fonts, mappings, drum samples note (generated at build? no: small synthesized samples committed)
- `.github/workflows/build.yml` — macOS / Windows / Linux / Linux-ARM64 builds

Audio model: tracks are decoded fully to float32 stereo RAM buffers
(resampled to engine rate). Each deck renders with varispeed + optional WSOLA
keylock; stems are 4 synchronized buffers with per-stem gain/mute. The mixer
sums decks through 3-band EQ (ISO/EQ curves), Color FX filter, crossfader
(Constant Power/Additive × Linear/Medium/Scratch), 8-band master EQ, limiter,
and splits to main + headphone (PFL) buses. Recording taps the main bus.
