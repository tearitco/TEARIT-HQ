# 📁 Chapter 2: The Filesystem is the Database
In a traditional OS, the filesystem is just for storage. In TPMOS, the filesystem **is** the system. 🏛️

---

## 🗺️ The Project Map
Everything has a place, and there is a place for everything. The root of the world is defined in `pieces/locations/location_kvp`.

### 🔑 `location_kvp`: The Compass
This file tells the system where to find the "Ground". If you move the project, you only change this file.
*   `project_root=/home/user/TPMOS/`
*   `pieces_dir=pieces/`
*   `apps_dir=pieces/apps/`

---

## 📂 The Directory Hierarchy
Let's look at how a project is structured. It's like a nested doll. 🪆

```text
/projects/my_game/
├── project.pdl         # The "World DNA"
├── manager/            # The "World Brain" (.+x binaries)
├── maps/               # The "Geography"
└── pieces/             # The "Inhabitants"
    ├── hero/
    │   ├── piece.pdl   # Hero DNA
    │   └── state.txt   # Hero Stats
    └── slime_01/
        ├── piece.pdl   # Slime DNA
        └── state.txt   # Slime Stats
```

---

## 📬 File-Based IPC (The Mailbox) ✉️
How do two programs talk to each other if they don't share memory? They write notes!

1.  **The History Buffer:** `keyboard/history.txt`. The input driver writes `A`, the Parser reads `A`.
2.  **The Pulse:** `frame_changed.txt`. When a renderer finishes a frame, it "pulses" this file. Other programs see the pulse and wake up.
3.  **The Response:** `last_response.txt`. When an Op runs, it writes its output here so the UI can show it to the user.

---

## 🛡️ CPU Safety & The `stat()` Guard
Reading files constantly can be heavy. To stay fast, we use the **Stat-First Pattern**. 🛡️

*   **Logic:** Don't open the file unless `stat()` tells you the "Modified Time" (mtime) has changed.
*   **Result:** 0% CPU usage when nothing is happening. TPMOS is a "Green" OS! 🌿

### Code Example: The `stat()` Guard Pattern
This is the most important pattern in TPMOS. Every module uses it:

```c
#include <sys/stat.h>

long last_pos = 0;
struct stat st;

// Only read the file if its size has changed
if (stat(history_path, &st) == 0) {
    if (st.st_size > last_pos) {
        // New input available - process it
        FILE *hf = fopen(history_path, "r");
        if (hf) {
            fseek(hf, last_pos, SEEK_SET);
            int key;
            while (fscanf(hf, "%d", &key) == 1) {
                process_key(key);
            }
            last_pos = ftell(hf);
            fclose(hf);
        }
    } else if (st.st_size < last_pos) {
        // File was truncated - reset position
        last_pos = 0;
    }
}
```

This pattern ensures **0% CPU usage when idle**. The module sleeps for 16ms between checks, never spinning wastefully.

---

## 🔧 Code Example: Parsing `location_kvp`
Every TPMOS binary starts by resolving its paths from `location_kvp`. Here's how it works:

```c
char project_root[4096] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;

    char line[1024];
    while (fgets(line, sizeof(line), kvp)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = trim_str(line);
            char *val = trim_str(eq + 1);
            if (strcmp(key, "project_root") == 0) {
                snprintf(project_root, sizeof(project_root), "%s", val);
            }
        }
    }
    fclose(kvp);
}
```

The `trim_str()` helper removes whitespace from keys and values. This ensures paths are clean even if the file has trailing newlines.

### Code Example: Reading Piece State
Once paths are resolved, reading a Piece's state is straightforward:

```c
int get_state_int(const char* piece_id, const char* key) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt",
             project_root, current_project, piece_id);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[256];
    int val = -1;
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

// Usage:
int player_x = get_state_int("player", "pos_x");
int player_y = get_state_int("player", "pos_y");
```

### Example: `state.txt` Format
A Piece's state file is a simple key-value list:

```
name=Zombie
type=zombie
pos_x=9
pos_y=2
pos_z=0
on_map=1
behavior=aggressive
speed=1
map_id=map_01_z0.txt
```

No JSON, no SQLite - just plaintext. This is **Data Sovereignty** in action: any tool can read and modify state, from `cat` to a C binary to an AI agent.

---

## 🪟 Windows Compatibility Notes
TPMOS is designed to be cross-platform. Here's what changes on Windows:

### Path Differences
- **Linux/macOS:** Forward slashes `/` (e.g., `pieces/world/map_01/`)
- **Windows:** Backslashes `\` (e.g., `pieces\world\map_01\`)
- **Solution:** Use `build_path_malloc()` helper which handles both:
  ```c
  char* build_path_malloc(const char* rel) {
      size_t sz = strlen(project_root) + strlen(rel) + 2;
      char* p = (char*)malloc(sz);
      if (p) snprintf(p, sz, "%s/%s", project_root, rel);
      return p;
  }
  ```

### File API Differences
| Linux/macOS | Windows Equivalent | Notes |
|-------------|-------------------|-------|
| `access(path, F_OK)` | `_access(path, 0)` | Use `#ifdef _WIN32` |
| `stat(path, &st)` | `stat(path, &st)` | Works on both (MSVCRT) |
| `getcwd(buf, sz)` | `_getcwd(buf, sz)` | Same header `<direct.h>` |
| `usleep(us)` | `Sleep(us/1000)` | `<windows.h>` vs `<unistd.h>` |

### Cross-Platform Pattern
```c
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define usleep(us) Sleep((us)/1000)
    #define access _access
    #define F_OK 0
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif
```

### Binary Convention
- **Linux/macOS:** `.+x` extension (e.g., `move_entity.+x`)
- **Windows:** `.exe` extension (e.g., `move_entity.exe`)
- The compile scripts (`compile_all.sh` vs `compile_all.ps1`) handle this automatically.

---

## ⚠️ Common Pitfalls

### Pitfall 1: Hardcoded Paths
**Symptom:** Binary works on your machine but fails after moving the project folder.
```c
// ❌ BAD: Hardcoded absolute path
FILE *f = fopen("/home/user/TPMOS/projects/fuzz-op/pieces/zombie/state.txt", "r");

// ✅ GOOD: Resolve from location_kvp
resolve_paths();
char path[4096];
snprintf(path, sizeof(path), "%s/projects/%s/pieces/zombie/state.txt",
         project_root, current_project);
```
**Fix:** Always call `resolve_paths()` at startup and use `project_root` variable.

### Pitfall 2: Forgetting to Update `location_kvp` After Moving
**Symptom:** Entire project "disappears" - no pieces, no maps, nothing loads.
**Cause:** `location_kvp` still points to the old directory.
**Fix:** Update the `project_root=` line in `pieces/locations/location_kvp` to the new path. The entire world will "wake up" instantly.

### Pitfall 3: `stat()` Race Conditions
**Symptom:** Input is occasionally missed or processed twice.
**Cause:** File size check and read aren't atomic - another process can write between them.
**Fix:** Always use `fseek(hf, last_pos, SEEK_SET)` before reading, and update `last_pos = ftell(hf)` after. If `st.st_size < last_pos`, reset to 0 (file was truncated).

### Pitfall 4: Reading State Without Checking File Exists
**Symptom:** Crash or garbage values when reading a Piece that doesn't exist.
**Fix:** Always check `fopen()` return value:
```c
FILE *f = fopen(path, "r");
if (!f) return -1;  // Piece doesn't exist yet
```

---

## 🏛️ Scholar's Corner: The "Lost Map of Lunar Streetrace"
Legend has it that a project was once "lost" during a massive server migration. For three days, the `lsr` simulation was invisible. Then, a developer realized that only the `location_kvp` file had been corrupted. The entire world—its companies, its bots, its history—was still there in the `projects/` folder. By writing a single line back into `location_kvp`, the entire universe "woke up" as if no time had passed. This proved that in TPMOS, **"Geography is Destiny."** If the files exist, the world is never truly lost. 🗺️✨

---

## 📝 Study Questions
1.  What is the role of the `location_kvp` file?
2.  Explain File-Based IPC in your own words.
3.  How does the "Stat-First Pattern" improve CPU efficiency?
4.  **Imagine:** You want to move your TPMOS project to a different drive. Which file do you need to update to ensure everything still works?

---
[Return to Index](INDEX.md)
