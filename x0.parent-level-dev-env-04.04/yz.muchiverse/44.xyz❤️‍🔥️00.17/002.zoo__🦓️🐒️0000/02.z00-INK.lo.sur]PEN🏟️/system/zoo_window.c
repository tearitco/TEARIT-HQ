/* zoo_window.c - shaped GL desktop window, ONE PER PIECE, direct port of
 * real egg-pals' own system/egg_window.c (read in full before writing
 * this - X11 Shape Extension + GLX, borderless override_redirect,
 * hand-rolled drag-then-snap-to-grid). Base technique unchanged; what's
 * different from the reference and why is listed below, not silently
 * assumed to be identical.
 *
 * WHAT'S DIFFERENT FROM egg_window.c, AND WHY:
 *
 * 1. Position field: this project's pieces already have pos_x/pos_y
 *    (used by ops/move_entity.c and ops/compose_frame.c's ASCII
 *    rendering) - there is no separate grid_x/grid_y field the way
 *    egg-pals' pets have (egg-pals has NO terminal/ascii map at all;
 *    a pet's position IS its desktop grid cell, nothing else). Reusing
 *    pos_x/pos_y directly means the ASCII terminal view and this
 *    desktop window are two RENDERINGS of the exact same field, always
 *    in sync automatically: move a pet in the terminal, its window
 *    moves; drag its window and drop it, the terminal's very next
 *    compose_frame shows it at the new tile. This was a deliberate
 *    design choice, not an oversight - see dox/
 *    pet-import-export-standard.md for the fuller reasoning.
 *
 * 2. Drop bounds are the ZOO'S OWN map dimensions (read from
 *    pieces/world_01/map_zoo/state.txt's width/height - 20x12 today),
 *    NOT the full desktop screen's grid (that would let a dragged
 *    pet's pos_x/pos_y exceed the ASCII map's own bounds, breaking
 *    collision/rendering there). Concretely: only a 1600x960 pixel
 *    region in the top-left corner of the desktop (columns 0-19, rows
 *    0-11 of the shared 80px desktop grid - see dox/
 *    desk-grid-prompt.txt) is actually draggable-into for a zoo pet.
 *    Real egg-pals pets have no such constraint (no ascii map to stay
 *    consistent with), so their own drop bounds really are the whole
 *    screen - this is a genuine, deliberate narrowing for this
 *    project, not a bug to "fix" by widening it.
 *
 * 3. No self-tick. egg_window.c self-ticks its own pet independently
 *    (`ops/+x/tick_pets.+x <pet_id>`) so a pet keeps living even after
 *    its window's terminal session ends - egg-pals' own tick_pets.c
 *    supports a single-pet argument for exactly this. This project's
 *    ops/tick_pets.c does NOT take a piece_id argument (it always
 *    scans and ticks the WHOLE map in one pass, run once per terminal
 *    tick from pal/main_loop.pal) - calling it per-window, per-poll,
 *    with no argument would move EVERY pet on every open window's own
 *    timer, causing double-ticking/races the moment more than one
 *    window is open. This window is purely a visual + drag mirror,
 *    matching egg_window.c's own "dumb renderer + input relay" rule
 *    (its own header comment) - world-ticking stays the terminal
 *    session's job alone, not duplicated here.
 *
 * 4. No facing-flip/tick_seq/poop/sleep-overlay handling - none of
 *    those fields exist on a zoo_0000 pet yet (hunger/happiness/energy
 *    only). Simplified to: redraw unconditionally every poll tick
 *    (~300ms, same interval as the reference) rather than gating on a
 *    tick_seq change - fine at this poll rate, avoids needing to add
 *    fields this project doesn't otherwise use. A real sprite (not the
 *    circle fallback) IS supported - see pieces/world_01/map_zoo/
 *    pet_rex/sprite.csv and pet_mochi/sprite.csv, generated via the
 *    same real FreeType-based emoji_gen_atlas.c/emoji_xtract.c pipeline
 *    already proven working this session (see mutaclsym's own GL emoji
 *    work for the fuller citation of that pipeline) - a real dog/cat
 *    sprite, not a placeholder.
 *
 * 5. X11/GLX only - this project has no Windows build target yet
 *    (unlike egg-pals, which supports both), so the reference's
 *    _WIN32 branch was not ported. Add it the same way egg_window.c
 *    does if a Windows build is ever needed here.
 *
 * Usage: zoo_window.+x <piece_id>
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GRID_CELL_PX 80
#define WIDTH GRID_CELL_PX
#define HEIGHT GRID_CELL_PX
#define PROJ_MAX_PATH 4096
#define PATH_BUF (PROJ_MAX_PATH + 256)
#define POLL_INTERVAL_USEC 300000
#define MAP_ID "map_zoo"

static char project_root[PROJ_MAX_PATH] = ".";
static GLuint g_texture = 0;
static int g_has_texture = 0;
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[256];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

/* Same find-or-append single-key writer as ops/xlector_input.c's own
 * set_kv_int() - the SAME state.txt this writes to is also read by
 * ops/compose_frame.c/move_entity.c, so the write must be safe against
 * the real newline-in-place-mutation bug this project family already
 * hit once (see mutaclsym's own dox/00-HANDOFF.md) - values are always
 * written fresh via snprintf, never stripped-in-place. */
static void write_pos(const char *path, int pos_x, int pos_y) {
    char lines[64][256];
    int nlines = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], sizeof(lines[0]), f)) nlines++;
        fclose(f);
    }
    int seen_x = 0, seen_y = 0;
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "pos_x") == 0) { fprintf(f, "pos_x=%d\n", pos_x); seen_x = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "pos_y") == 0) { fprintf(f, "pos_y=%d\n", pos_y); seen_y = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!seen_x) fprintf(f, "pos_x=%d\n", pos_x);
    if (!seen_y) fprintf(f, "pos_y=%d\n", pos_y);
    fclose(f);
}

static int load_sprite(const char *csv_path, unsigned char **out_pixels, int *out_res) {
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
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r;
            pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b;
            pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return 0; }
    *out_pixels = pixels;
    *out_res = res;
    return 1;
}

static void upload_texture(const unsigned char *pixels, int res, GLuint *tex_out) {
    glGenTextures(1, tex_out);
    glBindTexture(GL_TEXTURE_2D, *tex_out);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

static void draw_circle(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.2f, 0.6f, 0.9f);
    glVertex2f(0.0f, 0.0f);
    for (int i = 0; i <= 100; i++) {
        float angle = i * 2.0f * (float)M_PI / 100;
        glVertex2f(cosf(angle), sinf(angle));
    }
    glEnd();
}

static void draw_sprite(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* Same alpha-channel shape-mask technique as egg_window.c's own
 * build_shape_mask() - just no facing-flip parameter, since zoo_0000
 * pets have no facing field yet (item 4 in this file's header). */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIDTH, HEIGHT);
    XSetForeground(dpy, mask_gc, 1);
    if (g_sprite_pixels) {
        for (int y = 0; y < HEIGHT; y++) {
            int sy = (y * g_sprite_res) / HEIGHT;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            for (int x = 0; x < WIDTH; x++) {
                int sx = (x * g_sprite_res) / WIDTH;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127) {
                    XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
                }
            }
        }
    } else {
        XFillArc(dpy, mask, mask_gc, 0, 0, WIDTH, HEIGHT, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

#define POPUP_W 100
#define POPUP_H 32

static Window open_context_menu(Display *dpy, GC popup_gc, int root_x, int root_y) {
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    swa.event_mask = ExposureMask | ButtonPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  root_x, root_y, POPUP_W, POPUP_H, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(dpy, popup);
    XSetForeground(dpy, popup_gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
    return popup;
}

static void draw_context_menu(Display *dpy, Window popup, GC popup_gc) {
    XClearWindow(dpy, popup);
    XDrawRectangle(dpy, popup, popup_gc, 0, 0, POPUP_W - 1, POPUP_H - 1);
    XDrawString(dpy, popup, popup_gc, 14, POPUP_H / 2 + 4, "Close", 5);
}

static void close_context_menu(Display *dpy, Window popup) {
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, popup);
}

int main(int argc, char **argv) {
    const char *piece_id = (argc >= 2) ? argv[1] : NULL;
    resolve_root();

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;
    swa.override_redirect = True;

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, WIDTH, HEIGHT,
                              0, vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask | CWOverrideRedirect, &swa);

    XMapWindow(dpy, win);
    XStoreName(dpy, win, piece_id ? piece_id : "zoo_window");

    Pixmap mask = XCreatePixmap(dpy, win, WIDTH, HEIGHT, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);

    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);

    glClearColor(0.95f, 0.90f, 0.80f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    char state_path[PATH_BUF] = "";
    if (piece_id) {
        snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/%s/%s/state.txt", project_root, MAP_ID, piece_id);
        char sprite_path[PATH_BUF];
        snprintf(sprite_path, sizeof(sprite_path), "%s/pieces/world_01/%s/%s/sprite.csv", project_root, MAP_ID, piece_id);
        if (load_sprite(sprite_path, &g_sprite_pixels, &g_sprite_res)) {
            upload_texture(g_sprite_pixels, g_sprite_res, &g_texture);
            g_has_texture = 1;
        }
    }

    /* Drop bounds are the ZOO'S OWN map dimensions, not the desktop's -
     * see this file's own header comment item 2 for why. */
    char map_state_path[PATH_BUF];
    snprintf(map_state_path, sizeof(map_state_path), "%s/pieces/world_01/%s/state.txt", project_root, MAP_ID);
    int map_w = read_kv_int(map_state_path, "width", 20);
    int map_h = read_kv_int(map_state_path, "height", 12);
    int max_col = map_w - 1;
    int max_row = map_h - 1;
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;

    int xfd = ConnectionNumber(dpy);
    build_shape_mask(dpy, win, gc, mask);

    Window popup_win = 0;
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    XEvent xev;
    int drag_start_x = 0, drag_start_y = 0;
    int pos_x = 0, pos_y = 0;
    int win_start_x = 0, win_start_y = 0;
    int dragging = 0;
    int running = 1;
    int have_position = 0;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = POLL_INTERVAL_USEC;
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XNextEvent(dpy, &xev);

            if (xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc);
            } else if (xev.type == Expose) {
                glViewport(0, 0, WIDTH, HEIGHT);
                if (g_has_texture) draw_sprite(); else draw_circle();
                glXSwapBuffers(dpy, win);
            } else if (popup_win && xev.type == ButtonPress) {
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < POPUP_W &&
                             xev.xbutton.y >= 0 && xev.xbutton.y < POPUP_H;
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                if (inside) { running = 0; break; }
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                popup_win = open_context_menu(dpy, popup_gc, xev.xbutton.x_root, xev.xbutton.y_root);
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                pos_x = (win_start_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                pos_y = (win_start_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                pos_x = clampi(pos_x, 0, max_col);
                pos_y = clampi(pos_y, 0, max_row);
                win_start_x = pos_x * GRID_CELL_PX;
                win_start_y = pos_y * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_start_x, win_start_y);
                if (piece_id) write_pos(state_path, pos_x, pos_y);
            } else if (xev.type == MotionNotify) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_start_x += dx;
                win_start_y += dy;
                XMoveWindow(dpy, win, win_start_x, win_start_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
            } else if (xev.type == KeyPress) {
                running = 0;
                break;
            }
        }
        if (!running) break;

        /* No self-tick here - see this file's own header comment item 3.
         * Just re-read position (the terminal session's own tick_pets/
         * xlector_input/move_entity are the only things that change it)
         * and re-clamp/redraw every poll interval. */
        if (piece_id) {
            int new_x = clampi(read_kv_int(state_path, "pos_x", pos_x), 0, max_col);
            int new_y = clampi(read_kv_int(state_path, "pos_y", pos_y), 0, max_row);
            if (!have_position || new_x != pos_x || new_y != pos_y) {
                have_position = 1;
                if (!dragging) {
                    pos_x = new_x;
                    pos_y = new_y;
                    win_start_x = pos_x * GRID_CELL_PX;
                    win_start_y = pos_y * GRID_CELL_PX;
                    XMoveWindow(dpy, win, win_start_x, win_start_y);
                }
            }
        }

        glViewport(0, 0, WIDTH, HEIGHT);
        if (g_has_texture) draw_sprite(); else draw_circle();
        glXSwapBuffers(dpy, win);
    }

    if (popup_win) close_context_menu(dpy, popup_win);
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
