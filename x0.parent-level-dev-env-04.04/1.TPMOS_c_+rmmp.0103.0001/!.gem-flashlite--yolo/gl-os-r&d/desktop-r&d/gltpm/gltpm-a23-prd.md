# GLTPM Framework - Iteration 2 (A23) PRD

## 1. Overview
The GLTPM (OpenGL TPM) framework is being updated to align more closely with the architectural principles and implementation patterns of the original CHTPM (ASCII TPM) framework. The goal is to move away from hardcoded scene definitions and towards a modular, "dumb theater" model where the project manager owns the state, and the GL host is responsible for rendering substituted layouts.

## 2. Core Objectives
- **Sovereign Layouts:** Remove hardcoded entity definitions from `.gltpm` files.
- **ASCII-to-3D Canvas:** Implement a mechanism to convert ASCII map data (from `${game_map}`) into 3D tiles based on a project-specific registry.
- **CHTPM Parity:** Ensure `onClick="INTERACT"` and variable substitution behave exactly like `chtpm_parser.c`.
- **UI Consistency:** Fix menu ordering and navigation directions.

## 3. Architectural Changes

### 3.1 `gltpm_parser.c` (Host-side)
- **Variable Substitution:** Move to a pre-parse substitution model. The entire `.gltpm` file should have `${var}` placeholders replaced with values from `state.txt` and `gui_state.txt` BEFORE the tag parser runs.
- **Map-as-Canvas:**
    - Any `TOKEN_TEXT` that contains multiple lines or is identified as a map (e.g., following a label like "GAME MAP") should be processed as a grid of 3D tiles.
    - **Glyph Registry:** The parser must look for `projects/<project_id>/assets/tiles/registry.txt` to map ASCII characters (glyphs) to 3D Tile IDs or Sprite IDs.
    - **Asset Discovery:** For each ID found in the registry, the parser should auto-discover metadata in `assets/tiles/<id>.tile.txt` (for static tiles) or `assets/sprites/<id>.sprite.txt` (for dynamic entities).
- **Generic UI Architecture:** Move towards a more generic element structure similar to `chtpm_parser.c`'s `UIElement`, allowing for nested panels and recursive rendering.

### 3.2 `gl_desktop.c` (Host-side)
- **Menu Ordering:** Flip the rendering loop for menu buttons. Index 1 (and the accumulation buffer `Nav > 1`) should target the topmost button.
- **Interact Mode (Concern #3):**
    - When `Enter` is pressed on a button with `onClick="INTERACT"`, the host should immediately set `win->is_map_control = 1` locally.
    - This allows immediate routing of WASD/Arrow keys to the project's `history.txt`.
- **Input Routing:** Standardize key codes injected into `history.txt` to match CHTPM standards (e.g., Arrows = 1000-1003).

### 3.3 `fuzz-op-gl_manager.c` (Manager-side)
- **Sovereign State:** The manager must continue to own the camera coordinates (`cam_x`, `cam_y`, `cam_z`, etc.) and the `is_map_control` state.
- **Interact Handling:** The manager must handle `COMMAND: INTERACT` in its input thread to toggle internal logic if necessary.
- **Camera Logic:** Ensure WASD keys in `history.txt` trigger updates to the camera variables in `state.txt`.

## 4. Requirements & KPIs
- **KPI-1 (Map Parity):** `fuzz-op-gl` can use the exact same ASCII map as `fuzz-op`, and it renders correctly in 3D.
- **KPI-2 (Interaction Parity):** Pressing 'Enter' on "Control Map" enables 3D flight/movement immediately.
- **KPI-3 (Visual Parity):** Menus are top-down and match the ASCII layout's logical order.

## 5. Branching Design Tree
1. **Parser Refactor:** Implement pre-parse substitution -> **Dependency:** `gltpm_load_vars_from_file`.
2. **Canvas Implementation:** Implement ASCII-to-Tile logic -> **Dependency:** `registry.txt` loader.
3. **UI Fixes:** Flip menu Y-order in `gl_desktop.c`.
4. **Interact Loop:** Sync `onClick="INTERACT"` handling between host and manager.
