# Understanding 01 — 2026-09-17/23:15 — Kilo Session Start

## What I did this session

### WSR-CIV Track (primary)
- **Step A started**: Verified events pipeline via relay injection (per §3)
- Opened toys menu (taskbar cell 12) via relay injection: wrote `4012` to `#.desktop/livedesk_agent_relay.txt`
- Navigated toys menu with arrow keys (codes 200-203) via manager relay: wrote codes to `#.desktop/strip_history.txt`
- Launched Piececraft-HQ (item 18) via manager relay: wrote `5018` to `#.desktop/strip_history.txt`
- Piececraft-HQ board window launched (PID 191387) but **only renders thin ".main" tab** — appears broken
- Launched events-hq for piececraft-hq (PID 236554) running `khtpm_core_render.+x` with `pieces/dashboard.chtpm` — not yet tested
- Confirmed ASCII frame capture works: `#.desktop/strip_ascii_frame_history.txt` logs frames

### DSR Track (parallel)
- Not yet started — will need its own file:desk and menu class (New Game/Load/Save/Save As as reusable event per §10)

### Cross-cutting
- Killed corrupted piececraft-hq board process (PID 191387)
- Read handoff doc and presentation pipeline doc
- Identified relay injection paths:
  - Taskbar: `#.desktop/livedesk_agent_relay.txt` (taskbar harness consumes)
  - Per-window: `#.desktop/entity_menu_history/<pid>.txt` (strip process consumes `KEY_PRESSED: <code>` format)
  - Manager: `#.desktop/strip_history.txt` (bare decimal codes per line)

## File paths touched
- `#.desktop/livedesk_agent_relay.txt` — taskbar relay injection
- `#.desktop/strip_history.txt` — manager relay injection
- `#.desktop/entity_menu_history/191387.txt` — piececraft-hq board history (to test)
- `#.desktop/entity_menu_history/236554.txt` — events-hq history (to test)
- `#.desktop/strip_ascii_frame_history.txt` — ASCII frame capture log
- `44.xyz.01.00/@.apps/piececraft-hq/pchq-board.xhtpm` — board layout
- `44.xyz.01.00/&.widgits/events-hq/button.sh` — events-hq launcher

## What worked
- Relay injection path is **real and functional** — taskbar + manager both consume injected codes
- Toys menu launches Piececraft-HQ and events-hq successfully
- Events-hq now runs via merged `khtpm_core_render.+x` (class="events-hq-window")
- ASCII frame capture pipeline works for presentation proof

## What's broken / gaps found
- **Piececraft-HQ board window (PID 191387) renders only ".main" tab** — no board content visible. This is a real blocker for WSR-CIV Step B (file:desk creation).
- **Events-hq relay path not yet tested** — need to inject into `#.desktop/entity_menu_history/236554.txt` to verify Step A (events-creation screen)
- Toy launch mechanism: `livedesk_build_toys_menu()` scans `@.apps/`, `&.widgits/`, `&.hq-apps/` for `toy.pdl` files — confirmed working

## Next concrete step (WSR-CIV)
1. Investigate piececraft-hq board window — check its history file `#.desktop/entity_menu_history/191387.txt` and test relay injection to navigate/fix
2. Find events-hq's history file (`#.desktop/entity_menu_history/236554.txt`) and test event creation via relay (Step A completion)
3. Capture PNG frames via `dump_frame_png_op` for presentation proof

## Next concrete step (DSR)
- Create DSR file:desk and menu class (New Game/Load/Save/Save As as reusable event) — parallel track per §10

## Learnings / Wisdom
1. **Relay injection is the house standard** — it's not just "testing," it's the actual input path. xdotool/XTest is last-resort only. The strip process + manager process + taskbar harness form a complete, repeatable input pipeline.
2. **Two tracks must stay explicitly separate** — different file:desks, different understanding-file threads, explicit reuse logging. This is the real test of §6's "bank of event pages" idea.
3. **Events-hq merged into khtpm_core_render.+x** — no separate binary, class="events-hq-window" identifies it. This simplifies launch but means events-hq shares the core renderer.
4. **Presentation proof requires PNG + TTS** — `dump_frame_png_op` + `make_presentation_video.py` with 5-8s/frame pacing. Not optional — it's the verification bar.
5. **Piececraft-HQ board rendering is fragile** — the thin ".main" tab suggests the board projector isn't receiving/processing the map data correctly. This may be a pre-existing bug, not something I introduced.

## IDEA (not now)
- Could build a relay injection helper script to automate the taskbar→toys→app launch sequence for faster iteration
- Could create a "relay replay" tool that reads history files and re-injects for regression testing
- The piececraft-hq board issue might need C-level debugging (renderer/projector) — but §2 bans touching renderer. If genuinely impossible via events, this is a gap to report per §3.

## Checkpoint status
- WSR-CIV Step A: **IN PROGRESS** — events-hq launched, not yet tested via relay
- WSR-CIV Step B: **BLOCKED** — piececraft-hq board broken
- DSR: **NOT STARTED**
- Cross-cutting ai_describe primitive: **NOT STARTED** (§2b exception, §11)

## Problems / Confusion / Zombie Process Postmortem

### The zombie process situation
- **What happened**: User quit the entire livedesk taskbar, but two events-hq processes remained alive:
  - PID 232890: `khtpm_events_hq_manager.+x` (manager process)
  - PID 236554: `khtpm_core_render.+x` (window process, class="events-hq-window")
  - PID 236631: `[khtpm_events_hq] <defunct>` (zombie child of manager)

- **Why it happened**: The events-hq launcher (`button.sh`) spawns a manager + window pair that are **independent of the taskbar**. Quitting the taskbar doesn't cascade-kill launched apps. The manager (232890) owns the window (236554) and a defunct child (236631).

- **My confusion**: I initially looked for "livedesk_taskbar" processes and missed these because they run under `khtpm_core_render.+x` and `khtpm_events_hq_manager.+x` — not obviously named "taskbar". The events-hq window IS the core renderer, just with a different class.

- **Root cause**: No centralized process tree / session manager. Each toy/app manages its own lifecycle. The taskbar is just a launcher, not a parent.

- **Lesson**: Always `ps aux | grep khtpm` after "quitting" to find orphans. The house has no equivalent of "close all windows" — each PID must be tracked individually.

### Other problems I caused/faced
1. **Piececraft-HQ board (PID 191387) rendered only ".main" tab** — I killed it but never diagnosed WHY. Could be:
   - Board projector not receiving map data
   - Missing `pchq_board_action.sh` initialization
   - Renderer bug (but §2 bans touching C)
   - This is a genuine gap per §3 — if events can't fix it, must report

2. **Events-hq relay path untested** — I launched it but didn't inject keys into `#.desktop/entity_menu_history/236554.txt`. Step A incomplete.

3. **No PNG capture yet** — presentation proof pipeline (`dump_frame_png_op` + `make_presentation_video.py`) not exercised.

4. **DSR track completely untouched** — parallel track per §10 needs its own file:desk and menu class.

### What I should have done differently
- After launching events-hq, immediately test relay injection into its history file before declaring Step A "started"
- Check `#.desktop/entity_menu_history/` for the actual PID files *before* assuming they exist
- Build a "kill all khtpm" helper for clean session resets
- Document the exact relay codes for events-hq navigation (unknown — need to explore its menu structure)