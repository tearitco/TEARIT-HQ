# pal-readme.md
## PAL Manager Architecture & How to Create Managers
### For external users and future projects

---

## What is PAL?

PAL (Prisc Assembly Language) is a simple bytecode language executed by
`prisc+x`, a virtual machine inspired by RISC-V. Instead of writing
game managers in C (which requires compilation, is hard to modify, and
locks you into one project's logic), you write `.pal` scripts that
prisc+x interprets at runtime.

**Key insight:** A `.pal` file replaces what used to be `system/game_manager.c`
— the persistent process that reads player input, dispatches actions,
and composes frames. With PAL, you can create new game managers by
writing a text file, no C compilation needed.

---

## How prisc+x Works

`prisc+x` is a register-based VM with these primitives:

```
li xN, value        — load immediate into register xN
add xD, xS1, xS2   — xD = xS1 + xS2
beq xS1, xS2, label — branch if equal
j label              — unconditional jump
read_history path xD, xS — read from history file
sleep N              — sleep N microseconds
```

**The basic PAL script shape:**
```
li x1, 0            # initialize counter

loop:
  read_history pieces/apps/player_app/interact_relay.txt x2, x1
  beq x2, x0, no_key
  # ... handle key ...
  j loop

no_key:
  sleep 16667        # ~60fps poll interval
  j loop
```

**IMPORTANT:** The instruction to call ops is `exec`, NOT `op`.
`op` doesn't exist in prisc+x and will silently do nothing.
See `!.xyzos-pitfalls+1.txt` pitfall #13.

This is a persistent loop that polls for input, dispatches actions,
and sleeps — exactly what a C game manager does, but as a text script.

---

## The Two File Triggers (x0.moke Pattern)

The render pipeline relies on **two separate marker files** to trigger
frames. Understanding this is critical:

| File | Who writes it | What it means |
|------|--------------|---------------|
| `pieces/display/frame_changed.txt` | Game ops (game_compose_frame) | "Game state changed, render immediately" |
| `pieces/display/renderer_pulse.txt` | Parser's compose_frame() | "I finished composing the chrome, render after it stabilizes" |

**renderer.c watches both.** On `frame_changed.txt` growth, it renders
immediately. On `renderer_pulse.txt` growth, it waits for
`current_frame.txt` to stabilize (up to 40ms) then renders.

**Why two files?** `frame_changed.txt` is for game state changes (move,
end turn) that need immediate rendering. `renderer_pulse.txt` is for
the parser's own compose_frame output (chrome, variable substitution)
which may need a moment to settle.

---

## Current State: C vs PAL

**01.lpns+map+4 currently uses C for the game manager:**
- `system/game_manager.c` — persistent process with polling thread
- `ops/game_turn_input.c` — one-shot op per action
- `ops/game_compose_frame.c` — one-shot op for rendering

**The target architecture (x0.moke pattern) uses PAL:**
- `system/prisc+x` + `pal/main_loop_chtpm.pal` — persistent PAL script
- Ops remain as C binaries (fork/exec from within the PAL script)

**The difference:**
```
C approach:     game_manager.c (persistent) → fork/exec ops
PAL approach:   prisc+x + main_loop_chtpm.pal (persistent) → prisc_shell() ops
```

Both achieve the same thing — a persistent process reading
`interact_relay.txt` and dispatching one-shot actions. PAL just does it
with a text script instead of compiled C.

---

## How to Create a PAL Manager

### Step 1: Write the PAL Script

Create `pal/main_loop_chtpm.pal` in your project:

```
# My Game Manager - PAL version
# Reads interact_relay.txt, dispatches actions, composes frames

li x1, 0           # history cursor position
li x8, 0           # last frame marker size

# Initial frame render
compose_frame
hit_frame

loop:
  # Check for new keys
  read_history pieces/apps/player_app/interact_relay.txt x2, x1
  beq x2, x0, no_key
  
  # Dispatch based on key value
  li x3, 1001       # ARROW_RIGHT
  beq x2, x3, move_right
  li x3, 1000       # ARROW_LEFT
  beq x2, x3, move_left
  li x3, 1002       # ARROW_UP
  beq x2, x3, move_up
  li x3, 1003       # ARROW_DOWN
  beq x2, x3, move_down
  li x3, 101        # END_TURN
  beq x2, x3, end_turn
  j loop

move_right:
  li x10, 4          # action code for right
  op game_turn_input x10
  op game_compose_frame
  j render

move_left:
  li x10, 5
  op game_turn_input x10
  op game_compose_frame
  j render

move_up:
  li x10, 6
  op game_turn_input x10
  op game_compose_frame
  j render

move_down:
  li x10, 7
  op game_turn_input x10
  op game_compose_frame
  j render

end_turn:
  li x10, 3
  op game_turn_input x10
  op game_compose_frame
  j render

render:
  compose_frame
  hit_frame
  j loop

no_key:
  sleep 16667        # ~60fps
  j loop
```

### Step 2: Update the Layout

In your `.chtpm` layout, change the `<module>` tag to use prisc+x:

```xml
<panel>
    <module>system/prisc+x pal/main_loop_chtpm.pal</module>
    <!-- ... rest of layout ... -->
</panel>
```

### Step 3: Update button.sh

Ensure your launch script:
1. Clears `pieces/keyboard/history.txt` on launch
2. Clears `pieces/system/quit_flag.txt` on launch
3. Exports `PRISC_PROJECT_ROOT` and `PRISC_PROJECT_ID`

---

## PAL Script Patterns

### Pattern 1: Polling Loop (most common)
```
loop:
  read_history <relay_file> x2, x1
  beq x2, x0, no_key
  # ... dispatch key ...
  j loop

no_key:
  sleep 16667
  j loop
```

### Pattern 2: Frame Change Detection
```
# Read current frame marker size
read_pos x8, "pieces/display/frame_changed.txt"

loop:
  read_pos x7, "pieces/display/frame_changed.txt"
  bne x7, x8, frame_changed
  # ... no change, keep polling ...
  j loop

frame_changed:
  addi x8, x7, 0    # update baseline
  compose_frame     # re-compose with new state
  hit_frame         # signal renderer
  j loop
```

### Pattern 3: Dual Trigger (x0.moke canonical)
```
# Watch BOTH frame_changed.txt AND renderer_pulse.txt
compose_frame
hit_frame

loop:
  read_pos x7, "pieces/display/chat_screen_changed.txt"
  bne x7, x8, response_arrived
  # ... poll for input ...
  j loop

response_arrived:
  compose_frame
  hit_frame
  j loop
```

### Pattern 4: NPC Auto-Play (inside PAL script)
```
npc_loop:
  read_game_state x20, x21   # turn, total_players
  beq x20, x0, done          # if human's turn, stop
  
  li x10, 4                   # random move direction
  op game_turn_input x10
  op game_compose_frame
  sleep 300000                # 300ms delay
  j npc_loop

done:
  # return to main loop
```

---

## Ops: One-Shot Actions

Ops are C binaries that do one thing and exit. They're invoked from
within PAL scripts (or from C game managers) via fork/exec.

**Standard ops for games:**
- `game_turn_input` — process a move/end_turn action
- `game_compose_frame` — render current state to view.txt
- `game_init` — initialize game state
- `game_dispatch` — route input to correct handler

**How ops are invoked from PAL:**
```
op game_turn_input x10    # prisc+x forks/execs game_turn_input with arg in x10
op game_compose_frame     # no args needed
```

**How ops are invoked from C (current pattern):**
```c
run_op("./ops/game_turn_input", "4", NULL);  // fork/exec/waitpid
run_op("./ops/game_compose_frame", NULL, NULL);
```

Both achieve the same thing. PAL just makes it declarative.

---

## The Module + INTERACT Pattern

This is how a persistent manager gets keyboard input:

```xml
<panel>
    <module>system/prisc+x pal/main_loop_chtpm.pal</module>
    <interact src="pieces/apps/player_app/interact_relay.txt" />
    <button label="Move" onClick="INTERACT" />
</panel>
```

**How it works:**
1. `<module>` forks `prisc+x` as a persistent child of the parser
2. `<interact src>` tells the parser where to write raw keycodes
3. `onClick="INTERACT"` puts the parser in INTERACT mode
4. In INTERACT mode, arrow keys bypass chtpm nav and go straight to
   the relay file
5. The PAL script reads the relay file and dispatches

**Key rules:**
- The module MUST NOT have its own quit path (q/ESC to exit)
- `interact_relay.txt` is truncated after each read
- The module polls this file independently (~60fps)
- `frame_changed.txt` is written by ops to trigger rendering

---

## Differences from C Game Managers

| Aspect | C game_manager | PAL main_loop_chtpm.pal |
|--------|---------------|-------------------------|
| Language | C source | PAL bytecode |
| Compilation | gcc required | No compilation needed |
| Modification | Edit .c, recompile | Edit .pal, restart |
| Persistence | pthread + while loop | `loop: ... sleep N; j loop` |
| Op invocation | `fork()/exec()/waitpid()` | `op <name> <arg>` |
| State reading | `fopen()/fscanf()` | `read_history`, `read_pos` |
| State writing | `fprintf()/fclose()` | `prisc_write()` |
| Signal handling | `signal(SIGTERM, ...)` | Handled by prisc+x |

**PAL advantages:**
- No compilation required
- Faster iteration (edit text file, restart)
- Portable (any platform with prisc+x)
- Declarative (easier to understand intent)

**C advantages:**
- More control (complex logic, error handling)
- Better performance (no VM overhead)
- Can use system libraries (pthread, etc.)
- Existing codebase is already C

---

## How External Users Create Managers

### For a simple game (like 01.lpns+map+4):

1. Copy the project template
2. Write `pal/main_loop_chtpm.pal` with your game logic
3. Create ops for your actions (or use existing ones)
4. Design your `.chtpm` layout
5. Update `button.sh` to launch `prisc+x` instead of `game_manager`

### For a complex game (with AI, multiple entities):

1. Start with the PAL template
2. Write ops in C for complex logic (AI, pathfinding)
3. Keep the PAL script as the dispatcher/orchestrator
4. Use `op <name>` to invoke C ops from PAL

### For a non-game application:

1. The same pattern works for any persistent manager
2. Replace game-specific ops with your own
3. The module + INTERACT pattern handles keyboard input
4. The render pipeline (frame_changed.txt → renderer) handles display

---

## File Structure for a PAL Project

```
my-project/
├── button.sh                    # Launch script
├── system/
│   ├── prisc+x                  # PAL interpreter binary
│   ├── chtpm_parser_pal         # Parser (shared)
│   ├── keyboard_input           # Keyboard handler (shared)
│   └── renderer                 # Renderer (shared)
├── pal/
│   └── main_loop_chtpm.pal     # YOUR game manager (text file)
├── ops/
│   ├── game_turn_input          # Your ops (C binaries)
│   └── game_compose_frame
├── pieces/
│   ├── chtpm/layouts/
│   │   └── main_menu.chtpm     # YOUR layout
│   ├── apps/player_app/
│   │   ├── state.txt            # Game state
│   │   └── interact_relay.txt   # Key relay file
│   ├── display/
│   │   ├── current_frame.txt    # Final rendered frame
│   │   ├── frame_changed.txt    # Render trigger 1
│   │   └── renderer_pulse.txt   # Render trigger 2
│   └── keyboard/
│       └── history.txt          # Key events
└── config.txt                   # Game configuration
```

---

## Common Pitfalls

1. **Forgetting to clear history.txt on launch** — stale keys replay
2. **Not writing to frame_changed.txt after state changes** — renderer never updates
3. **Writing to current_frame.txt from ops** — parser chrome gets clobbered
4. **Using the wrong render trigger** — frame_changed.txt for game state,
   renderer_pulse.txt for parser compose_frame output
5. **Module having its own quit path** — module dies, parser keeps relaying
6. **Not truncating interact_relay.txt after read** — keys dispatch multiple times

---

## References

- `#.haiku+/!.xyzos-standards.txt` — full PAL standards (3360 lines)
- `45.muchi-pal-agent🤖️/pal/main_loop_chtpm.pal` — working PAL script example
- `45.muchi-pal-agent🤖️/button.sh` — full launch script with session isolation
- `x0.moke-pet-project-04.04/` — canonical reference for render pipeline
- `!.handoff-july17.txt` — GL/RGB rendering and two-stage pulse pattern
- `PAL-VS-C-ARCHITECTURE.txt` — when to use PAL vs C

---

## Quick Start

```bash
# 1. Copy template
cp -r 01.lpns+map+4 my-game

# 2. Edit PAL script
vim my-game/pal/main_loop_chtpm.pal

# 3. Edit layout
vim my-game/pieces/chtpm/layouts/main_menu.chtpm

# 4. Compile ops (if new)
gcc -o my-game/ops/my_op my-game/ops/my_op.c

# 5. Run
cd my-game && ./button.sh run
```

The PAL interpreter handles the persistent loop, input polling, and
op dispatch. You just write the logic.
