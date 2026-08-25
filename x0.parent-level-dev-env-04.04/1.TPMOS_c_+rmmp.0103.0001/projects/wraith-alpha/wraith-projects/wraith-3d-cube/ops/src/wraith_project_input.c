#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 1024
#define MAX_LINE 512

typedef struct {
    int is_map_control;
    int rot_x;
    int rot_y;
    int rot_z;
    int camera_mode;
    double cam_x;
    double cam_y;
    double cam_z;
    double cam_pitch;
    double cam_yaw;
    double cam_roll;
    char last_response[256];
} CubeState;

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

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void default_state(CubeState *state) {
    memset(state, 0, sizeof(*state));
    state->rot_x = 15;
    state->rot_y = 35;
    state->rot_z = 0;
    state->camera_mode = 4;
    state->cam_pitch = 15.0;
    snprintf(state->last_response, sizeof(state->last_response), "Wraith cube probe ready.");
}

static int read_state(const char *root, CubeState *state) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE];
    FILE *f;
    default_state(state);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "is_map_control=", 15) == 0) state->is_map_control = atoi(line + 15);
        else if (strncmp(line, "cube_rotation_x=", 16) == 0) state->rot_x = atoi(line + 16);
        else if (strncmp(line, "cube_rotation_y=", 16) == 0) state->rot_y = atoi(line + 16);
        else if (strncmp(line, "cube_rotation_z=", 16) == 0) state->rot_z = atoi(line + 16);
        else if (strncmp(line, "camera_mode=", 12) == 0) state->camera_mode = atoi(line + 12);
        else if (strncmp(line, "cam_x=", 6) == 0) state->cam_x = atof(line + 6);
        else if (strncmp(line, "cam_y=", 6) == 0) state->cam_y = atof(line + 6);
        else if (strncmp(line, "cam_z=", 6) == 0) state->cam_z = atof(line + 6);
        else if (strncmp(line, "cam_pitch=", 10) == 0) state->cam_pitch = atof(line + 10);
        else if (strncmp(line, "cam_yaw=", 8) == 0) state->cam_yaw = atof(line + 8);
        else if (strncmp(line, "cam_roll=", 9) == 0) state->cam_roll = atof(line + 9);
        else if (strncmp(line, "last_response=", 14) == 0) {
            snprintf(state->last_response, sizeof(state->last_response), "%s", line + 14);
        }
    }
    fclose(f);
    return 1;
}

static long read_cursor(const char *root) {
    char path[MAX_PATH_LEN];
    long cursor = 0;
    FILE *f;
    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fscanf(f, "%ld", &cursor) != 1) cursor = 0;
    fclose(f);
    return cursor;
}

static void write_cursor(const char *root, long cursor) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", cursor);
    fclose(f);
}

static int parse_key_line(const char *line, int *key) {
    const char *p = strstr(line, "KEY_PRESSED:");
    if (!p) return 0;
    p += 12;
    while (*p && isspace((unsigned char)*p)) p++;
    *key = atoi(p);
    return 1;
}

static void wrap_degrees(int *value) {
    while (*value < 0) *value += 360;
    while (*value >= 360) *value -= 360;
}

static void apply_key(CubeState *state, int key) {
    const int rot_step = 5;
    const double move_step = 0.25;

    if (!state->is_map_control) {
        return;
    }

    switch (key) {
        case 'a':
        case 'A':
            state->rot_y -= rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:y,-%d", rot_step);
            break;
        case 'd':
        case 'D':
            state->rot_y += rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:y,+%d", rot_step);
            break;
        case 'w':
        case 'W':
            state->rot_x -= rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:x,-%d", rot_step);
            break;
        case 's':
        case 'S':
            state->rot_x += rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:x,+%d", rot_step);
            break;
        case 'q':
        case 'Q':
            state->rot_z -= rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:z,-%d", rot_step);
            break;
        case 'e':
        case 'E':
            state->rot_z += rot_step;
            snprintf(state->last_response, sizeof(state->last_response), "ROTATE_CUBE:z,+%d", rot_step);
            break;
        case 'i':
        case 'I':
            state->cam_z += move_step;
            snprintf(state->last_response, sizeof(state->last_response), "CAMERA_MOVE:0,0,+%.2f", move_step);
            break;
        case 'k':
        case 'K':
            state->cam_z -= move_step;
            snprintf(state->last_response, sizeof(state->last_response), "CAMERA_MOVE:0,0,-%.2f", move_step);
            break;
        case 'j':
        case 'J':
            state->cam_x -= move_step;
            snprintf(state->last_response, sizeof(state->last_response), "CAMERA_MOVE:-%.2f,0,0", move_step);
            break;
        case 'l':
        case 'L':
            state->cam_x += move_step;
            snprintf(state->last_response, sizeof(state->last_response), "CAMERA_MOVE:+%.2f,0,0", move_step);
            break;
        case 'm':
        case 'M':
            state->camera_mode = (state->camera_mode % 4) + 1;
            snprintf(state->last_response, sizeof(state->last_response), "CAMERA_MODE:%d", state->camera_mode);
            break;
        case 'r':
        case 'R':
            default_state(state);
            state->is_map_control = 1;
            snprintf(state->last_response, sizeof(state->last_response), "RESET_CUBE_CAMERA");
            break;
        default:
            snprintf(state->last_response, sizeof(state->last_response), "IGNORED_KEY:%d", key);
            break;
    }
    wrap_degrees(&state->rot_x);
    wrap_degrees(&state->rot_y);
    wrap_degrees(&state->rot_z);
}

static int consume_history(const char *root, CubeState *state) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE];
    long cursor = read_cursor(root);
    long pos;
    int changed = 0;
    FILE *f;
    path_join(path, sizeof(path), root, "session/history.txt");
    f = fopen(path, "r");
    if (!f) return 0;
    if (cursor > 0) fseek(f, cursor, SEEK_SET);
    while (fgets(line, sizeof(line), f)) {
        int key = 0;
        if (parse_key_line(line, &key)) {
            apply_key(state, key);
            changed = 1;
        }
    }
    pos = ftell(f);
    fclose(f);
    if (pos >= 0) write_cursor(root, pos);
    return changed;
}

static void write_state(const char *root, const CubeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/wraith-3d-cube\n");
    fprintf(f, "game_map=cube_probe_01\n");
    fprintf(f, "is_map_control=%d\n", state->is_map_control);
    fprintf(f, "cube_rotation_x=%d\n", state->rot_x);
    fprintf(f, "cube_rotation_y=%d\n", state->rot_y);
    fprintf(f, "cube_rotation_z=%d\n", state->rot_z);
    fprintf(f, "camera_mode=%d\n", state->camera_mode);
    fprintf(f, "cam_x=%.2f\n", state->cam_x);
    fprintf(f, "cam_y=%.2f\n", state->cam_y);
    fprintf(f, "cam_z=%.2f\n", state->cam_z);
    fprintf(f, "cam_pitch=%.2f\n", state->cam_pitch);
    fprintf(f, "cam_yaw=%.2f\n", state->cam_yaw);
    fprintf(f, "cam_roll=%.2f\n", state->cam_roll);
    fprintf(f, "last_response=%s\n", state->last_response);
    fclose(f);
}

static void write_piece_state(const char *root, const CubeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "pieces/cube_probe/state.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "piece_id=cube_probe\n");
    fprintf(f, "artifact=pieces/cube_probe/artifact.txt\n");
    fprintf(f, "role=zslice_piece\n");
    fprintf(f, "surface=${game_map}\n");
    fprintf(f, "source_standard=piececraft_zslice_piece\n");
    fprintf(f, "pos_x=0\npos_y=0\npos_z=0\n");
    fprintf(f, "rot_x=%d\nrot_y=%d\nrot_z=%d\n", state->rot_x, state->rot_y, state->rot_z);
    fprintf(f, "camera_mode=%d\n", state->camera_mode);
    fprintf(f, "cam_x=%.2f\ncam_y=%.2f\ncam_z=%.2f\n", state->cam_x, state->cam_y, state->cam_z);
    fprintf(f, "selected=1\n");
    fprintf(f, "rgb_top=36,201,74\nrgb_side=23,142,49\n");
    fclose(f);
}

static void write_body(const char *root, const CubeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    path_join(path, sizeof(path), root, "session/wraith_body.txt");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "WRAITH 3D CUBE\n");
    fprintf(f, "Surface: ${game_map} | Toolbar: Control_Map INTERACT | Mode: is_map_control\n");
    fprintf(f, "    +--------+\n");
    fprintf(f, "   /        /|\n");
    fprintf(f, "  +--------+ |\n");
    fprintf(f, "  |        | +\n");
    fprintf(f, "  |        |/\n");
    fprintf(f, "  +--------+\n");
    fprintf(f, "Audit: piece=cube_probe artifact=pieces/cube_probe/artifact.txt rot=x%d,y%d,z%d\n",
        state->rot_x, state->rot_y, state->rot_z);
    fprintf(f, "Camera: mode=%d pos=%.2f,%.2f,%.2f pitch=%.2f yaw=%.2f roll=%.2f\n",
        state->camera_mode, state->cam_x, state->cam_y, state->cam_z,
        state->cam_pitch, state->cam_yaw, state->cam_roll);
    fprintf(f, "Last: %s\n", state->last_response);
    fprintf(f, "Keys: WASD rotate XY | QE roll | IJKL move camera | M mode | R reset | ESC exit\n");
    fclose(f);
}

static void write_scene(const char *root, const CubeState *state) {
    char path[MAX_PATH_LEN];
    FILE *f;
    int nav_base = read_chrome_content_start(6);
    (void)root;
    path_join(path, sizeof(path), root, "session/scene.objects.pdl");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Project-owned Wraith scene records.\n");
    fprintf(f, "# Loaded by wraith-alpha manager; converted by wraith_rgb_daemon later.\n");
    fprintf(f, "# Format is intentionally flat and auditable.\n");
    fprintf(f, "OBJECT tag=control id=control_map role=window_toolbar_item x=35 y=5 w=24 h=1 z=26 nav=%d source=semantic:window_toolbar fg=#E8F1F2 bg=#122333 border=#7EDFF2 action=INTERACT target_surface=game_map label=Control_Map\n", nav_base + 0);
    fprintf(f, "OBJECT tag=surface id=game_map role=game_map x=35 y=6 w=32 h=14 z=16 nav=0 source=semantic:game_map_surface fg=#E8F1F2 bg=#0B1118 border=#7EDFF2 action=- label=\n");
    fprintf(f, "OBJECT tag=model id=cube_probe role=zslice_piece x=35 y=6 w=32 h=14 z=23 source=projects/wraith-alpha/wraith-projects/wraith-3d-cube/pieces/cube_probe/artifact.txt fg=#24C94A bg=#0B1118 border=#FFD166 action=- label=PIECE_MODEL:source=pieces/cube_probe/artifact.txt;rot=%d,%d,%d;camera=%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f;selected=true\n",
        state->rot_x, state->rot_y, state->rot_z,
        state->camera_mode, state->cam_x, state->cam_y, state->cam_z,
        state->cam_pitch, state->cam_yaw, state->cam_roll);
    fclose(f);
}

int main(int argc, char **argv) {
    CubeState state;
    const char *root = ".";
    int changed;
    if (argc > 1 && argv[1][0]) {
        root = argv[1];
    }
    read_state(root, &state);
    changed = consume_history(root, &state);
    if (!changed) {
        return 0;
    }
    write_state(root, &state);
    write_piece_state(root, &state);
    write_body(root, &state);
    write_scene(root, &state);
    return 0;
}
