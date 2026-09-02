/* tp_test_send_click_abs - real mouse-click injection at ABSOLUTE
 * screen coordinates, same real XTest-direct standard as
 * tp_test_send_click.c (this file's own sibling), which only accepts a
 * window-name substring - useless for override_redirect windows that
 * never set a WM_NAME (confirmed 2026-08-29 testing armed-brush RMMV
 * placement, alongside tp_find_window_by_navtab.c's own fix for the
 * matching coordinate-discovery gap). This tool takes coordinates you
 * already resolved by some other means (nav_tab's own real xid, a real
 * screenshot, etc.) and just clicks there - no window lookup at all.
 *
 * Usage: tp_test_send_click_abs.+x <x> <y> <button:1|2|3>
 */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <x> <y> <button:1|2|3>\n", argv[0]);
        return 1;
    }
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    int button = atoi(argv[3]);
    if (button < 1 || button > 3) { fprintf(stderr, "bad button '%s' (use 1/2/3)\n", argv[3]); return 1; }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
    XSync(dpy, False);
    usleep(50000);
    XTestFakeButtonEvent(dpy, button, True, CurrentTime);
    XSync(dpy, False);
    usleep(50000);
    XTestFakeButtonEvent(dpy, button, False, CurrentTime);
    XSync(dpy, False);

    printf("clicked (%d,%d) button %d\n", x, y, button);
    XCloseDisplay(dpy);
    return 0;
}
