# 💪 Chapter 4: Muscles & Brains (Ops & Modules)
In TPMOS, we separate **Thinking** from **Doing**. This is the key to a stable, CPU-safe system. 🧠💪

---

## 🧠 The Brain (The Module)
The **Module** is a long-running background process (the "Brain"). Its job is to listen for user input and make decisions. 
*   **File:** Usually named `*_module.c` or `*_manager.c`.
*   **Job:** Polls the `history.txt` buffer and updates the `state.txt` mirror.

### 🛡️ The CPU-Safe Mandate
Because Modules run in the background, they must be "polite" to the CPU. We follow the **CPU-Safe Template**:
1.  **Throttling:** If the app isn't on screen, the Module sleeps for 100ms. If it is on screen, it sleeps for 16ms (60 FPS). 😴
2.  **Stat-First:** Don't read the history file unless its size has changed (`stat()`).
3.  **Signal Handling:** Always catch `SIGINT` (Ctrl+C) so you can clean up your children. 🧹

### 🛡️ The Network & Path Safety Mandates (New April 2026)
As TPMOS evolves, we've added critical mandates for system stability:

1.  **No Hardcoded Paths Mandate:** NEVER scatter hardcoded system paths like `"pieces/master_ledger/plugins/+x/piece_manager.+x"` across multiple .c files. Instead, each .c file defines its OWN path constant and helper at the top of the file:
    ```c
    #define TPM_PIECE_MANAGER_CMD "'./pieces/master_ledger/plugins/+x/piece_manager.+x'"
    /* or for project_root-based calls: */
    #define TPM_PIECE_MANAGER_PATH "pieces/master_ledger/plugins/+x/piece_manager.+x"
    static char* pm_path_for(const char *project_root) {
        char *r = NULL;
        asprintf(&r, "%s/%s", project_root, TPM_PIECE_MANAGER_PATH);
        return r;
    }
    ```
    This keeps each file self-contained. **No shared `.h` headers** — TPMOS convention is: executable modules and data files only.

2.  **No Hardcoded Type/Glyph Mappings:** Piece type → icon mappings (npc → `&`, zombie → `Z`, etc.) belong in the external data file `pieces/os/type_registry.pdl`. Each .c file that needs them has its own local lookup function. If you add a new piece type, update `type_registry.pdl` AND the local lookup in each relevant .c file.

3.  **Network Safety Mandate:** Ghost listeners cause crashes! All network-aware modules must ensure their ports (default 8000-8010) are released on exit. The `kill_all.sh` script is the surgical tool for this. 释放端口! 🔌

---

## 💪 The Muscle (The Op)
The **Op** is a short-lived binary (the "Muscle"). It does one thing and then exits.
*   **File:** Usually ends in `.+x` (e.g., `move_entity.+x`).
*   **Job:** Performs a specific action like moving a player or placing a tile.

### 🚫 Why no `system()`?
We never use the `system()` command in C. Why? Because it's hard to kill! Instead, we use `fork()` and `exec()`.
*   **Fork:** Create a child process.
*   **Exec:** Turn that child into the Muscle.
*   **Wait:** The Brain waits for the Muscle to finish.

### Code Comparison: `system()` vs `fork()/exec()`

```c
// ❌ BAD: Can't kill, becomes orphaned on Ctrl+C
system("pieces/apps/playrm/ops/+x/move_entity.+x player up");

// ✅ GOOD: Full control, always reaped
char* op_path = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
pid_t pid = fork();
if (pid == 0) {
    // Child process: redirect output, execute
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execl(op_path, op_path, "player", "up", NULL);
    _exit(127);  // execl failed
} else if (pid > 0) {
    // Parent process: wait for child
    int status;
    waitpid(pid, &status, 0);
}
free(op_path);
```

**Why this matters:** When you press Ctrl+C, the signal handler sets `g_shutdown = 1`. The main loop exits cleanly. But if a `system()` call spawned a child, that child becomes orphaned and keeps running forever! With `fork()/exec()/waitpid()`, every child is always reaped.

---

## 🛡️ The CPU-Safe Module Template
This is the most important pattern in TPMOS. Every module follows this structure:

```c
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

static volatile sig_atomic_t g_shutdown = 0;

// Signal handler for graceful shutdown
static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

// CPU-Safe command execution
static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int main(void) {
    // STEP 1: Register signal handlers
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // STEP 2: Set process group for clean cleanup
    setpgid(0, 0);

    // Initialize
    resolve_paths();
    update_state();
    trigger_render();

    // STEP 3: History file polling setup
    char *hist_path = build_path_malloc("pieces/apps/player_app/history.txt");
    long last_pos = 0;
    struct stat st;

    // STEP 4: Main loop with shutdown flag check
    while (!g_shutdown) {
        // STEP 5: Focus-aware throttling
        if (!is_active_layout()) {
            usleep(100000);  // 100ms when not in focus
            continue;
        }

        // STEP 6: stat()-first polling
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                // New input available
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        process_key(key);
                        update_state();
                        trigger_render();
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            }
        }

        // STEP 7: Bounded sleep (60 FPS max)
        usleep(16667);
    }

    // Cleanup
    free(hist_path);
    return 0;
}
```

**Key Patterns:**
1. **Signal handling:** `handle_sigint` sets flag, main loop checks it
2. **Process group:** `setpgid(0, 0)` ensures Ctrl+C kills all children
3. **Focus throttling:** 100ms sleep when not active, 16ms when active
4. **stat()-first:** Only read history if file size changed
5. **Bounded sleep:** `usleep(16667)` = 60 FPS max, never tight loop

---

## 📦 Organizing Ops: The Centralized Directory Pattern

### ✓ DO: Centralized Ops Directory
All ops for a project should live in ONE central location:

```
projects/<project_id>/ops/
├── +x/                    # Compiled binaries
│   ├── create_profile.+x
│   ├── auth_user.+x
│   └── get_session.+x
├── src/                   # Source code (optional)
│   ├── create_profile.c
│   ├── auth_user.c
│   └── get_session.c
└── ops_manifest.txt       # Registry of all ops (REQUIRED for Fondu)
```

### ops_manifest.txt Format
```
# Comment lines start with #
# Format: op_name|description|args_format

create_profile|Creates a new user profile|project_path:string,username:string
auth_user|Authenticates a user session|project_path:string,username:string
get_session|Get current session data|project_path:string,username:string
```

### project.pdl Registration
In your `project.pdl`, declare that your project exposes ops:

```
[META]
project_id = user
exposes_ops = true
entry_layout = projects/user/layouts/user.chtpm

[OPS]
ops_dir = projects/user/ops/
manifest = projects/user/ops/ops_manifest.txt
```

### ✗ DON'T: Scatter Ops
```
✗ projects/<project>/traits/move_entity.c     (scattered in traits/)
✗ projects/<project>/plugins/move_entity.c    (scattered in plugins/)
✗ projects/<project>/manager/move_entity.c    (scattered in manager/)
```

---

## 🔧 Fondu Installation of Ops

When you run `./fondu --install <project>`, Fondu will:

1. Read `project.pdl` → finds `exposes_ops = true`
2. Read `ops/ops_manifest.txt` → gets list of ops
3. Copy `ops/+x/*.+x` to `pieces/apps/installed/<project>/ops/+x/`
4. Register ops in `pieces/os/ops_registry/<project>.txt`
5. Update `pieces/os/ops_catalog.txt`

Now other projects can call your ops via PAL scripts:
```
OP user::create_profile "player1"
OP user::auth_user "player1"
```

### Code Example: Fondu Installer Flow
The Fondu installer (`pieces/system/fondu/fondu.c`) reads project metadata and copies files:

```c
// Simplified fondu.c install flow
void install_project(const char* project_id) {
    char pdl_path[4096], manifest_path[4096];
    snprintf(pdl_path, sizeof(pdl_path), "projects/%s/project.pdl", project_id);

    // 1. Read project.pdl to check exposes_ops
    if (!pdl_has_key(pdl_path, "exposes_ops")) return;

    // 2. Read ops_manifest.txt
    snprintf(manifest_path, sizeof(manifest_path),
             "projects/%s/ops/ops_manifest.txt", project_id);
    FILE *mf = fopen(manifest_path, "r");
    if (!mf) return;

    // 3. Copy each op to installed directory
    char line[1024];
    while (fgets(line, sizeof(line), mf)) {
        if (line[0] == '#') continue;  // Skip comments
        char *op_name = strtok(line, "|");
        char src[4096], dst[4096];
        snprintf(src, sizeof(src), "projects/%s/ops/+x/%s.+x", project_id, op_name);
        snprintf(dst, sizeof(dst), "pieces/apps/installed/%s/ops/+x/%s.+x", project_id, op_name);
        copy_file(src, dst);
    }
    fclose(mf);

    // 4. Register in ops_registry
    // 5. Update ops_catalog
}
```

### Fondu Uninstall/Update
- **Uninstall:** `./fondu --uninstall <project>` removes files from `pieces/apps/installed/` and updates registry
- **Update:** `./fondu --update <project>` compares versions, copies only changed ops
- **Dependency Resolution:** If project A depends on project B's ops, Fondu installs B first

---

## 💪 Code Example: The Canonical Op (`move_entity.c`)

The `move_entity` op is the most-called Muscle in TPMOS. Here's how it works:

```c
/* move_entity.c - The canonical TPMOS Op */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 8192

char project_root[MAX_PATH] = ".";
char current_project[256] = "template";

// Resolve paths from location_kvp
void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_root") == 0)
                    snprintf(project_root, sizeof(project_root), "%s", trim_str(eq + 1));
            }
        }
        fclose(kvp);
    }
}

// Read integer state value
int get_state_int(const char* piece_id, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt",
             project_root, current_project, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256]; int val = -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq && strncmp(line, key, strlen(key)) == 0) {
            val = atoi(trim_str(eq + 1));
            break;
        }
    }
    fclose(f);
    return val;
}

// Write integer state value
void set_state_int(const char* piece_id, const char* key, int val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt",
             project_root, current_project, piece_id);
    // Read existing lines, update matching key, write back
    // (Standard read-modify-write pattern - see CH2 for full code)
}

// Hit frame marker to trigger render
void hit_frame_marker() {
    FILE *f = fopen("pieces/display/frame_changed.txt", "a");
    if (f) { fprintf(f, "X\n"); fclose(f); }
}

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    resolve_paths();
    const char *id = argv[1];
    const char *dir = argv[2];

    int x = get_state_int(id, "pos_x");
    int y = get_state_int(id, "pos_y");
    int nx = x, ny = y;

    // Calculate new position
    if (strcmp(dir, "up") == 0) ny--;
    else if (strcmp(dir, "down") == 0) ny++;
    else if (strcmp(dir, "left") == 0) nx--;
    else if (strcmp(dir, "right") == 0) nx++;

    // Bounds check
    if (nx < 0 || nx >= 20 || ny < 0 || ny >= 10) return 0;

    // Collision check (read map tile at new position)
    char tile = get_tile_at(nx, ny);
    if (tile == '#' || tile == 'R') return 0;  // Blocked

    // Treasure tile interaction
    if (tile == 'T') {
        int hap = get_state_int(id, "happiness");
        if (hap != -1) set_state_int(id, "happiness", (hap < 90) ? hap + 10 : 100);
    }

    // Update position
    set_state_int(id, "pos_x", nx);
    set_state_int(id, "pos_y", ny);

    // Energy cost
    int enr = get_state_int(id, "energy");
    if (enr != -1) set_state_int(id, "energy", (enr > 0) ? enr - 1 : 0);

    // Trigger render
    hit_frame_marker();
    return 0;
}
```

**What this Op demonstrates:**
1. **Path resolution:** Always reads from `location_kvp` first
2. **State sovereignty:** Reads/writes plaintext `state.txt` files
3. **Single responsibility:** Does ONE thing (move entity) and exits
4. **Collision detection:** Checks map tiles before moving
5. **Tile interaction:** Special behavior on treasure tiles
6. **Render trigger:** Hits `frame_changed.txt` to trigger pipeline
7. **No `system()` calls:** Pure file I/O, no subprocess spawning

---

---

## 📜 External Ops KVP (Advanced)

For advanced setups, you can maintain an external KVP file:

**File:** `projects/<project_id>/ops/ops_registry.kvp`

**Format:**
```
op_create_profile=projects/user/ops/create_profile.c|username:string
op_auth_user=projects/user/ops/auth_user.c|username:string
op_get_session=projects/user/ops/get_session.c|username:string
```

**Use Cases:**
- Dynamic op discovery
- Ops that span multiple directories
- External/trunk ops that aren't in `projects/`

---

## 🎯 Why Centralize Ops?

| Benefit | Explanation |
|---------|-------------|
| ✓ **Fondu Bundling** | Fondu can install all ops as a single bundle |
| ✓ **Discovery** | Other projects discover ops via `--list-ops` |
| ✓ **Separation** | Clean separation of "muscle" (ops) from "brain" (manager) |
| ✓ **Testing** | Easy to test ops standalone (no manager needed) |
| ✓ **Reusability** | Reusable across projects (PAL scripts call by `project::op_name`) |
| ✓ **Single Truth** | Single source of truth for what ops exist |

---

## 🛠️ The Developer Workflow
1.  **Pick a Template:** Copy `#.docs/future-facing/cpu_safe_module_template.c`.
2.  **Define your Logic:** What happens when the user presses 'W'? (Call `move_entity.+x north`).
3.  **Update the Mirror:** Write the new coordinates to `state.txt`.
4.  **Pulse the Frame:** Tell the Renderer that something changed! 💓

> 💡 **Pro Tip:** Keep your Muscles "dumb" and your Brains "thin". If a Muscle can do it, don't put it in the Brain! ⚡

---

## ⚠️ Common Pitfalls

### Pitfall 1: Orphaned Processes on Ctrl+C
**Symptom:** After quitting, the game keeps running in the background (music plays, files change).
**Cause:** Missing signal handler or using `system()` instead of `fork()/exec()`.
**Fix:** Always register `handle_sigint` and use `setpgid(0, 0)`:
```c
signal(SIGINT, handle_sigint);
signal(SIGTERM, handle_sigint);
setpgid(0, 0);
```

### Pitfall 2: Tight Loop Without Sleep
**Symptom:** TPMOS process uses 100% CPU even when idle.
**Cause:** Main loop has no `usleep()` or uses `usleep(0)`.
**Fix:** Always sleep at the end of each iteration:
```c
// When active: 60 FPS
usleep(16667);
// When inactive: 10 FPS
usleep(100000);
```

### Pitfall 3: Scattering Ops in Wrong Directories
**Symptom:** Fondu can't find your ops, other projects can't call them.
**Cause:** Ops placed in `traits/`, `plugins/`, or `manager/` instead of centralized `ops/`.
**Fix:** All ops go in `projects/<id>/ops/` with `ops_manifest.txt`.

### Pitfall 4: Forgetting `ops_manifest.txt`
**Symptom:** `fondu --install` skips your project.
**Cause:** No manifest file or wrong format.
**Fix:** Create `ops/ops_manifest.txt` with `op_name|description|args_format` format.

### Pitfall 5: Thick Brain Anti-Pattern
**Symptom:** Manager is 5000+ lines, crashes take down everything, hard to debug.
**Cause:** Putting movement, hunger, rendering, AI all in one manager loop.
**Fix:** Move logic into separate ops. Manager should only route input and call ops.

---

---

## 🏛️ Scholar's Corner: The "Thin Brain" Revolution
In the early days of TPMOS, the `fuzz_legacy_manager.c` was over 5,000 lines of code. It handled movement, hunger, the market, and rendering all in one massive loop. It was a "Thick Brain." When one part of the code crashed, the entire pet simulation died! The "Thin Brain" revolution happened when we moved the movement logic into `move_entity.+x` and the hunger logic into PAL scripts. Suddenly, the manager was under 500 lines. The system became stable, and developers could finally sleep at night. 💤🧘‍♂️

### Modern Example: The User Project (April 2026)
The `projects/user/` project is the canonical example of centralized ops:

```
projects/user/
├── project.pdl (exposes_ops = true)
├── ops/
│   ├── +x/create_profile.+x
│   ├── +x/auth_user.+x
│   ├── +x/get_session.+x
│   └── ops_manifest.txt
└── manager/+x/user_manager.+x
```

When installed via Fondu (`./fondu --install user`), all three ops become available to every other project via PAL scripts. This is the "Ops as a Service" pattern that powers TPMOS. ⚡

---

## 📝 Study Questions
1.  Explain the difference between a "Module" and an "Op."
2.  Why is the `fork()` / `exec()` pattern preferred over the `system()` command?
3.  What are the three core rules of the "CPU-Safe Mandate"?
4.  **True or False:** A Module should sleep longer when its app is not the "active layout."
5.  **Short Answer:** What is the correct directory structure for organizing ops in a project?
6.  **Scenario:** You want to create a new op called `teleport_entity`. Where do you put it, and what file do you update to make it discoverable by Fondu?
7.  **Critical Thinking:** Why is centralizing ops better than scattering them in `traits/`, `plugins/`, etc.?

---
[Return to Index](INDEX.md)
