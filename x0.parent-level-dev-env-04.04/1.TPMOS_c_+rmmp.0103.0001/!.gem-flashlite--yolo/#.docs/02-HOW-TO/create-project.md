# Creating a New Project in TPMOS

This guide explains how to create and set up a new project within the TPMOS ecosystem, ensuring it integrates correctly with the system and is selectable from the loader menu.

## 1. Project Structure

A new project should follow a standard directory layout under the `projects/` directory:

```
projects/<new_project_id>/
├── project.pdl              # Project Descriptor (metadata, state, events)
├── history.txt              # Input history (key events) - Optional but recommended
├── layouts/                 # UI layout files (.chtpm)
│   └── <new_project_id>.chtpm
├── manager/                 # Manager source code
│   └── <new_project_id>_manager.c
│   └── +x/                  # Compiled manager binary
│       └── <new_project_id>_manager.+x
├── ops/                     # Op source code
│   └── ops_manifest.txt     # Registry of all ops
│   └── src/                 # Source files for ops
│       └── my_op.c
│   └── +x/                  # Compiled op binaries
│       └── my_op.+x
├── pieces/                  # Project-specific pieces and their state
├── maps/                    # Map data files
└── README.md                # Project overview (optional but good practice)
```

## 2. Project Descriptor (`project.pdl`)

The `project.pdl` file is mandatory and defines the project's core metadata. It uses a structured table format.

**Example (`projects/+-demo/project.pdl`):**
```pdl
SECTION      | KEY                | VALUE
----------------------------------------
META         | project_id         | +-demo
META         | version            | 1.0
META         | entry_layout       | projects/+-demo/layouts/+-demo.chtpm
STATE        | starting_map       | map_01
STATE        | player_piece       | selector
STATE        | current_world      | world_01
RESPONSE     | default            | Project +-demo loaded.
```

**Key Fields:**
*   `META | project_id`: Unique identifier for the project.
*   `META | entry_layout`: The primary `.chtpm` layout file to load when the project starts.
*   `STATE | starting_map`: The initial map file to load.
*   `STATE | player_piece`: The default piece controlled by the player (e.g., 'selector' or 'xlector').

## 3. Manager Module

The manager module (`<project_id>_manager.c`) is responsible for orchestrating the project's logic, handling input, managing state, and triggering renders.

**Key Responsibilities:**
*   Implement CPU-safe patterns (signal handling, fork/exec/waitpid, focus throttling).
*   Read/write state files (`state.txt`, `history.txt`, etc.).
*   Poll input history (`pieces/apps/player_app/history.txt`).
*   Call ops via `run_command()` or direct execution.
*   Update shared state files like `pieces/apps/player_app/state.txt` and `pieces/display/current_layout.txt`.
*   Trigger renders by writing to `pieces/display/frame_changed.txt`.
*   For projects with complex UI, populate variables for `${...}` substitution in layout files (e.g., `${directory_listing}`, `${unread_count}`).

**Compilation:**
```bash
gcc -o projects/+-demo/manager/+x/+-demo_manager.+x projects/+-demo/manager/+-demo_manager.c -pthread
```

## 4. Operations (Ops)

Ops are executable modules that perform specific actions (e.g., moving entities, creating items).

**Structure:**
- Source code in `projects/<project_id>/ops/src/`
- Compiled binaries in `projects/<project_id>/ops/+x/`
- Registry file: `projects/<project_id>/ops/ops_manifest.txt`

**`ops_manifest.txt` Format:**
```
# op_name|description|args_format
```

**Compilation:**
```bash
gcc -o projects/+-demo/ops/+x/my_op.+x projects/+-demo/ops/src/my_op.c
```

## 5. Registration and Installation (Manual)

To make your project selectable and its ops discoverable manually:

1.  **Compile Project Binaries:** Ensure manager and op binaries are created.
    ```bash
    ./button.sh compile
    ```
    This command should place compiled binaries in `projects/<project_id>/manager/+x/` and `projects/<project_id>/ops/+x/`.

2.  **Register Project and Ops Manually:**
    *   **Update Compiled Projects List:** Add your project ID to the global list of compiled projects.
        *   File: `pieces/os/compiled_projects.txt`
        *   Action: Append `<project_id>` to this file.
    *   **Create/Update Project Ops Registry:** Create a file for your project in the ops registry directory. This file lists the ops available for your project.
        *   File: `pieces/os/ops_registry/<project_id>.txt`
        *   Action: Create this file and list each op name (one per line) that is provided by your project.
    *   **Update Global Ops Catalog:** Add your project's ops to the global catalog for discoverability. This file typically contains OpName|Description|ArgsFormat.
        *   File: `pieces/os/ops_catalog.txt`
        *   Action: Append entries for each of your project's ops to this file.

This manual process ensures that the TPMOS system recognizes your project, its binaries, and its available operations.

## 6. GUI Integration and Selection

Once registered manually:
- Your project will appear in the project loader menu (if launched via `LOAD_PROJECT:<project_id>`).
- Its ops will be discoverable via `fondu --list-ops` or similar discovery mechanisms.
- The `entry_layout` specified in `project.pdl` will be loaded by the CHTPM parser.

This process ensures your project is recognized, executable, and its features can be accessed through the TPMOS interface.

---

This document provides the foundational steps for creating a new project. Future steps would involve implementing the specific logic for the "expand collapse" feature within the `+-demo` project's manager and layout files.
