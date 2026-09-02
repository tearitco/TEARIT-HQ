/* screen_rec_gui - the preview window. Ported from mutaclsym's gl_mirror.c:
 * poll pieces/display/rgb_frame.raw (a raw RGBA32 buffer screen_rec.c writes
 * continuously) and blit it as one textured quad. Adds what gl_mirror
 * doesn't need: a clickable Record/Stop button (plus the 'r' key), a red
 * border while recording, and a thumbnail strip of recordings/*.mp4 along
 * the bottom (click one to open it in the default video player).
 *
 * Thumbnails reuse the exact same raw-RGBA-texture trick as the live
 * preview: shell out to ffmpeg to grab one frame from each file as a fixed
 * WxH raw RGBA buffer, then load it the same way load_texture() already
 * does. No image-decoding library needed.
 *
 * Self-contained, no shared .h files -- same convention as
 * 150.gl-canvas/system/gl_canvas.c. */
#define _GNU_SOURCE
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH 4096
#define DEFAULT_WIDTH 480
#define DEFAULT_HEIGHT 270

#define WINDOW_W 640
#define BUTTON_BAR_H 44
#define BUTTON_W 120
#define BUTTON_H 28
#define BUTTON_X0 16

#define THUMB_W 120
#define THUMB_H 68
#define THUMB_PAD 8
#define TEXT_ROW_H 20
#define STRIP_H (THUMB_H + TEXT_ROW_H + THUMB_PAD * 2)
#define MAX_THUMBS 5
#define DEFAULT_PREVIEW_AREA_H 360

static char project_root[MAX_PATH] = ".";
static char recordings_dir[MAX_PATH];
static char thumbs_dir[MAX_PATH];
static char frame_raw_path[MAX_PATH];
static char frame_receipt_path[MAX_PATH];
static char recorder_state_path[MAX_PATH];
static char control_command_path[MAX_PATH];
static char gui_display_receipt_path[MAX_PATH];

static GLuint texture_id;
static unsigned char *frame_buffer = NULL;
static int last_seen_frame_seq = -1;
static volatile sig_atomic_t g_shutdown_requested = 0;
static int g_frame_w = DEFAULT_WIDTH;
static int g_frame_h = DEFAULT_HEIGHT;
static int g_window_w = WINDOW_W;
static int g_window_h = DEFAULT_PREVIEW_AREA_H + BUTTON_BAR_H + STRIP_H;
static int g_recording = 0;
static char g_active_recording_path[MAX_PATH] = "";

typedef struct {
    char path[MAX_PATH];
    time_t mtime;
    GLuint texture_id;
    int has_texture;
    int thumb_requested;
} ThumbEntry;

static ThumbEntry g_thumbs[MAX_THUMBS];
static int g_thumb_count = 0;

static void resolve_root(void)
{
    const char *env = getenv("SCREENREC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    else if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");

    snprintf(recordings_dir, sizeof(recordings_dir), "%s/recordings", project_root);
    snprintf(thumbs_dir, sizeof(thumbs_dir), "%s/pieces/display/thumbs", project_root);
    snprintf(frame_raw_path, sizeof(frame_raw_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(frame_receipt_path, sizeof(frame_receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    snprintf(recorder_state_path, sizeof(recorder_state_path), "%s/pieces/display/recorder_state.receipt.txt", project_root);
    snprintf(control_command_path, sizeof(control_command_path), "%s/pieces/control/record_command.txt", project_root);
    snprintf(gui_display_receipt_path, sizeof(gui_display_receipt_path), "%s/pieces/display/gui_display.receipt.txt", project_root);

    mkdir(thumbs_dir, 0755);
}

static int read_kv_int(const char *path, const char *key, int def)
{
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

static void read_kv_str(const char *path, const char *key, char *out, size_t out_size)
{
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) {
            char *val = eq + 1;
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            snprintf(out, out_size, "%s", val);
            break;
        }
    }
    fclose(f);
}

/* Same FNV-1a-64 algorithm as screen_rec.c's checksum_buffer() (and
 * mutaclsym's gl_mirror.c) -- lets this window's own receipt be compared
 * directly against the capture side's, so a display bug (wrong bytes, stale
 * frame, truncated read) shows up as a checksum mismatch in two text files
 * instead of only being "the preview looked wrong that one time". */
static unsigned long long checksum_buffer(const unsigned char *buf, size_t len)
{
    unsigned long long hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) { hash ^= buf[i]; hash *= 1099511628211ULL; }
    return hash;
}

static unsigned long long read_kv_hex64(const char *path, const char *key, unsigned long long def)
{
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    if (!buf[0]) return def;
    return strtoull(buf, NULL, 16);
}

static void write_gui_display_receipt(int w, int h, unsigned long long loaded_checksum,
                                       unsigned long long source_checksum, int source_frame_seq,
                                       int partial)
{
    FILE *f = fopen(gui_display_receipt_path, "w");
    if (!f) return;
    fprintf(f, "receipt_type=gui_display_upload\n");
    fprintf(f, "generated_by=screen_rec_gui\n");
    fprintf(f, "event=texture_upload\n");
    fprintf(f, "frame_w=%d\n", w);
    fprintf(f, "frame_h=%d\n", h);
    fprintf(f, "source_frame_seq=%d\n", source_frame_seq);
    fprintf(f, "loaded_partial=%d\n", partial);
    fprintf(f, "loaded_checksum_fnv1a64=0x%016llX\n", loaded_checksum);
    fprintf(f, "source_checksum_fnv1a64=0x%016llX\n", source_checksum);
    fprintf(f, "checksum_match=%d\n", (!partial && loaded_checksum == source_checksum) ? 1 : 0);
    fclose(f);
}

static void handle_signal(int sig) { (void)sig; g_shutdown_requested = 1; }

static void kill_daemon_on_exit(void)
{
    FILE *f = fopen("/tmp/screen_rec.pid", "r");
    if (!f) return;
    int pid = 0;
    if (fscanf(f, "%d", &pid) == 1 && pid > 0) {
        kill(pid, SIGTERM);
        fprintf(stderr, "screen_rec_gui: sent SIGTERM to daemon pid %d\n", pid);
    }
    fclose(f);
    remove("/tmp/screen_rec.pid");
}

static void send_record_command(const char *cmd)
{
    FILE *f = fopen(control_command_path, "w");
    if (f) { fprintf(f, "%s\n", cmd); fclose(f); }
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void draw_text(float x, float y, const char *s)
{
    glRasterPos2f(x, y);
    for (const char *c = s; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
}

static void draw_rect(float x0, float y0, float x1, float y1, float r, float g, float b)
{
    glDisable(GL_TEXTURE_2D);
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
}

static void draw_textured_rect(float x0, float y0, float x1, float y1, GLuint tex)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(x0, y0);
    glTexCoord2f(1, 1); glVertex2f(x1, y0);
    glTexCoord2f(1, 0); glVertex2f(x1, y1);
    glTexCoord2f(0, 0); glVertex2f(x0, y1);
    glEnd();
}

static void load_texture(void)
{
    int new_w = read_kv_int(frame_receipt_path, "frame_w", DEFAULT_WIDTH);
    int new_h = read_kv_int(frame_receipt_path, "frame_h", DEFAULT_HEIGHT);
    if (new_w > 0 && new_h > 0 && (new_w != g_frame_w || new_h != g_frame_h)) {
        g_frame_w = new_w;
        g_frame_h = new_h;
        free(frame_buffer);
        frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
    }
    if (!frame_buffer) frame_buffer = malloc((size_t)g_frame_w * g_frame_h * 4);
    if (!frame_buffer) return;

    size_t expected = (size_t)g_frame_w * g_frame_h * 4;
    int partial = 0;
    FILE *f = fopen(frame_raw_path, "rb");
    if (!f) {
        memset(frame_buffer, 0, expected);
        partial = 1;
    } else {
        size_t bytes_read = fread(frame_buffer, 1, expected, f);
        fclose(f);
        if (bytes_read < expected) {
            memset(frame_buffer + bytes_read, 0, expected - bytes_read);
            partial = 1;
        }
    }

    unsigned long long loaded_checksum = checksum_buffer(frame_buffer, expected);
    unsigned long long source_checksum = read_kv_hex64(frame_receipt_path, "frame_checksum_fnv1a64", 0);
    int source_frame_seq = read_kv_int(frame_receipt_path, "frame_seq", -1);
    write_gui_display_receipt(g_frame_w, g_frame_h, loaded_checksum, source_checksum, source_frame_seq, partial);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_frame_w, g_frame_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
}

/* ---------- recordings + thumbnails ---------- */

static void request_thumbnail(ThumbEntry *t)
{
    if (t->thumb_requested) return;
    t->thumb_requested = 1;

    char thumb_raw[MAX_PATH];
    snprintf(thumb_raw, sizeof(thumb_raw), "%s/%s.raw", thumbs_dir, path_basename(t->path));

    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -v quiet -i '%s' -vframes 1 "
        "-vf \"scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2:color=black\" "
        "-pix_fmt rgba -f rawvideo '%s' >/dev/null 2>&1 &",
        t->path, THUMB_W, THUMB_H, THUMB_W, THUMB_H, thumb_raw);
    system(cmd);
}

static void poll_thumbnail_ready(ThumbEntry *t)
{
    if (t->has_texture || !t->thumb_requested) return;

    char thumb_raw[MAX_PATH];
    snprintf(thumb_raw, sizeof(thumb_raw), "%s/%s.raw", thumbs_dir, path_basename(t->path));

    struct stat st;
    size_t expected = (size_t)THUMB_W * THUMB_H * 4;
    if (stat(thumb_raw, &st) != 0 || (size_t)st.st_size < expected) return;

    unsigned char *buf = malloc(expected);
    FILE *f = fopen(thumb_raw, "rb");
    if (f) {
        size_t got = fread(buf, 1, expected, f);
        fclose(f);
        if (got == expected) {
            if (!t->texture_id) glGenTextures(1, &t->texture_id);
            glBindTexture(GL_TEXTURE_2D, t->texture_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, THUMB_W, THUMB_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            t->has_texture = 1;
        }
    }
    free(buf);
}

static void scan_recordings(void)
{
    DIR *d = opendir(recordings_dir);
    if (!d) return;

    typedef struct { char path[MAX_PATH]; time_t mtime; } FoundRec;
    FoundRec found[64];
    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < 64) {
        size_t len = strlen(ent->d_name);
        if (len < 5 || strcmp(ent->d_name + len - 4, ".mp4") != 0) continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", recordings_dir, ent->d_name);
        if (g_recording && strcmp(full, g_active_recording_path) == 0) continue;

        struct stat st;
        if (stat(full, &st) != 0) continue;
        snprintf(found[n].path, sizeof(found[n].path), "%s", full);
        found[n].mtime = st.st_mtime;
        n++;
    }
    closedir(d);

    for (int i = 1; i < n; i++) {
        int j = i;
        while (j > 0 && found[j].mtime > found[j - 1].mtime) {
            FoundRec tmp = found[j];
            found[j] = found[j - 1];
            found[j - 1] = tmp;
            j--;
        }
    }
    if (n > MAX_THUMBS) n = MAX_THUMBS;

    ThumbEntry updated[MAX_THUMBS];
    memset(updated, 0, sizeof(updated));
    for (int i = 0; i < n; i++) {
        int reused = 0;
        for (int j = 0; j < g_thumb_count; j++) {
            if (strcmp(g_thumbs[j].path, found[i].path) == 0) {
                updated[i] = g_thumbs[j];
                reused = 1;
                break;
            }
        }
        if (!reused) {
            snprintf(updated[i].path, sizeof(updated[i].path), "%s", found[i].path);
            updated[i].mtime = found[i].mtime;
        }
    }
    memcpy(g_thumbs, updated, sizeof(g_thumbs));
    g_thumb_count = n;

    for (int i = 0; i < g_thumb_count; i++) {
        if (!g_thumbs[i].has_texture) request_thumbnail(&g_thumbs[i]);
        poll_thumbnail_ready(&g_thumbs[i]);
    }
}

/* ---------- GL callbacks ---------- */

static void display(void)
{
    int preview_area_h = g_window_h - BUTTON_BAR_H - STRIP_H;
    if (preview_area_h < 40) preview_area_h = 40;
    int preview_y0 = BUTTON_BAR_H + STRIP_H;

    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, g_window_w, g_window_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_window_w, 0, g_window_h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* preview, letterboxed within its own area */
    double tex_aspect = (double)g_frame_w / (double)g_frame_h;
    double area_aspect = (double)g_window_w / (double)preview_area_h;
    int vw, vh, vx, vy;
    if (area_aspect > tex_aspect) {
        vh = preview_area_h;
        vw = (int)(vh * tex_aspect + 0.5);
        vx = (g_window_w - vw) / 2;
        vy = preview_y0;
    } else {
        vw = g_window_w;
        vh = (int)(vw / tex_aspect + 0.5);
        vx = 0;
        vy = preview_y0 + (preview_area_h - vh) / 2;
    }
    draw_rect(0, preview_y0, g_window_w, g_window_h, 0.0f, 0.0f, 0.0f);
    draw_textured_rect(vx, vy, vx + vw, vy + vh, texture_id);

    if (g_recording) {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.9f, 0.1f, 0.1f);
        glLineWidth(4.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(vx + 2, vy + 2);
        glVertex2f(vx + vw - 2, vy + 2);
        glVertex2f(vx + vw - 2, vy + vh - 2);
        glVertex2f(vx + 2, vy + vh - 2);
        glEnd();
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    /* record/stop button */
    int by0 = STRIP_H + (BUTTON_BAR_H - BUTTON_H) / 2;
    if (g_recording) draw_rect(BUTTON_X0, by0, BUTTON_X0 + BUTTON_W, by0 + BUTTON_H, 0.75f, 0.15f, 0.15f);
    else draw_rect(BUTTON_X0, by0, BUTTON_X0 + BUTTON_W, by0 + BUTTON_H, 0.2f, 0.55f, 0.2f);
    glColor3f(1, 1, 1);
    draw_text(BUTTON_X0 + 14, by0 + 9, g_recording ? "STOP" : "RECORD");

    /* thumbnail strip */
    for (int i = 0; i < g_thumb_count; i++) {
        float tx0 = THUMB_PAD + i * (THUMB_W + THUMB_PAD);
        float ty0 = THUMB_PAD;
        if (g_thumbs[i].has_texture)
            draw_textured_rect(tx0, ty0, tx0 + THUMB_W, ty0 + THUMB_H, g_thumbs[i].texture_id);
        else
            draw_rect(tx0, ty0, tx0 + THUMB_W, ty0 + THUMB_H, 0.25f, 0.25f, 0.25f);

        char label[64];
        const char *base = path_basename(g_thumbs[i].path);
        snprintf(label, sizeof(label), "%.20s", base);
        glColor3f(0.85f, 0.85f, 0.85f);
        draw_text(tx0, ty0 + THUMB_H + 6, label);
        glColor3f(1, 1, 1);
    }

    glutSwapBuffers();
}

static void reshape(int width, int height)
{
    g_window_w = width;
    g_window_h = height;
    glutPostRedisplay();
}

static void timer(int value)
{
    (void)value;
    static int tick = 0;
    tick++;

    if (g_shutdown_requested) exit(0);

    /* Reload on frame_seq changing, NOT on rgb_frame.raw's own file size:
     * the preview is a fixed resolution once negotiated, so its byte size
     * never changes after the first frame -- checking size the way
     * gl_mirror.c checks its own source file would only ever fire once,
     * then silently stall forever. frame_seq (written by screen_rec.c,
     * incremented every preview write) is the real change signal, same
     * role as gl_mirror.c's separate rgb_frame_changed.txt pulse file. */
    int seq_now = read_kv_int(frame_receipt_path, "frame_seq", -1);
    if (seq_now != last_seen_frame_seq) {
        last_seen_frame_seq = seq_now;
        load_texture();
        glutPostRedisplay();
    }

    int recording_now = read_kv_int(recorder_state_path, "recording", 0);
    if (recording_now != g_recording) {
        g_recording = recording_now;
        glutPostRedisplay();
    }
    if (g_recording) read_kv_str(recorder_state_path, "output_path", g_active_recording_path, sizeof(g_active_recording_path));

    if (tick % 60 == 0) { /* ~once a second at 16ms ticks */
        scan_recordings();
        glutPostRedisplay();
    } else {
        for (int i = 0; i < g_thumb_count; i++) {
            if (!g_thumbs[i].has_texture) { poll_thumbnail_ready(&g_thumbs[i]); }
        }
    }

    glutTimerFunc(16, timer, 0);
}

static void keyboard(unsigned char key, int x, int y)
{
    (void)x; (void)y;
    if (key == 'r' || key == 'R') {
        send_record_command(g_recording ? "stop" : "start");
    } else if (key == 'q' || key == 'Q' || key == 3) {
        g_shutdown_requested = 1;
    }
}

static void mouse(int button, int state, int x, int y)
{
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    int gy = g_window_h - y; /* freeglut gives top-left origin; flip to bottom-origin */

    int by0 = STRIP_H + (BUTTON_BAR_H - BUTTON_H) / 2;
    if (x >= BUTTON_X0 && x <= BUTTON_X0 + BUTTON_W && gy >= by0 && gy <= by0 + BUTTON_H) {
        send_record_command(g_recording ? "stop" : "start");
        return;
    }

    if (gy >= THUMB_PAD && gy <= THUMB_PAD + THUMB_H) {
        int idx = (x - THUMB_PAD) / (THUMB_W + THUMB_PAD);
        int local_x = (x - THUMB_PAD) - idx * (THUMB_W + THUMB_PAD);
        if (idx >= 0 && idx < g_thumb_count && local_x >= 0 && local_x <= THUMB_W) {
            char cmd[MAX_PATH + 32];
            snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", g_thumbs[idx].path);
            system(cmd);
        }
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    atexit(kill_daemon_on_exit);

    resolve_root();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(g_window_w, g_window_h);
    glutCreateWindow("screen-rec (click RECORD, or press r; q to quit)");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutTimerFunc(16, timer, 0);
    glutIgnoreKeyRepeat(1);

    load_texture();
    scan_recordings();
    glutMainLoop();
    return 0;
}
