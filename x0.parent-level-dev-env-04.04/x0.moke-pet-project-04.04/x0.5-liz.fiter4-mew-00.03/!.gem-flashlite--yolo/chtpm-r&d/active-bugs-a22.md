# Active Bugs Registry (A22) - RESOLVED

## Bug #1: User Project Routing Failure [RESOLVED]
*   **Symptom**: Selecting the "user" project from the Project Loader fails to open the application.
*   **Fix**: Corrected the layout path in `pieces/os/project_routes.kvp` from `pieces/apps/user/...` to `projects/user/...`.
*   **Verification**: Path is now consistent with the actual filesystem topology.

## Bug #2: ESC Key Deadlock in Active Mode [RESOLVED]
*   **Symptom**: Pressing ESC in "Active [^]" mode does not return the user to "Nav >" mode.
*   **Fix**: Restored `active_index = -1` in the ESC handler within `chtpm_parser.c`.
*   **Verification**: ESC key now correctly deactivates focused elements and returns to navigation mode.

---
*Updated by Superior Agent A22 - April 22, 2026*
