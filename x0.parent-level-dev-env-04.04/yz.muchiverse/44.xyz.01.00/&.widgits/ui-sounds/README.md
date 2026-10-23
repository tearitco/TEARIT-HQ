# ui-sounds (U1, 2026-10-23)

LyfeDesk startup sound and UI sound effects as data. Design: BOOK `19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md`. **Nothing plays yet and nobody has listened**: this folder only makes and checks the sound files.

| File | What |
|---|---|
| `ui_sounds.pdl` | The `startup` sound (both themes) is a fast arpeggio at 168/180 bpm in two voices in contrary motion over a low C pedal, then a shimmering echo + reverb and a cosine trail-off, about 2.4 s (owner brief 2026-10-23: contrapuntal, Sega/Sonic-like, shimmer). `SEED`, two `THEME`s (`soft`, `retro`), 9 sounds per theme (`nav_move activate focus_win open_win close_win refused toggle_on toggle_off startup`), and the `CUE` table (event -> sound, min gap, category). Starting numbers, not tuned. |
| `ops/ui_sound_render.c` | `ui_sound_render <ui_sounds.pdl> <out_dir>` renders `<out_dir>/<theme>/<id>.wav` (mono 16-bit 44.1 kHz, 2 ms fade-in, 5 ms fade-out) and checks each file: duration, peak 0.02..0.7, rms <= 0.2, DC; a sound named by a `startup` cue must also be <= 2.5 s with peak <= 0.5. Every theme must define every sound a CUE names. |
| `ops/ui_sound_player.c` | U2: `ui_sound_player --pdl --sounds --settings --cues [--dry-run LOG] [--drain]`. Reads cue lines `<monotonic_ms> <pid> <name>` from a marker file (live: starts at the END, polls every 30 ms, truncates when big and consumed) and decides per cue: unknown / muted / ui_off / startup_off / stale (> 150 ms, startup exempt) / coalesced (global per cue name, any pid) / volume 0 / played (theme from `sound_theme`, falls back to the first theme). Settings (`sound_enabled sound_volume sound_ui sound_startup sound_theme` in `hq_ui.pdl`) are re-read for every batch. Live playback: one pa_simple stream, mono 44.1k, sounds in order. No sound server: says so and exits 0. |
| `ops/build_ui_sound_render.sh`, `ops/build_ui_sound_player.sh` | `-Wall -Wextra -Werror` build (shared `audio_dsp.c` is included with `-isystem`: it is not warning-clean and is not ours). |

Run: `sh ops/build_ui_sound_render.sh && ops/+x/ui_sound_render.+x ui_sounds.pdl /tmp/snd`. Harness: `ui_sounds.pal` (renderer) and `ui_player.pal` (player, 63 checks: exact-log drain cases + live cases), the renderer one has 150 checks incl. 41 negative fixtures, tree-hash determinism (twice = same bytes, other seed = different noise bytes).

`notes` sounds can have up to three simultaneous voices (`seq seq2 seq3`, `wave2 wave3`, `gain2 gain3`) and `shimmer`, `echo=<ms>,<fb>,<mix>`, `reverb`, `fade` (see the header of `ops/ui_sound_render.c`).

Change a sound: edit its `SOUND` row (both themes), rebuild the files, listen. Add a theme: a `THEME` row plus a `SOUND` row for every sound id (the renderer refuses an incomplete theme). Add an event: a new sound id in every theme plus a `CUE` row.

Not built yet: starting the player from autostart, real playback has never been run by a harness (only the decisions are tested; I played the wavs once by hand with paplay), hooks in the renderer (`ui_cue()`), the `sound_*` settings keys and the theme picker, the startup trigger, the spoken half of startup option C, wiring the render into `button.sh build`. Known gap: the 0.7 peak ceiling is subsumed by the rms ceiling for every sound the current kinds can make.
