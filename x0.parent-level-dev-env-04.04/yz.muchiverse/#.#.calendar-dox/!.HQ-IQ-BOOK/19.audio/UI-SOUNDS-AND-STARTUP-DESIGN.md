# LyfeDesk startup sound and UI sound effects (design, 2026-10-23)

Owner's ask (2026-10-23): "a generated audio file to play at startup; and start giving sound effects to menu movement / clicks, etc (mutable in settings)". This is a design only; nothing here is built. Labels: **READ** = from files, **RAN** = I ran it today, **PROPOSED** = design, **JUDGEMENT** = opinion, **NOT KNOWN** = unmeasured.
User-visible text says **LyfeDesk** (AGENTS.md); internal ids keep `livedesk`.

## 1. What exists (READ unless marked)
| Thing | Where | Use here |
|---|---|---|
| Procedural SFX renderer, deterministic (same args + seed = same bytes), 22.05 kHz mono 16-bit, waves sine/square/saw/triangle/noise, sweep, attack/decay, 2-pole low-pass, argument rejection | `&.hq-apps/sfx-hai/ops/sfx_render.c`, proof table in `SFX-RENDERER.md` | the sound generator for single blips. It is proof-gated: do not change it without redoing its table |
| Recipe bank `SFX | id | tags | kind | params` incl. `blip_select` (sine 660 Hz, 80 ms) | `&.hq-apps/sfx-hai/sfx.pdl` (4 rows) | model for our row format |
| The four numeric checks (duration, peak, dominant frequency, DC) | `ops/sfx_make.c`, `_shared-lib/audio_dsp.c` | reuse as the validator for every UI sound |
| `audio_fx` (pitch, formant, bright, warm, ...) | `_shared-lib/ops/audio_fx.c` | optional polish of a startup sound |
| Kokoro TTS on debil, per-line wavs | `19.audio/REMOTE-NARRATION-DEBIL.md` | option B for startup (a spoken "LyfeDesk") |
| Player: `paplay` (pulseaudio server) | `pet_voice.sh`, sfx-hai | the stand-in player today |
| Settings as `key=value` lines in `#.desktop/hq_ui.pdl`, **live-reloaded** by `khtpm_core_render` (e.g. `click_two_step`), written by read-modify-write toggles (`desktop_toggle_click_two_step`, line ~16562) and offered in the taskbar-settings window (`CLICK_TWOSTEP_TOGGLE`) | `khtpm_core_render.c`, `&.widgits/taskbar-settings/` | where "mutable in settings" lives; same pattern, no new mechanism |
| Input choke points in the renderer: `kh_nav_step(dir)` (focus moves), `activate_focused()` (Enter / activate), `click_focus_then_activate(hit)` (mouse), `dock_nav_step`, `poll_agent_history()` (relay input) | `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` (24,100 lines) | the cue call sites: a handful, not hundreds |
| Autostart: `LAUNCH | tool-bar ... 'boot'`; the taskbar manager spawns the desk in `ktb_init()` | `$.crypts/autostart.pdl`, `khtpm_taskbar_manager.c` | where the startup sound is triggered |
| No sound hooks in the renderer today; no mute setting anywhere in `hq_ui.pdl` | grep | so this is new, and new code will not collide with old |

**RAN today (this machine, load ~2.4):** `sfx_render` made a 3,572-byte blip; `paplay` of that 80 ms blip took **0.09 to 0.15 s wall** (so a process start per cue costs tens of ms plus a fork); server name `pulseaudio`, `libpulse-simple` headers present (`/usr/include/pulse/simple.h`), `alsa/asoundlib.h` present; the default sink was **SUSPENDED** (the first sound after idle may wake it and arrive late: NOT KNOWN how late).

## 1b. Owner decisions, 2026-10-23
1. Defaults ON (UI sounds and startup), volume 40. 2. Startup = **option C**: chime, then a spoken "LyfeDesk" (the chime half is built in U1; the spoken half is a later step on debil + `audio_fx`). 3. **Sound themes are selectable in settings** (`sound_theme=<id>` in `hq_ui.pdl`; the settings window lists the `THEME` rows). 4. First cue set: menu movement and activate first, then window focus/open/close, toggles and refused (all nine are already rendered; hooking is stepwise).
Consequences: sounds are keyed by (theme, id); every theme must define every sound a cue names (the renderer enforces it); the player loads the chosen theme and reloads on a `sound_theme` change.

### 1c. Startup brief, 2026-10-23 (BUILT in the renderer, not yet listened to)
"A fast-paced contrapuntal arpeggio, like the Sega / Sonic startup and trail-off; shimmering echo and reverb." Built as: two lines in contrary motion (one rises C5 to B6, one falls G6 to G4, sixteenth notes at 168 bpm) over a low C3 pedal, landing on a held chord, with an octave-up shimmer send, an echo, a Schroeder reverb and a cosine fade; about 2.4 s, peak 0.46 (soft) / 0.41 (retro), within the 2.5 s / 0.5 startup limits. The Sega *chant* itself is a voice: option C's spoken half ("LyfeDesk") is the closest equivalent and is still to do.

## 2. Decisions this design takes (PROPOSED)
1. **Never block or fork in the render loop.** A cue is one short append to a marker file; a separate small player process does the sound. This is the house marker rule (change detection = append-only file growth) and the house sharing rule (stateful + 2 consumers = op + file/IPC, not a header).
2. **All sounds are generated, never recorded.** Rows in a data file are rendered by a deterministic C op into wav files at build time; nothing is synthesised at click time. Same seed and rows give the same bytes (the same gate as rpg-dust R1).
3. **One mute truth:** keys in `hq_ui.pdl`, read live. Muted = the renderer does not even append cues.
4. **Agent/relay input is silent by default.** Tests drive windows through the relay (thousands of keys); they must not make noise. Cues from relay-origin events are dropped unless `sound_relay=1`.
5. **The sounds are judged by the owner's ear**, not by a number. The numeric checks only reject broken files (silent, clipped, clicky, too loud).

## 3. Pieces

### 3.1 The data: `#.desktop/sounds/ui_sounds.pdl` (PROPOSED)
```
SECTION | KEY | VALUE
SEED   | ui | 20261023                                       one seed; every noise source derives from it
SOUND  | startup    | notes | tempo=84 seq=C5:1 E5:1 G5:1 C6:3 vol=0.5 wave=triangle attack=0.01 decay=0.25 reverb=0.2 | LyfeDesk coming up, about 1.8 s
SOUND  | nav_move   | blip  | wave=sine f0=520 dur=0.025 attack=0.002 decay=0.02 vol=0.18 | focus moved one row
SOUND  | activate   | blip  | wave=triangle f0=660 f1=880 dur=0.07 vol=0.35 | Enter / click on a button
SOUND  | focus_win  | blip  | wave=sine f0=440 f1=520 dur=0.05 vol=0.25 | a window took focus
SOUND  | open_win   | blip  | wave=triangle f0=392 f1=784 dur=0.12 vol=0.35 | a window opened
SOUND  | close_win  | blip  | wave=triangle f0=784 f1=392 dur=0.12 vol=0.3 | a window closed
SOUND  | refused    | blip  | wave=square f0=180 dur=0.09 lp=900 vol=0.3 | nothing to do / blocked
SOUND  | toggle_on  | blip  | wave=sine f0=600 f1=800 dur=0.06 vol=0.3
SOUND  | toggle_off | blip  | wave=sine f0=800 f1=600 dur=0.06 vol=0.3
CUE    | nav_move   | sound=nav_move | min_gap_ms=60 | category=ui
CUE    | activate   | sound=activate | min_gap_ms=40 | category=ui
...
```
`SOUND` = what a sound is (rendered once). `CUE` = which UI event plays which sound and how often it may fire. A new sound or event is a new row, not new code. Kind `blip` reuses `sfx_render`'s parameters; kind `notes` (a short melody, layered) needs a new small renderer because `sfx_render` has no sequencing or layering and must not be changed (JUDGEMENT: a separate `ui_sound_render` op that text-includes `audio_dsp.c` is cheaper than re-proving `sfx_render`).

### 3.2 The renderer: `ui_sound_render` (PROPOSED, new C op)
`ui_sound_render <ui_sounds.pdl> <out_dir>` writes one wav per `SOUND` row, 44.1 kHz mono 16-bit as built (pulse upmixes to the stereo sink; the design first said stereo), fixed fade-in/out of at least 2 ms (no clicks). Checks per file, reusing `audio_dsp.c`: duration within 20 percent of the row, peak at most 0.7, **RMS at most a set ceiling** (a startup sound must not be louder than a click by accident), DC under 0.01, not silent. Exit 1 with the failing row named. Run by `button.sh build` so a fresh clone has the files (compiled programs and generated sounds are in no branch; same as today's binaries).

### 3.3 The cue line and the player (PROPOSED)
- **Writer** (in `khtpm_core_render.c`, one inline function `ui_cue(name)`): if `g_sound_enabled`, append `<monotonic_ms> <pid> <name>\n` to `#.desktop/ui_cue.txt` (open with `O_APPEND`, write one line, close; atomic for short lines). Per-process: at most one cue per `min_gap_ms` per name. Also skips when the event came from the relay.
- **Player** `ui_sound_player` (new C op, one process per desktop, started from autostart next to `world-manager`): at start, loads every wav in `#.desktop/sounds/` into memory and **opens one `pa_simple` stream** (libpulse-simple, already installed); at idle it checks the cue file size every ~30 ms (stat, no read when unchanged), reads new lines, and writes the PCM to the stream. It owns what a single process cannot: **global coalescing** (several windows are separate processes, so a mouse crossing three windows can fire three cues; the player keeps one `min_gap` per name across all pids), dropping cues older than 150 ms (a late blip is worse than none), a global cap of simultaneous sounds, and truncating `ui_cue.txt` when it exceeds 64 KB while nothing is pending. On start it seeks to the end of the file (no replay of old cues). No pulse server: it logs once and exits; the desktop is unaffected.
- **Why not `paplay` per cue:** RAN above, 90 to 150 ms wall per 80 ms sound and a fork per keypress on a weak machine; also overlapping cues would fight.
- **Test hook:** `ui_sound_player --dry-run <log>` writes a line `played <name> latency_ms=<n>` instead of calling pulse. All routing, coalescing and mute logic is tested without a sound card and without ears; only "does it sound right" needs a person.

### 3.4 Settings (PROPOSED), in `#.desktop/hq_ui.pdl`
```
sound_enabled=1      master switch (0 = renderer appends nothing, player idle)
sound_volume=50      0..100, applied by the player (scales PCM; also cap 100)
sound_ui=1           menu movement, clicks, window open/close, toggles
sound_startup=1      the startup sound
sound_relay=0        1 = also play cues caused by relay (test) input
```
The taskbar-settings window gets a **Sound** group: master On/Off, volume minus/plus, "UI sounds" toggle, "Startup sound" toggle, a **Test** button (plays `activate`). They follow the existing `CLICK_TWOSTEP_TOGGLE` pattern: a click event, a read-modify-write of the one key, live reload. Shared-layout rule applies (khtpm-shared-layout-caution): the new rows must not mutate window size non-idempotently.
Quick mute without opening settings: an environment override `LYFEDESK_MUTE=1` (for scripts), and an optional key chord is **not** proposed now (JUDGEMENT: more key grabs have already caused trouble, see the cursword note).

### 3.5 The startup sound (PROPOSED)
- **Option A (recommended): procedural jingle** from `SOUND | startup | notes | ...` with the `SEED`; the owner can tweak tempo/notes/seed and rebuild; a different seed gives a different (still tuned) variation. This matches "a generated audio file" and the rpg-dust "everything from a seed" habit.
- **Option B: a spoken "LyfeDesk"** from Kokoro on debil (the NIGHT narration route) plus `audio_fx`, rendered once and saved as a file. Needs a network host at build time only.
- **Option C: A then B** (chime, then the word). Decide by ear; the doc does not choose for you.
- **When it plays:** once per cold start, after the taskbar manager has spawned the desk, triggered by `ui_sound_player` itself on its first launch of the boot (stamp file `#.desktop/sounds/startup_stamp.txt` with the boot id; not on `reset`/`restart` loops, so a restart storm does not repeat it). Skipped if `sound_enabled=0` or `sound_startup=0`.
- Length at most 2.5 s, peak at most 0.5 and below the loudest UI sound plus a margin; fade-out over the last 150 ms.

### 3.6 Which UI events get a cue (PROPOSED first set)
| Event | Call site | Rate rule |
|---|---|---|
| focus moves (arrow / tab / digit jump) | `kh_nav_step`, `dock_nav_step` | the quietest and shortest sound; min gap 60 ms so a held key is a soft tick train, not a buzz |
| activate (Enter, second click) | `activate_focused()` | min gap 40 ms |
| click that only focuses (two-step mode) | `click_focus_then_activate` | uses `focus_win` |
| window open / close | window spawn in the taskbar manager; the close combo | one each |
| toggle switched | the settings toggles | on/off pair |
| action refused | the places that already print a refusal | `refused` |
| typing in `cli_io` | **not in v1** (JUDGEMENT: key clicks while typing get irritating fast; add as its own category later) | |
Optional idea, JUDGEMENT, not in v1: the pitch of `nav_move` steps up a pentatonic scale with the row number, so a menu "plays" as you move through it. Pleasant or silly is an ear decision.

## 4. Build order, each with a test that can fail (PROPOSED)
| Step | What | Test |
|---|---|---|
| U0 | this doc; owner picks defaults and startup option | the open decisions in section 6 |
| U1 | **BUILT 2026-10-23** (`&.widgits/ui-sounds/`, harness `ui_sounds` 108/0; the row format differs from section 3.1: `SOUND | theme | id | kind | params | note`, notes joined with commas `seq=C5:1,E5:1`, mono output). Original plan: `ui_sounds.pdl` + `ui_sound_render` + harness `ui_sounds`: files exist for every `SOUND`, same bytes twice, other seed differs, each numeric check has a negative fixture (silent, clipped, too long, too loud, DC offset) | harness PASS and a code mutant (drop the clip check) is caught |
| U2 | **BUILT 2026-10-23** (`&.widgits/ui-sounds/ops/ui_sound_player.c`, harness `ui_player` 63/0, 12 code mutants all caught; live playback itself unverified by a harness). Original plan: `ui_sound_player` with `--dry-run`: coalescing, stale-drop, global gap, mute, start-at-end, log truncation | harness drives the cue file by hand; mutants: ignore mute, replay old cues |
| U3 | `ui_cue()` in the renderer at 4 call sites + live reload of the keys | relay test on a scratch window: N arrow keys -> expected cue count in the dry-run log, `sound_enabled=0` -> zero, relay input -> zero unless `sound_relay=1`. (Relay passing is not proof for hardware focus bugs: the memory note on relay masking applies; the owner presses keys once.) |
| U4 | settings group + toggles | relay clicks flip each key in `hq_ui.pdl`; the Test button appends one `activate` cue |
| U5 | startup trigger + stamp | cold start plays once, `reset` does not repeat, off switch silences |
| U6 | measure: cue-to-first-sample latency (dry-run log), idle CPU of the player (gate: under 0.5 percent), cost of the append in the nav hot path | numbers from a run, not guesses |
Each step ends in a commit with an honest NOT-done list.

## 5. Risks and unknowns
- **Latency after idle:** the sink was SUSPENDED when probed. NOT KNOWN whether the first cue after idle is audibly late. Mitigation if it is: keep the stream open and cork/uncork, or keep it alive with a silent buffer; U6 measures it.
- **Hot path cost:** `kh_nav_step` runs in the render loop of a 24,000-line file; the append must be one `write`. If it shows up in a profile, move to a shared-memory ring later (the marker rule says file first).
- **Window flood:** many windows, one player; handled by the player's global gap, but two cue sources for one click (focus + activate in one gesture) must be one sound: the call sites use one cue per gesture.
- **Relay masking** (see above): tests cannot prove it feels right.
- **Volume surprise:** the master volume default must be conservative; the startup sound is the one most likely to startle at night.
- **Headless / no sound card / server down:** the player logs and exits; nothing else changes.
- **The renderer file is shared and heavy-collision:** only touch `khtpm_core_render.c` with explicit pathspec commits and a rebuild; running windows do not pick up a rebuild until relaunch.

## 6. Decisions needed from the owner
1. Defaults at first install: UI sounds on or off? (recommendation: **on at volume 40**, startup on; you asked for them; everything is one toggle away)
2. Startup option A, B or C (section 3.5)?
3. Sound character: soft synth blips (proposed) or more retro 8-bit square waves?
4. First cue set as in 3.6, or fewer (menu movement + activate only)?
5. Typing clicks later, yes or never?
6. Is the **pentatonic menu** idea wanted?
7. Should the startup sound seed be tweakable from the settings window, or only in the data file?

## 7. Honest limits
Nothing is built or heard. The proposed sound rows are starting numbers, not tuned. Latency, idle CPU and the suspended-sink delay are unmeasured. Whether a player process per desktop or a ring buffer is better can only be told after U6.
