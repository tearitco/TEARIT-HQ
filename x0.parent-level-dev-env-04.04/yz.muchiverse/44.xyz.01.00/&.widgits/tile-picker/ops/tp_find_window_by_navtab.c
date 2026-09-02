/* tp_find_window_by_navtab - find a khtpm-rendered window's real
 * on-screen rect via its own PID, WITHOUT relying on _NET_WM_PID (which
 * the window manager never sets - these are override_redirect windows
 * that deliberately bypass the WM, confirmed 2026-08-29 while testing
 * armed-brush RMMV placement: tp_find_window_by_pid.+x structurally
 * cannot work on ANY khtpm_core_render.c window, house-wide, not
 * just this one).
 *
 * Real fix: khtpm_core_render.c's own nav_tab_register() already
 * writes "#.desktop/nav_tab/<pid>" as "<ordinal> <xid-hex> <title>" for
 * every db-hq/events-hq/chat-hai window (palettes runs in db-hq mode,
 * so this covers it) - the real X window ID, self-recorded by the
 * process that owns it, at map time. This tool reads that file and
 * queries geometry DIRECTLY by window ID (XGetGeometry +
 * XTranslateCoordinates) - no tree walk, no WM cooperation needed.
 * Popup/entity-menu-context-menu mode does NOT call nav_tab_register
 * (not nav-tab-cycled), so this only covers db-hq/events-hq/chat-hai -
 * a real, honest scope limit, not a bug.
 *
 * Usage: tp_find_window_by_navtab.+x <house_root> <pid>
 * Prints "x y w h" on success (absolute screen coords), exits 1 if the
 * pid has no nav_tab entry or the window is gone.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF 4352

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <house_root> <pid>\n", argv[0]);
        return 1;
    }
    const char *house_root = argv[1];
    const char *pid_str = argv[2];

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/nav_tab/%s", house_root, pid_str);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "tp_find_window_by_navtab: no nav_tab entry for pid %s (not a db-hq/events-hq/chat-hai window, or it already closed)\n", pid_str);
        return 1;
    }
    int ord = 0;
    unsigned long xid = 0;
    int got = fscanf(f, "%d %lx", &ord, &xid);
    fclose(f);
    if (got < 2 || !xid) {
        fprintf(stderr, "tp_find_window_by_navtab: malformed nav_tab entry for pid %s\n", pid_str);
        return 1;
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }

    Window w = (Window)xid;
    Window junk;
    int rel_x, rel_y;
    unsigned int width, height, border, depth;
    if (!XGetGeometry(dpy, w, &junk, &rel_x, &rel_y, &width, &height, &border, &depth)) {
        fprintf(stderr, "tp_find_window_by_navtab: window 0x%lx (from pid %s's nav_tab entry) no longer exists\n", xid, pid_str);
        XCloseDisplay(dpy);
        return 1;
    }
    int abs_x, abs_y;
    Window child_ret;
    XTranslateCoordinates(dpy, w, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, &abs_x, &abs_y, &child_ret);

    printf("%d %d %u %u\n", abs_x, abs_y, width, height);
    XCloseDisplay(dpy);
    return 0;
}
