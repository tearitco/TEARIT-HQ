# 🏭 Chapter 5: The App Factory
How do you build a new tool for TPMOS? It's easier than you think! We call this the **App Factory** pattern. 🛠️🏗️

---

## 🧬 What is an App?
In TPMOS, an "App" is just a directory with a specific DNA file. 🧬

### 📂 The App Anatomy
```text
pieces/apps/my_new_app/
├── my_new_app.pdl      # The App DNA
├── layouts/            # The UI screens (.chtpm files)
├── manager/            # The Brain (+x/module.+x)
└── pieces/             # Local pieces used by the app
```

---

## 📝 The App DNA (`.pdl`)
Every app needs a descriptor so the OS knows how to launch it.

```pdl
<app_id>op-ed</app_id>
<version>6.0</version>
<layout_path>pieces/apps/op-ed/layouts/main.chtpm</layout_path>
<module_path>pieces/apps/op-ed/manager/+x/op-ed_module.+x</module_path>
```

---

## 🛠️ Featured Apps
TPMOS comes with several powerful system tools:

### 🖋️ `op-ed` (The Piece Editor)
The most important tool! It allows you to create, delete, and modify Pieces in real-time. 
*   **Feature:** Roundtrip Parity. You can edit a file, save it, and the game updates instantly.

### 🐾 `fuzz-ops` (The Pet Manager)
A complex simulation app that manages "Fuzzpets". 
*   **Feature:** It uses **PAL (Prisc Assembly Language)** to give pets AI behaviors like "Hunger" and "Play".

### 👤 `user` (The Identity Manager)
Handles logins, profiles, and preferences. 
*   **Feature:** Persistent user state across different projects.

---

## 🛰️ The Horizon: Upcoming Apps
Several high-impact apps are currently in the incubation phase:

*   🤖 **AI-Labs**: A laboratory for local LLM (like Qwen/Gemma) integration. Used for tech R&D and autonomous codegen. (Covered in CH10)
*   🌕 **LSR (Lunar Streetrace Raider)**: A "Civ-Lite" simulation where AI agents build lunar civilizations. (Covered in CH10)
*   ⛓️ **P2P-NET**: A decentralized networking layer with blockchain and NFT trading. (Covered in CH10)

For deep-dives into **fuzz-op** (pet simulation) and **op-ed** (piece editor), see CH7. For **PAL scripting** used by these apps, see CH6.

---

## 💻 Code Example: App Manager Loop
Every app's manager follows the same CPU-safe pattern (see CH4 for the full template). Here's a simplified example from `player_manager.c`:

```c
// player_manager.c - Main app loop
int main(void) {
    signal(SIGINT, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    char *hist_path = build_path_malloc("pieces/apps/player_app/history.txt");
    long last_pos = 0;
    struct stat st;

    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);  // 100ms when not in focus
            continue;
        }

        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(hist_path, "r");
            if (hf) {
                fseek(hf, last_pos, SEEK_SET);
                int key;
                while (fscanf(hf, "%d", &key) == 1) {
                    route_input(key);  // Route to appropriate op
                }
                last_pos = ftell(hf);
                fclose(hf);
            }
        }
        usleep(16667);  // 60 FPS
    }
    free(hist_path);
    return 0;
}
```

The `route_input()` function decides which op to call based on the key pressed.

---

## 💻 Code Example: op-ed Module (Editor App)
The `op-ed` module handles cursor movement and tile placement:

```c
// op-ed_module.c - Editor input handling
void process_key(int key) {
    int cx = get_state_int("editor", "cursor_x");
    int cy = get_state_int("editor", "cursor_y");

    if (key == 'w' || key == 1002) cy--;       // Up
    else if (key == 's' || key == 1003) cy++;  // Down
    else if (key == 'a' || key == 1000) cx--;  // Left
    else if (key == 'd' || key == 1001) cx++;  // Right
    else if (key == 10 || key == 13) {
        // ENTER: Place selected tile
        place_tile_at(cx, cy, current_tile);
    }
    else if (key == 27) {
        // ESC: Return to selector
        strcpy(active_target_id, "selector");
    }

    set_state_int("editor", "cursor_x", cx);
    set_state_int("editor", "cursor_y", cy);
    hit_frame_marker();  // Trigger render
}
```

---

## 📝 App DNA with Methods
An app's `.pdl` file can declare methods (actions the app can perform):

```
META | app_id | fuzz-op
META | version | 2.0
META | layout_path | projects/fuzz-op/layouts/fuzz-op.chtpm
META | module_path | projects/fuzz-op/manager/+x/fuzz-op_manager.+x

METHOD | feed | pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzpet feed
METHOD | sleep | pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzpet sleep
METHOD | play | pieces/apps/playrm/ops/+x/fuzzpet_action.+x fuzzpet play
```

Methods are callable by PAL scripts and the UI. See CH6 for PAL integration.

---

## 📦 Fondu App Installation
Fondu doesn't just install ops - it can install entire apps:

```bash
./fondu --install user
```

This copies:
1. `manager/+x/user_manager.+x` → `pieces/apps/installed/user/manager/`
2. `ops/+x/*.+x` → `pieces/apps/installed/user/ops/+x/`
3. `layouts/*.chtpm` → `pieces/apps/installed/user/layouts/`
4. Updates `os.chtpm` button registry

---

## 🛠️ Developer Example: Creating a New App

### Step 1: Directory Structure
```
pieces/apps/my_app/
├── my_app.pdl
├── layouts/
│   └── main.chtpm
├── manager/
│   └── my_app.c
└── pieces/
    └── my_piece/
        └── state.txt
```

### Step 2: Minimum Viable Manager (~50 lines)
```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

static volatile sig_atomic_t g_shutdown = 0;
static void handle_sigint(int sig) { g_shutdown = 1; }

int main(void) {
    signal(SIGINT, handle_sigint);
    setpgid(0, 0);

    // Initialize state
    FILE *f = fopen("pieces/apps/my_app/pieces/my_piece/state.txt", "w");
    if (f) {
        fprintf(f, "name=MyApp\nstatus=running\n");
        fclose(f);
    }

    // Simple loop
    while (!g_shutdown) {
        // Check for input, update state, trigger render
        usleep(16667);  // 60 FPS
    }
    return 0;
}
```

### Step 3: Add a Layout (`main.chtpm`)
```xml
<layout id="my_app.chtpm">
    <row>
        <cell>My App</cell>
    </row>
    <row>
        <cell>Status: ${status}</cell>
    </row>
</layout>
```

### Step 4: Register in `os.chtpm`
Add a button to `pieces/chtpm/layouts/os.chtpm`:
```xml
<button label="My App" onClick="KEY:a" href="pieces/apps/my_app/my_app.pdl" />
```

### Step 5: Compile and Test
```bash
gcc -o pieces/apps/my_app/manager/+x/my_app.+x pieces/apps/my_app/manager/my_app.c -pthread
./button.sh run
```
Press `a` to launch your app!

---

## ⚠️ Common Pitfalls

### Pitfall 1: Forgetting `is_active_layout()` Check
**Symptom:** App consumes CPU even when not on screen.
**Fix:** Always check layout before processing:
```c
if (!is_active_layout()) { usleep(100000); continue; }
```

### Pitfall 2: Not Registering App in `os.chtpm`
**Symptom:** App compiles but can't be launched from main menu.
**Fix:** Add `<button>` to `os.chtpm` with correct `href` to `.pdl` file.

### Pitfall 3: App DNA Path Mismatch
**Symptom:** OS can't find the module binary.
**Fix:** Ensure `module_path` in `.pdl` matches actual binary location (`+x/` directory).

---

---

## 🚀 Launching your App
To see your app in the OS, you just need to add a button to the main `os.chtpm` layout:

```xml
<button label="🚀 My New App" onClick="KEY:m" href="pieces/apps/my_new_app/my_new_app.pdl" />
```

> 💡 **Pro Tip:** Start by duplicating an existing app (like `playrm`) and changing the names. It's the fastest way to learn! 🏃‍♂️💨

---

## 🏛️ Scholar's Corner: The "Day the Editor Became the Game"
One of the most profound moments in TPMOS history was when a developer accidentally launched the **`op-ed`** (the editor) inside the `fuzz-op` (the pet game). Instead of crashing, the system simply rendered the editor's selector on top of the pet's map. The developer realized they could "play" the game by editing the world in real-time—placing walls to trap pets or changing their health with a few keystrokes. This blurred the line between **playing** and **building**, leading to our philosophy that "The Editor is the Game." 🖋️🎮

---

## 📝 Study Questions
1.  What is the minimum requirement for a directory to be considered an "App" in TPMOS?
2.  Which app is responsible for managing user profiles?
3.  How do you add a new app to the main OS menu?
4.  **True or False:** Every app must have its own `.pdl` file.

---
[Return to Index](INDEX.md)
