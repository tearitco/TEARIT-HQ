# GL-OS Architecture & Project Management
**Date:** May 30, 2026
**Status:** TECHNICAL SPECIFICATION - VERSION 3.0

---

## 1. CORE ARCHITECTURE: THE DESKTOP HOST

GL-OS is a high-fidelity OpenGL desktop environment that hosts independent, modular projects. It functions as a **Display Manager**, **Window Manager**, and **Input Router**, adhering to the TPMOS "filesystem as truth" principle.

### 1.1 The "Host" vs. "Sovereign" Model
-   **GL-OS as Host:** Manages the OpenGL context, windowing system (Z-order, focus, minimize/maximize), and global taskbar.
-   **Project Sovereignty:** Each project (e.g., `fuzz-op-gl`, `media-editor`) is an independent entity. Sovereign projects ignore the global terminal state and maintain their own layouts, pieces, and logic.
-   **The Theater Mode:** GL-OS provides the "Theater" (the window) where the project's "Performance" (the rendering and interaction) takes place.

---

## 2. DYNAMIC PROJECT LOADER (PDL-DRIVEN)

As of version 3.0, GL-OS no longer uses hardcoded project lists. It employs a dynamic, descriptor-driven discovery system.

### 2.1 Project Discovery Lifecycle
1.  **KVP Curated Scan:** The system first reads `pieces/apps/gl_os/gl_os_projects.kvp` for "pinned" or preferred projects.
2.  **Live Directory Scan:** It then scans the `projects/` directory for any folder containing a `project.pdl` file.
3.  **Metadata Extraction:** For each discovered project, GL-OS extracts:
    -   `app_title` (or `title`) from the `STATE` section for the UI label.
    -   `entry_layout` from the `META` section to determine the starting file.
4.  **Registration:** Discovered projects are added to the "Project Explorer" menu (up to 50 slots).

### 2.2 Intelligent Launching
When a project is launched from the GL-OS menu:
-   **Priority 1:** Launch using the `entry_layout` defined in the PDL.
-   **Priority 2 (Convention):** Look for `projects/<id>/layouts/<id>.gltpm` or `main.gltpm`.
-   **Priority 3 (Special):** Execute built-in logic (e.g., `emoji-studio` 3D host).
-   **Priority 4 (Fallback):** Launch a "Mirror Window" that follows global terminal state.

---

## 3. CHTMGL INTEGRATION & DEVELOPMENT TIERS

To bring rich, HTML-like UI capabilities to OpenGL, GL-OS is integrating the CHTMGL framework through a tiered development process.

### 3.1 The Development Pipeline
-   **`chtmgl-alpha`:** The primary testbed for porting CHTMGL features. Focuses on upgrading the parser to recognize `<window>`, `<panel>`, `<slider>`, and `<canvas>`.
-   **`chtmgl-beta`:** Staging area for verified features before they are moved into production-ready system tools.
-   **`media-editor`:** The target application, utilizing the advanced CHTMGL feature set to allow dragging/dropping of assets (audio, video, images, OBJ, FTL) and scene animation.

---

## 4. INPUT & IPC MODEL

### 4.1 Input Routing
-   **Menu Mode:** Mouse and keyboard interactions navigate the GL-OS UI or the project's CHTMGL overlay.
-   **Map Mode (INTERACT):** Input is routed directly to the project's `history.txt`. This allows sub-16ms latency for movement (Mode 1).
-   **Focus Lock:** GL-OS maintains an `input_focus.lock` to prevent terminal-based daemons from fighting for control when a GL window is active.

### 4.2 CHTMGL-TPMOS IPC
Communication between the GL View and the Project Manager uses specific message prefixes:
-   `SLIDER:<id>:<val>` - Direct attribute update.
-   `CANVAS:<x>,<y>` - Click/Interaction within a drawing context.
-   `EVENT:<name>` - Decoupled trigger for manager-side logic.
-   `VAR;<name>=<val>` - Real-time state synchronization.

---

## 5. RECENT ENHANCEMENTS (MAY 2026)

-   **Sovereignty Fix:** Development tiers (`alpha`, `beta`, `media-editor`) now skip global terminal sync, ensuring they always load their unique development layouts.
-   **Expanded Capacity:** Internal project structures expanded from 20 to 50 entries.
-   **Unified PDL Helper:** A robust `get_pdl_value` function integrated into `gl_desktop.c` for consistent metadata extraction across the OS.

---
*"One File, One Truth. GL-OS renders the reality described by the PDL."*
