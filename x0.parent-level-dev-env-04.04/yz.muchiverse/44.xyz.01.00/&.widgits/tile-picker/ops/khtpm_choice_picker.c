#define _POSIX_C_SOURCE 200809L /* CLOCK_MONOTONIC + snprintf (199309L hid snprintf's prototype under clang/macOS) */
/* khtpm_choice_picker.c — real khtpm-based "Show Choices" picker window
 * (2026-08-16, direct instruction: "its very old lets fix it to use
 * khtpm. it should still show books and random verse"). Replaces the
 * old tp_picker_window.c (GLX/OpenGL-based, architecturally unlike
 * every other khtpm app in this house) which was silently deployed
 * under the khtpm_show_choices.+x binary name despite khtpm_show_
 * choices.c's own real, already-written design never actually being
 * built - confirmed live: tp_picker_window.c's window opened a real X
 * connection and blocked in a real event loop with zero error, but
 * never produced a mapped, visible window (xwininfo showed nothing,
 * even searching its own exact "tile-picker" window name) - a real,
 * separate, pre-existing GLX bug, not something this session's
 * menu.chtpm work broke.
 *
 * Real design: this binary IS the picker (no relay-into-another-
 * process indirection - khtpm_show_choices.c's own SHOW_PAGE relay
 * design predates this session's real khtpm_core_render.c work;
 * simpler and lower-risk to have khtpm_show_choices.c exec this
 * directly than to also touch tp_desktop_window_rgb.c's own live
 * SHOW_PAGE relay handler). Reuses the exact same real shared Elem
 * model (&.widgits/_shared-lib/khtpm_render_core.c, 7th real consumer)
 * and every phantom-click/focus/PPosition fix proven this session on
 * khtpm_core_render.c - this is the SAME real engine, just
 * reading the choices_file's own real flat "OBJECT | label=.. |
 * action=.." format directly (no .chtpm authoring step needed - the
 * caller, e.g. book-stack's dispatch.sh, already generates/owns this
 * file's real content) and, on pick, WRITING the chosen action token
 * to a result file instead of dispatching a shell command - the real
 * contract khtpm_show_choices.c's own polling loop already expects.
 *
 * Usage: khtpm_choice_picker.+x <choices_file> <result_path> [x] [y]
 */
/* macOS leg 2026-08-22: libc headers must precede the .c-include below
 * (that file's own declaration order left snprintf undeclared at this
 * unit's first use under clang's stricter implicit-function rules). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp() - real, needed for the synthesized-Cancel check below */
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include "khtpm_css_parser.h" /* Elem's CssStyle field type - not otherwise used here, no runtime CSS needed for this flat single-page picker */
#include "khtpm_render_core.c" /* real .c, not a header - see that file's own comment */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define PATH_BUF 4352
#define MAX_ELEMS 64
#define CHROME_H 24
#define ROW_H 24

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
static char g_choices_file[PATH_BUF];
static char g_result_path[PATH_BUF];
static Elem *elem_new(void) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    return e;
}

/* Real flat "OBJECT | label=.. | action=.." parser - the choices_file's
 * own real format (see khtpm_show_choices.c's own header comment: "a
 * real, flat OBJECT|label=..|action=.. list, no PAGE header needed -
 * always exactly one page"). Not XML/.chtpm - this file's content is
 * generated fresh per-call by the caller, a markup-authoring round trip
 * would be real but pointless overhead for a format this simple. */
static Elem *g_items[MAX_ELEMS];
static int g_n_items = 0;

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = '\0';
}

static int load_choices(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (strncmp(line, "OBJECT", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        char *label_kv = strstr(p, "label=");
        char *action_kv = strstr(p, "action=");
        if (!label_kv || !action_kv) continue;
        Elem *e = elem_new();
        if (!e) break;
        char *label_start = label_kv + 6;
        char *label_end = strchr(label_start, '|');
        size_t llen = label_end ? (size_t)(label_end - label_start) : strlen(label_start);
        while (llen && label_start[llen - 1] == ' ') llen--;
        if (llen >= sizeof(e->label)) llen = sizeof(e->label) - 1;
        memcpy(e->label, label_start, llen);
        e->label[llen] = '\0';

        char *action_start = action_kv + 7;
        while (*action_start == ' ') action_start++;
        snprintf(e->onclick, sizeof(e->onclick), "%s", action_start);

        if (g_n_items < MAX_ELEMS) g_items[g_n_items++] = e;
    }
    fclose(f);

    /* REAL FIX 2026-08-28, direct live report ("show choices... is
     * supposed to close when clicked" - it wasn't reliably, real root
     * cause not yet pinned down; user's own pragmatic call: "dont
     * worry about making it auto close. cant u just add cancel
     * button?"). Real, guaranteed-safe escape hatch: if the caller's
     * choices_file didn't already include its own real Cancel row,
     * synthesize one - same pattern tp_desktop_window_rgb.c's own
     * dispatch already uses for context menus with no real Cancel
     * (see its own "if (!has_cancel...)" block). Empty onclick is the
     * real sentinel activate_focused() below checks for "cancel, don't
     * write a result" - khtpm_show_choices.c's own polling loop
     * already treats a missing/empty result file as a real, legitimate
     * cancel ("no pick made (cancelled or timed out)"), so this needs
     * no new contract, just uses what the caller already handles. */
    {
        int has_cancel = 0;
        for (int i = 0; i < g_n_items; i++) {
            if (strcasecmp(g_items[i]->label, "Cancel") == 0) { has_cancel = 1; break; }
        }
        if (!has_cancel && g_n_items < MAX_ELEMS) {
            Elem *e = elem_new();
            if (e) {
                snprintf(e->label, sizeof(e->label), "Cancel");
                e->onclick[0] = '\0';
                g_items[g_n_items++] = e;
            }
        }
    }
    return g_n_items > 0;
}

/* ---------- X11/Xft (same real proven shape as khtpm_entity_menu_
 * render.c - override_redirect + explicit focus grab, stale-event
 * drain, time-based phantom-click debounce) ---------- */
static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Colormap cmap;
static XftFont *font_ui;
static int screen;
static int g_win_x = 400, g_win_y = 300;
static int g_win_w = 320, g_win_h = 160;
static int g_quit = 0;
static struct timespec g_map_time;
#define PHANTOM_CLICK_GUARD_MS 150
static int g_focus_nav = 1;

static unsigned long alloc_pixel(const char *spec) {
    XColor c;
    XParseColor(dpy, cmap, spec, &c);
    XAllocColor(dpy, cmap, &c);
    return c.pixel;
}
static XftColor xft_color(const char *spec) {
    XftColor xc; XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b; sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}

static void assign_layout(void) {
    int y = CHROME_H;
    for (int i = 0; i < g_n_items; i++) {
        g_items[i]->x = 0; g_items[i]->y = y; g_items[i]->w = g_win_w; g_items[i]->h = ROW_H;
        g_items[i]->nav_index = i + 1;
        y += ROW_H;
    }
    g_win_h = y + 8;
}

static void redraw(void) {
    XSetForeground(dpy, gc, alloc_pixel("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);
    XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);

    XftColor title_col = xft_color("#eeeeee");
    const char *title = "Show Choices";
    XftDrawStringUtf8(xftdraw_buf, &title_col, font_ui, 8, 16, (const FcChar8 *)title, (int)strlen(title));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &title_col);

    for (int i = 0; i < g_n_items; i++) {
        Elem *it = g_items[i];
        int on = (it->nav_index == g_focus_nav);
        if (on) {
            XSetForeground(dpy, gc, alloc_pixel("#333333"));
            XFillRectangle(dpy, buf, gc, it->x, it->y, (unsigned)it->w, (unsigned)it->h);
        }
        XftColor row_col = xft_color(on ? "#ffdd55" : "#cccccc");
        char row[300];
        snprintf(row, sizeof(row), "[%s]%d. %s", on ? ">" : " ", it->nav_index, it->label);
        XftDrawStringUtf8(xftdraw_buf, &row_col, font_ui, 8, it->y + 16, (const FcChar8 *)row, (int)strlen(row));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &row_col);
    }

    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) { XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h); XDestroyImage(frame); }
    XFlush(dpy);
}

/* Real contract khtpm_show_choices.c's own polling loop expects: write
 * the picked action token to result_path, then exit - no shell
 * dispatch (this binary IS the picker, not a context-menu dispatcher,
 * even though it shares the same real Elem engine as khtpm_entity_
 * menu_render.c). */
static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_items) return;
    Elem *it = g_items[g_focus_nav - 1];
    /* Real Cancel sentinel (see load_choices()'s own synthesized-Cancel
     * comment) - empty onclick means "just close, no real pick was
     * made." Don't write result_path at all; khtpm_show_choices.c's
     * own polling loop already treats a missing result file as a real
     * cancel, not an error. */
    if (it->onclick[0]) {
        FILE *f = fopen(g_result_path, "w");
        if (f) { fprintf(f, "%s\n", it->onclick); fclose(f); }
    }
    g_quit = 1;
}

static void handle_key(KeySym ks, char ch) {
    if (ks == XK_Return || ks == XK_KP_Enter) { activate_focused(); return; }
    if (ks == XK_Escape) { g_quit = 1; return; }
    if (ks == XK_Up) { if (g_focus_nav > 1) g_focus_nav--; return; }
    if (ks == XK_Down) { if (g_focus_nav < g_n_items) g_focus_nav++; return; }
    if (ch >= '1' && ch <= '9') { int d = ch - '0'; if (d <= g_n_items) g_focus_nav = d; return; }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <choices_file> <result_path> [x] [y]\n", argv[0]); return 1; }
    snprintf(g_choices_file, sizeof(g_choices_file), "%s", argv[1]);
    snprintf(g_result_path, sizeof(g_result_path), "%s", argv[2]);
    if (argc >= 5) { g_win_x = atoi(argv[3]); g_win_y = atoi(argv[4]); }

    if (!load_choices(g_choices_file)) {
        fprintf(stderr, "khtpm_choice_picker: no choices loaded from %s\n", g_choices_file);
        return 1;
    }
    assign_layout();

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "khtpm_choice_picker: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);

    /* macOS leg (2026-08-22): a saved desktop_pos.txt can carry an x/y
     * past this display's right/bottom edge (live: Linux grid parked
     * book-stack at x=2320 on a 1680px screen - window mapped fully
     * off-screen, unclickable). Clamp against the ACTUAL screen here so
     * every caller gets an on-screen picker regardless of what it
     * passes in. */
    {
        int scr_w = DisplayWidth(dpy, screen);
        int scr_h = DisplayHeight(dpy, screen);
        /* g_win_w/g_win_h are final here - assign_layout() ran above. */
        if (g_win_x + (int)g_win_w > scr_w - 8) g_win_x = scr_w - (int)g_win_w - 8;
        if (g_win_y + (int)g_win_h > scr_h - 8) g_win_y = scr_h - (int)g_win_h - 8;
        if (g_win_x < 8) g_win_x = 8;
        if (g_win_y < 8) g_win_y = 8;
    }

    font_ui = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=12");

    XSetWindowAttributes swa;
    swa.background_pixel = alloc_pixel("#1c1c1c");
    swa.override_redirect = True; /* real popup - see khtpm_core_render.c's own comment on why */
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h, 0,
                         CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWOverrideRedirect | CWEventMask, &swa);
    XSizeHints *shints = XAllocSizeHints();
    if (shints) { shints->flags = PPosition; shints->x = g_win_x; shints->y = g_win_y; XSetWMNormalHints(dpy, win, shints); XFree(shints); }

    XMapRaised(dpy, win);
    XSync(dpy, False);
    XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
    clock_gettime(CLOCK_MONOTONIC, &g_map_time);
    {
        XEvent stale_ev;
        while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { /* discard */ }
    }

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    redraw();

    while (!g_quit) {
        fd_set fds; FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        struct timeval tv = { 0, 150000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) redraw();
            else if (ev.type == ButtonPress) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long ms_since_map = (now.tv_sec - g_map_time.tv_sec) * 1000L
                                   + (now.tv_nsec - g_map_time.tv_nsec) / 1000000L;
                if (ms_since_map < PHANTOM_CLICK_GUARD_MS) continue;
                for (int i = 0; i < g_n_items; i++) {
                    Elem *it = g_items[i];
                    if (ev.xbutton.x >= it->x && ev.xbutton.x < it->x + it->w &&
                        ev.xbutton.y >= it->y && ev.xbutton.y < it->y + it->h) {
                        g_focus_nav = it->nav_index;
                        activate_focused();
                        break;
                    }
                }
                if (!g_quit) redraw();
            } else if (ev.type == KeyPress) {
                char buf8[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf8, sizeof(buf8) - 1, &ks, NULL);
                buf8[n > 0 ? n : 0] = '\0';
                handle_key(ks, buf8[0]);
                if (!g_quit) redraw();
            }
        }
    }

    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
