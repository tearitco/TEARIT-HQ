# Corrections to the pc-hq levels review packet (after verifying the reviewer's answers against the code)

2026-10-07, claude. The reviewer (`00-REVIEW-PACKET-APPENDED.md`) had **no source files** ("source-refs folder was not uploaded"), so several answers rest on my packet's wrong statements. I checked the code; this file lists what was wrong, mine and theirs. The reviewer's own file is left untouched.

## A. Errors in MY packet (findings 2 and 5)
1. **"The active level is `mineclonia_sample`" is WRONG.** `board_config.txt` is a **dead key**: comments dated 2026-09-15 in `ops/pchq_board_projector.c` (~596-620) and `ops/pchq_board_action.sh` (~116-175) say nothing in the engine reads it for level selection and that earlier code writing it was "pure dead writes". The **real active map** is `pieces/world_01/state.txt` key `map_id` (empty or absent = the flat/procedural **default** world). The live `world_01/state.txt` has **no `map_id`**, so the loaded world is the procedural default (seed 1789563438, tick 375), not `mineclonia_sample`.
2. **"`open_pchq_board.sh` always opens at level 1" was misread.** That "level" is the **z floor**: the launcher resets the hero and xelector to `floor_z + 1` (floor_z from `board_manifest.txt`, currently 16). It is not a map. The launcher takes **no level argument** and does not read `board_config.txt`.
3. **The real map-load path** is the `CONFIRM_START_MAP:<map_id>` line appended to `pieces/system/widget_cmds/inbox.txt`, handled in `ops/pc_menu_input.c`, which runs `pc_generate_chunk.+x` (the File menu's `file-hq` / `load-map` verbs write exactly that line).

## B. Reviewer answers that do NOT hold
- **Q1/Q2/summary 2 ("make `maps.pdl` authoritative; parametrize `open_pchq_board.sh <level_id>` to write `active_level` into `board_config.txt`; the renderer reads it as it already does"):** it does not, so that change would be a **no-op**. A correct desktop load path must send `CONFIRM_START_MAP:<id>` to the running engine's inbox (or set `world_01/state.txt` `map_id` before the engine starts and generate). The principle of one list is still sound, but the list (`maps.pdl`) is **not** what the engine reads today either (the Desk/File menus read each map's `game.pdl`).
- **Q3 ("pieces state files are the truth; every view reads them"):** broadly right and matches the house; but `world_01/state.txt` (map_id, tick, seed) is part of that truth and the stale `board_config.txt` must not be.
- **Q7 step 1 (assert `board_config.txt` active_level=default):** wrong check; the right assertion is `world_01/state.txt` `map_id` empty (default) or equal to the expected id.
- **"`khtpm_core_render.c` already reads board_config.txt":** unverified and, per the code comments above, false for level selection.

## C. Reviewer answers that hold (adopted)
Add `kind` to the level registry; treat `default` as the first planet page, `mineclonia_sample`/`cdda_sample` as samples, `test_*` as tests and **list them** (drift is real: `maps.pdl` omits `test_terraces` and `test_walls`); leave the three duplicate `tick_animals` copies until the event/node design lands (add cross-reference comments only); measure the raymarch viewer on chunked data **before** committing to the voxel planet; swappable nodes are a **configuration** mechanism and the per-tick compiled code reads cached values, with `LINK` rows traversed on tunable change; CC-BY-SA attribution must **ship** with any distributed build (a credits file), reference-in-place is fine for internal use; page identity by permanent id with paths resolved from the id, containment rules by `kind` to stop cycles; reconcile the sandbox doc with the fact that `default` already is a planet page.

## D. What the owner asked next
"Live check: you do all through a savable, weightable, conceptable harness (in events / pal)." So the live check is **a pal harness whose case rows are Watch records** (counted reliability, scorable by TEARIT, concept-tagged), not a manual round. Status: the state/generation/registry part is being built in alpha (`PCHQ-LEVELS-HARNESS-REPORT.md` when done). **Not covered by that harness:** the actual desktop window opening and showing the sun and chicken; that needs the per-process relay on the live desktop and a screenshot op and has not been run.

## E. Open item to decide
Where should the **level registry** live and who reads it: keep per-map `game.pdl` as the engine reads today and make `maps.pdl` a derived index (a harness case checks they agree), or migrate the engine to read `maps.pdl`? Smaller and safer: derive and check, do not migrate.
