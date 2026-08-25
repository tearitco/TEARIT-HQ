/* x11_mirror - REAL SHARED BINARY (2026-08-17, khtpm-merge-how2.md
 * §5c.1/§5c.6, legacy-shared-fix.md §3) - one compiled binary, launched
 * by every legacy-GL project (mutaclysm/piececraft-xyz/my-chara-txt so
 * far, more to follow), replacing each project's own separate
 * gl_mirror.c copy. Real, deliberate move away from the earlier
 * per-project-copy pilot (mutaclysm's own original x11_mirror.c, now
 * superseded by this file) once direct instruction confirmed the goal
 * was "they should all use 1 same binary" - this file was ALREADY
 * nearly generic (every real path already derived from project_root),
 * so becoming the shared binary needed only genericizing the window
 * TITLE (see g_title/derive_title() below), not a rewrite.
 *
 * PARAMETERIZATION: window title is derived from basename(project_root)
 * (e.g. launched against ".../101.mutaclsym.../ " -> title
 * "101.mutaclsym... RGB mirror") - zero new config needed, and a real,
 * direct correctness win: piececraft-xyz's own OLD gl_mirror.c had a
 * stale, copy-pasted title ("tactics-txt RGB mirror" - wrong project
 * name entirely), so basename-derivation isn't just "good enough", it
 * actively FIXES a real pre-existing bug rather than preserving it. The
 * focus_lock file's own "project=" line (cosmetic only - confirmed no
 * real reader parses its content, only checks the file's EXISTENCE) is
 * derived the same way for consistency, not because anything depends
 * on it.
 *
 * REAL, ORIGINAL PILOT CONTEXT (2026-08-17, khtpm-merge-how2.md §5c.1)
 * of gl_mirror.c away from GL/GLUT to plain Xlib, per direct
 * instruction: "khtpmos's real X11 approach... now supersedes [GL] as
 * the house's actual preferred display technology." mutaclysm was the
 * first of 16 real gl_mirror.c copies converted - user asked to watch
 * that one directly before the rest got delegated.
 *
 * Job is UNCHANGED from gl_mirror.c: poll ops/compose_rgb_frame.+x's
 * output file (pieces/display/rgb_frame.raw, raw RGBA32, zero GL/X11
 * calls went into producing those bytes) and blit it into a window.
 * Only the FINAL DISPLAY STEP changed - same file-watch pulse, same
 * checksum algorithm, same receipt schema (so this pipeline's
 * correctness is still confirmed by reading a text file, not by eyes
 * on the screen), same keyboard/mouse forwarding, same focus-lock
 * mechanism, same drop-import queue. Ported line-by-line against
 * gl_mirror.c, not reinvented - see that file's own comment for the
 * real history/reasoning behind each of these pieces.
 *
 * REAL, DOCUMENTED SCOPE NARROWING vs. gl_mirror.c (flagged here, not
 * hidden): gl_mirror.c let the user freely resize the window to any
 * aspect ratio and letterboxed the texture inside it (update_texture_
 * viewport()'s aspect-fit math). This first pass keeps the X11 window's
 * size locked 1:1 to the frame's own real pixel size at all times (same
 * as gl_mirror.c already does via glutReshapeWindow() on every frame-
 * size CHANGE - the only thing dropped is manual free-resize-by-drag-
 * to-an-arbitrary-aspect). This keeps XPutImage a plain 1:1 blit with
 * no software scaling, so the FNV-1a-64 checksum this file writes is a
 * direct, exact comparison against gl_mirror.c's own real receipts -
 * the strongest, most literal proof-of-parity available for a pilot
 * conversion. Manual free-resize + letterbox can be added back as a
 * real follow-up (software scaling into the XImage before XPutImage)
 * once this base pipeline is confirmed correct - not needed for parity
 * proof itself.
 *
 * REAL FOLLOW-UP (2026-08-17, direct live report: "it still has gl
 * header... eventually we want to use our own header" - the point being
 * a plain OS-decorated window LOOKS identical whether GL or X11 is
 * underneath, so there was no visible signal the conversion had
 * happened): added the same custom chrome-bar treatment already proven
 * on db-hq/events-hq/chat-hai this session (_MOTIF_WM_HINTS
 * decorations=0 + a real, hand-drawn Xft title bar + drag-by-chrome-bar
 * via ButtonPress/MotionNotify/ButtonRelease) instead of relying on the
 * native WM titlebar. This IS the real, intended "you can tell it
 * converted" signal per direct instruction - not cosmetic polish. */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define CHROME_H 26

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 304

/* Same sentinel values as gl_mirror.c's own map_special_key() /
 * keyboard_input.c / move_player.c - not reinvented. */
#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
static char g_title[512] = "RGB mirror";
static char frame_source[PATH_BUF];
static char frame_pulse[PATH_BUF];
static char source_receipt[PATH_BUF];
static char display_receipt[PATH_BUF];
static char keyboard_history[PATH_BUF];
static char chtpm_keyboard_history[PATH_BUF];
static char focus_lock[PATH_BUF];
static char key_debug_log[PATH_BUF];

static unsigned char *frame_buffer = NULL;
static long last_pulse_size = -1;
static volatile sig_atomic_t g_shutdown_requested = 0;
static int g_frame_w = DEFAULT_WIDTH;
static int g_frame_h = DEFAULT_HEIGHT;
static int g_window_w = DEFAULT_WIDTH;
static int g_window_h = DEFAULT_HEIGHT;

static char g_drop_pet_id[256] = "";
static int g_drop_pending = 0;
static time_t g_drop_time = 0;

static unsigned long long g_loaded_frame_checksum = 0;
static size_t g_loaded_frame_bytes = 0;
static int g_loaded_frame_partial = 0;

/* ---------- X11 state ---------- */
static Display *dpy;
static Window win;
static int screen;
static GC gc;
static Visual *visual;
static int depth;
static Colormap cmap;
static XftFont *font_ui;
static XftDraw *xftdraw;
static XImage *ximg = NULL; /* rebuilt whenever g_frame_w/h changes */
/* REAL FIX (2026-08-17, direct live report: "the x flicker is still
 * happening its never happened on the other button hq apps") - chrome
 * (draw_chrome()) and the game frame (XPutImage) used to be drawn as
 * two separate, non-atomic operations straight onto the live window -
 * visible on every single frame-pulse update, unlike every other
 * WM-managed khtpm app (db-hq/events-hq/chat-hai in
 * khtpm_entity_menu_render.c), which all composite into an offscreen
 * Pixmap first and blit the WHOLE frame in one atomic XCopyArea. Same
 * real fix here: `buf` is the offscreen composite target now: */
static Pixmap buf = 0;
static int buf_w = 0, buf_h = 0;

static int g_dragging = 0;
static int g_drag_last_x = 0, g_drag_last_y = 0;
/* REAL FIX (2026-08-17, live report: "still not moved 50 down (overlaps
 * header still)"): the real taskbar header strip occupies y=50 to y=86
 * (36px tall, confirmed live via xwininfo) - it STARTS at y=50, doesn't
 * END there, so a floor of 50 left windows sitting right at its own top
 * edge, still fully overlapping it. */
static int g_win_x = 100, g_win_y = 90;

/* REAL, SAME CONVENTION as the khtpm -hq family's own window position
 * PDL (khtpm_entity_menu_render.c's dbhq_load_font_scale() reads
 * window_x/window_y from #.desktop/hq_ui.pdl at the khtpm house root,
 * same flat key=value shape, shared across db-hq/events-hq/chat-hai -
 * direct live ask 2026-08-17: "would be nice if eventually we could
 * share conventions"). Real, documented DIFFERENCE for now: mutaclsym's
 * own project_root and the khtpm house root are two separate,
 * unconnected trees (no PRISC_PROJECT_ROOT-to-house-root link exists
 * yet), so this reads a project-LOCAL file with the identical format/
 * key names, not the literal same shared file - genuine convergence
 * (reading #.desktop/hq_ui.pdl itself) is real follow-up work once
 * that link exists, not done here. */
static void load_window_pos_pdl(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/mirror_ui.pdl", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "window_x") == 0) g_win_x = atoi(val);
        else if (strcmp(line, "window_y") == 0) g_win_y = atoi(val);
    }
    fclose(f);
    if (g_win_y < 90) g_win_y = 90; /* same real "below the taskbar header" floor as WM_MANAGED_DRAG_MIN_Y in khtpm_entity_menu_render.c */
}

static unsigned long alloc_pixel(const char *hex) {
    XColor c;
    XParseColor(dpy, cmap, hex, &c);
    XAllocColor(dpy, cmap, &c);
    return c.pixel;
}

static XftColor xft_color(const char *hex) {
    XftColor c;
    XRenderColor rc; XColor xc;
    XParseColor(dpy, cmap, hex, &xc);
    rc.red = xc.red; rc.green = xc.green; rc.blue = xc.blue; rc.alpha = 0xffff;
    XftColorAllocValue(dpy, visual, cmap, &rc, &c);
    return c;
}

/* Same shape as shared-ops/dump_rgb_png.c's own read_kv_int() - already
 * proven against both ops/compose_rgb_frame.c's and shared-ops/
 * chtpm_rgb_render.c's own receipt output. */
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

/* Real, generic per-project title - see this file's own header comment
 * on why basename-derivation is correct (and actually fixes a real
 * pre-existing bug in at least one project's own old gl_mirror.c).
 *
 * REAL PER-PROJECT NUANCE (2026-08-17, found converting piececraft-xyz):
 * some projects (piececraft-xyz's own button.sh "run" case) launch out
 * of an EPHEMERAL, per-launch session dir
 * ("pieces/sessions/<timestamp>-<pid>/") as their real project_root,
 * not the actual project directory - basename() on that gives an ugly
 * timestamp string, not the real project name. Those projects already
 * write a real "pieces/system/real_project_root.txt" pointing back at
 * their own actual project dir (for board-viewer/widget-spawn logic to
 * find &.widgits/ - see piececraft-xyz's own button.sh comment). Prefer
 * that file's own content for the title when it exists; fall back to
 * plain basename(project_root) otherwise (mutaclysm/my-chara-txt's own
 * real shape - no session indirection, project_root IS the real
 * project dir already). */
static void derive_title(void) {
    char real_root_path[PATH_BUF];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", project_root);
    FILE *f = fopen(real_root_path, "r");
    char tmp[MAX_PATH];
    if (f) {
        if (fgets(tmp, sizeof(tmp), f)) {
            size_t len = strlen(tmp);
            while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r')) tmp[--len] = '\0';
        } else {
            tmp[0] = '\0';
        }
        fclose(f);
    } else {
        tmp[0] = '\0';
    }
    if (!tmp[0]) snprintf(tmp, sizeof(tmp), "%s", project_root);
    char *base = basename(tmp);
    snprintf(g_title, sizeof(g_title), "%s RGB mirror", base);
}

/* REAL, SAME CONVENTION as every other khtpm shared binary's own argv
 * contract (house_root/project_root as argv[1], not just an env var) -
 * a real, load-bearing correctness need for a genuinely SHARED binary:
 * multiple simultaneous instances (one per project) are now possible,
 * and `pkill -f x11_mirror` alone can no longer tell them apart since
 * pkill -f only matches argv, not environment. Passing project_root as
 * a real, visible argv[1] lets each project's own kill verb target
 * only its own instance (e.g. `pkill -f "x11_mirror.+x.*101.mutaclsym"`).
 * argv[1] takes priority over PRISC_PROJECT_ROOT if both are given;
 * falls back to the env var alone for back-compat with any caller that
 * hasn't been updated yet. */
static void resolve_root(int argc, char **argv) {
    if (argc >= 2 && argv[1][0]) {
        snprintf(project_root, sizeof(project_root), "%s", argv[1]);
    } else {
        const char *env = getenv("PRISC_PROJECT_ROOT");
        if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    }
    derive_title();
    snprintf(frame_source, sizeof(frame_source), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(source_receipt, sizeof(source_receipt), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    snprintf(frame_pulse, sizeof(frame_pulse), "%s/pieces/display/rgb_frame_changed.txt", project_root);
    snprintf(display_receipt, sizeof(display_receipt), "%s/pieces/display/gl_display.receipt.txt", project_root);
    snprintf(keyboard_history, sizeof(keyboard_history), "%s/pieces/apps/player_app/history.txt", project_root);
    snprintf(chtpm_keyboard_history, sizeof(chtpm_keyboard_history), "%s/pieces/keyboard/history.txt", project_root);
    snprintf(focus_lock, sizeof(focus_lock), "%s/pieces/system/gl_focus.lock", project_root);
    snprintf(key_debug_log, sizeof(key_debug_log), "%s/pieces/display/gl_key_debug.log", project_root);
}

/* Same shape as gl_mirror.c's own update_focus_lock()/remove_focus_lock() -
 * "owner=gl_mirror" kept verbatim (not renamed to x11_mirror) since any
 * real consumer of this lock file only checks for its EXISTENCE, not
 * its content, per gl_mirror.c's own header comment - changing the
 * value would be a real, pointless compatibility risk for zero gain. */
static void update_focus_lock(void) {
    FILE *f = fopen(focus_lock, "w");
    if (!f) return;
    fprintf(f, "owner=gl_mirror\n");
    fprintf(f, "project=%s\n", g_title);
    fclose(f);
}

static void remove_focus_lock(void) {
    remove(focus_lock);
}

static void append_key(int key) {
    FILE *f = fopen(keyboard_history, "a");
    if (f) { fprintf(f, "%d\n", key); fclose(f); }

    FILE *cf = fopen(chtpm_keyboard_history, "a");
    if (cf) { fprintf(cf, "KEY_PRESSED: %d\n", key); fclose(cf); }
}

static void check_for_drop(void);
static void trigger_pet_import(const char *pet_id) {
    if (!pet_id || !pet_id[0]) return;
    fprintf(stderr, "x11_mirror: importing pet '%s'\n", pet_id);
    snprintf(g_drop_pet_id, sizeof(g_drop_pet_id), "%s", pet_id);
    g_drop_pending = 1;
    g_drop_time = time(NULL);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && ./ops/pet_import '%s' 2>&1 &",
             project_root, pet_id);
    system(cmd);

    char close_request[1024];
    snprintf(close_request, sizeof(close_request),
             "/tmp/egg_window_close_%s.txt", pet_id);
    FILE *f = fopen(close_request, "w");
    if (f) { fprintf(f, "close=1\n"); fclose(f); }
}

static void check_for_drop(void) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/incoming_drop.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char pid[256] = {0};
    if (fgets(pid, sizeof(pid), f)) {
        size_t len = strlen(pid);
        while (len > 0 && (pid[len - 1] == '\n' || pid[len - 1] == '\r')) pid[--len] = '\0';
    }
    fclose(f);
    remove(path);
    if (pid[0]) trigger_pet_import(pid);
}

/* Identical algorithm to gl_mirror.c's own checksum_buffer() (and ops/
 * compose_rgb_frame.c's) - real, direct, checksum-level proof this
 * conversion loads byte-identical frames. */
static unsigned long long checksum_buffer(const unsigned char *buffer, size_t len) {
    unsigned long long hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (unsigned long long)buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

/* Real, minimal receipt - SAME filename/schema as gl_mirror.c's own
 * write_gl_display_receipt() (real, load-bearing per that file's header
 * - lets correctness be confirmed by reading a text file). Removed the
 * texture_view / display_scale fields since this build has no
 * separate viewport-vs-frame distinction (1:1 always, see this file's
 * own header comment on scope narrowing) - any real consumer diffing
 * receipts should treat texture_view_w==window_w/texture_view_h==
 * window_h as the correct at-parity reading, not a missing field. */
static void write_display_receipt(const char *event) {
    FILE *f = fopen(display_receipt, "w");
    time_t now;
    struct tm *tm_now;
    char stamp[64];
    if (!f) return;
    now = time(NULL);
    tm_now = gmtime(&now);
    if (tm_now) strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    else snprintf(stamp, sizeof(stamp), "unknown");

    fprintf(f, "receipt_type=gl_display_upload\n");
    fprintf(f, "generated_by=x11_mirror\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", stamp);
    fprintf(f, "event=%s\n", event ? event : "unknown");
    fprintf(f, "source_rgba32=%s\n", frame_source);
    fprintf(f, "texture_width_px=%d\n", g_frame_w);
    fprintf(f, "texture_height_px=%d\n", g_frame_h);
    fprintf(f, "expected_rgba_bytes=%d\n", g_frame_w * g_frame_h * 4);
    fprintf(f, "loaded_rgba_bytes=%lu\n", (unsigned long)g_loaded_frame_bytes);
    fprintf(f, "loaded_frame_partial=%d\n", g_loaded_frame_partial);
    fprintf(f, "loaded_rgba_checksum_fnv1a64=0x%016llX\n", g_loaded_frame_checksum);
    fprintf(f, "window_w=%d\n", g_window_w);
    fprintf(f, "window_h=%d\n", g_window_h);
    fprintf(f, "texture_view_x=0\n");
    fprintf(f, "texture_view_y=0\n");
    fprintf(f, "texture_view_w=%d\n", g_window_w);
    fprintf(f, "texture_view_h=%d\n", g_window_h);
    fprintf(f, "display_scale_x=1.000000\n");
    fprintf(f, "display_scale_y=1.000000\n");
    fprintf(f, "render_origin=top_left_texture_to_gl_viewport\n");
    fclose(f);
}

/* Rebuilds the XImage to match g_frame_w/h and resizes the real X11
 * window 1:1 to match - same real effect as gl_mirror.c's own
 * glutReshapeWindow() call on a genuine frame-size change. */
static void resize_to_frame(void) {
    if (ximg) { XDestroyImage(ximg); ximg = NULL; } /* frees ximg->data too */
    char *data = malloc((size_t)g_frame_w * g_frame_h * 4);
    ximg = XCreateImage(dpy, visual, (unsigned)depth, ZPixmap, 0, data,
                         (unsigned)g_frame_w, (unsigned)g_frame_h, 32, 0);
    g_window_w = g_frame_w;
    g_window_h = g_frame_h + CHROME_H;
    XResizeWindow(dpy, win, (unsigned)g_window_w, (unsigned)g_window_h);
}

/* Real replacement for gl_mirror.c's load_texture() - same real re-check
 * of the source receipt's frame_w/frame_h on every load (ops/
 * compose_rgb_frame.c and shared-ops/chtpm_rgb_render.c can each become
 * the active writer at different times), same checksum, same receipt
 * write. Difference: packs pixels into the XImage via XPutPixel instead
 * of glTexImage2D - same 0xRRGGBB packing convention already used by
 * this house's own dump_frame_png_op.c (see that file's XGetPixel/mask
 * logic, this is the write-direction mirror of it), alpha byte dropped
 * (opaque game frames, same real assumption gl_mirror.c's own
 * glClearColor(...,1.0f) opaque-background made). */
static void load_frame(void) {
    int new_w = read_kv_int(source_receipt, "frame_w", DEFAULT_WIDTH);
    int new_h = read_kv_int(source_receipt, "frame_h", DEFAULT_HEIGHT);
    if (new_w > 0 && new_h > 0 && (new_w != g_frame_w || new_h != g_frame_h || !ximg)) {
        g_frame_w = new_w;
        g_frame_h = new_h;
        free(frame_buffer);
        frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
        resize_to_frame();
    }
    if (!ximg) resize_to_frame();

    FILE *f = fopen(frame_source, "rb");
    if (!frame_buffer) frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
    if (!frame_buffer) { if (f) fclose(f); return; }

    if (!f) {
        memset(frame_buffer, 0, (size_t)g_frame_w * g_frame_h * 4);
        g_loaded_frame_bytes = 0;
        g_loaded_frame_partial = 1;
    } else {
        size_t bytes_read = fread(frame_buffer, 1, (size_t)g_frame_w * g_frame_h * 4, f);
        fclose(f);
        g_loaded_frame_bytes = bytes_read;
        g_loaded_frame_partial = (bytes_read < (size_t)g_frame_w * g_frame_h * 4);
        if (g_loaded_frame_partial) {
            memset(frame_buffer + bytes_read, 0, ((size_t)g_frame_w * g_frame_h * 4) - bytes_read);
        }
    }
    g_loaded_frame_checksum = checksum_buffer(frame_buffer, (size_t)g_frame_w * g_frame_h * 4);

    for (int y = 0; y < g_frame_h; y++) {
        for (int x = 0; x < g_frame_w; x++) {
            size_t o = ((size_t)y * g_frame_w + x) * 4;
            unsigned long px = ((unsigned long)frame_buffer[o] << 16)
                              | ((unsigned long)frame_buffer[o + 1] << 8)
                              | (unsigned long)frame_buffer[o + 2];
            XPutPixel(ximg, x, y, px);
        }
    }
    write_display_receipt("texture_upload");
}

#define CLOSE_BTN_W 26

/* REAL, hand-drawn chrome bar (2026-08-17 follow-up - see this file's
 * own top-of-file comment) - the real, visible "this is the converted
 * window" signal, same style already proven on db-hq/events-hq/
 * chat-hai this session (khtpm_entity_menu_render.c's own WM-managed
 * chrome). Title text is static (no dynamic status here, this window
 * has nothing else to report) - real content starts right below it.
 *
 * REAL FIX (direct live catch, same turn: "lets make sure we add a
 * close nav button 2 it or it wont close" - decorations=0 via
 * _MOTIF_WM_HINTS means there is NO native close X anymore, and this
 * window never had a close-on-click game element the way the game's
 * own menu items do - Ctrl+C still works from the keyboard, but a
 * mouse-only user had no way to close this window at all). Real [X]
 * button, right-aligned in the chrome bar, same real click-handling
 * shape as everything else in this file (checked before the drag-vs-
 * forward-to-game branch in main()'s ButtonPress handling). */
/* Rebuilds the offscreen Pixmap + its own XftDraw target whenever the
 * real window size changes (g_window_w/h, set by resize_to_frame()). */
static void ensure_buf(void) {
    if (buf && buf_w == g_window_w && buf_h == g_window_h) return;
    if (buf) XFreePixmap(dpy, buf);
    if (xftdraw) XftDrawDestroy(xftdraw);
    buf = XCreatePixmap(dpy, win, (unsigned)g_window_w, (unsigned)g_window_h, (unsigned)depth);
    xftdraw = XftDrawCreate(dpy, buf, visual, cmap);
    buf_w = g_window_w;
    buf_h = g_window_h;
}

static void draw_chrome(void) {
    XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_window_w, CHROME_H);
    if (font_ui) {
        XftColor title_col = xft_color("#eeeeee");
        char title[560]; snprintf(title, sizeof(title), "%s (x11)", g_title);
        XftDrawStringUtf8(xftdraw, &title_col, font_ui, 8, 18, (const FcChar8 *)title, (int)strlen(title));
        XftColorFree(dpy, visual, cmap, &title_col);
    }
    XSetForeground(dpy, gc, alloc_pixel("#5a2020"));
    XFillRectangle(dpy, buf, gc, g_window_w - CLOSE_BTN_W, 0, CLOSE_BTN_W, CHROME_H);
    if (font_ui) {
        XftColor x_col = xft_color("#eeeeee");
        XftDrawStringUtf8(xftdraw, &x_col, font_ui, g_window_w - CLOSE_BTN_W + 9, 18, (const FcChar8 *)"X", 1);
        XftColorFree(dpy, visual, cmap, &x_col);
    }
}

static void x11_display(void) {
    if (!ximg) return;
    ensure_buf();
    draw_chrome();
    XPutImage(dpy, buf, gc, ximg, 0, 0, 0, CHROME_H, (unsigned)g_frame_w, (unsigned)g_frame_h);

    if (g_drop_pending && (time(NULL) - g_drop_time < 2)) {
        XSetForeground(dpy, gc, 0x33cc33);
        XSetFunction(dpy, gc, GXand); /* crude translucency stand-in, real overlay is cosmetic-only */
        XFillRectangle(dpy, buf, gc, 0, CHROME_H, (unsigned)g_window_w, (unsigned)(g_window_h - CHROME_H));
        XSetFunction(dpy, gc, GXcopy);
    } else if (g_drop_pending) {
        g_drop_pending = 0;
    }

    /* One real, atomic blit of the fully-composited frame - this IS the
     * actual fix: the window never shows a partially-drawn intermediate
     * state (chrome-without-content, or vice versa) the way two direct-
     * to-window draws could. */
    XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_window_w, (unsigned)g_window_h, 0, 0);

    XFlush(dpy);
    write_display_receipt("display_swap");
}

static int map_special_key(KeySym ks) {
    if (ks == XK_Left) return ARROW_LEFT;
    if (ks == XK_Right) return ARROW_RIGHT;
    if (ks == XK_Up) return ARROW_UP;
    if (ks == XK_Down) return ARROW_DOWN;
    return 0;
}

static void log_key_debug(const char *source, int raw, int mapped) {
    FILE *f = fopen(key_debug_log, "a");
    if (f) { fprintf(f, "%s raw=%d mapped=%d\n", source, raw, mapped); fclose(f); }
}

static void write_click_kv(const char *key, int value) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", project_root);
    char lines[128][512];
    int n = 0, replaced = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[512];
        size_t klen = strlen(key);
        while (n < 128 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                snprintf(lines[n], sizeof(lines[0]), "%s=%d\n", key, value);
                replaced = 1;
            } else {
                snprintf(lines[n], sizeof(lines[0]), "%s", line);
            }
            n++;
        }
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "%s=%d\n", key, value);
    fclose(wf);
}

int main(int argc, char **argv) {
    struct stat st;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

    resolve_root(argc, argv);
    atexit(remove_focus_lock);
    update_focus_lock();

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "x11_mirror: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
    load_window_pos_pdl();

    /* REAL FOLLOW-UP (2026-08-17) - own chrome bar instead of the native
     * WM titlebar, same real WM-managed-but-undecorated shape already
     * proven on db-hq/events-hq/chat-hai (khtpm_entity_menu_render.c) -
     * override_redirect stays OFF (normal WM-managed window, normal
     * alt-tab/taskbar presence), only the DECORATIONS are turned off via
     * _MOTIF_WM_HINTS, same real mechanism that family already uses. */
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y,
                               (unsigned)g_frame_w, (unsigned)(g_frame_h + CHROME_H), 0,
                               BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    {
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags=MWM_HINTS_DECORATIONS, decorations=0 */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);
    }
    XStoreName(dpy, win, g_title);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | StructureNotifyMask);
    gc = XCreateGC(dpy, win, 0, NULL);
    font_ui = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=12");
    /* xftdraw is now created by ensure_buf() against the offscreen buf,
     * not directly against win - see that function's own header comment. */

    /* PPosition - without this most WMs ignore the requested x/y and use
     * their own cascade/placement policy instead (same real fix already
     * needed by every WM-managed khtpm app this session). */
    {
        XSizeHints *shints = XAllocSizeHints();
        if (shints) { shints->flags = PPosition; shints->x = g_win_x; shints->y = g_win_y; XSetWMNormalHints(dpy, win, shints); XFree(shints); }
    }

    XMapRaised(dpy, win);
    XSync(dpy, False);

    if (stat(frame_pulse, &st) == 0) last_pulse_size = st.st_size;
    load_frame();
    x11_display();

    while (!g_shutdown_requested) {
        check_for_drop();

        fd_set fds; FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        struct timeval tv = { 0, 33333 }; /* same 30fps cap as gl_mirror.c's own timer() */
        select(xfd + 1, &fds, NULL, NULL, &tv);

        if (stat(frame_pulse, &st) == 0 && st.st_size != last_pulse_size) {
            last_pulse_size = st.st_size;
            load_frame();
            x11_display();
        }

        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                x11_display();
            } else if (ev.type == KeyPress) {
                char buf[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &ks, NULL);
                if (n > 0 && (unsigned char)buf[0] == 3) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
                    FILE *qf = fopen(path, "w");
                    if (qf) fclose(qf);
                    g_shutdown_requested = 1;
                } else if (n > 0) {
                    log_key_debug("keyboard", (int)(unsigned char)buf[0], (int)(unsigned char)buf[0]);
                    append_key((int)(unsigned char)buf[0]);
                } else {
                    int mapped = map_special_key(ks);
                    log_key_debug("special", (int)ks, mapped);
                    if (mapped > 0) append_key(mapped);
                }
            } else if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                /* REAL FOLLOW-UP (2026-08-17) - a click inside the new
                 * chrome bar starts a drag (same real ButtonPress/
                 * MotionNotify/ButtonRelease shape as khtpm_entity_menu_
                 * render.c's own db-hq/events-hq/chat-hai drag blocks,
                 * ported not reinvented), a click below it forwards to
                 * the game exactly as gl_mirror.c always did - y offset
                 * by CHROME_H so game logic sees the same coordinate
                 * space as before the chrome bar existed. */
                if (ev.xbutton.y < CHROME_H && ev.xbutton.x >= g_window_w - CLOSE_BTN_W) {
                    g_shutdown_requested = 1;
                } else if (ev.xbutton.y < CHROME_H) {
                    g_dragging = 1;
                    g_drag_last_x = ev.xbutton.x_root;
                    g_drag_last_y = ev.xbutton.y_root;
                } else {
                    write_click_kv("last_click_x", ev.xbutton.x);
                    write_click_kv("last_click_y", ev.xbutton.y - CHROME_H);
                }
            } else if (ev.type == ButtonRelease && ev.xbutton.button == Button1) {
                g_dragging = 0;
            } else if (ev.type == MotionNotify) {
                if (g_dragging) {
                    int dx = ev.xmotion.x_root - g_drag_last_x;
                    int dy = ev.xmotion.y_root - g_drag_last_y;
                    g_win_x += dx; g_win_y += dy;
                    if (g_win_y < 90) g_win_y = 90; /* same real clamp as khtpm_entity_menu_render.c's WM_MANAGED_DRAG_MIN_Y - never gets stuck above the taskbar header */
                    XMoveWindow(dpy, win, g_win_x, g_win_y);
                    g_drag_last_x = ev.xmotion.x_root;
                    g_drag_last_y = ev.xmotion.y_root;
                }
            } else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                /* REAL BUG FIX 2026-08-20, direct user report ("shouldn't
                 * be still running i closed gl. make sure it closes term
                 * also"): closing the window via the WM's own [X] button/
                 * close control only ever set g_shutdown_requested=1 -
                 * this process's OWN loop exit and cleanup, but unlike
                 * the Ctrl+C path a few lines above, it never wrote
                 * quit_flag.txt - so nothing told the REST of the session
                 * (orchestrator and everything it launched: renderer,
                 * chtpm_parser_pal, chtpm_rgb_render, prisc+x,
                 * keyboard_input, the launching button.sh itself) to shut
                 * down. x11_mirror quietly exited alone, leaving every
                 * other process running forever. Same write, same path
                 * convention, as the existing Ctrl+C handler - real
                 * parity, not a new mechanism. */
                char path[512];
                snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
                FILE *qf = fopen(path, "w");
                if (qf) fclose(qf);
                g_shutdown_requested = 1;
            }
        }

        if (g_drop_pending && time(NULL) - g_drop_time >= 2) {
            g_drop_pending = 0;
            x11_display();
        }
    }

    if (ximg) XDestroyImage(ximg);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
