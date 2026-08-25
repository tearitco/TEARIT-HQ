/* tp_set_wm_pid - tag a just-launched gl_mirror window with the real
 * ICCCM/EWMH _NET_WM_PID property, since GLUT doesn't set it itself and
 * this house's WM doesn't synthesize one (confirmed via direct xprop
 * check 2026-08-04 - _NET_WM_PID: not found on a live gl_mirror window).
 *
 * Usage: tp_set_wm_pid.+x <window_title> <pid>
 *
 * Finds the window by TITLE **and requires it to have no _NET_WM_PID
 * property yet**. REAL BUG, found 2026-08-04: title alone is NOT
 * sufficient even at spawn time - if a DIFFERENT widget's window from
 * an earlier, still-running session already has this exact title
 * (every widget's gl_mirror shares "wsr-pal RGB mirror"), a plain
 * title-match here can find and re-tag THAT already-tagged window
 * instead of the freshly-spawned one, silently leaving the real new
 * window untagged and stamping a stale/wrong PID onto someone else's
 * window. Requiring "not yet tagged" as an extra match condition fixes
 * this: a freshly created window has no _NET_WM_PID yet, so it's always
 * unambiguous even with other identically-titled widgets already
 * running (confirmed reproducible: tile-picker + board-viewer both
 * open at once). Every project that wants its widget window
 * discoverable this way needs this same one-line addition in its own
 * button.sh right after spawning gl_mirror - see this project's own
 * button.sh for the call site.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int already_tagged(Display *d, Window w, Atom net_wm_pid) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    int tagged = 0;
    if (XGetWindowProperty(d, w, net_wm_pid, 0, 1, False, XA_CARDINAL,
                            &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems == 1) tagged = 1;
        if (prop) XFree(prop);
    }
    return tagged;
}

static Window find_untagged_by_name(Display *d, Window start, const char *target, Atom net_wm_pid) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strcmp(name, target) == 0 && !already_tagged(d, children[i], net_wm_pid)) {
                found = children[i];
            }
            XFree(name);
        }
        if (!found) found = find_untagged_by_name(d, children[i], target, net_wm_pid);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <window_title> <pid>\n", argv[0]);
        return 1;
    }
    long pid = atol(argv[2]);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    Atom net_wm_pid = XInternAtom(dpy, "_NET_WM_PID", False);
    Window w = 0;
    /* Poll briefly - the window may not exist yet the instant gl_mirror
     * is forked (same real race class as the "THREE-LAYER RACE FIX"
     * already documented in file-menu/board-viewer's own button.sh). */
    for (int i = 0; i < 30 && !w; i++) {
        w = find_untagged_by_name(dpy, RootWindow(dpy, DefaultScreen(dpy)), argv[1], net_wm_pid);
        if (!w) usleep(100000);
    }
    if (!w) { fprintf(stderr, "tp_set_wm_pid: no untagged window '%s' appeared\n", argv[1]); return 1; }

    XChangeProperty(dpy, w, net_wm_pid, XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *)&pid, 1);
    XFlush(dpy);
    printf("TAGGED window '%s' with _NET_WM_PID=%ld\n", argv[1], pid);
    XCloseDisplay(dpy);
    return 0;
}
