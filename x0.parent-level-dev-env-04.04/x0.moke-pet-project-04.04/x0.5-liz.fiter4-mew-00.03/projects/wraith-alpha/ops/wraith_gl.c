#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define WIDTH 1024
#define HEIGHT 640
#define WRAITH_KEYBOARD_HISTORY "pieces/keyboard/history.txt"
#define WRAITH_PROJECT_HISTORY "projects/wraith-alpha/session/history.txt"
#define WRAITH_SEMANTIC_META "pieces/display/current_frame.meta.pdl"
#define WRAITH_SEMANTIC_OBJECTS "pieces/display/current_frame.objects.pdl"
#define WRAITH_MOUSE_OFFSET "#.mouse-offset.txt"
#define WRAITH_FOCUS_LOCK "projects/wraith-alpha/session/input_focus.lock"
#define WRAITH_FRAME_SOURCE "projects/wraith-alpha/session/rgb/current_frame.rgba32"
#define WRAITH_FRAME_TRIGGER "projects/wraith-alpha/session/rgb/rgb_frame_changed.txt"
#define WRAITH_UI_STATE "projects/wraith-alpha/session/desktop_ui_state.txt"
#define WRAITH_GL_INPUT_RECEIPT "projects/wraith-alpha/session/rgb/gl_input.receipt.pdl"
#define WRAITH_GL_DISPLAY_RECEIPT "projects/wraith-alpha/session/rgb/gl_display.receipt.pdl"

static GLuint texture_id;
static unsigned char *frame_buffer = NULL;
static off_t last_pulse_size = 0;
static volatile sig_atomic_t g_shutdown_requested = 0;
static int g_mouse_button = -1;
static char g_pending_action[256] = "";
static int g_pending_action_set = 0;
static int g_window_w = WIDTH;
static int g_window_h = HEIGHT;
static int g_texture_view_x = 0;
static int g_texture_view_y = 0;
static int g_texture_view_w = WIDTH;
static int g_texture_view_h = HEIGHT;
static unsigned long long g_loaded_frame_checksum = 0;
static size_t g_loaded_frame_bytes = 0;
static int g_loaded_frame_partial = 0;

static unsigned long long checksum_buffer(const unsigned char *buffer, size_t len) {
    unsigned long long hash = 1469598103934665603ULL;
    size_t i;

    if (!buffer) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        hash ^= (unsigned long long)buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void write_timestamp(FILE *f) {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char stamp[64];

    if (!f) {
        return;
    }

    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "time");
    }

    fprintf(f, "[%s] ", stamp);
}

static void update_focus_lock(void) {
    FILE *f = fopen(WRAITH_FOCUS_LOCK, "w");
    if (!f) {
        return;
    }
    fprintf(f, "owner=wraith_gl\n");
    fprintf(f, "session=wraith-alpha\n");
    fclose(f);
}

static void remove_focus_lock(void) {
    remove(WRAITH_FOCUS_LOCK);
}

static void cleanup_runtime(void) {
    remove_focus_lock();
}

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown_requested = 1;
}

static void append_keyboard_event(int code) {
    FILE *f = fopen(WRAITH_KEYBOARD_HISTORY, "a");
    if (!f) {
        return;
    }
    write_timestamp(f);
    fprintf(f, "KEY_PRESSED: %d\n", code);
    fclose(f);
}

static void append_project_command(const char *command) {
    FILE *f = fopen(WRAITH_PROJECT_HISTORY, "a");
    if (!f || !command) {
        if (f) fclose(f);
        return;
    }
    write_timestamp(f);
    fprintf(f, "COMMAND: %s\n", command);
    fclose(f);
}

static void load_mouse_offset(int *offset_x, int *offset_y) {
    FILE *f = fopen(WRAITH_MOUSE_OFFSET, "r");
    char line[128];
    int x = 0;
    int y = 0;

    if (!offset_x || !offset_y) {
        return;
    }
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "OFFSET_X=%d", &x) == 1) {
                continue;
            }
            if (sscanf(line, "OFFSET_Y=%d", &y) == 1) {
                continue;
            }
        }
        fclose(f);
    }

    *offset_x = x;
    *offset_y = y;
}

static void clear_pending_action(void) {
    g_pending_action[0] = '\0';
    g_pending_action_set = 0;
}

static void set_pending_action(const char *command) {
    if (!command || command[0] == '\0') {
        clear_pending_action();
        return;
    }
    strncpy(g_pending_action, command, sizeof(g_pending_action) - 1);
    g_pending_action[sizeof(g_pending_action) - 1] = '\0';
    g_pending_action_set = 1;
}

static void activate_pending_action(void) {
    if (!g_pending_action_set || g_pending_action[0] == '\0') {
        return;
    }
    append_project_command(g_pending_action);
    clear_pending_action();
}

static int kvp_value(const char *line, const char *key, char *out, size_t out_sz) {
    const char *start, *end;
    size_t len;
    char needle[64];
    if (!line || !key || !out || out_sz == 0) return 0;
    snprintf(needle, sizeof(needle), "%s=", key);
    start = strstr(line, needle);
    if (!start) return 0;
    start += strlen(needle);
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end)) end++;
    len = (size_t)(end - start);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

/* Must match wraith_rgb_daemon.c's GLYPH_W/GLYPH_H (8/16), the same fix
   made there and in wraith-alpha_manager.c's meta emission 2026-07-13 --
   these are read from the same current_frame.meta.pdl
   cell_width_px/cell_height_px fields those two files' comments explain
   in full; this is only the fallback for when that file is missing. Was
   10/18, the same stale guess. Extracted from hit_test_semantic_action()
   so apply_mouse_offset() (below) can share the exact same cell size,
   rather than each caller re-deriving it and risking drift. */
static void read_cell_size(int *cell_w, int *cell_h) {
    FILE *meta = fopen(WRAITH_SEMANTIC_META, "r");
    char line[2048];

    *cell_w = 8;
    *cell_h = 16;
    if (!meta) {
        return;
    }
    while (fgets(line, sizeof(line), meta)) {
        if (strstr(line, "FRAME | cell_width_px |")) {
            *cell_w = atoi(strrchr(line, '|') + 1);
        } else if (strstr(line, "FRAME | cell_height_px |")) {
            *cell_h = atoi(strrchr(line, '|') + 1);
        }
    }
    fclose(meta);
}

/* multi-win-j13.txt PHASE 4 bugfix, 2026-07-13: #.mouse-offset.txt
   (load_mouse_offset()) was being applied ONLY inside
   hit_test_semantic_action() -- GL's own local click-to-activate
   hit-testing -- while the SAME texture_x/texture_y also gets forwarded
   unmodified via emit_mouse_event() to wraith-alpha_manager.c's
   handle_mouse() (which drives window-drag and everything else on the
   manager side). Any real calibration offset would therefore correct
   local button clicks but NOT drag hit-testing/deltas -- an
   inconsistency, independent of whatever the correct offset VALUE turns
   out to be (the calibration file itself doesn't currently exist in
   this project -- see PITFALLS_ACTIVE_2026-03-18.txt's own entry on
   this). Applying it ONCE, right after map_window_mouse_to_texture()
   succeeds, before texture_x/texture_y are used for ANYTHING, makes
   every downstream consumer see the same calibrated coordinate space.
   offset_x/offset_y are stored in the file as CHARACTER-CELL units
   (matching how the terminal-side calibration in chtpm_parser.c's own
   hardcoded `y - 7` was measured -- row units, not pixels), so they're
   converted to pixels here via the same cell size hit_test_semantic_action()
   uses, not added directly to a pixel value. */
static void apply_mouse_offset(int *texture_x, int *texture_y) {
    int cell_w, cell_h;
    int offset_x = 0, offset_y = 0;

    if (!texture_x || !texture_y) {
        return;
    }
    read_cell_size(&cell_w, &cell_h);
    load_mouse_offset(&offset_x, &offset_y);
    *texture_x += offset_x * (cell_w > 0 ? cell_w : 8);
    *texture_y += offset_y * (cell_h > 0 ? cell_h : 16);
}

static int hit_test_semantic_action(int px, int py, char *out, size_t out_sz) {
    FILE *objects = fopen(WRAITH_SEMANTIC_OBJECTS, "r");
    char line[2048];
    int cell_w, cell_h;
    int cell_x;
    int cell_y;
    int best_z = -1000000;
    int found = 0;

    if (out && out_sz > 0) {
        out[0] = '\0';
    }

    if (!objects) {
        return 0;
    }

    /* px/py arrive here ALREADY offset-corrected (see
       apply_mouse_offset()'s own comment -- mouse()'s GLUT_DOWN handler,
       the one caller, now applies it before this call), so this only
       needs cell size for the division, not the offset a second time. */
    read_cell_size(&cell_w, &cell_h);
    cell_x = (px / (cell_w > 0 ? cell_w : 8));
    cell_y = (py / (cell_h > 0 ? cell_h : 16));

    while (fgets(line, sizeof(line), objects)) {
        char value[256];
        int x, y, w, h, z;
        char action[256] = "";

        if (strncmp(line, "OBJECT |", 8) != 0) {
            continue;
        }
        if (!kvp_value(line, "x", value, sizeof(value))) continue;
        x = atoi(value);
        if (!kvp_value(line, "y", value, sizeof(value))) continue;
        y = atoi(value);
        if (!kvp_value(line, "w", value, sizeof(value))) continue;
        w = atoi(value);
        if (!kvp_value(line, "h", value, sizeof(value))) continue;
        h = atoi(value);
        if (kvp_value(line, "z", value, sizeof(value))) {
            z = atoi(value);
        } else {
            z = 0;
        }
        if (kvp_value(line, "action", action, sizeof(action)) && action[0] != '\0') {
            /* already populated */
        } else if (kvp_value(line, "nav", value, sizeof(value)) && atoi(value) > 0) {
            snprintf(action, sizeof(action), "SET_ACTIVE:%d", atoi(value));
        }

        if (action[0] == '\0') {
            continue;
        }
        if (cell_x < x || cell_y < y || cell_x >= x + w || cell_y >= y + h) {
            continue;
        }
        if (z >= best_z) {
            best_z = z;
            if (out && out_sz > 0) {
                strncpy(out, action, out_sz - 1);
                out[out_sz - 1] = '\0';
            }
            found = 1;
        }
    }

    fclose(objects);
    return found;
}

static void update_texture_viewport(int win_w, int win_h) {
    double texture_aspect = (double)WIDTH / (double)HEIGHT;
    double window_aspect;

    if (win_w <= 0) win_w = WIDTH;
    if (win_h <= 0) win_h = HEIGHT;

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

static int map_window_mouse_to_texture(int x, int y, int *texture_x, int *texture_y) {
    int local_x = x - g_texture_view_x;
    int local_y = y - g_texture_view_y;

    if (g_texture_view_w <= 0 || g_texture_view_h <= 0) {
        return 0;
    }
    if (local_x < 0 || local_y < 0 || local_x >= g_texture_view_w || local_y >= g_texture_view_h) {
        return 0;
    }

    if (texture_x) {
        *texture_x = (local_x * WIDTH) / g_texture_view_w;
    }
    if (texture_y) {
        *texture_y = (local_y * HEIGHT) / g_texture_view_h;
    }
    return 1;
}

/* multi-win-j13.txt PHASE 4 bugfix, live-tested 2026-07-13: same mapping
   as map_window_mouse_to_texture(), but CLAMPS into [0,WIDTH-1]x
   [0,HEIGHT-1] instead of refusing to map at all when the cursor is
   outside the texture view. Only ever used for GLUT_UP -- a release is
   a critical state transition (it's what ends a drag and triggers its
   persistence) and must never be silently dropped just because the
   mouse drifted a few pixels past the view's edge during a fast drag,
   or off the top into the OS window's own titlebar. Confirmed live:
   dragging a window down worked, dragging it back up did not -- the
   user's cursor path during the return drag (or its final release)
   left the texture view, mouse()'s old `if (inside_texture)` gate on
   GLUT_UP silently dropped that release, and the manager's
   g_window_drag_active got stuck true forever, misinterpreting every
   later click anywhere as "continue this now-stale drag" (the reported
   "mouse got offset" symptom). */
static void map_window_mouse_to_texture_clamped(int x, int y, int *texture_x, int *texture_y) {
    int local_x = x - g_texture_view_x;
    int local_y = y - g_texture_view_y;
    int tx, ty;

    if (g_texture_view_w <= 0 || g_texture_view_h <= 0) {
        if (texture_x) *texture_x = 0;
        if (texture_y) *texture_y = 0;
        return;
    }
    if (local_x < 0) local_x = 0;
    if (local_y < 0) local_y = 0;
    if (local_x >= g_texture_view_w) local_x = g_texture_view_w - 1;
    if (local_y >= g_texture_view_h) local_y = g_texture_view_h - 1;

    tx = (local_x * WIDTH) / g_texture_view_w;
    ty = (local_y * HEIGHT) / g_texture_view_h;
    if (tx >= WIDTH) tx = WIDTH - 1;
    if (ty >= HEIGHT) ty = HEIGHT - 1;
    if (texture_x) *texture_x = tx;
    if (texture_y) *texture_y = ty;
}

/* Extended 2026-07-13 (mouse-calibration follow-up): the original fields
   only covered raw window pixel -> texture pixel mapping, which turned
   out to be provably exact (verified directly against rasterized pixel
   output, see PITFALLS_ACTIVE_2026-03-18.txt entry 59/60's follow-up
   notes) -- everything AFTER that mapping (offset calibration, the cell
   division, and which OBJECT if any the click resolved to) was
   invisible in this receipt, even though that's exactly the stretch of
   the pipeline most likely to still hide a real bug (a HiDPI/display-
   scaling mismatch between what GLUT reports and actual framebuffer
   pixels would show up here as texture_view_w/h math looking correct
   but calibrated_cell_x/y landing on the wrong OBJECT anyway). New
   trailing params: calibrated_texture_x/y (post apply_mouse_offset(),
   what actually gets forwarded), offset_x/y_cells (the loaded
   #.mouse-offset.txt values, so it's visible at a glance whether
   calibration is even active), cell_x/y (the character-cell position
   used for hit-testing), and matched_action (the semantic action the
   click resolved to, or "none" -- empty string). Sentinel -1/"" for
   callers that don't have this data (motion(), and mouse_down/up when
   outside the texture view). */
static void write_gl_input_receipt(const char *event, int button, int raw_x, int raw_y,
                                   int inside_texture, int texture_x, int texture_y,
                                   int calibrated_x, int calibrated_y,
                                   int offset_x, int offset_y,
                                   int cell_x, int cell_y,
                                   const char *matched_action) {
    FILE *f = fopen(WRAITH_GL_INPUT_RECEIPT, "w");
    time_t now = time(NULL);
    struct tm *tm_now;
    char stamp[64];

    if (!f) {
        return;
    }
    tm_now = gmtime(&now);
    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "unknown");
    }

    fprintf(f, "receipt_type=gl_input_transform\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", stamp);
    fprintf(f, "event=%s\n", event ? event : "unknown");
    fprintf(f, "button=%d\n", button);
    fprintf(f, "window_w=%d\n", g_window_w);
    fprintf(f, "window_h=%d\n", g_window_h);
    fprintf(f, "texture_width_px=%d\n", WIDTH);
    fprintf(f, "texture_height_px=%d\n", HEIGHT);
    fprintf(f, "texture_view_x=%d\n", g_texture_view_x);
    fprintf(f, "texture_view_y=%d\n", g_texture_view_y);
    fprintf(f, "texture_view_w=%d\n", g_texture_view_w);
    fprintf(f, "texture_view_h=%d\n", g_texture_view_h);
    fprintf(f, "raw_mouse_x=%d\n", raw_x);
    fprintf(f, "raw_mouse_y=%d\n", raw_y);
    fprintf(f, "inside_texture=%d\n", inside_texture);
    fprintf(f, "texture_mouse_x=%d\n", texture_x);
    fprintf(f, "texture_mouse_y=%d\n", texture_y);
    fprintf(f, "mouse_offset_x_cells=%d\n", offset_x);
    fprintf(f, "mouse_offset_y_cells=%d\n", offset_y);
    fprintf(f, "calibrated_texture_x=%d\n", calibrated_x);
    fprintf(f, "calibrated_texture_y=%d\n", calibrated_y);
    fprintf(f, "cell_x=%d\n", cell_x);
    fprintf(f, "cell_y=%d\n", cell_y);
    fprintf(f, "matched_action=%s\n", (matched_action && matched_action[0]) ? matched_action : "none");
    fclose(f);
}

static void write_gl_display_receipt(const char *event) {
    FILE *f = fopen(WRAITH_GL_DISPLAY_RECEIPT, "w");
    time_t now = time(NULL);
    struct tm *tm_now;
    char stamp[64];
    double scale_x = 0.0;
    double scale_y = 0.0;

    if (!f) {
        return;
    }
    tm_now = gmtime(&now);
    if (tm_now) {
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", tm_now);
    } else {
        snprintf(stamp, sizeof(stamp), "unknown");
    }
    if (WIDTH > 0) {
        scale_x = (double)g_texture_view_w / (double)WIDTH;
    }
    if (HEIGHT > 0) {
        scale_y = (double)g_texture_view_h / (double)HEIGHT;
    }

    fprintf(f, "receipt_type=gl_display_upload\n");
    fprintf(f, "generated_by=wraith_gl\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", stamp);
    fprintf(f, "event=%s\n", event ? event : "unknown");
    fprintf(f, "source_rgba32=%s\n", WRAITH_FRAME_SOURCE);
    fprintf(f, "texture_width_px=%d\n", WIDTH);
    fprintf(f, "texture_height_px=%d\n", HEIGHT);
    fprintf(f, "expected_rgba_bytes=%d\n", WIDTH * HEIGHT * 4);
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

static int map_special_key(int key) {
    if (key == GLUT_KEY_LEFT) return 1000;
    if (key == GLUT_KEY_RIGHT) return 1001;
    if (key == GLUT_KEY_UP) return 1002;
    if (key == GLUT_KEY_DOWN) return 1003;
    return 0;
}

/* is_press: 1 for GLUT_DOWN and every motion() sample while still held,
   0 for GLUT_UP -- mirrors chtpm_parser.c's SGR M/m distinction (both
   renderers now send the real press/release state instead of leaving
   it ambiguous, multi-win-j13.txt Phase 4). */
static void emit_mouse_event(int button, int x, int y, int is_press) {
    char command[64];
    snprintf(command, sizeof(command), "MOUSE_MOVE %d %d %d %d", button, x, y, is_press);
    append_project_command(command);
}

static void load_texture(void) {
    FILE *f = fopen(WRAITH_FRAME_SOURCE, "rb");

    if (!frame_buffer) frame_buffer = malloc(WIDTH * HEIGHT * 4);
    if (!frame_buffer) {
        if (f) fclose(f);
        return;
    }

    if (!f) {
        memset(frame_buffer, 0, WIDTH * HEIGHT * 4);
        g_loaded_frame_bytes = 0;
        g_loaded_frame_partial = 1;
    } else {
        size_t bytes_read = fread(frame_buffer, 1, WIDTH * HEIGHT * 4, f);
        fclose(f);
        g_loaded_frame_bytes = bytes_read;
        g_loaded_frame_partial = (bytes_read < WIDTH * HEIGHT * 4);
        if (bytes_read < WIDTH * HEIGHT * 4) {
            memset(frame_buffer + bytes_read, 0, (WIDTH * HEIGHT * 4) - bytes_read);
        }
    }
    g_loaded_frame_checksum = checksum_buffer(frame_buffer, WIDTH * HEIGHT * 4);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
    write_gl_display_receipt("texture_upload");
}

static void display(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(g_texture_view_x, g_window_h - g_texture_view_y - g_texture_view_h,
               g_texture_view_w, g_texture_view_h);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
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

static void timer(int value) {
    struct stat st;
    (void)value;

    if (g_shutdown_requested) {
        cleanup_runtime();
        exit(0);
    }

    if (stat(WRAITH_FRAME_TRIGGER, &st) == 0) {
        if (st.st_size != last_pulse_size) {
            last_pulse_size = st.st_size;
            load_texture();
            glutPostRedisplay();
        }
    }

    glutTimerFunc(16, timer, 0);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;

    if (key == 3) {
        g_shutdown_requested = 1;
        return;
    }
    if (key == 10 || key == 13) {
        append_keyboard_event(13);
        activate_pending_action();
        return;
    }
    if (key == 8 || key == 127) {
        append_keyboard_event(key);
        return;
    }

    append_keyboard_event((int)key);
}

static void special_keyboard(int key, int x, int y) {
    int mapped;
    (void)x;
    (void)y;

    mapped = map_special_key(key);
    if (mapped > 0) {
        append_keyboard_event(mapped);
    }
}

static void mouse(int button, int state, int x, int y) {
    int texture_x = -1;
    int texture_y = -1;
    int inside_texture = map_window_mouse_to_texture(x, y, &texture_x, &texture_y);

    if (state == GLUT_DOWN) {
        g_mouse_button = button;
        if (!inside_texture) {
            write_gl_input_receipt("mouse_down", button, x, y, inside_texture, texture_x, texture_y,
                -1, -1, 0, 0, -1, -1, "");
            clear_pending_action();
            return;
        }
        /* multi-win-j13.txt PHASE 4 bugfix: calibration applied ONCE,
           here, before texture_x/texture_y are used for anything --
           see apply_mouse_offset()'s own comment for why applying it
           only inside hit_test_semantic_action() (the old code) left
           the forwarded MOUSE_MOVE coordinates -- which drive window-drag
           and everything else on the manager side -- uncalibrated. */
        apply_mouse_offset(&texture_x, &texture_y);
        emit_mouse_event(button, texture_x, texture_y, 1);
        {
            char command[256];
            int hit;
            int offset_x, offset_y, cell_w, cell_h, cell_x, cell_y;
            /* Receipt-diagnostics only (mouse-calibration follow-up,
               2026-07-13) -- reads the same offset/cell-size values
               apply_mouse_offset()/hit_test_semantic_action() already
               used, purely so this event's full journey (raw -> texture
               -> calibrated -> cell -> matched action) is visible in one
               place for diagnosing any future "click feels off" report
               without needing another deep-dive session. */
            load_mouse_offset(&offset_x, &offset_y);
            read_cell_size(&cell_w, &cell_h);
            cell_x = texture_x / (cell_w > 0 ? cell_w : 8);
            cell_y = texture_y / (cell_h > 0 ? cell_h : 16);
            hit = hit_test_semantic_action(texture_x, texture_y, command, sizeof(command));
            write_gl_input_receipt("mouse_down", button, x, y, inside_texture, texture_x, texture_y,
                texture_x, texture_y, offset_x, offset_y, cell_x, cell_y, hit ? command : "");
            if (hit) {
                if (g_pending_action_set && strcmp(g_pending_action, command) == 0) {
                    activate_pending_action();
                } else {
                    set_pending_action(command);
                }
            }
        }
        return;
    }

    if (state == GLUT_UP) {
        /* multi-win-j13.txt PHASE 4 bugfix: always send the release, even
           when the cursor is currently outside the texture view -- see
           map_window_mouse_to_texture_clamped()'s own comment for why a
           dropped release here is what caused "drag down works, drag
           back up doesn't". Clamped coords are fine for a release: all
           they're used for downstream is is_press=0, ending whatever
           drag/gesture was in progress -- the exact (x,y) on a release
           doesn't need to be precise the way a live drag-update does. */
        if (inside_texture) {
            apply_mouse_offset(&texture_x, &texture_y);
            write_gl_input_receipt("mouse_up", button, x, y, inside_texture, texture_x, texture_y,
                texture_x, texture_y, 0, 0, -1, -1, "");
            emit_mouse_event(button, texture_x, texture_y, 0);
        } else {
            int clamped_x, clamped_y;
            map_window_mouse_to_texture_clamped(x, y, &clamped_x, &clamped_y);
            apply_mouse_offset(&clamped_x, &clamped_y);
            write_gl_input_receipt("mouse_up_clamped", button, x, y, inside_texture, texture_x, texture_y,
                clamped_x, clamped_y, 0, 0, -1, -1, "");
            emit_mouse_event(button, clamped_x, clamped_y, 0);
        }
        if (g_mouse_button == button) {
            g_mouse_button = -1;
        }
    }
}

static void motion(int x, int y) {
    int texture_x = -1;
    int texture_y = -1;
    int inside_texture = map_window_mouse_to_texture(x, y, &texture_x, &texture_y);
    if (inside_texture) {
        apply_mouse_offset(&texture_x, &texture_y);
        write_gl_input_receipt("mouse_motion", g_mouse_button, x, y, inside_texture, texture_x, texture_y,
            texture_x, texture_y, 0, 0, -1, -1, "");
        emit_mouse_event(g_mouse_button, texture_x, texture_y, 1);
    } else {
        write_gl_input_receipt("mouse_motion", g_mouse_button, x, y, inside_texture, texture_x, texture_y,
            -1, -1, 0, 0, -1, -1, "");
    }
}

int main(int argc, char **argv) {
    struct stat st;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(cleanup_runtime);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Wraith Alpha RGB Mirror");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutTimerFunc(16, timer, 0);
    /* 2026-07-12: without this, X11's auto-repeat mechanism generates
       synthetic KeyRelease+KeyPress pairs for held keys (and, at
       certain timing boundaries, even for a quick tap), and freeglut
       delivers each as a distinct call to keyboard()/special_keyboard()
       -- neither of which has any repeat-suppression or dedup of its
       own. Symptom, confirmed live: arrow keys sometimes registering
       twice for one tap, or appearing to skip a step (the same
       repeat-boundary ambiguity that causes the double-fire also
       explains occasional drops in freeglut's internal repeat-vs-new-
       press heuristic). ASCII's input path never hits this -- it reads
       keys from the terminal, not GLUT/X11 callbacks -- which is why
       this was never seen there. glutIgnoreKeyRepeat(1) is GLUT's own,
       standard API for filtering OS-generated auto-repeat events so
       only genuine distinct key-down transitions reach the callbacks.
       This is the ONLY GL renderer file in the repo (every embedded
       Wraith project's window runs through this same process), so the
       fix applies to all of them at once, not just this project. */
    glutIgnoreKeyRepeat(1);

    update_focus_lock();

    if (stat(WRAITH_FRAME_TRIGGER, &st) == 0) {
        last_pulse_size = st.st_size;
    }

    load_texture();

    glutMainLoop();
    return 0;
}
