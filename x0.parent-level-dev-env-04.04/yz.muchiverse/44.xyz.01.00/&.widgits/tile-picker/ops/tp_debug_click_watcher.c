/* tp_debug_click_watcher - real, standalone, toggle-able desktop-click
 * logger, built to isolate the real Mutter/XWayland click-delivery
 * problem this session hit (RMMV-CLICK-CAPTURE-INVESTIGATION-2026-08-
 * 29.txt) - has ZERO knowledge of the rmmv/palettes feature, arming,
 * or any picker window; its only job is "while enabled, log every
 * real Button1 press on the real desktop." Direct instruction: "a
 * simple op that, when on, detects clicks on desktop, and writes them
 * to <shared-folder>debug/debug.txt."
 *
 * Uses XQueryPointer polling (NOT XGrabPointer/event delivery) -
 * confirmed as the real, Wayland-routing-safe technique by this same
 * session's own investigation (real hardware clicks are never
 * delivered as ButtonPress events to an XGrabPointer-holding XWayland
 * client under this Mutter version - a real, known, still-open
 * upstream bug, gitlab.gnome.org/GNOME/mutter/-/issues/642).
 * XQueryPointer is a synchronous request/reply, not an event, so it
 * sidesteps that gap - this tool itself is real, independent proof of
 * whether that theory is actually correct for REAL human clicks, not
 * just the synthetic ones already confirmed to work.
 *
 * Usage: tp_debug_click_watcher.+x <house_root>
 * Polls #.desktop/debug/debug_watch_enabled.txt every ~100ms; while it
 * contains "1", logs each real Button1 0->1 edge to
 * #.desktop/debug/debug.txt as "CLICK x=<root_x> y=<root_y> ts=<unix>".
 * Exits cleanly the moment the enabled file no longer says "1" (so the
 * Debug HQ window's own "Watch: OFF" toggle can just flip the file's
 * content - no signal/kill needed for a normal stop).
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define PATH_BUF 4352

static int read_enabled(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[16] = "";
    if (fgets(line, sizeof(line), f)) { /* nothing extra needed */ }
    fclose(f);
    return line[0] == '1';
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_debug_click_watcher.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];

    char debug_dir[PATH_BUF], enabled_path[PATH_BUF], log_path[PATH_BUF];
    snprintf(debug_dir, sizeof(debug_dir), "%s/#.desktop/debug", house_root);
    snprintf(enabled_path, sizeof(enabled_path), "%s/debug_watch_enabled.txt", debug_dir);
    snprintf(log_path, sizeof(log_path), "%s/debug.txt", debug_dir);
    mkdir(debug_dir, 0777);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_debug_click_watcher: cannot open display\n");
        return 1;
    }
    Window root = RootWindow(dpy, DefaultScreen(dpy));

    {
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "WATCHER_STARTED pid=%d\n", (int)getpid()); fclose(lf); }
    }

    int was_down = 0;
    while (read_enabled(enabled_path)) {
        Window root_ret, child_ret;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        if (XQueryPointer(dpy, root, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask)) {
            int down = (mask & Button1Mask) ? 1 : 0;
            if (down && !was_down) {
                FILE *lf = fopen(log_path, "a");
                if (lf) {
                    fprintf(lf, "CLICK x=%d y=%d ts=%ld\n", root_x, root_y, (long)time(NULL));
                    fclose(lf);
                }
            }
            was_down = down;
        }
        /* REAL FIX, found live testing this exact tool: 100ms was too
         * coarse - a real click's button-down window (a synthetic
         * XTest click holds for only ~50ms; a real human click can be
         * shorter) can fall entirely between two 100ms samples and
         * never get caught at all. 15ms gives a real margin against
         * even a fast click while still being cheap to poll. */
        usleep(15000);
    }

    {
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "WATCHER_STOPPED pid=%d\n", (int)getpid()); fclose(lf); }
    }
    XCloseDisplay(dpy);
    return 0;
}
