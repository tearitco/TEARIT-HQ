# lpns+map+4 Architecture Guide: How to Refactor to Ledger-Based

## What This Document Is

Complete architectural reference for `01.lpns+map+4` — a ledger-driven
turn-based game. Documents every file, data format, code path, and constraint.
Purpose: enable converting any state-file-based project (mutaclsym, wsr-pal,
pal-chat-irc, muchi-pal-agent) to use the same ledger architecture.

---

## Part 1: The Ledger Concept

### What Is a Ledger?

An **append-only log** of every action in the game. Every move, NPC turn,
end_turn — one line each, in order, forever.

**Nothing is ever deleted or modified.** State is never stored directly.
Current state is **derived by replaying the ledger** from the beginning.

This is **Event Sourcing** — history IS the truth. Current state is computed.

### Why Use a Ledger?

| Benefit | Explanation |
|---------|-------------|
| **Crash recovery** | Replay ledger → full state restored |
| **Undo** | Remove last N lines → game rewinds N turns |
| **Audit trail** | Every action permanently recorded |
| **Debugging** | "What happened at turn 47?" → read line 48 |
| **Consistency** | Single source of truth, no conflicting state files |
| **Simplicity** | Each op only appends one line |

### Trade-off

O(N) replay where N = total turns. For small games (hundreds of turns),
fast (~5-15ms). For thousands, consider periodic snapshots.

---

## Part 2: Data Formats

### 2.1 config.txt (root directory)

Tracks whose turn it is and who the players are. ONLY mutable game state
besides the ledger.

```
current_epoch=1
current_turn=0
num_players=4
num_npcs=3
player_1_type=human
player_1_name=alice
player_1_start_x=0
player_1_start_y=0
player_2_type=computer
player_2_name=bot1
player_2_start_x=7
player_2_start_y=0
```

Fields:
- current_turn: Global 0-indexed counter. Increments every action.
- current_epoch: Increments when current_turn % num_players == 0.
- player_N_type: "human" or "computer"
- player_N_start_x/y: Starting position (fallback when ledger empty)

Who writes: orchestrator.c (initial), game_turn_input.c (every turn)
Who reads: game_dispatch, game_turn_input, game_compose_frame, npc_auto_play

Update method: Atomic rename. Write config.txt.tmp, then rename() to config.txt.

### 2.2 data/master_ledger.txt

Append-only action log. One line per action.

Format:
```
timestamp|epoch|player|turn|action_data|action_type
```

Fields:
- timestamp: ISO 8601 YYYY-MM-DDTHH:MM:SS
- epoch: Integer epoch number
- player: Player name (must match player_N_name in config)
- turn: Global 0-indexed turn number
- action_data: Move=x:N,y:N | Word=the word | End_turn=N/A
- action_type: move, word, or end_turn

Example:
```
timestamp|epoch|player|turn|action_data|action_type
2026-07-25T17:50:35|1|alice|0|x:1,y:0|move
2026-07-25T17:50:35|1|bot1|1|x:7,y:1|move
2026-07-25T17:50:36|1|bot2|2|x:0,y:6|move
2026-07-25T17:50:36|1|bot3|3|x:6,y:7|move
2026-07-25T17:50:37|2|alice|4|x:2,y:0|move
```

Who writes: game_turn_input.c (append only)
Who reads: game_turn_input (replay), game_compose_frame (replay)

### 2.3 pieces/apps/player_app/view.txt

Rendered game view. Written by game_compose_frame.c.
Read by CHTPM parser for ${game_map} variable.

### 2.4 pieces/apps/player_app/interact_relay.txt

Key relay file. Parser writes bare keycodes (one int per line).
Game dispatch reads and truncates.

---

## Part 3: Code Architecture - File by File

### 3.1 ops/game_turn_input.c - The Core Op

THE MOST IMPORTANT FILE. Every action flows through here.

What it does (one-shot):
1. Reads config.txt to find whose turn it is
2. Reads player start position from config.txt
3. REPLAYS THE ENTIRE LEDGER to find current position
4. Computes new position based on action
5. APPENDS one line to ledger with new position
6. ATOMICALLY UPDATES config.txt (turn + possibly epoch)

Key pattern - ledger replay:
```c
int player_x = start_x, player_y = start_y;
FILE *ledger = fopen("data/master_ledger.txt", "r");
while (fgets(line, sizeof(line), ledger)) {
    char pname[50]; int new_x, new_y;
    if (sscanf(line, "%*[^|]|%*d|%49[^|]|%*d|x:%d,y:%d",
               pname, &new_x, &new_y) >= 3) {
        if (strcmp(pname, player_name) == 0) {
            player_x = new_x; player_y = new_y;
        }
    }
}
```

Key pattern - append to ledger:
```c
fprintf(ledger, "%s|%d|%s|%d|x:%d,y:%d|move\n",
        timestamp, epoch, player_name, turn, new_x, new_y);
```

Key pattern - atomic config update:
```c
FILE *cfg = fopen("config.txt", "r");
FILE *tmp = fopen("config.txt.tmp", "w");
while (fgets(line, sizeof(line), cfg)) {
    if (strncmp(line, "current_turn=", 13) == 0)
        fprintf(tmp, "current_turn=%d\n", new_turn);
    else fprintf(tmp, "%s", line);
}
fclose(cfg); fclose(tmp);
rename("config.txt.tmp", "config.txt");
```

Movement: action 4=right(+x), 5=left(-x), 6=up(-y), 7=down(+y)
Clamped to [0, MAP_SIZE-1] where MAP_SIZE=8.

### 3.2 ops/game_compose_frame.c - The Renderer

What it does (one-shot):
1. Reads config.txt to load all actors (names, start positions)
2. REPLAYS THE ENTIRE LEDGER to find every actor's current position
3. Renders 8x8 grid with actors as digits ('1'+i)
4. Writes rendered view to view.txt
5. Appends "G" to frame_changed.txt to signal renderer

Key pattern - actor reconstruction:
```c
struct { char name[50]; int x, y; } actors[MAX_ACTORS];
// Load from config
for (int i = 0; i < total_actors; i++) {
    actors[i].x = start_x[i]; actors[i].y = start_y[i];
}
// Replay ledger
FILE *ledger = fopen("data/master_ledger.txt", "r");
while (fgets(line, sizeof(line), ledger)) {
    char pname[50]; int new_x, new_y;
    if (sscanf(line, "%*[^|]|%*d|%49[^|]|%*d|x:%d,y:%d",
               pname, &new_x, &new_y) >= 3) {
        for (int i = 0; i < total_actors; i++) {
            if (strcmp(actors[i].name, pname) == 0) {
                actors[i].x = new_x; actors[i].y = new_y;
            }
        }
    }
}
```

### 3.3 ops/game_dispatch.c - Per-Tick Dispatcher

What it does (one-shot, every ~16ms):
1. Reads ALL keycodes from interact_relay.txt into buffer
2. Immediately truncates interact_relay.txt
3. For EACH keycode: converts to action, fork/exec game_turn_input
4. NPC auto-play: while current player is computer, call game_turn_input
5. If any action: fork/exec game_compose_frame

Key pattern - read-truncate:
```c
FILE *hf = fopen(".../interact_relay.txt", "r");
char buf[MAX_LINE * 16];
while (fgets(line, sizeof(line), hf)) { strcat(buf, line); }
fclose(hf);
FILE *trunc = fopen(".../interact_relay.txt", "w");
if (trunc) fclose(trunc);
```

Key pattern - NPC auto-play:
```c
while (max_loops-- > 0) {
    read_game_state(&turn, &total, ptype, sizeof(ptype));
    if (strcmp(ptype, "computer") != 0) return;
    int action = 4 + (rand() % 4);  // random direction
    execute_action(action);  // fork/exec game_turn_input
}
```

### 3.4 system/orchestrator.c - Startup

Startup sequence:
1. compile_binaries() - gcc all .c files
2. ensure_directories() - mkdir all data dirs
3. write_config() - write initial config.txt
4. write_ledger() - write header to data/master_ledger.txt
5. Clear stale relay/history/state files
6. initial_compose() - run game_compose_frame
7. Set env vars (PRISC_PROJECT_ROOT, PRISC_PROJECT_ID)
8. Launch: renderer, keyboard_input, chtpm_parser_pal

Kill pattern (3-layer):
1. kill(0, SIGTERM) - process group
2. File-backed PID tracking via proc_list.txt
3. kill_all.sh - surgical pkill -9 -f

### 3.5 pal/main_loop_chtpm.pal - The PAL Loop

```pal
exec ./ops/game_compose_frame
hit_frame

loop:
  exec ./ops/game_dispatch
  sleep 16667
  j loop
```

Initial frame render, then dispatch -> sleep 16.667ms -> repeat forever.
Loop never exits. Quit handled by parser killing module process.

### 3.6 Relay Chain (how keys flow)

```
keyboard_input.c
  -> pieces/keyboard/history.txt ("KEY_PRESSED: N")
  -> chtpm_parser_pal.c reads history.txt
  -> process_key(N) -> INTERACT branch
  -> inject_raw_key(eff)
  -> pieces/apps/player_app/interact_relay.txt ("N\n")
  -> game_dispatch reads relay
  -> game_turn_input appends to ledger
```

### 3.7 Variable System

- load_vars() reads: state.txt, gui_state.txt, view.txt
- game_map variable = content of view.txt
- ${game_map} in .chtpm layout = full rendered view
- Parser re-reads vars when state_changed.txt grows

---

## Part 4: Refactoring Guide

### 4.1 What CRUD Projects Currently Have (e.g. mutaclsym)

```
hero/state.txt          - pos_x, pos_y, interact_mode, render_mode
monsters/zombie_01/     - hp, pos_x, pos_y
monsters/skeleton_01/   - hp, pos_x, pos_y
```

Every op reads state file, modifies, writes back. No history.

### 4.2 What They Need (Ledger Architecture)

New files:
- config.txt - turn counter, epoch, player list
- data/master_ledger.txt - append-only action log

Config format:
```
current_epoch=1
current_turn=0
num_actors=1
actor_1_type=human
actor_1_name=hero
actor_1_start_x=5
actor_1_start_y=8
```

Ledger format (adapted):
```
timestamp|epoch|actor|turn|action_data|action_type
2026-07-25T10:00:00|1|hero|0|x:5,y:7|move
2026-07-25T10:00:00|1|zombie_01|1|x:3,y:4|monster_move
2026-07-25T10:00:01|1|hero|2|x:6,y:7|move
```

Action types needed:
- move - hero movement
- xlector_move - cursor in interact mode
- monster_move - NPC movement
- attack - bump attack
- end_turn - end turn
- interact_mode_on/off - toggle interact
- pickup/drop/eat/craft/examine - item actions

### 4.3 Files to Modify

MUST change:
1. ops/move_player.c - append to ledger, not write state.txt
2. ops/choice.c - append for interact toggle, item actions
3. ops/end_turn.c - append end_turn, increment turn
4. ops/tick_monsters.c - append monster moves to ledger
5. ops/compose_frame.c - replay ledger, render map
6. ops/compose_rgb_frame.c - replay ledger for GL
7. ops/game_dispatch.c - orchestrate everything
8. system/orchestrator.c - write initial config.txt + ledger header

KEEP as-is:
- system/chtpm_parser_pal.c (UI engine)
- system/keyboard_input.c (keyboard)
- system/renderer.c (frame display)
- system/chtpm_rgb_render.c (GL mirror)
- pal/main_loop_chtpm.pal (already done)

### 4.4 Refactor Order

Phase 1: Data layer
1. orchestrator.c writes config.txt with hero definition
2. orchestrator.c writes ledger header to data/master_ledger.txt
3. ensure_directories() creates data/

Phase 2: Core ops
4. move_player.c: remove state.txt writes, replay ledger for position,
   append move to ledger, atomic config update
5. end_turn.c: append end_turn, increment turn in config
6. compose_frame.c: replay ledger for all positions, render, write view.txt

Phase 3: NPC system
7. tick_monsters.c: replay ledger for monster positions, compute moves,
   append each monster move to ledger
8. game_dispatch.c: NPC auto-play loop after human action

Phase 4: Interaction
9. choice.c: append interact_mode_on/off to ledger
10. game_dispatch.c: relay dispatch + NPC loop + compose

Phase 5: State reconstruction
11. Create reconstruct_state() utility:
    Replay ledger -> hero pos, monster positions, etc.
    All ops call this instead of reading state files
12. Adapt move_player.c to use reconstruct_state for:
    hero pos, monster pos (bump check), interact_mode

### 4.5 State Reconstruction - The Hard Part

mutaclsym has richer state than lpns:
- Hero position + facing direction
- Monster positions + HP + type
- Inventory, active panel, interact_mode, render_mode, camera_mode

Recommended: HYBRID approach

Ledger tracks (events that change over time):
- Movement (hero + monsters)
- Combat (damage, death)
- Items (pickup, drop, eat)
- Turn progression

Files keep (static/rarely changing):
- Map layout (map.txt, furniture.txt)
- Monster definitions
- Item/recipe definitions

Config/files keep (dynamic but non-positional):
- interact_mode
- render_mode
- active_panel

### 4.6 The reconstruct_state Function

Key new code. Every op that needs current state calls this.

```c
typedef struct {
    int hero_x, hero_y;
    int xlector_x, xlector_y;
    int interact_mode, facing;
    int monster_count;
    struct {
        char name[64], type[64];
        int x, y, hp;
    } monsters[64];
} GameState;

GameState reconstruct_state(const char *config, const char *ledger) {
    GameState gs = {0};
    read_config(config, &gs);          // start positions
    replay_ledger(ledger, &gs);        // current positions
    read_dynamic_state(&gs);           // interact_mode, etc.
    return gs;
}
```

### 4.7 Per-Op Refactor Checklist

For each op (move_player, choice, end_turn, tick_monsters):

BEFORE (CRUD):
```c
// Read state
FILE *f = fopen("hero/state.txt", "r");
// ... parse pos_x, pos_y, etc.
// Modify
pos_x += dx;
// Write state
FILE *f = fopen("hero/state.txt", "w");
fprintf(f, "pos_x=%d\n", pos_x);
```

AFTER (Ledger):
```c
// Reconstruct state from ledger
GameState gs = reconstruct_state("config.txt", "data/master_ledger.txt");
// Modify
int new_x = gs.hero_x + dx;
// Validate
if (new_x < 0 || new_x >= MAP_W) return;
// Append to ledger
append_ledger("data/master_ledger.txt", "hero", new_x, new_y, "move");
// Update turn in config
increment_turn("config.txt");
```

### 4.8 Key Constraints

1. MAP_SIZE is defined per project (8 for lpns, variable for mutaclsym)
2. MAX_ACTORS may need to increase for mutaclsym (many monsters)
3. Ledger replay is O(N) - acceptable for hundreds of turns
4. For thousands of turns, add periodic snapshots:
   - Every 500 turns, write current_state.txt as a checkpoint
   - reconstruct_state: if snapshot exists, start replay from there
5. The atomic config update (tmp+rename) is CRITICAL - never write config.txt directly
6. All ops must be one-shot (fork/exec, not persistent)
7. game_dispatch truncates relay AFTER reading all keys - prevents race condition
