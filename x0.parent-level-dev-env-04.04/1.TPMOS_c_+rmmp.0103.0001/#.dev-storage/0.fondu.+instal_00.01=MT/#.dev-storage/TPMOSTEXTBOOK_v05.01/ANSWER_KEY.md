# 🔑 Answer Key: Study Questions
Mastered the Mono-OS? Check your answers here! 🎓🚀

---

### 🧬 Chapter 1: The Soul of a Piece
1.  **Layers of PMO:** PIECE (Atom/State), MODULE (Brain/Logic), OS (Theater/View).
2.  **DNA vs. Mirror:** DNA (.pdl) defines *potential* and methods; Mirror (state.txt) defines *current* runtime state.
3.  **No In-Memory HP:** Memory is volatile ("a lie"). Files ensure reality persists even if a process crashes.
4.  **True or False:** True. TPMOS is scale-free; both are sovereign Pieces.

### 📁 Chapter 2: The Filesystem is the Database
1.  **location_kvp:** Acts as the system's "Ground" or "Compass," defining absolute paths for the project root and pieces.
2.  **File-Based IPC:** Programs communicate by writing notes (files) like `history.txt` or `pulse.txt` instead of sharing RAM.
3.  **Stat-First Pattern:** It checks the file's modification time before opening it, keeping CPU at 0% when idle.
4.  **Moving a Project:** Update the `project_root` line in `pieces/locations/location_kvp`.

### 💓 Chapter 3: The 12-Step Pulse
1.  **Keystroke Journey:** Input -> History -> Parser -> App Buffer -> Module Tick -> Op Exec -> Piece Sovereignty -> Mirror Sync -> Stage Producer -> Parser Composition -> Final Render -> Display.
2.  **Stage Producer:** Reads all Mirrors on a map and draws the composite "View" (`view.txt`).
3.  **Mirror Sync Importance:** Ensures reality is written to disk so the Stage Producer sees the *latest* state.
4.  **Stutter Culprit:** Step 5 (Muscle Op execution) or Step 8 (Stage Production) taking too long.

### 💪 Chapter 4: Muscles & Brains
1.  **Module vs. Op:** A Module is a long-running "Brain"; an Op is a short-lived, single-task "Muscle."
2.  **Fork/Exec vs. System:** Fork/Exec gives full control over the child's lifecycle; `system()` is hard to track and kill.
3.  **CPU-Safe Mandate:** Focus-aware throttling, Stat-first polling, and Signal handling (SIGINT).
4.  **True or False:** True. It should sleep ~100ms when inactive.

### 🏭 Chapter 5: The App Factory
1.  **App Requirement:** A directory containing an app-id DNA file (`.pdl`) and a `layouts/` folder.
2.  **User Profiles:** Managed by the `user` app.
3.  **Adding to Menu:** Add a `<button href="...">` pointing to the app's `.pdl` in `os.chtpm`.
4.  **True or False:** True. The `.pdl` is the descriptor the OS needs to launch the app.

### 🎨 Chapter 6: Beyond ASCII (GL-OS)
1.  **OS or Visualizer:** It is a high-fidelity visualizer shell that mirrors ASCII reality.
2.  **Mapping Models:** It parses specific characters (like `[P]`) and maps them to 3D model paths in its configuration.
3.  **Shadow vs. Light:** Pixels are just a visual representation (shadow) of the true data stored in files (light).
4.  **Update Color:** Yes. If the character or its associated state changes, GL-OS updates the render instantly.

### 🤖 Chapter 7: The Guardians (Testing)
1.  **Bot is a User:** A bot uses the same input/output channels as a human, making tests "reality-accurate."
2.  **FSM States:** IDLE, NAVIGATING, INTERACTING, ASSERTING.
3.  **Input Pipeline:** It appends timestamped `KEY_PRESSED` lines to `keyboard/history.txt`.
4.  **P2P Test Script:** A PAL script that spawns two bots to trade an NFT and asserts the balance shift.

### 🛰️ Chapter 8: Future Horizons
1.  **AI-Labs in LSR:** It generates dynamic tech recipes, item names, and `.pdl` stats based on R&D "actions."
2.  **Unified Contract Model:** Allows the same legal/logic code to run on a blockchain (P2P) or in a world sim (LSR Gov).
3.  **Mining Coins:** By providing disk storage or CPU compute cycles to the infrastructure network.
4.  **Compute Piece:** You publish a "Worker Piece" with your CPU specs and an "Open" availability state.

### ⚡ Chapter 9: The Infinite Loop
1.  **Recursive Chip Design:** Building logic gates as Pieces so the OS can simulate and "test" its own hardware.
2.  **Scale-Free Simulation:** Since a Piece can be anything, a NAND gate follows the same state-rules as a Hero.
3.  **In-Game Programming:** Players use an in-game `op-ed` to write PAL scripts for their "Auto-Battler" bots.
4.  **True or False:** False. Both follow the same PIECE -> MODULE -> OS hierarchy.

### 🧪 Chapter 12: The Simulation Theater
1.  **Cell vs. Organ:** A cell is a single Piece; an organ is a Container Piece holding millions of cell Pieces.
2.  **Predictor Pulse:** It runs the 12-step loop much faster than 60 FPS to calculate far-future states.
3.  **Molecular Lab:** Bots "play" chemistry by testing millions of molecular Piece configurations for "Success" states.
4.  **True or False:** True. Marketing is the attempt to program a bot's internal state.

### 🤖 Chapter 17: Exo-Sovereignty
1.  **Exo-Sovereignty:** Bots that operate externally to the project pieces tree, possessing their own sovereign directory for memories and logic.
2.  **ASIL:** Agent-Supervised Imitation Learning allows bots to learn by recording and labeling human-guided sessions.
3.  **bot5 pattern:** Pure orchestration with no headers and no linking, ensuring high-speed process tracking and concurrent execution.


---
[Return to Index](INDEX.md)
