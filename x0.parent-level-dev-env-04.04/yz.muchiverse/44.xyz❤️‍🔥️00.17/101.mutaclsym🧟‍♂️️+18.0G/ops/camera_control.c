/* camera_control - one verb, one binary, no shared headers.
 * argv[1] = raw keycode (decimal). Handles all 3D camera movement and
 * rotation when render_mode==1 (3D GL view). This is a SEPARATE op from
 * move_player (which ONLY handles hero/cursor movement via arrow keys
 * and hero Z level via x/z).
 *
 * Camera keys are mode-dependent (see pov-cam.md for the full spec):
 *
 *   Mode 1 (first person): FIXED at hero eye level. Only look: q/e
 *   rotate yaw, r/t pitch up/down, f reset to default facing.
 *   No pan, no Z level - camera stays locked to hero position.
 *
 *   Mode 2 (third person): FIXED behind+above hero, looking down.
 *   Only look: q/e rotate yaw, r/t pitch up/down, f reset to default.
 *   No pan, no Z level - camera stays locked behind hero.
 *
 *   Mode 3 (free roam): all keys active - w/a/s/d pan,
 *   q/e rotate, r/t pitch, c/v Z level, f reset to bird's eye.
 *
 *   Mode 4 (bird's eye): w/a/s/d pan, c/v Z level, f center on hero.
 *   q/e/r/t are no-ops.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Self-filtering: returns 0 (no-op) for any key not in the camera set,
 * or when render_mode==0 (2D view). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
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

static double read_kv_double(const char *path, const char *key, double def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    double val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atof(eq + 1); break; }
    }
    fclose(f);
    return val;
}

#define YAW_STEP 10.0
#define PITCH_STEP 10.0
#define PAN_STEP 1.0

int main(int argc, char **argv) {
    if (argc < 2) return 0;
    int key = atoi(argv[1]);
    resolve_root();

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);

    int render_mode = read_kv_int(path, "render_mode", 0);
    if (render_mode != 1) return 0;

    int camera_mode = read_kv_int(path, "camera_mode", 1);
    double cam_pan_x = read_kv_double(path, "cam_pan_x", 0.0);
    double cam_pan_y = read_kv_double(path, "cam_pan_y", 0.0);
    double cam_pan_z = read_kv_double(path, "cam_pan_z", 0.0);
    double cam_yaw = read_kv_double(path, "cam_yaw", 0.0);
    double cam_pitch = read_kv_double(path, "cam_pitch", 6.0);
    int cam_z_level = read_kv_int(path, "cam_z_level", 0);

    int changed = 0;

    switch (camera_mode) {
    case 1: /* first person: FIXED at hero eye level, look only */
        if (key == 'q' || key == 'Q') { cam_yaw -= YAW_STEP; changed = 1; }
        else if (key == 'e' || key == 'E') { cam_yaw += YAW_STEP; changed = 1; }
        else if (key == 'r' || key == 'R') {
            cam_pitch += PITCH_STEP;
            if (cam_pitch > 89.0) cam_pitch = 89.0;
            changed = 1;
        }
        else if (key == 't' || key == 'T') {
            cam_pitch -= PITCH_STEP;
            if (cam_pitch < -89.0) cam_pitch = -89.0;
            changed = 1;
        }
        else if (key == 'f' || key == 'F') {
            cam_yaw = 180.0;
            cam_pitch = 6.0;
            changed = 1;
        }
        break;

    case 2: /* third person: FIXED behind+above hero, look only */
        if (key == 'q' || key == 'Q') { cam_yaw -= YAW_STEP; changed = 1; }
        else if (key == 'e' || key == 'E') { cam_yaw += YAW_STEP; changed = 1; }
        else if (key == 'r' || key == 'R') {
            cam_pitch += PITCH_STEP;
            if (cam_pitch > 89.0) cam_pitch = 89.0;
            changed = 1;
        }
        else if (key == 't' || key == 'T') {
            cam_pitch -= PITCH_STEP;
            if (cam_pitch < -89.0) cam_pitch = -89.0;
            changed = 1;
        }
        else if (key == 'f' || key == 'F') {
            cam_yaw = 180.0;
            cam_pitch = 6.0;
            changed = 1;
        }
        break;

    case 3: /* free roam camera: full control */
        if (key == 'w' || key == 'W') { cam_pan_z += PAN_STEP; changed = 1; }
        else if (key == 's' || key == 'S') { cam_pan_z -= PAN_STEP; changed = 1; }
        else if (key == 'a' || key == 'A') { cam_pan_x -= PAN_STEP; changed = 1; }
        else if (key == 'd' || key == 'D') { cam_pan_x += PAN_STEP; changed = 1; }
        else if (key == 'q' || key == 'Q') { cam_yaw -= YAW_STEP; changed = 1; }
        else if (key == 'e' || key == 'E') { cam_yaw += YAW_STEP; changed = 1; }
        else if (key == 'r' || key == 'R') {
            cam_pitch += PITCH_STEP;
            if (cam_pitch > 89.0) cam_pitch = 89.0;
            changed = 1;
        }
        else if (key == 't' || key == 'T') {
            cam_pitch -= PITCH_STEP;
            if (cam_pitch < -89.0) cam_pitch = -89.0;
            changed = 1;
        }
        else if (key == 'c' || key == 'C') { cam_z_level++; changed = 1; }
        else if (key == 'v' || key == 'V') { cam_z_level--; changed = 1; }
        else if (key == 'f' || key == 'F') {
            cam_pan_x = 0.0;
            cam_pan_z = 0.0;
            cam_yaw = 180.0;
            cam_pitch = -90.0;
            changed = 1;
        }
        break;

    case 4: /* bird's eye / board game view */
        if (key == 'w' || key == 'W') { cam_pan_y -= PAN_STEP; changed = 1; }
        else if (key == 's' || key == 'S') { cam_pan_y += PAN_STEP; changed = 1; }
        else if (key == 'a' || key == 'A') { cam_pan_x -= PAN_STEP; changed = 1; }
        else if (key == 'd' || key == 'D') { cam_pan_x += PAN_STEP; changed = 1; }
        else if (key == 'c' || key == 'C') { cam_z_level++; changed = 1; }
        else if (key == 'v' || key == 'V') { cam_z_level--; changed = 1; }
        else if (key == 'f' || key == 'F') {
            /* Center on hero: read hero pos, set pan to match */
            int hpx = read_kv_int(path, "pos_x", 0);
            int hpy = read_kv_int(path, "pos_y", 0);
            cam_pan_x = (double)hpx;
            cam_pan_y = (double)hpy;
            changed = 1;
        }
        break;

    default:
        return 0;
    }

    if (!changed) return 0;

    /* Read all lines for passthrough write-back, same pattern as
     * move_player.c. */
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char lines[32][MAX_LINE];
    int nlines = 0;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
    fclose(f);

    f = fopen(path, "w");
    if (!f) return 1;
    int panx_found = 0, pany_found = 0, panz_found = 0;
    int yaw_found = 0, pitch_found = 0, zlv_found = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "cam_pan_x") == 0) { fprintf(f, "cam_pan_x=%.2f\n", cam_pan_x); panx_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pan_y") == 0) { fprintf(f, "cam_pan_y=%.2f\n", cam_pan_y); pany_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pan_z") == 0) { fprintf(f, "cam_pan_z=%.2f\n", cam_pan_z); panz_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "cam_yaw") == 0) { fprintf(f, "cam_yaw=%.2f\n", cam_yaw); yaw_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pitch") == 0) { fprintf(f, "cam_pitch=%.2f\n", cam_pitch); pitch_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "cam_z_level") == 0) { fprintf(f, "cam_z_level=%d\n", cam_z_level); zlv_found = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!panx_found) fprintf(f, "cam_pan_x=%.2f\n", cam_pan_x);
    if (!pany_found) fprintf(f, "cam_pan_y=%.2f\n", cam_pan_y);
    if (!panz_found) fprintf(f, "cam_pan_z=%.2f\n", cam_pan_z);
    if (!yaw_found) fprintf(f, "cam_yaw=%.2f\n", cam_yaw);
    if (!pitch_found) fprintf(f, "cam_pitch=%.2f\n", cam_pitch);
    if (!zlv_found) fprintf(f, "cam_z_level=%d\n", cam_z_level);
    fclose(f);

    return 0;
}
