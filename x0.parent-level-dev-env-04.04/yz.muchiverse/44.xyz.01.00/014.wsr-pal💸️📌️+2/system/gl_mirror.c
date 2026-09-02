/* gl_mirror - the ONLY file in mutaclsym allowed to call GL/GLUT
 * primitives, per 2.muchi-verse/GRAND-ARCHITECTURE.md's GOVERNING
 * CONSTRAINT (prisc+x's long-term RISC-V-compilation goal means our own
 * code must never depend on GL primitives - only this one minimal
 * reader, ported near-unmodified from real 1.TPMOS
 * projects/wraith-alpha/ops/wraith_gl.c, is exempt). Its entire job:
 * poll ops/compose_rgb_frame.+x's output file
 * (pieces/display/rgb_frame.raw, a raw RGBA32 buffer - zero GL calls
 * went into producing those bytes) and blit it as one textured quad.
 *
 * Deliberately stripped down from wraith_gl.c's 799 lines: dropped
 * everything specific to wraith-alpha-desktop's multi-window manager -
 * mouse hit-testing (hit_test_semantic_action), click-to-activate
 * (emit_mouse_event/pending-action), window-drag calibration
 * (#.mouse-offset.txt / apply_mouse_offset), project command history.
 * mutaclsym has no window manager and no semantic-object click targets
 * to hit-test against - this window is a mirror, not a desktop.
 *
 * Keyboard forwarding and the focus-lock file (see below) were
 * INITIALLY dropped too, on the assumption OS window-focus alone would
 * keep the terminal and this GL window from double-writing into the
 * same input stream. Corrected after finding real 1.TPMOS's
 * pieces/joystick/plugins/joystick_input.c - both it AND
 * pieces/keyboard/plugins/keyboard_input.c explicitly check a shared
 * pieces/apps/gl_os/session/input_focus.lock file
 * (gl_os_has_focus()) before writing to the shared history file,
 * rather than trusting window-manager focus routing - a joystick
 * reading a raw /dev/input/js0 device has no concept of "window focus"
 * at all, so the lock file is the actual, load-bearing mechanism, not
 * a redundant belt-and-suspenders check. gl_desktop.c also has a real
 * glutJoystickFunc(joystick, 50) - so the full pattern is: GL owns
 * joystick/keyboard directly via GLUT callbacks while it holds the
 * lock; standalone terminal/joystick-reader processes back off while
 * it's held. This file now does its half of that: writes the lock on
 * startup, removes it via atexit(), and forwards keyboard input the
 * same way wraith_gl.c's own keyboard()/special_keyboard() do (see
 * this file's own append_key()/map_special_key()).
 * system/keyboard_input.c was updated to check the same lock.
 * Joystick itself (a real, separate process in 1.TPMOS, not part of
 * the GL file) is not yet ported here - tracked as later work, real
 * hardware needed to test against.
 *
 * What's KEPT, byte-for-byte in shape:
 *   - checksum_buffer() - identical FNV-1a-64 algorithm to
 *     ops/compose_rgb_frame.c's own copy (see that file's header) and
 *     to wraith_gl.c's, so a checksum computed by any of the three is
 *     directly comparable.
 *   - load_texture() / display() - the actual "mirror" content: read
 *     the raw file, upload via glTexImage2D, draw ONE glBegin(GL_QUADS)
 *     textured quad, glutSwapBuffers(). No other GL drawing anywhere.
 *   - timer() - polls the source file's mtime/size every 16ms and
 *     reloads on change. wraith_gl.c polls a SEPARATE trigger file
 *     (WRAITH_FRAME_TRIGGER) that its rgb daemon touches on every new
 *     frame; mutaclsym has no such pulse-file convention yet, so this
 *     polls rgb_frame.raw's own stat() directly instead (mtime OR size
 *     change) - one file fewer to keep in sync, same detection effect.
 *   - the receipt-writing pattern the user directly asked be ported:
 *     write_gl_display_receipt(), called from the same three sites
 *     wraith_gl.c calls it from (texture_upload, display_swap,
 *     reshape) - so this pipeline's correctness (did a real texture
 *     upload happen, what did it check-sum to, does that match what
 *     compose_rgb_frame.c said it wrote) can be confirmed by reading a
 *     text file, never by looking at the window. */
#define _GNU_SOURCE
/* Windows freeglut: must define before glut.h (1.TPMOS win_compat.h) */
#ifdef _WIN32
#define GLUT_DISABLE_ATEXIT_HACK
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define usleep(us) Sleep((DWORD)((us) / 1000))
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)

/* REAL FIX (direct user report: "gl is still display weird... cuts
 * the bottom of the game map off"): WIDTH/HEIGHT used to be hardcoded
 * constants matching ops/compose_rgb_frame.c's own fixed 640x304 -
 * correct for THAT renderer, but shared-ops/chtpm_rgb_render.c (a
 * SEPARATE renderer, real chtpm chrome + embedded map content, which
 * needs more vertical space - ~35 real lines vs 304px/16px=19 that
 * fit) writes a DIFFERENT, taller size to the exact same rgb_frame.raw
 * path. Hardcoding one renderer's own size here silently clipped the
 * other's. Now read DYNAMICALLY from pieces/display/
 * rgb_frame.receipt.txt's own frame_w/frame_h fields every time a new
 * frame loads (see read_kv_int()/load_texture() below) - same pattern
 * shared-ops/dump_rgb_png.c already used successfully for this exact
 * problem. DEFAULT_WIDTH/DEFAULT_HEIGHT are only the startup fallback
 * before any real receipt has been read. */
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 304

/* Same bare-decimal-keycode-per-line convention system/keyboard_input.c
 * writes to pieces/apps/player_app/history.txt (prisc+x's read_history
 * opcode fseeks/fscanf's this file, so the format is load-bearing, not
 * cosmetic). Forwarding real keys here (not in the earlier stripped-
 * down version of this port) turns the GL window into a second live
 * input source usable simultaneously with the terminal - same
 * ARROW_LEFT/RIGHT/UP/DOWN = 1000-1003 sentinel values
 * keyboard_input.c/move_player.c already use, which happen to be the
 * exact same values wraith_gl.c's own map_special_key() already
 * produces (not a coincidence to preserve - GLUT special-key handling
 * is the same shape everywhere in this project family). */
#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";
static char frame_source[PATH_BUF];
static char frame_pulse[PATH_BUF];
static char source_receipt[PATH_BUF];
static char display_receipt[PATH_BUF];
static char keyboard_history[PATH_BUF];
static char chtpm_keyboard_history[PATH_BUF];
static char interact_relay[PATH_BUF];
static char focus_lock[PATH_BUF];

static GLuint texture_id;
static unsigned char *frame_buffer = NULL;
static long last_pulse_size = -1;
static volatile sig_atomic_t g_shutdown_requested = 0;
/* Dynamic - see DEFAULT_WIDTH/DEFAULT_HEIGHT's own comment above.
 * Updated by load_texture() whenever rgb_frame.receipt.txt's own
 * frame_w/frame_h change, which also triggers a real glutReshapeWindow()
 * so the window's own pixel size always matches the texture 1:1 (no
 * stretch/squish regardless of which renderer is currently driving). */
static int g_frame_w = DEFAULT_WIDTH;
static int g_frame_h = DEFAULT_HEIGHT;
static int g_window_w = DEFAULT_WIDTH;
static int g_window_h = DEFAULT_HEIGHT;
static int g_texture_view_x = 0;
static int g_texture_view_y = 0;
static int g_texture_view_w = DEFAULT_WIDTH;
static int g_texture_view_h = DEFAULT_HEIGHT;
static unsigned long long g_loaded_frame_checksum = 0;
static size_t g_loaded_frame_bytes = 0;
static int g_loaded_frame_partial = 0;

/* Same helper shape as shared-ops/dump_rgb_png.c's own read_kv_int() -
 * that file already proved this exact pattern (read frame_w/frame_h
 * from the receipt rather than hardcoding them) works correctly
 * against both ops/compose_rgb_frame.c's and shared-ops/
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

static void resolve_root(void) {
#ifdef _WIN32
    /* Prefer relative "." when CWD is the project (emoji abs paths break
     * ANSI fopen on MinGW+OneDrive; same convention as orchestrator). */
    {
        struct stat st_p;
        if (stat("pieces", &st_p) == 0)
            snprintf(project_root, sizeof(project_root), ".");
        else {
            const char *env = getenv("PRISC_PROJECT_ROOT");
            if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
        }
    }
#else
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
#endif
    snprintf(frame_source, sizeof(frame_source), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(source_receipt, sizeof(source_receipt), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    /* REAL FIX (direct user redirect - "tpmos fuzz-op wraith. all use
     * a frame changed marker file... are u doing the same? if not,
     * look into that, its a must have, game change" - checking real
     * wraith_gl.c found the actual gap): this used to watch
     * pieces/display/frame_changed.txt - the same UPSTREAM trigger
     * ops/compose_rgb_frame.c's own hit_frame growth signal AND
     * shared-ops/chtpm_rgb_render.c's own INPUT trigger both use.
     * Confirmed via direct read that real wraith_gl.c does NOT do
     * this - its own WRAITH_FRAME_TRIGGER is a SEPARATE, DOWNSTREAM
     * file (rgb_frame_changed.txt) grown ONLY by wraith_rgb_daemon.c's
     * own pulse_rgb(), AFTER its output is fully written. Watching the
     * upstream trigger directly let this window "consume" a pulse
     * value before shared-ops/chtpm_rgb_render.c (a genuinely separate,
     * independently-timed process in chtpm mode) had finished its own
     * render - confirmed via a live checksum trace (rgb_frame.receipt
     * vs. gl_display.receipt, sampled every 30ms through a keypress
     * burst) that this wasn't just a brief lag but a PERMANENT stall
     * until an unrelated later trigger happened to fire. Now watches
     * the SAME downstream rgb_frame_changed.txt real wraith does -
     * both ops/compose_rgb_frame.c and shared-ops/chtpm_rgb_render.c
     * grow it, only after a real, complete write. */
    snprintf(frame_pulse, sizeof(frame_pulse), "%s/pieces/display/rgb_frame_changed.txt", project_root);
    snprintf(display_receipt, sizeof(display_receipt), "%s/pieces/display/gl_display.receipt.txt", project_root);
    snprintf(keyboard_history, sizeof(keyboard_history), "%s/pieces/apps/player_app/history.txt", project_root);
    snprintf(chtpm_keyboard_history, sizeof(chtpm_keyboard_history), "%s/pieces/keyboard/history.txt", project_root);
    snprintf(interact_relay, sizeof(interact_relay), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    snprintf(focus_lock, sizeof(focus_lock), "%s/pieces/system/gl_focus.lock", project_root);
}

/* Same shape as wraith_gl.c's update_focus_lock()/remove_focus_lock() -
 * as long as this file exists, this window is the authoritative input
 * source and any cooperating standalone input reader (system/
 * keyboard_input.c, and a future ported joystick reader) should back
 * off rather than also writing to the shared history file. See this
 * file's header comment for why this matters more than OS window-focus
 * alone (a joystick device has no window-focus concept at all). */
static void update_focus_lock(void) {
    FILE *f = fopen(focus_lock, "w");
    if (!f) return;
    fprintf(f, "owner=gl_mirror\n");
    fprintf(f, "project=wsr-pal\n");
    fclose(f);
}

static void remove_focus_lock(void) {
    remove(focus_lock);
}

/* Identical append format to system/keyboard_input.c's own append_key()
 * - one bare decimal int per line, appended (never overwritten, never
 * seeked into - prisc+x's read_history opcode owns the read cursor).
 *
 * CHTPM-BRIDGE ADDITION (see chtpm-to-pal-layout-plan.txt and
 * !.handoff-doc-j17.txt's own wraith-alpha research for the why): also
 * appends "KEY_PRESSED: N" to pieces/keyboard/history.txt - confirmed,
 * via direct read of real 1.TPMOS's own wraith_gl.c
 * (append_keyboard_event()), that this is the REAL mechanism a GL
 * window uses to feed chtpm's own input pipeline: wraith_gl.c writes
 * into the EXACT SAME file chtpm_parser.c itself reads, nothing more
 * elaborate. Before this fix, this window's own keyboard capture was a
 * dead end in chtpm mode - it wrote only to player_app/history.txt,
 * which neither chtpm_parser_pal.c nor the module (which reads
 * interact_relay.txt exclusively once in chtpm mode) ever look at. */
/* REMOVED 2026-07-31 (direct instruction: mirror 101.mutaclsym's own
 * real, working gl_mirror.c exactly): this used to ALSO write directly
 * to interact_relay.txt (a second, unconditional delivery path beyond
 * the two writes below), on top of chtpm_parser_pal.c's own SEPARATE,
 * gl_focus.lock-gated relay of the SAME key (via pieces/keyboard/
 * history.txt, written below). Two independent paths converging on
 * the same file - real, live-observed typing doubling in this exact
 * app - mutaclysm's own gl_mirror.c (confirmed via direct read, its
 * own append_key()) has never had this third write at all, only the
 * two below. Removed to match exactly - one write, into keyboard/
 * history.txt, and chtpm_parser_pal.c's own already-existing relay
 * logic is the SOLE path into interact_relay.txt from here on. */
static void append_key(int key) {
    FILE *f = fopen(keyboard_history, "a");
    if (f) { fprintf(f, "%d\n", key); fclose(f); }

    FILE *cf = fopen(chtpm_keyboard_history, "a");
    if (cf) { fprintf(cf, "KEY_PRESSED: %d\n", key); fclose(cf); }
}

/* Same GLUT_KEY_* -> 1000-1003 mapping as real wraith_gl.c's own
 * map_special_key() (kept, not reinvented - see this file's header). */
static int map_special_key(int key) {
    if (key == GLUT_KEY_LEFT) return ARROW_LEFT;
    if (key == GLUT_KEY_RIGHT) return ARROW_RIGHT;
    if (key == GLUT_KEY_UP) return ARROW_UP;
    if (key == GLUT_KEY_DOWN) return ARROW_DOWN;
    return 0;
}

/* Same FNV-1a-64 algorithm as ops/compose_rgb_frame.c's checksum_buffer()
 * (and real wraith_gl.c's) - see that file's header for why matching
 * checksums across the pipeline is the whole point. */
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

static void update_texture_viewport(int win_w, int win_h) {
    double texture_aspect = (double)g_frame_w / (double)g_frame_h;
    double window_aspect;

    if (win_w <= 0) win_w = g_frame_w;
    if (win_h <= 0) win_h = g_frame_h;

    g_window_w = win_w;
    g_window_h = win_h;
    window_aspect = (double)win_w / (double)win_h;

    if (window_aspect > texture_aspect) {
        g_texture_view_h = win_h;
        g_texture_view_w = (int)((double)win_h * texture_aspect + 0.5);
        g_texture_view_x = (win_w - g_texture_view_w) / 2;
        g_texture_view_y = 0;
    } else {
        g_texture_view_w = win_w;
        g_texture_view_h = (int)((double)win_w / texture_aspect + 0.5);
        g_texture_view_x = 0;
        g_texture_view_y = (win_h - g_texture_view_h) / 2;
    }
}

/* Per direct instruction: mirrors wraith_gl.c's write_gl_display_receipt()
 * closely enough that its own field names still mean the same thing -
 * this is the artifact that lets correctness be confirmed without eyes
 * on the screen. event is one of "texture_upload"/"display_swap"/
 * "reshape", matching the three call sites wraith_gl.c uses. */
static void write_gl_display_receipt(const char *event) {
    FILE *f = fopen(display_receipt, "w");
    time_t now;
    struct tm *tm_now;
    char stamp[64];
    double scale_x = 0.0, scale_y = 0.0;

    if (!f) return;
    now = time(NULL);
    tm_now = gmtime(&now);
    if (tm_now) strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    else snprintf(stamp, sizeof(stamp), "unknown");
    if (g_frame_w > 0) scale_x = (double)g_texture_view_w / (double)g_frame_w;
    if (g_frame_h > 0) scale_y = (double)g_texture_view_h / (double)g_frame_h;

    fprintf(f, "receipt_type=gl_display_upload\n");
    fprintf(f, "generated_by=gl_mirror\n");
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
    fprintf(f, "texture_view_x=%d\n", g_texture_view_x);
    fprintf(f, "texture_view_y=%d\n", g_texture_view_y);
    fprintf(f, "texture_view_w=%d\n", g_texture_view_w);
    fprintf(f, "texture_view_h=%d\n", g_texture_view_h);
    fprintf(f, "display_scale_x=%.6f\n", scale_x);
    fprintf(f, "display_scale_y=%.6f\n", scale_y);
    fprintf(f, "render_origin=top_left_texture_to_gl_viewport\n");
    fclose(f);
}

static void load_texture(void) {
    /* Real fix, see DEFAULT_WIDTH/DEFAULT_HEIGHT's own comment above:
     * re-check the SOURCE receipt's own frame_w/frame_h before every
     * load, not just once at startup - ops/compose_rgb_frame.c (640x304)
     * and shared-ops/chtpm_rgb_render.c (640x768) can each become the
     * active writer of rgb_frame.raw at different times (switching
     * between "run" and "chtpm" mode), and this window needs to track
     * whichever one is currently real. On a genuine size change,
     * reallocate frame_buffer to the new byte count AND resize the
     * actual window (glutReshapeWindow) so window pixels always map
     * 1:1 to texture pixels - never stretched/squished to fit a
     * mismatched old window size. */
    int new_w = read_kv_int(source_receipt, "frame_w", DEFAULT_WIDTH);
    int new_h = read_kv_int(source_receipt, "frame_h", DEFAULT_HEIGHT);
    if (new_w > 0 && new_h > 0 && (new_w != g_frame_w || new_h != g_frame_h)) {
        g_frame_w = new_w;
        g_frame_h = new_h;
        free(frame_buffer);
        frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
        glutReshapeWindow(g_frame_w, g_frame_h);
    }

    FILE *f = fopen(frame_source, "rb");

    if (!frame_buffer) frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
    if (!frame_buffer) {
        if (f) fclose(f);
        return;
    }

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

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    /* Internal format GL_RGBA8 is more reliable on Windows WGL than
     * legacy GL_RGBA for some drivers when uploading RGBA8 buffers. */
#ifdef _WIN32
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_frame_w, g_frame_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_frame_w, g_frame_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
#endif
    write_gl_display_receipt("texture_upload");
}

static void display(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Fixed-function path must be explicit on Windows freeglut/WGL:
     * default GL_MODULATE * a black current color draws a black quad
     * even when the texture is full of white text (live empty-window
     * report). GL_REPLACE + white color forces texture pixels through. */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);

    glViewport(g_texture_view_x, g_window_h - g_texture_view_y - g_texture_view_h,
               g_texture_view_w, g_texture_view_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glBegin(GL_QUADS);
    /* Image top (row 0) is first in rgb_frame.raw; OpenGL textures have
     * row 0 at the BOTTOM, so flip V so UI text appears top-down. */
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glEnd();

    glutSwapBuffers();
    write_gl_display_receipt("display_swap");
}

static void reshape(int width, int height) {
    update_texture_viewport(width, height);
    write_gl_display_receipt("reshape");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glutPostRedisplay();
}

static long g_last_raw_size = -1;
static long g_last_raw_mtime = -1;
static long g_last_receipt_mtime = -1;

static void timer(int value) {
    struct stat st;
    (void)value;

    if (g_shutdown_requested) exit(0);

    int dirty = 0;
    if (stat(frame_pulse, &st) == 0) {
        if (st.st_size != last_pulse_size) {
            last_pulse_size = st.st_size;
            dirty = 1;
        }
    }
    /* Fallback: watch rgb_frame.raw size AND mtime. Same byte count
     * with new content is the common path (fixed 640x768 RGBA); size-
     * only missed updates after Windows atomic rewrite. Also receipt
     * mtime when write_file_atomic updates both. */
    if (stat(frame_source, &st) == 0) {
        long mtime = (long)st.st_mtime;
        if ((long)st.st_size != g_last_raw_size || mtime != g_last_raw_mtime) {
            g_last_raw_size = (long)st.st_size;
            g_last_raw_mtime = mtime;
            dirty = 1;
        }
    }
    if (stat(source_receipt, &st) == 0) {
        long mtime = (long)st.st_mtime;
        if (mtime != g_last_receipt_mtime) {
            g_last_receipt_mtime = mtime;
            dirty = 1;
        }
    }
    if (dirty) {
        load_texture();
        glutPostRedisplay();
    } else {
        /* Keep painting even if nothing changed — some WGL drivers
         * leave the backbuffer black until a continuous redraw. */
        glutPostRedisplay();
    }

    glutTimerFunc(33, timer, 0); /* 30 fps cap */
}

/* Forwards every printable key verbatim - same "append everything, let
 * the pal script/ops decide what matters" shape as keyboard_input.c's
 * own main loop, not a curated allowlist here that could drift from
 * the actual dispatch op's own key handling.
 *
 * FIXED 2026-08-02 (real, live-caught bug: pressing plain 'q' quit an
 * entire board-viewer widget window while 'q' was also bound as a
 * real camera-yaw key there - it got appended as content AND treated
 * as quit, simultaneously). This file was the one remaining exception
 * to a standing, already-enforced house-wide rule - direct instruction
 * "never use q to quit in any of the refactors" - already applied to
 * chtpm_parser_pal.c and keyboard_input.c (see their own "QUIT KEY
 * REMOVED" comments) but never to this file. Real quit is Ctrl+C
 * (ETX, byte 3) ONLY, and it no longer gets remapped to/appended as a
 * literal 'q' keypress first - it's consumed here directly, never
 * forwarded as content to any project's own key relay. */
static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    if (key == 3) {
        g_shutdown_requested = 1;
        return;
    }
    append_key((int)key);
}

static void special_keyboard(int key, int x, int y) {
    (void)x;
    (void)y;
    int mapped = map_special_key(key);
    if (mapped > 0) append_key(mapped);
}

int main(int argc, char **argv) {
    struct stat st;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#ifndef _WIN32
    /* Real 1.TPMOS's own gl_os/keyboard_input.c both catch SIGHUP too
     * (this file originally didn't) - without it, closing the
     * controlling terminal while this process is running delivers an
     * UNCAUGHT SIGHUP, which terminates via the default disposition and
     * skips atexit() entirely, leaving gl_focus.lock stale on disk.
     * Every subsequent run then has gl_has_focus() permanently return
     * true, silently killing BOTH keyboard_input's and this window's
     * own input forwarding - the exact "JOYSTICK FIX" bug real
     * 1.TPMOS's run_chtpm.sh documents having already hit once. Catching
     * it here routes through the same g_shutdown_requested -> exit(0)
     * -> atexit(remove_focus_lock) path SIGINT/SIGTERM already use.
     * SIGHUP does not exist on Windows. */
    signal(SIGHUP, handle_signal);
#endif

    resolve_root();
    atexit(remove_focus_lock);
    update_focus_lock();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(g_frame_w, g_frame_h);
    /* Optional window placement: a project running MORE THAN ONE
     * gl_mirror (e.g. a host app + a separate WIDGIT program with its
     * own GL window) needs them side by side, but glut otherwise parks
     * every window at the same default spot - the widget ends up hidden
     * behind the host (live-observed in TSC_ELO). GL_MIRROR_X/Y set a
     * real position; unset means let the WM place it (no glut call,
     * unchanged default behavior). */
    const char *gmx = getenv("GL_MIRROR_X");
    const char *gmy = getenv("GL_MIRROR_Y");
    if (gmx && gmx[0] && gmy && gmy[0]) {
        glutInitWindowPosition(atoi(gmx), atoi(gmy));
    }
    glutCreateWindow("wsr-pal RGB mirror");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutTimerFunc(33, timer, 0); /* 30 fps cap */
    /* Same freeglut auto-repeat rationale as wraith_gl.c's own call -
     * irrelevant to this read-only mirror's single 'q' quit key today,
     * kept anyway since it's a one-line, zero-cost precedent to carry
     * forward rather than silently drop during the port. */
    glutIgnoreKeyRepeat(1);

    if (stat(frame_pulse, &st) == 0) {
        last_pulse_size = st.st_size;
    }
    if (stat(frame_source, &st) == 0) {
        g_last_raw_size = (long)st.st_size;
    }

    load_texture();
    glutPostRedisplay();

    glutMainLoop();
    return 0;
}
