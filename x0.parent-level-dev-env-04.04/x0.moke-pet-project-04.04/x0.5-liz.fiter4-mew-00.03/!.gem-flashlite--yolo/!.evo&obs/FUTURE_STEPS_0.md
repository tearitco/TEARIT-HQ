# TPMOS Future Development Roadmap (FUTURE_STEPS_0)
**Date:** 2026-04-22
**Status:** STRATEGIC VISION & IMPLEMENTATION STEPS
**Version:** 2.1 (Mirroring Clarification)

---

## 1. P2P Networking: Ring-Broadcast Hybrid Chat
### Current State Assessment
- `chat-op` provides the functional UI testbed (input field + scrollable message log).
- `p2p-net` has basic broadcast and subnet config logic.

### Technical Details of State
- **UI Driver**: `projects/chat-op/layouts/chat-op.chtpm` using `<cli_io>` for message entry.
- **Backend Hub**: `projects/p2p-net/manager/` will act as the routing engine.

### Desired Improvements
- **Hybrid Topology**: Combine broadcast discovery with a deterministic **Ring Path** for message propagation.
- **Single Room POC**: Unified global chatroom where every terminal acts as a relay node.

### Steps & KPIs
1.  **Hybrid Route Development**: Create `hybrid_route.+x` which attempts ring-forwarding to a specific neighbor but falls back to broadcast relay if the ring is broken.
2.  **Neighbor Heartbeat**: Implement a pulse that verifies the next peer in the ring is active.
3.  **Chat-Op Integration**: Bridge the manager to the P2P relay logic.
- **KPI**: A message sent from `chat-op` UI appears on a separate terminal instance via the Ring-P2P path.

---

## 2. XO (Exo-Operating) Bot: Cognitive Evolution
### Current State Assessment
- `bot5` is a pure orchestrator living in `xo/bot5/`.
- Infrastructure for recording frame/input sequences (Memories) is functional and Exo-Sovereign.

### Technical Details of State
- **Brain**: Code in `xo/bot5/cognition/`.
- **Memory**: Sovereign data piece in `xo/bot5/pieces/memories/`.
- **Format**: `F:XXXX | [TIMESTAMP] KEY_PRESSED: <code>`.

### Desired Improvements
- **Supervised Imitation**: Agent-labeled memories (`solve_quiz`, `find_zombie`) used as training sets.
- **RL Weight Training**: implementing probability matrices in `weights/` to map Frame-States to Input-Actions.
- **Cognitive Layers**: 
    - **Attention**: Text-parsing for CYOA/Chat sentiment.
    - **Knowledge Distillation**: Extracting heuristics from human runs into bot PDL.

### Steps & KPIs
1.  **Replay Engine**: Implement `replay_memory.+x` to verify capture fidelity.
2.  **Feature Extractor**: Develop `parse_frame_features.+x` to identify UI context.
3.  **Weight Trainer**: Implement `train_rl.+x` to adjust probability weights based on success pulses.
- **KPI**: Bot completes a 10-step navigation task autonomously with >90% accuracy using RL weights.

---

## 3. GL-OS: High-Fidelity File-State Mirroring
### Current State Assessment
- `gl_desktop.c` provides a windowed OpenGL environment. It currently mirrors ASCII-OS frame files, acting as a "TV" display rather than a separate "projector."

### Technical Infrastructure Clarification
- **Process Isolation**: GL-OS and ASCII TPMOS are **completely independent processes**. They do NOT directly access each other's runtime memory.
- **File-State Mirroring**: GL-OS applications mirror **file states** (e.g., reading `current_frame.txt`, `view.txt`, `state.txt`) written by ASCII applications or other GL-OS modules. This is the standard communication mechanism.
- **Universal Infrastructure**: All applications (ASCII or GL) MUST adhere to the TPMOS structure:
    - Sovereign Pieces (code & data may be separate but managed coherently).
    - Piece PDL descriptors.
    - File-based state (`state.txt`).
- **Mirror-less Applications**: It's understood that some applications may exist solely within the GL-OS environment or the ASCII environment, without a direct mirror in the other.

### Desired Improvements
- **Mirror Terminal Selection**: Allow launching GL-native projects (like `Fuzz-Op-GL`, `Op-Ed-GL`) from the ASCII Project Loader.
- **Virtual Windows**: Implement GL windows that render the content of ASCII project frame files, acting as interactive mirrors.
- **2D/3D Hybridity**: Basic 3D projects (Cube, Map-View) selectable as "Desktop Apps".

### Steps & KPIs
1.  **GL-Route Mapping**: Add GL-specific entry points (e.g., `fuzz-op-gl`) to `project_routes.kvp` or a dedicated GL route registry.
2.  **Virtual Window Implementation**: Create a `GLWindow` type that reads ASCII project frame files (e.g., `current_frame.txt`) and renders them within a GL-OS window.
3.  **Input Bridging**: Ensure input events (keyboard/joystick) can be captured by GL-OS and passed to the mirrored ASCII application's input history file.
- **KPI**: Launching "Fuzz-Op-GL" from the ASCII loader displays a GL window mirroring the ASCII Fuzz-Op's frame.

---

## 4. Op-Ed: Advanced Mapping & 3D ASCII POC
### Current State Assessment
- `op-ed` is functional for 2D mapping but lacks true Z-level rendering logic and dynamic event binding.

### Desired Improvements
- **RPG Maker MV Parity**: Implement full event binding via PDL and true multi-floor Z-level switching.
- **3D ASCII View (Explorative POC)**: A "3D Mode" using skewed ASCII projection (isometric) to visualize height differences in the terminal.
- **GL Tile-Version**: A 3D version of Op-Ed in GL-OS with tiles extruded based on Z-level data.

### Steps & KPIs
1.  **Z-Filtering Logic**: Refactor `render_map.c` to filter pieces and tiles based on `current_z` for display.
2.  **3D ASCII Projection Op**: Create `project_3d_ascii.+x` to generate isometric ASCII views from map data.
3.  **GL Tile Extrusion**: Map `pos_z` to Y-axis height in the GL renderer.
- **KPI**: Toggling "3D Mode" in Op-Ed shows a distinct isometric ASCII view; GL-Op-Ed renders extruded tiles based on Z-level.

---

## 5. Complexity & Implementation Order Report

### Relative Difficulty Assessment
| Project | Difficulty | Primary Challenge |
| :--- | :--- | :--- |
| **XO Bot (Imitation)** | **Easiest** | Orchestrating file reads into memory structures; relies on existing FSM patterns. |
| **Op-Ed (Z-Level & 3D ASCII)** | **Moderate** | Geometric transformations for 3D projection; managing Z-level state. |
| **Hybrid Chat** | **Moderate/Hard** | Network synchronization, message ordering, and robust error handling in a distributed ring. |
| **GL-OS (3D Theater)** | **Hardest** | Graphics rendering pipeline, managing GL contexts, and cross-process IPC for mirroring. |

### Recommended Implementation Order
1.  **XO Bot (Imitation)**: (PHASE 1 COMPLETE) Foundation for autonomous agent behavior.
2.  **Op-Ed (Z-Level & 3D ASCII)**: High visual impact for terminal-based systems, less complex than full GL.
3.  **Hybrid Chat**: Builds on existing manager/op structure for P2P networking.
4.  **GL-OS (Virtual Window & 3D)**: The most complex, requiring robust OpenGL integration and inter-process communication.

---
*"The file is the truth. GL-OS mirrors files, not memories. Build modularly."*
