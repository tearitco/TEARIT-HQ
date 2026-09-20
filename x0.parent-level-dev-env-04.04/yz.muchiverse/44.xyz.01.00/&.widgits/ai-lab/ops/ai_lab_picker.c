/* ai_lab_picker - a real, standalone "Add State" picker for h-ai-lab's
 * FSM viewer. Direct live design ask: "i wish it was more user
 * friendly like canvas-craft/events-hq... any similar ux design
 * ideas in line with our vision?" - direct follow-up choice: a real,
 * dedicated picker app, matching events-hq's own real Add Command
 * picker shape (pick a TYPE, then fill in plain-English fields),
 * not the generic <repeat>/action= vocabulary this house otherwise
 * holds to. events-hq's own picker turned out to be bespoke C
 * INSIDE the shared renderer (a g_is_db_hq-style exception) - this
 * house's OTHER real precedent for "a genuinely new interaction that
 * doesn't fit the generic vocabulary" is instead a small, separate,
 * self-contained binary (tile-picker's own tp_picker_window.c,
 * digit-key-select + Enter-commits, GLX there; plain Xlib+Xft here,
 * matching khtpm_core_render.c's own drawing style for visual
 * consistency without pulling in a GL dependency this doesn't need).
 *
 * Real flow, three real screens:
 *   1. TYPE   - "Terminal state" / "Linear step" / "Branch" (digit
 *      1-3). Framed in plain English, not FSM jargon - matches
 *      events-hq's own LABEL/FIELD1/FIELD2 plain-prompt discipline.
 *   2. NAME   - type the new state's name (real text edit: printable
 *      chars append, Backspace removes, Enter advances).
 *   3. NEXT   - skipped entirely for "Terminal state". Shows every
 *      REAL existing state name from the target fsm_table.pdl as a
 *      real, numbered, clickable-by-digit list (toggle on/off) - so
 *      a beginner picks real, valid targets instead of typing state
 *      names from memory and risking a typo. Free text can still be
 *      appended (a state that will exist once this same batch of
 *      edits is done) - real flexibility, not a hard rail.
 * Enter on the NEXT screen (or NAME screen for a Terminal state)
 * commits: appends a real `STATE | name | NEXT=...` row directly to
 * the target fsm_table.pdl, refusing a duplicate name (same real
 * check ai_lab_scan.sh's create_state already does - kept here too
 * since this binary writes the file directly, not through that
 * script). Escape at any screen cancels with no write, same
 * convention tp_picker_window.c already uses.
 *
 * Usage: ai_lab_picker.+x <fsm_table_path> <instance_display_name>
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define WIN_W 520
#define WIN_H 420
#define PATH_BUF 4352
#define MAX_EXISTING 32
#define MAX_SELECTED 16
#define NAME_BUF 64

typedef enum { SCR_TYPE, SCR_NAME, SCR_NEXT } PickerScreen;

static char g_table_path[PATH_BUF];
static char g_instance_name[256];

static char g_existing[MAX_EXISTING][NAME_BUF];
static int g_n_existing = 0;

static int g_type = 0;                 /* 1=terminal 2=linear 3=branch */
static char g_name[NAME_BUF] = "";
static int g_name_len = 0;
static int g_selected[MAX_EXISTING];   /* parallel to g_existing, 1=picked */
static char g_freetext[NAME_BUF] = ""; /* typed extra NEXT name, not in list */
static int g_freetext_len = 0;
static char g_error[256] = "";

static PickerScreen g_screen = SCR_TYPE;

static int load_existing(void) {
    FILE *f = fopen(g_table_path, "r");
    if (!f) return 0;
    char line[512];
    int n = 0;
    while (n < MAX_EXISTING && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *bar1 = strchr(line, '|');
        if (!bar1) continue;
        char *bar2 = strchr(bar1 + 1, '|');
        if (!bar2) continue;
        size_t len = (size_t)(bar2 - (bar1 + 1));
        if (len >= NAME_BUF) len = NAME_BUF - 1;
        char nm[NAME_BUF];
        memcpy(nm, bar1 + 1, len);
        nm[len] = 0;
        /* trim */
        char *s = nm; while (*s == ' ') s++;
        char *e = s + strlen(s); while (e > s && e[-1] == ' ') e--;
        *e = 0;
        if (!s[0]) continue;
        snprintf(g_existing[n], sizeof(g_existing[0]), "%s", s);
        n++;
    }
    fclose(f);
    g_n_existing = n;
    return n;
}

static int name_exists(const char *nm) {
    int i;
    for (i = 0; i < g_n_existing; i++)
        if (strcmp(g_existing[i], nm) == 0) return 1;
    return 0;
}

/* commits the real STATE row. Returns 1 on success (caller exits),
 * 0 on a real, shown validation failure (caller stays on screen). */
static int commit_state(void) {
    if (!g_name[0]) { snprintf(g_error, sizeof(g_error), "type a name first"); return 0; }
    if (name_exists(g_name)) { snprintf(g_error, sizeof(g_error), "'%s' already exists - pick a different name", g_name); return 0; }
    char next_buf[512] = "";
    int i, first = 1;
    for (i = 0; i < g_n_existing; i++) {
        if (!g_selected[i]) continue;
        if (!first) strncat(next_buf, ",", sizeof(next_buf) - strlen(next_buf) - 1);
        strncat(next_buf, g_existing[i], sizeof(next_buf) - strlen(next_buf) - 1);
        first = 0;
    }
    if (g_freetext_len > 0) {
        if (!first) strncat(next_buf, ",", sizeof(next_buf) - strlen(next_buf) - 1);
        strncat(next_buf, g_freetext, sizeof(next_buf) - strlen(next_buf) - 1);
    }
    FILE *f = fopen(g_table_path, "a");
    if (!f) { snprintf(g_error, sizeof(g_error), "can't write %s", g_table_path); return 0; }
    fprintf(f, "STATE | %-14s | NEXT=%s\n", g_name, next_buf);
    fclose(f);
    return 1;
}

static XftFont *g_font = NULL;
static XftFont *g_font_small = NULL;

static void draw_line(Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap,
                       XftFont *font, int x, int y, const char *color, const char *s) {
    XftColor col;
    XftColorAllocName(dpy, vis, cmap, color, &col);
    XftDrawStringUtf8(xd, &col, font, x, y, (const FcChar8 *)s, (int)strlen(s));
    XftColorFree(dpy, vis, cmap, &col);
}

static void render(Display *dpy, Window win, XftDraw *xd, Visual *vis, Colormap cmap, GC gc) {
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XFillRectangle(dpy, win, gc, 0, 0, WIN_W, WIN_H);

    char hdr[300];
    snprintf(hdr, sizeof(hdr), "Add a new state to %s", g_instance_name);
    draw_line(dpy, xd, vis, cmap, g_font, 16, 28, "#e8e8e8", hdr);

    if (g_screen == SCR_TYPE) {
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, 60, "#8a8a8a", "What kind of state is this? (press 1, 2, or 3)");
        draw_line(dpy, xd, vis, cmap, g_font, 30, 100, "#f0d9a0", "1) Terminal state - nothing happens after this one");
        draw_line(dpy, xd, vis, cmap, g_font, 30, 135, "#f0d9a0", "2) Linear step - always moves on to ONE next state");
        draw_line(dpy, xd, vis, cmap, g_font, 30, 170, "#f0d9a0", "3) Branch - can move on to more than one possible state");
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, WIN_H - 20, "#6a6a6a", "Escape to cancel");
    } else if (g_screen == SCR_NAME) {
        const char *kind = g_type == 1 ? "Terminal state" : g_type == 2 ? "Linear step" : "Branch";
        char sub[200]; snprintf(sub, sizeof(sub), "%s - type its name, then Enter", kind);
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, 60, "#8a8a8a", sub);
        char shown[NAME_BUF + 4];
        snprintf(shown, sizeof(shown), "%s_", g_name);
        draw_line(dpy, xd, vis, cmap, g_font, 30, 110, "#eaffea", shown);
        if (g_error[0]) draw_line(dpy, xd, vis, cmap, g_font_small, 16, 150, "#ff8a8a", g_error);
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, WIN_H - 20, "#6a6a6a", "Enter to continue, Escape to go back");
    } else { /* SCR_NEXT */
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, 60, "#8a8a8a",
                  "Pick which real state(s) come next (press a number to toggle), then Enter");
        int y = 95, i;
        for (i = 0; i < g_n_existing; i++) {
            char row[NAME_BUF + 8];
            snprintf(row, sizeof(row), "%s%d) %s", g_selected[i] ? "[x] " : "[ ] ", i + 1, g_existing[i]);
            draw_line(dpy, xd, vis, cmap, g_font, 30, y, g_selected[i] ? "#eaffea" : "#d8d8b0", row);
            y += 26;
            if (y > WIN_H - 90) break;
        }
        char ft[NAME_BUF + 40];
        snprintf(ft, sizeof(ft), "or type a not-yet-created state name: %s_", g_freetext);
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, WIN_H - 55, "#8a8a8a", ft);
        if (g_error[0]) draw_line(dpy, xd, vis, cmap, g_font_small, 16, WIN_H - 35, "#ff8a8a", g_error);
        draw_line(dpy, xd, vis, cmap, g_font_small, 16, WIN_H - 15, "#6a6a6a", "Enter to add this state, Escape to go back");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ai_lab_picker.+x <fsm_table_path> <instance_display_name>\n");
        return 2;
    }
    snprintf(g_table_path, sizeof(g_table_path), "%s", argv[1]);
    snprintf(g_instance_name, sizeof(g_instance_name), "%s", argv[2]);
    load_existing();

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "ai_lab_picker: cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 200, 200, WIN_W, WIN_H, 1,
                                      BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "Add State");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask | FocusChangeMask);
    XMapRaised(dpy, win);
    XSetWindowAttributes swa;
    swa.override_redirect = False;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &swa);

    Visual *vis = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XftDraw *xd = XftDrawCreate(dpy, win, vis, cmap);
    GC gc = XCreateGC(dpy, win, 0, NULL);

    g_font = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=15");
    if (!g_font) g_font = XftFontOpenName(dpy, screen, "fixed");
    g_font_small = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=12");
    if (!g_font_small) g_font_small = g_font;

    XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);

    int running = 1;
    while (running) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == Expose || ev.type == FocusIn) {
            render(dpy, win, xd, vis, cmap, gc);
        } else if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            g_error[0] = 0;
            if (ks == XK_Escape) {
                if (g_screen == SCR_TYPE) { running = 0; }
                else if (g_screen == SCR_NAME) { g_screen = SCR_TYPE; g_name[0] = 0; g_name_len = 0; }
                else { g_screen = SCR_NAME; }
            } else if (g_screen == SCR_TYPE) {
                if (ks == XK_1) { g_type = 1; g_screen = SCR_NAME; }
                else if (ks == XK_2) { g_type = 2; g_screen = SCR_NAME; }
                else if (ks == XK_3) { g_type = 3; g_screen = SCR_NAME; }
            } else if (g_screen == SCR_NAME) {
                if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (!g_name[0]) { snprintf(g_error, sizeof(g_error), "type a name first"); }
                    else if (name_exists(g_name)) { snprintf(g_error, sizeof(g_error), "'%s' already exists", g_name); }
                    else if (g_type == 1) { if (commit_state()) running = 0; }
                    else { g_screen = SCR_NEXT; }
                } else if (ks == XK_BackSpace) {
                    if (g_name_len > 0) g_name[--g_name_len] = 0;
                } else {
                    char buf[8]; int n = XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL);
                    if (n == 1 && buf[0] >= 32 && buf[0] < 127 && g_name_len < NAME_BUF - 1) {
                        g_name[g_name_len++] = buf[0]; g_name[g_name_len] = 0;
                    }
                }
            } else { /* SCR_NEXT */
                if (ks >= XK_1 && ks <= XK_9) {
                    int idx = (int)(ks - XK_1);
                    if (idx < g_n_existing) g_selected[idx] = !g_selected[idx];
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (commit_state()) running = 0;
                } else if (ks == XK_BackSpace) {
                    if (g_freetext_len > 0) g_freetext[--g_freetext_len] = 0;
                } else {
                    char buf[8]; int n = XLookupString(&ev.xkey, buf, sizeof(buf), NULL, NULL);
                    if (n == 1 && buf[0] >= 32 && buf[0] < 127 && g_freetext_len < NAME_BUF - 1) {
                        g_freetext[g_freetext_len++] = buf[0]; g_freetext[g_freetext_len] = 0;
                    }
                }
            }
            if (running) render(dpy, win, xd, vis, cmap, gc);
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XftDrawDestroy(xd);
    XCloseDisplay(dpy);
    return 0;
}
