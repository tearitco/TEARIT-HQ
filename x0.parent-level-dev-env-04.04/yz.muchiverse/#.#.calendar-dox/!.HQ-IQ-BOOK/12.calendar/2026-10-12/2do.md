# 2026-10-12 night log (claude): dsr-rpg board, pc-hq sessions, the load guard, board CPU

Written at the end of a long session that ended after an overheat crash. Facts below were observed; "not seen" marks what was not.

## Built and pushed (branch claude, main fast-forwarded to b1cfc5e35 at the time)
- **pc-hq sessions** (`design-docs/PCHQ-SESSIONS-DESIGN.md`): several pc-hq boards at once. `PCHQ_PKG` = the DATA root of an instance, programs stay in `@.apps/piececraft-hq` (no symlinks: they do not work on Windows). `button.sh` (CODE_DIR vs data root), `open_pchq_board.sh` (scoped guard/reaping/engine check, `same_instance()` reads `/proc/<pid>/environ`), `ops/pchq_instance_init.sh` (plain copies incl. `pieces/registry`, the HUD font: its absence made the dsr HUD black on black), session-local `board_view.txt` / `canvas_click.txt` (renderer writes, bv_render_2d/3d read first), projector/menu/clock find the instance by `PCHQ_HOST_ID`/`PCHQ_PKG`, right-click scripts take the data root from the window's environment. Seen: dsr board next to the rpg-pet board, rpg-pet's pids unchanged, its world stayed `rpg-pet`.
- **dsr-rpg** (`@.apps/dsr-rpg`, own instance `dsr-rpg-pchq`): six main characters (Rex soldier, Gus chef, Pip clown, Orin king, Maren queen, Fenn fairy) as DB actors 25-30 + party `DsrRpgParty`; greet/reply event pages rewritten per character; ONE island city overmap room (`ROOM ... over | 20 | 13 | 32 | worldmap`, tiles scaled, island on a square ocean) with castle, bank, store, school, hotel (World_B tiles) and five interiors; per-room geometry in the daemon (`g_gr`, `draw_over`, `over_land`); **real RPG Maker autotiles** (`draw_auto`: tile = kind x 48 + shape from 8 neighbours, derived from and verified against the tsots maps; grass kind 0, sea kind 0 draws the shore); selected cast row shows `[^]` (`nav-selected` class, shared draw core); toys entries `dsr-rpg` and `dsr-rpg board` (`design-docs/SANDBOX-TOYS-AND-BOARDS.md`).
- **load guard**: top bar shows `<pid> %<cpu>` (whole machine), red `!%<cpu>` when windows >= 5, cpu >= 85 or temp >= 80 (`livedesk_taskbar.pdl` keys); a tab with the same label on the three boards; marker-driven (`load_state.txt` + `load_changed.txt` size growth).
- **board CPU** (`03-pitfalls/hq-cpu-safety.md` section 5): dsr board window ~28% -> 14% of a core, with the method, the traps and the numbers.

## Decisions / owner direction recorded
- "Marker" branch (worktree `/home/no/staging/marker`, branch `marker`): redo polling to marker-driven for everything in the house; merge into claude and delete `marker` when it all proves working. Created, EMPTY: the profile showed the dsr daemon is only ~2-4% of a core; the cost is in the khtpm windows (reparse/redraw), so the first conversion target should be the window's 16 ms canvas poll tick and its per-tick vars/hash reads.
- Emoji overlays on buildings: dropped. Tiles/emoji style: later.
- HUD/CPU `%` meaning: the top bar and the board tab show the WHOLE machine; a per-window figure was tried (`SELF_CPU`) and reverted at the owner's request.

## Not done / open (in priority order)
1. Verify on screen after a board relaunch: HUD text in the dsr board, `[^]` on the selected row, load tab on the boards, red top-bar colour (needs the taskbar header restarted). None of these were seen working before the session ended.
2. Top bar `%` can read 0 for up to a minute after a manager restart (the dock only republishes on a warning flip or the minute tick).
3. dsr-rpg gameplay: jobs and shops (characters spend money in stores / work there), crime to the castle dungeon, soldiers work at the castle with the king, Harold the hotel janitor, chickens/horses/pigs, fairies, bad guys; interior furniture; the chef has no sprite (Gus wears an elder); chat voice lines are English placeholders.
4. Marker conversion on the `marker` branch (above); layout reuse CPU gain unmeasured on a quiet machine.
5. Bug bounty still open: the random 3D flash while piloting through a door (rpg-pet board).
6. `pc_synch_request.txt` is still house-wide; instance chunks/maps are not game-specific.
