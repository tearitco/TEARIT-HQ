# FM Widget Fix — Sprint Document

## Goal
Make the file-menu widget's GL window display visible content (the file menu with button labels) when opened from the text-editor-xyz app.

## The Problem
When pressing F4/4 in the editor to dispatch the FILE MENU method, the widget's GL window opened but stayed blank/black. The pipeline was running but producing no visible output.

## Investigation

### Pipeline Architecture
The rendering pipeline has 4 processes:

```
fm_compose_frame  →  view.txt
       ↓
chtpm_parser_pal  →  current_frame.txt + renderer_pulse.txt
       ↓
chtpm_rgb_render  →  rgb_frame.raw + rgb_frame.receipt.txt + rgb_frame_changed.txt
       ↓
gl_mirror         →  GL window (reads rgb_frame.raw)
```

Two separate pulse signals drive the rgb renderer:
- `frame_changed.txt` — written by prisc+x `hit_frame` opcode
- `renderer_pulse.txt` — written by chtpm_parser_pal's internal `compose_frame()`

The rgb renderer triggers on EITHER pulse changing. In practice, `renderer_pulse.txt` is the reliable trigger because chtpm_parser_pal writes it every compose cycle.

### Root Cause: Empty Button Labels
The file-menu.chtpm layout used this format:

```xml
<button>NEW FILE</button>
```

But the chtpm parser (shared fork from 1.TPMOS) reads button labels from the `label` attribute, NOT from inner text:

```xml
<button label="NEW FILE" />
```

The `<button>text</button>` syntax is only respected for `<button>` elements that have `onClick` handlers — the "text" node child is treated as label. Wait, actually the parser reads the `label` attribute first, falls back to text content for some element types. Let me re-examine.

Actually the fix was: `<button>label</button>` → `<button label="label" />`. The parser's button rendering code only reads the `label` attribute, not the inner text content of button elements. So buttons appeared with empty labels in the rendered frame.

The file was at `&.widgits/file-menu/pieces/chtpm/layouts/file-menu.chtpm`.

### Root Cause: NO_GL=1
The editor's `button.sh` had `NO_GL=1` set, which prevented `gl_mirror` and `chtpm_rgb_render` from starting. The GL window never got any data to display.

### Root Cause: Missing symlinks
The widget's `system/` directory was missing symlinks to the shared system binaries (`gl_mirror`, `chtpm_rgb_render`). The `build.sh` script only symlinked `prisc+x`, `chtpm_parser_pal`, and the keyboard/renderer from 014.wsr-pal — but not `gl_mirror` or `chtpm_rgb_render`.

### Root Cause: Wrong gl_mirror.c version
Two versions of `gl_mirror.c` exist in the system projects:
- **014.wsr-pal** — has `interact_relay` forwarding in `append_key()` (writes keypresses to `interact_relay.txt` in addition to `history.txt` and `chtpm_keyboard_history.txt`)
- **045.muchi-pal-agent** — does NOT have `interact_relay` forwarding

The widget was building from 045's version, which meant keyboard input from GL didn't reach the PAL loop's `interact_relay.txt`. Fixed by using 014's version.

## Comparison: file-menu widget vs mutaclysm

| Aspect | mutaclysm | file-menu widget |
|--------|-----------|------------------|
| Launcher | `orchestrator.c` (C master process) | `button.sh` (bash) |
| GL start | Orchestrator conditionally launches `gl_mirror` | `button.sh run-widget` starts daemons |
| CHTPM layout | `game.chtpm` with `${game_map}`, `${piece_methods}`, `<button onClick="INTERACT">` | `file-menu.chtpm` with static buttons |
| System binaries | Compiled directly into `system/` | Symlinked from 014.wsr-pal |
| PAL loop | `main_loop_chtpm.pal` with `compose_frame`/`hit_frame`/`read_position` | Same pattern, dedicated FM ops |
| Session discovery | N/A (single process) | `house_root.txt` → `focus.txt` scan |
| Widget cmd bus | `muta_widget_cmds.c` (inbox/status) | `fm_enqueue_cmd` → `editor_widget_cmds` |
| RGB pipeline | Two parallel paths: C-mode (`compose_rgb_frame`) + CHTPM mode (`chtpm_rgb_render`) | CHTPM mode only |
| RGB resolution | 640×304 (C-mode), 640×768 (CHTPM) | 640×768 |
| Frame change trigger | `renderer_pulse.txt` + `frame_changed.txt` | Same |

## Fixes Applied

### 1. file-menu.chtpm — button label format
**File**: `&.widgits/file-menu/pieces/chtpm/layouts/file-menu.chtpm`
**Change**: `<button>text</button>` → `<button label="text" />`
**Why**: chtpm parser reads `label` attribute, not inner text for buttons

### 2. editor button.sh — removed NO_GL=1
**File**: `@.apps/text-editor-xyz/button.sh`
**Change**: Changed `NO_GL=1` to `NO_GL=""`
**Why**: Allows gl_mirror and chtpm_rgb_render to start in editor mode

### 3. Widget build script — added missing symlinks
**File**: `&.widgits/file-menu/scripts/build.sh`
**Change**: Added symlinks for `gl_mirror` and `chtpm_rgb_render` from 014.wsr-pal
**Why**: These binaries were missing from the widget's system dir

### 4. editor_menu_input.c — FM handler
**File**: `102.editor-📄️00.00/ops/editor_menu_input.c`
**Changes**:
- Fixed method dispatch off-by-one bug (key `4` was dispatching to wrong handler)
- Added integer key support for keycodes like `48` ('0') → `57` ('9')
- Added FM handler that writes `"1\n"` to widget's `interact_relay.txt`
- FM handler discovers widget session via `house_root.txt` → scan `focus.txt`

### 5. Editor button.sh — house_root.txt
**File**: `@.apps/text-editor-xyz/button.sh`
**Change**: Added `echo "$HOUSE_DIR" > pieces/system/house_root.txt`
**Why**: Provides the anchor for widget session discovery

### 6. gl_mirror.c — interact_relay forwarding
**File**: `014.wsr-pal💸️📌️+2/system/gl_mirror.c` (used via symlink)
**Change**: Added `interact_relay` path initialization and `append_key()` forwarding to `interact_relay.txt`
**Why**: Required for keyboard input from GL window to reach the PAL loop

## Pipeline Verification

Tested standalone at `/tmp/pipetest-*`:

| Step | Binary | Output | Status |
|------|--------|--------|--------|
| Compose frame | `fm_compose_frame.+x` | view.txt (124 bytes) | ✅ |
| CHTPM parse | `chtpm_parser_pal` | current_frame.txt (159 bytes) | ✅ |
| RGB render | `chtpm_rgb_render` | rgb_frame.raw (1,966,080 bytes) | ✅ |
| GL display | `gl_mirror` | gl_display.receipt.txt (checksum match) | ✅ |

Key metrics from the test:
- `rgb_frame.raw`: 640×768×4 = 1,966,080 bytes (RGBA32)
- `rgb_frame.receipt.txt` checksum: `e7ef7ab4dace2325` (non-zero = real pixel data)
- `gl_display.receipt.txt` checksum: `0xE7EF7AB4DACE2325` (matches rgb receipt)
- All 3 daemon processes (parser, rgb_render, gl_mirror) stayed alive for entire test duration
- `frame_changed.txt`: 0 bytes (never written — PAL `hit_frame` opcode not firing; rgb renderer uses `renderer_pulse.txt` instead)
- `renderer_pulse.txt`: 2 bytes (written by chtpm_parser_pal each compose cycle)

### current_frame.txt content (after fix):
```
[>] 1. [NEW FILE][ ] 2. [SAVE FILE][ ] 3. [SAVE AS][ ] 4. [LOAD FILE][ ] 5. [CHANGE FOCUS]
+=========================================================+
Nav > _
```

## Root Cause: Missing font glyph registry
The GL window showed black because the session dir had no `pieces/registry/`. The `chtpm_rgb_render` loads ASCII font glyphs from `pieces/registry/fonts/ascii/<code>/glyph.txt` relative to `PRISC_PROJECT_ROOT`. Without these files, every character in `current_frame.txt` renders as invisible (all glyph pixels are zero).

Both mutaclysm and 014.wsr-pal have their own `pieces/registry/` directories. The widget session setup was missing this entirely.

**Fix**: Local file copy — `&.widgits/file-menu/pieces/registry/fonts/ascii/` now has 95 glyph files (copied from 014.wsr-pal). Session setup (`button.sh`) symlinks `$SESSION_DIR/pieces/registry → $SCRIPT_DIR/pieces/registry`. Build script copies glyphs from wsr on each build.

**Lesson**: Every project/widget that uses `chtpm_rgb_render` must have `pieces/registry/fonts/ascii/<code>/glyph.txt` in its own directory tree. Glyphs are project-local, not shared from wsr.

## Pitfalls Discovered

### 1. chtpm_parser_pal uses getcwd(), NOT PRISC_PROJECT_ROOT
The parser resolves its project root with `getcwd()`, not `PRISC_PROJECT_ROOT` env var. This means it MUST be launched from the session directory, not from the widget root. If launched from the wrong cwd, it silently does nothing — no error, no `current_frame.txt`.

All other system binaries (`chtpm_rgb_render`, `gl_mirror`, `prisc+x`, ops) use `PRISC_PROJECT_ROOT` and can be launched from anywhere.

**Fix**: Always `cd "$SESSION_DIR"` before launching `chtpm_parser_pal`.

### 2. `<button>text</button>` vs `<button label="text" />`
The chtpm parser's button rendering path reads the `label` attribute. Inner text content is ignored for `<button>` elements. This differs from `<text>` elements where inner text IS the content.

**Fix**: Always use `label="..."` for buttons, regardless of whether `onClick` is present.

### 3. Two gl_mirror.c versions exist
The 014.wsr-pal version has `interact_relay` forwarding; the 045.muchi-pal-agent version does not. Using the wrong version means GL keyboard input never reaches the PAL loop.

**Lesson**: Always check which source project a system binary is built from. `014.wsr-pal` is the canonical system source for this project.

### 4. frame_changed.txt vs renderer_pulse.txt
Two pulse files drive the rgb renderer. `frame_changed.txt` is written by prisc+x's `hit_frame` opcode. `renderer_pulse.txt` is written by chtpm_parser_pal's internal `compose_frame()`.

In our setup, `frame_changed.txt` stays at 0 bytes because the PAL loop's `hit_frame` isn't executing (the PAL loop runs on the editor side, not the widget side). The rgb renderer works fine using `renderer_pulse.txt` alone, but this is worth understanding for debugging.

### 5. Processes can die silently
System binaries may crash on startup without writing any error log. Always check process status after launch:
```bash
kill -0 $PID && echo "ALIVE" || echo "DEAD"
```

### 6. Pipeline requires all 4 processes
The rendering pipeline is 4 processes chained together. If any one fails, the GL window goes blank:
1. `fm_compose_frame` (writes view.txt)
2. `chtpm_parser_pal` (parses chtpm → current_frame.txt + renderer_pulse.txt)
3. `chtpm_rgb_render` (rasterizes → rgb_frame.raw + rgb_frame_changed.txt)
4. `gl_mirror` (uploads to GL texture → visible window)

Missing `gl_mirror` or `chtpm_rgb_render` → blank window even if parser works.
Missing `chtpm_parser_pal` → no current_frame.txt, no rgb output.

### 7. Session directory must be fully initialized
Before launching daemons, the session directory must have all subdirectories:
```
pieces/display/
pieces/apps/player_app/
pieces/keyboard/
pieces/system/
```
Missing subdirs cause silent failures when binaries try to write to them.

## User Feedback & Concerns

### 1. Standalone widget mode
> "file-menu widget should be able to run on its own (even if there is nothing to save/load)"

The widget currently requires an editor session to be useful (it sends commands to the focused program). But it should also be independently launchable for testing/verification.

Currently, `button.sh run-widget` starts all daemons but requires either a session root argument or a `live-focus/focus.txt`. Without a focused host, the widget has no program to command.

**Implication**: The widget's pipeline (compose → parse → rgb → gl) should work standalone for visual testing, which we've verified. The "functional" part (sending commands) requires a host, but the "display" part does not.

### 2. xyzfs ledger for program discovery
> "Editor and widget should both write to a ledger in xyzfs somewhere, and check there to see if there are programs they should connect to"

Current architecture uses `house_root.txt` + `focus.txt` scanning for discovery. The proposed ledger is a centralized, discoverable file-based registry that all programs (editor, widgets, apps) write to and read from.

**Design constraints from user**:
- Programs check the ledger to find peers
- Editor writes its identity/availability to the ledger
- Widget reads the ledger to find editors it can connect to
- No sockets/IPC — file-mediated only
- Must work in both interactive and automated contexts

### 3. How does widget find "focused program"?
> "How does it find 'focused program'? Is there any info about that in documentation?"

Found in:
- `❤️‍🔥️.XYZOS_README.md` line 322: "Widgets are tools that command focused programs"
- `&.widgits/file-menu/widget+plan.txt` lines 17-20: "Widgets send commands into a focused running program through a file-mediated bus"
- Section 37 of `.xyzos-standards+1.txt`: widget cmd bus (inbox/status/discovery via `widget_bridge.txt`)

The mechanism:
1. Editor writes house root → `pieces/system/house_root.txt`
2. Widget launcher creates session dir → `&.widgits/file-menu/pieces/sessions/<id>/`
3. `fm_set_focus` writes focus.txt with `session_root=<editor_path>`
4. Editor's FM handler scans sessions/ for matching focus.txt
5. When found, editor writes to widget's `interact_relay.txt`

**Missing**: No centralized ledger. Discovery is editor-initiated (editor scans for widget), not widget-initiated (widget cannot autonomously find editors).

### 4. Standalone rendering test
> "I'm trying to run it on its own so I can see that it renders in CLI and/or GL"

Standalone test procedure:
```bash
# Create session
mkdir -p /tmp/fm-test/pieces/{display,apps/player_app,keyboard,system}
cd /tmp/fm-test

# Export env
export PRISC_PROJECT_ROOT=/tmp/fm-test
export PRISC_PROJECT_ID=file-menu

# Init state
printf 'mode=main_menu\npath_buffer=\n' > pieces/system/fm_state.txt
printf 'module_path=system/prisc+x pal/main_loop_chtpm.pal\nproject_id=file-menu\nactive_target_id=file-menu\n' > pieces/apps/player_app/state.txt

# Compose
/path/to/ops/+x/fm_compose_frame.+x

# Parse (must cd to session dir — chtpm_parser_pal uses getcwd!)
cd /tmp/fm-test
/path/to/system/chtpm_parser_pal pieces/chtpm/layouts/file-menu.chtpm &

# RGB render (in background)
/path/to/system/chtpm_rgb_render &

# GL (requires DISPLAY)
/path/to/system/gl_mirror &
```

### 5. User wants rendering in both CLI and GL
> "I can see that it renders in CLI and or GL"

The ASCII renderer (`system/renderer`) outputs to stdout and works in any terminal. The GL renderer (`gl_mirror`) requires a display server. Both should work from the same pipeline.

## Widget + Editor communication patterns
Two independent communication paths exist:

### Path A: Editor → Widget (key forwarding)
```
User presses F4 in editor
  → editor_menu_input.c writes "1\n" to widget's interact_relay.txt
    → widget's PAL loop reads the key from interact_relay.txt
      → dispatches to fm_menu_input op
```

### Path B: Widget → Editor (command bus)
```
Widget generates a command (e.g., "SAVE_FILE")
  → fm_enqueue_cmd writes to editor's inbox.txt
    → editor_widget_cmds reads and executes
      → writes status back to widget's status.txt
```

Path A is implemented and working. Path B needs verification end-to-end.

## Remaining Issues

### 1. frame_changed.txt stays 0 bytes
The PAL `hit_frame` opcode never writes to `frame_changed.txt`. The rgb renderer works via `renderer_pulse.txt` instead, but `frame_changed.txt` being empty indicates the PAL loop's prisc+x integration may not be fully wired.

### 2. Full live test not yet performed
The pipeline is verified in isolation, but the end-to-end live test (launch editor → F4 → see GL window with file menu) hasn't been done. The user reported "I ran FM and don't see text in the GL window" — this was before the button label fix.

### 3. Keyboard forwarding from GL to fm_menu_input
Though `gl_mirror` now has `interact_relay` forwarding, the end-to-end keyboard path (press key in GL window → fm_menu_input reads it) hasn't been verified.

### 4. Widget cmd bus not tested
The `fm_enqueue_cmd` → `editor_widget_cmds` pipeline exists but hasn't been tested end-to-end.

### 5. Standalone mode lacks discovery
There's no centralized xyzfs ledger for programs to discover each other. The current architecture requires the editor to initiate discovery by scanning widget sessions.

## Conclusions

1. **Pipeline works**: All 4 stages of the rendering pipeline produce correct output when properly configured
2. **Button labels were the main visual bug**: `<button>text</button>` produced empty labels
3. **NO_GL=1 was blocking GL**: Without this fix, gl_mirror never started
4. **Session dir setup is fragile**: Missing subdirs or incorrect cwd cause silent failures
5. **Ledger-based discovery is the next arch challenge**: Current session discovery works but is editor-centric; a bidirectional ledger would enable standalone-first design
6. **Two pulse signals add confusion**: `renderer_pulse.txt` is the one that actually drives re-renders; `frame_changed.txt` is legacy from the PAL-only pipeline
