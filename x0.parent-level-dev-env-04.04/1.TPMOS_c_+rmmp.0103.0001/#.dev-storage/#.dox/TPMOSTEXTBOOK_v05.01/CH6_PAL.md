# 🔧 Chapter 6: PAL: The Assembly Language of TPMOS
How do you orchestrate complex operations across an entire project? You write a **PAL script**. PAL (Prisc Assembly Language) is the glue that holds TPMOS together. 🔗🧠

---

## 📜 What is PAL?
PAL is an assembly-style scripting language designed specifically for TPMOS. It lets you chain together Ops (Muscles) into complex workflows without writing C code.

### The Instruction Set
PAL has a RISC-inspired instruction set:

| Instruction | Purpose | Example |
|-------------|---------|---------|
| `OP` | Call an external op | `OP playrm::move_entity "player" "up"` |
| `call` | Call a subroutine (label) | `call game_loop` |
| `sleep` | Pause execution (ms) | `sleep 100` |
| `beq` | Branch if equal | `beq r0, r1, done` |
| `jalr` | Jump and link register | `jalr handler` |
| `j` | Unconditional jump | `j start` |
| `lw` | Load word from memory | `lw r1, 0(r2)` |
| `sw` | Store word to memory | `sw r1, 0(r2)` |
| `addi` | Add immediate | `addi r1, r0, 1` |
| `halt` | Stop execution | `halt` |
| `read_history` | Read from history buffer | `read_history r1` |
| `read_state` | Read piece state | `read_state r1, "player", "pos_x"` |
| `hit_frame` | Trigger render | `hit_frame` |
| `read_pos` | Read entity position | `read_pos r1, r2, "player"` |

### Register Model
PAL uses 16 general-purpose registers (`r0`-`r15`) and a 4096-word memory space. Labels provide control flow targets.

---

## 💻 Code Example: The PAL Interpreter
The PAL interpreter (`pieces/system/prisc/prisc+x.c`, 771 lines) parses and executes PAL scripts. Here's the core execution loop:

```c
// prisc+x.c - Core instruction dispatch
typedef struct {
    char name[32];
    char handler[256];  // Path to .+x binary
    char desc[128];
} CustomOp;

CustomOp custom_ops[MAX_OPS];
int op_count = 0;

void execute_instruction(int pc) {
    Inst *inst = &program[pc];

    switch (inst->op) {
        case OP_CUSTOM: {
            // Find and execute the custom op
            for (int i = 0; i < op_count; i++) {
                if (strcmp(custom_ops[i].name, inst->custom_name) == 0) {
                    // Execute via fork()/exec()
                    char cmd[4096];
                    snprintf(cmd, sizeof(cmd), "%s %s %s",
                             custom_ops[i].handler,
                             inst->literal_arg,
                             inst->literal_arg2);
                    run_command(cmd);
                    break;
                }
            }
            break;
        }
        case OP_SLEEP:
            usleep(inst->imm * 1000);  // Convert ms to us
            break;
        case OP_BEQ:
            if (regs[inst->rs1] == regs[inst->rs2])
                pc = find_label(inst->label_ref);
            break;
        case OP_HIT_FRAME:
            hit_frame_marker();  // Trigger render pipeline
            break;
        // ... more instructions
    }
}
```

**Key insight:** PAL scripts compile to an in-memory instruction array, then execute sequentially. Custom ops are dispatched via `fork()/exec()` - the same CPU-safe pattern as C modules.

---

## 📝 PAL Program Example
Here's a simple PAL script that creates a user, moves them, and checks for treasure:

```asm
; welcome_sequence.asm - New player welcome flow
start:
    ; Create user profile
    call user::create_profile "player1"
    sleep 200

    ; Move player to starting position
    OP playrm::move_entity "player1" "right"
    sleep 100
    OP playrm::move_entity "player1" "right"
    sleep 100

    ; Read player position
    read_pos r1, r2, "player1"
    addi r3, r0, 5        ; r3 = 5 (target X)
    beq r1, r3, found_treasure

    ; No treasure, continue normally
    OP playrm::render_map
    halt

found_treasure:
    ; Player reached treasure!
    OP playrm::fuzzpet_action "player1" "celebrate"
    hit_frame
    halt
```

---

## 💻 Code Example: Custom Op Registration
Ops become PAL-callable through the `CustomOp` registry:

```c
// Registering an op for PAL use
void register_custom_op(const char* name, const char* handler_path, const char* desc) {
    if (op_count >= MAX_OPS) return;

    strncpy(custom_ops[op_count].name, name, 31);
    strncpy(custom_ops[op_count].handler, handler_path, 255);
    strncpy(custom_ops[op_count].desc, desc, 127);
    op_count++;
}

// Usage (called during PAL initialization):
register_custom_op(
    "playrm::move_entity",
    "pieces/apps/playrm/ops/+x/move_entity.+x",
    "Move an entity in a direction"
);
```

Once registered, the op is callable from any PAL script via `OP playrm::move_entity "player" "up"`.

---

## 🔧 PAL + Fondu Integration
When you install a project via Fondu, PAL scripts are installed alongside ops:

```bash
./fondu --install user
```

This copies:
1. `ops/+x/*.+x` → `pieces/apps/installed/user/ops/+x/`
2. `scripts/*.asm` → `pieces/apps/installed/user/scripts/`
3. Updates `ops_catalog.txt` with available PAL-callable ops

Now any project can call:
```asm
OP user::create_profile "new_user"
OP user::auth_user "new_user"
```

### Cross-Project PAL Calls
PAL supports calling ops from any installed project:
```asm
; This script calls ops from multiple projects
OP user::create_profile "player1"
OP playrm::move_entity "player1" "right"
OP p2p::connect_peer "node_01"
```

## 🖋️ The PAL Editor (GUI Scripting)
TPMOS now features a built-in **PAL Editor** within the `op-ed` suite. This allows developers to write, debug, and bind scripts to Pieces without leaving the OS.

### GUI Features:
*   **Instruction Palette:** Select common instructions (addi, beq, OP) from a visual list.
*   **Live Binding:** Save a script and automatically bind it to a Piece method (e.g., `on_interact`).
*   **State Visibility:** View register values (`r0`-`r7`) in real-time (planned).

### 🗺️ Future Roadmap: High-Leverage Blocks
To make PAL scripting accessible to all, we are planning to implement high-level "Blocks" inspired by **Scratch** and **RPGMaker MV**. These will be visual abstractions over raw PAL instructions.

#### 🧩 Scratch-Inspired Control Blocks (Top 10):
1.  **When Flag Clicked:** The global start button for autonomous script execution.
2.  **Forever:** Continuous loop for persistent sensing and background logic.
3.  **If [ ] Then:** The fundamental logic gate for branching behaviors.
4.  **Move [10] Steps:** Direct motion abstraction (calls `move_entity`).
5.  **Go to x:[ ] y:[ ]:** Absolute placement for resetting or teleporting Pieces.
6.  **Touching [ ]?:** Collision detection for walls, sprites, or mouse.
7.  **Change [Var] by [n]:** High-level variable manipulation (scores, health).
8.  **Wait [1] Sec:** Timing control to ensure CPU-safe execution speeds.
9.  **Switch Costume:** Dynamic icon/mirror state updates for animation.
10. **Broadcast [Msg]:** IPC system allowing Pieces to communicate events.

#### 🎭 RPGMaker-Inspired Event Commands (Game Progression):
*   **Message:** `Show Text`, `Show Choices`, `Input Number`, `Select Item`, `Show Scrolling Text`.
*   **Party:** `Change Gold`, `Change Items`, `Change Weapons`, `Change Armors`, `Change Party Member`.
*   **Game Progression:** `Control Switches`, `Control Variables`, `Control Self Switch`, `Control Timer`.

### Code Example: PAL Editor Module
The editor (`projects/op-ed/manager/pal_editor_module.c`) manages the script lines in memory before saving:

```c
// Add instruction to current script
if (key == 'a' || key == 'A') {
    int op_idx = cursor_line % op_count;
    if (instruction_count < MAX_INSTRUCTIONS) {
        strncpy(instructions[instruction_count].mnemonic, available_ops[op_idx].name, 31);
        instructions[instruction_count].args[0] = '\0';
        instruction_count++;
        cursor_line = instruction_count - 1;
    }
}

// Save and Bind
if (key == 's' || key == 'S') {
    save_script_to_file();
    // Bind to piece.pdl via piece_manager
    bind_script_to_method(active_piece, active_event, script_path);
}
```

---

## 📋 PAL Best Practices

1. **Keep scripts under 100 lines.** Long scripts are hard to debug. Split into subroutines with `call`.
2. **Use `call` for subroutines, `OP` for external ops.** `call` jumps to a label within the script; `OP` forks a new process.
3. **Always `sleep` between heavy operations.** Give the filesystem time to flush:
   ```asm
   OP playrm::render_map
   sleep 50  ; Let render complete before next action
   ```
4. **Error handling with `beq` checks:**
   ```asm
   read_state r1, "player", "health"
   beq r1, r0, player_dead  ; If health == 0, jump
   ```
5. **Always end with `halt`.** Without it, the interpreter runs off the end of the program.

---

## 🏛️ Architectural Note: PAL as the Glue
PAL sits at the center of the TPMOS architecture:

```
┌─────────────┐
│   MODULE    │ The Brain (C code)
│  (Manager)  │
└──────┬──────┘
       │ calls
       ▼
┌─────────────┐
│    PAL      │ The Glue (Assembly scripts)
│  (prisc+x)  │
└──────┬──────┘
       │ calls
       ▼
┌─────────────┐
│     OP      │ The Muscle (C binaries)
│  (move_etc) │
└──────┬──────┘
       │ updates
       ▼
┌─────────────┐
│  STATE.TXT  │ The Reality (Plaintext files)
│   (Disk)    │
└──────┬──────┘
       │ triggers
       ▼
┌─────────────┐
│  RENDERER   │ The Display (ASCII or GL)
│  (Screen)   │
└─────────────┘
```

**Modules** (Brains) call **PAL scripts**, which call **Ops** (Muscles), which update **Piece state files**, which trigger **Renders**. This is the complete TPMOS pipeline.

---

## 🏋️ Developer Exercise: Write a PAL Script
Write a PAL script that:
1. Creates a user profile
2. Moves the player 5 steps right
3. Checks if player reached a treasure tile
4. Updates happiness if treasure found
5. Renders the final state

**Solution:**
```asm
; treasure_hunt.asm
start:
    OP user::create_profile "hunter"
    sleep 200

    ; Move 5 steps right
    OP playrm::move_entity "hunter" "right"
    sleep 100
    OP playrm::move_entity "hunter" "right"
    sleep 100
    OP playrm::move_entity "hunter" "right"
    sleep 100
    OP playrm::move_entity "hunter" "right"
    sleep 100
    OP playrm::move_entity "hunter" "right"
    sleep 100

    ; Check happiness (treasure interaction is automatic in move_entity)
    read_state r1, "hunter", "happiness"
    addi r2, r0, 60
    beq r1, r2, treasure_found  ; If happiness >= 60, likely found treasure

    ; No treasure
    OP playrm::render_map
    halt

treasure_found:
    OP playrm::fuzzpet_action "hunter" "celebrate"
    hit_frame
    OP playrm::render_map
    halt
```

---

## ⚠️ Common Pitfalls

### Pitfall 1: Infinite Loops in PAL Scripts
**Symptom:** System hangs, CPU spikes to 100%.
**Cause:** `j` instruction without proper exit condition, or recursive `call` without base case.
**Fix:** Always ensure loops have a terminating condition:
```asm
; ❌ BAD: Infinite loop
loop:
    OP playrm::move_entity "player" "right"
    j loop

; ✅ GOOD: Bounded loop
addi r1, r0, 0    ; counter = 0
addi r2, r0, 5    ; max = 5
loop:
    beq r1, r2, done
    OP playrm::move_entity "player" "right"
    addi r1, r1, 1
    sleep 100
    j loop
done:
    halt
```

### Pitfall 2: Forgetting `sleep` Between Heavy Ops
**Symptom:** Ops fail silently, state doesn't update.
**Cause:** Filesystem hasn't flushed previous write before next op reads.
**Fix:** Add `sleep 50-200` between ops that read/write state:
```asm
OP playrm::move_entity "player" "right"
sleep 100  ; Let state flush to disk
OP playrm::render_map
```

### Pitfall 3: Cross-Project Call Failures
**Symptom:** `OP other_project::some_op` does nothing.
**Cause:** Target project not installed via Fondu, or op not in catalog.
**Fix:** Run `./fondu --install other_project` first, verify op exists in `ops_catalog.txt`.

### Pitfall 4: Not Ending with `halt`
**Symptom:** Interpreter crashes or executes garbage memory after script.
**Cause:** Script falls off the end without `halt`.
**Fix:** Every execution path must end with `halt`.

---

## 🏛️ Scholar's Corner: The "Assembly That Assembled Itself"
In the early days of TPMOS, a developer wrote a PAL script to automate the process of creating new PAL scripts. The script would read a template, substitute variable names, and write the output to a new `.asm` file. One day, the developer accidentally pointed the script at itself. The meta-script generated a new version of itself, which generated another, which generated another... Within seconds, there were 10,000 copies of the script filling the disk. This became known as **"The Assembly That Assembled Itself."** It taught us the critical importance of **output path validation** and the power (and danger) of self-referential systems. 📜♾️

---

## 📝 Study Questions
1.  What is the difference between `call` and `OP` in PAL?
2.  Why does PAL use `fork()/exec()` for custom ops instead of direct function calls?
3.  Explain the role of `sleep` in PAL scripts. What happens if you remove it?
4.  **Write a PAL script** that moves a player from position (0,0) to (3,3) diagonally (right, down, right, down, right, down).
5.  **True or False:** PAL scripts share memory with the module that calls them.
6.  **Scenario:** You wrote a PAL script that calls `OP user::create_profile`, but nothing happens. What are three possible causes?
7.  **Critical Thinking:** Why is PAL designed as an assembly language rather than a high-level scripting language like Python?

---
[Return to Index](INDEX.md)
