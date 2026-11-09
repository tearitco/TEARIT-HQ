# Teropoly (internal name `deskopoly`): onboarding for the next agent (2026-11-08)

Owner, 2026-11-08: *"i want to transfer all that knowledge and intelligence to the next agents in on boarding, tho this is new work it will make up the bulk of the work going forward in future."*
This is that handoff. **Read this before touching the game, the editor's Play button, the clock daemon, or any harness that drives a real window.** Labels: **OBS** = observed this session with a number or picture; **DOC** = my reasoning, check before relying on it.

**Name.** The user sees **Teropoly** (popup title, table talk, players bar, `GAME` rows). Internal identifiers stay `deskopoly` (directories, ops, harness cases, ledger paths) until a deep rename (same rule as LyfeDesk/livedesk in AGENTS.md). Do not rename them piecemeal.

## 1. What exists (one paragraph per piece)

- **Engine** `@.apps/deskopoly/ops/deskopoly.c`: one process per verb, no resident process. The **ledger** (`<game>/ledger.txt`, append-only) is the only authority; `ui`, `mechanics`, `page`, `check` are projections derived from it. Classic Monopoly rules (40 tiles, auctions are milestone M7, debt queue M8, trading and chat M9, personalities M10, fuzz M12, spec: `08-roadmap/design-docs/DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08.md`).
- **Driver** `ops/deskopoly_driver.c`: the *resident* side that the editor's **Player > Play** starts (book `game.pdl` row `GAME | driver`). It ticks the engine on a timer, walks the RPG Maker characters (writes `events.txt`), keeps the board picture current, publishes the feed `<editor>/state/game_ui.txt`, and takes commands from `<editor>/state/games/deskopoly/action.txt` (append only, written by `deskopoly_ctl <command> <games_dir>`).
- **The feed**: `game_ui.txt` is plain data that the editor's window loads LAST, so it overrides editor keys (hide palettes, show the players bar, game chat, popup buttons). A new bar id draws blank: reuse `side-l`/`side-r`. Details: memory note `reference-editor-game-feed-overrides`.
- **The picture**: the generated desk `deskopoly-pchq/pieces/system/maps/deskopoly/play/` (`cells.*`, `map.png`, `map.txt`, `events.txt`). **Players are `events.txt` rows, not map cells**: `x y r g b <charset> <idx> <dir> <pat> <glyph>`. The 3D loader reads exactly 9 or 15 fields and silently DROPS any other count; the glyph must be a LETTER (a digit parses as a float, makes the row 10 fields, and every player vanished in 3D until 2026-11-08). OBS.
- **Human seat**: Setup screen lets a person take seat p1; the driver then stops polling the engine while the game waits for them (0 engine calls while a human thinks, OBS 17 per 10 s before, 0 after).

## 2. The harnesses (pal + case `.pdl` + C ops; NEVER Python or new `.sh`; AGENTS.md)

Run any case: `cd &.widgits/_shared-lib/harness; rm -f results/X.txt; nice -n 10 ./ops/+x/harness_case_op.+x cases/X.pdl > /tmp/X.log 2>&1; ./ops/+x/harness_verdict_op.+x cases/X.pdl | tail -1`. **Never pipe the case op into `head`** (SIGPIPE kills it mid-run and leaves processes). Read `harness/README.md` BEFORE writing a case (verbs `RUN` (fields split on ` | `, a literal bar is `\p`), `EXPECT_*`, `WAIT_HAS`, `CAPTURE`/`CALC`, `SPAWN`, `ENV`, `SNAPSHOT`/`RESTORE`, `SCRATCH`).

| Case | Checks | What it proves | Needs |
|---|---|---|---|
| `deskopoly` | 105 | rules, replay reproduces state, a hand-edited ledger is refused, ghost files change nothing, deleted projections regenerate identical | scratch |
| `dsk_classic` / `dsk_play` / `dsk_jail` / `dsk_cards` / `dsk_build` | 79 / 48 / 49 / 42 / 59 | the classic ruleset, play loop, jail, cards, building | scratch |
| `dsk_page` | 42 | the map the editor draws during Play (houses, hotels, mortgages, players as events) | scratch |
| `dsk_driver` | 68 | the REAL driver in a scratch house: setup screen, start, pause freezes the ledger, step, walking not teleporting, restart continues from the ledger, owner exit clears the feed, usage errors; **cost guards**: quiet rows rebuild nothing (cells.marker unchanged), a purchase does (vacuity) | scratch, ~35 s |
| `live_dsk_seat` | 23 | THE LIVE ONE: in the real rpg-edit-hq window, take seat p1 and play three rounds with real taps (Roll, Buy, End turn), 2D and 3D pictures, replay check, restores the editor as found | `LIVE_OK=1`, editor open on the deskopoly book, ~55 s, saves 10 PNGs in `@.apps/deskopoly/shots/` (git-ignored, wiped per run) |
| `lc_clock_burn` | 7 | the clock daemon's real-time fire budget (section 5), with its own negative run | scratch, ~25 s |
| `lc_clock_recur` / `lc_clock_cmds` / `lc_clock_restore` / `needs_clock` | 57 / 103 / 27 / 15 | the clock's recurrence, commands, restore | scratch |

Rules that cost runs: (1) negative-test every new guard once (run the case against the OLD binary or a wrong expectation and watch it go red; I did this for `lc_clock_burn`, the COST guard in `dsk_driver` and `live_dsk_seat`); (2) a **vacuity guard** next to every "nothing happened" check; (3) `EXPECT_DEAD` after a `kill` needs a `SLEEP` first (the daemon takes ~1 s to exit); (4) an `ENV` row applies to everything after it (a mid-case `ENV` is how `lc_clock_burn` changes the gap between its two cases); (5) a case that starts a daemon must stop it by exact pid, never by pattern.

## 3. Driving the real editor like a person (reusable scripts for agents and presentations)

**What to look for, per project, is in `06-testing/WHAT-TO-LOOK-FOR-ACCEPTANCE-COMPENDIUM-PER-PROJECT.md`: read it with this section. A test that passes while the screen is empty is the failure mode of 2026-11-08.**

Files: `@.apps/deskopoly/nav.pdl` (the map: protocol, stable ids, scripts, camera, facts) and `@.apps/deskopoly/tests/*.pdl` (STEP scripts run by `relay_drive <pid> run <script> <winid>`):
`dsk-new-game` (popup New game -> setup), `dsk-seat-setup` (Play, speed fast, you play p1, START GAME; 17 steps), `dsk-seat-turns` (three rounds as p1; 24 steps; seed 7 is deterministic: a Luck tile, Buy Power 150, Buy Orange3 200).

The protocol (each line cost a failed run; all OBS in the real window):
1. **Leave In: first.** With `In: ON` digits go to the game world. ONE Esc leaves In (more are harmless). Press `tb-in` once at the end to hand the editor back as found.
2. **Use gentle mode**: `RELAY_DRIVE_NO_ESC=1`. relay_drive's normal mode sends Esc (flips In and closes an open dropdown) and a `p` key (typed into an open dropdown's search), so menus could never be seen. Gentle mode finds the element in the window's OWN text dump `#.desktop/ascii_frames/<pid>.<seq>.objects.pdl` (highest seq of our pid): `OBJECT tag= id= class= x= y= w= h= nav= active= label=`. It now polls up to 5 s for the element, because the dump is rewritten a moment AFTER the frame changes (the first run after a rebuild failed at `tapid pm-play` for exactly that).
3. **Menu rows react to a click** (`tapid pm-play`), **popup buttons to nav digits + Enter** (`press label:Roll`). Nav numbers shift (22/27/37), so match by id or label, never by number.
4. **Tap once and verify in the ledger.** A blind double tap pressed Roll and then End turn. Sync on the LEDGER, not the popup text (the pets overwrite it).
5. **Start state**: delete the stale feed (`game_ui.txt`) first, because a killed session leaves the old one behind and waiting for it passes on stale data; then `deskopoly_ctl new_game` and `setup_reset` (3 players, no seat, seed 7, normal speed; setup preferences otherwise persist across New game).
6. **Never drive the window while the owner does.** Record anything you change (`last_book.txt`, play-mode flag, sides file) and restore by hash.

Cameras (the unresolved one): an automated overhead 3D camera is NOT solved. `board_ctl camset` pan values are relative to the session's `cam_home.txt`, which differs per session and appears re-captured on desk reload (one session `8 8 0 0`, the next `16 16 60 0`); a replayed key sequence worked once and showed only sky the next time. The live case takes the default 3D frame and a key-`0` 2D frame instead. 20 presses of `c` raise the camera into the sky; restore with 20 `v`.

## 4. CPU: what burned, how it was found, how to look (read `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` items 23-27)

The CPU guard ended 30 house processes at 12:12 (incident `#.hq-apps/proc-mon/state/incidents/20261108-121200-ACT.txt`). Chrome was at 457% too, but the incident's own table named **our** `lc_clock.+x` at **364%** inclusive of children: three causes, all fixed in commit `0ebb47e91`:

| Cause | Measured | Fix |
|---|---|---|
| clock daemon: the rpg clock runs at rate `sec` (1 game hour per real second); a 5-game-minute reminder falls due 12 times a second; each fire is a full event run (~1.8 s CPU, even for pages shipped OFF); `CATCHUP_CAP` bounds a poll, not work per second | 364% of a core, 130-160 wakeups/s; scratch daemon 104 ledger rows in 9 s | daemon-only real-time budget: at most one fire per reminder per `LC_CLOCK_MIN_FIRE_GAP_MS` (default 30 s), one event per batch, the rest collapses into `caught up N`; scratch 8 rows in 9 s; live with the clock RUNNING (In: off) mean 8% of a core, bursts to 30% every ~30 s (CORRECTED 17:4x: the first "below 0.5%" was measured while In: ON had the clock paused); paused 0% |
| driver rebuilt the board picture every tick (3 engine launches + 2.5 MB PNG + cells.marker growth makes the 3D engine reload the desk) | 6.1% of a core while a game ran | `page_needed()`: byte cursor over NEW ledger rows, rebuild only for ownership/building/mortgage rows; unknown verbs rebuild (fail-safe) |
| driver's fixed `usleep(80000)` | 12-14 wakeups/s even stopped | 250 ms unless the game is live or a walk/resync/page is pending (80 ms): stopped 12 -> 4/s |

**How to measure (no ptrace here, `strace` fails with ptrace_scope=1 and prints a false 0):**
- CPU **including reaped children**: sample `/proc/<pid>/stat` fields 14+15 (own) and 16+17 (children) twice, divide by CLK_TCK and the interval. `top` hides the fork storm because each child is short-lived.
- **Wakeups per second**: `voluntary_ctxt_switches` (+ nonvoluntary) in `/proc/<pid>/status` over 10 s; `/proc/<pid>/wchan` shows `do_wait` (a process spawning and waiting) versus `hrtimer_nanosleep` (a plain sleep).
- Process creations machine-wide: field 5 of `/proc/loadavg` (last pid) differences. OBS: 48 per second with only the editor open (the baseline of the prisc loops and the parser, not changed).
- A producer is optimized only after you know who reads it (CPU-OPTIMIZATION-PITFALLS 5): the `ui.txt` inode count and `cells.marker` size are the cheap, writer-side counters for the driver.
- **Always measure with a browser running** (AGENTS.md). Say idle and active numbers in the commit.

Open CPU facts: the baseline house loops are still not free (OBS with only the editor and a running game: `prisc+x` 3% each x2, `khtpm_core_render` 3-7%, `chtpm_parser` 1% x2, `bv_render_3d` 44 wakeups/s); `cpu_lint` only flags `usleep` <= 5 ms, so an 80 ms poll or a 300 ms poll that forks passes the lint: **it cannot see the problem above**, only the context-switch counter and the incident table can.

## 5. The machine can reset the session under you (not ours, but know the signature)

2026-11-08 11:23: an amdgpu GPU reset (kernel blamed `chrome`) killed GNOME, every khtpm window and the guard; a command of mine returned exit 137. Recipe in memory note `project-gpu-reset-kills-session-2026-11-08`: `uptime`/`who -b`, `journalctl -k -b | grep -E 'GPU reset|ring .* timeout|for process'`, then kill your orphans (`lc_clock`, `bv_render_3d` outlive the session) by pid, restore backed-up live files by hash, restart the guard.

## 6. Open items and owner decisions (do not decide for the owner)

- **Blue dotted lines on the wooden floor at a low camera** (OBS: pale sky-coloured lines across the floor at grazing angle; they vanish with a higher camera). Looks like sky showing through voxel seams in the raymarch; engine-side, shared by every board; unconfirmed that this is what the owner saw (`blue bug artifact`). Needs an owner confirmation and a look at `&.widgits/board-viewer/ops/bv_gpu_raymarch.c`.
- 3D event scale: players are a fixed 0.7 x 1.6 box (a tile is much wider). My recommendation: per-desk `event_scale` in `view.pdl`, default 1.0 (rebuilds the shared engine). Alternatives: 2x2 tile block, or leave. Owner decides.
- Neutral tile/card names, day/night following ledger ROUNDS (replay-safe, my recommendation) versus the real clock, official-sheet verification, whether a missing ledger should default to classic instead of mini.
- **A tab that says `Player: playing` may have no driver behind it** (OBS 2026-11-08 after the session reset: flag `mode=on`, no `deskopoly_driver`, no feed, so no side menus or pieces; Player > Play started it and showed the Setup screen, pieces appear after START GAME because the world stays on the authored `board` desk until then). RESOLVED 2026-11-08: the core now restarts a missing driver (watchdog in `petslike_core.c`, case `game_driver_watchdog`, 10 checks, red on the old binary); the live relaunch was not done by me (a broad process kill was declined), so confirm once after the next editor relaunch. CPU-OPTIMIZATION-PITFALLS 27.
- UX polish: cash (left bar) vs net worth (footer) labels; the subtitle "pets play, you watch or take a seat" is stale while a human plays.
- Later milestones: M7 auctions, M8 debt queue, M9 trading and chat, M10 personalities, M12 fuzz; the owner wants a narrated captioned YouTube video (XO folder `31.Nov/nov-8/10.youtube-deskopoly-plays-in-the-editor`, predates the glyph fix and human seat).

## 7. External references (for other agents)

Repo: `@.apps/deskopoly/{game.pdl,nav.pdl,state.pdl,harness.pdl,tests/,ops/}`, `&.widgits/_shared-lib/harness/{README.md,cases/live_dsk_seat.pdl,cases/dsk_driver.pdl,cases/lc_clock_burn.pdl}`, `&.widgits/_shared-lib/ops/relay_drive.c`, `&.widgits/livedesk-clock/ops/lc_clock.c`. Book: `16.game/DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md`, `02-architecture/AGENT-TOOLBOX-AND-CONTROLS.md`, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` (D53), `03-pitfalls/RUNTIME-FILE-GROWTH-STANDARD.md`, `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md`, `06-testing/HARNESS-AUTHORING-GUIDE.md`. Commits on branch `claude`: `c359dd42a`, `2f2e8b020`, `06e4e8bf3`, `54e328fe8`, `3a74263c6`, `0ebb47e91`.
