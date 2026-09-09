/* livedesk_splash - a small X11 "Building livedesk…" window shown while
 * build_khtpm_strip.sh recompiles. Replaces the zenity/xmessage dialog
 * (direct instruction 2026-09-09: "the popup should be x11 layout
 * style, not gl") with a house-style override-redirect box, themed from
 * #.desktop/livedesk_theme.pdl, that shows a real progress bar +
 * elapsed seconds.
 *
 * Progress is measured by WATCHING THE COMPILE directly (direct
 * instruction: "watching compile is most accurate"): at startup it
 * records which of the build's known output binaries already exist and
 * their mtimes; the bar fills as each one is (re)written by the running
 * gcc. A time term (elapsed / EXPECT_SECONDS) keeps the bar moving
 * during the one long silent unit (khtpm_core_render.c itself), and the
 * displayed % is the max of the two so it never stalls or lies about
 * being done.
 *
 * Usage: livedesk_splash <house_root> <ops_+x_dir>
 * Lifetime: exits on SIGTERM/SIGINT (build_khtpm_strip.sh's EXIT trap
 * kills it the instant the build ends), on all binaries fresh, or after
 * a hard safety timeout.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#define W 460
#define H 132
#define EXPECT_SECONDS 30.0
#define HARD_TIMEOUT_SECONDS 240

/* The binaries build_khtpm_strip.sh + build_core_render.sh produce.
 * Order roughly matches build order; the big silent one is
 * khtpm_core_render.+x. Missing entries just never count - safe. */
static const char *g_targets[] = {
    "khtpm_taskbar_manager_main.+x",
    "swatch_picker_manager.+x",
    "apply_theme_op.+x",
    "khtpm_core_render.+x",
    "tp_asset_to_sprite.+x",
    "khtpm_strip_render_ascii.+x",
    "khtpm_strip_keyboard_ascii.+x",
    "khtpm_render_ascii.+x",
    "khtpm_kbd_ascii.+x",
};
#define N_TARGETS ((int)(sizeof(g_targets) / sizeof(g_targets[0])))

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static void read_theme(const char *house, char *bg, char *fg, size_t n) {
    snprintf(bg, n, "%s", "#1c1c1c");
    snprintf(fg, n, "%s", "#e0e0e0");
    char path[4096];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|'); if (!p) continue; p++;
        while (*p == ' ') p++;
        char *e = strchr(p, '|'); if (!e) continue;
        char key[16]; size_t kl = (size_t)(e - p);
        while (kl && p[kl - 1] == ' ') kl--;
        if (kl >= sizeof(key)) continue;
        memcpy(key, p, kl); key[kl] = 0;
        char *v = e + 1; while (*v == ' ') v++;
        v[strcspn(v, " \r\n")] = 0;
        if (v[0] != '#') continue;
        if (!strcmp(key, "bg")) snprintf(bg, n, "%s", v);
        else if (!strcmp(key, "fg")) snprintf(fg, n, "%s", v);
    }
    fclose(f);
}

static const char *shade(const char *hex, int d) {
    static char out[8];
    int r = 0, g = 0, b = 0;
    if (sscanf(hex, "#%2x%2x%2x", &r, &g, &b) != 3) return hex;
    r += d; g += d; b += d;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    snprintf(out, sizeof(out), "#%02x%02x%02x", r, g, b);
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: livedesk_splash <house_root> <ops_+x_dir>\n"); return 2; }
    const char *house = argv[1];
    const char *xdir  = argv[2];

    signal(SIGTERM, on_sig);
    signal(SIGINT,  on_sig);

    /* baseline: mtime of each target that exists right now */
    time_t base_mtime[N_TARGETS];
    int base_exists[N_TARGETS];
    for (int i = 0; i < N_TARGETS; i++) {
        char p[4096]; struct stat st;
        snprintf(p, sizeof(p), "%s/%s", xdir, g_targets[i]);
        if (stat(p, &st) == 0) { base_exists[i] = 1; base_mtime[i] = st.st_mtime; }
        else { base_exists[i] = 0; base_mtime[i] = 0; }
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 0;               /* headless / no X - just no splash */
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int sw = DisplayWidth(dpy, scr), sh = DisplayHeight(dpy, scr);

    char bg_hex[16], fg_hex[16];
    read_theme(house, bg_hex, fg_hex, sizeof(bg_hex));

    Colormap cmap = DefaultColormap(dpy, scr);
    XColor c;
    unsigned long bg = BlackPixel(dpy, scr), fg = WhitePixel(dpy, scr),
                  trough = bg, barfill = fg, dim = fg;
    if (XParseColor(dpy, cmap, bg_hex, &c) && XAllocColor(dpy, cmap, &c)) bg = c.pixel;
    if (XParseColor(dpy, cmap, fg_hex, &c) && XAllocColor(dpy, cmap, &c)) fg = c.pixel;
    if (XParseColor(dpy, cmap, shade(bg_hex, 22), &c) && XAllocColor(dpy, cmap, &c)) trough = c.pixel;
    if (XParseColor(dpy, cmap, fg_hex, &c) && XAllocColor(dpy, cmap, &c)) barfill = c.pixel;
    if (XParseColor(dpy, cmap, shade(fg_hex, -70), &c) && XAllocColor(dpy, cmap, &c)) dim = c.pixel;

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.border_pixel = dim;
    swa.event_mask = ExposureMask;
    Window win = XCreateWindow(dpy, root, (sw - W) / 2, (sh - H) / 3, W, H, 1,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &swa);
    XStoreName(dpy, win, "livedesk");
    XMapRaised(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);
    XftDraw *xft = XftDrawCreate(dpy, win, DefaultVisual(dpy, scr), cmap);
    XftFont *fbig = XftFontOpenName(dpy, scr, "DejaVu Sans:pixelsize=15:bold");
    XftFont *fsm  = XftFontOpenName(dpy, scr, "DejaVu Sans:pixelsize=11");
    if (!fbig) fbig = XftFontOpenName(dpy, scr, "fixed");
    if (!fsm)  fsm  = fbig;
    XftColor xfg, xdimc;
    XftColorAllocName(dpy, DefaultVisual(dpy, scr), cmap, fg_hex, &xfg);
    XftColorAllocName(dpy, DefaultVisual(dpy, scr), cmap, shade(fg_hex, -70), &xdimc);

    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);

    for (;;) {
        if (g_stop) break;

        /* drain expose events */
        while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); }

        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) / 1e9;
        if (elapsed > HARD_TIMEOUT_SECONDS) break;

        /* how many targets have been (re)written since we started */
        int done = 0, last_done = -1;
        for (int i = 0; i < N_TARGETS; i++) {
            char p[4096]; struct stat st;
            snprintf(p, sizeof(p), "%s/%s", xdir, g_targets[i]);
            if (stat(p, &st) != 0) continue;
            if (!base_exists[i] || st.st_mtime > base_mtime[i]) { done++; last_done = i; }
        }
        double by_files = (double)done / (double)N_TARGETS;
        double by_time  = elapsed / EXPECT_SECONDS; if (by_time > 0.97) by_time = 0.97;
        double frac = by_files > by_time ? by_files : by_time;
        if (done >= N_TARGETS) frac = 1.0;
        if (frac > 1.0) frac = 1.0;

        /* paint */
        XSetForeground(dpy, gc, bg);
        XFillRectangle(dpy, win, gc, 0, 0, W, H);

        XftDrawStringUtf8(xft, &xfg, fbig, 20, 34,
                          (const FcChar8 *)"Building livedesk\xE2\x80\xA6", 20);

        int bx = 20, by = 58, bw = W - 40, bh = 20;
        XSetForeground(dpy, gc, trough);
        XFillRectangle(dpy, win, gc, bx, by, bw, bh);
        XSetForeground(dpy, gc, barfill);
        XFillRectangle(dpy, win, gc, bx, by, (int)(bw * frac + 0.5), bh);
        XSetForeground(dpy, gc, dim);
        XDrawRectangle(dpy, win, gc, bx, by, bw, bh);

        char sub[160];
        const char *step = (last_done >= 0 && frac < 1.0) ? g_targets[last_done]
                         : (frac >= 1.0 ? "done" : "compiling khtpm_core_render\xE2\x80\xA6");
        snprintf(sub, sizeof(sub), "%s", step);
        XftDrawStringUtf8(xft, &xdimc, fsm, 20, by + bh + 22,
                          (const FcChar8 *)sub, (int)strlen(sub));

        char right[64];
        snprintf(right, sizeof(right), "%d%%   %.0fs   %d/%d",
                 (int)(frac * 100 + 0.5), elapsed, done, N_TARGETS);
        XGlyphInfo gi;
        XftTextExtentsUtf8(dpy, fsm, (const FcChar8 *)right, (int)strlen(right), &gi);
        XftDrawStringUtf8(xft, &xdimc, fsm, W - 20 - gi.xOff, 34,
                          (const FcChar8 *)right, (int)strlen(right));

        XFlush(dpy);
        if (done >= N_TARGETS) { usleep(400000); break; }   /* let "100%" show a beat */
        usleep(180000);
    }

    XftColorFree(dpy, DefaultVisual(dpy, scr), cmap, &xfg);
    XftColorFree(dpy, DefaultVisual(dpy, scr), cmap, &xdimc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
