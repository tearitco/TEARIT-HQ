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
 * 2026-09-20 LABELLED GRID: the grid cell is now the REAL desk grid cell
 * (desk_grid.pdl cell_px, default 80 reference px, scaled per screen by
 * khtpm_ui_scale.c) so a labelled cell is exactly where placement snaps -
 * before, the overlay drew fixed 64px screen cells while consumers snap to
 * 64 (File Explorer script) or 80 (tp_place_desktop_rmmv's x/80). Columns
 * are lettered A,B,.. along the top, rows numbered 1,2,.. down the left.
 * Keyboard (same behaviour as csv-hq's <grid>, via khtpm_grid_jump.c): type a
 * ref in either order (c7 / 7c) - shown as "jump: c7_" - Enter jumps the
 * highlight there, arrows move it, Enter again places at that cell's centre
 * (the same RMMV_CLICK / FE_PLACE_CLICK / drop-zone path a mouse click there
 * takes), Backspace edits, Esc cancels, a mouse click still places. The first
 * key only ARMS the highlight, so the Enter that launched Place can never
 * place by itself. Env: TP_PLACE_CELL_REF=<ref px> overrides the cell,
 * TP_PLACE_NO_ARGB=1 forces the plain visual. Design + behaviour spec:
 * 08-roadmap/design-docs/GRID-ELEMENT-DESIGN.md ("Reuse by overlay pickers"),
 * PLACEMENT-FROM-EXPLORER.md. Keys arrive as KeyPress under the keyboard grab
 * and via XQueryKeymap edges when another client holds it (HOUSE_CODE_PITFALLS
 * #24 - relay/Xephyr tests cannot prove real-hardware key delivery).
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
#include "khtpm_ui_scale.c" /* screen click -> reference px for desktop_pos.txt */
/* TRANSITIONAL cross-binary include (see INMEM-DB-STATE-LAYER-PLAN.md section 5): the pure
 * cell-jump state machine shared with csv-hq's <grid>. Kept to this one file on purpose. */
#include "khtpm_grid_jump.c"

#define PATH_BUF 4352

/* Grid geometry in SCREEN px. Cell k spans [cell_edge(k), cell_edge(k+1)); each edge is the
 * exact screen position of reference px k*g_cell_ref (khtpm_ui_scale.c maps k*base_cell to
 * k*scaled_cell exactly), so edges never drift from what the consumers snap to. */
static int g_auto = 100, g_base_cell = 80, g_cell_ref = 80;
static int g_cols = 1, g_rows = 1;

static int cell_edge(int k) { return kps_ref_to_screen(k * g_cell_ref, g_base_cell, g_auto); }

/* index of the cell containing screen coordinate px (>= 0) */
static int cell_at(int px) {
    int k;
    if (px <= 0) return 0;
    k = kps_screen_to_ref(px, g_base_cell, g_auto) / (g_cell_ref > 0 ? g_cell_ref : 1);
    if (k < 0) k = 0;
    while (k > 0 && cell_edge(k) > px) k--;
    while (k < 100000 && cell_edge(k + 1) <= px) k++;
    return k;
}

/* hq_ui.pdl / desk_grid.pdl live in <house>/#.desktop. Palettes passes that dir as
 * desktop_root; File Explorer passes the house root - accept either. */
static void resolve_settings_dir(const char *root, char *out, size_t outsz) {
    char p[PATH_BUF];
    snprintf(out, outsz, "%s", root);
    snprintf(p, sizeof(p), "%s/hq_ui.pdl", root);
    if (access(p, R_OK) == 0) return;
    snprintf(p, sizeof(p), "%s/desk_grid.pdl", root);
    if (access(p, R_OK) == 0) return;
    snprintf(p, sizeof(p), "%s/#.desktop", root);
    if (access(p, R_OK | X_OK) == 0) snprintf(out, outsz, "%s", p);
}

/* TP_PLACE_DEBUG=1: one stderr line per key event/edge (for diagnosing key delivery). */
static int g_dbg = -1;
#define PDBG(...) do { if (g_dbg < 0) g_dbg = (getenv("TP_PLACE_DEBUG") && getenv("TP_PLACE_DEBUG")[0] == '1'); \
                       if (g_dbg) { fprintf(stderr, "[placer] " __VA_ARGS__); fputc('\n', stderr); } } while (0)

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* All overlay drawing state (panes tile the screen around the picker's hole). */
typedef struct {
    Display *dpy;
    int sw, sh, use_argb, n;
    struct { Window w; int x, y, ww, wh; } panes[4];
    GC gcs[4];
    int hx, hy, hw, hh;                 /* picker hole (hw == 0: none) */
    XFontStruct *fs;
    int lab_h, lab_w;                   /* column-label band height, row-label gutter width */
    unsigned long c_line, c_label, c_ok, c_bad, c_band, c_okfill, c_badfill;
    int pill_x, pill_y, pill_w, pill_h; /* status pill rect (screen px) */
    GjState gj;
    int kb_active, ptr_x, ptr_y;
    long long last_key_ms;              /* when the last grid key was handled */
    char err[64];
    long long err_until;
    int use_zones, hover_pid;
    const char *root, *skip_dir, *place_name;
} Ov;

static int ov_cell_valid(const Ov *o, int r, int c, int *cx, int *cy) {
    int x0 = cell_edge(c), x1 = cell_edge(c + 1), y0 = cell_edge(r), y1 = cell_edge(r + 1);
    int mx, my;
    if (x1 > o->sw) x1 = o->sw; /* a cell cut by the screen edge: use its visible part */
    if (y1 > o->sh) y1 = o->sh;
    mx = (x0 + x1) / 2; my = (y0 + y1) / 2;
    if (mx >= o->sw) mx = o->sw - 1;
    if (my >= o->sh) my = o->sh - 1;
    if (cx) *cx = mx;
    if (cy) *cy = my;
    if (o->hw > 0 && mx >= o->hx && mx < o->hx + o->hw && my >= o->hy && my < o->hy + o->hh) return 0;
    return 1;
}

static void ov_cell_name(const Ov *o, int r, int c, char *out, size_t outsz) {
    char col[8];
    (void)o;
    gj_col_to_letters(c, col, sizeof(col));
    snprintf(out, outsz, "%s%d", col, r + 1);
}

static void ov_draw_pane(Ov *o, int i) {
    Display *dpy = o->dpy;
    Window w = o->panes[i].w;
    GC gc = o->gcs[i];
    int ox = o->panes[i].x, oy = o->panes[i].y, ww = o->panes[i].ww, wh = o->panes[i].wh, k;
    XClearWindow(dpy, w);
    XSetForeground(dpy, gc, o->c_line);
    XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    for (k = 0; k < g_cols + 1; k++) {
        int x = cell_edge(k);
        if (x >= ox && x < ox + ww) XDrawLine(dpy, w, gc, x - ox, 0, x - ox, wh);
    }
    for (k = 0; k < g_rows + 1; k++) {
        int y = cell_edge(k);
        if (y >= oy && y < oy + wh) XDrawLine(dpy, w, gc, 0, y - oy, ww, y - oy);
    }
    if (o->fs) {
        char s[16];
        int fasc = o->fs->ascent, fdesc = o->fs->descent;
        XSetForeground(dpy, gc, o->c_band);
        XFillRectangle(dpy, w, gc, -ox, -oy, (unsigned)o->sw, (unsigned)o->lab_h);
        XFillRectangle(dpy, w, gc, -ox, o->lab_h - oy, (unsigned)o->lab_w, (unsigned)(o->sh - o->lab_h));
        XSetForeground(dpy, gc, o->c_label);
        for (k = 0; k < g_cols; k++) {
            int x0 = cell_edge(k), x1 = cell_edge(k + 1), tw, n;
            if (x1 > o->sw) x1 = o->sw; /* label the visible part of a cell cut by the screen edge */
            if (x1 <= ox || x0 >= ox + ww) continue;
            gj_col_to_letters(k, s, sizeof(s));
            n = (int)strlen(s);
            tw = XTextWidth(o->fs, s, n);
            if (x1 - x0 < tw + 2) continue;
            XDrawString(dpy, w, gc, (x0 + x1 - tw) / 2 - ox, fasc + 2 - oy, s, n);
        }
        for (k = 0; k < g_rows; k++) {
            int y0 = cell_edge(k), y1 = cell_edge(k + 1), tw, n;
            if (y1 > o->sh) y1 = o->sh;
            if (y1 <= oy || y0 >= oy + wh || y1 <= o->lab_h) continue;
            snprintf(s, sizeof(s), "%d", k + 1);
            n = (int)strlen(s);
            tw = XTextWidth(o->fs, s, n);
            XDrawString(dpy, w, gc, (o->lab_w - tw) / 2 - ox, (y0 + y1 + fasc - fdesc) / 2 - oy, s, n);
        }
    }
    if (o->kb_active) {
        int r = o->gj.row, c = o->gj.col;
        int ok = ov_cell_valid(o, r, c, NULL, NULL);
        int x0 = cell_edge(c) - ox, y0 = cell_edge(r) - oy;
        int cw = cell_edge(c + 1) - cell_edge(c), ch = cell_edge(r + 1) - cell_edge(r);
        if (o->use_argb) {
            XSetForeground(dpy, gc, ok ? o->c_okfill : o->c_badfill);
            XFillRectangle(dpy, w, gc, x0, y0, (unsigned)cw, (unsigned)ch);
        }
        XSetForeground(dpy, gc, ok ? o->c_ok : o->c_bad);
        XSetLineAttributes(dpy, gc, 3, LineSolid, CapButt, JoinMiter);
        XDrawRectangle(dpy, w, gc, x0 + 1, y0 + 1, (unsigned)(cw - 3), (unsigned)(ch - 3));
        XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
    }
    if (o->fs) {
        char l1[96], nm[24];
        const char *l2 = "Enter = jump | Enter again = place | Esc = cancel | click = place";
        int fh = o->fs->ascent + o->fs->descent, lx = o->pill_x - ox, ly = o->pill_y - oy;
        int bad = 0;
        if (o->err[0]) { snprintf(l1, sizeof(l1), "%s", o->err); bad = 1; }
        else if (o->gj.jump[0]) snprintf(l1, sizeof(l1), "jump: %s_", o->gj.jump);
        else if (o->kb_active) {
            ov_cell_name(o, o->gj.row, o->gj.col, nm, sizeof(nm));
            snprintf(l1, sizeof(l1), "cell %s%s", nm, ov_cell_valid(o, o->gj.row, o->gj.col, NULL, NULL) ? "" : " (under the picker)");
            bad = !ov_cell_valid(o, o->gj.row, o->gj.col, NULL, NULL);
        } else snprintf(l1, sizeof(l1), "type a cell (e.g. c7) + Enter");
        XSetForeground(dpy, gc, o->c_band);
        XFillRectangle(dpy, w, gc, lx, ly, (unsigned)o->pill_w, (unsigned)o->pill_h);
        XSetForeground(dpy, gc, o->c_line);
        XDrawRectangle(dpy, w, gc, lx, ly, (unsigned)(o->pill_w - 1), (unsigned)(o->pill_h - 1));
        XSetForeground(dpy, gc, bad ? o->c_bad : o->c_label);
        XDrawString(dpy, w, gc, lx + 8, ly + 4 + o->fs->ascent, l1, (int)strlen(l1));
        XSetForeground(dpy, gc, o->c_label);
        XDrawString(dpy, w, gc, lx + 8, ly + 6 + fh + o->fs->ascent, l2, (int)strlen(l2));
    }
}

static void ov_redraw(Ov *o) {
    int i;
    for (i = 0; i < o->n; i++) ov_draw_pane(o, i);
    XFlush(o->dpy);
}

static void ov_set_err(Ov *o, const char *msg) {
    snprintf(o->err, sizeof(o->err), "%s", msg);
    o->err_until = now_ms() + 1800;
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

/* Keys the labelled grid understands. Resolved to keycodes once (layout-independent by
 * keysym, shift ignored) so the SAME table serves KeyPress events and XQueryKeymap edge
 * polling. `rep` = held-key autorepeat is wanted (arrows, Backspace); everything else is
 * one action per physical press. */
typedef struct { KeyCode kc; GjKey key; char ch; int rep; } KeyEnt;
#define MAX_KEYS 80
static KeyEnt g_keys[MAX_KEYS];
static int g_nkeys;

static void keys_add(Display *d, KeySym ks, GjKey k, char ch, int rep) {
    KeyCode kc = XKeysymToKeycode(d, ks);
    if (!kc || g_nkeys >= MAX_KEYS) return;
    g_keys[g_nkeys].kc = kc; g_keys[g_nkeys].key = k; g_keys[g_nkeys].ch = ch; g_keys[g_nkeys].rep = rep;
    g_nkeys++;
}

static void keys_init(Display *d) {
    int i;
    g_nkeys = 0;
    for (i = 0; i < 26; i++) keys_add(d, XK_a + i, GJ_KEY_CHAR, (char)('a' + i), 0);
    for (i = 0; i < 10; i++) {
        keys_add(d, XK_0 + i, GJ_KEY_CHAR, (char)('0' + i), 0);
        keys_add(d, XK_KP_0 + i, GJ_KEY_CHAR, (char)('0' + i), 0);
    }
    keys_add(d, XK_Return, GJ_KEY_ENTER, 0, 0);
    keys_add(d, XK_KP_Enter, GJ_KEY_ENTER, 0, 0);
    keys_add(d, XK_BackSpace, GJ_KEY_BACKSPACE, 0, 1);
    keys_add(d, XK_Up, GJ_KEY_UP, 0, 1);
    keys_add(d, XK_Down, GJ_KEY_DOWN, 0, 1);
    keys_add(d, XK_Left, GJ_KEY_LEFT, 0, 1);
    keys_add(d, XK_Right, GJ_KEY_RIGHT, 0, 1);
}

static int key_by_code(KeyCode kc) {
    int i;
    for (i = 0; i < g_nkeys; i++) if (g_keys[i].kc == kc) return i;
    return -1;
}

/* Keep the drop-zone hover (File Explorer's green window highlight) in step with the
 * keyboard cursor, exactly as pointer motion does for the mouse. */
static void ov_update_hover(Ov *o) {
    int cx, cy, zp;
    char zd[PATH_BUF];
    if (!o->use_zones) return;
    ov_cell_valid(o, o->gj.row, o->gj.col, &cx, &cy);
    zp = o->kb_active ? pz_hit(o->root, cx, cy, o->skip_dir, zd, sizeof(zd)) : 0;
    if (zp != o->hover_pid) { o->hover_pid = zp; pz_write_hover(o->root, zp, o->place_name); }
}

/* One grid key. Returns 1 when the user asked to place, with the cell centre (screen px)
 * in the cx and cy outputs. The FIRST key of any kind only arms the highlight (at the pointer's cell),
 * so an Enter left over from launching Place can never place by itself. */
static int ov_key(Ov *o, GjKey k, char ch, int *cx, int *cy) {
    char pending[GJ_BUF_CAP];
    GjAction a;
    PDBG("ov_key key=%d ch=%c kb_active=%d jump='%s'", (int)k, ch ? ch : '-', o->kb_active, o->gj.jump);
    o->last_key_ms = now_ms();
    if (!o->kb_active) {
        Window rr, cc; int rx, ry, wx, wy; unsigned int m;
        o->kb_active = 1;
        if (XQueryPointer(o->dpy, DefaultRootWindow(o->dpy), &rr, &cc, &rx, &ry, &wx, &wy, &m)) {
            o->ptr_x = rx; o->ptr_y = ry;
        }
        o->gj.col = cell_at(o->ptr_x); o->gj.row = cell_at(o->ptr_y);
        gj_clamp(&o->gj);
        o->gj.jump[0] = '\0';
        ov_update_hover(o);
        ov_redraw(o);
        if (k == GJ_KEY_ENTER) return 0;
    }
    o->last_key_ms = now_ms();
    snprintf(pending, sizeof(pending), "%s", o->gj.jump);
    a = gj_step(&o->gj, k, ch);
    switch (a) {
    case GJ_ENTER_CELL: {
        char nm[24], msg[64];
        if (!ov_cell_valid(o, o->gj.row, o->gj.col, cx, cy)) {
            ov_cell_name(o, o->gj.row, o->gj.col, nm, sizeof(nm));
            snprintf(msg, sizeof(msg), "cell %s is under the picker", nm);
            ov_set_err(o, msg);
            ov_redraw(o);
            return 0;
        }
        return 1;
    }
    case GJ_NONE:
        if (k == GJ_KEY_ENTER && pending[0]) {
            char msg[64];
            snprintf(msg, sizeof(msg), "no cell '%s'", pending);
            ov_set_err(o, msg);
            ov_redraw(o);
        }
        break;
    case GJ_MOVED: case GJ_JUMPED: case GJ_BUFFER:
        o->err[0] = '\0';
        ov_update_hover(o);
        ov_redraw(o);
        break;
    default: break;
    }
    return 0;
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

    /* Grid geometry: the real desk cell, scaled for this screen (see header). */
    {
        char sdir[PATH_BUF];
        const char *ce = getenv("TP_PLACE_CELL_REF");
        resolve_settings_dir(desktop_root, sdir, sizeof(sdir));
        kps_load_for_tool(sdir, sw, sh, &g_auto, &g_base_cell);
        g_cell_ref = (ce && atoi(ce) > 0) ? atoi(ce) : g_base_cell;
        for (g_cols = 1; g_cols < 4096 && cell_edge(g_cols) < sw; g_cols++) ;
        for (g_rows = 1; g_rows < 4096 && cell_edge(g_rows) < sh; g_rows++) ;
    }

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
    int use_argb = !(getenv("TP_PLACE_NO_ARGB") && getenv("TP_PLACE_NO_ARGB")[0] == '1') &&
                   XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo);
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
    swa.event_mask = ButtonPressMask | KeyPressMask | KeyReleaseMask | ExposureMask | PointerMotionMask;
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.background_pixel = 0; /* ARGB: transparent; default: black, no amber wash */
    unsigned long mask = CWOverrideRedirect | CWEventMask | CWBackPixel | CWBorderPixel | CWColormap;

    Ov ov;
    memset(&ov, 0, sizeof(ov));
    ov.dpy = dpy; ov.sw = sw; ov.sh = sh; ov.use_argb = use_argb;
    if (pw > 0 && ph > 0) { ov.hx = px; ov.hy = py; ov.hw = pw; ov.hh = ph; }
    #define ADD_PANE(_x,_y,_w,_h) do { \
        if ((_w) > 0 && (_h) > 0) { \
            ov.panes[ov.n].x = (_x); ov.panes[ov.n].y = (_y); \
            ov.panes[ov.n].ww = (_w); ov.panes[ov.n].wh = (_h); \
            ov.panes[ov.n].w = XCreateWindow(dpy, root, (_x), (_y), (unsigned)(_w), (unsigned)(_h), 0, \
                                            depth, InputOutput, vis, mask, &swa); \
            ov.n++; \
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

    /* Colours (ARGB pixels are premultiplied; the plain visual needs allocated pixels). */
    if (use_argb) {
        ov.c_line = 0xE0FFCC00UL; ov.c_label = 0xFFFFE066UL; ov.c_ok = 0xFF66FF66UL; ov.c_bad = 0xFFFF5555UL;
        ov.c_band = 0xC0000000UL; ov.c_okfill = 0x50105010UL; ov.c_badfill = 0x50501010UL;
    } else {
        const char *names[5] = { "#ffcc00", "#ffe066", "#66ff66", "#ff5555", "#000000" };
        unsigned long *dst[5] = { &ov.c_line, &ov.c_label, &ov.c_ok, &ov.c_bad, &ov.c_band };
        for (int i = 0; i < 5; i++) {
            XColor col;
            XParseColor(dpy, cmap, names[i], &col);
            XAllocColor(dpy, cmap, &col);
            *dst[i] = col.pixel;
        }
        ov.c_okfill = ov.c_ok; ov.c_badfill = ov.c_bad;
    }
    {
        static const char *fonts[] = { "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1", "fixed", "6x13", "7x13", "9x15" };
        for (int i = 0; i < 5 && !ov.fs; i++) ov.fs = XLoadQueryFont(dpy, fonts[i]);
    }
    if (ov.fs) {
        char wide[16];
        int fh = ov.fs->ascent + ov.fs->descent, ptw;
        const char *hint = "Enter = jump | Enter again = place | Esc = cancel | click = place";
        snprintf(wide, sizeof(wide), "%d", g_rows);
        ov.lab_h = fh + 4;
        ov.lab_w = XTextWidth(ov.fs, wide, (int)strlen(wide)) + 10;
        ptw = XTextWidth(ov.fs, hint, (int)strlen(hint));
        ov.pill_w = ptw + 16;
        ov.pill_h = 2 * fh + 12;
        /* first anchor that is not under the picker hole (labels/pill must stay visible) */
        {
            int cand[3][2] = { { ov.lab_w + 8, ov.lab_h + 8 }, { sw - ov.pill_w - 8, ov.lab_h + 8 }, { ov.lab_w + 8, sh - ov.pill_h - 8 } };
            ov.pill_x = cand[0][0]; ov.pill_y = cand[0][1];
            for (int i = 0; i < 3; i++) {
                int hit = ov.hw > 0 && cand[i][0] < ov.hx + ov.hw && cand[i][0] + ov.pill_w > ov.hx &&
                          cand[i][1] < ov.hy + ov.hh && cand[i][1] + ov.pill_h > ov.hy;
                if (!hit) { ov.pill_x = cand[i][0]; ov.pill_y = cand[i][1]; break; }
            }
        }
    }
    ov.gj.rows = g_rows; ov.gj.cols = g_cols;
    ov.root = desktop_root;
    ov.use_zones = getenv("FE_PLACE_ZONES") && getenv("FE_PLACE_ZONES")[0] == '1';
    ov.skip_dir = getenv("FE_PLACE_SKIP_DIR");
    ov.place_name = getenv("FE_PLACE_NAME");
    keys_init(dpy);
    for (int i = 0; i < ov.n; i++) {
        XMapRaised(dpy, ov.panes[i].w);
        ov.gcs[i] = XCreateGC(dpy, ov.panes[i].w, 0, NULL);
        if (ov.fs) XSetFont(dpy, ov.gcs[i], ov.fs->fid);
    }
    ov_redraw(&ov);
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
    const int use_zones = ov.use_zones;
    const char *skip_dir = ov.skip_dir;
    const char *place_name = ov.place_name;
    char zone_dest[PATH_BUF];
    int zone_pid = 0;
    zone_dest[0] = 0;
    /* Esc is also polled from the server's key state, so it cancels even when
     * another client owns the keyboard grab. An Esc already held when the
     * overlay opens (the keypress that launched it) does not count. The typing
     * keys (letters, digits, Enter, Backspace, arrows) use the same two routes:
     * KeyPress events when the grab succeeded, keymap-edge polling when it did
     * not. down[] records "physically down, already acted on" per keycode so an
     * event and a poll edge for the same press act once, and keys already held
     * at launch (the Enter that opened Place) are never treated as new. */
    const KeyCode esc_kc = XKeysymToKeycode(dpy, XK_Escape);
    char keys[32], down[256];
    int esc_prev = 0, done = 0;
    memset(down, 0, sizeof(down));
    XQueryKeymap(dpy, keys);
    if (esc_kc) esc_prev = (keys[esc_kc >> 3] >> (esc_kc & 7)) & 1;
    for (int i = 0; i < g_nkeys; i++) {
        KeyCode kc = g_keys[i].kc;
        down[kc] = (keys[kc >> 3] >> (kc & 7)) & 1;
        if (down[kc]) PDBG("key kc=%d already held at launch (ignored until released)", (int)kc);
    }
    while (!done) {
        XEvent xev;
        int place_req = 0, pcx = 0, pcy = 0;
        if (ov.err[0] && now_ms() >= ov.err_until) { ov.err[0] = '\0'; ov_redraw(&ov); }
        if (!XPending(dpy)) {
            XQueryKeymap(dpy, keys);
            if (esc_kc) {
                int esc_now = (keys[esc_kc >> 3] >> (esc_kc & 7)) & 1;
                if (esc_now && !esc_prev) { cancelled = 1; break; }
                esc_prev = esc_now;
            }
            for (int i = 0; i < g_nkeys && !place_req; i++) {
                KeyCode kc = g_keys[i].kc;
                int now = (keys[kc >> 3] >> (kc & 7)) & 1;
                if (now && !down[kc]) {
                    down[kc] = 1;
                    PDBG("poll edge kc=%d", (int)kc);
                    place_req = ov_key(&ov, g_keys[i].key, g_keys[i].ch, &pcx, &pcy);
                } else if (!now && down[kc]) {
                    down[kc] = 0;
                }
            }
            if (!place_req) {
                int cfd = ConnectionNumber(dpy);
                fd_set rf;
                struct timeval tv = { 0, 20000 };
                FD_ZERO(&rf);
                FD_SET(cfd, &rf);
                select(cfd + 1, &rf, NULL, NULL, &tv);
                continue;
            }
        } else {
            XNextEvent(dpy, &xev);
            if (xev.type == MotionNotify) {
                int mx = xev.xmotion.x_root, my = xev.xmotion.y_root, moved = 1;
                PDBG("Motion %d,%d ptr=%d,%d kb=%d", mx, my, ov.ptr_x, ov.ptr_y, ov.kb_active);
                if (ov.kb_active) {
                    /* the pointer moving away hands control back to the mouse. A motion event
                     * within 300 ms of a key is ignored: it is a late event from before the
                     * keyboard took over (seen once as an intermittent mode drop), and a real
                     * mouse move keeps sending events, so it still takes effect a moment later. */
                    if (abs(mx - ov.ptr_x) + abs(my - ov.ptr_y) < 6 || now_ms() - ov.last_key_ms < 300) moved = 0;
                    else { ov.kb_active = 0; ov.gj.jump[0] = '\0'; ov.err[0] = '\0'; ov_update_hover(&ov); ov_redraw(&ov); }
                }
                if (moved && use_zones) {
                    char zd[PATH_BUF];
                    int zp = pz_hit(desktop_root, mx, my, skip_dir, zd, sizeof(zd));
                    if (zp != ov.hover_pid) { ov.hover_pid = zp; pz_write_hover(desktop_root, zp, place_name); }
                }
            } else if (xev.type == Expose) {
                for (int i = 0; i < ov.n; i++)
                    if (ov.panes[i].w == xev.xexpose.window) ov_draw_pane(&ov, i);
            } else if (xev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&xev.xkey, 0);
                if (ks == XK_Escape) { cancelled = 1; break; }
                int ti = key_by_code(xev.xkey.keycode);
                PDBG("KeyPress kc=%d ti=%d down=%d", (int)xev.xkey.keycode, ti, (int)down[xev.xkey.keycode]);
                if (ti >= 0 && !down[xev.xkey.keycode]) {
                    down[xev.xkey.keycode] = 1;
                    place_req = ov_key(&ov, g_keys[ti].key, g_keys[ti].ch, &pcx, &pcy);
                }
            } else if (xev.type == KeyRelease) {
                KeyCode kc = xev.xkey.keycode;
                int ti = key_by_code(kc), autorep = 0;
                PDBG("KeyRelease kc=%d ti=%d", (int)kc, ti);
                if (XEventsQueued(dpy, QueuedAfterReading) > 0) {
                    XEvent nx;
                    XPeekEvent(dpy, &nx);
                    if (nx.type == KeyPress && nx.xkey.keycode == kc && nx.xkey.time == xev.xkey.time) {
                        XNextEvent(dpy, &nx); /* autorepeat release+press pair: not a new physical press */
                        autorep = 1;
                        if (ti >= 0 && g_keys[ti].rep)
                            place_req = ov_key(&ov, g_keys[ti].key, g_keys[ti].ch, &pcx, &pcy);
                    }
                }
                if (!autorep) down[kc] = 0;
            } else if (xev.type == ButtonPress) {
                click_x = xev.xbutton.x_root;
                click_y = xev.xbutton.y_root;
                if (use_zones)
                    zone_pid = pz_hit(desktop_root, click_x, click_y, skip_dir, zone_dest, sizeof(zone_dest));
                break;
            }
        }
        if (place_req) {
            /* keyboard place: the cell centre stands in for a mouse click there */
            click_x = pcx;
            click_y = pcy;
            if (use_zones)
                zone_pid = pz_hit(desktop_root, click_x, click_y, skip_dir, zone_dest, sizeof(zone_dest));
            done = 1;
        }
    }
    if (use_zones && ov.hover_pid) pz_write_hover(desktop_root, 0, "");
    XUngrabKeyboard(dpy, CurrentTime);
    for (int i = 0; i < ov.n; i++) {
        XFreeGC(dpy, ov.gcs[i]);
        XDestroyWindow(dpy, ov.panes[i].w);
    }
    if (ov.fs) XFreeFont(dpy, ov.fs);
    XCloseDisplay(dpy);

    /* Real ledger-write, same convention khtpm_core_render.c's
     * own dbhq_rmmv_handle_desktop_click() uses - tp_place_desktop_
     * rmmv.+x reads its own click position straight from this file
     * (no TP_INITIAL_X/Y env vars anymore, see its own header comment
     * on why - a real, caller-agnostic op), so this write IS the real
     * hand-off, not just a debug trail. */
    if (cancelled) return 0;
    /* click_x/click_y are SCREEN px (hit-tests above used them as such). Every
     * consumer below (RMMV_CLICK ledger row -> tp_place_desktop_rmmv.+x ->
     * desktop_pos.txt, and the File Explorer click file) records a saved entity
     * position, which is REFERENCE px (khtpm_ui_scale.c; identity on the
     * reference screen). */
    click_x = kps_screen_to_ref(click_x, g_base_cell, g_auto);
    click_y = kps_screen_to_ref(click_y, g_base_cell, g_auto);
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
