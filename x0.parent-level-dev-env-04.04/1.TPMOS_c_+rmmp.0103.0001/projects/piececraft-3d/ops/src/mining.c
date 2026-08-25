#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAP_ROWS 10
#define MAP_COLS 20

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

void update_inventory(const char* piece_id, char tile) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/inventory.txt", project_root, project_id, piece_id);
    char item_name[32];
    if (tile == 'T') strcpy(item_name, "wood");
    else if (tile == 'R') strcpy(item_name, "stone");
    else sprintf(item_name, "block_%c", tile);

    char lines[100][MAX_LINE];
    int lc = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
            char *eq = strchr(lines[lc], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[lc]), item_name) == 0) {
                    int count = atoi(trim_str(eq + 1));
                    snprintf(lines[lc], sizeof(lines[0]), "%s=%d\n", item_name, count + 1);
                    found = 1;
                } else *eq = '=';
            }
            lc++;
        }
        fclose(f);
    }
    if (!found && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=1\n", item_name);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < lc; i++) fputs(lines[i], f);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *active_piece = argv[1];
    resolve_paths();

    int sx = get_state_int("xlector", "pos_x");
    int sy = get_state_int("xlector", "pos_y");
    if (sx == -1 || sy == -1) return 1;

    char map_path[MAX_PATH];
    snprintf(map_path, sizeof(map_path), "%s/projects/%s/maps/map_01_z0.txt", project_root, project_id);
    
    char map[MAP_ROWS][MAP_COLS + 5];
    FILE *mf = fopen(map_path, "r");
    if (!mf) return 1;
    int rc = 0;
    while (rc < MAP_ROWS && fgets(map[rc], sizeof(map[rc]), mf)) rc++;
    fclose(mf);

    char tile = map[sy][sx];
    if (tile != '.' && tile != '#' && tile != ' ' && tile != '\n' && tile != '\r') {
        map[sy][sx] = '.';
        mf = fopen(map_path, "w");
        if (mf) {
            for (int i = 0; i < rc; i++) fputs(map[i], mf);
            fclose(mf);
        }
        update_inventory(active_piece, tile);
        printf("Mined %c at (%d,%d)\n", tile, sx, sy);
        
        // Pulse
        char pulse_path[MAX_PATH];
        snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
        FILE *pf = fopen(pulse_path, "a");
        if (pf) { fprintf(pf, "M\n"); fclose(pf); }
    }

    return 0;
}
