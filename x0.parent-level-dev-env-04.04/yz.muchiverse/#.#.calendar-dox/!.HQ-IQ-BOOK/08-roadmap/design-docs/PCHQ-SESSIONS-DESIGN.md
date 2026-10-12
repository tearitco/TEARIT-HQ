# pc-hq sessions: several pc-hq boards at once, one binary set, one instance directory each (design, 2026-10-12)

Status: BUILT 2026-10-12 for the dsr-rpg instance (decisions: env var PCHQ_PKG, instance under the game, no symlinks, no map sharing, dsr-rpg first). What exists: button.sh CODE_DIR/data split, scoped launcher, pchq_instance_init.sh, session-local board_view/canvas_click, host id for the engine session, right-click scripts. Still house-wide: pc_synch_request.txt; instance chunks/maps. (Original design text below.) Written by claude from the owner's words and from reading the real files (launcher, board-viewer launcher, projector sources, rpg-pet daemon, the IRC app).

## 1. The owner's ask (2026-10-12)
> "we need to make it so pc-hq can run separate sessions ... sessions like in the p2p apps, so that multiple controls could run different ports on the same binary ... see irc `044.pal-chat-irc...`"

Why now: dsr-rpg (a carbon copy of rpg-pet, `DSR-RPG-DESIGN.md`) must run next to rpg-pet, and later chess, doom, tsots. Today only ONE pc-hq board can exist.

## 2. Why only one board exists today (read, not assumed)
1. `@.apps/piececraft-hq/open_pchq_board.sh` has a **single-instance guard**: it kills every `khtpm_core_render ... pchq-board.xhtpm` window and every `pchq_board_projector` before it launches. Any second board launch replaces the first.
2. Every pc-hq board is the **same host package**, `@.apps/piececraft-hq`: one `pieces/world_01` (current map/desk), `hero_01`, `xelector_01`, `sun_01`, `moon_01`, `pieces/system/` (maps, `widget_cmds/inbox.txt`, `board_manifest.txt`, `locations.pdl`, `house_root.txt`), `state/ui.txt`. rpg-pet's daemon writes these files directly (15 hard-coded paths in `rpg_pet.c`), and switches game by sending `CONFIRM_START_MAP:<book>` to that one inbox.
3. Four **house-wide singleton files** in `#.desktop/`: `pchq_board_view.txt` (15 refs), `pchq_canvas_click.txt` (7), `pc_synch_request.txt` (4), `pchq_ctx_dir.txt` (3). Two boards would overwrite each other's click and view.
4. About **16 source files** hard-code `@.apps/piececraft-hq` (`pc_menu_input.c` 11, `pchq_board_projector.c` 7, `khtpm_core_render.c` 7, `khtpm_taskbar_manager.c` 5, `pc_hq_status_manager.c` 5, `pc_compose_frame.c` 5, `pc_clock_daemon.c` 5, `launch_hq_info_window.sh` 5, `pchq_board_action.sh` 3, and 7 one- or two-reference files). The package is 231 MB, almost all of it map data (tsots), so an instance must not be a copy of it.

## 3. What already works the way the owner wants (precedents to reuse, not invent)
- **IRC / the p2p apps** (`044.pal-chat-irc...`): the same binaries run many times; each run makes `pieces/sessions/<id>/` and keeps ALL its state inside it (`pieces/keyboard/history.txt`, `pieces/apps/player_app/view.txt`, `debug/frame_history.txt`); `PRISC_PROJECT_ROOT=<session dir>` tells the shared binary which instance it is; two users = two sessions (`sessA`, `sessB`) on different ports.
- **board-viewer already scopes sessions per host project** (`board-viewer/button.sh`, 2026-08-03 correction): the engine session dir is ephemeral, `bv_state.txt` carries `focused_project_root=<host>`, and the ledger id is `board-viewer:<basename of host root>`, "so each host project gets its own independently-discoverable, independently-refocusable widget session". Every `bv_*` op re-reads `focused_project_root` each frame. So **the engine needs no change**: give it a different host root and it already makes a separate session.
- **layout-studio's sandbox** already launches a board window from another folder with `PCHQ_BOARD_TPL` and its own `state/`.

## 4. The design: a pc-hq *instance* = a host root directory, chosen by `PCHQ_PKG`
- **Instance** = a directory holding only DATA: `pieces/{world_01,hero_01,xelector_01,sun_01,moon_01,display,keyboard,apps,registry}`, `pieces/system/{maps/<its own books>,widget_cmds/inbox.txt,board_manifest.txt,locations.pdl,house_root.txt}`, `state/`, and its board template. `@.apps/piececraft-hq` itself is simply the DEFAULT instance and keeps working unchanged.
- **Same binaries, different root**: the compiled programs stay in `@.apps/piececraft-hq/ops/+x` (and board-viewer, khtpm_core_render). They learn which instance they serve from the environment variable **`PCHQ_PKG`** (absolute path of the instance root), exactly like `PRISC_PROJECT_ROOT` in the IRC app; unset = the default instance. No second compiled copy, no header-link split (house rule).
- **Instance basename must be unique** (board-viewer scopes its ledger by it): `rpg-pet-pchq`, `dsr-rpg-pchq`, ...; a game keeps its instance under its own app folder: `@.apps/dsr-rpg/pchq/`.
- **Maps are not copied.** An instance lists the books it uses in `pieces/system/maps/`; its own books (`dsr-rpg`) live there, shared read-only books (tsots, chess) are reached by an explicit `maps_extra` search path in the instance's `board_manifest.txt`, so 231 MB is never duplicated.
- **Singleton files become per-instance**: `#.desktop/pchq_canvas_click.txt` -> `<instance>/state/canvas_click.txt` (and `.seen`), `pchq_board_view.txt` -> `<instance>/state/board_view.txt`, `pc_synch_request.txt`, `pchq_ctx_dir.txt` likewise. The default instance keeps writing the old `#.desktop` paths too (a compat mirror) until nothing reads them.
- **Launcher**: `open_pchq_board.sh [house] ` with `PCHQ_PKG` set: (a) bootstraps the instance directory if it is missing (`pchq_instance_init`: creates the empty `pieces/` skeleton from `piececraft-hq/defaults`), (b) **scopes the single-instance guard** by instance (kill only windows whose command line carries this instance's template path / projector arg, never another instance's), (c) starts the instance's own engine session (the existing `engine_up`/`button.sh run` path, with the host root = the instance), (d) launches `khtpm_core_render <house> <template> <instance name>`, (e) records the PIDs in the proc ledger with the instance name so proc-mon and quit/reset reap them.
- **Games**: rpg-pet and dsr-rpg get an `@.apps/<game>/pchq/` instance; their daemons take the instance root from an argument / `PCHQ_PKG` instead of the 15 hard-coded `piececraft-hq` paths (one variable `PCHQ` in `rpg_pet.c`); `CONFIRM_START_MAP:<book>` goes to the instance's own inbox. Their viewer gate checks THEIR instance's window (the `SIBLING_APP` guard added to rpg-pet on 2026-10-11 is the stop-gap until then).

## 5. Ports
The p2p apps need ports because they talk across machines. pc-hq boards on one machine do not: the session is a directory, the engine talks to the window through files. **Per-instance ports are therefore not part of v1.** The 3D GPU daemon (`bv_render_3d --daemon`) is per engine session already; its socket / lock names must be checked to contain the session id (phase S2 test), and are the one place a port-like collision could hide.

## 6. Cost and safety
- One engine stack per instance (renderer, two parsers, two prisc, the 3D daemon, clock daemon). With the viewer gate (no window = no render) an idle instance must stay near 1%; measured per phase with all four `/proc` stat fields, per the house CPU standard (`hq-cpu-safety.md`).
- Stale-session reaping in the launcher ("reap stale/orphaned engine sessions") currently deletes every session directory whose engine is not running: it must only delete sessions of ITS OWN instance, or a second launch would delete the first game's live session.
- `quit`/`reset` (`button.sh`) must stop all instances' stacks, not just the default's (leaked engine stacks are the known "real lag").

## 7. Phases (each ends with a screenshot or measurement; the owner's live rpg-pet board is never restarted by a test)
- **S0 design + inventory** (this doc).
- **S1 instance skeleton**: `pchq_instance_init`; `PCHQ_PKG` read in the 4 singleton files + the launcher; scoped guard; scoped reaping. Test with a SCRATCH instance (`/tmp`-style folder under a test app), never touching rpg-pet's running board. Evidence: two board windows open at once (default instance + scratch), each with its own session dir, neither killed by the other's launch.
- **S2 parametrise the ops**: the ~16 files read `PCHQ_PKG`; the 3D daemon naming checked. Evidence: the scratch instance shows its own map; the default instance's `pieces/` files are unchanged (mtime/size listing before and after).
- **S3 rpg-pet on its own instance**: `rpg_pet.c` uses `PCHQ`; `@.apps/rpg-pet/pchq/` instance; `open_board.sh` sets `PCHQ_PKG`. Evidence: the owner's rpg-pet board unchanged in behaviour; CPU not worse.
- **S4 dsr-rpg phase 0 on its own instance** (carbon copy, `DSR-RPG-DESIGN.md`): both boards open side by side.
- **S5** chess / doom / tsots as instances; retire the compat mirrors of the `#.desktop` singletons.

## 8. Risks
- Shared hot files (`khtpm_core_render.c`, `pc_menu_input.c`) are edited by other agents: every commit is index-only with the hunks reviewed (memory `pathspec commit sweeps whole file`).
- A missed hard-coded path silently writes to the default instance: the S2 evidence step (default instance files unchanged while the scratch one is driven) is the guard; also a static check `grep -c piececraft-hq` on each converted file.
- tsots map event packages reference `@.apps/piececraft-hq/...` in their own `cmd_N.sh`: they stay valid for the default instance; an instance that wants tsots events needs those paths resolved from `PCHQ_PKG` (out of scope until S5).
- Running two 3D engines at once on the weak machine: measured before promising 3D in both; 2D first.

## 9. Open questions (a recommended answer each)
1. **Env var or argument?** `PCHQ_PKG` env var (inherited by every child op, no per-op argv change) with the default instance when unset. *Recommend: env var.*
2. **Where do instances live?** Under their game: `@.apps/<game>/pchq/`. *Recommend: yes.*
3. **Compat mirror of the `#.desktop` singletons** for the default instance until every reader moves. *Recommend: yes, drop in S5.*
4. **Do dsr-rpg and rpg-pet share maps?** No: each has its own book dir in its own instance. *Recommend: no sharing.*
5. **Order.** S1 first on a scratch instance so the owner's live board is never touched. *Recommend: yes.*
