/*
 * Op: interact
 * Usage: ./+x/interact.+x <piece_id>
 * Args:
 *   piece_id - The interactor (e.g., "player", "selector")
 * 
 * Behavior:
 *   1. Reads engine state for current project_id
 *   2. Gets piece position (x, y, z)
 *   3. Scans projects/{project_id}/pieces/ for ANY entity at the SAME position
 *   4. If entity found, reads its "on_interact" script path from state.txt
 *   5. Executes script via prisc+x
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <process.h>
    #define SETENV(name, value, overwrite) _putenv_s(name, value)
#else
    #include <unistd.h>
    #define SETENV(name, value, overwrite) setenv(name, value, overwrite)
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
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
    strcpy(out_val, "unknown"); char path[4096]; int found = 0;
    char root[4096]; snprintf(root, sizeof(root), "%s/projects/%s/pieces", project_root, current_project);
    found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0);
    if (!found) { snprintf(root, sizeof(root), "%s/pieces", project_root); found = find_piece_state_recursive(root, piece_id, path, sizeof(path), 0); }
    if (!found) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; if (strcmp(trim_str(line), key) == 0) { strncpy(out_val, trim_str(eq + 1), 255); break; } }
        }
        fclose(f);
    }
}

void log_event(const char* event, const char* piece, const char* details) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t rawtime; struct tm *timeinfo; char timestamp[100];
            time(&rawtime); timeinfo = localtime(&rawtime);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
            fprintf(f, "[%s] %s: %s | %s\n", timestamp, event, piece, details);
            fclose(f);
        }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    const char *active_piece = (argc > 1) ? argv[1] : "selector";
    resolve_paths();
    
    char val[MAX_LINE];
    get_piece_state(active_piece, "pos_x", val); int ax = atoi(val);
    get_piece_state(active_piece, "pos_y", val); int ay = atoi(val);
    get_piece_state(active_piece, "pos_z", val); int az = (strcmp(val, "unknown") == 0) ? 0 : atoi(val);
    
    char *pieces_dir = NULL;
    if (asprintf(&pieces_dir, "%s/projects/%s/pieces", project_root, current_project) != -1) {
        DIR *dir = opendir(pieces_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (strcmp(entry->d_name, active_piece) == 0) continue;
                
                get_piece_state(entry->d_name, "pos_x", val); int px = atoi(val);
                get_piece_state(entry->d_name, "pos_y", val); int py = atoi(val);
                get_piece_state(entry->d_name, "pos_z", val); int pz = (strcmp(val, "unknown") == 0) ? 0 : atoi(val);
                
                int is_adjacent = (px == ax && py == ay) || (px == ax && py == ay - 1) || (px == ax && py == ay + 1) || (px == ax - 1 && py == ay) || (px == ax + 1 && py == ay);
                if (is_adjacent && pz == az) {
                    char script_rel[MAX_PATH]; get_piece_state(entry->d_name, "on_interact", script_rel);
                    char *details = NULL;
                    if (asprintf(&details, "interacted with %s at (%d,%d) script=%s", entry->d_name, px, py, script_rel) != -1) {
                        log_event("Interact", active_piece, details); free(details);
                    }
                    if (strcmp(script_rel, "unknown") != 0) {
                        SETENV("PRISC_PROJECT_ROOT", project_root, 1);
                        char *cmd = NULL;
                        if (asprintf(&cmd, "%s/pieces/system/prisc/prisc+x %s/projects/%s/%s '' '' %s/pieces/apps/player_app/manager/player_ops.txt",
                                 project_root, project_root, current_project, script_rel, project_root) != -1) {
                            system(cmd); free(cmd);
                        }
                    }
                    break;
                }
            }
            closedir(dir);
        }
        free(pieces_dir);
    }
    return 0;
}

