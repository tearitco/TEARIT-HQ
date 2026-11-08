# Agent toolbox and board controls (one page, 2026-11-05)

Read this first when you drive, test or change a petslike board (rpg-edit-hq, rpg-space, rpg-pet, ...) or commit work. Every row names the proof (a harness case that can fail), so you can trust it or re-run it. Paths are from the house `44.xyz.01.00/`; ops live in `&.widgits/_shared-lib/ops/+x/`, harness in `&.widgits/_shared-lib/harness/` (verbs and traps: its `README.md`). No Python, no new `.sh`: tools are C ops, harnesses are pal + case `.pdl`.

## 1. Before you choose how to build something
Ask the design-choice test (AGENTS.md; log: `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`): one authority per fact, modular, scalable, long term, house shapes, cost and risk. Add an entry to the log.

## 2. Tools (what to run, when)
| Need | Run | Proof case | Notes |
|---|---|---|---|
| Commit only your files | `ops/+x/commit_scoped.+x <repo> --branch claude --message-file <f> --trailer '<line>' [--push] -- <paths>` | `commit_scoped` 22 | Refuses wrong branch, `.`, `-A`, `xyzfs/users`. Foreign staged files stay staged. |
| Compile or relaunch with the 3D engine running | `ops/+x/cpu_gate.+x <house> run <label> --stop-engines -- <cmd>` | `cpu_gate_stop_engines` 8 | One heavy job at a time, niced; stops only this house's engine by pid. Write compiler output to a file, never pipe into `head` (it kills the compiler). |
| After rebuilding an engine op | `board-viewer/ops/+x/bv_session_sync.+x <house> [--dry-run]` | `bv_session_sync` 12 | A session runs its own binary copy; without this the old code keeps running. |
| Is a runtime file growing without a cap | `ops/+x/growth_probe.+x <house> --standard 03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md [--min-mb N] [--live-min M]` | `growth_probe` 12 | Exit 1 = a live large file the standard does not name. Findings table is in the standard. |
| Copy a book doc to the owner's XO folder | `ops/+x/xo_export.+x <source> <subfolder> <pre.name> [--force]` | `xo_export` 22 | Docs/media only, `<pre>.<name>`, never overwrites owner edits without `--force`. |
| Drive, freeze, photograph a board | `board-viewer/tools/+x/board_ctl.+x <cmd> <app dir>`: `preflight`, `key`, `engine-key`, `click`, `freeze`/`thaw`, `camera`, `snap`, `expect`, `recover` | `board_ctl_freeze` 16 | Skill `.claude/skills/rpg-space-board-driving/SKILL.md` (sections 00 and 14). |
| Save and restore real files in a live case | harness verbs `SNAPSHOT` / `RESTORE` (hash-verified); `INCLUDE` for shared fixtures (`fixtures/editor_house.pdl`) | `harness_snapshot` 7, `harness_include` 3 | Five older live cases still hand-roll their restore: convert each when next run live. |
| Drive the real editor window like a person | `RELAY_DRIVE_NO_ESC=1 ops/+x/relay_drive.+x <pid> run <script> <winid>` with `@.apps/deskopoly/tests/*.pdl`; map in `@.apps/deskopoly/nav.pdl` | `live_dsk_seat` 23 (`LIVE_OK=1`) | Gentle mode reads the window's own `#.desktop/ascii_frames/<pid>.<seq>.objects.pdl` (no Esc, no `p`) and polls up to 5 s for the element. Menu rows: `tapid`; popup buttons: `press label:...`. Onboarding: `16.game/TEROPOLY-ONBOARDING-...-2026-11-08.md`. |
| Find what a hot house process really costs | sample `/proc/<pid>/stat` fields 14+15 (own) and 16+17 (reaped children) twice; `voluntary_ctxt_switches` in `/proc/<pid>/status`; `wchan` (`do_wait` = spawning) | `lc_clock_burn` 7, `dsk_driver` 68 (cost guards) | `top` hides short-lived children; `strace` prints a false 0 here (ptrace_scope=1); read the guard's newest `incidents/*-ACT.txt` table first. CPU-OPTIMIZATION-PITFALLS 23-27. |

## 3. The three key paths (they reach different consumers)
| Path | Reaches | Does not reach |
|---|---|---|
| `board_ctl engine-key` / `bv_keys.+x` (engine relay) | the ENGINE: xelector, camera, `9` | the game daemon (it reads `<session>/pieces/keyboard/history.txt`, written only by the window) |
| per-pid window relay `#.desktop/entity_menu_history/<pid>.txt` | the window; Enter reaches the game only while the window believes it has X focus, else it activates the focused menu item | a reliable Enter |
| physical key (`board_ctl key`, pointer over the window, restored afterwards) | everything, like the owner's key | nothing: use it to prove Enter/Esc behaviour |

## 4. Board controls (what the owner's keys do, 2026-11-05)
| Control | Behaviour | Proof / doc |
|---|---|---|
| Enter on a pet or Harold | possesses it: `possessed_id` (xelector state) is the ONE authority (decision D1) | `possess_enter` 26, `possess_enter_daemon` 4 |
| Tab / click on a pet row | `select <id>`: if something is possessed, possession follows; otherwise a plain selection | `select_follows_possession` 6 |
| `9` | releases possession where the xelector stands | `possess_enter` |
| Esc | first Esc releases possession and stays In; the second leaves In. The daemon publishes `<app>/board/state/esc_first.txt` (`57`), the window consumes it once | `esc_first_publish` 7, `live_esc_first` 7 |
| Menu row "Possession: lock to hero / free" or verb `lockhero [on\|off]` | hero_01 `de_possessible 1`, pets `possessable 0`; the daemon refuses pets while locked | `possess_enter` |
| Pets wander even with INT on | `board_ctl freeze <app>` before measuring positions; `thaw` after | `board_ctl_freeze` |

Detail and history of the look/feel: `18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md`; how to test deterministically: `21.drive-doc-fix/TESTING-BOARD-BEHAVIOUR-DETERMINISTICALLY.md`.

## 5. Runtime files now capped (so you do not add them to a growth list again)
`.loop_guard.log` (8 KB), events-hq `messages.txt`/`history.txt` (256 KB then newest 64 KB, `mr_log_cap` 9), petslike `relations.txt` (compaction at 128 KB, `relations_cap` 10), prisc no longer logs a missing optional `default_op.txt` (`prisc_optional_default_ops` 5). Still uncapped, owner decision: the editor event `master_ledger.txt` files. Table: `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`.

## 6. Traps that cost time today
`EXPECT_HAS` reads only 16 KB; a literal `|` in a `RUN` or `EXPECT` splits the row (write `\p`); an unknown harness verb is silently ignored (check the README); `pkill -f` in a tool shell kills that shell; a daemon runs old code until restarted, a session until `bv_session_sync`; never compile while the engine runs; restore every live change by hash.
