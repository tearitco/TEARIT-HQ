# TPM (True Piece Method) & CHTPM OS Monolith Documentation
**Date:** 2026-03-12
**Status:** AUTHORITATIVE CANONICAL MONOLITH
**Version:** 6.0 (Standardized Ops Edition)

---

## 1. Core Philosophy: The PMO Hierarchy
The system follows a strict thought priority: **PIECE -> MODULE -> OS**. This ensures bottom-up stability and auditable reality.

### 1.1 PIECE (The Atomic Unit / Soul)
- **Definition:** A directory containing text files that define a "Piece" of reality.
- **Sovereignty:** A Piece owns its state exclusively. No in-memory shortcuts.
- **Components:**
    - `piece.pdl` (DNA): The ultimate source of truth. Defines state, methods, and responses.
    - `state.txt` (Mirror): A flat `key=value` file optimized for high-speed reads (Direct Mirror Access).
- **Ontology:** Scale does not change rules. A NAND gate, a Pet, and a Galaxy are all Pieces.

### 1.2 MODULE (The Logic Agent / Brain)
- **Definition:** Orchestrates Pieces to perform complex actions.
- **Responsibility:** Interprets input, executes Ops, and performs the **Mirror Sync**.
- **Modern Mandate:** Delegation. The Brain should not contain "Muscle" logic (like movement). It should delegate to standalone binaries (Ops).

### 1.3 OS / CHTPM (The Theater / View)
- **Definition:** The environment, navigation, and user interface.
- **Responsibility:** Substitutes variables (`${var}`), routes input (Nav vs. Interact), and composes the final frame.
- **Mantra:** "The Parser is the mirror's eye, not the mirror's mind."

---

## 2. The Ops Pipeline (The Muscle Layer)
The "Ops Track" is the standard for modern Piecemark applications. It separates decision-making from execution.

### 2.1 Standardized Traits (Ops)
- **move_entity.+x**: Handles all movement, boundaries (1-18, 1-8), and collisions. It is project-aware via `project.pdl` resolution.
- **place_tile.+x**: Updates project maps safely.
- **create_piece.+x**: Instantiates new piece directories within a project.

### 2.2 Non-Destructive State Synchronization
Multiple components (Module, Renderer, Daemons) may write to the unified `state.txt` of an application. 
**Mandate:** Components must never overwrite the entire file. Use the `set_state_field` pattern to update specific keys while preserving others.

---

## 3. The 12-Step Pipeline (Input to Render)
1. **INPUT:** `keyboard_input.+x` writes ASCII codes to `pieces/keyboard/history.txt`.
2. **ROUTING:** `chtpm_parser.c` reads the key.
3. **RELAY:** If in **INTERACT** mode, Parser "injects" the key into the buffer specified by the `<interact src="...">` tag (Standard: `pieces/apps/player_app/history.txt`).
4. **TICK:** The Project Manager (e.g., `fuzz-op_manager.+x`) polls this shared history buffer.
5. **TRAIT/OP:** The Module calls a standardized "Muscle" Op (e.g., `move_entity.+x`) to process logic.
6. **SOVEREIGNTY:** The Op updates the Piece DNA (`.pdl`) or Mirror (`state.txt`) within the project's `pieces/` directory.
7. **MIRROR SYNC:** The update is flushed to the Piece Mirror (`state.txt`) for high-speed read access.
8. **STAGE:** The Stage Producer (`render_map.+x`) reads all Mirrors and writes `view.txt`.
9. **SYNC:** The Stage Producer performs a **Non-Destructive Update** to `player_app/state.txt`, adding pet stats without wiping module data.
10. **COMPOSITION:** Parser substitutes `${game_map}` from `view.txt` and loads system variables (Clock, Turn).
11. **RENDER:** Parser writes the final composite frame to `current_frame.txt`.
12. **DISPLAY:** The `renderer` (ASCII or GL) prints the frame to the user.

---

## 4. Advanced Capabilities

### 4.1 Prisc & PAL (Assembly Scripting)
**PAL (Prisc Assembly Language)** is a RISC-like scripting language for game events.
- **Prisc VM:** `prisc+x` executes PAL scripts (`.asm`).
- **Purpose:** Allows users to create complex dialogue trees, quests, and AI without recompiling C code.
- **Integration:** Modules call `prisc+x <script.asm>`, which then calls Ops (Muscles) to modify world state.

### 4.2 Dynamic Interaction (<interact> Tag)
The `<interact>` layout tag now supports variable substitution (e.g., `src="projects/${project_id}/history.txt"`).
- **Unified Buffer:** `pieces/apps/player_app/history.txt` is the standard for multi-project compatibility.

### 4.3 Fondu Architecture (Project Lifecycle & Ops Registry)
**Fondu** is the TPMOS project lifecycle manager and ops discovery system.

**Core Insight:** Everything is a PROJECT. "Apps" are just projects launched from different entry points.

```
Project States:
  ACTIVE (projects/)     → Editable, in compile cycle, dev-facing
  ARCHIVED (trunk/)      → Source only, excluded from compile, backup
  INSTALLED (pieces/apps/installed/) → Compiled, read-only, user-facing
```

**Fondu Responsibilities:**
1. **Lifecycle Management:** Move projects between states (`--archive`, `--restore`, `--install`, `--uninstall`)
2. **Ops Registry:** Register exposed ops in `pieces/os/ops_registry/` (discoverable via `--list-ops`)
3. **Compile Manifest:** Maintain `pieces/os/compiled_projects.txt` (controls build list)
4. **Automated Deployment:** Compile + deploy + register ops in one command

**The Ops Ecosystem:**
```
projects/user/ops/+x/         projects/fuzz-op/
├── create_profile.+x  ──────>│ manager/
├── auth_user.+x              │   └── fuzz-op_manager.c
└── get_session.+x            │       # PAL script:
                              │       OP user::create_profile "player1"
pieces/os/ops_registry/       │       OP user::auth_user "player1"
└── user.txt                  │
    (registered ops)          │
```

**Why C from Start:** Fondu dogfoods the architecture - it's a modular C utility with swappable ops, just like the projects it deploys.

**KISS Principle:** "Automate what humans do manually, keep everything swappable."

---

## 5. Technical Pitfalls (The "7 Deadly Pitfalls" + Extensions)

1.  **THE TRIM TRAP:** Filesystem mirrors often have trailing spaces/newlines. Always `trim()` values before comparison.
2.  **popen OVERHEAD:** Calling `popen` 100 times per frame will kill performance. Use **Direct Mirror Access**.
3.  **mtime RESOLUTION:** Filesystem `mtime` has 1s resolution. Use **Marker-File Size Detection**.
4.  **BUFFER TRUNCATION:** Large variables in tag attributes can cause memory corruption. Use **asprintf** for all path and command concatenation.
5.  **RELATIVE PATH CONFUSION:** Never hardcode `../`. Always resolve from `location_kvp` project root.
6.  **OVERWRITING STRUCTURE:** `write_piece_state()` must be section-aware. `render_map.c` must use **Non-Destructive Updates**.
7.  **REDUNDANT RESPONSES:** Background stdout is lost. Use `last_response.txt` and bake it into the frame.
8.  **THE DUAL INJECTION BUG:** Don't inject both numeric and joystick codes for a single action.
9.  **THE STALE ROOT DRIFTER:** Ensure `run_chtpm.sh` resolves `project_root` dynamically at runtime.
10. **THE USE-AFTER-FREE PULSE:** Never `free()` a path string before the `fopen()` call is finished.

---

## 6. Architectural Nuances
- **Training Parity:** Align UI indices with INTERACT hotkeys. The UI is the "Legend".
- **Silent Daemon Pattern:** Background daemons must hit the global `frame_changed.txt` marker.
- **Shadow Selector:** In entity control mode, the selector ('X') is hidden but follows the entity's coordinates. ESC returns control to the selector at that spot.
- **Parser Sovereignty:** Exactly one writer (Parser), many readers. Never swap roles.

---
"If it's not in a file, it's a lie."
