# ⚡ Chapter 11: The Infinite Loop (Recursive Forge)
TPMOS is not just an OS; it is a **Recursive World Engine**. By combining `op-ed` with `PAL`, we create a "Forge" where reality can build itself. 🌀🧱

---

## 🎮 RPGMaker-Compliant World Building
`op-ed` and `LSR` are designed with **RPGMaker-style parity** in mind.
*   **The Grid:** All maps use standardized tile coordinate systems (X, Y, Z).
*   **The Piece:** Just as RPGMaker has "Events," TPMOS has "Pieces." Every Piece can have a script (`.asm`) triggered by touch, interaction, or timer.
*   **Universal Compatibility:** A project built in `op-ed` is natively playable in `playrm` or `lsr`. The editor is the game, and the game is the editor. 🔄

---

## ⚔️ The PAL Auto-Battler
Recursive PAL scripting allows for **Auto-Battlers** and autonomous agents.
*   **Chimochio Bots:** These aren't just NPCs; they are bots that run their own PAL scripts to navigate, mine, and fight.
*   **In-Game Editor:** Players can use a simplified `op-ed` *within the game* to "program" their bot's behavior using PAL. This creates a recursive loop: You are playing a game about a bot that is playing a game about survival. 🤖🎮

### Code Example: PAL Auto-Battler Script
```asm
; auto_battler.asm - Bot combat loop
start:
    ; Find nearest enemy
    call scanner::find_nearest_enemy r1, r2  ; r1=x, r2=y

    ; Move toward enemy
    read_pos r3, r4, "chimochio_bot"
    beq r1, r3, same_x  ; If same X, check Y
    blt r1, r3, move_right
    j move_left

same_x:
    beq r2, r4, in_range  ; If same Y too, we're adjacent
    blt r2, r4, move_down
    j move_up

move_right:
    OP playrm::move_entity "chimochio_bot" "right"
    sleep 100000
    j start

move_left:
    OP playrm::move_entity "chimochio_bot" "left"
    sleep 100000
    j start

move_down:
    OP playrm::move_entity "chimochio_bot" "down"
    sleep 100000
    j start

move_up:
    OP playrm::move_entity "chimochio_bot" "up"
    sleep 100000
    j start

in_range:
    ; Attack!
    OP playrm::fuzzpet_action "chimochio_bot" "attack"
    sleep 200000

    ; Check if enemy defeated
    read_state r5, "enemy_01", "health"
    beq r5, r0, victory

    ; Enemy still alive, continue fighting
    j start

victory:
    call bot_tester::log_pass "Enemy defeated!"
    halt
```

---

## 🔌 From Pixels to Silicon: Circuit Simulation
Because of the **Scale-Free Philosophy** (Chapter 1), TPMOS doesn't distinguish between a character and a component.
*   **NAND Gates as Pieces:** If a NAND gate is a sovereign Piece with state `on=1`, you can build a CPU in TPMOS. 💾
*   **Recursive Chip Design:** AI-Labs (Chapter 10) can distill the laws of physics and electricity. This turns TPMOS into an **Electrical Chip Design Suite**, where the OS can simulate its own future hardware at a physical level.
*   **The Digital Twin:** AI models simulate circuits, thermodynamics, and physical stress, then generate the `.pdl` and `.c` code to manifest them in the "World." 🔋📐

### Code Example: NAND Gate as a Piece
A NAND gate Piece definition:
```
META | piece_id | nand_gate_01
META | version | 1.0

STATE | type | nand_gate
STATE | input_a | 0
STATE | input_b | 0
STATE | output | 1
STATE | on_map | 1
STATE | pos_x | 5
STATE | pos_y | 3

METHOD | evaluate | projects/fuzz-op/ops/+x/nand_eval.+x nand_gate_01
```

### Code Example: NAND Evaluation Op
```c
// nand_eval.c - Evaluate NAND gate logic
int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *gate_id = argv[1];

    resolve_paths();

    int a = get_state_int(gate_id, "input_a");
    int b = get_state_int(gate_id, "input_b");

    // NAND: output = !(a && b)
    int output = !(a && b);

    set_state_int(gate_id, "output", output);
    hit_frame_marker();

    return 0;
}
```

### Code Example: Circuit Simulation Loop
```c
// circuit_sim.c - Clock-driven circuit evaluation
void run_circuit_simulation() {
    int clock_state = 0;

    while (!g_shutdown) {
        // Toggle clock
        clock_state = !clock_state;
        set_state_int("clock", "state", clock_state);

        if (clock_state) {
            // Rising edge: evaluate all gates
            DIR *dir = opendir("projects/circuit/pieces/");
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, "nand") != NULL) {
                    char cmd[4096];
                    snprintf(cmd, sizeof(cmd),
                             "projects/fuzz-op/ops/+x/nand_eval.+x %s",
                             entry->d_name);
                    run_command(cmd);
                }
            }
            closedir(dir);
        }

        usleep(50000);  // 20Hz clock (50ms period)
    }
}
```

---

## 🤝 The Synergy of Test & Play
The exact same PAL scripts used to **Test** the OS for bugs (Chapter 9) are used to drive **Game Logic** and **Bot AI**.
*   **Assert = Victory:** In testing, an "Assert" checks for a bug. In an auto-battler, an "Assert" checks for a win condition.
*   **The Recursive Reality:** "If a human can click it, a piece can script it." Every manual action in `op-ed` can be automated by a PAL script, allowing the world to expand itself even when the user is away. 🌙

---

## 🛠️ Developer Example: Building a 4-Bit Adder

### Step 1: Create NAND Gate Pieces
Create 20+ NAND gate Pieces on a map, each with unique `piece_id`:
```
nand_001: pos_x=1, pos_y=1, input_a=0, input_b=0, output=1
nand_002: pos_x=2, pos_y=1, input_a=0, input_b=0, output=1
...
```

### Step 2: Wire Them Together
Wire gates by connecting outputs to inputs via state references:
```c
// Wire nand_001 output to nand_003 input_a
int out = get_state_int("nand_001", "output");
set_state_int("nand_003", "input_a", out);
```

### Step 3: Run Clock Pulse
```bash
# Start circuit simulation
./projects/circuit/manager/+x/circuit_manager.+x
```

### Step 4: Verify Output
Set input bits on the 4-bit adder inputs, wait for propagation, read output bits:
```c
// Read 4-bit sum
int sum_0 = get_state_int("output_0", "value");
int sum_1 = get_state_int("output_1", "value");
int sum_2 = get_state_int("output_2", "value");
int sum_3 = get_state_int("output_3", "value");
int total = sum_0 + (sum_1 << 1) + (sum_2 << 2) + (sum_3 << 3);
printf("4-bit adder result: %d\n", total);
```

---

## ⚠️ Common Pitfalls

### Pitfall 1: Gate Evaluation Order
**Symptom:** Circuit produces wrong output, or output flickers.
**Cause:** Gates evaluated before their inputs have updated (race condition).
**Fix:** Topologically sort gates before evaluation:
```c
// Evaluate gates in dependency order (inputs before outputs)
for (int depth = 0; depth < max_depth; depth++) {
    evaluate_gates_at_depth(depth);
}
```

### Pitfall 2: Clock Too Fast
**Symptom:** State changes don't propagate, circuit appears frozen.
**Cause:** Clock period shorter than file I/O latency.
**Fix:** Minimum clock period should be 50ms (20Hz) to allow filesystem flush:
```c
usleep(50000);  // 50ms = 20Hz max
```

### Pitfall 3: Infinite PAL Recursion
**Symptom:** System hangs, stack overflow.
**Cause:** PAL script calls itself directly or indirectly without base case.
**Fix:** Always ensure recursive `call` has a terminating condition:
```asm
; ❌ BAD: Infinite recursion
loop:
    call loop

; ✅ GOOD: Bounded recursion
addi r1, r0, 5
recurse:
    beq r1, r0, done
    addi r1, r1, -1
    call recurse
done:
    halt
```

---

## 🏛️ Scholar's Corner: The "Bootloader in a Piece"
One of our senior architects once built a working 4-bit CPU entirely inside a TPMOS project. Every logic gate (NAND, NOR, NOT) was a sovereign Piece on a 64x64 map. The "Clock Pulse" was driven by the system's `renderer_pulse.txt`. When the project was launched, you could see the "electricity" (the state changing from 0 to 1) flowing through the tiles. He eventually loaded a mini-bootloader into the map's memory pieces, effectively running a **Computer inside a Computer inside a Computer.** This proved that in TPMOS, **"Scale is just a variable."** 📟🔌🌀

---

## 📝 Study Questions
1.  Explain the concept of "Recursive Chip Design" within TPMOS.
2.  How does the "Scale-Free Philosophy" enable the simulation of hardware?
3.  Describe how an in-game editor can be used to "program" bots in real-time.
4.  **Write the code** for a NOR gate Piece evaluation op.
5.  **True or False:** A NAND gate Piece and a Player Piece follow different underlying rules in TPMOS.
6.  **Scenario:** Your 4-bit adder produces incorrect results. The first two bits are right but the last two are wrong. What's the most likely cause?

---
[Return to Index](INDEX.md)
