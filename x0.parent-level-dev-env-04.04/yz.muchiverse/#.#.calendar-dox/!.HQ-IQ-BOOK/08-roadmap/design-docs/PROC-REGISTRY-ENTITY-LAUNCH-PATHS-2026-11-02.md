# Entity launch paths and the process registry (investigation, 2026-11-02)

Question (owner): do the other entities get registered too, and could placing from the palette do it automatically?
Status: option A BUILT AND TESTED 2026-11-02 (section "Result" at the end). The table below is the state BEFORE that change.

## Every way a desk entity process (`khtpm_entity.+x <pal>`) is started
| # | Path | Starts it with | Registered now? |
|---|------|----------------|-----------------|
| 1 | Taskbar manager: boot / session load / desk switch (`livedesk_spawn_desk`, `livedesk_place_pal`, `livedesk_ensure_cursword`) | `ktb_system_recorded` | YES (v1 ledger as before, plus v2 via `register-v1`, 2026-11-02). Live: 6 rows after reset. |
| 2 | Palette "place" -> `palettes_menu.sh place` -> `tp_place_desktop.c` (line ~436) | its own `system("setsid '<exe>' '<dir>' &")`, no manager involved | NO (read only). Not in the lint baseline either: `spawn_lint` looks for `setsid()` calls and shell `setsid nohup`, not a `setsid` inside a `system()` string. |
| 3 | Per-pet / per-app `button.sh` (`@.apps/pets/pieces/{cat,chicken,dog}`, `asa-&-ava/pieces/{asa,ava}`) | bare `setsid nohup ... &` | NO. These 5 ARE in the baseline. |
| 4 | `$.crypts/scrypts/openall/run.sh` (restore list) | bare `setsid nohup` | NO, in baseline. |
| 5 | Window modules (`<module>` managers of every HQ window) | `launch_module` in `khtpm_core_render.c` | YES (2026-11-02): the child registers itself before `execv`, owner = the renderer. |

So: no, placing from the palette does not register automatically. It bypasses the manager (path 2), which is the same kind of stray-maker the registry exists to stop. Whether the manager later notices the tile (a placed tile is a real pal under the active desk, so the next desk load runs path 1 for it) was not tested.

## Options (not built)
A. **Self-register in the entity renderer (recommended).** `khtpm_core_render.c` `tp_main()` (tile/entity mode, argc==2) records itself at startup through `proc_reg_op register` (lifetime `house`, role `entity`, app = the pal's folder name). One site covers paths 1-4 and any future launcher; re-registering a pid replaces its row, so the manager's row is simply refreshed. Cost: one extra fork/exec per tile start. Caveat: it registers at start of the process, not before it exists, so a crash in the first milliseconds leaves no row (the audit would still list it as unregistered, which is the intended signal).
B. Fix each launcher (funnel for paths 2-4). Needed anyway for the lint baseline, but leaves new launchers unprotected.
C. Both: A for the guarantee, B to shrink the baseline.

## Also found
- `spawn_lint` blind spot: `system("... setsid ... &")` strings in C. Extend the lint, then expect new baseline rows (tp_place_desktop is one; others unknown until it is run).
- Known gap: path 1 tiles are `house` lifetime, so closing one tile window does not rely on the registry to end its process (the manager's own close handling does that today; not changed).

## Result (2026-11-02, built + tested)
Option A is in `khtpm_entity.c` (the real source of `khtpm_entity.+x`; `khtpm_core_render.c` carries a duplicate `tp_main` that got the same block) right after the house root is resolved: the entity runs `proc_reg_op <house> register-v1 <pid> entity entity <pal folder>`. `register-v1` (new, `proc_reg_op.c`) writes the v2 row (lifetime `house`) and never touches the v1 ledger, so the manager's own v1 line/owner is untouched. Also this day: `launch_module` (window managers, owner = renderer), the taskbar manager's `ktb_system_recorded` (via `register-v1`), `run_khtpm_strip.sh` (manager + strip through the funnel), `world-manager/button.sh` (registers `$$` before `exec`), `open_hotbar.sh` (funnel).
Tests: (1) reset of the live desktop: audit 12 -> 0 violations, tile geometry identical except one strip 2095 vs 2096 px wide (unexplained, probably unrelated). (2) The palette-style launch (bare `setsid khtpm_entity.+x <pal>`, no manager) on a scratch copy of a pal in /tmp: row appears as `house / entity / <folder>`, audit 0, row gone after kill + `prune`. (3) harness `spawn_lint` 19/0, `proc_registry_v2` 54/0. Not done: a negative control binary without the block (the 12 pre-change violations are the control); the palette's actual place button was not clicked (the launch line it uses was reproduced by hand); the manager's relaunch of a killed tile happened to be seen live (new pid, window back).
Cost: one extra fork/exec of a tiny op per tile start. Footprint: no new runtime files; rows live in `#.desktop/proc_registry.pdl` (already capped, see RUNTIME-FILE-GROWTH-STANDARD).
Still open: extend `spawn_lint` to see `system("... setsid ...")`; fix launchers 2-4 so the baseline shrinks; why the ui-sound player was not running right after the reset (it played the chime, then was gone; stable since a manual start); an accident during the test: I killed a LIVE tile by mistake (the manager relaunched it within seconds).
