# GL-OS DEVELOPMENT ROADMAP (TECHNICAL SPECIFICATION)

This roadmap outlines the technical implementation steps to bring GL-OS to parity with ASCII-OS, adhering to the TPM (TPM Piece Method) philosophy.

## PHASE 1: UI PARITY AND SUB-LAYOUTS
### Implementation
- **Step 1:** Audit `write_gl_os_state` to ensure exclusive `[>]` rendering.
- **Step 2:** Refactor `execute_option` logic to support `menu_options` state switching.
- **Step 3:** Implement `active_menu_context` stack to handle "Desktop Apps" sub-navigation.

## PHASE 2: FUZZ-OP-GL (VIRTUAL WINDOW)
### Implementation
- **Step 4:** Define `GLWindow` structure in `gl_desktop.c`.
- **Step 5:** Implement `fuzz_op_gl_render_loop` to sync with `pieces/apps/fuzzpet_app/fuzzpet/current_frame.txt`.
- **Step 6:** Implement `focused_window_input_bridge` to delegate input to the active app-window.

## PHASE 3: TILE RENDERING & VIEW TOGGLE
### Implementation
- **Step 7:** Define `pieces/display/tile_pallet.txt` (ASCII char to RGB mapping).
- **Step 8:** Implement `render_map_to_tiles` function:
    - Iterate map array.
    - Resolve char to RGB from pallet.
    - Apply Z-height extrusion for 3D view (tile = 1 Z-unit).
- **Step 9:** Implement `gl_render_mode_toggle` (2D/3D).
- **Step 10:** Implement basic 3D camera controller.

## CONSTRAINTS & PRINCIPLES
- **TPM Philosophy:** Preserve modular component-based architecture.
- **Rendering:** GL-system for maps; standard UI for menus.
- **Coordinate System:** 3D height derived directly from map Z-level data.
