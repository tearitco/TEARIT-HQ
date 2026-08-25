# Windows Text Renderer Documentation
# Document: windows_renderer.md
# Date: March 31, 2026
# Status: IMPLEMENTED

================================================================================
## OVERVIEW
================================================================================

`windows_renderer.c` is the Windows-specific text renderer for CHTPM+OS. It
displays the ASCII frame UI in PowerShell/CMD terminals, providing the same
functionality as `renderer.c` on Linux but using Windows Console API.

================================================================================
## ARCHITECTURE: PLATFORM-SPECIFIC RENDERERS
================================================================================

CHTPM+OS uses modular, platform-specific renderers:

```
Linux:  pieces/display/renderer.c         -> renderer.+x
Windows: pieces/display/windows_renderer.c -> renderer.+x
```

Both compile to the same binary name (`renderer.+x`), so the orchestrator and
other components don't need platform-specific modifications.

### Why Separate Files?

Terminal handling is fundamentally different between platforms:

| Feature | Linux | Windows |
|---------|-------|---------|
| Screen clear | ANSI escape (`\033[H\033[J`) | Console API (`FillConsoleOutputCharacter`) |
| Sleep | `usleep()` | `Sleep()` |
| Console mode | termios.h | windows.h (GetConsoleMode/SetConsoleMode) |
| Path separator | `/` | `\\` |

Keeping them separate:
- Avoids `#ifdef _WIN32` clutter in source files
- Makes platform-specific debugging easier
- Allows independent optimization
- Follows existing CHTPM+OS modular patterns (win_compat.h, win_spawn.h)

### Orchestrator Integration

The orchestrator handles rendering differently on each platform:

**Linux:**
```c
/* fork()+exec() automatically inherits stdout/stderr */
launch_and_register("renderer", "pieces/display/plugins/+x/renderer.+x", NULL, false);
```

**Windows:**
```c
/* Render directly in orchestrator thread */
/* Child processes don't inherit console stdout on Windows */
void* render_thread_func(void* arg) {
    /* Read current_frame.txt and print to console */
    /* Poll renderer_pulse.txt for changes */
    /* Clear screen using Windows Console API */
    while (!should_exit) {
        if (pulse_changed()) {
            clear_screen_win32();
            print_frame();
        }
        Sleep(17);  /* ~60 FPS */
    }
}
```

**Why does Windows render inside the orchestrator?**

On Linux, `fork()` creates a child process that inherits all file descriptors from the parent, including stdout. The external `renderer.+x` process can `printf()` and it appears in the same terminal.

On Windows, `_spawnl()` creates a process that does NOT inherit the parent's console stdout handle - even with `_P_NOWAIT`. The child's output goes to a separate buffer that isn't visible.

**Solution:** The orchestrator's render thread handles display rendering directly on Windows, reading `current_frame.txt` and printing to its own console. This maintains the modular `windows_renderer.c` for the rendering logic while ensuring output appears in the correct terminal.

**Architecture Summary:**
| Platform | Renderer Binary | Render Location |
|----------|----------------|-----------------|
| Linux | `renderer.+x` | External process (fork inherits stdout) |
| Windows | `renderer.+x` | Internal thread (orchestrator has console) |

The `windows_renderer.c` still exists and compiles to `renderer.+x` - it's used for:
- Standalone testing
- Future use if console inheritance is fixed
- Reference implementation for the render loop logic

### Code Convention

The CHTPM+OS codebase follows these conventions for platform-specific code:

1. **Modular separation** - Separate source files when behavior differs significantly
2. **Same output name** - Both platforms produce `renderer.+x`
3. **Minimal #ifdef** - Use `#ifdef _WIN32` only for spawn/integration logic
4. **Mirror structure** - Same function names, same flow, different implementation

================================================================================
## HOW IT WORKS
================================================================================

### Main Loop

```
1. Clear session frame history
2. Render initial frame from current_frame.txt
3. Loop forever:
   a. Check renderer_pulse.txt for changes (stat())
   b. If file size changed:
      - Read current_frame.txt
      - Clear screen (or add separator if history mode)
      - Print frame content
      - Log to ledger files
   c. Sleep 17ms (~60 FPS)
```

### Key Functions

**clear_screen()**
- Uses Windows Console API to fill screen with spaces
- Restores cursor to (0,0)
- Preserves original console attributes

**is_history_on()**
- Reads `pieces/display/state.txt`
- Returns 1 if history mode (scrolling output), 0 if static UI (clear screen)

**render_display()**
- Gets timestamp
- Clears screen or adds separator based on history mode
- Reads and prints `current_frame.txt`
- Logs to display ledger, master ledger, and session history

================================================================================
## COMPILATION
================================================================================

Compiled automatically by `compile_all.ps1` on Windows:

```powershell
.\button.ps1 compile
```

This compiles `windows_renderer.c` to `pieces/display/plugins/+x/renderer.+x`

The Linux `renderer.c` is NOT compiled on Windows.

================================================================================
## USAGE
================================================================================

The renderer is spawned automatically by the orchestrator when you run:

```powershell
.\button.ps1 run
```

You don't need to run it manually.

### Manual Testing (Optional)

To test the renderer standalone:

```powershell
# Create a test frame
echo "Test Frame" > pieces/display/current_frame.txt
echo "pulse" > pieces/display/renderer_pulse.txt

# Run renderer
.\pieces\display\plugins\+x\renderer.+x
```

Press Ctrl+C to stop.

================================================================================
## FILE LOCATIONS
================================================================================

**Source:**
- `pieces/display/windows_renderer.c`

**Binary (after compile):**
- `pieces/display/plugins/+x/renderer.+x`

**Runtime Files:**
- `pieces/display/current_frame.txt` - Frame content to display
- `pieces/display/renderer_pulse.txt` - Trigger file (size changes = new frame)
- `pieces/display/state.txt` - "on" or "off" (history mode toggle)
- `pieces/display/ledger.txt` - Render log
- `pieces/debug/frames/session_frame_history.txt` - Frame history for debugging

================================================================================
## CONFIGURATION
================================================================================

### Toggle Frame History

To switch between scrolling output and static UI:

1. In the CHTPM+OS menu, select "Toggle Frame History"
2. Or manually edit `pieces/display/state.txt`:
   - "on" = History mode (scrolling, frames separated by blank lines)
   - "off" = Static UI (clear screen each frame)

### Frame Rate

Default: ~60 FPS (17ms sleep)

To change, edit the `Sleep(17)` call in `windows_renderer.c`:
- Lower value = higher FPS (more CPU)
- Higher value = lower FPS (less CPU)

================================================================================
## TROUBLESHOOTING
================================================================================

### Issue: Renderer doesn't display anything

**Check:**
1. Is `current_frame.txt` being created?
   ```powershell
   Get-Content pieces/display/current_frame.txt
   ```
2. Is the orchestrator running?
   ```powershell
   Get-Process | Where-Object {$_.Path -like "*orchestrator*"}
   ```
3. Check renderer ledger for errors:
   ```powershell
   Get-Content pieces/display/ledger.txt
   ```

### Issue: Screen flickers

**Cause:** History mode is off, screen clears every frame.

**Fix:** Enable history mode in the CHTPM+OS menu.

### Issue: Output is garbled

**Cause:** Console encoding or buffer issues.

**Fix:**
1. Try running in a fresh PowerShell window
2. Check console font (use Consolas or Lucida Console)
3. Increase console buffer size

### Issue: Renderer exits immediately

**Check:**
1. Is `renderer_pulse.txt` accessible?
2. Are there any error messages in the terminal?
3. Run from PowerShell (not CMD or WSL)

================================================================================
## COMPARISON: LINUX VS WINDOWS RENDERERS
================================================================================

### renderer.c (Linux)

```c
// Screen clear
printf("\033[H\033[J");

// Sleep
usleep(16667);

// Console mode
tcsetattr(tty_fd, TCSAFLUSH, &raw);
```

### windows_renderer.c (Windows)

```c
// Screen clear
FillConsoleOutputCharacterA(h_console, ' ', dwConSize, coordScreen, &cCharsWritten);
FillConsoleOutputAttribute(h_console, orig_csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
SetConsoleCursorPosition(h_console, coordScreen);

// Sleep
Sleep(17);

// Console mode
GetConsoleMode(h_console_input, &orig_console_mode);
SetConsoleMode(h_console_input, mode);
```

================================================================================
## DEVELOPMENT NOTES
================================================================================

### Adding New Features

If you add a feature to one renderer, consider adding it to the other:
- Frame timestamp display
- Ledger logging
- Session history
- History mode toggle

### Testing Changes

1. Compile: `.\button.ps1 compile`
2. Run: `.\button.ps1 run`
3. Observe terminal output
4. Check ledger files for errors

### Code Style

- Use `static` for internal functions
- Prefix globals with clear names
- Comment Windows API calls
- Keep functions small and focused

================================================================================
## SEE ALSO
================================================================================

- `#.docs/#.piece.mark.c-look_0.txt` - Expected terminal output example
- `win-butt-prog-rep.txt` - Windows button progress report
- `#.buttons/windows_linux_parity.txt` - Platform parity analysis
- `pieces/display/renderer.c` - Linux renderer source
- `pieces/display/gl_renderer.c` - OpenGL renderer (separate window)

================================================================================
# END OF DOCUMENTATION
================================================================================
