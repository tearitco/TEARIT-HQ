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
    // PRIORITY 1: Environment variable from PAL module
    char *env_project = getenv("PRISC_PROJECT_ID");
    if (env_project && strlen(env_project) > 0) {
        strncpy(current_project, env_project, sizeof(current_project)-1);
    }
    
    // PRIORITY 2: location_kvp for project_root
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
    
    // PRIORITY 3: Project-local manager state (player_app)
    if (strlen(current_project) == 0 || strcmp(current_project, "template") == 0) {
        char *local_mgr_path = NULL;
        if (asprintf(&local_mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(local_mgr_path, "r");
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
            free(local_mgr_path);
        }
    }
    
    // PRIORITY 4: Global manager state (fallback)
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
                        if (strcmp(k, "project_id") == 0) strncpy(current_project, v, sizeof(current_project)-1);
                    }
                }
                fclose(f);
            }
            free(p_mgr_path);
        }
    }
    
    // Final default
    if (strlen(current_project) == 0) {
        strncpy(current_project, "template", sizeof(current_project)-1);
    }
}

void get_state_val(const char* piece_id, const char* key, char* out_val) {
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

void set_state_val(const char* piece_id, const char* key, const char* val) {
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

void set_manager_val(const char* key, const char* val) {
    // Write to project-local manager state FIRST
    char *local_path = NULL;
    if (asprintf(&local_path, "%s/projects/%s/manager/state.txt", project_root, current_project) != -1) {
        char lines[100][MAX_LINE];
        int line_count = 0, found = 0;
        FILE *f = fopen(local_path, "r");
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
        f = fopen(local_path, "w");
        if (f) {
            for (int i = 0; i < line_count; i++) fputs(lines[i], f);
            fclose(f);
        }
        free(local_path);
    }
    
    // ALSO write to global manager state for compatibility
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) == -1) return;
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
    
    char active_target[64] = "fuzzpet";
    
    // Read from project-local manager state FIRST
    char *local_mgr_path = NULL;
    if (asprintf(&local_mgr_path, "%s/projects/%s/manager/state.txt", project_root, current_project) != -1) {
        FILE *f = fopen(local_mgr_path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line), "active_target_id") == 0) {
                        strncpy(active_target, trim_str(eq + 1), 63);
                    }
                }
            }
            fclose(f);
        }
        free(local_mgr_path);
    }
    
    // Fallback to global if not found
    if (strcmp(active_target, "fuzzpet") == 0) {
        char *mgr_path = NULL;
        if (asprintf(&mgr_path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(mgr_path, "r");
            if (f) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), f)) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        if (strcmp(trim_str(line), "active_target_id") == 0) {
                            strncpy(active_target, trim_str(eq + 1), 63);
                        }
                    }
                }
                fclose(f);
            }
            free(mgr_path);
        }
    }
    
    if (strcmp(active_target, "selector") == 0) {
        // Switch to player
        set_manager_val("active_target_id", "fuzzpet");
        printf("Switched to Player Mode\n");
    } else {
        // Switch to selector
        set_manager_val("active_target_id", "selector");
        
        // Sync selector position to fuzzpet
        char x[16], y[16], z[16];
        get_state_val("fuzzpet", "pos_x", x);
        get_state_val("fuzzpet", "pos_y", y);
        get_state_val("fuzzpet", "pos_z", z);
        
        char *sel_dir = NULL;
        asprintf(&sel_dir, "%s/projects/%s/pieces/selector", project_root, current_project);
        char *mkdir_cmd = NULL;
        asprintf(&mkdir_cmd, "mkdir -p %s", sel_dir);
        system(mkdir_cmd);
        free(mkdir_cmd);
        free(sel_dir);
        
        set_state_val("selector", "pos_x", x);
        set_state_val("selector", "pos_y", y);
        set_state_val("selector", "pos_z", z);
        set_state_val("selector", "on_map", "1");
        set_state_val("selector", "type", "selector");
        
        printf("Switched to Selector Mode (at %s,%s)\n", x, y);
    }
    
    hit_frame_marker();
    return 0;
}
