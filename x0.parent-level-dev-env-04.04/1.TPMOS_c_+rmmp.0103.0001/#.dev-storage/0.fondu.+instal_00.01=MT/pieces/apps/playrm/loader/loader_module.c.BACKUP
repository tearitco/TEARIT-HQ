/*
 * loader_module.c - Project Loader Bridge
 * 
 * Responsibilities:
 * 1. Scan projects/ directory
 * 2. Update loader.pdl with dynamic METHOD entries for each project
 * 3. Handle KEY:n input to load selected project and switch layout
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char projects[20][MAX_LINE];
int project_count = 0;

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
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void update_loader_pdl() {
    char *projects_dir_path = NULL;
    if (asprintf(&projects_dir_path, "%s/projects", project_root) == -1) return;
    DIR *dir = opendir(projects_dir_path);
    if (!dir) { free(projects_dir_path); return; }

    project_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && project_count < 20) {
        if (entry->d_name[0] == '.') continue;
        char *abs_p = NULL;
        if (asprintf(&abs_p, "%s/%s", projects_dir_path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(abs_p, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(projects[project_count++], entry->d_name, MAX_LINE - 1);
        }
        free(abs_p);
    }
    closedir(dir);
    free(projects_dir_path);

    char *pdl_path = NULL;
    if (asprintf(&pdl_path, "%s/pieces/apps/playrm/loader/loader.pdl", project_root) != -1) {
        FILE *f = fopen(pdl_path, "w");
        if (f) {
            fprintf(f, "SECTION      | KEY                | VALUE\n");
            fprintf(f, "----------------------------------------\n");
            fprintf(f, "META         | piece_id           | loader\n");
            fprintf(f, "META         | version            | 1.0\n");
            fprintf(f, "META         | determinism        | strict\n\n");
            fprintf(f, "STATE        | name                 | Project Loader\n");
            fprintf(f, "STATE        | status               | active\n\n");
            fprintf(f, "METHOD       | move                 | void\n");
            for (int i = 0; i < project_count; i++) {
                fprintf(f, "METHOD       | %s | LOAD_PROJECT:%s\n", projects[i], projects[i]);
            }
            fclose(f);
        }
        free(pdl_path);
    }
}

void load_project(const char* name) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=%s\n", name);
            // DEFAULT FOCUS:
            fprintf(f, "active_target_id=selector\n");
            fprintf(f, "current_z=0\n");
            fprintf(f, "last_key=None\n");
            fclose(f);
        }
        free(path);
    }
    
    // PROJECT PIECE INITIALIZATION (TPM Compliance)
    // Initialize selector for all projects
    char *sel_dir = NULL;
    if (asprintf(&sel_dir, "%s/projects/%s/pieces/selector", project_root, name) != -1) {
        char *cmd = NULL;
        asprintf(&cmd, "mkdir -p %s", sel_dir);
        system(cmd); free(cmd);
        free(sel_dir);
    }
    
    // SIGNAL: Change layout to game.chtpm
    char *layout_path = NULL;
    if (asprintf(&layout_path, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(layout_path, "a");
        if (f) {
            fprintf(f, "pieces/apps/playrm/layouts/game.chtpm\n");
            fclose(f);
        }
        free(layout_path);
    }

    // Hit global frame marker to trigger parser reload
    char *pulse = NULL;
    if (asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "L\n"); fclose(f); }
        free(pulse);
    }
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        int res = (strstr(line, "playrm/layouts/loader.chtpm") != NULL);
        free(path);
        return res;
    }
    fclose(f);
    free(path);
    return 0;
}

int main() {
    resolve_paths();
    update_loader_pdl();
    
    // Set engine to focus on loader initially (FORCED ON STARTUP)
    char *mgr_state = NULL;
    if (asprintf(&mgr_state, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(mgr_state, "w");
        if (f) {
            fprintf(f, "project_id=template\n");
            fprintf(f, "active_target_id=loader\n");
            fprintf(f, "current_z=0\n");
            fprintf(f, "last_key=None\n");
            fclose(f);
        }
        free(mgr_state);
    }
    
    // Also sync player_app/state.txt immediately so parser sees it
    char *app_state = NULL;
    if (asprintf(&app_state, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(app_state, "w");
        if (f) {
            fprintf(f, "project_id=template\n");
            fprintf(f, "active_target_id=loader\n");
            fclose(f);
        }
        free(app_state);
    }

    // Initial signal to parser to load methods
    char *pulse = NULL;
    if (asprintf(&pulse, "%s/pieces/apps/fuzzpet_app/fuzzpet/view_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "X\n"); fclose(f); }
        free(pulse);
    }

    long last_pos = 0;
    struct stat st;
    char *history_path = NULL;
    if (asprintf(&history_path, "%s/pieces/apps/fuzzpet_app/fuzzpet/history.txt", project_root) == -1) return 1;
    
    while (1) {
        if (!is_active_layout()) {
            usleep(100000); // Sleep longer when inactive
            continue;
        }
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        if (key >= '1' && key <= '9') {
                            int idx = key - '1';
                            if (idx < project_count) {
                                load_project(projects[idx]);
                            }
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            }
        }
        usleep(16667);
    }
    free(history_path);
    return 0;
}
