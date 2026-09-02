/* tp_test_send_click - real mouse-click injection for automated testing,
 * same real standard as tp_test_send_key.c (this file's own sibling) -
 * uses XTest (libXtst) directly, NOT xdotool. Written 2026-08-16 after
 * finding `101.drag-drop-test=ON🀄️/ops/dd_drag_drop.c` (and its own
 * siblings dd_find_window.c/dd_move_window.c) had independently drifted
 * onto shelling out to the `xdotool` CLI instead of following this same
 * house's own already-proven XTest-direct standard - confirmed xdotool
 * is NOT installed here (`which xdotool` empty, no dpkg entry), so that
 * whole harness doesn't actually run on this machine. This tool is the
 * real fix: the one genuinely working mouse-injection path, matching
 * tp_test_send_key.c exactly (same find_by_name() window-tree walk, no
 * new dependency).
 *
 * Usage: tp_test_send_click.+x <window_name_substring> <button:1|2|3>
 *        [rel_x] [rel_y]
 * e.g. tp_test_send_click.+x "tile:ava" 3
 *      tp_test_send_click.+x "tile:ava" 3 40 40
 *
 * Moves the real pointer to the target window's position (its own
 * top-left, root-relative, via XTranslateCoordinates) plus an optional
 * rel_x/rel_y offset (defaults to the window's own real center via
 * XGetWindowAttributes), then synthesizes a real button press+release
 * at that position - a genuine X11 event, not a fake file-based
 * shortcut, same as tp_test_send_key.c's own real key events. */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Window find_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strstr(name, target)) found = children[i];
            XFree(name);
        }
        if (!found) found = find_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <window_name_substring> <button:1|2|3> [rel_x] [rel_y]\n", argv[0]);
        return 1;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    Window w = find_by_name(dpy, RootWindow(dpy, DefaultScreen(dpy)), argv[1]);
    if (!w) { fprintf(stderr, "window '%s' not found\n", argv[1]); return 1; }

    int button = atoi(argv[2]);
    if (button < 1 || button > 3) { fprintf(stderr, "bad button '%s' (use 1/2/3)\n", argv[2]); return 1; }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, w, &attrs)) { fprintf(stderr, "XGetWindowAttributes failed\n"); return 1; }

    int rel_x = (argc > 3) ? atoi(argv[3]) : attrs.width / 2;
    int rel_y = (argc > 4) ? atoi(argv[4]) : attrs.height / 2;

    Window child;
    int abs_x, abs_y;
    if (!XTranslateCoordinates(dpy, w, RootWindow(dpy, DefaultScreen(dpy)), rel_x, rel_y, &abs_x, &abs_y, &child)) {
        fprintf(stderr, "XTranslateCoordinates failed\n");
        return 1;
    }

    XTestFakeMotionEvent(dpy, DefaultScreen(dpy), abs_x, abs_y, 0);
    XSync(dpy, False);
    XTestFakeButtonEvent(dpy, button, True, 0);
    XSync(dpy, False);
    XTestFakeButtonEvent(dpy, button, False, 0);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    printf("SENT button %d click at (%d,%d) [rel %d,%d] on window matching '%s'\n",
           button, abs_x, abs_y, rel_x, rel_y, argv[1]);
    return 0;
}
