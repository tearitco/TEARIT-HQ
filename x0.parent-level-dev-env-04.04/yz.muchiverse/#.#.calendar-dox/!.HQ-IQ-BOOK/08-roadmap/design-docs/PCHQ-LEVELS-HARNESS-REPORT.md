# pc-hq levels harness report (2026-10-07)

Harness: `44.xyz.01.00/&.widgits/_shared-lib/harness/pchq_levels.pal` + `cases/pchq_levels.pdl` (pal + case file + the shared case/verdict ops; no new .sh). Run from the harness folder: `/tmp/prisc pchq_levels.pal`, read `results/pchq_levels.txt.verdict.txt` (rows `PASS|case/label`, `FAIL|case/label|seen`, one `RUN|` row per run; the framework's own ledger, nothing new).

All checks run on scratch copies under `/tmp/hc_*` of `@.apps/piececraft-hq` files. `pc_generate_chunk` is compiled into scratch and run with `PRISC_PROJECT_ROOT=<scratch root>`; no `real_project_root.txt` is copied, so its real root is the scratch root and it cannot write to the house. `pc_menu_input` and the clock daemon are never run.

## Verdict

`VERDICT|FAIL|passed=89|failed=4` (expected: the four failures are the strict registry cases that report real drift). Negative test: a copy expecting `floor_z=99` gave `VERDICT|FAIL` with `FAIL|generation/board_manifest-floor_z-16-written|content differs` (copy deleted).

## Findings

| # | Finding | Evidence (case) | State |
|---|---|---|---|
| 1 | `maps.pdl` omits `test_terraces` and `test_walls` (folders with `game.pdl`, load fine) | `registry/FINDING-drift-*` PASS (drift is exactly these two); `registry/registry_complete-*` FAIL | DRIFT, owner decision, not fixed |
| 2 | `maps.pdl` PATH column for `mineclonia_sample` and `cdda_sample` is `maps/<id>/map.txt`, which does not exist (real file: `maps/<id>/desk1/map.txt`); `default` row points at a chunk file | `registry/FINDING-path-drift-*` PASS; `registry/registry_paths_exist-*` FAIL | DRIFT (new, beyond the two missing rows) |
| 3 | `board_config.txt active_level=mineclonia_sample` disagrees with `world_01/state.txt map_id` (tracked copy: `test_walls`); it is a dead key | `active-map-source/board_config_is_stale` PASS | STALE, do not read for selection |
| 4 | Default world pieces are sound: sun/moon `celestial_body` with numeric pos, a chicken row, `default/game.pdl` lists desk1+desk2, every map's desk dir has `map.txt` | `default-world-pieces/*` all PASS | OK |
| 5 | Generation is deterministic (same seed: whole `pieces/` tree identical; different seed differs); writes hero, world, `floor_z=16` manifest | `generation/*` PASS | OK |
| 6 | What loads: all five maps (including the two unregistered) load through the handler's argv `<seed> 0 0 map:<id>`: `world_01 map_id=<id>`, `desk_id=desk1`, hero at tile 1,1. terraces differ from default. The registry is a UI list, not a gate | `map-load/*` PASS | OK |
| 7 | An unknown map id does not fail: rc 0, stderr "falling back to flat", `map_id=` empty. A mistyped `CONFIRM_START_MAP` silently gives the flat world | `map-load/nosuch-*` PASS | Footgun, documented |
| 8 | The tracked `world_01/state.txt` has `map_id=test_walls` (flat default = empty `map_id`; a flat generation is checked to write an empty one) | `active-map-source`, `generation/flat-world-map_id-*` | Note |

## Not covered

- No GUI or window check: nothing proves a pc-hq window shows the level that `map_id` names. That needs the per-process relay (`#.desktop/entity_menu_history/<pid>.txt`) on the live desktop.
- The `CONFIRM_START_MAP` inbox handler itself (game_state write, display pin, clock daemon and trigger watcher launch) is not run; only the generation argv it builds is. Handler dispatch (including the 2026-09-15 off-by-one fix) is therefore unproven here.
- `events.pdl` triggers, desk switching (`CONFIRM_SET_DESK`), sun/moon motion (clock daemon), and the 3D/2D rendering of the generated chunk are not checked.
- Sun/moon/animals are checked in the tracked files only, not regenerated (the generator does not create sun/moon).
