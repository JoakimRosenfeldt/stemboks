# Stemboks code review (Opus)

Scope: `src/engine/*.cpp`, `src/controllers/ControllerManager.cpp`, `src/stems/StemManager.cpp`, `src/library/Library.cpp`, `src/control/*`, skim of `src/ui/*.cpp`, `CMakeLists.txt`, `.github/workflows/build.yml`, `res/mappings/*.mapping`. Static review only. Nothing was built or run. The local `build/CMakeCache.txt` shows `ONNX_LIB-NOTFOUND`, and no CI job installs ONNX Runtime, so the `STEMBOKS_HAVE_ONNX` code has never been compiled.

Findings are ranked by how much they break real use. Line numbers refer to the current working tree.

---

## Critical

### C1. Shipped controller mappings write controls the engine never reads
- `res/mappings/*.mapping` vs `src/engine/Deck.cpp:19-28`, `src/engine/AudioEngine.cpp:107-126`
- The mappings bind these names, and nothing in `src/` handles any of them:
  - `deck.N.cue.button`
  - `deck.N.pads.cell.K.press` (192 bindings)
  - `deck.N.scratch.wheelMove` / `wheelTouch`
  - `deck.N.tempo.keyLock`
  - `deck.N.loop.reloop` / `activeLoop` / `fourBeatExit`
  - `deck.N.jump.fourForward` / `fourBack`
  - `deck.N.colorFx.amount` (the engine uses lowercase `colorfx`)
  - `sampler.K.transport.trigger`
  - `fx.unit.U.slot.S.*`
  - `fx.unit.U.assign.deck.N` (the engine reads `deck.N.fx.unit1`)
  - `library.scroll` / `activate` / `loadToDeck.N`
  - `mixer.masterCue`
- What works from hardware: play, sync, PFL, volume, gain, EQ, tempo fader, quantize, loop in/out, jump, crossfader and headphone mix. CUE, hot cues, jog wheels, key lock, auto loop, samplers, beat FX, the color FX knob and library browsing do nothing.
- Fix: pick one vocabulary. Either rename the mapping controls to the `Deck.cpp` names (`transport.cue`, `jog.move`, `jog.touch`, `key.lock`, `loop.active`, `colorfx.amount`, `hotcue.K.activate`, `sampler.K.play`), or add an alias table in `ControllerManager::applyBinding` that rewrites names before `registry.set`. Pads need a small dispatcher that follows the current pad mode, like `DeckWidget::triggerPad`. Add a startup check that warns about any mapping control no component consumes.

---

## High

### H1. Key shift changes tempo, not pitch
- `src/engine/Deck.cpp:367-369`, `:418-426`
- `rate` is multiplied by `2^(shift/12)`, and with `stretch = KeyLock || KeyShift != 0` that rate goes to the WSOLA stretcher, which holds pitch constant. The result is that `key.shift = +1` plays about 6% faster at the same pitch. The displayed BPM (`put(s, Bpm, bpmOf*tempo)`, line 348) leaves out the factor, so sync and the BPM readout are also wrong.
- Fix: keep `rate = tempo * sourceRate * (1 + bend)` for transport. To shift pitch by `p = 2^(n/12)`, run the stretcher at `rate / p` to produce `count * p` intermediate frames, then linearly resample those down to `count` output frames. Advance `position` by `rate * count` only. Keep a scratch buffer sized `blockFrames * 2 * maxP`, allocated up front.

### H2. The audio callback takes contended locks and does allocation and disk I/O
- Locks on the render path:
  - `AudioEngine.cpp:268`: `renderMix` holds `std::recursive_mutex`.
  - `Deck.cpp:343`: each deck render holds another `std::recursive_mutex`.
  - `Deck.cpp:344`: `track()` takes a `QMutex`.
- The same mutexes are held on the UI and MIDI thread by `Deck::onControlChanged` (all 4 decks lock on every `registry.set` of any control, `Deck.cpp:276-278`), by `loadTrack` (which emits signals while locked), by the color FX change handler (runs `process(512)` while locked, `Deck.cpp:136-142`), and by `AudioEngine::setRecording`, which opens the file and writes the header under the engine mutex (`AudioEngine.cpp:256-260`).
- `ControlRegistry::get` and `setSilent` (`ControlRegistry.cpp:9-26`) take a `QMutex` and hash a `QString` on every call. That adds up to roughly 200 calls per block across 4 decks plus the mixer. `setSilent` can insert into the `QHash`, which allocates and rehashes.
- `AudioEngine.cpp:352`: `m_recorder->write` runs FFmpeg encoding plus `av_interleaved_write_frame` (file I/O) inside the callback. `av_audio_fifo_write` (`Decoder.cpp:206`) can also reallocate.
- Effect: dropouts at the 256 and 512 frame buffers the app offers (`docs/rea/runtime-evidence.md:46`), and worse while recording or moving UI sliders.
- Fix:
  - Make audio-thread parameters `std::atomic<double>` fields cached per deck and mixer. The registry `changed` handler copies values into them, and the audio thread never touches `ControlRegistry`.
  - Publish out-values (position, level, BPM) through atomics that the UI timer polls.
  - Send commands (seek, play, cue, hot cue, loop) through a lock-free SPSC queue, or at minimum `try_lock` in `render` and output the previous state or silence on contention.
  - Move the recorder to a worker thread fed by a lock-free ring buffer of float frames.

### H3. Echo-family beat FX cancel the dry signal
- `src/engine/Effects.cpp:102-113`, `:184`; `src/engine/AudioEngine.cpp:295-298`
- For Echo, Lowcut, Multitap, Spiral and Dub, `wet` is only the delayed signal, so the slot outputs `(1-mix)*x + mix*delayed`. The mixer then adds `fx - dry` to the main bus, which is `mix * send * (delayed - x)`. With `mix = 1`, which `DeckWidget.cpp:134` sets on every pad FX press, and `send = 1`, the deck's dry signal is subtracted completely, so only the echo tail is heard. The same applies to the deck color FX `decho`.
- Fix: for the delay family, set `wet = x + delayed` (or `x + mix*delayed` and skip the dry crossfade), so the mix knob controls echo level and never removes the dry signal.

### H4. Non-ASCII file paths fail to open on Windows
- `src/engine/Decoder.cpp:34`, `:226`, `:264`
- `QFile::encodeName` is `toLocal8Bit()` in Qt 6. On Windows that is the ANSI code page. FFmpeg's file protocol on `_WIN32` expects UTF-8 and converts it to UTF-16. Any path with `é`, `ø`, CJK and so on fails to decode, read tags or record.
- Fix: pass `path.toUtf8()` to `avformat_open_input`, `avformat_alloc_output_context2` and `avio_open` on all platforms. FFmpeg expects UTF-8 on macOS and Linux too.

### H5. Headphone cue is unusable on common setups
- `src/engine/AudioEngine.cpp:178-200`, `:340-347`, `:354-386`; `src/ui/PrefsWidget.cpp:102-104`
- Three problems:
  - **Channel settings are ignored.** Devices always open with 2 channels. Most DJ controller interfaces (DDJ-FLX4/FLX6/400/1000) are one 4-channel device: master on 1-2, phones on 3-4. `sound_*_channels` is never read, so PFL cannot reach the phones.
  - **PFL is thrown away without a second device.** When there is no separate headphone device, `headOut` is computed and discarded.
  - **The two-device ring buffer has no drift handling.** Two devices on different clocks either underrun (clicks) or fill the 32768-frame queue. Once full, new audio is dropped (`available` clamps the writer) and the old audio stays, so headphone latency climbs to about 740 ms and stays there.
- Fix:
  - Open the main device with `channels = max(mainHighestChannel, phonesHighestChannel)` and write master and phones into the chosen channel pairs.
  - For separate devices, keep the queue near a target of about 2 periods: when fill goes over the target, advance `headRead` (drop the oldest frames), and when it underruns, output silence. A small resampler can come later.

### H6. Folder scan can use gigabytes of RAM for cover art
- `src/ui/LibraryWidget.cpp:77-82`, `src/engine/Decoder.cpp:144-150`
- The scan worker calls `Decoder::readTags` for every file and keeps every `TrackInfo` in one `QVector` until the scan ends. `readTags` decodes embedded artwork into a `QImage` (a 1000x1000 cover is 4 MB as RGB32). A 5,000-track folder can need over 10 GB.
- Fix: add a `bool withArtwork` parameter to `readTags`, or clear `info.artwork` before `tracks.append(info)`. Load artwork only when a track goes onto a deck.

### H7. The ONNX separation path does not match Demucs and has never been compiled
- `src/stems/StemManager.cpp:105-240`
- Problems:
  - Demucs `spectro()` uses `torch.stft(..., normalized=True)` and drops the Nyquist bin. Its input is therefore `[.., 2048, T]` scaled by `1/sqrt(4096)`. This code builds 2049 bins with no normalization.
  - Demucs pads by `3*hop/2` and keeps frames `[2 : 2+le]`. Frame alignment here is different.
  - Hybrid (`hdemucs`/`htdemucs`) exports normally take two inputs (waveform and spectrogram) and return two outputs (spectral and temporal, which are summed). The code requires exactly one input (line 189), so a standard export is rejected with "Model mismatch". A model that did accept 2049 bins would get out-of-range input and produce garbage.
  - `docs/rea/binary-features.md:54` records the original's parts as `drums`, `inst`, `vocals`. The real `stems-hdemucs-mmi-v1.onnx` may not be 4-stem at all.
- Fix:
  - Inspect the actual model's inputs and outputs (`onnxruntime` Python `get_inputs()`/`get_outputs()`) before writing the tensor code.
  - Match Demucs `_spec`/`_ispec` exactly (2048 bins, `normalized=True`, `3*hop/2` reflect pad, frame crop), feed both inputs if the model has them, and sum `istft(spec_out) + time_out`.
  - Add a CI job (Linux x86_64 is enough) that installs `onnxruntime` so this code at least compiles.

---

## Medium

### M1. CUE during playback keeps playing
- `src/engine/Deck.cpp:235`
- `if (m_playing) { seekSec(t->cueSec); s.cuePreview = false; }` leaves `m_playing` true. On CDJ/DDJ hardware, CUE during playback returns to the cue point and pauses.
- Fix: call `pause()` after the seek. Leave `cuePreview` false so releasing CUE does nothing.

### M2. The master "limiter" saturates all audio
- `src/engine/AudioEngine.cpp:345`
- With `app.masterLimiter = 1` (the default, `MainWindow.cpp:189`), every sample goes through `tanh`, even far below full scale. `tanh(0.8) = 0.66`. That is constant level loss and harmonic distortion on normal program material.
- Fix: the gain computer already keeps peaks at or below 1, so drop the `tanh`, or apply a soft knee only above about 0.9: `y = |x| < 0.9 ? x : sign(x)*(0.9 + 0.1*tanh((|x|-0.9)/0.1))`.

### M3. One corrupt packet fails the whole track load
- `src/engine/Decoder.cpp:105-113`
- Any `avcodec_send_packet` or `receive_frame` error, including `AVERROR_INVALIDDATA` from a damaged MP3 frame, returns `fail(ret)`. `av_read_frame` errors other than EOF also fail. Plenty of real MP3s have junk frames.
- Fix: on `AVERROR_INVALIDDATA` (and `AVERROR(EAGAIN)` from send), unref the packet and continue. Fail only on a fatal read error or if no audio was decoded at all.

### M4. Key lock on stem tracks smears the stems against each other
- `src/engine/Deck.cpp:419-424`, `src/engine/TimeStretch.cpp:21-34`
- Each of the 4 stem stretchers picks its own WSOLA splice offset (up to ±128 frames, about 3 ms) from its own content. The summed stems are no longer sample-aligned, which causes flamming and comb filtering.
- Fix: compute the splice offset once (on the mix or the summed stems) and apply the same `chosen` value to every stem. For example, give `TimeStretcher` a multi-channel-set mode or expose a `process(..., forcedOffset)`.

### M5. Library scan skips `.stem.mp4` and blocks the UI
- `src/library/Library.cpp:255-257`, `src/ui/LibraryWidget.cpp:72`, `:80`
- `QFileInfo::suffix()` returns `mp4` for `x.stem.mp4`, and `mp4` is not in `supportedExtensions()`, so NI Stems files are never imported.
- After the scan, `addOrUpdateTrack` runs on the GUI thread once per file. Each call is a separate autocommit (an fsync each) plus `StemManager::stemsExistFor`, which can open containers with FFmpeg. Thousands of tracks freeze the UI for tens of seconds.
- Fix: match on `fileName().endsWith("." + ext, Qt::CaseInsensitive)`, or add `mp4`. Wrap the batch in one `m_db.transaction()`/`commit()`, and compute `stemsExistFor` in the worker.

### M6. Audio device settings never take effect
- `src/ui/PrefsWidget.cpp:100-113`, `src/engine/AudioEngine.cpp:150-207`
- Sample rate, device and buffer size are saved, but nothing calls `AudioEngine::stop()`/`start()`. Changes only apply after restarting the app.
- `Deck.cpp:144-151` re-prepares color FX at the new rate while the engine keeps running at the old one. That is harmless, but it hides the problem.
- Fix: on a change to `sound_*` settings, call `stop()` then `start()` on the GUI thread and report failure in the prefs UI.

### M7. Mappings are not found in packaged builds
- `CMakeLists.txt:127-128`, `.github/workflows/build.yml` (package steps), `src/controllers/ControllerManager.cpp:203`
- The search paths are `appDir/mappings`, `appDir/../share/stemboks/mappings`, `cwd/res/mappings` and the data dir. The macOS bundle (`Contents/MacOS`) and the Windows `dist/` get no mappings, and `cwd` is `/` when the app is launched from Finder. Only Linux `make install` works.
- Fix: copy `res/mappings` into `Contents/Resources/mappings` (add a `MACOSX_PACKAGE_LOCATION` source property) and next to the exe on Windows. Add `appDir/../Resources/mappings` to the search list. Or embed the mappings in the `.qrc` and read them from `:/mappings`.

### M8. Controller LEDs miss engine-driven state changes
- `src/controllers/ControllerManager.cpp:318-332`
- Feedback is sent only from `ControlRegistry::changed`, which only `set()` emits. The engine updates state with `setSilent`: track end (`Deck.cpp:216`), brake, hot cue presence (`:317`, `:320`), loop enabled (`:297`) and sampler end (`AudioEngine.cpp:313`). The play LED stays lit after a track ends.
- Fix: on the GUI timer, compare the current value of each bound output control with the last value sent, and send MIDI when it differs.

### M9. Jog ticks are lost to a read-modify-write race
- `src/engine/Deck.cpp:349-357`, `src/controllers/ControllerManager.cpp:373`
- The MIDI handler does `set(c, get(c) + step)` on the GUI thread. The audio thread reads `Move` at line 349 and later writes 0 at line 357. A tick added between those two points is wiped. Under fast scratching this happens often.
- Fix: make jog delta a per-deck `std::atomic<double>`. The controller adds with a CAS loop and the audio thread takes it with `exchange(0.0)`.

### M10. Auto gain can add +24 dB
- `src/engine/Analysis.cpp:90`, `src/engine/Deck.cpp:386-387`
- The target is RMS -14 dBFS, clamped to ±24 dB, and auto gain is on by default. A quiet or dynamic track (RMS -30 dBFS) gets +16 dB and clips hard into the master stage.
- Fix: also track the peak during analysis and cap the gain at `-20*log10(peak) - 1` dB, plus a sane upper bound such as +12 dB.

---

## Low

### L1. Roll and Stretch beat FX restart on every tempo change
- `src/engine/Effects.cpp:80-84`, `src/engine/AudioEngine.cpp:293`
- `loopFrames` comes from `m_bpm`, which the mixer updates every block from `effectiveBpm()` (it changes with pitch bend and the tempo fader). Any change of one frame refills `m_roll` and clears `m_rollCaptured`, so the roll re-captures mid-effect.
- Fix: latch the BPM when the effect is enabled, or recompute `loopFrames` only when the captured loop wraps.

### L2. Slip with waveform scratch jumps back after one block
- `src/engine/Deck.cpp:359-363`, `:447`
- `s.scratching` is cleared at the end of every render. Between two `scratchTo` mouse events the next block sees `slip == false` and seeks to the slip position, so dragging alternates between the drag position and the slip position.
- Fix: keep `scratching` true while `jog.touch` or a mouse drag is active, driven by press and release, and do not reset it per block.

### L3. Reloop lands at an arbitrary point in the loop
- `src/engine/Deck.cpp:402-404`
- If the playhead is far past `loopOut` when the loop is re-enabled, `fmod` wraps to a phase-dependent point instead of `loopIn`.
- Fix: when the loop turns on and `position >= loopOut`, set `position = loopIn`. Keep `fmod` only for the normal wrap, where the overshoot is less than one block.

### L4. Out-of-range float-to-int conversion in block count
- `src/engine/Deck.cpp:416`
- While braking, `step` goes toward 0 and `std::ceil(|boundary-position|/step)` can exceed `INT_MAX`. Casting that to `int` is undefined behavior. On x86 it gives `INT_MIN`, which `max(1, ...)` then clamps.
- Fix: `count = int(std::min<double>(count, std::max(1.0, std::ceil(...))))`.

### L5. Strict length check rejects real sidecar stems
- `src/stems/StemManager.cpp:303`
- All 4 stems must have exactly the same frame count. MP3 and FLAC stems from other tools usually differ by encoder padding, so they are silently ignored.
- Fix: accept a mismatch of a few thousand frames and zero-pad to the longest stem. `Deck::sample()` already bounds-checks.

### L6. Reload after separation loses playhead and loop
- `src/ui/MainWindow.cpp:64-69`
- `loadTrackToDeck` on a paused deck goes through `Deck::loadTrack`, which resets the position to 0 and clears `loopIn`/`loopOut`, which are not persisted.
- Fix: carry the position and loop over when reloading the same path, or attach the stems to the existing `LoadedTrack` under the deck lock.

### L7. Settings file is written non-atomically
- `src/control/AppSettings.cpp:39-41`
- A crash or power loss during `QFile::write` truncates `usersettings.json`.
- Fix: use `QSaveFile` plus `commit()`, as `Library::mirror` already does.

### L8. `ControllerManager.h` uses `QHash` without including it
- `src/controllers/ControllerManager.h:64`
- It compiles today only because `<QHash>` arrives through other headers. The moc TU and `PrefsWidget.cpp` include this header and depend on Qt's internal include graph, which differs between Qt minor versions and toolchains (MSYS2).
- Fix: add `#include <QHash>` to the header.

### L9. hidapi fallback may not compile or link
- `CMakeLists.txt:44-51`, `:107-111`; `src/controllers/ControllerManager.cpp:16`
- Two problems:
  - With the FetchContent build, the public include directory is `hidapi/`, so the header is `<hidapi.h>`. `<hidapi/hidapi.h>` only resolves when the system also ships headers.
  - On Linux, hidapi's own CMake enables the libusb backend by default, which needs `libusb-1.0-dev`, which the CI does not install. If neither `hidapi::hidapi` nor `hidapi` exists as a target, nothing is linked and `hid_*` symbols are unresolved.
- CI currently gets the system package, so this only bites on other machines.
- Fix: `#if __has_include(<hidapi/hidapi.h>)` with a fallback to `<hidapi.h>`. Set `HIDAPI_WITH_LIBUSB OFF` before FetchContent on Linux, and fail configuration if no hidapi target exists.

### L10. Library search treats `%` and `_` as wildcards
- `src/library/Library.cpp:147-148`
- The values are bound, so this is not SQL injection, but searching for `100%` or `_` matches everything.
- Fix: escape `\`, `%` and `_` in `search` and add `ESCAPE '\'` to each `LIKE`.

### L11. Exit waits for a running separation job
- `src/stems/StemManager.cpp:278-280`
- `aboutToQuit` calls `waitForFinished()` on a job that can run for minutes, so the app looks hung on exit.
- Fix: add a cancel flag checked between chunks in `separate()` and in `heuristic()`, set it on quit, and then wait.

---

## Checked with no defect found
- **MIDI parsing bounds** (`ControllerManager.cpp:42-57`, `:118-179`, `:248-251`): token index checks match the access patterns (`t.size() >= 28` covers `t[19]`), callback messages under 2 bytes are rejected, and HID byte and bit indices are checked against the report size.
- **SQL**: every value goes through `addBindValue`. `distinct()` concatenates only hard-coded column names.
- **FFmpeg resource handling**: `Input` and `Recorder::Impl` free every context on all paths. Decode flushes both the codec and swr. `avcodec_get_supported_config` is version-gated correctly.
- **TimeStretch buffers**: reads go through bounds-checked `read()`. `m_overlap` keeps its capacity across `clear()` in Qt 6, so `resize` on the audio thread does not allocate after warm-up.
- **Effects buffer indexing**: delay, comb, allpass and roll indices are all taken modulo their buffer sizes. The prewarm in `prepareBeatFx`/`prepareColorFx` reserves the largest delay and roll buffers.
- **Include path case**: every `#include "..."` matches the on-disk file name exactly, so case-sensitive Linux filesystems are fine.
