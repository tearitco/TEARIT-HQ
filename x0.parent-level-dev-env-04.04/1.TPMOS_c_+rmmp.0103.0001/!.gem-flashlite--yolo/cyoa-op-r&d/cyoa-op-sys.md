# CYOA Engine Operational System (Dynamic Content)

The `cyoa-engine` project is a high-level reference for dynamic, data-driven content within the TPMOS ecosystem. It demonstrates how to turn external filesystem data (books/pages) into interactive CHTPM menus.

## 1. Dynamic PDL Generation (The "Sovereign Engine" Pattern)
Unlike static projects, the `cyoa-engine` manager *rewrites* its own Piece DNA based on the current context (Book List or Page Choices).

### The Lifecycle:
1.  **Discovery**: `resolve_books()` scans the `data_xl_root` for directories.
2.  **DNA Synthesis**: `write_pdl()` generates `projects/cyoa-engine/pieces/engine/piece.pdl`.
    -   In **Book Mode**: Methods are created for each book folder found.
    -   In **Choice Mode**: Methods are created for each branch identified by `get_choices.+x`.
3.  **Command Binding**: Every generated method is bound to a custom command string (e.g., `CYOA:SELECT:%d`).

## 2. Interactive Loop & Input Routing
The CYOA engine bypasses standard Op-dispatch by processing its own custom commands in the manager.

### Input Flow:
1.  **Parser**: Handles numeric keypresses (1-9) or button clicks.
2.  **Injection**: Writes `COMMAND: CYOA:SELECT:n` to `player_app/history.txt`.
3.  **Manager**: `process_command()` parses the ID and switches the internal `status` or `current_page`.
4.  **Pulse**: `hit_frame_marker()` signals CHTPM to re-render the new state.

## 3. External Tool Integration (mpg123)
Audio is handled by a specialized Op `play_audio.+x` which forks `mpg123`.

-   **Process Management**: The manager uses `stop_audio()` (sending `--stop`) to ensure only one track plays at a time.
-   **PID Tracking**: The `play_audio` Op (visible in source) saves the `mpg123` PID to `audio.pid` to allow for clean termination.

## 4. Key Takeaways for MP3 Store Migration
-   **PDL vs Variable**: `cyoa-engine` uses `${piece_methods}` (PDL-driven) while `mp3-store` currently uses a custom `${mp3_list}` variable. Using `${piece_methods}` is the canonical TPM way.
-   **Path Resolution**: `cyoa-engine` uses `location.txt` for local data overrides, which is critical for accessing external libraries like `#mp3-library`.
-   **Command Pattern**: Using `MP3:PLAY:n` style commands in `onClick` is more robust than hardcoded `system()` calls in the manager.

---
*Documented by Gemini CLI - 2026-04-21*
