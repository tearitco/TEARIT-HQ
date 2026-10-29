# 🌈 Settings > Theme > Custom: the prism (2026-10-28)
Labels: **SAID** owner's words, **RAN** I ran it, **MINE** my proposal, **OPEN** undecided.

## SAID
"For the settings menu, for theme, I want to add a 'custom' theme, and if clicked it has a default 'rainbow' theme made with a prism of shapes and color/light hex algo, but 4 vars of that theme can be adjusted to make different look: algo, shapes, color pair or w/e." Answers given: drawn as **window backgrounds**; the 4 variables are **shape / light algorithm / hue / brightness**; produced by **a small C op that writes an image**.

## Built (slices 1 and 2, RAN)
| Piece | What | Evidence |
|---|---|---|
| `prism_op` | renders the prism: shape 0-3 (triangle, hexagon, diamond, circle) x algo 0-3 (radial, linear, spiral, noise) x hue 0-359 x light 0-100, size 64-1024; writes `.rgba` + `.png`; default rainbow = 1,2,0,60 | harness 26/0, 12 mutants; PNG decoded independently |
| `prism_settings_op` | one control step (shape/algo wrap, hue +-30 wrap, light +-10 clamp, Custom on/off, reset) saved in `#.desktop/livedesk_prism.pdl`, then re-renders `#.desktop/prism_theme.{rgba,png}` | harness 37/0, 17 mutants |
| `swatch_picker_manager` | routes `PICK:PRISM_*` to the op; **ignores a non-digit PICK** (before: `atoi("PRISM_...")` = swatch 0, i.e. it would have advanced a real theme pick) | harness 11/0 on the real manager; the OLD manager fails 7 cases; 4 mutants |
| `taskbar_settings_projector` + `taskbar-settings-pal.xhtpm` + css | 5 new labels and 13 rows (switch + 4 variables with step buttons); the old 24 output lines are unchanged (diffed) | harness `prism_settings_wiring` 28/0, 10 mutants (every template action is accepted by the op, every label exists) |

## Not built (said plainly)
- **Slice 3: nothing draws the prism yet.** The renderer has **no background-image support** (searched `khtpm_draw_core.c` / `khtpm_core_render.c`), so the Custom switch saves the variables and renders the picture but the windows look the same. MINE: slice 3 adds a cached background draw in the shared draw core (rehearsed on a staging copy, CPU measured with a browser open) and picks readable text colours from the image. OPEN: owner go.
- No live window was opened. The compiled `+x` binaries of the manager and the projector are **not** rebuilt for the live taskbar; the running window keeps the old ones until they are rebuilt and the window relaunched.
- No preview picture inside the settings window (no image element exists); the picture is at `#.desktop/prism_theme.png`.
- The colour pair idea ("color pair") was not one of the 4 chosen variables.

## To try it live (MINE, owner decides; back up first)
Back up `#.desktop/livedesk_theme.pdl`, `taskbar_settings_state.txt`, `hq_ui.pdl`; `sh '&.widgits/_shared-lib/ops/build_prism_settings_op.sh'`; rebuild the projector (`ops/build_taskbar_settings_projector.sh`) and the manager (taskbar ops build), relaunch the settings window. Do NOT press Enter on a focused swatch (it applies a real theme).
