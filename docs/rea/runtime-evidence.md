# Stembox 1.8.9 — runtime evidence (first-party, this machine)

Target: `/Applications/Stembox.app`, CFBundleShortVersionString 1.8.9,
`com.stembox.stembox`, "(c) Mixystore", LSMinimumSystemVersion 14.0.
Binary: Mach-O arm64, 9,358,768 bytes, sha256
`d39d666b51e08ec414bb73c0c7233473af1470319e7923ca9a4c01f4c8df46be`
(REA `observe_native_ui` evidence `ev_7a7ccbf8e162d4b19351af9ee9fa0bc009d785453a36ed0c01bbe1f65e16196a`;
REA `inspect_macho` evidence `ev_d0cec695785ba15e82dd42721d77ad9eab680afd19534d1ed7b08cb94ccdaa10`).

## Stack (from bundle)

- Qt 6 Widgets (QtCore/Gui/Widgets/Network/Sql/Svg/Xml/DBus/Concurrent/Core5Compat), Mixxx skin system
  (`WidgetGroup`, `ConfigKey` `[Group],key`, `WPushButton`), Mixxx class names in binary
  (`BaseExternalLibraryFeature`, `BaseExternalPlaylistModel`); shipped QSS header says the
  skin derives from `res/skins/LateNight/style.qss`, fork `4ae413db`.
- FFmpeg libavcodec 62 / libavformat 62 / libavutil 60 / libswresample 6, libFLAC 12,
  mpg123, TagLib 2, SoundTouch 2, Signalsmith Stretch, ONNX Runtime 1.22.0, hidapi,
  SQLite3 Multiple Ciphers 2.5.1, sentry-native 0.16.8 (crashpad), monocypher, kaitai-struct.
- Stem separation model is downloaded: `https://stemboxdj.com/downloads/stems-hdemucs-mmi-v1.onnx`
  (HTDemucs). License/update checks go to a Supabase project
  (`...supabase.co/functions/v1`, `/rest/v1/settings?key=eq.software_minimums`).
  Update dialog observed live: "Stembox 1.9.0 (you have 1.8.9)".

## Screens observed live (docs/rea/shots/)

1. **Mix (Large)**: 46pt top nav `EXIT | MENU | BROWSE | TAG LIST | SEARCH` with 192pt edge
   pads; giant black waveform area with centered STEMBOX logo when empty; two CDJ-style
   deck bars at bottom (Player 1/2): artwork disc, BPM/KEY/TIME boxes, TRACK --,
   CUE (orange outline) / SYNC (blue outline), big `REMAIN/TIME 00:00.00`, TEMPO 100% `±6`
   (green), BPM readout `0.0`, MT (orange), QUANTIZE (red), red "Not Loaded.",
   COLOR FX selector (FILTER blue / REVERB 50%), 8 pads A–H (2 rows × 4).
2. **BROWSE → SOURCE**: header `SOURCE` + `?` diag button + `STEMS MANAGER` button;
   USB device rows (blue USB tile "No Device"), `STEMBOX LIBRARY` row with chevron;
   right inspector: Source/Songs/Playlists/Date/Total + `Open library` button.
3. **SEARCH**: back arrow, search field (teal outline), font-size A/A/A, table headers
   PREVIEW | TRACK ▲ | BPM ▲ | KEY ▲, zebra rows, full on-screen QWERTY keyboard
   (123 / CLEAR / SPACE / backspace key), deck bars stay visible at bottom.
4. **MENU (preferences)**, sidebar: Stembox / Controls / Pads / Mixer / Recording /
   Display / Settings / About. Selected = blue `#1a79eb`-ish fill, dark bg `#0d1017`-ish.
   - Stembox: Mix view LARGE/SPLIT, Preview style WAVEFORM/GRID, Waveform Color
     BLUE/RGB/3BAND, End of track warning 20/30/40 S, Play marker LEFT/CENTER,
     Track time display REMAIN/TIME, Theme DARK/LIGHT, Key colors MATCHING/CAMELOT,
     Hotcue pads PLAY/HOLD, Backspin LONG—SHORT slider.
   - Controls: controller status, Manage HID devices, Sound output (CoreAudio),
     Sample Rate 44100 Hz, Rescan, Main / Headphones (CUE) device+channel dropdowns,
     Audio Buffer 5.8/11.6/23.2/46.4 ms (=256/512/1024/2048 frames @44.1k).
   - Pads: tabs PAD FX / SAMPLER / DRUM STATION; 8 pad FX slots
     (ROLL 1/2, ECHO 1, VINYL BRAKE, REVERB 50%, ENIGMA JET +1/16, PHASER 1/16,
     FLANGER 1/16, TRANS 1/16), RELEASE FX badges, FX type HOLD/TOGGLE.
     Locked behind license in this install ("Not available in free trial" diagonal overlay).
   - Mixer: crossfader disable/reverse (HAMMER STYLE), invert tempo fader,
     mode CONSTANT POWER/ADDITIVE, curve LINEAR/MEDIUM/SCRATCH, EQ sensitivity slider,
     8-band master EQ (45 Hz…12 kHz), EQ curve ISO/EQ, Auto gain, Master limiter.
   - Recording: START RECORDING (red outline), directory, WAV/MP3, 16/24/32-float bits.
   - Display: force second screen, frame rate 30/60 FPS, Minimize/Full screen,
     Brightness/Contrast 100%, Preferred screen, 1 SCREEN/2 SCREENS,
     Readapt to this screen, Interface size 60/80/100/125/140%.
   - Settings: prevent loading on playing deck LOCKED/UNLOCKED, vinyl start/stop sound,
     touch waveform NAVIGATE/TOUCH CUE, search keyboard AZERTY/QWERTY,
     multi meta column in split view, delete from tag list once played, Color FX buttons
     ON/OFF vs SELECT ONLY.

## Data (~/Library/Application Support/stembox-substrat/)

- `library.db` (SQLite, 10 backup generations), `usersettings.json`
  (`format: stembox-substrat-usersettings`, groups `[Controls]`, `[Mixer]`, `[Recording]`,
  `[Stembox]`, per-controller `[StemboxSonParControleur]`),
  `mappings-actifs.conf` (active mapping per device), `stembox_license.json`,
  `cache/ondes/*.onde` (waveform cache), `cache/sonie/*.lufs` (loudness cache),
  `historique/sessions.json`, TSV sidecars `grilles-locales.tsv` (beatgrids),
  `hotcues-locaux.tsv`.

## Controllers (shipped mappings)

Pioneer DDJ-400, DDJ-800, DDJ-1000, DDJ-FLX4, DDJ-FLX6, DDJ-FLX10, DDJ-GRV6.
Format: plain text, header `SBX_CONTROLLER_MAPPING 6`, `device midi …`,
`platter-calibration`, `platter-ring`, `modifier`/`modifier-feedback`,
`effect-selection`, `input "learned-N" "<dotted.control>" write absolute linear … midi midi-cc/note …`.
Control namespace is the CH-03 registry (docs/controles/controles.csv, ~3066 controls,
4 decks expanded identically), translated from Mixxx-style `[ChannelN]` keys via
`client-control-translations.tsv`. FX vocabulary from FLX10 mapping: lowcutecho, echo,
mtdelay, spiral, reverb, trans, enigmajet, flanger, phaser, stretch, sliproll, roll,
mobiussaw, mobiustri; Color FX: space, decho, crush, pitch, noise, filter.

## Bundled extras

Drum station samples (kick/snare/clap/hat + freestyle & groove-circuit kits:
LATIN HOUSE / TECH HOUSE / UK GARAGE), mobius_saw/mobius_tri effect samples,
OpenSans fonts + 3 custom "Stembox" display fonts, skin dispositions (28 XML templates:
cdj_deck, deck_bottom, large_wf, gridedit, key, mix_large/split/active, navbar, pads…).
