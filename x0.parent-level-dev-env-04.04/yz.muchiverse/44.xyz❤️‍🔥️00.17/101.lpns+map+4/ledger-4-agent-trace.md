# ledger-4-agent-trace.md
## Full Architectural Trace for Future Agents
### 01.lpns+map+4 (LPNS Map Variant)

---

## Purpose

This document traces every process, file read, file write, and signal in the
running system. It is written for agents who need to modify, debug, or extend
the project without reading source code. Cross-references to actual source
lines are given where critical.

**Companion docs:**
- `FUNCTIONALITY_TRACE.md` — high-level overview, how to extend
- `TPMOS_DRAGON_COMPAT.md` — portability, self-contained vs shared

---

## 1. Launch Sequence

```
button.sh run
  └─ exec system/orchestrator           (line 131 of orchestrator.c)
       │
       ├─ compile_binaries()            (line 121) — gcc -pthread for game_manager, gcc for rest
       ├─ mkdir -p data pieces/...      (line 142) — ensures all directories exist
       ├─ write_config()                (line 84) — writes config.txt (4 players, positions)
       ├─ write_ledger()                (line 113) — writes data/master_ledger.txt (header only)
       ├─ Clear files:
       │    pieces/os/proc_list.txt     (line 147) — empty for PID tracking
       │    pieces/display/frame_changed.txt (line 151)
       │    pieces/apps/player_app/history.txt (line 153)
       │    pieces/keyboard/history.txt (line 155)
       │    pieces/apps/player_app/interact_relay.txt (line 157)
       │    pieces/system/quit_flag.txt (line 160)
       ├─ game_compose_frame (one-shot) (line 163) — initial frame render
       ├─ setenv PRISC_PROJECT_ROOT, PRISC_PROJECT_ID (lines 165-166)
       └─ Launch 3 persistent services (lines 169-171):
            ├─ ./system/renderer         → PID logged to proc_list.txt
            ├─ ./system/keyboard_input   → PID logged to proc_list.txt
            └─ ./system/chtpm_parser_pal pieces/chtpm/layouts/lpns_main_menu.chtpm
                 → PID logged to proc_list.txt
```

**After launch, orchestrator sleeps in `while(!should_exit) { sleep(1); }` loop
(line 175), reaping zombie children.**

---

## 2. Process Tree (Steady State)

```
orchestrator (PID tracked)
  ├─ renderer            — polls frame_changed.txt + renderer_pulse.txt
  ├─ keyboard_input      — reads /dev/tty, writes keyboard/history.txt
  └─ chtpm_parser_pal    — reads keyboard/history.txt, renders frames
       └─ <module> fork  — system/game_manager (persistent child)
            └─ game_manager polling thread — polls interact_relay.txt
                 └─ fork/exec ops (one-shot per action):
                      ├─ ops/game_turn_input N
                      └─ ops/game_compose_frame
```

**Total persistent processes: 4** (orchestrator, renderer, keyboard_input, chtpm_parser_pal+game_manager as parent/child pair).

---

## 3. Render Pipeline (x0.moke Canonical Pattern)

This is the **single most important section** for understanding why frames appear.

### 3.1 Who writes what

| Writer | File | When |
|--------|------|------|
| `game_compose_frame.c:main()` | `frame_changed.txt` (append "G") | After writing `view.txt` (line 126-130) |
| `chtpm_parser_pal.c:compose_frame()` | `renderer_pulse.txt` | After composing `current_frame.txt` (parser internal) |
| `chtpm_parser_pal.c:compose_frame()` | `current_frame.txt` | Full overwrite with chrome + game_map |
| `game_manager.c:pulse_frame_marker()` | `frame_changed.txt` (append "G") | **REMOVED** — was redundant, caused double-render. See line 39 (dead code). |

### 3.2 Who reads what

| Reader | File | Trigger condition |
|--------|------|-------------------|
| `renderer.c:main()` | `frame_changed.txt` | `file_size() != last_marker` → immediate render |
| `renderer.c:main()` | `renderer_pulse.txt` | `file_size() != last_renderer_pulse` → wait for `current_frame.txt` to stabilize (20×2ms polls), then render |
| `renderer.c:main()` | `current_frame.txt` | Read on render, printed to stdout with CRLF conversion |
| `chtpm_parser_pal.c:main()` | `keyboard/history.txt` | Reads `KEY_PRESSED: N` lines from byte 0 (reset each launch) |
| `game_manager.c:polling_thread()` | `interact_relay.txt` | Reads from `last_relay_pos`, truncates after read |

### 3.3 Render flow for a keypress

```
1. User presses key → /dev/tty
2. keyboard_input.c reads /dev/tty → append_key():
     a. Writes "[timestamp] KEY_PRESSED: N\n" to pieces/keyboard/history.txt
     b. Writes "N\n" to pieces/apps/player_app/history.txt (for prisc+x)
3. chtpm_parser_pal.c main loop detects new KEY_PRESSED line
4. process_key(keycode):
     a. If NAV mode (active_index==-1):
        - Arrow keys → move focus_index
        - Enter on INTERACT button → active_index = focus_index
        - Enter on KEY:101 button → send_command("KEY:101")
     b. If INTERACT mode (active_index != -1):
        - Arrow keys → inject_raw_key(1000-1003) → appends to interact_relay.txt
        - ESC → active_index = -1 (exit INTERACT)
5. compose_frame() called:
     a. Reads state.txt, view.txt, loads variables
     b. Interpolates ${game_map} from view.txt content
     c. Writes pieces/display/current_frame.txt
     d. Writes pieces/display/renderer_pulse.txt
6. renderer.c detects renderer_pulse.txt growth:
     a. Waits for current_frame.txt to stabilize (up to 40ms)
     b. Reads current_frame.txt
     c. Prints to stdout with CRLF
```

### 3.4 Render flow for NPC auto-play

```
1. game_manager.c:poll_relay() reads interact_relay.txt
2. Finds keycode (e.g. 1001 for arrow_right)
3. execute_action(action):
     a. fork/exec ops/game_turn_input with action code
     b. fork/exec ops/game_compose_frame (writes view.txt + frame_changed.txt)
4. frame_changed.txt grows → renderer.c detects → renders immediately
5. npc_auto_play() runs:
     a. Picks random direction (4-7)
     b. execute_action() → game_turn_input + game_compose_frame
     c. frame_changed.txt grows → renderer renders
     d. usleep(300000) — 300ms delay
     e. Repeats until human's turn
```

---

## 4. Key Codec Table

| Keycode | Action | Source |
|---------|--------|--------|
| 1000 | move_left (action 5) | Arrow left in INTERACT mode |
| 1001 | move_right (action 4) | Arrow right in INTERACT mode |
| 1002 | move_up (action 6) | Arrow up in INTERACT mode |
| 1003 | move_down (action 7) | Arrow down in INTERACT mode |
| 101 | end_turn (action 3) | End Turn button via KEY:101 |
| 119 | word (action 1) | 'w' key (not wired in layout) |
| 27 | exit INTERACT | ESC key in INTERACT mode |
| 13 | activate button / enter | Enter on focused element |

**Mapping in game_manager.c:keycode_to_action() (line 61):**
```
1000 → 5 (left)    1001 → 4 (right)
1002 → 6 (up)      1003 → 7 (down)
119 → 1 (word)     101 → 3 (end_turn)
```

**Mapping in game_turn_input.c (action codes):**
```
1 = word    3 = end_turn
4 = right   5 = left   6 = up   7 = down
```

---

## 5. File I/O Summary

### Files written every frame (by game_compose_frame.c)
- `data/master_ledger.txt` — append only (action record)
- `config.txt` — rewrite via temp file (turn/epoch increment)
- `pieces/apps/player_app/view.txt` — full overwrite (game map content)
- `pieces/display/frame_changed.txt` — append "G"

### Files written by chtpm_parser_pal.c
- `pieces/display/current_frame.txt` — full overwrite (chrome + game_map)
- `pieces/display/renderer_pulse.txt` — append (triggers renderer)
- `pieces/display/active_gui_index.txt` — on focus change

### Files read by each process

**renderer.c:**
- `pieces/system/quit_flag.txt` — check `buf[0] == '1'`
- `pieces/display/frame_changed.txt` — file_size()
- `pieces/display/renderer_pulse.txt` — file_size()
- `pieces/display/current_frame.txt` — fread on render

**keyboard_input.c:**
- `/dev/tty` — read(0, &c, 1) with raw mode
- Writes to `pieces/keyboard/history.txt` and `pieces/apps/player_app/history.txt`

**chtpm_parser_pal.c:**
- `pieces/keyboard/history.txt` — KEY_PRESSED: N format
- `pieces/apps/player_app/state.txt` — load_vars()
- `pieces/apps/player_app/manager/gui_state.txt` — load_vars()
- `pieces/display/active_gui_index.txt` — on focus change
- Layout file: `pieces/chtpm/layouts/lpns_main_menu.chtpm`

**game_manager.c:**
- `pieces/apps/player_app/interact_relay.txt` — poll + truncate
- `config.txt` — read game state
- Fork/exec `ops/game_turn_input` and `ops/game_compose_frame`

**game_turn_input.c (one-shot op):**
- `config.txt` — read current_turn, current_epoch, player positions
- `data/master_ledger.txt` — read (replay for position) + append (new action)
- Writes to `config.txt` via temp file

**game_compose_frame.c (one-shot op):**
- `config.txt` — read player names, positions, game state
- `data/master_ledger.txt` — replay for positions
- Writes to `pieces/apps/player_app/view.txt`
- Writes to `pieces/display/frame_changed.txt`

---

## 6. State Reconstruction (Ledger Replay)

All positions are reconstructed by replaying `data/master_ledger.txt`:
```
Header: timestamp|epoch|player|turn|action_data|action_type
Data:   2026-07-24T19:00:00|1|alice|0|x:1,y:0|move
```

**Replay logic (game_turn_input.c line 74-92, game_compose_frame.c line 60-82):**
1. Read starting positions from config.txt (`player_N_start_x/y`)
2. Scan ledger for matching player name
3. For each `move` action: update x,y to new values
4. For `end_turn`/`word` actions: no position change

**This means the ledger is the single source of truth for all positions.**
Config.txt starting positions are initial values only.

---

## 7. Layout Anatomy (lpns_main_menu.chtpm)

```xml
<panel time_reactive="true">
    <module>system/game_manager</module>          ← forks game_manager as persistent child
    <interact src="pieces/apps/player_app/interact_relay.txt" />  ← INTERACT mode target

    <text label="${game_map}" />                   ← game_compose_frame writes view.txt, parser reads as ${game_map}
    <button label="Move (arrows)" onClick="INTERACT" />  ← enters INTERACT mode
    <button label="End Turn" onClick="KEY:101" />         ← sends keycode 101 to interact_relay.txt
    <text label="[KEY]: ${last_key}" />            ← shows last key pressed
</panel>
```

**How ${game_map} works:**
1. `game_compose_frame.c` writes game map content to `pieces/apps/player_app/view.txt`
2. `chtpm_parser_pal.c:load_vars()` reads `state.txt` and `view.txt`
3. Parser substitutes `${game_map}` in layout with view.txt content
4. Parser writes final composed frame to `current_frame.txt`

---

## 8. NPC Auto-Play Logic

**Location:** `game_manager.c:npc_auto_play()` (line 113)

**Algorithm:**
```
while current player is "computer":
    pick random direction (4=right, 5=left, 6=up, 7=down)
    execute_action(direction)  → game_turn_input + game_compose_frame
    sleep(300ms)               → human can see NPC moves
    read next player type
repeat until human's turn
```

**Key detail:** NPC auto-play runs AFTER any human action completes. It
executes all consecutive computer players' turns before returning control
to the human. This means after alice moves, bot1/bot2/bot3 all move
before alice can act again.

**Player type lookup:** `game_manager.c:read_game_state()` (line 74)
reads `config.txt`, computes `current_player = (current_turn % num_players) + 1`,
then finds `player_N_type`.

**Bug fix applied:** `sscanf` in config lookups uses temp `pidx` variable
(line 44 of game_turn_input.c) to avoid overwriting `current_player`.

---

## 9. Signal Handling

| Process | SIGTERM | SIGINT | quit_flag.txt |
|---------|---------|--------|---------------|
| orchestrator | `handle_signal()` → `kill_all_tracked()` → `_exit(0)` | same | no |
| renderer | default (exit) | default | `while(!quit_requested())` checks `buf[0] == '1'` |
| keyboard_input | default (exit) | default | writes "1" to quit_flag.txt on 'q' |
| chtpm_parser_pal | default (exit) | SIGINT handler cleanup | no |
| game_manager | `running = 0` → joins poll thread | same | no |

**Two-phase kill in orchestrator (line 29-56):**
1. Phase 1: SIGTERM to all tracked PIDs (200ms wait)
2. Phase 2: SIGKILL survivors + waitpid(WNOHANG)

---

## 10. CPU Safety

| Process | Sleep interval | Location |
|---------|---------------|----------|
| orchestrator | `sleep(1)` main loop | line 176 |
| renderer | `usleep(16667)` main loop (~60fps) | line 113 |
| keyboard_input | `usleep(10000)` in read_key spin-wait + main loop | copied from xer/ |
| game_manager | `usleep(16667)` poll interval (POLL_INTERVAL) | line 16, 162 |
| NPC auto-play | `usleep(300000)` between NPC moves | line 124 |
| game_turn_input | none (one-shot, exits immediately) | — |
| game_compose_frame | none (one-shot, exits immediately) | — |

**All processes register SIGTERM handlers. orchestrator uses two-phase kill
(SIGTERM → 200ms → SIGKILL) for reliable cleanup.**

---

## 11. Known Bugs & Gotchas

1. **game_compose_frame writes `view.txt`, not `current_frame.txt` directly.**
   Parser reads view.txt via load_vars() and substitutes into ${game_map}.
   This is the correct pattern (not writing current_frame.txt directly).

2. **`pulse_frame_marker()` in game_manager.c (line 39) is dead code.**
   It was removed from execute_action() but the function definition remains.
   `game_compose_frame` already writes to frame_changed.txt.

3. **interact_relay.txt is truncated after every read** (line 147-149 of
   game_manager.c). This means keycodes are consumed exactly once.

4. **keyboard_input.c must clear history.txt on launch** (button.sh should
   do `: > pieces/keyboard/history.txt`). Without this, stale keys from
   previous sessions replay on startup.

5. **quit_flag.txt must be cleared on launch** (orchestrator.c line 160).
   If left with "1" from a previous session, renderer exits immediately.

6. **No collision detection** — players can occupy the same tile.

7. **NPC moves are random** — no pathfinding or strategy.

8. **Epoch transitions have no special logic** — just increments when
   all players have taken a turn.

---

## 12. How to Test Headless

```bash
cd 01.lpns+map+4

# Start the game
sh button.sh run &

# Wait for startup
sleep 2

# Inject Enter to activate Move button
echo "[2026-07-24 19:00:00] KEY_PRESSED: 13" >> pieces/keyboard/history.txt
sleep 0.5

# Inject right arrow in INTERACT mode
echo "[2026-07-24 19:00:01] KEY_PRESSED: 1001" >> pieces/keyboard/history.txt
sleep 0.5

# Inject ESC to exit INTERACT
echo "[2026-07-24 19:00:02] KEY_PRESSED: 27" >> pieces/keyboard/history.txt
sleep 0.5

# Check results
cat pieces/display/current_frame.txt | tail -20
cat pieces/system/manager.log | tail -10
cat config.txt | grep current_turn
cat data/master_ledger.txt
```

---

## 13. Extension Points

**Add a new action:**
1. Add keycode mapping in `game_manager.c:keycode_to_action()`
2. Handle action in `game_turn_input.c` (the switch on `action`)
3. Add button in `lpns_main_menu.chtpm` with appropriate `onClick`

**Add a new player:**
1. Update `config.txt` fields: `num_players`, `player_N_type`, `player_N_name`, `player_N_start_x/y`
2. `game_compose_frame.c` already handles up to `MAX_ACTORS` (8)

**Add map features:**
1. Modify `game_compose_frame.c` to render additional elements in view.txt
2. The parser will pick up changes via ${game_map} substitution

**Add NPC AI:**
1. Modify `game_manager.c:npc_auto_play()` with smarter logic
2. Current: random direction. Could: follow player, avoid others, etc.

---

## 14. Differences from x0.moke Reference

| Aspect | x0.moke | 01.lpns+map+4 |
|--------|---------|----------------|
| Game manager | `system/prisc+x` + `pal/main_loop_chtpm.pal` | `system/game_manager.c` (C binary) |
| Parser | `chtpm_parser.c` (upstream) | `chtpm_parser_pal.c` (PAL fork) |
| Render trigger | `renderer_pulse.txt` (compose_frame) + `frame_changed.txt` (game) | Same pattern |
| State bridge | `state.txt` + `manager/gui_state.txt` | `view.txt` (game_map variable) |
| Button dispatch | `${piece_methods}` (dynamic) | Hardcoded buttons |
| Menu nav | `${piece_methods}`-generated | Hardcoded `INTERACT` + `KEY:101` |
| Session isolation | Full (copies + symlinks) | None (runs in project dir) |

**This project uses a simpler, self-contained architecture suitable for
a standalone game. x0.moke's architecture is designed for multi-project
sharing via TPMOS.**
