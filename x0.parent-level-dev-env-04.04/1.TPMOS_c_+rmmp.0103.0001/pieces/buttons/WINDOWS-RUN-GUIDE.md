# Windows Execution Guide for CHTPM+OS
# Date: 2026-04-02
# Status: WORKING with Known Limitations

## The Problem: .+x File Extension

On Linux, files with `.+x` extension are executed directly because the shell doesn't care about extensions.

On Windows, the `.+x` extension is **not recognized**, so you get a dialog asking "How do you want to open this file?"

## Solution: Use cmd.exe /c with Full Paths

All provided batch scripts now use this pattern:

```batch
cmd.exe /c "C:\full\path\to\binary.+x"
```

This works because `cmd.exe /c` executes the file directly without needing a file association.

## Quick Start

### Test GL-OS Desktop (OpenGL) - WORKING ✓

```cmd
.\win_debug.bat
```

This opens an OpenGL window with the GL-OS desktop.

### Test Full CHTPM Pipeline

```cmd
.\run_chtpm_debug.bat
```

This shows what's being launched and starts the orchestrator.

### Standard Launch

```cmd
.\run_chtpm.bat
```

Clean launch without debug output.

## Optional: Register .+x Extension

If you want to run `.+x` files directly without wrappers:

```cmd
.\register_plus_x.bat
```

**Requires Administrator privileges.** This registers `. +x` as an executable file type system-wide.

After registration, you can run:

```cmd
.\pieces\chtpm\plugins\+x\orchestrator.+x
```

directly from any command prompt.

## Known Limitations on Windows

### LIMITATION 1: Arrow Keys in MSYS2/mintty - NOT WORKING

**Symptom:** Physical arrow keys don't work when running via MSYS2/mintty bash.

**Root Cause:** `_kbhit()` doesn't work in pseudo-terminals (mintty), only in native Windows console.

**Workaround:** Use number keys (1-9) for menu navigation instead of arrow keys.

**Status:** Fix attempted with `fgetc()` fallback and escape sequence parsing, but still broken. May require running in native Windows console (not MSYS2 bash).

**Files Affected:** `pieces/chtpm/plugins/orchestrator.c` (readKey function)

**Reference:** PITFALLS_ACTIVE_2026-03-18.txt #44

---

### LIMITATION 2: XInput Controller Support - UNTESTED

**Symptom:** Physical Xbox controller input not verified.

**Root Cause:** XInput code implemented (`joystick_input_win.c`) but not tested with actual hardware.

**Status:** Code compiles successfully, requires physical Xbox controller for verification.

**Workaround:** Use keyboard input (number keys work for navigation).

**Files Affected:** `pieces/joystick/plugins/joystick_input_win.c`

**Reference:** PITFALLS_ACTIVE_2026-03-18.txt #45

---

### LIMITATION 3: Process Spawning Differences

**Symptom:** Child processes may not spawn correctly if not using batch wrappers.

**Root Cause:** Windows uses `_spawnl(_P_DETACH)` instead of `fork()/exec()`.

**Workaround:** Always use provided batch scripts (`win_debug.bat`, `run_chtpm.bat`, etc.)

**Files Affected:** `pieces/chtpm/plugins/orchestrator.c`

---

## How The System Works

The CHTPM+OS runtime follows this pipeline:

1. **orchestrator.+x** starts and spawns child processes:
   - **renderer.+x** - Reads `current_frame.txt` and displays it
   - **gl_renderer.+x** - OpenGL renderer (if enabled)
   - **clock_daemon.+x** - Updates game time
   - **chtpm_parser.+x** - Parses layouts, handles input
   - **response_handler.+x** - Handles system responses
   - **joystick_input.+x** - Gamepad support

2. **Keyboard input** goes to `pieces/keyboard/history.txt`

3. **chtpm_parser** reads input, updates layout, writes to `pieces/display/current_frame.txt`

4. **renderer** reads `current_frame.txt` and prints to console

5. **GL renderer** (if running) shows OpenGL window with same content

## Troubleshooting

### "This command cannot be run completely"

Make sure you're using the batch wrappers (`win_debug.bat`, etc.) which use `cmd.exe /c` with full paths.

### "File not found"

Run `compile_all.ps1` first to build all binaries.

### "OpenGL not found"

Run the automated dependency installer from the root directory:
```powershell
.\install_deps.ps1
```

Or install MSYS2 with freeglut manually:
```bash
pacman -S mingw-w64-x86_64-freeglut
```

### "Child processes not spawning"

The orchestrator uses `_spawnl(_P_DETACH)` on Windows to spawn child processes. Check Task Manager to verify they're running.

## Architecture Notes

### Linux vs Windows Process Spawning

| Linux | Windows |
|-------|---------|
| `fork()` + `exec()` | `_spawnl(_P_DETACH)` |
| Child inherits cwd | Must `_chdir()` before spawn |
| Easy I/O redirect | Hard (requires handles) |

### File-Based IPC

All communication happens via files:
- `pieces/keyboard/history.txt` - Input events
- `pieces/display/current_frame.txt` - Rendered frame
- `pieces/master_ledger/master_ledger.txt` - System audit log
- `pieces/apps/*/history.txt` - Per-app input

This is the **True Piece Method (TPM)** - "If it's not in a file, it's a lie."

## Contact

See `README.md` for full documentation.
See `#.docs/` for detailed standards.
