/*
 * Op: toggle_selector
 * Usage: ./+x/toggle_selector.+x
 * 
 * Toggles active_target_id between fuzzpet and selector.
 * Syncs selector position to fuzzpet when activating.
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
#include <sys/stat.h>
#include <dirent.h>

#define MAX_PATH 8192
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

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

void set_piece_state(const char* piece_id, const char* key, const char* val) {
    char path[4096]; int found = 0;
    char root[4096]; snprintf(root, sizeof(root), "%s/projects/%s/pieces", project_root, current_project);
    found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0);
    if (!found) { snprintf(root, sizeof(root), "%s/pieces", project_root); found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0); }
    if (!found) return;
    char lines[100][MAX_LINE]; int lc = 0, fk = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
            char *eq = strchr(lines[lc], '=');
            if (eq) { *eq = '\0'; if (strcmp(trim_str(lines[lc]), key) == 0) { snprintf(lines[lc], sizeof(lines[0]), "%s=%s\n", key, val); fk = 1; } else *eq = '='; }
            lc++;
        }
        fclose(f);
    }
    if (!fk && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=%s\n", key, val);
    f = fopen(path, "w"); if (f) { for (int i = 0; i < lc; i++) fputs(lines[i], f); fclose(f); }
}

void set_manager_val(const char* key, const char* val) {
    char *local_path = NULL;
    if (asprintf(&local_path, "%s/projects/%s/manager/state.txt", project_root, current_project) != -1) {
        char lines[100][MAX_LINE]; int lc = 0, fk = 0;
        FILE *f = fopen(local_path, "r");
        if (f) {
            while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
                char *eq = strchr(lines[lc], '=');
                if (eq) { *eq = '\0'; if (strcmp(trim_str(lines[lc]), key) == 0) { snprintf(lines[lc], sizeof(lines[0]), "%s=%s\n", key, val); fk = 1; } else *eq = '='; }
                lc++;
            }
            fclose(f);
        }
        if (!fk && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=%s\n", key, val);
        f = fopen(local_path, "w"); if (f) { for (int i = 0; i < lc; i++) fputs(lines[i], f); fclose(f); }
        free(local_path);
    }
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        char lines[100][MAX_LINE]; int lc = 0, fk = 0;
        FILE *f = fopen(path, "r");
        if (f) {
            while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
                char *eq = strchr(lines[lc], '=');
                if (eq) { *eq = '\0'; if (strcmp(trim_str(lines[lc]), key) == 0) { snprintf(lines[lc], sizeof(lines[0]), "%s=%s\n", key, val); fk = 1; } else *eq = '='; }
                lc++;
            }
            fclose(f);
        }
        if (!fk && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=%s\n", key, val);
        f = fopen(path, "w"); if (f) { for (int i = 0; i < lc; i++) fputs(lines[i], f); fclose(f); }
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
    resolve_paths();
    char active_target[64] = "fuzzpet";
    char *local_mgr_path = NULL;
    if (asprintf(&local_mgr_path, "%s/projects/%s/manager/state.txt", project_root, current_project) != -1) {
        FILE *f = fopen(local_mgr_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) { *eq = '\0'; if (strcmp(trim_str(line), "active_target_id") == 0) strncpy(active_target, trim_str(eq + 1), 63); }
            }
            fclose(f);
        }
        free(local_mgr_path);
    }
    
    if (strcmp(active_target, "selector") == 0) {
        set_manager_val("active_target_id", "fuzzpet");
        printf("Switched to Player Mode\n");
    } else {
        set_manager_val("active_target_id", "selector");
        char x[16], y[16], z[16];
        get_piece_state("fuzzpet", "pos_x", x);
        get_piece_state("fuzzpet", "pos_y", y);
        get_piece_state("fuzzpet", "pos_z", z);
        set_piece_state("selector", "pos_x", x);
        set_piece_state("selector", "pos_y", y);
        set_piece_state("selector", "pos_z", z);
        set_piece_state("selector", "on_map", "1");
        set_piece_state("selector", "type", "selector");
        printf("Switched to Selector Mode (at %s,%s)\n", x, y);
    }
    hit_frame_marker();
    return 0;
}

