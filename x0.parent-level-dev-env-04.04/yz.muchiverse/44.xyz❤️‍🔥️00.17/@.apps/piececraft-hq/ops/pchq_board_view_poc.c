/* pchq_board_view_poc.c - real proof-of-concept, 2026-08-30: prove the
 * real board-viewer 3D pixel pipeline (bv_render_3d.c's own raymarch
 * output, rgb_frame_3d_overlay.raw) can be displayed via a real X11
 * window using the EXACT SAME blit mechanism the legacy chtpm engine
 * already uses for this - direct instruction: "u should do it the same
 * way the legacy chtpm parser does it. if possible steal code/ops w/e
 * u have to."
 *
 * STOLEN, NOT REINVENTED: load_frame()/x11_display()'s core logic below
 * is a direct, deliberate port of &.widgits/_shared-lib/ops/x11_mirror.c
 * - same real RGBA32-file-to-XImage-via-XPutPixel loading, same real
 * XPutImage blit call. x11_mirror.c reads rgb_frame.raw (the flat/2D
 * composited frame); this PoC reads rgb_frame_3d_overlay.raw directly
 * (the real 3D raymarch buffer bv_render_3d.c writes BEFORE
 * bv_compose_frame.c composites it into rgb_frame.raw) - same real
 * pixel format (RGBA32), same real receipt convention
 * (overlay_w/overlay_h in rgb_frame_3d_overlay.receipt.txt, vs.
 * x11_mirror.c's own frame_w/frame_h receipt keys).
 *
 * This is a PoC only - no chrome, no khtpm Elem/CSS integration, no
 * menu, no interact/camera keys. Its only job is proving the real pixel
 * pipeline: does the exact real blit mechanism x11_mirror.c already
 * uses successfully display bv_render_3d.c's real 3D output? Real next
 * step once this is confirmed: port this same blit logic INTO
 * khtpm_entity_menu_render.c as a real new window mode, with real
 * khtpm chrome/nav/menu around it - not done here.
 *
 * Usage: pchq_board_view_poc.+x <board_viewer_session_dir>
 * (the real, live board-viewer session dir for piececraft-hq, e.g.
 * &.widgits/board-viewer/pieces/sessions/<id> - found via `ps aux` or
 * ledger_peers.+x for now; real session-discovery wiring is also a
 * later step, not attempted in this PoC). */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4096

static char frame_source[PATH_BUF];
static char receipt_path[PATH_BUF];
static int g_frame_w = 640, g_frame_h = 480;
static unsigned char *frame_buffer = NULL;
static XImage *ximg = NULL;
static Display *dpy;
static Window win;
static GC gc;
static Visual *visual;
static int depth;

/* Same real "KEY | int" pipe-format reader every other real KV reader
 * in this house uses, adapted for the receipt's own plain "key=value"
 * shape (matches x11_mirror.c's own read_kv_int() exactly). */
static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[128];
    size_t klen = strlen(key);
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            val = atoi(line + klen + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

/* STOLEN VERBATIM (adapted var names only) from x11_mirror.c's own
 * resize_to_frame() + load_frame() - see this file's own header
 * comment for why. */
static void resize_to_frame(void) {
    if (ximg) { XDestroyImage(ximg); ximg = NULL; }
    char *data = malloc((size_t)g_frame_w * g_frame_h * 4);
    ximg = XCreateImage(dpy, visual, (unsigned)depth, ZPixmap, 0, data,
                         (unsigned)g_frame_w, (unsigned)g_frame_h, 32, 0);
    XResizeWindow(dpy, win, (unsigned)g_frame_w, (unsigned)g_frame_h);
}

static void load_frame(void) {
    int new_w = read_kv_int(receipt_path, "overlay_w", g_frame_w);
    int new_h = read_kv_int(receipt_path, "overlay_h", g_frame_h);
    if (new_w > 0 && new_h > 0 && (new_w != g_frame_w || new_h != g_frame_h || !ximg)) {
        g_frame_w = new_w;
        g_frame_h = new_h;
        free(frame_buffer);
        frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
        resize_to_frame();
    }
    if (!ximg) resize_to_frame();
    if (!frame_buffer) frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);

    FILE *f = fopen(frame_source, "rb");
    if (!f) { memset(frame_buffer, 0, (size_t)g_frame_w * g_frame_h * 4); return; }
    size_t bytes_read = fread(frame_buffer, 1, (size_t)g_frame_w * g_frame_h * 4, f);
    fclose(f);
    if (bytes_read < (size_t)g_frame_w * g_frame_h * 4) {
        memset(frame_buffer + bytes_read, 0, ((size_t)g_frame_w * g_frame_h * 4) - bytes_read);
    }

    /* STOLEN VERBATIM from x11_mirror.c's own load_frame() inner loop -
     * same real 0xRRGGBB packing, same real alpha-drop (opaque game
     * frames). */
    for (int y = 0; y < g_frame_h; y++) {
        for (int x = 0; x < g_frame_w; x++) {
            size_t o = ((size_t)y * g_frame_w + x) * 4;
            unsigned long px = ((unsigned long)frame_buffer[o] << 16)
                              | ((unsigned long)frame_buffer[o + 1] << 8)
                              | (unsigned long)frame_buffer[o + 2];
            XPutPixel(ximg, x, y, px);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pchq_board_view_poc.+x <board_viewer_session_dir>\n");
        return 1;
    }
    snprintf(frame_source, sizeof(frame_source), "%s/pieces/display/rgb_frame_3d_overlay.raw", argv[1]);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame_3d_overlay.receipt.txt", argv[1]);

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 140, 90,
                               (unsigned)g_frame_w, (unsigned)g_frame_h, 0,
                               BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "pchq-board-view-poc (stolen x11_mirror.c blit)");
    XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);
    XMapRaised(dpy, win);
    gc = XCreateGC(dpy, win, 0, NULL);

    /* Real, simple poll loop - load + blit once per ~300ms tick, same
     * real cadence class every other real display mirror in this house
     * uses, no event-driven "wake on file change" sophistication needed
     * for a PoC. */
    for (int i = 0; i < 40; i++) {
        load_frame();
        if (ximg) XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, (unsigned)g_frame_w, (unsigned)g_frame_h);
        XFlush(dpy);
        usleep(300000);
        XEvent ev;
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose && ximg) {
                XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, (unsigned)g_frame_w, (unsigned)g_frame_h);
                XFlush(dpy);
            }
        }
    }
    return 0;
}
