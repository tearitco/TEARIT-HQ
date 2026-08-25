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
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char projects[50][MAX_LINE];
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
    while ((entry = readdir(dir)) != NULL && project_count < 50) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "trunk") == 0) continue;  // Skip archive directory
        
        char *abs_p = NULL;
        if (asprintf(&abs_p, "%s/%s", projects_dir_path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(abs_p, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Check for project.pdl before adding
            char *pdl_path = NULL;
            if (asprintf(&pdl_path, "%s/project.pdl", abs_p) != -1) {
                if (access(pdl_path, F_OK) == 0) {
                    strncpy(projects[project_count++], entry->d_name, MAX_LINE - 1);
                }
                free(pdl_path);
            }
        }
        free(abs_p);
    }
    closedir(dir);
    free(projects_dir_path);

    // Sort projects alphabetically for consistent ordering with UI
    for (int i = 0; i < project_count - 1; i++) {
        for (int j = i + 1; j < project_count; j++) {
            if (strcmp(projects[i], projects[j]) > 0) {
                char tmp[MAX_LINE];
                strncpy(tmp, projects[i], MAX_LINE - 1);
                strncpy(projects[i], projects[j], MAX_LINE - 1);
                strncpy(projects[j], tmp, MAX_LINE - 1);
            }
        }
    }

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

void get_pdl_value(const char* project, const char* section, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, project) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        int in_section = 0;
        while (fgets(line, sizeof(line), f)) {
            /* Check if this line starts with the section name */
            if (strncmp(line, section, strlen(section)) == 0 && line[strlen(section)] == ' ') {
                in_section = 1;
                continue;
            }
            /* Check for new section header */
            if (strncmp(line, "SECTION", 7) == 0 || (line[0] != ' ' && line[0] != '\t' && line[0] != '\n' && line[0] != '-' && strstr(line, "|") == NULL && strlen(trim_str(line)) > 0)) {
                if (in_section) break;  /* We've left our section */
            }
            
            if (in_section) {
                char *pipe1 = strchr(line, '|');
                if (pipe1) {
                    char k_buf[MAX_LINE];
                    char *pipe2 = strchr(pipe1 + 1, '|');
                    if (pipe2) {
                        int k_len = pipe2 - pipe1 - 1;
                        if (k_len >= MAX_LINE) k_len = MAX_LINE - 1;
                        strncpy(k_buf, trim_str(pipe1 + 1), k_len);
                        k_buf[k_len] = '\0';
                        
                        if (strcmp(trim_str(k_buf), key) == 0) {
                            strncpy(out_val, trim_str(pipe2 + 1), out_sz - 1);
                            out_val[out_sz - 1] = '\0';
                            fclose(f);
                            free(path);
                            return;
                        }
                    }
                }
            }
        }
        fclose(f);
    }
    free(path);
}

static void load_entry_layout_from_routes(const char *name, char *entry_layout, size_t out_sz) {
    char *route_path = NULL;
    if (asprintf(&route_path, "%s/pieces/os/project_routes.kvp", project_root) == -1) return;

    FILE *f = fopen(route_path, "r");
    free(route_path);
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq;
        char *key;
        char *value;

        if (line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        key = trim_str(line);
        value = trim_str(eq + 1);
        if (strcmp(key, name) == 0) {
            strncpy(entry_layout, value, out_sz - 1);
            entry_layout[out_sz - 1] = '\0';
            break;
        }
    }

    fclose(f);
}

static void probe_entry_layout(const char *name, char *entry_layout, size_t out_sz) {
    const char *probes[] = {
        "projects/%s/layouts/%s.chtpm",
        "projects/%s/layouts/main.chtpm",
        "projects/%s/layouts/quiz.chtpm",
        "pieces/apps/%s/layouts/%s.chtpm"
    };

    for (int i = 0; i < 4; i++) {
        char probe[MAX_PATH];
        char *abs_path = NULL;

        snprintf(probe, sizeof(probe), probes[i], name, name);
        if (asprintf(&abs_path, "%s/%s", project_root, probe) == -1) continue;

        if (access(abs_path, F_OK) == 0) {
            strncpy(entry_layout, probe, out_sz - 1);
            entry_layout[out_sz - 1] = '\0';
            free(abs_path);
            return;
        }

        free(abs_path);
    }
}

void load_project(const char* name) {
    char entry_layout[MAX_PATH] = "";
    char starting_map[MAX_LINE] = "map_01.txt";
    char player_piece[MAX_LINE] = "selector";
    
    get_pdl_value(name, "META", "entry_layout", entry_layout, sizeof(entry_layout));
    get_pdl_value(name, "STATE", "starting_map", starting_map, sizeof(starting_map));
    get_pdl_value(name, "STATE", "player_piece", player_piece, sizeof(player_piece));
    
    /* DEBUG: Log what was read from PDL */
    FILE *dbgf = fopen("debug.txt", "a");
    if (dbgf) {
        fprintf(dbgf, "LOADER PDL READ: project=%s, entry_layout='%s', starting_map='%s', player_piece='%s'\n", 
                name, entry_layout, starting_map, player_piece);
        fclose(dbgf);
    }
    
    if (entry_layout[0] == '\0') {
        load_entry_layout_from_routes(name, entry_layout, sizeof(entry_layout));
    }

    if (entry_layout[0] == '\0') {
        probe_entry_layout(name, entry_layout, sizeof(entry_layout));
    }

    if (entry_layout[0] == '\0') {
        snprintf(entry_layout, sizeof(entry_layout), "projects/%s/layouts/%s.chtpm", name, name);
    }
    
    if (strstr(starting_map, ".txt") == NULL) {
        size_t len = strnlen(starting_map, sizeof(starting_map));
        if (len + 4 < sizeof(starting_map)) {
            memcpy(starting_map + len, ".txt", 5); /* include null terminator */
        } else if (sizeof(starting_map) > 5) {
            /* Truncate safely while guaranteeing .txt suffix */
            size_t keep = sizeof(starting_map) - 5;
            starting_map[keep] = '\0';
            memcpy(starting_map + keep, ".txt", 5);
        }
    }

    /* DEBUG: Log layout resolution */
    FILE *dbg = fopen("debug.txt", "a");
    if (dbg) { 
        fprintf(dbg, "LOADER: Project=%s, Layout=%s, Map=%s, Player=%s\n", name, entry_layout, starting_map, player_piece); 
        fclose(dbg); 
    }

    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=%s\n", name);
            fprintf(f, "current_map=%s\n", starting_map);
            fprintf(f, "active_target_id=%s\n", player_piece);
            fprintf(f, "current_z=0\n");
            fprintf(f, "last_key=None\n");
            fclose(f);
        }
        free(path);
    }
    
    // PROJECT PIECE INITIALIZATION (TPM Compliance)
    char *piece_dir = NULL;
    if (asprintf(&piece_dir, "%s/projects/%s/pieces/%s", project_root, name, player_piece) != -1) {
        char *cmd = NULL;
#ifndef _WIN32
        if (asprintf(&cmd, "mkdir -p '%s'", piece_dir) != -1) {
            system(cmd); free(cmd);
        }
#else
        /* Windows mkdir creates parents by default, but we use shell silently to be safe */
        STARTUPINFO si; PROCESS_INFORMATION pi; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); ZeroMemory(&pi, sizeof(pi));
        char mkdir_cmd[MAX_PATH + 64];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "cmd /c if not exist \"%s\" mkdir \"%s\"", piece_dir, piece_dir);
        if (CreateProcess(NULL, mkdir_cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 1000);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
#endif
        free(piece_dir);
    }

    if (asprintf(&path, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "%s\n", entry_layout);
            fclose(f);
        }
        free(path);
    }

    // Hit global frame marker to trigger parser reload
    char *pulse = NULL;
    if (asprintf(&pulse, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "L\n"); fclose(f); }
        free(pulse);
    }

    // Hit state marker to force variable refresh
    if (asprintf(&pulse, "%s/pieces/apps/player_app/state_changed.txt", project_root) != -1) {
        FILE *f = fopen(pulse, "a");
        if (f) { fprintf(f, "S\n"); fclose(f); }
        free(pulse);
    }

    // TRUNCATE VIEW FILES (Prevent Ghost Frame)
    char *v1 = NULL, *v2 = NULL;
    if (asprintf(&v1, "%s/pieces/apps/player_app/view.txt", project_root) != -1) {
        FILE *f = fopen(v1, "w"); if (f) fclose(f);
        free(v1);
    }
    if (asprintf(&v2, "%s/pieces/apps/fuzzpet_app/fuzzpet/view.txt", project_root) != -1) {
        FILE *f = fopen(v2, "w"); if (f) fclose(f);
        free(v2);
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

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/playrm/loader/gui_state.txt", project_root);
    
    char *markup = malloc(32768);
    if (!markup) return;
    markup[0] = '\0';

    for (int i = 0; i < project_count; i++) {
        char item[512];
        /* Calculate padding for box layout consistency */
        int label_len = strlen(projects[i]);
        int padding = 45 - label_len;
        if (padding < 0) padding = 0;
        
        char pad_str[64] = "";
        for (int p = 0; p < padding; p++) strcat(pad_str, " ");

        snprintf(item, sizeof(item), 
                 "<text label=\"║  \" /><button label=\"%s\" onClick=\"LOAD_PROJECT:%s\" /><text label=\"%s ║\" /><br/>", 
                 projects[i], projects[i], pad_str);
        
        if (strlen(markup) + strlen(item) < 32767) {
            strcat(markup, item);
        }
    }

    FILE *gf = fopen(path, "w");
    if (gf) {
        fprintf(gf, "project_list=%s\n", markup);
        fprintf(gf, "piece_methods=\n");
        fclose(gf);
    }
    free(markup);

    /* Signal Parser to reload variables */
    char *sc = NULL;
    if (asprintf(&sc, "%s/pieces/apps/player_app/state_changed.txt", project_root) != -1) {
        FILE *f = fopen(sc, "a");
        if (f) { fprintf(f, "S\n"); fclose(f); }
        free(sc);
    }
}

int main() {
    resolve_paths();
    update_loader_pdl();
    write_gui_state();

    // Set engine to focus on loader initially (FORCED ON STARTUP)
    // NOTE: Don't change project_id - loader is part of playrm app
    char *mgr_state = NULL;
    if (asprintf(&mgr_state, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(mgr_state, "r+");  // Read existing first
        if (f) {
            char line[MAX_LINE];
            int has_project_id = 0;
            long pos = 0;
            // Check if project_id already exists
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "project_id=", 11) == 0) has_project_id = 1;
                pos = ftell(f);
            }
            // If no project_id, set to playrm
            if (!has_project_id) {
                fseek(f, 0, SEEK_END);
                fprintf(f, "project_id=playrm\n");
            }
            fclose(f);
        }
        free(mgr_state);
    }
    
    // Also sync player_app/state.txt immediately so parser sees it
    // (Don't overwrite project_id, just ensure active_target_id is set)
    char *app_state = NULL;
    if (asprintf(&app_state, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(app_state, "r+");
        if (f) {
            char line[MAX_LINE];
            int has_target = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "active_target_id=", 17) == 0) has_target = 1;
            }
            if (!has_target) {
                fseek(f, 0, SEEK_END);
                fprintf(f, "active_target_id=loader\n");
            }
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

    /* DAEMON MODE: Stay running to prevent relaunch loops */
    while (1) {
        usleep(1000000); /* Check every 1s (low CPU) */
        /* Optional: Re-scan projects if directory changed? 
           For now, just stay alive. */
    }

    return 0;
}
