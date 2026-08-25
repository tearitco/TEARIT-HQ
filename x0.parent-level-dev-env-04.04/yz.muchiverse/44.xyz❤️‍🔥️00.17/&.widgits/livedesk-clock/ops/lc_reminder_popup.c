/* lc_reminder_popup.c — livedesk-clock reminder window (design doc §6.2).
 *
 * HOUSE WINDOW STANDARD (recorded for future agents, do not regress):
 * This is an X11 RGB window styled by khtpm_css_parser — NOT GL. The
 * house standard for popup/dialog windows is "X11 RGB window + CSS",
 * opened the same way db-hq / events-hq / open-hai / context-menu open
 * (own detached process via `setsid nohup <bin> <house> <payload>` +
 * #.desktop/hq_ui.pdl font_scale + a matching .css file). It is NOT a
 * GL window (no freeglut/gl-canvas/glReadPixels mirror) — GL is only
 * for gl_mirror-style RGB mirrors, not for house popups. The color/
 * font/chrome helpers below are direct ports of khtpm_hq_render.c's
 * proven versions so this window looks and behaves like the others.
 *
 * Usage:  lc_reminder_popup <house_root> <textfile>
 * Launched by the clock daemon's fire_reminder(). Renders the text
 * file's lines into a CSS-styled X11 RGB window with a house chrome
 * bar (title + [x], drag-to-move), closes on Escape / any click / [x].
 * CSS: found dynamically via find_app_dir() (see below), currently at
 * <house>/&.widgits/livedesk-clock/reminder.css. Scale:
 * <house>/#.desktop/hq_ui.pdl font_scale. Agent relay for harnesses:
 * <house>/#.desktop/lc_reminder_relay.txt (decimal ASCII codes).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include "khtpm_css_parser.h"

#define MAX_LINES 64
#define MAX_LINE_LEN 1024
#define PAD 14
#define PATH_BUF 4096

typedef struct {
    char text[MAX_LINE_LEN];
    CssStyle style;
    XftFont *font;
    XftColor col;
    int tw;   /* text width px */
    int th;   /* ascent + descent px */
} LLine;

static Display *dpy;
static int screen;
static Colormap cmap;
static GC gc;
static XftDraw *xft;
static double g_font_scale = 1.0;
static char g_house_root[PATH_BUF];

static int g_quit = 0;
static int g_chrome_h = 26;
static int g_close_x, g_close_y, g_close_w, g_close_h;
static int g_drag_last_x, g_drag_last_y, g_dragging = 0;
static Window g_win;
static int g_win_x, g_win_y, g_win_w, g_win_h;
static int g_border_w = 1;
static Atom g_wm_delete = None;

static LLine g_lines[MAX_LINES];
static int g_n = 0;

/* ------------------------------------------------------------------ */
/* house helpers (ports of khtpm_hq_render.c)                          */
/* ------------------------------------------------------------------ */

static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        return c.pixel;
    }
    return BlackPixel(dpy, screen);
}

static XftColor xft_color(const char *spec) {
    XftColor xc;
    XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b;
        sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257);
        rc.green = (unsigned short)(g * 257);
        rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}

static int scaled(int base_px) { return (int)(base_px * g_font_scale + 0.5); }

static XftFont *font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "Noto Sans CJK SC";
    int size = scaled(st->has_font_size ? st->font_size : 15);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size,
             (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
    if (!f) f = XftFontOpenName(dpy, screen, "Noto Sans CJK SC:pixelsize=13");
    return f;
}

static void load_font_scale(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        double v = atof(eq + 1);
        if (strcmp(line, "font_scale") == 0 && v >= 0.5 && v <= 3.0) g_font_scale = v;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* reminder payload                                                    */
/* ------------------------------------------------------------------ */

static void load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char buf[MAX_LINE_LEN];
    while (g_n < MAX_LINES && fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        snprintf(g_lines[g_n].text, MAX_LINE_LEN, "%s", buf);
        g_n++;
    }
    fclose(f);
}

/* line -> CSS classes: first line = title, "note:" = note, else meta. */
static void line_classes(int i, char classes[][32], int *n) {
    *n = 0;
    if (i == 0) { snprintf(classes[(*n)++], 32, "title"); return; }
    if (strncmp(g_lines[i].text, "note:", 5) == 0) snprintf(classes[(*n)++], 32, "note");
    else snprintf(classes[(*n)++], 32, "meta");
}

static void measure_lines(const CssSheet *sheet, const CssStyle *win_style) {
    for (int i = 0; i < g_n; i++) {
        char classes[CSS_MAX_CLASSES][32];
        int nclasses = 0;
        line_classes(i, classes, &nclasses);
        css_compute_style(sheet, "line", NULL, classes, nclasses, 0, &g_lines[i].style);
        if (!g_lines[i].style.has_fg_color && win_style->has_fg_color)
            snprintf(g_lines[i].style.fg_color, sizeof(g_lines[i].style.fg_color), "%s", win_style->fg_color);
        g_lines[i].font = font_for(&g_lines[i].style);
        if (!g_lines[i].font) continue;
        const char *fg = g_lines[i].style.has_fg_color ? g_lines[i].style.fg_color : "#000000";
        g_lines[i].col = xft_color(fg);
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, g_lines[i].font, (const FcChar8 *)g_lines[i].text,
                           (int)strlen(g_lines[i].text), &ext);
        g_lines[i].tw = ext.width;
        g_lines[i].th = g_lines[i].font->ascent + g_lines[i].font->descent;
    }
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */
/* ------------------------------------------------------------------ */

static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, alloc_pixel("#2b2b2b"));
    XFillRectangle(dpy, g_win, gc, 0, 0, g_win_w, g_chrome_h);

    char tspec[64];
    snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d:bold", scaled(10));
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (!titlefont) {
        snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d", scaled(10));
        titlefont = XftFontOpenName(dpy, screen, tspec);
    }
    if (titlefont) {
        XftColor titlecol = xft_color("#eeeeee");
        int ty = (g_chrome_h + titlefont->ascent - titlefont->descent) / 2;
        XftDrawStringUtf8(xft, &titlecol, titlefont, scaled(8), ty,
                          (const FcChar8 *)"⏰ reminder", (int)strlen("⏰ reminder"));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
        XftFontClose(dpy, titlefont);
    }

    /* close [x] button */
    g_close_w = scaled(20); g_close_h = g_chrome_h - scaled(8);
    g_close_x = g_win_w - g_close_w - scaled(4);
    g_close_y = scaled(4);
    XSetForeground(dpy, gc, alloc_pixel("#888888"));
    XDrawRectangle(dpy, g_win, gc, g_close_x, g_close_y, g_close_w, g_close_h);
    XftFont *cf = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=12");
    if (cf) {
        XftColor xcol = xft_color("#eeeeee");
        XGlyphInfo cext;
        XftTextExtentsUtf8(dpy, cf, (const FcChar8 *)"x", 1, &cext);
        int cx = g_close_x + (g_close_w - cext.width) / 2;
        int cy = g_close_y + (g_close_h + cf->ascent - cf->descent) / 2;
        XftDrawStringUtf8(xft, &xcol, cf, cx, cy, (const FcChar8 *)"x", 1);
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &xcol);
        XftFontClose(dpy, cf);
    }
}

static void redraw(void) {
    CssStyle ws;
    css_style_init(&ws);
    ws.has_bg_color = 1;
    snprintf(ws.bg_color, sizeof(ws.bg_color), "%s", "#1e1e2e");
    XSetForeground(dpy, gc, alloc_pixel(ws.bg_color));
    XFillRectangle(dpy, g_win, gc, 0, 0, g_win_w, g_win_h);

    /* CSS border drawn inside the window (house style: no X border) */
    if (g_border_w > 0) {
        XSetForeground(dpy, gc, alloc_pixel("#4a9eff"));
        for (int b = 0; b < g_border_w; b++)
            XDrawRectangle(dpy, g_win, gc, b, b, g_win_w - 1 - b * 2, g_win_h - 1 - b * 2);
    }

    int ypos = g_chrome_h + PAD;
    for (int i = 0; i < g_n; i++) {
        if (!g_lines[i].font) continue;
        XftDrawStringUtf8(xft, &g_lines[i].col, g_lines[i].font,
                          PAD, ypos + g_lines[i].font->ascent,
                          (const FcChar8 *)g_lines[i].text,
                          (int)strlen(g_lines[i].text));
        ypos += g_lines[i].th;
    }
    draw_chrome_bar();
    XFlush(dpy);
}

/* ------------------------------------------------------------------ */
/* agent relay (harness-testable, same contract as db_hq_agent_relay)  */
/* ------------------------------------------------------------------ */

static long g_relay_cursor = -1;

static void dispatch_relay_code(int code) {
    if (code == 27) g_quit = 1; /* Escape closes */
}

static void poll_agent_relay(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/lc_reminder_relay.txt", g_house_root);
    struct stat stt;
    if (stat(path, &stt) != 0) return;
    if (g_relay_cursor < 0) { g_relay_cursor = 0; return; } /* start at head */
    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) dispatch_relay_code(code);
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

/* REAL, dynamic path discovery (2026-08-17, direct instruction: "we
 * dont hardcode, see how tpmos's button.sh does dynamic path
 * discovery"). Same real precedent as play_event.sh's own upward
 * landmark search / khtpm_taskbar_manager.c's own toys_scan_one_root(). */
static int find_app_dir(const char *house_root, const char *app_name, char *out, size_t outsz) {
    static const char *roots[] = { "*.monads", "&.widgits", "&.hq-apps", "@.apps", NULL };
    for (int i = 0; roots[i]; i++) {
        char parent[PATH_BUF];
        snprintf(parent, sizeof(parent), "%s/%s", house_root, roots[i]);
        DIR *d = opendir(parent);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strstr(ent->d_name, app_name)) {
                snprintf(out, outsz, "%s/%s", parent, ent->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    out[0] = '\0';
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <textfile>\n", argv[0]);
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    load_file(argv[2]);
    if (g_n == 0) { snprintf(g_lines[0].text, MAX_LINE_LEN, "(empty)"); g_n = 1; }

    dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);
    gc = DefaultGC(dpy, screen);

    load_font_scale();
    g_chrome_h = scaled(26);

    char lc_dir[PATH_BUF];
    find_app_dir(g_house_root, "livedesk-clock", lc_dir, sizeof(lc_dir));
    char css_path[PATH_BUF];
    snprintf(css_path, sizeof(css_path), "%s/reminder.css", lc_dir);
    CssSheet sheet;
    memset(&sheet, 0, sizeof(sheet));
    css_load(css_path, &sheet);

    CssStyle win_style;
    css_style_init(&win_style);
    css_compute_style(&sheet, "window", NULL, NULL, 0, 0, &win_style);
    measure_lines(&sheet, &win_style);

    int max_w = 0, tot_h = 0;
    for (int i = 0; i < g_n; i++) {
        if (g_lines[i].tw > max_w) max_w = g_lines[i].tw;
        tot_h += g_lines[i].th;
    }
    int border_w = 1;
    if (win_style.has_border_width && win_style.border_width >= 1) border_w = win_style.border_width;
    if (border_w > 6) border_w = 6;
    g_border_w = border_w;
    g_win_w = max_w + PAD * 2 + border_w * 2;
    if (g_win_w < 260) g_win_w = 260;
    g_win_h = g_chrome_h + tot_h + PAD * (g_n + 1) + border_w * 2;

    Window root = RootWindow(dpy, screen);
    int sw = DisplayWidth(dpy, screen);
    g_win_x = sw - g_win_w - 24;
    g_win_y = 48;

    /* Window creation is a direct port of khtpm_hq_render.c's block
     * (house standard, lines ~1168-1251): a normally WM-MANAGED window
     * (so Mutter applies real focus policy) with the title bar/border
     * suppressed via _MOTIF_WM_HINTS decorations=0 — NOT override_
     * redirect, NOT GL. Every khtpm window (db-hq/events-hq/context)
     * opens this way: borderless, own drawn chrome, Escape/click close. */
    XSetWindowAttributes swa;
    swa.background_pixel = alloc_pixel("#1e1e2e");
    swa.border_pixel = alloc_pixel(win_style.has_border_color ? win_style.border_color : "#4a9eff");
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     ButtonMotionMask | KeyPressMask | StructureNotifyMask;
    g_win = XCreateWindow(dpy, root, g_win_x, g_win_y, g_win_w, g_win_h, 0,
                          DefaultDepth(dpy, screen), InputOutput,
                          DefaultVisual(dpy, screen), CWBackPixel | CWBorderPixel | CWEventMask, &swa);

    {
        /* _MOTIF_WM_HINTS: flags=MWM_HINTS_DECORATIONS(2), decorations=0
         * — hides the WM title bar/border (this is what kills the "GL
         * header"; same hint khtpm_hq_render.c uses). */
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 };
        XChangeProperty(dpy, g_win, motif_hints, motif_hints, 32, PropModeReplace,
                        (unsigned char *)hints, 5);

        XWMHints *wmhints = XAllocWMHints();
        if (wmhints) {
            wmhints->flags = InputHint;
            wmhints->input = True;
            XSetWMHints(dpy, g_win, wmhints);
            XFree(wmhints);
        }

        Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, g_win, &wm_delete, 1);
        g_wm_delete = wm_delete;
    }
    {
        /* class must match the xwayland grab-access allowlist, same as
         * every other MuchiverseLivedesk window. */
        XClassHint *ch = XAllocClassHint();
        if (ch) {
            ch->res_name = (char *)"MuchiverseLivedesk";
            ch->res_class = (char *)"MuchiverseLivedesk";
            XSetClassHint(dpy, g_win, ch);
            XFree(ch);
        }
    }
    XStoreName(dpy, g_win, "⏰ reminder");
    XMapRaised(dpy, g_win);
    XSync(dpy, False);
    { XWindowAttributes wa; if (XGetWindowAttributes(dpy, g_win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }
    /* drain stale queued events for this recycled window id (same race
     * guard khtpm_hq_render.c documents) so a leftover click can't
     * phantom-close the popup the instant it maps. */
    XSync(dpy, False);
    { XEvent stale_ev; while (XCheckWindowEvent(dpy, g_win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }
    XftDraw *xd = XftDrawCreate(dpy, g_win, DefaultVisual(dpy, screen), cmap);
    xft = xd;
    redraw();

    while (!g_quit) {
        if (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0) redraw();
                break;
            case ButtonPress:
                if (ev.xbutton.button == 1) {
                    int px = ev.xbutton.x, py = ev.xbutton.y;
                    if (px >= g_close_x && px < g_close_x + g_close_w &&
                        py >= g_close_y && py < g_close_y + g_close_h) {
                        g_quit = 1;
                    } else if (py < g_chrome_h) {
                        g_dragging = 1;
                        g_drag_last_x = ev.xbutton.x_root;
                        g_drag_last_y = ev.xbutton.y_root;
                    } else {
                        g_quit = 1; /* click anywhere in the body closes */
                    }
                }
                break;
            case ButtonRelease:
                g_dragging = 0;
                break;
            case MotionNotify:
                if (g_dragging) {
                    int dx = ev.xmotion.x_root - g_drag_last_x;
                    int dy = ev.xmotion.y_root - g_drag_last_y;
                    g_drag_last_x = ev.xmotion.x_root;
                    g_drag_last_y = ev.xmotion.y_root;
                    g_win_x += dx; g_win_y += dy;
                    XMoveWindow(dpy, g_win, g_win_x, g_win_y);
                }
                break;
            case KeyPress:
                {
                    KeySym ks = XLookupKeysym(&ev.xkey, 0);
                    if (ks == XK_Escape) g_quit = 1;
                    else g_quit = 1; /* any key closes (simple popup) */
                }
                break;
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == g_wm_delete) g_quit = 1;
                break;
            default:
                break;
            }
        } else {
            struct timeval tv = {0, 200000};
            select(0, NULL, NULL, NULL, &tv);
            poll_agent_relay();
        }
    }

    for (int i = 0; i < g_n; i++)
        if (g_lines[i].font) XftFontClose(dpy, g_lines[i].font);
    XftDrawDestroy(xd);
    XDestroyWindow(dpy, g_win);
    XCloseDisplay(dpy);
    return 0;
}
