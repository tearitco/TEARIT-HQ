# PIECEMARK-IT / CHTPM+OS Monolith
**Version:** 6.0 (Standardized Ops Edition)
**Date:** April 2, 2026
**Philosophy:** PIECE -> MODULE -> OS (PMO)

This is the central documentation for the Piecemark-IT ecosystem, a file-backed, auditable, and modular OS environment built on the **True Piece Method (TPM)**.

---

## 🛠️ Prerequisites & Installation

### Windows
1.  Open PowerShell as Administrator.
2.  Run the dependency installer:
    ```powershell
    .\install_deps.ps1
    ```
3.  Restart your terminal.
4.  Compile the project:
    ```powershell
    .\button.ps1 compile
    ```

### macOS / Linux
1.  Run the dependency installer:
    ```bash
    ./install_deps.sh
    ```
2.  Compile the project:
    ```bash
    ./button.sh compile
    ```

---

## 📑 Table of Contents
1. [Core Philosophy (PMO/TPM)](#1-core-philosophy-pmotpm)
2. [PJARGO Dictionary (Internal Dialect)](#2-pjargo-dictionary-internal-dialect)
3. [System Architecture](#3-system-architecture)
4. [App vs. Project (Critical Distinction)](#4-app-vs-project-critical-distinction)
5. [Fondu Lifecycle Manager](#5-fondu-lifecycle-manager)
6. [The Runtime Pipeline (12 Steps)](#6-the-runtime-pipeline-12-steps)
7. [Development Standards](#7-development-standards)
8. [How to Add to the Codebase](#8-how-to-add-to-the-codebase)
9. [Current Status & Roadmap](#9-current-status--roadmap)
10. [Tools & Maintenance](#10-tools--maintenance)

---

## 🧬 1. Core Philosophy (PMO/TPM)
The system follows a strict thought priority: **PIECE -> MODULE -> OS**.

*   **PIECE (The Atomic Unit):** A directory containing text files defining state (`state.txt`) and DNA (`piece.pdl`). A piece owns its state exclusively. "If it's not in a file, it's a lie."
*   **MODULE (The Brain):** Orchestrates pieces, interprets input, and executes **Ops**. It should delegate "Muscle" logic to standalone binaries.
*   **OS / CHTPM (The Theater):** The environment and UI. It substitutes variables (`${var}`), routes input, and composes the final frame. "The Parser is the mirror's eye, not the mirror's mind."

### TPM Principles:
*   **Data Sovereignty:** Each piece owns its files.
*   **Auditability:** All major events log to `master_ledger.txt`.
*   **File-Based IPC:** Communication happens via files, not memory.
*   **Recursive Containment:** Maps contain pieces; pieces can contain pieces (inventory/interiors).

---

## 🗣️ 2. PJARGO Dictionary (Internal Dialect)
*High-leverage vocabulary for developers and agents.*

*   **W-first:** World-first path resolution (`world_<id>/map_<id>/...`).
*   **RT Parity:** Roundtrip parity (Save -> Load yields identical state).
*   **Slice-Safe:** Isolate changes so other subsystems keep running.
*   **Ghost Hardcode:** Hidden project-specific assumptions in generic code (AVOID).
*   **Mirror:** The `state.txt` runtime representation.
*   **DNA:** The `piece.pdl` definition layer.
*   **Sea Shell:** The core (Parser, Nav) is strong; growth (Apps) is at the edges.

---

## 🏗️ 3. System Architecture
The filesystem is the database.

```
/
├── pieces/
│   ├── apps/          # System tools (op-ed, gl-os, user)
│   ├── chtpm/         # Parser, orchestrator, layouts
│   ├── display/       # current_frame.txt, current_layout.txt
│   ├── system/        # pdl_reader, kvp_db, prisc (VM)
│   └── world/         # Global world instances
├── projects/          # User-created content (fuzz-op, lsr, demo_1)
├── #.docs/            # Canonical documentation
├── #.plans/           # Future roadmap
└── #.tools/           # Maintenance scripts
```

### World/Map/Piece Nesting Model:
`projects/<project>/pieces/world_<id>/map_<id>/<piece_id>/`

---

## 🚀 4. App vs. Project (Critical Distinction)

| Feature | APP (System Tool) | PROJECT (User Content) |
| :--- | :--- | :--- |
| **Location** | `pieces/apps/<app_id>/` | `projects/<project_id>/` |
| **Launch** | via `<button href="...">` | via Loader Menu or Fondu |
| **Descriptor** | `<app_id>.pdl` (layout_path) | `project.pdl` (entry_layout) |
| **Editable** | Yes (source in `pieces/apps/`) | Yes (source in `projects/`) |
| **Production** | N/A | Installed via Fondu to `pieces/apps/installed/` |
| **Example** | `player_app`, `chtpm` (system) | `op-ed`, `fuzz-op`, `user`, `test_fondu` |

**Note:** Projects can be installed as production apps via Fondu (`./fondu --install <project>`).

---

## 🔧 5. Fondu Lifecycle Manager

**Fondu** is the TPMOS project lifecycle manager. It automates project deployment, ops registration, and archive management.

### Location
`pieces/system/fondu/fondu.+x`

### Commands

| Command | Description |
| :--- | :--- |
| `--install <project>` | Compile, deploy to `pieces/apps/installed/`, register ops |
| `--uninstall <app>` | Remove from installed/, unregister ops |
| `--archive <project>` | Move to `projects/trunk/` (source only, not compiled) |
| `--restore <project>` | Move from trunk/ back to `projects/`, recompile |
| `--list` | Show all projects and their states |
| `--list-ops` | Show all available ops (from ops catalog) |
| `--help` | Show help message |

### Project States

```
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│   ACTIVE     │      │   ARCHIVED   │      │  INSTALLED   │
│  projects/   │─────>│projects/trunk│─────>│pieces/apps/  │
│              │      │              │      │installed/    │
│ - Editable   │      │ - Source     │      │ - Compiled   │
│ - In compile │      │ - Not built  │      │ - Read-only  │
│ - Dev-facing │      │ - Backup     │      │ - User-facing│
└──────────────┘      └──────────────┘      └──────────────┘
       ↑                      ↑                      ↑
       └──────────────────────┴──────────────────────┘
                    Fondu moves projects between states
```

### Ops Registry

When you install a project with ops, Fondu:
1. Registers ops in `pieces/os/ops_registry/<project>.txt`
2. Updates `pieces/os/compiled_projects.txt`
3. Generates `pieces/os/ops_catalog.txt` (human-readable)

Other projects can then call these ops via PAL scripts:
```pal
OP user::create_profile "player1"
OP user::auth_user "player1"
```

### Example Workflow

```bash
# Install a project to production
./pieces/system/fondu/fondu.+x --install op-ed

# List all projects
./pieces/system/fondu/fondu.+x --list

# List available ops
./pieces/system/fondu/fondu.+x --list-ops

# Archive a project (move to trunk)
./pieces/system/fondu/fondu.+x --archive lsr

# Restore from trunk
./pieces/system/fondu/fondu.+x --restore lsr
```

**Reference:** `#.docs/fondu-investigation-report.txt` for complete documentation.

---

## ⚙️ 6. The Runtime Pipeline (12 Steps)
1.  **INPUT:** `keyboard_input.+x` writes to `keyboard/history.txt`.
2.  **ROUTING:** `chtpm_parser.c` reads the key.
3.  **RELAY:** Parser injects key into active app history (e.g., `player_app/history.txt`).
4.  **TICK:** App Module polls history buffer.
5.  **TRAIT/OP:** Module calls a "Muscle" Op (e.g., `move_entity.+x`).
6.  **SOVEREIGNTY:** Op updates Piece DNA (`.pdl`) or Mirror (`state.txt`).
7.  **MIRROR SYNC:** Update flushed to `state.txt` for fast reading.
8.  **STAGE:** `render_map.+x` reads mirrors and writes `view.txt`.
9.  **SYNC:** `render_map.+x` appends to `frame_changed.txt` (pulse).
10. **COMPOSITION:** Parser substitutes variables and loads system state.
11. **RENDER:** Parser writes composite frame to `current_frame.txt`.
12. **DISPLAY:** Renderer (ASCII or GL) prints to user.

---

## 🛠️ 7. Development Standards
### CPU Safety (Mandatory)
Modules must follow the `fuzz_legacy_manager.c` pattern:
*   **Signal Handling:** Explicit `SIGINT` handlers to prevent orphaned children.
*   **Process Groups:** `setpgid(0, 0)` and group-killing on exit.
*   **Fork/Exec:** Prefer `fork()/exec()/waitpid()` over `system()`.
*   **Throttling:** Use `is_active_layout()` to sleep 100ms when not in focus.
*   **Stat-First:** Only open files if `stat()` confirms they changed.

### UI/Layout Patterns
*   **Buttons:** NEVER manually write `[>]`. Use `<button label="X" onClick="KEY:n" />`.
*   **CLI Input:** Use `<cli_io id="x" label="${var}" />` for text input. The parser handles buffering and masking.

---

## ➕ 8. How to Add to the Codebase
### Creating a New App
1.  Setup: `mkdir -p pieces/apps/<name>/{layouts,plugins/+x,pieces}`.
2.  DNA: Create `<name>.pdl` with `app_id` and `layout_path`.
3.  Logic: Implement `_module.c` using the **CPU-Safe Template**.
4.  Wiring: Add a `<button href="...">` to `os.chtpm` or the loader.

### Creating a New Project
1.  Setup: `mkdir -p projects/<name>/{layouts,manager/+x,maps,pieces}`.
2.  DNA: Create `project.pdl` with `project_id` and `entry_layout`.
3.  Logic: Implement `_manager.c` to handle game state.

---

## 📅 9. Current Status & Roadmap
**"Stabilize the Core, Ignite the AI."**

### ✅ COMPLETED (April 2026)

**Fondu Lifecycle Manager** - Phase 1 Complete
- ✓ Install/uninstall projects
- ✓ Archive/restore functionality
- ✓ Ops registry system
- ✓ Auto-generated ops catalog
- Reference: `#.docs/fondu-investigation-report.txt`

**op-ed (RMMP Editor)** - 81% Feature Complete
- ✓ Piece selection (SPACE key)
- ✓ Undo (Ctrl+Z), Clear tile (Backspace)
- ✓ Create new map (ADD)
- ✓ Focus sync, PDL integration
- ✓ Save/load with roundtrip parity
- ⚠ Context menu (partial)
- ❌ Delete piece, Mirror sync (missing)
- Reference: `#.docs/op-ed-investigation-report.txt`

**Windows Support** - Working with Limitations
- ✓ GL-OS Desktop (OpenGL)
- ✓ Full CHTPM Pipeline
- ⚠ Arrow keys don't work in MSYS2/mintty (use number keys)
- ⚠ XInput controller support untested
- Reference: `pieces/buttons/WINDOWS-RUN-GUIDE.md`

**CHTPM Color Support** - Parser + Project Smoke Test Added
- ✓ Parser now accepts `fg` / `bg` element attributes
- ✓ Terminal renderer applies ANSI colors when present
- ✓ Manager-projected dynamic color KVPs are supported
- ✓ `+-demo` now exercises static color markup as a simple smoke test
- ✓ `xo-pet-v1` now emits color theme state through `gui_state.txt`
- Reference: `chtpm-colors.txt`

---

### 🎯 Near-Term Priorities (Apr 2026)

**1. Core Stabilization**
- [ ] op-ed: Add delete piece functionality
- [ ] op-ed: Add mirror sync support
- [ ] Clean up project_routes.kvp (APP vs PROJECT topology)
- [ ] Populate or remove #.ref/ directory

**2. PAL Scripting & AI**
- [ ] PAL Mastery: Complete scripting engine
- [ ] AI-Recurse: Programmable bot behaviors
- [ ] Auto-battle systems

**3. P2P Market & Trunking**
- [ ] P2P-NET: Decentralized trading layer
- [ ] Entity trunking for high-turnover NPCs
- [ ] Market auctions and trading

**4. Advanced Synthesis**
- [ ] Chemistry/evolution systems
- [ ] Scripted contracts (legal/social logic)
- [ ] AI distillation for R&D

---

## 🔭 10. Future Roadmap
*   **AI-LABS:** Knowledge distillation, local LLM training, and chat instance management.
*   **LSR (Lunar Streetrace Raider):** A "Civ-Lite" simulation with AI-driven R&D and a lunar economy.
*   **P2P-NET:** TPM-compliant blockchain, NFT items, and decentralized trading.
*   **GL-OS (The Visual Shell):** High-fidelity OpenGL rendering shell mirroring text frames.
    *   **Perspective Views:** Real-time 1st person, 3rd person, and free-cam 3D renders of underlying 2D ASCII-OS projects.
    *   **Near-Term Targets:** Rapid implementation of 2D/3D visualizations for `op-ed` and `fuzz-ops`.
    *   **Concept:** GL-OS acts as a visual "theater" that bridges ASCII logic into a modern 3D viewport.
*   **Kernel:** Long-horizon path to RISC-V/x86 kernel using Piece architecture.

---

## 🔧 11. Tools & Maintenance
*   `./compile_all.sh`: Rebuilds all system and project modules.
*   `./run_chtpm.sh`: Starts the environment pipeline.
*   `./kill_all.sh`: Forcefully terminates all PMO processes.
*   `#.tools/pmo_cpu_health.sh`: Monitors for runaway processes.
*   `#.tools/fix_old_pieces.sh`: Migrates legacy pieces to `map_id` standards.

---
"Softness wins. The empty center of the flexbox holds ten thousand things."
