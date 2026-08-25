# TPMOS Bible Appendix: Frame Pipeline Debugging (Wraith ASCII/GL)
**Date:** 2026-07-06  
**Scope:** Never tell a virgin agent to "go check the frames" without explaining WHERE and HOW  
**Applies to:** Any project with ASCII + rasterized display modes (wraith, chtpm)

---

## The Mantra: "If it's not in current_frame.txt, it's not in the pixels"

When debugging a feature that works in **ASCII but not GL** (or vice versa), you need to trace the frame pipeline:

```
Parser (wraith_parser_alpha.c)
  ↓ writes
current_frame.txt (plain ASCII text, character grid)
  ↓ read by
wraith_rgb_daemon.c (rasterizer)
  ↓ writes
current_frame.rgba32 (pixel bitmap)
  ↓ loaded by
wraith_gl.c (texture display)
  ↓ shows in
GL Window (user sees pixels, not ASCII)
```

Each layer does exactly one thing; breakage usually means data loss between layers.

---

## Step 1: Verify What's Actually In current_frame.txt

Before assuming anything about rendering, **inspect the actual frame file**:

```bash
cat /path/to/pieces/display/current_frame.txt | grep "your_feature"
```

or from the frame **history** (which ALWAYS has the most recent frame):

```bash
# Find the latest frame in session history
tail -100 /path/to/pieces/debug/frames/session_frame_history.txt | grep -A 5 "your_feature"
```

**What you're checking:** Does the ASCII text that the **parser** wrote actually contain the characters/boxes/text you expect?

- If **YES** → problem is downstream (rgb daemon or GL rendering)
- If **NO** → problem is the parser itself (wraith_parser_alpha.c or chtpm_parser.c)

### Example: cli_io Input Boxes

Layout file has:
```xml
<cli_io id="edit_x" label="  X position" target_id="1" />
```

Parser should render as:
```
[>] 5. [hi_]  (where 'hi' is the input_buffer content)
```
or (if empty):
```
[>] 5. [     ]  (showing label or placeholder)
```

**Check:** Does `current_frame.txt` actually show `[hi_]` or `[ ... ]` at that position?

- If **yes** → the parser rendered it; problem is downstream rasterization
- If **no** → the parser never wrote it; check wraith_parser_alpha.c's cli_io rendering code

---

## Step 2: Check Session Frame History (Don't Tail The Live Frame)

The frame history file is **authoritative** and never in flux:

```bash
/path/to/pieces/debug/frames/session_frame_history.txt
```

Each frame entry is marked:
```
--- FRAME UPDATE at YYYY-MM-DD HH:MM:SS ---
```

Search backward through history to find the exact moment your feature was active:

```bash
grep -n "FRAME UPDATE\|your_feature" /path/to/session_frame_history.txt | tail -20
```

Then read around that line:
```bash
sed -n '950,1000p' /path/to/session_frame_history.txt
```

**Why not the live frame?** It can change mid-read, be mid-write, or get swapped by the parser while you're looking. History is frozen.

---

## Step 3: Understand Parser vs. Rasterizer Layers

### The Parser Layer (wraith_parser_alpha.c / chtpm_parser.c)
- **Reads:** Layout file (`.chtpm`)  
- **Reads:** UI state (gui_state.txt, cli_buffers.txt)  
- **Reads:** Keyboard input (history.txt)  
- **Writes:** current_frame.txt (ASCII text, pure character grid)  
- **What it renders:**
  - `<text label="...">` → literal text characters
  - `<button>` → `[>]` (focused) or `[ ]` (unfocused) + label text
  - `<cli_io>` → `[input_text_here_]` (the input box with buffer content)
  - Navigation indices (1., 2., 3., ...)
  - All rendered in character-grid positions (row/col)

### The Rasterizer Layer (wraith_rgb_daemon.c)
- **Reads:** current_frame.txt (the ASCII grid the parser wrote)  
- **Reads:** Font metrics (what size each character occupies in pixels)  
- **Writes:** current_frame.rgba32 (pixel bitmap)  
- **Does NOT do:**
  - Parse `.chtpm` layout files
  - Recognize `<cli_io>` or `<button>` tags
  - Make UI decisions (which element is focused, etc.)
- **What it does:**
  - Rasterizes every character in current_frame.txt to pixels
  - Creates hit-box metadata (current_frame.objects.pdl) mapping pixel regions to actions
  - Preserves all text, all characters, all spacing

### The Display Layer (wraith_gl.c)
- **Reads:** current_frame.rgba32 (pixel bitmap)  
- **Reads:** current_frame.objects.pdl (hit-box metadata from rasterizer)  
- **Does:**
  - Uploads the pixel bitmap as a GL texture
  - Renders the texture to the window
  - Translates mouse clicks to hit-box lookups
  - Translates keyboard input into the same history files the parser reads
- **Does NOT do:**
  - Parse UI elements
  - Render text (that's all in the rgba32 pixels already)
  - Make any layout decisions

---

## Step 4: Troubleshoot ASCII → GL Gap

**Symptom:** Works in ASCII, broken in GL (or vice versa)

### Most Common: Text/Box Missing in GL

**What to check:**

1. **Is it in current_frame.txt?**
   ```bash
   grep "your_text\|\[hi" /path/to/pieces/display/current_frame.txt
   ```
   - If **no** → parser didn't write it; fix the parser
   - If **yes** → continue to step 2

2. **Is wraith_rgb_daemon running and writing rgba32?**
   ```bash
   ls -ltr /path/to/projects/wraith-alpha/session/rgb/current_frame.rgba32
   stat /path/to/pieces/display/current_frame.objects.pdl
   ```
   - Timestamps should be recent (within last few seconds)
   - If they're old or missing → rgb daemon is dead; restart or debug daemon startup

3. **Check character encoding / rasterization quality:**
   - Some characters (unicode, wide chars, special box-draw) may not rasterize correctly
   - `[hi ]` should rasterize fine (ASCII 91, 104, 105, 32)
   - But if you see corruption in GL, the rasterizer may have a font/char bug

### Most Common: Hit-Box Mismatch (GL Clicks Land on Wrong Element)

This is a **rasterizer + GL geometry problem**, not a parser problem:

1. **Verify the parser placed the element at the right row/col**
   - Check current_frame.txt; count characters
   - Confirm navigation index lines up (e.g., `[>] 5. [hi_]` should be element #5)

2. **Verify wraith_rgb_daemon wrote hit-boxes correctly**
   ```bash
   cat /path/to/pieces/display/current_frame.objects.pdl | grep -A 2 "index.*5"
   ```
   - Should show pixel coordinates for that element
   - If missing → daemon didn't parse the element

3. **Verify GL coordinate math**
   - GL uses top-left origin (0,0 = top-left), while some systems use bottom-left
   - Click at (x, y) should hit the pixel region the rasterizer claims
   - Use on-screen debug overlay (if available) to visualize hit-boxes

---

## Step 5: The Marker File Pulse (How to Force a Re-render)

If you edit `current_frame.txt` directly for debugging, the daemon may not notice:

```bash
# Touch the marker to wake the daemon
echo "X" >> /path/to/pieces/display/frame_changed.txt
```

The daemon watches this file's size; when it grows, the daemon re-reads current_frame.txt and re-rasterizes.

---

## Step 6: Correlate Input → Frame Change

When you press a key in the GL window, the flow is:

```
GL Window (user presses key)
  ↓ appends to
pieces/keyboard/history.txt
  ↓ read by (on next pulse)
wraith_parser_alpha.c (processes key)
  ↓ updates gui_state.txt or rewrites current_frame.txt
  ↓ marks
frame_changed.txt (signals daemon)
  ↓ daemon reads and rasterizes
current_frame.rgba32 (updated pixels)
  ↓ loaded and displayed by
GL Window (shows new frame)
```

**To debug input lag or lost keystrokes:**

1. Inject a test key into `pieces/keyboard/history.txt` yourself
2. Wait one parser pulse (usually <100ms)
3. Check if current_frame.txt changed
4. If not → parser didn't see the key; check history.txt format
5. If yes → check if rgba32 timestamp updated (daemon processed it)
6. If not → daemon dead or not reading marker file

---

## Reference: Key File Locations

| File | Purpose | Who writes | Who reads |
|------|---------|-----------|----------|
| `.chtpm` layout | UI definition | Hand-edited | Parser |
| `current_frame.txt` | ASCII render output | Parser | Rasterizer |
| `frame_changed.txt` | Marker: "re-rasterize now" | Parser | Rasterizer (watches size) |
| `current_frame.rgba32` | Pixel bitmap | Rasterizer | GL renderer |
| `current_frame.objects.pdl` | Hit-box + action metadata | Rasterizer | GL renderer |
| `current_frame.meta.pdl` | Cell geometry (char size, etc.) | Rasterizer | (diagnostic) |
| `session_frame_history.txt` | Archived frames (read-only log) | Parser | (analysis only) |
| `gui_state.txt` | Live UI state (focus, inputs) | Parser | Manager, next parser cycle |
| `cli_buffers.txt` | Persisted text input buffers | Parser | Parser (restore on reparse) |
| `pieces/keyboard/history.txt` | Input events | GL + keyboard daemon | Parser |

---

## Golden Rule for Frame Debugging

**Never assume the next layer did what you think.** Always verify at the **boundary:**

- **Parser working?** Check `current_frame.txt` contains your text.
- **Rasterizer working?** Check `current_frame.rgba32` timestamp and size.
- **GL working?** Check pixel data with a screenshot or pixel-peek tool.

This is the **only way** to isolate which layer broke.
