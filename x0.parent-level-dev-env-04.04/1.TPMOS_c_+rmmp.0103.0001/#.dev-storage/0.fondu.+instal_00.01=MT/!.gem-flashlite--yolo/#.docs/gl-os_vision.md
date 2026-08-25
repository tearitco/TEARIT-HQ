# GL-OS Vision & Architecture: The Virtual Desktop Host
**Date:** 2026-04-22
**Status:** ARCHITECTURAL BLUEPRINT - VERSION 2.0

---

## 1. CORE VISION: GL-OS AS A HOST ENVIRONMENT

GL-OS is designed as a **high-fidelity, modular desktop environment** that **hosts independent GL-native applications**. It is NOT a direct renderer of ASCII processes or their internal memory states. Instead, it functions as a sophisticated display manager and input router for projects that adhere to TPMOS principles.

### 1.1 The "Host" vs. "Mirror" Distinction

-   **GL-OS as Host:** The GL-OS desktop environment manages windows, input events, and the rendering pipeline for GL-native applications. It provides the "theater" for these applications.
-   **File-State Mirroring:** GL-OS applications within this host environment interact with **project-specific data files** (e.g., `current_frame.txt`, `state.txt`, `view.txt`, tile definitions) located within their own project directories (e.g., `projects/gl-os/fuzz-op-gl/`). This ensures data sovereignty and decouples GL applications from the internal state of ASCII TPMOS processes.
-   **Process Sovereignty:** GL-OS and any ASCII TPMOS applications remain entirely **independent processes**. Communication and state synchronization between them are mediated exclusively through file I/O, adhering to the TPMOS "filesystem as truth" principle.

---

## 2. ARCHITECTURE: Modular GL Applications

### 2.1 Project Structure (`projects/gl-os/`)

Independent GL applications will reside in their own subdirectories under `projects/gl-os/`. Each application will follow a standard structure:

```
projects/gl-os/
└── fuzz-op-gl/              # Example: A GL-native version of Fuzz-Op
    ├── fuzz-op-gl.pdl       # Application descriptor (methods, state, etc.)
    ├── layouts/             # GLSL shaders, UI definitions
    │   └── fuzz-op-gl.glsl
    ├── manager/             # Application logic (C/C++)
    │   ├── src/
    │   │   └── fuzz-op_manager.c
    │   └── +x/
    │       └── fuzz-op_manager.+x
    ├── pieces/              # Project-specific data
    │   ├── fuzzball/        # Player character piece
    │   │   ├── piece.pdl
    │   │   └── state.txt
    │   ├── world_01/map_01/ # Map data
    │   │   └── ...
    └── assets/              # Graphical assets
        ├── tiles/           # 8x8x8 RGB cube definitions (.txt)
        │   ├── wall.txt
        │   └── floor.txt
        └── sprites/         # 2D/3D sprites (ASCII art with transparency)
            └── player.png # Example sprite file
```

### 2.2 Rendering Pipeline
-   **Tile-Based Rendering:** GL-native applications will utilize a renderer that interprets `.txt` files containing RGB256 codes for 8x8x8 cubes.
    -   **2D View:** Cubes are rendered as flat tiles.
    -   **3D View:** Cubes are extruded based on Z-level data, creating a sense of depth.
-   **Sprite Support:** Rendering will also accommodate 2D/3D sprites with transparency layers for entities and other dynamic objects.
-   **Camera System:** Support for switchable camera perspectives: **Free Camera**, **1st Person**, and **3rd Person**.

---

## 3. DEVELOPMENT & INTEGRATION

### 3.1 Launching GL Apps
-   **Project Loader Integration:** The ASCII Project Loader menu (`playrm/loader.chtpm`) will include entries for GL projects (e.g., "Fuzz-Op-GL", "Op-Ed-GL").
-   **`gl_desktop.c` Role:** The GL-OS desktop manager will intercept selections for GL projects. Instead of just displaying text or mirroring ASCII, it will:
    1.  Launch the corresponding GL application binary (e.g., `projects/gl-os/fuzz-op-gl/+x/fuzz-op_manager.+x`).
    2.  Manage its window, input, and lifecycle.
    3.  Ensure GL window state is isolated and controlled.

### 3.2 Input Handling & Synchronization
-   **Input Bridging:** GL windows will capture input events (keyboard, mouse, joystick) and relay them to the appropriate application process. The exact mechanism (e.g., piping, shared file) will be defined per application, prioritizing TPM's file-based IPC.
-   **State Synchronization:** GL applications will read and write their state to files within their project directory, maintaining consistency.

---

## 4. FUTURE CONSIDERATIONS

-   **Inter-Theater Communication:** While GL-OS apps are sovereign, future enhancements may involve mechanisms for GL apps to "read" specific ASCII state files (e.g., global player stats) for display purposes, further enabling hybrid experiences.
-   **Asset Pipeline:** Standardizing asset formats (tiles, sprites) for seamless integration across projects.

---

## 5. DOCUMENTATION & TERMINOLOGY

-   **File-State Mirroring:** GL-OS applications mirror the *state of data files* written by their respective projects. This is the primary mechanism for rendering content.
-   **Process Sovereignty:** GL-OS and application processes are independent.
-   **Exo-Sovereign Apps:** Applications like `fuzz-op-gl` and `op-ed-gl` are external to the core GL-OS manager but hosted by it.

---
*"GL-OS is the canvas. Projects are the paintings. Synchronization is via the file system."*
