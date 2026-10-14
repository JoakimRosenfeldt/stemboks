# Stembox 1.8.9 binary findings

Inspected 2026-10-14. This installed build has Mixxx-style UI and control interfaces, but substantial Stembox-specific engine, audio, library, controller, and licensing code. Stock Mixxx capabilities must not be assumed present.

## Target and evidence

- Executable: `/Applications/Stembox.app/Contents/MacOS/Stembox`.
- SHA-256: `d39d666b51e08ec414bb73c0c7233473af1470319e7923ca9a4c01f4c8df46be`.
- Observed: ARM64 Mach-O, UUID `1165F982-1E57-3C8E-9A39-AE5141967B9C`, minimum macOS 14.0, SDK 26.5. `Info.plist` declares version/build `1.8.9`, identifier `com.stembox.stembox`, copyright Mixystore.
- REA 6.4.0 `inspect-macho` succeeded. Evidence ID: `ev_d0cec695785ba15e82dd42721d77ad9eab680afd19534d1ed7b08cb94ccdaa10`.
- Corroboration: `file`, `shasum`, `otool -L`, `otool -tvV`, `nm -C`, ASCII strings, printable UTF-16LE runs with file offsets, and shipped resources. Addresses below are unslid virtual addresses.
- REA deep-analysis limit: `inspect`, `analyze`, and procedure `search` were attempted with Hopper. The default launcher path was wrong; the installed launcher is under `~/Applications/Hopper Disassembler.app`. After correcting it and choosing the demo, background analysis ran beyond REA's 120-second startup deadline. The last attempt was stopped during dataflow analysis, too late to recover a REA session. A `--no-analysis` retry conflicted with REA's forced `--analysis`. No successful REA pseudocode, search result, or cross-reference dossier was obtained. Findings below identify their actual evidence instead.
- No playback, stem inference, recording, activation, or endpoint request was exercised. A process-list read saw an already-running Stembox and its Crashpad handler; this investigation did not launch Stembox.

## Linked libraries and versions

Observed direct Mach-O loads, corroborated by REA and `otool -L`:

| Dependency | Recorded version / detail |
| --- | --- |
| Qt Core, Gui, Widgets, Svg, Xml, Concurrent, Sql, Network, Core5Compat | All 6.8.3 |
| ONNX Runtime | `libonnxruntime.1.22.0.dylib`, 1.22.0 |
| FFmpeg | avformat 62.12.100, avcodec 62.28.100, avutil 60.26.100, swresample 6.3.100; bundled avutil contains `FFmpeg version 8.1` |
| SoundTouch | Load-command current version 1.8.9, compatibility 2.0.0; bundled library string and licence index say 2.4.1. These are different version fields, not evidence that the implementation is 1.8.9. |
| mpg123 | Load-command version 0.0.0; bundled library string and licence index say 1.32.10 |
| FLAC | `libFLAC.12.dylib`, load-command version 12.1.0; licence index says release 1.4.3 |
| TagLib | `libtag.2.dylib`, 2.3.1 |
| hidapi | `libhidapi.0.dylib`, 0.15.0 |
| System libraries | curl ABI current 9.0.0, zlib 1.2.12, bsm 1.0.0, iconv 7.0.0, libc++ 2100.43.0, libSystem 1356.0.0, libobjc 228.0.0. These load-command values are not necessarily package release numbers. |

System framework loads include CoreAudio, AudioToolbox, CoreMIDI, ApplicationServices, Foundation, Security, CoreFoundation, AppKit, ImageIO, Metal, OpenGL, AGL, IOKit, DiskArbitration, UniformTypeIdentifiers, CoreGraphics, and CoreText. Presence establishes linkage, not active use of every framework.

`Contents/Resources/licences/index.txt` also declares static Monocypher 4.0.2, SQLite3 Multiple Ciphers 2.5.1, Kaitai runtime 0.11, Signalsmith Stretch `a670068d`, Signalsmith Linear `146b26f9`, and Sentry Native 0.16.8 with Crashpad. Symbols corroborate crypto, SQLite, Kaitai, Signalsmith, Sentry, and Crashpad code; exact source revisions come from the shipped index. Binary strings independently identify SQLite 3.53.4 and Sentry 0.16.8. The cross-platform index lists ASIO SDK 2.3.4, but that does not establish an ASIO backend in this macOS executable.

Bundled FFmpeg configuration names a macOS ARM64 build and contains `--enable-version3 --disable-gpl --disable-nonfree`. The licence README also discusses a Windows `n8.1.2` build; that Windows declaration is not a macOS patch-version fingerprint.

## Mixxx heritage and upstream identity

Observed symbols include `ControlObject`, `ControlProxy`, `SkinContext`, `WLibrarySidebar`, `WWidgetStack`, `BaseExternalPlaylistModel`, `ControllerScriptEngineLegacy`, `EffectManifest`, and `EngineEffectParameter`. Shipped XML uses `skins:LateNight`, `[Channel1]` through `[Channel4]`, and Mixxx-style widget/template and effect-rack names. For example, the translation TSV maps `[EffectRack1_EffectUnit1_Effect1], enabled` onto `fx.unit.1.slot.1.enabled`.

These names and resource conventions strongly support Mixxx heritage. Official upstream examples include [WLibrarySidebar in the 2.5 branch](https://raw.githubusercontent.com/mixxxdj/mixxx/2.5/src/widget/wlibrarysidebar.h) and [EffectsManager in the 2.6 branch](https://raw.githubusercontent.com/mixxxdj/mixxx/2.6/src/effects/effectsmanager.h).

Inference: a Mixxx 2.x-derived interface or compatibility layer is the safest lineage claim. A 2.6-era base is a plausible candidate because upstream introduced stem playback there, as documented in [Mixxx's stem-mixing announcement](https://mixxx.org/news/2024-08-26-stem-mixing/). This is weak dating evidence: Stembox's stem and engine implementations have their own namespaces, so custom additions or backports can explain the same capabilities.

Unknown: exact upstream release, branch, and commit. Searches of executable ASCII/UTF-16 strings, symbols, and readable bundled resources found no explicit Mixxx version or commit ID. The classes span multiple upstream releases. Qt 6 and ONNX alone cannot identify a fork point. The long SQLite source ID is a dependency identifier, not a Mixxx commit.

## Feature inventory

“Observed” below means code, strings, or shipped configuration exists. It does not mean runtime behavior was tested.

| Area | Evidence and assessment |
| --- | --- |
| Decks | Observed four channel groups, `app.deckCount`, a 2/4-deck toggle, and four-deck XML. Split view pairs decks 1/3 and 2/4. `sbx::engine::Deck` exports play/cue, hotcue, loop, slip, quantize, key-lock/shift, tempo, EQ/filter, reverse, sync, and scratch operations. |
| Stems / ONNX | Observed `stembox::stems::SeparationJob::run` at `0x10023d6b0`, Queue and ModelInstaller classes, ONNX linkage, model-download/checksum strings, and AAC/MP4 packaging errors. Parts are `drums`, `inst`, and `vocals`; writer accepts at most four stems plus a premix. `StemMixProvider` and Deck methods control mute, volume, meters, and FX routing. Strings describe pausing separation while decks play, resuming when stopped, trial refusal, removable-volume destinations, and appliance queue import via `STEMBOX QUEUE.json`. Inference: the named hdemucs model supplies local separation; actual tensor layout and model execution were not checked. |
| FX units | Observed Beat FX on unit 1 and Color FX on unit 2 in binary strings and the translation TSV. The command catalogue declares four units and four slots, but explicitly says Multi Beat FX reads only units 1/2 and slots 1/2/3. Thus a catalogue count of four is not proof of four active stock Mixxx racks. Two pad-FX pages supply 16 pads. `creerEffectsManagerMinimal` at `0x1002f69bc` calls effect manifests in disassembly. |
| FX algorithms | Exported manifests include Echo, LowCutEcho, DubEcho, Reverb, Space, Phaser, Flanger, Trans, Spiral, Stretch, MtDelai, Mobius/MobiusSaw/MobiusTri, EnigmaJet, Filter, PitchShift, Roll, SlipRoll, VinylBrake, Backspin, BitCrusher, and WhiteNoise. Preset/selection IDs and per-part routing appear in shipped controls. |
| Recording | Observed `RecordingWriter::start` at `0x10025ec94`, writer loop, bus accumulator, splitting methods, and DlgPrefRecord. UI offers 16-bit, 24-bit, and float32 recording. A literal message says MP3 recording is unavailable and falls back to WAV16. Inference: WAV is the implemented mix-recording path; MP3 preference labels do not prove an MP3 encoder path. |
| Broadcast | Only `broadcast.`, `app.broadcast.*`, and `record_broadcast` declarations found. No matching broadcaster, Icecast/Shoutcast/libshout implementation symbol or endpoint was found in the bounded symbol/string search. Unproven; do not count this as working broadcast support. |
| AutoDJ | Only `autodj.` and `app.autodj.` setting prefixes found in executable strings. No AutoDJ implementation symbol was found. Unproven. |
| Vinyl | Observed software jog scratch, vinyl start/stop, `VinylResampler`, Backspin and VinylBrake effects. `vinyl.` settings and a `timecode` audio-role string also exist. No timecode decoder/VinylControl implementation was found. The audio path says the product opens outputs only. Software vinyl behavior is supported by code; external DVS is unproven. |
| Second screen | Observed `FenetrePrincipale::poserSurDeuxEcrans` at `0x100115bf0`, `chooseTwoScreens`, and UI text for one deck per screen with screen swapping. Inference: independent CDJ-style deck windows are implemented. |
| License check | Observed LicenseGate resolve/activate/deactivate, stored JSON, device identity, signed tokens, offline fallback, seven-day trial and seat-limit messages. `verifyToken` at `0x10024ac90` calls `crypto_ed25519_check` at `0x10024ae88`. `callFn` constructs a Supabase function URL and posts JSON. No bypass or activation was attempted. |
| Sentry | Observed statically linked Sentry 0.16.8, Crashpad symbols/helper, DSN, `send_crash_reports`, consent-related messages, and diagnostic-report code. The existing Crashpad process had the Sentry minidump URL and `stembox.log` attachment in its arguments. This proves handler configuration, not upload success or current consent state. |
| Library / controllers | Observed Stembox library/database/schema, session history, tempo/key/gain/waveform/silence analyzers, rekordbox USB readers and export writers, MIDI/HID mapping and learn code, sampler/drum machine, and FLX10 jog-screen output code. Export writers include `ecrireExportPdb`, `ecrireExportExtPdb`, `ecrireOneLibrary`, and Pioneer analysis/waveform serialization. |

## Audio APIs and formats

Observed macOS backend is `sbx::audio::CoreAudioBackend::open` at `0x10063f614`, with CoreAudioEnumerator and imports for AudioObject properties, AudioComponent creation, AudioUnit setup, and AudioOutputUnit start/stop. CoreMIDI input/output imports and hidapi support controller I/O. Product strings explicitly say it has no audio inputs and only opens outputs. Main and headphones devices are selectable.

No PortAudio, RtAudio, JACK, ALSA, or WASAPI implementation name was found in the main executable's symbols/strings. ASIO and other platform labels appear, including a message that ASIO is absent when not built for Windows. They are not evidence of usable macOS APIs. SoundTouch and Signalsmith code support time stretching; `SBX_ETIREUR`/`soundtouch` strings suggest backend selection, whose default was not established.

- Observed decoder paths: built-in WAV/AIFF/AIFC parsing, `openMp3` via mpg123, `openFlac` via libFLAC, and FFmpeg `openAudioContainer`/`openStemStream`.
- Observed library extension list: `.aif`, `.aiff`, `.flac`, `.m4a`, `.mp3`, `.ogg`, `.opus`, `.wav`, `.wave`; stem lookup also uses `.stem.mp4` and `.stem.m4a`. Playlist suffixes include `.m3u`, `.m3u8`, `.pls`.
- Important limit: an executable error explicitly says it recognizes Ogg but no Ogg decoder is linked in this version. Extension recognition does not prove Ogg/Opus playback. FFmpeg's presence does not prove every installed codec is reachable through the normal track loader.
- Inference: MP3, FLAC, supported PCM/float WAV and AIFF, and supported FFmpeg audio/stem containers are intended playback formats. Exact AAC/M4A variants and codec reachability remain untested. Unsupported AIFC compression has an explicit rejection message. Sampler file-picker text restricts selection to MP3/WAV.

## Network endpoints and URLs

Observed literals unless marked inferred. URLs were not contacted. `SB` below means `https://eqilsyogxtjcauubedcc.supabase.co`.

| URL / route | Evidence / likely role |
| --- | --- |
| `SB/functions/v1` | License API base. At `0x10024a09c`, callFn loads this base, appends `/` and its function argument, then invokes QNetworkAccessManager::post at `0x10024a278`. |
| `SB/functions/v1/license-info`, `/activate`, `/deactivate` | Inferred complete routes from the observed URL constructor and UTF-16 function-name literals at file offsets `0x751448`, `0x75152a`, `0x751588`. |
| `SB/functions/v1/app-report` | Literal diagnostic-report endpoint. |
| `SB/rest/v1/settings?key=eq.software_minimums&select=value` | Literal minimum-version/update policy query. |
| `SB/rest/v1/software_versions?select=version,url,notes&platform=eq.%1&published=eq.true&order=released_at.desc&limit=1` | UTF-16 literal at file offset `0x751854`; update discovery with a platform placeholder. |
| `https://stemboxdj.com/downloads/stems-hdemucs-mmi-v1.onnx` | Literal model-download URL. Expected SHA-256 `052892804c5b97f792904b4019615628ff8dfc0dd38a28ca14c4aac5af621af6`; UI estimates 480 MB. |
| `https://stemboxdj.com`, `/software`, `/mapping`, `/account#licenses` | Product, purchase/download, controller-mapping, and activation-management links. |
| Sentry | DSN host `o4511790337359872.ingest.de.sentry.io`, project `4511790354071632`, public DSN key `65390c919420798f2a8021c298f90f3f`. Existing handler arguments resolve `/api/4511790354071632/minidump/`. |

License calls set an `apikey` header using an embedded Supabase publishable key. License strings include token storage, offline expiry, device identity, clock high-water state, disabled licenses, trial-device reuse and seat limits. Exact server policy and grace durations were not recovered. Generic SVG namespace URLs and third-party licence URLs were excluded from application endpoints.

## Stembox-specific versus stock Mixxx

Observed Stembox-specific code includes `sbx::engine`, `sbx::audio`, `sbx::library`, `sbx::control`, `sbx::substrat`, custom French-named window/control adapters, WStembox widgets, minimal effects-manager construction, ONNX model installation/separation/queueing, appliance stem exchange, rekordbox export, license gating, update checking, diagnostic-report delivery, and Sentry setup.

Inference: this is substantially more than a branded Mixxx skin. It retains Mixxx-compatible interface conventions while shipping replacement subsystems. No stock Mixxx EngineMaster, EngineBuffer, SoundSourceProxy, AutoDJProcessor, or VinylControl implementation name was found in the searched symbols. Name absence cannot prove complete code removal, and no source-level or function-byte comparison was performed.

All requested areas were inspected. Exact upstream commit, untested runtime behavior, catalogue-only AutoDJ/broadcast/DVS, complete file-format reachability, active FX-unit counts beyond documented wiring, and server-side license policy remain unresolved.

## Reproduction

```sh
BIN=/Applications/Stembox.app/Contents/MacOS/Stembox
rea inspect-macho "$BIN" --format json
file "$BIN"
shasum -a 256 "$BIN"
otool -L "$BIN"
nm -C "$BIN"
strings -a "$BIN"
otool -tvV "$BIN"
```

Read `Contents/Info.plist`, `Contents/Resources/licences/index.txt`, `Contents/Resources/licences/LISEZ-MOI.md`, `Contents/Resources/docs/controles/controles.csv`, `Contents/Resources/ressources/traduction/client-control-translations.tsv`, and `Contents/Resources/src/interface-stembox/dispositions/*.xml` alongside executable evidence. Temporary raw outputs for this pass are under `/tmp/stembox-rea-20261014`; they are not committed artifacts.
