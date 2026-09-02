/* vvarware-report - right-sidebar live progress report for the vvarware
 * monad. Tails the monad's master_ledger.txt (last N lines) and redraws
 * master_ledger.txt (last N lines) and redraws on a timer, so the bot's
 * progress is always visible on the right side of the desktop.
 *
 * KISS v1: same file-mediated pattern as every other widgit - own
 * binary, reads the ledger file, redraws. X11 only (Linux primary).
 *
 * Usage: vvarware_report.+x <house_root>
 *
 * Windows compatibility is deferred (see khtpm-vs-khtpm.md - khtpm_*
 * plat_win variants hold the Win32 ports when needed).
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define MAX_LINES 32
#define MAX_CHARS 160
#define POLL_USEC 1000000
#define PANEL_W 420

static volatile sig_atomic_t g_run = 1;
static void on_sig(int s) { (void)s; g_run = 0; }

static unsigned long alloc_color_or(Display *dpy, const char *name,
                                    unsigned long fallback) {
    XColor c, exact;
    if (XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
                         name, &c, &exact))
        return c.pixel;
    return fallback;
}

static int load_theme(const char *house_root, char *bg, size_t bg_sz,
                      char *fg, size_t fg_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/theme.txt", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    snprintf(bg, bg_sz, "#222222");
    snprintf(fg, fg_sz, "#eeeeee");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = '\0';
        if (strcmp(line, "bg") == 0) snprintf(bg, bg_sz, "%s", v);
        else if (strcmp(line, "fg") == 0) snprintf(fg, fg_sz, "%s", v);
    }
    fclose(f);
    return 1;
}

/* read_tail - load up to MAX_LINES of the newest lines of a file into
 * the given array (newest at index 0). Returns the count. */
static int read_tail(const char *path, char lines[MAX_LINES][MAX_CHARS]) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *buf = malloc(262144);
    if (!buf) { fclose(f); return 0; }
    size_t n = fread(buf, 1, 262143, f);
    buf[n] = '\0';
    fclose(f);

    /* split into lines */
    char *sp[4096];
    int nsp = 0;
    char *tok = strtok(buf, "\n");
    while (tok && nsp < 4096) { sp[nsp++] = tok; tok = strtok(NULL, "\n"); }

    int take = nsp > MAX_LINES ? MAX_LINES : nsp;
    for (int i = 0; i < take; i++) {
        snprintf(lines[i], MAX_CHARS, "%s", sp[nsp - 1 - i]);
    }
    free(buf);
    return take;
}

/* wrap_draw - draw a long line wrapped to the panel width. Returns y
 * after drawing. */
static int draw_wrapped(Display *dpy, Window win, GC gc, int x0, int y0,
                        int width, int line_h, const char *s) {
    int y = y0;
    size_t len = strlen(s);
    int max_chars_per_line = width / 7;
    if (max_chars_per_line < 10) max_chars_per_line = 10;
    size_t off = 0;
    while (off < len) {
        size_t take = len - off;
        if (take > (size_t)max_chars_per_line) take = max_chars_per_line;
        XDrawString(dpy, win, gc, x0, y, s + off, (int)take);
        y += line_h;
        off += take;
        if (y > 1200) break;
    }
    return y;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: vvarware_report.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];

    char ledger_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path),
             "%s/*.monads/*.hard-vvar-agent-Q0000/pieces/brain/master_ledger.txt",
             house_root);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "vvarware-report: cannot open display\n"); return 1; }

    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));

    char theme_bg[32] = "#222222", theme_fg[32] = "#eeeeee";
    load_theme(house_root, theme_bg, sizeof(theme_bg), theme_fg, sizeof(theme_fg));
    unsigned long bg = alloc_color_or(dpy, theme_bg, WhitePixel(dpy, DefaultScreen(dpy)));
    unsigned long fg = alloc_color_or(dpy, theme_fg, BlackPixel(dpy, DefaultScreen(dpy)));

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.event_mask = ExposureMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               screen_w - PANEL_W, 40, PANEL_W, screen_h - 120, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, fg);
    XSetBackground(dpy, gc, bg);

    signal(SIGTERM, on_sig);
    signal(SIGINT, on_sig);

    char lines[MAX_LINES][MAX_CHARS];
    int last_n = -1;
    int need_redraw = 1;

    XFlush(dpy);

    while (g_run) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) need_redraw = 1;
            else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape || ks == XK_q || ks == XK_Q) g_run = 0;
            }
        }
        if (g_run) {
            int n = read_tail(ledger_path, lines);
            if (n != last_n) { last_n = n; need_redraw = 1; }
            else if (n > 0) {
                /* cheap mtime check to catch same-count edits */
                static time_t last_mt = 0;
                struct stat st;
                if (stat(ledger_path, &st) == 0 && st.st_mtime != last_mt) {
                    last_mt = st.st_mtime;
                    need_redraw = 1;
                }
            }
        }
        if (need_redraw) {
            XSetForeground(dpy, gc, bg);
            XFillRectangle(dpy, win, gc, 0, 0, PANEL_W, screen_h - 120);
            XSetForeground(dpy, gc, fg);
            char title[128];
            snprintf(title, sizeof(title), "vvarware report (%d lines)", last_n);
            XDrawString(dpy, win, gc, 8, 18, title, (int)strlen(title));
            int y = 40;
            for (int i = 0; i < last_n && i < MAX_LINES; i++) {
                y = draw_wrapped(dpy, win, gc, 8, y, PANEL_W - 16, 16, lines[i]);
                y += 8;
                if (y > screen_h - 150) break;
            }
            XFlush(dpy);
            need_redraw = 0;
        }
        usleep(POLL_USEC);
    }

    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
