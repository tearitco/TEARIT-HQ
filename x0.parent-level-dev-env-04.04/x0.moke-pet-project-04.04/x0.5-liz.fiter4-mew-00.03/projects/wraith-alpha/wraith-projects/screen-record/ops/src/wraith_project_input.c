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
    long history_cursor;
    char state[32];
    char detail[256];
    char output_rel[256];
    char audio_mode[64];
    char poster_rel[256];
    char poster_abs[MAX_PATH_LEN];
    char lines[PREVIEW_H][PREVIEW_W + 1];
    char cell_bg[PREVIEW_H][PREVIEW_W][8];
    char cell_fg[PREVIEW_H][PREVIEW_W][8];
} RecordState;

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
    snprintf(joined, sizeof(joined), "%s/%s", root, rel);
    if (realpath(joined, resolved)) {
        snprintf(out, out_sz, "%s", resolved);
        return;
    }
    snprintf(out, out_sz, "%s", joined);
}

static void trim_newline(char *s) {
    if (s) s[strcspn(s, "\r\n")] = '\0';
}

static void copy_str(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

static void fill_placeholder(RecordState *state, const char *glyph) {
    int y, x;
    char use = (glyph && glyph[0]) ? glyph[0] : '.';
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            state->lines[y][x] = use;
            snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#E8F1F2");
            snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#162534");
        }
        state->lines[y][PREVIEW_W] = '\0';
    }
}

static void default_state(RecordState *state) {
    memset(state, 0, sizeof(*state));
    snprintf(state->state, sizeof(state->state), "stopped");
    snprintf(state->detail, sizeof(state->detail), "Screen record ready.");
    snprintf(state->audio_mode, sizeof(state->audio_mode), "video_only");
    snprintf(state->poster_rel, sizeof(state->poster_rel), "session/last_capture.png");
    fill_placeholder(state, ".");
}

static void load_previous_state(const char *root, RecordState *state) {
    char path[MAX_PATH_LEN];
    char line[512];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "state=", 6) == 0) copy_str(state->state, sizeof(state->state), line + 6);
        else if (strncmp(line, "detail=", 7) == 0) copy_str(state->detail, sizeof(state->detail), line + 7);
        else if (strncmp(line, "output_rel=", 11) == 0) copy_str(state->output_rel, sizeof(state->output_rel), line + 11);
        else if (strncmp(line, "audio_mode=", 11) == 0) copy_str(state->audio_mode, sizeof(state->audio_mode), line + 11);
        else if (strncmp(line, "history_cursor=", 15) == 0) state->history_cursor = atol(line + 15);
    }
    fclose(f);
}

static void load_status(const char *root, RecordState *state) {
    char path[MAX_PATH_LEN];
    char line[512];
    FILE *f;
    path_join(path, sizeof(path), root, "session/record.status");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "state=", 6) == 0) copy_str(state->state, sizeof(state->state), line + 6);
        else if (strncmp(line, "detail=", 7) == 0) copy_str(state->detail, sizeof(state->detail), line + 7);
        else if (strncmp(line, "output=", 7) == 0) copy_str(state->output_rel, sizeof(state->output_rel), line + 7);
        else if (strncmp(line, "audio=", 6) == 0) copy_str(state->audio_mode, sizeof(state->audio_mode), line + 6);
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
    if (stat(path, &st) != 0) return 0;
    if (*cursor < 0 || st.st_size < *cursor) *cursor = 0;
    if (st.st_size <= *cursor) return 0;
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *tag = strstr(line, "COMMAND: ");
        if (!tag) continue;
        tag += 9;
        trim_newline(tag);
        snprintf(out, out_sz, "%s", tag);
        found = 1;
    }
    fclose(f);
    *cursor = (long)st.st_size;
    return found;
}

static int run_record_op(const char *root, const char *arg) {
    char op_path[MAX_PATH_LEN];
    pid_t pid;
    int status = -1;
    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_screen_record.+x", root);
    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, arg, root, (char *)NULL);
        _exit(1);
    }
    if (pid < 0) return -1;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static void handle_command(const char *root, RecordState *state) {
    char command[128];
    if (!read_latest_command(root, &state->history_cursor, command, sizeof(command))) return;
    if (strcmp(command, "SCREENREC_START") == 0) {
        if (run_record_op(root, "--start") == 0) {
            snprintf(state->state, sizeof(state->state), "starting");
            snprintf(state->detail, sizeof(state->detail), "Start requested");
        }
    } else if (strcmp(command, "SCREENREC_STOP") == 0) {
        if (run_record_op(root, "--stop") == 0) {
            snprintf(state->detail, sizeof(state->detail), "Stop requested");
        }
    } else if (strcmp(command, "SCREENREC_REFRESH") == 0) {
        snprintf(state->detail, sizeof(state->detail), "Refresh requested");
    }
}

static void load_poster_and_project(RecordState *state) {
    int x, y, src_w, src_h, src_channels;
    unsigned char *pixels;
    static const char *ramp = " .:-=+*#%@";
    const int ramp_len = 10;
    struct stat st;

    if (stat(state->poster_abs, &st) != 0) {
        fill_placeholder(state, state->state[0] ? state->state : ".");
        return;
    }
    pixels = stbi_load(state->poster_abs, &src_w, &src_h, &src_channels, 4);
    if (!pixels) {
        fill_placeholder(state, "!");
        return;
    }
    state->poster_loaded = 1;
    state->poster_width = src_w;
    state->poster_height = src_h;
    for (y = 0; y < PREVIEW_H; y++) {
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
                snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "#E8F1F2");
                snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#0B1118");
                continue;
            }
            lum = (r * 30 + g * 59 + b * 11) / 100;
            bucket = (lum * (ramp_len - 1)) / 255;
            if (bucket < 0) bucket = 0;
            if (bucket >= ramp_len) bucket = ramp_len - 1;
            state->lines[y][x] = ramp[bucket];
            snprintf(state->cell_bg[y][x], sizeof(state->cell_bg[y][x]), "#%02X%02X%02X", r, g, b);
            snprintf(state->cell_fg[y][x], sizeof(state->cell_fg[y][x]), "%s", lum < 128 ? "#F7FAFC" : "#0B1118");
        }
        state->lines[y][PREVIEW_W] = '\0';
    }
    stbi_image_free(pixels);
}

static void write_grid(const char *root, const RecordState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int y, x;
    path_join(path, sizeof(path), root, "session/record_preview.grid.pdl");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "GRID id=screen_record_ascii rows=%d cols=%d cell_w=1 cell_h=1 source_ref=%s\n", PREVIEW_H, PREVIEW_W, state->poster_rel);
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            fprintf(f, "CELL row=%d col=%d ch_hex=%02X fg=%s bg=%s\n",
                y + 1, x + 1, (unsigned char)(state->lines[y][x] ? state->lines[y][x] : ' '),
                state->cell_fg[y][x], state->cell_bg[y][x]);
        }
    }
    fclose(f);
}

static void write_body(const char *root, const RecordState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "<text label=\"| |  WRAITH SCREEN RECORD | gl-session capture proof                                     |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  STATE:%s                                                                      |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->state);
    fprintf(f, "<text label=\"| |  AUDIO:%s                                                                      |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->audio_mode);
    fprintf(f, "<text label=\"| |  OUTPUT:%s\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->output_rel[0] ? state->output_rel : "(pending)");
    fprintf(f, "<text label=\"| |  DETAIL:%s\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->detail);
    fprintf(f, "<text label=\"| |  [ASCII Poster Preview]                                                              |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<grid source=\"projects/wraith-alpha/wraith-projects/screen-record/session/record_preview.grid.pdl\" prefix=\"| |  \" suffix=\"                                                           |\" />\n");
    fprintf(f, "<text label=\"| |  \" /><button compact=\"true\" label=\"Start\" onClick=\"PROJECT_ACTION:SCREENREC_START\" /><text label=\" \" /><button compact=\"true\" label=\"Stop\" onClick=\"PROJECT_ACTION:SCREENREC_STOP\" /><text label=\" \" /><button compact=\"true\" label=\"Refresh\" onClick=\"PROJECT_ACTION:SCREENREC_REFRESH\" /><text label=\"                           |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fclose(f);
}

static void write_state(const char *root, const RecordState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/screen-record\n");
    fprintf(f, "state=%s\n", state->state);
    fprintf(f, "detail=%s\n", state->detail);
    fprintf(f, "output_rel=%s\n", state->output_rel);
    fprintf(f, "audio_mode=%s\n", state->audio_mode);
    fprintf(f, "poster_loaded=%d\n", state->poster_loaded);
    fprintf(f, "poster_width=%d\n", state->poster_width);
    fprintf(f, "poster_height=%d\n", state->poster_height);
    fprintf(f, "history_cursor=%ld\n", state->history_cursor);
    fclose(f);
}

static void write_scene(const char *root, const RecordState *state) {
    char path[MAX_PATH_LEN];
    char poster_ref[MAX_PATH_LEN];
    FILE *f;
    int y, x;
    int nav_base = read_chrome_content_start(6);
    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    build_scene_ref(poster_ref, sizeof(poster_ref), root, state->poster_rel);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "OBJECT tag=panel id=record_controls role=chtmgl_panel x=3 y=6 w=24 h=12 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Record_Controls\n", poster_ref);
    fprintf(f, "OBJECT tag=text id=record_header role=chtmgl_header x=4 y=6 w=22 h=1 z=28 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=SCREEN_RECORD\n", poster_ref);
    fprintf(f, "OBJECT tag=text id=record_state role=record_state x=5 y=8 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=STATE:%s\n", poster_ref, state->state);
    fprintf(f, "OBJECT tag=text id=record_audio role=record_audio x=5 y=9 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=AUDIO:%s\n", poster_ref, state->audio_mode);
    fprintf(f, "OBJECT tag=control id=record_start role=chtmgl_button x=5 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:SCREENREC_START label=Start\n", nav_base + 0, poster_ref);
    fprintf(f, "OBJECT tag=control id=record_stop role=chtmgl_button x=14 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:SCREENREC_STOP label=Stop\n", nav_base + 1, poster_ref);
    fprintf(f, "OBJECT tag=control id=record_refresh role=chtmgl_button x=5 y=14 w=17 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#FFD166 action=PROJECT_ACTION:SCREENREC_REFRESH label=Refresh\n", nav_base + 2, poster_ref);
    fprintf(f, "OBJECT tag=panel id=record_visual_panel role=chtmgl_panel x=31 y=6 w=52 h=18 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Last_Capture\n", poster_ref);
    fprintf(f, "OBJECT tag=text id=record_output role=record_output x=33 y=8 w=48 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=OUTPUT:%s\n", poster_ref, state->output_rel[0] ? state->output_rel : "(pending)");
    fprintf(f, "OBJECT tag=img id=record_poster role=image_asset x=62 y=9 w=18 h=10 z=33 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=LAST_CAPTURE\n", poster_ref);
    fprintf(f, "OBJECT tag=panel id=record_preview_panel role=record_ascii_preview x=33 y=9 w=28 h=13 z=26 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=ASCII_POSTER\n", poster_ref);
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            char glyph[2];
            glyph[0] = state->lines[y][x] ? state->lines[y][x] : ' ';
            glyph[1] = '\0';
            fprintf(f, "OBJECT tag=text id=record_preview_%02d_%02d role=record_preview_cell x=%d y=%d w=1 h=1 z=30 nav=0 source_ref=%s fg=%s bg=%s border=%s action=- label=%s\n",
                y + 1, x + 1, 35 + x, 10 + y, poster_ref,
                state->cell_fg[y][x], state->cell_bg[y][x], state->cell_bg[y][x], glyph);
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root = ".";
    RecordState state;
    default_state(&state);
    if (argc > 1 && argv[1][0]) root = argv[1];
    load_previous_state(root, &state);
    path_join(state.poster_abs, sizeof(state.poster_abs), root, state.poster_rel);
    if (!(argc > 2 && strcmp(argv[2], "marker_tick") == 0)) {
        handle_command(root, &state);
    }
    load_status(root, &state);
    load_poster_and_project(&state);
    write_state(root, &state);
    write_grid(root, &state);
    write_body(root, &state);
    write_scene(root, &state);
    return 0;
}
