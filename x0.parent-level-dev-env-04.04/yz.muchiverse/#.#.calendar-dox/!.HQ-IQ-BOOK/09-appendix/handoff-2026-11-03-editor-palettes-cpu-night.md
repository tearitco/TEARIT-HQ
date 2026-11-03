# Handoff 2026-11-03: rpg-edit-hq palettes + settings, and the CPU night

Branch `claude` (pushed to origin `claude`, `main`, `grok`). House root = `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`. Read first: AGENTS.md, `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` (22 items), `03-pitfalls/hq-cpu-safety.md` sections 9-10i.

## 1. What landed (commit, what it is)
| commit | change |
|---|---|
| 867d5a08a ... 825eb7df5 | editor Settings tab (`edit_prefs`: `editor_open=last\|own`, `sidebar_start=last\|pets\|palettes\|none`, values `state/prefs.txt`); `%` tab shows no pid; sidebar history `state/sides_history.pdl` |
| d19dd9727 | palette levels: palettes > tile type > sheet > tileset > tiles (`ops/pal_side.c`, nav `board/state/pal_nav.txt`), bounded niced helper (`run_until_written`, refuses when machine >70% busy) |
| 270f6f50d | tile paging past 48: `< previous 48` / `next 48 >`, line `tiles 49-96 of 512`, page resets on pick/back |
| f407884e6 | load governor for the 3D frame stream (`_shared-lib/load_governor.c`) |
| 910978c16 | `frame_history.txt` capped (loop_guard trims 1 MB -> 256 KB) |
| 718d56312 | game clock started by a verb is owned by the resident daemon (was an orphan ticking with all windows closed) |
| ac82fa1e7 | `chtpm_rgb_render` not started for khtpm boards (`PCHQ_ENGINE_MODE=1` + `NO_GL=1`; `FORCE_RGB_COMPOSITOR=1` overrides): 3D camera use 106% -> 72% of a core; engine per-frame profile (`touch pieces/display/.gpu_debug`) |
| 0fb8d0468 | `cpu_gate` reads the die sensor (k10temp/coretemp), not just acpitz |
| 379145d26 | pitfalls doc; guard clears a stale `%` label when no taskbar manager runs |
| 999054e36, 8ae686603, a5ee6bb66 | `spawn_lint` sees `setsid` inside C strings (`c-string-setsid`); 8 of 9 sites now go through `kh_spawn.sh`; baseline keeps only `board_ctl.c:96` (false positive) |
| 4ea92e85f | dead daemon-side `b` build key removed (building = engine placer Enter -> `place_requests.txt`) |

## 2. Rules that bit tonight (short)
- Heavy work only through `cpu_gate <house> run <label> -- <cmd>`; re-check EVERY round of a test loop. A guard kill is our footprint.
- Never `pkill -f '<pattern>'` in a tool shell (kills the shell, exit 144). Select with `ps -eo pid=,args= | awk '$2 ~ /^ops\/.../ {print $1}'`.
- Deletions use `"${VAR:?}/..."`; the safety check blocks bare `rm "$P/${k}..."`.
- Commit with explicit pathspec from the REPO root (`git commit -- <repo-root-relative paths>`); `git add --` first for new files. Never `git add -A`, never stash, never touch `xyzfs/users`.
- Restore live palette state by md5 after tests (`&.widgits/palettes/*_active.txt`).
- A running binary keeps old code: daemons/editor need relaunch after a rebuild; board sessions run their own bv_render_3d copy.

## 3. State left behind
- Editor (rpg-edit-hq) board is running the OLD daemon binary (before the `b` removal); relaunch: `sh @.apps/rpg-edit-hq/board/open_board.sh` through `cpu_gate run`.
- Desktop shell was restarted at 04:22 (`sh _.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh new`); `cpu_guard` restarted with the stale-label fix.
- NOT rebuilt: other petslike boards (rpg-space, rpg-pet, civ-rpgce, ...) still run the old `petslike_core.c` (menu launch not yet registered) until their next build.
- Not exercised live: the five converted launches (petslike ctx menu, open-hai stats, tile-picker x3) and the three pc_menu_input entity-menu launches; only a test `kh_spawn` row and builds were verified.

## 4. Test guide (how to verify, each with evidence)
1. **Gate + health:** `"$H/&.widgits/_shared-lib/ops/+x/cpu_gate.+x" "$H" check` prints busy/house %/temp (die temp should match `k10temp`); `--max-temp 20` must exit 3.
2. **Lint ratchets:** `spawn_lint.+x "$H" <PROC-REGISTRY-BASELINE-2026-11-02.txt>` -> `new=0`; negative test: add `system("setsid nohup x &");` to a scratch .c under the house, expect `NEW c-string-setsid`, then remove it. (Not yet run: do it.) `cpu_lint` -> `new=0` (baseline 569).
3. **Palette paging:** `ops/+x/pal_side.+x "$H" list; pick cdda; pick pngs_normal_32x32; pick furniture; next` -> `pal_more=tiles 49-96 of 512`, `pal_can_prev=1`; 12x `next` stops at `481-512` with `pal_can_next` empty; `back` then re-pick -> page 0. Real window: `dump_frame_png_op.+x <win> out.png` (editor window 1300x802), crop the left 300 px. Restore with `list` then `pets` and md5-compare `_active.txt` files.
4. **3D CPU:** after relaunch (`cpu_gate run`), send 100 `rt` via `bv_keys.+x <session> rt` (0.2 s apart, 20 s), sample /proc/<pid>/stat deltas for ALL house processes (relative-path commands too). Expect no `chtpm_rgb_render` in the session, house ~60-75% of a core during keys, 1% within seconds after; canvas must change (PNG diff of 886x640 canvas at x 208,y 105).
5. **Orphans:** close the window, wait 25 s, list `bv_render_3d|lc_clock|chtpm_rgb|prisc` -> none left.
6. **`%` tab:** `cat '#.desktop/load_state.txt'` live number while the manager runs; kill the manager (by pid) and within a guard sweep the label must read empty (this real path is NOT tested yet, only the helper functions).
7. **Settings tab:** Settings ▾ > editor opens / sidebar start cycles values; `edit_prefs show`; relaunch honours `editor_open`.

## 5. Open items (ordered by my pick)
1. Drive the other palette kinds' tile buttons (df, doom, ohrrpgce, my-palettes) in the real window; palettes list opens scrolled.
2. Rebuild other petslike boards through the gate, one at a time, to pick up the registered ctx-menu launch.
3. Remaining engine cost during 3D use: GPU phase 31 ms/frame, scene load 10 ms, overlay write 7-16 ms (tmpfs per-session display folder would save ~15-20%, needs cleanup trap + guard sweep of `/dev/shm/lyfedesk-*`); the engine alone caps ~14 fps.
4. Settings HQ app (not built; prefs live in the editor's own tab); 5/6 camera key dead code; 125 unregistered spawn sites in the baseline; cpu_lint 569; sprite caches under `&.widgits/palettes/sprites` uncapped; `cpu_guard_alert.txt` awaits owner review; optional busy-check before TTS synthesis.
5. The owner's tell "cpu stays high after the camera stops" was NOT reproduced (engine 0 frames, house 1% after input stops; 400-key flood left no backlog). Next time it happens read the newest `&.hq-apps/proc-mon/state/incidents/*-WARN.txt` and check the `%` writer is alive (the 95% seen was a frozen label, section 10i).
