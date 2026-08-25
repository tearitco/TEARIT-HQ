# TPMOS Project Template
This directory serves as a template for creating new TPMOS projects.

## Project Structure
To create a new project (e.g., `my-project`):

1. **Directories:**
   Create the following structure:
   ```
   projects/my-project/
     ├── config/
     ├── layouts/
     ├── manager/
     ├── ops/
     ├── pieces/
     ├── sandbox/
     └── state/
   ```

2. **Files:**
   - **Manager:** `projects/my-project/manager/my-project_manager.c`
   - **Layout:** `projects/my-project/layouts/my-project.chtpm`
   - **Project PDL:** `projects/my-project/project.pdl`

## Registration & Integration

### 1. Register in Routes
Add the project to `pieces/os/project_routes.kvp`:
`my-project=projects/my-project/layouts/my-project.chtpm`

### 2. Compilation
The system automatically compiles all managers named `${proj_name}_manager.c` within `projects/${proj_name}/manager/` when running `./button.sh compile`. No changes needed.

### 3. Cleanup (kill_all.sh)
Add your project manager to `pieces/os/kill_all.sh` to ensure it is killed during system cleanup:
```bash
surgical_kill "my-project_manager"
```

## Developer Notes
- Ensure your manager has a clean exit and registers signal handlers (SIGINT/SIGTERM).
- Always use the TPMOS pattern: `PIECE` → `MODULE` → `OS`.
- If your manager needs system libraries (like `libcrypto`), you must manually add the compilation rule to `#.dev-storage/#.tools/compile_all.sh` similar to how the `user` manager is handled.
