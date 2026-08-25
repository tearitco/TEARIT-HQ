/* dump_frame_png_op.c — real, standalone TPMOS-shaped op (2026-08-16,
 * direct correction: "u are using include instead of launching the
 * binary thru fork/exec/sys like tpmos/wraith does. this is the
 * standard for binary calls"). Matches the real shape confirmed by
 * reading `1.TPMOS_c_+rmmp.0103.0001/projects/fuzz-op/ops/toggle_clock.c`
 * and `pieces/chtpm/ops/resolve_project_op.c`: a small, standalone
 * binary, own `main()`, invoked as a real subprocess (fork/exec or
 * system()), does ONE discrete job, exits — never text-included into a
 * caller's own translation unit. Lives in `&.widgits/_shared-lib/ops/`,
 * the real per-house analog of TPMOS's own cross-project
 * `pieces/chtpm/ops/` (confirmed via a direct read of that real
 * directory — genuinely shared, cross-project ops live there, not
 * inside any one project's own `ops/`).
 *
 * Usage: dump_frame_png_op <window_id_hex> <out_png_path>
 *
 * Opens its OWN X11 connection (does not share the caller's Display
 * handle/GC/double-buffer Pixmap — a separate process can't reach into
 * another process's memory anyway), captures the target window's real
 * on-screen pixels directly via XGetImage (same principle a real
 * screenshot tool like `import`/`xwd` already uses — no dependency on
 * the caller's own internal back-buffer, just whatever's currently
 * blitted to the window, which the caller has already flushed by the
 * time this fires off a relay-triggered 'p' keypress). */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: dump_frame_png_op <window_id_hex> <out_png_path>\n");
        return 1;
    }
    Window win = (Window)strtoul(argv[1], NULL, 16);
    const char *out_path = argv[2];

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "dump_frame_png_op: cannot open display\n"); return 1; }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) {
        fprintf(stderr, "dump_frame_png_op: XGetWindowAttributes failed for window 0x%lx\n", (unsigned long)win);
        XCloseDisplay(dpy);
        return 1;
    }
    int w = attrs.width, h = attrs.height;

    XImage *img = XGetImage(dpy, win, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "dump_frame_png_op: XGetImage failed for window 0x%lx\n", (unsigned long)win);
        XCloseDisplay(dpy);
        return 1;
    }

    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); XCloseDisplay(dpy); return 1; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            size_t o = ((size_t)y * w + x) * 3;
            rgb[o] = (unsigned char)((px >> 16) & 0xff);
            rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
            rgb[o + 2] = (unsigned char)(px & 0xff);
        }
    }
    XDestroyImage(img);
    XCloseDisplay(dpy);

    int ok = stbi_write_png(out_path, w, h, 3, rgb, w * 3);
    free(rgb);
    fprintf(stderr, ok ? "dump_frame_png_op: wrote %s (%dx%d)\n" : "dump_frame_png_op: write failed\n", out_path, w, h);
    return ok ? 0 : 1;
}
