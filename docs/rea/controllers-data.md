# Stembox 1.8.9 controllers and data

Inspected the installed macOS bundle and local data on 2026-10-14. The bundle's `CFBundleVersion` and `CFBundleShortVersionString` both report `1.8.9`.

The bundle contains seven controller mappings, all MIDI. The control registry expands to 3,257 unique paths. The local library uses SQLite schema version 11. Historical logs record an FLX10 jog-screen interface opening and closing.

This is an artifact inventory, not a hardware compatibility test. Database access was read-only. The report contains database schema and counts, no library records, media identities, personal paths, or activation values.

## Sources

The source labels below refer to local installed artifacts, not repository source code.

- S1: `/Applications/Stembox.app/Contents/Info.plist`.
- S2: `/Applications/Stembox.app/Contents/Resources/mappings/*.mapping`.
- S3: `/Applications/Stembox.app/Contents/MacOS/mappings/`, including `LISEZ-MOI.md` and `Pioneer-DDJ-FLX4.midi.xml`.
- S4: `/Applications/Stembox.app/Contents/Resources/ressources/traduction/client-control-translations.tsv`.
- S5: `/Applications/Stembox.app/Contents/Resources/docs/controles/controles.csv`.
- S6: `~/Library/Application Support/stembox-substrat/library.db`, queried with `sqlite3 -readonly ... '.schema'`, schema PRAGMAs, and aggregate counts.
- S7: `~/Library/Application Support/stembox-substrat/mappings-actifs.conf` and `mappings/*.mapping`.
- S8: `~/Library/Application Support/stembox-substrat/logs/stembox.log` and `stembox-precedent.log`, bounded to the last 2,000 lines per file.
- S9: `~/Library/Application Support/stembox-substrat/cache/`, `usersettings.json`, and `stembox_license.json`. Cache headers, structure, and counts were inspected. Activation inspection returned keys and types only.
- S10: `/Applications/Stembox.app/Contents/Resources/licences/index.txt`.

## Bundled controllers

Each model has a `Pioneer-DDJ-<model>.mapping` file. Deck numbers below are registry targets found in the file, not a count of physical decks. Input and output counts are record counts, not unique physical controls. Sources: S2, S3, S7.

- Pioneer DDJ-1000: MIDI port `DDJ-1000`, decks 1 through 4, 469 inputs, 591 outputs.
- Pioneer DDJ-400: MIDI port `DDJ-400`, decks 1 and 2, 206 inputs, 290 outputs.
- Pioneer DDJ-800: MIDI port `DDJ-800`, decks 1 through 4, 464 inputs, 590 outputs.
- Pioneer DDJ-FLX10: MIDI port `DDJ-FLX10`, decks 1 through 4, 459 inputs, 542 outputs.
- Pioneer DDJ-FLX4: MIDI port `DDJ-FLX4`, decks 1 and 2, 208 inputs, 291 outputs.
- Pioneer DDJ-FLX6: MIDI port `DDJ-FLX6`, decks 1 through 4, 441 inputs, 583 outputs.
- Pioneer DDJ-GRV6: MIDI port `DDJ-GRV6`, decks 1 through 4, 451 inputs, 586 outputs.

All seven `.mapping` files are byte-identical between `Resources/mappings` and `MacOS/mappings`. The seven local user copies also match the bundled files at inspection time.

S3 documents a startup copy into the user's mappings folder only when the file is absent. Existing user mappings survive updates. Native `.mapping` files take priority over legacy `.midi.xml` conversion. These are claims in the shipped README, not a startup-copy experiment in this investigation.

The README identifies GRV6 as a community mapping and explicitly says community mappings were not tested on hardware by the authors. Its controller list is older than the seven files in this bundle. The file inventory is the basis for the list above.

## Native mapping format

All seven files start with `SBX_CONTROLLER_MAPPING 6`. They are line-based text with whitespace-separated positional fields, double-quoted strings, and escaped text inside strings. They contain mapping data, not executable scripts. Sources: S2, S3.

The observed record families are:

- `id`, `name`, and `device` describe the mapping and matching MIDI port.
- `platter-calibration` stores a platter index, two directional counts, and a turn count. Some models also have `platter-ring` records.
- `modifier` and `modifier-feedback` define held layers and their feedback messages.
- `input` links a binding ID and registry path to an action, input mode, curve, numeric parameters, and MIDI match fields.
- `output` links a registry value to a MIDI message. Its byte expressions include `literal` and `value7`.
- `output-color` associates an output binding with a registry color-index path.
- `effect-selection` stores a named effect-selection intent in four of the seven mappings.
- `setting` stores a key, UI label, description, choice parameters, machine values, and display labels.
- `wake` occurs in the FLX10 mapping as `wake 0 3 191 66 127`.

A small verbatim FLX4 excerpt shows identity, an input, and a feedback output. This is an excerpt, not a complete preset:

```text
SBX_CONTROLLER_MAPPING 6
id "Pioneer-DDJ-FLX4"
name "Pioneer DDJ-FLX4"
device midi 0 0 "DDJ-FLX4"
input "learned-210" "deck.1.transport.playing" write absolute linear 1 0 127 0 1 1 0 0 0.5 0.050000000000000003 0 0 0.02 0 0 midi midi-note 0 11 0 0 0
output "vu-deck-1" "deck.1.level.meterLeft" midi none scale 0 1 0 3 literal 176 literal 2 value7 0
```

The input matches MIDI note 11 on channel 0 and targets `deck.1.transport.playing`. The output uses status byte 176, hexadecimal `B0`, controller number 2, and a seven-bit value derived from the meter.

Every observed `input` has 28 tokens, including the record name. Every `output` has 16. Input actions include `write`, `write-literal`, `toggle`, and `increment`. Input modes include `absolute` and `binary-offset`. Every observed input curve is `linear`. Numeric fields and unused trailing match fields have not been fully assigned parser semantics here.

### Legacy XML and translation data

`MacOS/mappings/Pioneer-DDJ-FLX4.midi.xml` has root `StemboxControllerPreset`, `schemaVersion="1"`, and `stemboxVersion="2.6"`. That last attribute belongs to the legacy file and does not identify the installed app version. Source: S3.

Its children are `info`, `settings`, `jogcalibration`, and `controller`. The controller has `controls` and `outputs`, plus empty `scriptfiles`, `screens`, and `qmllibraries` elements. The file contains 206 input `control` elements and 164 output elements. A control carries `group`, `key`, `description`, hexadecimal MIDI `status` and `midino`, and `options`, such as `normal` or `soft-takeover`.

The format-1 translation TSV has 481 non-comment rows and six columns. Source: S4.

```text
direction<TAB>source group<TAB>source key<TAB>disposition<TAB>target/layer<TAB>detail
input<TAB>[Channel1]<TAB>cue_default<TAB>control<TAB>deck.1.cue.button<TAB>-
```

Directions total 239 `input`, 117 `output`, and 125 `display` rows. Dispositions total 451 `control`, 20 `effect-selection`, eight `modifier`, and two `modifier-feedback` rows. The file header requires rejecting and logging unknown source keys without approximate matching.

## MIDI and HID evidence

Every bundled `device` record selects `midi`. The presets contain `midi-note`, `midi-cc`, and outgoing MIDI byte templates. No bundled `.mapping` selects an HID device. Source: S2.

The license manifest declares `hidapi` 0.15.0 with dynamic linkage. This documents an HID dependency, but does not establish a generic HID mapping grammar or a supported HID-controller list. Source: S10.

FLX10 jog-screen support has runtime evidence. The previous log records initialization SysEx, a 2,390-message opening sequence, an open interface, artwork transfers to decks 1 through 4, and a closed interface. The messages identify the FLX10 screen subsystem. They do not identify the transport of every screen operation. Source: S8.

Representative path-free log evidence:

```text
2026-10-12 17:08:58.100 W stembox.ecransjog: ouverture : SysEx d'init envoyes, maintien lance
2026-10-12 17:09:00.027 W stembox.ecransjog: ouverture : sequence de 2390 messages envoyee
2026-10-12 17:09:00.243 W stembox.ecransjog: interface ecran FLX10 ouverte
2026-10-12 17:11:41.187 W stembox.ecransjog: interface ecran FLX10 fermee
2026-10-14 16:52:09.664 W stembox.ecransjog: ecrans de jog FLX10 : ON
```

The retained files contain 586 and 572 lines, so the 2,000-line tail bound covers both completely. Startup messages record preference loading and bundled drum/sample loading. Both files also contain untranslated-control warnings. No general MIDI enumeration or successful mapping-load message was found in this bounded log set. The current `ON` message alone is not proof of an open interface.

## Control registry

`controles.csv` is 73,044 bytes. It contains 549 data rows with seven semicolon-separated columns. Blank lines and lines beginning with `#` are ignored, except that `#expand` declarations define template expansion. Source: S5.

```text
chemin ; genre ; plage ; defaut ; loi ; attributs ; description
deck.{d}.transport.playing ; toggle ; 0..1 ; 0 ; none ; rt,grouped ; Lecture en cours
```

The columns mean registry path, control kind, range, default, curve, attributes, and description. Attributes in the file are `rt`, `rtw`, `persist`, `grouped`, and `perTrack`, with comma separation and `-` for none.

The eight kinds, with template-row count followed by expanded-path count, are:

- `value`: 86 rows, 589 paths.
- `toggle`: 92 rows, 686 paths.
- `momentary`: 20 rows, 236 paths.
- `trigger`: 160 rows, 556 paths.
- `encoder`: four rows, 25 paths.
- `enum`: 26 rows, 95 paths.
- `choice`: six rows, 18 paths.
- `readonly`: 155 rows, 1,052 paths.

`enum` represents named states. `choice` represents admissible values separated by `|`, and stores the value rather than its position in the list. The header specifies refusal of values outside that list and saturation at its ends. Observed curve tokens are `none`, `linear`, `log`, `gainTaper`, `audioTaper`, `eqGain`, and `centreDetent`.

Expansion declarations are:

```text
d = 1..4     decks
p = 1..4     preview decks
s = 1..16    samplers
m = 1..4     microphones
a = 1..4     auxiliary inputs
h = 1..16    hot cues
u = 1..4     effect units
t = 1..4     effect slots per unit
k = 1..8     effect parameters
q = 1..16    effect pads
n = 1..16    pads and sequencer steps
l = 1..8     rhythm tracks
c = 1..8     controllers
```

Expansion produces 3,257 paths with no duplicates. Each `deck.1` through `deck.4` namespace has 391 paths. Microphone and auxiliary expansion variables are declared but unused in this file. Namespaces are `deck`, `preview`, `sampler`, `mixer`, `clock`, `fx`, `app`, `drum`, `stems`, `library`, `controller`, `license`, and `ui`.

The count is a calculation over shipped data, not an observation of a live registry. This reproduces it without reading user data:

```python
from pathlib import Path
import itertools
import re

text = Path("/Applications/Stembox.app/Contents/Resources/docs/controles/controles.csv").read_text()
expansions = {
	key: range(int(lo), int(hi) + 1)
	for key, lo, hi in re.findall(r"^#expand\s+(\w+)\s*=\s*(\d+)\.\.(\d+)", text, re.M)
}
paths = []
for line in text.splitlines():
	if not line.strip() or line.startswith("#"):
		continue
	path = line.split(";", 1)[0].strip()
	variables = list(dict.fromkeys(re.findall(r"\{(\w+)\}", path)))
	for values in itertools.product(*(expansions[key] for key in variables)):
		expanded = path
		for key, value in zip(variables, values):
			expanded = expanded.replace("{" + key + "}", str(value))
		paths.append(expanded)
print(len(paths), len(set(paths)))  # 3257 3257
```

## SQLite library

The file opens as SQLite without a key in this installation. `PRAGMA user_version` is 11 and `PRAGMA application_id` is 0. There are 19 application tables, one FTS5 virtual table, four FTS shadow tables, 16 explicit indexes, and three triggers. Source: S6.

Counts below come from one read transaction. Column lists preserve declared SQLite types. Primary keys, relationships, and constraints follow the lists. No table rows were exported.

### `analysis_states`

Rows: 0.

```text
track_id INTEGER
state TEXT
progress REAL
error TEXT
updated_at INTEGER
```

### `app_metadata`

Rows: 0.

```text
name TEXT
value TEXT
```

### `beat_grids`

Rows: 0.

```text
track_id INTEGER
tempo REAL
first_beat REAL
locked INTEGER
encoded_points BLOB
```

### `crate_items`

Rows: 0.

```text
crate_id INTEGER
track_id INTEGER
```

### `crates`

Rows: 0.

```text
id INTEGER
name TEXT
created_at INTEGER
```

### `external_items`

Rows: 46.

```text
media_id INTEGER
external_key TEXT
track_id INTEGER
title TEXT
artist TEXT
```

### `external_media`

Rows: 1.

```text
id INTEGER
serial TEXT
label TEXT
last_seen_at INTEGER
fingerprint TEXT
track_count INTEGER
```

### `library_node_items`

Rows: 0.

```text
node_id INTEGER
track_id INTEGER
position INTEGER
```

### `library_nodes`

Rows: 0.

```text
id INTEGER
parent_id INTEGER
folder INTEGER
name TEXT
sort_order INTEGER
source_folder TEXT
updated_at INTEGER
```

### `library_tracks`

Rows: 0.

```text
track_id INTEGER
added_at INTEGER
```

### `markers`

Rows: 0.

```text
id INTEGER
track_id INTEGER
frame_position REAL
length_frames REAL
label TEXT
color_index INTEGER
```

### `playlist_items`

Rows: 46.

```text
playlist_id INTEGER
track_id INTEGER
position INTEGER
```

### `playlists`

Rows: 1.

```text
id INTEGER
name TEXT
created_at INTEGER
```

### `session_items`

Rows: 0.

```text
session_id INTEGER
track_id INTEGER
started_at INTEGER
duration_nanos INTEGER
```

### `sessions`

Rows: 0.

```text
id INTEGER
started_at INTEGER
ended_at INTEGER
label TEXT
```

### `stem_preparations`

Rows: 0.

```text
track_id INTEGER
state TEXT
progress REAL
updated_at INTEGER
error TEXT
```

### `tracks`

Rows: 46.

```text
id INTEGER
file_path TEXT
title TEXT
artist TEXT
album TEXT
genre TEXT
comment TEXT
content_fingerprint TEXT
year INTEGER
rating INTEGER
color_index INTEGER
sample_rate INTEGER
channel_count INTEGER
frame_count INTEGER
beat_tempo REAL
beats_locked INTEGER
musical_key INTEGER
replay_gain REAL
available INTEGER
hidden INTEGER
stems_ready INTEGER
play_count INTEGER
modified_at INTEGER
last_scanned_at INTEGER
musical_mode INTEGER
origin_beats INTEGER
origin_key INTEGER
origin_cues INTEGER
origin_replay_gain INTEGER
origin_tempo INTEGER
device_path TEXT
resolved_path TEXT
analysis_path TEXT
artwork_path TEXT
```

### `tracks_fts`

Row count unavailable through a normal SELECT. See the FTS note below.

```text
title [no declared type]
artist [no declared type]
album [no declared type]
genre [no declared type]
file_path [no declared type]
year_text [no declared type]
```

### `tracks_fts_config`

Rows: 1.

```text
k [no declared type]
v [no declared type]
```

### `tracks_fts_data`

Rows: 24.

```text
id INTEGER
block BLOB
```

### `tracks_fts_docsize`

Rows: 46.

```text
id INTEGER
sz BLOB
```

### `tracks_fts_idx`

Rows: 22.

```text
segid [no declared type]
term [no declared type]
pgno [no declared type]
```

### `watched_folders`

Rows: 0.

```text
id INTEGER
folder_path TEXT
recursive INTEGER
enabled INTEGER
```

### `waveforms`

Rows: 0.

```text
track_id INTEGER
format TEXT
sample_count INTEGER
data BLOB
```

### Keys, relationships, and indexes

`id` is an integer primary key on `tracks`, `watched_folders`, `playlists`, `crates`, `sessions`, `markers`, `external_media`, and `library_nodes`. `app_metadata.name` is its primary key. The one-row-per-track tables use `track_id` as their primary key.

Composite primary keys are `playlist_items.playlist_id, position`, `crate_items.crate_id, track_id`, `session_items.session_id, track_id, started_at`, `external_items.media_id, external_key`, and `library_node_items.node_id, position`.

Unique constraints cover `tracks.file_path`, `watched_folders.folder_path`, `crates.name`, `external_media.serial`, the playlist-and-track pair, and the marker track-and-frame-position pair. `tracks.rating` has a check restricting it to 0 through 5.

Track references use `ON DELETE CASCADE`, except `external_items.track_id`, which uses `ON DELETE SET NULL`. Playlist, crate, session, external-media, and library-node ownership references cascade. `library_nodes.parent_id` references the same table and cascades.

The `tracks` schema comment distinguishes `file_path` as identity from `resolved_path` as the path to open. This distinction comes from schema text, not a comparison of user values.

Explicit track indexes cover title, artist, album, genre, year, rating, modified time, fingerprint, scan time, tempo, musical key and mode, and file path. Title, artist, album, and genre indexes use `COLLATE NOCASE`. Other explicit indexes cover playlist track, marker track, library-node parent and sort order, and library-node-item track.

`tracks_fts` is an external-content FTS5 table with `content='tracks'`, `content_rowid='id'`, and `tokenize='trigram'`. Its columns are `title`, `artist`, `album`, `genre`, `file_path`, and `year_text`. The three triggers maintain it after insert, delete, and update, casting `tracks.year` to text for `year_text`.

A normal `SELECT count(*) FROM tracks_fts` fails with `no such column: T.year_text`, because the backing `tracks` table has `year`, not `year_text`. The shadow table `tracks_fts_docsize` has 46 rows. That is a shadow-table count, not a successful virtual-table count or a search validation. No repair was attempted.

## Cache formats

Cache basenames are 40 hexadecimal characters in every inspected `.onde` and `.lufs` file. This is consistent with a 160-bit hash representation. The hash algorithm and identity inputs are unknown. No cache basenames are reproduced. Source: S9.

### Waveform cache, `.onde`

`cache/ondes` contains 45 files, sized 426,248 through 1,106,320 bytes. All have the same observed binary layout:

```text
offset  bytes  observed interpretation
0       4      ASCII SBOV
4       4      big-endian unsigned integer, 3
8       8      big-endian unsigned integer, N
16      8      big-endian IEEE-754 double
24      4*N    payload
```

Every inspected file satisfies `file_size == 24 + 4*N`. `N` ranges from 106,556 through 276,574. The double ranges from about 120.81 through 313.57, and `N / double` is approximately 882.

The integer 3 appears to be a format version. The double is consistent with duration in seconds, and the payload with four bytes per waveform point. Those semantic names are inferences from the layout and ratios. The individual payload-byte meanings were not established. No compressed audio container or text serialization was observed.

### Loudness cache, `.lufs`

`cache/sonie` contains two files, both eight bytes. Each is plain text with integer token `1`, one ASCII space, and a negative decimal number with three fractional digits. Neither has a trailing newline.

```text
1 <decimal-number>
```

The first token appears to be a format version. The extension and directory name associate the scalar with loudness. Whether it is integrated LUFS or another loudness-related value remains unverified.

## Settings and active mapping selection

`usersettings.json` is a UTF-8 JSON object with top-level `format`, `version`, and `groupes` keys. The observed format marker is `stembox-substrat-usersettings` and the version is 1. `groupes` contains bracketed legacy group names. Every observed leaf setting is a JSON string, including settings that represent numbers or flags. Source: S9.

The five groups contain 97 settings:

- `[Controls]`: two keys for rate direction and remaining duration.
- `[Mixer]`: 16 keys for gain, EQ, limiter, and crossfader settings.
- `[Recording]`: three keys for bit depth, format, and directory.
- `[StemboxSonParControleur]`: seven per-controller audio keys. Keys use a controller prefix, slash, and setting name.
- `[Stembox]`: 69 keys for display, pads, effects, audio, theme, and transport preferences.

This structural example substitutes placeholders for user values:

```json
{
	"format": "stembox-substrat-usersettings",
	"version": 1,
	"groupes": {
		"[Mixer]": { "auto_gain": "<string>" },
		"[Recording]": { "Directory": "<path>" }
	}
}
```

`mappings-actifs.conf` is a 221-byte text file with `#` comment lines and one tab-separated device-to-mapping row. Its header defines one row per device and says removing a row restores selection of the most recently written mapping. The observed selection is `DDJ-FLX10` to `Pioneer-DDJ-FLX10`. Source: S7.

```text
<device identifier><TAB><mapping identifier>
```

The empty `preferences.json.instance` file also exists. Its locking or lifecycle role was not established by reading its contents.

## License file shapes

`stembox_license.json` is a flat JSON object with these six keys. Only names and JSON types were collected. No activation values are included. Source: S9.

```text
activatedAt     string
clockHighWater  integer
key             string
kind            string
lastCheck       string
token           string
```

The key names suggest activation time, clock tracking, license kind, last verification, and credentials. Validation rules, token encoding, cryptography, and offline policy were not established from this file shape.

The third-party notice manifest `licences/index.txt` is a separate file. It contains 25 non-comment records, each with five tab-separated fields. Its shipped header names them as follows. Source: S10.

```text
nom<TAB>version<TAB>licence<TAB>liaison<TAB>fichier
```

`fichier` is relative to the index, or the sentinel `ABSENT`. The header says `ABSENT` is reported as a missing notice. No observed row uses it. Repeated dependency names correspond to separate notice records, so 25 records does not mean 25 distinct libraries. The index documents dependency notices, not user activation data.

## Limits

Bundled mapping presence establishes shipped presets. Hardware testing, mapping-parser execution, MIDI enumeration, and HID report decoding were outside this inventory. Runtime claims above come only from retained logs. Cache header semantics beyond the measured layout remain inferred. Library schema and counts describe this local version-11 database, not every installation or migration state.
