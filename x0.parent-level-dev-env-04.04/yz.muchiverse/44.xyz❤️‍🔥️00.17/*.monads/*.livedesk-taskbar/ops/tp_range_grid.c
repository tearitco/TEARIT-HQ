/* tp_range_grid - standalone real range-finder popup, 2026-08-05.
 * Direct instruction ("try the range finder as well... get a bit of
 * momentum"): the range-finder grid tp_desktop_window.c's own
 * draw_range_grid() already draws was tightly coupled to that process's
 * own X11 event loop state (popup_win variables etc), so it couldn't be
 * launched independently from a different dispatch path (e.g. a real
 * CHTPM screen's own shell-exec command). This pulls the SAME real
 * drawing code out into its own small, self-contained, launchable X11
 * popup - real code reuse (not reinvented), usable from BOTH the raw
 * X11 popup chain AND the new CHTPM "User" screen.
 *
 * REAL FIX 2026-08-05, direct instruction ("the grid cant cover up dog
 * or surrounding tiles, it should just be a transparent outline like a
 * png. would it help to make a png/pixel .txt of it and use that?"):
 * a real PNG asset isn't needed - the grid is pure geometry (rectangle
 * outlines), so the SAME real X11 Shape Extension technique this house
 * already uses for real sprite transparency (tp_desktop_window.c's own
 * build_shape_mask(), ported from egg_window.c) applies directly: build
 * a synthetic 1-bit mask where ONLY the outline stroke pixels are
 * opaque, everything else (cell interiors, background) stays fully
 * transparent - the real desktop (dog, nearby tiles) shows through
 * everywhere except the grid lines themselves.
 *
 * Usage: tp_range_grid.+x [x] [y]  (screen position, optional)
 * Any click, or Escape, closes it.
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>

/* REAL FIX 2026-08-05, direct instruction ("it needs to be the size of
 * the grid on desktop"): CELL now matches GRID_CELL_PX, the SAME real
 * desktop-grid cell size tp_desktop_window.c/egg_window.c already use
 * (confirmed this session) - so a movement-range cell visually lines
 * up 1:1 with a real desktop grid cell, not an arbitrary made-up size. */
#define CELL 80
#define COLS 5
#define ROWS 5
#define RANGE 2
#define WIN_W (COLS * CELL)
#define WIN_H (ROWS * CELL)

/* Same outline geometry drawn on BOTH the real window (visible strokes)
 * and the shape mask (opaque region) - keeping them in one function
 * guarantees they never drift apart. in_range cells get a thicker
 * (double-drawn) outline so the range boundary stays visually distinct
 * even without any fill. */
static void draw_grid_outline(Display *dpy, Drawable d, GC gc) {
    XDrawRectangle(dpy, d, gc, 0, 0, WIN_W - 1, WIN_H - 1);
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int dr = r - ROWS / 2, dc = c - COLS / 2;
            int dist = (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
            XDrawRectangle(dpy, d, gc, c * CELL, r * CELL, CELL - 1, CELL - 1);
            if (dist <= RANGE) {
                /* in-range: a second, inset outline - reads as a
                 * "highlighted" cell without ever filling it solid. */
                XDrawRectangle(dpy, d, gc, c * CELL + 3, r * CELL + 3, CELL - 7, CELL - 7);
            }
        }
    }
}

static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_W, WIN_H);
    XSetForeground(dpy, mask_gc, 1);
    draw_grid_outline(dpy, mask, mask_gc);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

int main(int argc, char **argv) {
    int x = argc > 1 ? atoi(argv[1]) : 300;
    int y = argc > 2 ? atoi(argv[2]) : 300;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tp_range_grid: cannot open display\n"); return 1; }

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                x, y, WIN_W, WIN_H, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

    Pixmap mask = XCreatePixmap(dpy, win, WIN_W, WIN_H, 1);
    GC mask_gc = XCreateGC(dpy, mask, 0, NULL);
    build_shape_mask(dpy, win, mask_gc, mask);
    XFreeGC(dpy, mask_gc);
    XFreePixmap(dpy, mask);

    XMapRaised(dpy, win);
    XStoreName(dpy, win, "range-finder");
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XGrabPointer(dpy, win, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
    XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);

    int running = 1;
    while (running) {
        XEvent xev;
        XNextEvent(dpy, &xev);
        if (xev.type == Expose) {
            XClearWindow(dpy, win);
            draw_grid_outline(dpy, win, gc);
        } else if (xev.type == ButtonPress || xev.type == KeyPress) {
            running = 0;
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
