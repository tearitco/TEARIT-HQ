/*
 * playrm_module.c - Player Bridge Module (Engine Host v2.0)
 * 
 * Responsibilities:
 * 1. Project Initialization & Loading
 * 2. Input Routing to Reusable Ops (Mode 1 Movement)
 * 3. State Management (active_target_id, project_id, last_key)
 * 4. Launching Transient PAL Glue (Mode 2 Decision)
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

/* TPM path helpers — canonical: see pieces/os/type_registry.pdl for type icons */
#define TPM_PIECE_MANAGER_PATH "pieces/master_ledger/plugins/+x/piece_manager.+x"
#define TPM_OPS_ROOT           "pieces/apps/playrm/ops/+x"

static char* pm_path_for(const char *project_root) {
    char *r = NULL;
    if (asprintf(&r, "%s/%s", project_root, TPM_PIECE_MANAGER_PATH) == -1) return NULL;
    return r;
}

static char* ops_path_for(const char *project_root) {
    char *r = NULL;
    if (asprintf(&r, "%s/%s", project_root, TPM_OPS_ROOT) == -1) return NULL;
    return r;
}

static char* op_full_path(const char *project_root, const char *op_name) {
    char *r = NULL;
    if (asprintf(&r, "%s/%s/%s.+x", project_root, TPM_OPS_ROOT, op_name) == -1) return NULL;
    return r;
}

#define MAX_PATH 4096
#define MAX_LINE 256
#define MAX_VAR_VALUE 16384

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "template";
char active_target_id[MAX_LINE] = "hero";
char last_key_str[MAX_LINE] = "None";
int current_z = 0;

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

void load_engine_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line);
                    char *v = trim_str(eq + 1);
                    if (strcmp(k, "project_id") == 0) strncpy(current_project, v, MAX_LINE-1);
                    else if (strcmp(k, "active_target_id") == 0) strncpy(active_target_id, v, MAX_LINE-1);
                    else if (strcmp(k, "current_z") == 0) current_z = atoi(v);
                }
            }
            fclose(f);
        }
        free(path);
    }
}

void save_engine_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=%s\n", current_project);
            fprintf(f, "active_target_id=%s\n", active_target_id);
            fprintf(f, "current_z=%d\n", current_z);
            fprintf(f, "last_key=%s\n", last_key_str);
            fclose(f);
        }
        free(path);
    }

    /* NEW: Update gui_state.txt with dynamic methods (Dumb Theater Mode) */
    char *gui_path = NULL;
    if (asprintf(&gui_path, "%s/projects/%s/manager/gui_state.txt", project_root, current_project) != -1) {
        /* Capture output of get_piece_methods_op */
        char *op_path = NULL;
        asprintf(&op_path, "'%s/pieces/chtpm/ops/+x/get_piece_methods_op.+x' '%s' '%s'", project_root, active_target_id, current_project);
        
        FILE *pf = popen(op_path, "r");
        if (pf) {
            char methods[MAX_VAR_VALUE];
            size_t n = fread(methods, 1, sizeof(methods)-1, pf);
            methods[n] = '\0';
            pclose(pf);

            FILE *gf = fopen(gui_path, "w");
            if (gf) {
                fprintf(gf, "piece_methods=%s\n", methods);
                fclose(gf);
            }
        }
        free(op_path);
        free(gui_path);
    }
}

void call_op(const char* op_name, const char* arg1, const char* arg2, const char* arg3) {
    char *ops = ops_path_for(project_root);
    char *cmd = NULL;
    if (!ops) return;
    if (arg3) {
        asprintf(&cmd, "'%s/%s.+x' '%s' '%s' '%s' > /dev/null 2>&1", ops, op_name, arg1, arg2, arg3);
    } else if (arg2) {
        asprintf(&cmd, "'%s/%s.+x' '%s' '%s' > /dev/null 2>&1", ops, op_name, arg1, arg2);
    } else {
        asprintf(&cmd, "'%s/%s.+x' '%s' > /dev/null 2>&1", ops, op_name, arg1);
    }
    if (cmd) {
        system(cmd);
        free(cmd);
    }
    free(ops);
}

void trigger_render() {
    char *op = op_full_path(project_root, "render_map");
    if (!op) return;
    char *cmd = NULL;
    if (asprintf(&cmd, "'%s' > /dev/null 2>&1", op) != -1) {
        system(cmd);
        free(cmd);
    }
    free(op);
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "M\n"); fclose(f); }
        free(path);
    }
}

/* Get method name from piece's PDL at given index (2-8) */
void get_method_name(const char* piece_id, int index, char* out_method, char* project) {
    strcpy(out_method, "");
    char *pm = pm_path_for(project_root);
    if (!pm) return;
    char *cmd = NULL;
    if (asprintf(&cmd, "'%s' '%s' list-methods", pm, piece_id) == -1) { free(pm); return; }
    free(pm);
    
    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) return;
    
    char line[128];
    int current_idx = 2;
    while (fgets(line, sizeof(line), fp)) {
        char *m = trim_str(line);
        if (strlen(m) == 0 || strcmp(m, "move") == 0 || strcmp(m, "select") == 0) continue;
        if (current_idx == index) {
            strncpy(out_method, m, 63);
            pclose(fp);
            return;
        }
        current_idx++;
    }
    pclose(fp);
}

/* Execute method by reading the VALUE from piece PDL (TPM-driven dispatch) */
void execute_method(const char* piece_id, const char* method) {
    char method_value[MAX_LINE] = "";

    /* Step 1: Query piece_manager for the method's PDL VALUE */
    char *pm = pm_path_for(project_root);
    char *cmd = NULL;
    if (pm && asprintf(&cmd, "'%s' '%s' get-method-value '%s'", pm, piece_id, method) != -1) {
        free(pm);
        FILE *fp = popen(cmd, "r");
        free(cmd);
        if (fp) {
            char line[512];
            if (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\n")] = '\0';
                strncpy(method_value, line, sizeof(method_value) - 1);
            }
            pclose(fp);
        }
    }

    /* Step 2: Handle the VALUE */
    /* If not found, try generic op path */
    if (strlen(method_value) == 0 || strcmp(method_value, "NOT_FOUND") == 0) {
        char *ops = ops_path_for(project_root);
        if (ops) {
            asprintf(&cmd, "'%s/%s.+x' '%s' '%s' > /dev/null 2>&1", ops, method, piece_id, current_project);
            free(ops);
        }
    }
    /* If VALUE is "void", skip execution (built-in method, no +x script) */
    else if (strcmp(method_value, "void") == 0) {
        return;
    }
    /* If VALUE is a raw shell command (no +x), execute directly */
    else if (strstr(method_value, ".+x") == NULL) {
        system(method_value);
        return;
    }
    /* VALUE is a +x path — resolve relative to project_root and execute */
    else {
        char op_path[MAX_PATH] = "";
        char *space = strchr(method_value, ' ');
        if (space) {
            strncpy(op_path, method_value, space - method_value);
        } else {
            strncpy(op_path, method_value, sizeof(op_path) - 1);
        }
        char full_path[MAX_PATH];
        if (op_path[0] == '/') {
            strncpy(full_path, op_path, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", project_root, op_path);
        }
        asprintf(&cmd, "'%s' '%s' '%s' > /dev/null 2>&1", full_path, piece_id, current_project);
    }

    if (cmd) {
        system(cmd);
        free(cmd);
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
        int res = (strstr(line, "playrm/layouts/game.chtpm") != NULL || 
                   strstr(line, "man-pal.chtpm") != NULL ||
                   strstr(line, "os.chtpm") != NULL);
        free(path);
        return res;
    }
    fclose(f);
    free(path);
    return 0;
}

int main() {
    resolve_paths();
    load_engine_state();
    trigger_render();

    long last_pos = 0;
    struct stat st;
    char *history_path = NULL;
    if (asprintf(&history_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;

    int last_key_processed = -1;

    while (1) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[256];
                    while (fgets(line, sizeof(line), hf)) {
                        int key = atoi(line);
                        if (key <= 0) continue;
                        if (key == last_key_processed) continue;
                        last_key_processed = key;

                        /* 1. Standard Key Debug (Mirroring working modules) */
                        if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
                        else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
                        else if (key == 27) strcpy(last_key_str, "ESC");
                        else if (key == 1000) strcpy(last_key_str, "LEFT");
                        else if (key == 1001) strcpy(last_key_str, "RIGHT");
                        else if (key == 1002) strcpy(last_key_str, "UP");
                        else if (key == 1003) strcpy(last_key_str, "DOWN");
                        else snprintf(last_key_str, sizeof(last_key_str), "CODE %d", key);

                        load_engine_state(); // Refresh context before action
                        save_engine_state(); // Sync last_key to UI immediately

                        /* 2. Mode 1: Uniform Movement Relay */
                        int moved = 0;
                        const char* dir = NULL;
                        if (key == 'w' || key == 'W' || key == 1002) { dir = "w"; moved = 1; }
                        else if (key == 's' || key == 'S' || key == 1003) { dir = "s"; moved = 1; }
                        else if (key == 'a' || key == 'A' || key == 1000) { dir = "a"; moved = 1; }
                        else if (key == 'd' || key == 'D' || key == 1001) { dir = "d"; moved = 1; }
                        
                        if (moved) {
                            call_op("move_entity", active_target_id, dir, current_project);
                        }
                        /* 3. Hotkey Method System (Keys 2-8) */
                        else if (key >= '2' && key <= '8' && strcmp(active_target_id, "xlector") != 0 && strcmp(active_target_id, "hero") != 0) {
                            char method[64] = {0};
                            get_method_name(active_target_id, key - '0', method, current_project);
                            if (strlen(method) > 0) {
                                execute_method(active_target_id, method);
                                // Set response
                                char *resp_path = NULL;
                                if (asprintf(&resp_path, "%s/pieces/apps/editor/response.txt", project_root) != -1) {
                                    FILE *rf = fopen(resp_path, "w");
                                    if (rf) {
                                        char resp[128];
                                        if (strcmp(method, "toggle_emoji") == 0) {
                                            // Toggle handled internally
                                            snprintf(resp, sizeof(resp), "Method: %s", method);
                                        } else {
                                            snprintf(resp, sizeof(resp), "[RESP]: %s executed", method);
                                        }
                                        fprintf(rf, "%-57s", resp);
                                        fclose(rf);
                                    }
                                    free(resp_path);
                                }
                            }
                        }
                        /* 4. Mode 2: Transient PAL Decision Glue */
                        else {
                            bool is_pal_project = (strcmp(current_project, "fuzzpet_v2") == 0 || strcmp(current_project, "man-pal") == 0);
                            if (is_pal_project && key >= '1' && key <= '8') {
                                char *prisc_cmd = NULL;
                                asprintf(&prisc_cmd, "PRISC_KEY=%d PRISC_ACTIVE_TARGET='%s' PRISC_PROJECT_ROOT='%s' PRISC_PROJECT_ID='%s' '%s/pieces/system/prisc/prisc+x' '%s/projects/%s/scripts/game_loop.asm' > /dev/null 2>&1",
                                         key, active_target_id, project_root, current_project, project_root, project_root, current_project);
                                if (prisc_cmd) { system(prisc_cmd); free(prisc_cmd); }
                            }
                            /* Fallbacks for non-PAL projects */
                            else if (key == '1') call_op("fuzzpet_action", active_target_id, "feed", NULL);
                            else if (key == '9') {
                                // Return to xlector
                                strcpy(active_target_id, "xlector");
                                save_engine_state();
                            }
                        }
                        trigger_render();
                        /* Tick is now written by render_map.c after view is composed */
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    free(history_path);
    return 0;
}
