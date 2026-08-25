# GLTPM Fuzz-Op: Functional Parity Roadmap
**Date:** 2026-04-23
**Status:** AUTHORITATIVE ARCHITECTURAL HANDOFF
**Mission:** Transition `fuzz-op` from an ASCII-only simulation to a data-driven 3D GLTPM virtual window with 1:1 functional parity.

---

## 1. Executive Summary: The "Basics Proven" State
As of today, the following infrastructure is verified and operational:
- **Host Container:** `gl_desktop.c` acts as the OS theater, managing virtual windows.
- **GLTPM Parser:** `gltpm_parser.c` is integrated into the host. It supports `${var}` substitution, `<tilemap>` loading, `<sprite>` rendering, and `<button>` overlays.
- **IPC Loop:** Selecting a GLTPM layout (e.g., `demo_world.gltpm`) opens a window that writes to the project's `history.txt`, which a background manager then processes to update `state.txt` and a `frame_changed.txt` marker.
- **Separation of Concerns:** Project logic (Managers/Ops) remains sovereign in `/projects/`, while display logic is handled by the Host window.

---

## 2. The GLTPM Pulse: 12-Step Data Flow
To achieve parity, we follow this strict circulatory system. If it's not in a file, it's a lie.

1.  **Input Capture:** User presses a key or clicks a button in the GL virtual window.
2.  **Normalization:** `gl_desktop.c` identifies the active `gltpm_app` window.
3.  **Injection:** The host appends a timestamped command (e.g., `COMMAND: OP fuzz-op::scan`) to `projects/fuzz-op/session/history.txt`.
4.  **Wake-up:** The host (optionally) signals the project manager or the manager detects the `stat()` delta on `history.txt`.
5.  **Tick:** `fuzz-op_manager.c` reads the command queue.
6.  **Action:** The manager forks/execs a TPM Op (e.g., `move_entity.+x`).
7.  **Sovereignty:** The Op updates the Piece state (e.g., `projects/fuzz-op/pieces/fuzzball/state.txt`).
8.  **Pulse:** The manager appends a byte to `projects/fuzz-op/session/frame_changed.txt`.
9.  **Detection:** `gl_desktop.c` (via its `timer()` loop) sees the marker growth.
10. **Rebuild:** `gltpm_parser.c` re-parses the `.gltpm` layout, re-loads `state.txt` variables, and scans the `pieces/` directory for updated coordinates.
11. **Composition:** A new OpenGL scene graph is generated in memory.
12. **Render:** The host `draw_window()` loop renders the new scene into the virtual window's rectangle.

---

## 3. Immediate Future Steps (The Parity Phase)

### Phase 1: Asset Compliance Registry
We must bridge the visual and textual worlds. Every GL asset needs metadata.
- **Path:** `projects/fuzz-op/assets/`
- **Tiles:** Create `.tile.txt` files for `grass`, `wall`, and `treasure`. Each must define `ascii`, `unicode`, and `rgb_top`.
- **Sprites:** Create `.sprite.txt` files for `hero_idle`, `zombie_idle`, and `tree`.

### Phase 2: The Fuzz-Op GLTPM Layout
Create `projects/fuzz-op/layouts/main.gltpm`.
- **Dynamic Tilemap:** Use `<tilemap source=".../map_01_z0.txt" legend=".../registry.txt" />`.
- **Recursive Pieces:** Instead of hardcoded entities, the parser must eventually iterate the `pieces/` directory to render sprites at `${piece:pos_x}` and `${piece:pos_y}`.
- **Button HUD:** Add overlays for `[Scan]`, `[Collect]`, and `[Place]`.

### Phase 3: Interactive Parity (Map Control)
- **Xelector Logic:** Map arrow keys to update a `xelector` piece's state. 
- **Map Mode:** When "Control Map" is toggled, WASD should route movement commands directly to the `fuzz-op` manager instead of the GL-OS menu.
- **Command Dispatch:** Ensure `onClick` actions correctly trigger the existing C-based Ops in `projects/fuzz-op/ops/`.

### Phase 4: Z-Level & Camera Compliance
- **Extrusion:** Tiles with `pos_z > 0` must use the `draw_tile()` extrusion logic.
- **Camera Modes:** Support the 4 modes (1st Person, Tactical, 3rd Person Follow, Free) by translating camera state to OpenGL `gluPerspective` and `glTranslatef` calls.

---

## 4. Technical Specifications

### Tile Metadata (`.tile.txt`)
```text
tile_id=wall
ascii=#
unicode=█
rgb_top=100,100,100
extrude=1.5
solid=1
```

### Sprite Metadata (`.sprite.txt`)
```text
sprite_id=fuzzball_idle
ascii=@
unicode=🐶
rgb=255,200,200
anchor=center
```

### Coordinate System
- **Grid Space:** Integers (e.g., 5, 5).
- **GL Space:** `grid_val * 1.2f` (to allow for tile spacing/gutters).
- **Z-Space:** `0.0` is ground. Each Z-level adds `0.5f` to the extrusion height.

---

## 5. Handover Instruction
If you are a new agent:
1. **Verification:** Run `./button.sh compile` and launch GL-OS. Ensure "GLTPM Apps" is visible.
2. **Path Resolution:** Always use `resolve_root()` to find `project_root`. Never hardcode `/home/debilu/...`.
3. **Safety:** Never use `system()`. Use the `fork/exec` patterns found in `fuzz-op_manager.c`.
4. **Mantra:** If the graphical window and the ASCII state file disagree, the file is the truth. Update the code to fix the agreement.
