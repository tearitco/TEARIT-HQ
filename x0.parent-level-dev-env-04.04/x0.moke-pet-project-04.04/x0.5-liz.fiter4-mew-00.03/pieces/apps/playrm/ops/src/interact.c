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

/* MAX_PATH per compile clean standards - use ifndef to avoid Windows redefinition */
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MAX_LINE 256

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    const char* attempts[] = {"pieces/locations/location_kvp", "../pieces/locations/location_kvp", "../../pieces/locations/location_kvp", "../../../pieces/locations/location_kvp"};
    for (int i = 0; i < 4; i++) {
        FILE *kvp = fopen(attempts[i], "r");
        if (kvp) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kvp)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(line, "project_root") == 0) {
                        strncpy(project_root, eq + 1, MAX_PATH - 1);
                        project_root[strcspn(project_root, "\n\r")] = 0;
                        fclose(kvp);
                        goto found_root;
                    }
                }
            }
            fclose(kvp);
        }
    }
found_root:;
    char *engine_state_path = NULL;
    if (asprintf(&engine_state_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *ef = fopen(engine_state_path, "r");
        if (ef) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), ef)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), sizeof(current_project)-1);
                }
            }
            fclose(ef);
        }
        free(engine_state_path);
    }
}

void get_piece_state(const char* piece_id, const char* key, char* out_val) {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id) == -1) {
        strcpy(out_val, "unknown");
        return;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Fallback: try global pieces */
        free(path);
        asprintf(&path, "%s/pieces/world/map_01/%s/state.txt", project_root, piece_id);
        f = fopen(path, "r");
    }

    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), key) == 0) {
                    strncpy(out_val, trim_str(eq + 1), 255);
                    fclose(f);
                    free(path);
                    return;
                }
            }
        }
        fclose(f);
    }
    free(path);
    strcpy(out_val, "unknown");
}

/* Log to master ledger */
void log_event(const char* event, const char* piece, const char* details) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/master_ledger/master_ledger.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            time_t rawtime;
            struct tm *timeinfo;
            char timestamp[100];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
            fprintf(f, "[%s] %s: %s | %s\n", timestamp, event, piece, details);
            fclose(f);
        }
        free(path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <piece_id>\n", argv[0]);
        return 1;
    }
    
    resolve_paths();
    const char* active_piece = argv[1];
    
    char val[MAX_LINE];
    get_piece_state(active_piece, "pos_x", val);
    int ax = atoi(val);
    get_piece_state(active_piece, "pos_y", val);
    int ay = atoi(val);
    get_piece_state(active_piece, "pos_z", val);
    int az = (strcmp(val, "unknown") == 0) ? 0 : atoi(val);
    
    /* Scan for entities at same position */
    char *pieces_dir = NULL;
    if (asprintf(&pieces_dir, "%s/projects/%s/pieces", project_root, current_project) != -1) {
        DIR *dir = opendir(pieces_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (strcmp(entry->d_name, active_piece) == 0) continue;
                
                get_piece_state(entry->d_name, "pos_x", val);
                int px = atoi(val);
                get_piece_state(entry->d_name, "pos_y", val);
                int py = atoi(val);
                get_piece_state(entry->d_name, "pos_z", val);
                int pz = (strcmp(val, "unknown") == 0) ? 0 : atoi(val);
                
                /* Adjacency check: same tile OR one tile away (UP, DOWN, LEFT, RIGHT) */
                int is_adjacent = (px == ax && py == ay) || 
                                  (px == ax && py == ay - 1) || 
                                  (px == ax && py == ay + 1) || 
                                  (px == ax - 1 && py == ay) || 
                                  (px == ax + 1 && py == ay);

                if (is_adjacent && pz == az) {
                    /* Found target! Check for interaction script */
                    char script_rel[MAX_PATH];
                    get_piece_state(entry->d_name, "on_interact", script_rel);
                    
                    char *details = NULL;
                    if (asprintf(&details, "interacted with %s at (%d,%d) script=%s", entry->d_name, px, py, script_rel) != -1) {
                        log_event("Interact", active_piece, details);
                        free(details);
                    }

                    if (strcmp(script_rel, "unknown") != 0) {
                        /* Execute via Prisc */
                        SETENV("PRISC_PROJECT_ROOT", project_root, 1);
                        char *cmd = NULL;
                        if (asprintf(&cmd, "%s/pieces/system/prisc/prisc+x %s/projects/%s/%s '' '' %s/pieces/apps/player_app/manager/player_ops.txt",
                                 project_root, project_root, current_project, script_rel, project_root) != -1) {
                            system(cmd);
                            free(cmd);
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
