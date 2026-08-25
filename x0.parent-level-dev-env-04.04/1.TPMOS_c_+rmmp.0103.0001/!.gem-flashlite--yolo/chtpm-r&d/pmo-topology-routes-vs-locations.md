# PMO Topology: Routes vs. Locations R&D

## 1. The Functional Difference

This document clarifies the distinct roles of `project_routes.kvp` and `location_kvp` within the TPMOS architecture.

### A. `location_kvp` (The "Where" Registry)
*   **Location:** `pieces/locations/location_kvp`
*   **Purpose:** Provides a static mapping of physical filesystem paths for core system components.
*   **Usage:** Read by almost every binary (Managers, Ops, Parser) at startup to resolve the `project_root` and find sibling directories (e.g., `system_dir`, `pieces_dir`).
*   **Dynamic Generation:** Usually generated at runtime by `run_orchestrator.sh` or `run_chtpm.sh` to ensure paths are accurate to the current installation directory.
*   **Mandate:** "Only location_kvp is truth." Absolute paths should never be hardcoded; they must be resolved through this file.

### B. `project_routes.kvp` (The "How" Registry)
*   **Location:** `pieces/os/project_routes.kvp`
*   **Purpose:** Maps logical Project IDs to their entry-point `.chtpm` layout files.
*   **Usage:** Exclusively used by the CHTPM Parser's `LOAD_PROJECT` command. When a user selects a project in the loader, the parser looks up the ID in this file to decide which layout to display.
*   **Fallback:** If a project is missing here, the parser falls back to `pieces/apps/playrm/layouts/404.chtpm`.

---

## 2. The `fuzzpet_app` vs. `fuzz-op` Situation

### Current State
*   **`fuzzpet_app` (Legacy App):** Located in `pieces/apps/fuzzpet_app/`. This was the original, hardcoded "Pet Sim" application.
*   **`fuzz-op` (Modern Project):** Located in `projects/fuzz-op/`. This is the new, project-topology version of the pet sim.

### Is `fuzzpet_app` safe to delete?
**No.** Currently, it is **not safe** to delete the `fuzzpet_app` directory because the core `chtpm_parser.c` still has hardcoded dependencies on it:

1.  **Global Fallback:** In `load_vars()`, if the parser is not in a "modern layout," it defaults to loading state from `pieces/apps/fuzzpet_app/manager/state.txt`.
2.  **Hardcoded Load:** The parser calls `load_state_file("pieces/apps/fuzzpet_app/fuzzpet/state.txt", "fuzzpet")` unconditionally during every variable load cycle.
3.  **Variable Shadowing:** Many UI variables (like `${pet_hunger}`, `${pet_energy}`) are still being fed from the legacy app's state file rather than the project-specific one, unless explicitly overridden in the `fuzz-op` special case block.

### Recommendation
To successfully retire `fuzzpet_app`:
1.  Refactor `chtpm_parser.c` to remove hardcoded paths to `pieces/apps/fuzzpet_app`.
2.  Update the `fuzz-op` project to own all relevant pet state.
3.  Transition the "default" state loading to a more generic system-wide state file or clear it entirely when no project is active.

---

## 4. Initialization & Generation Issues

### Stale `location_kvp` Generation
Research into `pieces/buttons/shared/run_orchestrator.sh` reveals that the `location_kvp` file is generated on every launch with **hardcoded legacy paths**.

*   **Observed Paths in Script:**
    ```bash
    fuzzpet_app_dir=${POSIX_PATH}/pieces/apps/fuzzpet_app
    fuzzpet_dir=${POSIX_PATH}/pieces/apps/fuzzpet_app/fuzzpet
    manager_dir=${POSIX_PATH}/pieces/apps/fuzzpet_app/manager
    ```
*   **Missing modern mappings:** The script does not yet include mappings for modern projects like `fuzz-op` or `chat-op`.
*   **Dead links:** Mappings like `chat_app_dir=projects/chat-app/` (seen in some versions of the file) point to non-existent directories, as the modern equivalent is `projects/chat-op/`.

### The Dependencies Trap
While the user correctly noted that `fuzzpet` was superseded by `fuzz-op`, the system is currently "haunted" by the legacy app because:
1.  `run_orchestrator.sh` continues to bake the paths into `location_kvp`.
2.  `chtpm_parser.c` continues to use those `location_kvp` keys (or hardcoded fallbacks to those paths) for state loading.

---
## 5. Final Summary: Routes vs. Locations

| Feature | `project_routes.kvp` | `location_kvp` |
| :--- | :--- | :--- |
| **Logic** | "How do I start this project?" | "Where is this system component?" |
| **Data Type** | Project ID → Layout Path | System Key → Absolute Path |
| **Primary User** | `chtpm_parser.c` (`LOAD_PROJECT`) | All binaries (for `project_root`) |
| **Status** | Modern, project-aware | Mixed (Legacy & System) |

---
*Documented by Superior Agent A22 - April 22, 2026*
