/* ie_main.c — Muchi Image Editor Phase-1 (Photoshop-shaped)
 *
 * freeglut UI + RGBA layers + brush/eraser/fill/rect + PNG via ffmpeg.
 * CPU-safe: ≤20fps UI, dirty-only composite, main-loop sleep, no busy spin.
 *
 *   sh button.sh r   Esc quit
 */
#define _GNU_SOURCE
#include <GL/freeglut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "../../shared/media_drop_path.h"
#include "../../shared/chtpm_nav_mock.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- layout / budget ---- */
static int g_win_w = 1280, g_win_h = 820;
#define WIN_W (g_win_w)
#define WIN_H (g_win_h)
#define MENU_H 24
#define TOOL_W 56
#define RIGHT_W 200
#define STATUS_H 24
#define MAX_LAYERS 6
#define CANVAS_W 800
#define CANVAS_H 600
#define UNDO_SLOTS 8

#define UI_FPS_PLAY  20
#define UI_FPS_IDLE   6
#define UI_TIMER_MS  50
#define UI_TIMER_IDLE_MS 200
#define SLEEP_US_ACTIVE 5000
#define SLEEP_US_IDLE   25000

typedef enum {
    TOOL_BRUSH = 0,
    TOOL_ERASER,
    TOOL_FILL,
    TOOL_RECT,
    TOOL_EYEDROP,
    TOOL_HAND,
    TOOL_COUNT
} Tool;

typedef struct {
    int used;
    int visible;
    char name[32];
    unsigned char *rgba; /* CANVAS_W * CANVAS_H * 4 */
} Layer;

static Layer g_layers[MAX_LAYERS];
static int g_n_layers = 0;
static int g_active = 0;
static Tool g_tool = TOOL_BRUSH;
static int g_brush = 12;
static unsigned char g_fg[4] = { 30, 120, 220, 255 };
static unsigned char g_bg[4] = { 255, 255, 255, 255 };
static float g_zoom = 1.0f;
static int g_pan_x = 0, g_pan_y = 0;
static int g_file_menu = 0;
static int g_painting = 0;
static int g_rect_on = 0;
static int g_rect_x0, g_rect_y0, g_rect_x1, g_rect_y1;
static int g_hand_ox, g_hand_oy, g_hand_px, g_hand_py;
static char g_status[256] = "";
static char g_project_root[1024] = ".";
static char g_doc_name[64] = "Untitled";
static char g_ffmpeg[256] = "ffmpeg";

/* composite + GL */
static unsigned char g_comp[CANVAS_W * CANVAS_H * 4];
static int g_comp_dirty = 1;
static GLuint g_tex = 0;
static int g_tex_dirty = 1;

/* undo: snapshot of active layer only */
static unsigned char *g_undo[UNDO_SLOTS];
static int g_undo_n = 0;
static int g_undo_i = -1; /* last push index */

/* CPU / dirty */
static int g_ui_dirty = 1;
static int g_glut_ready = 0;
static double g_last_ui = 0;
static int g_fps_n = 0;
static double g_fps_t0 = 0;
static float g_ui_fps = 0;
static volatile int g_quit = 0;
static int g_canvas_tick = 0; /* throttle canvas.raw writes */

/* XDND */
static Display *g_xdpy = NULL;
static Window g_xwin = 0;
static Atom g_xa_XdndAware, g_xa_XdndEnter, g_xa_XdndPosition, g_xa_XdndStatus;
static Atom g_xa_XdndLeave, g_xa_XdndDrop, g_xa_XdndFinished, g_xa_XdndSelection;
static Atom g_xa_XdndActionCopy, g_xa_text_uri_list;
static Window g_xdnd_source = 0;
static int g_xdnd_setup = 0;

static const char *tool_names[] = {
    "Brush", "Eraser", "Fill", "Rect", "Eyedrop", "Hand"
};

/* ---- utils ---- */
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static int clampi(int v, int a, int b) {
    return v < a ? a : (v > b ? b : v);
}
static void mark_ui_dirty(void) {
    g_ui_dirty = 1;
    if (g_glut_ready) glutPostRedisplay();
}
static void mark_comp_dirty(void) {
    g_comp_dirty = 1;
    g_tex_dirty = 1;
    mark_ui_dirty();
}

static void shell_quote(const char *in, char *out, size_t n) {
    size_t j = 0;
    if (j + 1 < n) out[j++] = '\'';
    for (const char *p = in; *p && j + 2 < n; p++) {
        if (*p == '\'') {
            if (j + 4 >= n) break;
            out[j++] = '\''; out[j++] = '\\'; out[j++] = '\''; out[j++] = '\'';
        } else {
            out[j++] = *p;
        }
    }
    if (j + 1 < n) out[j++] = '\'';
    out[j] = 0;
}

/* ---- layers ---- */
static void layer_clear(Layer *L, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!L || !L->rgba) return;
    size_t n = (size_t)CANVAS_W * CANVAS_H;
    for (size_t i = 0; i < n; i++) {
        L->rgba[i * 4] = r;
        L->rgba[i * 4 + 1] = g;
        L->rgba[i * 4 + 2] = b;
        L->rgba[i * 4 + 3] = a;
    }
}

static int layer_add(const char *name, int fill_white) {
    if (g_n_layers >= MAX_LAYERS) return -1;
    int i = g_n_layers;
    Layer *L = &g_layers[i];
    memset(L, 0, sizeof(*L));
    L->used = 1;
    L->visible = 1;
    snprintf(L->name, sizeof(L->name), "%s", name ? name : "Layer");
    L->rgba = (unsigned char *)calloc((size_t)CANVAS_W * CANVAS_H * 4, 1);
    if (!L->rgba) return -1;
    if (fill_white)
        layer_clear(L, 255, 255, 255, 255);
    else
        layer_clear(L, 0, 0, 0, 0); /* transparent */
    g_n_layers++;
    g_active = i;
    mark_comp_dirty();
    return i;
}

static void layers_free_all(void) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        free(g_layers[i].rgba);
        g_layers[i].rgba = NULL;
        g_layers[i].used = 0;
    }
    g_n_layers = 0;
    g_active = 0;
}

static void undo_free(void) {
    for (int i = 0; i < UNDO_SLOTS; i++) {
        free(g_undo[i]);
        g_undo[i] = NULL;
    }
    g_undo_n = 0;
    g_undo_i = -1;
}

static void undo_push_active(void) {
    if (g_active < 0 || g_active >= g_n_layers) return;
    Layer *L = &g_layers[g_active];
    if (!L->rgba) return;
    size_t bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    g_undo_i = (g_undo_i + 1) % UNDO_SLOTS;
    if (!g_undo[g_undo_i])
        g_undo[g_undo_i] = (unsigned char *)malloc(bytes);
    if (!g_undo[g_undo_i]) return;
    memcpy(g_undo[g_undo_i], L->rgba, bytes);
    if (g_undo_n < UNDO_SLOTS) g_undo_n++;
}

static void undo_pop(void) {
    if (g_undo_n <= 0 || g_undo_i < 0) {
        snprintf(g_status, sizeof(g_status), "Nothing to undo");
        mark_ui_dirty();
        return;
    }
    if (g_active < 0 || !g_layers[g_active].rgba || !g_undo[g_undo_i]) return;
    size_t bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    memcpy(g_layers[g_active].rgba, g_undo[g_undo_i], bytes);
    free(g_undo[g_undo_i]);
    g_undo[g_undo_i] = NULL;
    g_undo_i = (g_undo_i - 1 + UNDO_SLOTS) % UNDO_SLOTS;
    g_undo_n--;
    mark_comp_dirty();
    snprintf(g_status, sizeof(g_status), "Undo");
}

/* alpha over: dst = src over dst */
static void blend_pixel(unsigned char *dst, const unsigned char *src) {
    int sa = src[3];
    if (sa == 0) return;
    if (sa == 255) {
        dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255;
        return;
    }
    int da = dst[3];
    int out_a = sa + (da * (255 - sa)) / 255;
    if (out_a <= 0) {
        dst[0] = dst[1] = dst[2] = dst[3] = 0;
        return;
    }
    for (int c = 0; c < 3; c++) {
        int v = (src[c] * sa + dst[c] * da * (255 - sa) / 255) / out_a;
        dst[c] = (unsigned char)clampi(v, 0, 255);
    }
    dst[3] = (unsigned char)clampi(out_a, 0, 255);
}

static void composite_layers(void) {
    if (!g_comp_dirty) return;
    /* checkerboard under transparent areas */
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            int i = (y * CANVAS_W + x) * 4;
            int chk = ((x / 8) ^ (y / 8)) & 1;
            unsigned char g = chk ? 200 : 160;
            g_comp[i] = g; g_comp[i + 1] = g; g_comp[i + 2] = g; g_comp[i + 3] = 255;
        }
    }
    for (int li = 0; li < g_n_layers; li++) {
        Layer *L = &g_layers[li];
        if (!L->used || !L->visible || !L->rgba) continue;
        for (int p = 0; p < CANVAS_W * CANVAS_H; p++)
            blend_pixel(g_comp + p * 4, L->rgba + p * 4);
    }
    g_comp_dirty = 0;
    g_tex_dirty = 1;
}

static void write_canvas_raw(void) {
    if (++g_canvas_tick < 8) return; /* ~few Hz when dirty paints */
    g_canvas_tick = 0;
    composite_layers();
    char path[1200];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/canvas.raw", g_project_root);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(g_comp, 1, (size_t)CANVAS_W * CANVAS_H * 4, f);
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/canvas.receipt.txt", g_project_root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "width=%d\nheight=%d\nbytes_per_pixel=4\nmode=canvas\n", CANVAS_W, CANVAS_H);
        fclose(f);
    }
}

/* ---- paint ops ---- */
static void put_px(Layer *L, int x, int y, const unsigned char col[4], int erase) {
    if (!L || !L->rgba) return;
    if (x < 0 || y < 0 || x >= CANVAS_W || y >= CANVAS_H) return;
    unsigned char *p = L->rgba + (y * CANVAS_W + x) * 4;
    if (erase) {
        p[0] = p[1] = p[2] = p[3] = 0;
    } else {
        blend_pixel(p, col);
        /* for opaque brush, also stamp alpha strongly */
        if (col[3] >= 250) {
            p[0] = col[0]; p[1] = col[1]; p[2] = col[2]; p[3] = 255;
        }
    }
}

static void stamp_brush(int cx, int cy, int erase) {
    if (g_active < 0 || g_active >= g_n_layers) return;
    Layer *L = &g_layers[g_active];
    int r = g_brush;
    if (r < 1) r = 1;
    int r2 = r * r;
    unsigned char col[4] = { g_fg[0], g_fg[1], g_fg[2], g_fg[3] };
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r2)
                put_px(L, x, y, col, erase);
        }
    }
    mark_comp_dirty();
}

static void stroke_line(int x0, int y0, int x1, int y1, int erase) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    int step = g_brush > 2 ? g_brush / 2 : 1;
    int n = 0;
    for (;;) {
        if ((n++ % step) == 0 || (x == x1 && y == y1))
            stamp_brush(x, y, erase);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

static void flood_fill(int sx, int sy) {
    if (g_active < 0 || g_active >= g_n_layers) return;
    Layer *L = &g_layers[g_active];
    if (!L->rgba) return;
    if (sx < 0 || sy < 0 || sx >= CANVAS_W || sy >= CANVAS_H) return;
    unsigned char *base = L->rgba;
    unsigned char tr = base[(sy * CANVAS_W + sx) * 4 + 0];
    unsigned char tg = base[(sy * CANVAS_W + sx) * 4 + 1];
    unsigned char tb = base[(sy * CANVAS_W + sx) * 4 + 2];
    unsigned char ta = base[(sy * CANVAS_W + sx) * 4 + 3];
    if (tr == g_fg[0] && tg == g_fg[1] && tb == g_fg[2] && ta == g_fg[3])
        return;
    /* stack-based flood (bounded) */
    int *stack = (int *)malloc((size_t)CANVAS_W * CANVAS_H * sizeof(int));
    if (!stack) return;
    int sp = 0;
    stack[sp++] = sy * CANVAS_W + sx;
    int filled = 0;
    const int MAX_FILL = CANVAS_W * CANVAS_H;
    while (sp > 0 && filled < MAX_FILL) {
        int idx = stack[--sp];
        int x = idx % CANVAS_W, y = idx / CANVAS_W;
        unsigned char *p = base + idx * 4;
        if (p[0] != tr || p[1] != tg || p[2] != tb || p[3] != ta) continue;
        p[0] = g_fg[0]; p[1] = g_fg[1]; p[2] = g_fg[2]; p[3] = g_fg[3];
        filled++;
        if (x > 0) stack[sp++] = idx - 1;
        if (x + 1 < CANVAS_W) stack[sp++] = idx + 1;
        if (y > 0) stack[sp++] = idx - CANVAS_W;
        if (y + 1 < CANVAS_H) stack[sp++] = idx + CANVAS_W;
    }
    free(stack);
    mark_comp_dirty();
    snprintf(g_status, sizeof(g_status), "Fill %d px", filled);
}

static void draw_rect_on_layer(int x0, int y0, int x1, int y1, int filled) {
    if (g_active < 0 || g_active >= g_n_layers) return;
    Layer *L = &g_layers[g_active];
    int xa = x0 < x1 ? x0 : x1, xb = x0 < x1 ? x1 : x0;
    int ya = y0 < y1 ? y0 : y1, yb = y0 < y1 ? y1 : y0;
    xa = clampi(xa, 0, CANVAS_W - 1);
    xb = clampi(xb, 0, CANVAS_W - 1);
    ya = clampi(ya, 0, CANVAS_H - 1);
    yb = clampi(yb, 0, CANVAS_H - 1);
    for (int y = ya; y <= yb; y++) {
        for (int x = xa; x <= xb; x++) {
            int edge = (x == xa || x == xb || y == ya || y == yb);
            if (filled || edge)
                put_px(L, x, y, g_fg, 0);
        }
    }
    mark_comp_dirty();
}

/* ---- image I/O (ffmpeg one-shot — never on paint path) ---- */
static int import_image_path(const char *path) {
    if (!path || !path[0] || !media_path_is_readable_file(path)) {
        snprintf(g_status, sizeof(g_status), "Not a readable file");
        mark_ui_dirty();
        return 0;
    }
    int kind = media_kind_from_path(path);
    if (kind != 3 && kind != 0) {
        snprintf(g_status, sizeof(g_status), "Not an image: %.80s", path);
        mark_ui_dirty();
        return 0;
    }
    char qpath[MEDIA_PATH_MAX + 8], cmd[MEDIA_PATH_MAX + 256];
    shell_quote(path, qpath, sizeof(qpath));
    /* decode to temp RGBA at canvas size */
    snprintf(cmd, sizeof(cmd),
             "nice -n 10 %s -hide_banner -loglevel error -threads 1 -i %s "
             "-frames:v 1 -f rawvideo -pix_fmt rgba -s %dx%d -y /tmp/ie_import.rgba 2>/dev/null",
             g_ffmpeg, qpath, CANVAS_W, CANVAS_H);
    if (system(cmd) != 0) {
        snprintf(g_status, sizeof(g_status), "Import failed (ffmpeg)");
        mark_ui_dirty();
        return 0;
    }
    FILE *f = fopen("/tmp/ie_import.rgba", "rb");
    if (!f) {
        snprintf(g_status, sizeof(g_status), "Import read failed");
        mark_ui_dirty();
        return 0;
    }
    int li = layer_add("Import", 0);
    if (li < 0) {
        fclose(f);
        snprintf(g_status, sizeof(g_status), "Layer limit (%d)", MAX_LAYERS);
        mark_ui_dirty();
        return 0;
    }
    size_t need = (size_t)CANVAS_W * CANVAS_H * 4;
    size_t n = fread(g_layers[li].rgba, 1, need, f);
    fclose(f);
    if (n < need)
        memset(g_layers[li].rgba + n, 0, need - n);
    /* basename */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(g_layers[li].name, sizeof(g_layers[li].name), "%.28s", base);
    snprintf(g_doc_name, sizeof(g_doc_name), "%.60s", base);
    mark_comp_dirty();
    snprintf(g_status, sizeof(g_status), "Imported %s → layer %d", base, li + 1);
    return 1;
}

static void export_png(void) {
    composite_layers();
    char outp[1200], cmd[1600];
    snprintf(outp, sizeof(outp), "%s/pieces/apps/player_app/export.png", g_project_root);
    FILE *f = fopen("/tmp/ie_export.rgba", "wb");
    if (!f) return;
    fwrite(g_comp, 1, (size_t)CANVAS_W * CANVAS_H * 4, f);
    fclose(f);
    char qout[1300];
    shell_quote(outp, qout, sizeof(qout));
    snprintf(cmd, sizeof(cmd),
             "nice -n 10 %s -hide_banner -loglevel error -y -f rawvideo -pix_fmt rgba "
             "-s %dx%d -i /tmp/ie_export.rgba -frames:v 1 %s 2>/tmp/ie_export.log",
             g_ffmpeg, CANVAS_W, CANVAS_H, qout);
    if (system(cmd) == 0)
        snprintf(g_status, sizeof(g_status), "Exported %s", outp);
    else
        snprintf(g_status, sizeof(g_status), "Export failed — /tmp/ie_export.log");
    mark_ui_dirty();
}

static void new_document(void) {
    layers_free_all();
    undo_free();
    layer_add("Background", 1);
    layer_add("Layer 1", 0);
    g_active = 1;
    g_zoom = 1.0f;
    g_pan_x = g_pan_y = 0;
    snprintf(g_doc_name, sizeof(g_doc_name), "Untitled");
    snprintf(g_status, sizeof(g_status), "New document %dx%d", CANVAS_W, CANVAS_H);
    mark_comp_dirty();
}

static void demo_document(void) {
    new_document();
    /* paint a simple demo on layer 1 */
    g_active = 1;
    g_fg[0] = 220; g_fg[1] = 80; g_fg[2] = 60; g_fg[3] = 255;
    g_brush = 28;
    stamp_brush(200, 180, 0);
    g_fg[0] = 40; g_fg[1] = 160; g_fg[2] = 90;
    stamp_brush(400, 300, 0);
    g_fg[0] = 50; g_fg[1] = 100; g_fg[2] = 220;
    draw_rect_on_layer(500, 80, 720, 220, 1);
    g_fg[0] = 30; g_fg[1] = 30; g_fg[2] = 40;
    g_brush = 8;
    stroke_line(80, 500, 720, 480, 0);
    g_brush = 12;
    g_fg[0] = 30; g_fg[1] = 120; g_fg[2] = 220;
    snprintf(g_doc_name, sizeof(g_doc_name), "Demo");
    snprintf(g_status, sizeof(g_status), "Demo doc — B brush · E eraser · drop PNG/JPG");
}

/* ---- coordinate map: window → canvas ---- */
static void canvas_view_rect(float *ox, float *oy, float *dw, float *dh) {
    float area_x = TOOL_W + 8;
    float area_y = MENU_H + 8;
    float area_w = WIN_W - TOOL_W - RIGHT_W - 16;
    float area_h = WIN_H - MENU_H - STATUS_H - 16;
    float base = fminf(area_w / (float)CANVAS_W, area_h / (float)CANVAS_H);
    float sc = base * g_zoom;
    *dw = CANVAS_W * sc;
    *dh = CANVAS_H * sc;
    *ox = area_x + (area_w - *dw) * 0.5f + g_pan_x;
    *oy = area_y + (area_h - *dh) * 0.5f + g_pan_y;
}

static int win_to_canvas(int mx, int my, int *cx, int *cy) {
    float ox, oy, dw, dh;
    canvas_view_rect(&ox, &oy, &dw, &dh);
    if (mx < ox || my < oy || mx >= ox + dw || my >= oy + dh) return 0;
    *cx = (int)((mx - ox) / dw * CANVAS_W);
    *cy = (int)((my - oy) / dh * CANVAS_H);
    *cx = clampi(*cx, 0, CANVAS_W - 1);
    *cy = clampi(*cy, 0, CANVAS_H - 1);
    return 1;
}

/* ---- XDND (house path aware) ---- */
static void xdnd_setup(void) {
    if (g_xdnd_setup) return;
    g_xdpy = glXGetCurrentDisplay();
    if (!g_xdpy) return;
    g_xwin = glXGetCurrentDrawable();
    if (!g_xwin) return;
    g_xa_XdndAware = XInternAtom(g_xdpy, "XdndAware", False);
    g_xa_XdndEnter = XInternAtom(g_xdpy, "XdndEnter", False);
    g_xa_XdndPosition = XInternAtom(g_xdpy, "XdndPosition", False);
    g_xa_XdndStatus = XInternAtom(g_xdpy, "XdndStatus", False);
    g_xa_XdndLeave = XInternAtom(g_xdpy, "XdndLeave", False);
    g_xa_XdndDrop = XInternAtom(g_xdpy, "XdndDrop", False);
    g_xa_XdndFinished = XInternAtom(g_xdpy, "XdndFinished", False);
    g_xa_XdndSelection = XInternAtom(g_xdpy, "XdndSelection", False);
    g_xa_XdndActionCopy = XInternAtom(g_xdpy, "XdndActionCopy", False);
    g_xa_text_uri_list = XInternAtom(g_xdpy, "text/uri-list", False);
    Atom ver = 5;
    XChangeProperty(g_xdpy, g_xwin, g_xa_XdndAware, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&ver, 1);
    g_xdnd_setup = 1;
}

static void xdnd_send_status(Window src, int accept) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.display = g_xdpy;
    ev.xclient.window = src;
    ev.xclient.message_type = g_xa_XdndStatus;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = accept ? 1 : 0;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, src, False, NoEventMask, &ev);
}

static void xdnd_send_finished(Window src) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.display = g_xdpy;
    ev.xclient.window = src;
    ev.xclient.message_type = g_xa_XdndFinished;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)g_xwin;
    ev.xclient.data.l[1] = 1;
    ev.xclient.data.l[2] = (long)g_xa_XdndActionCopy;
    XSendEvent(g_xdpy, src, False, NoEventMask, &ev);
}

static void on_drop_path(const char *path, void *user) {
    (void)user;
    import_image_path(path);
}

static void xdnd_poll(void) {
    if (!g_xdnd_setup) {
        xdnd_setup();
        if (!g_xdnd_setup) return;
    }
    XEvent ev;
    /* Do not swallow WM_DELETE_WINDOW (title-bar ✕) — put non-Xdnd msgs back. */
    Atom wm_protocols = XInternAtom(g_xdpy, "WM_PROTOCOLS", False);
    Atom wm_delete = XInternAtom(g_xdpy, "WM_DELETE_WINDOW", False);
    while (XCheckTypedEvent(g_xdpy, ClientMessage, &ev)) {
        Atom t = ev.xclient.message_type;
        if (t == g_xa_XdndEnter) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
        } else if (t == g_xa_XdndPosition) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            xdnd_send_status(g_xdnd_source, 1);
            snprintf(g_status, sizeof(g_status), "Drop image to import as layer");
            mark_ui_dirty();
        } else if (t == g_xa_XdndLeave) {
            g_xdnd_source = 0;
        } else if (t == g_xa_XdndDrop) {
            g_xdnd_source = (Window)ev.xclient.data.l[0];
            Atom prop = XInternAtom(g_xdpy, "IE_DROP_PROP", False);
            XConvertSelection(g_xdpy, g_xa_XdndSelection, g_xa_text_uri_list,
                              prop, g_xwin, CurrentTime);
        } else if (t == wm_protocols &&
                   (Atom)ev.xclient.data.l[0] == wm_delete) {
            g_quit = 1;
        } else {
            XPutBackEvent(g_xdpy, &ev);
            break;
        }
    }
    while (XCheckTypedEvent(g_xdpy, SelectionNotify, &ev)) {
        if (ev.xselection.property != None) {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char *data = NULL;
            if (XGetWindowProperty(g_xdpy, g_xwin, ev.xselection.property, 0, 65536, True,
                                   AnyPropertyType, &actual_type, &actual_format,
                                   &nitems, &bytes_after, &data) == Success && data) {
                char st[256];
                media_import_uri_list((char *)data, g_project_root, on_drop_path, NULL, st, sizeof(st));
                if (st[0]) snprintf(g_status, sizeof(g_status), "%s", st);
                XFree(data);
            }
        }
        if (g_xdnd_source) xdnd_send_finished(g_xdnd_source);
        g_xdnd_source = 0;
        mark_ui_dirty();
    }
}

/* ---- draw ---- */
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
static void text(float x, float y, const char *s) {
    glColor3f(0.93f, 0.94f, 0.96f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}
static void text_dim(float x, float y, const char *s) {
    glColor3f(0.55f, 0.58f, 0.62f);
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
}

static void draw_menu(void) {
    rect(0, 0, WIN_W, MENU_H, 0.20f, 0.21f, 0.23f, 1);
    rect(4, 2, 48, MENU_H - 4, g_file_menu ? 0.32f : 0.22f, g_file_menu ? 0.36f : 0.23f, 0.28f, 1);
    text(14, 16, "File");
    text_dim(64, 16, "Edit");
    text_dim(110, 16, "Image");
    text_dim(168, 16, "Layer");
    char pn[96];
    snprintf(pn, sizeof(pn), "  %s  —  Muchi Image", g_doc_name);
    text_dim(230, 16, pn);
    if (g_file_menu) {
        const char *items[] = {
            "New", "Demo Document", "Export PNG", "Add Layer", "Flatten note", "Quit  Esc"
        };
        float mx = 4, my = MENU_H, mw = 170, mh = 22;
        rect(mx, my, mw, mh * 6 + 4, 0.18f, 0.19f, 0.21f, 0.98f);
        for (int i = 0; i < 6; i++) {
            float iy = my + 2 + i * mh;
            rect(mx + 2, iy, mw - 4, mh - 2, 0.24f, 0.26f, 0.30f, 1);
            text(mx + 10, iy + 14, items[i]);
        }
    }
}

static void draw_toolbox(void) {
    float x = 0, y = MENU_H;
    rect(x, y, TOOL_W, WIN_H - MENU_H - STATUS_H, 0.16f, 0.17f, 0.19f, 1);
    const char *lab[] = { "B", "E", "G", "R", "I", "H" };
    for (int i = 0; i < TOOL_COUNT; i++) {
        float iy = y + 8 + i * 44;
        int on = (g_tool == (Tool)i);
        rect(6, iy, TOOL_W - 12, 38, on ? 0.28f : 0.20f, on ? 0.40f : 0.21f, on ? 0.55f : 0.24f, 1);
        text(20, iy + 24, lab[i]);
    }
    /* fg / bg swatches */
    float sy = y + 8 + TOOL_COUNT * 44 + 16;
    rect(10, sy + 10, 28, 28, g_bg[0] / 255.f, g_bg[1] / 255.f, g_bg[2] / 255.f, 1);
    rect(18, sy, 28, 28, g_fg[0] / 255.f, g_fg[1] / 255.f, g_fg[2] / 255.f, 1);
    text_dim(8, sy + 52, "X swap");
    char bs[16];
    snprintf(bs, sizeof(bs), "[ ]%d", g_brush);
    text_dim(8, sy + 70, bs);
}

static void draw_layers_panel(void) {
    float x = WIN_W - RIGHT_W;
    float y = MENU_H;
    rect(x, y, RIGHT_W, WIN_H - MENU_H - STATUS_H, 0.14f, 0.15f, 0.17f, 1);
    text(x + 10, y + 18, "LAYERS");
    text_dim(x + 10, y + 36, "1-6 select  V vis");
    for (int i = g_n_layers - 1; i >= 0; i--) {
        Layer *L = &g_layers[i];
        if (!L->used) continue;
        float iy = y + 50 + (g_n_layers - 1 - i) * 36;
        int on = (i == g_active);
        rect(x + 6, iy, RIGHT_W - 12, 32, on ? 0.30f : 0.20f, on ? 0.34f : 0.21f, on ? 0.42f : 0.24f, 1);
        char line[48];
        snprintf(line, sizeof(line), "%s%d %s", L->visible ? "[>]" : "[ ]", i + 1, L->name);
        text(x + 12, iy + 20, line);
    }
    text_dim(x + 10, WIN_H - STATUS_H - 80, "Ctrl+Z undo");
    text_dim(x + 10, WIN_H - STATUS_H - 64, "+ layer  Del clear");
    text_dim(x + 10, WIN_H - STATUS_H - 48, "wheel zoom");
}

static void draw_canvas(void) {
    float ox, oy, dw, dh;
    canvas_view_rect(&ox, &oy, &dw, &dh);
    /* workspace chrome */
    rect(TOOL_W, MENU_H, WIN_W - TOOL_W - RIGHT_W, WIN_H - MENU_H - STATUS_H, 0.10f, 0.10f, 0.11f, 1);
    rect(ox - 2, oy - 2, dw + 4, dh + 4, 0.05f, 0.05f, 0.06f, 1);

    composite_layers();
    if (!g_tex) {
        glGenTextures(1, &g_tex);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    if (g_tex_dirty) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CANVAS_W, CANVAS_H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_comp);
        g_tex_dirty = 0;
        write_canvas_raw();
    }
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(ox, oy);
    glTexCoord2f(1, 0); glVertex2f(ox + dw, oy);
    glTexCoord2f(1, 1); glVertex2f(ox + dw, oy + dh);
    glTexCoord2f(0, 1); glVertex2f(ox, oy + dh);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    /* live rect rubber-band */
    if (g_rect_on) {
        float scx = dw / CANVAS_W, scy = dh / CANVAS_H;
        float rx = ox + g_rect_x0 * scx;
        float ry = oy + g_rect_y0 * scy;
        float rw = (g_rect_x1 - g_rect_x0) * scx;
        float rh = (g_rect_y1 - g_rect_y0) * scy;
        glColor4f(g_fg[0] / 255.f, g_fg[1] / 255.f, g_fg[2] / 255.f, 0.85f);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(rx, ry); glVertex2f(rx + rw, ry);
        glVertex2f(rx + rw, ry + rh); glVertex2f(rx, ry + rh);
        glEnd();
        glLineWidth(1);
    }
}

static void draw_status(void) {
    rect(0, WIN_H - STATUS_H, WIN_W, STATUS_H, 0.12f, 0.13f, 0.15f, 1);
    char line[320];
    if (!g_status[0])
        snprintf(g_status, sizeof(g_status), "Photoshop-shaped MVP · drop images · B/E/G/R/I");
    snprintf(line, sizeof(line), "%s  ·  %s  brush=%d  z=%.0f%%  UI %.0ffps",
             g_status, tool_names[g_tool], g_brush, g_zoom * 100.f, g_ui_fps);
    text_dim(8, WIN_H - 8, line);
}

static void draw_ui(void) {
    double t = now_sec();
    int cap = g_painting ? UI_FPS_PLAY : UI_FPS_IDLE;
    if (g_last_ui > 0 && (t - g_last_ui) < (1.0 / (double)cap))
        return;
    if (!g_painting && !g_ui_dirty && !g_rect_on && g_last_ui > 0 && (t - g_last_ui) < 0.4)
        return;
    g_last_ui = t;
    g_fps_n++;
    if (g_fps_t0 <= 0) g_fps_t0 = t;
    if (t - g_fps_t0 >= 1.0) {
        g_ui_fps = (float)g_fps_n / (float)(t - g_fps_t0);
        g_fps_n = 0;
        g_fps_t0 = t;
    }

    glClearColor(0.10f, 0.11f, 0.13f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    chtpm_nav_set_window(WIN_W, WIN_H);
    chtpm_nav_begin();
    chtpm_nav_add("Methods/File", 0, 0, (float)WIN_W, (float)MENU_H, 0);
    chtpm_nav_add("ToolStrip", 0, (float)MENU_H, (float)TOOL_W,
                  (float)(WIN_H - MENU_H - STATUS_H), 1);
    chtpm_nav_add("Canvas", (float)TOOL_W, (float)MENU_H,
                  (float)(WIN_W - TOOL_W - RIGHT_W),
                  (float)(WIN_H - MENU_H - STATUS_H), 2);
    chtpm_nav_add("Layers", (float)(WIN_W - RIGHT_W), (float)MENU_H,
                  (float)RIGHT_W, (float)(WIN_H - MENU_H - STATUS_H), 3);
    chtpm_nav_add("Status", 0, (float)(WIN_H - STATUS_H), (float)WIN_W, (float)STATUS_H, 4);
    glPushMatrix();
    glTranslatef(0, (float)chtpm_nav_bar_h(), 0);
    draw_toolbox();
    draw_canvas();
    draw_layers_panel();
    draw_menu();
    draw_status();
    if (g_file_menu) draw_menu();
    chtpm_nav_draw();
    glPopMatrix();

    glutSwapBuffers();
    g_ui_dirty = 0;
}

/* ---- input ---- */
static void file_action(int item) {
    if (item == 0) new_document();
    else if (item == 1) demo_document();
    else if (item == 2) export_png();
    else if (item == 3) {
        char nm[32];
        snprintf(nm, sizeof(nm), "Layer %d", g_n_layers + 1);
        if (layer_add(nm, 0) < 0)
            snprintf(g_status, sizeof(g_status), "Max %d layers", MAX_LAYERS);
        else
            snprintf(g_status, sizeof(g_status), "Added %s", nm);
        mark_ui_dirty();
    } else if (item == 4) {
        snprintf(g_status, sizeof(g_status), "Export flattens layers to PNG");
        mark_ui_dirty();
    } else if (item == 5) {
        g_quit = 1;
    }
    g_file_menu = 0;
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    if (key == 27 || key == 3) { g_quit = 1; return; }
    {
        int sh = glutGetModifiers() & GLUT_ACTIVE_SHIFT;
        if (chtpm_nav_on_key(key, sh)) {
            char m[160];
            chtpm_nav_status(m, sizeof(m));
            snprintf(g_status, sizeof(g_status), "%s", m);
            mark_ui_dirty();
            return;
        }
    }
    int ctrl = glutGetModifiers() & GLUT_ACTIVE_CTRL;
    if (ctrl && (key == 'z' || key == 'Z' || key == 26)) { undo_pop(); return; }
    if (ctrl && (key == 's' || key == 'S' || key == 19)) { export_png(); return; }
    if (ctrl && (key == 'n' || key == 'N' || key == 14)) { new_document(); return; }

    if (key == 'b' || key == 'B') { g_tool = TOOL_BRUSH; snprintf(g_status, sizeof(g_status), "Brush"); }
    else if (key == 'e' || key == 'E') { g_tool = TOOL_ERASER; snprintf(g_status, sizeof(g_status), "Eraser"); }
    else if (key == 'g' || key == 'G') { g_tool = TOOL_FILL; snprintf(g_status, sizeof(g_status), "Fill"); }
    else if (key == 'r' || key == 'R') { g_tool = TOOL_RECT; snprintf(g_status, sizeof(g_status), "Rect"); }
    else if (key == 'i' || key == 'I') { g_tool = TOOL_EYEDROP; snprintf(g_status, sizeof(g_status), "Eyedropper"); }
    else if (key == 'h' || key == 'H') { g_tool = TOOL_HAND; snprintf(g_status, sizeof(g_status), "Hand (pan)"); }
    else if (key == 'x' || key == 'X') {
        unsigned char t[4];
        memcpy(t, g_fg, 4); memcpy(g_fg, g_bg, 4); memcpy(g_bg, t, 4);
        snprintf(g_status, sizeof(g_status), "Swap FG/BG");
    } else if (key == '[') {
        g_brush = clampi(g_brush - 2, 1, 128);
        snprintf(g_status, sizeof(g_status), "Brush %d", g_brush);
    } else if (key == ']') {
        g_brush = clampi(g_brush + 2, 1, 128);
        snprintf(g_status, sizeof(g_status), "Brush %d", g_brush);
    } else if (key == '+' || key == '=') {
        g_zoom = clampf(g_zoom * 1.15f, 0.25f, 8.f);
    } else if (key == '-' || key == '_') {
        g_zoom = clampf(g_zoom / 1.15f, 0.25f, 8.f);
    } else if (key == 'd' || key == 'D') {
        demo_document();
    } else if (key == 'v' || key == 'V') {
        if (g_active >= 0 && g_active < g_n_layers) {
            g_layers[g_active].visible = !g_layers[g_active].visible;
            mark_comp_dirty();
        }
    } else if (key == 127 || key == 8) {
        /* clear active layer */
        if (g_active >= 0 && g_active < g_n_layers) {
            undo_push_active();
            if (g_active == 0)
                layer_clear(&g_layers[0], 255, 255, 255, 255);
            else
                layer_clear(&g_layers[g_active], 0, 0, 0, 0);
            mark_comp_dirty();
            snprintf(g_status, sizeof(g_status), "Cleared layer");
        }
    } else if (key >= '1' && key <= '0' + MAX_LAYERS) {
        int i = key - '1';
        if (i < g_n_layers) {
            g_active = i;
            snprintf(g_status, sizeof(g_status), "Active %s", g_layers[i].name);
        }
    } else if (key == 'n' || key == 'N') {
        char nm[32];
        snprintf(nm, sizeof(nm), "Layer %d", g_n_layers + 1);
        layer_add(nm, 0);
    }
    mark_ui_dirty();
}

static void special(int key, int x, int y) {
    (void)x; (void)y;
    if (key == GLUT_KEY_UP) g_pan_y += 20;
    else if (key == GLUT_KEY_DOWN) g_pan_y -= 20;
    else if (key == GLUT_KEY_LEFT) g_pan_x += 20;
    else if (key == GLUT_KEY_RIGHT) g_pan_x -= 20;
    else if (key == GLUT_KEY_PAGE_UP) g_zoom = clampf(g_zoom * 1.2f, 0.25f, 8.f);
    else if (key == GLUT_KEY_PAGE_DOWN) g_zoom = clampf(g_zoom / 1.2f, 0.25f, 8.f);
    mark_ui_dirty();
}

static void mouse(int button, int state, int mx, int my) {
    my = chtpm_nav_mouse_y(my);
    if (button == 3 && state == GLUT_DOWN) { /* wheel up */
        g_zoom = clampf(g_zoom * 1.1f, 0.25f, 8.f);
        mark_ui_dirty();
        return;
    }
    if (button == 4 && state == GLUT_DOWN) {
        g_zoom = clampf(g_zoom / 1.1f, 0.25f, 8.f);
        mark_ui_dirty();
        return;
    }

    if (button != GLUT_LEFT_BUTTON) return;

    if (state == GLUT_UP) {
        if (g_rect_on) {
            undo_push_active();
            draw_rect_on_layer(g_rect_x0, g_rect_y0, g_rect_x1, g_rect_y1, 0);
            g_rect_on = 0;
        }
        g_painting = 0;
        mark_ui_dirty();
        return;
    }

    /* menu */
    if (my < MENU_H) {
        if (mx >= 4 && mx < 52) { g_file_menu = !g_file_menu; mark_ui_dirty(); return; }
        g_file_menu = 0;
        mark_ui_dirty();
        return;
    }
    if (g_file_menu) {
        if (mx >= 4 && mx < 174 && my >= MENU_H && my < MENU_H + 22 * 6 + 4) {
            int item = (my - MENU_H - 2) / 22;
            if (item >= 0 && item < 6) file_action(item);
            return;
        }
        g_file_menu = 0;
    }

    /* toolbox */
    if (mx < TOOL_W && my >= MENU_H && my < WIN_H - STATUS_H) {
        int idx = (my - MENU_H - 8) / 44;
        if (idx >= 0 && idx < TOOL_COUNT) {
            g_tool = (Tool)idx;
            snprintf(g_status, sizeof(g_status), "%s", tool_names[g_tool]);
            mark_ui_dirty();
        }
        return;
    }

    /* layers panel */
    if (mx >= WIN_W - RIGHT_W && my >= MENU_H + 50) {
        int row = (my - (MENU_H + 50)) / 36;
        int li = g_n_layers - 1 - row;
        if (li >= 0 && li < g_n_layers) {
            g_active = li;
            snprintf(g_status, sizeof(g_status), "Active %s", g_layers[li].name);
            mark_ui_dirty();
        }
        return;
    }

    int cx, cy;
    if (!win_to_canvas(mx, my, &cx, &cy)) return;

    if (g_tool == TOOL_HAND) {
        g_painting = 1;
        g_hand_ox = mx; g_hand_oy = my;
        g_hand_px = g_pan_x; g_hand_py = g_pan_y;
        return;
    }
    if (g_tool == TOOL_EYEDROP) {
        composite_layers();
        unsigned char *p = g_comp + (cy * CANVAS_W + cx) * 4;
        g_fg[0] = p[0]; g_fg[1] = p[1]; g_fg[2] = p[2]; g_fg[3] = 255;
        snprintf(g_status, sizeof(g_status), "FG #%02X%02X%02X", p[0], p[1], p[2]);
        mark_ui_dirty();
        return;
    }
    if (g_tool == TOOL_FILL) {
        undo_push_active();
        flood_fill(cx, cy);
        return;
    }
    if (g_tool == TOOL_RECT) {
        g_rect_on = 1;
        g_rect_x0 = g_rect_x1 = cx;
        g_rect_y0 = g_rect_y1 = cy;
        g_painting = 1;
        mark_ui_dirty();
        return;
    }
    if (g_tool == TOOL_BRUSH || g_tool == TOOL_ERASER) {
        undo_push_active();
        g_painting = 1;
        g_hand_ox = cx; g_hand_oy = cy; /* last canvas pos */
        stamp_brush(cx, cy, g_tool == TOOL_ERASER);
        return;
    }
}

static void motion(int mx, int my) {
    my = chtpm_nav_mouse_y(my);
    if (!g_painting && !g_rect_on) return;
    if (g_tool == TOOL_HAND && g_painting) {
        g_pan_x = g_hand_px + (mx - g_hand_ox);
        g_pan_y = g_hand_py + (my - g_hand_oy);
        mark_ui_dirty();
        return;
    }
    int cx, cy;
    if (!win_to_canvas(mx, my, &cx, &cy)) return;
    if (g_rect_on) {
        g_rect_x1 = cx; g_rect_y1 = cy;
        mark_ui_dirty();
        return;
    }
    if (g_tool == TOOL_BRUSH || g_tool == TOOL_ERASER) {
        stroke_line(g_hand_ox, g_hand_oy, cx, cy, g_tool == TOOL_ERASER);
        g_hand_ox = cx; g_hand_oy = cy;
    }
}

static void timer_cb(int v) {
    (void)v;
    if (g_quit) {
        layers_free_all();
        undo_free();
        exit(0);
    }
    xdnd_poll();
    if (g_ui_dirty || g_painting || g_rect_on)
        glutPostRedisplay();
    glutTimerFunc(g_painting ? UI_TIMER_MS : UI_TIMER_IDLE_MS, timer_cb, 0);
}

static void reshape(int w, int h) {
    if (w < 900) w = 900;
    if (h < 560) h = 560;
    g_win_w = w;
    g_win_h = h;
    glViewport(0, 0, w, h);
    mark_ui_dirty();
}

static void on_sig(int s) {
    (void)s;
    g_quit = 1;
}

static void on_window_close(void) {
    g_quit = 1;
}

int main(int argc, char **argv) {
    if (argc > 1) snprintf(g_project_root, sizeof(g_project_root), "%s", argv[1]);
    else {
        const char *e = getenv("PRISC_PROJECT_ROOT");
        if (e && e[0]) snprintf(g_project_root, sizeof(g_project_root), "%s", e);
    }
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    if (system("command -v ffmpeg >/dev/null 2>&1") != 0)
        snprintf(g_status, sizeof(g_status), "WARN: ffmpeg missing — import/export limited");

    demo_document();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1280, 820);
    glutCreateWindow("Muchi Image — Photoshop-shaped · drop PNG/JPG");
    g_glut_ready = 1;
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    glutCloseFunc(on_window_close);
    glutDisplayFunc(draw_ui);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIgnoreKeyRepeat(1);
    glutTimerFunc(UI_TIMER_IDLE_MS, timer_cb, 0);
    mark_ui_dirty();

    while (!g_quit) {
        xdnd_poll();
        glutMainLoopEvent();
        usleep(g_painting ? SLEEP_US_ACTIVE : SLEEP_US_IDLE);
    }
    layers_free_all();
    undo_free();
    return 0;
}
