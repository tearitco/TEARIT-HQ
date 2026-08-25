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

int consume_inventory(const char* piece_id, const char* item_name) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/inventory.txt", project_root, project_id, piece_id);
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
                    if (count > 0) {
                        snprintf(lines[lc], sizeof(lines[0]), "%s=%d\n", item_name, count - 1);
                        found = 1;
                    }
                } else *eq = '=';
            }
            lc++;
        }
        fclose(f);
    }
    if (found) {
        f = fopen(path, "w");
        if (f) {
            for (int i = 0; i < lc; i++) fputs(lines[i], f);
            fclose(f);
        }
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *active_piece = argv[1];
    const char *block_to_place = (argc >= 3) ? argv[2] : "stone";
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
    if (tile == '.') {
        if (consume_inventory(active_piece, block_to_place)) {
            char tile_char = 'R';
            if (strcmp(block_to_place, "wood") == 0) tile_char = 'T';
            else if (strcmp(block_to_place, "stone") == 0) tile_char = 'R';
            
            map[sy][sx] = tile_char;
            mf = fopen(map_path, "w");
            if (mf) {
                for (int i = 0; i < rc; i++) fputs(map[i], mf);
                fclose(mf);
            }
            printf("Placed %s at (%d,%d)\n", block_to_place, sx, sy);
            
            // Pulse
            char pulse_path[MAX_PATH];
            snprintf(pulse_path, sizeof(pulse_path), "%s/pieces/display/frame_changed.txt", project_root);
            FILE *pf = fopen(pulse_path, "a");
            if (pf) { fprintf(pf, "P\n"); fclose(pf); }
        } else {
            printf("No %s in inventory!\n", block_to_place);
        }
    }

    return 0;
}
