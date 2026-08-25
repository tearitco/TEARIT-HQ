# GLTPM Iteration 2 (A23) - Technical Issues

## Issue #1: Pre-Parse Variable Substitution in `gltpm_parser.c`
**Goal:** Mirror `chtpm_parser.c` behavior where the entire layout file is substituted before tag parsing.
- **Task:** Refactor `gltpm_load_scene` to read the layout into a buffer, substitute all `${var}` using `gltpm_substitute_vars`, and then parse the resulting string.
- **Benefit:** Allows `${game_map}` to be used anywhere and simplifies tag attributes.

## Issue #2: ASCII-to-Tile Canvas Rendering
**Goal:** Render ASCII map strings as 3D tile grids.
- **Task:** 
    - In `gltpm_load_scene`, after substitution, detect multi-line text blocks.
    - Load `projects/<id>/assets/tiles/registry.txt`.
    - Map each character in the text block to a Tile or Sprite using the registry.
    - Load metadata (`color`, `extrude`, etc.) from corresponding `.tile.txt` or `.sprite.txt` files.
- **KPI:** Removing `<tilemap>` and `<sprite>` from `main.gltpm` and replacing with `${game_map}` results in the same scene.

## Issue #3: Menu Rendering Order and Navigation Fix
**Goal:** Fix reverse Y-order (Concern #2).
- **Task:** 
    - In `gl_desktop.c`, update the button rendering loop to draw from top to bottom.
    - Ensure `win->selected_index` maps 0 to the top button.
    - Update `keyboard()` and accumulation logic to match the new visual order.

## Issue #4: Responsive `onClick="INTERACT"` Handling
**Goal:** Fix "Enter" key not entering interact mode (Concern #3).
- **Task:**
    - Update `dispatch_gltpm_button` in `gl_desktop.c` to check if `onClick` is `"INTERACT"`.
    - If so, set `win->is_map_control = 1` immediately.
    - Ensure key forwarding (WASD/Arrows) is active when `is_map_control` is set.
    - Verify `fuzz-op-gl_manager.c` handles the `INTERACT` command and subsequent keys.

## Issue #5: Standardize Input Key Codes
**Goal:** Ensure cross-framework compatibility.
- **Task:** 
    - Update `keyboard()` and `special_keyboard()` in `gl_desktop.c` to use canonical CHTPM key codes:
        - UP: 1002
        - DOWN: 1003
        - LEFT: 1000
        - RIGHT: 1001
        - ESC: 27
        - ENTER: 13
    - Update `fuzz-op-gl_manager.c` to listen for these codes.
