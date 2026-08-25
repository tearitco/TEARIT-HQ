#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
        return;
    }
    if (!getcwd(project_root, sizeof(project_root))) {
        snprintf(project_root, sizeof(project_root), ".");
    }
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static void write_crlf(const char *s, FILE *out) {
    for (const char *p = s; *p; p++) {
        if (*p == '\n') fputc('\r', out);
        fputc(*p, out);
    }
}

static void render_frame(void) {
    char frame_path[PATH_BUF], history_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(history_path, sizeof(history_path), "%s/pieces/display/frame_history.txt", project_root);

    FILE *f = fopen(frame_path, "r");
    if (!f) return;

    char content[8192];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);

    time_t rawtime = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&rawtime));

    write_crlf("\n\n\n\n\n", stdout);
    char marker[128];
    snprintf(marker, sizeof(marker), "--- FRAME UPDATE at %s ---\n", timestamp);
    write_crlf(marker, stdout);
    write_crlf(content, stdout);
    fflush(stdout);

    FILE *hf = fopen(history_path, "a");
    if (hf) {
        fprintf(hf, "\n--- FRAME at %s ---\n%s\n", timestamp, content);
        fclose(hf);
    }
}

static int quit_requested(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[8] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return buf[0] == '1';
}

int main(void) {
    resolve_root();

    char pulse_path[PATH_BUF], renderer_pulse_path[PATH_BUF], frame_path[PATH_BUF];
    snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
    snprintf(renderer_pulse_path, sizeof(renderer_pulse_path), "%s/pieces/display/renderer_pulse.txt", project_root);
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);

    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/display/frame_history.txt", project_root);
    FILE *clear = fopen(history_path, "w");
    if (clear) fclose(clear);

    render_frame();
    long last_marker = file_size(pulse_path);
    long last_renderer_pulse = file_size(renderer_pulse_path);

    while (!quit_requested()) {
        long m = file_size(pulse_path);
        long rm = file_size(renderer_pulse_path);
        int should_render = 0;
        if (m != last_marker) { last_marker = m; should_render = 1; }
        if (rm != last_renderer_pulse) { last_renderer_pulse = rm; should_render = 1; }
        if (should_render) {
            long prev = -1;
            for (int i = 0; i < 20; i++) {
                long cur = file_size(frame_path);
                if (cur == prev) break;
                prev = cur;
                usleep(2000);
            }
            render_frame();
        }
        usleep(33333); /* 30 fps cap */
    }
    return 0;
}
