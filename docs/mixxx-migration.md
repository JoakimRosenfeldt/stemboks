# Mixxx runtime

Stemboks embeds Mixxx 2.6 at commit `527211c16adede822c262998dcb81089f81c87bf`.
The archive and Microsoft GSL dependency are pinned by SHA-256 in
`cmake/Mixxx.cmake`. This is a 2.6 development snapshot, chosen for native
four-stem playback. It is not an identified fork point of the original Stembox.

## Ownership

The existing Qt interface uses `AudioEngine` and `Deck` as adapters.
`AudioEngine` owns `mixxx::CoreServices`. Mixxx owns streaming decoding,
playback, sync, scratching, hotcues, loops, time stretching, effects, audio
routing, controller scripts, feedback, and recording. The old miniaudio
playback and WSOLA renderer are not compiled into the application.

`MixxxControlBridge` translates Stembox dotted controls into Mixxx controls.
Mixxx state flows back to the interface on the GUI thread. Audio callbacks do
not access the application registry or its mutex. Native controller mappings
operate on the same engine state.

Product types live in namespace `stemboks` because upstream also defines
`Deck`, `Library`, `ControllerManager`, and `WaveformData`.

## Usage

```cpp
AudioEngine::instance().start();
AudioEngine::instance().deck(0)->loadPath(path);
AudioEngine::instance().showPreferences(parent);
```

`Deck` exposes a metadata and waveform snapshot for the existing painter.
Mixxx's loaded Track owns audible cue and beatgrid state. The grid editor
calls `applyBeatgrid` so the displayed grid and quantization use one value.
Existing clone cues are imported when the native track has no saved cues.

The product library keeps its playlists, tag list, USB import and search.
The engine database and settings live in `mixxx-engine` below the existing
Stemboks data directory. They do not modify an installed Mixxx configuration.

## Stems

Existing NI stem containers load directly. `MixxxStemSource` prepares WAV
sidecars off the GUI thread as cached, lossless float PCM stem containers with the
NI manifest. Mixxx reads and processes the four stems in its native deck.
Cache identity includes the source and sidecars. The adapter retains the
original library identity for metadata and exported cues.

The ONNX separation queue remains an offline preparation service. It does not
render playback audio or open output devices.

## Startup and shutdown

Start MixxxApplication, isolated settings and resources, CoreServices, player
adapters and controls, audio devices, then controllers. Destroy the window,
preferences and control adapters before CoreServices, while QApplication is
still alive. CoreServices performs upstream subsystem teardown.

`cmake/mixxx-embedded.patch` allows system dependencies, accepts an existing
Microsoft GSL target, suppresses the upstream first-run library chooser for
the embedded application, exposes load errors, and permits embedded argument
configuration after QApplication creation. It also hides test friend declarations
from newer Qt moc versions and adds a second native output effect slot for
the master limiter.

## Design choice

Two designs were compared: embed CoreServices behind the current interface,
or replace the interface with a Mixxx skin. Both use the same engine and build
dependencies. Embedding preserves the current screens and avoids a second UI
rewrite. A separate review scored embedding 21/25 and the skin route 17/25.

The accepted cost is maintaining the control adapter and a small upstream
patch. Pulling selected DSP files into the old renderer was rejected because
that would retain the custom audio lifecycle. A separate Mixxx process was
rejected because it would require an additional control protocol.

Mixxx's own preferences handle sound devices, channel routing, latency and
controller mappings. Product appearance, library navigation and separation
settings remain in the existing interface.
