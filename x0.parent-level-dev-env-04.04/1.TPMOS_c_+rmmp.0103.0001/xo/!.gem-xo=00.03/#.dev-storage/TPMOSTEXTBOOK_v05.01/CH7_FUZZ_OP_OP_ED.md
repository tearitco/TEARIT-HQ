# 🎮 Chapter 7: fuzz-op & op-ed: The Flagship Apps
TPMOS comes with two flagship applications that demonstrate the full power of the Piece architecture: **fuzz-op** (a pet simulation) and **op-ed** (a real-time piece editor). Together, they prove that "The Editor is the Game." 🖋️🎮

---

## 🐾 fuzz-op: The Pet Simulation

### Architecture Overview
fuzz-op is a pet simulation project that demonstrates the complete TPMOS pipeline:
- **Manager:** `projects/fuzz-op/manager/fuzz-op_manager.c` (~600 lines)
- **Zombie AI:** `pieces/apps/fuzzpet_app/traits/zombie_ai.c` (~200 lines)
- **Ops:** Uses `move_entity.+x`, `render_map.+x`, and custom fuzzpet actions
- **Pieces:** `fuzzpet`, `zombie_01`, `xlector`, and map tiles

### Code Example: fuzz-op Manager Main Loop
The manager handles input routing, xlector system, Z-level changes, and zombie AI calls:

```c
// fuzz-op_manager.c - Input routing
void route_input(int key) {
    // Handle quit key (return to xlector)
    if (key == '9' || key == 27) {
        strcpy(active_target_id, "xlector");
        log_resp("Returned to Xlector.");
    }

    // Handle selection (ENTER selects entity at cursor)
    if (strcmp(active_target_id, "xlector") == 0 && key == 10) {
        int cx = get_state_int_fast("xlector", "pos_x");
        int cy = get_state_int_fast("xlector", "pos_y");

        // Find entity at xlector position
        DIR *dir = opendir(pieces_dir);
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strstr(entry->d_name, "zombie") != NULL) continue;  // Can't select zombies
            int tx = get_state_int_fast(entry->d_name, "pos_x");
            int ty = get_state_int_fast(entry->d_name, "pos_y");
            if (tx == cx && ty == cy) {
                strncpy(active_target_id, entry->d_name, 63);
                log_resp("Selected %s.", entry->d_name);
                break;
            }
        }
        closedir(dir);
    }

    // Handle movement (WASD or arrows)
    if (key == 'w' || key == 'a' || key == 's' || key == 'd' ||
        (key >= 1000 && key <= 1003)) {
        char* trait = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
        char dir_str[16];
        if (key == 'w' || key == 1002) strcpy(dir_str, "up");
        else if (key == 's' || key == 1003) strcpy(dir_str, "down");
        else if (key == 'a' || key == 1000) strcpy(dir_str, "left");
        else if (key == 'd' || key == 1001) strcpy(dir_str, "right");

        pid_t p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout);
            execl(trait, trait, active_target_id, dir_str, NULL);
            exit(1);
        } else waitpid(p, NULL, 0);
        free(trait);

        // Call zombie AI to chase player
        char* zombie_cmd = build_path_malloc(
            "pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x");
        p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout);
            execl(zombie_cmd, zombie_cmd, "zombie_01", active_target_id, NULL);
            exit(1);
        } else waitpid(p, NULL, 0);
        free(zombie_cmd);
    }

    perform_mirror_sync();
    save_manager_state();
    trigger_render();
}
```

**Key patterns demonstrated:**
1. **Xlector system:** Press 9 to return to xlector, ENTER to select entity at cursor
2. **Z-level changes:** `x` = up, `z` = down (range 0-2 for sky/ground)
3. **Zombie AI calls:** After every player move, zombie_ai.+x is called to chase the target
4. **Mirror sync:** Updates fuzzpet_mirror state for UI display
5. **Render trigger:** Calls render_map after every state change

### Code Example: Zombie AI Chase Logic
The zombie AI (`zombie_ai.c`) implements simple chase behavior:

```c
// zombie_ai.c - Chase AI implementation
int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    const char *zombie_id = argv[1];
    const char *target_id = argv[2];

    resolve_paths();  // From location_kvp

    // Get zombie position
    int zx = get_state_int(zombie_id, "pos_x");
    int zy = get_state_int(zombie_id, "pos_y");
    int zz = get_state_int(zombie_id, "pos_z");
    if (zx == -1 || zy == -1) return 0;  // Zombie doesn't exist
    if (zz == -1) zz = 0;

    // Get target (player) position
    int tx = get_state_int(target_id, "pos_x");
    int ty = get_state_int(target_id, "pos_y");
    int tz = get_state_int(target_id, "pos_z");
    if (tz == -1) tz = 0;

    // Only chase if on same Z level
    if (zz != tz) return 0;

    // Don't chase if already adjacent (attack range)
    int dist = abs(tx - zx) + abs(ty - zy);
    if (dist <= 1) return 0;

    // Simple chase: move X first, then Y
    int next_x = zx, next_y = zy;
    if (tx > zx) next_x++;
    else if (tx < zx) next_x--;
    else if (ty > zy) next_y++;
    else if (ty < zy) next_y--;

    // Bounds check (0-19 map)
    if (next_x < 0) next_x = 0;
    if (next_x > 19) next_x = 19;
    if (next_y < 0) next_y = 0;
    if (next_y > 19) next_y = 19;

    // Update zombie position
    set_state_int(zombie_id, "pos_x", next_x);
    set_state_int(zombie_id, "pos_y", next_y);

    return 0;
}
```

### Piece State Examples
```
# zombie_01/state.txt
name=Zombie
type=zombie
pos_x=9
pos_y=2
pos_z=0
on_map=1
behavior=aggressive
speed=1
map_id=map_01_z0.txt

# xlector/state.txt
name=Xlector
type=xlector
pos_x=5
pos_y=4
pos_z=0
on_map=1
map_id=map_01_z0.txt
```

### Future Features
- **Multi-zombie support:** AI flocking behavior, multiple zombies chasing
- **Hunger/happiness decay:** Stats decrease over time, requiring player attention
- **Breeding/genetics system:** Pets reproduce with inherited traits
- **Inventory and item collection:** Pick up and use items
- **Multi-map exploration:** Z-levels 0-2 with different maps
- **GL-OS 3D rendering:** Pet world rendered in full 3D (see CH8)

---

## 🖋️ op-ed: The Piece Editor

### Architecture Overview
op-ed is a real-time grid editor for creating and modifying TPMOS maps:
- **Manager:** `projects/op-ed/manager/op-ed_manager.c`
- **Features:** Cursor movement, tile placement, MAPS and GLYPHS palettes, save/load
- **Roundtrip Parity:** Edit a file, save it, and the game updates instantly

### Code Example: op-ed Module
The editor handles cursor movement and tile placement:

```c
// op-ed_module.c - Editor input handling
void process_key(int key) {
    int cx = get_state_int("editor", "cursor_x");
    int cy = get_state_int("editor", "cursor_y");

    // Cursor movement
    if (key == 'w' || key == 1002) cy--;       // Up
    else if (key == 's' || key == 1003) cy++;  // Down
    else if (key == 'a' || key == 1000) cx--;  // Left
    else if (key == 'd' || key == 1001) cx++;  // Right

    // Place tile (ENTER)
    else if (key == 10 || key == 13) {
        place_tile_at(cx, cy, current_tile);
        hit_frame_marker();  // Instant update!
    }

    // Switch palette (2 = MAPS, 3 = GLYPHS)
    else if (key == '2') current_palette = PALETTE_MAPS;
    else if (key == '3') current_palette = PALETTE_GLYPHS;

    // Toggle emoji rendering (4)
    else if (key == '4') emoji_mode = !emoji_mode;

    // Quit (ESC)
    else if (key == 27) {
        strcpy(active_target_id, "xlector");
    }

    set_state_int("editor", "cursor_x", cx);
    set_state_int("editor", "cursor_y", cy);
    hit_frame_marker();
}
```

### Map File Format
Maps are simple text grids - what you see is what you get:

```
####################
#  ........@......T#
#  ......Z........T#
#  ...XR...@......T#
#  ....R..........T#
#  ....R......@...T#
#  ....R..........T#
#  ................#
#                  #
####################
```

**Tile Legend:**
- `#` = Wall
- `.` = Empty floor
- `@` = Decorative item
- `T` = Treasure
- `R` = Resource
- `X` = Player start
- `Z` = Zombie spawn
- ` ` (space) = Void (outside map)

### Palette System
op-ed has two palettes:
- **MAPS:** Wall, floor, treasure, resources, player start, zombie spawn
- **GLYPHS:** Decorative characters (@, *, +, etc.) for detail work

Press `2` for MAPS, `3` for GLYPHS. Press `4` to toggle emoji rendering.

### 🖋️ Live GUI Scripting (PAL Editor)
A major upgrade to `op-ed` is the **PAL Editor**. Pressing `8` while a Piece is selected opens a visual assembly environment where you can write and bind behaviors (e.g., `on_interact`) directly to that Piece. This completes the "Editor is the Game" loop.

### Roundtrip Parity
The key feature of op-ed is **roundtrip parity**: when you place a tile and press ENTER, the change is written directly to the map file. The next render cycle reads the updated file and displays it instantly. There's no "save" button - the file IS the game state.

### Future Features
- **Multi-layer editing:** Edit Z-levels 0-2 simultaneously
- **Bulk operations:** Fill regions, copy/paste map sections
- **Template library:** Pre-built map sections (rooms, corridors, dungeons)
- **GL-OS live preview:** Edit in 2D, see in 3D simultaneously
- **Collaborative editing:** Multi-user via P2P-NET (see CH10)
- **Undo/redo history:** Full edit history with rollback

---

## 🔗 Shared Architecture Patterns

Both fuzz-op and op-ed demonstrate core TPMOS principles:

| Pattern | fuzz-op | op-ed |
|---------|---------|-------|
| 12-step pipeline | ✅ Full pipeline | ✅ Full pipeline |
| Read/write Piece state | ✅ zombie_01, xlector | ✅ editor cursor |
| fork()/exec() for ops | ✅ move_entity, zombie_ai | ✅ place_tile |
| Trigger renders via frame_changed.txt | ✅ | ✅ |
| CPU-safe mandates | ✅ Signal handling, throttling | ✅ Signal handling, throttling |
| PAL script integration | ✅ fuzzpet actions | ✅ (future) |

---

## 🛠️ Developer Example: Extending fuzz-op

Let's add a new feature: **pet food** that the player can place and the fuzzpet can eat.

### Step 1: Add New Piece Type
Create `projects/fuzz-op/pieces/pet_food/state.txt`:
```
name=Pet Food
type=item
pos_x=0
pos_y=0
pos_z=0
on_map=0
nutrition=20
```

### Step 2: Write the `eat_food` Op
Create `projects/fuzz-op/ops/src/eat_food.c`:
```c
// eat_food.c - Pet eats food, gains energy
int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    const char *pet_id = argv[1];
    const char *food_id = argv[2];

    resolve_paths();

    int nutrition = get_state_int(food_id, "nutrition");
    if (nutrition <= 0) return 0;  // No nutrition left

    int energy = get_state_int(pet_id, "energy");
    if (energy != -1) {
        int new_energy = energy + nutrition;
        if (new_energy > 100) new_energy = 100;
        set_state_int(pet_id, "energy", new_energy);
    }

    // Remove food (set on_map=0)
    set_state_int(food_id, "on_map", 0);

    hit_frame_marker();
    return 0;
}
```

Compile: `gcc -o projects/fuzz-op/ops/+x/eat_food.+x projects/fuzz-op/ops/src/eat_food.c`

### Step 3: Update Manager
In `fuzz-op_manager.c`, add handling for the 'e' key:
```c
if (key == 'e') {
    // Find nearest food and eat it
    char* cmd = build_path_malloc(
        "projects/fuzz-op/ops/+x/eat_food.+x fuzzpet pet_food");
    run_command(cmd);
    free(cmd);
}
```

### Step 4: Test with op-ed
1. Launch op-ed (`button.sh run`, select op-ed from menu)
2. Place a `pet_food` piece on the map
3. Switch back to fuzz-op
4. Move fuzzpet adjacent to food
5. Press 'e' - fuzzpet should gain energy!

---

## ⚠️ Common Pitfalls

### Pitfall 1: Zombie AI Not Moving (Hardcoded Paths)
**Symptom:** Zombie stays in place, never chases player.
**Cause:** `zombie_ai.+x` binary doesn't exist, or source has hardcoded paths like `projects/fuzz-op/` instead of resolving from `location_kvp`.
**Fix:** Ensure `zombie_ai.c` uses `resolve_paths()` and is compiled to `pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x`. Add compile entry to `compile_all.sh`.

### Pitfall 2: Manager Running from Wrong Directory
**Symptom:** Game launches but nothing works - no pieces, no maps, no zombie.
**Cause:** Manager binary was compiled/launched from a different directory (e.g., Trash, old backup).
**Fix:** Kill all processes (`./button.sh kill`), recompile from correct directory (`./button.sh compile`), restart (`./button.sh run`).

### Pitfall 3: Xlector State Corruption
**Symptom:** Selecting an entity breaks movement or causes crashes.
**Cause:** Xlector's `map_id` doesn't match the entity's `map_id`.
**Fix:** When selecting an entity, update xlector's `map_id` to match:
```c
// In route_input, after selecting entity:
char entity_map_id[256] = "";
// Read entity's map_id from its state.txt
// Write to xlector's state.txt
```

### Pitfall 4: op-ed Changes Not Appearing in Game
**Symptom:** You place tiles in op-ed but the game map doesn't update.
**Cause:** op-ed didn't hit `frame_changed.txt` after writing the map file.
**Fix:** Ensure `hit_frame_marker()` is called after every tile placement.

### Pitfall 5: Z-Level Desync
**Symptom:** Player moves up/down Z-levels but map doesn't change.
**Cause:** Manager's `current_z_val` not updated, or `save_manager_state()` writes wrong map name.
**Fix:** Update `current_z_val` on Z change, ensure `save_manager_state()` writes `current_map=map_01_z%d.txt` with correct Z.

---

## 🏛️ Scholar's Corner: The "Day the Editor Became the Game"
One of the most profound moments in TPMOS history was when a developer accidentally launched the **`op-ed`** (the editor) inside the `fuzz-op` (the pet game). Instead of crashing, the system simply rendered the editor's xlector on top of the pet's map. The developer realized they could "play" the game by editing the world in real-time—placing walls to trap pets or changing their health with a few keystrokes.

But the real revelation came next. The developer placed a `zombie_01` piece directly on top of the fuzzpet using op-ed. When they switched back to fuzz-op, the zombie immediately began chasing the pet (because the zombie AI was already running!). The line between **playing** and **building** had completely dissolved. This led to our philosophy that **"The Editor is the Game"** - and eventually to the recursive forge concept (CH11), where the act of building the world IS the gameplay. 🖋️🎮

---

## 📝 Study Questions
1.  How does the xlector system work in fuzz-op? What keys select and deselect entities?
2.  Why does the zombie AI check Z-level before chasing? What would happen without this check?
3.  Explain "roundtrip parity" in op-ed. Why is it significant?
4.  **Write the code** to add a new "water" tile to op-ed that decreases player energy when stepped on.
5.  **True or False:** fuzz-op and op-ed use different rendering pipelines.
6.  **Scenario:** You place a zombie in op-ed, switch to fuzz-op, but the zombie doesn't move. List three possible causes.
7.  **Critical Thinking:** How does the "Editor is the Game" philosophy change how we think about game design?

---
[Return to Index](INDEX.md)
