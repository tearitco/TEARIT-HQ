#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../../../../../../libraries/stb_image.h"

#include "../../../../../../pieces/chtpm/ops/lib/tpmos_share_kvp_runtime.c"
#include "../../../../../../pieces/chtpm/ops/lib/tpmos_live_frame_cache.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024
#define PREVIEW_W 24
#define PREVIEW_H 12
#define WEBCAM_FRAME_CACHE_KEY "projects/wraith-alpha/wraith-projects/web-cam/session/current_frame"

typedef struct {
    int frame_loaded;
    int frame_width;
    int frame_height;
    long history_cursor;
    long frame_epoch;
    char device[64];
    char capture_profile[32];
    char state[32];
    char detail[256];
    char frame_rel[256];
    char frame_abs[MAX_PATH_LEN];
    char lines[PREVIEW_H][PREVIEW_W + 1];
    char cell_bg[PREVIEW_H][PREVIEW_W][8];
    char cell_fg[PREVIEW_H][PREVIEW_W][8];
} WebcamState;

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

static void fill_placeholder(WebcamState *state, const char *glyph) {
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

static void default_state(WebcamState *state) {
    memset(state, 0, sizeof(*state));
    snprintf(state->device, sizeof(state->device), "/dev/video0");
    snprintf(state->capture_profile, sizeof(state->capture_profile), "fast");
    snprintf(state->state, sizeof(state->state), "stopped");
    snprintf(state->detail, sizeof(state->detail), "Web cam ready.");
    snprintf(state->frame_rel, sizeof(state->frame_rel), "session/current_frame.png");
    fill_placeholder(state, ".");
}

static void load_previous_state(const char *root, WebcamState *state) {
    char blob[4096];
    char *line;
    char *saveptr = NULL;
    if (tpmos_share_kvp_read_text(root, "session/state.txt", blob, sizeof(blob)) != 0 || !blob[0]) return;
    line = strtok_r(blob, "\n", &saveptr);
    while (line) {
        trim_newline(line);
        if (strncmp(line, "state=", 6) == 0) copy_str(state->state, sizeof(state->state), line + 6);
        else if (strncmp(line, "detail=", 7) == 0) copy_str(state->detail, sizeof(state->detail), line + 7);
        else if (strncmp(line, "device=", 7) == 0) copy_str(state->device, sizeof(state->device), line + 7);
        else if (strncmp(line, "capture_profile=", 16) == 0) copy_str(state->capture_profile, sizeof(state->capture_profile), line + 16);
        else if (strncmp(line, "history_cursor=", 15) == 0) state->history_cursor = atol(line + 15);
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

static void load_status(const char *root, WebcamState *state) {
    char blob[4096];
    char *line;
    char *saveptr = NULL;
    if (tpmos_share_kvp_read_text(root, "session/webcam.status", blob, sizeof(blob)) != 0 || !blob[0]) return;
    line = strtok_r(blob, "\n", &saveptr);
    while (line) {
        trim_newline(line);
        if (strncmp(line, "state=", 6) == 0) copy_str(state->state, sizeof(state->state), line + 6);
        else if (strncmp(line, "detail=", 7) == 0) copy_str(state->detail, sizeof(state->detail), line + 7);
        else if (strncmp(line, "frame_epoch=", 12) == 0) state->frame_epoch = atol(line + 12);
        line = strtok_r(NULL, "\n", &saveptr);
    }
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

static int run_capture_op(const char *root, const char *arg, const char *device, const char *profile) {
    char op_path[MAX_PATH_LEN];
    pid_t pid;
    int status = -1;
    snprintf(op_path, sizeof(op_path), "%s/ops/+x/wraith_webcam_capture.+x", root);
    pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, arg, root, device, profile ? profile : "fast", (char *)NULL);
        _exit(1);
    }
    if (pid < 0) return -1;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static void handle_command(const char *root, WebcamState *state) {
    char command[128];
    if (!read_latest_command(root, &state->history_cursor, command, sizeof(command))) return;
    if (strcmp(command, "WEBCAM_START") == 0) {
        if (run_capture_op(root, "--start", state->device, state->capture_profile) == 0) {
            snprintf(state->state, sizeof(state->state), "starting");
            snprintf(state->detail, sizeof(state->detail), "Starting capture on %s (%s)", state->device, state->capture_profile);
        }
    } else if (strcmp(command, "WEBCAM_STOP") == 0) {
        if (run_capture_op(root, "--stop", state->device, state->capture_profile) == 0) {
            snprintf(state->state, sizeof(state->state), "stopped");
            snprintf(state->detail, sizeof(state->detail), "Capture stopped");
        }
    } else if (strcmp(command, "WEBCAM_FAST") == 0) {
        snprintf(state->capture_profile, sizeof(state->capture_profile), "fast");
        snprintf(state->detail, sizeof(state->detail), "Capture profile set to fast");
    } else if (strcmp(command, "WEBCAM_DEBUG") == 0) {
        snprintf(state->capture_profile, sizeof(state->capture_profile), "debug");
        snprintf(state->detail, sizeof(state->detail), "Capture profile set to debug");
    } else if (strcmp(command, "WEBCAM_REFRESH") == 0) {
        snprintf(state->detail, sizeof(state->detail), "Refresh requested");
    }
}

static void load_frame_and_project(WebcamState *state) {
    int x, y, src_w, src_h, src_channels;
    unsigned char *pixels;
    unsigned char *cache_pixels = NULL;
    size_t cache_bytes = 0;
    unsigned char *blob_pixels = NULL;
    size_t blob_size = 0;
    static const char *ramp = " .:-=+*#%@";
    const int ramp_len = 10;
    struct stat st;

    pixels = NULL;
    if (tpmos_live_frame_cache_read_rgba(WEBCAM_FRAME_CACHE_KEY, &cache_pixels, &cache_bytes, &src_w, &src_h, &src_channels, NULL, NULL) == 0 && cache_pixels && cache_bytes > 0) {
        pixels = cache_pixels;
        cache_pixels = NULL;
    } else if (stat(state->frame_abs, &st) == 0) {
        pixels = stbi_load(state->frame_abs, &src_w, &src_h, &src_channels, 4);
    } else if (tpmos_share_kvp_read_blob_direct("session/current_frame.png", &blob_pixels, &blob_size, NULL) == 0 && blob_pixels && blob_size > 0) {
        pixels = stbi_load_from_memory(blob_pixels, (int)blob_size, &src_w, &src_h, &src_channels, 4);
        free(blob_pixels);
        blob_pixels = NULL;
    } else {
        fill_placeholder(state, state->state[0] ? state->state : ".");
        return;
    }

    if (!pixels) {
        if (blob_pixels) {
            free(blob_pixels);
            blob_pixels = NULL;
        }
        fill_placeholder(state, "!");
        return;
    }

    state->frame_loaded = 1;
    state->frame_width = src_w;
    state->frame_height = src_h;

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

    if (cache_bytes > 0) free(pixels);
    else stbi_image_free(pixels);
}

static void write_grid(const char *root, const WebcamState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int y, x;
    path_join(path, sizeof(path), root, "session/webcam_preview.grid.pdl");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "GRID id=webcam_ascii rows=%d cols=%d cell_w=1 cell_h=1 source_ref=%s\n", PREVIEW_H, PREVIEW_W, state->frame_rel);
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            fprintf(f, "CELL row=%d col=%d ch_hex=%02X fg=%s bg=%s\n",
                y + 1, x + 1, (unsigned char)(state->lines[y][x] ? state->lines[y][x] : ' '),
                state->cell_fg[y][x], state->cell_bg[y][x]);
        }
    }
    fclose(f);
}

static void write_body(const char *root, const WebcamState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    (void)root;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "<text label=\"| |  WRAITH WEB CAM | live capture proof                                              |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<text label=\"| |  DEVICE:%s                                                             |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->device);
    fprintf(f, "<text label=\"| |  PROFILE:%s                                                            |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->capture_profile);
    fprintf(f, "<text label=\"| |  STATE:%s                                                                      |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->state);
    fprintf(f, "<text label=\"| |  DETAIL:%s\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->detail);
    fprintf(f, "<text label=\"| |  [ASCII Color Preview]                                                               |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fprintf(f, "<grid source=\"projects/wraith-alpha/wraith-projects/web-cam/session/webcam_preview.grid.pdl\" prefix=\"| |  \" suffix=\"                                                           |\" />\n");
    fprintf(f, "<text label=\"| |  %dx%d frame=%ld                                                              |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n", state->frame_width, state->frame_height, state->frame_epoch);
    fprintf(f, "<text label=\"| |  \" /><button compact=\"true\" label=\"Start\" onClick=\"PROJECT_ACTION:WEBCAM_START\" /><text label=\" \" /><button compact=\"true\" label=\"Stop\" onClick=\"PROJECT_ACTION:WEBCAM_STOP\" /><text label=\" \" /><button compact=\"true\" label=\"Fast\" onClick=\"PROJECT_ACTION:WEBCAM_FAST\" /><text label=\" \" /><button compact=\"true\" label=\"Debug\" onClick=\"PROJECT_ACTION:WEBCAM_DEBUG\" /><text label=\" \" /><button compact=\"true\" label=\"Refresh\" onClick=\"PROJECT_ACTION:WEBCAM_REFRESH\" /><text label=\"     |\" /><br visibility=\"${desktop_active_window_body_visible}\" />\n");
    fclose(f);
}

static void write_state(const char *root, const WebcamState *state) {
    char payload[4096];
    snprintf(payload, sizeof(payload),
        "project_id=wraith-alpha/wraith-projects/web-cam\n"
        "device=%s\n"
        "capture_profile=%s\n"
        "state=%s\n"
        "detail=%s\n"
        "frame_width=%d\n"
        "frame_height=%d\n"
        "frame_loaded=%d\n"
        "history_cursor=%ld\n",
        state->device,
        state->capture_profile,
        state->state,
        state->detail,
        state->frame_width,
        state->frame_height,
        state->frame_loaded,
        state->history_cursor);
    tpmos_share_kvp_write_text(root, "session/state.txt", payload);
}

static void write_scene(const char *root, const WebcamState *state) {
    char path[MAX_PATH_LEN];
    char frame_ref[MAX_PATH_LEN];
    FILE *f;
    int y, x;
    int nav_base = read_chrome_content_start(6);
    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    build_scene_ref(frame_ref, sizeof(frame_ref), root, state->frame_rel);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "OBJECT tag=panel id=webcam_controls role=chtmgl_panel x=3 y=6 w=24 h=12 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Webcam_Controls\n", frame_ref);
    fprintf(f, "OBJECT tag=text id=webcam_header role=chtmgl_header x=4 y=6 w=22 h=1 z=28 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=WEB_CAM\n", frame_ref);
    fprintf(f, "OBJECT tag=text id=webcam_state role=webcam_state x=5 y=8 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=STATE:%s\n", frame_ref, state->state);
    fprintf(f, "OBJECT tag=text id=webcam_meta role=webcam_meta x=5 y=9 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=%dx%d\n", frame_ref, state->frame_width, state->frame_height);
    fprintf(f, "OBJECT tag=text id=webcam_profile role=webcam_profile x=5 y=10 w=20 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=PROFILE:%s\n", frame_ref, state->capture_profile);
    fprintf(f, "OBJECT tag=control id=webcam_start role=chtmgl_button x=5 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:WEBCAM_START label=Start\n", nav_base + 0, frame_ref);
    fprintf(f, "OBJECT tag=control id=webcam_stop role=chtmgl_button x=14 y=12 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:WEBCAM_STOP label=Stop\n", nav_base + 1, frame_ref);
    fprintf(f, "OBJECT tag=control id=webcam_fast role=chtmgl_button x=5 y=14 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:WEBCAM_FAST label=Fast\n", nav_base + 2, frame_ref);
    fprintf(f, "OBJECT tag=control id=webcam_debug role=chtmgl_button x=14 y=14 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#FFD166 action=PROJECT_ACTION:WEBCAM_DEBUG label=Debug\n", nav_base + 3, frame_ref);
    fprintf(f, "OBJECT tag=control id=webcam_refresh role=chtmgl_button x=5 y=16 w=17 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#FFD166 action=PROJECT_ACTION:WEBCAM_REFRESH label=Refresh\n", nav_base + 4, frame_ref);
    fprintf(f, "OBJECT tag=panel id=webcam_visual_panel role=chtmgl_panel x=31 y=6 w=52 h=18 z=24 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Webcam_Frame\n", frame_ref);
    fprintf(f, "OBJECT tag=text id=webcam_detail role=webcam_detail x=33 y=8 w=48 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=DEVICE:%s\n", frame_ref, state->device);
    fprintf(f, "OBJECT tag=img id=webcam_frame role=image_asset x=62 y=9 w=18 h=10 z=33 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=WEBCAM_FRAME\n", frame_ref);
    fprintf(f, "OBJECT tag=panel id=webcam_preview_panel role=webcam_ascii_preview x=33 y=9 w=28 h=13 z=26 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=ASCII_PREVIEW\n", frame_ref);
    for (y = 0; y < PREVIEW_H; y++) {
        for (x = 0; x < PREVIEW_W; x++) {
            char glyph[2];
            glyph[0] = state->lines[y][x] ? state->lines[y][x] : ' ';
            glyph[1] = '\0';
            fprintf(f, "OBJECT tag=text id=webcam_preview_%02d_%02d role=webcam_preview_cell x=%d y=%d w=1 h=1 z=30 nav=0 source_ref=%s fg=%s bg=%s border=%s action=- label=%s\n",
                y + 1, x + 1, 35 + x, 10 + y, frame_ref,
                state->cell_fg[y][x], state->cell_bg[y][x], state->cell_bg[y][x], glyph);
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root = ".";
    WebcamState state;
    default_state(&state);
    if (argc > 1 && argv[1][0]) root = argv[1];
    load_previous_state(root, &state);
    path_join(state.frame_abs, sizeof(state.frame_abs), root, state.frame_rel);
    if (!(argc > 2 && strcmp(argv[2], "marker_tick") == 0)) {
        handle_command(root, &state);
    }
    load_status(root, &state);
    load_frame_and_project(&state);
    write_state(root, &state);
    write_grid(root, &state);
    write_body(root, &state);
    write_scene(root, &state);
    return 0;
}
