/* tp_arm_placer_rmmv - the "click desktop to place" half of RMMV tile
 * placement (TILE-SYSTEM-DESIGN.md §4b.3, §6 item 6).
 *
 * REAL REWRITE 2026-08-29 (RMMV-CLICK-CAPTURE-INVESTIGATION-2026-08-29
 * .txt + the follow-up live diagnosis that superseded it): the
 * original version used XGrabPointer on the root window. Two real,
 * confirmed-live findings killed that approach entirely:
 *   1. A known, still-open Mutter bug (gitlab.gnome.org/GNOME/mutter/
 *      -/issues/642) - real hardware pointer events are never
 *      delivered to an XGrabPointer-holding XWayland client, only
 *      synthetic XTest-injected ones.
 *   2. Switching to XQueryPointer polling (khtpm_core_render.c's
 *      own dbhq_rmmv_poll_pointer()) fixed synthetic clicks but NOT
 *      real ones either - direct, decisive live evidence from a real,
 *      standalone diagnostic tool built for exactly this
 *      (tp_debug_click_watcher.c): every real click it ever logged,
 *      across many real attempts, fell INSIDE the bounds of a real,
 *      already-open khtpm window - never once on genuinely bare
 *      desktop. The real, confirmed conclusion: this Mutter/XWayland
 *      setup only makes real hardware click state visible to an
 *      XWayland client's X11 view AT ALL when the click lands on a
 *      real XWayland surface - bare Wayland-native desktop space is
 *      invisible to X11 entirely, grab or no grab, poll or no poll.
 *
 * Real fix, direct instruction ("maybe we do need a screen wide
 * transparent click capture surface?"): this op now creates a real,
 * full-screen, InputOnly (invisible, draws nothing, needs no opacity
 * trick) override_redirect window covering the whole real screen, and
 * waits for a NORMAL ButtonPress event on it (no grab at all) - since
 * it's a real, mapped XWayland surface everywhere the user could
 * click, every real click now lands ON a real surface, sidestepping
 * the invisible-bare-desktop gap entirely instead of fighting it.
 *
 * Usage: tp_arm_placer_rmmv.+x <widget_state_dir> <desktop_root>
 * Spawned detached (setsid) by palettes_menu.sh's arm_rmmv(). On a
 * real click, sets TP_INITIAL_X/Y and execs tp_place_desktop_rmmv.+x.
 * Escape cancels silently.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
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
        fprintf(stderr, "Usage: tp_arm_placer_rmmv.+x <widget_state_dir> <desktop_root> [picker_x picker_y picker_w picker_h]\n");
        return 1;
    }
    const char *widget_state_dir = argv[1];
    const char *desktop_root = argv[2];
    /* REAL, NEW 2026-08-29 - the picker window's own real rect
     * (optional - a caller with no picker window at all, e.g. a future
     * non-palettes use of this same op, just gets one true full-screen
     * window instead, argc<7). Zero-width/height (no rect given)
     * degrades cleanly to that same single-window behavior. */
    int px = 0, py = 0, pw = 0, ph = 0;
    if (argc >= 7) {
        px = atoi(argv[3]); py = atoi(argv[4]); pw = atoi(argv[5]); ph = atoi(argv[6]);
    }

    char ops_dir[PATH_BUF];
    resolve_ops_dir(ops_dir, sizeof(ops_dir));

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_arm_placer_rmmv: cannot open display\n");
        return 1;
    }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    Visual *vis = DefaultVisual(dpy, screen);

    /* REAL FIX 2026-08-29, direct live report ("maybe we can make the
     * screen a bit more opaque so we can see that mouse capture screen
     * actually starts?") - InputOnly (the original design here) is a
     * real X11 hard constraint: it CANNOT have any visual appearance
     * at all, by definition, no matter what. Switched to real
     * InputOutput windows with a real, subtle amber tint + real
     * _NET_WM_WINDOW_OPACITY (very low, 0.12 - visible confirmation
     * without obscuring the desktop underneath) - same real opacity
     * mechanism this session already proved working elsewhere
     * (set_window_opacity, khtpm_strip_parser.c's own precedent).
     * override_redirect so they never get WM decoration/management
     * (same real reasoning as every other real popup-style window in
     * this house).
     *
     * REAL FIX 2026-08-29, found live testing the first version of
     * this file (a single, true full-screen window): that window sat
     * ON TOP of the picker too, silently swallowing every click meant
     * for the picker itself (e.g. picking a DIFFERENT tile to re-arm
     * with) before the picker ever saw it - a real regression vs. the
     * even-earlier grab-based version, which at least let clicks
     * inside the picker's own rect fall through. Real fix: tile up to
     * 4 real windows covering the WHOLE screen EXCEPT the picker's own
     * rect (top/bottom full-width strips, left/right strips filling
     * the middle band) - the picker's own area is genuinely uncovered,
     * so a click there goes straight to the real picker window exactly
     * as if this op didn't exist at all. */
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor amber;
    XParseColor(dpy, cmap, "#ffaa00", &amber);
    XAllocColor(dpy, cmap, &amber);

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.event_mask = ButtonPressMask | KeyPressMask;
    swa.background_pixel = amber.pixel;
    unsigned long mask = CWOverrideRedirect | CWEventMask | CWBackPixel;

    Window wins[4];
    int n_wins = 0;
    if (pw <= 0 || ph <= 0) {
        wins[n_wins++] = XCreateWindow(dpy, root, 0, 0, (unsigned)sw, (unsigned)sh, 0,
                                        depth, InputOutput, vis, mask, &swa);
    } else {
        /* top strip: full width, above the picker */
        if (py > 0) {
            wins[n_wins++] = XCreateWindow(dpy, root, 0, 0, (unsigned)sw, (unsigned)py, 0,
                                            depth, InputOutput, vis, mask, &swa);
        }
        /* bottom strip: full width, below the picker */
        if (py + ph < sh) {
            wins[n_wins++] = XCreateWindow(dpy, root, 0, py + ph, (unsigned)sw, (unsigned)(sh - (py + ph)), 0,
                                            depth, InputOutput, vis, mask, &swa);
        }
        /* left strip: just the picker's own vertical band, left of it */
        if (px > 0) {
            wins[n_wins++] = XCreateWindow(dpy, root, 0, py, (unsigned)px, (unsigned)ph, 0,
                                            depth, InputOutput, vis, mask, &swa);
        }
        /* right strip: just the picker's own vertical band, right of it */
        if (px + pw < sw) {
            wins[n_wins++] = XCreateWindow(dpy, root, px + pw, py, (unsigned)(sw - (px + pw)), (unsigned)ph, 0,
                                            depth, InputOutput, vis, mask, &swa);
        }
    }
    for (int i = 0; i < n_wins; i++) {
        XMapRaised(dpy, wins[i]);
        Atom opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long val = (unsigned long)(0.12 * (double)0xFFFFFFFFUL);
        XChangeProperty(dpy, wins[i], opacity_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&val, 1);
    }
    XFlush(dpy);
    usleep(200000); /* same real "opacity needs a real first paint before it sticks" delay this session already found/fixed elsewhere */
    for (int i = 0; i < n_wins; i++) {
        Atom opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long val = (unsigned long)(0.12 * (double)0xFFFFFFFFUL);
        XChangeProperty(dpy, wins[i], opacity_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&val, 1);
    }
    XFlush(dpy);
    /* Real keyboard grab still needed for Escape - InputOnly windows
     * don't get keyboard focus by default the way a real click target
     * would, and this window's whole point is to never require the
     * user to click IT first. Pointer is NOT grabbed - the real fix is
     * that these windows' own mapped presence covers the real click
     * target now, not a grab. */
    XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    XSync(dpy, False);

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
    for (int i = 0; i < n_wins; i++) XDestroyWindow(dpy, wins[i]);
    XCloseDisplay(dpy);

    /* Real ledger-write, same convention khtpm_core_render.c's
     * own dbhq_rmmv_handle_desktop_click() uses - tp_place_desktop_
     * rmmv.+x reads its own click position straight from this file
     * (no TP_INITIAL_X/Y env vars anymore, see its own header comment
     * on why - a real, caller-agnostic op), so this write IS the real
     * hand-off, not just a debug trail. */
    if (cancelled) return 0;
    {
        char led[PATH_BUF];
        snprintf(led, sizeof(led), "%s/nav_master_ledger.txt", desktop_root);
        FILE *lf = fopen(led, "a");
        if (lf) {
            fprintf(lf, "RMMV_CLICK pid=%d x=%d y=%d\n", (int)getpid(), click_x, click_y);
            fclose(lf);
        }
    }

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/tp_place_desktop_rmmv.+x' '%s' '%s' >/dev/null 2>&1",
             ops_dir, widget_state_dir, desktop_root);
    system(cmd);
    return 0;
}
