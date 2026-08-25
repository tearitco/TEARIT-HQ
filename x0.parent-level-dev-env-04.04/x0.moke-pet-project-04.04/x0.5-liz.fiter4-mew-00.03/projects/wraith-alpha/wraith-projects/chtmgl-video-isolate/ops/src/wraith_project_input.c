#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../../../../../../libraries/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024
#define PREVIEW_W 24
#define PREVIEW_H 12

typedef struct {
    int poster_loaded;
    int poster_width;
    int poster_height;
    int current_frame_loaded;
    int video_loaded;
    int audio_loaded;
    int autoplay_debug;
    double video_duration;
    int video_width;
    int video_height;
    int frame_index;
    int frame_total;
    long history_cursor;
    char video_rel[256];
    char video_abs[MAX_PATH_LEN];
    char poster_rel[256];
    char poster_abs[MAX_PATH_LEN];
    char current_frame_rel[256];
    char current_frame_abs[MAX_PATH_LEN];
    char audio_rel[256];
    char audio_abs[MAX_PATH_LEN];
    char video_state[32];
    char audio_state[32];
    char last_response[256];
    char lines[PREVIEW_H][PREVIEW_W + 1];
    char color_lines[PREVIEW_H][PREVIEW_W * 24 + 16];
    char cell_bg[PREVIEW_H][PREVIEW_W][8];
    char cell_fg[PREVIEW_H][PREVIEW_W][8];
} VideoProbeState;

static void path_join(char *out, size_t out_sz, const char *root, const char *rel) {
    snprintf(out, out_sz, "%s/%s", root, rel);
}

/* 2026-07-11: reads CHROME_CONTENT_START's live value, published by
 * wraith-alpha_manager.c (see that file's publish_chrome_reserved_nav_count(),
 * added same day) to pieces/display/chrome_reserved_nav_count.txt, instead
 * of hardcoding this project's own guess about where its nav range may
 * safely start. This ops binary is fork+exec'd by
 * wraith-alpha_manager.c's run_active_project_input_op() WITHOUT a chdir(),
 * so it inherits the manager's own cwd -- the relative path below is
 * correct without combining it with `root` (which is this PROJECT's own
 * dir, not the repo root). Falls back to the literal this file always used
 * before if the manager hasn't published yet. */
static int read_chrome_content_start(int fallback) {
    FILE *f = fopen("pieces/display/chrome_reserved_nav_count.txt", "r");
    int value;
    if (!f) return fallback;
    if (fscanf(f, "%d", &value) != 1 || value <= 0) {
        fclose(f);
        return fallback;
    }
    fclose(f);
    return value;
}

static void build_scene_ref(char *out, size_t out_sz, const char *root, const char *rel) {
    char joined[MAX_PATH_LEN];
    char resolved[MAX_PATH_LEN];

    out[0] = '\0';
    if (!root || !root[0] || !rel || !rel[0]) {
        return;
    }
    snprintf(joined, sizeof(joined), "%s/%s", root, rel);
    if (realpath(joined, resolved) != NULL) {
        snprintf(out, out_sz, "%s", resolved);
        return;
    }
    snprintf(out, out_sz, "%s", joined);
}

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void fill_placeholder(VideoProbeState *state, const char *msg) {
    int y;
    for (y = 0; y < PREVIEW_H; y++) {
        memset(state->lines[y], '.', PREVIEW_W);
        state->lines[y][PREVIEW_W] = '\0';
        for (int x = 0; x < PREVIEW_W; x++) {
            snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#0B1118");
            snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#E8F1F2");
        }
    }
    if (msg && msg[0]) {
        snprintf(state->last_response, sizeof(state->last_response), "%s", msg);
    }
}

static void default_state(VideoProbeState *state) {
    memset(state, 0, sizeof(*state));
    snprintf(state->video_rel, sizeof(state->video_rel), "../../../../../#.media-library/sample-10s-vp9.mp4");
    snprintf(state->poster_rel, sizeof(state->poster_rel), "session/poster.png");
    snprintf(state->current_frame_rel, sizeof(state->current_frame_rel), "session/current_frame.png");
    snprintf(state->audio_rel, sizeof(state->audio_rel), "session/video_audio.mp3");
    snprintf(state->video_state, sizeof(state->video_state), "stopped");
    snprintf(state->audio_state, sizeof(state->audio_state), "stopped");
    state->autoplay_debug = 1;
    snprintf(state->last_response, sizeof(state->last_response), "CHTMGL Video isolate ready.");
    fill_placeholder(state, state->last_response);
}

static int run_cmd(const char *cmd) {
    int status = system(cmd);
    if (status < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void probe_video_metadata(VideoProbeState *state) {
    char cmd[MAX_PATH_LEN * 2];
    FILE *fp;
    char line[256];

    struct stat st;
    if (stat(state->video_abs, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(state->video_state, sizeof(state->video_state), "missing");
        snprintf(state->last_response, sizeof(state->last_response), "Video missing: %s", state->video_rel);
        return;
    }

    state->video_loaded = 1;

    snprintf(cmd, sizeof(cmd),
        "ffprobe -v error -select_streams v:0 -show_entries stream=width,height -show_entries format=duration -of default=noprint_wrappers=1 '%s'",
        state->video_abs);
    fp = popen(cmd, "r");
    if (!fp) {
        snprintf(state->last_response, sizeof(state->last_response), "ffprobe launch failed");
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        if (sscanf(line, "width=%d", &state->video_width) == 1) {
            continue;
        }
        if (sscanf(line, "height=%d", &state->video_height) == 1) {
            continue;
        }
        if (sscanf(line, "duration=%lf", &state->video_duration) == 1) {
            continue;
        }
    }

    pclose(fp);
}

static void ensure_audio_track(VideoProbeState *state) {
    struct stat audio_st;
    struct stat video_st;
    char cmd[MAX_PATH_LEN * 3];

    if (!state->video_loaded) {
        return;
    }
    if (stat(state->video_abs, &video_st) != 0) {
        return;
    }

    if (stat(state->audio_abs, &audio_st) == 0 && audio_st.st_mtime >= video_st.st_mtime) {
        state->audio_loaded = 1;
        return;
    }

    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s' -vn -ac 2 -ar 22050 -codec:a libmp3lame '%s' >/dev/null 2>&1",
        state->video_abs,
        state->audio_abs);
    if (run_cmd(cmd) == 0) {
        state->audio_loaded = 1;
    } else {
        state->audio_loaded = 0;
        snprintf(state->audio_state, sizeof(state->audio_state), "missing");
    }
}

static void invalidate_stale_current_frame(const char *root, VideoProbeState *state) {
    char stamp_path[MAX_PATH_LEN];
    char saved[256];
    char current[256];
    struct stat st;
    FILE *f;

    snprintf(stamp_path, sizeof(stamp_path), "%s/session/video.source.txt", root);
    if (stat(state->video_abs, &st) != 0) {
        return;
    }
    snprintf(current, sizeof(current), "%s|%ld|%ld", state->video_abs, (long) st.st_mtime, (long) st.st_size);
    f = fopen(stamp_path, "r");
    if (!f) {
        remove(state->current_frame_abs);
        state->current_frame_loaded = 0;
        return;
    }
    if (!fgets(saved, sizeof(saved), f)) {
        fclose(f);
        remove(state->current_frame_abs);
        state->current_frame_loaded = 0;
        return;
    }
    fclose(f);
    trim_newline(saved);
    if (strcmp(saved, current) != 0) {
        remove(state->current_frame_abs);
        state->current_frame_loaded = 0;
        state->frame_index = 0;
        state->frame_total = 0;
    }
}

static void ensure_poster(VideoProbeState *state) {
    struct stat poster_st;
    struct stat video_st;
    char cmd[MAX_PATH_LEN * 3];

    if (!state->video_loaded) {
        return;
    }

    if (stat(state->video_abs, &video_st) != 0) {
        return;
    }

    if (stat(state->poster_abs, &poster_st) == 0 && poster_st.st_mtime >= video_st.st_mtime) {
        return;
    }

    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -i '%s' -vf \"thumbnail,scale=320:240\" -frames:v 1 '%s' >/dev/null 2>&1",
        state->video_abs,
        state->poster_abs);
    if (run_cmd(cmd) == 0) {
        snprintf(state->last_response, sizeof(state->last_response), "Poster refreshed from %s", state->video_rel);
    } else {
        snprintf(state->last_response, sizeof(state->last_response), "Poster extraction failed");
    }
}

static int load_and_project_poster(VideoProbeState *state) {
    int x, y;
    int src_w, src_h, src_channels;
    unsigned char *pixels;
    static const char *ramp = " .:-=+*#%@";
    const int ramp_len = 10;

    pixels = stbi_load(state->poster_abs, &src_w, &src_h, &src_channels, 4);
    if (!pixels) {
        fill_placeholder(state, "Poster load failed.");
        return 0;
    }

    state->poster_loaded = 1;
    state->poster_width = src_w;
    state->poster_height = src_h;

    for (y = 0; y < PREVIEW_H; y++) {
        int color_pos = 0;
        for (x = 0; x < PREVIEW_W; x++) {
            int src_x = (x * src_w) / PREVIEW_W;
            int src_y = (y * src_h) / PREVIEW_H;
            int idx = (src_y * src_w + src_x) * 4;
            int r = pixels[idx];
            int g = pixels[idx + 1];
            int b = pixels[idx + 2];
            int a = pixels[idx + 3];
            int lum;
            int bucket;

            if (a < 32) {
                state->lines[y][x] = ' ';
                snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#0B1118");
                snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#E8F1F2");
                color_pos += snprintf(state->color_lines[y] + color_pos,
                    sizeof(state->color_lines[y]) - (size_t) color_pos,
                    "  ");
                continue;
            }

            lum = (r * 30 + g * 59 + b * 11) / 100;
            bucket = (lum * (ramp_len - 1)) / 255;
            if (bucket < 0) bucket = 0;
            if (bucket >= ramp_len) bucket = ramp_len - 1;
            state->lines[y][x] = ramp[bucket];
            snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#%02X%02X%02X", r, g, b);
            if (lum < 128) {
                snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#F7FAFC");
            } else {
                snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#0B1118");
            }
            color_pos += snprintf(state->color_lines[y] + color_pos,
                sizeof(state->color_lines[y]) - (size_t) color_pos,
                "\x1b[48;2;%d;%d;%dm  ", r, g, b);
        }
        state->lines[y][PREVIEW_W] = '\0';
        snprintf(state->color_lines[y] + color_pos,
            sizeof(state->color_lines[y]) - (size_t) color_pos,
            "\x1b[0m");
    }

    if (!strstr(state->last_response, "Poster")) {
        snprintf(state->last_response, sizeof(state->last_response),
            "VIDEO ready %dx%d %.2fs", state->video_width, state->video_height, state->video_duration);
    }
    stbi_image_free(pixels);
    return 1;
}

static void load_playback_status(const char *root, VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;

    path_join(path, sizeof(path), root, "session/video.playback");
    f = fopen(path, "r");
    if (!f) {
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (sscanf(line, "frame_index=%d", &state->frame_index) == 1) {
            continue;
        }
        if (sscanf(line, "frame_total=%d", &state->frame_total) == 1) {
            continue;
        }
        if (strncmp(line, "state=", 6) == 0 && line[6]) {
            snprintf(state->video_state, sizeof(state->video_state), "%s", line + 6);
        }
    }
    fclose(f);
}

static void prefer_current_frame(VideoProbeState *state) {
    struct stat st;
    if (stat(state->current_frame_abs, &st) == 0 && S_ISREG(st.st_mode)) {
        state->current_frame_loaded = 1;
        snprintf(state->poster_abs, sizeof(state->poster_abs), "%s", state->current_frame_abs);
    }
}

static void load_previous_state(const char *root, VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    char line[512];
    FILE *f;

    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) {
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "video_state=", 12) == 0 && line[12]) {
            snprintf(state->video_state, sizeof(state->video_state), "%s", line + 12);
        } else if (strncmp(line, "audio_state=", 12) == 0 && line[12]) {
            snprintf(state->audio_state, sizeof(state->audio_state), "%s", line + 12);
        } else if (strncmp(line, "video_loaded=", 13) == 0 && line[13]) {
            state->video_loaded = atoi(line + 13) ? 1 : 0;
        } else if (strncmp(line, "audio_loaded=", 13) == 0 && line[13]) {
            state->audio_loaded = atoi(line + 13) ? 1 : 0;
        } else if (strncmp(line, "video_width=", 12) == 0 && line[12]) {
            state->video_width = atoi(line + 12);
        } else if (strncmp(line, "video_height=", 13) == 0 && line[13]) {
            state->video_height = atoi(line + 13);
        } else if (strncmp(line, "video_duration=", 15) == 0 && line[15]) {
            state->video_duration = atof(line + 15);
        } else if (strncmp(line, "poster_loaded=", 14) == 0 && line[14]) {
            state->poster_loaded = atoi(line + 14) ? 1 : 0;
        } else if (strncmp(line, "poster_width=", 13) == 0 && line[13]) {
            state->poster_width = atoi(line + 13);
        } else if (strncmp(line, "poster_height=", 14) == 0 && line[14]) {
            state->poster_height = atoi(line + 14);
        } else if (strncmp(line, "current_frame_loaded=", 21) == 0 && line[21]) {
            state->current_frame_loaded = atoi(line + 21) ? 1 : 0;
        } else if (strncmp(line, "frame_index=", 12) == 0 && line[12]) {
            state->frame_index = atoi(line + 12);
        } else if (strncmp(line, "frame_total=", 12) == 0 && line[12]) {
            state->frame_total = atoi(line + 12);
        } else if (strncmp(line, "autoplay_debug=", 15) == 0 && line[15]) {
            state->autoplay_debug = atoi(line + 15) ? 1 : 0;
        } else if (strncmp(line, "last_response=", 14) == 0 && line[14]) {
            snprintf(state->last_response, sizeof(state->last_response), "%s", line + 14);
        } else if (strncmp(line, "history_cursor=", 15) == 0 && line[15]) {
            state->history_cursor = atol(line + 15);
        }
    }
    fclose(f);
}

static int read_latest_command(const char *root, long *cursor, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    char line[512];
    struct stat st;
    FILE *f;
    int found = 0;

    out[0] = '\0';
    path_join(path, sizeof(path), root, "session/history.txt");
    if (stat(path, &st) != 0) {
        return 0;
    }
    if (*cursor < 0 || st.st_size < *cursor) {
        *cursor = 0;
    }
    if (st.st_size <= *cursor) {
        return 0;
    }
    f = fopen(path, "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        char *tag = strstr(line, "COMMAND: ");
        if (!tag) {
            continue;
        }
        tag += 9;
        trim_newline(tag);
        snprintf(out, out_sz, "%s", tag);
        found = 1;
    }

    fclose(f);
    *cursor = (long) st.st_size;
    return found;
}

static int run_video_player(const char *root, const char *arg) {
    char op_path[MAX_PATH_LEN];
    pid_t pid;
    int status = -1;

    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_video_player.+x", root);
    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, arg, root, (char *)NULL);
        _exit(1);
    }
    if (pid < 0) {
        return -1;
    }
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static int run_audio_player(const char *root, const char *arg) {
    char op_path[MAX_PATH_LEN];
    pid_t pid;
    int status = -1;

    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_audio.+x", root);
    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, arg, root, (char *)NULL);
        _exit(1);
    }
    if (pid < 0) {
        return -1;
    }
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static int handle_command(const char *root, VideoProbeState *state) {
    char command[128];

    if (!read_latest_command(root, &state->history_cursor, command, sizeof(command))) {
        return 0;
    }

    if (strcmp(command, "VIDEO_PLAY") == 0) {
        if (!state->video_loaded) {
            snprintf(state->video_state, sizeof(state->video_state), "missing");
            snprintf(state->last_response, sizeof(state->last_response), "Video missing: %s", state->video_rel);
            return 1;
        }
        if (run_video_player(root, state->video_abs) == 0) {
            if (state->audio_loaded) {
                run_audio_player(root, state->audio_abs);
                snprintf(state->audio_state, sizeof(state->audio_state), "playing");
            }
            snprintf(state->video_state, sizeof(state->video_state), "playing");
            snprintf(state->last_response, sizeof(state->last_response), "Video playing: %s", state->video_rel);
        } else {
            snprintf(state->video_state, sizeof(state->video_state), "error");
            snprintf(state->last_response, sizeof(state->last_response), "Video play failed");
        }
        return 1;
    }

    if (strcmp(command, "VIDEO_STOP") == 0) {
        if (run_video_player(root, "--stop") == 0) {
            if (state->audio_loaded) {
                run_audio_player(root, "--stop");
                snprintf(state->audio_state, sizeof(state->audio_state), "stopped");
            }
            snprintf(state->video_state, sizeof(state->video_state), state->video_loaded ? "stopped" : "missing");
            snprintf(state->last_response, sizeof(state->last_response), "Video stopped");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Video stop failed");
        }
        return 1;
    }

    if (strcmp(command, "VIDEO_PAUSE") == 0) {
        if (run_video_player(root, "--pause") == 0) {
            if (state->audio_loaded) {
                run_audio_player(root, "--pause");
                snprintf(state->audio_state, sizeof(state->audio_state), "paused");
            }
            snprintf(state->video_state, sizeof(state->video_state), "paused");
            snprintf(state->last_response, sizeof(state->last_response), "Video paused");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Video pause failed");
        }
        return 1;
    }

    if (strcmp(command, "VIDEO_RESUME") == 0) {
        if (run_video_player(root, "--resume") == 0) {
            if (state->audio_loaded) {
                run_audio_player(root, "--resume");
                snprintf(state->audio_state, sizeof(state->audio_state), "playing");
            }
            snprintf(state->video_state, sizeof(state->video_state), "playing");
            snprintf(state->last_response, sizeof(state->last_response), "Video resumed");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Video resume failed");
        }
        return 1;
    }

    if (strcmp(command, "VIDEO_REFRESH") == 0) {
        ensure_poster(state);
        return 1;
    }
    return 0;
}

static void maybe_autoplay_debug(const char *root, VideoProbeState *state, int command_handled) {
    if (command_handled || !state->autoplay_debug || !state->video_loaded) {
        return;
    }
    if (state->current_frame_loaded || state->frame_total > 0) {
        return;
    }
    if (run_video_player(root, state->video_abs) == 0) {
        if (state->audio_loaded) {
            run_audio_player(root, state->audio_abs);
            snprintf(state->audio_state, sizeof(state->audio_state), "playing");
        }
        snprintf(state->video_state, sizeof(state->video_state), "playing");
        snprintf(state->last_response, sizeof(state->last_response), "Video autoplay debug: %s", state->video_rel);
    }
}

static void write_state(const char *root, const VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/chtmgl-video-isolate\n");
    fprintf(f, "reference_project=x0.parent-level-dev-env-02.01/#.CHTMGL.E23=cordsclean]💯️\n");
    fprintf(f, "game_map=chtmgl_video_surface\n");
    fprintf(f, "is_map_control=0\n");
    fprintf(f, "widget_probe=video,img,poster,transport,in_wraith_frame_swap\n");
    fprintf(f, "autoplay_debug=%d\n", state->autoplay_debug);
    fprintf(f, "video_asset=%s\n", state->video_rel);
    fprintf(f, "video_loaded=%d\n", state->video_loaded);
    fprintf(f, "video_width=%d\n", state->video_width);
    fprintf(f, "video_height=%d\n", state->video_height);
    fprintf(f, "video_duration=%.2f\n", state->video_duration);
    fprintf(f, "poster_asset=%s\n", state->poster_rel);
    fprintf(f, "poster_loaded=%d\n", state->poster_loaded);
    fprintf(f, "poster_width=%d\n", state->poster_width);
    fprintf(f, "poster_height=%d\n", state->poster_height);
    fprintf(f, "current_frame_asset=%s\n", state->current_frame_rel);
    fprintf(f, "current_frame_loaded=%d\n", state->current_frame_loaded);
    fprintf(f, "frame_index=%d\n", state->frame_index);
    fprintf(f, "frame_total=%d\n", state->frame_total);
    fprintf(f, "audio_asset=%s\n", state->audio_rel);
    fprintf(f, "audio_loaded=%d\n", state->audio_loaded);
    fprintf(f, "audio_state=%s\n", state->audio_state);
    fprintf(f, "ascii_preview=%dx%d\n", PREVIEW_W, PREVIEW_H);
    fprintf(f, "video_state=%s\n", state->video_state);
    fprintf(f, "last_response=%s\n", state->last_response);
    fprintf(f, "history_cursor=%ld\n", state->history_cursor);
    fclose(f);
}

static void write_body(const char *root, const VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    const char *grid_project_rel = "projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/session/poster_preview.grid.pdl";
    const char *row_prefix = "| |  ";
    const char *row_suffix = "                                                           |";
    FILE *f;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "<text label=\"| |  CHTMGL VIDEO ISOLATE | video-first proof                                            |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  [HEADER] CHTMGL Video Tag Probe                                                     |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  VIDEO_ISOLATE                                                                       |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  [ASCII Color Preview]                                                               |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<grid source=\"%s\" prefix=\"%s\" suffix=\"%s\" />\n", grid_project_rel, row_prefix, row_suffix);
    fprintf(f, "<text label=\"| |  STATE:%s                                                                          |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->video_state);
    fprintf(f, "<text label=\"| |  %dx%d_%.2fs                                                                        |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->video_width, state->video_height, state->video_duration);
    fprintf(f, "<text label=\"| |  FRAME:%d/%d                                                                         |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->frame_index, state->frame_total);
    fprintf(f, "<text label=\"| |  \" /><button compact=\"true\" label=\"Play\" onClick=\"PROJECT_ACTION:VIDEO_PLAY\" /><text label=\" \" /><button compact=\"true\" label=\"Pause\" onClick=\"PROJECT_ACTION:VIDEO_PAUSE\" /><text label=\"                                                      |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  \" /><button compact=\"true\" label=\"Resume\" onClick=\"PROJECT_ACTION:VIDEO_RESUME\" /><text label=\" \" /><button compact=\"true\" label=\"Stop\" onClick=\"PROJECT_ACTION:VIDEO_STOP\" /><text label=\"                                                     |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  \" /><button compact=\"true\" label=\"Refresh_Poster\" onClick=\"PROJECT_ACTION:VIDEO_REFRESH\" /><text label=\"                                                |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  VIDEO:%s                                                                          |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->video_state);
    fprintf(f, "<text label=\"| |  Last: %s\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->last_response);
    fclose(f);
}

static void write_poster_grid(const char *root, const VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int y;
    int x;

    path_join(path, sizeof(path), root, "session/poster_preview.grid.pdl");
    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "GRID id=video_poster_ascii rows=%d cols=%d cell_w=1 cell_h=1 source_ref=%s\n",
        PREVIEW_H,
        PREVIEW_W,
        state->current_frame_loaded ? state->current_frame_rel : state->poster_rel);
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            unsigned char glyph = (unsigned char)(state->lines[y][x] ? state->lines[y][x] : ' ');
            fprintf(f, "CELL row=%d col=%d ch_hex=%02X fg=%s bg=%s\n",
                y + 1,
                x + 1,
                glyph,
                state->cell_fg[y][x],
                state->cell_bg[y][x]);
        }
    }
    fclose(f);
}

static void write_scene(const char *root, const VideoProbeState *state) {
    char path[MAX_PATH_LEN];
    char video_scene_ref[MAX_PATH_LEN];
    char poster_scene_ref[MAX_PATH_LEN];
    char current_frame_scene_ref[MAX_PATH_LEN];
    FILE *f;
    int y;
    int x;
    int nav_base = read_chrome_content_start(6);

    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    build_scene_ref(video_scene_ref, sizeof(video_scene_ref), root, state->video_rel);
    build_scene_ref(poster_scene_ref, sizeof(poster_scene_ref), root, state->poster_rel);
    build_scene_ref(current_frame_scene_ref, sizeof(current_frame_scene_ref), root, state->current_frame_rel);

    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "OBJECT tag=panel id=video_controls role=chtmgl_panel x=3 y=6 w=24 h=12 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Video_Controls\n", video_scene_ref);
    fprintf(f, "OBJECT tag=text id=video_header role=chtmgl_header x=4 y=6 w=22 h=1 z=28 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=VIDEO_ISOLATE\n", video_scene_ref);
    fprintf(f, "OBJECT tag=text id=video_status role=video_status x=5 y=8 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=STATE:%s\n", video_scene_ref, state->video_state);
    fprintf(f, "OBJECT tag=text id=video_meta role=video_meta x=5 y=9 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=%dx%d_%.2fs\n", video_scene_ref, state->video_width, state->video_height, state->video_duration);
    fprintf(f, "OBJECT tag=text id=video_frame role=video_frame_meta x=5 y=10 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=FRAME:%d/%d\n", video_scene_ref, state->frame_index, state->frame_total);
    fprintf(f, "OBJECT tag=control id=video_play role=chtmgl_button x=5 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:VIDEO_PLAY label=Play\n", nav_base + 0, video_scene_ref);
    fprintf(f, "OBJECT tag=control id=video_pause role=chtmgl_button x=14 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:VIDEO_PAUSE label=Pause\n", nav_base + 1, video_scene_ref);
    fprintf(f, "OBJECT tag=control id=video_resume role=chtmgl_button x=5 y=14 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:VIDEO_RESUME label=Resume\n", nav_base + 2, video_scene_ref);
    fprintf(f, "OBJECT tag=control id=video_stop role=chtmgl_button x=14 y=14 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:VIDEO_STOP label=Stop\n", nav_base + 3, video_scene_ref);
    fprintf(f, "OBJECT tag=control id=video_refresh role=chtmgl_button x=5 y=16 w=17 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#FFD166 action=PROJECT_ACTION:VIDEO_REFRESH label=Refresh_Poster\n", nav_base + 4, video_scene_ref);

    fprintf(f, "OBJECT tag=panel id=video_visual_panel role=chtmgl_panel x=31 y=6 w=52 h=18 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Video_Poster\n", state->current_frame_loaded ? current_frame_scene_ref : poster_scene_ref);
    fprintf(f, "OBJECT tag=text id=video_title role=video_state x=33 y=8 w=48 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=VIDEO:%s\n", video_scene_ref, state->video_state);
    fprintf(f, "OBJECT tag=img id=video_poster role=image_asset x=62 y=9 w=18 h=10 z=33 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=VIDEO_FRAME:missingno-gowon\n", state->current_frame_loaded ? current_frame_scene_ref : poster_scene_ref);
    fprintf(f, "OBJECT tag=panel id=poster_preview_panel role=video_poster_preview x=33 y=9 w=28 h=13 z=26 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=ASCII_POSTER\n", state->current_frame_loaded ? current_frame_scene_ref : poster_scene_ref);

    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            char glyph[2];
            glyph[0] = state->lines[y][x] ? state->lines[y][x] : ' ';
            glyph[1] = '\0';
            fprintf(f, "OBJECT tag=text id=poster_preview_%02d_%02d role=poster_preview_cell x=%d y=%d w=1 h=1 z=30 nav=0 source_ref=%s fg=%s bg=%s border=%s action=- label=%s\n",
                y + 1,
                x + 1,
                35 + x,
                10 + y,
                state->current_frame_loaded ? current_frame_scene_ref : poster_scene_ref,
                state->cell_fg[y][x],
                state->cell_bg[y][x],
                state->cell_bg[y][x],
                glyph);
        }
    }

    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=62 y=9 w=18 h=10 z=20 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=video_surface_hint role=widget_surface_probe x=54 y=23 w=28 h=1 z=24 source=projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/layouts/chtmgl-video-isolate.chtpm fg=#7EDFF2 bg=#0B1118 border=#FFD166 action=- label=VIDEO:in_wraith_frame_swap\n");
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root = ".";
    int marker_tick_mode = 0;
    VideoProbeState state;

    default_state(&state);
    if (argc > 1 && argv[1][0]) {
        root = argv[1];
    }
    if (argc > 2 && strcmp(argv[2], "marker_tick") == 0) {
        marker_tick_mode = 1;
    }

    path_join(state.video_abs, sizeof(state.video_abs), root, state.video_rel);
    path_join(state.poster_abs, sizeof(state.poster_abs), root, state.poster_rel);
    path_join(state.current_frame_abs, sizeof(state.current_frame_abs), root, state.current_frame_rel);
    path_join(state.audio_abs, sizeof(state.audio_abs), root, state.audio_rel);

    load_previous_state(root, &state);
    if (!marker_tick_mode) {
        probe_video_metadata(&state);
        ensure_audio_track(&state);
        ensure_poster(&state);
    }
    invalidate_stale_current_frame(root, &state);
    prefer_current_frame(&state);
    load_and_project_poster(&state);
    load_playback_status(root, &state);
    if (!marker_tick_mode) {
        int command_handled = handle_command(root, &state);
        maybe_autoplay_debug(root, &state, command_handled);
    }
    load_playback_status(root, &state);
    prefer_current_frame(&state);
    load_and_project_poster(&state);
    write_state(root, &state);
    write_poster_grid(root, &state);
    write_body(root, &state);
    write_scene(root, &state);
    return 0;
}
