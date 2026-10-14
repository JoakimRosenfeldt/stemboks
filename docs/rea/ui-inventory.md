# Stembox 1.8.9 UI inventory

Static inventory of the installed bundle, inspected 2026-10-14. `Info.plist` reports `CFBundleShortVersionString=1.8.9`. The skin defines Large and Split layouts, four deck identities, one movable library, and full or compact pad displays.

## Evidence and notation

The source root is `/Applications/Stembox.app/Contents/Resources`. No top-level window width, height, or minimum screen resolution is declared by this root. All 28 XML files in `src/interface-stembox/dispositions`, `src/substrat/dispositions/stembox-racine.xml`, the 45,566-byte QSS, every SVG under the requested theme directory, all six bundled fonts, and the requested user settings were inspected.

`N` means deck 1 through 4. `P` means pad 1 through 8. `[ChannelN],key` is a concrete control family. Braced names in the declaration appendix preserve template variables, with `{group}` expanded to `[Channel{chanNum}]`. XML `f` means a fixed logical pixel dimension, `me` an expanding dimension, and `max` an upper limit, and `min` a minimum/content-sized dimension. These are declarations, not measured physical screen pixels. QSS dimensions retain their exact `px` units. `left` denotes an explicit `LeftButton` action. `state` denotes a connection without a button action or bound property.

XML elements and settings are observed evidence. Behavioral statements taken from shipped comments are identified below. Custom C++ widgets expose only their skin parameters here. This inventory does not establish their internal control bindings, rendered colors, or live behavior.

The shipped root explicitly calls itself a reconstruction assembly rather than the original client root. The QSS header identifies an extraction from `res/skins/LateNight/style.qss`, fork `4ae413db`, rather than a complete inherited stylesheet. Older comments conflict with later elements in several files. Current XML declarations take precedence.

## Window and navigation

`StemboxRoot` is vertical. Its first child, `StemboxNavRowWrap`, is `0me,46f`. Its second child, `StemboxBody`, is stacked. `[Stembox],mix_mode=0` shows `StemboxNavRowLarge` and `StemboxBodyLarge`. Value `1` shows `StemboxNavRow` and `StemboxBodySplit`.

Large has five buttons, each separated by `5f`, with `StemboxNavEdgePad` of `192f,0me` at both ends. Split has two `StemboxNavBar` instances separated by `StemboxNavDivider`, `40f,0me`. The left starts with EXIT and the right with MENU. Split columns use horizontal `i` size policy and minimum `0,0`, so content does not establish an unequal minimum width.
| ObjectName / text | Left-button ConfigKey | Displayed state |
| --- | --- | --- |
| `StemboxNavExit` / EXIT | `[Stembox],exit_request` | Two states, same text |
| `StemboxNavMenu` / MENU | `[Stembox],show_preferences` | `[Stembox],show_preferences` |
| `StemboxNavBrowse` / BROWSE | `[Stembox],browse_full / browse_left / browse_right` | `[Stembox],browse_lit_full / browse_lit_left / browse_lit_right` |
| `StemboxNavPlaylist` / TAG LIST | `[Stembox],playlist_full / playlist_left / playlist_right` | `[Stembox],playlist_mode` |
| `StemboxNavDisplay` / SEARCH | `[Stembox],search_full / search_left / search_right` | `[Stembox],search_mode` |
### Large mix

`stembox_mix_large.xml` has an upper `WidgetStack` whose library page is triggered by `[Stembox],lib_full`. `LibrarySingleton` replaces the upper mix area. The lower display stays outside that stack.

With `[Stembox],large_4decks=0`, `StemboxLargeWaveforms` stacks two full-width waveform views. `StemboxLargeWf1` displays deck 1 or `StemboxLargeWf1Alt` displays deck 3. `StemboxLargeWf2` displays deck 2 or `StemboxLargeWf2Alt` displays deck 4. `[Stembox],deck_active3` and `deck_active4` select those alternatives. `StemboxLargeRow` is `0me,130f`, followed by `StemboxBottomRow`, `0me,148f`. Each side's CDJ information and bottom controls follow its selected deck.

With `large_4decks=1`, all four waveforms use `stembox_large_wf.xml`, ordered 4, 1, 2, 3. Each waveform has a `30f` live or idle deck tag. Decks 1 and 2 occupy one `StemboxLargeRow4`, `0me,150f`; decks 3 and 4 occupy another outside the library stack. Each row combines `130f` deck information and a `20f` pad strip. `vue_large_compacte` removes the MT, QUANTIZE, FX, TRACK, and CUE/SYNC groups, moves the overview into the PLAYER bar, and substitutes compact pads. The upper row disappears behind browsing; the lower row remains.

The center corridor is `40f`, formed from `StemboxLargeCenterBar` `10f`, `StemboxLargeCenterFill` `20f`, and another `10f` bar. Its upper fill is black. Multi Beat FX adds `StemboxLargeMultiFxRow` or `StemboxLargeMultiFxRow4` with `20f` `StemboxMultiFxTabsLarge` widgets, decks 1 and 2, gated by `[Stembox],beat_fx_multi`.

### Split mix

`stembox_mix_split.xml` lays out left column, `StemboxCdjDivider` `40f,0me`, and right column. Left alternates decks 1 and 3; right alternates 2 and 4. Each side uses stacked deck containers and `deck_active3` or `deck_active4` visibility bindings. A deck's preview twin is 1↔2 or 3↔4.

`[Stembox],lib_left` or `lib_right` replaces only that side's upper deck with `StemboxSplitLibDock` containing `LibrarySingleton`. The other side and both deck-bottom zones remain. Comments describe mutual exclusion of the two library docks and reuse of existing deck widgets; the XML alone does not prove the controller enforces it.

`StemboxDeckBottom` follows its content height. Full mode totals `52f` for `StemboxDeckBar2` plus `96f` for `StemboxPads`, normally `148f`. Compact mode hides the whole `52f` band and shows `StemboxPadsSlim`, `20f`.

`stembox_mix_active.xml` is a separate two-deck layout using decks 1 and 2, the same `40f` divider, library swaps, and bottom templates. The shipped root does not reference it. Its presence does not establish an active third window mode.

## Deck structure and controls

`stembox_cdj_deck.xml` takes `chanNum` and `otherNum`. It sets `group=[ChannelN]` and `other=[ChannelOther]`. Top-to-bottom structure follows.

1. `StemboxDeckInfo`, `0me,52f`: `StemboxDeckCover` `44f,44f`, a `10f` gap, note or stems icon `18f,18f`, title `0me,26f`, artist `0me,20f`, then BPM `58f,46f`, KEY `50f,46f`, TIME `62f,46f`, and grid-edit button `24f,24f`. Title uses `titleInfo`; artist uses `artist`. Both elide right and have minimum `0,0`.
2. `StemboxDeckWaveforms`, expanding and visible only in Split. Optional Multi FX tabs are `0me,20f` inside a `24f` row. A `41f` spacer precedes `StemboxWfThin` or `StemboxGridPreview`, each `0me,64f`. Both preview styles require loaded host and twin tracks. A `1f` top pad precedes expanding `StemboxWfBig`; the big wave reserves `12` for the cue marker. `StemboxWfBottomPad` is `0me,32f`. Empty decks substitute `StemboxDeckLogo`, `256f,43f`, with a `40f` spacer above its centered area.
3. `StemboxDeckBar`, `0me,68f`: PLAYER `52f,58f`, TRACK `52f,58f`, CUE over SYNC inside `52f,58f`, time, tempo-range selector `56f,62f`, and BPM `120f,65f`. Full time container is `238f,63f`; compact is `158f,63f`. Time digits are `0me,47f`, small-digit ratio `0.58`. BPM digits are `0me,48f`, ratio `0.58`. TRACK is a `trackno` readout rather than a hardcoded number.
4. The separate deck-bottom template contains MT and QUANTIZE, overview, FX indication, and pads.
| Control or display | ObjectName and geometry | ConfigKey / exact skin parameter |
| --- | --- | --- |
| Stem availability / EQ-to-stems | `StemboxDeckNote`, `18f,18f` | `[ChannelN],stem_count` double NOT chooses note versus stems; `[ChannelN],eq_stems` chooses white versus rose icon |
| Original BPM / grid diagnostic | `StemboxDeckStatVal`, `0me,26f`; hit area `StemboxBpmDiagHit`, `58f,46f` | Readout `Mode=filebpm`; diagnostic action `[Stembox],grid_diag` |
| Original key / duration | `StemboxDeckStatVal`, `0me,26f` | Key binds `[ChannelN],key` and sets `UseFileKey=true`; comment names the original `file_key`; TIME readout `Mode=duration`; empty key displays `#` |
| Grid panel toggle | `StemboxGridEditBtn`, `24f,24f` | Action `[Stembox],grid_editN`; enabled by `[ChannelN],track_loaded` |
| Waveform preview style | `StemboxWfThin` / `StemboxGridPreview`, `0me,64f` | Persistent `[Stembox],preview_style`, 0 waveform / 1 grid; preview channel `otherNum`; zoom group is host deck |
| Waveform hold zoom | `StemboxWfBig`, expanding | `StemboxZoomHold=true`; comments specify hold left half to zoom out, right half to zoom in. Thin preview omits this flag |
| Menu occlusion | Waveform container visibility | `[ChannelN],stembox_menu_dessus` inverted; Large waveform area uses inverted `[Stembox],show_preferences` |
| Player live / idle | `StemboxPlayerBox` / `StemboxPlayerBoxIdle`, `52f,58f` | `[Stembox],deck_liveN` chooses variants |
| Cue arcs | `StemboxCueWave`, within `14f,40f` wrappers | Display `[ChannelN],cue_indicator`; comments describe steady at cue or playing, blinking when paused away from cue |
| Headphone CUE | `StemboxCueBtn`, `46f,22f` | Left action and state `[ChannelN],pfl`. This is headphone monitoring |
| SYNC indicator | `StemboxSyncBtn`, `46f,22f` | Display-only `[ChannelN],stembox_follower`; no click action or `sync_enabled` binding |
| Time / alert caption | `StemboxRemain`, `0me,47f`; `StemboxBarCap`, `0me,11f` | `Mode=time`; custom `StemboxTimeCap` takes deck group. Comments name NO FX PART SELECT and VINYL MODE ON/OFF alerts |
| Slip indicator | `StemboxSlipTag`, `40f,17f` | `[ChannelN],stembox_slip_mode`; compact variant uses the same key on an enclosing `44f,63f` group so inactive SLIP reserves no width |
| Tempo range | `StemboxRateRangeBtn`, `56f,62f` | Custom widget takes `Deck=N`; comment names `[ChannelN],rateRange`, cycles ±6, ±10, ±16, WIDE ±100% |
| BPM state | `StemboxBpmBox` / `StemboxBpmBoxIdle`, `120f,65f` | `deck_liveN`; BPM caption switches to BEAT SYNC on `[ChannelN],stembox_follower`. Later comment removes orange MASTER display |
| Master tempo | `StemboxToggle`, `60f,20f` | Left action and state `[ChannelN],keylock` |
| Quantize | `StemboxQuantize`, `60f,20f` | Left action and state `[ChannelN],quantize` |
| Overview / seek | `StemboxOverview`, `0me,48f` | `[ChannelN],playposition`; visible on `track_loaded`; `StemboxNotLoadedSmall` replaces it when empty |
| Effect indication | `StemboxFxBlock`, `54f,48f` | Custom `StemboxFx`, `Deck=N`, `ColorFxOnDecks=1,3`. Left decks show Color FX; comments describe other decks showing shared Beat FX |
### Beat-grid editor

`StemboxGridEditPanel` is `154f,287f`, visible on `[Stembox],grid_editN`. The Large layout reserves `StemboxGridEditColumn`, `160f,0me`, gated by `[Stembox],grille_ouverte`. Split places the editor beside the entire waveform column. The panel has TEMPO, POSITION, and 1ST TIME sections. Exact click keys are `[ChannelN],stembox_grid_bpm_down`, `stembox_grid_bpm_up`, `beats_translate_earlier`, `beats_translate_later`, `stembox_grid_first_earlier`, and `stembox_grid_first_later`. Tempo and position buttons set `StemboxRepeat=true`. The first-beat buttons use `StemboxGridEditKeyRed` and `StemboxHoverText=#000000`. The appendix retains every declared row and button size.

### Waveform colors

XML specifies background `#000000`, ordinary signal `#cc7a1f`, bass `#054CE3`, mids `#DE7002`, highs `#FAF4E8`, and beat lines `#000000`. Default hotcue marks use `#FF0000` with black text; comments say native widgets replace the mark color with `[ChannelN],hotcue_X_color`. Loop regions use `[ChannelN],loop_start_position`, `loop_end_position`, and `loop_enabled`, active `#ff9800` at opacity `0.60`, inactive `#ffffff` at `0.20`. Overview hotcue alignment is `top|left`; large waveform alignment is `top|hcenter`.

## Mixer

The specified XML files declare no channel volume faders, crossfader, EQ knobs, gain knobs, master meter, or master-volume widgets. There are no `ConfigKey` bindings for those controls in this skin set. The `40f` center divider is a decorative widget. The deck CUE buttons provide PFL, and `StemboxFx` / `StemboxMultiFx` provide effect indications.

Mixer preferences are present in the saved JSON. `[Mixer]` has automatic gain, master limiter, EQ curve and sensitivity, eight EQ gains, and crossfader curve, mode, reverse, and disable keys. The deck `eq_stems` display documents controller EQ-to-stems mode; the comment names VOCAL, INST, and DRUMS. No exact mixer control geometry or C++ binding can be recovered from these files.

## Library views

`LibraryContainer` contains the optional left navigation and `StemboxMain`. One `LibrarySingleton` moves between full-width and Split docks. `StemboxMain` holds a header and content, with SOURCE, navigation lists, tracks, search, and STEMS MANAGER subtrees. An unnamed native `LibrarySidebar` supplies the SOURCE and category lists.

- SOURCE: `[Stembox],at_root=1`. `StemboxSourceHeaderRow` is `0me,70f`, with SOURCE, diagnostic `?` `30f,30f` to `source_diag_open`, and STEMS MANAGER `190f,34f` to `stems_page_open`. `StemboxRootRow` holds device or source rows and the stats panel. Its exact device content is supplied by the library/controller, absent from the XML.
- USB and statistics: `StemboxStatsPanel` is `360f,0me`; Split QSS overrides width to `270px`. Rows Source, Songs, Playlists, Date, Total, and Available are `46f` high with `1f` horizontal dividers. Available hides for `src_library_loaded`. `StemboxProgressRow`, `52f`, follows `src_analyzing` and shows Loading plus a `10f` progress track. `StemboxProgressFill` starts `0f,10f`. Eject drive acts on `src_eject_drive` and follows `src_device_loaded`. Open library shares `StemboxEjectBtn`, acts on `stembox_lib_manage`, and follows `src_library_loaded`.
- Sidebar below SOURCE: `StemboxLeftNavInner` is `66f,0me`; `StemboxUsbCell` is `70f` high, with `StemboxUsbHit` `66f,70f`, blue `StemboxUsbBox` `64f,66f`, glyph `13f,18f`, and USB label `15f` high. USB returns through `go_source`. Sidebar hides at root, during search, or on `nav_hidden`. Right divider is `1f`.
- Navigation categories: ARTIST, ALBUM, TRACK, KEY, PLAYLIST, HISTORY, MATCHING, SEARCH. Actions are `[Stembox],go_artist`, `go_album`, `go_track`, `go_key`, `go_playlist`, `go_history`, `go_matching`, and `go_search`. Display controls are `nav_active_artist`, `nav_active_album`, `nav_active_track`, `nav_active_key`, `nav_active_playlist`, `nav_active_history`, `nav_active_matching`, and `search_mode`. `StemboxNavIcon` is `27f,27f`; labels are `0me,18f`. TRACK adds `RShift=4f`; the others use zero shifts.
- Header: `StemboxTopBar` is `0me,70f`, with `StemboxBackWrap` `50f,40f`, `StemboxBackButton` `34f,34f` to `header_back`, and `StemboxListIcon` `24f,24f`. `back_hidden` suppresses the back wrapper. `[Stembox],nav_mode` selects header icons: 0 playlist, 1 artist, 2 album, 3 key, 4 track, 5 matching, 6 history. `StemboxPlaylistTitle` is `0me,34f`, right-elided. `StemboxFontSize` is `82f,70f`; `font_state=0/1/2` selects small/medium/large icons, each acting on `font_cycle`. Exact track-row font sizes are not in this QSS.
- Playlists and key browse: `StemboxNavPane` follows inverted `show_tracks`; the native library supplies lists. `StemboxLibraryPreview` is `760f,0me`, only in Large, below SOURCE and outside TAG LIST mode. The KEY route and header icon are explicit. Musical-key naming conventions, sorting, color mapping, and playlist table columns are supplied elsewhere.
- Tracks: `StemboxTracksPane` follows `show_tracks`. `StemboxResultsWrap` and `StemboxResultsInner` have minimum `0,0`. The native `Library` sets `ShowButtonText=false`, `TrackTableBackgroundColorOpacity=0`, and inherited `SignalColor_12`, whose value is not defined in these files. Table column definitions are absent.
- Search: `StemboxHeaderSearch` replaces icon/title with `SearchBox` in a `46f` row. `StemboxKeyboard` is `0me,200max`, with an extra `6f` bottom pad. `[Stembox],keyboard_azerty` selects AZERTY when 1 or QWERTY when 0. Exact key actions are `kb_a` through `kb_z`, `kb_back`, `kb_clear`, `kb_123`, and `kb_space`. Wide keys are CLEAR and SPACE. SearchBox has no explicit XML ConfigKey. Comments describe empty results until input, driven by `search_empty` elsewhere.
- History: `[Stembox],go_history` and `nav_active_history`. PAST SESSIONS is `150f,34f`, acts on `go_past_sessions`, and hides while `past_sessions_open`. A comment describes one session per Stembox launch.
- Matching: route `go_matching`, selection `nav_active_matching`. The current XML contains the normal empty table when no reference track exists. A later comment explicitly removes the old "Please load a track on the opposite deck" message. Residual QSS for `StemboxMatchingEmpty` does not establish a current widget.
- STEMS MANAGER: `stems_page` replaces the ordinary navigation and tracks. Back acts on `stems_page_close`; header is `70f`, `StemboxStemsDest` is `0me,70f`, and `StemboxStemsList` expands. Comments describe custom destination cards, radio indicators, lock, and arrow. Their exact internal geometry is absent.

## Pad modes

Full `StemboxPads` is `96f` with two rows of four. Compact `StemboxPadsSlim` is `20f` with eight positions in one line. Mode visibility is `[Stembox],pad_<mode>_N`; physical press highlighting is `[Stembox],padpress_N_P`. There is no on-screen mode-selector action in these templates.
| Mode key suffix | Current widget / labels | Bindings and limitations |
| --- | --- | --- |
| `hotcue` | `StemboxPad`, A B C D / E F G H; chip `14,14`, badge `31,14` | Native HotcueButton/HotcueLabel take `[ChannelN]` and `Hotcue=1..8`. Loop and stems badges come from custom/native code. Compact `StemboxPadSlimChip` is `20,4`, blank text, inset `7f` |
| `beatjump` | Two `StemboxBjRow`: backward/forward 4, 8 / 16, 32 beats | `Deck=N`, `PadBase=1/5`, `N1=4/16`, `N2=8/32`. Native painted rows; no explicit jump action ConfigKeys in XML |
| `beatloop` | Two `StemboxBlRow`: 1/4, 1/2, 1, 2 / 4, 8, LOOP SAVER | Group `[ChannelN]`; `S1/S2/S3/S4=0.25/0.5/1/2`, then `4/8/empty/empty`; second row `Saver=true`. Comment names `beatloop_<S>_enabled` indication. No saver action key declared |
| `keyshift` | Two `StemboxKsRow`: 0, +1, +2, +3 / -3, -2, -1, 0 | `PadBase=1/5`; K classifications `zero/up/up/up` and `down/down/down/zero`. Comment names `pitch_adjust`; exact action implementation absent |
| `sampler` | Two `StemboxSampRow`, `PadBase=1/5` | Current row comment describes one fixed soundboard shared across decks, `stembox::sampler::kSlots`. Sound names are absent. Older `[Sampler1..16]` parameters remain at call sites but are unused by current rows |
| `keyboard` | One custom `StemboxStemPads`, `Deck=N` | Current mode is STEMS. Compact comment names ACTIVE PART, three stems, FX PART SELECT, three FX. Stem roles depend on `stem_count`; comment says pad 2 is INST for three stems, BASS for four. Muted stem opacity is described as 20%, not a QSS declaration |
| `padfx` | Eight `StemboxFxCell`, `Slot=1..8`, `PK=padpress_N_P` | Cells read `padfx_name_SLOT` / `padfx_vlabel_SLOT`; comment names repaint signal `[Stembox],padfx_labels`. Effect emblems and colors are drawn in C++ |
| `padfx2` | One custom `StemboxDrums`, `Deck=N` | Current mode is DRUM STATION. Comments name volume, instrument, REVERB, NOISE, time divisions, dot matrices, step glow, and a level gauge. No action keys or exact painted geometry in XML |
Comments label shifted controller modes as SHIFT+BEAT JUMP for beatloop, SHIFT+SAMPLER for keyshift, SHIFT+HOT CUE for stems, and SHIFT+PAD FX 1 for drums. Controller mappings were outside this inventory.

Unreferenced older templates remain shipped: `stembox_pad_beatjump.xml`, `stembox_pad_bl.xml`, `stembox_pad_ks.xml`, `stembox_pad_sampler.xml`, `stembox_pad_stem.xml`, `stembox_pad_num.xml`, and `stembox_pad_label.xml`. They are included in the declaration appendix. Their obsolete sampler, placeholder, or second-effect-bank comments do not describe the current composed pad pages. The older beatloop cell accepts a size parameter; the current page offers six loop sizes and LOOP SAVER.

## Second screen and CDJ view

The saved settings contain `[Stembox],force_second_screen="1"` and `known_screens="MPG321UX OLED|Built-in Retina Display|SideScreen"`. These prove a saved request and remembered screen names, not which displays are connected or what is currently visible.

`stembox_cdj_deck.xml` supplies the CDJ view described above, reused in the Split, Large, and unreferenced active layouts. Comments on `[ChannelN],stembox_menu_dessus` explicitly say that a menu covering one deck hides that deck's GL waveform while the deck on the other screen stays visible. The resource set has no second-screen root, screen geometry, deck-to-screen assignment, window title, or independent second-screen mixer layout. Those details remain unresolved without implementation or runtime evidence.

## Fonts and QSS summary

The six font files are under `ressources/binaires-client/fonts`. Families below come from each TTF name table, not filename inference.
| File | Family | Subfamily |
| --- | --- | --- |
| `DHA Stembox.ttf` | `DHA Stembox` | `Regular` |
| `HVTC Stembox.ttf` | `HVTC Stembox` | `Regular` |
| `MRRT Stembox.ttf` | `MRRT Stembox` | `Regular` |
| `OpenSans-Bold.ttf` | `Open Sans` | `Bold` |
| `OpenSans-Regular.ttf` | `Open Sans` | `Regular` |
| `OpenSans-Semibold.ttf` | `Open Sans Semibold` | `Regular` |
QSS uses `MRRT Stembox` for deck title `17px` bold, artist `13px`, and playlist title `22px`. `HVTC Stembox` supplies navigation `11px`, sidebar labels `10px`, SOURCE `22px` bold, stats `17px` or `13px` in Split, deck captions `9px` or `10px`, small stats `18px`, PLAYER `25px`, TRACK `34px`, time/BPM `46px`, hotcue letter `11px`, hotcue name `12px`, and keyboard `20px` or `12px` in Split. `DHA Stembox` has a residual matching-empty label at `20px`. `Open Sans` styles analysis/eject buttons at `12px`. The complete rule list below retains overrides, borders, radii, margins, padding, and exact colors.

Main colors are active blue `#1A79EB` / `#1a79eb`, hover blue `#2d7ff9`, active nav text `#33baff`, background `#000000`, `#060606`, `#0a0a0a`, `#141414`, and `#1e1e1e`, CUE orange `#e08a2a`, SYNC blue `#2f9bd6`, QUANTIZE/SLIP red `#ff0000`, loops `#ff9800`, keyshift up `#f0c000`, down `#a05cf0`, and zero `#1A79EB`. Keyshift cell numbers remain white. Pads fade vertically from `#131312` to `#090907` with `#2c2c2c` borders. The Split divider fades `#303030` to `#000000`. Nav row stops are `#1e1e1e` at 0/0.28/0.72/1 and `#303030` at 0.5.

Some QSS selectors have no corresponding current XML object, including `StemboxMixLibDivider`, `StemboxAnalyzePauseBtn`, `StemboxMatchingEmpty`, `StemboxKey2`, and old pad cell styles. Their presence establishes styling definitions only. The declaration appendix distinguishes shipped definitions from template composition.

## Saved preferences

`usersettings.json` has `format="stembox-substrat-usersettings"`, `version=1`, and five groups. All saved values below are JSON strings. They are this installation's values, not factory defaults or a complete settings schema. Missing preferences such as `keyboard_azerty`, `large_4decks`, sidebar hiding, or `font_state` have no persisted value in this snapshot.

### Non-pad preferences
| Exact ConfigKey | Saved string |
| --- | --- |
| `[Controls],RateDir` | `"0"` |
| `[Controls],ShowDurationRemaining` | `"1"` |
| `[Mixer],auto_gain` | `"1"` |
| `[Mixer],eq_curve` | `"1"` |
| `[Mixer],eq_gain_1` | `"0"` |
| `[Mixer],eq_gain_2` | `"0"` |
| `[Mixer],eq_gain_3` | `"0"` |
| `[Mixer],eq_gain_4` | `"0"` |
| `[Mixer],eq_gain_5` | `"0"` |
| `[Mixer],eq_gain_6` | `"0"` |
| `[Mixer],eq_gain_7` | `"0"` |
| `[Mixer],eq_gain_8` | `"0"` |
| `[Mixer],eq_sensitivity` | `"100"` |
| `[Mixer],master_limiter` | `"1"` |
| `[Mixer],xfader_curve` | `"1"` |
| `[Mixer],xfader_disable` | `"0"` |
| `[Mixer],xfader_mode` | `"1"` |
| `[Mixer],xfader_reverse` | `"0"` |
| `[Recording],BitDepth` | `"0"` |
| `[Recording],Directory` | `"/Users/joakimrosenfeldtpedersen/Music/Stembox/Recordings"` |
| `[Recording],Format` | `"0"` |
| `[StemboxSonParControleur],ddj-flx10/sound_api` | `"CoreAudio"` |
| `[StemboxSonParControleur],ddj-flx10/sound_buffer_frames` | `"512"` |
| `[StemboxSonParControleur],ddj-flx10/sound_headphones_channels` | `""` |
| `[StemboxSonParControleur],ddj-flx10/sound_headphones_device` | `"---"` |
| `[StemboxSonParControleur],ddj-flx10/sound_main_channels` | `""` |
| `[StemboxSonParControleur],ddj-flx10/sound_main_device` | `"---"` |
| `[StemboxSonParControleur],ddj-flx10/sound_sample_rate` | `"44100"` |
| `[Stembox],backspin` | `"4"` |
| `[Stembox],force_second_screen` | `"1"` |
| `[Stembox],frame_rate` | `"60"` |
| `[Stembox],hotcue_mode` | `"0"` |
| `[Stembox],key_colors` | `"0"` |
| `[Stembox],known_screens` | `"MPG321UX OLED&#124;Built-in Retina Display&#124;SideScreen"` |
| `[Stembox],mix_mode` | `"0"` |
| `[Stembox],padfx_release_s` | `"3"` |
| `[Stembox],padfx_type` | `"0"` |
| `[Stembox],play_marker_position` | `"1"` |
| `[Stembox],preview_style` | `"0"` |
| `[Stembox],sound_api` | `"CoreAudio"` |
| `[Stembox],sound_buffer_frames` | `"512"` |
| `[Stembox],sound_headphones_channels` | `""` |
| `[Stembox],sound_headphones_device` | `"---"` |
| `[Stembox],sound_main_channels` | `""` |
| `[Stembox],sound_main_device` | `"---"` |
| `[Stembox],sound_sample_rate` | `"44100"` |
| `[Stembox],theme` | `"0"` |
| `[Stembox],track_end_warning` | `"30"` |
| `[Stembox],waveform_color` | `"1"` |
`[Controls],ShowDurationRemaining="1"` records remaining-time display. `[Stembox],mix_mode="0"` records Large, `preview_style="0"` records waveform preview, and `frame_rate="60"` records the requested rate. `theme`, `waveform_color`, `key_colors`, `hotcue_mode`, and mixer enum meanings are not fully specified by these files. `sound_*` records CoreAudio, 44,100 Hz, and a 512-frame buffer; device `---` and empty channel strings are saved placeholders.

### Pad FX configuration

Each row expands to exact `[Stembox],padfx_name_SLOT`, `padfx_vlabel_SLOT`, `padfx_effect_SLOT`, `padfx_bparam_SLOT`, `padfx_bval_SLOT`, and `padfx_releasefx_SLOT` keys. Effect numeric identifiers are recorded without inventing an enum. `padfx_type="0"` and `padfx_release_s="3"` are in the preceding table.
| Slot | name | vlabel | effect | bparam | bval | releasefx |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `"ROLL"` | `"1/2"` | `"-2"` | `"0"` | `"0.5"` | `"0"` |
| 2 | `"ECHO"` | `"1"` | `"2"` | `"1"` | `"1"` | `"0"` |
| 3 | `"VINYL BRAKE"` | `""` | `"9"` | `"0"` | `"0"` | `"0"` |
| 4 | `"REVERB"` | `"50%"` | `"1"` | `"1"` | `"0.5"` | `"0"` |
| 5 | `"ENIGMA JET"` | `"+1/16"` | `"4"` | `"1"` | `"0.0625"` | `"0"` |
| 6 | `"PHASER"` | `"1/16"` | `"5"` | `"1"` | `"0.0625"` | `"0"` |
| 7 | `"FLANGER"` | `"1/16"` | `"6"` | `"1"` | `"0.0625"` | `"0"` |
| 8 | `"TRANS"` | `"1/16"` | `"0"` | `"2"` | `"0.0625"` | `"0"` |
## Exact XML declaration index

Each subsection names its source file. Rows cover every named widget and every unnamed control/custom widget. Anonymous layout spacers are omitted. All non-child properties, state text, and direct connections are retained. `currentpage` attributes and bindings on unnamed layout containers appear in the separate binding list. The Large library page additionally declares `trigger="[Stembox],lib_full"` and `on_hide_select="0"`. Repeated identical rows are collapsed. Dynamic properties drawn inside C++ widgets have no invented ConfigKeys.
### library.xml
Source: [library.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/library.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `LibraryContainer` | `Layout=horizontal; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxLeftNavToggleWrap` | `Layout=horizontal; SizePolicy=max,me` | `[Stembox],nav_hidden → visible NOT` |
| `WidgetGroup` | `StemboxLeftNav` | `Layout=vertical; SizePolicy=max,me` | `[Stembox],at_root → visible NOT` |
| `WidgetGroup` | `StemboxLeftNavInner` | `Layout=vertical; SizePolicy=f,me; Size=66f,0me` | `[Stembox],search_mode → visible NOT` |
| `WidgetGroup` | `StemboxUsbCell` | `Layout=stacked; Size=0me,70f` | none |
| `PushButton` | `StemboxUsbHit` | `Size=66f,70f; NumberStates=1` | `[Stembox],go_source → left` |
| `WidgetGroup` | `StemboxUsbBox` | `Layout=vertical; Size=64f,66f` | none |
| `PushButton` | `StemboxUsbGlyph` | `Size=13f,18f; NumberStates=1` | `[Stembox],go_source → left` |
| `Label` | `StemboxUsbLabel` | `Text=USB; Size=0me,15f; Alignment=center` | none |
| `WidgetGroup` | `StemboxNavRightDividerWrap` | `SizePolicy=max,me` | `[Stembox],at_root → visible NOT` |
| `WidgetGroup` | `StemboxNavRightDivider` | `Size=1f,0me` | `[Stembox],search_mode → visible NOT` |
| `WidgetGroup` | `StemboxMain` | `Layout=vertical; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxTopBar` | `Layout=horizontal; Size=0me,70f` | `[Stembox],at_root → visible NOT` |
| `WidgetGroup` | `StemboxBackWrap` | `Layout=horizontal; Size=50f,40f; MaximumSize=50,40` | `[Stembox],back_hidden → visible NOT` |
| `PushButton` | `StemboxBackButton` | `Size=34f,34f; MaximumSize=34,34; NumberStates=1` | `[Stembox],header_back → left` |
| `WidgetStack` | `StemboxListIcon` | `Size=24f,24f; attributes={'currentpage': '[Stembox],nav_mode'}` | `[Stembox],at_root → visible NOT; [Stembox],search_mode → visible NOT` |
| `PushButton` | `[unnamed]` | `Size=24f,24f; NumberStates=1` | none |
| `Label` | `StemboxPlaylistTitle` | `Text=; Size=0me,34f; Alignment=left; Elide=right` | `[Stembox],at_root → visible NOT; [Stembox],search_mode → visible NOT` |
| `WidgetGroup` | `StemboxHeaderSearch` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],search_mode → visible` |
| `WidgetGroup` | `StemboxSearchBar` | `Layout=horizontal; Size=0me,46f` | none |
| `SearchBox` | `[unnamed]` | none | none |
| `LibrarySidebar` | `[unnamed]` | none | none |
| `WidgetGroup` | `StemboxPastSessionsGroup` | `SizePolicy=f,me; Layout=horizontal` | `[Stembox],nav_active_history → visible` |
| `PushButton` | `StemboxPastSessionsBtn` | `Size=150f,34f; NumberStates=1; State.Text=PAST SESSIONS` | `[Stembox],go_past_sessions → left` |
| `WidgetStack` | `StemboxFontSize` | `SizePolicy=f,me; Size=82f,70f; attributes={'currentpage': '[Stembox],font_state'}` | `[Stembox],at_root → visible NOT` |
| `PushButton` | `[unnamed]` | `SizePolicy=me,me; NumberStates=1` | `[Stembox],font_cycle → left` |
| `WidgetGroup` | `StemboxContent` | `Layout=vertical; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxNavPane` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],stems_page → visible NOT` |
| `WidgetGroup` | `StemboxSourceHeaderRow` | `Layout=horizontal; Size=0me,70f; SizePolicy=me,f` | `[Stembox],at_root → visible` |
| `Label` | `StemboxSourceHeader` | `Text=SOURCE; SizePolicy=min,me; Alignment=left` | none |
| `PushButton` | `StemboxSourceDiagBtn` | `Size=30f,30f; NumberStates=1; State.Text=?` | `[Stembox],source_diag_open → left` |
| `PushButton` | `StemboxStemsManagerBtn` | `Size=190f,34f; NumberStates=1; State.Text=STEMS MANAGER` | `[Stembox],stems_page_open → left` |
| `WidgetGroup` | `StemboxRootRow` | `Layout=horizontal; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxVDivider` | `Size=2f,0me` | none |
| `StemboxLibraryPreview` | `StemboxLibraryPreview` | `SizePolicy=f,me; Size=760f,0me` | none |
| `WidgetGroup` | `StemboxVDivider` | `Size=2f,0me` | `[Stembox],at_root → visible` |
| `WidgetGroup` | `StemboxStatsPanel` | `Layout=vertical; Size=360f,0me` | `[Stembox],at_root → visible` |
| `WidgetGroup` | `StemboxStatRow` | `Layout=horizontal; Size=0me,46f` | none |
| `Label` | `StemboxStatKey` | `Text=Source; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatSource` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `WidgetGroup` | `StemboxHDivider` | `Size=0me,1f` | none |
| `Label` | `StemboxStatKey` | `Text=Songs; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatSongs` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `Label` | `StemboxStatKey` | `Text=Playlists; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatPlaylists` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `Label` | `StemboxStatKey` | `Text=Date; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatDate` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `Label` | `StemboxStatKey` | `Text=Total; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatTotal` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `WidgetGroup` | `StemboxHDivider` | `Size=0me,1f` | `[Stembox],src_library_loaded → visible NOT` |
| `WidgetGroup` | `StemboxStatRow` | `Layout=horizontal; Size=0me,46f` | `[Stembox],src_library_loaded → visible NOT` |
| `Label` | `StemboxStatKey` | `Text=Available; Size=0me,46f; Alignment=left` | none |
| `Label` | `StemboxStatAvail` | `Text=—; Size=0me,46f; Alignment=right` | none |
| `WidgetGroup` | `StemboxHDivider` | `Size=0me,1f` | `[Stembox],src_analyzing → visible` |
| `WidgetGroup` | `StemboxProgressRow` | `Layout=vertical; Size=0me,52f` | `[Stembox],src_analyzing → visible` |
| `Label` | `StemboxStatKey` | `Text=Loading; Size=0me,34f; Alignment=left` | none |
| `Label` | `StemboxProgressText` | `Text=—; Size=0me,34f; Alignment=right` | none |
| `WidgetGroup` | `StemboxProgressTrack` | `Layout=horizontal; Size=0me,10f` | none |
| `WidgetGroup` | `StemboxProgressFill` | `Size=0f,10f` | none |
| `WidgetGroup` | `StemboxProgressRest` | `SizePolicy=me,me` | none |
| `PushButton` | `StemboxEjectBtn` | `SizePolicy=me,me; NumberStates=1; State.Text=Eject drive` | `[Stembox],src_eject_drive → left` |
| `PushButton` | `StemboxEjectBtn` | `SizePolicy=me,me; NumberStates=1; State.Text=Open library` | `[Stembox],stembox_lib_manage → left` |
| `WidgetGroup` | `StemboxTracksPane` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],show_tracks → visible` |
| `WidgetGroup` | `StemboxResultsWrap` | `Layout=vertical; SizePolicy=me,me; MinimumSize=0,0` | none |
| `WidgetGroup` | `StemboxResultsInner` | `Layout=vertical; SizePolicy=me,me; MinimumSize=0,0` | none |
| `Library` | `[unnamed]` | `ShowButtonText=false; TrackTableBackgroundColorOpacity=0; SignalColor={SignalColor_12}` | none |
| `WidgetGroup` | `StemboxKeyboard` | `Layout=vertical; Size=0me,200max` | `[Stembox],search_mode → visible` |
| `WidgetGroup` | `StemboxKeyboardBottomPad` | `Size=0me,6f` | `[Stembox],search_mode → visible` |
| `WidgetGroup` | `StemboxSourceHeaderRow` | `Layout=horizontal; Size=0me,70f; SizePolicy=me,f` | none |
| `PushButton` | `StemboxBackButton` | `Size=34f,34f; MaximumSize=34,34; NumberStates=1` | `[Stembox],stems_page_close → left` |
| `Label` | `StemboxSourceHeader` | `Text=STEMS MANAGER; SizePolicy=me,me; Alignment=left` | none |
| `StemboxStemsDest` | `StemboxStemsDest` | `Size=0me,70f; SizePolicy=me,f` | none |
| `StemboxStems` | `StemboxStemsList` | `SizePolicy=me,me` | none |
All direct bindings, including anonymous wrappers: `[Stembox],nav_hidden → visible NOT`; `[Stembox],at_root → visible NOT`; `[Stembox],search_mode → visible NOT`; `[Stembox],go_source → LeftButton`; `[Stembox],back_hidden → visible NOT`; `[Stembox],header_back → LeftButton`; `[Stembox],nav_mode → currentpage`; `[Stembox],search_mode → visible`; `[Stembox],nav_active_history → visible`; `[Stembox],past_sessions_open → visible NOT`; `[Stembox],go_past_sessions → LeftButton`; `[Stembox],font_state → currentpage`; `[Stembox],font_cycle → LeftButton`; `[Stembox],stems_page → visible NOT`; `[Stembox],show_tracks → visible NOT`; `[Stembox],at_root → visible`; `[Stembox],source_diag_open → LeftButton`; `[Stembox],stems_page_open → LeftButton`; `[Stembox],mix_mode → visible NOT`; `[Stembox],playlist_mode → visible NOT`; `[Stembox],src_library_loaded → visible NOT`; `[Stembox],src_analyzing → visible`; `[Stembox],src_device_loaded → visible`; `[Stembox],src_eject_drive → LeftButton`; `[Stembox],src_library_loaded → visible`; `[Stembox],stembox_lib_manage → LeftButton`; `[Stembox],show_tracks → visible`; `[Stembox],keyboard_azerty → visible`; `[Stembox],keyboard_azerty → visible NOT`; `[Stembox],stems_page → visible`; `[Stembox],stems_page_close → LeftButton`.
Template instantiations: `stembox_navitem.xml: Label=ARTIST, IconGrey=skins:LateNight/stembox_nav_artist.svg, IconGreen=skins:LateNight/stembox_nav_artist_on.svg, GoControl=[Stembox],go_artist, ActiveControl=[Stembox],nav_active_artist, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=ALBUM, IconGrey=skins:LateNight/stembox_nav_album.svg, IconGreen=skins:LateNight/stembox_nav_album_on.svg, GoControl=[Stembox],go_album, ActiveControl=[Stembox],nav_active_album, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=TRACK, IconGrey=skins:LateNight/stembox_nav_track.svg, IconGreen=skins:LateNight/stembox_nav_track_on.svg, GoControl=[Stembox],go_track, ActiveControl=[Stembox],nav_active_track, LShift=0f, RShift=4f`; `stembox_navitem.xml: Label=KEY, IconGrey=skins:LateNight/stembox_nav_key.svg, IconGreen=skins:LateNight/stembox_nav_key_on.svg, GoControl=[Stembox],go_key, ActiveControl=[Stembox],nav_active_key, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=PLAYLIST, IconGrey=skins:LateNight/stembox_nav_playlist.svg, IconGreen=skins:LateNight/stembox_nav_playlist_on.svg, GoControl=[Stembox],go_playlist, ActiveControl=[Stembox],nav_active_playlist, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=HISTORY, IconGrey=skins:LateNight/stembox_nav_history.svg, IconGreen=skins:LateNight/stembox_nav_history_on.svg, GoControl=[Stembox],go_history, ActiveControl=[Stembox],nav_active_history, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=MATCHING, IconGrey=skins:LateNight/stembox_nav_matching.svg, IconGreen=skins:LateNight/stembox_nav_matching_on.svg, GoControl=[Stembox],go_matching, ActiveControl=[Stembox],nav_active_matching, LShift=0f, RShift=0f`; `stembox_navitem.xml: Label=SEARCH, IconGrey=skins:LateNight/stembox_nav_search.svg, IconGreen=skins:LateNight/stembox_nav_search_on.svg, GoControl=[Stembox],go_search, ActiveControl=[Stembox],search_mode, LShift=0f, RShift=0f`; `stembox_key.xml: K=A, C=[Stembox],kb_a`; `stembox_key.xml: K=Z, C=[Stembox],kb_z`; `stembox_key.xml: K=E, C=[Stembox],kb_e`; `stembox_key.xml: K=R, C=[Stembox],kb_r`; `stembox_key.xml: K=T, C=[Stembox],kb_t`; `stembox_key.xml: K=Y, C=[Stembox],kb_y`; `stembox_key.xml: K=U, C=[Stembox],kb_u`; `stembox_key.xml: K=I, C=[Stembox],kb_i`; `stembox_key.xml: K=O, C=[Stembox],kb_o`; `stembox_key.xml: K=P, C=[Stembox],kb_p`; `stembox_key_back.xml: C=[Stembox],kb_back`; `stembox_key.xml: K=Q, C=[Stembox],kb_q`; `stembox_key.xml: K=S, C=[Stembox],kb_s`; `stembox_key.xml: K=D, C=[Stembox],kb_d`; `stembox_key.xml: K=F, C=[Stembox],kb_f`; `stembox_key.xml: K=G, C=[Stembox],kb_g`; `stembox_key.xml: K=H, C=[Stembox],kb_h`; `stembox_key.xml: K=J, C=[Stembox],kb_j`; `stembox_key.xml: K=K, C=[Stembox],kb_k`; `stembox_key.xml: K=L, C=[Stembox],kb_l`; `stembox_key_wide.xml: K=CLEAR, C=[Stembox],kb_clear`; `stembox_key.xml: K=123, C=[Stembox],kb_123`; `stembox_key.xml: K=W, C=[Stembox],kb_w`; `stembox_key.xml: K=X, C=[Stembox],kb_x`; `stembox_key.xml: K=C, C=[Stembox],kb_c`; `stembox_key.xml: K=V, C=[Stembox],kb_v`; `stembox_key.xml: K=B, C=[Stembox],kb_b`; `stembox_key.xml: K=N, C=[Stembox],kb_n`; `stembox_key.xml: K=M, C=[Stembox],kb_m`; `stembox_key_wide.xml: K=SPACE, C=[Stembox],kb_space`.
### stembox_bj_row.xml
Source: [stembox_bj_row.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_bj_row.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `StemboxBjRow` | `StemboxBjRow` | `SizePolicy=me,me; N1={N1}; N2={N2}; Deck={Deck}; PadBase={PadBase}` | none |
### stembox_bl_row.xml
Source: [stembox_bl_row.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_bl_row.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `StemboxBlRow` | `StemboxBlRow` | `SizePolicy=me,me; Group=[Channel{chanNum}]; Deck={Deck}; Saver={Saver}; L1={L1}; S1={S1}; L2={L2}; S2={S2}; L3={L3}; S3={S3}; L4={L4}; S4={S4}` | none |
### stembox_cdj_deck.xml
Source: [stembox_cdj_deck.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_cdj_deck.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxCdjDeck` | `Layout=vertical; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxDeckLargeTopGap` | `SizePolicy=me,me` | `[Stembox],vue_large_compacte → visible NOT` |
| `WidgetGroup` | `StemboxDeckInfo` | `Layout=horizontal; Size=0me,52f` | none |
| `CoverArt` | `StemboxDeckCover` | `TooltipId=coverart; Size=44f,44f; Group=[Channel{chanNum}]; DefaultCover=skins:LateNight/stembox_disk_ph.png` | none |
| `PushButton` | `StemboxDeckNote` | `Size=18f,18f; NumberStates=2` | `[Channel{chanNum}],stem_count → state NOTNOT; [Channel{chanNum}],eq_stems → visible NOT` |
| `PushButton` | `StemboxDeckNote` | `Size=18f,18f; NumberStates=1` | `[Channel{chanNum}],eq_stems → visible` |
| `TrackProperty` | `StemboxDeckTitle` | `TooltipId=track_title; SizePolicy=me,f; Size=0me,26f; MinimumSize=0,0; Property=titleInfo; Alignment=left; Elide=right; Channel={chanNum}` | none |
| `TrackProperty` | `StemboxDeckArtist` | `TooltipId=track_artist; SizePolicy=me,f; Size=0me,20f; MinimumSize=0,0; Property=artist; Alignment=left; Elide=right; Channel={chanNum}` | none |
| `WidgetGroup` | `StemboxDeckStatHit` | `Layout=stacked; Size=58f,46f` | none |
| `PushButton` | `StemboxBpmDiagHit` | `Size=58f,46f; NumberStates=1` | `[Stembox],grid_diag → left` |
| `WidgetGroup` | `StemboxDeckStat` | `Layout=vertical; Size=58f,46f` | none |
| `Label` | `StemboxDeckStatCap` | `Text=BPM; Size=0me,12f; Alignment=center` | none |
| `StemboxReadout` | `StemboxDeckStatVal` | `Size=0me,26f; Alignment=center; Mode=filebpm; SmallRatio=1.0; Channel={chanNum}` | none |
| `WidgetGroup` | `StemboxDeckStat` | `Layout=vertical; Size=50f,46f` | none |
| `Label` | `StemboxDeckStatCap` | `Text=KEY; Size=0me,12f; Alignment=center` | none |
| `Label` | `StemboxDeckStatVal` | `Text=#; Size=0me,26f; Alignment=center` | `[Channel{chanNum}],track_loaded → visible NOT` |
| `Key` | `StemboxDeckStatVal` | `TooltipId=visual_key; Size=0me,26f; Alignment=center; UseFileKey=true; Channel={chanNum}` | `[Channel{chanNum}],key → state; [Channel{chanNum}],track_loaded → visible` |
| `WidgetGroup` | `StemboxDeckStat` | `Layout=vertical; Size=62f,46f` | none |
| `Label` | `StemboxDeckStatCap` | `Text=TIME; Size=0me,12f; Alignment=center` | none |
| `StemboxReadout` | `StemboxDeckStatVal` | `Size=0me,26f; Alignment=center; Mode=duration; Channel={chanNum}` | none |
| `PushButton` | `StemboxGridEditBtn` | `Size=24f,24f; NumberStates=2; State.Text=i` | `[Stembox],grid_edit{chanNum} → left; [Channel{chanNum}],track_loaded → enabled` |
| `WidgetGroup` | `StemboxDeckWaveforms` | `Layout=horizontal; SizePolicy=me,me` | `[Stembox],mix_mode → visible` |
| `StemboxMultiFx` | `StemboxMultiFxTabs` | `Size=0me,20f; Deck={chanNum}` | none |
| `Visual` | `StemboxWfThin` | `TooltipId=waveform_display; Size=0me,64f; Channel={otherNum}; StemboxLeftFade=true; StemboxPlayMarkerYOffset=1; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; BeatColor=#000000; StemboxZoomGroup=[Channel{chanNum}]` | `[Channel{chanNum}],track_loaded → visible` |
| `StemboxZoomGroup` | `[unnamed]` | `Variable=[Channel{chanNum}]` | none |
| `StemboxGridPreview` | `StemboxGridPreview` | `SizePolicy=me,f; Size=0me,64f; Deck={chanNum}` | `[Channel{chanNum}],track_loaded → visible` |
| `WidgetGroup` | `StemboxWfTopPad` | `Size=0me,1f` | `[Channel{chanNum}],track_loaded → visible` |
| `Visual` | `StemboxWfBig` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel={chanNum}; StemboxZoomHold=true; StemboxCueMargin=12; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; BeatColor=#000000; DefaultMark=top&#124;hcenter #FF0000 #000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff9800 0.60 #ffffff 0.20` | `[Channel{chanNum}],track_loaded → visible` |
| `PushButton` | `StemboxDeckLogo` | `Size=256f,43f; NumberStates=1` | none |
| `WidgetGroup` | `StemboxWfBottomPad` | `Size=0me,32f` | none |
| `WidgetGroup` | `StemboxDeckLargeGap` | `SizePolicy=me,f; Size=0me,10f` | `[Stembox],mix_mode → visible NOT` |
| `WidgetGroup` | `StemboxDeckBar` | `Layout=horizontal; Size=0me,68f` | none |
| `WidgetGroup` | `StemboxPlayerBoxIdle` | `Layout=vertical; Size=52f,58f` | `[Stembox],deck_live{chanNum} → visible NOT` |
| `Label` | `StemboxPlayerCapIdle` | `Text=PLAYER; Size=0me,14f; Alignment=center` | none |
| `Label` | `StemboxPlayerNumIdle` | `Text={chanNum}; Size=0me,40f; Alignment=center` | none |
| `WidgetGroup` | `StemboxPlayerBox` | `Layout=vertical; Size=52f,58f` | `[Stembox],deck_live{chanNum} → visible` |
| `Label` | `StemboxPlayerCap` | `Text=PLAYER; Size=0me,14f; Alignment=center` | none |
| `PushButton` | `StemboxCueWave` | `SizePolicy=me,me; NumberStates=2` | `[Channel{chanNum}],cue_indicator → state` |
| `Label` | `StemboxPlayerNum` | `Text={chanNum}; SizePolicy=min,me; Alignment=center` | none |
| `Label` | `StemboxBarCap` | `Text=TRACK; Size=0me,14f; Alignment=center` | none |
| `StemboxReadout` | `StemboxTrackNo` | `Size=0me,40f; Alignment=center; Mode=trackno; Channel={chanNum}` | none |
| `PushButton` | `StemboxCueBtn` | `Size=46f,22f; NumberStates=2; State.Text=CUE` | `[Channel{chanNum}],pfl → left; [Channel{chanNum}],pfl → state` |
| `PushButton` | `StemboxSyncBtn` | `Size=46f,22f; NumberStates=2; State.Text=SYNC` | `[Channel{chanNum}],stembox_follower → state` |
| `StemboxTimeCap` | `StemboxBarCap` | `Size=0me,11f; Group=[Channel{chanNum}]` | none |
| `StemboxReadout` | `StemboxRemain` | `Size=0me,47f; Alignment=left; Mode=time; SmallRatio=0.58; Channel={chanNum}` | none |
| `Label` | `StemboxSlipTag` | `Text=SLIP; Size=40f,17f; Alignment=center` | `[Channel{chanNum}],stembox_slip_mode → visible` |
| `Label` | `StemboxSlipTag` | `Text=SLIP; Size=40f,17f; Alignment=center` | none |
| `Overview` | `StemboxOverview` | `TooltipId=waveform_overview; SizePolicy=me,f; Size=0me,48f; Channel={chanNum}; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;left #FF0000 #000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff9800 0.60 #ffffff 0.20` | `[Channel{chanNum}],playposition → state; [Channel{chanNum}],track_loaded → visible` |
| `StemboxRateRange` | `StemboxRateRangeBtn` | `Size=56f,62f; Deck={chanNum}` | none |
| `WidgetGroup` | `StemboxBpmStack` | `Layout=stacked; Size=120f,65f` | none |
| `WidgetGroup` | `StemboxBpmBoxIdle` | `Layout=vertical; Size=120f,65f` | `[Stembox],deck_live{chanNum} → visible NOT` |
| `Label` | `StemboxBarCapIdle` | `Text=BPM; Size=0me,11f; Alignment=left` | none |
| `StemboxReadout` | `StemboxBpmBigIdle` | `Size=0me,48f; Alignment=right; Mode=bpm; SmallRatio=0.58; Channel={chanNum}` | none |
| `WidgetGroup` | `StemboxBpmBox` | `Layout=vertical; Size=120f,65f` | `[Stembox],deck_live{chanNum} → visible` |
| `Label` | `StemboxBarCap` | `Text=BPM; Size=0me,11f; Alignment=left` | `[Channel{chanNum}],stembox_follower → visible NOT` |
| `Label` | `StemboxBarCap` | `Text=BEAT SYNC; Size=0me,11f; Alignment=left` | `[Channel{chanNum}],stembox_follower → visible` |
| `StemboxReadout` | `StemboxBpmBig` | `Size=0me,48f; Alignment=right; Mode=bpm; SmallRatio=0.58; Channel={chanNum}` | none |
All direct bindings, including anonymous wrappers: `[Stembox],mix_mode → visible NOT`; `[Stembox],vue_large_compacte → visible NOT`; `[Channel{chanNum}],stem_count → state NOTNOT`; `[Channel{chanNum}],eq_stems → visible NOT`; `[Channel{chanNum}],eq_stems → visible`; `[Stembox],grid_diag → LeftButton`; `[Channel{chanNum}],track_loaded → visible NOT`; `[Channel{chanNum}],key → state`; `[Channel{chanNum}],track_loaded → visible`; `[Stembox],grid_edit{chanNum} → LeftButton`; `[Channel{chanNum}],track_loaded → enabled`; `[Stembox],mix_mode → visible`; `[Stembox],beat_fx_multi → visible`; `[Channel{otherNum}],track_loaded → visible`; `[Stembox],preview_style → visible NOT`; `[Channel{chanNum}],stembox_menu_dessus → visible NOT`; `[Stembox],preview_style → visible`; `[Stembox],deck_live{chanNum} → visible NOT`; `[Stembox],deck_live{chanNum} → visible`; `[Channel{chanNum}],cue_indicator → state`; `[Channel{chanNum}],pfl → LeftButton`; `[Channel{chanNum}],pfl → state`; `[Channel{chanNum}],stembox_follower → state`; `[Stembox],vue_large_compacte → visible`; `[Channel{chanNum}],stembox_slip_mode → visible`; `[Channel{chanNum}],playposition → state`; `[Channel{chanNum}],stembox_follower → visible NOT`; `[Channel{chanNum}],stembox_follower → visible`.
Template instantiations: `stembox_gridedit.xml: chanNum={chanNum}`.
### stembox_deck_bottom.xml
Source: [stembox_deck_bottom.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_deck_bottom.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxDeckBottom` | `Layout=vertical; SizePolicy=me,min` | none |
| `WidgetGroup` | `StemboxDeckBar2` | `Layout=horizontal; Size=0me,52f` | `[Stembox],vue_large_compacte → visible NOT` |
| `PushButton` | `StemboxToggle` | `Size=60f,20f; NumberStates=2; State.Text=MT` | `[Channel{chanNum}],keylock → left; [Channel{chanNum}],keylock → state` |
| `PushButton` | `StemboxQuantize` | `Size=60f,20f; NumberStates=2; State.Text=QUANTIZE` | `[Channel{chanNum}],quantize → left; [Channel{chanNum}],quantize → state` |
| `Overview` | `StemboxOverview` | `TooltipId=waveform_overview; SizePolicy=me,f; Size=0me,48f; Channel={chanNum}; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;left #FF0000 #000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff9800 0.60 #ffffff 0.20` | `[Channel{chanNum}],playposition → state; [Channel{chanNum}],track_loaded → visible` |
| `Label` | `StemboxNotLoadedSmall` | `Text=Not Loaded.; SizePolicy=me,f; Size=0me,48f; Alignment=center` | `[Channel{chanNum}],track_loaded → visible NOT` |
| `StemboxFx` | `StemboxFxBlock` | `Size=54f,48f; Deck={chanNum}; ColorFxOnDecks=1,3` | none |
| `WidgetGroup` | `StemboxPadsSlim` | `Layout=vertical; Size=0me,20f; SizePolicy=me,f` | `[Stembox],vue_large_compacte → visible` |
| `StemboxStemPads` | `[unnamed]` | `SizePolicy=me,me; Deck={chanNum}` | none |
| `StemboxDrums` | `[unnamed]` | `SizePolicy=me,me; Deck={chanNum}` | none |
| `WidgetGroup` | `StemboxPads` | `Layout=vertical; Size=0me,96f` | `[Stembox],vue_large_compacte → visible NOT` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_hotcue_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_beatjump_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_sampler_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_padfx_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_padfx2_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_beatloop_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_keyshift_{chanNum} → visible` |
| `WidgetGroup` | `StemboxPadPage` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],pad_keyboard_{chanNum} → visible` |
All direct bindings, including anonymous wrappers: `[Stembox],vue_large_compacte → visible NOT`; `[Channel{chanNum}],keylock → LeftButton`; `[Channel{chanNum}],keylock → state`; `[Channel{chanNum}],quantize → LeftButton`; `[Channel{chanNum}],quantize → state`; `[Channel{chanNum}],playposition → state`; `[Channel{chanNum}],track_loaded → visible`; `[Channel{chanNum}],track_loaded → visible NOT`; `[Stembox],vue_large_compacte → visible`; `[Stembox],pad_hotcue_{chanNum} → visible`; `[Stembox],pad_beatjump_{chanNum} → visible`; `[Stembox],pad_sampler_{chanNum} → visible`; `[Stembox],pad_keyboard_{chanNum} → visible`; `[Stembox],pad_beatloop_{chanNum} → visible`; `[Stembox],pad_keyshift_{chanNum} → visible`; `[Stembox],pad_padfx2_{chanNum} → visible`; `[Stembox],pad_padfx_{chanNum} → visible`.
Template instantiations: `stembox_pad_slim.xml: group=[Channel{chanNum}], n=1, PK=padpress_{chanNum}_1`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=2, PK=padpress_{chanNum}_2`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=3, PK=padpress_{chanNum}_3`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=4, PK=padpress_{chanNum}_4`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=5, PK=padpress_{chanNum}_5`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=6, PK=padpress_{chanNum}_6`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=7, PK=padpress_{chanNum}_7`; `stembox_pad_slim.xml: group=[Channel{chanNum}], n=8, PK=padpress_{chanNum}_8`; `stembox_bj_row.xml: N1=4, N2=8, Deck={chanNum}, PadBase=1`; `stembox_bj_row.xml: N1=16, N2=32, Deck={chanNum}, PadBase=5`; `stembox_samp_row.xml: Deck={chanNum}, PadBase=1`; `stembox_samp_row.xml: Deck={chanNum}, PadBase=5`; `stembox_bl_row.xml: group=[Channel{chanNum}], Deck={chanNum}, Saver=false, L1=1/4, S1=0.25, L2=1/2, S2=0.5, L3=1, S3=1, L4=2, S4=2`; `stembox_bl_row.xml: group=[Channel{chanNum}], Deck={chanNum}, Saver=true, L1=4, S1=4, L2=8, S2=8, L3=, S3=, L4=, S4=`; `stembox_ks_row.xml: Deck={chanNum}, PadBase=1, L1=0, K1=zero, L2=+1, K2=up, L3=+2, K3=up, L4=+3, K4=up`; `stembox_ks_row.xml: Deck={chanNum}, PadBase=5, L1=-3, K1=down, L2=-2, K2=down, L3=-1, K3=down, L4=0, K4=zero`; `stembox_padfx_cells.xml: chanNum={chanNum}`; `stembox_pad.xml: group=[Channel{chanNum}], n=1, L=A, PK=padpress_{chanNum}_1`; `stembox_pad.xml: group=[Channel{chanNum}], n=2, L=B, PK=padpress_{chanNum}_2`; `stembox_pad.xml: group=[Channel{chanNum}], n=3, L=C, PK=padpress_{chanNum}_3`; `stembox_pad.xml: group=[Channel{chanNum}], n=4, L=D, PK=padpress_{chanNum}_4`; `stembox_pad.xml: group=[Channel{chanNum}], n=5, L=E, PK=padpress_{chanNum}_5`; `stembox_pad.xml: group=[Channel{chanNum}], n=6, L=F, PK=padpress_{chanNum}_6`; `stembox_pad.xml: group=[Channel{chanNum}], n=7, L=G, PK=padpress_{chanNum}_7`; `stembox_pad.xml: group=[Channel{chanNum}], n=8, L=H, PK=padpress_{chanNum}_8`.
### stembox_gridedit.xml
Source: [stembox_gridedit.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_gridedit.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxGridEditPanel` | `Layout=vertical; SizePolicy=f,f; Size=154f,287f` | none |
| `Label` | `StemboxGridEditTitle` | `Text=BEAT GRID EDIT; Size=0me,12f; Alignment=center` | none |
| `WidgetGroup` | `StemboxGridEditDiv` | `Size=0me,1f` | none |
| `Label` | `StemboxGridEditCap` | `Text=TEMPO; Size=0me,20f; Alignment=left` | none |
| `StemboxReadout` | `StemboxGridEditBpm` | `Size=0me,38f; Alignment=center; Mode=filebpm; SmallRatio=1.0; Channel={chanNum}` | none |
| `PushButton` | `StemboxGridEditKey` | `Size=0me,0me; SizePolicy=me,me; NumberStates=1; StemboxRepeat=true; State.Text=−` | `[Channel{chanNum}],stembox_grid_bpm_down → left` |
| `PushButton` | `StemboxGridEditKey` | `Size=0me,0me; SizePolicy=me,me; NumberStates=1; StemboxRepeat=true; State.Text=+` | `[Channel{chanNum}],stembox_grid_bpm_up → left` |
| `Label` | `StemboxGridEditCap` | `Text=POSITION; Size=0me,22f; Alignment=left` | none |
| `PushButton` | `StemboxGridEditKey` | `Size=0me,0me; SizePolicy=me,me; NumberStates=1; StemboxRepeat=true; State.Text=◀` | `[Channel{chanNum}],beats_translate_earlier → left` |
| `PushButton` | `StemboxGridEditKey` | `Size=0me,0me; SizePolicy=me,me; NumberStates=1; StemboxRepeat=true; State.Text=▶` | `[Channel{chanNum}],beats_translate_later → left` |
| `Label` | `StemboxGridEditCap` | `Text=1ST TIME; Size=0me,22f; Alignment=left` | none |
| `PushButton` | `StemboxGridEditKeyRed` | `StemboxHoverText=#000000; Size=0me,0me; SizePolicy=me,me; NumberStates=1; State.Text=◀` | `[Channel{chanNum}],stembox_grid_first_earlier → left` |
| `PushButton` | `StemboxGridEditKeyRed` | `StemboxHoverText=#000000; Size=0me,0me; SizePolicy=me,me; NumberStates=1; State.Text=▶` | `[Channel{chanNum}],stembox_grid_first_later → left` |
All direct bindings, including anonymous wrappers: `[Stembox],grid_edit{chanNum} → visible`; `[Channel{chanNum}],stembox_grid_bpm_down → LeftButton`; `[Channel{chanNum}],stembox_grid_bpm_up → LeftButton`; `[Channel{chanNum}],beats_translate_earlier → LeftButton`; `[Channel{chanNum}],beats_translate_later → LeftButton`; `[Channel{chanNum}],stembox_grid_first_earlier → LeftButton`; `[Channel{chanNum}],stembox_grid_first_later → LeftButton`.
### stembox_key.xml
Source: [stembox_key.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_key.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `PushButton` | `StemboxKey` | `SizePolicy=me,me; NumberStates=1; State.Text={K}` | `{C} → left` |
All direct bindings, including anonymous wrappers: `{C} → LeftButton`.
### stembox_key_back.xml
Source: [stembox_key_back.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_key_back.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `PushButton` | `StemboxKeyBack` | `SizePolicy=me,me; NumberStates=1` | `{C} → left` |
All direct bindings, including anonymous wrappers: `{C} → LeftButton`.
### stembox_key_wide.xml
Source: [stembox_key_wide.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_key_wide.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `PushButton` | `StemboxKeyWide` | `SizePolicy=me,me; NumberStates=1; State.Text={K}` | `{C} → left` |
All direct bindings, including anonymous wrappers: `{C} → LeftButton`.
### stembox_ks_row.xml
Source: [stembox_ks_row.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_ks_row.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `StemboxKsRow` | `StemboxKsRow` | `SizePolicy=me,me; Deck={Deck}; PadBase={PadBase}; L1={L1}; K1={K1}; L2={L2}; K2={K2}; L3={L3}; K3={K3}; L4={L4}; K4={K4}` | none |
### stembox_large_wf.xml
Source: [stembox_large_wf.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_large_wf.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxWfTagLive` | `Layout=vertical; Size=30f,0me; SizePolicy=f,me` | `[Stembox],deck_live{chanNum} → visible` |
| `Label` | `StemboxWfTagNum` | `Text={chanNum}; SizePolicy=me,me; Alignment=center` | none |
| `WidgetGroup` | `StemboxWfTagIdle` | `Layout=vertical; Size=30f,0me; SizePolicy=f,me` | `[Stembox],deck_live{chanNum} → visible NOT` |
| `Label` | `StemboxWfTagNumIdle` | `Text={chanNum}; SizePolicy=me,me; Alignment=center` | none |
| `Visual` | `{wfName}` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel={chanNum}; StemboxZoomHold=true; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;hcenter#FF0000#000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff98000.60 #ffffff0.20` | none |
All direct bindings, including anonymous wrappers: `[Stembox],deck_live{chanNum} → visible`; `[Stembox],deck_live{chanNum} → visible NOT`.
### stembox_mix_active.xml
Source: [stembox_mix_active.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_mix_active.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxSplitLibDock` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],lib_left → visible` |
| `SingletonContainer` | `LibrarySingleton` | `SizePolicy=me,me; MinimumSize=0,0` | none |
| `WidgetGroup` | `StemboxCdjDivider` | `Size=40f,0me; SizePolicy=f,me; Layout=vertical` | none |
| `WidgetGroup` | `StemboxSplitLibDock` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],lib_right → visible` |
All direct bindings, including anonymous wrappers: `[Stembox],lib_left → visible NOT`; `[Stembox],lib_left → visible`; `[Stembox],lib_right → visible NOT`; `[Stembox],lib_right → visible`.
Template instantiations: `stembox_cdj_deck.xml: chanNum=1, otherNum=2`; `stembox_deck_bottom.xml: chanNum=1, sg1=[Sampler1], sg2=[Sampler2], sg3=[Sampler3], sg4=[Sampler4], sg5=[Sampler5], sg6=[Sampler6], sg7=[Sampler7], sg8=[Sampler8]`; `stembox_cdj_deck.xml: chanNum=2, otherNum=1`; `stembox_deck_bottom.xml: chanNum=2, sg1=[Sampler9], sg2=[Sampler10], sg3=[Sampler11], sg4=[Sampler12], sg5=[Sampler13], sg6=[Sampler14], sg7=[Sampler15], sg8=[Sampler16]`.
### stembox_mix_large.xml
Source: [stembox_mix_large.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_mix_large.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetStack` | `[unnamed]` | `SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxLargeWaveforms` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],show_preferences → visible NOT` |
| `WidgetGroup` | `StemboxGridEditColumn` | `Layout=vertical; SizePolicy=f,me; Size=160f,0me` | `[Stembox],grille_ouverte → visible` |
| `WidgetGroup` | `StemboxLargeMultiFxRow4` | `Layout=horizontal; Size=0me,24f; SizePolicy=me,f` | `[Stembox],beat_fx_multi → visible` |
| `StemboxMultiFx` | `StemboxMultiFxTabsLarge` | `Size=0me,20f; Deck=1` | none |
| `StemboxMultiFx` | `StemboxMultiFxTabsLarge` | `Size=0me,20f; Deck=2` | none |
| `WidgetGroup` | `StemboxLargeRow4` | `Layout=horizontal; Size=0me,150f; SizePolicy=me,f` | none |
| `WidgetGroup` | `StemboxLargeCenterBar` | `Size=10f,0me` | none |
| `WidgetGroup` | `StemboxLargeCenterFill` | `Size=20f,0me` | none |
| `WidgetGroup` | `StemboxLargeTopPad` | `Size=0me,22f; SizePolicy=me,f` | none |
| `Visual` | `StemboxLargeWf1` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel=1; StemboxZoomHold=true; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;hcenter#FF0000#000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff98000.60 #ffffff0.20` | none |
| `Visual` | `StemboxLargeWf1Alt` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel=3; StemboxZoomHold=true; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;hcenter#FF0000#000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff98000.60 #ffffff0.20` | none |
| `WidgetGroup` | `StemboxLargeLogoRow` | `Layout=vertical; SizePolicy=me,f; Size=0me,43f` | `[Channel1],track_loaded → visible NOT` |
| `PushButton` | `StemboxDeckLogo` | `Size=256f,43f; NumberStates=1` | none |
| `Visual` | `StemboxLargeWf2` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel=2; StemboxZoomHold=true; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;hcenter#FF0000#000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff98000.60 #ffffff0.20` | none |
| `Visual` | `StemboxLargeWf2Alt` | `TooltipId=waveform_display; SizePolicy=me,me; Size=0me,0me; Channel=4; StemboxZoomHold=true; BgColor=#000000; SignalColor=#cc7a1f; SignalLowColor=#054CE3; SignalMidColor=#DE7002; SignalHighColor=#FAF4E8; DefaultMark=top&#124;hcenter#FF0000#000000; MarkRange=loop_start_position loop_end_position loop_enabled #ff98000.60 #ffffff0.20` | none |
| `WidgetGroup` | `StemboxLargeMultiFxRow` | `Layout=horizontal; Size=0me,24f; SizePolicy=me,f` | `[Stembox],beat_fx_multi → visible` |
| `WidgetGroup` | `StemboxLargeRow` | `Layout=horizontal; Size=0me,130f; SizePolicy=me,f` | none |
| `WidgetGroup` | `StemboxLargeCenterFill` | `SizePolicy=me,me` | none |
| `SingletonContainer` | `LibrarySingleton` | `SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxLargeRow4` | `Layout=horizontal; Size=0me,150f; SizePolicy=me,f` | `[Stembox],large_4decks → visible` |
| `WidgetGroup` | `StemboxBottomRow` | `Layout=horizontal; Size=0me,148f; SizePolicy=me,f` | `[Stembox],large_4decks → visible NOT` |
All direct bindings, including anonymous wrappers: `[Stembox],large_4decks → visible`; `[Stembox],show_preferences → visible NOT`; `[Stembox],grille_ouverte → visible`; `[Stembox],beat_fx_multi → visible`; `[Stembox],large_4decks → visible NOT`; `[Stembox],deck_active3 → visible NOT`; `[Stembox],deck_active3 → visible`; `[Channel1],track_loaded → visible NOT`; `[Channel2],track_loaded → visible NOT`; `[Stembox],deck_active4 → visible NOT`; `[Stembox],deck_active4 → visible`.
Template instantiations: `stembox_large_wf.xml: chanNum=4, wfName=StemboxLargeWf4`; `stembox_large_wf.xml: chanNum=1, wfName=StemboxLargeWf1`; `stembox_large_wf.xml: chanNum=2, wfName=StemboxLargeWf2`; `stembox_large_wf.xml: chanNum=3, wfName=StemboxLargeWf3`; `stembox_gridedit.xml: chanNum=1`; `stembox_gridedit.xml: chanNum=2`; `stembox_gridedit.xml: chanNum=3`; `stembox_gridedit.xml: chanNum=4`; `stembox_cdj_deck.xml: chanNum=1, otherNum=2`; `stembox_deck_bottom.xml: chanNum=1, sg1=[Sampler1], sg2=[Sampler2], sg3=[Sampler3], sg4=[Sampler4], sg5=[Sampler5], sg6=[Sampler6], sg7=[Sampler7], sg8=[Sampler8]`; `stembox_cdj_deck.xml: chanNum=2, otherNum=1`; `stembox_deck_bottom.xml: chanNum=2, sg1=[Sampler9], sg2=[Sampler10], sg3=[Sampler11], sg4=[Sampler12], sg5=[Sampler13], sg6=[Sampler14], sg7=[Sampler15], sg8=[Sampler16]`; `stembox_cdj_deck.xml: chanNum=3, otherNum=4`; `stembox_cdj_deck.xml: chanNum=4, otherNum=3`; `stembox_deck_bottom.xml: chanNum=3, sg1=[Sampler1], sg2=[Sampler2], sg3=[Sampler3], sg4=[Sampler4], sg5=[Sampler5], sg6=[Sampler6], sg7=[Sampler7], sg8=[Sampler8]`; `stembox_deck_bottom.xml: chanNum=4, sg1=[Sampler9], sg2=[Sampler10], sg3=[Sampler11], sg4=[Sampler12], sg5=[Sampler13], sg6=[Sampler14], sg7=[Sampler15], sg8=[Sampler16]`.
### stembox_mix_split.xml
Source: [stembox_mix_split.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_mix_split.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxSplitLibDock` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],lib_left → visible` |
| `SingletonContainer` | `LibrarySingleton` | `SizePolicy=me,me; MinimumSize=0,0` | none |
| `WidgetGroup` | `StemboxCdjDivider` | `Size=40f,0me; SizePolicy=f,me; Layout=vertical` | none |
| `WidgetGroup` | `StemboxSplitLibDock` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],lib_right → visible` |
All direct bindings, including anonymous wrappers: `[Stembox],lib_left → visible NOT`; `[Stembox],deck_active3 → visible NOT`; `[Stembox],deck_active3 → visible`; `[Stembox],lib_left → visible`; `[Stembox],lib_right → visible NOT`; `[Stembox],deck_active4 → visible NOT`; `[Stembox],deck_active4 → visible`; `[Stembox],lib_right → visible`.
Template instantiations: `stembox_cdj_deck.xml: chanNum=1, otherNum=2`; `stembox_cdj_deck.xml: chanNum=3, otherNum=4`; `stembox_deck_bottom.xml: chanNum=1, sg1=[Sampler1], sg2=[Sampler2], sg3=[Sampler3], sg4=[Sampler4], sg5=[Sampler5], sg6=[Sampler6], sg7=[Sampler7], sg8=[Sampler8]`; `stembox_deck_bottom.xml: chanNum=3, sg1=[Sampler1], sg2=[Sampler2], sg3=[Sampler3], sg4=[Sampler4], sg5=[Sampler5], sg6=[Sampler6], sg7=[Sampler7], sg8=[Sampler8]`; `stembox_cdj_deck.xml: chanNum=2, otherNum=1`; `stembox_cdj_deck.xml: chanNum=4, otherNum=3`; `stembox_deck_bottom.xml: chanNum=2, sg1=[Sampler9], sg2=[Sampler10], sg3=[Sampler11], sg4=[Sampler12], sg5=[Sampler13], sg6=[Sampler14], sg7=[Sampler15], sg8=[Sampler16]`; `stembox_deck_bottom.xml: chanNum=4, sg1=[Sampler9], sg2=[Sampler10], sg3=[Sampler11], sg4=[Sampler12], sg5=[Sampler13], sg6=[Sampler14], sg7=[Sampler15], sg8=[Sampler16]`.
### stembox_navbar.xml
Source: [stembox_navbar.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_navbar.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxNavBar` | `Layout=horizontal; SizePolicy=i,me; MinimumSize=0,0` | none |
| `PushButton` | `{menuObj}` | `SizePolicy=me,me; NumberStates=2; State.Text={menuText}` | `{menuAction} → left; {menuDisplay} → state` |
| `PushButton` | `StemboxNavBrowse` | `SizePolicy=me,me; NumberStates={browseStates}; State.Text=BROWSE` | `{browseAction} → left; {browseDisplay} → state` |
| `PushButton` | `StemboxNavPlaylist` | `SizePolicy=me,me; NumberStates=2; State.Text=TAG LIST` | `{playlistAction} → left; [Stembox],playlist_mode → state` |
| `PushButton` | `StemboxNavDisplay` | `SizePolicy=me,me; NumberStates=2; State.Text=SEARCH` | `{searchAction} → left; [Stembox],search_mode → state` |
All direct bindings, including anonymous wrappers: `{menuAction} → LeftButton`; `{menuDisplay} → state`; `{browseAction} → LeftButton`; `{browseDisplay} → state`; `{playlistAction} → LeftButton`; `[Stembox],playlist_mode → state`; `{searchAction} → LeftButton`; `[Stembox],search_mode → state`.
### stembox_navitem.xml
Source: [stembox_navitem.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_navitem.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxNavItem` | `Layout=stacked; SizePolicy=me,me` | `{ActiveControl} → visible NOT` |
| `PushButton` | `StemboxNavHit` | `SizePolicy=me,me; NumberStates=1` | `{GoControl} → left` |
| `PushButton` | `StemboxNavIcon` | `Size=27f,27f; NumberStates=1` | none |
| `PushButton` | `StemboxNavLabel` | `Size=0me,18f; NumberStates=1; State.Text={Label}` | none |
| `WidgetGroup` | `StemboxNavItemOn` | `Layout=stacked; SizePolicy=me,me` | `{ActiveControl} → visible` |
| `PushButton` | `StemboxNavLabelOn` | `Size=0me,18f; NumberStates=1; State.Text={Label}` | none |
All direct bindings, including anonymous wrappers: `{ActiveControl} → visible NOT`; `{GoControl} → LeftButton`; `{ActiveControl} → visible`.
### stembox_pad.xml
Source: [stembox_pad.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxPad` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `HotcueButton` | `StemboxPadChip` | `Group=[Channel{chanNum}]; Hotcue={n}; MinimumSize=14,14; MaximumSize=14,14; SizePolicy=f,f; NumberStates=3; State.Text={L}` | none |
| `HotcueLabel` | `StemboxPadLoopIcon` | `Group=[Channel{chanNum}]; Hotcue={n}; LoopIconOnly=true; MinimumSize=31,14; MaximumSize=31,14; SizePolicy=f,f` | none |
| `HotcueLabel` | `StemboxPadName` | `Group=[Channel{chanNum}]; Hotcue={n}; SizePolicy=me,me; Alignment=left; Elide=right` | none |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`.
### stembox_pad_beatjump.xml
Source: [stembox_pad_beatjump.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_beatjump.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxBjCell` | `Layout=horizontal; SizePolicy=me,me` | none |
| `Label` | `StemboxBjArrow` | `Text={AL}; SizePolicy=min,me; Alignment=right&#124;vcenter` | none |
| `Label` | `StemboxBjNum` | `Text={N}; Size=24f,0me; Alignment=center` | none |
| `Label` | `StemboxBjArrow` | `Text={AR}; SizePolicy=min,me; Alignment=left&#124;vcenter` | none |
### stembox_pad_bl.xml
Source: [stembox_pad_bl.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_bl.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxBlCell` | `Layout=horizontal; SizePolicy=me,me` | `[Channel{chanNum}],beatloop_{SIZE}_enabled → highlight` |
| `Label` | `StemboxBlIcon` | `Text=⟳; SizePolicy=min,me; Alignment=center` | none |
| `Label` | `StemboxBlNum` | `Text={LBL}; SizePolicy=me,me; Alignment=center` | none |
All direct bindings, including anonymous wrappers: `[Channel{chanNum}],beatloop_{SIZE}_enabled → highlight`.
### stembox_pad_fx.xml
Source: [stembox_pad_fx.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_fx.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `StemboxFxCell` | `StemboxFxCell` | `SizePolicy=me,me; Slot={SLOT}; PK={PK}` | none |
### stembox_pad_ks.xml
Source: [stembox_pad_ks.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_ks.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxKsCell` | `Layout=horizontal; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `Label` | `{ICO}` | `Text={IC}; SizePolicy=min,me; Alignment=center` | none |
| `Label` | `StemboxKsNum` | `Text={LBL}; SizePolicy=me,me; Alignment=center` | none |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`.
### stembox_pad_label.xml
Source: [stembox_pad_label.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_label.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxPadModeCell` | `Layout=vertical; SizePolicy=me,me` | none |
| `Label` | `StemboxPadModeLabel` | `Text={T}; SizePolicy=me,me; Alignment=center` | none |
### stembox_pad_num.xml
Source: [stembox_pad_num.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_num.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxPadNum` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `Label` | `StemboxPadNumLabel` | `Text={N}; SizePolicy=me,me; Alignment=center` | none |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`.
### stembox_pad_sampler.xml
Source: [stembox_pad_sampler.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_sampler.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxSampCell` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `Label` | `StemboxSampNum` | `Text={SN}; SizePolicy=me,me; Alignment=center` | `{SG},track_loaded → visible NOT` |
| `TrackProperty` | `StemboxSampTitle` | `Property=titleInfo; Group={SG}; SizePolicy=me,me; Alignment=center; Elide=right` | `{SG},track_loaded → visible` |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`; `{SG},track_loaded → visible NOT`; `{SG},track_loaded → visible`.
### stembox_pad_slim.xml
Source: [stembox_pad_slim.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_slim.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxPadSlim` | `Layout=horizontal; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `HotcueButton` | `StemboxPadSlimChip` | `Group=[Channel{chanNum}]; Hotcue={n}; MinimumSize=20,4; MaximumSize=20,4; SizePolicy=f,f; NumberStates=3; State.Text=` | none |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`.
### stembox_pad_stem.xml
Source: [stembox_pad_stem.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_pad_stem.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxSampCell` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],{PK} → highlight` |
| `Label` | `StemboxSampNum` | `Text={LBL}; SizePolicy=me,me; Alignment=center` | none |
All direct bindings, including anonymous wrappers: `[Stembox],{PK} → highlight`.
### stembox_padfx_cells.xml
Source: [stembox_padfx_cells.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_padfx_cells.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
Template instantiations: `stembox_pad_fx.xml: PK=padpress_{chanNum}_1, SLOT=1`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_2, SLOT=2`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_3, SLOT=3`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_4, SLOT=4`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_5, SLOT=5`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_6, SLOT=6`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_7, SLOT=7`; `stembox_pad_fx.xml: PK=padpress_{chanNum}_8, SLOT=8`.
### stembox_samp_row.xml
Source: [stembox_samp_row.xml](</Applications/Stembox.app/Contents/Resources/src/interface-stembox/dispositions/stembox_samp_row.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `StemboxSampRow` | `StemboxSampRow` | `SizePolicy=me,me; Deck={Deck}; PadBase={PadBase}` | none |
### stembox-racine.xml
Source: [stembox-racine.xml](</Applications/Stembox.app/Contents/Resources/src/substrat/dispositions/stembox-racine.xml>).
| Widget type | ObjectName | Declared properties | Direct ConfigKeys |
| --- | --- | --- | --- |
| `WidgetGroup` | `StemboxRoot` | `Layout=vertical; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxNavRowWrap` | `Layout=vertical; Size=0me,46f` | none |
| `WidgetGroup` | `StemboxNavRowLarge` | `Layout=horizontal; Size=0me,46f` | `[Stembox],mix_mode → visible NOT` |
| `WidgetGroup` | `StemboxNavEdgePad` | `Size=192f,0me` | none |
| `PushButton` | `StemboxNavExit` | `SizePolicy=me,me; NumberStates=2; State.Text=EXIT` | `[Stembox],exit_request → left` |
| `PushButton` | `StemboxNavMenu` | `SizePolicy=me,me; NumberStates=2; State.Text=MENU` | `[Stembox],show_preferences → left; [Stembox],show_preferences → state` |
| `PushButton` | `StemboxNavBrowse` | `SizePolicy=me,me; NumberStates=2; State.Text=BROWSE` | `[Stembox],browse_full → left; [Stembox],browse_lit_full → state` |
| `PushButton` | `StemboxNavPlaylist` | `SizePolicy=me,me; NumberStates=2; State.Text=TAG LIST` | `[Stembox],playlist_full → left; [Stembox],playlist_mode → state` |
| `PushButton` | `StemboxNavDisplay` | `SizePolicy=me,me; NumberStates=2; State.Text=SEARCH` | `[Stembox],search_full → left; [Stembox],search_mode → state` |
| `WidgetGroup` | `StemboxNavRow` | `Layout=horizontal; Size=0me,46f` | `[Stembox],mix_mode → visible` |
| `WidgetGroup` | `StemboxNavDivider` | `Size=40f,0me; SizePolicy=f,me` | none |
| `WidgetGroup` | `StemboxBody` | `Layout=stacked; SizePolicy=me,me` | none |
| `WidgetGroup` | `StemboxBodyLarge` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],mix_mode → visible NOT` |
| `WidgetGroup` | `StemboxBodySplit` | `Layout=vertical; SizePolicy=me,me` | `[Stembox],mix_mode → visible` |
All direct bindings, including anonymous wrappers: `[Stembox],mix_mode → visible NOT`; `[Stembox],exit_request → LeftButton`; `[Stembox],show_preferences → LeftButton`; `[Stembox],show_preferences → state`; `[Stembox],browse_full → LeftButton`; `[Stembox],browse_lit_full → state`; `[Stembox],playlist_full → LeftButton`; `[Stembox],playlist_mode → state`; `[Stembox],search_full → LeftButton`; `[Stembox],search_mode → state`; `[Stembox],mix_mode → visible`.
Template instantiations: `stembox_navbar.xml: menuObj=StemboxNavExit, menuText=EXIT, menuAction=[Stembox],exit_request, menuDisplay=[Stembox],exit_request, browseStates=2, browseAction=[Stembox],browse_left, browseDisplay=[Stembox],browse_lit_left, playlistAction=[Stembox],playlist_left, searchAction=[Stembox],search_left`; `stembox_navbar.xml: menuObj=StemboxNavMenu, menuText=MENU, menuAction=[Stembox],show_preferences, menuDisplay=[Stembox],show_preferences, browseStates=2, browseAction=[Stembox],browse_right, browseDisplay=[Stembox],browse_lit_right, playlistAction=[Stembox],playlist_right, searchAction=[Stembox],search_right`; `stembox_mix_large.xml: `; `stembox_mix_split.xml: `.
## Exact QSS rules

Source: 
[style-stembox.qss](</Applications/Stembox.app/Contents/Resources/ressources/theme-client/style-stembox.qss>). Comments and extra whitespace are removed; selectors, property values, declaration order, and hex case are preserved. This is the complete extracted stylesheet, including definitions for residual widgets.
```css
#TitleText { font-family: "MRRT Stembox"; font-weight: normal; }
#StemboxMixLibDivider { background-color: #f5f5f5; min-height: 6px; max-height: 6px; }
#StemboxSourceHeader { background-color: #141414; color: #ffffff; font-family: "HVTC Stembox"; font-size: 22px; font-weight: bold; padding-left: 24px; }
#StemboxSourceBar { background-color: #141414; }
#StemboxSourceHeaderRow { background-color: #141414; }
#StemboxStemsManagerBtn { background-color: #1e1e1e; color: #cfcfcf; border: 1px solid #2f2f2f; border-radius: 6px; font-family: "HVTC Stembox"; font-size: 13px; font-weight: bold; }
#StemboxStemsManagerBtn:hover { background-color: #2d7ff9; color: #ffffff; border-color: #2d7ff9; }
#StemboxPastSessionsBtn { background-color: #1e1e1e; color: #cfcfcf; border: 1px solid #2f2f2f; border-radius: 6px; font-family: "HVTC Stembox"; font-size: 13px; font-weight: bold; }
#StemboxPastSessionsBtn:hover { background-color: #2d7ff9; color: #ffffff; border-color: #2d7ff9; }
#StemboxSourceDiagBtn { background-color: #1e1e1e; color: #9a9a9a; border: 1px solid #2f2f2f; border-radius: 11px; margin: 4px; font-family: "HVTC Stembox"; font-size: 13px; font-weight: bold; }
#StemboxSourceDiagBtn:hover { background-color: #2d7ff9; color: #ffffff; border-color: #2d7ff9; }
#StemboxStemsDest { background-color: #141414; }
#StemboxStemsList { background-color: #0a0a0a; }
#StemboxSourceClose { background-color: transparent; border: none; }
#StemboxSourceClose:hover { background-color: #d9363e; border-radius: 8px; }
#StemboxUsbHit { background-color: transparent; border: none; }
#StemboxBpmDiagHit { background-color: transparent; border: none; }
#StemboxStatsPanel { background-color: #1e1e1e; }
#StemboxStatsPanel WLabel { color: #ffffff; font-family: "HVTC Stembox"; font-size: 17px; font-weight: bold; padding-right: 22px; }
#StemboxStatKey { color: #8f8f8f; font-weight: normal; padding-left: 22px; padding-right: 0px; }
#StemboxStatsPanel #StemboxStatSource { color: #1a79eb; }
#StemboxStatRow { background-color: #1e1e1e; }
#StemboxSplitLibDock #StemboxStatsPanel WLabel { font-size: 13px; }
#StemboxSplitLibDock #StemboxStatsPanel { min-width: 270px; max-width: 270px; }
#StemboxProgressText { color: #1a79eb; padding-right: 22px; }
#StemboxProgressTrack { background-color: #0d0d0d; border-radius: 5px; margin-left: 22px; margin-right: 22px; }
#StemboxProgressFill { background-color: #1a79eb; border-radius: 5px; }
#StemboxProgressRest { background: transparent; }
#StemboxAnalyzePauseBtn { background-color: #3a3a3a; color: #ffffff; font-family: "Open Sans"; font-size: 12px; font-weight: normal; border: 1px solid #555555; border-radius: 5px; padding: 7px 14px; }
#StemboxAnalyzePauseBtn:hover { background-color: #4a4a4a; border: 1px solid #666666; }
#StemboxAnalyzePauseBtn[displayValue="1"] { background-color: #1a79eb; color: #ffffff; border: 1px solid #1a79eb; }
#StemboxAnalyzePauseBtn[displayValue="1"]:hover { background-color: #21a6ed; border: 1px solid #21a6ed; }
#StemboxAnalyzePauseBtn:pressed { background-color: #555555; border: 1px solid #666666; }
#StemboxEjectBtn { background-color: #3a3a3a; color: #cccccc; font-family: "Open Sans"; font-size: 12px; font-weight: normal; border: 1px solid #555555; border-radius: 5px; padding: 7px 14px; }
#StemboxEjectBtn:hover { background-color: #4a3a2a; border: 1px solid #886644; color: #ffcc88; }
#StemboxEjectBtn:pressed { background-color: #5a4a3a; border: 1px solid #aa8855; color: #ffcc88; }
#StemboxVDivider { background-color: #0d0d0d; }
#StemboxHDivider { background-color: #0d0d0d; }
#StemboxTopBar { background-color: #141414; border-bottom: 1px solid #1d1d1d; padding: 0px 24px; }
#StemboxBackButton { background-color: transparent; border: 1px solid rgba(255, 255, 255, 0.18); border-radius: 4px; padding: 0px; min-width: 34px; max-width: 34px; min-height: 34px; max-height: 34px; }
#StemboxBackButton:hover { border-color: rgba(255, 255, 255, 0.22); }
#StemboxListIcon { background-color: transparent; margin-right: 10px; }
#StemboxPlaylistTitle { color: #b8b8b8; font-family: "MRRT Stembox"; font-size: 22px; font-weight: normal; padding-left: 0px; padding-right: 10px; }
#StemboxMatchingEmpty { background-color: #000000; }
#StemboxMatchingEmptyLabel { color: #8a8a8a; font-family: "DHA Stembox"; font-size: 20px; qproperty-wordWrap: true; padding: 40px; }
#StemboxLeftNav { background-color: #0d0d0d; }
#LibraryContainer, #StemboxMain, #StemboxContent, #StemboxNavPane, #StemboxRootRow, #StemboxTracksPane, #StemboxLeftNav { background-color: #0a0a0a; }
#StemboxNavRightDivider { background-color: #000000; }
#StemboxUsbCell { background-color: transparent; min-height: 70px; max-height: 70px; }
#StemboxUsbBox { background-color: #1b3a6a; min-height: 66px; max-height: 66px; min-width: 64px; max-width: 64px; }
#StemboxUsbGlyph { background-color: transparent; }
#StemboxUsbLabel { color: #ffffff; font-family: "HVTC Stembox"; font-size: 15px; font-weight: bold; }
#StemboxNavItem, #StemboxNavItemOn { background-color: #2a2a2a; border-bottom: 1px solid #1a1a1a; border-radius: 3px; }
#StemboxNavItemOn { background-color: #1a79eb; }
#StemboxNavIcon { background-color: transparent; }
#StemboxNavHit { background: transparent; border: none; }
#StemboxNavLabel { color: #cfcfcf; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; border: none; }
#StemboxNavLabelOn { color: #ffffff; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; border: none; }
#StemboxKeyboard { background-color: #0a0a0a; }
#StemboxKey, #StemboxKeyWide { background-color: #2e2e2e; border: none; border-radius: 6px; color: #ffffff; font-family: "HVTC Stembox"; font-size: 20px; font-weight: bold; margin: 5px; }
#StemboxKey:pressed, #StemboxKeyWide:pressed { background-color: #3d3d3d; border: none; }
#StemboxKeyBack { background-color: transparent; border: none; margin: 5px; }
#StemboxSearchBar WSearchLineEdit { color: #ffffff; padding-left: 44px; }
#StemboxSearchBar WSearchLineEdit::drop-down { width: 0px; border: none; }
#StemboxSearchBar WSearchLineEdit::down-arrow { image: none; width: 0px; height: 0px; }
#StemboxKeyWide { min-width: 180px; }
#StemboxSplitLibDock #StemboxKey, #StemboxSplitLibDock #StemboxKeyWide { font-size: 12px; }
#StemboxSplitLibDock #StemboxKeyWide { min-width: 0px; }
#StemboxLibToggle { background-color: transparent; border: none; padding: 0px; }
#StemboxLibToggle:hover { background-color: #232323; border-radius: 3px; }
#StemboxPrefsButton { background-color: transparent; border: none; padding: 1px; }
#StemboxPrefsButton:hover { background-color: #232323; border-radius: 3px; }
#StemboxCdjRow { background-color: #060606; }
#StemboxCdjDivider { background-color: qlineargradient(x1:0.5, y1:0, x2:0.5, y2:1, stop:0 #303030, stop:1 #000000); }
#StemboxCdjDeck { background-color: #000000; }
#StemboxDeckInfo { background-color: #141414; padding-left: 6px; padding-right: 6px; }
#StemboxLargeCenterBar { background-color: #141414; }
#StemboxLargeCenterFill { background-color: #000000; }
#StemboxLargeTopPad { background-color: #000000; }
#StemboxLargeLogoRow { background-color: #000000; }
#StemboxGridEditColumn { background-color: #000000; }
#StemboxNavRow, #StemboxNavRowLarge { background-color: qlineargradient(x1:0, y1:0.5, x2:1, y2:0.5, stop:0 #1e1e1e, stop:0.28 #1e1e1e, stop:0.5 #303030, stop:0.72 #1e1e1e, stop:1 #1e1e1e); padding-left: 6px; padding-right: 6px; }
#StemboxNavRowLarge { padding-top: 4px; padding-bottom: 4px; }
#StemboxNavDivider { background-color: #303030; }
#StemboxNavBar { background-color: transparent; padding: 8px 0px; }
#StemboxNavMenu, #StemboxNavExit, #StemboxNavBrowse, #StemboxNavPlaylist, #StemboxNavDisplay { background-color: #2a2a2a; border: none; border-radius: 4px; color: #c8c8c8; font-family: "HVTC Stembox"; font-size: 11px; font-weight: bold; }
#StemboxNavMenu:hover, #StemboxNavBrowse:hover, #StemboxNavPlaylist:hover, #StemboxNavDisplay:hover { background-color: #363636; }
#StemboxNavExit:hover { background-color: #d9363e; color: #ffffff; }
#StemboxNavExit:pressed { background-color: #b02a30; color: #ffffff; }
#StemboxNavMenu:pressed, #StemboxNavBrowse:pressed, #StemboxNavPlaylist:pressed, #StemboxNavDisplay:pressed, #StemboxNavMenu[displayValue="1"], #StemboxNavBrowse[displayValue="1"], #StemboxNavPlaylist[displayValue="1"], #StemboxNavDisplay[displayValue="1"], #StemboxNavMenu:checked, #StemboxNavBrowse:checked, #StemboxNavPlaylist:checked, #StemboxNavDisplay:checked { color: #33baff; }
#StemboxDeckCover { background-color: #000000; }
#StemboxDeckNote { background-color: transparent; }
#StemboxDeckTitle { color: #ffffff; font-family: "MRRT Stembox"; font-size: 17px; font-weight: bold; background: transparent; qproperty-alignment: 'AlignLeft | AlignVCenter'; padding-top: 3px; }
#StemboxDeckArtist { color: #9a9a9a; font-family: "MRRT Stembox"; font-size: 13px; background: transparent; }
#StemboxDeckStat { background-color: #1c1c1c; border: 1px solid #333333; border-radius: 3px; }
#StemboxDeckStatCap { color: #7a7a7a; font-family: "HVTC Stembox"; font-size: 9px; font-weight: bold; background: transparent; }
#StemboxDeckStatVal { color: #ffffff; font-family: "HVTC Stembox"; font-size: 18px; font-weight: normal; background: transparent; qproperty-normalColor: #ffffff; }
#StemboxGridEditBtn { background-color: #ffffff; color: #000000; border: none; margin: 4px; border-radius: 8px; font-family: "HVTC Stembox"; font-size: 11px; font-weight: bold; }
#StemboxGridEditBtn:hover { background-color: #1A79EB; color: #ffffff; }
#StemboxGridEditBtn[value="1"] { background-color: #1A79EB; color: #ffffff; }
#StemboxGridEditBtn:disabled { background-color: #3a3a3a; color: #6a6a6a; }
#StemboxGridEditBtn:disabled:hover { background-color: #3a3a3a; color: #6a6a6a; }
#StemboxGridEditPanel { background-color: #1c1c1c; border: 1px solid #4a4a4a; border-radius: 6px; padding-left: 8px; padding-right: 8px; padding-top: 0px; padding-bottom: 6px; }
#StemboxGridEditTitle { color: #c8c8c8; font-family: "HVTC Stembox"; font-size: 10px; background: transparent; }
#StemboxGridEditDiv { background-color: #4a4a4a; }
#StemboxGridEditCap { color: #e8e8e8; font-family: "HVTC Stembox"; font-size: 10px; background: transparent; }
#StemboxGridEditBpm { color: #ffffff; font-family: "HVTC Stembox"; font-size: 26px; background: transparent; qproperty-normalColor: #ffffff; }
#StemboxGridEditKey { background-color: #2a2a2a; color: #ffffff; border: 1px solid #5a5a5a; border-radius: 4px; font-family: "HVTC Stembox"; font-size: 14px; font-weight: bold; }
#StemboxGridEditKey:hover { background-color: #1A79EB; border-color: #1A79EB; }
#StemboxGridEditKey:pressed { background-color: #0d47a1; }
#StemboxGridEditKeyRed { background-color: #2a1414; color: #ff3b30; border: 1px solid #ff3b30; border-radius: 4px; font-family: "HVTC Stembox"; font-size: 14px; font-weight: bold; }
#StemboxGridEditKeyRed:hover { background-color: #ff3b30; color: #000000; }
#StemboxGridEditKeyRed:pressed { background-color: #b71c1c; color: #000000; }
#StemboxDeckBar { background-color: #000000; padding-left: 6px; padding-right: 6px; }
#StemboxPlayerBox { border: 1px solid #555555; border-radius: 3px; }
#StemboxPlayerBoxIdle { border: 1px solid #1A79EB; border-radius: 3px; }
#StemboxPlayerCapIdle { color: #1A79EB; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; }
#StemboxPlayerNumIdle { color: #1A79EB; font-family: "HVTC Stembox"; font-size: 25px; font-weight: normal; background: transparent; }
#StemboxWfTagLive { background-color: #141414; border: 1px solid #ffffff; margin: 1px; }
#StemboxWfTagIdle { background-color: #141414; border: 1px solid #1A79EB; margin: 1px; }
#StemboxWfTagNum { color: #ffffff; font-family: "HVTC Stembox"; font-size: 13px; font-weight: bold; background: transparent; }
#StemboxWfTagNumIdle { color: #1A79EB; font-family: "HVTC Stembox"; font-size: 13px; font-weight: bold; background: transparent; }
#StemboxPlayerCap, #StemboxBarCap { color: #ffffff; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; }
#StemboxSlipTag { color: #ff0000; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; border: 1px solid #ff0000; padding-top: 1px; border-radius: 3px; }
#StemboxNoFxPart { color: #ffffff; font-family: "HVTC Stembox"; font-size: 9px; font-weight: bold; background: #6a6a6a; border: 1px solid #8a8a8a; border-radius: 3px; }
#StemboxPlayerNum { color: #ffffff; font-family: "HVTC Stembox"; font-size: 25px; font-weight: normal; background: transparent; }
#StemboxCueWave { background: transparent; border: none; }
#StemboxTrackNo { color: #ffffff; font-family: "HVTC Stembox"; font-size: 34px; font-weight: normal; background: transparent; }
#StemboxCueBtn { color: #e08a2a; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background-color: #1a1206; border: 1px solid #e08a2a; border-radius: 3px; }
#StemboxCueBtn[displayValue="1"], #StemboxCueBtn:checked { background-color: #e08a2a; color: #000000; }
#StemboxSyncBtn { color: #2f9bd6; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background-color: #06121a; border: 1px solid #2f9bd6; border-radius: 3px; }
#StemboxSyncBtn[displayValue="1"], #StemboxSyncBtn:checked { background-color: #2f9bd6; color: #000000; }
#StemboxRemain { color: #ffffff; font-family: "HVTC Stembox"; font-size: 46px; font-weight: normal; background: transparent; qproperty-normalColor: #ffffff; }
#StemboxBpmBox { border: 1px solid #666666; padding-left: 3px; padding-right: 10px; border-radius: 3px; }
#StemboxBpmBoxIdle { border: 1px solid #1A79EB; padding-left: 3px; padding-right: 10px; border-radius: 3px; }
#StemboxBarCapIdle { color: #1A79EB; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; }
#StemboxBpmBigIdle { color: #1A79EB; font-family: "HVTC Stembox"; font-size: 46px; font-weight: normal; background: transparent; qproperty-normalColor: #1A79EB; qproperty-masterColor: #1A79EB; }
#StemboxBpmBig { color: #ffffff; font-family: "HVTC Stembox"; font-size: 46px; font-weight: normal; background: transparent; qproperty-masterColor: #ffffff; qproperty-normalColor: #ffffff; }
#StemboxDeckBar2 { background-color: #000000; padding-left: 6px; padding-right: 6px; }
#StemboxToggle { color: rgba(224, 138, 42, 0.5); font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; border: 1px solid rgba(224, 138, 42, 0.5); padding-top: 2px; border-radius: 3px; }
#StemboxToggle[displayValue="1"], #StemboxToggle:checked { color: #e08a2a; background: transparent; border: 1px solid #e08a2a; }
#StemboxQuantize { color: rgba(255, 0, 0, 0.5); font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; border: 1px solid rgba(255, 0, 0, 0.5); padding-top: 2px; border-radius: 3px; }
#StemboxQuantize[displayValue="1"], #StemboxQuantize:checked { color: #ff0000; background: transparent; border: 1px solid #ff0000; }
#StemboxKey2 { color: #ffffff; font-family: "HVTC Stembox"; font-size: 21px; font-weight: normal; background: transparent; }
#StemboxNotLoadedSmall { color: #ff0000; font-size: 15px; font-weight: 900; letter-spacing: -1px; background: transparent; }
#StemboxPads { background-color: #060606; }
#StemboxPadsSlim { background-color: #060606; }
#StemboxPadSlim { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131312, stop:1 #090907); border: 1px solid #2c2c2c; border-radius: 3px; margin: 1px; padding: 0px; }
#StemboxPadSlim[highlight="1"] { border: 1px solid #ffffff; }
#StemboxPadSlimChip { border: none; border-radius: 2px; padding: 0px; }
#StemboxPadSlimChip[displayValue="0"] { background-color: transparent; }
#StemboxPadNum { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131312, stop:1 #090907); border: 1px solid #2c2c2c; border-radius: 3px; margin: 1px; padding: 0px; }
#StemboxPadNum[highlight="1"] { border: 1px solid #ffffff; }
#StemboxPadNumLabel { color: #6a6a6a; font-family: "HVTC Stembox"; font-size: 10px; font-weight: bold; background: transparent; }
#StemboxBlCell, #StemboxKsCell { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131312, stop:1 #090907); border: 1px solid #2c2c2c; border-radius: 3px; margin: 2px; }
#StemboxBlCell[highlight="1"], #StemboxKsCell[highlight="1"] { border: 1px solid #ffffff; }
#StemboxBlIcon { color: #ff9800; font-size: 13px; font-weight: bold; background: transparent; padding-left: 4px; padding-right: 2px; }
#StemboxBlNum { color: #ff9800; font-family: "HVTC Stembox"; font-size: 16px; font-weight: bold; background: transparent; }
#StemboxKsIconUp { color: #f0c000; font-size: 11px; background: transparent; padding-left: 4px; }
#StemboxKsIconDown { color: #a05cf0; font-size: 11px; background: transparent; padding-left: 4px; }
#StemboxKsIconZero { color: #1A79EB; font-size: 11px; background: transparent; padding-left: 4px; }
#StemboxKsNum { color: #ffffff; font-family: "HVTC Stembox"; font-size: 16px; font-weight: bold; background: transparent; }
#StemboxPadsSlim #StemboxBlCell, #StemboxPadsSlim #StemboxKsCell { margin: 1px; }
#StemboxPadsSlim #StemboxBlNum, #StemboxPadsSlim #StemboxKsNum { font-size: 10px; }
#StemboxPadsSlim #StemboxBlIcon { font-size: 9px; padding-left: 3px; padding-right: 1px; }
#StemboxPadsSlim #StemboxKsIconUp, #StemboxPadsSlim #StemboxKsIconDown, #StemboxPadsSlim #StemboxKsIconZero { font-size: 8px; padding-left: 3px; }
#StemboxPad { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131312, stop:1 #090907); border: 1px solid #2c2c2c; border-radius: 3px; margin: 2px; padding: 4px 6px; }
#StemboxPad[highlight="1"] { border: 1px solid #ffffff; }
#StemboxPadChip { font-family: "HVTC Stembox"; font-size: 11px; font-weight: bold; border: none; border-radius: 2px; padding: 0px; color: #000000; }
#StemboxPadChip[displayValue="0"] { background-color: transparent; color: #6a6a6a; }
#StemboxPadChip[light="true"] { color: #000000; }
#StemboxPadChip[dark="true"] { color: #000000; }
#StemboxPadName { color: #ffffff; font-family: "HVTC Stembox"; font-size: 12px; font-weight: bold; background: transparent; padding-top: 3px; }
#StemboxPadModeCell { background-color: #141414; border: 1px solid #2c2c2c; margin: 2px; }
#StemboxPadModeLabel { color: #ffffff; font-family: "HVTC Stembox"; font-size: 16px; font-weight: bold; background: transparent; }
#StemboxBjCell { background-color: #141414; border: 1px solid #2c2c2c; }
#StemboxBjCell[highlight="1"] { border: 1px solid #ffffff; }
#StemboxBjNum { color: #ffffff; font-family: "HVTC Stembox"; font-size: 13px; font-weight: 900; background-color: #141414; }
#StemboxBjArrow { color: #ff0000; font-family: "HVTC Stembox"; font-size: 17px; font-weight: 900; background: transparent; }
#StemboxBjRow, #StemboxBlRow, #StemboxKsRow, #StemboxFxCell, #StemboxSampRow { font-family: "HVTC Stembox"; font-weight: 900; }
#StemboxSampCell { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131312, stop:1 #090907); border: 1px solid #2c2c2c; border-radius: 3px; margin: 2px; }
#StemboxSampCell[highlight="1"] { border: 1px solid #ffffff; }
#StemboxSampNum { color: #6a6a6a; font-family: "HVTC Stembox"; font-size: 16px; font-weight: bold; background: transparent; }
#StemboxSampTitle { color: #ffffff; font-family: "HVTC Stembox"; font-size: 11px; font-weight: bold; background: transparent; padding: 0px 3px; }
#StemboxPadPress { background: transparent; border: 1px solid #ffffff; margin: 1px; }
#StemboxFxRBadge { color: #ffffff; background-color: #d0202a; border-radius: 2px; font-family: "HVTC Stembox"; font-size: 9px; font-weight: 900; }
#StemboxFxRHide { color: transparent; background: transparent; }
WTrackTableView::item:selected, WLibrarySidebar::item:selected { background-color: #1A79EB; color: #ffffff; }
WTrackTableView::item:hover:!selected, WLibrarySidebar::item:hover:!selected { background-color: rgba(26, 121, 235, 0.5); }
#StemboxLargeRow #StemboxPlayerBoxIdle { border: 1px solid #555555; }
#StemboxLargeRow #StemboxPlayerCapIdle { color: #ffffff; }
#StemboxLargeRow #StemboxPlayerNumIdle { color: #ffffff; }
#StemboxLargeRow #StemboxBpmBoxIdle { border: 1px solid #666666; }
#StemboxLargeRow #StemboxBarCapIdle { color: #ffffff; }
#StemboxLargeRow #StemboxBpmBigIdle { color: #ffffff; qproperty-masterColor: #ffffff; qproperty-normalColor: #ffffff; }
QComboBox QAbstractItemView { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; outline: none; padding: 2px; }
QComboBox QAbstractItemView::item { min-height: 30px; padding: 4px 10px; border: none; }
QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover { background-color: #1A79EB; color: #ffffff; }
```
## Theme icon inventory

Source directory: `docs/REFERENCES-VISUELLES/icones/theme` under the resource root. The table covers every SVG recursively. Intrinsic dimensions and `viewBox` are exact. Colors list literal hex paints; named colors are listed separately. QSS may recolor icons at runtime. `viewBox` units are not widget dimensions.
| SVG path | Intrinsic width × height | viewBox | Literal paints |
| --- | --- | --- | --- |
| `classic/style/stembox_logo.svg` | `162 × 42` | `0 0 162 42` | No literal paint found |
| `classic/style/stembox_logo_small.svg` | `74 × 24` | `0 0 74 24` | No literal paint found |
| `icons/scalable/apps/stembox.svg` | `512 × 512` | `0 0 512 512` | No literal paint found |
| `icons/scalable/apps/stembox_ios.svg` | `512 × 512` | `0 0 512 512` | No literal paint found |
| `icons/scalable/apps/stembox_macos.svg` | `512 × 512` | `0 0 512 512` | No literal paint found |
| `palemoon/style/stembox_logo.svg` | `160 × 40` | `0 0 160 40` | No literal paint found |
| `palemoon/style/stembox_logo_small.svg` | `74 × 24` | `0 0 74 24` | No literal paint found |
| `preferences/dark/ic_preferences_about.svg` | `24 × 24` | `0 0 24 24` | `#2c2c2c`, `none` |
| `preferences/dark/ic_preferences_controllers.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_crossfader.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_display.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_effects.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_minimize.svg` | `24 × 24` | `0 0 24 24` | `#2c2c2c`, `none` |
| `preferences/dark/ic_preferences_pads.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `#ffffff`, `none` |
| `preferences/dark/ic_preferences_recording.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_settings.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_preferences_stembox.svg` | `32 × 32` | `0 0 32 32` | `#0f0f0f`, `none` |
| `preferences/dark/ic_stembox_symbolic.svg` | `32 × 32` | `0 0 32 32` | `#e8e8e8`, `none` |
| `preferences/light/ic_preferences_about.svg` | `24 × 24` | `0 0 24 24` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_controllers.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_crossfader.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_display.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_effects.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_minimize.svg` | `24 × 24` | `0 0 24 24` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_pads.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_recording.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_settings.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_preferences_stembox.svg` | `32 × 32` | `0 0 32 32` | `#ffffff`, `none` |
| `preferences/light/ic_stembox_symbolic.svg` | `32 × 32` | `0 0 32 32` | `#2b2b2b`, `none` |
| `prefs_dropdown_arrow.svg` | `14 × 9` | `0 0 14 9` | `#aeb4bd`, `none` |
| `prefs_dropdown_arrow_dark.svg` | `14 × 9` | `0 0 14 9` | `#000000`, `none` |
| `skins/LateNight/stembox_logo_24px.svg` | `78 × 24` | `0 0 78 24` | No literal paint found |
| `stembox-icon-logo-christmas.svg` | `236 × 48` | `0 0 236 48` | No literal paint found |
| `stembox-icon-logo-symbolic.svg` | `236 × 48` | `0 0 236 48` | No literal paint found |
| `stembox_back.svg` | none | `0 0 24 24` | `#9a9a9a`, `none` |
| `stembox_blank.svg` | none | `0 0 22 44` | No literal paint found |
| `stembox_burger.svg` | none | `0 0 24 24` | `#cfcfcf` |
| `stembox_burger_on.svg` | none | `0 0 24 24` | `#0e92d9` |
| `stembox_check.svg` | none | `0 0 24 24` | `#ffffff` |
| `stembox_close.svg` | none | `0 0 24 24` | `#d0d0d0`, `none` |
| `stembox_cuewave_left.svg` | none | `0 0 22 44` | `#e08a2a`, `none` |
| `stembox_cuewave_right.svg` | none | `0 0 22 44` | `#e08a2a`, `none` |
| `stembox_fontsize.svg` | none | `0 0 54 26` | `#dcdcdc`, `#ffffff`, `none` |
| `stembox_fontsize_0.svg` | none | `0 0 86 70` | `#3a3a3a`, `#8f8f8f`, `#ffffff`, `none` |
| `stembox_fontsize_1.svg` | none | `0 0 86 70` | `#3a3a3a`, `#8f8f8f`, `#ffffff`, `none` |
| `stembox_fontsize_2.svg` | none | `0 0 86 70` | `#3a3a3a`, `#8f8f8f`, `#ffffff`, `none` |
| `stembox_hdr_album.svg` | none | `0 0 219 212` | `#cfcfcf` |
| `stembox_hdr_artist.svg` | none | `0 0 231 196` | `#cfcfcf` |
| `stembox_hdr_key.svg` | none | `0 0 235 214` | `#cfcfcf` |
| `stembox_hdr_track.svg` | none | `0 0 169 213` | `#cfcfcf` |
| `stembox_ic_album.svg` | none | `0 0 219 212` | `#cfcfcf` |
| `stembox_ic_artist.svg` | none | `0 0 231 196` | `#cfcfcf` |
| `stembox_ic_key.svg` | none | `0 0 235 214` | `#cfcfcf` |
| `stembox_ic_playlist.svg` | none | `0 0 220 213` | `#cfcfcf` |
| `stembox_kb_back.svg` | none | `0 0 94 80` | `#2e2e2e`, `#ffffff`, `none` |
| `stembox_kb_back_frame.svg` | none | `0 0 80 80` | `#2e2e2e`, `none` |
| `stembox_kb_back_x.svg` | none | `0 0 44 30` | `#ffffff`, `none` |
| `stembox_list.svg` | none | `0 0 220 213` | `#cfcfcf` |
| `stembox_logo.svg` | `340 × 136` | `0 0 340 136` | No literal paint found |
| `stembox_nav_album.svg` | none | `-27.38 -26.50 273.75 265.00` | `#cfcfcf` |
| `stembox_nav_album_on.svg` | none | `-27.38 -26.50 273.75 265.00` | `#ffffff` |
| `stembox_nav_artist.svg` | none | `-28.88 -24.50 288.75 245.00` | `#cfcfcf` |
| `stembox_nav_artist_on.svg` | none | `-28.88 -24.50 288.75 245.00` | `#ffffff` |
| `stembox_nav_folder.svg` | none | `-28.62 -25.00 286.25 250.00` | `#cfcfcf` |
| `stembox_nav_folder_on.svg` | none | `-28.62 -25.00 286.25 250.00` | `#41d158` |
| `stembox_nav_history.svg` | none | `-27.50 -26.25 275.00 262.50` | `#cfcfcf` |
| `stembox_nav_history_on.svg` | none | `-27.50 -26.25 275.00 262.50` | `#ffffff` |
| `stembox_nav_key.svg` | none | `-29.38 -26.75 293.75 267.50` | `#cfcfcf` |
| `stembox_nav_key_on.svg` | none | `-29.38 -26.75 293.75 267.50` | `#ffffff` |
| `stembox_nav_matching.svg` | none | `-25.62 -25.62 256.25 256.25` | `#cfcfcf` |
| `stembox_nav_matching_on.svg` | none | `-25.62 -25.62 256.25 256.25` | `#ffffff` |
| `stembox_nav_playlist.svg` | none | `-27.50 -26.62 275.00 266.25` | `#cfcfcf` |
| `stembox_nav_playlist_on.svg` | none | `-27.50 -26.62 275.00 266.25` | `#ffffff` |
| `stembox_nav_search.svg` | none | `0 0 230 214` | `#cfcfcf` |
| `stembox_nav_search_on.svg` | none | `0 0 230 214` | `#ffffff` |
| `stembox_nav_track.svg` | none | `-26.12 -26.62 261.25 266.25` | `#cfcfcf` |
| `stembox_nav_track_on.svg` | none | `-26.12 -26.62 261.25 266.25` | `#ffffff` |
| `stembox_note.svg` | `327.000000pt × 439.000000pt` | `0 0 327.000000 439.000000` | `#ffffff`, `none` |
| `stembox_prefs.svg` | none | `0 0 24 24` | `#cfcfcf` |
| `stembox_search_icon.svg` | `38 × 24` | `0 0 38 24` | `#ffffff`, `none` |
| `stembox_stems.svg` | `18 × 18` | `0 0 18 18` | `#ffffff` |
| `stembox_stems_rose.svg` | `18 × 18` | `0 0 18 18` | `#EF5DA8` |
| `stembox_usb.svg` | none | `0 0 32 32` | `#ffffff`, `none` |
| `templates/logo_stembox.svg` | `359.981 × 437.632` | `0 0 359.981 437.632` | No literal paint found |
White versus gray navigation variants use `_on` filenames. Most normal nav icons are `#cfcfcf`, active icons `#ffffff`; the unused folder active variant is `#41d158`. Stem icons are `18×18`, white or rose `#EF5DA8`. Cue-wave arcs are `#e08a2a`. Preferences include dark and light icon sets for display, pads, recording, crossfader, controllers, effects, settings, Stembox, about, and minimize, plus `ic_stembox_symbolic`. These icon categories do not prove complete preference-page controls or page dimensions.

## Source manifest and limits

The manifest identifies the XML and QSS snapshots. SHA-256 values are file-byte hashes. XML line counts include comments. TTF names and SVG metadata were parsed directly. Settings values were loaded from the specified JSON. No application was launched and no preferences were changed.
| Source | Bytes | Lines | SHA-256 |
| --- | --- | --- | --- |
| `src/interface-stembox/dispositions/library.xml` | 81899 | 1193 | `68d0b9abdc6de065c2a46ee0f844e5f4f4995e5d56634a697f3e0948fcf3ea0a` |
| `src/interface-stembox/dispositions/stembox_bj_row.xml` | 686 | 14 | `66440911b0bfcd71529f18919db5bc9807f3f30e82d968040dd5bda99b5e1b25` |
| `src/interface-stembox/dispositions/stembox_bl_row.xml` | 1007 | 18 | `42434b8211e79b9ddadea558a02b19091fea1dc68375e1b192498301dc819b69` |
| `src/interface-stembox/dispositions/stembox_cdj_deck.xml` | 69169 | 1103 | `25fff307eebcab6467da706c45ef71f980332168ffcaf9e5b38e4ca5f4a8e54d` |
| `src/interface-stembox/dispositions/stembox_deck_bottom.xml` | 31083 | 397 | `1bcd1de865a6a28ce41a8c140ac1cbe1dcb48d3281570729d761b51f1a87f779` |
| `src/interface-stembox/dispositions/stembox_gridedit.xml` | 9751 | 183 | `491b7c3957709e4ba6b532890697ba733a9548fb65216a5c06bcebe9d3bb5484` |
| `src/interface-stembox/dispositions/stembox_key.xml` | 510 | 17 | `1796b66409c07b9c51eb51cf5810b21708180f30dcf1e928e2fac3ea7abbfe9e` |
| `src/interface-stembox/dispositions/stembox_key_back.xml` | 829 | 20 | `b48a4ad148b7cc960811e63d58f33583fae7e959df30f64fe9d181c06fcb9b36` |
| `src/interface-stembox/dispositions/stembox_key_wide.xml` | 728 | 20 | `9fdc62e00a05c64b986c2ee68bb2e22fe649c82625537c39dc9b9f27cb574cf0` |
| `src/interface-stembox/dispositions/stembox_ks_row.xml` | 851 | 16 | `d0ea0b774da7035852d114afc54e4611c618fa9994b8812e8664bc08f792db67` |
| `src/interface-stembox/dispositions/stembox_large_wf.xml` | 3884 | 79 | `bdead25e0f61f73a4ecd40f6b9ed005873c14c226a37dc218eff06e143d5c785` |
| `src/interface-stembox/dispositions/stembox_mix_active.xml` | 6338 | 119 | `f076a25391e419d5cbcda6de47f5472937d0b5520509b030bc8a89da54bd9da6` |
| `src/interface-stembox/dispositions/stembox_mix_large.xml` | 42089 | 612 | `5b0a9b8a1b6b489eb63e15e8d77f817aff75d823b2ce550d3d03b3be583cd534` |
| `src/interface-stembox/dispositions/stembox_mix_split.xml` | 13702 | 244 | `0403994942d9036a41e839b9a359fb8a7994577a54eb6d8d50f8ec5140c0ea14` |
| `src/interface-stembox/dispositions/stembox_navbar.xml` | 6164 | 107 | `7f7bd90d6d2858e255ff22d46289cbacfa3baca2d9287ca5cb6f0f980b052922` |
| `src/interface-stembox/dispositions/stembox_navitem.xml` | 8630 | 177 | `75a7f7726a49c6146bcabac6c7f25bf5e6af29b66a4728c7ed7360d14616131d` |
| `src/interface-stembox/dispositions/stembox_pad.xml` | 2730 | 56 | `cbe41a36c0e9227f5b2c4e40eb0600ef6eb7931d6443cd75016348d2cff2ef92` |
| `src/interface-stembox/dispositions/stembox_pad_beatjump.xml` | 2095 | 30 | `8f409b7bef5da2127651793b094d1c116e0ac859b71710b1405bff065c40afcb` |
| `src/interface-stembox/dispositions/stembox_pad_bl.xml` | 1359 | 30 | `e98f5a781c7f48e89857ef2f6d101f20eab8c2a302a6700ea26921d384214c49` |
| `src/interface-stembox/dispositions/stembox_pad_fx.xml` | 920 | 16 | `17ed5612d92575e8da9bb640a1c1b4a7e04a3c11eebd90ce8133dcebe5c88a99` |
| `src/interface-stembox/dispositions/stembox_pad_ks.xml` | 1358 | 30 | `3768fd6828ad0da17eed97353010f13da2bb4907fe4f16871cd622d656f691bd` |
| `src/interface-stembox/dispositions/stembox_pad_label.xml` | 731 | 19 | `8188c9e9fc107b5b88d1425045cd228cda2adbb2c223d4cad89f7203116f7a21` |
| `src/interface-stembox/dispositions/stembox_pad_num.xml` | 1030 | 25 | `377a90909a506c989a1723a107f0021e9756613dfdbbc64d4be8689202c8539a` |
| `src/interface-stembox/dispositions/stembox_pad_sampler.xml` | 1384 | 29 | `3069096e2b6c02f9eced38e6886d0a548d6be4c0f16e857bf0f6045d6a82f39a` |
| `src/interface-stembox/dispositions/stembox_pad_slim.xml` | 2626 | 50 | `301b7f337b8ee8ec1b480d5751f42bac816ef020fb40d4272856c33cc61c2918` |
| `src/interface-stembox/dispositions/stembox_pad_stem.xml` | 846 | 20 | `1a2dc826387e323299f1b7d0de8916c7d1861c39cc824261b461c5da3ca3ff78` |
| `src/interface-stembox/dispositions/stembox_padfx_cells.xml` | 1675 | 14 | `a6799a744bcc9d79dbd4644516374783e2d8365b06d9ee7a4f597db69f5f7074` |
| `src/interface-stembox/dispositions/stembox_samp_row.xml` | 728 | 13 | `d476570865fccbd1235f841c7ca6cf95741d83f8742bdd81e61a005fef5b1fd7` |
| `src/substrat/dispositions/stembox-racine.xml` | 12965 | 237 | `2526ac9b6ebf247b9f77f7cd7f6f9fb853d8c1e23cc8be38cac1839ffb2640ac` |
| `ressources/theme-client/style-stembox.qss` | 45566 | 897 | `6eb54031690409420beb8950540903fc11487964543d0328963a741d57a6283e` |
| `/Users/joakimrosenfeldtpedersen/Library/Application Support/stembox-substrat/usersettings.json` | 3918 | 113 | `6332ac60e8a22da3d7ad48f29f5fc442981ba52721efec932f37d63a2ff3d1a3` |
| `/Applications/Stembox.app/Contents/Info.plist` | 1405 | 41 | `630ee9934755fae0157cfc495b149265230adf09e3c48e87510d149f8d944725` |
Coverage is complete for the requested static files. Mixer widgets, native library columns and rows, custom pad action internals, effect/sampler enums, second-screen window placement, and unpersisted preference defaults remain unresolved by this evidence. Comments naming those behaviors are retained as descriptions, not runtime verification.
