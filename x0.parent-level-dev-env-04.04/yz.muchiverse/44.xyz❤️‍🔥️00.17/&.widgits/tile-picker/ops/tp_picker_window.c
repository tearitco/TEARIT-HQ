/* tp_picker_window - the missing "picker" itself: a real GL window
 * listing glyph choices by index, so a person (or an automated key-
 * injection test, see scenarios/) can actually pick one instead of only
 * ever driving tp_set_brush/tp_place_desktop from a raw shell command.
 *
 * Usage: tp_picker_window.+x <widget_state_dir> <desktop_root> [items_file]
 * items_file defaults to pieces/system/picker_items.txt next to this
 * binary's own project root - a real, data-driven METHOD-style table
 * (SECTION|INDEX|GLYPH rows), same "read a .pdl-style table instead of
 * hardcoding a switch" convention pc_menu_input.c's own load_menu_items()
 * already uses (see aomorai-editor-blueprint.md §8) - extending the list
 * later means editing this file, not recompiling.
 *
 * Digit keys 1-9 move the highlighted selection to that index (direct
 * jump, not incremental - same one-keystroke-selects convention as this
 * house's other numbered menus, e.g. active_panel's panel_cursor/
 * panel_digit_accum in mutaclysm's own move_player.c/choice.c). Enter
 * commits: shells out to tp_set_brush.+x then tp_place_desktop.+x for the
 * selected glyph - reusing those ops exactly as a human typing the same
 * two commands would, not reimplementing their logic here. Escape or
 * right-click cancels/closes with no placement.
 *
 * "each unit has an index so u can do k3 testing" (direct instruction,
 * 2026-08-04): index 3 is reachable by injecting a '3' keypress (e.g. via
 * XTestFakeKeyEvent - xdotool is not installed on this machine, XTest is
 * the available substitute, see scenarios/test_tile_picker_pick.sh),
 * making "pick item 3" a scriptable, provable action, not something that
 * only a human clicking can trigger.
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "self_exe.h" /* macOS leg: portable /proc/self/exe replacement */

#define WIN_W 220
#define WIN_H 240
#define POLL_INTERVAL_USEC 100000
#define PATH_BUF 4352
#define MAX_ITEMS 9

typedef struct {
    int index;
    char glyph;
} PickerItem;

static PickerItem g_items[MAX_ITEMS];
static int g_item_count = 0;

static int load_items(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int n = 0;
    while (n < MAX_ITEMS && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ITEM", 4) != 0) continue;
        int idx = 0;
        char glyph = 0;
        /* ITEM         | 1                  | # */
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        idx = atoi(p);
        p = strchr(p, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        glyph = *p;
        if (idx > 0 && glyph >= 32 && glyph <= 126) {
            g_items[n].index = idx;
            g_items[n].glyph = glyph;
            n++;
        }
    }
    fclose(f);
    g_item_count = n;
    return n > 0;
}

static GLuint g_font_base = 0;
static int g_font_loaded = 0;

static int load_font(Display *dpy) {
    XFontStruct *fi = XLoadQueryFont(dpy, "-sony-fixed-medium-r-normal--24-170-100-100-c-120-iso8859-1");
    if (!fi) fi = XLoadQueryFont(dpy, "fixed");
    if (!fi) return 0;
    g_font_base = glGenLists(256);
    glXUseXFont(fi->fid, 0, 256, g_font_base);
    return 1;
}

static void draw_text(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    glListBase(g_font_base);
    glCallLists((int)strlen(s), GL_UNSIGNED_BYTE, s);
}

static void render(int selected) {
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!g_font_loaded) return;
    glColor3f(0.9f, 0.9f, 0.95f);
    for (int i = 0; i < g_item_count; i++) {
        char line[32];
        int is_sel = (g_items[i].index == selected);
        snprintf(line, sizeof(line), "%s%d: %c", is_sel ? "> " : "  ",
                 g_items[i].index, g_items[i].glyph);
        if (is_sel) glColor3f(1.0f, 0.85f, 0.2f);
        else glColor3f(0.85f, 0.85f, 0.9f);
        draw_text(-0.9f, 0.85f - i * 0.22f, line);
    }
}

/* Reuses tp_set_brush.+x + tp_place_desktop.+x exactly as a human typing
 * the same two commands would - see this file's own header comment. */
static void commit_selection(const char *ops_dir, const char *wdir,
                              const char *desk, char glyph) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s/tp_set_brush.+x' '%s' '%c' >/dev/null 2>&1",
             ops_dir, wdir, glyph);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "'%s/tp_place_desktop.+x' '%s' '%s' >/dev/null 2>&1",
             ops_dir, wdir, desk);
    system(cmd);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tp_picker_window.+x <widget_state_dir> <desktop_root> [items_file]\n");
        return 1;
    }
    const char *wdir = argv[1];
    const char *desk = argv[2];

    char self_path[PATH_BUF], ops_dir[PATH_BUF];
    ssize_t len = self_exe_readlink(self_path, sizeof(self_path));
    if (len <= 0) { fprintf(stderr, "tp_picker_window: cannot resolve own path\n"); return 1; }
    self_path[len] = '\0';
    {
        char *slash = strrchr(self_path, '/');
        if (slash) *slash = '\0';
        snprintf(ops_dir, sizeof(ops_dir), "%s", self_path);
    }

    char items_path[PATH_BUF];
    if (argc >= 4 && argv[3][0]) {
        snprintf(items_path, sizeof(items_path), "%s", argv[3]);
    } else {
        /* ops/+x/../../pieces/system/picker_items.txt */
        snprintf(items_path, sizeof(items_path), "%s/../../pieces/system/picker_items.txt", ops_dir);
    }
    if (!load_items(items_path)) {
        fprintf(stderr, "tp_picker_window: could not load items from %s\n", items_path);
        return 1;
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "tp_picker_window: cannot open display\n"); return 1; }

    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);
    if (!vi) { fprintf(stderr, "tp_picker_window: no suitable visual\n"); return 1; }

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;
    swa.override_redirect = False; /* a real, titled, WM-managed window - this is the picker UI, not a desk-entity stamp */

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 400, 300, WIN_W, WIN_H,
                                0, vi->depth, InputOutput, vi->visual,
                                CWColormap | CWEventMask | CWOverrideRedirect, &swa);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "tile-picker");

    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    g_font_loaded = load_font(dpy);

    int selected = g_items[0].index;
    int xfd = ConnectionNumber(dpy);
    int running = 1;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (xev.type == Expose) {
                glViewport(0, 0, WIN_W, WIN_H);
                render(selected);
                glXSwapBuffers(dpy, win);
            } else if (xev.type == KeyPress) {
                char buf[8];
                KeySym ks;
                XLookupString(&xev.xkey, buf, sizeof(buf), &ks, NULL);
                if (ks >= XK_1 && ks <= XK_9) {
                    int digit = (int)(ks - XK_0);
                    for (int i = 0; i < g_item_count; i++)
                        if (g_items[i].index == digit) { selected = digit; break; }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    for (int i = 0; i < g_item_count; i++) {
                        if (g_items[i].index == selected) {
                            commit_selection(ops_dir, wdir, desk, g_items[i].glyph);
                            break;
                        }
                    }
                } else if (ks == XK_Escape) {
                    running = 0;
                }
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                running = 0;
            }
        }
        if (!running) break;
        glViewport(0, 0, WIN_W, WIN_H);
        render(selected);
        glXSwapBuffers(dpy, win);
    }

    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
