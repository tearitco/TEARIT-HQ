# 01.lpns+map+4 — Functionality Trace
## For agents building on this infrastructure

---

## Architecture Overview

```
button.sh run
  └─ system/orchestrator.c          (fork/exec launcher, PID tracking)
       ├─ system/renderer.c         (polls frame_changed.txt → renders to stdout)
       ├─ system/keyboard_input.c   (reads /dev/tty → writes keyboard/history.txt + player_app/history.txt)
       └─ system/chtpm_parser_pal.c (reads layouts, handles nav/INTERACT/CLI_IO)
            └─ <module> tag forks system/game_manager.c  (reads interact_relay.txt → calls ops/)
```

## Data Flow

### 1. Key Press → Parser
```
/dev/tty → keyboard_input.c → pieces/keyboard/history.txt
                                  Format: [YYYY-MM-DD HH:MM:SS] KEY_PRESSED: N
                                  (dual-write: also bare int to pieces/apps/player_app/history.txt)
```

### 2. Parser → Nav/INTERACT
```
chtpm_parser_pal.c main loop:
  while(1) reads keyboard/history.txt for KEY_PRESSED: lines
  → process_key(keycode)
  → If active_index==-1 (NAV mode):
      Arrow/w/s = move focus
      Enter on INTERACT button → active_index = focus_index (activates)
      Enter on KEY:101 button → send_command("KEY:101") → inject_raw_key(101)
  → If active element is INTERACT:
      Arrow keys → inject_raw_key(1000-1003) → writes to interact_relay.txt
      ESC → exits INTERACT mode
  → compose_frame() writes to current_frame.txt
  → Pulses frame_changed.txt
```

### 3. Game Manager → Ops
```
game_manager.c (launched by <module> tag in layout):
  polling_thread reads interact_relay.txt
  → keycode_to_action(keycode) maps 1000-1003 to move actions, 101 to end_turn
  → run_op("./ops/game_turn_input", action) → fork/exec
  → run_op("./ops/game_compose_frame") → fork/exec
  → pulses frame_changed.txt
  → npc_auto_play() runs if next player is "computer" type
```

### 4. Ops → State
```
ops/game_turn_input.c:
  Reads config.txt (current_turn, current_epoch, num_players)
  Replays master_ledger.txt to get player position
  Writes move to master_ledger.txt
  Increments current_turn in config.txt
  Epochs advance when all players have taken a turn

ops/game_compose_frame.c:
  Reads config.txt + replays master_ledger.txt
  Writes 8x8 ASCII map to pieces/apps/player_app/state.txt
  Parser reads state.txt → composes layout with ${game_map} variable
```

## Key Files

| File | Purpose |
|------|---------|
| `config.txt` | Game state: turn, epoch, player types/names/positions |
| `data/master_ledger.txt` | Action history (replayed to reconstruct positions) |
| `pieces/display/current_frame.txt` | Final rendered frame |
| `pieces/display/frame_changed.txt` | Pulse marker (renderer + parser poll this) |
| `pieces/keyboard/history.txt` | Key events in KEY_PRESSED: format |
| `pieces/apps/player_app/history.txt` | Key events as bare integers |
| `pieces/apps/player_app/interact_relay.txt` | Arrow keycodes for game_manager (truncated after read) |
| `pieces/apps/player_app/state.txt` | Game map variable (written by game_compose_frame) |
| `pieces/system/quit_flag.txt` | "1" to shutdown (empty = running) |
| `pieces/os/proc_list.txt` | PID tracking for orchestrator cleanup |
| `pieces/chtpm/layouts/lpns_main_menu.chtpm` | Layout with module/interact/button tags |

## Buttons in Layout

| Button | onClick | What happens |
|--------|---------|--------------|
| Move (arrows) | `INTERACT` | Parser enters INTERACT mode; arrow keys → relay → game_manager |
| End Turn | `KEY:101` | Parser injects keycode 101 → relay → game_manager → action 3 (end_turn) → turn+1 |

## NPC Auto-Play

After any human action, `game_manager.c:npc_auto_play()`:
1. Reads `config.txt` for current player's `player_N_type`
2. If "computer", picks random move (4=right, 5=left, 6=up, 7=down)
3. Executes via `game_turn_input` + `game_compose_frame`
4. 300ms delay between NPC moves for visibility
5. Repeats until it's a human player's turn

## CPU Safety Rules

- **keyboard_input.c**: `usleep(10000)` in read_key() spin-wait AND main loop
- **renderer.c**: `usleep(16667)` main loop (~60fps)
- **game_manager.c**: `usleep(50000)` poll interval (50ms)
- **orchestrator.c**: `sleep(1)` main loop + file-backed PID tracking
- **All processes**: `setpgid(pid, pid)` so they can be killed by process group

## How to Test

```bash
cd 01.lpns+map+4
sh button.sh run          # Start all services

# Headless test (inject keys into keyboard/history.txt):
echo "[2026-07-24 19:00:00] KEY_PRESSED: 13" >> pieces/keyboard/history.txt
# → Should activate Move button (interact mode)

echo "[2026-07-24 19:00:01] KEY_PRESSED: 1001" >> pieces/keyboard/history.txt
# → Should move alice RIGHT (1001 = ARROW_RIGHT)

echo "[2026-07-24 19:00:02] KEY_PRESSED: 27" >> pieces/keyboard/history.txt
# → Should exit INTERACT mode (ESC)

echo "[2026-07-24 19:00:03] KEY_PRESSED: 13" >> pieces/keyboard/history.txt
# → Navigate to End Turn button, press Enter → turn advances

# Verify:
cat pieces/display/current_frame.txt | tail -10
cat pieces/system/manager.log | tail -10
cat config.txt | grep current_turn
```

## How to Extend

1. **Add new action**: Add keycode mapping in `game_manager.c:keycode_to_action()`, handle action in `game_turn_input.c`
2. **Add new button**: Add `<button label="..." onClick="KEY:N" />` in layout, map keycode N in game_manager
3. **Add new map feature**: Modify `game_compose_frame.c` to render additional elements
4. **Add new NPC behavior**: Modify `game_manager.c:npc_auto_play()` with smarter AI

## Known Limitations

- Map is fixed 8x8 grid
- NPCs only move randomly (no pathfinding)
- No collision detection between players
- Epoch transitions have no special logic
- CLI_IO input not wired to game actions yet
