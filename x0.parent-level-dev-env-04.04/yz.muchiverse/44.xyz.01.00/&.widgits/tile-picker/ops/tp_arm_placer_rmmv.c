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
 * 2026-09-18: the amber fill + 12% opacity looked like a solid yellow
 * screen. User: tic-tac-toe wireframe, not a wash. 32-bit ARGB when
 * available (transparent fill, yellow lines); else default visual +
 * yellow lines on a near-clear black (no amber fill). Grid is 64px,
 * aligned to root so it matches desk snap.
 *
 * Usage: tp_arm_placer_rmmv.+x <widget_state_dir> <desktop_root>
 * Spawned detached (setsid) by palettes_menu.sh's arm_rmmv(). On a
 * real click, sets TP_INITIAL_X/Y and execs tp_place_desktop_rmmv.+x.
 * Escape cancels silently.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define PATH_BUF 4352

#define PLACE_CELL 64

static void draw_wire_grid(Display *dpy, Window w, GC gc, int ox, int oy, int ww, int wh) {
    int x, y;
    int x0 = (PLACE_CELL - (ox % PLACE_CELL)) % PLACE_CELL;
    int y0 = (PLACE_CELL - (oy % PLACE_CELL)) % PLACE_CELL;
    for (x = x0; x < ww; x += PLACE_CELL)
        XDrawLine(dpy, w, gc, x, 0, x, wh);
    for (y = y0; y < wh; y += PLACE_CELL)
        XDrawLine(dpy, w, gc, 0, y, ww, y);
}

static void resolve_ops_dir(char *out, size_t out_sz) {
    char self_path[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) { out[0] = '\0'; return; }
    self_path[len] = '\0';
    char *slash = strrchr(self_path, '/');
    if (slash) *slash = '\0';
    snprintf(out, out_sz, "%s", self_path);
}

/* Optional (FE_PLACE_ZONES=1, set by File Explorer's Place): while the overlay is up,
 * an open HQ window that published a drop zone (#.desktop/khtpm_drop_zones/<pid>.txt,
 * same registry a dragged desk pal hit-tests) highlights under the pointer via
 * #.desktop/drag_hover_pid.txt, and a click inside it reports that zone's dest dir
 * instead of a desk position. Zones whose dest == FE_PLACE_SKIP_DIR (the source's own
 * folder) are ignored. Rect test only: the overlay covers the windows underneath. */
static int pz_hit(const char *root, int rx, int ry, const char *skip_dir,
                  char *dest, size_t destsz) {
    char dirp[PATH_BUF];
    snprintf(dirp, sizeof(dirp), "%s/#.desktop/khtpm_drop_zones", root);
    DIR *d = opendir(dirp);
    if (!d) return 0;
    struct dirent *de;
    int found = 0;
    while (!found && (de = readdir(d)) != NULL) {
        if (!strstr(de->d_name, ".txt")) continue;
        char fp[PATH_BUF], line[PATH_BUF], zdest[PATH_BUF];
        int pid = 0, x = 0, y = 0, w = 0, h = 0;
        zdest[0] = 0;
        snprintf(fp, sizeof(fp), "%s/%s", dirp, de->d_name);
        FILE *f = fopen(fp, "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f)) {
            char *nl = strchr(line, '\n'); if (nl) *nl = 0;
            if (!strncmp(line, "pid=", 4)) pid = atoi(line + 4);
            else if (!strncmp(line, "x=", 2)) x = atoi(line + 2);
            else if (!strncmp(line, "y=", 2)) y = atoi(line + 2);
            else if (!strncmp(line, "w=", 2)) w = atoi(line + 2);
            else if (!strncmp(line, "h=", 2)) h = atoi(line + 2);
            else if (!strncmp(line, "dest=", 5)) snprintf(zdest, sizeof(zdest), "%s", line + 5);
        }
        fclose(f);
        if (!zdest[0] || pid <= 0) continue;
        if (skip_dir && skip_dir[0] && !strcmp(zdest, skip_dir)) continue;
        if (w > 0 && h > 0 && rx >= x && rx < x + w && ry >= y && ry < y + h) {
            snprintf(dest, destsz, "%s", zdest);
            found = pid;
        }
    }
    closedir(d);
    return found;
}

static void pz_write_hover(const char *root, int pid, const char *name) {
    char path[PATH_BUF], tmp[PATH_BUF + 8];
    snprintf(path, sizeof(path), "%s/#.desktop/drag_hover_pid.txt", root);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "pid=%d\nname=%s\n", pid, name ? name : "");
    fclose(f);
    rename(tmp, path);
}

/* Give X input focus to the top-level window owned by `pid` (EWMH
 * _NET_ACTIVE_WINDOW, source=2 "pager" so focus-stealing prevention lets
 * it through). Palettes leaves its own picker window uncovered and
 * focused, so XGrabKeyboard below works there. A launcher whose window
 * lost focus (e.g. File Explorer: its right-click popup just closed)
 * passes FE_PLACE_FOCUS_PID so the grab has a focused X client and Esc
 * reaches us under Mutter/XWayland. */
static void focus_window_of_pid(Display *dpy, Window root, long pid) {
    Atom a_list = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    Atom a_pid = XInternAtom(dpy, "_NET_WM_PID", True);
    Atom a_act = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if (!a_list || !a_pid || !a_act) return;
    Atom type; int fmt; unsigned long n, after; unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, root, a_list, 0, 4096, False, XA_WINDOW,
                           &type, &fmt, &n, &after, &data) != Success || !data) return;
    Window *wins = (Window *)data, target = 0;
    for (unsigned long i = 0; i < n && !target; i++) {
        unsigned char *pd = NULL; unsigned long pn, pa; Atom pt; int pf;
        if (XGetWindowProperty(dpy, wins[i], a_pid, 0, 1, False, XA_CARDINAL,
                               &pt, &pf, &pn, &pa, &pd) == Success && pd) {
            if (pn >= 1 && *(unsigned long *)pd == (unsigned long)pid) target = wins[i];
            XFree(pd);
        }
    }
    XFree(data);
    if (!target) return;
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = target;
    ev.xclient.message_type = a_act;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(dpy, False);
    usleep(150000);
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
    XVisualInfo vinfo;
    int use_argb = XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo);
    if (use_argb) {
        vis = vinfo.visual;
        depth = vinfo.depth;
    }
    Colormap cmap = use_argb
        ? XCreateColormap(dpy, root, vis, AllocNone)
        : DefaultColormap(dpy, screen);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.override_redirect = True;
    swa.event_mask = ButtonPressMask | KeyPressMask | ExposureMask | PointerMotionMask;
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.background_pixel = 0; /* ARGB: transparent; default: black, no amber wash */
    unsigned long mask = CWOverrideRedirect | CWEventMask | CWBackPixel | CWBorderPixel | CWColormap;

    struct { Window w; int x, y, ww, wh; } panes[4];
    int n_wins = 0;
    #define ADD_PANE(_x,_y,_w,_h) do { \
        if ((_w) > 0 && (_h) > 0) { \
            panes[n_wins].x = (_x); panes[n_wins].y = (_y); \
            panes[n_wins].ww = (_w); panes[n_wins].wh = (_h); \
            panes[n_wins].w = XCreateWindow(dpy, root, (_x), (_y), (unsigned)(_w), (unsigned)(_h), 0, \
                                            depth, InputOutput, vis, mask, &swa); \
            n_wins++; \
        } \
    } while (0)
    if (pw <= 0 || ph <= 0) {
        ADD_PANE(0, 0, sw, sh);
    } else {
        if (py > 0) ADD_PANE(0, 0, sw, py);
        if (py + ph < sh) ADD_PANE(0, py + ph, sw, sh - (py + ph));
        if (px > 0) ADD_PANE(0, py, px, ph);
        if (px + pw < sw) ADD_PANE(px + pw, py, sw - (px + pw), ph);
    }
    #undef ADD_PANE

    GC gcs[4];
    for (int i = 0; i < n_wins; i++) {
        XMapRaised(dpy, panes[i].w);
        gcs[i] = XCreateGC(dpy, panes[i].w, 0, NULL);
        if (use_argb)
            XSetForeground(dpy, gcs[i], 0xE0FFCC00UL); /* AARRGGBB yellow */
        else {
            XColor yel;
            XParseColor(dpy, cmap, "#ffcc00", &yel);
            XAllocColor(dpy, cmap, &yel);
            XSetForeground(dpy, gcs[i], yel.pixel);
        }
        XSetLineAttributes(dpy, gcs[i], 1, LineSolid, CapButt, JoinMiter);
        draw_wire_grid(dpy, panes[i].w, gcs[i], panes[i].x, panes[i].y, panes[i].ww, panes[i].wh);
    }
    XFlush(dpy);
    /* Real keyboard grab still needed for Escape - InputOnly windows
     * don't get keyboard focus by default the way a real click target
     * would, and this window's whole point is to never require the
     * user to click IT first. Pointer is NOT grabbed - the real fix is
     * that these windows' own mapped presence covers the real click
     * target now, not a grab. */
    {
        const char *fp = getenv("FE_PLACE_FOCUS_PID");
        if (fp && atol(fp) > 0) focus_window_of_pid(dpy, root, atol(fp));
    }
    /* Retry: another client may hold the grab for a moment (a popup that is
     * closing). Esc below does not depend on this succeeding. */
    for (int gtry = 0; gtry < 20; gtry++) {
        if (XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) break;
        usleep(50000);
    }
    XSync(dpy, False);

    int click_x = -1, click_y = -1, cancelled = 0;
    const int use_zones = getenv("FE_PLACE_ZONES") && getenv("FE_PLACE_ZONES")[0] == '1';
    const char *skip_dir = getenv("FE_PLACE_SKIP_DIR");
    const char *place_name = getenv("FE_PLACE_NAME");
    char zone_dest[PATH_BUF];
    int zone_pid = 0, hover_pid = 0;
    zone_dest[0] = 0;
    /* Esc is also polled from the server's key state, so it cancels even when
     * another client owns the keyboard grab. An Esc already held when the
     * overlay opens (the keypress that launched it) does not count. */
    const KeyCode esc_kc = XKeysymToKeycode(dpy, XK_Escape);
    char keys[32];
    int esc_prev = 0, done = 0;
    XQueryKeymap(dpy, keys);
    if (esc_kc) esc_prev = (keys[esc_kc >> 3] >> (esc_kc & 7)) & 1;
    while (!done) {
        XEvent xev;
        if (!XPending(dpy)) {
            if (esc_kc) {
                XQueryKeymap(dpy, keys);
                int esc_now = (keys[esc_kc >> 3] >> (esc_kc & 7)) & 1;
                if (esc_now && !esc_prev) { cancelled = 1; break; }
                esc_prev = esc_now;
            }
            int cfd = ConnectionNumber(dpy);
            fd_set rf;
            struct timeval tv = { 0, 20000 };
            FD_ZERO(&rf);
            FD_SET(cfd, &rf);
            select(cfd + 1, &rf, NULL, NULL, &tv);
            continue;
        }
        XNextEvent(dpy, &xev);
        if (xev.type == MotionNotify && use_zones) {
            char zd[PATH_BUF];
            int zp = pz_hit(desktop_root, xev.xmotion.x_root, xev.xmotion.y_root, skip_dir, zd, sizeof(zd));
            if (zp != hover_pid) { hover_pid = zp; pz_write_hover(desktop_root, zp, place_name); }
        } else if (xev.type == Expose) {
            for (int i = 0; i < n_wins; i++)
                if (panes[i].w == xev.xexpose.window)
                    draw_wire_grid(dpy, panes[i].w, gcs[i], panes[i].x, panes[i].y, panes[i].ww, panes[i].wh);
        } else if (xev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&xev.xkey, 0);
            if (ks == XK_Escape) { cancelled = 1; break; }
        } else if (xev.type == ButtonPress) {
            click_x = xev.xbutton.x_root;
            click_y = xev.xbutton.y_root;
            if (use_zones)
                zone_pid = pz_hit(desktop_root, click_x, click_y, skip_dir, zone_dest, sizeof(zone_dest));
            break;
        }
    }
    if (use_zones && hover_pid) pz_write_hover(desktop_root, 0, "");
    XUngrabKeyboard(dpy, CurrentTime);
    for (int i = 0; i < n_wins; i++) {
        XFreeGC(dpy, gcs[i]);
        XDestroyWindow(dpy, panes[i].w);
    }
    XCloseDisplay(dpy);

    /* Real ledger-write, same convention khtpm_core_render.c's
     * own dbhq_rmmv_handle_desktop_click() uses - tp_place_desktop_
     * rmmv.+x reads its own click position straight from this file
     * (no TP_INITIAL_X/Y env vars anymore, see its own header comment
     * on why - a real, caller-agnostic op), so this write IS the real
     * hand-off, not just a debug trail. */
    if (cancelled) return 0;
    {
        /* Real, NEW 2026-09-03 - size-capped ledger write. Same
         * convention khtpm_core_render.c's nav_ledger_write() uses:
         * nav_master_ledger.txt is append-only and previously grew
         * unbounded; consumers only read the newest rows, so once the
         * file exceeds the cap we keep the newest tail (whole lines,
         * atomic tmp+rename) and drop the head. */
        enum { NAV_LEDGER_CAP = 250u * 1024u };
        char led[PATH_BUF], tmp[PATH_BUF];
        snprintf(led, sizeof(led), "%s/nav_master_ledger.txt", desktop_root);
        FILE *lf = fopen(led, "a");
        if (lf) {
            fprintf(lf, "RMMV_CLICK pid=%d x=%d y=%d\n", (int)getpid(), click_x, click_y);
            fclose(lf);
            long sz = 0;
            FILE *sf = fopen(led, "rb");
            if (sf) { fseek(sf, 0, SEEK_END); sz = ftell(sf); fclose(sf); }
            if (sz > (long)NAV_LEDGER_CAP) {
                long start = sz - (long)NAV_LEDGER_CAP;
                if (start < 0) start = 0;
                snprintf(tmp, sizeof(tmp), "%s.tmp.%d", led, (int)getpid());
                FILE *rf = fopen(led, "rb");
                FILE *wf = fopen(tmp, "wb");
                if (rf && wf) {
                    fseek(rf, start, SEEK_SET);
                    int copy = 0;
                    char c;
                    while ((c = fgetc(rf)) != EOF) {
                        if (c == '\n') copy = 1;
                        if (copy) fputc(c, wf);
                    }
                    fclose(rf); rf = NULL;
                    fclose(wf); wf = NULL;
                    if (rename(tmp, led) != 0) unlink(tmp);
                }
                if (rf) fclose(rf);
                if (wf) fclose(wf);
            }
        }
    }

    {
        const char *clickf = getenv("FE_PLACE_CLICK");
        if (clickf && clickf[0]) {
            FILE *cf = fopen(clickf, "w");
            if (cf) {
                fprintf(cf, "x=%d\ny=%d\n", click_x, click_y);
                if (zone_pid) fprintf(cf, "zone_pid=%d\nzone_dest=%s\n", zone_pid, zone_dest);
                fclose(cf);
            }
            return 0;
        }
    }

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/tp_place_desktop_rmmv.+x' '%s' '%s' >/dev/null 2>&1",
             ops_dir, widget_state_dir, desktop_root);
    system(cmd);
    return 0;
}
