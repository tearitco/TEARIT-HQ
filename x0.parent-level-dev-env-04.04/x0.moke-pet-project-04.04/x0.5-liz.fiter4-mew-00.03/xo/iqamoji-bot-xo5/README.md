# XO Bot: External Operating Exo-Bot (bot5)
**Status:** Phase 1 (Memory Acquisition) Complete

## 1. Overview
The XO Bot (External Operating Bot) is an **Exo-Bot**. It is an external entity that operates on the TPMOS system from the outside. Its code and its cognitive memory are entirely sovereign to the `xo/` directory tree.

## 2. Exo-Sovereign Structure
- `cognition/`: Brain source code (C binaries).
- `pieces/`: The bot's own sovereign data piece.
    - `memories/`: Private experience storage (Frame + Input sequences).
    - `weights/`: RL policy probability matrices.
- `memory_map.kvp`: Internal registry for memory goals.

## 3. Usage

### Recording a Session
The bot observes the system and records its experience into its own private memories:
```bash
./xo/bot5/cognition/+x/record_session.+x <project_root>
```
*Press Ctrl+C to stop.*

### Renaming a Memory
Assign a logical goal (e.g., `solve_quiz`) to a timestamped folder:
```bash
./xo/bot5/cognition/+x/rename_memory.+x <project_root> <old_name> <new_name>
```

## 4. Technical Vision
The XO Bot is designed for **Directory Traversal**. It is not bound to a single project instance and will eventually have the ability to move between and operate on multiple TPMOS workspaces autonomously, just like a human agent.

---
*"Operating externally, learning internally, sovereign eternally."*
