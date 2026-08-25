/*
 * Op: collect_op
 * Usage: ./+x/collect_op.+x
 * 
 * Picks up items (like 'T' for treasure) at the selector's current position.
 * OPTIMIZED: Uses PRISC_PROJECT_ROOT and PRISC_PROJECT_ID environment variables
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 8192
#define MAX_LINE 1024
#define MAP_ROWS 10
#define MAP_COLS 20

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";
char active_map_file[MAX_LINE] = "map_01.txt";

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
    // PRIORITY 1: Environment variables from PAL module
    char *env_root = getenv("PRISC_PROJECT_ROOT");
    char *env_project = getenv("PRISC_PROJECT_ID");
    
    if (env_root && strlen(env_root) > 0) {
        strncpy(project_root, env_root, MAX_PATH-1);
    }
    if (env_project && strlen(env_project) > 0) {
        strncpy(current_project, env_project, sizeof(current_project)-1);
    }
    
    // PRIORITY 2: Fallback to location_kvp file
    if (strlen(project_root) == 0 || strcmp(project_root, ".") == 0) {
        FILE *kvp = fopen("pieces/locations/location_kvp", "r");
        if (!kvp) kvp = fopen("../../../locations/location_kvp", "r");
        if (!kvp) kvp = fopen("../../../../locations/location_kvp", "r");
        
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH-1);
                }
            }
            fclose(kvp);
        }
    }
    
    // PRIORITY 3: Fallback to manager state for project_id
    if (strlen(current_project) == 0) {
        char *p_mgr_path = NULL;
        if (asprintf(&p_mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(p_mgr_path, "r");
            if (f) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), f)) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        char *k = trim_str(line);
                        char *v = trim_str(eq + 1);
                        if (strcmp(k, "project_id") == 0) {
                            strncpy(current_project, v, sizeof(current_project)-1);
                        }
                    }
                }
                fclose(f);
            }
            free(p_mgr_path);
        }
    }
    
    if (strlen(current_project) == 0) {
        strncpy(current_project, "template", sizeof(current_project)-1);
    }
}

void get_piece_state(const char* piece_id, const char* key, char* out_val) {
    strcpy(out_val, "");
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), key) == 0) {
                    strcpy(out_val, trim_str(eq + 1));
                    break;
                }
            }
        }
        fclose(f);
    }
    free(path);
}

void set_piece_state(const char* piece_id, const char* key, const char* val) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) return;
    char lines[100][MAX_LINE];
    int line_count = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[line_count], sizeof(lines[0]), f) && line_count < 99) {
            char *eq = strchr(lines[line_count], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[line_count]), key) == 0) {
                    snprintf(lines[line_count], sizeof(lines[0]), "%s=%s\n", key, val);
                    found = 1;
                } else *eq = '=';
            }
            line_count++;
        }
        fclose(f);
    }
    if (!found && line_count < 100) snprintf(lines[line_count++], sizeof(lines[0]), "%s=%s\n", key, val);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < line_count; i++) fputs(lines[i], f);
        fclose(f);
    }
    free(path);
}

void log_event(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] EventFire: %s | Source: collect_op\n", now, msg);
            fclose(f);
        }
        free(path);
    }
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(path);
    }
}

int main() {
    resolve_paths();
    char sx_s[16], sy_s[16];
    get_piece_state("selector", "pos_x", sx_s);
    get_piece_state("selector", "pos_y", sy_s);
    int sx = atoi(sx_s), sy = atoi(sy_s);

    char *map_path = NULL;
    if (asprintf(&map_path, "%s/projects/%s/maps/%s", project_root, current_project, active_map_file) == -1) return 1;
    
    char map[MAP_ROWS][MAP_COLS + 2];
    FILE *mf = fopen(map_path, "r");
    if (!mf) { free(map_path); return 1; }
    for (int i = 0; i < MAP_ROWS; i++) {
        if (!fgets(map[i], sizeof(map[i]), mf)) break;
    }
    fclose(mf);

    char tile = map[sy][sx];
    if (tile == 'T') {
        map[sy][sx] = '.';
        mf = fopen(map_path, "w");
        if (mf) {
            for (int i = 0; i < MAP_ROWS; i++) fputs(map[i], mf);
            fclose(mf);
        }
        
        char hap_s[16];
        get_piece_state("selector", "happiness", hap_s);
        int hap = atoi(hap_s);
        char new_hap[16];
        sprintf(new_hap, "%d", hap + 10);
        set_piece_state("selector", "happiness", new_hap);
        
        log_event("Treasure Collected!");
        printf("Collected Treasure at (%d,%d)\n", sx, sy);
    } else {
        log_event("Nothing to collect here.");
        printf("Nothing at (%d,%d)\n", sx, sy);
    }

    free(map_path);
    hit_frame_marker();
    return 0;
}
