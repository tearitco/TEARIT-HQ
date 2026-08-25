# 💓 Chapter 3: The 12-Step Pulse
How does a single keypress turn into a pixel on your screen? It's a 12-step journey through the system's "Circulatory System". 🩸

---

## 🏃‍♂️ The 12-Step Sprint

### Step 1: ⌨️ INPUT
`keyboard_input.+x` (the Muscle) catches your key and scribbles it into `pieces/keyboard/history.txt`.

```c
// keyboard_input_linux.c - Raw terminal input
struct termios raw = termios;
raw.c_lflag &= ~(ICANON | ECHO);  // Disable line buffering
tcsetattr(STDIN_FILENO, TCSANOW, &raw);

int key = getchar();  // Read single keystroke
fprintf(history_file, "%d\n", key);  // Write to history buffer
```

### Step 2-3: 🗺️ ROUTING & 📢 RELAY
`chtpm_parser.c` (the OS) reads the key. It uses **W-first (World-First)** resolution, checking `world_xx/map_xx/` paths before legacy fallbacks.

```c
// chtpm_parser.c - W-first path resolution
// Check project-specific world first
snprintf(path, sizeof(path), "%s/projects/%s/pieces/world/",
         project_root, current_project);
if (access(path, F_OK) == 0) {
    // Use project-specific world
} else {
    // Fallback to global pieces/world/
}
```

If the user is "interacting" with an app, the Parser throws the key into the app's own history buffer (e.g., `player_app/history.txt`).

### 🌓 The Dual Execution Model
Once the key is relayed, the system enters one of two "modes":

*   **⚡ Mode 1: Realtime Input (Direct C)**
    *   **Latency:** ~16ms (Instant).
    *   **Action:** Movement (WASD), Menu Navigation.
    *   **Path:** Module calls Muscle Op directly. No scripting involved.
*   **📜 Mode 2: Event Scripts (PAL Scripting)**
    *   **Latency:** ~50-100ms.
    *   **Action:** NPC Dialogue, Quests, AI triggers.
    *   **Path:** Module calls `prisc+x` -> executes `event.asm` -> calls Muscle Ops.

### Step 4: 🧠 TICK
The App Module (the Brain) wakes up! It sees the new key in its history and decides which Mode to use.

### Step 5-6: 💪 TRAIT/OP & 🧱 SOVEREIGNTY
The Brain says, "The user pressed 'UP'. Call the `move_entity.+x` Muscle!" The Muscle checks boundaries and updates the Piece's `state.txt`.

```c
// Module calls Op via fork()/exec()
char* trait = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
pid_t p = fork();
if (p == 0) {
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execl(trait, trait, active_target_id, dir_str, NULL);
    exit(1);
} else {
    waitpid(p, NULL, 0);  // Wait for Op to complete
}
free(trait);
```

**Why `fork()/exec()` instead of `system()`?** Because `system()` spawns a subshell that can become orphaned on Ctrl+C. With `fork()/exec()/waitpid()`, we have full control over the child lifecycle - no zombie processes!

### Step 7: 🪞 MIRROR SYNC
The Muscle flushes the new state to disk. Reality is now officially updated.

### Step 8: 🎬 STAGE
`render_map.+x` (the Stage Producer) sees the change. It reads ALL the Pieces on the map and draws a "View" (`view.txt`).

```c
// render_map.c - Read all pieces, produce view.txt
DIR *dir = opendir(pieces_dir);
struct dirent *entry;
while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') continue;
    // Read each piece's state.txt
    // Check if piece is on current map
    // Write to view.txt with position and character
}
closedir(dir);
```

### Step 9-12: 🔄 SYNC → 🎨 COMPOSITION → 🖼️ RENDER → 📺 DISPLAY
The Stage Producer updates the app's general state. The Parser takes `view.txt` and swaps out all variables (like `${player_health}`). The final frame is written to `current_frame.txt`, and the Renderer (ASCII or GL) blasts it onto your monitor!

---

## 📊 Pipeline Architecture Diagram

```
┌─────────────┐
│  KEYBOARD   │ Step 1: Write key to history.txt
│  (Muscle)   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   PARSER    │ Steps 2-3: Route key to app history
│   (OS)      │ W-first path resolution
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   MODULE    │ Step 4: Read history, decide action
│   (Brain)   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│     OP      │ Steps 5-6: Execute action, update state
│  (Muscle)   │ fork()/exec()/waitpid()
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  STATE.TXT  │ Step 7: Mirror sync (reality updated)
│  (Disk)     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  RENDER_MAP │ Step 8: Read all pieces, produce view
│  (Stage)    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  PARSER     │ Steps 9-11: Compose frame with variables
│  (Theater)  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  RENDERER   │ Step 12: Display on screen
│  (ASCII/GL) │
└─────────────┘
```

---

## ⏱️ The Pulse Frequency
This entire 12-step loop happens in milliseconds. It is the "Heartbeat" of TPMOS. 💓

> ⚠️ **Warning:** If any step takes too long (e.g., a slow script), the pulse "stutters". This is why we keep Muscles small and fast! ⚡

### 🪟 Windows Note: `fork()/exec()` → `CreateProcess()`
On Windows, the POSIX `fork()/exec()` pattern doesn't exist. Instead, we use `CreateProcess()`:

```c
#ifdef _WIN32
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    // Hide window, redirect output
    CreateProcess(NULL, cmd_line, NULL, NULL, FALSE,
                  CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // fork()/exec()/waitpid() on Linux/macOS
    pid_t p = fork();
    if (p == 0) { execl(...); exit(1); }
    else waitpid(p, NULL, 0);
#endif
```

The dual-execution model still works identically - just the spawn mechanism differs. The compile scripts (`compile_all.sh` vs `compile_all.ps1`) handle building the correct version for each platform.

---

## ⚠️ Common Pitfalls

### Pitfall 1: Using `system()` Instead of `fork()/exec()`
**Symptom:** After pressing Ctrl+C to quit, orphaned processes keep running and consuming CPU.
**Cause:** `system()` spawns a subshell that becomes orphaned on signal.
**Fix:** Always use `fork()/exec()/waitpid()`:
```c
// ❌ BAD
system("move_entity.+x player up");

// ✅ GOOD
pid_t p = fork();
if (p == 0) { execl(...); exit(1); }
else waitpid(p, NULL, 0);
```

### Pitfall 2: Forgetting `waitpid()` After `fork()`
**Symptom:** Zombie processes accumulate (ironically, actual zombie processes!).
**Cause:** Parent doesn't reap child exit status.
**Fix:** Always call `waitpid(pid, &status, 0)` in the parent branch.

### Pitfall 3: Not Hitting `frame_changed.txt` After State Changes
**Symptom:** Piece state updates but screen doesn't refresh.
**Cause:** Renderer only checks for changes when `frame_changed.txt` is modified.
**Fix:** Call `hit_frame_marker()` after any state change:
```c
FILE *f = fopen("pieces/display/frame_changed.txt", "a");
if (f) { fprintf(f, "X\n"); fclose(f); }
```

### Pitfall 4: Blocking the Pipeline with Slow Ops
**Symptom:** System "stutters" or feels laggy.
**Cause:** An op or script takes too long, delaying the next pulse.
**Fix:** Keep ops under 16ms. If heavy work is needed, do it in a background process and let the pipeline continue.

---

## 🏛️ Scholar's Corner: The "Pulse That Never Ended"
During an early experiment with recursive PAL scripts (Chapter 7), a developer accidentally created a loop where a bot would "move north" every time the frame changed. Because the pulse was so fast, the bot moved through 10,000 maps in under a minute! The developer tried to kill the process, but every time a new frame rendered, a new bot was spawned. This became known as **"The Pulse That Never Ended."** It taught us the critical importance of the **Kill All** script and the value of throttling (Chapter 4). 💓♾️

---

## 📝 Study Questions
1.  Describe the journey of a keypress from Step 1 to Step 12.
2.  What is the role of the "Stage Producer" in the 12-step pipeline?
3.  Why is "Mirror Sync" (Step 7) so critical before the "Stage" (Step 8) begins?
4.  If the renderer "stutters," which step in the pipeline is most likely the culprit?

---
[Return to Index](INDEX.md)
