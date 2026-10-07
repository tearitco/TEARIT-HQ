# hai manager handoff — everything built and decided on 2026-10-06 (claude)

Role: the owner appointed claude manager of the handoff / bot system (graveyard 🪦 quests, ghosts 👻, robots, phones, server). Branch `claude` (pushed). This file is the map: what exists, how to prove it,
what is live vs not, what is waiting on the owner, and the landmines found. Read the linked design docs for the "why"; read this for the "where".

## 1. Map of the system (what each thing is, where, how to prove it)

| Thing | Where | Prove it (fresh run) | State |
|---|---|---|---|
| **Quest board (graveyard)** | `^.grave/` (`README.md`, `quests/INDEX.md`, `quests/Qnnn-*/QUEST.md`, `_TEMPLATE`, `ops/quest_new.sh`, `ops/quest_delete.sh`) | `bash @.apps/board-hq/verify.sh` | live data; Q001-Q009 (see index) |
| **Generic board window** | `@.apps/board-hq/` (README there: data-source format, ops, verifier) | `verify.sh --selftest` then `verify.sh` (24 checks, headless) | built + scored; **not linked from the h-ai menu**; one window of mine may be open (`state/quests/window.pid`) |
| **Confirm popup for Backspace** | generic renderer feature `confirm=` (`XHTPM-PARSER-REFERENCE.md`), `khtpm_core_render.c` +34 lines | board `verify.sh` (popup / cancel / Enter checks) | in renderer built 22:00; chat-hai, open-hai, media-3d-hq opted in; keyboard only |
| **Phones (one per entity)** | `&.widgits/_shared-lib/khtpm_phone.c`, `ops/phone_ensure_op.c`, entity folders `<entity>/inventory/zz.phone/`, `^.hai-phone/_TEMPLATE`, index `^.hai-server/phones.index` | `phone_ensure_op.+x --selftest` | live: 55 entities have uid + phone + sprite (migration applied); spawn hook adds one to every new entity |
| **phone.send / server.route events** | `ops/phone_send_op.c`, `ops/server_route_op.c`, `^.hai-server/tunables.conf`, ledger `^.hai-server/ledger.txt` | `bash ^.grave/quests/Q009-first-events-phone-send-and-route/verify.sh` (14 checks) | proven on sandbox |
| **Router loop** | `^.hai-server/router.pal` (template), `button.sh start|stop|status|once` | `bash ^.grave/quests/Q009-.../loop_test.sh` (6 checks) | proven on a COPY. **The live router has never been started**; `start` refuses the live dir without `HAI_ROUTER_LIVE_OK=1` (owner's OK) |
| **Per-user data branches** | `$.crypts/save-user-data.sh`, `seed-user.sh`, `AGENTS.md` section "User desk data is NOT in code branches", design `USER-DATA-BRANCHES-DESIGN.md`, quest Q007 | `sh '$.crypts/button.sh' save-data` | `xyzfs/users` untracked on `claude`; local orphan branches `user/<uuid8>` (+ `jb`, the owner's integration branch, local only, **never push `user/*`**) |
| **Build gate** | `hash_gate.sh`, `build_core_render.sh` CR_SRCS | `bash ^.grave/quests/Q003-build-gate-include-list/verify.sh` + `gate_test.sh` | fixed (every `#included` file listed) |
| **Human-input log (IRL step 1-2)** | renderer `kh_human_log`; `#.desktop/human_input/<pid>.txt`; viewer `$.crypts/irl-demos.sh` | `sh '$.crypts/irl-demos.sh' -n 20` | recording owner's real X key/click events with element context; nothing learns from it yet |
| **Dock + hotbar position memory** | `#.desktop/dock_state.pdl`, `#.desktop/slide_offsets.pdl` | restart the desk | committed; see §4 hotbar note |
| **tb Player: save-game / load-game** | `khtpm_taskbar_manager.c` (ids 161/162), `&.widgits/_shared-lib/ops/game_slot_op.c` | `bash &.widgits/_shared-lib/ops/game_slot_verify.sh` (13 checks) | built; **running taskbar still has the old binary** (restart needed to see it); v1 does NOT restore |
| **Hotbar header picture (desk)** | `@.apps/hotbar-hq` (`hotbar_manager.c` `holder_sprite`, `hotbar-desk.xhtpm`), renderer `no-nav` for toolbar-row items | PNG of the real template through `--dump-and-exit` (see 2do.md) | desk done and built; pc-hq not done; takes effect on a hotbar reopened after the build |
| **Drag and drop reference** | `02-architecture/DRAG-AND-DROP-BETWEEN-MENUS.md` | read | documentation only |
| **h-ai menu rows** | `#.desktop/livedesk_taskbar.pdl` `ai_menu_5..7` (Quest board / Ghost roster / Phones) -> `@.apps/board-hq/ops/open_board_menu.sh <name>` | open the tb h-ai dropdown; menu built from the real pdl and the wrapper opened + closed a board (2026-10-06) | LIVE at once (menu rows are read when the menu opens, no restart). Not in pc-hq |
| **Tasks as event-type pdl, doc entities, pc-hq answer** | `08-roadmap/design-docs/TASKS-AS-EVENT-DATA-DESIGN.md` (§5.5 doc = entity, location = state; §5.6 pc-hq table) | read | design only |
| **Ghost design** | `08-roadmap/design-docs/GRAVEYARD-GHOSTS-DESIGN.md` (§6b placer, §6c assign/work/pending) | read | design only; roster has just `_TEMPLATE`; nothing placeable yet |
| **Phones / robots / server design** | `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` (§3b-3f incl. IRL status) | read | |
| **Save slots design** | `SAVE-SLOTS-DESIGN.md` | read | |

## 2. Commits today on `claude` (newest first; all pushed)

`e0ce3a1ab` docs ghost assign/work · `c1e70670e` tb save-game/load-game · `ec156153c` board create/delete + confirm popup · `3f00921cf` phone directory board · `5fdd3de33` generic board ·
`653b3c688` router loop · `09f0c5b7b` phone sprite · `86ecacde9` build gate · `222cf194a` / `d7c0d89e7` IRL steps 2/1 · `b5a7ce968` phone.send + server.route · `c768a2e9b` seed user · `4561c355e` AGENTS.md rules ·
`83d4ba18c` save-user-data · `9710f4462` hotbar offset persists · `09cd41ae4` untrack xyzfs/users · `e2dea775f` dock rows persist · `83c1fcd59` data-branch design · `c41e7f935` phones migration (55) ·
`7765165fc` phones spawn hook · `354abbb86` merge of origin/opencode (review branch) · `9028e0706` HALO_CHAT v0.1 landed.

## 3. Waiting on the owner (decisions, none blocking the others)

1. **Start the live router?** `HAI_ROUTER_LIVE_OK=1 sh '^.hai-server/button.sh' start` (it moves real entity messages every 2 s; stop = `... stop`).
2. **Restart the taskbar** to see save-game / load-game (I do not restart the owner's desktop; I did once by mistake with `gtk-launch`).
3. **Save-slot restore** design: snapshot copies vs event-ledger replay (`SAVE-SLOTS-DESIGN.md` "Not done").
4. **Ghosts**: who approves `pending` (judge recommends; official LLM or owner decides?), tier filter for `work`, quest card vs draggable rows (`DRAG-AND-DROP-BETWEEN-MENUS.md` §7), glyph per tier, per-ghost spend cap, worktree per ghost.
5. **IRL** = inverse reinforcement learning (learn from the owner's demonstrations)? I assumed so; unconfirmed. Password fields are not yet excluded from the human-input log (do before any wider use).
6. **Hotbar position**: the owner's original was x=1695, y=1465 (saved offset +827). I displaced it to 868 and overwrote the saved offset to 0 earlier. Not touched since the owner said "no stop moving hotbar. i moved it!". Drag it back, or tell me to set it.
7. Other branches (opencode, kilo, grok, main) still track `xyzfs/users`; their owners convert (rules in AGENTS.md). `NNEST-12.00-halo` and kilo attrition worktrees hold tracked copies.

## 4. Landmines found this stretch (each cost real time; do not repeat)

- **dash is `sh` here.** `${var//a/b}`, `[[ ]]`, arrays are bash-only. The router template fill failed with "Bad substitution"; now awk. Use `#!/bin/bash` or POSIX tools.
- **A folder is named `&.widgits`.** `sed` expands `&` in the replacement; unquoted `&` in `sh -c` backgrounds. Use awk `index()/substr()` or bash substitution; always quote paths.
- **Module launcher argument rule**: the renderer prefixes EVERY `<module src=...>` argument with the house path and appends `<house> <package_dir> <module id>`. Match words by last path component; ignore extras.
- **Relay key codes are DECIMAL** (`KEY_PRESSED: 51` is `3`; `3` alone is Ctrl-C). Nav numbers are global per window: a new first element (e.g. a text field) shifts every row's number: read it from the frame.
- **Backspace confirm focus**: pressing Enter on a row (select) republishes and can move focus; focus by digit only, then Backspace.
- **Ops copies of shared files**: `_.monads/_.livedesk-taskbar/ops/khtpm_render_core.c` is an untracked build-time copy of `&.widgits/_shared-lib/khtpm_render_core.c`; edit the shared original and re-copy before compiling a scratch binary, else the include picks the stale copy.
- **`khtpm_taskbar_manager.c` is CRLF.** Edit with `newline=''` and CRLF anchors; verify "bare LF = 0" afterwards (a Python rewrite once produced a 12,564-line diff).
- **`button.sh restart` does NOT close ordinary HQ windows (found 2026-10-06; the hotbar survived restarts and the new one never showed).** `restart` == `run` and only calls `ops/crypt_autostart.c`, whose pre-launch sweep kills the taskbar pid, the open-entity registry, and command lines containing `tp_taskbar` / `khtpm_strip_parser` / `khtpm_taskbar_manager_main` / `tp_desktop_window`. A hotbar or board window is a plain `khtpm_core_render.+x` process: no match. (`quit`/`reset` DO kill every `khtpm_core_render` by name; `restart` does not.) Fix: data list `$.crypts/close_on_restart.pdl` (`CLOSE | name | substring`; also requires the house root in the command line) closed by `$.crypts/close_listed.sh`, called from `button.sh` restart/run, quit and reset; add a row for any window or helper that must not outlive a restart. Proof: `bash '$.crypts/test_close_listed.sh'` (7 checks on a scratch house). `sh '$.crypts/close_listed.sh' <house> --dry-run` shows what it would close. Note `crypt_autostart` is only compiled when its binary is missing, so changing that C file alone changes nothing: that is why this fix lives in `button.sh`.
- **My first theory was WRONG and was disproved by a test before I shipped it**: I assumed the reaper skips ledger lines written with starttime `0`. It does not (`0` = unknown, not checked). `kh_proc_reap_all` only skips a line whose NON-ZERO starttime mismatches (PID-reuse guard). Lesson: reproduce the claimed mechanism with the real function before fixing. The ledger helper `&.widgits/_shared-lib/ops/proc_ledger_add.sh` (real pgid + starttime; used by `open_hotbar.sh`, `open_board.sh`, the router) is still an improvement and is tested (`tests/test_proc_ledger_add.sh`), but it is not the fix for this bug.
- **Test windows: never leave them running.** The renderer re-execs itself, so `kill $!` can miss it. Use `--dump-and-exit` (renders once to `/tmp/entity-menu-frame.png` + `.receipt.txt` and quits) or `--headless`; never filter `ps` output with text that appears in your own command line (`pkill -f`/`grep` self-match killed my shell again). I once left five test hotbars on the owner's screen.
- **Scratch builds first.** Compile renderer/manager to a scratch path, test, then run the hash-gated build (replaces the binary new windows launch from; running processes keep theirs).
- **`git checkout`/ff from a branch that tracks `xyzfs/users` onto one that does not deletes the files from disk** (reproduced; it deleted the owner's key files once; restored byte-identical). Merge in a scratch worktree.
- **`--git-common-dir` is cwd-relative**; resolve from the house dir. **`pkill -f` self-matches** and kills your own shell: pidfile kill or a runtime-built pattern.
- **A leftover `>>` to a wrong relative path** creates stray files (I created and removed a `.gitignore` in `yz.muchiverse/` once). Check `git status` for `??` before committing. Runtime ignores need `**/` anchors (the app sits under `x0.../44.xyz.01.00/`).
- **`sleep` under prisc+x is microseconds** (`2000000` = 2 s).
- **Warnings baseline**: `khtpm_taskbar_manager.c` has ~123 pre-existing format-truncation warnings; compare against `git show HEAD:...` compiled the same way, not against a failing build.

## 5. House rules that applied (so you do not re-derive them)

Commit only your own paths (`git diff --cached --name-only` must be exactly your files); never `git add -A`, stash, reset others' work, push `user/*`, or touch another tool's branch. Marker rule: change detection = append-only size growth, never mtime.
Never report done without a fresh build + fresh run + evidence (every item in §1 has a command). Do not move or close the owner's windows: record positions first, launch and close only your own. Wrap multi-minute work in `nice -n 15`.
API keys: owner said ignore (not rotated).

## 6. Suggested next steps (my recommendation, cheapest and most central first)

1. Link the board into the h-ai menu and add the placer entries for graves/ghosts (`GRAVEYARD-GHOSTS-DESIGN.md` §6b). 2. Ghost inbox: quest -> `task` to the ghost's phone. 3. Keyboard `assign`, then the quest-card drag (`DRAG-AND-DROP-BETWEEN-MENUS.md` §5).
4. `pending` + the rule-based judge. 5. `work` (lease). 6. IRL: demonstrations as event records with verdicts, then a simple preference learner proposing tunables. 7. Save-slot restore once the owner picks the model.
