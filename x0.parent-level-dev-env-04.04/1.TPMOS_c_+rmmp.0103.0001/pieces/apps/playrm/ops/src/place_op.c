/*
 * Op: place_op
 * Usage: ./+x/place_op.+x
 * 
 * Places an item (like 'R' for rock) at the selector's current position.
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
#include <sys/stat.h>

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
    char *env_root = getenv("PRISC_PROJECT_ROOT");
    char *env_project = getenv("PRISC_PROJECT_ID");
    
    if (env_root && strlen(env_root) > 0) strncpy(project_root, env_root, MAX_PATH-1);
    if (env_project && strlen(env_project) > 0) strncpy(current_project, env_project, sizeof(current_project)-1);
    
    if (strlen(project_root) == 0 || strcmp(project_root, ".") == 0) {
        FILE *kvp = fopen("pieces/locations/location_kvp", "r");
        if (!kvp) kvp = fopen("../../../locations/location_kvp", "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_root") == 0) strncpy(project_root, trim_str(eq + 1), MAX_PATH-1);
                }
            }
            fclose(kvp);
        }
    }
    
    if (strlen(current_project) == 0 || strcmp(current_project, "template") == 0) {
        char *path = NULL;
        asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root);
        FILE *f = fopen(path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), sizeof(current_project)-1);
                }
            }
            fclose(f);
        }
        free(path);
    }
    if (strlen(current_project) == 0) strncpy(current_project, "template", sizeof(current_project)-1);
}

int find_piece_state_recursive(const char* dir_path, const char* piece_id, char* out_path, size_t out_size, int depth) {
    DIR *dir; struct dirent *entry;
    if (!dir_path || !piece_id || !out_path || depth > 8) return 0;
    dir = opendir(dir_path); if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char child[4096]; struct stat st;
        if (entry->d_name[0] == '.') continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        if (strcmp(entry->d_name, piece_id) == 0) {
            char state_path[4096]; snprintf(state_path, sizeof(state_path), "%s/state.txt", child);
            if (access(state_path, F_OK) == 0) { strncpy(out_path, state_path, out_size - 1); out_path[out_size - 1] = '\0'; closedir(dir); return 1; }
        }
        if (find_piece_state_recursive(child, piece_id, out_path, out_size, depth + 1)) { closedir(dir); return 1; }
    }
    closedir(dir); return 0;
}

void get_piece_state(const char* piece_id, const char* key, char* out_val) {
    strcpy(out_val, ""); char path[4096]; int found = 0;
    char root[4096]; snprintf(root, sizeof(root), "%s/projects/%s/pieces", project_root, current_project);
    found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0);
    if (!found) { snprintf(root, sizeof(root), "%s/pieces", project_root); found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0); }
    if (!found) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; if (strcmp(trim_str(line), key) == 0) { strcpy(out_val, trim_str(eq + 1)); break; } }
        }
        fclose(f);
    }
}

void log_event(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t now = time(NULL);
            fprintf(f, "[%ld] EventFire: %s | Source: place_op\n", now, msg);
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

int main(int argc, char *argv[]) {
    const char *piece_id = (argc > 1) ? argv[1] : "selector";
    resolve_paths();
    char sx_s[16], sy_s[16];
    get_piece_state(piece_id, "pos_x", sx_s);
    get_piece_state(piece_id, "pos_y", sy_s);
    if (sx_s[0] == '\0') strcpy(sx_s, "0"); if (sy_s[0] == '\0') strcpy(sy_s, "0");
    int sx = atoi(sx_s), sy = atoi(sy_s);

    char *map_path = NULL;
    if (asprintf(&map_path, "%s/projects/%s/maps/%s", project_root, current_project, active_map_file) == -1) return 1;
    
    char map[MAP_ROWS][MAP_COLS + 2];
    FILE *mf = fopen(map_path, "r");
    if (!mf) { free(map_path); return 1; }
    int rows = 0; while (rows < MAP_ROWS && fgets(map[rows], sizeof(map[0]), mf)) rows++;
    fclose(mf);

    // Place a rock
    if (sy >= 0 && sy < rows && sx >= 0 && sx < (int)strlen(map[sy])) {
        map[sy][sx] = 'R';
        mf = fopen(map_path, "w");
        if (mf) {
            for (int i = 0; i < rows; i++) fputs(map[i], mf);
            fclose(mf);
        }
        log_event("Placed Rock!");
        printf("Placed Rock at (%d,%d)\n", sx, sy);
    }

    free(map_path);
    hit_frame_marker();
    return 0;
}

