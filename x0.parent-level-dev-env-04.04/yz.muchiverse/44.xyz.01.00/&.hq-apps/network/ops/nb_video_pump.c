#define _POSIX_C_SOURCE 200809L
/* nb_video_pump.c - 8fps resident converter: watches a nb_video_player
 * session (video.playback + current_frame.png) and rewrites the tile's
 * sprite.csv from the newest frame. Does NOT touch khtpm_core_render.c -
 * it relies on hq_sprite()'s sprite.csv mtime-reload to redraw the tile.
 *
 * usage: nb_video_pump <session_root> <sprite_dir> <fps>
 *
 * session_root : the player's project root (has session/ under it)
 * sprite_dir   : the m<N> sprite dir whose sprite.csv we rewrite
 * fps          : poll rate; must match the player's frame rate (8)
 *
 * Loop contract:
 *   - read <session>/video.playback (state=/frame_index=/frame_total=)
 *   - if current_frame.png exists and its mtime > last converted:
 *       convert it into <sprite_dir>/sprite.csv (via nb_media_to_sprite)
 *   - exit 0 when playback reports stopped at frame_total (natural end)
 *     or video.control reads "stop"; tolerate missing files (ffmpeg
 *     pre-extract still running / paused) by just keeping on looping.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static void read_kv_line(FILE *f, const char *key, char *out, size_t out_sz) {
    char line[256];
    out[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        size_t n = strcspn(line, "\r\n");
        line[n] = '\0';
        size_t klen = strlen(key);
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            snprintf(out, out_sz, "%s", line + klen + 1);
            return;
        }
    }
}

static void read_playback(const char *session, char *state, int *frame, int *total) {
    char path[4096], buf[64];
    snprintf(path, sizeof(path), "%s/video.playback", session);
    *frame = 0;
    *total = 0;
    buf[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    read_kv_line(f, "state", buf, sizeof(buf));
    snprintf(state, 64, "%s", buf);
    read_kv_line(f, "frame_index", buf, sizeof(buf));
    *frame = atoi(buf);
    read_kv_line(f, "frame_total", buf, sizeof(buf));
    *total = atoi(buf);
    fclose(f);
}

static int control_is_stop(const char *session) {
    char path[4096], line[64];
    snprintf(path, sizeof(path), "%s/video.control", session);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    return strcmp(line, "stop") == 0;
}

/* Resolve nb_media_to_sprite.+x next to this binary (same ops/+x dir). */
static void resolve_op_path(const char *argv0, char *out, size_t out_sz) {
    const char *slash = strrchr(argv0, '/');
    if (slash) {
        size_t n = (size_t)(slash - argv0);
        snprintf(out, out_sz, "%.*s/nb_media_to_sprite.+x", (int)n, argv0);
    } else {
        snprintf(out, out_sz, "nb_media_to_sprite.+x");
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <session_root> <sprite_dir> <fps>\n", argv[0]);
        return 1;
    }
    const char *root = argv[1];
    const char *sprite_dir = argv[2];
    int fps = atoi(argv[3]);
    if (fps <= 0) fps = 8;

    char session[4096];
    snprintf(session, sizeof(session), "%s/session", root);
    mkdir(session, 0755);

    char op_path[4096];
    resolve_op_path(argv[0], op_path, sizeof(op_path));

    char state[64];
    int frame_index = 0, frame_total = 0;
    long last_conv = 0;

    for (;;) {
        read_playback(session, state, &frame_index, &frame_total);

        char cur[4096];
        snprintf(cur, sizeof(cur), "%s/current_frame.png", session);
        struct stat st;
        int have_frame = stat(cur, &st) == 0;

        if (have_frame && st.st_mtime > last_conv) {
            char cmd[8192];
            snprintf(cmd, sizeof(cmd),
                "'%s' '%s' '%s' >/dev/null 2>&1", op_path, cur, sprite_dir);
            system(cmd);
            last_conv = st.st_mtime;
        }

        if (control_is_stop(session)) {
            fprintf(stderr, "nb_video_pump: control=stop, exiting\n");
            return 0;
        }
        if (strcmp(state, "stopped") == 0 && frame_total > 0 && frame_index >= frame_total) {
            fprintf(stderr, "nb_video_pump: playback complete %d/%d, exiting\n",
                frame_index, frame_total);
            return 0;
        }

        usleep(1000000 / fps);
    }
    return 0;
}