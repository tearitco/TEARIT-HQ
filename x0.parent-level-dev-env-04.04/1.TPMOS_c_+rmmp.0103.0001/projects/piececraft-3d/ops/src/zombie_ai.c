#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char project_id[MAX_LINE] = "piececraft-3d";

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    char *env_root = getenv("PRISC_PROJECT_ROOT");
    if (env_root) strncpy(project_root, env_root, MAX_PATH-1);
    else {
        FILE *kvp = fopen("pieces/locations/location_kvp", "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                if (strncmp(line, "project_root=", 13) == 0) {
                    strncpy(project_root, trim_str(line + 13), MAX_PATH - 1);
                    break;
                }
            }
            fclose(kvp);
        }
    }
}

int get_state_int(const char* piece_id, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, project_id, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[MAX_LINE];
    int val = -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), key) == 0) {
                val = atoi(trim_str(eq + 1));
                break;
            }
        }
    }
    fclose(f);
    return val;
}

void set_state_int(const char* piece_id, const char* key, int val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, project_id, piece_id);
    char lines[100][MAX_LINE];
    int lc = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
            char *eq = strchr(lines[lc], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[lc]), key) == 0) {
                    snprintf(lines[lc], sizeof(lines[0]), "%s=%d\n", key, val);
                    found = 1;
                } else *eq = '=';
            }
            lc++;
        }
        fclose(f);
    }
    if (!found && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=%d\n", key, val);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < lc; i++) fputs(lines[i], f);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    const char *zombie_id = argv[1];
    const char *target_id = argv[2];
    resolve_paths();

    int zx = get_state_int(zombie_id, "pos_x");
    int zy = get_state_int(zombie_id, "pos_y");
    int tx = get_state_int(target_id, "pos_x");
    int ty = get_state_int(target_id, "pos_y");

    if (zx == -1 || tx == -1) return 1;

    int dx = (tx > zx) ? 1 : (tx < zx ? -1 : 0);
    int dy = (ty > zy) ? 1 : (ty < zy ? -1 : 0);

    // Simple pathfinding: move in one axis at a time
    int nx = zx, ny = zy;
    if (dx != 0) nx += dx;
    else if (dy != 0) ny += dy;

    // Boundary check
    if (nx < 0) nx = 0; if (nx >= 20) nx = 19;
    if (ny < 0) ny = 0; if (ny >= 10) ny = 9;

    set_state_int(zombie_id, "pos_x", nx);
    set_state_int(zombie_id, "pos_y", ny);

    // Pulse
    char pulse_path[MAX_PATH];
    snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *pf = fopen(pulse_path, "a");
    if (pf) { fprintf(pf, "Z\n"); fclose(pf); }

    return 0;
}
