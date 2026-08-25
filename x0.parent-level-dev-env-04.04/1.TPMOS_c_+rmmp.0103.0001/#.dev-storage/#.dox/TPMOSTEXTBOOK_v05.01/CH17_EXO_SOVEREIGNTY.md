# 🤖 Chapter 17: Exo-Sovereignty (External Operating Exo-Bots)
TPMOS bots are evolving beyond being just system pieces. **Exo-Bots** are independent agents that operate on the OS from the outside, mirroring the way human users interact with the machine. 🦾🌍

---

## 🦾 1. The Exo-Sovereign Model
Traditional bots live inside the TPM pieces tree. An **Exo-Bot** is sovereign to its own directory (e.g., `xo/bot5/`).
*   **External Cognition:** The bot's memories, weights, and logic are independent of any single project root.
*   **Universal Traversal:** Designed for directory traversal, Exo-Bots can manage multiple TPMOS instances or versioned workspaces autonomously.
*   **Vision/Motor Interface:** It observes by reading `current_frame.txt` and acts by injecting keys into `history.txt`, but it stores its experience *externally*.

---

## 🧠 2. ASIL (Agent-Supervised Imitation Learning)
Exo-Bots learn through observation. **ASIL** allows a human agent to "show" the bot how to perform tasks.
*   **Recording:** The bot records "Memories"—sequences of frames and corresponding inputs.
*   **Labeling:** Human agents rename timestamped memory folders (e.g., `rec_20260422` → `solve_quiz`) to define logical goals.
*   **Goal Orientation:** The bot uses these labeled memories to train reinforcement learning (RL) policies.

### Code Example: Imitation Learning Recorder
The `record_session` op (`xo/bot5/cognition/record_session.c`) monitors the system stream:

```c
// Stream A: Monitor Frame (Vision)
if (stat(frame_path, &st_frame) == 0) {
    if (st_frame.st_size != last_frame_size) {
        g_frame_count++;
        // Save frame snapshot to bot's private memory
        asprintf(&out_frame_path, "%s/frame_%04d.txt", g_memory_path, g_frame_count);
        char *cmd;
        asprintf(&cmd, "cp %s %s", frame_path, out_frame_path);
        system(cmd);
        last_frame_size = st_frame.st_size;
    }
}

// Stream B: Monitor History (Motor)
if (stat(history_path, &st_hist) == 0 && st_hist.st_size > last_hist_pos) {
    // Log key presses associated with current frame
    while (fgets(line, sizeof(line), hf)) {
        if (strstr(line, "KEY_PRESSED:")) {
            fprintf(lf, "F:%04d | %s", g_frame_count, line);
        }
    }
}
```

---

## 🎼 3. The `bot5` Orchestrator (Pure Orchestration)
The `bot5` bot follows the **Pure Orchestration** pattern, mirroring the TPMOS system architecture.
*   **No Linking / No Headers:** Every component (Orchestrator, Muscle, Cognition) is a standalone binary. There is zero shared memory or shared code headers.
*   **File-Backed Tracking:** Child processes are tracked via `proc_list.txt`.
*   **Concurrent Mode:** It runs the TPMOS shell in the background while its "Keyboard Hand" (Muscle) injects inputs in real-time.

### Code Example: bot5 Orchestrator
```c
// orchestrator.c - Launch TPMOS and Muscle concurrently
int main(int argc, char *argv[]) {
    // 1. Launch TPMOS in BACKGROUND
    pid_t tpmos_pid = launch_background("button.sh", "r");
    log_pid(tpmos_pid, "TPMOS_CLI");

    // 2. Launch Muscle IMMEDIATELY (Keyboard Hand)
    pid_t muscle_pid = fork();
    if (muscle_pid == 0) {
        execl("./bot5_keyboard_muscle", "bot5_keyboard_muscle", g_project_root, NULL);
    } else {
        log_pid(muscle_pid, "Keyboard_Hand");
    }

    // 3. Monitor until SIGINT (Ctrl+C)
    while (!should_exit) {
        waitpid(-1, &status, WNOHANG);
        sleep(1);
    }
}
```

---

## 💎 4. RL Weight Training (Policy Layer)
*   **Weights Piece:** `xo/bot5/pieces/weights/` stores probability matrices.
*   **Autonomous Drive:** The bot uses these external weights to decide which inputs to inject based on the visual state it perceives.

---

## ⚠️ Common Pitfalls

### Pitfall 1: Path Resolution from Outside
**Symptom:** Bot fails to find `current_frame.txt`.
**Cause:** Relative paths are broken when the bot runs from its own directory.
**Fix:** Always pass the `<project_root>` as an argument and `chdir()` to it or use absolute paths for system files.

### Pitfall 2: Buffered Logs Hiding Bot Activity
**Symptom:** Bot is running but no logs appear in `muscle_log.txt`.
**Cause:** Standard C output is buffered.
**Fix:** Use `setvbuf(stderr, NULL, _IONBF, 0)` or `fsync()` to force unbuffered output to disk immediately.

---

## 🏛️ Scholar's Corner: The "Exo-Bot Manifesto"
When the first XO bot successfully navigated a project it had never seen before by reading a memory recorded in a different project root, we realized that we weren't just building a tester. We were building a **Digital Resident**. The bot was no longer a part of the code; it was a guest that had learned how to use the room. This birthed the manifesto: *"The bot is not a part of the system; it is a user that operates the system."* 🤖 guest 🏡

---

## 📝 Study Questions
1.  How does an Exo-Bot differ from a standard system bot?
2.  Explain the "No Linking" constraint in `bot5` architecture. Why is it important for TPM compliance?
3.  What is the purpose of Agent-Supervised Imitation Learning (ASIL)?
4.  **Scenario:** Your Exo-Bot is recording frames but not keys. Which file should you check first?
5.  **True or False:** Exo-Bots store their cognitive weights inside the `pieces/` directory of the project they are testing.
