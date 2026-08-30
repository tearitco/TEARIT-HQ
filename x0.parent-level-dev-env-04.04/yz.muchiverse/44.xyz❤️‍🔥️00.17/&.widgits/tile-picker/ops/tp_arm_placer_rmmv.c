/* tp_arm_placer_rmmv - the "click desktop to place" half of RMMV tile
 * placement (TILE-SYSTEM-DESIGN.md §4b.3, §6 item 6). Deliberate,
 * simplified parallel of tp_arm_placer.c: same real global pointer/
 * keyboard grab + wait-for-ButtonPress-or-Escape technique, but with
 * the board-viewer/mutaclysm-hero branch dropped entirely - RMMV tile
 * placement targets the bare desktop only for now (the 2D->3D bridge
 * into Mutaclysm/piececraft is real, separate, deferred work per
 * TILE-SYSTEM-DESIGN.md §4a/§5, not built here). No live "ARMED" HUD
 * either - that's tile-picker's own pieces/system/tp_state.txt display
 * convention, which nothing in the palettes UI currently watches; kept
 * out rather than wiring a status display no one reads yet.
 *
 * Usage: tp_arm_placer_rmmv.+x <widget_state_dir> <desktop_root>
 * Spawned detached (setsid) by palettes_menu.sh's arm_rmmv() on a real
 * RMMV tile-grid click. On a real desktop click, sets TP_INITIAL_X/Y
 * and execs tp_place_desktop_rmmv.+x (same sibling-binary resolution
 * tp_arm_placer.c already uses). Escape cancels silently.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352

static void resolve_ops_dir(char *out, size_t out_sz) {
    char self_path[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) { out[0] = '\0'; return; }
    self_path[len] = '\0';
    char *slash = strrchr(self_path, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s", self_path);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tp_arm_placer_rmmv.+x <widget_state_dir> <desktop_root>\n");
        return 1;
    }
    const char *widget_state_dir = argv[1];
    const char *desktop_root = argv[2];

    char ops_dir[PATH_BUF];
    resolve_ops_dir(ops_dir, sizeof(ops_dir));

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_arm_placer_rmmv: cannot open display\n");
        return 1;
    }
    Window root = RootWindow(dpy, DefaultScreen(dpy));

    if (XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                      None, None, CurrentTime) != GrabSuccess) {
        fprintf(stderr, "tp_arm_placer_rmmv: could not grab pointer\n");
        XCloseDisplay(dpy);
        return 1;
    }
    if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) {
        XUngrabPointer(dpy, CurrentTime);
        fprintf(stderr, "tp_arm_placer_rmmv: could not grab keyboard\n");
        XCloseDisplay(dpy);
        return 1;
    }

    int click_x = -1, click_y = -1, cancelled = 0;
    while (1) {
        XEvent xev;
        XNextEvent(dpy, &xev);
        if (xev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&xev.xkey, 0);
            if (ks == XK_Escape) { cancelled = 1; break; }
        } else if (xev.type == ButtonPress) {
            click_x = xev.xbutton.x_root;
            click_y = xev.xbutton.y_root;
            break;
        }
    }
    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XCloseDisplay(dpy);

    /* REAL, NEW 2026-08-29, direct live report ("nothing happened when
     * i tried it") - clear the picker's own "armed" note on every real
     * exit path (cancelled OR placed), so it never shows stale. See
     * palettes_menu.sh's arm_rmmv() for the write side. */
    char armed_path[PATH_BUF];
    snprintf(armed_path, sizeof(armed_path), "%s/rmmv_armed.txt", widget_state_dir);
    unlink(armed_path);

    if (cancelled) return 0;

    char envx[32], envy[32];
    snprintf(envx, sizeof(envx), "%d", click_x);
    snprintf(envy, sizeof(envy), "%d", click_y);
    setenv("TP_INITIAL_X", envx, 1);
    setenv("TP_INITIAL_Y", envy, 1);

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/tp_place_desktop_rmmv.+x' '%s' '%s' >/dev/null 2>&1",
             ops_dir, widget_state_dir, desktop_root);
    system(cmd);
    return 0;
}
