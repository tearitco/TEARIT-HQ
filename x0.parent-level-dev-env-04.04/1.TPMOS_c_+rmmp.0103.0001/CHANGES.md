# Changes Log - TPMOS Windows Port (kilo branch)

## 2026-10-01 - Hardcoded Path Removal & Interact-Mode Bug Fix

### 1. Hardcoded Path Removal (location_kvp)
**Files Changed:**
- `pieces/locations/location_kvp` - **DELETED** (was checked-in with absolute paths)
- `.gitignore` - Added `pieces/locations/location_kvp` to ignore list

**Details:**
The checked-in `location_kvp` contained hardcoded absolute paths:
```
project_root=C:/Users/jbro8/OneDrive/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001
pieces_dir=C:/Users/jbro8/OneDrive/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001\pieces
...
```

**Fix:** The launch scripts already dynamically generate this file at runtime:
- `run_chtpm.sh` (Linux/macOS): Uses `SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"`
- `pieces/buttons/shared/run_orchestrator.sh` (Windows MSYS2): Uses `pwd -W` for native Windows paths

All managers read `location_kvp` via `resolve_paths()` at startup to get `project_root`, then build paths relative to it (e.g., `projects/cpp-llm/config/apis.txt`).

**Project Rename Note:** Directory `1.TPMOS_c_+rmmp.0103.0001` → `200.TPMOS_c_+rmmp.0103.0001` blocked (current session inside it). Must run `git mv` from external shell.

---

### 2. Interact-Mode Bug Fix (active_gui_is_typing guard)
**Problem:** AI managers triggered `process_input_trigger()` on Enter key without checking if a text field was actively being typed in. This caused Enter in text fields (search, file paths, input boxes) to incorrectly fire AI queries instead of submitting the field.

**Root Cause:** The Enter-key handler in all three AI managers was:
```c
if (key == 10 || key == 13) { process_input_trigger(); state_changed = 1; }
```

**Fix Applied to Three Managers:**

#### `projects/cpp-llm/manager/cpp-llm_manager.c`
- Added `get_active_gui_is_typing()` function (reads `pieces/display/active_gui_is_typing.txt` via `project_root`)
- Gated Enter handler:
```c
if (key == 10 || key == 13) {
    if (!get_active_gui_is_typing()) {
        process_input_trigger(); state_changed = 1;
    }
}
```

#### `projects/groq-ollama/manager/groq-ollama_manager.c`
- Added same `get_active_gui_is_typing()` function
- Same guard on Enter handler

#### `projects/gem-dev/manager/gem-dev_manager.c`
- Added same `get_active_gui_is_typing()` function  
- Same guard on Enter handler

**Pattern Source:** `projects/agy-text-editor/manager/agy-text-editor_manager.c` already implemented this correctly (lines 686-688):
```c
if (strcmp(layout, "editor.chtpm") == 0 && get_active_gui_is_typing()) {
    handle_interact_key(key);
    return 1;
}
```

**Signal:** `pieces/display/active_gui_is_typing.txt` is the reliable typing signal:
- `1` = text element actively being typed in (interact mode)
- `0` or missing = navigation mode (keys trigger commands)

**Syntax Check:** All three files pass `gcc -fsyntax-only` (no output = success).

---

### Pending / Next Steps

1. **Project directory rename** - `git mv 1.TPMOS_c_+rmmp.0103.0001 200.TPMOS_c_+rmmp.0103.0001` (from external shell)
2. **Windows manager builds** - cpp-llm (via `button.ps1 c`), groq-ollama, gem-dev, slop-ed-dev
3. **API endpoint tunneling** - `apis.txt` files point to `10.0.0.x` (local network Ollama/Gemini); unreachable from Windows without SSH/VPN
4. **Run verification** - Launch each manager, test interact mode in text fields
5. **Commit** - Stage only modified files (not runtime state), commit to `kilo` branch only