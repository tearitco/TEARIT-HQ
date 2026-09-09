# pc-hq — documentation index

piececraft-hq is a Unix-desktop-integrated 3-D game: an X11 "hq" window
(`khtpm_core_render` + `pchq-board.xhtpm`) that mirrors a live
board-viewer engine session (prisc VM + `bv_render_3d` raymarch +
`chtpm_parser_pal` Interact machinery). Many moving parts, many
generations of design — hence this index.

## LIVING (read these; they supersede everything under HISTORIC)

| Doc | Scope |
|---|---|
| **`pchq-vs-tpmos.md`** | **THE authority** for the board window: Interact Mode, the input/render pipeline, and the parity gap vs the TPMOS "diamond" standard (fuzz-op / `chtpm_parser.c`). Body = Interact (D1-D9, PR-1..4). Appendix A = the deep loop/pipeline comparison (P1-P7): board-viewer runs the pre-diamond `civ-txt`-lineage `main_module.pal` loop (33 Hz, one-key-per-iter, 4 fork-ops/frame, raymarch every frame) instead of mutaclysm's `game_dispatch` + `sleep 16667` diamond — the structural cause of xelector lag. |
| `PIECECRAFT-HQ-LAUNCH-STANDARDIZE.md` | How the board window is launched: `open_pchq_board.sh` (the standard x11-hq shape), engine-session lifecycle, orphan reaping, `livedesk_proc_list.txt` registration. §6-7 = what landed (incl. the `button.sh` engine-mode split that fixed the blank/small window). |
| **`pchq-vs-muta.md`** | The camera / POV key model vs mutaclysm. The camera math is fully ported in `bv_menu_input.c`. **Landed** (`4e5af97f`, `de5ef0be`, `d565c6e2`): POV keys restored to `1-4` (the `5-8` "one map" remap is abandoned); `0` 2D/3D toggle fixed (projector publishes `canvas_raw` by `render_mode` — 2D → composited `rgb_frame.raw`); double-arrow fixed (renderer routes `13`/`27` to `keyboard/history.txt` only, all other keys to `interact_relay.txt` only); unified `pieces/system/keybinds.pdl` (KEY/VERB/AXIS); full mutaclysm `ops/choice.c` `9`/Enter possession parity. Still open: chrome-free 2D pixel path; ship a real `hero_01/piece.pdl`. |
| **`PCHQ-2D-TILE-VIEW.md`** | **Design (2026-09-09, not built).** The `0` view becomes a real flat RPG-Maker-style tile grid (new `bv_render_2d.c`), not the raymarch's `chtpm_rgb_render` text-chrome fallback. `` ` `` toggles 2D tile ⇄ emoji style. INTERACT permanently on (no arm). Manually-drawn matrix grid at the house `desk_grid.pdl` cell size. One shared context-menu path for 2D + 3D. Supersedes `pchq-vs-muta.md` §6/B1. |
| `PIECECRAFT-HQ-GAME-EDITOR-AND-PLAY.md` | The game itself (edit vs play loops, maps/desks, world gen). |
| `piececraft-hq.md` | Original studio-vision overview. |

## HISTORIC — kept for archaeology; design conclusions SUPERSEDED

These captured real investigations at the time. Their *findings* are
folded into `pchq-vs-tpmos.md`; their *proposed designs* are not the
current plan. Each carries a top-of-file HISTORIC banner.

| Doc | What it was | Superseded by |
|---|---|---|
| `PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md` | grok's FocusOut-disengage / `g_x11_window_focused` design (`74488d53`) | `pchq-vs-tpmos.md` D1/D4/D5/D7 — the renderer-side focus/interact state machine is the problem, not the fix; PR-2 removes it |
| `pc-hq-leg-vs-nu-fix.md` | legacy vs new-khtpm board comparison, WM-managed window arc | `pchq-vs-tpmos.md` §1 (last-good `run_pchq_board_mode()` model) |
| `PLAN-pchq-interact-camera-pov.md` | camera/POV key table + arrow-code remap (200-203→1000-1003) | still-valid key table; interact routing → `pchq-vs-tpmos.md` D2/D8 |
| `PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md` | the run_pchq_board_mode → static-xhtpm port | `pchq-vs-tpmos.md` (the port dropped parity; the doc says how to get it back) |
| `PCHQ-BOARD-HISTORY-INJECTION-CHECKLIST-2026-08-30.md` | relay-file wiring checklist | `pchq-vs-tpmos.md` D2 (the `KEY_PRESSED:` format bug the checklist missed) |
| `PIECECRAFT-HQ-KHTPM-INFO-WINDOW-2026-08-30.md` | the separate info window (`pc_hq_status_manager`) | still descriptive; lifecycle → `PIECECRAFT-HQ-LAUNCH-STANDARDIZE.md` |
| `pc-hq-bugs.md` | running bug log (override_redirect, stale grab, badge) | historic bug archaeology |
| `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN*.md`, `PIECECRAFT-INSCENE-DESKS-IMPLEMENTATION-2026-08-30.md`, `PIECECRAFT-LOCAL-VERIFY-2026-08-29.md` | in-scene-desks era design/verify | historic |
| `09-appendix/09-appendix/6.gl-to-x11-toolbar-piececraft-delegation.md` | GL→X11 toolbar delegation | historic |

## Key code (current)
- `@.apps/piececraft-hq/` — `open_pchq_board.sh` (launcher), `button.sh`
  (terminal/engine), `pchq-board.xhtpm`, `ops/pchq_board_projector.c`,
  `ops/pchq_board_action.sh`.
- `&.widgits/board-viewer/` — `pal/main_module.pal` (the loop to port),
  `default_op.txt`, `ops/bv_render_3d.c` (raymarch), `ops/bv_menu_input.c`,
  `ops/bv_compose_frame.c`.
- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` — the shared X11
  window binary: `handle_key` Interact forward (~7060), `kh_scan_interact_relay`
  (~4510), `kh_interact_append_13` (~4595), `hq_run_event_loop` (~8524).
- Reference: `1.TPMOS…/projects/fuzz-op/`, `pieces/chtpm/plugins/chtpm_parser.c`,
  `pieces/display/renderer.c`; `101.mutaclsym…19.00/pal/game_module_3d.pal`,
  `ops/game_dispatch.c`.

## Related memory
`[[piececraft-hq-launch-and-interact]]`, `[[proc-ledger-consolidation]]`,
`[[tpmos-reference-location]]`, `[[prefer-marker-files-not-mtime]]`.
