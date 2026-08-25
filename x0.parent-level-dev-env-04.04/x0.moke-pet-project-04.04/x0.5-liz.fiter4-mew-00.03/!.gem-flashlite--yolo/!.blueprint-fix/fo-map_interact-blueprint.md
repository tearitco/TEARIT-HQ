# Fuzz-Op Map & Interaction Blueprint (GLTPM Standard)

This document outlines the architecture for decoupled user input, state management, and rendering in a TPMOS project. It assumes a "virgin agent" is implementing a project from scratch.

## 1. Input Decoupling (The History Pipeline)

In a standard TPMOS project, input is NOT handled by direct polling of the keyboard by the manager. Instead, it follows a "History-First" pattern.

### A. The Input Files
- **Global History:** `pieces/apps/player_app/history.txt` (Legacy/System events).
- **Project History:** `projects/<project_id>/session/history.txt` (Project-specific events).

### B. Input Format
The project history file typically receives appended lines from the UI/Parser:
`[YYYY-MM-DD HH:MM:SS] KEY_PRESSED: 1002` (where 1002 is a key code)
`[YYYY-MM-DD HH:MM:SS] COMMAND: OP fuzz-op::scan`

### C. The Listener (Manager Thread)
The Manager runs a background thread that:
1.  Stores the `last_pos` (byte offset) of the history file.
2.  Polls the file every **~16ms (60 FPS)** for new data (`st_size > last_pos`).
3.  Reads only the new content, parses the key/command, and dispatches it to `route_input()`.

---

## 2. Signaling & Markers (The Pulse)

TPMOS avoids expensive file-size polling for rendering. It uses **Marker Files** (the "Pulse") to wake up the system.

### A. The Signal Files
- **Frame Marker:** `projects/<project_id>/session/frame_changed.txt`
- **State Marker:** `pieces/apps/player_app/state_changed.txt`

### B. The Protocol
When a change occurs (input processed, entity moved, state updated):
1.  **Write Logic:** Append a single character (e.g., `G
` for GL-OS or `M
` for Map) to the marker file.
2.  **Read Logic:** The Renderer/Parser `stat()`s the marker file. If it has grown, it performs a render cycle.

---

## 3. Method Execution (PDL-Driven Interaction)

Interactions are not hardcoded. They are discovered at runtime from the active Piece's DNA.

### A. Discovery
1.  Identify the `active_target_id` (e.g., `pet_01`) from the manager state.
2.  Call `pdl_reader.+x <piece_id> list_methods`.
3.  Map indices 2-8 to the returned list.

### B. Execution
1.  When a user presses a "Method Key" (e.g., '2'), the manager finds the 2nd method name (e.g., `feed`).
2.  Call `pdl_reader.+x <piece_id> get_method <name>` to get the handler path.
3.  Execute the handler (typically a `+x` Op) using `fork()/exec()`.

---

## 4. Map Updates & Rendering

The map is a "Theater" that reflects the current state of all Pieces in a1 location.

### A. The Update Trigger
After any movement or method execution:
1.  **Sync Mirror:** Update `projects/<project_id>/pieces/<piece_id>_mirror/state.txt` so the UI has a local copy.
2.  **Trigger Render Op:** Call `pieces/apps/playrm/ops/+x/render_map.+x`.

### B. The Rendering Chain
1.  `render_map.+x` reads the current `map_id` (e.g., `map_01_z0.txt`).
2.  It iterates through all Piece directories in that map/location.
3.  It reads their `pos_x`, `pos_y`, and `icon` (or `char`) from `state.txt`.
4.  It composes a text grid (the "Frame").
5.  It writes the frame to `pieces/display/current_frame.txt`.
6.  **Crucial:** The Manager then hits the `frame_changed.txt` marker to tell the UI to refresh.

---

## 5. Summary Lifecycle

1.  **USER ACTION** -> Appended to `session/history.txt`.
2.  **MANAGER** -> Detects growth, reads key, calls `route_input()`.
3.  **MANAGER** -> Calls Movement Op or PDL-Method Op.
4.  **OP** -> Modifies Piece `state.txt`.
5.  **MANAGER** -> Calls `render_map.+x`.
6.  **RENDERER** -> Reads all pieces, writes `current_frame.txt`.
7.  **MANAGER** -> Appends `G
` to `session/frame_changed.txt`.
8.  **UI/GL-OS** -> Detects `G
`, reads `current_frame.txt`, and draws.

---
*Language Agnostic Implementation Note:* This pattern works in C, Python, or Shell. "If it's not in a file, it's a lie. The marker is the clock."
---

## 6. Xelector Management & Interaction

### A. Xelector Definition
The Xelector is the logical cursor representing the user's focus on the map grid. It is treated as a sovereign Piece, managed like any other entity with its own state file. Its position (`pos_x`, `pos_y`, `pos_z`) is stored in `projects/<project_id>/pieces/xlector/state.txt` and synced to `projects/<project_id>/session/state.txt` for host access.

### B. Xelector Movement
*   **Map Control Mode (`is_map_control = 1`):**
    *   User input (Arrow keys on Host, WASD/ZXQE for camera movement) is captured by the Host (`gl_desktop.c` or CHTPM parser).
    *   The Host injects normalized key press events (e.g., `KEY_PRESSED: 1000` for Left Arrow) into the project's `session/history.txt`.
    *   The Manager's input thread (`gltpm_input_thread` or `route_input` in ASCII) detects these keys.
    *   **Direct Op Delegation:** If the key corresponds to movement (Arrow keys), the Manager calls the `move_entity.+x` Op, specifically targeting the `xlector` Piece and passing the direction (e.g., `move_entity.+x xlector up`).
    *   The `move_entity.+x` Op updates the `pos_x`, `pos_y`, and `pos_z` values in `projects/<project_id>/pieces/xlector/state.txt`.
*   **Menu Navigation Mode (`is_map_control = 0`):**
    *   Arrow keys are intercepted by the Host's menu navigation logic (`move_selection` function in `gl_desktop.c` or parser logic).
    *   This updates the `selected_index` within the Host's `DesktopWindow` struct or `GLTPMScene` struct.
    *   The Xelector's position on the map remains static during menu navigation.

### C. Relinquishing Control
*   **ESC Key:** This is the universal "escape" key. When pressed, it signals the Host (e.g., `gl_desktop.c` or CHTPM Parser) to:
    1.  Set `is_map_control = 0` locally within the Host's window state.
    2.  Inject `KEY_PRESSED: 27` into the project's `session/history.txt`.
    3.  The Manager, upon receiving this `ESC` command, updates `is_map_control = 0` in its sovereign `session/state.txt`.
    4.  The Manager pulses the `frame_changed.txt` marker.
    5.  The Host detects the pulse, reloads the state, updates its internal `is_map_control` flag, and renders the menu, showing the correct navigation indicators (`[>]`).
*   **Menu Navigation:** Selecting a menu option (e.g., "Control Map" toggle, "Back to Main Menu") also relinquishes Map Control. The Manager receives the input, updates `is_map_control = 0` in `state.txt`, pulses the frame, and the Host renders the menu accordingly.

### D. Entity Interaction & Control
*   **Selection:**
    1.  In Map Control mode (`is_map_control = 1`), the Manager continuously monitors the `xlector`'s position by reading `projects/<project_id>/pieces/xlector/state.txt`.
    2.  If the Xelector's coordinates (`pos_x`, `pos_y`) overlap with another entity's coordinates (also read from their respective `state.txt` files), the Manager identifies the entity.
    3.  The Manager then sets `active_target_id` to the selected entity's ID (e.g., `active_target_id = "pet_01"`).
    4.  This change is saved to `session/state.txt` and pulsed via `state_changed.txt` to notify the Host.
*   **Method Invocation:**
    1.  Once an entity is targeted (`active_target_id` is set), pressing Method Keys ('2'-'8' or corresponding joystick inputs) triggers the Manager.
    2.  The Manager uses PDL discovery (`pdl_reader.+x`) to find the correct handler Op for the `active_target_id` and the requested action (e.g., `get_method feed`).
    3.  The Manager executes the handler Op via `fork()/exec()`, passing control to the specialized Op for action.
*   **Shadowing:**
    *   In specific camera modes (e.g., 1st Person, 3rd Person Follow), the camera's view (`cam_pos`, `cam_rot` managed in the Manager's state and synced to `session/state.txt`) is dynamically updated to follow the `active_target_id`'s position and orientation. This provides a "shadow" or "following" perspective.

---
*Documented by Gemini CLI - 2026-04-24*