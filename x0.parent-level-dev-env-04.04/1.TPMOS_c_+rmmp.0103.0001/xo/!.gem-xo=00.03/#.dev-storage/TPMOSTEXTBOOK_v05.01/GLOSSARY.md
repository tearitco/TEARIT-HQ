# 📖 Glossary of Terms
Welcome to the official TPMOS Lexicon. This glossary defines the core concepts and the **PJARGO** (Internal Dialect) used within the ecosystem. 🗣️🏛️

---

### 🏛️ Core Concepts
*   **TPM (True Piece Method):** The governing philosophy where reality is decomposed into atomic "Pieces" that own their own state.
*   **PMO (Piece-Module-OS):** The 3-tier architectural hierarchy for system development.
*   **Piece:** A directory containing a `.pdl` (DNA) and `state.txt` (Mirror). The "Soul" of an object.
*   **Module:** A background C-process ("Brain") that manages Pieces and handles input.
*   **Mirror:** The `state.txt` file representing a Piece's current physical/logic state.
*   **DNA:** The `.pdl` file defining a Piece's potential, methods, and traits.
*   **Pulse:** The recurring heartbeat of the 12-step pipeline that updates the frame.
*   **Sovereignty:** The principle that a Piece exclusively owns its files; no other process should overwrite its state directly.

### 🗣️ PJARGO (Internal Dialect)
*   **W-first:** A world-first path resolution strategy (`world_01/map_01/...`).
*   **RT Parity:** Roundtrip parity—the guarantee that Saving and Loading results in bit-for-bit identical state.
*   **Ghost Hardcode:** A forbidden practice where project-specific assumptions are hidden in generic system code. 👻
*   **Muscle Layer:** The "Ops" binaries (`.+x`) that perform the actual work for a Brain Module.
*   **Sea Shell:** The core system (Parser, Nav) is strong; growth occurs at the flexible edges (Apps/Projects).
*   **Mirror Sync:** The process of flushing in-memory changes to the `state.txt` file.
*   **W-first (World-First):** The canonical path resolution strategy that prioritizes `world_<id>/map_<id>/` before checking legacy fallback paths. 🗺️
*   **Mode 1 (Realtime):** The direct C-based execution path for high-speed input like movement (latency ~16ms).
*   **Mode 2 (Event):** The PAL-based execution path for complex scripts, AI, and dialogue (latency ~50-100ms).
*   **Thin Brain:** A module that delegates logic to ops rather than implementing it directly (opposite of "Thick Brain").
*   **Thick Brain:** An anti-pattern where a module implements all logic internally, leading to bloat and instability.

### ⚙️ Technical Terms
*   **PAL (Prisc Assembly Language):** The RISC-like scripting language used for AI and events.
*   **prisc+x:** The PAL interpreter binary.
*   **KVP (Key-Value Pair):** The primary data format used in `.pdl`, `state.txt`, and `location_kvp`.
*   **Exo-Bot:** An external agent that operates on the OS from the outside, sovereign to its own directory.
*   **ASIL:** Agent-Supervised Imitation Learning, a process where bots learn from human demonstrations.
*   **PAL Editor:** A built-in GUI for writing and binding PAL scripts to Pieces.
*   **Artifact:** A voxel bitmask (8x8x8) used to reconstruct 3D shapes in GL-OS.
*   **Host (GL):** The 3D rendering frontend that mirrors the system state.
*   **Manager (GL):** The sovereign logic backend for 3D project sessions.
*   **CHTPM:** The "CH" range of TPM, typically referring to the character-based (ASCII) OS shell.
*   **GL-OS:** The OpenGL visual theater that mirrors the CHTPM shell.
*   **.+x:** TPMOS binary extension on Linux/macOS; `.exe` on Windows.
*   **CreateProcess():** Windows equivalent of `fork()/exec()`.
*   **Fondu:** Package manager for TPMOS ops and apps.
*   **P2P-NET:** Decentralized networking layer.
*   **Div-Points:** Dividend Points, earned through participation in the network.
*   **Escrow:** File-based contract enforcement mechanism.
*   **Block:** Unit of blockchain ledger.
*   **Wallet Piece:** P2P identity container.
*   **Frame Bridge:** Sync mechanism between ASCII and GL rendering.
*   **op-ed:** TPMOS piece editor application.
*   **fuzz-op:** TPMOS pet simulation project.
*   **stat()-first:** CPU-safe polling pattern that only reads files when size changes.
*   **fork()/exec()/waitpid():** POSIX process spawning pattern for CPU-safe op execution.
*   **setpgid():** Sets process group for clean Ctrl+C cleanup.
*   **hit_frame_marker():** Function that triggers render pipeline by writing to `frame_changed.txt`.
*   **resolve_paths():** Function that reads `location_kvp` to determine project root.
*   **build_path_malloc():** Helper that constructs platform-aware paths.

---
[Return to Index](INDEX.md)
