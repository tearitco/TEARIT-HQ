#define _POSIX_C_SOURCE 200809L
/* network_browser_render.c - real khtpm-based X11 window for the
 * "network browser" HQ app, CENTROID_GOLD_STD.md's first real proof
 * case (2026-08-31). Shares the real Elem model (&.widgits/_shared-lib/
 * khtpm_render_core.c) the same way khtpm_choice_picker.c does - its
 * own header comment explains why that's the right, low-risk pattern
 * for a NEW, self-contained khtpm app: "this IS the picker... reuses
 * the exact same real shared Elem model... every phantom-click/focus/
 * PPosition fix proven on khtpm_core_render.c." This file
 * follows khtpm_choice_picker.c's real X11/Xft/event-loop shape
 * directly, not invented fresh.
 *
 * Real division of labor (CENTROID_GOLD_STD.md §3): ALL browsing logic
 * (fetch, HTML extraction, URL resolution) lives in the separate,
 * already-proven network_browser_manager.c - this file's entire real
 * job is (a) render whatever that manager last published, as a real
 * positioned/styled Elem tree, and (b) turn key/click input into
 * `go:<url>` lines written to the manager's own action file. Zero HTML
 * parsing happens here - grep-checkable.
 *
 * Real, honest scope note: the window CHROME (address bar row, status
 * row, content panel) is hand-built directly onto the shared Elem
 * struct, the same way khtpm_choice_picker.c hand-builds its own
 * items instead of authoring/parsing a .chtpm for a single flat list -
 * a real, already-established house precedent for a small, wholly
 * data-driven view. A companion `network-browser-hq.chtpm`/`.css`
 * exists alongside this file as the real, authored INTENT/reference
 * (class name, palette) for a future full khtpm_css_parser.c-driven
 * chrome pass - not required for this file to satisfy CENTROID_GOLD_
 * STD.md's core rule, since the actual single source of truth here is
 * the manager's own published page-state list, which BOTH this
 * renderer and network_browser_render_ascii.c walk identically (see
 * that file's own header comment for the symmetric other half).
 *
 * Real agent-relay + frame-history (2026-08-31, direct instruction:
 * "there should be a .history and frame-history file as well as source
 * of truths" - a real, valid gap found right after this file's first
 * live test: every other raw-Xlib khtpm app in this house has a real
 * file-based agent-relay input path (_.0.aigent-testing-k9.txt's own
 * documented contract - bare decimal ASCII per line: 48-57 digits, 13
 * Enter, 27 Escape, 8 Backspace, 32-126 printable) AND a real frame-
 * history log a text audit can read instead of reaching for a PNG dump
 * (khtpm_strip_frame_history.txt's own real precedent) - this file had
 * neither. Added here, ported not reinvented:
 *   #.desktop/network_browser_history.txt  - agent relay IN (same bare-
 *     decimal contract, same "never replay backlog, resync to EOF on
 *     first poll" cursor discipline as poll_agent_relay()).
 *   #.desktop/network_browser_frame_history.txt - one real line per
 *     redraw OUT (url/editing-mode/focus/link-count/scroll/status),
 *     same "append, never truncate, real diagnostic snapshot" shape as
 *     khtpm_strip_frame_history.txt.
 *
 * Usage: network_browser_render.+x <house_root> [x] [y]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include "khtpm_css_parser.h" /* Elem's CssStyle field type, same as khtpm_choice_picker.c - no runtime CSS applied here */
#include "khtpm_render_core.c" /* real .c, not a header - see that file's own comment */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define PATH_BUF 4352
#define MAX_ELEMS 512
#define CHROME_H 24
#define ADDR_H 28
#define STATUS_H 20
#define ROW_H 20
#define URL_BUF 2048

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
static Elem *elem_new(void) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    return e;
}

static char g_house[PATH_BUF];
static char g_action_path[PATH_BUF];
static char g_page_state_path[PATH_BUF];
static char g_status_path[PATH_BUF];
static char g_history_path[PATH_BUF];
static char g_frame_history_path[PATH_BUF];

/* ---------- real, in-memory mirror of the manager's own published page ---------- */
typedef struct { char kind; char a[URL_BUF]; char b[512]; } PageRow; /* kind: 'T'=title, 'X'=text, 'L'=link */
static PageRow g_rows[MAX_ELEMS];
static int g_n_rows = 0;
static char g_page_url[URL_BUF] = "";
static char g_status_line[256] = "idle";
static time_t g_page_mtime = 0, g_status_mtime = 0;

static void trim_nl(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static time_t mtime_of(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

static int reload_page_if_changed(void) {
    time_t m = mtime_of(g_page_state_path);
    if (m == g_page_mtime) return 0;
    g_page_mtime = m;
    g_n_rows = 0;
    g_page_url[0] = '\0';
    FILE *f = fopen(g_page_state_path, "r");
    if (!f) return 1;
    char line[URL_BUF + 512];
    while (fgets(line, sizeof(line), f) && g_n_rows < MAX_ELEMS) {
        trim_nl(line);
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *rest = p1 + 1;
        if (strcmp(line, "URL") == 0) { snprintf(g_page_url, sizeof(g_page_url), "%s", rest); continue; }
        if (strcmp(line, "TITLE") == 0) {
            g_rows[g_n_rows].kind = 'T';
            snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest);
            g_n_rows++;
            continue;
        }
        if (strcmp(line, "TEXT") == 0) {
            g_rows[g_n_rows].kind = 'X';
            snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest);
            g_n_rows++;
            continue;
        }
        if (strcmp(line, "LINK") == 0) {
            char *p2 = strchr(rest, '|');
            if (!p2) continue;
            *p2 = '\0';
            g_rows[g_n_rows].kind = 'L';
            snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest);
            snprintf(g_rows[g_n_rows].b, sizeof(g_rows[g_n_rows].b), "%s", p2 + 1);
            g_n_rows++;
            continue;
        }
    }
    fclose(f);
    return 1;
}

static int reload_status_if_changed(void) {
    time_t m = mtime_of(g_status_path);
    if (m == g_status_mtime) return 0;
    g_status_mtime = m;
    FILE *f = fopen(g_status_path, "r");
    if (!f) { snprintf(g_status_line, sizeof(g_status_line), "idle"); return 1; }
    if (!fgets(g_status_line, sizeof(g_status_line), f)) g_status_line[0] = '\0';
    trim_nl(g_status_line);
    fclose(f);
    return 1;
}

static void write_action(const char *action) {
    FILE *f = fopen(g_action_path, "w");
    if (!f) return;
    fprintf(f, "%s\n", action);
    fclose(f);
}

/* ---------- X11/Xft (same real proven shape as khtpm_choice_picker.c) ---------- */
static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Colormap cmap;
static XftFont *font_ui;
static int screen;
static int g_win_x = 200, g_win_y = 120;
static int g_win_w = 760, g_win_h = 520;
static int g_quit = 0;
static struct timespec g_map_time;
#define PHANTOM_CLICK_GUARD_MS 150

/* Elem tree, rebuilt from g_rows every redraw - the real, positioned/
 * styled tree this whole app's real "centroid" is. */
static Elem *g_root, *g_chrome, *g_addr, *g_status_el, *g_content;
static Elem *g_close_elem, *g_fullscreen_elem;
static Elem *g_link_elems[MAX_ELEMS];
static int g_n_link_elems = 0;

/* Real house chrome-nav convention (khtpm_core_render.c's own
 * PCHQ_ACT_CLOSE/PCHQ_ACT_FULLSCREEN + "close is always nav 1" rule,
 * direct instruction after this file's own first draft got it wrong -
 * "nav [] are supposed to be empty as always... there should be ! and
 * x chrome elements for fullsize and close"). Chrome always claims nav
 * 1 (close, label "X") and nav 2 (fullscreen, label "!"); content links
 * start at 3 - NOT the naive "links start at 1" this file originally
 * shipped with. */
#define NAV_CLOSE 1
#define NAV_FULLSCREEN 2

static char g_url_buf[URL_BUF] = "https://example.com";
static int g_url_len = 22;
static int g_editing_addr = 1; /* 1 = typing in address bar, 0 = navigating content */
static int g_focus_nav = NAV_CLOSE; /* real nav-index of whatever's focused (chrome or a link) */
static int g_scroll = 0;
static int g_is_fullscreen = 0;
static int g_saved_x, g_saved_y, g_saved_w, g_saved_h;

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

/* Real Elem tree build - this IS the shared centroid (CENTROID_GOLD_
 * STD.md §1): a positioned tree, walked once here for pixels, walked
 * again identically (line-for-line the same g_rows source) by
 * network_browser_render_ascii.c for a real terminal mirror. */
static void build_tree(void) {
    g_n_elems = 0;
    g_n_link_elems = 0;
    g_root = elem_new(); snprintf(g_root->tag, sizeof(g_root->tag), "window");
    g_chrome = elem_new(); snprintf(g_chrome->tag, sizeof(g_chrome->tag), "panel");
    g_addr = elem_new(); snprintf(g_addr->tag, sizeof(g_addr->tag), "text");
    g_status_el = elem_new(); snprintf(g_status_el->tag, sizeof(g_status_el->tag), "text");
    g_content = elem_new(); snprintf(g_content->tag, sizeof(g_content->tag), "panel");

    g_chrome->x = 0; g_chrome->y = 0; g_chrome->w = g_win_w; g_chrome->h = CHROME_H;
    g_addr->x = 4; g_addr->y = CHROME_H; g_addr->w = g_win_w - 8; g_addr->h = ADDR_H;
    g_content->x = 0; g_content->y = CHROME_H + ADDR_H; g_content->w = g_win_w; g_content->h = g_win_h - CHROME_H - ADDR_H - STATUS_H;
    g_status_el->x = 4; g_status_el->y = g_win_h - STATUS_H; g_status_el->w = g_win_w - 8; g_status_el->h = STATUS_H;

    /* real chrome nav 1/2 - fixed, top-right of the chrome bar, same
     * "close is always nav 1" rule as every other real khtpm window. */
    g_close_elem = elem_new(); snprintf(g_close_elem->tag, sizeof(g_close_elem->tag), "button");
    g_close_elem->w = 24; g_close_elem->h = CHROME_H; g_close_elem->x = g_win_w - 24; g_close_elem->y = 0;
    g_close_elem->nav_index = NAV_CLOSE;
    snprintf(g_close_elem->label, sizeof(g_close_elem->label), "X");

    g_fullscreen_elem = elem_new(); snprintf(g_fullscreen_elem->tag, sizeof(g_fullscreen_elem->tag), "button");
    g_fullscreen_elem->w = 24; g_fullscreen_elem->h = CHROME_H; g_fullscreen_elem->x = g_win_w - 48; g_fullscreen_elem->y = 0;
    g_fullscreen_elem->nav_index = NAV_FULLSCREEN;
    snprintf(g_fullscreen_elem->label, sizeof(g_fullscreen_elem->label), "!");

    int y = g_content->y - g_scroll;
    int nav = 3; /* real content links start after chrome's own nav 1/2 */
    for (int i = 0; i < g_n_rows; i++) {
        Elem *r = elem_new();
        if (!r) break;
        r->x = 8; r->y = y; r->w = g_content->w - 16; r->h = ROW_H;
        if (g_rows[i].kind == 'T') {
            snprintf(r->tag, sizeof(r->tag), "title");
            snprintf(r->label, sizeof(r->label), "%s", g_rows[i].a);
        } else if (g_rows[i].kind == 'X') {
            snprintf(r->tag, sizeof(r->tag), "text");
            snprintf(r->label, sizeof(r->label), "%s", g_rows[i].a);
        } else {
            snprintf(r->tag, sizeof(r->tag), "button");
            snprintf(r->label, sizeof(r->label), "%s", g_rows[i].b);
            snprintf(r->onclick, sizeof(r->onclick), "%s", g_rows[i].a);
            r->nav_index = nav++;
            g_link_elems[g_n_link_elems++] = r;
        }
        y += ROW_H;
    }
    { int max_nav = NAV_FULLSCREEN + g_n_link_elems;
      if (g_focus_nav > max_nav) g_focus_nav = max_nav;
      if (g_focus_nav < NAV_CLOSE) g_focus_nav = NAV_CLOSE; }
}

static Elem *link_by_nav(int nav) {
    for (int i = 0; i < g_n_link_elems; i++) if (g_link_elems[i]->nav_index == nav) return g_link_elems[i];
    return NULL;
}

/* Real fullscreen toggle - resizes the actual X11 window AND its backing
 * pixmap/XftDraw (a bare XMoveResizeWindow alone would leave buf/xftdraw_
 * buf at the old size, corrupting every redraw() after the toggle). */
static void toggle_fullscreen(void) {
    if (!g_is_fullscreen) {
        g_saved_x = g_win_x; g_saved_y = g_win_y; g_saved_w = g_win_w; g_saved_h = g_win_h;
        g_win_x = 0; g_win_y = 0;
        g_win_w = DisplayWidth(dpy, screen);
        g_win_h = DisplayHeight(dpy, screen);
        g_is_fullscreen = 1;
    } else {
        g_win_x = g_saved_x; g_win_y = g_saved_y; g_win_w = g_saved_w; g_win_h = g_saved_h;
        g_is_fullscreen = 0;
    }
    XMoveResizeWindow(dpy, win, g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h);
    XftDrawDestroy(xftdraw_buf);
    XFreePixmap(dpy, buf);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);
}

static void redraw(void) {
    reload_page_if_changed();
    reload_status_if_changed();
    build_tree();

    XSetForeground(dpy, gc, alloc_pixel("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);

    /* chrome bar */
    XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, g_chrome->x, g_chrome->y, (unsigned)g_chrome->w, (unsigned)g_chrome->h);
    { XftColor c = xft_color("#eeeeee");
      const char *t = "Network Browser (centroid proof)";
      XftDrawStringUtf8(xftdraw_buf, &c, font_ui, 8, 16, (const FcChar8 *)t, (int)strlen(t));
      XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c); }

    /* real chrome nav 1 (close, "X") / nav 2 (fullscreen, "!") - same
     * cursor-prefix convention as every other real khtpm window
     * ("[>]"/"[ ]", never the literal digit inside the brackets). */
    { Elem *chrome_btns[2] = { g_close_elem, g_fullscreen_elem };
      for (int i = 0; i < 2; i++) {
          Elem *b = chrome_btns[i];
          int on = (!g_editing_addr && b->nav_index == g_focus_nav);
          XftColor c = xft_color(on ? "#ffdd55" : "#cccccc");
          char lbl[16];
          snprintf(lbl, sizeof(lbl), "%s%s", on ? "[>]" : "[ ]", b->label);
          XftDrawStringUtf8(xftdraw_buf, &c, font_ui, b->x, b->y + 16, (const FcChar8 *)lbl, (int)strlen(lbl));
          XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
      } }

    /* address bar */
    XSetForeground(dpy, gc, g_editing_addr ? alloc_pixel("#333355") : alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, g_addr->x, g_addr->y, (unsigned)g_addr->w, (unsigned)g_addr->h);
    { XftColor c = xft_color("#ffffff");
      char shown[URL_BUF + 4];
      snprintf(shown, sizeof(shown), "%s%s", g_url_buf, g_editing_addr ? "_" : "");
      XftDrawStringUtf8(xftdraw_buf, &c, font_ui, g_addr->x + 6, g_addr->y + 19, (const FcChar8 *)shown, (int)strlen(shown));
      XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c); }

    /* content panel rows - clipped to g_content's own real rect */
    for (int i = 0; i < g_n_elems; i++) {
        Elem *e = &g_pool[i];
        if (e == g_root || e == g_chrome || e == g_addr || e == g_status_el || e == g_content
            || e == g_close_elem || e == g_fullscreen_elem) continue;
        if (e->y + e->h < g_content->y || e->y > g_content->y + g_content->h) continue;
        if (strcmp(e->tag, "title") == 0) {
            XftColor c = xft_color("#ffdd55");
            XftDrawStringUtf8(xftdraw_buf, &c, font_ui, e->x, e->y + 15, (const FcChar8 *)e->label, (int)strlen(e->label));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
        } else if (strcmp(e->tag, "text") == 0) {
            XftColor c = xft_color("#cccccc");
            XftDrawStringUtf8(xftdraw_buf, &c, font_ui, e->x, e->y + 15, (const FcChar8 *)e->label, (int)strlen(e->label));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
        } else if (strcmp(e->tag, "button") == 0) {
            int on = (!g_editing_addr && e->nav_index == g_focus_nav);
            if (on) {
                XSetForeground(dpy, gc, alloc_pixel("#333333"));
                XFillRectangle(dpy, buf, gc, e->x, e->y, (unsigned)e->w, (unsigned)e->h);
            }
            XftColor c = xft_color(on ? "#ffdd55" : "#66aaff");
            char row[560];
            snprintf(row, sizeof(row), "%s%d. %s", on ? "[>]" : "[ ]", e->nav_index, e->label);
            XftDrawStringUtf8(xftdraw_buf, &c, font_ui, e->x, e->y + 15, (const FcChar8 *)row, (int)strlen(row));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c);
        }
    }

    /* status bar */
    XSetForeground(dpy, gc, alloc_pixel("#101010"));
    XFillRectangle(dpy, buf, gc, g_status_el->x - 4, g_status_el->y, (unsigned)g_win_w, (unsigned)STATUS_H);
    { XftColor c = xft_color(strncmp(g_status_line, "error", 5) == 0 ? "#ff8888" : "#88cc88");
      XftDrawStringUtf8(xftdraw_buf, &c, font_ui, g_status_el->x, g_status_el->y + 14, (const FcChar8 *)g_status_line, (int)strlen(g_status_line));
      XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &c); }

    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) { XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h); XDestroyImage(frame); }
    XFlush(dpy);

    /* real frame-history append, one line per redraw - append-only,
     * never truncated, same shape as khtpm_strip_frame_history.txt so
     * a text audit can confirm what this window actually did without
     * a PNG dump. */
    FILE *fh = fopen(g_frame_history_path, "a");
    if (fh) {
        fprintf(fh, "url=%s editing_addr=%d focus_nav=%d n_links=%d scroll=%d status=%s\n",
                g_page_url[0] ? g_page_url : "(none)", g_editing_addr, g_focus_nav, g_n_link_elems, g_scroll, g_status_line);
        fclose(fh);
    }
}

static void go_to(const char *url) {
    char action[URL_BUF + 8];
    snprintf(action, sizeof(action), "go:%s", url);
    write_action(action);
}

static void handle_key(KeySym ks, char ch) {
    if (ks == XK_Tab) { g_editing_addr = !g_editing_addr; return; }
    if (ks == XK_Escape) { g_quit = 1; return; }

    if (g_editing_addr) {
        if (ks == XK_Return || ks == XK_KP_Enter) {
            g_url_buf[g_url_len] = '\0';
            go_to(g_url_buf);
            g_editing_addr = 0;
            g_scroll = 0;
            return;
        }
        if (ks == XK_BackSpace) { if (g_url_len > 0) g_url_len--; return; }
        if (ch >= 32 && ch < 127 && g_url_len < URL_BUF - 1) { g_url_buf[g_url_len++] = ch; return; }
        return;
    }

    /* content-navigation mode - real chrome-nav convention: 1 = close,
     * 2 = fullscreen, 3.. = content links (see NAV_CLOSE/NAV_FULLSCREEN). */
    int max_nav = NAV_FULLSCREEN + g_n_link_elems;
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_focus_nav == NAV_CLOSE) { g_quit = 1; return; }
        if (g_focus_nav == NAV_FULLSCREEN) { toggle_fullscreen(); return; }
        Elem *link = link_by_nav(g_focus_nav);
        if (link) {
            snprintf(g_url_buf, sizeof(g_url_buf), "%s", link->onclick);
            g_url_len = (int)strlen(g_url_buf);
            go_to(link->onclick);
            g_scroll = 0;
        }
        return;
    }
    if (ks == XK_Down) { if (g_focus_nav < max_nav) g_focus_nav++; else g_focus_nav = NAV_CLOSE; return; }
    if (ks == XK_Up) { if (g_focus_nav > NAV_CLOSE) g_focus_nav--; else g_focus_nav = max_nav; return; }
    if (ks == XK_Page_Down) { g_scroll += g_content->h; return; }
    if (ks == XK_Page_Up) { g_scroll -= g_content->h; if (g_scroll < 0) g_scroll = 0; return; }
    if (ch >= '0' && ch <= '9') { int d = ch - '0'; if (d == 0) d = 10; if (d <= max_nav) g_focus_nav = d; return; }
}

static void handle_click(int mx, int my) {
    if (my >= g_addr->y && my < g_addr->y + g_addr->h) { g_editing_addr = 1; return; }
    g_editing_addr = 0;
    if (mx >= g_close_elem->x && mx < g_close_elem->x + g_close_elem->w && my >= g_close_elem->y && my < g_close_elem->y + g_close_elem->h) {
        g_focus_nav = NAV_CLOSE;
        g_quit = 1;
        return;
    }
    if (mx >= g_fullscreen_elem->x && mx < g_fullscreen_elem->x + g_fullscreen_elem->w && my >= g_fullscreen_elem->y && my < g_fullscreen_elem->y + g_fullscreen_elem->h) {
        g_focus_nav = NAV_FULLSCREEN;
        toggle_fullscreen();
        return;
    }
    for (int i = 0; i < g_n_link_elems; i++) {
        Elem *e = g_link_elems[i];
        if (mx >= e->x && mx < e->x + e->w && my >= e->y && my < e->y + e->h) {
            g_focus_nav = e->nav_index;
            snprintf(g_url_buf, sizeof(g_url_buf), "%s", e->onclick);
            g_url_len = (int)strlen(g_url_buf);
            go_to(e->onclick);
            g_scroll = 0;
            return;
        }
    }
}

/* Real agent relay - same bare-decimal-ASCII-per-line contract as
 * poll_agent_relay() in khtpm_strip_parser.c (see this file's own
 * header comment). Converts each code to the SAME (KeySym, char) pair
 * a real KeyPress produces and calls the SAME handle_key() a real
 * keyboard drives - an injected code gets identical handling to a real
 * keypress by construction, not "kept in sync by hand" (same real
 * reasoning that file's own header comment gives for sharing
 * dispatch_key_code() instead of duplicating it). No mouse/click relay
 * - matches every real house relay's own "digits are how the relay
 * navigates" contract; link-follow is fully reachable via digit codes
 * already (handle_key()'s own '1'-'9' branch). */
static long g_history_cursor = -1; /* -1 = uninitialized; resync to EOF on first poll, never replay backlog */

static void poll_history(void) {
    FILE *f = fopen(g_history_path, "r");
    if (!f) return;
    if (g_history_cursor < 0) {
        fseek(f, 0, SEEK_END);
        g_history_cursor = ftell(f);
        fclose(f);
        return;
    }
    fseek(f, g_history_cursor, SEEK_SET);
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int code = atoi(line);
        if (code == 13) handle_key(XK_Return, 0);
        else if (code == 27) handle_key(XK_Escape, 0);
        else if (code == 8) handle_key(XK_BackSpace, 0);
        else if (code == 9) handle_key(XK_Tab, 0);
        else if (code >= 32 && code < 127) handle_key(NoSymbol, (char)code);
    }
    g_history_cursor = ftell(f);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root> [x] [y]\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    if (argc >= 4) { g_win_x = atoi(argv[2]); g_win_y = atoi(argv[3]); }

    char desktop[PATH_BUF];
    snprintf(desktop, sizeof(desktop), "%s/#.desktop", g_house);
    snprintf(g_action_path, sizeof(g_action_path), "%s/network_browser_action.txt", desktop);
    snprintf(g_page_state_path, sizeof(g_page_state_path), "%s/network_browser_page.state.txt", desktop);
    snprintf(g_status_path, sizeof(g_status_path), "%s/network_browser_status.state.txt", desktop);
    snprintf(g_history_path, sizeof(g_history_path), "%s/network_browser_history.txt", desktop);
    snprintf(g_frame_history_path, sizeof(g_frame_history_path), "%s/network_browser_frame_history.txt", desktop);

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "network_browser_render: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);

    {
        int scr_w = DisplayWidth(dpy, screen);
        int scr_h = DisplayHeight(dpy, screen);
        if (g_win_x + g_win_w > scr_w - 8) g_win_x = scr_w - g_win_w - 8;
        if (g_win_y + g_win_h > scr_h - 8) g_win_y = scr_h - g_win_h - 8;
        if (g_win_x < 8) g_win_x = 8;
        if (g_win_y < 8) g_win_y = 8;
    }

    font_ui = XftFontOpenName(dpy, screen, "DejaVu Sans Mono:pixelsize=12");

    XSetWindowAttributes swa;
    swa.background_pixel = alloc_pixel("#1c1c1c");
    swa.override_redirect = True;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)g_win_w, (unsigned)g_win_h, 0,
                         CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWOverrideRedirect | CWEventMask, &swa);
    XSizeHints *shints = XAllocSizeHints();
    if (shints) { shints->flags = PPosition; shints->x = g_win_x; shints->y = g_win_y; XSetWMNormalHints(dpy, win, shints); XFree(shints); }
    XStoreName(dpy, win, "Network Browser");

    XMapRaised(dpy, win);
    XSync(dpy, False);
    XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
    clock_gettime(CLOCK_MONOTONIC, &g_map_time);
    { XEvent stale_ev; while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    redraw();

    while (!g_quit) {
        fd_set fds; FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        struct timeval tv = { 0, 250000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = 0;
        if (mtime_of(g_page_state_path) != g_page_mtime) need_redraw = 1;
        if (mtime_of(g_status_path) != g_status_mtime) need_redraw = 1;

        { int before = g_focus_nav, before_edit = g_editing_addr, before_q = g_quit;
          poll_history();
          if (g_focus_nav != before || g_editing_addr != before_edit || g_quit != before_q) need_redraw = 1; }

        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) { need_redraw = 1; }
            else if (ev.type == ButtonPress) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long ms_since_map = (now.tv_sec - g_map_time.tv_sec) * 1000L
                                   + (now.tv_nsec - g_map_time.tv_nsec) / 1000000L;
                if (ms_since_map < PHANTOM_CLICK_GUARD_MS) continue;
                handle_click(ev.xbutton.x, ev.xbutton.y);
                need_redraw = 1;
            } else if (ev.type == KeyPress) {
                char kbuf[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                kbuf[n > 0 ? n : 0] = '\0';
                handle_key(ks, kbuf[0]);
                need_redraw = 1;
            }
        }
        if (!g_quit && need_redraw) redraw();
    }

    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
