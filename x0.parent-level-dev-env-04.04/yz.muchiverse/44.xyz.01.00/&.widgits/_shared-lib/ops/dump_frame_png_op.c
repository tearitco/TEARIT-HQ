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
 *        dump_frame_png_op --root <out_png_path>
 *
 * Opens its OWN X11 connection (does not share the caller's Display
 * handle/GC/double-buffer Pixmap — a separate process can't reach into
 * another process's memory anyway), captures the target window's real
 * on-screen pixels directly via XGetImage (same principle a real
 * screenshot tool like `import`/`xwd` already uses — no dependency on
 * the caller's own internal back-buffer, just whatever's currently
 * blitted to the window, which the caller has already flushed by the
 * time this fires off a relay-triggered 'p' keypress).
 *
 * REAL, NEW 2026-09-03 (direct live report: a per-window dump this same
 * tool always produced looked fine in isolation, yet disagreed with the
 * owner's own real screenshot of the SAME window showing its "X" chrome
 * cut off past the real screen edge - "update png dump code... so we
 * can have accurate receipts"). Root cause of the disagreement: a
 * cropped-to-one-window dump can never show whether that window's own
 * real on-screen position runs past the real display edge - it only
 * ever proves the window's OWN content is internally consistent, not
 * where it actually sits. Two real, generic fixes, not app-specific:
 * (1) every dump now also prints a real geometry receipt to stderr -
 * the window's real ROOT-relative x/y (via XTranslateCoordinates, not
 * XWindowAttributes' own x/y, which are parent-relative and silently
 * wrong for any reparented window - a real, separate bug class this
 * tool could have hit even before today), the real DisplayWidth/
 * DisplayHeight, and an explicit OFF-SCREEN flag the instant the
 * window's own right/bottom edge exceeds them - text a debugging
 * session can trust without eyeballing pixels. (2) a new `--root` mode
 * dumps the WHOLE real screen instead of one window, the only way to
 * see a window's real position relative to the actual visible desktop
 * edge in the same image a human screenshot would show - use this,
 * not a per-window dump, whenever off-screen placement itself is what's
 * being verified. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

static int capture_and_write(Display *dpy, Window win, int x0, int y0, int w, int h, const char *out_path) {
    XImage *img = XGetImage(dpy, win, x0, y0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) {
        fprintf(stderr, "dump_frame_png_op: XGetImage failed for window 0x%lx\n", (unsigned long)win);
        return 1;
    }
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); return 1; }
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
    int ok = stbi_write_png(out_path, w, h, 3, rgb, w * 3);
    free(rgb);
    fprintf(stderr, ok ? "dump_frame_png_op: wrote %s (%dx%d)\n" : "dump_frame_png_op: write failed\n", out_path, w, h);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: dump_frame_png_op <window_id_hex> <out_png_path>\n"
                        "       dump_frame_png_op --root <out_png_path>\n");
        return 1;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "dump_frame_png_op: cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, screen), sh = DisplayHeight(dpy, screen);

    if (strcmp(argv[1], "--root") == 0) {
        /* REAL, NEW 2026-09-03 - whole-screen dump, see this file's own
         * header comment: the only way to show a window's real position
         * relative to the real visible desktop edge, same as a human's
         * own full screenshot. */
        Window root = RootWindow(dpy, screen);
        fprintf(stderr, "dump_frame_png_op: --root display=%dx%d\n", sw, sh);
        int rc = capture_and_write(dpy, root, 0, 0, sw, sh, argv[2]);
        XCloseDisplay(dpy);
        return rc;
    }

    Window win = (Window)strtoul(argv[1], NULL, 16);
    const char *out_path = argv[2];

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) {
        fprintf(stderr, "dump_frame_png_op: XGetWindowAttributes failed for window 0x%lx\n", (unsigned long)win);
        XCloseDisplay(dpy);
        return 1;
    }
    int w = attrs.width, h = attrs.height;

    /* REAL, NEW 2026-09-03 - the geometry receipt itself. XTranslate-
     * Coordinates, not attrs.x/attrs.y - those are PARENT-relative
     * (silently wrong the instant a window is reparented, e.g. by a
     * window manager), this is real root/screen-relative position,
     * the only thing "is this off-screen" can honestly be checked
     * against. */
    Window child;
    int root_x = 0, root_y = 0;
    XTranslateCoordinates(dpy, win, RootWindow(dpy, screen), 0, 0, &root_x, &root_y, &child);
    int off_right = (root_x + w) > sw, off_bottom = (root_y + h) > sh, off_left = root_x < 0, off_top = root_y < 0;
    fprintf(stderr, "dump_frame_png_op: window 0x%lx root_pos=%d,%d size=%dx%d display=%dx%d%s\n",
            (unsigned long)win, root_x, root_y, w, h, sw, sh,
            (off_right || off_bottom || off_left || off_top) ? " OFF-SCREEN" : "");
    if (off_right)  fprintf(stderr, "  -> right edge %d exceeds display width %d by %d px\n", root_x + w, sw, root_x + w - sw);
    if (off_bottom) fprintf(stderr, "  -> bottom edge %d exceeds display height %d by %d px\n", root_y + h, sh, root_y + h - sh);
    if (off_left)   fprintf(stderr, "  -> left edge %d is negative\n", root_x);
    if (off_top)    fprintf(stderr, "  -> top edge %d is negative\n", root_y);

    int rc = capture_and_write(dpy, win, 0, 0, w, h, out_path);
    XCloseDisplay(dpy);
    return rc;
}
