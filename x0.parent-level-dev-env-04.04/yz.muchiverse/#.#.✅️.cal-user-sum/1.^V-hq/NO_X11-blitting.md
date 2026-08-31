# Separating X11 Blitting from Portable Rendering Logic

**Status**: Deferred architectural pattern (NOT currently scheduled for immediate refactoring).
**Purpose**: Document an existing partial pattern in this codebase, so a future no-X11 or non-X11 target can reuse compose logic without re-deriving this analysis.

## Goal

Separate the rendering pipeline into two strictly independent stages:

1. **Compose stage** (portable): Write RGBA pixels into a plain byte buffer in portable C, with zero dependency on any windowing system API. This is where the real, complex drawing logic lives (raycasting, geometry, layering, text rendering, alpha-blending).

2. **Present stage** (platform-specific): Take a completed RGBA buffer and display it on screen via the target platform's native API (X11/XPutImage, Linux framebuffer, DRM/KMS, a hypothetical future custom OS, etc.). This stage is where X11 coupling, GL calls, or platform-specific blitting belongs.

**The insight**: If compose is truly separated from present, then porting this codebase to run without X11 only requires writing a new present-stage backend — the compose logic (typically 80-95% of the real work) can be reused unchanged.


## Current Real Precedent: The File-Handoff Pattern

The closest example of this separation already exists in this codebase: the **3D board-viewer overlay pattern**.

**Location**: `khtpm_entity_menu_render.c:run_pchq_board_mode()` at lines 10269-10399.

**How it works**:

1. **Composer process** (separate binary): `board-viewer` runs `bv_render_3d.+x`, which:
   - Performs a per-pixel DDA raymarcher (portable C, zero X11 calls)
   - Reads voxel grids, camera settings, lighting from config files
   - Writes raw RGBA32 pixels directly into a plain `unsigned char fb[FRAME_H][FRAME_W][4]` buffer (see `bv_render_3d.c:1168`)
   - Writes ONLY this byte buffer to disk: `pieces/display/rgb_frame_3d_overlay.raw` (see `bv_render_3d.c:2275`)

2. **Presenter process** (separate binary): `khtpm_entity_menu_render` reads the file and:
   - Opens an X11 display and window
   - Reads the raw RGBA file
   - Uses `XPutImage()` to blit the buffer to the X11 window
   - Discards and re-reads the file on every frame

**Why this is the model to generalize**: These are two completely independent processes. The compose step has ZERO knowledge of X11. The present step has ZERO knowledge of raymarching. They communicate only through a plain file containing raw bytes.

**Real code citations**:
- Compose: `&.widgits/board-viewer/ops/bv_render_3d.c:1168` (buffer declaration), `:1239` and `:2265` (writing pixels via plain array indexing), `:2275` (writing to raw file)
- Present: `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c:10328-10329` (file paths), `:10335-10388` (X11 window setup), later lines doing `XPutImage()`


## Current Real Counter-Example: X11-Coupled Compose Logic

Not all rendering functions in this codebase currently follow the clean separation. The most prominent counter-example is also in the same codebase:

**Location**: `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c:draw_sprite_rgb()` at lines 1402-1420.

**What it does**: Composes per-pixel alpha-blended sprites, but embeds X11 calls inside the loop:

```c
static void draw_sprite_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;
    for (int y = 0; y < WIN_PX; y++) {
        int sy = (y * g_sprite_res) / WIN_PX;
        if (sy >= g_sprite_res) sy = g_sprite_res - 1;
        for (int x = 0; x < WIN_PX; x++) {
            int sx = (x * g_sprite_res) / WIN_PX;
            if (sx >= g_sprite_res) sx = g_sprite_res - 1;
            unsigned char *p = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            XSetForeground(dpy, gc, ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, y);  // <-- X11 call INSIDE the pixel loop
        }
    }
}
```

**The problem**: Every pixel write is an actual X11 function call (XSetForeground + XDrawPoint). This cannot be ported to any non-X11 target without completely rewriting the function. The alpha-blending math (the real algorithmic work) is locked to X11.

**Why it's harder to port**: A no-X11 target would need to:
1. Allocate a plain RGBA buffer
2. Rewrite the entire function to write `buffer[y][x] = {r, g, b, 255}` instead of XDrawPoint
3. Call a separate present function to display that buffer

The algorithmic work (alpha blending) could be reused, but the X11-coupled structure forces a rewrite.

**Compare to the file-handoff pattern**: The raymarcher in `bv_render_3d.c` does exactly the alpha-blending-level work (lighting, texture sampling, color interpolation), but writes to `fb[y][x][0..3]` — plain array indexing, zero X11 calls. Porting to a new platform only requires changing the final `fwrite(fb, ...)` to write to a different target.


## Another Existing Pattern: Offscreen Compose -> X11 Present

The same codebase has a semi-separated pattern that's closer to the goal:

**Location**: `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c:3680-3692` (the "Present:" comment and loop).

**Pattern**:
1. All drawing operations write to an offscreen X11 Pixmap (`g_buf`)
2. At the end of each frame, capture that Pixmap:
   ```c
   XImage *frame = XGetImage(dpy, g_buf, 0, 0, WIN_PX, WIN_PX, AllPlanes, ZPixmap);
   if (frame) {
       XPutImage(dpy, win, g_buf_gc, frame, 0, 0, 0, 0, WIN_PX, WIN_PX);
       XDestroyImage(frame);
   } else {
       XCopyArea(dpy, g_buf, win, g_buf_gc, 0, 0, WIN_PX, WIN_PX, 0, 0);
   }
   ```

**Assessment**: This is a halfway pattern. It correctly separates the "capture and present" step from the drawing loop. However, the drawing loop itself still calls X11 primitives (XSetForeground, XDrawPoint, XDrawArc, XShapeCombineMask) to compose into the Pixmap. Porting this to a no-X11 target would still require rewriting the compose functions themselves, not just the present step.

**Why it's not enough**: The real compositing work (draw_sprite_rgb, build_shape_mask, drawing arcs and shape masks) is still X11-coupled. Only the final "readback and blit to window" step is cleanly separated.


## Recommended Convention for Future Code

For any new compositing function or when refactoring existing ones:

### 1. Function Signature Discipline

Every compose function should take a plain RGBA buffer as a parameter:

```c
/* GOOD: portable, no platform-specific types in signature */
static void draw_sprite_to_rgba(
    unsigned char *rgba_buffer,      /* w*h*4 RGBA32 pixels */
    int buffer_w, int buffer_h,      /* buffer dimensions */
    const unsigned char *sprite,     /* source sprite pixels */
    int sprite_w, int sprite_h,
    int x, int y,                    /* sprite position */
    int bg_r, int bg_g, int bg_b     /* background color for alpha blending */
) {
    for (int row = 0; row < sprite_h && (y + row) < buffer_h; row++) {
        for (int col = 0; col < sprite_w && (x + col) < buffer_w; col++) {
            int dst_idx = ((y + row) * buffer_w + (x + col)) * 4;
            const unsigned char *src = &sprite[(row * sprite_w + col) * 4];
            int a = src[3];
            if (a <= 0) continue;
            rgba_buffer[dst_idx + 0] = (src[0] * a + bg_r * (255 - a)) / 255;
            rgba_buffer[dst_idx + 1] = (src[1] * a + bg_g * (255 - a)) / 255;
            rgba_buffer[dst_idx + 2] = (src[2] * a + bg_b * (255 - a)) / 255;
            rgba_buffer[dst_idx + 3] = 255;
        }
    }
}

/* BAD: X11 types in signature, calls X11 inside */
static void draw_sprite_rgb(Display *dpy, Drawable buf, GC gc, ...);
```

### 2. Naming Convention

Suffix compose functions with `_rgba` to signal "this works on plain buffers":
- `draw_sprite_rgba()` — composes sprite to RGBA buffer
- `render_terrain_rgba()` — composes terrain to RGBA buffer
- `composite_layers_rgba()` — composites multiple layers to RGBA buffer

Suffix presentation/blitting functions with `_x11` (or `_drm`, `_framebuffer`, etc. depending on target):
- `present_rgba_x11()` — takes RGBA buffer, blits to X11 window
- `present_rgba_framebuffer()` — takes RGBA buffer, writes to `/dev/fb0`
- `present_rgba_drm()` — takes RGBA buffer, uses DRM/KMS ioctl

This makes `grep _rgba` find every portable compose function, and `grep present_x11` find every X11-specific present function.

### 3. File-Level Isolation (Like gl_mirror.c)

Follow the precedent from `2.muchi-verse/GRAND-ARCHITECTURE.md`, which states:

> "gl_mirror — the ONLY file in mutaclsym allowed to call GL/GLUT primitives"

Establish an analogous rule for X11 in this codebase:

> "khtpm_present_x11.c (or a similar dedicated file) is the ONLY file permitted to call Xlib drawing functions (XSetForeground, XDrawPoint, XPutImage, XCopyArea, XSync, etc.)."

All other files should work exclusively on RGBA buffers. If a file currently does both (e.g., `draw_sprite_rgb` calling XSetForeground), refactor it:

```
BEFORE (X11-coupled):
  tp_desktop_window_rgb.c:draw_sprite_rgb()
    - reads sprite pixels
    - computes alpha blending
    - calls XSetForeground/XDrawPoint per pixel

AFTER (split):
  tp_desktop_window_rgb.c (or a new compose file):draw_sprite_rgba()
    - reads sprite pixels
    - computes alpha blending
    - writes to RGBA buffer (zero X11)
  
  khtpm_present_x11.c:present_rgba_x11()
    - allocates XImage
    - copies RGBA buffer to XImage
    - calls XPutImage (ONLY X11 call site)
```

### 4. Interchange Format

Prefer file-based handoff for heavy workloads (like board-viewer), but for in-process work:

- Allocate a plain `unsigned char rgba_buffer[height][width][4]`
- Call all compose functions on it
- Once all compositing is done, pass it to a single present function

Example pipeline:
```c
unsigned char frame[HEIGHT][WIDTH][4];
memset(frame, 0, sizeof(frame));

/* Compose stage (portable) */
render_terrain_rgba((unsigned char*)frame, WIDTH, HEIGHT, ...);
draw_sprites_rgba((unsigned char*)frame, WIDTH, HEIGHT, ...);
render_text_rgba((unsigned char*)frame, WIDTH, HEIGHT, ...);

/* Present stage (X11-specific) */
present_rgba_x11(dpy, win, (unsigned char*)frame, WIDTH, HEIGHT);
```

Every step before `present_rgba_x11()` is portable and reusable.


## What a No-X11 Backend Would Need to Implement

A future port to a new display target (e.g., raw Linux framebuffer, DRM/KMS, a hypothetical custom OS) would only need to provide these:

```c
/* The ONLY interface a new platform needs to implement */

/* Initialize the display target, return an opaque handle */
void *display_init(const char *display_name);

/* Blit a w*h*4 RGBA buffer to the display at the given offset */
void display_blit_rgba(void *display_handle, 
                       const unsigned char *rgba_buffer,
                       int w, int h,
                       int dst_x, int dst_y);

/* Teardown */
void display_close(void *display_handle);
```

**Possible implementations** (not being built now, just for clarity):

1. **Linux framebuffer** (`/dev/fb0`):
   - Open `/dev/fb0` with `fbmem = mmap(0, ...)`
   - `display_blit_rgba()`: convert RGBA to framebuffer format (usually RGB565 or XRGB8888), `memcpy()` to the right offset

2. **DRM/KMS** (modern Linux):
   - Open `/dev/dri/card0`, request a dumb buffer via ioctl
   - `display_blit_rgba()`: memcpy RGBA pixels to the KMS buffer, submit flip ioctl

3. **Hypothetical custom OS** (future RISC-V target):
   - Custom syscall to check if drawing is allowed
   - `display_blit_rgba()`: syscall with buffer pointer and dimensions

4. **macOS/Cocoa** (later):
   - Create an NSView, wrap RGBA buffer in a CGImage
   - `display_blit_rgba()`: update NSView bitmap and trigger refresh

**Key point**: The compose logic (all the `*_rgba` functions, the raymarchers, alpha blending, text rendering) remains 100% portable across all these targets. Only the `display_blit_rgba()` implementation changes.

---

## Current Scope (Honest Assessment)

This codebase has:

- **Fully portable**: `bv_render_3d.c` (raymarcher, writes plain buffer)
- **Partially portable**: `tp_desktop_window_rgb.c` (has `draw_sprite_rgb` X11-coupled, but also has the XGetImage/XPutImage-present pattern that's close to correct)
- **Not yet split**: Many other rendering functions still inline X11 calls

**This is NOT a bug or poor design**—it's a historical layering of features. The codebase evolved as a single-platform X11 application, so mixing compose and present was pragmatic at the time.

---

## Implementation Notes

This is a **deferred architecture improvement**, not an active work item:

- **Why deferred**: The codebase is stable and functional on X11. Refactoring every compose function is a large, low-priority task.
- **When to do it**: Incrementally, when touching a rendering function anyway for bug fixes or features. The convention above can be adopted for all NEW functions immediately.
- **How to validate**: Any future no-X11 port can measure success by: "Did I only need to write a new `present_rgba_*` function, or did I have to rewrite compose functions?"

This document exists so that a future developer/agent working on a no-X11 port (or any platform-specific rendering backend) can:
1. Read this file instead of reverse-engineering the pattern
2. Know exactly which functions are already portable (grep `_rgba`)
3. Know which files are X11-coupled (grep `-x11` or `khtpm_present_x11`)
4. Use the signature/naming conventions to guide the refactoring incrementally

---

## References

- **File-handoff pattern**: `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c:10269-10399` (run_pchq_board_mode, reads rgb_frame_3d_overlay.raw)
- **Portable raymarcher**: `&.widgits/board-viewer/ops/bv_render_3d.c:1168` (buffer), `:1239`, `:2265` (pixel writes), `:2275` (write to file)
- **Counter-example (X11-coupled)**: `*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c:1402-1420` (draw_sprite_rgb), `:3680-3692` (Present/readback pattern)
- **Platform backend isolation precedent**: `*.monads/system/gl_mirror.c` (comments at lines 1-3, cites GRAND-ARCHITECTURE.md)
- **Governing rule**: `2.muchi-verse/GRAND-ARCHITECTURE.md` (GOVERNING CONSTRAINT section, explains why platform-specific API calls are isolated)
