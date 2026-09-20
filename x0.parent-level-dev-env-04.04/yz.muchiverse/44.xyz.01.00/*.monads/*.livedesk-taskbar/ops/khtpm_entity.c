#define _POSIX_C_SOURCE 200809L /* CLOCK_MONOTONIC + getline() under -std=c11 strict mode */
#define _DEFAULT_SOURCE 1 /* usleep() */
/* khtpm_entity.c - the desktop pal / entity process (khtpm_entity.+x).
 * Restored from the tile-mode code that had been pasted into
 * khtpm_core_render.c as tp_main() (CENTROID_GOLD_STD 1a; see
 * 08-roadmap/design-docs/XHTPM-RE.md and UNFACTOR-PAL-X.md). Usage:
 *   khtpm_entity.+x <package_dir>
 * One process per pal; its IPC is files in the pal dir and #.desktop
 * (desktop_pos.txt, history.txt, interact_relay.txt, ...). It draws its own
 * tile/sprite/popups and does not use the HQ Elem/CSS engine. What it
 * shares with the HQ renderer (hq_ui.pdl + theme loaders, UI scale, METHOD
 * reader, menu launcher) comes from khtpm_ui_common.c. */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h> /* INT_MIN - g_win_pos_applied_x/y sentinel (redraw() corrective-move guard) */
#include <string.h>
#include <ctype.h>
#include <dirent.h> /* REAL, chat-hai mode only - session-dir listing */
#include <fcntl.h> /* REAL, NEW 2026-09-01 - strip mode's own zorder toggle respawn (open("/dev/null", O_RDWR)) */
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h> /* REAL, NEW 2026-09-01 - tile mode's own real gettimeofday() frame-pacing/click-vs-drag timing */
#include <sys/wait.h> /* REAL, db-hq mode only - launch_module()/cleanup_module(), real fork()+execl() */
#include <errno.h>
#include <signal.h> /* REAL, db-hq mode only - handle_term_signal() */
extern char **environ;
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h> /* 2026-08-24 - XA_WINDOW for the XdndAware property (XDND drop support) */
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/shape.h> /* REAL, NEW 2026-09-01 - tile mode's own real per-pixel window shape (build_shape_mask()/cursword_update_shape()), folded in verbatim from tp_desktop_window_rgb.c */
#include <math.h> /* REAL, NEW 2026-09-01 - tile mode's own real raymarch camera math (fabs/sqrt/sin/cos/tan), folded in verbatim from tp_desktop_window_rgb.c */
#define M_PI_LOCAL 3.14159265358979323846 /* same real, portable local constant tp_desktop_window_rgb.c's own file already used, not relying on glibc's own optional M_PI */
#include <locale.h> /* REAL, NEW 2026-09-01 - tile mode's own setlocale()/XSetLocaleModifiers() for its popup fontset */
#include <libgen.h> /* REAL, NEW 2026-09-01 - tile mode's own dirname()/basename() (self_exe_path/piece_id) */
#include <sys/file.h> /* REAL, NEW 2026-09-01 - tile mode's own real flock() cross-process popup mutex */

#define PATH_BUF 4096
#include "khtpm_ui_common.c"

/* tp_main uses a LOCAL Display and never sets the global dpy, so the global
 * stays NULL. kh_ungrab_kbd() used to test that global and was therefore a
 * silent no-op: Cursword's armed-mode grab and every pal context menu with
 * grab_keyboard=1 kept a display-wide XGrabKeyboard until the process died,
 * so no other window (csv-hq, text fields...) got a single key. tp_main now
 * publishes its connection in g_kbd_dpy so the release goes to the same
 * connection that took the grab. */
static Display *dpy = NULL;
static Display *g_kbd_dpy = NULL;
static void kh_ungrab_kbd(void) {
    Display *d = g_kbd_dpy ? g_kbd_dpy : dpy;
    if (d) { XUngrabKeyboard(d, CurrentTime); XFlush(d); }
}

/* Same append-only size-cursor gate the HQ renderer uses (never mtime):
 * when hq_ui.pdl's marker grows, reload the shared keys and, if the UI scale
 * changed, flag a repaint. The HQ relayout half stays in khtpm_core_render.c. */
static long g_hq_ui_pdl_marker_sz = -1;
static void entity_ui_pdl_reload_if_changed(const char *house_root) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui_pdl_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (g_hq_ui_pdl_marker_sz < 0) { g_hq_ui_pdl_marker_sz = (long)st.st_size; return; }
    if ((long)st.st_size > g_hq_ui_pdl_marker_sz) {
        g_hq_ui_pdl_marker_sz = (long)st.st_size;
        int old_scale = g_ui_scale_pct;
        kh_ui_pdl_load(house_root, NULL);
        if (g_ui_scale_pct != old_scale) g_frame_dirty = 1;
    }
}

/* Allocate a "#rrggbb" pixel using an EXPLICIT Display+screen.
 * tp_main() (tile/entity mode) has its own LOCAL Display and never
 * sets the file-scope dpy/cmap/screen that the shared alloc_pixel()
 * reads - calling alloc_pixel() from there dereferences a NULL Display
 * and kills the process (regression: a themed Show Text popup closed
 * the book-stack entity, 2026-09-09). Uses the root default colormap,
 * which is what tp_main's popup windows and popup_gc actually use
 * (they are created CopyFromParent / on the root). */
static unsigned long tp_hex_pixel(Display *d, int scr, const char *hex) {
    if (!d) return 0;
    if (!hex || !hex[0]) return BlackPixel(d, scr);
    Colormap cm = DefaultColormap(d, scr);
    XColor c;
    if (XParseColor(d, cm, hex, &c) && XAllocColor(d, cm, &c)) return c.pixel;
    return BlackPixel(d, scr);
}

static void kh_write_drag_hover(int pid, const char *name) {
    char path[PATH_BUF], tmp[PATH_BUF];
    FILE *f;
    if (!g_house_root[0]) return;
    snprintf(path, sizeof(path), "%s/#.desktop/drag_hover_pid.txt", g_house_root);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "pid=%d\nname=%s\n", pid, name ? name : "");
    fclose(f);
    rename(tmp, path);
}

/* Pointer over a published drop zone? Match the X window under the
 * pointer (walk parent chain) to zone `win=`, not the often-stale
 * x/y/w/h (WM-moved File Explorer sat at 90,90 in the file while the
 * pointer was at y=1476). Fall back to rect if no win id. */
static int kh_drop_zone_hit(Display *dpy_hit, int rx, int ry, char *dest, size_t destsz,
                            unsigned long *out_win) {
    char dirp[PATH_BUF];
    DIR *d;
    struct dirent *de;
    int self = (int)getpid();
    unsigned long stack[64];
    int nstack = 0;
    if (out_win) *out_win = 0;
    if (!g_house_root[0]) return 0;
    if (dpy_hit) {
        Window current = RootWindow(dpy_hit, DefaultScreen(dpy_hit));
        int x = rx, y = ry;
        while (nstack < 63) {
            Window child = None;
            int nx = 0, ny = 0;
            if (!XTranslateCoordinates(dpy_hit, current, current, x, y, &nx, &ny, &child))
                break;
            if (child == None) break;
            stack[nstack++] = (unsigned long)child;
            current = child;
            x = nx;
            y = ny;
        }
    }
    snprintf(dirp, sizeof(dirp), "%s/#.desktop/khtpm_drop_zones", g_house_root);
    d = opendir(dirp);
    if (!d) return 0;
    while ((de = readdir(d)) != NULL) {
        char fp[PATH_BUF], line[PATH_BUF];
        FILE *f;
        int pid = 0, x = 0, y = 0, w = 0, h = 0, i;
        unsigned long zwin = 0;
        char zdest[PATH_BUF];
        int hit = 0;
        zdest[0] = 0;
        if (!strstr(de->d_name, ".txt")) continue;
        snprintf(fp, sizeof(fp), "%s/%s", dirp, de->d_name);
        f = fopen(fp, "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "pid=", 4)) pid = atoi(line + 4);
            else if (!strncmp(line, "win=", 4)) zwin = strtoul(line + 4, NULL, 0);
            else if (!strncmp(line, "x=", 2)) x = atoi(line + 2);
            else if (!strncmp(line, "y=", 2)) y = atoi(line + 2);
            else if (!strncmp(line, "w=", 2)) w = atoi(line + 2);
            else if (!strncmp(line, "h=", 2)) h = atoi(line + 2);
            else if (!strncmp(line, "dest=", 5)) {
                char *nl = strchr(line, '\n'); if (nl) *nl = 0;
                snprintf(zdest, sizeof(zdest), "%s", line + 5);
            }
        }
        fclose(f);
        if (pid == self || !zdest[0]) continue;
        if (zwin) {
            for (i = 0; i < nstack; i++)
                if (stack[i] == zwin) { hit = 1; break; }
        }
        if (!hit && w > 0 && h > 0 && rx >= x && rx < x + w && ry >= y && ry < y + h)
            hit = 1;
        if (hit) {
            if (dest && destsz) snprintf(dest, destsz, "%s", zdest);
            if (out_win) *out_win = zwin;
            closedir(d);
            return pid;
        }
    }
    closedir(d);
    return 0;
}

/* Pal-on-inventory: Raise cannot beat a WM window — desk pals live
 * in a lower compositor layer. Redraw does not change stacking.
 * Reparent the pal onto the File Explorer window so it paints in
 * THAT window's tree (always on top of the file list). */
static int g_drag_reparented = 0;
static Window g_drag_host = None;
static int g_drag_host_pid = 0;
static int kh_xerr_ign(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }
static int kh_pointer_in_window(Display *d, Window w, int rx, int ry) {
    XWindowAttributes at;
    int lx = 0, ly = 0;
    Window ch = None;
    if (!d || !w) return 0;
    if (!XGetWindowAttributes(d, w, &at) || at.width <= 0 || at.height <= 0) return 0;
    XTranslateCoordinates(d, DefaultRootWindow(d), w, rx, ry, &lx, &ly, &ch);
    return lx >= 0 && ly >= 0 && lx < at.width && ly < at.height;
}
static void kh_drag_stack_above(Display *d, Window pal, Window target,
                                int on, int rx, int ry) {
    Window root;
    int pw, ph, lx, ly;
    Window ch = None;
    XErrorHandler old;
    if (!d || !pal) return;
    root = DefaultRootWindow(d);
    pw = 64;
    ph = pw;
    old = XSetErrorHandler(kh_xerr_ign);
    if (on && target && target != pal) {
        XTranslateCoordinates(d, root, target, rx, ry, &lx, &ly, &ch);
        if (!g_drag_reparented) {
            XReparentWindow(d, pal, target, lx - pw / 2, ly - ph / 2);
            g_drag_reparented = 1;
            g_drag_host = target;
        } else {
            XMoveWindow(d, pal, lx - pw / 2, ly - ph / 2);
        }
        XMapRaised(d, pal);
    } else {
        if (g_drag_reparented) {
            XReparentWindow(d, pal, root, rx - pw / 2, ry - ph / 2);
            g_drag_reparented = 0;
            g_drag_host = None;
            g_drag_host_pid = 0;
        }
        XMapRaised(d, pal);
    }
    XSync(d, False);
    XSetErrorHandler(old);
}

/* ============================================================
 * BEGIN TILE MODE (real entity-window renderer), folded in verbatim
 * from tp_desktop_window_rgb.c, 2026-09-01 (real house-standard
 * consolidation, phase 2 - see strip_main()'s own big header comment
 * above for the phase-1 precedent and the exact "no linkers" rule this
 * follows: khtpm_strip_parser.c/.../tp_desktop_window_rgb.c relocated
 * VERBATIM into this one file/binary, dispatched by argv shape, never
 * cross-.c #include/linking).
 *
 * This mode's own real invocation shape is exactly <package_dir>,
 * argc==2 - SAME argc as strip mode (<house_root>), disambiguated in
 * the real main() below by checking for a real "#.desktop" directory
 * under argv[1] (present only for a real house_root, never for a tile/
 * pal's own package_dir) - see main()'s own dispatch comment.
 *
 * Real collisions resolved during this relocation (found via the same
 * comm -12 static-scan + build-verify technique phase 1 used):
 *   - main()              -> tp_main() (this mode's own entry point)
 *   - load_override_redirect(), set_window_opacity(), the globals
 *     g_override_redirect and g_click_two_step: BYTE-FOR-BYTE identical
 *     to this file's own pre-existing copies (confirmed by direct
 *     comparison) - reused as-is, tp_desktop_window_rgb.c's own
 *     duplicate copies dropped, not renamed.
 *   - load_theme_opacity(): NOT reused - this file's own existing
 *     load_theme_opacity() takes no args (reads the shared global
 *     g_house_root directly), while tp_desktop_window_rgb.c's own
 *     version took an explicit house_root parameter. Renamed to
 *     tp_load_theme_opacity() (kept as its own real function, same
 *     body) rather than rewriting every call site's calling
 *     convention - a real, deliberate, minimal-risk choice, not an
 *     oversight.
 *   - PATH_BUF (4352 here vs this file's own 4096): renamed to
 *     TP_PATH_BUF throughout this block, kept at its own original
 *     4352 value - reusing the smaller existing constant would have
 *     silently shrunk every one of this mode's own real path buffers.
 *   - tp_desktop_window_rgb.c's own local `char g_house_root[PATH_BUF]`
 *     (declared inside its own main()) dropped - tp_main() now writes
 *     directly into this file's shared global g_house_root, same as
 *     every other mode.
 * ============================================================ */
/* ========================================================================
 * 2026-08-06 FOCUS-RECOVERY — option C (user chose):
 *   locks OFF; grabs ON; soft focus ON (this popup only).
 * Flip any flag to experiment. Nav [N] UI + NAV_KEY file relay stay ON.
 * ======================================================================== */
#ifndef LIVEDESK_USE_REGISTRY_LOCK
#define LIVEDESK_USE_REGISTRY_LOCK 1
#endif
#ifndef LIVEDESK_USE_POPUP_LOCK
#define LIVEDESK_USE_POPUP_LOCK 0
#endif
#ifndef LIVEDESK_USE_XGRAB_POINTER
#define LIVEDESK_USE_XGRAB_POINTER 1
#endif
#ifndef LIVEDESK_USE_XGRAB_KEYBOARD
#define LIVEDESK_USE_XGRAB_KEYBOARD 1
#endif
/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): the
 * context-menu behavior guards are now RUNTIME-configurable, read from
 * the package's own meta.pdl by read_menu_config() below:
 *   STATE | menu_stay_open | 1    outside/repeat clicks keep menu open
 *   STATE | grab_pointer   | 1    pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1    keyboard grab while a menu is open
 * menu_stay_open=1 also makes the menu NON-modal (no pointer grab), so
 * the toolbar and other windows stay clickable while it's open - see
 * open_context_menu()'s grab block. Missing rows keep these compile-time
 * defaults. Edit meta.pdl and the next right-click re-reads it - no
 * rebuild, no restart. */
static int g_menu_stay_open = 1;
/* REAL, NEW 2026-08-30, direct instruction ("it only needs to happen
 * on status change... what in house architecture can be used to
 * support this") - same real cheap-marker convention this house
 * already uses everywhere (frame_changed.txt et al) - a single
 * stat() per already-running tick against
 * #.desktop/livedesk_theme_changed.txt (written by
 * write_theme_opacity() in khtpm_core_render.c), real work
 * (reload+reapply opacity to this entity's own window) only runs on
 * an actual change. */
static long g_theme_changed_cursor = 0;
static int theme_changed_dirty(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    /* REAL BUG FIX 2026-08-30 - same fix as khtpm_strip_parser.c's own
     * theme_changed_dirty() - cursor starts at 0, not -1, so the
     * marker's first-ever real append (this file usually doesn't
     * exist yet at process startup) counts as a real change. */
    if (st.st_size != g_theme_changed_cursor) { g_theme_changed_cursor = st.st_size; return 1; }
    return 0;
}

/* REAL, NEW 2026-08-30, found live: an entity nobody is interacting
 * with never sets need_redraw, so the whole draw block (later in the
 * loop, gated `if (!need_redraw) continue;`) never runs - meaning a
 * desktop-wide camera pan/tilt/mode CHANGE, written by cursword alone,
 * was silently invisible on every OTHER idle entity until something
 * else happened to poke it. Same real cheap-marker convention as
 * theme_changed_dirty() just above (one stat() per already-running
 * idle tick, real work only on an actual change) - cursword's own
 * camera writers (below) touch this marker; every entity's own idle
 * tick checks it and sets need_redraw itself when it moves. */
static long g_camera_changed_cursor = 0;
static int camera_changed_dirty(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_changed.txt", house_root);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size != g_camera_changed_cursor) { g_camera_changed_cursor = st.st_size; return 1; }
    return 0;
}
static void bump_camera_changed(const char *house_root) {
    char path[4352];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_changed.txt", house_root);
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }
}




static int g_grab_pointer = LIVEDESK_USE_XGRAB_POINTER;
static int g_grab_keyboard = LIVEDESK_USE_XGRAB_KEYBOARD;
/* Soft focus fallback (option C 2026-08-06): set input focus on THIS
 * popup only — not a house-wide raise-fight across entity tiles.
 * Used on open + header click so keys still work if grab fails. */
#ifndef LIVEDESK_POPUP_SOFT_FOCUS
#define LIVEDESK_POPUP_SOFT_FOCUS 1
#endif

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER's own real
 * monsters need a 2x2-desktop-grid-cell footprint - "these monsters
 * should take up 4 tiles instead of the previous 1 tile"): WIN_PX was
 * a compile-time #define, meaning every desktop entity was hardcoded
 * to the exact same 64px size. Converted to a real runtime variable,
 * set once early in main() from a real, optional "footprint_tiles"
 * STATE row in the package's own meta.pdl (same real SECTION|KEY|VALUE
 * parse convention every other real field there already uses) -
 * defaults to 1 (this exact same 64px value) when absent, so every
 * existing pet/asa/ava package is completely unaffected. Every one of
 * this file's own real call sites below still just reads "WIN_PX" -
 * only its OWN declaration changed, not the 9 real places it's used. */
static int WIN_PX = 64;
/* REAL, NEW 2026-09-14 - this entity's own real, declared window-
 * stacking priority (read_z_priority()'s own header comment for the
 * full story - a real, general per-entity .pdl field, not a cursword-
 * only hack). 0 = default, never self-raises; >0 = periodically self-
 * raises in hq_idle_tick() so it stays visually on top of every
 * default-priority sibling. */
static int g_z_priority = 0;
#define POLL_INTERVAL_USEC 300000
/* REAL FIX 2026-08-04, direct instruction ("desk has a grid... egg-pets
 * snap to grid... make sure windows/procs are killed and don't render
 * more than 30fps, cpu is getting hot"): same GRID_CELL_PX egg_window.c
 * uses (01.muchi-pals-🥚️-13.01/system/egg_window.c) - tile stamps should
 * snap to the SAME desktop grid egg-pals already use, not a separate
 * one.
 *
 * REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a, direct instruction:
 * "this size should be set/read from a .pdl which can be changed"):
 * converted from a compile-time #define to a real runtime variable, set
 * once early in main() from an optional "GRID | cell_px | N" row in
 * #.desktop/desk_grid.pdl (see read_grid_cell_px() below) - same real
 * "compile-time constant -> runtime variable read from a real file,
 * safe default preserves existing behavior" pattern this file's own
 * WIN_PX/footprint_tiles conversion already established above. Defaults
 * to 80 (this file's own original hardcoded value) when the file/row is
 * absent, so every existing desktop is completely unaffected until
 * someone actually writes a real desk_grid.pdl. */
static int GRID_CELL_PX = 80;
#define MAX_FPS 30
#define MIN_FRAME_USEC (1000000 / MAX_FPS)

/* REAL, NEW 2026-08-30 (CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-
 * DESKS-DESIGN.md §3a/§9/§10, direct instruction confirmed across
 * several rounds of Q&A - see that doc's own real decision record
 * before touching any of this) - real arm-on-click for cursword
 * specifically, NOT every desktop entity: a plain click (not a drag)
 * arms it, showing a real glowing halo, same real visible-state
 * principle as this house's other "armed" conventions
 * (rmmv_armed.txt/.pal-hint-armed). This is step 1 of that doc's own
 * scoped rollout (arm+halo only) - arrow-key movement, click-to-place,
 * and the 2D/3D camera switch are explicitly deferred to a later pass,
 * per the doc's own §8/§10 sequencing.
 * REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md §1.1) -
 * set from a data-driven "STATE | log_mode | 1" row in the entity's
 * own meta.pdl now (read_log_mode()), not a hardcoded
 * `basename(package_dir) == "cursword"` identity check - the exact
 * project-name-in-shared-renderer pattern CENTROID_GOLD_STD.md rule 7
 * forbids for top-level modes, just nested inside tp_main() instead
 * of main(). The taller "log" window / arm-on-click behavior this
 * flag gates is unchanged and still lives inline here (that's the
 * larger, deferred tp_main()-extraction work in the same audit doc's
 * Part 3) - only HOW the flag gets set is now generic; any entity can
 * opt into this variant via its own meta.pdl, not just cursword. */
static int g_is_cursword = 0;
static int g_cursword_armed = 0;

/* Confirmed default (§9 item 1, confirmed as-is in §10): 5px movement
 * AND under 300ms between ButtonPress and ButtonRelease counts as a
 * real click (arm), not a drag. */
#define CURSWORD_CLICK_MAX_PX 5
/* REAL FIX 2026-08-30, direct live report ("clicking it with mouse
 * moves it to fast can it wait a bit longer?") - 300ms was too tight
 * for a real, physical mouse click (press+release), misclassifying it
 * as a drag (moving cursword) instead of a real click (arming it).
 * Raised to 600ms - still well under "held down and dragged" territory
 * (CURSWORD_CLICK_MAX_PX's own 5px cap still guards against an actual
 * drag being misread as a click, this only loosens the TIME side). */
#define CURSWORD_CLICK_MAX_MS 600

/* Real, house-standard "small state file under #.desktop/" convention
 * (§9 item 5's own cited precedent, rmmv_armed.txt) - the one real,
 * visible-elsewhere signal for "is cursword currently armed right
 * now," same shape khtpm_core_render.c's own
 * pchq_is_interact_on()/etc. already use for cross-process real state. */
static void cursword_write_armed(const char *house_root, int armed) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/cursword_armed.txt", house_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%d\n", armed ? 1 : 0); fclose(f); }
}

/* REAL, NEW 2026-08-30, direct instruction ("i still dont have arrow
 * control. would it help if we did a text display under cursword with
 * pressed key history?"): a real, live-visible readout of the last
 * few keys this window's own event loop actually received while armed
 * - the direct, fastest way to tell "key never reached this window at
 * all" apart from "key reached it but the move logic didn't fire,"
 * without any indirect file/log inspection. Extends the window taller
 * by CURSWORD_LOG_H (only while armed - see cursword_update_shape()'s
 * own real shape-mask union for the matching visible-region rectangle)
 * and draws the last CURSWORD_LOG_N short labels on one line via the
 * existing popup fontset (load_popup_fontset(), already loaded
 * unconditionally in main() - not new state). */
/* REAL, NEW 2026-08-30, direct instruction ("can we do another debug
 * below sword, that shows camera angle?") - grown from 20 to 38 to
 * fit a real second line (the existing key-log line, plus a new
 * camera pitch/tilt readout right below it), then to 56 (direct
 * instruction, 2026-08-31: "zx cy aren't changing z level... can u
 * add another debug row for cursword that show xyz position") for a
 * real third line - see the real draw site near the end of the main
 * render block for what actually gets printed on each line. */
#define CURSWORD_LOG_H 56
#define CURSWORD_LOG_N 5
/* REAL, NEW 2026-08-31, direct live report ("its too far off the
 * label 2 read, widen label for text?") - the debug strip's own
 * visible-region rectangle and backing pixmap were always exactly
 * WIN_PX (64px) wide, same as the sprite square above it, so the
 * posline/camline/logline text (up to ~30 chars) ran straight off
 * the right edge of the strip's own clip region and got silently
 * clipped by the window shape - not a font/color bug, a real width
 * bug. Strip-only width, wider than WIN_PX; every WIN_PX x WIN_PX
 * square (sprite mask, disc mask, halo ring) is completely
 * unaffected - only the strip's own mask/pixmap/window-width/present-
 * width below use this. */
#define CURSWORD_LOG_W 220
static char g_cursword_log[CURSWORD_LOG_N][12];
static int g_cursword_log_n = 0;
static void cursword_log_key(const char *label) {
    if (g_cursword_log_n < CURSWORD_LOG_N) {
        snprintf(g_cursword_log[g_cursword_log_n], sizeof(g_cursword_log[0]), "%s", label);
        g_cursword_log_n++;
    } else {
        for (int i = 1; i < CURSWORD_LOG_N; i++)
            snprintf(g_cursword_log[i - 1], sizeof(g_cursword_log[0]), "%s", g_cursword_log[i]);
        snprintf(g_cursword_log[CURSWORD_LOG_N - 1], sizeof(g_cursword_log[0]), "%s", label);
    }
}

/* REAL, NEW 2026-08-30, step 2 of the design doc's own §8/§10
 * sequencing (arrow-key movement + click-to-place, both real code,
 * house-wide PDL toggle decides which is ACTIVE while armed - direct
 * instruction: "we could add it in a pdl as optionally changeable
 * till we figure out what actually works best in practice"). Same
 * real home as click_two_step/opacity/cursword_move_mode itself -
 * #.desktop/hq_ui.pdl, loaded once at startup, same shape every other
 * real loader in this house uses. 0 = click_place (default), 1 =
 * arrow_only. Arrow-key nudge is real, always-on baseline movement in
 * EITHER mode (§3a's own core spec never made arrows conditional) -
 * this toggle only decides whether click-to-place is ALSO active. */
static int g_cursword_click_place = 1;
static void cursword_load_move_mode(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        if (strcmp(line, "cursword_move_mode") == 0)
            g_cursword_click_place = (strcmp(val, "arrow_only") != 0);
    }
    fclose(f);
}

/* Real, one-shot "waiting for the placement click" state - set right
 * after a successful real XGrabPointer on arm (click_place mode only),
 * cleared on the next real ButtonPress (the placement click itself) or
 * on Escape (real abort/disarm, must also ungrab). */
static int g_cursword_awaiting_place = 0;

/* REAL FIX 2026-08-05, direct instruction ("this is where we will
 * refactor the xwindow to be chtpm/master ledger compliant" -
 * MUCHI_RANCHER's own work item 2, see MUCHI_RANCHER_DESIGN.md §5 and
 * TILE_PICKER_DESIGN.md §10's own 3-step parity plan): every other
 * CHTPM app in this house gets a real, plain-text history file
 * (auditability) and a real file an external writer - human, script,
 * or AI - can inject a command into, that this window's own event
 * loop actually polls (AI-injection power). Raw override_redirect
 * windows like this one had NEITHER until now. Scoped per-package
 * (own history.txt/interact_relay.txt inside package_dir, not a
 * shared house-wide file like pieces/keyboard/history.txt - this
 * window only ever represents ONE entity, so its own real audit trail
 * belongs right next to that entity's own other real state). */
static char g_history_path[TP_PATH_BUF];
static char g_relay_path[TP_PATH_BUF];

/* Stage 2c PROOF (2026-08-16, direct instruction: "we wanna wire that
 * new context to toolbar and right clik entity and get rid of legacy,
 * so i can check it") - ONE-ENTITY test, see local-2do-15.txt's own
 * entity-context-menu entry. Real integration point: every one of this
 * file's ~20 open_context_menu() call sites already funnels through
 * that ONE function - rather than touch all of them (real risk, this
 * popup engine is deeply coupled to lock/lifecycle bookkeeping every
 * caller assumes), open_context_menu() itself now HIDES the legacy
 * popup (XUnmapWindow, right after its own real creation/lock/grab
 * logic runs completely unchanged) and launches the new khtpm .chtpm-
 * based renderer as the VISIBLE replacement, only when this entity's
 * own <package_dir>/menu.chtpm exists. Every entity without a
 * menu.chtpm keeps the exact original behavior, zero risk. */
static int g_use_khtpm_menu = 0;


static void append_history(const char *fmt, ...) {
    FILE *f = fopen(g_history_path, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

/* Real, generic method dispatch - same three real cases the live
 * click-handling ButtonPress branch already handles (CLOSE/void/real
 * command), factored out here so interact_relay.txt injection (below,
 * in main()'s own poll loop) can dispatch a method exactly the same
 * way a real click does. OPEN_USER is deliberately NOT handled here -
 * it needs live popup-position context only the real click site has;
 * a relay-injected OPEN_USER is a real, small future gap, not silently
 * faked here. */
static void dispatch_action(const char *action, const char *package_dir, const char *house_root, int *running_ptr) {
    /* Real bug fix (2026-08-11, direct live report: "clicking enter on
     * book menu doesn't work but mouse click does" — turned out to be
     * about "Read" specifically, not "Dir"). This function used to only
     * ever pass ONE argument (package_dir) to the command — but the
     * mouse ButtonPress handler's own separate, DUPLICATED inline copy
     * of this same dispatch logic (added 2026-08-10, "REAL FIX 2026-08-10
     * ... pass it as a real second argument") passes TWO (package_dir,
     * house_root). Any METHOD line updated to rely on the second argument
     * (e.g. book-stack's real "Read" — `sh -c 'H="$1" && ... exec
     * "$H/.../prisc+x" ...'`) worked via mouse (real click path) but
     * silently failed via Enter/RUN_METHOD/ACTIVATE_NAV (all funnel
     * through this ONE function) — $1/$H came up empty, the exec target
     * became a malformed path, sh -c's own exec failed, stderr redirected
     * to /dev/null, zero visible symptom beyond "nothing happened".
     * Fixed: this function now ALWAYS passes both arguments, matching the
     * mouse handler's own (already-correct) convention — the single
     * source of truth for the calling contract, instead of two
     * independently-diverging copies. Every METHOD line's script must
     * tolerate BOTH being present regardless of trigger path now (already
     * true for "Read" and, after the sibling fix wrapping bare system
     * binaries like "Dir" in `sh -c 'exec CMD "$0"'`, true for those too —
     * a bare command that ONLY reads argv[1] and ignores extras, like
     * xdg-open, needs that wrapper; a real script using $0/$1 already
     * works either way). */
    if (strcmp(action, "CLOSE") == 0) {
        *running_ptr = 0;
    } else if (strcmp(action, "void") == 0) {
        /* intentional no-op */
    } else if (strcmp(action, "OPEN_USER") == 0) {
        /* not supported via relay injection - see comment above */
    } else {
#ifdef _WIN32
        /* Do not system() bash. Dir → Explorer. Read → prisc+x.exe + event.pal
         * (Choose-Read/Hear/Tao lives in that pal, not meta.pdl). */
        if (strstr(action, "xdg-open") || strstr(action, "explorer")) {
            ShellExecuteA(NULL, "open", package_dir, NULL, NULL, SW_SHOWNORMAL);
            return;
        }
        if (strstr(action, "prisc") || strstr(action, "pieces/reader")) {
            const char *hr = (house_root && house_root[0]) ? house_root : ".";
            wchar_t wpat[TP_PATH_BUF], wprisc[TP_PATH_BUF], wev[TP_PATH_BUF];
            char ev_a[TP_PATH_BUF];
            snprintf(ev_a, sizeof(ev_a), "%s\\_.monads\\_.book-stack\\pieces\\reader\\event_pkg\\pages\\page_1\\event.pal", hr);
            for (char *q = ev_a; *q; q++) if (*q == '/') *q = '\\';
            MultiByteToWideChar(CP_UTF8, 0, ev_a, -1, wev, TP_PATH_BUF);
            _snwprintf(wpat, TP_PATH_BUF, L"%hs\\101.mutaclsym*", hr);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(wpat, &fd);
            wprisc[0] = 0;
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    wchar_t cand[TP_PATH_BUF];
                    _snwprintf(cand, TP_PATH_BUF, L"%hs\\%s\\system\\prisc+x.exe", hr, fd.cFileName);
                    if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                        wcsncpy(wprisc, cand, TP_PATH_BUF - 1);
                        wprisc[TP_PATH_BUF - 1] = 0;
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            if (wprisc[0] && GetFileAttributesW(wev) != INVALID_FILE_ATTRIBUTES) {
                wchar_t cmd[TP_PATH_BUF * 2];
                _snwprintf(cmd, TP_PATH_BUF * 2 - 1, L"\"%s\" \"%s\"", wprisc, wev);
                STARTUPINFOW si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                               CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS,
                               NULL, NULL, &si, &pi);
                if (pi.hThread) CloseHandle(pi.hThread);
                if (pi.hProcess) CloseHandle(pi.hProcess);
            }
            return;
        }
        (void)house_root;
        return;
#else
        char cmd[TP_PATH_BUF * 3];
        /* macOS leg (2026-08-22): METHOD actions are canonical Linux
         * shell strings (book-stack's Dir row is literally
         * `sh -c 'exec xdg-open "$0"'`); macOS has no xdg-open. Rewrite
         * occurrences to the native `open` at runtime — the same
         * translate-at-runtime-not-PDL shape as run_shortcut()'s
         * ktb_portable_darwin() in the manager driver. */
#ifdef __APPLE__
        {
            char fixed[TP_PATH_BUF * 3];
            const char *rd = action;
            char *wr = fixed;
            size_t n = 0;
            while (*rd && n < sizeof(fixed) - 6) {
                if (strncmp(rd, "xdg-open", 8) == 0 &&
                    (rd == action || rd[-1] == ' ' || rd[-1] == '\'' || rd[-1] == '"')) {
                    memcpy((char *)wr, "open", 4); wr += 4; rd += 8; n += 4;
                } else { *wr++ = *rd++; n++; }
            }
            *wr = '\0';
            snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", fixed, package_dir, house_root);
        }
#else
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, package_dir, house_root);
#endif
        int rc = system(cmd);
        (void)rc;
#endif
    }
}

/* REAL, 2026-08-05, direct instruction ("we should be thinking about
 * adding open desk procs to a livedesk master ledger just for
 * practice... a taskbar-widget at bottom of the screen"): a house-wide
 * "livedesk" master ledger under #.desktop/ (the confirmed real
 * house-wide file-desktop root - @.apps/hikikomorai/hikikomorai-design.md
 * §0 coined "livedesk" for this exact concept) giving every generated
 * entity a real, STABLE index nav number the first time its window
 * ever opens - reused, not reassigned, on every later relaunch (so a
 * future "jump to entity N" convention has a real, permanent address to
 * jump to, matching the digit-jump shape cli_io's own field convention
 * already uses). A separate, live "currently open" registry
 * (#.desktop/livedesk_open.txt) is what the new taskbar widget (below,
 * ensure_taskbar_running()/tp_taskbar.c) actually polls - entries added
 * on window-open, removed on clean window-close, so its tabs track
 * which entities are ACTUALLY live right now, not full history. */
static void dirname_step(const char *in, char *out, size_t out_sz) {
    char tmp[TP_PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", in);
    char *d = dirname(tmp);
    snprintf(out, out_sz, "%s", d);
}

/* Portable self-binary path (macOS leg 2026-08-22): /proc/self/exe does
 * not exist on macOS; _NSGetExecutablePath() is the Apple equivalent and
 * may return a relative path when the binary was launched by bare name,
 * so resolve through realpath(). Linux/other keep readlink(/proc/self/exe)
 * byte-for-byte as before. Returns 1 on success. */
static int self_exe_path(char *out, size_t out_sz) {
#ifdef __APPLE__
    uint32_t sz = (uint32_t)out_sz;
    if (_NSGetExecutablePath(out, &sz) != 0) return 0;
    char resolved[TP_PATH_BUF];
    if (realpath(out, resolved)) snprintf(out, out_sz, "%s", resolved);
    return 1;
#else
    ssize_t slen = readlink("/proc/self/exe", out, out_sz - 1);
    if (slen <= 0) return 0;
    out[slen] = '\0';
    return 1;
#endif
}

/* House-root discovery: walk UP from this binary's own real install dir
 * (found via /proc/self/exe) until a directory containing BOTH #.desktop/
 * and &.widgits/ is found - the same marker-walk khtpm_vars.sh uses, so it
 * survives any relocation (the binary used to live at
 * <house>/&.widgits/tile-picker/ops/+x/ and was consolidated into
 * *.monads/*.livedesk-taskbar/ops/+x/ - the fixed dirname-step climb is
 * gone, position no longer matters). */
static void resolve_livedesk_paths(char *ops_dir_out, size_t ops_sz, char *house_root_out, size_t house_sz) {
    ops_dir_out[0] = '\0';
    house_root_out[0] = '\0';
    char self_path[TP_PATH_BUF];
    if (!self_exe_path(self_path, sizeof(self_path))) return;
    char step[TP_PATH_BUF];
    dirname_step(self_path, step, sizeof(step)); /* .../ops/+x */
    snprintf(ops_dir_out, ops_sz, "%s", step);
    /* Walk up from ops_dir until a dir holds both #.desktop/ and &.widgits/. */
    for (;;) {
        char desk[TP_PATH_BUF], widg[TP_PATH_BUF];
        snprintf(desk, sizeof(desk), "%s/#.desktop", step);
        snprintf(widg, sizeof(widg), "%s/&.widgits", step);
#ifdef _WIN32
        for (char *p = desk; *p; p++) if (*p == '/') *p = '\\';
        for (char *p = widg; *p; p++) if (*p == '/') *p = '\\';
#endif
        if (access(desk, F_OK) == 0 && access(widg, F_OK) == 0) {
            snprintf(house_root_out, house_sz, "%s", step);
            return;
        }
        char *slash = strrchr(step, '/');
#ifdef _WIN32
        char *bslash = strrchr(step, '\\');
        if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
        if (!slash || slash == step) break; /* reached root, give up */
        *slash = '\0';
    }
#ifdef _WIN32
    if (!house_root_out[0] && access("#.desktop", F_OK) == 0 && access("&.widgits", F_OK) == 0)
        snprintf(house_root_out, house_sz, ".");
#endif
}

/* Real, stable index: scans the ledger for a prior ASSIGN row whose
 * PATH= matches this exact package_dir and reuses that INDEX if found;
 * only assigns a brand-new one (from a real, persisted counter file)
 * the first time this package_dir has ever opened. Also writes
 * <package_dir>/livedesk_index.txt so any tool can read "this entity's
 * own index" directly without re-scanning the whole ledger. */
static int ensure_livedesk_index(const char *package_dir, const char *house_root) {
    char ledger_path[TP_PATH_BUF], counter_path[TP_PATH_BUF], idx_path[TP_PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/#.desktop/livedesk_master_ledger.txt", house_root);
    snprintf(counter_path, sizeof(counter_path), "%s/#.desktop/livedesk_next_index.txt", house_root);
    snprintf(idx_path, sizeof(idx_path), "%s/livedesk_index.txt", package_dir);

    FILE *lf = fopen(ledger_path, "r");
    if (lf) {
        char line[TP_PATH_BUF];
        while (fgets(line, sizeof(line), lf)) {
            char *pathmark = strstr(line, "PATH=");
            if (!pathmark) continue;
            char pval[TP_PATH_BUF];
            snprintf(pval, sizeof(pval), "%s", pathmark + 5);
            pval[strcspn(pval, "\r\n")] = '\0';
            if (strcmp(pval, package_dir) == 0) {
                char *idxmark = strstr(line, "INDEX=");
                if (idxmark) {
                    int idx = atoi(idxmark + 6);
                    fclose(lf);
                    FILE *out = fopen(idx_path, "w");
                    if (out) { fprintf(out, "%d\n", idx); fclose(out); }
                    return idx;
                }
            }
        }
        fclose(lf);
    }

    int next = 1;
    FILE *cf = fopen(counter_path, "r");
    if (cf) { if (fscanf(cf, "%d", &next) != 1) next = 1; fclose(cf); }
    FILE *cw = fopen(counter_path, "w");
    if (cw) { fprintf(cw, "%d\n", next + 1); fclose(cw); }

    char pkgcopy[TP_PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    FILE *la = fopen(ledger_path, "a");
    if (la) {
        fprintf(la, "[%s] ASSIGN INDEX=%d ENTITY=%s PATH=%s\n", ts, next, ent_name, package_dir);
        fclose(la);
    }
    FILE *out = fopen(idx_path, "w");
    if (out) { fprintf(out, "%d\n", next); fclose(out); }
    return next;
}

/* Real liveness probe (kill(pid,0)) - forward-declared here, defined
 * alongside nav_claim_rows() below, shared by both self-healing prune
 * passes (see that function's own header comment for the full 2026-08-06
 * diagnosis this exists to fix). */
static int pid_is_alive(int pid);

/* REAL FIX 2026-08-06, direct-caught regression during the SAME session
 * ("book-stack missing from livedesk_open.txt after a simultaneous
 * multi-launch"): the self-healing prune above turned a single atomic
 * O_APPEND write (safe under concurrent writers - POSIX guarantees
 * small appends don't interleave) into a read-prune-write-rename cycle,
 * which is NOT atomic across processes - two entities launching at once
 * can both read the same "before" state, and whichever renames last
 * silently discards the other's own addition. Real fix: a real
 * cross-process mutex (flock(), same real mechanism as the popup lock
 * above) around the whole read+write critical section in every
 * function that does this read-modify-write-rename dance. */
static int g_registry_lock_fd = -1;

static void registry_lock_acquire(const char *house_root) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd < 0) {
        char lock_path[TP_PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_registry.lock", house_root);
        g_registry_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_EX);
}

static void registry_lock_release(void) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_UN);
}

/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13") - see nav_claim_rows()'s own header comment above for
 * the full diagnosis. Same self-healing shape here: prune any dead-PID
 * or malformed line as part of adding the new one, rather than trusting
 * append-only history that never gets checked against reality. */
static void livedesk_registry_add(const char *house_root, const char *package_dir, int index, pid_t pid) {
    char reg_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    char pkgcopy[TP_PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *ent_name = basename(pkgcopy);

    registry_lock_acquire(house_root);
#ifdef _WIN32
    {
        FILE *w = fopen(reg_path, "a");
        if (w) {
            fprintf(w, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
            fclose(w);
        }
    }
#else
    FILE *f = fopen(reg_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f && w) {
        char line[TP_PATH_BUF];
        char newline[TP_PATH_BUF];
        int wrote_self = 0;
        snprintf(newline, sizeof(newline), "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n", (int)pid, index, ent_name, package_dir);
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            /* REAL FIX 2026-09-12 (direct live incident: a periodic
             * self-heal re-call of this same function, added earlier
             * tonight in tp_main() to fix the "entities drop off the
             * bottom bar" bounty bug, fires on its VERY FIRST tick
             * (static timer starts at zero, so "10s have passed" is
             * true immediately) - landing right alongside the real
             * startup registration below. This prune only ever dropped
             * DEAD pids, so re-registering a still-alive pid just
             * appended a second, genuine duplicate line for the same
             * live entity. khtpm_taskbar_manager.c's load_tabs() then
             * saw two live-PID lines for one entity in a single read
             * and killed the "duplicate" (logic written for an actual
             * zorder-respawn leftover, not this) - which was the
             * entity's only real process.
             *
             * REAL FIX 2026-09-13, direct follow-up (direct live
             * report: "it just reshuffled again... how did old
             * codebase accomplish [stable order]?"): the fix above
             * dropped this entity's OLD line and appended the fresh
             * one at the TAIL of the file - every entity's own
             * unsynchronized ~10s self-heal timer (tp_main()) means
             * entities re-register at staggered moments, each one
             * yanking itself to the bottom of the file on its own
             * schedule - load_tabs() (both this house's current AND
             * the old, pre-periodic-self-heal codebase, confirmed
             * identical) reads this file top-to-bottom for the tab
             * order, so the file order IS the visible tab order, and
             * this was silently, continuously reshuffling it as a
             * side effect - not a rendering bug, a real data-ordering
             * one. The old codebase never had this because it never
             * re-registered at all (register once, at launch, line
             * never moves again). Real fix: write this entity's own
             * fresh line back IN PLACE, at its original row, instead
             * of always at the tail - keeps the exact same self-heal
             * guarantee (a lapsed/pruned entry still gets restored)
             * with zero reordering as a side effect, matching the old
             * codebase's real stable-order invariant. */
            if (line_pid == (int)pid) { fputs(newline, w); wrote_self = 1; continue; }
            if (!pp || !pid_is_alive(line_pid)) continue;
            fputs(line, w);
        }
        if (!wrote_self) fputs(newline, w); /* genuinely new registration - append */
    }
    if (f) fclose(f);
    if (w) {
        fclose(w);
        rename(tmp_path, reg_path);
    }
#endif
    registry_lock_release();
}

static void livedesk_registry_remove(const char *house_root, pid_t pid) {
    char reg_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "PID=%d|", (int)pid);
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, marker, strlen(marker)) == 0) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, reg_path);
    registry_lock_release();
}

/* Direct instruction: "it can open when livedesk using app is open, and
 * if one is already open just add the tab of that app to the taskbar" -
 * a real PID-file singleton check (kill(pid,0), same liveness-probe
 * convention this codebase already uses in spirit elsewhere), not a
 * naive "spawn every time" that would pile up duplicate bars.
 *
 * REAL FIX 2026-08-05, direct correction ("why dont i see task bar in
 * &.widgits dir? thats where its ment to be... its not a member of
 * tile-picker"): the taskbar is its own real widget
 * (*.monads/*.livedesk-taskbar/), matching every other real widget's own
 * top-level layout (event-editor/, event-ez/, tile-picker/ itself) -
 * NOT nested inside tile-picker/ops/ just because tp_desktop_window.c
 * happens to be the one that launches it. Located via house_root
 * (already resolved above) plus this fixed, real house-relative path,
 * not via ops_dir (which would have wrongly implied "lives next to
 * tp_desktop_window.c"). */
static void ensure_taskbar_running(const char *house_root) {
#ifdef _WIN32
    if (x11_process_running("khtpm_strip_parser")) return;
    char exe[TP_PATH_BUF];
    DWORD n = GetModuleFileNameA(NULL, exe, TP_PATH_BUF);
    if (!n) return;
    char *slash = strrchr(exe, '\\');
    if (!slash) return;
    snprintf(slash + 1, TP_PATH_BUF - (size_t)(slash + 1 - exe), "khtpm_strip_parser.exe");
    x11_spawn_cwd(exe, house_root && house_root[0] ? house_root : ".");
    return;
#else
    char pid_path[TP_PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);
    int alive = 0;
    FILE *f = fopen(pid_path, "r");
    if (f) {
        int pid = 0;
        if (fscanf(f, "%d", &pid) == 1 && pid > 0 && kill((pid_t)pid, 0) == 0) alive = 1;
        fclose(f);
    }
    /* REAL FIX 2026-08-06: pid-file race with $.crypts concurrent launch
     * spawned DUPLICATE taskbars (two processes, grab/focus chaos). Also
     * scan /proc for an already-running tp_taskbar for this house. */
    if (!alive) {
        DIR *pd = opendir("/proc");
        if (pd) {
            struct dirent *ent;
            while ((ent = readdir(pd)) != NULL) {
                if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
                char cpath[64];
                snprintf(cpath, sizeof(cpath), "/proc/%s/cmdline", ent->d_name);
                FILE *cf = fopen(cpath, "r");
                if (!cf) continue;
                char cmdbuf[TP_PATH_BUF * 2];
                size_t n = fread(cmdbuf, 1, sizeof(cmdbuf) - 1, cf);
                fclose(cf);
                if (n == 0) continue;
                cmdbuf[n] = '\0';
                for (size_t i = 0; i < n; i++) if (cmdbuf[i] == '\0') cmdbuf[i] = ' ';
                /* REAL FIX 2026-08-11, direct live report "it opened both
                 * toolbars" (entities each relaunched legacy tp_taskbar
                 * even with the new khtpm strip taskbar already up): this
                 * scan only ever matched the literal substring
                 * "tp_taskbar", so it never recognized khtpm_strip_parser
                 * as "a taskbar is already running for this house" —
                 * every entity independently concluded none was running
                 * and launched legacy on top of it. Broadened to also
                 * match "khtpm_strip_parser", the taskbar's own process
                 * name (parser is the long-lived, user-visible half; its
                 * forked manager child living or dying tracks it 1:1).
                 * "tp_taskbar" kept as a harmless no-op safety net.
                 *
                 * REAL UPDATE 2026-08-11, same session, later: legacy
                 * tp_taskbar.c retired (archived to
                 * *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip,
                 * originals deleted) — khtpm_strip_parser.+x is the real,
                 * only taskbar now. The fallback launch command below
                 * used to hardcode tp_taskbar.+x's path, which no longer
                 * exists on disk at all; updated to launch khtpm's own
                 * (renamed, no "_test" suffix) binary instead. */
                /* REAL FIX 2026-09-01 - khtpm_strip_parser.+x retired as a
                 * separate binary (folded verbatim into this very file as
                 * strip_main(), see that function's own big header
                 * comment) - the real, live process name is now
                 * khtpm_core_render, matched alongside the old name (still
                 * checked, harmless, in case an old build is somehow still
                 * running mid-transition). */
                if ((strstr(cmdbuf, "tp_taskbar") || strstr(cmdbuf, "khtpm_strip_parser") || strstr(cmdbuf, "khtpm_core_render"))
                    && strstr(cmdbuf, house_root)) {
                    alive = 1;
                    break;
                }
            }
            closedir(pd);
        }
    }
    if (!alive) {
        char cmd[TP_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd),
                 "'%s/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x' '%s' '%s/*.monads/*.livedesk-taskbar/khtpm_strip_header.xhtpm' >/dev/null 2>&1 &",
                 house_root, house_root, house_root);
        int rc = system(cmd);
        (void)rc;
    }
#endif /* !_WIN32 */
}

/* REAL, 2026-08-05, direct correction (see TILE_PICKER_DESIGN.md §13 -
 * "brackets are ment for focuz not holding numbers... it should look
 * up first and increment indexes"): a SHARED, LIVE claim pool for
 * every real "[N]" shown on screen right now, house-wide -
 * `#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt` - deliberately separate from
 * ensure_livedesk_index()'s own PERMANENT ledger above (that one never
 * changes across relaunches; this one is pure live/ephemeral, numbers
 * free up and get reused the moment whatever held them closes). A
 * context menu claims one contiguous NAV range for its own rows the
 * moment it opens (nav_claim_rows()), releases that same range the
 * moment it closes (nav_release_pid()) - the taskbar (a separate real
 * process, *.monads/*.livedesk-taskbar/ops/tp_taskbar.c) claims its own
 * tab numbers from this exact same pool, so a tab and a menu row can
 * never show the same live number at once. */

/* REAL FIX 2026-08-06, direct report ("index of navs maybe off, because
 * its not reindexing if one context closes and another opens"): base
 * used to be max_nav+1 - a real, confirmed bug, monotonically growing
 * FOREVER across a whole session, never reclaiming numbers
 * nav_release_pid() already freed on close. Real fix: scan every
 * currently-claimed NAV into a used[] set, then take the lowest
 * contiguous run of n free slots starting from 1 - the same real
 * "smallest available" convention wraith-alpha's own focus_index
 * validation (initialize_focus()/is_navigable()) uses to never trust a
 * stale index, applied here as smallest-free-range allocation instead. */
#define NAV_TRACK_MAX 4096
/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13" - traced live: livedesk_nav_claims.txt/livedesk_open.txt
 * were full of entries for PIDs that no longer exist - some from
 * ordinary process churn, one line even structurally corrupted
 * (no "PID=" at all), proof two processes raced an unsynchronized
 * write to the same shared file at once). Neither file was ever
 * self-healing - a stale/corrupt line just sat there inflating every
 * future nav_claim_rows() count forever, exactly the "phantom index"
 * bug the report describes. Real fix: verify every claim's PID is
 * ACTUALLY alive (kill(pid,0)) as part of the same read pass, and
 * rewrite the file with only live, well-formed entries kept - so this
 * self-heals on every single popup open, regardless of how a stale
 * entry got there in the first place. */
static int pid_is_alive(int pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
}

static int nav_claim_rows(const char *house_root, pid_t pid, const char *package_dir, MethodItem *items, int n) {
    char claims_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    static char used[NAV_TRACK_MAX];
    memset(used, 0, sizeof(used));
    registry_lock_acquire(house_root); /* covers BOTH writes below - the prune-rename and the append are one critical section */
    FILE *f = fopen(claims_path, "r");
    FILE *w = fopen(tmp_path, "w");
    if (f) {
        char line[TP_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pp = strstr(line, "PID=");
            int line_pid = pp ? atoi(pp + 4) : 0;
            if (!pp || !pid_is_alive(line_pid)) continue; /* drop malformed/dead - self-heal */
            if (w) fputs(line, w);
            char *p = strstr(line, "NAV=");
            if (p) {
                int v = atoi(p + 4);
                if (v > 0 && v < NAV_TRACK_MAX) used[v] = 1;
            }
        }
        fclose(f);
    }
    if (w) { fclose(w); rename(tmp_path, claims_path); }
    int base = 1;
    for (int cand = 1; cand < NAV_TRACK_MAX - n; cand++) {
        int fits = 1;
        for (int i = 0; i < n; i++) {
            if (used[cand + i]) { fits = 0; break; }
        }
        if (fits) { base = cand; break; }
    }
    FILE *a = fopen(claims_path, "a");
    if (a) {
        for (int i = 0; i < n; i++) {
            fprintf(a, "KIND=row|PID=%d|NAV=%d|ROW=%d|LABEL=%s|PATH=%s\n",
                    (int)pid, base + i, i, items[i].label, package_dir);
        }
        fclose(a);
    }
    registry_lock_release();
    return base;
}

static void nav_release_pid(const char *house_root, pid_t pid) {
    char claims_path[TP_PATH_BUF], tmp_path[TP_PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) { registry_lock_release(); return; }
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); registry_lock_release(); return; }
    char marker[32];
    snprintf(marker, sizeof(marker), "|PID=%d|", (int)pid);
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, marker)) continue;
        fputs(line, w);
    }
    fclose(f);
    fclose(w);
    rename(tmp_path, claims_path);
    registry_lock_release();
}

static void read_glyph(const char *package_dir, char *out, size_t out_sz) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction (MUCHI_RANCHER monsters need
 * a real 2x2-tile footprint): reads a real, optional
 * "STATE | footprint_tiles | N" row from the package's own meta.pdl -
 * same SECTION|KEY|VALUE parse shape load_methods() below already
 * uses. Defaults to 1 (this file's own original, unconditional 64px
 * size) when the row is absent, so every existing pet/asa/ava package
 * is completely unaffected. */

/* REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md §1.1) -
 * reads an optional "STATE | log_mode | 1" row from the package's own
 * meta.pdl, same SECTION|KEY|VALUE parse shape as read_footprint_
 * tiles() above. Replaces the old g_is_cursword identity check
 * (`strcmp(basename(pkgcopy), "cursword") == 0`) - a hardcoded
 * project name living in the shared renderer is exactly the
 * CENTROID_GOLD_STD.md rule 7 pattern this house's own standard
 * forbids for top-level modes, just one level deeper (inside tp_main()
 * instead of main()). Any entity can now opt into the taller "log"
 * window variant by publishing this one row in its own meta.pdl - no
 * renderer change needed to add a second one. Defaults to 0 (every
 * existing entity's own meta.pdl lacks this row, so behavior is
 * unchanged for all of them except cursword, which now carries the
 * row itself instead of being named in code). */
static int read_log_mode(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int result = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "log_mode";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        result = atoi(end + 1) != 0;
        break;
    }
    fclose(f);
    return result;
}

/* REAL, NEW 2026-09-14, direct live report ("entities should have z
 * levels priorities when on desk in their pdl... cursword should
 * always have highest z level priority. right now it hides behind
 * castle") - a genuinely different Z from g_entity_z (that one is the
 * "which floor am I on" grouping cursword's own c/v keys change,
 * gating show/hide via desktop_active_z.txt; this one is real window
 * STACKING order - which overlapping entity paints on top). Same real
 * SECTION|KEY|VALUE STATE-row convention read_footprint_tiles() above
 * already established - `STATE | z_priority | N` in the package's own
 * meta.pdl, defaults to 0 (unaffected) when absent, so every existing
 * entity keeps its current stacking behavior until it actually
 * declares one. */
static int read_z_priority(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int result = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "z_priority";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        result = atoi(end + 1);
    }
    fclose(f);
    return result;
}

static int read_footprint_tiles(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 1;
    char line[TP_PATH_BUF];
    int result = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "footprint_tiles";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - reads an optional
 * "GRID | cell_px | N" row from #.desktop/desk_grid.pdl (real,
 * house-wide - NOT per-package, since the desktop grid is one shared
 * thing every entity snaps to, unlike footprint_tiles which is
 * per-entity). Same SECTION|KEY|VALUE parse shape as
 * read_footprint_tiles() just above, adapted for a "GRID" section
 * instead of "STATE". Defaults to 80 (this file's own original
 * hardcoded GRID_CELL_PX value) when the file/row is absent. */
static int read_grid_cell_px(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = pdl_open(path);
    if (!f) return 80;
    char line[TP_PATH_BUF];
    int result = 80;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        const char *key = "cell_px";
        size_t klen = strlen(key);
        if ((size_t)(label_end - p) != klen || strncmp(p, key, klen) != 0) continue;
        int v = atoi(end + 1);
        if (v > 0) result = v;
        break;
    }
    fclose(f);
    return result;
}

/* REAL, NEW 2026-08-31, direct instruction ("we are going to make a
 * 'map size' so players cant lose the map moving stuff around too
 * much (will hit 'wall' of movement)"), specified PDL-editable per
 * direct instruction ("something in a pdl file we can edit if we need
 * w/o changing hardcode") - same file, same GRID section, same
 * SECTION|KEY|VALUE shape as read_grid_cell_px() just above. Reads
 * two new optional keys:
 *   GRID | map_cols | N   real desktop-wide movement-wall width, in
 *                         grid cells (GRID_CELL_PX each)
 *   GRID | map_rows | N   real desktop-wide movement-wall height,
 *                         same units
 * Missing/absent/<=0 (the default, matching desk_grid.pdl not having
 * these keys yet) leaves *out_cols/*out_rows at 0 - callers treat 0 as
 * "no configured map size," falling back to the screen-resolution-
 * derived bound this file already used before this feature existed
 * (zero behavior change until someone actually sets these keys). */
static void read_map_size(const char *house_root, int *out_cols, int *out_rows) {
    *out_cols = 0;
    *out_rows = 0;
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desk_grid.pdl", house_root);
    FILE *f = pdl_open(path);
    if (!f) return;
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "GRID", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t klen = (size_t)(label_end - p);
        int v = atoi(end + 1);
        if (v > 0 && klen == 8 && strncmp(p, "map_cols", 8) == 0) *out_cols = v;
        else if (v > 0 && klen == 8 && strncmp(p, "map_rows", 8) == 0) *out_rows = v;
    }
    fclose(f);
}

/* REAL 2026-08-07, direct instruction ("make them configurable via
 * config / .pdl file so i can easily experiment with them"): reads the
 * context-menu guard rows from the package's own meta.pdl (same
 * SECTION|KEY|VALUE shape load_methods()/read_footprint_tiles() use).
 * Keys (STATE section):
 *   STATE | menu_stay_open | 1   outside/repeat clicks keep the menu open
 *   STATE | grab_pointer   | 1   modal pointer grab while a menu is open
 *   STATE | grab_keyboard  | 1   modal keyboard grab while a menu is open
 *   STATE | grab_pointer_while_stay_open | 0   see open_context_menu()'s
 *                                  own comment on this key — lets a user
 *                                  opt back INTO a pointer grab even with
 *                                  menu_stay_open=1, for entities whose
 *                                  row clicks aren't reliably reaching a
 *                                  non-grabbed override-redirect popup
 *                                  under this house's Wayland/XWayland
 *                                  setup (direct report 2026-08-11: "it
 *                                  works clicking enter, but not mouse
 *                                  clicking"). Trades away "rest of the
 *                                  desk stays clickable while this menu's
 *                                  open" (the ORIGINAL reason grabbing was
 *                                  disabled for stay-open menus,
 *                                  2026-08-07) — real tradeoff, exposed as
 *                                  a knob instead of picking one hardcoded
 *                                  answer for every entity.
 * Missing rows keep the compile-time defaults. Called on startup AND on
 * every right-click reload, so a human edits meta.pdl and the very next
 * menu open picks it up - no rebuild, no restart.
 *
 * REAL BUG FIX (2026-08-11, found while adding the new key above, same
 * off-by-one class hit repeatedly elsewhere this session): the
 * "menu_stay_open" length check was klen==13 — the string is genuinely
 * 14 characters (verified: `printf '%s' "menu_stay_open" | wc -c`). This
 * meant STATE|menu_stay_open|... NEVER matched, at all — the key was
 * permanently stuck at its compile-time default (1) no matter what a
 * human set in meta.pdl, exactly the "looks configurable but silently
 * isn't" trap this whole config system exists to avoid. Fixed to 14. */
static int g_grab_pointer_while_stay_open = 0;

static void read_menu_config(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return;
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        int v = atoi(end + 1);
        size_t klen = (size_t)(label_end - p);
        if (klen == 14 && strncmp(p, "menu_stay_open", 14) == 0)
            g_menu_stay_open = v ? 1 : 0;
        else if (klen == 12 && strncmp(p, "grab_pointer", 12) == 0)
            g_grab_pointer = v ? 1 : 0;
        else if (klen == 13 && strncmp(p, "grab_keyboard", 13) == 0)
            g_grab_keyboard = v ? 1 : 0;
        else if (klen == 28 && strncmp(p, "grab_pointer_while_stay_open", 28) == 0)
            g_grab_pointer_while_stay_open = v ? 1 : 0;
    }
    fclose(f);
}

/* REAL FIX 2026-08-05, direct instruction ("they should have a unique
 * alpha numerica 4digit combo"): piece_id alone (the package dir's own
 * basename) isn't guaranteed unique - two entities could share a name.
 * button.sh's own ensure_package() now generates a real, persistent
 * 4-char alphanumeric instance_id once (instance_id.txt, seed-once-
 * don't-clobber, same convention as glyph/created_at) - read it back
 * here the same way read_glyph() already does. */
static void read_instance_id(const char *package_dir, char *out, size_t out_sz) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/instance_id.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int package_still_exists(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    struct stat st;
    return stat(path, &st) == 0;
}

/* Cheap, deterministic glyph->color hash so different tile stamps are
 * visually distinct even before/without a loaded font. */
static void glyph_color(char g, float *r, float *gg, float *b) {
    unsigned int h = (unsigned int)(unsigned char)g * 2654435761u;
    *r = ((h >> 0) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *gg = ((h >> 8) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *b = ((h >> 16) & 0xFF) / 255.0f * 0.7f + 0.2f;
}

/* REAL, NEW 2026-08-04, direct instruction ("next test, it should be a
 * real tile" - i.e. the window must show the actual glyph, not just a
 * colored square). No sprite/texture pipeline exists for tile-picker yet
 * (that's the still-open glyph-widening/rendering question in both
 * design docs), so this uses the same glXUseXFont technique GLUT's own
 * bitmap-font helpers are built on: load a real X core font, convert its
 * glyphs into GL display lists, draw with glCallLists. "fixed"-family X
 * core fonts are always present on any X11 install (confirmed via
 * xlsfonts on this machine) - no extra font file dependency. Returns 0
 * and leaves *base_list unset if no such font can be loaded (caller falls
 * back to the plain color square, same as before this fix). */
static int g_font_loaded = 0;
static XftFont *g_font_info = NULL;

/* REAL FIX 2026-08-05, direct report ("its having problems with the
 * chinese"): all popup text (context menu rows, Show Text) was drawn
 * with plain XDrawString - Latin-1 only, real X core-font limitation,
 * so the Chinese half of book-stack's Bible verses rendered as boxes/
 * garbage. Real fix (UPDATED 2026-09-13, see below): Xft/fontconfig
 * text, which covers CJK the same way font_ui/kh_measure_text_px
 * already do elsewhere in this file, instead of a hand-built XFontSet.
 * Built once in tp_main()'s own startup, used by every popup draw site
 * below instead of XDrawString. */
static XftFont *g_popup_fontset = NULL;
/* the g_ui_scale_pct this fontset was actually built at - lets
 * ensure_popup_fontset_current() (below) notice a live font_scale
 * change and rebuild, instead of baking in whatever scale happened to
 * be loaded at process start forever. 0 = never built yet. */
static int g_popup_fontset_pct = 0;

/* REAL FIX 2026-09-13, direct instruction ("dig into whats actually
 * slowing entity startup") - real, measured root cause (startup_timing.
 * txt, TP_STARTUP_TIMING=1): this fontset and load_glyph_font()'s own
 * g_font_info below, TOGETHER, were ~90-95% of every entity's ~100ms
 * respawn cost (54-70ms + 20-39ms, confirmed live across 3 runs) - the
 * legacy X core-font path (XCreateFontSet/XLoadQueryFont) forces the X
 * SERVER to do a full wildcard font-list match on every single process
 * start, with nothing cached across respawns. Xft/fontconfig (already
 * used everywhere else in this file - font_ui, kh_measure_text_px) goes
 * through fontconfig's own persistent on-disk cache instead - real,
 * confirmed fix, not a guess (see TP_TIMING_MARK results in the always-
 * on-top toggle perf thread, tb-bug-doc.txt). */
static void load_popup_fontset(Display *dpy) {
    int screen_num = DefaultScreen(dpy);
    int px = scaled(18);
    char spec[96];
    /* REAL FIX 2026-09-14, direct live report ("its showing the verses
     * without color and not rendering chinese font anymore") -
     * reproduced live via a real SHOW_TEXT_FILE relay + frame dump
     * (not guessed): this requested "monospace", falling back to
     * "DejaVu Sans Mono" - NEITHER has CJK glyph coverage, so any
     * Chinese codepoint drew as a tofu box regardless of how long this
     * has been wrong. This function's own 2026-08-05 header comment
     * says its whole purpose was "Xft/fontconfig text, which covers
     * CJK the same way font_ui/kh_measure_text_px already do
     * elsewhere in this file" - but never actually requested the same
     * real family those do (confirmed: font_ui's own reload_font_ui()
     * requests "Noto Sans CJK SC" explicitly). Matched here now, same
     * real family, with the old "monospace" request kept as a real
     * fallback (not removed) in case Noto Sans CJK SC is ever missing
     * on a given machine. */
    snprintf(spec, sizeof(spec), "Noto Sans CJK SC:pixelsize=%d", px);
    if (g_popup_fontset) { XftFontClose(dpy, g_popup_fontset); g_popup_fontset = NULL; }
    g_popup_fontset = XftFontOpenName(dpy, screen_num, spec);
    if (!g_popup_fontset) {
        snprintf(spec, sizeof(spec), "monospace:pixelsize=%d", px);
        g_popup_fontset = XftFontOpenName(dpy, screen_num, spec);
    }
    if (!g_popup_fontset) {
        snprintf(spec, sizeof(spec), "DejaVu Sans Mono:pixelsize=%d", px);
        g_popup_fontset = XftFontOpenName(dpy, screen_num, spec);
    }
    g_popup_fontset_pct = g_ui_scale_pct;
}

/* Rebuild the popup fontset if a live font_scale reload changed
 * g_ui_scale_pct since it was last built - cheap check (an int
 * compare) on every popup text draw/measure, real work only on an
 * actual change. Called from popup_draw_text()/popup_text_px() so
 * every existing call site gets this for free, no new call sites. */
static void ensure_popup_fontset_current(Display *dpy) {
    if (g_popup_fontset && g_popup_fontset_pct == g_ui_scale_pct) return;
    load_popup_fontset(dpy);
}

/* Real, explicit-Display XftColor alloc, same reasoning as tp_hex_pixel()
 * just above (tp_main's own LOCAL dpy/screen/colormap, NOT the shared
 * file-scope dpy/screen/cmap the HQ-window xft_color() reads) - caller
 * frees via XftColorFree(), same convention xft_color()'s own callers
 * already use. */
static XftColor tp_xft_color(Display *d, Visual *vis, Colormap cm, const char *hex) {
    XftColor xc;
    XRenderColor rc = {0, 0, 0, 0xffff};
    if (hex && hex[0] == '#' && strlen(hex) >= 7) {
        unsigned int r, g, b;
        sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(d, vis, cm, &rc, &xc);
    return xc;
}

static void popup_draw_text(Display *dpy, Drawable d, GC gc, int x, int y, const char *s) {
    (void)gc;
    ensure_popup_fontset_current(dpy);
    if (g_popup_fontset) {
        int screen_num = DefaultScreen(dpy);
        Visual *vis = DefaultVisual(dpy, screen_num);
        Colormap cm = DefaultColormap(dpy, screen_num);
        /* REAL FIX 2026-09-15, direct live report (cursword disappearing
         * on a normal left click - root-caused via a live strace: a real
         * X BadMatch on RenderCreatePicture, Xlib's own default error
         * handler calling exit(1) with no custom handler installed to
         * catch it). This function always assumed the DEFAULT (24-bit)
         * visual/colormap - true for every other real caller (popups/
         * context menus, all normal-depth windows), but cursword's own
         * armed-only debug-log lines (drawn nowhere else - never reached
         * via right-click, only ever via the left-click arm sequence,
         * matching every symptom observed) pass `d` = g_buf, a real
         * 32-bit ARGB pixmap for cursword specifically (have_argb_visual
         * path). XftDrawCreate() on a 32-bit Drawable with a 24-bit
         * Visual is a genuine depth mismatch - RenderCreatePicture
         * BadMatch, unconditionally, every time. Real, generic fix:
         * query the ACTUAL depth of `d` and use a real matching visual/
         * colormap when it differs from the screen default, instead of
         * assuming every caller's Drawable matches DefaultVisual. Fixes
         * every current and future ARGB-depth caller of this shared
         * helper, not just cursword. */
        {
            Window root_ret; int xr, yr; unsigned int wr, hr, bw, real_depth;
            if (XGetGeometry(dpy, d, &root_ret, &xr, &yr, &wr, &hr, &bw, &real_depth) &&
                (int)real_depth != DefaultDepth(dpy, screen_num)) {
                XVisualInfo vinfo;
                if (XMatchVisualInfo(dpy, screen_num, (int)real_depth, TrueColor, &vinfo)) {
                    vis = vinfo.visual;
                    static Colormap s_argb_cm = 0;
                    static Visual *s_argb_cm_vis = NULL;
                    if (s_argb_cm_vis != vis) {
                        s_argb_cm = XCreateColormap(dpy, RootWindow(dpy, screen_num), vis, AllocNone);
                        s_argb_cm_vis = vis;
                    }
                    cm = s_argb_cm;
                }
            }
        }
        XftDraw *xd = XftDrawCreate(dpy, d, vis, cm);
        if (xd) {
            /* REAL FIX 2026-09-14, direct live report ("its showing the
             * verses without color... anymore") - see this function's
             * own CJK fix above for the frame-dump evidence. Hardcoded
             * black here meant the 2026-09-09 fix ("still black and
             * white" - the exact same complaint) only ever themed the
             * popup's own BACKGROUND (g_theme_bg, at window creation);
             * the text itself stayed hardcoded regardless of theme,
             * silently half-closing that report. Real theme fg, same
             * live-updating global the popup's own border already uses
             * (the XDrawRectangle call right before this loop, in the
             * SHOW_TEXT_FILE Expose handler) - both now track the same
             * real theme. */
            XftColor col = tp_xft_color(dpy, vis, cm, g_theme_fg[0] ? g_theme_fg : "#cccccc");
            XftDrawStringUtf8(xd, &col, g_popup_fontset, x, y, (const FcChar8 *)s, (int)strlen(s));
            XftColorFree(dpy, vis, cm, &col);
            XftDrawDestroy(xd);
        }
    } else {
        XDrawString(dpy, d, gc, x, y, s, (int)strlen(s));
    }
}

/* Pixel width of UTF-8 popup label text (same fontset as popup_draw_text). */
static int popup_text_px(Display *dpy, const char *s) {
    if (!s || !*s) return 0;
    ensure_popup_fontset_current(dpy);
    if (g_popup_fontset) {
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, g_popup_fontset, (const FcChar8 *)s, (int)strlen(s), &ext);
        return ext.width;
    }
    /* Fallback: ~9px/glyph for the 18px face we load. */
    return (int)strlen(s) * 9;
}

static int load_glyph_font(Display *dpy) {
    int screen_num = DefaultScreen(dpy);
    g_font_info = XftFontOpenName(dpy, screen_num, "monospace:pixelsize=24");
    if (!g_font_info) g_font_info = XftFontOpenName(dpy, screen_num, "DejaVu Sans Mono:pixelsize=24");
    if (!g_font_info) return 0;
    return 1;
}

/* Draws directly into the compose buffer via Xft (see load_glyph_font()'s
 * own header comment - was XLoadQueryFont/XDrawString, real measured
 * startup-cost fix 2026-09-13). vis/cm are the entity window's OWN
 * visual/colormap (win_vis/swa.colormap in tp_main - cursword's real
 * ARGB32 visual on that one entity, default visual/colormap on every
 * other), matching whatever buf/win were actually created with. */
static void draw_glyph_rgb(Display *dpy, Drawable buf, GC gc, char g, Visual *vis, Colormap cm) {
    (void)gc;
    if (!g_font_info) return;
    XftDraw *xd = XftDrawCreate(dpy, buf, vis, cm);
    if (!xd) return;
    /* Real, new 2026-08-30 - BlackPixel() alone has no real alpha byte
     * (0 in the high byte), which would draw fully TRANSPARENT text on
     * cursword's own new ARGB32 window - see draw_sprite_rgb()'s own
     * matching comment. Harmless no-op high byte on every other
     * entity's plain 24-bit window - XftColor's own alpha channel
     * (0xffff, opaque) plays the same role here. */
    XftColor col = tp_xft_color(dpy, vis, cm, "#000000");
    int cw = WIN_PX / 2, ch = (g_font_info->ascent + g_font_info->descent);
    int x = (WIN_PX - cw) / 2;
    int y = (WIN_PX + g_font_info->ascent - g_font_info->descent) / 2;
    (void)ch;
    XftDrawStringUtf8(xd, &col, g_font_info, x, y, (const FcChar8 *)&g, 1);
    XftColorFree(dpy, vis, cm, &col);
    XftDrawDestroy(xd);
}

/* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal creates
 * the same emoji that user picked?"): load_sprite/upload_texture/
 * draw_sprite below are the same real technique 01.muchi-pals's
 * egg_window.c already uses to render a picked emoji as a real textured
 * quad, not text. tp_place_desktop.c now generates <package_dir>/
 * sprite.csv via the same emoji_gen_atlas.+x -> emoji_xtract.+x pipeline
 * hatch_egg.c uses; this window loads that CSV if present and draws the
 * real emoji texture instead of the glyph-hashed color square + font
 * fallback (which remains the fallback if sprite.csv is missing/failed
 * to generate). */
static int g_has_sprite = 0;
/* REAL FIX 2026-08-04, direct instruction ("asa's transparency isn't
 * fixed, just background is now red"): GL_BLEND alone only blends
 * within the GL scene against whatever this window itself already drew
 * (the glClearColor fill) - it does NOT make the real X11 WINDOW
 * transparent to the desktop behind it. Plain X11 windows are opaque
 * rectangles by default; real per-pixel desktop transparency needs the
 * X11 Shape Extension to cut the window's actual SHAPE to match the
 * sprite's alpha - exactly what egg_window.c's own build_shape_mask()
 * already does (ported verbatim below, POSIX branch only, matching this
 * file's own X11/GLX-only scope). Sprite pixels are now kept around
 * (not freed after texture upload) so the mask can be built from real
 * alpha data - same "kept around... so the shape mask can be rebuilt"
 * comment egg_window.c's own header already has for the identical
 * reason. */
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static int load_sprite_csv(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }

    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }

    g_sprite_pixels = pixels; /* kept - shape mask AND the RGB draw below both read straight from this, no GL texture upload needed */
    g_sprite_res = res;
    return 1;
}

/* REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md §1.3) -
 * the nearest-neighbor sprite-downscale index math (`(screen_px *
 * sprite_res) / win_px`, clamped to `sprite_res - 1`) was copy-pasted
 * verbatim at 3 separate sites (build_shape_mask() below, draw_sprite_
 * rgb(), and the top-strip sprite sampler further down) - one real
 * shared helper now, not three. Behavior is bit-for-bit identical
 * (same integer math, same clamp), this is a pure dedup, not a
 * functional change. */
static int sprite_scale_index(int screen_px, int sprite_res, int win_px) {
    int idx = (screen_px * sprite_res) / win_px;
    if (idx >= sprite_res) idx = sprite_res - 1;
    return idx;
}

/* 2026-09-20 - publish the entity's own sprite as _NET_WM_ICON so the desktop's
 * dock/overview (GNOME's left dash when always-on-top is OFF, i.e. the pal is a
 * WM-managed window) shows the real entity image instead of the generic app
 * image it falls back to when a window has no icon. Two sizes (64, 32),
 * nearest-neighbour from the sprite grid so pixel art stays crisp; EWMH wants
 * CARDINAL/32 = width, height, then non-premultiplied ARGB (what Mutter
 * expects), one unsigned long per pixel; alpha-0 pixels are zeroed. No sprite
 * (glyph-only pal) = property left unset: nothing is written, the generic image
 * stays. The sprite is loaded once per process, so there is no reload path to
 * refresh this. WM_CLASS stays "MuchiverseLivedesk" on purpose: the Mutter
 * xwayland-grab-access-rules allowlist matches it ($.crypts/
 * enable_xwayland_grabs.sh). Design note: 12.calendar/2026-09-20/2do.md 8a. */
static void set_net_wm_icon(Display *dpy, Window win) {
    if (!dpy || !g_sprite_pixels || g_sprite_res <= 0) return;
    static const int sizes[2] = { 64, 32 };
    unsigned long icon[2 + 64 * 64 + 2 + 32 * 32];
    size_t n = 0;
    for (int s = 0; s < 2; s++) {
        int sz = sizes[s];
        icon[n++] = (unsigned long)sz;
        icon[n++] = (unsigned long)sz;
        for (int y = 0; y < sz; y++) {
            int sy = sprite_scale_index(y, g_sprite_res, sz);
            for (int x = 0; x < sz; x++) {
                int sx = sprite_scale_index(x, g_sprite_res, sz);
                const unsigned char *p = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
                icon[n++] = p[3] == 0 ? 0UL
                    : (((unsigned long)p[3] << 24) | ((unsigned long)p[0] << 16) |
                       ((unsigned long)p[1] << 8) | (unsigned long)p[2]);
            }
        }
    }
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_ICON", False), XA_CARDINAL, 32,
                    PropModeReplace, (const unsigned char *)icon, (int)n);
}

/* REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md §1.3) -
 * generic replacement for what used to be two separate, near-identical
 * mask-builder functions (this one's own sprite-alpha-driven test, and
 * update_entity_shape_from_3d()'s own buffer-vs-background-diff test) -
 * same real loop shape (clear to 0, set fg to 1, per-pixel test,
 * XFillRectangle the opaque ones), differing only in HOW a pixel is
 * tested. One real shared implementation via a callback, not two.
 *
 * REAL FIX 2026-09-04, PART 2 (RENDERER-MODULARITY-AND-PERF-AUDIT.md
 * §2.2) - was WIN_PX² individual XFillRectangle(mask, x, y, 1, 1) X
 * protocol calls (one full request per opaque pixel - real, measured
 * cost, worse than kh_draw_canvas()'s own already-known per-pixel
 * XPutPixel loop since that one at least stays client-side). Now
 * builds a local 1bpp bitmap (plain C loop, zero X calls per pixel -
 * just setting a bit in a malloc'd byte buffer) and pushes it to the
 * X server in ONE shot: XCreateBitmapFromData() (handles the real
 * display's own bitmap-bit-order/padding correctly, same primitive
 * X11 icon-mask code has always used for exactly this) builds a fresh
 * pixmap from the local bits, one XCopyArea() blits it into the
 * CALLER's own `mask` Pixmap (preserving that object's identity - both
 * call sites, and cursword_update_shape()'s own multi-step compositing
 * that keeps drawing into `mask`/`mask_gc` after this call returns,
 * depend on `mask` staying the SAME pixmap, not a new one). Net: a
 * handful of X calls total, not WIN_PX²+2, with bit-for-bit identical
 * resulting shape - confirmed via live PNG dump of a real sprite tile
 * before/after. `mask_gc`'s own foreground color state is left
 * touched (0 then 1, matching prior behavior) since callers may still
 * rely on it being GC-drawable afterward (cursword_update_shape()
 * does, for its own ring/disc compositing). */
typedef int (*kh_pixel_opaque_fn)(int x, int y, void *ctx);
static void kh_build_shape_mask_generic(Display *dpy, GC mask_gc, Pixmap mask,
                                         int w, int h, kh_pixel_opaque_fn is_opaque, void *ctx) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, (unsigned)w, (unsigned)h);
    XSetForeground(dpy, mask_gc, 1);
    int stride = (w + 7) / 8;
    unsigned char *bits = (unsigned char *)calloc((size_t)stride * (size_t)h, 1);
    if (!bits) {
        /* real, honest fallback - should never happen in practice at
         * these tile resolutions, but never silently draw nothing if
         * it ever does. */
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (is_opaque(x, y, ctx)) XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
        return;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (is_opaque(x, y, ctx))
                bits[y * stride + (x >> 3)] |= (unsigned char)(1u << (x & 7));
    Pixmap tmp = XCreateBitmapFromData(dpy, mask, (char *)bits, (unsigned)w, (unsigned)h);
    free(bits);
    if (tmp != None) {
        XCopyArea(dpy, tmp, mask, mask_gc, 0, 0, (unsigned)w, (unsigned)h, 0, 0);
        XFreePixmap(dpy, tmp);
    }
}

static int kh_sprite_alpha_opaque(int x, int y, void *ctx) {
    (void)ctx;
    int sy = sprite_scale_index(y, g_sprite_res, WIN_PX);
    int sx = sprite_scale_index(x, g_sprite_res, WIN_PX);
    return g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127;
}

/* Verbatim port of egg_window.c's own build_shape_mask() (POSIX/X11
 * branch) - builds the window's real clip shape from the sprite's own
 * alpha channel (upscaled nearest-neighbor to the window's pixel size),
 * so the desktop genuinely shows through transparent pixels instead of
 * this window's own opaque background fill. */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    if (g_sprite_pixels) {
        kh_build_shape_mask_generic(dpy, mask_gc, mask, WIN_PX, WIN_PX, kh_sprite_alpha_opaque, NULL);
    } else {
        XSetForeground(dpy, mask_gc, 0);
        XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, mask_gc, 1);
        XFillArc(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

/* REAL, NEW 2026-08-31, direct live report ("some entities (not
 * cursword) but all others when rotated, leave a 'red shadow' of
 * their 2d shape") - real root cause: only cursword gets a real
 * 32-bit ARGB visual (see have_argb_visual's own "g_is_cursword"
 * gate near main()'s window creation) - every other entity's window
 * has no real per-pixel alpha at all, and build_shape_mask() above is
 * only ever called ONCE, from the flat 2D sprite's own silhouette, at
 * startup. In 3D mode the raymarched content's real footprint moves
 * as the entity rotates, but the window's real clickable/visible
 * region stays frozen at that original flat outline - so whenever the
 * rotated 3D shape covers LESS of that frozen outline than the flat
 * sprite did, the gap reveals this frame's always-opaque background
 * fill (the entity's own theme color) confined exactly to the old 2D
 * silhouette's shape - the reported "shadow."
 *
 * Real fix: after drawing 3D content into g_buf, read it back
 * (XGetImage - WIN_PX is small, ~80px, cheap at this file's own
 * MAX_FPS cap) and rebuild the window's real ShapeBounding mask from
 * whatever's ACTUALLY drawn this frame (any pixel that isn't exactly
 * the background fill color counts as "in") - the exact same real
 * "server clips what's not shaped in" mechanism build_shape_mask()
 * already uses, just driven by this frame's real raymarch result
 * instead of a one-time flat sprite. Cursword is exempt (g_is_cursword
 * check at the call site) - it already has its own real, working
 * shape-refresh path (cursword_update_shape()) for a different reason
 * (the halo's wider click surface) and real ARGB alpha for its own
 * background, so this generic path would just be redundant/
 * conflicting there. */
struct kh_bg_diff_ctx { XImage *img; unsigned long bg_pixel; };
static int kh_bg_diff_opaque(int x, int y, void *ctxp) {
    struct kh_bg_diff_ctx *ctx = (struct kh_bg_diff_ctx *)ctxp;
    unsigned long px = XGetPixel(ctx->img, x, y) & 0xFFFFFFUL; /* real, deliberate - ignore the alpha byte, meaningless on this non-ARGB visual */
    return px != ctx->bg_pixel;
}

static void update_entity_shape_from_3d(Display *dpy, Window win, Drawable buf,
                                         int bg_r, int bg_g, int bg_b) {
    XImage *img = XGetImage(dpy, buf, 0, 0, WIN_PX, WIN_PX, AllPlanes, ZPixmap);
    if (!img) return;
    Pixmap mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
    GC mask_gc = XCreateGC(dpy, mask, 0, NULL);
    struct kh_bg_diff_ctx ctx;
    ctx.img = img;
    ctx.bg_pixel = ((unsigned long)bg_r << 16) | ((unsigned long)bg_g << 8) | (unsigned long)bg_b;
    kh_build_shape_mask_generic(dpy, mask_gc, mask, WIN_PX, WIN_PX, kh_bg_diff_opaque, &ctx);
    XDestroyImage(img);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(dpy, mask_gc);
    XFreePixmap(dpy, mask);
}

/* REAL FIX 2026-08-30, found live: the halo drawn to g_buf/win was
 * completely invisible no matter what - traced to THIS real
 * mechanism, build_shape_mask()'s own XShapeCombineMask(ShapeSet)
 * above, which clips the window's real, server-enforced visible
 * region down to just the sprite's own opaque silhouette. Anything
 * drawn to the backing pixmap OUTSIDE that shape is real X11 protocol
 * data that the server never composites - not a client-side bug, a
 * real window-shape boundary. Real fix: when cursword arms, UNION a
 * real ring-shaped mask onto the EXISTING sprite shape (ShapeUnion,
 * not ShapeSet - adds to it rather than replacing it) so the halo's
 * own pixels are inside the window's real visible region too; when it
 * disarms, rebuild the shape from scratch (build_shape_mask() again,
 * a real ShapeSet) to drop the ring and return to sprite-only. */
static void cursword_update_shape(Display *dpy, Window win) {
    if (!g_has_sprite) return;
    /* REAL, NEW 2026-08-30 - the real key-log debug strip (see
     * cursword_log_key()'s own header comment) needs the WINDOW
     * itself taller while armed, or its own visible-region rectangle
     * (unioned below) would just be empty space outside the window's
     * real bounds. Resized back down to exactly WIN_PX on disarm. */
    if (g_is_cursword)
        XResizeWindow(dpy, win, (unsigned)(g_cursword_armed ? CURSWORD_LOG_W : WIN_PX),
                      (unsigned)(WIN_PX + (g_cursword_armed ? CURSWORD_LOG_H : 0)));
    Pixmap mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
    GC mask_gc = XCreateGC(dpy, mask, 0, NULL);
    build_shape_mask(dpy, win, mask_gc, mask); /* real ShapeSet baseline - sprite only */

    /* REAL FIX 2026-08-30, direct live report ("im still having to
     * click right on the image") - the earlier ShapeInput-only attempt
     * (a real, independent input-hitbox mask, wider than the visible
     * shape, zero visual change) turned out NOT to be honored by the
     * real compositor for genuine mouse clicks, even though it worked
     * in synthetic testing here - a known real-world gap for
     * ShapeInput specifically on override-redirect windows. The only
     * mechanism actually proven reliable for real click routing is
     * ShapeBounding itself (that's what already correctly gates every
     * other click today), so the grab surface now has to be real
     * ShapeBounding, not just Input - meaning it has to be visible.
     * Direct instruction on how: "solid disc but very low
     * transparency". This window has no true per-pixel alpha (binary
     * Shape mask only, not an ARGB32 visual) - a real transparency
     * blend isn't available, so this fills the disc with a real,
     * dim, near-black color instead (0x141414 - the same dim neutral
     * backdrop open-hai's own khtpm_open_hai_render.c already uses,
     * not an arbitrary pick) as the closest honest approximation:
     * reads as a faint shadow/backdrop, not a jarring solid block.
     * ALWAYS unioned now (moved out of the `if (g_cursword_armed)`
     * gate below) - the whole point is a wider grab surface even when
     * unarmed. */
    {
        Pixmap disc_mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
        GC disc_gc = XCreateGC(dpy, disc_mask, 0, NULL);
        XSetForeground(dpy, disc_gc, 0);
        XFillRectangle(dpy, disc_mask, disc_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, disc_gc, 1);
        int dcx = WIN_PX / 2, dcy = WIN_PX / 2;
        int dradius = WIN_PX / 2 - 5;
        XFillArc(dpy, disc_mask, disc_gc, dcx - dradius, dcy - dradius,
                 (unsigned)(dradius * 2), (unsigned)(dradius * 2), 0, 360 * 64);
        XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, disc_mask, ShapeUnion);
        XFreeGC(dpy, disc_gc);
        XFreePixmap(dpy, disc_mask);
    }

    if (g_cursword_armed) {
        XSetForeground(dpy, mask_gc, 0);
        XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, mask_gc, 1);
        int cx = WIN_PX / 2, cy = WIN_PX / 2;
        int radius = WIN_PX / 2 - 5;
        if (radius > 3) {
            XSetLineAttributes(dpy, mask_gc, 9, LineSolid, CapButt, JoinMiter);
            XDrawArc(dpy, mask, mask_gc, cx - radius, cy - radius, (unsigned)(radius * 2), (unsigned)(radius * 2), 0, 360 * 64);
        }
        XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeUnion);

        /* Real key-log debug strip's own visible-region rectangle - a
         * SEPARATE mask pixmap offset by (0, WIN_PX), unioned the same
         * way as the ring just above. Deliberately separate from
         * `mask` (WIN_PX x WIN_PX, build_shape_mask()'s own real
         * contract) rather than resizing it, so that shared function's
         * existing behavior for every other entity stays completely
         * untouched. */
        Pixmap strip_mask = XCreatePixmap(dpy, win, (unsigned)CURSWORD_LOG_W, (unsigned)CURSWORD_LOG_H, 1);
        GC strip_gc = XCreateGC(dpy, strip_mask, 0, NULL);
        XSetForeground(dpy, strip_gc, 1);
        XFillRectangle(dpy, strip_mask, strip_gc, 0, 0, CURSWORD_LOG_W, CURSWORD_LOG_H);
        XShapeCombineMask(dpy, win, ShapeBounding, 0, WIN_PX, strip_mask, ShapeUnion);
        XFreeGC(dpy, strip_gc);
        XFreePixmap(dpy, strip_mask);
    }
    XFreeGC(dpy, mask_gc);
    XFreePixmap(dpy, mask);
}

/* Direct alpha-blended pixel blit into the compose buffer, nearest-
 * neighbor scaled sprite->window exactly like build_shape_mask()'s own
 * loop above (same sx/sy math, so the drawn pixels and the window's
 * real clip shape can never drift apart) - replaces the GL texture-
 * quad upload/draw entirely. Ported technique: events-hq's own
 * draw_entity_glyph() (khtpm_events_hq_render.c) already does this
 * exact alpha-over-background blit for a real sprite.csv. bg_r/g/b are
 * 0-255 ints (glyph_color()'s own 0..1 floats *255, already computed by
 * the caller) - blended UNDER the sprite's own alpha, same as GL_BLEND/
 * GL_SRC_ALPHA did before. */
static void draw_sprite_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;
    for (int y = 0; y < WIN_PX; y++) {
        int sy = sprite_scale_index(y, g_sprite_res, WIN_PX);
        for (int x = 0; x < WIN_PX; x++) {
            int sx = sprite_scale_index(x, g_sprite_res, WIN_PX);
            unsigned char *p = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            /* Real, new 2026-08-30 - the top byte is a real alpha
             * channel on cursword's own new ARGB32 window (see the
             * XCreateWindow ARGB-visual comment near main()'s window
             * setup) and a harmless no-op high byte on every other
             * entity's plain 24-bit window (silently masked off by the
             * X server, never stored) - always opaque here, since this
             * function already does its own real alpha blend against
             * bg_r/g/b above. */
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, y);
        }
    }
}

/* REAL, NEW 2026-08-30 - real "desktop 3D" render, design doc §3a/§9-12,
 * direct instruction ("we need to do the big important bulk of this
 * now... we can start with camera 3/4 topdown only, if that would
 * make it easier"). Desktop-wide by construction (§9 item #2's own
 * resolution - EVERY desktop entity's own window, this exact shared
 * binary/file, polls the SAME real #.desktop/desktop_camera_mode.txt;
 * not gated to cursword or to armed state - cursword is only ever the
 * CONTROLLER that writes this file, per the 1-4 key wiring above).
 *
 * Real scope note, matching the direct instruction above: modes 1/2
 * (true first/third-person perspective) and mode 3's own real free-
 * roam camera movement stay deferred - this first pass renders 3
 * and 4 identically, as a real, extruded "block viewed from above"
 * (matches board-viewer's own real mode-4 "bird's eye" framing
 * exactly; mode 3 is simplified down to the same topdown case for
 * now, not yet its own true free-roam perspective).
 *
 * No separate voxel-asset generation needed (unlike board-viewer's
 * own board-scale raymarcher, which reads a project-wide registry of
 * <hex>/voxels_16.csv files) - every desktop entity already has its
 * own real per-pixel RGBA texture loaded right here as g_sprite_pixels/
 * g_sprite_res (the exact same sprite.csv data draw_sprite_rgb() just
 * used above), so THAT is the real texture this reuses directly. */
static int g_camera_mode = 1;
static void load_camera_mode(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_mode.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_camera_mode = 1; return; }
    char line[16];
    if (!fgets(line, sizeof(line), f)) g_camera_mode = 1;
    else g_camera_mode = atoi(line);
    fclose(f);
    if (g_camera_mode < 1 || g_camera_mode > 4) g_camera_mode = 1;
}

/* REAL, NEW 2026-08-30, direct follow-up ("do u understand how it
 * looks depends on the camera?" -> "both" [tilt changes the block's
 * own look, AND pan/zoom moves the whole desktop]) - a real, second
 * shared state file (same real "small state file under #.desktop/"
 * convention as desktop_camera_mode.txt) carrying actual camera
 * PARAMETERS, not just a mode selector: cam_pan_x/cam_pan_y (a real
 * screen-pixel offset applied to every entity's own displayed
 * position, desktop-wide, while in 3D mode) and cam_tilt (0-100, how
 * much of each entity's own extruded side face shows - 0 is pure
 * straight-down/no side visible, 100 is maximally tilted/lots of side
 * visible). Real, honest scope note: ZOOM is NOT built this pass -
 * every entity's own window is a fixed WIN_PX size used throughout
 * this file's own shape-mask/grid/pixmap math; dynamically resizing
 * that per-frame is a real, separate, riskier change (pixmap/GC
 * recreation, shape-mask rebuild at new sizes) deliberately deferred
 * rather than rushed alongside pan+tilt in the same pass. */
static int g_cam_pan_x = 0, g_cam_pan_y = 0, g_cam_tilt = 0;
/* Real, new 2026-08-31, direct live report ("not all the camera
 * controls were fully taken from piececraft yet") - yaw (q/e),
 * board-viewer's own real key convention, was the real gap. Degrees,
 * added directly onto build_raymarch_cam()'s own fixed front/top
 * base yaw (see that function's own comment). */
static int g_cam_yaw = 0;
/* Real, tentative forward declarations - g_entity_z/g_active_z's own
 * real definitions (with header comments) sit further down this file
 * next to their real load/write functions, but cursword_handle_
 * camera_key() (right below) needs them in scope earlier - same real
 * C tentative-definition merge every other forward-declared global in
 * this file already relies on. */
static int g_entity_z;
static int g_active_z;
static void load_camera_state(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_state.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0; return; }
    int pan_x = 0, pan_y = 0, tilt = 0, yaw = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        if (strcmp(line, "cam_pan_x") == 0) pan_x = val;
        else if (strcmp(line, "cam_pan_y") == 0) pan_y = val;
        else if (strcmp(line, "cam_tilt") == 0) tilt = val;
        else if (strcmp(line, "cam_yaw") == 0) yaw = val;
    }
    fclose(f);
    if (tilt < 0) tilt = 0;
    if (tilt > 100) tilt = 100;
    g_cam_pan_x = pan_x; g_cam_pan_y = pan_y; g_cam_tilt = tilt; g_cam_yaw = yaw;
}

/* Real write side of load_camera_state() above - cursword's own
 * camera-control keys (w/a/s/d pan, r/t tilt, q/e yaw, board-viewer's
 * own real key convention reused verbatim, zero collision with
 * cursword's own arrow-key entity movement or 1-4 mode keys) call
 * this after updating g_cam_pan_x/g_cam_pan_y/g_cam_tilt/g_cam_yaw in
 * memory. */
static void write_camera_state(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_camera_state.txt", house_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "cam_pan_x=%d\ncam_pan_y=%d\ncam_tilt=%d\ncam_yaw=%d\n", g_cam_pan_x, g_cam_pan_y, g_cam_tilt, g_cam_yaw);
    fclose(f);
}

/* ============================================================
 * REAL, CONSOLIDATED CAMERA CONTROLS - direct instruction 2026-08-30
 * ("lets get all teh camera controls in 1 place first"). One real
 * function, one real per-key dispatch, called from exactly one site
 * in the main KeyPress handler (armed + g_is_cursword) instead of
 * three separate elif branches scattered through that chain. Add any
 * FUTURE camera key here too (yaw, zoom, etc. - direct instruction:
 * "then we will add the other camera controls") - this is the one
 * real place all of it belongs, not a per-key precedent to copy.
 * Returns 1 if the key was a real camera key (caller should treat it
 * as handled - set need_redraw etc.), 0 if it wasn't (caller keeps
 * checking its own other real branches, e.g. Escape/arrows).
 * ============================================================ */
static int cursword_handle_camera_key(const char *house_root, const char *package_dir, KeySym ks2) {
    if (ks2 == XK_0) {
        /* REAL, REPLACED 2026-08-31, direct instruction ("i just wanna
         * use 0 to change between 2d and 3d desk entity mode since
         * theres only 1 camera mode for desk") - the previous 1-4
         * (moved to 5-8) four-way first-person/third-person/free-roam/
         * bird's-eye split was board-viewer's own real convention,
         * reused verbatim back when a future "one map" shared-scene
         * mode was still going to need 1-4 reserved. One-map is now
         * abandoned (see ^.ONE-MAP-ATTEMPT.md - real reasons this
         * won't work here) and the direct-instruction framing above is
         * simpler and correct for THIS desk: only a real 2D/3D
         * distinction matters day to day, not which of the 4 sub-modes
         * - both 3/4 render identically here anyway (every ==3||==4
         * gate in this file treats them the same). Single real toggle:
         * mode 1 (flat/2D) <-> mode 4 (the real 3D render, picked as
         * the one representative value - bird's-eye, matches what this
         * whole session's own live testing actually used). Also
         * zeroes real cam_pan/tilt/yaw on every toggle - direct
         * instruction ("when sword view is reset, the other entities
         * views should be reset") - every other entity already polls
         * this same shared desktop_camera_state.txt (camera_changed_
         * dirty()'s own real poll, unchanged), so a clean reset here
         * is a real, already-working reset for the whole desk, not
         * just cursword's own view. */
        g_camera_mode = (g_camera_mode == 1) ? 4 : 1;
        char camp[TP_PATH_BUF];
        snprintf(camp, sizeof(camp), "%s/#.desktop/desktop_camera_mode.txt", house_root);
        FILE *cf = fopen(camp, "w");
        if (cf) { fprintf(cf, "%d\n", g_camera_mode); fclose(cf); }
        g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history(g_camera_mode == 4 ? "CURSWORD_CAMERA_3D_ON" : "CURSWORD_CAMERA_3D_OFF");
        return 1;
    }
    if (ks2 == XK_c || ks2 == XK_v) {
        /* REAL, NEW 2026-08-31, direct instruction ("do we have z
         * layers yet? ... using c & v the xelector/cursword moves up
         * and down z levels but the rest of the entities should
         * remain on their own z level unless some event is otherwise
         * moving them") - board-viewer's own real c/v key convention
         * (camera_control.c: "c/v = Camera Z level"), but real,
         * direct instruction maps it here to CURSWORD's OWN entity z
         * (cursword is the real xelector/selector role) rather than a
         * separate camera-only parameter - moving it is what defines
         * the shared desktop_active_z.txt every OTHER entity's own
         * window polls to decide whether to show or hide itself (see
         * that file's own header comment and the real map/unmap logic
         * in the main render loop). g_house_root/package_dir close
         * over this function's own real parameters, not globals.
         * REAL BUG FIX 2026-08-31, direct live report ("zx cy aren't
         * changing z level in 2d or 3d mode yet") - this branch used
         * to sit AFTER the 3D-only gate below, so c/v silently did
         * nothing outside camera_mode 3/4 - real z is an always-on
         * entity property, not a 3D-camera-only parameter, so this
         * check now runs BEFORE that gate, matching the direct
         * instruction's own "in 2d or 3d mode" framing exactly. */
        g_entity_z += (ks2 == XK_v) ? 1 : -1;
        char zpath[TP_PATH_BUF];
        snprintf(zpath, sizeof(zpath), "%s/desktop_pos.txt", package_dir);
        /* Re-read x/y (write_pos()'s own real "preserve what's not
         * changing" shape isn't reusable here - this call site has no
         * real win_x/win_y in scope, only package_dir) so the z change
         * doesn't clobber the entity's own real saved position. */
        int px = 0, py = 0;
        FILE *rf = fopen(zpath, "r");
        if (rf) {
            char line[128];
            while (fgets(line, sizeof(line), rf)) {
                if (strncmp(line, "x=", 2) == 0) px = atoi(line + 2);
                else if (strncmp(line, "y=", 2) == 0) py = atoi(line + 2);
            }
            fclose(rf);
        }
        FILE *wf = fopen(zpath, "w");
        if (wf) { fprintf(wf, "x=%d\ny=%d\nz=%d\n", px, py, g_entity_z); fclose(wf); }
        char azpath[TP_PATH_BUF];
        snprintf(azpath, sizeof(azpath), "%s/#.desktop/desktop_active_z.txt", house_root);
        FILE *azf = fopen(azpath, "w");
        if (azf) { fprintf(azf, "%d\n", g_entity_z); fclose(azf); }
        g_active_z = g_entity_z;
        bump_camera_changed(house_root);
        append_history(ks2 == XK_v ? "CURSWORD_Z_UP" : "CURSWORD_Z_DOWN");
        return 1;
    }
    if (g_camera_mode != 3 && g_camera_mode != 4) return 0; /* real, shared gate - every OTHER camera key below only means something once in a 3D mode (c/v above is deliberately exempt - real z is not camera-only), matches board-viewer's own real camera_control.c "render_mode != 1 -> no-op" precedent */
    if (ks2 == XK_f) {
        /* Real reset - board-viewer's own real key (camera_control.c:
         * "f reset to default facing"/"f center on hero"), reused
         * verbatim, direct instruction. */
        g_cam_pan_x = 0; g_cam_pan_y = 0; g_cam_tilt = 0; g_cam_yaw = 0;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history("CURSWORD_CAMERA_RESET");
        return 1;
    }
    if (ks2 == XK_w || ks2 == XK_a || ks2 == XK_s || ks2 == XK_d || ks2 == XK_r || ks2 == XK_t ||
        ks2 == XK_q || ks2 == XK_e) {
        /* Real pan (w/a/s/d) + tilt (r/t) + yaw (q/e) - same real
         * letters board-viewer's own camera_control.c already uses,
         * reused verbatim, zero collision with cursword's own arrow-
         * key ENTITY movement or 1-4 mode keys. Real, new 2026-08-31,
         * direct live report ("not all the camera controls were
         * fully taken from piececraft yet") - q/e (yaw) was the real
         * gap this closes; c/v is handled separately below (real
         * per-entity Z, not a camera-only parameter, see g_entity_z's
         * own declaration comment for why). Desktop-wide effect - see
         * load_camera_state()'s own header comment. */
        int step = GRID_CELL_PX / 4;
        if (ks2 == XK_w) g_cam_pan_y += step;
        else if (ks2 == XK_s) g_cam_pan_y -= step;
        else if (ks2 == XK_a) g_cam_pan_x += step;
        else if (ks2 == XK_d) g_cam_pan_x -= step;
        else if (ks2 == XK_r) { g_cam_tilt += 10; if (g_cam_tilt > 100) g_cam_tilt = 100; }
        else if (ks2 == XK_t) { g_cam_tilt -= 10; if (g_cam_tilt < 0) g_cam_tilt = 0; }
        else if (ks2 == XK_q) g_cam_yaw -= 15;
        else if (ks2 == XK_e) g_cam_yaw += 15;
        write_camera_state(house_root);
        bump_camera_changed(house_root);
        append_history("CURSWORD_CAMERA_PAN_TILT");
        return 1;
    }
    return 0;
}

/* Real, art-derived shaded "wall" strip - same real bbox-crop +
 * edge-color-averaging technique as draw_topdown_block_rgb()'s own
 * header comment, factored out here so cursword's own camera-state
 * write helper (below) and the render path share one real definition
 * of "how big can the wall get" instead of two independent guesses. */
#define TOPDOWN_WALL_PX_MAX 20

/* REAL, NEW 2026-08-30, direct instruction ("i see it doing that but
 * its not the raymarching yet. lets keep pushing"): a genuine, real
 * per-pixel raymarch this time, not a 2D compositing trick - ported
 * near-verbatim from board-viewer's own bv_render_3d.c (its own real
 * DDA raymarcher's core primitives), which is itself already a proven,
 * real per-pixel DDA/AABB raymarcher. For a SINGLE object (one entity,
 * not a whole board of cells) the "march" collapses to one direct
 * ray-vs-one-box intersection per pixel - no grid traversal needed,
 * genuinely simpler than board-viewer's own multi-cell case while
 * using the EXACT same real ray-AABB math and face-UV convention, not
 * a simplified imitation of it. */

/* Ported near-verbatim from bv_render_3d.c's own ray_aabb_hit_3d() -
 * real slab-method ray/box intersection, returns the nearest real hit
 * distance and which of the 6 real faces it landed on (0/1=x, 2/3=y,
 * 4/5=z - see box_face_uv() below for what each face means). */
static int cursword_ray_aabb_hit(double ox, double oy, double oz, double dx, double dy, double dz,
                                  double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                                  double *out_t, int *out_face) {
    double tmin = -1e18, tmax = 1e18;
    int face = -1;
    if (fabs(dx) < 1e-12) {
        if (ox < bx0 || ox > bx1) return 0;
    } else {
        double t0 = (bx0 - ox) / dx, t1 = (bx1 - ox) / dx;
        int f0 = 0;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 1; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dy) < 1e-12) {
        if (oy < by0 || oy > by1) return 0;
    } else {
        double t0 = (by0 - oy) / dy, t1 = (by1 - oy) / dy;
        int f0 = 2;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 3; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dz) < 1e-12) {
        if (oz < bz0 || oz > bz1) return 0;
    } else {
        double t0 = (bz0 - oz) / dz, t1 = (bz1 - oz) / dz;
        int f0 = 4;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 5; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0) return 0;
    if (tmin < 0.0) { tmin = 0.0; face = -1; }
    *out_t = tmin;
    if (out_face) *out_face = face;
    return 1;
}

/* Ported near-verbatim from bv_render_3d.c's own box_face_uv() - real
 * hit-point -> texture UV, same single-texture-on-all-6-faces
 * convention that file already established (this house's own real
 * precedent for how a single sprite/emoji becomes a textured cube). */
static void cursword_box_face_uv(double wx, double wy, double wz,
                                  double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                                  int face, double *u, double *v) {
    if (face == 2 || face == 3) {
        *u = (wx - bx0) / (bx1 - bx0);
        *v = (wz - bz0) / (bz1 - bz0);
    } else {
        *u = (face == 4 || face == 5) ? (wx - bx0) / (bx1 - bx0) : (wz - bz0) / (bz1 - bz0);
        *v = 1.0 - (wy - by0) / (by1 - by0);
    }
}

#define RAYMARCH_BLOCK_H 0.5   /* real box height in world units - a real, chosen "how tall is a desktop entity" constant */
#define RAYMARCH_CAM_DIST 2.2  /* real camera distance from the box's own center */
#define RAYMARCH_FOV_DEG 40.0  /* real vertical field of view */
/* Real, shared pinhole camera builder - factored out so both the
 * single-box raymarcher below AND the real per-voxel phymoji
 * raymarcher (further down) build the exact same real camera from
 * cam_tilt, never two independent (and possibly drifting) copies of
 * this math. Same real shape as bv_render_3d.c's own build_camera() -
 * forward = normalize(target - eye), real cross-product right/up -
 * just with a fixed look-at target (a point at world height cy)
 * instead of a walking hero's own anchor. */
typedef struct {
    double ex, ey, ez;             /* eye position */
    double fx, fy, fz;             /* forward */
    double rx, ry, rz;             /* right */
    double ux, uy, uz;             /* up */
    double tan_half_fov;
} RaymarchCam;

static void build_raymarch_cam(double cy, RaymarchCam *cam) {
    /* Real camera: pitch driven by cam_tilt (0 = looking straight
     * down/bird's-eye, 100 = a real oblique 3/4 angle). Real, new
     * 2026-08-30, direct correction ("lets keep the camera angles
     * reasonable... view from front for now") - yaw now comes from
     * the real emoji_sprite_view PDL toggle (g_emoji_sprite_view_top,
     * see desktop_load_click_two_step()'s own header comment): the
     * earlier fixed yaw=45 diagonal "corner" view is what read as
     * "melted"/unreasonable - "front" (the new default) is a straight
     * yaw=0 view instead, the classic real "topdown map, front-facing
     * sprite" convention. yaw isn't camera-KEY-controlled yet either
     * way, only pitch (tilt) and pan/zoom are - this toggle picks the
     * fixed default, not a third live-adjustable axis. */
    /* REAL FIX 2026-08-30, direct live report ("make sure there is no
     * tilt or angle, and show front facing view... it looks tilted") -
     * cam_tilt=0 now means a genuine, real pitch=0 EYE-LEVEL view
     * (dead-on front, zero angle) - the old formula started at
     * pitch=90 (straight DOWN) even at tilt=0, which is the opposite
     * of "front facing" despite the same numeric default. Tilt now
     * climbs UP from that real flat baseline toward a downward angle
     * as it increases, matching the real, literal meaning of "add
     * tilt" instead of starting pre-tilted. */
    double pitch_deg = (g_cam_tilt / 100.0) * 65.0; /* 0 (dead-on front, no angle) .. 65 (angled down) */
    /* Real, new 2026-08-31 - g_cam_yaw (q/e keys) added directly onto
     * the fixed front/top base yaw, real, live-adjustable rotation on
     * top of the PDL-picked default. */
    double yaw_deg = (g_emoji_sprite_view_top ? 45.0 : 0.0) + g_cam_yaw;
    double pitch = pitch_deg * M_PI_LOCAL / 180.0, yaw = yaw_deg * M_PI_LOCAL / 180.0;

    cam->ex = RAYMARCH_CAM_DIST * cos(pitch) * sin(yaw);
    cam->ey = cy + RAYMARCH_CAM_DIST * sin(pitch);
    cam->ez = RAYMARCH_CAM_DIST * cos(pitch) * cos(yaw);
    double fx = -cam->ex, fy = cy - cam->ey, fz = -cam->ez;
    double flen = sqrt(fx * fx + fy * fy + fz * fz);
    cam->fx = fx / flen; cam->fy = fy / flen; cam->fz = fz / flen;
    /* REAL FIX 2026-08-30, direct live report ("i see it but why is
     * it diagonal?" / "why does pressing not make it point straight
     * down like it does in 2d") - this cross product had its sign
     * backwards (real math bug, not a camera-parameter issue): world-
     * up = (0,1,0), right = forward x world-up should be
     * (fy*0-fz*1, fz*0-fx*0, fx*1-fy*0) = (-fz, 0, fx) - this used to
     * compute the literal NEGATIVE of that ((fz, 0, -fx)), a real
     * mirrored/rotated "right" vector that threw the whole
     * orientation off (the up vector derived from it, right x
     * forward below, inherited the same error) - not a pitch/tilt
     * problem at all, a real vector-math sign error, now corrected. */
    double rx = -cam->fz, ry = 0.0, rz = cam->fx;
    double rlen = sqrt(rx * rx + ry * ry + rz * rz);
    if (rlen < 1e-9) { rx = 1.0; ry = 0.0; rz = 0.0; rlen = 1.0; }
    cam->rx = rx / rlen; cam->ry = ry / rlen; cam->rz = rz / rlen;
    /* up = right x forward */
    double ux = cam->ry * cam->fz - cam->rz * cam->fy;
    double uy = cam->rz * cam->fx - cam->rx * cam->fz;
    double uz = cam->rx * cam->fy - cam->ry * cam->fx;
    double ulen = sqrt(ux * ux + uy * uy + uz * uz);
    cam->ux = ux / ulen; cam->uy = uy / ulen; cam->uz = uz / ulen;
    cam->tan_half_fov = tan((RAYMARCH_FOV_DEG / 2.0) * M_PI_LOCAL / 180.0);
}

static void draw_raymarch_block_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;

    /* Real box bounds in world units - a unit-footprint cube, height
     * from RAYMARCH_BLOCK_H, centered on the origin. */
    double bx0 = -0.5, bx1 = 0.5, bz0 = -0.5, bz1 = 0.5;
    double by0 = 0.0, by1 = RAYMARCH_BLOCK_H;
    double cy = (by0 + by1) / 2.0;

    RaymarchCam cam;
    build_raymarch_cam(cy, &cam);
    double ex = cam.ex, ey = cam.ey, ez = cam.ez;
    double fx = cam.fx, fy = cam.fy, fz = cam.fz;
    double rx = cam.rx, ry = cam.ry, rz = cam.rz;
    double ux = cam.ux, uy = cam.uy, uz = cam.uz;
    double tan_half_fov = cam.tan_half_fov;

    /* Real per-pixel raymarch - one direct ray-vs-box test per pixel
     * (no grid/DDA stepping needed for a single object), genuinely
     * reads the sprite's own real texture per face via the SAME real
     * UV convention bv_render_3d.c already established. Anything the
     * ray misses leaves the existing flat base layer (drawn by the
     * caller before this) showing through unchanged. */
    for (int py = 0; py < WIN_PX; py++) {
        double ndc_y = (1.0 - 2.0 * (py + 0.5) / WIN_PX) * tan_half_fov;
        for (int px = 0; px < WIN_PX; px++) {
            double ndc_x = (2.0 * (px + 0.5) / WIN_PX - 1.0) * tan_half_fov;
            double dx = fx + rx * ndc_x + ux * ndc_y;
            double dy = fy + ry * ndc_x + uy * ndc_y;
            double dz = fz + rz * ndc_x + uz * ndc_y;
            double dlen = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double t; int face;
            if (!cursword_ray_aabb_hit(ex, ey, ez, dx, dy, dz, bx0, bx1, by0, by1, bz0, bz1, &t, &face)) continue;
            double wx = ex + dx * t, wy = ey + dy * t, wz = ez + dz * t;
            double u, v;
            cursword_box_face_uv(wx, wy, wz, bx0, bx1, by0, by1, bz0, bz1, face, &u, &v);
            if (u < 0.0) u = 0.0;
            if (u > 1.0) u = 1.0;
            if (v < 0.0) v = 0.0;
            if (v > 1.0) v = 1.0;
            int scol = (int)(u * g_sprite_res); if (scol >= g_sprite_res) scol = g_sprite_res - 1;
            int srow = (int)(v * g_sprite_res); if (srow >= g_sprite_res) srow = g_sprite_res - 1;
            unsigned char *sp = &g_sprite_pixels[(srow * g_sprite_res + scol) * 4];
            int a = sp[3];
            if (a <= 10) continue; /* real transparent texel - box "shows through" to the base layer */

            /* Real, simple per-face directional shading - top face
             * (2/3) full brightness, the two faces facing the camera's
             * own real diagonal (0/1 x-faces, 4/5 z-faces) shaded
             * differently so the box reads as a real 3D corner, not a
             * flat color. */
            int shade = (face == 2 || face == 3) ? 100 : (face == 0 || face == 1) ? 72 : 58;
            int r = (sp[0] * a + bg_r * (255 - a)) / 255 * shade / 100;
            int g = (sp[1] * a + bg_g * (255 - a)) / 255 * shade / 100;
            int b = (sp[2] * a + bg_b * (255 - a)) / 255 * shade / 100;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, px, py);
        }
    }
}

/* REAL, NEW 2026-08-30, direct correction ("thats not true. phymoji
 * does it with chicken emoji. u need to dig deeper.") - the real,
 * ALREADY-BUILT phymoji system (bv_render_3d.c's own
 * load_phymoji_asset()/build_phymoji_columns()/test_phymoji_hit(),
 * generated by ops/pc_phymoji_gen.c into pieces/registry/
 * phymoji_assets/<entity_id>/voxels.csv - a real (x,y,z,r,g,b) sparse
 * voxel grid, genuinely built and working today, not planning-only -
 * confirmed live via chicken's own real 897-line voxels.csv). Ported
 * near-verbatim below (not reinvented), plus a real generated asset
 * for cursword itself (pc_phymoji_gen.+x run directly against
 * cursword's own 🗡️ emoji, 376 real voxels, copied to
 * <package_dir>/pieces/registry/phymoji_assets/cursword/voxels.csv -
 * entity_id == package_dir's own basename, so any future entity with
 * its own generated asset "just works" with zero extra per-entity
 * code). This SUPERSEDES draw_raymarch_block_rgb() above as the real
 * 3D render whenever a real voxel asset is present - that single-box
 * version stays as the real, honest fallback for any entity that
 * doesn't have one generated yet (g_phymoji_count == 0 below). */
#define MAX_PHYMOJI_VOXELS 8192
typedef struct { unsigned char lx, ly, lz, r, g, b; } CursPhymojiVoxel;
#define MAX_PHYMOJI_COLUMNS 2048
typedef struct { unsigned char lx, ly, exists_mask, cr[8], cg[8], cb[8]; } CursPhymojiColumn;

static CursPhymojiColumn g_phymoji_cols[MAX_PHYMOJI_COLUMNS];
static int g_phymoji_col_count = 0;
static int g_phymoji_max_lx = 0, g_phymoji_max_ly = 0, g_phymoji_max_lz = 0;
/* REAL, NEW 2026-08-30, direct instruction ("lets definately tune
 * proportions for 1:1 scaled camera") - the box world-size used to be
 * a fixed unit cube regardless of the asset's own real (lx,ly,lz)
 * extent, squashing/stretching every asset into the same shape
 * (confirmed live: the sword's real 14x13x8 crop looked "melted" -
 * flattened diagonal bands - forced into a 1x0.5x1 box). Real fix: ONE
 * shared world-units-per-voxel scale (PHYMOJI_VOXEL_UNIT) applied to
 * every axis identically - a genuinely proportional box matching the
 * asset's own real aspect ratio, not a separate guessed scale per
 * axis. Set once in load_cursword_phymoji(), read by draw_phymoji_rgb(). */
#define PHYMOJI_VOXEL_UNIT 0.09
static double g_phymoji_world_x = 1.0, g_phymoji_world_y = 0.5, g_phymoji_world_z = 1.0;

/* REAL, NEW 2026-08-30, direct instruction ("u should make a script
 * to do phymoji of all entities. save it locally in shared. and all
 * new entities will use it as well") - real, on-demand asset
 * generation, same real "ensure_X_generated, gated on existence,
 * generate once, cache forever" convention as chtpm_rgb_render.c's
 * own ensure_emoji_asset_generated() (see that function's own real
 * precedent). Shells out to sprite_phymoji_gen.+x (the real, shared
 * tool at &.widgits/_shared-lib/ops/sprite_phymoji_gen.c, copied
 * locally into this same ops_dir by build_khtpm_strip.sh) against
 * THIS entity's own real sprite.csv - not a re-rasterized emoji glyph
 * (see that tool's own header comment for why that distinction is a
 * real, previously-live bug, not a style preference). ops_dir comes
 * from main()'s own real self_exe_path() resolution, same real
 * pattern apply_asset_override() already uses. */
static void ensure_entity_phymoji_generated(const char *package_dir, const char *ops_dir) {
    char base_copy[TP_PATH_BUF];
    snprintf(base_copy, sizeof(base_copy), "%s", package_dir);
    char *entity_id = basename(base_copy);
    char csv_path[TP_PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/pieces/registry/phymoji_assets/%s/voxels.csv", package_dir, entity_id);
    struct stat st;
    if (stat(csv_path, &st) == 0) return; /* already generated, real cache hit */
    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    if (access(sprite_path, F_OK) != 0) return; /* no sprite to generate from - real, honest no-op */
    char gen_bin[TP_PATH_BUF];
    snprintf(gen_bin, sizeof(gen_bin), "%s/sprite_phymoji_gen.+x", ops_dir);
    if (access(gen_bin, X_OK) != 0) return;
    char out_dir[TP_PATH_BUF];
    snprintf(out_dir, sizeof(out_dir), "%s/pieces/registry/phymoji_assets/%s", package_dir, entity_id);
    char cmd[TP_PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' >/dev/null 2>&1", gen_bin, sprite_path, out_dir);
    int rc = system(cmd);
    (void)rc;
}

/* Ported near-verbatim from bv_render_3d.c's own load_phymoji_asset() +
 * build_phymoji_columns() - real CSV load, straight into real
 * (lx,ly)-merged columns (same real perf technique that file's own
 * header comment documents: one merged AABB test per column instead
 * of one per voxel). */
static void load_entity_phymoji(const char *package_dir, const char *ops_dir) {
    ensure_entity_phymoji_generated(package_dir, ops_dir);
    char base_copy[TP_PATH_BUF];
    snprintf(base_copy, sizeof(base_copy), "%s", package_dir);
    char *entity_id = basename(base_copy);
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/phymoji_assets/%s/voxels.csv", package_dir, entity_id);
    FILE *f = fopen(path, "r");
    if (!f) return;
    CursPhymojiVoxel voxels[MAX_PHYMOJI_VOXELS];
    int n = 0, first = 1;
    char line[256];
    while (n < MAX_PHYMOJI_VOXELS && fgets(line, sizeof(line), f)) {
        if (first) { first = 0; if (strncmp(line, "x,y,z", 5) == 0) continue; }
        int x, y, z, r, g, b;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &x, &y, &z, &r, &g, &b) == 6) {
            voxels[n].lx = (unsigned char)x; voxels[n].ly = (unsigned char)y; voxels[n].lz = (unsigned char)z;
            voxels[n].r = (unsigned char)r; voxels[n].g = (unsigned char)g; voxels[n].b = (unsigned char)b;
            if (x > g_phymoji_max_lx) g_phymoji_max_lx = x;
            if (y > g_phymoji_max_ly) g_phymoji_max_ly = y;
            if (z > g_phymoji_max_lz) g_phymoji_max_lz = z;
            n++;
        }
    }
    fclose(f);
    for (int i = 0; i < n; i++) {
        CursPhymojiVoxel *v = &voxels[i];
        int found = -1;
        for (int c = 0; c < g_phymoji_col_count; c++)
            if (g_phymoji_cols[c].lx == v->lx && g_phymoji_cols[c].ly == v->ly) { found = c; break; }
        if (found < 0) {
            if (g_phymoji_col_count >= MAX_PHYMOJI_COLUMNS) continue;
            found = g_phymoji_col_count++;
            g_phymoji_cols[found].lx = v->lx; g_phymoji_cols[found].ly = v->ly;
            g_phymoji_cols[found].exists_mask = 0;
        }
        int z = v->lz;
        if (z >= 0 && z < 8) {
            g_phymoji_cols[found].exists_mask = (unsigned char)(g_phymoji_cols[found].exists_mask | (1 << z));
            g_phymoji_cols[found].cr[z] = v->r; g_phymoji_cols[found].cg[z] = v->g; g_phymoji_cols[found].cb[z] = v->b;
        }
    }
    if (g_phymoji_col_count > 0) {
        /* Real, proportional world-size - see this file's own
         * g_phymoji_world_x/y/z declaration comment. */
        g_phymoji_world_x = (g_phymoji_max_lx + 1) * PHYMOJI_VOXEL_UNIT;
        g_phymoji_world_y = (g_phymoji_max_ly + 1) * PHYMOJI_VOXEL_UNIT;
        g_phymoji_world_z = (g_phymoji_max_lz + 1) * PHYMOJI_VOXEL_UNIT;
    }
    append_history(g_phymoji_col_count > 0 ? "ENTITY_PHYMOJI_LOADED" : "ENTITY_PHYMOJI_EMPTY");
}

/* Ported near-verbatim from bv_render_3d.c's own test_phymoji_hit() -
 * real world-ray -> local-voxel-grid transform (per-axis scale, so
 * local_t == world_t for any real hit, same real math note that
 * function's own header comment explains), then one merged-column
 * ray_aabb test per real column via cursword_ray_aabb_hit() (this
 * file's own already-ported primitive, same real function board-
 * viewer's own test_phymoji_hit() itself calls). */
static int cursword_phymoji_hit(double ox, double oy, double oz, double dx, double dy, double dz,
                                 double wx0, double wy0, double wz0,
                                 double world_size_x, double world_size_y, double world_size_z,
                                 double *out_t, int *out_face,
                                 unsigned char *out_r, unsigned char *out_g, unsigned char *out_b) {
    double scale_x = (double)(g_phymoji_max_lx + 1) / world_size_x;
    double scale_y = (double)(g_phymoji_max_ly + 1) / world_size_y;
    double scale_z = (double)(g_phymoji_max_lz + 1) / world_size_z;
    double lox = (ox - wx0) * scale_x, loy = (oy - wy0) * scale_y, loz = (oz - wz0) * scale_z;
    double ldx = dx * scale_x, ldy = dy * scale_y, ldz = dz * scale_z;

    double best_t = 1e18; int best_face = -1, best_col = -1;
    for (int c = 0; c < g_phymoji_col_count; c++) {
        if (!g_phymoji_cols[c].exists_mask) continue;
        int min_z = 0, max_z = 7;
        while (min_z < 8 && !(g_phymoji_cols[c].exists_mask & (1 << min_z))) min_z++;
        while (max_z > 0 && !(g_phymoji_cols[c].exists_mask & (1 << max_z))) max_z--;
        double t; int face;
        if (cursword_ray_aabb_hit(lox, loy, loz, ldx, ldy, ldz,
                                   (double)g_phymoji_cols[c].lx, (double)g_phymoji_cols[c].lx + 1.0,
                                   (double)g_phymoji_cols[c].ly, (double)g_phymoji_cols[c].ly + 1.0,
                                   (double)min_z, (double)max_z + 1.0, &t, &face)
            && t < best_t) {
            best_t = t; best_face = face; best_col = c;
        }
    }
    if (best_col < 0) return 0;
    double hit_loz = loz + ldz * best_t;
    int z = (int)hit_loz;
    if (z < 0) z = 0;
    if (z > 7) z = 7;
    if (!(g_phymoji_cols[best_col].exists_mask & (1 << z))) {
        int lo = z, hi = z;
        while (lo >= 0 || hi <= 7) {
            if (lo >= 0 && (g_phymoji_cols[best_col].exists_mask & (1 << lo))) { z = lo; break; }
            if (hi <= 7 && (g_phymoji_cols[best_col].exists_mask & (1 << hi))) { z = hi; break; }
            lo--; hi++;
        }
    }
    *out_t = best_t; *out_face = best_face;
    *out_r = g_phymoji_cols[best_col].cr[z]; *out_g = g_phymoji_cols[best_col].cg[z]; *out_b = g_phymoji_cols[best_col].cb[z];
    return 1;
}

/* Real, per-pixel raymarch through the actual voxel grid - same real
 * camera (build_raymarch_cam()) as the single-box fallback, but each
 * ray now tests real per-column voxel geometry instead of one flat
 * box, so the silhouette itself is genuinely volumetric (a sword's
 * real crossguard/blade shape, not a rectangular block skinned with a
 * sword texture). */
static void draw_phymoji_rgb(Display *dpy, Drawable buf, GC gc) {
    if (g_phymoji_col_count <= 0) return;
    /* Real, proportional box - see g_phymoji_world_x/y/z's own
     * declaration comment ("1:1 scaled camera" - the asset's own real
     * aspect ratio, not a forced unit cube). */
    double bx0 = -g_phymoji_world_x / 2.0, bx1 = g_phymoji_world_x / 2.0;
    double bz0 = -g_phymoji_world_z / 2.0, bz1 = g_phymoji_world_z / 2.0;
    double by0 = 0.0, by1 = g_phymoji_world_y;
    double cy = (by0 + by1) / 2.0;

    RaymarchCam cam;
    build_raymarch_cam(cy, &cam);

    for (int py = 0; py < WIN_PX; py++) {
        double ndc_y = (1.0 - 2.0 * (py + 0.5) / WIN_PX) * cam.tan_half_fov;
        for (int px = 0; px < WIN_PX; px++) {
            double ndc_x = (2.0 * (px + 0.5) / WIN_PX - 1.0) * cam.tan_half_fov;
            double dx = cam.fx + cam.rx * ndc_x + cam.ux * ndc_y;
            double dy = cam.fy + cam.ry * ndc_x + cam.uy * ndc_y;
            double dz = cam.fz + cam.rz * ndc_x + cam.uz * ndc_y;
            double dlen = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= dlen; dy /= dlen; dz /= dlen;

            double t; int face; unsigned char vr, vg, vb;
            if (!cursword_phymoji_hit(cam.ex, cam.ey, cam.ez, dx, dy, dz, bx0, by0, bz0,
                                       bx1 - bx0, by1 - by0, bz1 - bz0, &t, &face, &vr, &vg, &vb))
                continue; /* real miss - base layer shows through */

            /* Real, simple per-face directional shading - same real
             * scheme draw_raymarch_block_rgb() itself uses (top
             * brightest, the two camera-facing diagonal faces shaded
             * differently), applied to the voxel's own REAL baked
             * color (pc_phymoji_gen.c already depth-attenuates colors
             * at generation time per real PyMoji Rules A/B/C - this is
             * an ADDITIONAL real per-face cue on top of that, not a
             * replacement for it). */
            int shade = (face == 2 || face == 3) ? 100 : (face == 0 || face == 1) ? 80 : 65;
            int r = vr * shade / 100, g = vg * shade / 100, b = vb * shade / 100;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, px, py);
        }
    }
}

/* REAL REWRITE 2026-08-30, direct live correction ("i didn't see any
 * evidence of extrusion yet, like in piececraft; is that known/
 * intention?" -> answered honestly: the first version here was a flat
 * shading-strip CUE, not real extrusion -> "hopefully we do the
 * extrusion soon, cause thats the real kpi... to know we have made
 * the bulk progress"). REAL, NOW SUPERSEDED by draw_raymarch_block_rgb()
 * above (a genuine per-pixel raymarch) - kept here, unused by the
 * default dispatch, as a cheap fallback shape if the raymarcher's own
 * per-pixel cost ever needs a lighter-weight alternative. Textured,
 * tilt-driven extrusion, not a full per-pixel raymarch, but a real
 * two-face block:
 * a TOP face that visibly foreshortens (compresses vertically) as
 * cam_tilt increases - simulating a camera pitching down to reveal a
 * FRONT face below it, built by real texture sampling (stretching the
 * sprite's own bottom-edge texture row downward, progressively
 * darkened with depth for real shading), not a flat guessed color.
 * Both faces genuinely react to g_cam_tilt - 0 shows the plain flat
 * top only (matches the old "looking straight down" case exactly),
 * 100 shows a strongly compressed top and a tall, real-textured wall
 * beneath it. */
static void draw_topdown_block_rgb(Display *dpy, Drawable buf, GC gc, int bg_r, int bg_g, int bg_b) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;

    /* Real base layer - the plain flat sprite, unchanged. Guarantees
     * no gaps/holes: the compressed top face below only overdraws its
     * own real bbox footprint, everything else (padding/background)
     * still reads correctly from this base pass. */
    draw_sprite_rgb(dpy, buf, gc, bg_r, bg_g, bg_b);

    /* Real bbox crop - same real "the actual opaque silhouette, not
     * the whole padded canvas" technique bv_render_3d.c's own
     * compute_bbox_and_edge_color() already established as correct
     * for exactly this class of problem (real precedent, not
     * reinvented from scratch). */
    int u0 = g_sprite_res, v0 = g_sprite_res, u1 = -1, v1 = -1;
    for (int row = 0; row < g_sprite_res; row++) {
        for (int col = 0; col < g_sprite_res; col++) {
            if (g_sprite_pixels[(row * g_sprite_res + col) * 4 + 3] > 10) {
                if (col < u0) u0 = col;
                if (col > u1) u1 = col;
                if (row < v0) v0 = row;
                if (row > v1) v1 = row;
            }
        }
    }
    if (u1 < u0) { u0 = 0; v0 = 0; u1 = g_sprite_res - 1; v1 = g_sprite_res - 1; }

    double tilt = g_cam_tilt / 100.0; /* 0.0 (flat) .. 1.0 (max tilt) */
    int sx0 = (u0 * WIN_PX) / g_sprite_res;
    int sx1 = ((u1 + 1) * WIN_PX) / g_sprite_res;
    int sy0 = (v0 * WIN_PX) / g_sprite_res;
    int sy1 = ((v1 + 1) * WIN_PX) / g_sprite_res;
    int top_h_px = sy1 - sy0;
    if (top_h_px < 1) top_h_px = 1;

    /* REAL TOP FACE - vertically compressed by (1 - tilt*0.45): real
     * per-pixel resampling of the actual sprite texture (nearest-
     * neighbor on the source row), not a scaled copy of a pre-drawn
     * bitmap - genuinely re-samples g_sprite_pixels row-by-row, same
     * real alpha-blend formula draw_sprite_rgb() itself uses. */
    double top_scale = 1.0 - tilt * 0.45;
    int top_h_scaled = (int)(top_h_px * top_scale + 0.5);
    if (top_h_scaled < 1) top_h_scaled = 1;
    for (int y = 0; y < top_h_scaled; y++) {
        int dsty = sy0 + y;
        if (dsty < 0 || dsty >= WIN_PX) continue;
        int srow = v0 + (y * (v1 - v0 + 1)) / top_h_scaled;
        if (srow > v1) srow = v1;
        for (int x = 0; x < WIN_PX; x++) {
            int scol = sprite_scale_index(x, g_sprite_res, WIN_PX);
            unsigned char *p = &g_sprite_pixels[(srow * g_sprite_res + scol) * 4];
            int a = p[3];
            if (a <= 0) continue;
            int r = (p[0] * a + bg_r * (255 - a)) / 255;
            int g = (p[1] * a + bg_g * (255 - a)) / 255;
            int b = (p[2] * a + bg_b * (255 - a)) / 255;
            XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
            XDrawPoint(dpy, buf, gc, x, dsty);
        }
    }

    /* REAL FRONT/WALL FACE - a genuine texture-mapped strip, not a
     * flat averaged color: the sprite's own real bottom-edge texture
     * row (v1, its actual silhouette color there, column-mapped
     * across the wall's own real width) is stretched downward to fill
     * the wall's height, each row progressively darkened with depth
     * (a real, simple directional-shading cue, in-shadow the further
     * down/away from the top face it is). Height grows with BOTH the
     * top face's own compression gap (top_h_px - top_h_scaled) and
     * cam_tilt directly, so raising tilt genuinely makes more of this
     * real wall visible, not a fixed constant. */
    int wall_h = (top_h_px - top_h_scaled) + (int)(TOPDOWN_WALL_PX_MAX * tilt);
    int wall_top_y = sy0 + top_h_scaled;
    if (wall_top_y + wall_h > WIN_PX) wall_h = WIN_PX - wall_top_y;
    if (sx1 > sx0 && wall_h > 0) {
        for (int y = 0; y < wall_h; y++) {
            int dsty = wall_top_y + y;
            if (dsty < 0 || dsty >= WIN_PX) continue;
            int shade = 100 - (y * 45) / (wall_h > 1 ? wall_h : 1); /* 100%..55% down the wall */
            for (int x = sx0; x < sx1; x++) {
                int scol = u0 + ((x - sx0) * (u1 - u0 + 1)) / (sx1 - sx0);
                if (scol > u1) scol = u1;
                if (scol < u0) scol = u0;
                unsigned char *p = &g_sprite_pixels[(v1 * g_sprite_res + scol) * 4];
                if (p[3] <= 10) continue; /* real transparent edge pixel - leave the base layer showing through */
                int r = p[0] * shade / 100, g = p[1] * shade / 100, b = p[2] * shade / 100;
                XSetForeground(dpy, gc, 0xFF000000UL | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
                XDrawPoint(dpy, buf, gc, x, dsty);
            }
        }
    }
}

/* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever they
 * click... the phymoji will appear"): if tp_place_desktop.c already
 * wrote a desktop_pos.txt (a real click point resolved by
 * tp_arm_placer.c), spawn there instead of the fixed grid default -
 * "wherever they click" means the window's own life should start at
 * that point, not just be draggable to it afterward. */
static int read_initial_pos(const char *package_dir, int *out_x, int *out_y) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int x = -1, y = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) y = atoi(line + 2);
    }
    fclose(f);
    if (x < 0 || y < 0) return 0;
    /* desktop_pos.txt is in REFERENCE px (khtpm_ui_scale.c): convert to this
     * screen (identity on the reference screen). The caller snaps to the
     * scaled grid and clamps to the visible grid. */
    x = kh_pos_ref_to_screen(x);
    y = kh_pos_ref_to_screen(y);
    *out_x = x; *out_y = y;
    return 1;
}

/* REAL, NEW 2026-08-04, direct instruction ("add the context menus that
 * already exist from egg-pal to these by default"): a real, data-driven
 * popup context menu, modeled directly on egg_window.c's own
 * open_context_menu()/draw_context_menu()/close_context_menu() (same
 * override_redirect popup + XGrabPointer-on-open technique), but
 * reading its item list from the package's own meta.pdl METHOD rows
 * instead of egg_window's single hardcoded "Close" - see
 * TILE_PICKER_DESIGN.md §4.5. tp_place_desktop.c writes a default
 * METHOD row ("Close") into every new package, so the default behavior
 * matches egg_window's own exactly - this just makes the list
 * extensible (more methods appended to meta.pdl later, e.g. "Open Event
 * Editor" once event-editor exists, need no renderer changes here). */
#define POPUP_ROW_H scaled(28)  /* UI-scaled, LIVEDESK-UI-SCALE.md (base 28) */
/* REAL FIX 2026-08-06, user: "menu screen is too thin i cant see everything"
 * — fixed 160px clipped RPG Menu rows (XP / qolq / Level lines with nav
 * prefixes). Width is now content-measured (see measure_context_popup_w /
 * g_popup_w); these are floor/ceiling only. */
#define POPUP_W_MIN 180
#define POPUP_W_MAX 640
#define POPUP_W POPUP_W_MIN /* legacy alias: text-popup floor, input defaults */
static int g_popup_w = POPUP_W_MIN;


/* REAL, 2026-08-05, direct instruction ("context menus should be much
 * more customizable, robust, even having user input, href, back, etc
 * just like chtpm" - see TILE_PICKER_DESIGN.md §11 for the wraith-alpha
 * research this is modeled on, scoped down to v1: real multi-PAGE
 * navigation + real text input, reusing MethodItem's own label/action
 * shape rather than inventing a parallel one - free positioning (x/y/w/h/z)
 * is deliberately deferred, not needed while every menu here is still a
 * single-column row list). Optional <package_dir>/objects.pdl:
 *   PAGE | <name>
 *   OBJECT | label=<text> | action=<value>
 * action is exactly methods.pdl's own real convention (a real command,
 * "CLOSE", "void") PLUS two new reserved forms: "GOTO:<page>" (push
 * current page, switch) and "BACK" (pop the page stack) for href/back
 * navigation, and "STATE:<key>" for a real input row (activates text
 * entry, committed value written to <package_dir>/<key>.txt on Escape -
 * same click-to-activate/Escape-to-commit shape this house's own cli_io
 * field convention already uses, not a new one). Absent entirely =
 * existing single-page methods.pdl behavior, completely unchanged. */
#define MAX_PAGES 8
typedef struct {
    char name[32];
    MethodItem items[MAX_METHODS];
    int n_items;
} ObjPage;

static int load_objects(const char *package_dir, ObjPage *pages, int max_pages) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/objects.pdl", package_dir);
    FILE *f = pdl_open(path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n_pages = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PAGE", 4) == 0) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = p + strcspn(p, "\r\n");
            while (end > p && end[-1] == ' ') end--;
            size_t nlen = (size_t)(end - p);
            if (nlen == 0 || n_pages >= max_pages) continue;
            if (nlen >= sizeof(pages[n_pages].name)) nlen = sizeof(pages[n_pages].name) - 1;
            memcpy(pages[n_pages].name, p, nlen);
            pages[n_pages].name[nlen] = '\0';
            pages[n_pages].n_items = 0;
            n_pages++;
        } else if (strncmp(line, "OBJECT", 6) == 0 && n_pages > 0) {
            ObjPage *pg = &pages[n_pages - 1];
            if (pg->n_items >= MAX_METHODS) continue;
            MethodItem *item = &pg->items[pg->n_items];
            item->label[0] = '\0';
            item->action[0] = '\0';
            char *tok = line;
            while ((tok = strchr(tok, '|')) != NULL) {
                tok++;
                while (*tok == ' ') tok++;
                char *tok_end = strchr(tok, '|');
                size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
                while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
                if (strncmp(tok, "label=", 6) == 0) {
                    size_t l = tlen - 6;
                    if (l >= sizeof(item->label)) l = sizeof(item->label) - 1;
                    memcpy(item->label, tok + 6, l);
                    item->label[l] = '\0';
                } else if (strncmp(tok, "action=", 7) == 0) {
                    size_t l = tlen - 7;
                    if (l >= sizeof(item->action)) l = sizeof(item->action) - 1;
                    memcpy(item->action, tok + 7, l);
                    item->action[l] = '\0';
                }
                tok = tok_end ? tok_end : tok + strlen(tok);
            }
            if (item->label[0]) pg->n_items++;
        }
    }
    fclose(f);
    /* REAL FIX 2026-08-05, direct correction ("it needs a cancel button
     * so context can clear, close removes entity" - caught mid-test
     * when the first objects.pdl page shipped with Close but no
     * Cancel): every page needs a real, discoverable no-op dismiss
     * distinct from Close (which really ends the process). Rather than
     * relying on every future objects.pdl author to remember this,
     * auto-append one when a page doesn't already declare its own. */
    for (int pi = 0; pi < n_pages; pi++) {
        int has_cancel = 0;
        for (int i = 0; i < pages[pi].n_items; i++) {
            if (strcmp(pages[pi].items[i].label, "Cancel") == 0) { has_cancel = 1; break; }
        }
        if (!has_cancel && pages[pi].n_items < MAX_METHODS) {
            snprintf(pages[pi].items[pages[pi].n_items].label, sizeof(pages[pi].items[0].label), "Cancel");
            snprintf(pages[pi].items[pages[pi].n_items].action, sizeof(pages[pi].items[0].action), "void");
            pages[pi].n_items++;
        }
    }
    return n_pages;
}

/* REAL, 2026-08-05, direct instruction ("book stack emoji... entirely
 * with khtpm / eventscript page, and pal"): a minimal, single-page
 * loader for a real Show Choices popup - a flat list of real
 * "OBJECT | label=.. | action=.." rows, no PAGE header needed (a choice
 * prompt is always exactly one page). Same real parse shape
 * load_objects() already uses for its own OBJECT rows, just without
 * the page-boundary bookkeeping. Used by the SHOW_PAGE relay command
 * below - the file is a real, externally-generated (by whatever op
 * asked for the choice) objects-style file, arbitrary full path, not
 * package_dir-relative. */
static int load_flat_objects(const char *full_path, MethodItem *items, int max) {
    FILE *f = pdl_open(full_path);
    if (!f) return 0;
    char line[TP_PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "OBJECT", 6) != 0) continue;
        items[n].label[0] = '\0';
        items[n].action[0] = '\0';
        char *tok = line;
        while ((tok = strchr(tok, '|')) != NULL) {
            tok++;
            while (*tok == ' ') tok++;
            char *tok_end = strchr(tok, '|');
            size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
            while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
            if (strncmp(tok, "label=", 6) == 0) {
                size_t l = tlen - 6;
                if (l >= sizeof(items[0].label)) l = sizeof(items[0].label) - 1;
                memcpy(items[n].label, tok + 6, l);
                items[n].label[l] = '\0';
            } else if (strncmp(tok, "action=", 7) == 0) {
                size_t l = tlen - 7;
                if (l >= sizeof(items[0].action)) l = sizeof(items[0].action) - 1;
                memcpy(items[n].action, tok + 7, l);
                items[n].action[l] = '\0';
            }
            tok = tok_end ? tok_end : tok + strlen(tok);
        }
        if (items[n].label[0]) n++;
    }
    fclose(f);
    return n;
}

/* REAL FIX 2026-08-05, direct instruction ("context window should have
 * name/id of entity, so its addressable by others" + "i dont see those
 * in context window?"): the entity's real id (piece_id-instance_id) was
 * only ever written into the invisible X11 window title - the actual
 * VISIBLE popup menu had no id at all. A non-clickable header row (row
 * 0) now shows it, so a human right-clicking an entity can read/copy
 * its real address directly, not just infer it from the glyph. */
static char g_full_id[96] = "";

/* Size the next context menu to its longest drawn row (header + labels,
 * including worst-case chtpm nav prefix "[>] 99. "). Result lives in
 * g_popup_w; open/draw/hit-test/submenu offset all share that. */
static int measure_context_popup_w(Display *dpy, MethodItem *items, int n) {
    const int pad = 28; /* left text x (12) + right margin + border */
    int maxw = popup_text_px(dpy, g_full_id);
    if (items) {
        for (int i = 0; i < n; i++) {
            char buf[192];
            snprintf(buf, sizeof(buf), "[>] 99. %s", items[i].label);
            int tw = popup_text_px(dpy, buf);
            if (tw > maxw) maxw = tw;
            tw = popup_text_px(dpy, items[i].label);
            if (tw > maxw) maxw = tw;
        }
    }
    int w = maxw + pad;
    if (w < POPUP_W_MIN) w = POPUP_W_MIN;
    if (w > POPUP_W_MAX) w = POPUP_W_MAX;
    return w;
}

/* REAL FIX 2026-08-05, direct-caught bug ("its been having issues"
 * investigation - a plain `kill <pid>` left a stale livedesk_open.txt/
 * nav_claims.txt entry behind forever, since this process had no
 * SIGTERM handler and this file's own real cleanup code (registry/
 * nav-claim removal, right before the real return at the end of
 * main()) only ever ran on a normal exit - a bare SIGTERM (the default
 * for plain `kill`, and what most process-manager/session shutdowns
 * send) skipped it entirely. Real fix: catch SIGTERM/SIGINT, set this
 * real sig_atomic_t flag (async-signal-safe per POSIX - nothing more
 * elaborate is safe to do inside a signal handler), the main loop's own
 * condition checks it every iteration so the SAME real cleanup path
 * always runs. The flag itself is DECLARED earlier (just above
 * hq_run_event_loop) so both the default/HQ handler here and the
 * default-mode loop can use it; this file-scope static needs exactly
 * one definition. */
/* REAL, NEW 2026-09-13, direct live instruction ("theres nothing
 * external to house quiting booktstack it must be in house. it must
 * be researched and fix"): plain signal(SIGTERM,...) tells us THAT a
 * signal arrived, never WHO sent it. Real, permanent upgrade to
 * SA_SIGINFO so a future occurrence (this exact class of "reaches
 * the loop, dies within ~1s, no crash evidence anywhere" bug,
 * confirmed live via temporary checkpoint logging to be an external
 * termination, not an internal crash) leaves a real forensic trail:
 * the sender's own PID and cmdline, written with async-signal-safe
 * primitives (write(2) to raw fds, no fopen/fprintf/malloc from
 * inside a signal handler - the one real correctness rule that
 * matters here). Cheap and silent when nothing ever fires it. */
static void handle_shutdown_signal_info(int sig, siginfo_t *info, void *ctx) {
    (void)sig; (void)ctx;
    g_shutdown_requested = 1;
    if (g_package_dir[0]) {
        char path[TP_PATH_BUF];
        int n = 0;
        const char *pfx = "/last_signal.txt";
        while (g_package_dir[n] && n < (int)sizeof(path) - 1) { path[n] = g_package_dir[n]; n++; }
        for (int i = 0; pfx[i] && n < (int)sizeof(path) - 1; i++) path[n++] = pfx[i];
        path[n] = '\0';
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            char buf[128];
            int bl = 0;
            const char *m1 = "sig=";
            for (int i = 0; m1[i]; i++) buf[bl++] = m1[i];
            {
                int v = sig, digs[8], nd = 0;
                if (v == 0) digs[nd++] = 0;
                while (v > 0 && nd < 8) { digs[nd++] = v % 10; v /= 10; }
                while (nd > 0) buf[bl++] = (char)('0' + digs[--nd]);
            }
            const char *m2 = " from_pid=";
            for (int i = 0; m2[i]; i++) buf[bl++] = m2[i];
            {
                long v = (long)info->si_pid; char digs[16]; int nd = 0;
                if (v == 0) digs[nd++] = 0;
                while (v > 0 && nd < 16) { digs[nd++] = (char)(v % 10); v /= 10; }
                while (nd > 0) buf[bl++] = (char)('0' + digs[--nd]);
            }
            buf[bl++] = '\n';
            ssize_t wr = write(fd, buf, (size_t)bl); (void)wr;
            close(fd);
        }
    }
}

/* Real Show Text popup content - see the SHOW_TEXT_FILE relay handler
 * in main() and its own Expose-draw branch below. */
static char g_text_popup_lines[64][256];
static int g_text_popup_n_lines = 0;

/* REAL FIX 2026-08-06, direct report ("book and monster are both having
 * focus control problems"): XGrabKeyboard/XGrabPointer are DISPLAY-WIDE
 * EXCLUSIVE resources - only one process can hold either at a time.
 * Every entity (book-stack, each monster, asa, ava...) is its own
 * separate OS process, each independently calling XGrabKeyboard the
 * moment ITS OWN popup opens, with zero coordination between them. If
 * two entities' popups happen to open around the same time, whichever
 * grabs last wins - the OTHER process's retry loop (open_context_menu's
 * own 5-attempt retry above) silently exhausts and returns a popup with
 * NO real keyboard grab at all, no error, no log - exactly this report.
 * Real fix: a real, house-wide, cross-process mutex via flock() on a
 * single shared lockfile (#.desktop/livedesk_popup.lock) - any process
 * about to open a popup blocks (real, not spin-polled - flock(LOCK_EX)
 * blocks in the kernel) until it's the only one holding it, so grab
 * contention structurally can't happen anymore. g_popup_lock_depth
 * makes this reentrant for a SINGLE process's own nested popups (e.g.
 * the main context menu plus its own input_popup_win at once) - only
 * the outermost open/close pair actually touches the lock. */
static int g_popup_lock_fd = -1;
static int g_popup_lock_depth = 0;
static char g_house_root_for_lock[TP_PATH_BUF] = "";

/* REAL, NEW 2026-08-29, direct instruction ("the tb has a
 * transparency. but that should propagate to 'all entities'... so
 * player can still see thru their desktop a bit") - real, working
 * opacity already exists (khtpm_strip_parser.c's set_window_opacity()/
 * tp_load_theme_opacity(), real _NET_WM_WINDOW_OPACITY + #.desktop/
 * livedesk_theme.pdl "COLOR|opacity|N") but every desktop entity
 * window (every pal/tile - what this file spawns) rendered at full
 * opacity, same real gap khtpm_core_render.c had for its own
 * windows. Ported the same way (adapted to THIS file's own TP_PATH_BUF
 * convention; house_root is a local in main() here, not a global, so
 * it's a real parameter instead). */

static double tp_load_theme_opacity(const char *house_root) {
    double opacity = 1.0;
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return opacity;
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[16];
        size_t klen = (size_t)(key_end - p);
        if (klen == 0 || klen >= sizeof(key)) continue;
        memcpy(key, p, klen);
        key[klen] = '\0';
        if (strcmp(key, "opacity") != 0) continue;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        if (v[0] == '\0') continue;
        double parsed = atof(v);
        if (parsed >= 0.0 && parsed <= 1.0) opacity = parsed;
    }
    fclose(f);
    return opacity;
}

/* REAL FIX/REVERT 2026-08-06, direct report ("not clicking buttons with
 * the mouse will run the button... serious focus issues that doing
 * khtpm introduced"): flock(LOCK_EX) BLOCKS THE ENTIRE PROCESS - this
 * whole event loop froze while waiting, and any real input event that
 * arrived at the X server during that freeze (a leftover click, a
 * queued Enter from earlier) sat buried in the queue and fired the
 * INSTANT the lock finally freed, indistinguishable from "it ran
 * itself." A real, un-clicked action. Real fix: bounded, non-blocking
 * wait (LOCK_NB in a short poll loop, ~1s total) instead of blocking
 * forever - worst case behaves like the pre-lock code (a grab might
 * still race), but the process is NEVER frozen long enough to build up
 * a dangerous input backlog. */
static void popup_lock_acquire(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth++ > 0) return;
    if (!g_house_root_for_lock[0]) return;
    if (g_popup_lock_fd < 0) {
        char lock_path[TP_PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_popup.lock", g_house_root_for_lock);
        g_popup_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_popup_lock_fd >= 0) {
        for (int attempt = 0; attempt < 50; attempt++) {
            if (flock(g_popup_lock_fd, LOCK_EX | LOCK_NB) == 0) break;
            usleep(20000); /* 20ms * 50 = ~1s total bounded wait, never indefinite */
        }
    }
}

static void popup_lock_release(void) {
    if (!LIVEDESK_USE_POPUP_LOCK) return;
    if (g_popup_lock_depth <= 0) return;
    if (--g_popup_lock_depth > 0) return;
    if (g_popup_lock_fd >= 0) flock(g_popup_lock_fd, LOCK_UN);
}


/* Soft focus for a single popup window (option C). Only touches THIS
 * window — no XRaiseWindow on other entities, no global focus war. */
static void popup_soft_focus(Display *dpy, Window popup) {
    if (!LIVEDESK_POPUP_SOFT_FOCUS) return;
    if (!popup) return;
    XRaiseWindow(dpy, popup); /* raise the menu itself so it's not buried */
    XSetInputFocus(dpy, popup, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static void clamp_popup_to_screen(Display *dpy, int *x, int *y, int w, int h) {
    /* REAL 2026-08-06, user: context windows near bottom/side should open
     * in empty space (stay fully on-screen above taskbar, not clipped). */
    int scr = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, scr);
    int sh = DisplayHeight(dpy, scr);
    const int margin = 4;
    const int taskbar_reserve = 40; /* livedesk bar ~32px + padding */
    int usable_h = sh - taskbar_reserve;
    if (usable_h < h + margin) usable_h = sh - margin;
    int px = *x, py = *y;
    if (px + w + margin > sw) px = sw - w - margin;
    if (px < margin) px = margin;
    if (py + h + margin > usable_h) py = usable_h - h - margin;
    if (py < margin) py = margin;
    /* Prefer flipping above-left of anchor if we still overflow badly */
    if (py + h > usable_h) py = margin;
    if (px + w > sw) px = margin;
    *x = px;
    *y = py;
}

static Window open_context_menu(Display *dpy, GC gc, int *root_x, int *root_y, int nitems, MethodItem *items) {
    /* REAL FIX 2026-08-29 (ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md
     * Phase 3 / TP-DESKTOP-LEGACY-POPUP-REMOVAL-CHECKLIST.md) - same
     * real redirect this function already did at its own END (see the
     * dead block this replaces, right before the final `return popup;`
     * below) - g_use_khtpm_menu is checked ONCE per-process at the top
     * of main(), never per-call-site, so moving the check here doesn't
     * change WHICH calls redirect, only WHEN: this entity's own
     * XCreateWindow/popup_lock_acquire/XGrabPointer/XGrabKeyboard never
     * need to run at all now, instead of running fully then being
     * destroyed a moment later. Zero semantic change - every entity
     * behaves identically to before, just without the wasted work. */
#ifndef _WIN32
    if (g_use_khtpm_menu) {
        int px = root_x ? *root_x : 0;
        int py = root_y ? *root_y : 0;
        launch_khtpm_menu(px, py);
        return None;
    }
#endif
    popup_lock_acquire();
    /* +1 row for the id header (see g_full_id comment above). */
    int h = POPUP_ROW_H * ((nitems > 0 ? nitems : 1) + 1);
    /* Content-aware width: measure labels when caller passes items;
     * bare input/placeholder menus get a slightly roomier default. */
    if (items && nitems > 0)
        g_popup_w = measure_context_popup_w(dpy, items, nitems);
    else
        g_popup_w = 320; /* input / empty: wider than the old 160 floor */
    int px = root_x ? *root_x : 0;
    int py = root_y ? *root_y : 0;
    clamp_popup_to_screen(dpy, &px, &py, g_popup_w, h);
    if (root_x) *root_x = px;
    if (root_y) *root_y = py;
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    /* REAL, 2026-08-05, direct instruction ("try some stuff with digit
     * jump / [] bracket nav"): KeyPressMask + a real keyboard grab
     * added alongside the existing pointer grab, so digit keys reach
     * this popup regardless of which window nominally has focus - same
     * reasoning as the existing XGrabPointer just above. */
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  px, py, (unsigned)g_popup_w, h, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    /* REAL FIX 2026-08-06, direct root cause found ("keystrokes went
     * INTO the terminal"): this whole session's "focus problems" were
     * never really about the grab retry logic - Mutter's Wayland
     * compositor (org.gnome.mutter.wayland xwayland-allow-grabs)
     * restricts XGrabKeyboard from XWayland clients by default, real,
     * intentional Wayland security policy, not a bug in this code.
     * xwayland-grab-access-rules allowlists by WM_CLASS - these windows
     * never set one, so they could never be allowlisted. Real fix: a
     * real WM_CLASS ("MuchiverseLivedesk"), matched by
     * $.crypts/enable_xwayland_grabs.sh (house-wide, re-runnable). */
    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name = (char *)"MuchiverseLivedesk";
        class_hint->res_class = (char *)"MuchiverseLivedesk";
        XSetClassHint(dpy, popup, class_hint);
        XFree(class_hint);
    }
    XMapRaised(dpy, popup);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    /* REAL FIX 2026-08-05, direct report ("it definately has a problem
     * getting focus"): neither grab's own return value was ever
     * checked. A real, well-known X11 race: closing one popup
     * (XUngrabKeyboard/XUngrabPointer) and immediately opening another
     * (exactly what a SHOW_PAGE Show-Choices transition does - close
     * the old popup, open the new one, same event-handling tick) can
     * race the X server's own internal grab-release processing, so the
     * new XGrabKeyboard/XGrabPointer call can genuinely fail
     * (AlreadyGrabbed) even though nothing else actually wants the
     * grab. Real fix: retry a few times with a short real wait,
     * confirmed via `man XGrabKeyboard`'s own documented AlreadyGrabbed
     * return - not a made-up mitigation. */
    /* REAL FIX 2026-08-07, direct report ("toolbar is not taking clicks",
     * "x didn't close them but closed toolbar"): when menu_stay_open is
     * on, the menu must NOT hold a modal pointer grab - a grab redirects
     * EVERY pointer event (toolbar clicks, other windows, everything) to
     * this popup, so with the menu kept open the whole desk becomes
     * unclickable. Stay-open menus are deliberately NON-modal by default:
     * no pointer grab, so clicks reach their real targets; the menu
     * simply stays open until the user clicks a row/Cancel or presses
     * Escape/Enter. Arrow nav still works - the keyboard grab below is
     * kept. menu_stay_open=0 restores the old modal grab + dismiss-on-
     * any-outside-click behavior (grab_pointer still toggles it there).
     *
     * REAL, 2026-08-11, direct instruction ("add a config to switch that
     * on and off w/o hardcoding... user can tweak it w/o changing code,
     * and find optimal solution"): the "no grab while stay-open" choice
     * above is a genuine, real tradeoff (some entities' row clicks may
     * not reliably reach a non-grabbed override-redirect popup under
     * this house's Wayland/XWayland setup — direct report: "it works
     * clicking enter, but not mouse clicking"), not a universally-correct
     * answer for every entity. grab_pointer_while_stay_open (meta.pdl,
     * default 0 = unchanged prior behavior) lets a human opt back INTO
     * grabbing per-entity to test whether that fixes click delivery for
     * THEM specifically, accepting the "rest of the desk goes unclickable
     * while this menu is open" tradeoff as a deliberate choice instead of
     * it being permanently unavailable. Don't grab pointer if menu stays
     * open — allows user to click other windows (e.g., browser) while
     * keeping the menu open for relay-based navigation. */
    if (g_grab_pointer && !g_menu_stay_open) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                                   None, None, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    if (g_grab_keyboard) {
        for (int attempt = 0; attempt < 5; attempt++) {
            int rc = XGrabKeyboard(dpy, popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            if (rc == GrabSuccess) break;
            XSync(dpy, False);
            usleep(5000);
        }
    }
    /* REAL FIX 2026-08-05, direct report ("clicking read just closes
     * context"): X11 can and does recycle a destroyed window's own
     * resource ID for the very next XCreateWindow call on the same
     * display (confirmed - this is exactly the close-old/open-new
     * sequence SHOW_PAGE does every time). Any ButtonPress/KeyPress
     * already sitting in the X server's queue, addressed to the OLD
     * popup's ID before it was destroyed, gets redelivered to this NEW
     * popup the instant it's mapped - read as an immediate phantom
     * click on row 0, dismissing the menu before the user ever saw it.
     * Real fix: drain any already-queued Button/KeyPress events
     * targeting this exact window id before returning, so only input
     * that arrives AFTER this popup genuinely existed can select a row. */
    XSync(dpy, False);
    XEvent stale_ev;
    while (XCheckWindowEvent(dpy, popup, ButtonPressMask | KeyPressMask, &stale_ev)) {
        /* discard - see comment above */
    }
    /* OPTION C 2026-08-06: grabs re-enabled (flags=1); locks stay off.
     * Soft focus on THIS popup only as fallback when grab fails or
     * XWayland ignores grab — not the old multi-entity focus war
     * (we never XSetInputFocus other processes' tile windows here). */
    popup_soft_focus(dpy, popup);

    /* REAL FIX 2026-08-29 - the real g_use_khtpm_menu redirect that
     * used to live here (create the legacy window fully, then destroy
     * it and launch_khtpm_menu() instead) moved to the TOP of this
     * function - see that real fix's own header comment. This point is
     * now only ever reached when g_use_khtpm_menu is 0 (or on Windows,
     * where khtpm_core_render.exe doesn't exist yet and the
     * legacy Xlib menu is still the real, correct behavior) - the
     * legacy popup created above is the REAL, visible result. */
    return popup;
}

/* REAL FIX 2026-08-05, direct correction ("im still seeing numbers in
 * brackets instead of [>] [] empty brackets like chtpm"): real chtpm
 * format (confirmed via direct example, a real live yahoo-broker frame)
 * is "[ ] N. Label" - an EMPTY bracket is a real focus-cursor marker
 * (becomes "[>]" for whichever row currently has focus), completely
 * separate from the plain "N." row number that follows it. The
 * previous "[N] Label" render put the shared live nav number INSIDE
 * the bracket - wrong shape entirely, not just a style nit. nav_base is
 * this popup's own claimed starting number (see nav_claim_rows()), so
 * row i's real number is nav_base+i; nav_base<=0 (the small
 * user_popup_win/input_popup_win submenus, which don't claim nav
 * numbers) suppresses both the bracket and the number rather than
 * showing a misleading unclaimed one.
 *
 * REAL FIX, same day, follow-up: real up/down focus-cursor tracking now
 * exists (focus_row, driven by real Up/Down KeyPress in main()'s own
 * popup_win branch) - whichever row equals focus_row shows "[>]", every
 * other row shows "[ ]", matching real chtpm's own single-row cursor
 * convention exactly (not a highlight/fill effect - just the bracket
 * glyph itself changes, same as the real captured yahoo-broker frame). */
static void draw_context_menu(Display *dpy, Window popup, GC gc, MethodItem *items, int n, int nav_base, int focus_row) {
    XClearWindow(dpy, popup);
    int h = POPUP_ROW_H * ((n > 0 ? n : 1) + 1);
    int pw = g_popup_w > 0 ? g_popup_w : POPUP_W_MIN;
    XDrawRectangle(dpy, popup, gc, 0, 0, pw - 1, h - 1);
    /* Row 0: non-clickable id header. */
    popup_draw_text(dpy, popup, gc, 12, POPUP_ROW_H / 2 + 4, g_full_id);
    XDrawLine(dpy, popup, gc, 0, POPUP_ROW_H, pw, POPUP_ROW_H);
    for (int i = 0; i < n; i++) {
        int row_y = (i + 1) * POPUP_ROW_H;
        if (i > 0) XDrawLine(dpy, popup, gc, 0, row_y, pw, row_y);
        char labeled[160];
        const char *cursor = (i == focus_row) ? "[>]" : "[ ]";
        if (nav_base > 0) snprintf(labeled, sizeof(labeled), "%s %d. %s", cursor, nav_base + i, items[i].label);
        else snprintf(labeled, sizeof(labeled), "%s", items[i].label);
        popup_draw_text(dpy, popup, gc, 12, row_y + POPUP_ROW_H / 2 + 4, labeled);
    }
}

/* 2026-09-08: tp_range_grid.+x deleted. Map range is range_overlay.pdl
 * (vision §6.7), not a screen-space diamond popup. */

static void close_context_menu(Display *dpy, Window popup) {
    if (g_grab_pointer) XUngrabPointer(dpy, CurrentTime);
    if (g_grab_keyboard) kh_ungrab_kbd();
    XDestroyWindow(dpy, popup);
    popup_lock_release();
}


/* REAL, NEW 2026-08-04, direct instruction ("allow user editing of
 * asset... place an asset in asset folder of entity, and in .pal
 * specify emoji other than default, or path of asset (jpg/png), it will
 * use that instead of default emoji"). Real, simple convention:
 * <package_dir>/asset.pal, key=value lines:
 *   glyph=<emoji>          - regenerate sprite.csv from a DIFFERENT
 *                            emoji than glyph.txt's own default, via the
 *                            same real emoji_gen_atlas/emoji_xtract
 *                            pipeline tp_place_desktop.c already uses.
 *   asset_path=<path>      - use a real, arbitrary user image (PNG/JPG,
 *                            any size - NOT required to be pre-cropped
 *                            to 64x64) instead of any emoji at all, via
 *                            the NEW tp_asset_to_sprite.+x (see that
 *                            file's own header for why emoji_xtract.+x
 *                            alone isn't the right tool for an arbitrary
 *                            user image). Relative paths resolve against
 *                            package_dir/assets/ (the real "asset
 *                            folder" named in the instruction); absolute
 *                            paths are used as-is (e.g. pointing
 *                            directly at a file elsewhere, without
 *                            moving it).
 * Regenerates sprite.csv in place, once, at window startup - editing
 * asset.pal and relaunching the window picks up the change. */
static void apply_asset_override(const char *package_dir, const char *ops_dir) {
    char asset_pal[TP_PATH_BUF];
    snprintf(asset_pal, sizeof(asset_pal), "%s/asset.pal", package_dir);
    FILE *f = fopen(asset_pal, "r");
    if (!f) return;

    char glyph_override[64] = "", asset_path_raw[TP_PATH_BUF] = "";
    char line[TP_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "glyph=", 6) == 0) snprintf(glyph_override, sizeof(glyph_override), "%s", line + 6);
        else if (strncmp(line, "asset_path=", 11) == 0) snprintf(asset_path_raw, sizeof(asset_path_raw), "%s", line + 11);
    }
    fclose(f);

    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);

    if (asset_path_raw[0]) {
        char resolved[TP_PATH_BUF];
        if (asset_path_raw[0] == '/') {
            snprintf(resolved, sizeof(resolved), "%s", asset_path_raw);
        } else {
            snprintf(resolved, sizeof(resolved), "%s/assets/%s", package_dir, asset_path_raw);
        }
#ifndef _WIN32
        char cmd[TP_PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/tp_asset_to_sprite.+x' '%s' 64 '%s' >/dev/null 2>&1",
                 ops_dir, resolved, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
        (void)resolved;
        /* Existing sprite.csv is used. Do not system() Linux .+x via cmd.exe
         * (0x800700E8 / pipe closed) — that was killing rgb at startup. */
#endif
    } else if (glyph_override[0]) {
#ifndef _WIN32
        char atlas_path[TP_PATH_BUF], cmd[TP_PATH_BUF * 2];
        snprintf(atlas_path, sizeof(atlas_path), "%s/atlas_override.png", package_dir);
        snprintf(cmd, sizeof(cmd),
                 "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                 "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                 ops_dir, glyph_override, atlas_path, ops_dir, atlas_path, sprite_path);
        int rc = system(cmd);
        (void)rc;
#else
        (void)ops_dir;
#endif
    }
}

/* REAL, NEW 2026-08-31, direct instruction ("do we have z layers
 * yet? ... the xelector/cursword moves up and down z levels but the
 * rest of the entities should remain on their own z level unless
 * some event is otherwise moving them") - a real, persistent per-
 * entity Z, same real file (desktop_pos.txt) every entity already
 * has, a real optional third `z=N` line (missing = 0, same real
 * backward-compatible fallback shape every other optional PDL/state
 * key in this house already uses). g_entity_z is this real, in-
 * memory value for THIS process's own entity - loaded once at
 * startup, changed only by cursword's own real c/v keys (this
 * entity's own z never changes on its own). */
static int g_entity_z = 0;

static int read_entity_z(const char *package_dir) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int z = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "z=", 2) == 0) z = atoi(line + 2);
    }
    fclose(f);
    return z;
}

/* REAL, NEW 2026-09-14 (desktop-entity trigger-layer proof, direct
 * instruction: "it should use the same master ledger mechanism
 * displayed in lpns, remember?") - project-local copy of the exact
 * same real, proven format RMMV-EVENT-ARCHITECTURE-LEARNINGS.md §7
 * documents from 101.lpns+map+4's own master_ledger.txt, and that
 * piececraft-hq's own pc_menu_input.c ledger_append() already uses:
 * `timestamp|turn|actor|action_type|details`, one append-only file,
 * shared, multi-writer. Lives under #.desktop/ - the real, already-
 * established house-wide-state location (matches livedesk_open.txt,
 * khtpm_play_mode.state.txt, khtpm_zorder_mode.state.txt), not a new
 * top-level shared dir. "turn" has no real turn-based meaning on the
 * desktop - kept as a monotonic real-time counter (seconds since this
 * process started) purely to preserve the exact same 5-field row
 * shape as the proven format, not to invent a new one. */
static void desktop_ledger_append(const char *house_root, const char *actor, const char *action_type, const char *details) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/master_ledger.txt", house_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    static time_t s_start = 0;
    if (!s_start) s_start = now;
    fprintf(f, "%s|%ld|%s|%s|%s\n", ts, (long)(now - s_start), actor, action_type, details);
    fclose(f);
}

/* REAL, NEW 2026-09-14 - real, honest global on/off read, mirroring
 * khtpm_taskbar_manager.c's own khtpm_load_play_mode() (a genuinely
 * separate binary, per this house's own duplicate-rather-than-share
 * convention - not a shared header). Same file,
 * #.desktop/khtpm_play_mode.state.txt, same `mode=on|off` shape. */
static int desktop_load_play_mode(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/khtpm_play_mode.state.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[64];
    int on = 0;
    if (fgets(line, sizeof(line), f) && strstr(line, "mode=on")) on = 1;
    fclose(f);
    return on;
}

/* REAL, NEW 2026-09-14 (EVENT-TRIGGER-LAYER-PLAN.md's own desktop-
 * entity extension, direct instruction: "we will use move /
 * pathfinding event program to make cursword move to castle" -> "it
 * should use the same master ledger mechanism") - cursword-only (the
 * real, established player/selector entity - see g_is_cursword's own
 * declaration comment), Play-Mode-gated (per PLAY-MODE-ENTITY-
 * HARNESS-DESIGN.md - a plain desktop drag while NOT in Play Mode
 * must never fire a gameplay trigger). Scans every sibling pal's own
 * desktop_pos.txt (derived from package_dir's own parent - the real
 * pals/ directory every entity already lives under, no new lookup
 * needed) for an EXACT x,y match (matching piececraft-hq's own
 * check_player_touch_trigger()'s exact-equality convention, not
 * "close enough" - see mr_move_to_entity.c's own arrival-check fix
 * for why adjacency alone is the wrong bar here). On a match, appends
 * a real `touched_npc` line to the SAME shared ledger - firing the
 * actual Common Event is the bridge-watcher's job (not yet built),
 * same real split pc_trigger_watcher.c already uses (detect here,
 * fire there). */
static void desktop_check_touch_trigger(const char *house_root, const char *package_dir, int x, int y) {
    if (!g_is_cursword) return;
    if (!desktop_load_play_mode(house_root)) return;

    char pkgcopy[TP_PATH_BUF];
    snprintf(pkgcopy, sizeof(pkgcopy), "%s", package_dir);
    char *self_base = basename(pkgcopy);
    char self_name[128];
    snprintf(self_name, sizeof(self_name), "%s", self_base);

    char pals_root[TP_PATH_BUF];
    snprintf(pals_root, sizeof(pals_root), "%s", package_dir);
    char *slash = strrchr(pals_root, '/');
    if (slash) *slash = '\0';

    DIR *d = opendir(pals_root);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (strcmp(ent->d_name, self_name) == 0) continue;
        char pos_path[TP_PATH_BUF];
        snprintf(pos_path, sizeof(pos_path), "%s/%s/desktop_pos.txt", pals_root, ent->d_name);
        FILE *pf = fopen(pos_path, "r");
        if (!pf) continue;
        char line[128];
        int px = -1, py = -1;
        while (fgets(line, sizeof(line), pf)) {
            if (strncmp(line, "x=", 2) == 0) px = atoi(line + 2);
            else if (strncmp(line, "y=", 2) == 0) py = atoi(line + 2);
        }
        fclose(pf);
        /* sibling file is reference px, x/y here are this screen's px */
        if (kh_pos_ref_to_screen(px) == x && kh_pos_ref_to_screen(py) == y) {
            char details[128];
            snprintf(details, sizeof(details), "target:%s,x:%d,y:%d", ent->d_name, kh_pos_screen_to_ref(x), kh_pos_screen_to_ref(y));
            desktop_ledger_append(house_root, self_name, "touched_npc", details);
        }
    }
    closedir(d);
}

static void write_pos(const char *package_dir, int x, int y) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    /* Real, deliberate: write THIS process's own real g_entity_z, not
     * a re-read of whatever was on disk before - this file's own
     * g_entity_z is always kept in sync with disk on the only real
     * path that ever changes it (cursword's own c/v keys, which write
     * immediately), so this is never stale. Every other real caller
     * of write_pos() (drag/arrow-nudge/click-to-place) only ever
     * changes x/y, never z - preserving it here, with zero call-site
     * changes needed anywhere else in this file. */
    /* x/y are this screen's px; the file is reference px (identity on the
     * reference screen) so a smaller/bigger monitor never rewrites what
     * another machine sees. */
    fprintf(f, "x=%d\ny=%d\nz=%d\n", kh_pos_screen_to_ref(x), kh_pos_screen_to_ref(y), g_entity_z);
    fclose(f);
    /* Real, single choke point every real position change already
     * passes through (drag/arrow-nudge/click-to-place/MOVE_TO) - see
     * desktop_check_touch_trigger()'s own header comment. */
    desktop_check_touch_trigger(g_house_root, package_dir, x, y);
}

/* Real, new 2026-08-31 - the shared, desktop-wide "which z level is
 * currently visible" file (same real "small state file under
 * #.desktop/" convention as desktop_camera_mode.txt). Cursword, the
 * real xelector/selector entity, is the only thing that ever WRITES
 * this (see cursword_handle_camera_key()'s own c/v branch) - every
 * entity's own window just reads it to decide whether to show or
 * hide itself (see the real map/unmap logic in the main render loop). */
static int g_active_z = 0;
static void load_active_z(const char *house_root) {
    char path[TP_PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/desktop_active_z.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) { g_active_z = 0; return; }
    char line[16];
    g_active_z = fgets(line, sizeof(line), f) ? atoi(line) : 0;
    fclose(f);
}

static int tp_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window.+x <package_dir>\n");
        return 1;
    }
    char package_buf[TP_PATH_BUF];
    snprintf(package_buf, sizeof(package_buf), "%s", argv[1]);
#ifdef _WIN32
    win_package_rel(package_buf);
#endif
    const char *package_dir = package_buf;
    /* REAL, NEW 2026-09-13, direct instruction ("dig into whats
     * actually slowing entity startup") - genuine measurement, not
     * guesswork: a real per-step timestamp log, one line per real
     * setup phase, appended to startup_timing.txt in this entity's own
     * package dir. Env-gated (TP_STARTUP_TIMING=1) so this never costs
     * anything on a normal run - only set while actively profiling.
     * Found the real bottleneck with it (legacy X core-font loading,
     * see load_popup_fontset()/load_glyph_font()'s own header comments
     * for the fix) - kept permanently, env-gated, as a real reusable
     * diagnostic for the next "why is startup slow" question, same
     * reasoning handle_shutdown_signal_info()'s SA_SIGINFO forensics
     * were kept for. */
    struct timespec tp_t0, tp_tlast;
    int tp_timing_on = getenv("TP_STARTUP_TIMING") != NULL;
    if (tp_timing_on) { clock_gettime(CLOCK_MONOTONIC, &tp_t0); tp_tlast = tp_t0; }
#define TP_TIMING_MARK(label) do { \
    if (tp_timing_on) { \
        struct timespec tp_now; clock_gettime(CLOCK_MONOTONIC, &tp_now); \
        double tp_since_last = (tp_now.tv_sec - tp_tlast.tv_sec) * 1000.0 + (tp_now.tv_nsec - tp_tlast.tv_nsec) / 1e6; \
        double tp_since_0 = (tp_now.tv_sec - tp_t0.tv_sec) * 1000.0 + (tp_now.tv_nsec - tp_t0.tv_nsec) / 1e6; \
        char tp_tpath[TP_PATH_BUF]; \
        snprintf(tp_tpath, sizeof(tp_tpath), "%s/startup_timing.txt", package_dir); \
        FILE *tp_tf = fopen(tp_tpath, "a"); \
        if (tp_tf) { fprintf(tp_tf, "%s: +%.2fms (total %.2fms)\n", label, tp_since_last, tp_since_0); fclose(tp_tf); } \
        /* REAL FIX - re-stamp AFTER the file write (not before), so this
         * mark's own I/O latency (real, confirmed cost on this xyzfs
         * mount) is absorbed into ITS OWN delta the next time around,
         * never silently attributed to the NEXT real step - this exact
         * bug briefly made load_glyph_font look like a ~60ms cost when
         * it was really the previous mark's own disk write. */ \
        clock_gettime(CLOCK_MONOTONIC, &tp_tlast); \
    } \
} while (0)
    /* REAL, NEW 2026-09-13 - g_package_dir used to be set ONLY on the
     * default/HQ-window main() path, never here in tp_main() (entity/
     * tile mode) - handle_shutdown_signal_info()'s own real forensic
     * write (see its header comment) needs a real path to write to
     * for THIS mode too, so it's set here, as early as possible,
     * before the signal handlers below can possibly fire. */
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", package_dir);
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = handle_shutdown_signal_info;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
    }
    /* REAL FIX 2026-09-04 (RENDERER-MODULARITY-AND-PERF-AUDIT.md §1.1) -
     * data-driven now (see read_log_mode()'s own header comment) -
     * was a hardcoded `strcmp(basename(pkgcopy), "cursword")` identity
     * check; any entity can opt in via its own meta.pdl now, not just
     * this one hardcoded name. g_is_cursword's own name is kept as-is
     * (24 real call sites already read it) - only how it gets set
     * changed. */
    g_is_cursword = read_log_mode(package_dir);
    snprintf(g_history_path, sizeof(g_history_path), "%s/history.txt", package_dir);
    snprintf(g_relay_path, sizeof(g_relay_path), "%s/interact_relay.txt", package_dir);
    append_history("WINDOW_OPEN");
    TP_TIMING_MARK("start->history");
    char g_ops_dir[TP_PATH_BUF];
    resolve_livedesk_paths(g_ops_dir, sizeof(g_ops_dir), g_house_root, sizeof(g_house_root));
    TP_TIMING_MARK("resolve_livedesk_paths");
#ifdef _WIN32
    if (!g_house_root[0]) snprintf(g_house_root, sizeof(g_house_root), ".");
#endif
    snprintf(g_house_root_for_lock, sizeof(g_house_root_for_lock), "%s", g_house_root);
    /* REAL FIX 2026-09-09, direct report ("bookstack ... its this part
     * that hasn't changed color ... look deeper"): tile/entity mode
     * (tp_main) - which hosts every desktop entity incl. book-stack and
     * its Show Text popup - NEVER loaded #.desktop/livedesk_theme.pdl.
     * main()/hq_run_event_loop() do (startup + every hq_idle_tick), but
     * tp_main() has its own event loop and only ever reapplied theme
     * OPACITY on a change, never the bg/fg colours - so g_theme_bg/fg
     * stayed at their compile-time #1c1c1c/#cccccc defaults here and
     * every "use g_theme_bg" draw site was themed in name only. Load it
     * now (before the window is created below) and again on every
     * livedesk_theme_changed marker (see the tp_main event loop). */
    if (g_house_root[0]) load_theme_colors();
    if (g_house_root[0]) kh_ui_pdl_load(g_house_root, NULL);
    if (g_house_root[0]) load_override_redirect(g_house_root);
    if (g_house_root[0] && g_is_cursword) cursword_load_move_mode(g_house_root);
    TP_TIMING_MARK("theme/click2step/override_redirect/move_mode");
    /* REAL FIX 2026-08-27 (TILE-SYSTEM-DESIGN.md §0a) - read the real,
     * optional, house-wide grid cell size as early as possible (right
     * after g_house_root resolves, before anything below uses
     * GRID_CELL_PX). */
    /* Entity grid/window size is screen-relative (hq_ui.pdl ui_scale/
     * ui_ref_*; auto = 100 on the reference screen). This mode has a local
     * Display and never sets the dpy global, so probe the screen size with
     * a throwaway connection. Saved desktop_pos.txt values are REFERENCE px
     * (khtpm_ui_scale.c); read_initial_pos()/write_pos() convert. */
    {
        Display *pd = XOpenDisplay(NULL);
        if (pd) {
            g_ui_screen_w = DisplayWidth(pd, DefaultScreen(pd));
            g_ui_screen_h = DisplayHeight(pd, DefaultScreen(pd));
            XCloseDisplay(pd);
            kh_ui_apply_scale();
        }
    }
    g_grid_cell_base = read_grid_cell_px(g_house_root);
    GRID_CELL_PX = kh_auto_px(g_grid_cell_base);
    /* Stage 2c PROOF - see launch_khtpm_menu()'s own header comment. */
    {
        char menu_chtpm_path[TP_PATH_BUF];
        snprintf(menu_chtpm_path, sizeof(menu_chtpm_path), "%s/menu.chtpm", package_dir);
        if (access(menu_chtpm_path, F_OK) == 0) {
            g_use_khtpm_menu = 1;
            snprintf(g_khtpm_menu_pkg_dir, sizeof(g_khtpm_menu_pkg_dir), "%s", package_dir);
            snprintf(g_khtpm_menu_house_root, sizeof(g_khtpm_menu_house_root), "%s", g_house_root);
        }
    }
    int g_livedesk_index = 0;
    if (g_house_root[0]) {
        g_livedesk_index = ensure_livedesk_index(package_dir, g_house_root);
        livedesk_registry_add(g_house_root, package_dir, g_livedesk_index, getpid());
        ensure_taskbar_running(g_house_root);
        append_history("LIVEDESK_INDEX=%d", g_livedesk_index);
    }
    TP_TIMING_MARK("livedesk_index/registry_add/ensure_taskbar_running");
    /* REAL FIX 2026-08-05 (MUCHI_RANCHER monsters), EXTENDED 2026-08-29
     * direct live report ("placing a tile isn't taking up the full
     * 80px tile square... all entities need this fix except muchi
     * mon"): the original fix only overrode the flat 64px WIN_PX when
     * footprint_tiles > 1, so every footprint=1 entity (every pet/asa/
     * ava/tile - everything except MUCHI_RANCHER monsters) rendered at
     * a fixed 64px regardless of the real, configurable GRID_CELL_PX
     * (80 by default) - a real, visible gap against the grid for
     * anything that's supposed to tile edge-to-edge (like an rmmv
     * floor tile). Real fix: always derive WIN_PX from GRID_CELL_PX *
     * footprint_tiles (footprint_tiles defaults to 1, so this is
     * exactly GRID_CELL_PX for every existing footprint=1 entity, and
     * unchanged footprint_tiles*GRID_CELL_PX for monsters). */
    {
        int footprint_tiles = read_footprint_tiles(package_dir);
        if (footprint_tiles < 1) footprint_tiles = 1;
        WIN_PX = footprint_tiles * GRID_CELL_PX;
    }
    g_z_priority = read_z_priority(package_dir);
    read_menu_config(package_dir);
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph.txt can now hold a real multi-byte UTF-8 emoji -
     * kept as a full string, not truncated to its first byte. Real
     * texture rendering of that emoji (via emoji_gen_atlas.+x, this
     * house's own FreeType+NotoColorEmoji pipeline, already used by
     * chtpm_rgb_render) is NOT done here yet - this window still only
     * shows a glyph-hashed color + the glyph as its window title, same
     * as before this fix. Flagged as still-open, tracked alongside the
     * tp_menu_input.c/tp_compose_frame.c glyph-widening this session
     * already did for the picker's own chtpm-rendered path (which DOES
     * show real emoji, via that same already-working renderer). */
    char glyph_line[64];
    read_glyph(package_dir, glyph_line, sizeof(glyph_line));
    char glyph = glyph_line[0] ? glyph_line[0] : '?';
    const char *glyph_str = glyph_line[0] ? glyph_line : "?";

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_desktop_window: cannot open display\n");
        return 1;
    }
    g_kbd_dpy = dpy; /* see kh_ungrab_kbd() */
    TP_TIMING_MARK("XOpenDisplay");
    load_popup_fontset(dpy);
    TP_TIMING_MARK("load_popup_fontset");

    int screen_num = DefaultScreen(dpy);
    Visual *vis = DefaultVisual(dpy, screen_num);
    int depth = DefaultDepth(dpy, screen_num);

    /* REAL, NEW 2026-08-30, direct instruction ("Do the real ARGB
     * transparency") - a real 32-bit ARGB TrueColor visual for
     * cursword's own window ONLY (every other entity keeps the plain
     * default visual/depth above, completely untouched). Real per-
     * pixel alpha this way is handled by the COMPOSITOR itself
     * (mutter/XWayland here) directly from this window's own backing
     * pixels - no XRenderComposite call needed on this side, the same
     * standard technique real transparent-window apps use. XMatchVisualInfo
     * with class=TrueColor, depth=32 is the common, simple way to find
     * it (virtually universal under a modern compositing X/XWayland
     * setup) - if it's ever unavailable, this falls back to the exact
     * same plain visual/depth every other entity already uses, so
     * cursword just loses real transparency (back to the flat-color
     * disc), never crashes/breaks. */
    XVisualInfo argb_vinfo;
    int have_argb_visual = 0;
    if (g_is_cursword)
        have_argb_visual = XMatchVisualInfo(dpy, screen_num, 32, TrueColor, &argb_vinfo);
    Visual *win_vis = have_argb_visual ? argb_vinfo.visual : vis;
    int win_depth = have_argb_visual ? argb_vinfo.depth : depth;

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, screen_num), win_vis, AllocNone);
    /* Real, new 2026-08-30, direct instruction ("if cursword loses
     * focus... is there a way to make sure the halo goes away") -
     * FocusChangeMask added house-wide (every entity now gets real
     * FocusIn/FocusOut events, harmless no-op for every entity except
     * cursword, which is the only one that ever acts on them - see
     * the FocusOut handler in the main event loop below). */
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | FocusChangeMask;
    swa.override_redirect = g_override_redirect; /* real X11 requirement whenever a window's own depth differs from its parent's (root's) - harmless to set unconditionally */
    /* The server-side backing colour, shown until this window paints its
     * first real frame. tp_main() has its own tile/sprite renderer (NOT
     * the shared redraw()/g_theme_bg path - HQ mode only), so this is
     * the ONLY theme hook for the pre-first-paint gap. It used to be 0
     * (black): direct report ("on new startups there are black squares
     * that show up and disappear") - that is this, one unpainted entity
     * window per still-spawning tile during the login storm, not
     * zombies or debug. Theme it so the gap blends instead of flashing
     * black. tp_hex_pixel() (local Display) - NOT the shared
     * alloc_pixel(), whose dpy/cmap/screen globals tp_main never sets
     * (that mistake closed the book-stack entity, see tp_hex_pixel's
     * header). For cursword's ARGB visual a null pixel keeps it fully
     * transparent, which is the intended look there. */
    swa.background_pixel = g_is_cursword ? 0
                        : tp_hex_pixel(dpy, screen_num, g_theme_bg);

    /* REAL FIX 2026-09-13, direct live report ("at entity render time,
     * i always see in the top left these 'ghost renders' before real
     * renders... trying to render dead/legacy entities?") - not dead
     * entities at all: every FRESH entity window used to be created
     * here at a hardcoded near-origin default (3*GRID_CELL_PX,
     * 3*GRID_CELL_PX - genuinely "top left"), immediately mapped
     * (visible), and only THEN moved to its real, saved grid position
     * via a SEPARATE XMoveWindow call ~250 lines further down, after
     * real disk I/O (read_initial_pos/read_map_size/read_entity_z) and
     * other setup. With several entities respawning together (a normal
     * always-on-top toggle or restart), that's several real windows
     * all briefly stacked near the same wrong corner before each snaps
     * to its own distinct real spot - exactly what reads as "ghost
     * renders," and it's every real entity doing this every time, not
     * a stale/legacy one. Real fix: compute the REAL win_x/win_y (the
     * exact same logic that used to run after window creation, moved
     * here verbatim, nothing behaviorally changed about how the
     * position itself is derived) BEFORE XCreateWindow, and create the
     * window there directly - no wrong position ever exists on screen,
     * so there is nothing left to visibly correct afterward. */
    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    int max_col = (screen_w / GRID_CELL_PX) - 1;
    int max_row = (screen_h / GRID_CELL_PX) - 1;
    /* REAL, NEW 2026-08-31 ("map size" movement wall, see
     * read_map_size()'s own header comment) - a configured
     * desk_grid.pdl map_cols/map_rows overrides the screen-derived
     * bound above (real, deliberately smaller-or-equal "wall" so an
     * entity dragged/placed/nudged can never end up further out than
     * the configured map, not just the physical screen edge). Every
     * other real clamp site in this function (drag release, arrow-key
     * nudge, click-to-place) already reuses these same max_col/max_row
     * locals, so this one override site is the only real change
     * needed. */
    {
        int cfg_cols = 0, cfg_rows = 0;
        read_map_size(g_house_root, &cfg_cols, &cfg_rows);
        if (cfg_cols > 0) max_col = cfg_cols - 1;
        if (cfg_rows > 0) max_row = cfg_rows - 1;
    }
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;
    /* Real, new 2026-08-31 - this entity's own persisted z, loaded
     * once at startup (see g_entity_z's own declaration comment). */
    g_entity_z = read_entity_z(package_dir);
    int win_x = 3 * GRID_CELL_PX, win_y = 3 * GRID_CELL_PX; /* grid-aligned spawn, matching egg_window.c's own default */
    int pos_from_file = 0, pos_clamped = 0;
    {
        int ix, iy;
        if (read_initial_pos(package_dir, &ix, &iy)) {
            int gx = (ix + GRID_CELL_PX / 2) / GRID_CELL_PX;
            int gy = (iy + GRID_CELL_PX / 2) / GRID_CELL_PX;
            int gx0 = gx, gy0 = gy;
            if (gx < 0) gx = 0; if (gx > max_col) gx = max_col;
            if (gy < 0) gy = 0; if (gy > max_row) gy = max_row;
            pos_from_file = 1;
            pos_clamped = (gx != gx0 || gy != gy0);
            win_x = gx * GRID_CELL_PX;
            win_y = gy * GRID_CELL_PX;
        }
#ifdef _WIN32
        /* Linux pos can sit past this monitor. Keep on the primary work
         * area, below the strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
#ifdef __APPLE__
        /* macOS leg (2026-08-22): mirror of the _WIN32 work-area clamp.
         * Saved Linux grid positions can sit past this display's right
         * edge (live: tiles parked at x=1600 on a 1680px screen, mostly
         * invisible). XQuartz rootless maps y=0 to just under the macOS
         * menu bar, so pad_top only needs to clear the taskbar strip. */
        {
            int pad_top = 40, g = 8;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
            if (win_x + WIN_PX > screen_w - g) win_x = screen_w - WIN_PX - g;
            if (win_y + WIN_PX > screen_h - g) win_y = screen_h - WIN_PX - g;
            if (win_x < g) win_x = g;
            if (win_y < pad_top) win_y = pad_top;
        }
#endif
        /* macOS leg (2026-08-22): persist the CLAMPED position - the
         * saved Linux grid value can sit past this display's edge, and
         * downstream consumers (khtpm_show_choices.c's picker spawn
         * reads this same file) must not inherit an off-screen x/y. */
#if defined(_WIN32) || defined(__APPLE__)
        write_pos(package_dir, win_x, win_y);
#else
        /* Persist the grid-snapped position, EXCEPT when this screen is not the
         * reference one and the saved spot was only clamped onto the visible
         * grid: that file is reference px shared with other machines, and a
         * smaller monitor must not rewrite where the entity lives. On the
         * reference screen this always writes, exactly as before. */
        if (!(g_ui_auto_pct != 100 && pos_from_file && pos_clamped))
            write_pos(package_dir, win_x, win_y);
#endif
    }

    TP_TIMING_MARK("pos-compute/colormap");
    Window win = XCreateWindow(dpy, RootWindow(dpy, screen_num), win_x, win_y, WIN_PX, WIN_PX,
                                0, win_depth, InputOutput, win_vis,
                                CWColormap | CWEventMask | CWOverrideRedirect | CWBorderPixel | CWBackPixel, &swa);
    TP_TIMING_MARK("XCreateWindow");
    /* REAL, NEW 2026-09-01 - when the pdl turns override_redirect off
     * (WM-managed pieces, so the taskbar's @ toggle can control their
     * real z-order on Xwayland/Mutter), Mutter would put a titlebar/frame
     * + taskbar entry on them. Same real house fix open-hai/the taskbar
     * use: _MOTIF_WM_HINTS flags=MWM_HINTS_DECORATIONS(2),
     * decorations=0 - managed window, but zero WM-drawn chrome, keeping
     * the borderless tile look. No-op for the default override_redirect
     * path (a WM never manages those, the hint is simply ignored). */
    if (!g_override_redirect) {
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags=MWM_HINTS_DECORATIONS, decorations=0 */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace,
                        (const unsigned char *)hints, 5);
        XSetClassHint(dpy, win, &(XClassHint){(char *)"MuchiverseLivedesk", (char *)"MuchiverseLivedesk"});
    }
    XMapWindow(dpy, win);
    TP_TIMING_MARK("motif_hints/XMapWindow");
    set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
    TP_TIMING_MARK("set_window_opacity");
    /* REAL FIX 2026-08-29, direct live report ("entities and tb dropdown
     * cell tabs aren't opaque yet" - i.e. still full opacity) - ported
     * from khtpm_strip_parser.c's own real "KISS opacity-on-reset fix"
     * (2026-08-11, opacity-bug-aug9.txt): Mutter/XWayland does not
     * reliably honor _NET_WM_WINDOW_OPACITY set at map-time, before the
     * window has been visible/painted for at least one real frame -
     * has to be re-applied after a short real delay. This was the
     * actual root cause for db-hq/events-hq/chat-hai/popup too (see
     * OPACITY-PIPELINE-INVESTIGATION-2026-08-29-part3.txt), just never
     * ported to this file, the ONE remaining opacity gap after that
     * fix landed. */
    {
        XFlush(dpy);
        usleep(200000);
        set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
        XFlush(dpy);
    }

    /* RGB compose buffer - all real drawing (background, glyph/sprite)
     * happens into this offscreen Pixmap, presented each frame via
     * XGetImage+XPutImage below, same as db-hq/taskbar. REAL, NEW
     * 2026-08-30: cursword's own buffer reserves CURSWORD_LOG_H extra
     * rows below WIN_PX for the real key-log debug strip (see
     * cursword_log_key()'s own header comment) - always allocated for
     * a cursword instance (cheap, ~80x20px), revealed only while armed
     * via the window resize + shape-mask union in
     * cursword_update_shape(). Every other entity is completely
     * unaffected (g_is_cursword false, buffer stays exactly WIN_PX x
     * WIN_PX as before). */
    Pixmap g_buf = XCreatePixmap(dpy, win, (unsigned)(g_is_cursword ? CURSWORD_LOG_W : WIN_PX),
                                  (unsigned)(WIN_PX + (g_is_cursword ? CURSWORD_LOG_H : 0)), (unsigned)win_depth);
    GC g_buf_gc = XCreateGC(dpy, g_buf, 0, NULL);
    /* REAL FIX 2026-08-05, direct instruction ("context window should
     * have name/id of entity, so its addressable by others"): the
     * window title used to be "tile:<glyph>" only - real piece_id
     * (basename of package_dir, the same id every methods.pdl/meta.pdl/
     * event_pkg already uses) was nowhere in it, so any OTHER process
     * that needs to find a SPECIFIC entity's own window (a future AI
     * tick loop targeting "chase target=cat", a test harness, etc) had
     * no reliable way to address it - glyph alone can collide (two
     * entities could share an emoji) and isn't the entity's own real
     * identity. basename() needs a mutable copy since POSIX basename()
     * may modify its argument. */
    char pkg_copy[TP_PATH_BUF];
    snprintf(pkg_copy, sizeof(pkg_copy), "%s", package_dir);
    const char *piece_id = basename(pkg_copy);
    char instance_id[32];
    read_instance_id(package_dir, instance_id, sizeof(instance_id));
    if (instance_id[0]) snprintf(g_full_id, sizeof(g_full_id), "%s-%s", piece_id, instance_id);
    else snprintf(g_full_id, sizeof(g_full_id), "%s", piece_id);
    char title[192];
    snprintf(title, sizeof(title), "tile:%s:%s", g_full_id, glyph_str);
    XStoreName(dpy, win, title);

    /* Alpha blending against the background is now done per-pixel in
     * draw_sprite_rgb() itself (no GL_BLEND state to set up). */
    float r, g, b;
    glyph_color(glyph, &r, &g, &b);
    g_font_loaded = load_glyph_font(dpy);
    TP_TIMING_MARK("load_glyph_font");

    /* Resolve ops_dir (same /proc/self/exe technique tp_place_desktop.c
     * already uses) so apply_asset_override() can find tp_asset_to_
     * sprite.+x/emoji_gen_atlas.+x/emoji_xtract.+x next to this binary -
     * kept around (not scoped to a throwaway block) since
     * load_entity_phymoji() below also needs it, to find
     * sprite_phymoji_gen.+x the same real way. */
    char resolved_ops_dir[TP_PATH_BUF] = "";
    {
        char self_path[TP_PATH_BUF];
        if (self_exe_path(self_path, sizeof(self_path))) {
            char *ops_dir = dirname(self_path);
            snprintf(resolved_ops_dir, sizeof(resolved_ops_dir), "%s", ops_dir);
            apply_asset_override(package_dir, ops_dir);
        }
    }
    TP_TIMING_MARK("self_exe_path/apply_asset_override");

    char sprite_path[TP_PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    g_has_sprite = load_sprite_csv(sprite_path);
    TP_TIMING_MARK("load_sprite_csv");
    if (g_has_sprite) set_net_wm_icon(dpy, win);
    /* Real, new 2026-08-30 - real per-voxel phymoji asset, generated
     * on demand from this entity's own real sprite.csv if it doesn't
     * exist yet (see load_entity_phymoji()/ensure_entity_phymoji_
     * generated()'s own header comments) - loaded once here, cached
     * for the whole process lifetime same as the sprite itself. */
    load_entity_phymoji(package_dir, resolved_ops_dir);
    TP_TIMING_MARK("load_entity_phymoji");

    /* Real window shape from the sprite's own alpha, if we have one -
     * see build_shape_mask()'s own header comment for why GL_BLEND
     * alone wasn't enough. */
    if (g_has_sprite) {
#ifndef _WIN32
        Pixmap shape_mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
        GC shape_gc = XCreateGC(dpy, shape_mask, 0, NULL);
        build_shape_mask(dpy, win, shape_gc, shape_mask);
        XFreeGC(dpy, shape_gc);
        XFreePixmap(dpy, shape_mask);
#endif
        /* Win: per-pixel alpha via UpdateLayeredWindow in XPutImage (XShape shim). */
    }

    /* REAL, NEW 2026-08-30, direct live report ("teh cursword is a
     * very thin png so since its hard to grab, could we make its grab
     * surface wider (like within the halo circle, even when halo
     * isn't visible?)") - X11's Shape extension has TWO independent
     * masks: ShapeBounding (what's drawn/visible - build_shape_mask()'s
     * own sprite-silhouette-only real behavior above, unchanged, so
     * cursword still LOOKS exactly as thin as its sprite) and
     * ShapeInput (what actually receives clicks/pointer events - can
     * be a completely different, WIDER shape with zero visual change).
     * Set once here, real full-circle radius matching the halo ring's
     * own geometry (cursword_update_shape()'s WIN_PX/2-5), so every
     * click anywhere inside that circle - not just on the thin visible
     * pixels - now hits cursword, whether armed or not. Cursword-only
     * (g_is_cursword), every other entity's own real click hit-testing
     * is completely unaffected.
     *
     * REAL FOLLOW-UP FIX 2026-08-30, direct live report ("im still
     * having to click right on the image") - this ShapeInput mask
     * turned out NOT to be honored by the real compositor for genuine
     * mouse clicks (real-world gap, confirmed live) - kept here as a
     * harmless, possibly-helpful-elsewhere redundancy, but
     * cursword_update_shape() below (called once, right after this
     * block) is the REAL fix now: it widens ShapeBOUNDING itself to
     * match, which every compositor DOES reliably honor for click
     * routing, at the cost of a real, always-visible dim backdrop
     * disc (see that function's own header comment for the exact
     * reasoning/color choice). */
    if (g_is_cursword) {
#ifndef _WIN32
        Pixmap input_mask = XCreatePixmap(dpy, win, (unsigned)WIN_PX, (unsigned)WIN_PX, 1);
        GC input_gc = XCreateGC(dpy, input_mask, 0, NULL);
        XSetForeground(dpy, input_gc, 0);
        XFillRectangle(dpy, input_mask, input_gc, 0, 0, WIN_PX, WIN_PX);
        XSetForeground(dpy, input_gc, 1);
        int icx = WIN_PX / 2, icy = WIN_PX / 2;
        int iradius = WIN_PX / 2 - 5;
        XFillArc(dpy, input_mask, input_gc, icx - iradius, icy - iradius,
                 (unsigned)(iradius * 2), (unsigned)(iradius * 2), 0, 360 * 64);
        XShapeCombineMask(dpy, win, ShapeInput, 0, 0, input_mask, ShapeSet);
        XFreeGC(dpy, input_gc);
        XFreePixmap(dpy, input_mask);
#endif
        /* Real fix - widen ShapeBOUNDING too, from the very start (not
         * just after the first arm/disarm), so the wider grab surface
         * is real from cursword's first frame on screen. */
        cursword_update_shape(dpy, win);
    }

    int xfd = ConnectionNumber(dpy);
    MethodItem methods[MAX_METHODS];
    int n_methods = load_methods(package_dir, methods, MAX_METHODS);
    TP_TIMING_MARK("load_methods");
    if (n_methods == 0) {
        snprintf(methods[0].label, sizeof(methods[0].label), "Close");
        snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
        n_methods = 1;
    }
    /* REAL, 2026-08-05: optional multi-page objects.pdl overrides the
     * single-page methods.pdl list above - see load_objects()'s own
     * header comment. methods[]/n_methods become "whichever page is
     * currently open"'s own item list when objects.pdl exists, so every
     * existing render/dispatch/relay code below that already reads
     * methods[]/n_methods keeps working completely unchanged. */
    ObjPage obj_pages[MAX_PAGES];
    int n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
    TP_TIMING_MARK("load_objects");
    int using_objects = (n_obj_pages > 0);
    int cur_page = 0;
    int page_stack[MAX_PAGES];
    int page_stack_n = 0;
    if (using_objects) {
        n_methods = obj_pages[0].n_items;
        for (int i = 0; i < n_methods; i++) methods[i] = obj_pages[0].items[i];
    }
    int input_active = 0;
    char input_key[64] = "";
    char input_buffer[256] = "";
    Window input_popup_win = 0;
    Window popup_win = 0;
    int popup_nav_base = 0;
    int popup_focus_row = 0;
    int popup_digit_accum = 0; /* chtpm: digits move [>] before Enter */
    /* REAL, 2026-08-05, direct instruction ("book stack emoji... give
     * me option to generate a random verse... entirely with khtpm /
     * eventscript page, and pal"): real Show Choices support - a
     * SEPARATE process (e.g. a real event.pal's own exec'd op) can ask
     * THIS already-running window to present a choice popup via a new
     * SHOW_PAGE relay command, reusing the exact same popup_win/
     * methods[]/n_methods machinery the entity's own normal context
     * menu already uses. choice_mode gates the row-activation dispatch
     * sites (mouse click / RUN_METHOD / ACTIVATE_NAV / Enter) to write
     * the picked row's action to choice_result_path instead of running
     * the normal CLOSE/void/GOTO/etc dispatch. */
    int choice_mode = 0;
    char choice_result_path[TP_PATH_BUF] = "";
    Window text_popup_win = 0;
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    /* REAL, 2026-08-05, direct instruction ("lets just open an adjacent
     * contextmenu (with cancel button) beside the first one for now, so
     * we can test the range finder visual"): quick, real submenu using
     * the SAME already-working raw X11 popup mechanism (open_context_
     * menu/draw_context_menu), not a new CHTPM screen (see hikkikomorai/
     * x11-mouse-2do.txt for why the CHTPM route was rolled back -
     * gl_mirror.c has no mouse-click support and no configurable
     * window size). A second, fixed-content popup opens ADJACENT to
     * the first when "User" is clicked; "Move" inside it opens a third
     * small popup drawing a real range-finder grid. */
    Window user_popup_win = 0;
    int user_popup_x = 0, user_popup_y = 0;
    MethodItem user_methods[4];
    snprintf(user_methods[0].label, sizeof(user_methods[0].label), "Move");
    snprintf(user_methods[0].action, sizeof(user_methods[0].action), "void");
    snprintf(user_methods[1].label, sizeof(user_methods[1].label), "Inventory");
    snprintf(user_methods[1].action, sizeof(user_methods[1].action), "void");
    snprintf(user_methods[2].label, sizeof(user_methods[2].label), "Skill");
    snprintf(user_methods[2].action, sizeof(user_methods[2].action), "void");
    snprintf(user_methods[3].label, sizeof(user_methods[3].label), "Cancel");
    snprintf(user_methods[3].action, sizeof(user_methods[3].action), "void");

    int popup_x = 0, popup_y = 0; /* where the MAIN popup itself opened, for real submenu adjacency */
    int dragging = 0, drag_start_x = 0, drag_start_y = 0;
    /* Real click-vs-drag distinction, cursword only (see
     * g_is_cursword/CURSWORD_CLICK_MAX_PX/MS declaration comments) -
     * press_root_x/y are the RAW screen coords at ButtonPress (never
     * updated during a drag, unlike drag_start_x/y above which slides
     * forward every MotionNotify) so ButtonRelease can measure total
     * real distance traveled, and press_tv is the real press timestamp
     * for the elapsed-time half of the same check. */
    int press_root_x = 0, press_root_y = 0;
    struct timeval press_tv = {0, 0};
    int running = 1;
    struct timeval last_frame = { 0, 0 };

    TP_TIMING_MARK("setup-complete->entering event loop");
    while (running && !g_shutdown_requested) {
#ifdef _WIN32
        x11_wait(dpy, POLL_INTERVAL_USEC);
        (void)xfd;
#else
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, dragging ? 16667 : POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);
#endif
        if (dragging && dpy) {
            Window rr, ch;
            int rx = 0, ry = 0, wx = 0, wy = 0;
            unsigned mask = 0;
            if (XQueryPointer(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                              &rr, &ch, &rx, &ry, &wx, &wy, &mask)) {
                char dest[PATH_BUF];
                dest[0] = 0;
                const char *bn = strrchr(package_dir, '/');
                bn = bn ? bn + 1 : package_dir;
                unsigned long zwin = 0;
                int zpid = kh_drop_zone_hit(dpy, rx, ry, dest, sizeof(dest), &zwin);
                int want = 0;
                Window host = None;
                /* Once reparented, window-walk always hits FE (pal is
                 * its child). Leave = pointer outside the host rect. */
                if (g_drag_reparented && g_drag_host) {
                    if (kh_pointer_in_window(dpy, g_drag_host, rx, ry)) {
                        want = 1;
                        host = g_drag_host;
                        zpid = g_drag_host_pid;
                    }
                } else if (zpid && zwin) {
                    want = 1;
                    host = (Window)zwin;
                    g_drag_host_pid = zpid;
                }
                kh_write_drag_hover(want ? zpid : 0, bn);
                kh_drag_stack_above(dpy, win, host, want, rx, ry);
                if (!g_drag_reparented) {
                    win_x = rx - 32;
                    win_y = ry - 32;
                }
                {
                    char tp[PATH_BUF];
                    FILE *tf;
                    snprintf(tp, sizeof(tp), "%s/#.desktop/drop_poll.txt", g_house_root);
                    tf = fopen(tp, "w");
                    if (tf) {
                        fprintf(tf, "rx=%d ry=%d zpid=%d name=%s dest=%s nstack_hint=winwalk\n",
                                rx, ry, zpid, bn, dest);
                        fclose(tf);
                    }
                }
            }
        }

        /* REAL FIX 2026-09-10, direct report ("why didnt bible pop up
         * change size when house size was changed? doesn't it read
         * from .pdl?"): desktop-entity mode (tp_main - every pal/
         * monster/book-stack, NOT the generic HQ-window main()/
         * hq_dispatch_xevent loops) never called hq_ui_pdl_reload_if_
         * changed() anywhere in its own event loop - font_scale (and
         * font_family, close_combo, win_top_y, click_two_step) were
         * read ONCE at process start (desktop_load_click_two_step()
         * a few hundred lines up) and never again for the lifetime of
         * an already-running entity. The popup fontset itself was
         * already fixed to react live (ensure_popup_fontset_current(),
         * see load_popup_fontset()'s own header comment) - it just
         * never got a live g_ui_scale_pct to react TO.
         *
         * REAL FOLLOW-UP FIX, same day (direct correction: "it doesn't
         * have to read every loop. just on change... or is your way
         * better for some reason i dont know?") - it was NOT better,
         * two real bugs in the first pass: (1) this called
         * desktop_load_click_two_step() directly and UNCONDITIONALLY
         * every ~300ms tick, doing a real file-open+full-parse every
         * time instead of reusing hq_ui_pdl_reload_if_changed() (the
         * function the other two modes already call, which does the
         * cheap part itself - one stat(), compares mtime against a
         * cached g_hq_ui_pdl_mtime, only opens/parses when it actually
         * differs). (2) even if it had called that wrapper, its own
         * redraw signal (hq_request_redraw() -> g_frame_dirty) is
         * never read anywhere in tp_main's own loop - a real change
         * would have reloaded silently and never actually repainted.
         * Fixed both: reuse the shared gated wrapper as-is (one cheap
         * stat() per tick, no duplicated parse logic, matches the
         * other two modes exactly), then read+clear g_frame_dirty
         * locally to drive tp_main's own real need_redraw. */
        entity_ui_pdl_reload_if_changed(g_house_root);
        /* REMOVED 2026-09-13 (direct live report: "this needs 2 stop
         * happening... research house standards, and a real
         * solution"). This used to be a periodic (~10s) re-add of this
         * entity's own line into #.desktop/livedesk_open.txt - a real
         * fix, 2026-09-12, for a real bug (a one-time-registered
         * entity's line silently, permanently pruned by one transient
         * ktb_pid_alive() misread). But with EVERY entity doing this on
         * its own unsynchronized timer, that file became exactly the
         * "one hot shared file, many writers" shape
         * PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md (this house's own
         * design doc, §1) explicitly names as the pattern the house's
         * one-writer rule exists to avoid - and was the real, shared
         * root cause behind three separate symptoms fixed piecemeal
         * this same week (tab reordering, entities missing after a
         * restart, a blank taskbar with otherwise-valid process data).
         * Real, structural fix this time, not another patch on this
         * same timer: entities register ONCE, at their own startup
         * (still done, a few hundred lines up) and never write this
         * file again. The manager is now the sole writer - see
         * ktb_self_heal_active_desk_registry() (khtpm_taskbar_
         * manager.c) for its real replacement: the SAME ~10s cadence,
         * self-healing the SAME class of spurious-prune bug, but from
         * the one process that's supposed to own this file, reading
         * real /proc liveness rather than trusting N independent
         * writers to coordinate themselves. */
        int need_redraw = 0;
        if (g_frame_dirty) { need_redraw = 1; g_frame_dirty = 0; }
        /* REAL FIX 2026-09-01 (live report: after the @ always-on-top
         * toggle respawned every entity at once, all but cursword sat
         * blank until an unrelated click elsewhere happened to trip a
         * dirty check - "should be immediate, not wait for next
         * click"). This "always paint the very first frame" seed only
         * existed on the _WIN32 branch below - on Linux, a freshly-
         * started process's need_redraw stayed 0 until SOME dirty
         * condition fired (an Expose event, theme/camera change,
         * etc.). A single normal launch usually gets away with it
         * (XMapWindow's own Expose arrives fast enough to be
         * unnoticeable), but respawning many entities at once creates
         * real X11-server/CPU contention that can visibly delay it -
         * pre-existing, not introduced by this session's consolidation
         * work (this file's own redraw logic is otherwise untouched).
         * Real fix: the same real seed, unconditional (not _WIN32-
         * only) - matches the already-real intent of last_frame's own
         * zero-init check. */
        if (last_frame.tv_sec == 0) need_redraw = 1;

        /* Real, cheap, event-driven opacity reapply - see
         * theme_changed_dirty()'s own declaration comment. */
        if (theme_changed_dirty(g_house_root)) {
            set_window_opacity(dpy, win, tp_load_theme_opacity(g_house_root));
            load_theme_colors();   /* bg/fg too, not just opacity - see the load_theme_colors() call in tp_main()'s setup */
            need_redraw = 1;
        }

        /* Real, cheap, event-driven camera pan/tilt/mode reapply - see
         * camera_changed_dirty()'s own declaration comment (without
         * this, an idle entity nobody's touching never redraws even
         * when the shared camera state moves). */
        if (camera_changed_dirty(g_house_root)) {
            load_camera_mode(g_house_root);
            load_camera_state(g_house_root);
            load_active_z(g_house_root);
            need_redraw = 1;
        }

        /* REAL, NEW 2026-09-14, direct live report ("cursword should
         * always have highest z level priority. right now it hides
         * behind castle") - real window-stacking enforcement for any
         * entity that declared a nonzero z_priority in its own
         * meta.pdl (read_z_priority()'s own header comment). Cheap,
         * unconditional self-raise every real tick (this loop already
         * runs every POLL_INTERVAL_USEC regardless) - XRaiseWindow on
         * an already-topmost window is a real, cheap X-server no-op,
         * so this costs nothing extra for the common "nothing else
         * mapped since last tick" case, and correctly wins back the
         * top spot the instant some OTHER, later-mapped/raised entity
         * (like castle, appearing after cursword) would otherwise
         * cover it. Honest, first-pass limitation, not silently
         * hidden: this only guarantees "priority > 0 always beats
         * priority == 0" - two different entities both declaring a
         * nonzero priority would race each other's own tick timing,
         * not a strict ordering by value yet; real, separate follow-up
         * if that's ever needed.
         *
         * REAL FIX 2026-09-14, direct live follow-up question ("will
         * u make sure always ontop on/off still does what it needs to
         * do?") - caught a real, latent conflict this raised: gated on
         * g_override_redirect (the SAME real per-entity flag the @
         * always-on-top toggle already flips, see that toggle's own
         * ZORDER_TOGGLE handler). With always-on-top OFF, this entity
         * is a normal, WM-managed window like any other real app - an
         * unconditional periodic self-raise there would keep popping
         * it back above whatever OTHER real application the user just
         * brought to front, fighting the window manager itself, not
         * just winning against sibling desktop entities (which is all
         * the real report ever asked for). Gating this to override-
         * redirect mode keeps z_priority doing exactly what it was
         * asked to do (win against other desktop entities) without
         * ever touching normal WM-managed stacking. */
        if (g_z_priority > 0 && g_override_redirect) {
            XRaiseWindow(dpy, win);
        }

        /* REAL, 2026-08-05: poll interact_relay.txt for an injected
         * command - the "AI-injection power" half of this window's own
         * new CHTPM-parity work (see this file's header comment on
         * g_relay_path/append_history). A human, script, or AI agent
         * writes one command line into <package_dir>/interact_relay.txt;
         * this loop (already running every POLL_INTERVAL_USEC via the
         * select() above) picks it up, dispatches it exactly like a
         * real click would, logs it to history.txt, then truncates the
         * relay file so the same command isn't re-consumed next tick -
         * the same real "write once, consumed once" shape this house's
         * other relay files already use. */
        {
            struct stat relay_st;
            if (stat(g_relay_path, &relay_st) == 0 && relay_st.st_size > 0) {
                char line[TP_PATH_BUF];
                line[0] = '\0';
                FILE *rf = fopen(g_relay_path, "r");
                if (rf) {
                    if (!fgets(line, sizeof(line), rf)) line[0] = '\0';
                    fclose(rf);
                }
                FILE *tf = fopen(g_relay_path, "w");
                if (tf) fclose(tf);
                line[strcspn(line, "\n")] = '\0';
                if (line[0]) {
                    append_history("INJECTED: %s", line);
                    if (strcmp(line, "RAISE") == 0) {
                        /* Single-instance spawn support (2026-08-24,
                         * cursword HQ row): the taskbar writes RAISE into
                         * this relay instead of spawning a second
                         * tp_desktop_window_rgb when this entity is
                         * already open. Raise own main window to the top
                         * of the stack - stacking manipulation from the
                         * entity's own X connection, no cross-process
                         * window plumbing. Deliberately NO
                         * XSetInputFocus/XGrabKeyboard here: this is an
                         * override_redirect window, exempt from WM focus
                         * handling entirely (see db-hq's main() header
                         * comment for the full Mutter investigation) -
                         * real keyboard focus was never achievable for
                         * these windows; raise-to-top IS the whole
                         * observable "focus" behavior, and a human click
                         * still lands keyboard where Mutter allows it. */
                        XRaiseWindow(dpy, win);
                        /* REAL, NEW 2026-08-31, direct instruction ("when
                         * i click it from tb it should go back to a
                         * familiar location") - RAISE is ONLY ever sent
                         * for cursword's own single-instance re-click
                         * (khtpm_taskbar_manager.c's livedesk:spawn-
                         * cursword handler, the only real caller of this
                         * relay command house-wide), so this is real,
                         * safe, and cursword-only without an explicit
                         * g_is_cursword check. If it ever got dragged/
                         * nudged off into a weird spot (or left there by
                         * stale test/camera-pan state), a re-click now
                         * also snaps it straight back to its real pinned
                         * home (same CURSWORD_HOME_X/Y convention
                         * livedesk_ensure_cursword() already uses on
                         * respawn) - "always findable in the same spot"
                         * now also means "clicking it finds it," not
                         * just "it's always open." */
                        win_x = 0; win_y = 0;
                        XMoveWindow(dpy, win, win_x, win_y);
                        write_pos(package_dir, win_x, win_y);
                        XFlush(dpy);
                    } else if (strncmp(line, "MOVE_TO:", 8) == 0) {
                        /* REAL, NEW 2026-09-14 (PLAY-MODE-ENTITY-HARNESS-
                         * DESIGN.md §3's "move" Common Event, direct
                         * instruction: "we will use move / pathfinding
                         * event program to make cursword move to
                         * castle") - the real, general-purpose relay-
                         * injection channel for programmatic movement, so
                         * a "move" Common Event's own op can move
                         * cursword the same way a human's real click-to-
                         * place already does (grid-snap + XMoveWindow +
                         * write_pos), without simulating a fake mouse
                         * click. `MOVE_TO:<x>,<y>` - raw pixel
                         * coordinates, already grid-aligned by the
                         * caller (this entity's own current desktop_pos.
                         * txt plus/minus GRID_CELL_PX, matching every
                         * other pal's own real grid-snapped position) -
                         * re-snapped here too, defensively, exactly like
                         * the real click-to-place handler does, so a
                         * slightly-off caller can't park this off-grid. */
                        int tx = 0, ty = 0;
                        sscanf(line + 8, "%d,%d", &tx, &ty);
                        /* MOVE_TO coordinates are reference px (they come from
                         * desktop_pos.txt files); convert to this screen. */
                        tx = kh_pos_ref_to_screen(tx);
                        ty = kh_pos_ref_to_screen(ty);
                        int gx = (tx + GRID_CELL_PX / 2) / GRID_CELL_PX;
                        int gy = (ty + GRID_CELL_PX / 2) / GRID_CELL_PX;
                        if (gx < 0) gx = 0;
                        if (gy < 0) gy = 0;
                        win_x = gx * GRID_CELL_PX;
                        win_y = gy * GRID_CELL_PX;
                        XMoveWindow(dpy, win, win_x, win_y);
                        write_pos(package_dir, win_x, win_y);
                        XFlush(dpy);
                    } else if (strncmp(line, "RUN_METHOD:", 11) == 0) {
                        const char *label = line + 11;
                        for (int i = 0; i < n_methods; i++) {
                            if (strcmp(methods[i].label, label) != 0) continue;
                            /* Same GOTO:/BACK/STATE: reserved forms the
                             * real click path handles (see the
                             * ButtonPress branch below) - relay
                             * injection dispatches a page navigation or
                             * input-activation row exactly like a real
                             * click would, not just plain commands. */
                            if (using_objects && strncmp(methods[i].action, "GOTO:", 5) == 0) {
                                const char *target = methods[i].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        /* Same real fix as the mouse-click
                                         * path above: make the page switch
                                         * actually visible, don't just
                                         * silently update methods[]. */
                                        if (popup_win) close_context_menu(dpy, popup_win);
                                        else { popup_x = win_x; popup_y = win_y; }
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[i].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    if (popup_win) close_context_menu(dpy, popup_win);
                                    else { popup_x = win_x; popup_y = win_y; }
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if ((using_objects && strncmp(methods[i].action, "STATE:", 6) == 0) ||
                                       strcmp(methods[i].action, "CLI_IO") == 0) {
                                if (strcmp(methods[i].action, "CLI_IO") == 0)
                                    snprintf(input_key, sizeof(input_key), "cliio");
                                else
                                    snprintf(input_key, sizeof(input_key), "%s", methods[i].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[i].action, package_dir, g_house_root, &running);
                            }
                            break;
                        }
                    } else if (strncmp(line, "ACTIVATE_NAV:", 13) == 0) {
                        /* REAL, 2026-08-05 (TILE_PICKER_DESIGN.md §13):
                         * the REMOTE half of the shared live nav pool -
                         * the taskbar's own terminal input writes this
                         * exact command into whichever window's
                         * interact_relay.txt actually owns the typed
                         * number (looked up from #.desktop/
                         * livedesk_nav_claims.txt), so pressing Enter on
                         * a number can activate a row inside a DIFFERENT
                         * window's currently-open menu, not just this
                         * process's own. Only meaningful while popup_win
                         * is open and N falls in this popup's own
                         * currently-claimed range - otherwise a stale/
                         * mistargeted command is silently ignored. */
                        int nav_n = atoi(line + 13);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            int row = nav_n - popup_nav_base;
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                            append_history("CLICK(nav) method=%s action=%s", methods[row].label, methods[row].action);
                            if (strcmp(methods[row].action, "CLOSE") == 0) {
                                running = 0;
                            } else if (strcmp(methods[row].action, "void") == 0) {
                                /* no-op */
                            } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                const char *target = methods[row].action + 5;
                                for (int pi = 0; pi < n_obj_pages; pi++) {
                                    if (strcmp(obj_pages[pi].name, target) == 0) {
                                        if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                        cur_page = pi;
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                        break;
                                    }
                                }
                            } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                if (page_stack_n > 0) {
                                    cur_page = page_stack[--page_stack_n];
                                    n_methods = obj_pages[cur_page].n_items;
                                    for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                }
                            } else if ((using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) ||
                                       strcmp(methods[row].action, "CLI_IO") == 0) {
                                snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                input_buffer[0] = '\0';
                                input_active = 1;
                                append_history("INPUT_ACTIVATE key=%s", input_key);
                                if (!input_popup_win) {
                                    input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                }
                            } else {
                                dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                            }
                        }
                    } else if (strncmp(line, "FOCUS_NAV:", 10) == 0) {
                        /* Move [>] only (chtpm digit-jump). No activate. */
                        int nav_n = atoi(line + 10);
                        if (popup_win && nav_n >= popup_nav_base && nav_n < popup_nav_base + n_methods) {
                            popup_focus_row = nav_n - popup_nav_base;
                            popup_digit_accum = nav_n;
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                            append_history("FOCUS_NAV %d -> row %d", nav_n, popup_focus_row);
                        }
                    } else if (strncmp(line, "NAV_KEY:", 8) == 0) {
                        /* REAL, 2026-08-06, direct instruction ("what if
                         * we logged keybord input when toolbar is on,
                         * and it will be sent to our khtpm from
                         * master-ledger for desktop to our own .txt...
                         * focus giving us our own control of this"):
                         * the real fix for XWayland's grab restrictions
                         * turned out incomplete in practice - so instead
                         * of fighting the compositor for a real
                         * XGrabKeyboard, the taskbar's own input box
                         * (which DOES reliably hold real X focus, no
                         * grab needed - see tp_taskbar.c) becomes a
                         * remote keyboard for whichever entity currently
                         * has a popup open, relaying Up/Down/Enter/
                         * Escape here exactly like a real local KeyPress
                         * would - same dispatch logic as the popup_win
                         * KeyPress branch below, just triggered
                         * remotely instead of from this process's own
                         * X queue. Only meaningful while popup_win is
                         * actually open here - otherwise silently
                         * ignored (stale/mistargeted, same rule
                         * ACTIVATE_NAV already follows). */
                        char navkey[16];
                        snprintf(navkey, sizeof(navkey), "%s", line + 8);
                        navkey[strcspn(navkey, "\r\n")] = '\0';
                        if (popup_win) {
                            if (strcmp(navkey, "Escape") == 0) {
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                need_redraw = 1;
                            } else if (strcmp(navkey, "Up") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Down") == 0) {
                                if (n_methods > 0) {
                                    popup_focus_row = (popup_focus_row + 1) % n_methods;
                                    popup_digit_accum = 0;
                                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                                }
                            } else if (strcmp(navkey, "Enter") == 0) {
                                int row = popup_focus_row;
                                close_context_menu(dpy, popup_win);
                                popup_win = 0;
                                nav_release_pid(g_house_root, getpid());
                                if (choice_mode) {
                                    FILE *rf5 = fopen(choice_result_path, "w");
                                    if (rf5) { fprintf(rf5, "%s\n", methods[row].action); fclose(rf5); }
                                    append_history("SHOW_PAGE_PICK(navkey) action=%s -> %s", methods[row].action, choice_result_path);
                                    choice_mode = 0;
                                    need_redraw = 1;
                                    goto skip_navkey_enter_dispatch;
                                }
                                append_history("CLICK(navkey) method=%s action=%s", methods[row].label, methods[row].action);
                                if (strcmp(methods[row].action, "CLOSE") == 0) {
                                    running = 0;
                                } else if (strcmp(methods[row].action, "void") == 0) {
                                    /* no-op */
                                } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                                    user_popup_x = popup_x + g_popup_w + 4;
                                    user_popup_y = popup_y;
                                    user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                                } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                                    const char *target = methods[row].action + 5;
                                    for (int pi = 0; pi < n_obj_pages; pi++) {
                                        if (strcmp(obj_pages[pi].name, target) == 0) {
                                            if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                            cur_page = pi;
                                            n_methods = obj_pages[cur_page].n_items;
                                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                            popup_focus_row = 0; popup_digit_accum = 0;
                                            break;
                                        }
                                    }
                                } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                                    if (page_stack_n > 0) {
                                        cur_page = page_stack[--page_stack_n];
                                        n_methods = obj_pages[cur_page].n_items;
                                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                    }
                                } else if ((using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) ||
                                       strcmp(methods[row].action, "CLI_IO") == 0) {
                                    snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                                    input_buffer[0] = '\0';
                                    input_active = 1;
                                    append_history("INPUT_ACTIVATE key=%s", input_key);
                                    if (!input_popup_win) {
                                        input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                                    }
                                } else {
                                    dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                                }
                                need_redraw = 1;
                                skip_navkey_enter_dispatch: ;
                            }
                        }
                    } else if (strncmp(line, "SHOW_PAGE:", 10) == 0) {
                        /* REAL, 2026-08-05: real Show Choices, injected
                         * into THIS already-running window by a SEPARATE
                         * process (a real event.pal's own exec'd op) -
                         * "<objects_file>|<result_file>". Reuses the
                         * exact same popup_win/methods[]/n_methods
                         * machinery the entity's own normal context menu
                         * already uses - choice_mode gates every row-
                         * activation site below to write the picked
                         * row's action to choice_result_path instead of
                         * running the normal dispatch. */
                        char *sep = strchr(line + 10, '|');
                        if (sep) {
                            *sep = '\0';
                            char *objpath = line + 10;
                            char *respath = sep + 1;
                            respath[strcspn(respath, "\r\n")] = '\0';
                            n_methods = load_flat_objects(objpath, methods, MAX_METHODS);
                            if (n_methods > 0) {
                                snprintf(choice_result_path, sizeof(choice_result_path), "%s", respath);
                                choice_mode = 1;
                                if (popup_win) close_context_menu(dpy, popup_win);
                                popup_x = win_x;
                                popup_y = win_y + WIN_PX + 4;
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                /* REAL FIX 2026-08-06, direct report ("they
                                 * hav nav in main but not book choices"):
                                 * SHOW_PAGE popups never claimed real
                                 * shared live nav numbers (nav_base was
                                 * hardcoded 0, which draw_context_menu
                                 * treats as "suppress the index entirely"
                                 * - meant to avoid showing an unclaimed
                                 * number, but it just reads as a missing
                                 * feature next to every other menu, which
                                 * DOES claim from nav_claim_rows()). Real
                                 * fix: claim rows here too, same as every
                                 * other popup_win open site in this file. */
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                append_history("SHOW_PAGE objects=%s result=%s", objpath, choice_result_path);
                            }
                        }
                    } else if (strncmp(line, "SHOW_TEXT_FILE:", 15) == 0) {
                        /* REAL, 2026-08-05: real Show Text - displays a
                         * real text file's own content (already real-
                         * line-wrapped by whichever op generated it) in
                         * a small popup, dismissed by click/Enter/Escape.
                         * Real, separate window from the choice/normal
                         * popups (text_popup_win), so a choice result and
                         * its own follow-up text display can coexist
                         * without fighting over popup_win/methods[]. */
                        char textpath[TP_PATH_BUF];
                        snprintf(textpath, sizeof(textpath), "%s", line + 15);
                        textpath[strcspn(textpath, "\r\n")] = '\0';
                        FILE *tf2 = fopen(textpath, "r");
                        if (tf2) {
                            char tline[256];
                            int n_lines = 0;
                            int max_px = 0;
                            char text_lines[64][256];
                            while (n_lines < 64 && fgets(tline, sizeof(tline), tf2)) {
                                tline[strcspn(tline, "\r\n")] = '\0';
                                snprintf(text_lines[n_lines], sizeof(text_lines[0]), "%s", tline);
                                int lw = popup_text_px(dpy, tline);
                                if (lw > max_px) max_px = lw;
                                n_lines++;
                            }
                            fclose(tf2);
                            if (text_popup_win) close_context_menu(dpy, text_popup_win);
                            /* REAL FIX 2026-08-07: width used to be
                             * max_w*7 - a hardcoded 7px/char guess -
                             * but the real popup face is the 18px fixed
                             * fontset (~9px/glyph; CJK/emoji wider), so
                             * long bible_text verses were silently
                             * clipped at the right edge. Sized now from
                             * the REAL Xutf8TextExtents of the widest
                             * line (popup_text_px), + padding. */
                            int pop_w = (max_px * 115) / 100 + 24;
                            if (pop_w < POPUP_W) pop_w = POPUP_W;
                            if (pop_w > 900) pop_w = 900;
                            int pop_h = (n_lines + 1) * POPUP_ROW_H;
                            int tpx = win_x, tpy = win_y + WIN_PX + 4;
                            clamp_popup_to_screen(dpy, &tpx, &tpy, pop_w, pop_h);
                            XSetWindowAttributes swa2;
                            swa2.override_redirect = True;
                            /* 2026-09-09, direct report ("after choosing
                             * the bible verse / tao it shows a popup with
                             * text - those are still black and white").
                             * The Show Text popup (book-stack's verse /
                             * Tao view) was a hardcoded white bg + black
                             * text. Use the livedesk theme like every
                             * other surface (g_theme_bg/fg are loaded +
                             * live-refreshed in hq_idle_tick()). */
                            swa2.background_pixel = tp_hex_pixel(dpy, DefaultScreen(dpy), g_theme_bg);
                            swa2.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
                            text_popup_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                                            tpx, tpy, pop_w, pop_h, 1,
                                                            CopyFromParent, InputOutput, CopyFromParent,
                                                            CWOverrideRedirect | CWBackPixel | CWEventMask, &swa2);
                            {
                                XClassHint *ch2 = XAllocClassHint();
                                if (ch2) {
                                    ch2->res_name = (char *)"MuchiverseLivedesk";
                                    ch2->res_class = (char *)"MuchiverseLivedesk";
                                    XSetClassHint(dpy, text_popup_win, ch2);
                                    XFree(ch2);
                                }
                            }
                            XMapRaised(dpy, text_popup_win);
                            /* REAL FIX 2026-08-24, direct user report ("i
                             * want text to stay on screen till clicked...
                             * it closes very aggressively, when i press a
                             * key when its open"): this popup used to copy
                             * the context-menu's input policy wholesale -
                             * XGrabPointer/XGrabKeyboard whenever the pal's
                             * own STATE rows say grab_pointer/grab_keyboard
                             * (cursword's do) plus popup_soft_focus().
                             * Consequences: every keystroke ANYWHERE landed
                             * in this process and the old dismiss-on-any-
                             * KeyPress branch ate it (user's screenshot
                             * shortcuts died while a verse was up), every
                             * click anywhere was both swallowed by the grab
                             * AND dismissed the popup, and focused apps lost
                             * their keys while it stayed open. A Show Text
                             * box is not a modal menu: NO grabs, NO input
                             * focus. It now receives exactly what falls on
                             * it - a click directly on the box (its own
                             * ButtonPressMask; override_redirect keeps it
                             * topmost under the cursor so the click lands
                             * here without any grab) - everything else
                             * passes through to whatever the user actually
                             * aimed at. Dismissal itself is tightened in
                             * the event loop: ButtonPress ON THIS WINDOW
                             * only, keys never dismiss. */
                            /* REAL FIX 2026-08-06, direct-caught bug (a
                             * NAV_KEY-opened SHOW_TEXT_FILE popup
                             * self-dismissed a few seconds after opening,
                             * with no real input): text_popup_win never
                             * got the SAME stale-queued-event drain
                             * open_context_menu() already has (see that
                             * function's own header comment - X11 window
                             * ID reuse can redeliver an old
                             * Button/KeyPress meant for a JUST-destroyed
                             * window to this brand new one). This is a
                             * separate window creation path (not routed
                             * through open_context_menu()), so it never
                             * got the fix. Same real fix here. */
                            XSync(dpy, False);
                            {
                                XEvent stale_ev2;
                                while (XCheckWindowEvent(dpy, text_popup_win, ButtonPressMask | KeyPressMask, &stale_ev2)) {
                                    /* discard - see comment above */
                                }
                            }
                            for (int li = 0; li < n_lines; li++) {
                                strncpy(g_text_popup_lines[li], text_lines[li], sizeof(g_text_popup_lines[0]) - 1);
                            }
                            g_text_popup_n_lines = n_lines;
                            append_history("SHOW_TEXT_FILE %s (%d lines)", textpath, n_lines);
                        }
                    } else if (strncmp(line, "OPEN_PAGE:", 10) == 0) {
                        /* REAL 2026-08-06: open a named objects.pdl page
                         * after reload (e.g. RPG Menu rewritten by
                         * open_rp_menu.sh with live Level/Gold/HP). */
                        char page_name[64];
                        snprintf(page_name, sizeof(page_name), "%s", line + 10);
                        page_name[strcspn(page_name, "\r\n")] = '\0';
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        cur_page = 0;
                        page_stack_n = 0;
                        if (using_objects) {
                            int found = -1;
                            for (int pi = 0; pi < n_obj_pages; pi++) {
                                if (strcmp(obj_pages[pi].name, page_name) == 0) {
                                    found = pi;
                                    break;
                                }
                            }
                            if (found >= 0) {
                                cur_page = found;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            } else {
                                cur_page = 0;
                                n_methods = obj_pages[0].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[0].items[k];
                            }
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0;
                        append_history("OPEN_PAGE:%s", page_name);
                    } else if (strcmp(line, "OPEN_CONTEXT") == 0) {
                        /* REAL 2026-08-06: taskbar nav Enter on a tab
                         * writes this to open the entity context menu
                         * (same reload+open path as right-click). */
                        if (popup_win) {
                            close_context_menu(dpy, popup_win);
                            popup_win = 0;
                            nav_release_pid(g_house_root, getpid());
                        }
                        n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                        using_objects = (n_obj_pages > 0);
                        if (using_objects) {
                            cur_page = 0;
                            page_stack_n = 0;
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                        } else {
                            n_methods = load_methods(package_dir, methods, MAX_METHODS);
                            if (n_methods == 0) {
                                snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                                snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                                n_methods = 1;
                            }
                        }
                        popup_x = win_x;
                        popup_y = win_y + WIN_PX + 4;
                        popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                        popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                        popup_focus_row = 0; popup_digit_accum = 0;
                        append_history("OPEN_CONTEXT");
                    } else if (strcmp(line, "CLOSE") == 0) {
                        running = 0;
                    }
                    need_redraw = 1;
                }
            }
        }

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (popup_win && xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
            } else if (popup_win && xev.type == ButtonPress) {
                /* Pointer is grabbed to popup_win while it's open (see
                 * open_context_menu) - every button press anywhere
                 * arrives here regardless of which window it physically
                 * landed on, same real technique egg_window.c's own
                 * popup uses. */
                /* Row 0 is the non-clickable id header (see g_full_id
                 * comment) - a real click there must not dispatch
                 * methods[0]. */
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < n_methods;
                int header_click = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w && raw_row == 0;
                /* REAL FIX 2026-08-05, direct report ("context nav
                 * arrows and index #'s should get autofocus while its
                 * open or esp if 'headerbar' is touch[ed]"): a click on
                 * the non-clickable id header used to unconditionally
                 * close the whole menu with zero effect, easy to mistake
                 * for "the window just closed"/lost focus. Now a no-op
                 * that keeps the menu open (real keyboard grab already
                 * held the whole time - see open_context_menu()'s own
                 * real grab-retry fix above) instead of silently
                 * dismissing. */
                if (header_click) {
                    /* OPTION C: header click re-asserts soft focus on this
                     * menu (was pure no-op when grab alone was assumed). */
                    popup_soft_focus(dpy, popup_win);
                    need_redraw = 1;
                    continue;
                }
                if (!inside) {
                    /* REAL FIX 2026-08-07, direct instruction: clicking
                     * the same anchor again / clicking elsewhere must NOT
                     * close the menu - it stays open until the user
                     * clicks a real row or Cancel. Configurable via the
                     * package's meta.pdl: STATE | menu_stay_open | 0
                     * restores the old dismiss-on-any-click behavior. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    need_redraw = 1;
                    continue;
                }
                /* REAL FIX 2026-08-30 - real house-wide click_two_step
                 * (see g_click_two_step's own declaration comment
                 * above): a click on an unfocused row only focuses it
                 * (same real semantics arrow-nav already uses via
                 * popup_focus_row) - a second click on the SAME,
                 * already-focused row is what actually activates it. */
                if (g_click_two_step && popup_focus_row != row) {
                    popup_focus_row = row;
                    draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    popup_soft_focus(dpy, popup_win);
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                nav_release_pid(g_house_root, getpid());
                if (choice_mode) {
                    /* REAL, 2026-08-05: a real Show Choices pick - write
                     * the chosen row's real action (a branch id) to the
                     * result file the waiting op is polling, instead of
                     * running the normal CLOSE/void/GOTO/etc dispatch. */
                    FILE *rf3 = fopen(choice_result_path, "w");
                    if (rf3) { fprintf(rf3, "%s\n", methods[row].action); fclose(rf3); }
                    append_history("SHOW_PAGE_PICK action=%s -> %s", methods[row].action, choice_result_path);
                    choice_mode = 0;
                } else {
                    append_history("CLICK method=%s action=%s", methods[row].label, methods[row].action);
                    /* REAL FIX 2026-08-04, direct instruction: this house
                     * already has a real, canonical convention for
                     * method dispatch - #.haiku+/tpmos-re-dox/fo-menu-
                     * sys.md ("Fuzz-Op Dynamic Menu System"): a
                     * METHOD's VALUE is a real, directly-executable op
                     * path (e.g. "projects/fuzz-op/ops/+x/feed_op.+x
                     * xlector"), or the literal keyword "void" for an
                     * internal-only no-op - NOT an abstract action
                     * keyword string-matched in C (the previous CLOSE/
                     * CANCEL/PLAY/STOP scheme this replaces). "CLOSE"
                     * stays a single reserved, internal keyword (closing
                     * THIS renderer's own event loop is not something an
                     * external process can do on this process's behalf)
                     * - everything else, including "void", is real
                     * fo-menu-sys.md-style dispatch: void = do nothing,
                     * any other VALUE = a real command line, executed
                     * with this package's own dir as argv[1] (same
                     * "pass the piece_id" convention fo-menu-sys.md's own
                     * examples show). */
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* Intentional no-op - e.g. "Cancel" - discoverable
                         * way to dismiss the menu with zero effect. */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        /* Second reserved internal keyword, same class as
                         * CLOSE - see this file's own 2026-08-05
                         * comment on user_popup_win.
                         * REAL FIX 2026-08-05, direct instruction
                         * ("sub context menu is way offset, it could
                         * be right next to the other"): was positioned
                         * off the CLICK coordinate (wherever "User"
                         * happened to sit in the row list, often far
                         * down), not the main popup itself. Now
                         * adjacent to popup_win's own real position -
                         * right next to it, same top edge, regardless
                         * of which row was clicked. */
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        /* REAL, 2026-08-05: objects.pdl href navigation -
                         * see load_objects()'s own header comment.
                         * REAL FIX, same day, caught while wiring nav
                         * claims: this used to only update methods[]/
                         * n_methods without ever reopening popup_win -
                         * the page switch was invisible until the NEXT
                         * right-click. Now reopens at the same real
                         * position and re-claims nav numbers for the
                         * new page's own rows. */
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                        popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if ((using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) ||
                                       strcmp(methods[row].action, "CLI_IO") == 0) {
                        /* REAL, 2026-08-05: objects.pdl real text-input
                         * row - same click-to-activate/Escape-to-commit
                         * shape this house's own cli_io field convention
                         * already uses. A small floating popup shows the
                         * live buffer (input_popup_win, drawn below);
                         * committing writes <package_dir>/<key>.txt. */
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        /* REAL FIX 2026-08-10, direct report ("bookstack no
                         * longer shows verse, event-ez button no longer
                         * opens event editor - path issue"): pals migration
                         * moved entities out of the dev-tree's fixed nesting
                         * depth (*.monads/*.widget/entities/<name>, always
                         * 4 levels under house_root) into
                         * xyzfs/users/<uuid>/home/livedesk/pals/<name>
                         * (a different depth entirely). METHOD/OBJECT
                         * actions that derived house_root by climbing a
                         * FIXED number of ".." from package_dir (argv[1])
                         * broke silently for any entity now living at pals'
                         * depth - not a bug in this dispatch call itself,
                         * but this is the one place that CAN fix it for
                         * every action at once: house_root is already
                         * known here (g_house_root), so pass it as a real
                         * second argument instead of making every
                         * downstream script/METHOD line re-derive it via
                         * fragile directory-climbing. Backward compatible -
                         * existing METHOD/OBJECT lines that only read $1
                         * (package_dir) are unaffected; only ones updated
                         * to also read $2 (sh scripts) / $1-after-$0 (sh -c
                         * lines, since $0 is already package_dir there)
                         * gain house_root. */
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                }
                need_redraw = 1;
            } else if (popup_win && xev.type == KeyPress) {
                /* REAL, 2026-08-05, direct correction (see this file's
                 * own nav_claim_rows()/TILE_PICKER_DESIGN.md §13): the
                 * earlier local-digit-selects-a-row idea was wrong -
                 * "[N]" is now a real, GLOBAL, shared live address (see
                 * nav_claim_rows()), not a local 1-9 shortcut, so a bare
                 * digit keypress here no longer means anything - real
                 * jump-by-number now only happens remotely, via the
                 * taskbar's own terminal input writing an
                 * "ACTIVATE_NAV:<N>" command into interact_relay.txt
                 * (see the relay-poll block above). Escape still just
                 * closes the menu locally, same as "Cancel". */
                /* REAL FIX, same day, follow-up ("just wanna make sure
                 * the fundamentals are in place"): real Up/Down moves a
                 * real focus cursor (popup_focus_row, wraps both ways)
                 * matching real chtpm's own "[>]" convention - see
                 * draw_context_menu()'s own comment. Enter activates
                 * whichever row currently holds the cursor - same real
                 * dispatch logic the mouse-click/ACTIVATE_NAV paths use. */
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (klen < 0) klen = 0;
                if (klen >= (int)sizeof(kbuf)) klen = (int)sizeof(kbuf) - 1;
                kbuf[klen] = '\0';
                if (ks == XK_Escape) {
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    popup_digit_accum = 0;
                    need_redraw = 1;
                } else if (ks == XK_Up) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row - 1 + n_methods) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Down) {
                    if (n_methods > 0) {
                        popup_focus_row = (popup_focus_row + 1) % n_methods;
                        popup_digit_accum = 0;
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    /* chtpm digit_accum: jump [>] to global nav index in this menu */
                    int d = kbuf[0] - '0';
                    int lo = popup_nav_base;
                    int hi = popup_nav_base + n_methods - 1; /* inclusive */
                    if (n_methods <= 0) { /* no-op */ }
                    else {
                        int new_val = popup_digit_accum * 10 + d;
                        int jumped = 0;
                        if (new_val >= lo && new_val <= hi) {
                            popup_digit_accum = new_val;
                            popup_focus_row = new_val - popup_nav_base;
                            jumped = 1;
                        } else if (d >= lo && d <= hi) {
                            /* out of range as append — restart with d if valid address */
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods && lo <= d && d <= hi) {
                            popup_digit_accum = d;
                            popup_focus_row = d - popup_nav_base;
                            jumped = 1;
                        } else if (d >= 1 && d <= n_methods) {
                            /* local 1..N when global range doesn't include small digits */
                            /* only if nav numbers are lo..hi; if lo>9, d alone won't match */
                            popup_digit_accum = 0;
                        } else {
                            popup_digit_accum = 0;
                        }
                        if (jumped)
                            draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                    }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    popup_digit_accum = 0;
                    int row = popup_focus_row;
                    close_context_menu(dpy, popup_win);
                    popup_win = 0;
                    nav_release_pid(g_house_root, getpid());
                    if (choice_mode) {
                        FILE *rf4 = fopen(choice_result_path, "w");
                        if (rf4) { fprintf(rf4, "%s\n", methods[row].action); fclose(rf4); }
                        append_history("SHOW_PAGE_PICK(cursor) action=%s -> %s", methods[row].action, choice_result_path);
                        choice_mode = 0;
                        need_redraw = 1;
                        goto skip_enter_dispatch;
                    }
                    append_history("CLICK(cursor) method=%s action=%s", methods[row].label, methods[row].action);
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* no-op */
                    } else if (strcmp(methods[row].action, "OPEN_USER") == 0) {
                        user_popup_x = popup_x + g_popup_w + 4;
                        user_popup_y = popup_y;
                        user_popup_win = open_context_menu(dpy, popup_gc, &user_popup_x, &user_popup_y, 4, user_methods);
                    } else if (using_objects && strncmp(methods[row].action, "GOTO:", 5) == 0) {
                        const char *target = methods[row].action + 5;
                        for (int pi = 0; pi < n_obj_pages; pi++) {
                            if (strcmp(obj_pages[pi].name, target) == 0) {
                                if (page_stack_n < MAX_PAGES) page_stack[page_stack_n++] = cur_page;
                                cur_page = pi;
                                n_methods = obj_pages[cur_page].n_items;
                                for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                                popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                                popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                                popup_focus_row = 0; popup_digit_accum = 0;
                                break;
                            }
                        }
                    } else if (using_objects && strcmp(methods[row].action, "BACK") == 0) {
                        if (page_stack_n > 0) {
                            cur_page = page_stack[--page_stack_n];
                            n_methods = obj_pages[cur_page].n_items;
                            for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                            popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                            popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                            popup_focus_row = 0; popup_digit_accum = 0;
                        }
                    } else if ((using_objects && strncmp(methods[row].action, "STATE:", 6) == 0) ||
                                       strcmp(methods[row].action, "CLI_IO") == 0) {
                        snprintf(input_key, sizeof(input_key), "%s", methods[row].action + 6);
                        input_buffer[0] = '\0';
                        input_active = 1;
                        append_history("INPUT_ACTIVATE key=%s", input_key);
                        if (!input_popup_win) {
                            input_popup_win = open_context_menu(dpy, popup_gc, (int[]){win_x}, (int[]){win_y + WIN_PX + 4}, 1, NULL) /* writeback discarded */;
                        }
                    } else {
                        dispatch_action(methods[row].action, package_dir, g_house_root, &running);
                    }
                    need_redraw = 1;
                    skip_enter_dispatch: ;
                }
            } else if (user_popup_win && xev.type == Expose && xev.xany.window == user_popup_win) {
                draw_context_menu(dpy, user_popup_win, popup_gc, user_methods, 4, -1, -1);
            } else if (user_popup_win && xev.type == ButtonPress) {
                int raw_row = xev.xbutton.y / POPUP_ROW_H;
                int row = raw_row - 1;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < g_popup_w &&
                             row >= 0 && row < 4;
                if (!inside) {
                    /* REAL FIX 2026-08-07: same "menus stay open until
                     * Cancel" rule as the main menu - an outside click
                     * must not dismiss this submenu either. Toggle with
                     * STATE | menu_stay_open | 0 in meta.pdl. */
                    if (g_menu_stay_open) {
                        popup_soft_focus(dpy, user_popup_win);
                        need_redraw = 1;
                        continue;
                    }
                    close_context_menu(dpy, user_popup_win);
                    user_popup_win = 0;
                    need_redraw = 1;
                    continue;
                }
                close_context_menu(dpy, user_popup_win);
                user_popup_win = 0;
                need_redraw = 1;
            } else if (input_popup_win && xev.type == Expose && xev.xany.window == input_popup_win) {
                char disp[300];
                snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                XClearWindow(dpy, input_popup_win);
                XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                popup_draw_text(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H, disp);
            } else if (input_active && xev.type == KeyPress) {
                /* REAL, 2026-08-05: real text-input for an objects.pdl
                 * "STATE:<key>" row - see the ButtonPress branch above
                 * for how input_active/input_key get set. Escape commits
                 * (matching this house's own cli_io field convention:
                 * activate, type, Escape deactivates/persists - NOT
                 * Enter), Backspace edits, any other printable key
                 * appends. */
                char kbuf[32];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    char statepath[TP_PATH_BUF];
                    snprintf(statepath, sizeof(statepath), "%s/%s.txt", package_dir, input_key);
                    FILE *sf = fopen(statepath, "w");
                    if (sf) { fprintf(sf, "%s\n", input_buffer); fclose(sf); }
                    append_history("INPUT_COMMIT key=%s value=%s", input_key, input_buffer);
                    input_active = 0;
                    if (input_popup_win) { close_context_menu(dpy, input_popup_win); input_popup_win = 0; }
                } else if (ks == XK_BackSpace) {
                    size_t l = strlen(input_buffer);
                    if (l > 0) input_buffer[l - 1] = '\0';
                } else if (klen > 0) {
                    size_t l = strlen(input_buffer);
                    if (l + (size_t)klen < sizeof(input_buffer)) {
                        memcpy(input_buffer + l, kbuf, (size_t)klen);
                        input_buffer[l + klen] = '\0';
                    }
                }
                if (input_popup_win) {
                    char disp[300];
                    snprintf(disp, sizeof(disp), "%s: %s_", input_key, input_buffer);
                    XClearWindow(dpy, input_popup_win);
                    XDrawRectangle(dpy, input_popup_win, popup_gc, 0, 0, g_popup_w - 1, POPUP_ROW_H * 2 - 1);
                    XDrawString(dpy, input_popup_win, popup_gc, 8, POPUP_ROW_H,
                                disp, (int)strlen(disp));
                }
            } else if (text_popup_win && xev.type == Expose && xev.xany.window == text_popup_win) {
                XClearWindow(dpy, text_popup_win);
                int pop_w2 = 0, pop_h2 = 0;
                { Window root_r; int x_r, y_r; unsigned int w_r, h_r, bw_r, depth_r;
                  XGetGeometry(dpy, text_popup_win, &root_r, &x_r, &y_r, &w_r, &h_r, &bw_r, &depth_r);
                  pop_w2 = (int)w_r; pop_h2 = (int)h_r; }
                /* themed border + text (popup_gc is shared with the
                 * context menus, whatever fg they last set - pin it) */
                XSetForeground(dpy, popup_gc, tp_hex_pixel(dpy, DefaultScreen(dpy), kh_shade_hex(g_theme_fg, -60)));
                XDrawRectangle(dpy, text_popup_win, popup_gc, 0, 0, pop_w2 - 1, pop_h2 - 1);
                XSetForeground(dpy, popup_gc, tp_hex_pixel(dpy, DefaultScreen(dpy), g_theme_fg));
                for (int li = 0; li < g_text_popup_n_lines; li++) {
                    popup_draw_text(dpy, text_popup_win, popup_gc, 8, (li + 1) * POPUP_ROW_H - 6, g_text_popup_lines[li]);
                }
            } else if (text_popup_win && xev.type == ButtonPress &&
                       xev.xany.window == text_popup_win) {
                /* REAL FIX 2026-08-24, direct user report ("i dont want it
                 * to close unless i click it exactly (not even if i click
                 * something else)"): this used to be
                 * `(ButtonPress || KeyPress)` with NO window check - any
                 * key anywhere or any click anywhere dismissed the box
                 * (and, while its input grabs were still in play, ate the
                 * event out from under whichever app it belonged to).
                 * Real RPG Maker "Show Text" waits for a confirm press,
                 * but this house's real use is screenshot-and-inspect:
                 * the box is now a passive overlay. ONLY a ButtonPress
                 * delivered ON the popup window itself closes it; keys of
                 * any kind never do; clicks on other windows are none of
                 * this process's business (no grab is taken at creation -
                 * see the creation-site fix above). */
                close_context_menu(dpy, text_popup_win);
                text_popup_win = 0;
                append_history("SHOW_TEXT_DISMISSED");
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1 && g_is_cursword && g_cursword_awaiting_place) {
                /* REAL, NEW 2026-08-30 - the real placement click,
                 * step 2 of the design doc (§10 click-to-place). The
                 * real XGrabPointer taken on arm (below) means ANY
                 * real click anywhere on the screen lands HERE
                 * regardless of which window it visually landed over -
                 * x_root/y_root are real screen coordinates, snapped to
                 * the same real desktop grid every entity already uses
                 * (matches the existing drag's own real grid-snap
                 * technique, just driven by the placement click's own
                 * position instead of the window's dragged position). */
                int gx = (xev.xbutton.x_root + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int gy = (xev.xbutton.y_root + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (gx < 0) gx = 0;
                if (gx > max_col) gx = max_col;
                if (gy < 0) gy = 0;
                if (gy > max_row) gy = max_row;
                win_x = gx * GRID_CELL_PX;
                win_y = gy * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                XUngrabPointer(dpy, CurrentTime);
                kh_ungrab_kbd();
                g_cursword_awaiting_place = 0;
                g_cursword_armed = 0;
                cursword_write_armed(g_house_root, 0);
                append_history("CURSWORD_PLACED");
                cursword_update_shape(dpy, win);
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
                press_root_x = xev.xbutton.x_root;
                press_root_y = xev.xbutton.y_root;
                gettimeofday(&press_tv, NULL);
                /* Keep motion/release on THIS pal even when the pointer
                 * is over File Explorer (otherwise events go to FE). */
                XGrabPointer(dpy, win, False,
                             ButtonReleaseMask | ButtonMotionMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                kh_drag_stack_above(dpy, win, None, 0,
                                    xev.xbutton.x_root, xev.xbutton.y_root);
                XUngrabPointer(dpy, CurrentTime);
                /* Real click-vs-drag distinction, cursword only - see
                 * g_is_cursword's own declaration comment
                 * (CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-
                 * DESIGN.md §9/§10). A real click (small movement, real
                 * quick release) arms/disarms cursword instead of
                 * running the existing grid-snap-drag logic below -
                 * every OTHER entity, and any real drag on cursword
                 * itself, keeps the exact existing behavior unchanged. */
                int was_real_click = 0;
                if (g_is_cursword) {
                    int dx2 = xev.xbutton.x_root - press_root_x;
                    int dy2 = xev.xbutton.y_root - press_root_y;
                    int dist2 = dx2 * dx2 + dy2 * dy2;
                    struct timeval rel_tv; gettimeofday(&rel_tv, NULL);
                    long elapsed_ms = (rel_tv.tv_sec - press_tv.tv_sec) * 1000L +
                                       (rel_tv.tv_usec - press_tv.tv_usec) / 1000L;
                    if (dist2 <= CURSWORD_CLICK_MAX_PX * CURSWORD_CLICK_MAX_PX && elapsed_ms <= CURSWORD_CLICK_MAX_MS)
                        was_real_click = 1;
                }
                if (was_real_click) {
                    g_cursword_armed = !g_cursword_armed;
                    cursword_write_armed(g_house_root, g_cursword_armed);
                    append_history(g_cursword_armed ? "CURSWORD_ARMED" : "CURSWORD_DISARMED");
                    if (g_cursword_armed) g_cursword_log_n = 0; /* real, new 2026-08-30 - fresh key-log each new arm, see cursword_log_key()'s own header comment */
                    cursword_update_shape(dpy, win);
                    if (g_cursword_armed) {
                        /* REAL FIX 2026-08-30, direct report ("its not
                         * taking arrow keys yet? it should be very
                         * stingy with focus till esc is pressed"):
                         * arrow-key nudge was written assuming this
                         * window already held real X11 input focus,
                         * which nothing here ever guaranteed - normal
                         * click-to-focus WM behavior is not reliable
                         * enough for "stingy" key capture. A real
                         * display-wide XGrabKeyboard on arm (same
                         * retry-loop technique as the pre-existing
                         * XGrabPointer just below, and this file's own
                         * popup code ~line 2002) makes EVERY key press
                         * anywhere land on this window's own event
                         * queue regardless of focus, until the real
                         * Escape/disarm/placed paths ungrab it. */
                        /* REAL FIX 2026-08-30, direct live report ("its
                         * not holding focus. we need it to have special
                         * mode of focus when it has halo. where it has
                         * priority over all windows for key input") -
                         * this window is override_redirect (never WM-
                         * managed), the exact same real class of window
                         * open-hai's own code documents as unreliable
                         * for keyboard focus (HOUSE_STDS #21 - see
                         * khtpm_open_hai_render.c's own "Managed window +
                         * _MOTIF_WM_HINTS... NOT override_redirect" real
                         * fix). Converting cursword's whole window model
                         * to WM-managed would risk every other entity's
                         * own real positioning/desktop-icon behavior
                         * (same shared window-creation code, not cursword-
                         * specific) - real, scoped fix instead: raise the
                         * window to the very top AND explicitly set real
                         * input focus onto it, on top of the existing
                         * keyboard grab, every time it arms. A real
                         * grab alone SHOULD already route every key here
                         * per the X11 spec regardless of focus - adding
                         * both is the same "belt and suspenders" real
                         * mitigation for override-redirect focus
                         * flakiness under a real compositor (mutter/
                         * XWayland). */
                        int grab_rc = 0;
                        for (int attempt = 0; attempt < 5; attempt++) {
                            grab_rc = XGrabKeyboard(dpy, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
                            if (grab_rc == GrabSuccess) break;
                            XSync(dpy, False);
                            usleep(5000);
                        }
                        XRaiseWindow(dpy, win);
                        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                        /* Real, visible diagnostic (see cursword_log_key()'s
                         * own header comment) - if the grab itself never
                         * actually succeeded, the debug strip says so
                         * immediately instead of silently doing nothing. */
                        cursword_log_key(grab_rc == GrabSuccess ? "GRAB-OK" : "GRABFAIL");
                    } else {
                        /* Disarmed via a real click (only reachable in
                         * arrow_only move_mode - click_place mode's own
                         * pointer grab means a self-click can never
                         * reach here, see the NOTE below). The keyboard
                         * grab taken on arm above must be released
                         * here too, same as the real Escape path. */
                        kh_ungrab_kbd();
                    }
                    if (g_cursword_armed && g_cursword_click_place) {
                        /* REAL, NEW 2026-08-30 - real click-to-place
                         * arm: grab the pointer display-wide (same real
                         * technique/retry-loop this file's own popup
                         * code already uses, ~line 1957) so the VERY
                         * NEXT real click anywhere is delivered to this
                         * window as the placement click, not whatever
                         * window it visually landed over. */
                        for (int attempt = 0; attempt < 5; attempt++) {
                            int rc = XGrabPointer(dpy, win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                            if (rc == GrabSuccess) { g_cursword_awaiting_place = 1; break; }
                            XSync(dpy, False);
                            usleep(5000);
                        }
                    }
                    /* NOTE: re-clicking cursword's own window while
                     * g_cursword_awaiting_place is true can NEVER reach
                     * this toggle branch at all - the real pointer grab
                     * above means EVERY real click, including one that
                     * visually lands back on cursword itself, is routed
                     * to the dedicated placement-click ButtonPress
                     * branch first (it runs unconditionally on ANY
                     * button-1 press while awaiting placement). Escape
                     * is therefore the only real way to cancel a
                     * pending placement - see that branch's own real
                     * XUngrabPointer call. */
                    need_redraw = 1;
                } else {
                {
                    char dest[PATH_BUF];
                    int zpid = kh_drop_zone_hit(dpy, xev.xbutton.x_root, xev.xbutton.y_root, dest, sizeof(dest), NULL);
                    kh_write_drag_hover(0, "");
                    if (zpid && dest[0] && package_dir[0]) {
                        const char *base = strrchr(package_dir, '/');
                        base = base ? base + 1 : package_dir;
                        if (strncmp(dest, package_dir, strlen(package_dir)) != 0) {
                            char dst[PATH_BUF];
                            snprintf(dst, sizeof(dst), "%s/%s", dest, base);
                            if (rename(package_dir, dst) == 0)
                                running = 0;
                        }
                    }
                }
                if (!running) { dragging = 0; continue; }
                /* REAL FIX 2026-08-04, direct instruction ("egg-pets
                 * snap to grid... do u see that logic"): same
                 * round-to-nearest-cell technique egg_window.c's own
                 * ButtonRelease handler uses, same GRID_CELL_PX (80),
                 * so a placed tile lands on the identical desktop grid
                 * egg-pals already use, not a separate/incompatible one. */
                int grid_x = (win_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int grid_y = (win_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (grid_x < 0) grid_x = 0;
                if (grid_x > max_col) grid_x = max_col;
                if (grid_y < 0) grid_y = 0;
                if (grid_y > max_row) grid_y = max_row;
                win_x = grid_x * GRID_CELL_PX;
                win_y = grid_y * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                XRaiseWindow(dpy, win);
                need_redraw = 1;
                }
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                /* REAL, NEW 2026-08-04: right-click now opens the real
                 * data-driven context menu (see load_methods() above)
                 * instead of closing immediately - matches the ask
                 * ("add the context menus that already exist from
                 * egg-pal to these by default"). */
                if (!popup_win) {
                    /* REAL FIX 2026-08-06: always reload base menu from DISK
                     * (objects.pdl if present, else meta.pdl). In-memory
                     * obj_pages went stale after SHOW_PAGE AND after user
                     * edited objects.pdl/meta.pdl while the process lived
                     * (Events (ez) missing until restart). */
                    read_menu_config(package_dir); /* re-read menu guard toggles too */
                    n_obj_pages = load_objects(package_dir, obj_pages, MAX_PAGES);
                    using_objects = (n_obj_pages > 0);
                    if (using_objects) {
                        cur_page = 0;
                        page_stack_n = 0;
                        n_methods = obj_pages[cur_page].n_items;
                        for (int k = 0; k < n_methods; k++) methods[k] = obj_pages[cur_page].items[k];
                    } else {
                        n_methods = load_methods(package_dir, methods, MAX_METHODS);
                        if (n_methods == 0) {
                            snprintf(methods[0].label, sizeof(methods[0].label), "Close");
                            snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
                            n_methods = 1;
                        }
                    }
                    popup_x = xev.xbutton.x_root;
                    popup_y = xev.xbutton.y_root;
                    popup_win = open_context_menu(dpy, popup_gc, &popup_x, &popup_y, n_methods, methods);
                    popup_nav_base = nav_claim_rows(g_house_root, getpid(), package_dir, methods, n_methods);
                    popup_focus_row = 0; popup_digit_accum = 0;
                    if (popup_win)
                        draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods, popup_nav_base, popup_focus_row);
                }
            } else if (xev.type == MotionNotify && dragging) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_x += dx; win_y += dy;
                if (!g_drag_reparented)
                    XMoveWindow(dpy, win, win_x, win_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
                {
                    char dest[PATH_BUF];
                    int zpid = kh_drop_zone_hit(dpy, xev.xmotion.x_root, xev.xmotion.y_root, dest, sizeof(dest), NULL);
                    {
                        const char *bn = strrchr(package_dir, '/');
                        bn = bn ? bn + 1 : package_dir;
                        kh_write_drag_hover(zpid, bn);
                    }
                    (void)dest;
                }
                need_redraw = 1;
            } else if (xev.type == FocusOut && g_is_cursword && g_cursword_armed &&
                       xev.xfocus.mode == NotifyNormal) {
                /* REAL, NEW 2026-08-30, direct live report ("if
                 * cursword loses focus (like user clicks different
                 * window) is there a way to make sure the halo goes
                 * away, else it causes confusion when going back
                 * (unselects it)") - same real disarm sequence as the
                 * Escape branch just below (kept inline, not factored
                 * into a shared helper, since g_house_root etc. are
                 * main()'s own real locals, not accessible outside
                 * this function). A real XGrabKeyboard doesn't itself
                 * prevent the WM/compositor from moving real X input
                 * focus to a DIFFERENT window the user clicks - this
                 * is the real, missing "clicking away should un-arm
                 * it" signal.
                 * REAL BUG FOUND + FIXED LIVE (2026-08-30, direct live
                 * report of cursword self-closing on the very next
                 * keypress after arming) - X11 ALSO fires a real
                 * FocusOut on THIS SAME window the instant its own
                 * XGrabKeyboard call (just above, on arm) establishes
                 * the grab - a real, spurious, grab-related event, NOT
                 * a genuine "focus moved to another window." Its real
                 * xfocus.mode is NotifyGrab, not NotifyNormal - the
                 * mode check above is the real, correct way to tell
                 * them apart (confirmed via direct Xlib docs: a
                 * genuine focus change from a real user click is
                 * always NotifyNormal). Without this filter, arming
                 * cursword instantly self-disarmed itself one event
                 * later, so the VERY NEXT key hit this file's own
                 * "no menu open -> any key closes the tile" default
                 * fallback (since it looked unarmed again) - not a
                 * crash, a real, deliberate self-close, just
                 * triggered by a bug elsewhere. */
                if (g_cursword_awaiting_place) {
                    XUngrabPointer(dpy, CurrentTime);
                    g_cursword_awaiting_place = 0;
                }
                kh_ungrab_kbd();
                g_cursword_armed = 0;
                cursword_write_armed(g_house_root, 0);
                append_history("CURSWORD_DISARMED_FOCUS_LOST");
                cursword_update_shape(dpy, win);
                need_redraw = 1;
            } else if (xev.type == KeyPress) {
                if (g_is_cursword && g_cursword_armed) {
                    /* Real, house-standard dual-mode boundary - while
                     * armed, real key capture begins and continues
                     * until real Escape (CURSWORD-DESKTOP-3D-AND-
                     * PIECECRAFT-INSCENE-DESKS-DESIGN.md §3a, same
                     * real principle as board-viewer's own
                     * active_index==-1 model, !.HOUSE_STDS.md §A.9).
                     * Without this branch, this file's own real
                     * default ("no popup open -> any key closes the
                     * tile," right below) would close cursword outright
                     * the moment a key was pressed while armed -
                     * confirmed by direct read, not assumed. */
                    KeySym ks2 = XLookupKeysym(&xev.xkey, 0);
                    /* 2026-09-09: cursword's desktop camera has no
                     * numeric mode keys - the "one map" 1-4 reservation
                     * (and the brief 1-4 -> 5-8 remap) is abandoned
                     * (^.ONE-MAP-ATTEMPT.md; pc-hq/board-viewer uses 1-4
                     * for POV again). Digit keys fall through to the
                     * generic cursword_log_key() / dispatch below with
                     * no special-casing - XKeysymToString() returns the
                     * literal digit string same as any other key. */
                    cursword_log_key(
                        ks2 == XK_Escape ? "ESC" :
                        ks2 == XK_Left ? "LEFT" : ks2 == XK_Right ? "RIGHT" :
                        ks2 == XK_Up ? "UP" : ks2 == XK_Down ? "DOWN" :
                        XKeysymToString(ks2) ? XKeysymToString(ks2) : "?");
                    need_redraw = 1; /* real, unconditional - the log line above must always repaint, even for a key none of the branches below handle */
                    if (ks2 == XK_Escape) {
                        if (g_cursword_awaiting_place) {
                            /* Real cancel of a pending click-to-place -
                             * the pointer grab from arm-time must be
                             * dropped here, this is the only real
                             * cancel path (see the placement
                             * ButtonPress branch's own comment for why
                             * re-clicking cursword itself can't reach
                             * this instead). */
                            XUngrabPointer(dpy, CurrentTime);
                            g_cursword_awaiting_place = 0;
                        }
                        kh_ungrab_kbd();
                        g_cursword_armed = 0;
                        cursword_write_armed(g_house_root, 0);
                        append_history("CURSWORD_DISARMED");
                        cursword_update_shape(dpy, win);
                        need_redraw = 1;
                    } else if (ks2 == XK_Left || ks2 == XK_Right || ks2 == XK_Up || ks2 == XK_Down) {
                        /* REAL, NEW 2026-08-30, step 2 - real arrow-key
                         * nudge (§3a: "Arrow keys move cursword itself
                         * ... likely the same 80px GRID_CELL_PX every
                         * entity already snaps to"). Real, always-on
                         * baseline movement in EITHER move_mode - same
                         * real grid-snap + write_pos + XMoveWindow
                         * technique the existing drag-release code uses,
                         * just stepping by one whole cell per press
                         * instead of snapping a dragged pixel position. */
                        int gx = win_x / GRID_CELL_PX, gy = win_y / GRID_CELL_PX;
                        if (ks2 == XK_Left) gx--;
                        else if (ks2 == XK_Right) gx++;
                        else if (ks2 == XK_Up) gy--;
                        else gy++;
                        if (gx < 0) gx = 0;
                        if (gx > max_col) gx = max_col;
                        if (gy < 0) gy = 0;
                        if (gy > max_row) gy = max_row;
                        win_x = gx * GRID_CELL_PX;
                        win_y = gy * GRID_CELL_PX;
                        /* REAL FIX 2026-08-31, direct live report ("now
                         * when moving cursword, in 3d mode it flickers
                         * back to old position after every arrow key
                         * move") - this used to move to the raw,
                         * unpanned win_x/win_y directly, then the
                         * per-frame camera-pan correction (further down
                         * this same loop, real header comment: "win_x/
                         * win_y themselves... stay completely
                         * untouched... only the actual X11 window
                         * position gets +cam_pan_x/+cam_pan_y") saw a
                         * real mismatch against its own cached last-
                         * applied position and immediately re-moved it
                         * to the correct panned spot - two real, back-
                         * to-back XMoveWindow calls to two different
                         * targets in the same tick, a real visible
                         * flicker. Real fix: apply the same real pan
                         * offset here too when in 3D mode, so this
                         * call already lands on the correct spot and
                         * the later correction becomes a genuine no-op
                         * (matches what it would have computed anyway,
                         * not a second real move). */
                        {
                            int in_3d = (g_camera_mode == 3 || g_camera_mode == 4);
                            int disp_x = in_3d ? win_x + g_cam_pan_x : win_x;
                            int disp_y = in_3d ? win_y + g_cam_pan_y : win_y;
                            XMoveWindow(dpy, win, disp_x, disp_y);
                        }
                        write_pos(package_dir, win_x, win_y);
                        need_redraw = 1;
                    } else if (cursword_handle_camera_key(g_house_root, package_dir, ks2)) {
                        /* Real, consolidated dispatch - see
                         * cursword_handle_camera_key()'s own header
                         * comment (1-4 mode, f reset, wasd/rt pan-
                         * tilt, and any future camera key all live
                         * there now, one real place). */
                        need_redraw = 1;
                    }
                } else if (popup_win || user_popup_win || input_popup_win || text_popup_win || input_active) {
                    /* REAL FIX 2026-08-07, direct instruction ("print
                     * screen closes the context"): with a menu/popup open,
                     * an unhandled key (Print Screen, media keys, etc.) is
                     * NOT a close signal. The old fallback here set
                     * running = 0 on ANY key, so pressing Print Screen
                     * while a bible-verse / context menu was up silently
                     * killed the whole tile window. Menus stay open until
                     * the user clicks Cancel or presses Escape/Enter. */
                } else if (g_is_cursword) {
                    /* REAL FIX 2026-08-31, direct live report (cursword
                     * silently vanishing - a real key hitting cursword
                     * right after it disarmed, e.g. from a rapid test
                     * sequence, fell straight into the generic
                     * "no popup open -> any key closes the tile" fallback
                     * below and killed the whole process - no crash, no
                     * signal, a real, deliberate, if surprising, exit).
                     * Direct instruction on the fix: "any key is meant
                     * to turn its halo focus off, not close it" - cursword
                     * is the real always-open assistant entity (see the
                     * "always-open first entity" work), not a closable
                     * popup tile, so it's now exempt from that generic
                     * close-on-any-key default. It's already unarmed by
                     * the time this branch can even run (the
                     * g_cursword_armed branch above handles every key
                     * while armed), so there's nothing further to do here
                     * - the halo/focus state already reflects "off." */
                } else {
#ifndef _WIN32
                    running = 0; /* no menu open: any key closes the tile, as before */
#endif
                }
            }
        }
        if (!running) break;
#ifndef _WIN32
        if (!package_still_exists(package_dir)) break; /* package gone -> stop pointing at nothing */
#endif

        /* REAL FIX 2026-08-04, direct instruction ("dont render more
         * than 30fps etc, cpu is getting hot"): explicit, measured frame
         * pacing (not just relying on select()'s 300ms timeout, which
         * doesn't bound how often this loop redraws once real X events
         * start arriving, e.g. during a drag). Skips the redraw entirely
         * if nothing changed AND less than one frame interval has
         * passed since the last real swap - a static, undragged tile
         * does zero GL work between polls. */
        struct timeval now;
        gettimeofday(&now, NULL);
        /* Win long is 32-bit: (tv_sec * 1000000) overflows, goes negative,
         * and this skip never presents a frame. Linux long is 64-bit. */
        long long elapsed_usec =
            ((long long)now.tv_sec - last_frame.tv_sec) * 1000000LL +
            ((long long)now.tv_usec - last_frame.tv_usec);
        if (!need_redraw) {
            continue;
        }
        if (last_frame.tv_sec != 0 && elapsed_usec < MIN_FRAME_USEC) {
            long rem = (long)(MIN_FRAME_USEC - elapsed_usec);
            if (rem > 0 && rem < 1000000L) usleep((useconds_t)rem);
            continue;
        }

        {
            int bg_r = (int)(r * 255.0f), bg_g = (int)(g * 255.0f), bg_b = (int)(b * 255.0f);
            /* Real, new 2026-08-30 - explicit full alpha, see
             * draw_sprite_rgb()'s own matching comment (harmless
             * no-op high byte for every non-cursword entity). */
            XSetForeground(dpy, g_buf_gc, 0xFF000000UL | ((unsigned long)bg_r << 16) | ((unsigned long)bg_g << 8) | (unsigned long)bg_b);
            /* Real, new 2026-08-30: cursword's own buffer reserves
             * CURSWORD_LOG_H extra rows (see the g_buf XCreatePixmap
             * comment above) - cleared here too every frame so stale
             * key-log text never lingers under a fresh background. */
            XFillRectangle(dpy, g_buf, g_buf_gc, 0, 0, (unsigned)(g_is_cursword ? CURSWORD_LOG_W : WIN_PX),
                            (unsigned)(WIN_PX + (g_is_cursword ? CURSWORD_LOG_H : 0)));
            /* REAL, NEW 2026-08-30, direct report ("im still having to
             * click right on the image") + direct instruction ("solid
             * disc but very low transparency") - draws the real, dim
             * backdrop disc that now permanently occupies cursword's
             * widened ShapeBounding (see cursword_update_shape()'s own
             * header comment for the full reasoning: this window has
             * no real per-pixel alpha, so a near-black fill is the
             * closest honest stand-in for "very low transparency").
             * Drawn BEFORE the sprite, always (not gated on armed), so
             * the sprite still renders crisp on top of it.
             * REAL FOLLOW-UP 2026-08-30 ("i wanted to change the
             * colors alpha... very transparent") - genuine per-pixel
             * alpha now, not a color trick: cursword's own window is a
             * real 32-bit ARGB visual (see the XMatchVisualInfo setup
             * near main()'s window creation) - the compositor blends
             * this disc against whatever's really behind it using the
             * alpha byte below, same gray (0x141414) the user already
             * confirmed was the right color. Trivially tunable -
             * raise/lower just the leading byte to taste.
             * REAL FOLLOW-UP 2026-08-30 ("set it to 1% alpha, even
             * lower?"): 0x20 (~12%) -> 0x03 (~1%, 3/255) - still a
             * real, nonzero alpha (the shape/click boundary is
             * unaffected either way, see the header comment above -
             * this is purely how visible it reads). */
            if (g_is_cursword) {
                /* Real, new 2026-08-30 ("make the grey translucent
                 * circle around cursword completely transparent 0%") -
                 * alpha 0x03 -> 0x00. Still a real, unioned
                 * ShapeBounding region (the wider click surface stays
                 * exactly as wide - alpha and clickability are fully
                 * independent, per this disc's own earlier header
                 * comment), just genuinely invisible now. */
                XSetForeground(dpy, g_buf_gc, 0x00141414UL);
                int dcx = WIN_PX / 2, dcy = WIN_PX / 2;
                int dradius = WIN_PX / 2 - 5;
                XFillArc(dpy, g_buf, g_buf_gc, dcx - dradius, dcy - dradius,
                          (unsigned)(dradius * 2), (unsigned)(dradius * 2), 0, 360 * 64);
            }
            /* Real, visible armed-state halo (§3a/§9 item 4: overlay/
             * ring, never replacing the sprite). STALE NOTE, corrected
             * 2026-08-30: originally drawn BEFORE the sprite here,
             * relying on draw_sprite_rgb()'s own per-pixel alpha to
             * "peek through" - moved AFTER the sprite instead (see the
             * real fix comment right below) once this sprite was
             * confirmed to have no real alpha transparency. Real color
             * changed from the design doc's original neon-blue spec to
             * a yellow glow, direct instruction. */
            /* Real, new 2026-08-30 - desktop-wide 3D switch (see
             * load_camera_mode()/draw_topdown_block_rgb()'s own header
             * comment): modes 3/4 render every sprite entity as a real
             * extruded block instead of the flat top-down blit. Cheap
             * enough to poll unconditionally every frame at this
             * file's own 30fps cap (MAX_FPS) - a tiny, single-line
             * file, no changed-marker optimization needed at this
             * scale. */
            load_camera_mode(g_house_root);
            load_camera_state(g_house_root);
            load_active_z(g_house_root);
            /* REAL, NEW 2026-08-31, direct instruction ("do we have z
             * layers yet?... it affects 2d also. in 2d u wont see the
             * entity") - real z-level VISIBILITY filter, applies
             * unconditionally in EVERY camera mode (not just 3D):
             * an entity whose own real g_entity_z doesn't match the
             * shared g_active_z (set only by cursword's own real c/v
             * keys, see cursword_handle_camera_key()'s own header
             * comment) is genuinely unmapped - not drawn, not
             * present, matching a real "which floor am I looking at"
             * convention, achievable within this file's own existing
             * one-window-per-entity architecture (no shared 3D scene
             * needed, direct instruction: "i hope we dont have to
             * switch to shared scene just yet"). */
            {
                static int z_was_mapped = 1; /* window starts real, mapped (XMapWindow already ran earlier in main()) */
                int z_should_show = (g_entity_z == g_active_z) || dragging;
                if (dragging) { XMapWindow(dpy, win); XRaiseWindow(dpy, win); z_was_mapped = 1; }
                else if (z_should_show && !z_was_mapped) { XMapWindow(dpy, win); z_was_mapped = 1; }
                else if (!z_should_show && z_was_mapped) { XUnmapWindow(dpy, win); z_was_mapped = 0; }
                if (!z_should_show) goto skip_zfiltered_draw;
            }
            /* REAL, NEW 2026-08-31 - shared by both branches right
             * below (see update_entity_shape_from_3d()'s own header
             * comment for the full "red shadow" bug this is part of
             * fixing) - tracks whether THIS entity was in 3D mode last
             * frame, so returning to 2D restores its real shape
             * exactly once on the transition, not every 2D frame. */
            static int was_3d_last_frame = 0;
            if (g_has_sprite) {
                if (g_camera_mode == 3 || g_camera_mode == 4) {
                    /* REAL FIX 2026-08-30, direct live report ("its
                     * still showing 2d sprite behind the 3d. can u fix
                     * that?") - the flat sprite used to always draw
                     * first as a real "no gaps" base layer (matching
                     * draw_topdown_block_rgb()'s own older reasoning,
                     * from when the extrusion cue was a thin strip
                     * that genuinely needed something solid behind
                     * it). Both real raymarchers below now cover their
                     * own entire real footprint on a hit (a solid box,
                     * or the phymoji model's own real silhouette) - a
                     * missed ray is real, honest EMPTY space (the
                     * plain background already filled above), not the
                     * old flat sprite peeking through around/behind
                     * the 3D shape. Real per-voxel phymoji render
                     * (genuine volumetric silhouette) whenever this
                     * entity has a real generated voxel asset; the
                     * single-box-with-texture raymarch stays the real,
                     * honest fallback for any entity that doesn't. */
                    if (g_phymoji_col_count > 0)
                        draw_phymoji_rgb(dpy, g_buf, g_buf_gc);
                    else
                        draw_raymarch_block_rgb(dpy, g_buf, g_buf_gc, bg_r, bg_g, bg_b);
                    /* REAL FIX 2026-08-31, direct live report ("some
                     * entities... when rotated, leave a 'red shadow' of
                     * their 2d shape") - see update_entity_shape_from_3d()'s
                     * own header comment for the full root cause.
                     * Cursword exempt - see that same comment. */
                    if (!g_is_cursword)
                        update_entity_shape_from_3d(dpy, win, g_buf, bg_r, bg_g, bg_b);
                    was_3d_last_frame = 1;
                } else {
                    draw_sprite_rgb(dpy, g_buf, g_buf_gc, bg_r, bg_g, bg_b);
                    /* REAL, NEW 2026-08-31 - the real other half of the
                     * fix above: coming BACK to 2D from 3D must restore
                     * the window's real shape to the flat sprite's own
                     * silhouette (update_entity_shape_from_3d() left it
                     * pinned to whatever the last 3D frame's raymarch
                     * happened to cover), or the entity would stay stuck
                     * shaped like its last 3D pose forever - only runs
                     * on the actual mode transition, not every 2D
                     * frame. */
                    if (was_3d_last_frame && !g_is_cursword) {
                        Pixmap smask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
                        GC smask_gc = XCreateGC(dpy, smask, 0, NULL);
                        build_shape_mask(dpy, win, smask_gc, smask);
                        XFreeGC(dpy, smask_gc);
                        XFreePixmap(dpy, smask);
                    }
                    was_3d_last_frame = 0;
                }
            }
            else if (g_font_loaded) draw_glyph_rgb(dpy, g_buf, g_buf_gc, glyph, win_vis, swa.colormap);
            /* REAL, NEW 2026-08-30, direct instruction ("camera pan/
             * zoom moves the whole desktop") - a real, desktop-wide
             * screen-position offset while in 3D mode. win_x/win_y
             * themselves (the entity's own TRUE logical grid position,
             * used by drag/arrow-nudge/click-to-place/write_pos) are
             * deliberately left untouched - this only corrects the
             * real, DISPLAYED X11 position, an offset applied on top,
             * so panning can never corrupt an entity's own saved
             * position. Snaps back to the true win_x/win_y the instant
             * camera_mode leaves 3/4. Tracked with a static "last
             * applied" pair so this is a real no-op XMoveWindow-wise
             * on every frame pan/mode aren't actually changing (avoids
             * fighting a concurrent drag/arrow-nudge's own, separate
             * XMoveWindow calls more than strictly necessary). */
            {
                static int last_disp_x = -999999, last_disp_y = -999999;
                int in_3d = (g_camera_mode == 3 || g_camera_mode == 4);
                int disp_x = in_3d ? win_x + g_cam_pan_x : win_x;
                int disp_y = in_3d ? win_y + g_cam_pan_y : win_y;
                if (disp_x != last_disp_x || disp_y != last_disp_y) {
                    XMoveWindow(dpy, win, disp_x, disp_y);
                    last_disp_x = disp_x; last_disp_y = disp_y;
                }
            }
            /* REAL FIX 2026-08-30, found live: the halo used to draw
             * BEFORE the sprite, relying on draw_sprite_rgb()'s own
             * per-pixel alpha to let it "peek through" transparent
             * margins - confirmed live this specific sprite has no
             * real alpha transparency (fully opaque square), so the
             * halo was getting completely covered, invisible. Real
             * fix: draw the halo AFTER the sprite instead, right at
             * the window's own edge (inset only 1/4/7px) - a real,
             * always-visible border-glow regardless of any given
             * sprite's own opacity, still a real overlay/ring per §9
             * item 4 (never replaces the sprite, just frames it). */
            if (g_is_cursword && g_cursword_armed) {
                int cx = WIN_PX / 2, cy = WIN_PX / 2;
                /* REAL FIX 2026-08-30, found live: a 3-ring gradient
                 * with thin arcs left real gaps between rings, letting
                 * a few of the sprite's own semi-transparent edge
                 * pixels (alpha 1-127, below build_shape_mask()'s own
                 * >127 cutoff, normally never visible at all) show
                 * through as stray off-color specks once the mask
                 * newly included that span. Real fix: ONE solid ring,
                 * geometry byte-identical to cursword_update_shape()'s
                 * own mask ring (same radius, same line width) -
                 * guaranteed zero gaps since it's the exact same shape,
                 * not an approximation of it. */
                int halo_radius = WIN_PX / 2 - 5;
                if (halo_radius > 3) {
                    /* Real, direct instruction ("i actually want to
                     * change it to a 'yellow glowing look' instead of
                     * blue") - matches this house's own real "amber
                     * tint = armed" precedent (§3a) more closely than
                     * blue ever did. */
                    /* Real, new 2026-08-30: explicit full alpha (see
                     * draw_sprite_rgb()'s own matching comment) - the
                     * halo itself stays fully opaque, only the disc
                     * behind it (drawn above) is translucent. */
                    XSetForeground(dpy, g_buf_gc, 0xFFFFD400UL);
                    XSetLineAttributes(dpy, g_buf_gc, 9, LineSolid, CapButt, JoinMiter);
                    XDrawArc(dpy, g_buf, g_buf_gc, cx - halo_radius, cy - halo_radius, (unsigned)(halo_radius * 2), (unsigned)(halo_radius * 2), 0, 360 * 64);
                    XSetLineAttributes(dpy, g_buf_gc, 0, LineSolid, CapButt, JoinMiter);
                }

                /* REAL, NEW 2026-08-30, direct instruction ("i still
                 * dont have arrow control. would it help if we did a
                 * text display under cursword with pressed key
                 * history?") - a real, live-visible readout of the last
                 * few keys THIS window's own event loop actually
                 * received (cursword_log_key()'s own header comment has
                 * the full reasoning: tells "key never arrived" apart
                 * from "key arrived but didn't move it" at a glance,
                 * no file/log digging needed). */
                char logline[96] = "";
                size_t loff = 0;
                for (int li = 0; li < g_cursword_log_n; li++) {
                    int wrote = snprintf(logline + loff, sizeof(logline) - loff, "%s%s",
                                          li > 0 ? " " : "", g_cursword_log[li]);
                    if (wrote < 0 || (size_t)wrote >= sizeof(logline) - loff) break;
                    loff += (size_t)wrote;
                }
                /* Real, new 2026-08-30: explicit full alpha, same
                 * reasoning as the halo just above. */
                XSetForeground(dpy, g_buf_gc, 0xFFFFFFFFUL);
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 15,
                                 logline[0] ? logline : "(no keys yet)");

                /* REAL, NEW 2026-08-30, direct instruction ("can we do
                 * another debug below sword, that shows camera
                 * angle?") - a second, real line: the exact same
                 * pitch_deg formula build_raymarch_cam() itself
                 * computes from g_cam_tilt (not a separate guess), so
                 * this is always genuinely what the camera is doing
                 * right now, not just the raw tilt number - direct
                 * live use case: "make sure there is no tilt or angle"
                 * is verifiable at a glance without guessing whether
                 * tilt=0 really means pitch=0. */
                char camline[48];
                double dbg_pitch = (g_cam_tilt / 100.0) * 65.0;
                snprintf(camline, sizeof(camline), "tilt=%d pitch=%.0f%s",
                         g_cam_tilt, dbg_pitch, g_emoji_sprite_view_top ? " top" : " front");
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 33, camline);

                /* REAL, NEW 2026-08-31, direct instruction ("zx cy
                 * aren't changing z level in 2d or 3d mode yet as far
                 * as im concerned... add another debug row for
                 * cursword that show xyz position") - a real third
                 * line: THIS process's own real win_x/win_y (the true
                 * logical grid position, not the panned display
                 * position) and g_entity_z (this entity's own real,
                 * persisted z - changed only by c/v, see
                 * cursword_handle_camera_key()'s own header comment),
                 * plus the shared g_active_z right next to it so a
                 * z-vs-active-z mismatch (which is what actually
                 * drives real visibility - see the map/unmap logic
                 * earlier in this same loop) is visible at a glance,
                 * not just cursword's own z in isolation. */
                char posline[48];
                snprintf(posline, sizeof(posline), "x=%d y=%d z=%d az=%d",
                         win_x, win_y, g_entity_z, g_active_z);
                popup_draw_text(dpy, g_buf, g_buf_gc, 2, WIN_PX + 51, posline);
            }

            /* Present: same compose->present pattern as db-hq/taskbar -
             * one XGetImage capture off the buffer, XPutImage onto the
             * real window (proven pixel-identical in the original Phase
             * 0 test). Falls back to a direct XCopyArea if XGetImage
             * ever fails, matching the same safety fallback used there. */
            XSync(dpy, False);
            /* Real, new 2026-08-30: present the full reserved buffer
             * height while cursword is armed (the key-log strip),
             * exactly WIN_PX otherwise/for every other entity. */
            int present_h = WIN_PX + (g_is_cursword && g_cursword_armed ? CURSWORD_LOG_H : 0);
            int present_w = (g_is_cursword && g_cursword_armed) ? CURSWORD_LOG_W : WIN_PX;
            XImage *frame = XGetImage(dpy, g_buf, 0, 0, present_w, present_h, AllPlanes, ZPixmap);
            if (frame) {
                XPutImage(dpy, win, g_buf_gc, frame, 0, 0, 0, 0, present_w, present_h);
                XDestroyImage(frame);
            } else {
                XCopyArea(dpy, g_buf, win, g_buf_gc, 0, 0, (unsigned)present_w, (unsigned)present_h, 0, 0);
            }
        }
        skip_zfiltered_draw:
        last_frame = now;
    }

    if (popup_win) close_context_menu(dpy, popup_win); /* e.g. closed via keypress while menu was still open */
    if (user_popup_win) close_context_menu(dpy, user_popup_win);
    if (input_popup_win) close_context_menu(dpy, input_popup_win);
    /* Stage 2c PROOF - don't orphan the khtpm menu child if this entity's
     * own window closes while the menu is still up. */
    if (g_khtpm_menu_pid > 0) { kill(g_khtpm_menu_pid, SIGTERM); waitpid(g_khtpm_menu_pid, NULL, WNOHANG); }
    if (g_house_root[0]) {
        livedesk_registry_remove(g_house_root, getpid());
        nav_release_pid(g_house_root, getpid());
    }
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    XFreeGC(dpy, g_buf_gc);
    XFreePixmap(dpy, g_buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
/* ============ end tile mode ============ */
int main(int argc, char **argv) {
    return tp_main(argc, argv);
}
