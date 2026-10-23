# ui-sounds (U1, 2026-10-23)

LyfeDesk startup sound and UI sound effects as data. Design: BOOK `19.audio/UI-SOUNDS-AND-STARTUP-DESIGN.md`. **Nothing plays yet and nobody has listened**: this folder only makes and checks the sound files.

| File | What |
|---|---|
| `ui_sounds.pdl` | `SEED`, two `THEME`s (`soft`, `retro`), 9 sounds per theme (`nav_move activate focus_win open_win close_win refused toggle_on toggle_off startup`), and the `CUE` table (event -> sound, min gap, category). Starting numbers, not tuned. |
| `ops/ui_sound_render.c` | `ui_sound_render <ui_sounds.pdl> <out_dir>` renders `<out_dir>/<theme>/<id>.wav` (mono 16-bit 44.1 kHz, 2 ms fade-in, 5 ms fade-out) and checks each file: duration, peak 0.02..0.7, rms <= 0.2, DC; a sound named by a `startup` cue must also be <= 2.5 s with peak <= 0.5. Every theme must define every sound a CUE names. |
| `ops/build_ui_sound_render.sh` | `-Wall -Wextra -Werror` build (shared `audio_dsp.c` is included with `-isystem`: it is not warning-clean and is not ours). |

Run: `sh ops/build_ui_sound_render.sh && ops/+x/ui_sound_render.+x ui_sounds.pdl /tmp/snd`. Harness: `&.widgits/_shared-lib/harness/ui_sounds.pal`, 108 checks incl. 41 negative fixtures, tree-hash determinism (twice = same bytes, other seed = different noise bytes).

Change a sound: edit its `SOUND` row (both themes), rebuild the files, listen. Add a theme: a `THEME` row plus a `SOUND` row for every sound id (the renderer refuses an incomplete theme). Add an event: a new sound id in every theme plus a `CUE` row.

Not built yet: the player process, the cue file, hooks in the renderer, the `sound_*` settings keys and the theme picker, the startup trigger, the spoken half of startup option C, wiring the render into `button.sh build`. Known gap: the 0.7 peak ceiling is subsumed by the rms ceiling for every sound the current kinds can make.
