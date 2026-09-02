#define _POSIX_C_SOURCE 200809L
/* khtpm_generic_host.c — a real, minimal, working implementation of the
 * generic-capabilities pattern documented in
 * khtpm-core/GENERIC-CAPABILITIES-PATTERN.md. Read that doc first.
 *
 * This is a genuine, runnable demo of the REAL house architecture: one
 * shared, generic renderer that knows NOTHING about "network browser"
 * specifically. It only knows how to:
 *   1. Parse a real .chtpm (khtpm_chtpm_loader.c).
 *   2. Launch a real child <module> process (network_browser_manager.+x).
 *   3. Re-parse the SAME .chtpm file whenever the manager rewrites it
 *      (real, generic capability #1 - reparse_chtpm_if_changed()).
 *   4. Draw whatever generic tags it finds (window/page/sidebar/panel/
 *      scrolllist/text/item/cli_io).
 *   5. Dispatch real shell commands on click/Enter (action="...").
 *   6. Support a real, generic armed text-input field (<cli_io>, real
 *      generic capability #2) - click or Enter/digit to ARM, type,
 *      Backspace edits, Escape cancels, Enter COMMITS.
 *
 * network_browser_manager.c (the real business logic - fetches a URL,
 * does simple HTML extraction, republishes a real .chtpm projection
 * every tick) is completely unmodified from the real house version.
 * This file is the "generic renderer" side, trimmed down from the real
 * house's own ~18,000-line khtpm_core_render.c (which also serves 7
 * OTHER, unrelated apps - too much to hand you as a first example) to
 * just the real, generic mechanism this one app actually needs. Every
 * real behavior here is ported faithfully from that file, not
 * reinvented - see this file's own comments for exactly where.
 *
 * Usage: khtpm_generic_host.+x <house_root> <chtpm_path>
 * (same real argv shape every khtpm app in the house uses)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c"
#include "khtpm_chtpm_loader.c"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define PATH_BUF 4096
#define WIN_W 700
#define WIN_H 520
#define ROW_H 22
#define SIDEBAR_W 180

static char g_house_root[PATH_BUF];
static char g_chtpm_path[PATH_BUF];
static char g_package_dir[PATH_BUF];
static struct timespec g_chtpm_mtime;
static Elem *g_window;
static int g_quit = 0;
static int g_focus_nav = 1;
static int g_n_nav = 0;
static Elem *g_nav[MAX_ELEMS];
static Elem *g_armed = NULL; /* the one cli_io field currently accepting keystrokes, or NULL */
static pid_t g_module_pid = -1;

static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw;
static Colormap cmap;
static XftFont *font;
static int screen;

/* ---------- generic capability: real .chtpm re-parse on file change ----------
 * Ported from khtpm_core_render.c's own reparse_chtpm_if_changed() -
 * same real mtime-gated shape, same real "drop transient state tied to
 * the old tree" reasoning (an armed field pointing into a just-freed
 * tree is a real dangling-pointer bug, not a hypothetical one - this
 * house hit it live). */
static int reparse_if_changed(void) {
    struct stat st;
    if (stat(g_chtpm_path, &st) != 0) return 0;
    if (st.st_mtim.tv_sec == g_chtpm_mtime.tv_sec && st.st_mtim.tv_nsec == g_chtpm_mtime.tv_nsec) return 0;
    g_chtpm_mtime = st.st_mtim;
    if (g_armed) { XUngrabKeyboard(dpy, CurrentTime); g_armed = NULL; }
    g_n_elems = 0;
    Elem *new_window = parse_chtpm(g_chtpm_path);
    if (!new_window) return 0;
    g_window = new_window;
    return 1;
}

/* ---------- generic capability: launch a real <module> child process ----------
 * Ported verbatim from khtpm_core_render.c's own launch_module() - the
 * renderer has zero knowledge of what the module IS (network_browser_
 * manager.+x here), only that <module src="..."/> names a real
 * executable to fork+exec once, passing house_root/package_dir as its
 * own real argv[1]/argv[2]. */
static pid_t launch_module(const char *src) {
    if (!src || !src[0]) return -1;
    char full_path[PATH_BUF];
    if (src[0] == '/') snprintf(full_path, sizeof(full_path), "%s", src);
    else snprintf(full_path, sizeof(full_path), "%s/%s", g_house_root, src);
    pid_t pid = fork();
    if (pid == 0) {
        execl(full_path, full_path, g_house_root, g_package_dir, (char *)NULL);
        _exit(1);
    } else if (pid < 0) {
        fprintf(stderr, "khtpm_generic_host: launch_module: fork failed for %s\n", full_path);
    }
    return pid;
}

/* ---------- generic dispatch: action="<real shell command>" ----------
 * Same real shape as khtpm_core_render.c's own dispatch()/
 * default_cli_io_run_action(): the action string IS a real, directly-
 * executable command; house_root/package_dir are appended as its own
 * trailing argv (a plain shell command that ignores extra args, like
 * `xdg-open`, works fine either way - a real script that wants them
 * reads $1/$2, same convention every house METHOD/action= line uses).
 * A cli_io's own commit additionally appends the live typed value. */
static void dispatch(const char *action, const char *typed_value) {
    if (strcmp(action, "CLOSE") == 0 || strcmp(action, "void") == 0) { g_quit = 1; return; }
    char cmd[PATH_BUF * 3];
    if (typed_value)
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' '%s' >/dev/null 2>&1 &", action, g_package_dir, g_house_root, typed_value);
    else
        snprintf(cmd, sizeof(cmd), "%s '%s' '%s' >/dev/null 2>&1 &", action, g_package_dir, g_house_root);
    int rc = system(cmd);
    (void)rc;
}

/* ---------- real, generic layout pass ----------
 * Deliberately simple (fixed-height rows, no real flex CSS) - this
 * house's own real flex engine (css_layout_pass() in khtpm_render_
 * core.c) is real but explicitly flagged as not yet proven on a live
 * app; a small teaching example is the wrong place to be its first
 * test. A sidebar+panel window (the shape this demo's own real .chtpm
 * uses) is simple enough that explicit positions are honest and clear.
 * Real per-app apps in the house that need this same shape often do
 * exactly this - see network-browser-hq.css's own header comment
 * ("Geometry stays in a small C layout pass"). */
static void layout_children(Elem *parent, int x, int y, int w) {
    int cy = y;
    for (int i = 0; i < parent->n_children; i++) {
        Elem *c = parent->children[i];
        if (strcmp(c->tag, "scrolllist") == 0) {
            layout_children(c, x, cy, w);
            for (int j = 0; j < c->n_children; j++) cy = c->children[j]->y + c->children[j]->h;
            continue;
        }
        c->x = x + 4;
        c->y = cy;
        c->w = w - 8;
        c->h = (strcmp(c->tag, "cli_io") == 0) ? ROW_H + 6 : ROW_H;
        cy += c->h;
    }
}

static void layout_pass(void) {
    Elem *page = find_by_tag(g_window, "page");
    Elem *root = page ? page : g_window;
    Elem *sidebar = find_by_tag(root, "sidebar");
    Elem *panel = find_by_tag(root, "panel");
    if (sidebar && panel) {
        layout_children(sidebar, 24, 24, SIDEBAR_W - 24);
        layout_children(panel, SIDEBAR_W + 8, 24, WIN_W - SIDEBAR_W - 16);
    } else {
        layout_children(root, 8, 24, WIN_W - 16);
    }
    /* Real, sequential nav_index assignment - document order, any real
     * clickable tag (item/cli_io). Matches wraith-alpha's own
     * "1-based sequential number assigned every redraw" convention
     * (Elem.nav_index's own field comment in khtpm_render_core.c). */
    g_n_nav = 0;
    for (int i = 0; i < g_n_elems; i++) {
        Elem *e = &g_pool[i];
        e->nav_index = 0;
        if (strcmp(e->tag, "item") == 0 || strcmp(e->tag, "cli_io") == 0) {
            e->nav_index = ++g_n_nav;
            if (g_n_nav <= MAX_ELEMS) g_nav[g_n_nav - 1] = e;
        }
    }
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

/* ---------- generic capability: real armed cli_io input ----------
 * Ported from khtpm_core_render.c's own activate_focused()/
 * default_cli_io_handle_key() - real click-or-Enter-to-arm, type,
 * Backspace, Escape cancels, Enter commits. The one, real, non-obvious
 * fix this needs (found live in the real house, see GENERIC-
 * CAPABILITIES-PATTERN.md): a real XGrabKeyboard while armed, or
 * typing silently breaks the instant the pointer leaves the window
 * under a focus-follows-mouse WM. */
static void arm(Elem *e) {
    g_armed = e;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) break;
        XSync(dpy, False);
        usleep(5000);
    }
}

static void activate(Elem *e) {
    if (!e) return;
    if (strcmp(e->tag, "cli_io") == 0) { arm(e); return; }
    if (e->onclick[0]) dispatch(e->onclick, NULL);
}

static void handle_key(KeySym ks, char ch) {
    if (g_armed) {
        if (ks == XK_Return || ks == XK_KP_Enter) {
            dispatch(g_armed->onclick, g_armed->input_buffer);
            g_armed->input_buffer[0] = '\0';
            XUngrabKeyboard(dpy, CurrentTime);
            g_armed = NULL;
            return;
        }
        if (ks == XK_Escape) { XUngrabKeyboard(dpy, CurrentTime); g_armed = NULL; return; }
        if (ks == XK_BackSpace) {
            size_t n = strlen(g_armed->input_buffer);
            if (n > 0) g_armed->input_buffer[n - 1] = '\0';
            return;
        }
        if (ch >= 32 && ch < 127) {
            size_t n = strlen(g_armed->input_buffer);
            if (n + 1 < sizeof(g_armed->input_buffer)) { g_armed->input_buffer[n] = ch; g_armed->input_buffer[n + 1] = '\0'; }
        }
        return;
    }
    if (ks == XK_Escape) { g_quit = 1; return; }
    if (ks == XK_Return || ks == XK_KP_Enter) { if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) activate(g_nav[g_focus_nav - 1]); return; }
    if (ks == XK_Down) { if (g_n_nav > 0) g_focus_nav = g_focus_nav < g_n_nav ? g_focus_nav + 1 : 1; return; }
    if (ks == XK_Up) { if (g_n_nav > 0) g_focus_nav = g_focus_nav > 1 ? g_focus_nav - 1 : g_n_nav; return; }
    if (ch >= '1' && ch <= '9') { int d = ch - '0'; if (d <= g_n_nav) g_focus_nav = d; }
}

static void handle_click(int mx, int my) {
    Elem *e = hit_test(g_window, mx, my);
    if (!e) return;
    if (e->nav_index > 0) g_focus_nav = e->nav_index;
    activate(e);
}

/* ---------- drawing (a real, minimal thin renderer over the shared tree) ----------
 * The real house's own draw_elem() (khtpm_draw_core.c) is more capable
 * (sprites, emoji, DPI scaling, real CSS colors) but drags in globals
 * this small demo doesn't have real infrastructure for. This is the
 * SAME real principle - one parsed tree, walked once, drawn generically
 * by tag - just without those extras. */
static unsigned long col(const char *hex) {
    XColor c; XParseColor(dpy, cmap, hex, &c); XAllocColor(dpy, cmap, &c); return c.pixel;
}
static XftColor xcol(const char *hex) {
    XftColor xc; XRenderColor rc = {0,0,0,0xffff};
    unsigned r,g,b; sscanf(hex + 1, "%02x%02x%02x", &r,&g,&b);
    rc.red=(unsigned short)(r*257); rc.green=(unsigned short)(g*257); rc.blue=(unsigned short)(b*257);
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}
static void draw_one(Elem *e) {
    int on = (e->nav_index > 0 && e->nav_index == g_focus_nav);
    int armed = (e == g_armed);
    if (strcmp(e->tag, "cli_io") == 0) {
        XSetForeground(dpy, gc, col(armed ? "#333355" : "#2a2a2a"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, (unsigned)e->w, (unsigned)e->h);
        XSetForeground(dpy, gc, col(armed ? "#ffdd55" : on ? "#88aaff" : "#555555"));
        XDrawRectangle(dpy, buf, gc, e->x, e->y, (unsigned)e->w - 1, (unsigned)e->h - 1);
        char shown[512];
        const char *bracket = armed ? "[^]" : on ? "[>]" : "[ ]";
        snprintf(shown, sizeof(shown), "%s%s%s%s", bracket, e->label, e->input_buffer, armed ? "_" : "");
        XftColor c = xcol("#ffffff");
        XftDrawStringUtf8(xftdraw, &c, font, e->x + 6, e->y + e->h - 8, (const FcChar8 *)shown, (int)strlen(shown));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
    } else if (strcmp(e->tag, "item") == 0) {
        char row[600];
        snprintf(row, sizeof(row), "%s%d. %s", on ? "[>]" : "[ ]", e->nav_index, e->label);
        XftColor c = xcol(on ? "#ffdd55" : "#66aaff");
        XftDrawStringUtf8(xftdraw, &c, font, e->x, e->y + e->h - 6, (const FcChar8 *)row, (int)strlen(row));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
    } else if (strcmp(e->tag, "text") == 0) {
        XftColor c = xcol(strcmp(e->id, "status") == 0 ? "#88cc88" :
                           (e->n_classes > 0 && strcmp(e->classes[0], "page-title") == 0) ? "#ffdd55" : "#cccccc");
        XftDrawStringUtf8(xftdraw, &c, font, e->x, e->y + e->h - 6, (const FcChar8 *)e->label, (int)strlen(e->label));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
    }
}
static void draw_tree(Elem *e) {
    if (!e) return;
    if (e != g_window) draw_one(e);
    for (int i = 0; i < e->n_children; i++) draw_tree(e->children[i]);
}

static void redraw(void) {
    layout_pass();
    XSetForeground(dpy, gc, col("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, WIN_W, WIN_H);
    /* real sidebar/panel separator, cheap visual polish only */
    XSetForeground(dpy, gc, col("#333333"));
    XDrawLine(dpy, buf, gc, SIDEBAR_W, 0, SIDEBAR_W, WIN_H);
    draw_tree(g_window);
    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, WIN_W, WIN_H, AllPlanes, ZPixmap);
    if (frame) { XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, WIN_W, WIN_H); XDestroyImage(frame); }
    XFlush(dpy);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <chtpm_path>\n", argv[0]); return 1; }
    /* REAL BUG FOUND LIVE while building this demo: do NOT set SIGCHLD
     * to SIG_IGN here. Signal disposition (unlike handler function
     * pointers) survives exec() - the module child (network_browser_
     * manager.+x) inherits it, and THAT process's own real curl fetch
     * goes through system(), which internally forks+waitpid()s for the
     * curl subprocess. With SIGCHLD ignored, curl's child gets reaped
     * automatically by the kernel before system() can waitpid() it,
     * so system() returns -1 with no real exit status - the manager's
     * own real fetch silently "fails" with rc=-1 for every URL, no
     * matter how healthy curl and the network actually are. Confirmed
     * by direct comparison: the exact same curl command succeeds when
     * run directly, only fails as this process's own real child. Real
     * fix: leave SIGCHLD at its real default (SIG_DFL) - a harmless
     * real zombie briefly exists for the module child between its own
     * exit and this process's shutdown-time waitpid() call below,
     * which is fine for a real app this short-lived. */
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_chtpm_path, sizeof(g_chtpm_path), "%s", argv[2]);
    { char tmp[PATH_BUF]; snprintf(tmp, sizeof(tmp), "%s", g_chtpm_path); char *slash = strrchr(tmp, '/'); if (slash) *slash = '\0'; snprintf(g_package_dir, sizeof(g_package_dir), "%s", tmp); }

    g_window = parse_chtpm(g_chtpm_path);
    if (!g_window) { fprintf(stderr, "khtpm_generic_host: failed to parse %s\n", g_chtpm_path); return 1; }
    { struct stat st; if (stat(g_chtpm_path, &st) == 0) g_chtpm_mtime = st.st_mtim; }

    /* Real, generic module launch - find any <module src="..."/> in the
     * freshly-parsed tree and start it exactly once. Zero per-app
     * knowledge: this code has no idea "network_browser_manager" exists. */
    Elem *module = find_by_tag(g_window, "module");
    if (module && module->label[0]) g_module_pid = launch_module(module->label);

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "khtpm_generic_host: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);
    font = XftFontOpenName(dpy, screen, "DejaVu Sans Mono:pixelsize=13");
    XSetWindowAttributes swa;
    swa.background_pixel = col("#1c1c1c");
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), 200, 120, WIN_W, WIN_H, 0,
                         CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &swa);
    XStoreName(dpy, win, "khtpm_generic_host demo");
    XMapWindow(dpy, win);
    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, WIN_W, WIN_H, (unsigned)DefaultDepth(dpy, screen));
    xftdraw = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    int xfd = ConnectionNumber(dpy);
    while (!g_quit) {
        fd_set fds; FD_ZERO(&fds); FD_SET(xfd, &fds);
        struct timeval tv = {0, 150000};
        select(xfd + 1, &fds, NULL, NULL, &tv);
        int need_redraw = reparse_if_changed(); /* cheap - one stat() per idle tick */
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) need_redraw = 1;
            else if (ev.type == ButtonPress) { handle_click(ev.xbutton.x, ev.xbutton.y); need_redraw = 1; }
            else if (ev.type == KeyPress) {
                char kbuf[8]; KeySym ks;
                XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                handle_key(ks, kbuf[0]);
                need_redraw = 1;
            }
        }
        if (need_redraw) redraw();
    }
    if (g_module_pid > 0) { kill(g_module_pid, SIGTERM); waitpid(g_module_pid, NULL, WNOHANG); }
    XCloseDisplay(dpy);
    return 0;
}
