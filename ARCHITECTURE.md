# Architecture

Stemboks uses the Mixxx engine behind its Stembox-style Qt 6 interface.
See [the migration design](docs/mixxx-migration.md) for the pinned upstream
revision, lifecycle, integration boundaries, and alternatives considered.

| Owner | Responsibilities |
| --- | --- |
| Mixxx CoreServices | Streaming decode, playback, sync, scratch, loops, cues, beatgrids, analysis, effects, mixing, audio devices, controllers, recording |
| AudioEngine | CoreServices lifetime, native preferences, sampler loading, recording adapter |
| Deck | Track loading and metadata/waveform snapshots for the existing widgets |
| MixxxControlBridge | GUI-thread translation between dotted controls and upstream controls |
| Product library | Source browsing, playlists, search, tag list, Rekordbox USB import, compatibility sidecars |
| StemManager | Offline ONNX separation queue |
| MixxxStemSource | Lossless cached NI containers for separated WAV sidecars |
| Qt widgets | Stembox layouts, waveform painting, virtual keyboard, preferences, second screen |

The audio callback belongs to Mixxx. It does not call ControlRegistry.
The application contains no second playback or controller device backend.
