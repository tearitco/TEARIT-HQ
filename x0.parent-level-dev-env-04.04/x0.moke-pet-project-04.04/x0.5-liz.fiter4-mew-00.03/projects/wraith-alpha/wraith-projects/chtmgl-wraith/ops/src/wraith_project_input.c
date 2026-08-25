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
    int loaded;
    int width;
    int height;
    int channels;
    int audio_loaded;
    char asset_rel[256];
    char asset_abs[MAX_PATH_LEN];
    char audio_rel[256];
    char audio_abs[MAX_PATH_LEN];
    char audio_state[32];
    char last_response[256];
    char lines[PREVIEW_H][PREVIEW_W + 1];
} ImageProbeState;

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

    if (!out || out_sz == 0) {
        return;
    }
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

static void fill_placeholder(ImageProbeState *state, const char *msg) {
    int y;
    for (y = 0; y < PREVIEW_H; y++) {
        memset(state->lines[y], '.', PREVIEW_W);
        state->lines[y][PREVIEW_W] = '\0';
    }
    if (msg && msg[0]) {
        snprintf(state->last_response, sizeof(state->last_response), "%s", msg);
    }
}

static void default_state(ImageProbeState *state) {
    memset(state, 0, sizeof(*state));
    snprintf(state->asset_rel, sizeof(state->asset_rel), "../../../../../#.media-library/missingno.png");
    snprintf(state->audio_rel, sizeof(state->audio_rel), "../../../../../#.media-library/gowon.mp3");
    snprintf(state->audio_state, sizeof(state->audio_state), "stopped");
    snprintf(state->last_response, sizeof(state->last_response), "CHTMGL Wraith image probe ready.");
    fill_placeholder(state, state->last_response);
}

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static int load_and_project_image(ImageProbeState *state) {
    int x, y;
    int src_w, src_h, src_channels;
    unsigned char *pixels;
    static const char *ramp = " .:-=+*#%@";
    const int ramp_len = 10;

    pixels = stbi_load(state->asset_abs, &src_w, &src_h, &src_channels, 4);
    if (!pixels) {
        fill_placeholder(state, "Image load failed.");
        return 0;
    }

    state->loaded = 1;
    state->width = src_w;
    state->height = src_h;
    state->channels = 4;

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
                continue;
            }

            lum = (r * 30 + g * 59 + b * 11) / 100;
            bucket = (lum * (ramp_len - 1)) / 255;
            if (bucket < 0) bucket = 0;
            if (bucket >= ramp_len) bucket = ramp_len - 1;
            state->lines[y][x] = ramp[bucket];
        }
        state->lines[y][PREVIEW_W] = '\0';
    }

    snprintf(state->last_response, sizeof(state->last_response),
        "IMG loaded %dx%d from %s", src_w, src_h, state->asset_rel);
    stbi_image_free(pixels);
    return 1;
}

static void load_audio_probe(ImageProbeState *state) {
    struct stat st;
    if (stat(state->audio_abs, &st) == 0 && S_ISREG(st.st_mode)) {
        state->audio_loaded = 1;
        if (!state->audio_state[0]) {
            snprintf(state->audio_state, sizeof(state->audio_state), "stopped");
        }
    } else {
        snprintf(state->audio_state, sizeof(state->audio_state), "missing");
    }
}

static void load_previous_state(const char *root, ImageProbeState *state) {
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
        if (strncmp(line, "audio_state=", 12) == 0 && line[12]) {
            snprintf(state->audio_state, sizeof(state->audio_state), "%s", line + 12);
        } else if (strncmp(line, "last_response=", 14) == 0 && line[14]) {
            snprintf(state->last_response, sizeof(state->last_response), "%s", line + 14);
        }
    }

    fclose(f);
}

static int read_latest_command(const char *root, char *out, size_t out_sz) {
    char path[MAX_PATH_LEN];
    char line[512];
    FILE *f;
    int found = 0;

    if (!out || out_sz == 0) {
        return 0;
    }
    out[0] = '\0';

    path_join(path, sizeof(path), root, "session/history.txt");
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
    return found;
}

static int run_audio_op(const char *root, const char *arg) {
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

static void handle_command(const char *root, ImageProbeState *state) {
    char command[128];

    if (!read_latest_command(root, command, sizeof(command))) {
        return;
    }

    if (strcmp(command, "AUDIO_PLAY") == 0) {
        if (!state->audio_loaded) {
            snprintf(state->audio_state, sizeof(state->audio_state), "missing");
            snprintf(state->last_response, sizeof(state->last_response), "Audio missing: %s", state->audio_rel);
            return;
        }
        if (run_audio_op(root, state->audio_abs) == 0) {
            snprintf(state->audio_state, sizeof(state->audio_state), "playing");
            snprintf(state->last_response, sizeof(state->last_response), "Audio playing: %s", state->audio_rel);
        } else {
            snprintf(state->audio_state, sizeof(state->audio_state), "error");
            snprintf(state->last_response, sizeof(state->last_response), "Audio play failed");
        }
        return;
    }

    if (strcmp(command, "AUDIO_STOP") == 0) {
        if (run_audio_op(root, "--stop") == 0) {
            snprintf(state->audio_state, sizeof(state->audio_state), state->audio_loaded ? "stopped" : "missing");
            snprintf(state->last_response, sizeof(state->last_response), "Audio stopped");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Audio stop failed");
        }
        return;
    }

    if (strcmp(command, "AUDIO_PAUSE") == 0) {
        if (run_audio_op(root, "--pause") == 0) {
            snprintf(state->audio_state, sizeof(state->audio_state), "paused");
            snprintf(state->last_response, sizeof(state->last_response), "Audio paused");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Audio pause failed");
        }
        return;
    }

    if (strcmp(command, "AUDIO_RESUME") == 0) {
        if (run_audio_op(root, "--resume") == 0) {
            snprintf(state->audio_state, sizeof(state->audio_state), "playing");
            snprintf(state->last_response, sizeof(state->last_response), "Audio resumed");
        } else {
            snprintf(state->last_response, sizeof(state->last_response), "Audio resume failed");
        }
        return;
    }
}

static void write_state(const char *root, const ImageProbeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/chtmgl-wraith\n");
    fprintf(f, "reference_project=projects/chtmgl-alpha\n");
    fprintf(f, "game_map=chtmgl_preview_surface\n");
    fprintf(f, "is_map_control=0\n");
    fprintf(f, "widget_probe=header,panel,button,checkbox,slider,img,media,menu,surface\n");
    fprintf(f, "image_asset=%s\n", state->asset_rel);
    fprintf(f, "image_loaded=%d\n", state->loaded);
    fprintf(f, "image_width=%d\n", state->width);
    fprintf(f, "image_height=%d\n", state->height);
    fprintf(f, "ascii_preview=%dx%d\n", PREVIEW_W, PREVIEW_H);
    fprintf(f, "audio_asset=%s\n", state->audio_rel);
    fprintf(f, "audio_loaded=%d\n", state->audio_loaded);
    fprintf(f, "audio_state=%s\n", state->audio_state);
    fprintf(f, "last_response=%s\n", state->last_response);
    fclose(f);
}

static void write_body(const char *root, const ImageProbeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int y;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "CHTMGL WRAITH | image-first media probe\n");
    fprintf(f, "[HEADER] CHTMGL Image Tag Probe\n");
    fprintf(f, "IMG:id=missingno src=%s state=%s dims=%dx%d preview=%dx%d\n",
        state->asset_rel,
        state->loaded ? "loaded" : "missing",
        state->width,
        state->height,
        PREVIEW_W,
        PREVIEW_H);
    fprintf(f, "[ASCII Preview]\n");
    for (y = 0; y < PREVIEW_H; y++) {
        fprintf(f, "%s\n", state->lines[y]);
    }
    fprintf(f, "AUDIO:id=gowon src=%s state=%s controls=play,pause,resume,stop\n",
        state->audio_rel,
        state->audio_state);
    fprintf(f, "Audit: shared decode drives both body preview and GL scene text rows\n");
    fprintf(f, "Future: ANSI/color projection can reuse the same decoded RGB path\n");
    fprintf(f, "Last: %s\n", state->last_response);
    fclose(f);
}

static void write_scene(const char *root, const ImageProbeState *state) {
    char path[MAX_PATH_LEN];
    char asset_scene_ref[MAX_PATH_LEN];
    char audio_scene_ref[MAX_PATH_LEN];
    FILE *f;
    int y;
    int nav_base = read_chrome_content_start(6);

    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    build_scene_ref(asset_scene_ref, sizeof(asset_scene_ref), root, state->asset_rel);
    build_scene_ref(audio_scene_ref, sizeof(audio_scene_ref), root, state->audio_rel);
    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "# Project-owned Wraith scene records.\n");
    fprintf(f, "# First media slice: semantic image row + coarse ASCII preview rows.\n");
    fprintf(f, "OBJECT tag=panel id=controls_panel role=chtmgl_panel x=3 y=6 w=24 h=9 z=24 nav=0 source=reference:chtmgl-alpha fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Controls\n");
    fprintf(f, "OBJECT tag=text id=header_label role=chtmgl_header x=4 y=6 w=22 h=1 z=28 nav=0 source=reference:chtmgl-alpha fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=CHTMGL_UI\n");
    fprintf(f, "OBJECT tag=control id=btn_test role=chtmgl_button x=5 y=8 w=16 h=1 z=30 nav=%d source=reference:chtmgl-alpha fg=#E8F1F2 bg=#22384A border=#FFD166 action=EVENT:test_click label=Test_Button\n", nav_base + 0);
    fprintf(f, "OBJECT tag=control id=chk_feature role=chtmgl_checkbox x=5 y=10 w=18 h=1 z=30 nav=%d source=reference:chtmgl-alpha fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=EVENT:toggle_feature label=Enable_Feature\n", nav_base + 1);
    fprintf(f, "OBJECT tag=control id=slider_volume role=chtmgl_slider x=5 y=12 w=18 h=1 z=30 nav=%d source=reference:chtmgl-alpha fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=EVENT:slider_volume label=Volume_50\n", nav_base + 2);

    fprintf(f, "OBJECT tag=panel id=visual_panel role=chtmgl_panel x=31 y=6 w=52 h=18 z=24 nav=0 source=reference:chtmgl-alpha fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=Visuals_Media\n");
    fprintf(f, "OBJECT tag=text id=image_state role=chtmgl_image_state x=33 y=8 w=48 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=IMG:missingno.png_%s_%dx%d\n",
        asset_scene_ref,
        state->loaded ? "loaded" : "missing",
        state->width,
        state->height);
    fprintf(f, "OBJECT tag=img id=image_asset role=image_asset x=62 y=9 w=18 h=10 z=33 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=IMG_TEXTURE:missingno.png\n",
        asset_scene_ref);
    fprintf(f, "OBJECT tag=panel id=image_preview_panel role=chtmgl_image_preview x=33 y=9 w=28 h=13 z=26 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#FFD166 action=- label=ASCII_PREVIEW\n",
        asset_scene_ref);

    for (y = 0; y < PREVIEW_H; y++) {
        fprintf(f, "OBJECT tag=text id=image_preview_%02d role=image_preview_row x=35 y=%d w=%d h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#0B1118 border=#0B1118 action=- label=%s\n",
            y + 1, 10 + y, PREVIEW_W, asset_scene_ref, state->lines[y]);
    }

    fprintf(f, "OBJECT tag=text id=audio_state role=chtmgl_audio_state x=62 y=20 w=18 h=1 z=30 nav=0 source_ref=%s fg=#E8F1F2 bg=#162534 border=#7EDFF2 action=- label=AUDIO:gowon_%s\n",
        audio_scene_ref, state->audio_state);
    fprintf(f, "OBJECT tag=control id=audio_play role=chtmgl_button x=62 y=21 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:AUDIO_PLAY label=Play\n",
        nav_base + 5, audio_scene_ref);
    fprintf(f, "OBJECT tag=control id=audio_pause role=chtmgl_button x=71 y=21 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:AUDIO_PAUSE label=Pause\n",
        nav_base + 6, audio_scene_ref);
    fprintf(f, "OBJECT tag=control id=audio_resume role=chtmgl_button x=62 y=22 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:AUDIO_RESUME label=Resume\n",
        nav_base + 7, audio_scene_ref);
    fprintf(f, "OBJECT tag=control id=audio_stop role=chtmgl_button x=71 y=22 w=8 h=1 z=30 nav=%d source_ref=%s fg=#E8F1F2 bg=#22384A border=#7EDFF2 action=PROJECT_ACTION:AUDIO_STOP label=Stop\n",
        nav_base + 8, audio_scene_ref);

    fprintf(f, "OBJECT tag=control id=menu_options role=chtmgl_menu x=64 y=6 w=16 h=1 z=30 nav=%d source=reference:chtmgl-alpha fg=#E8F1F2 bg=#2A3E52 border=#FFD166 action=EVENT:menu_options label=Options\n", nav_base + 3);
    fprintf(f, "OBJECT tag=control id=control_surface role=window_toolbar_item x=54 y=23 w=24 h=1 z=32 nav=%d source=semantic:window_toolbar fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT target_surface=game_map label=Control_Surface\n", nav_base + 4);
    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=62 y=9 w=18 h=10 z=20 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=chtmgl_preview role=widget_surface_probe x=62 y=22 w=18 h=2 z=24 source=projects/wraith-alpha/wraith-projects/chtmgl-wraith/layouts/chtmgl-wraith.chtpm fg=#7EDFF2 bg=#0B1118 border=#FFD166 action=- label=CHTMGL_SURFACE:source=layouts/chtmgl-wraith.chtpm;reference=chtmgl-alpha;canvas=game_map\n");
    fclose(f);
}

int main(int argc, char **argv) {
    const char *root = ".";
    ImageProbeState state;

    default_state(&state);
    if (argc > 1 && argv[1][0]) {
        root = argv[1];
    }
    path_join(state.asset_abs, sizeof(state.asset_abs), root, state.asset_rel);
    path_join(state.audio_abs, sizeof(state.audio_abs), root, state.audio_rel);
    load_previous_state(root, &state);
    load_and_project_image(&state);
    load_audio_probe(&state);
    handle_command(root, &state);
    write_state(root, &state);
    write_body(root, &state);
    write_scene(root, &state);
    return 0;
}
