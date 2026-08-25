/*
 * Op: scan_op
 * Usage: ./+x/scan_op.+x
 * 
 * Scans the current selector tile and logs detailed info.
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
#include <dirent.h>

#define MAX_PATH 8192
#define MAX_LINE 1024

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
    // PRIORITY 1: Environment variables from PAL module (fastest, most accurate)
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
    if (strlen(current_project) == 0 || strcmp(current_project, "template") == 0) {
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
    
    // Final fallback — template is the system default
    if (strlen(current_project) == 0) {
        strncpy(current_project, "template", sizeof(current_project)-1);
    }
}

char get_tile_at(int x, int y) {
    char *map_path = NULL;
    if (asprintf(&map_path, "%s/projects/%s/maps/%s", project_root, current_project, active_map_file) == -1) return '#';
    FILE *mf = fopen(map_path, "r");
    if (!mf) { free(map_path); return '#'; }
    char line[MAX_LINE];
    int cur_y = 0;
    char tile = '#';
    while (fgets(line, sizeof(line), mf)) {
        if (cur_y == y) {
            if (x < (int)strlen(line)) tile = line[x];
            break;
        }
        cur_y++;
    }
    fclose(mf); free(map_path);
    return tile;
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

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] EventFire: %s | Source: scan_op\n", now, msg);
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
    get_piece_state("xlector", "pos_x", sx_s);
    get_piece_state("xlector", "pos_y", sy_s);
    int tx = atoi(sx_s);
    int ty = atoi(sy_s);
    int tz = 0;
    
    char desc[256] = "";
    char tile = get_tile_at(tx, ty);
    
    switch(tile) {
        case '#': strcpy(desc, "Wall (Impassable)"); break;
        case '.': strcpy(desc, "Floor (Empty)"); break;
        case 'R': strcpy(desc, "Rock (Solid)"); break;
        case 'T': strcpy(desc, "Treasure (Shiny!)"); break;
        default: sprintf(desc, "Terrain (%c)", tile); break;
    }
    
    // Check for entities
    char *pieces_dir = NULL;
    if (asprintf(&pieces_dir, "%s/projects/%s/pieces", project_root, current_project) != -1) {
        DIR *dir = opendir(pieces_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (strcmp(entry->d_name, "xlector") == 0) continue;
                
                char val[64], px[64], py[64], pz[64], type[64];
                get_piece_state(entry->d_name, "on_map", val);
                if (strcmp(val, "1") != 0) continue;
                
                get_piece_state(entry->d_name, "pos_x", px);
                get_piece_state(entry->d_name, "pos_y", py);
                get_piece_state(entry->d_name, "pos_z", pz);
                get_piece_state(entry->d_name, "type", type);
                
                if (atoi(px) == tx && atoi(py) == ty && atoi(pz) == tz) {
                    snprintf(desc, sizeof(desc), "%s entity: %s", type, entry->d_name);
                    break;
                }
                }
                closedir(dir);
                }
                free(pieces_dir);
                }

                char final_msg[512];
                snprintf(final_msg, sizeof(final_msg), "Scan (%d,%d,%d): %s", tx, ty, tz, desc);
                set_response(final_msg);
    printf("%s\n", final_msg);
    
    hit_frame_marker();
    
    return 0;
}
