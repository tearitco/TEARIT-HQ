/* tp_find_window_by_pid - find a top-level X window's on-screen rect by
 * the PID of the process that owns it.
 *
 * Usage: tp_find_window_by_pid.+x <pid>
 * Prints "x y w h" on success (absolute screen coords), exits 1 if no
 * window found.
 *
 * Direct instruction 2026-08-04 ("give each window a pid"): every
 * widget in this house shares the SAME window title ("wsr-pal RGB
 * mirror", since gl_mirror is copied wholesale from wsr-pal by every
 * project's own build.sh) - title matching (the technique
 * egg_window.c's own find_mirror_rect() uses, since mutaclysm's
 * DIFFERENT gl_mirror fork has a distinct title) cannot disambiguate
 * between two widget instances here. The standard, real fix: every
 * ICCCM-compliant window manager (confirmed present: gnome-session is
 * this machine's WM) sets the real _NET_WM_PID property on each
 * top-level window to the PID of the process that created it - reading
 * that property directly identifies a specific window regardless of its
 * title, and pairs naturally with ledger_peers.+x's own PID column
 * (the real widget-discovery mechanism, see board-viewer's own
 * button.sh ledger_append.c call).
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>

static Window find_by_pid(Display *d, Window start, Atom net_wm_pid, long target_pid) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;
        if (XGetWindowProperty(d, children[i], net_wm_pid, 0, 1, False, XA_CARDINAL,
                                &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
            if (prop) {
                long pid = *(long *)prop;
                if (actual_format == 32 && nitems == 1 && pid == target_pid) found = children[i];
                XFree(prop);
            }
        }
        if (!found) found = find_by_pid(d, children[i], net_wm_pid, target_pid);
    }
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    long target_pid = atol(argv[1]);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    Atom net_wm_pid = XInternAtom(dpy, "_NET_WM_PID", True);
    if (net_wm_pid == None) { fprintf(stderr, "_NET_WM_PID atom not supported\n"); return 1; }

    Window w = find_by_pid(dpy, RootWindow(dpy, DefaultScreen(dpy)), net_wm_pid, target_pid);
    if (!w) { fprintf(stderr, "no window found for pid %ld\n", target_pid); return 1; }

    Window junk;
    int rel_x, rel_y;
    unsigned int width, height, border, depth;
    XGetGeometry(dpy, w, &junk, &rel_x, &rel_y, &width, &height, &border, &depth);
    int abs_x, abs_y;
    Window child_ret;
    XTranslateCoordinates(dpy, w, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, &abs_x, &abs_y, &child_ret);

    printf("%d %d %u %u\n", abs_x, abs_y, width, height);
    XCloseDisplay(dpy);
    return 0;
}
