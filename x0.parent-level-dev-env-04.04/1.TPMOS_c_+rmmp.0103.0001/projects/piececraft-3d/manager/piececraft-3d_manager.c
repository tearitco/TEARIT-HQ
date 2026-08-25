#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <dirent.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_CMD 8192

char project_root[MAX_PATH] = ".";
char project_id[64] = "piececraft-3d";
char active_target_id[64] = "xlector";
int is_map_control = 0;
char last_response[256] = "Initialized.";

float cam_pos[3] = {0.0f, 0.0f, 0.0f};
float cam_rot[3] = {15.0f, 0.0f, 0.0f};
int camera_mode = 4;

static volatile sig_atomic_t g_shutdown = 0;

void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH - 1);
            }
        }
        fclose(kvp);
    }
}

int get_piece_state_int(const char* pid, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, project_id, pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[MAX_LINE];
    int val = -1;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(trim_str(line), key) == 0) {
                val = atoi(trim_str(eq + 1));
                break;
            }
        }
    }
    fclose(f);
    return val;
}

void set_piece_state_int(const char* pid, const char* key, int val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, project_id, pid);
    char lines[100][MAX_LINE];
    int lc = 0, found = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
            char *eq = strchr(lines[lc], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(lines[lc]), key) == 0) {
                    snprintf(lines[lc], sizeof(lines[0]), "%s=%d\n", key, val);
                    found = 1;
                } else *eq = '=';
            }
            lc++;
        }
        fclose(f);
    }
    if (!found && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "%s=%d\n", key, val);
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < lc; i++) fputs(lines[i], f);
        fclose(f);
    }
}

void save_manager_state() {
    char path[MAX_PATH];
    
    // 1. Project-Specific session state (for GL-OS parser)
    snprintf(path, sizeof(path), "%s/projects/%s/session/state.txt", project_root, project_id);
    
    int stone_count = 0, wood_count = 0;
    char inv_path[MAX_PATH];
    snprintf(inv_path, sizeof(inv_path), "%s/projects/%s/pieces/%s/inventory.txt", project_root, project_id, active_target_id);
    FILE *inv_f = fopen(inv_path, "r");
    if (inv_f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), inv_f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "stone") == 0) stone_count = atoi(trim_str(eq + 1));
                else if (strcmp(trim_str(line), "wood") == 0) wood_count = atoi(trim_str(eq + 1));
            }
        }
        fclose(inv_f);
    }

    int sx = get_piece_state_int("xlector", "pos_x");
    int sy = get_piece_state_int("xlector", "pos_y");
    int sz = get_piece_state_int("xlector", "pos_z");

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "active_target_id=%s\n", active_target_id);
        fprintf(f, "is_map_control=%d\n", is_map_control);
        fprintf(f, "last_response=%s\n", last_response);
        fprintf(f, "inv_stone=%d\n", stone_count);
        fprintf(f, "inv_wood=%d\n", wood_count);
        fprintf(f, "xelector_pos_x=%d\n", sx);
        fprintf(f, "xelector_pos_y=%d\n", sy);
        fprintf(f, "current_z=%d\n", sz);

        /* SOVEREIGN DRIFT FIX: Manager MUST export its camera source of truth */
        fprintf(f, "camera_mode=%d\n", camera_mode);
        fprintf(f, "cam_x=%.2f\ncam_y=%.2f\ncam_z=%.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
        fprintf(f, "cam_pitch=%.2f\ncam_yaw=%.2f\ncam_roll=%.2f\n", cam_rot[0], cam_rot[1], cam_rot[2]);
        fclose(f);
    }

    // 2. GUI State fallback (for legacy CHTPM)
    snprintf(path, sizeof(path), "%s/projects/%s/manager/state.txt", project_root, project_id);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "active_piece=%s\n", active_target_id);
        fprintf(f, "last_response=%s\n", last_response);
        fprintf(f, "inv_stone=%d\n", stone_count);
        fprintf(f, "inv_wood=%d\n", wood_count);
        fclose(f);
    }

    // 3. Player App Global State (to sync render_map)
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s\n", project_id);
        fprintf(f, "active_target_id=%s\n", active_target_id);
        fclose(f);
    }
}

void trigger_render() {
    // 1. Trigger render_map to update view.txt (Sovereign Map Sync)
    char render_op[MAX_PATH];
    snprintf(render_op, sizeof(render_op), "%s/pieces/apps/playrm/ops/+x/render_map.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        execl(render_op, render_op, NULL);
        exit(1);
    } else waitpid(pid, NULL, 0);

    // 2. Pulse GL-OS frame change
    char gl_pulse[MAX_PATH];
    snprintf(gl_pulse, sizeof(gl_pulse), "%s/projects/%s/session/frame_changed.txt", project_root, project_id);
    FILE *pf = fopen(gl_pulse, "a");
    if (pf) { fprintf(pf, "G\n"); fclose(pf); }
}

void route_input(int key) {
    // Escape / Back to Xlector
    if (key == 27 || key == '9' || key == 2008) {
        strncpy(active_target_id, "xlector", 63);
        is_map_control = 0;
        strcpy(last_response, "Returned to Xlector.");
        save_manager_state();
        trigger_render();
        return;
    }

    // Possession (Enter)
    if (key == 10 || key == 13 || key == 2000) {
        if (strcmp(active_target_id, "xlector") == 0) {
            int cx = get_piece_state_int("xlector", "pos_x");
            int cy = get_piece_state_int("xlector", "pos_y");
            
            DIR *dir = opendir("projects/piececraft-3d/pieces");
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
                    if (strstr(entry->d_name, "zombie") != NULL) continue;
                    
                    int tx = get_piece_state_int(entry->d_name, "pos_x");
                    int ty = get_piece_state_int(entry->d_name, "pos_y");
                    if (tx == cx && ty == cy) {
                        strncpy(active_target_id, entry->d_name, 63);
                        snprintf(last_response, sizeof(last_response), "Possessed %s.", entry->d_name);
                        save_manager_state();
                        trigger_render();
                        break;
                    }
                }
                closedir(dir);
            }
        }
        return;
    }

    // WASD / Arrow Movement
    if (key == 'w' || key == 'a' || key == 's' || key == 'd' ||
        key == 'W' || key == 'A' || key == 'S' || key == 'D' ||
        (key >= 1000 && key <= 1003)) {
        
        char dir_str[8];
        if (key == 'w' || key == 'W' || key == 1002) strcpy(dir_str, "up");
        else if (key == 's' || key == 'S' || key == 1003) strcpy(dir_str, "down");
        else if (key == 'a' || key == 'A' || key == 1000) strcpy(dir_str, "left");
        else if (key == 'd' || key == 'D' || key == 1001) strcpy(dir_str, "right");

        char op_path[MAX_PATH];
        snprintf(op_path, sizeof(op_path), "%s/pieces/apps/playrm/ops/+x/move_entity.+x", project_root);
        
        pid_t pid = fork();
        if (pid == 0) {
            execl(op_path, op_path, active_target_id, dir_str, project_id, NULL);
            exit(1);
        } else waitpid(pid, NULL, 0);

        // Sync Xlector if possessing something
        if (strcmp(active_target_id, "xlector") != 0) {
            int px = get_piece_state_int(active_target_id, "pos_x");
            int py = get_piece_state_int(active_target_id, "pos_y");
            if (px != -1) set_piece_state_int("xlector", "pos_x", px);
            if (py != -1) set_piece_state_int("xlector", "pos_y", py);
        }

        // Trigger Zombie AI
        snprintf(op_path, sizeof(op_path), "%s/projects/%s/ops/+x/zombie_ai.+x", project_root, project_id);
        pid = fork();
        if (pid == 0) {
            execl(op_path, op_path, "zombie_01", active_target_id, NULL);
            exit(1);
        } else waitpid(pid, NULL, 0);

        save_manager_state();
        trigger_render();
        return;
    }

    // Z/X Elevation
    if (key == 'z' || key == 'Z' || key == 'x' || key == 'X') {
        int cz = get_piece_state_int(active_target_id, "pos_z");
        if (cz == -1) cz = 0;
        int nz = cz;
        if (key == 'x' || key == 'X') nz++;
        else nz--;
        
        set_piece_state_int(active_target_id, "pos_z", nz);
        if (strcmp(active_target_id, "xlector") != 0) {
            set_piece_state_int("xlector", "pos_z", nz);
        }
        
        save_manager_state();
        trigger_render();
        return;
    }

    // PDL-Driven Dynamic Methods (Keys 2-9)
    if ((key >= '2' && key <= '9') || (key >= 2002 && key <= 2009)) {
        int idx = (key >= 2002) ? (key - 2000) : (key - '0');
        
        char reader_path[MAX_PATH];
        snprintf(reader_path, sizeof(reader_path), "%s/pieces/system/pdl/+x/pdl_reader.+x", project_root);
        
        char pdl_path[MAX_PATH];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/pieces/%s/piece.pdl", project_root, project_id, active_target_id);
        
        if (access(pdl_path, F_OK) == 0) {
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "%s %s get_method_at %d", reader_path, pdl_path, idx - 2);
            
            FILE *p = popen(cmd, "r");
            if (p) {
                char handler[MAX_LINE];
                if (fgets(handler, sizeof(handler), p)) {
                    handler[strcspn(handler, "\n\r")] = 0;
                    if (strlen(handler) > 0 && strcmp(handler, "void") != 0) {
                        /* Execute the handler Op */
                        char full_cmd[MAX_CMD];
                        snprintf(full_cmd, sizeof(full_cmd), "%s %s", handler, active_target_id);
                        system(full_cmd);
                        snprintf(last_response, sizeof(last_response), "Executed %d.", idx);
                    }
                }
                pclose(p);
            }
        }
        save_manager_state();
        trigger_render();
    }
}

void* input_thread(void* arg) {
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0; struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *f = fopen(hist_path, "r");
            if (f) {
                fseek(f, last_pos, SEEK_SET);
                int key;
                while (fscanf(f, "%d", &key) == 1) route_input(key);
                last_pos = ftell(f);
                fclose(f);
            }
        }
        usleep(16667);
    }
    return NULL;
}

void* gltpm_input_thread(void* arg) {
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/projects/%s/session/history.txt", project_root, project_id);
    long last_pos = 0; struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(hist_path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), f)) {
                        char *cmd = strstr(line, "COMMAND:");
                        char *kpress = strstr(line, "KEY_PRESSED:");
                        if (cmd) {
                            char *action = trim_str(cmd + 8);
                            if (strcmp(action, "INTERACT") == 0) {
                                is_map_control = 1;
                                save_manager_state();
                                trigger_render();
                            }
                            else if (strcmp(action, "ESC") == 0) {
                                is_map_control = 0;
                                save_manager_state();
                                trigger_render();
                            }
                            else if (strncmp(action, "CAMERA_SET:", 11) == 0) {
                                float x, y, z, p, yaw, r;
                                if (sscanf(action + 11, "%f,%f,%f,%f,%f,%f", &x, &y, &z, &p, &yaw, &r) == 6) {
                                    cam_pos[0] = x; cam_pos[1] = y; cam_pos[2] = z;
                                    cam_rot[0] = p; cam_rot[1] = yaw; cam_rot[2] = r;
                                    save_manager_state();
                                    trigger_render();
                                }
                            }
                            else if (strncmp(action, "CAMERA_MODE:", 12) == 0) {
                                camera_mode = atoi(action + 12);
                                save_manager_state();
                                trigger_render();
                            }
                            else if (strncmp(action, "CAMERA_MOVE:", 12) == 0) {
                                float dx=0, dy=0, dz=0;
                                if (sscanf(action + 12, "%f,%f,%f", &dx, &dy, &dz) == 3) {
                                    cam_pos[0] += dx; cam_pos[1] += dy; cam_pos[2] += dz;
                                    save_manager_state();
                                    trigger_render();
                                }
                            }
                            else if (strcmp(action, "OP piececraft::mining") == 0) route_input('2');
                            else if (strcmp(action, "OP piececraft::placing") == 0) route_input('3');
                        } else if (kpress) {
                            int key_code = atoi(kpress + 12);
                            if (key_code > 0) {
                                if (key_code == 27) {
                                    is_map_control = 0;
                                    save_manager_state();
                                    trigger_render();
                                } else if (is_map_control) {
                                    /* In Map Control, only route Arrow keys to entity movement */
                                    /* WASD and ZX are handled as Camera Fly by the Host */
                                    int moved = 0;
                                    if (key_code >= 1000 && key_code <= 1003) {
                                        char op_path[MAX_PATH];
                                        snprintf(op_path, sizeof(op_path), "%s/pieces/apps/playrm/ops/+x/move_entity.+x", project_root);
                                        char dir_str[16];
                                        if (key_code == 1002) strcpy(dir_str, "up");
                                        else if (key_code == 1003) strcpy(dir_str, "down");
                                        else if (key_code == 1000) strcpy(dir_str, "left");
                                        else if (key_code == 1001) strcpy(dir_str, "right");
                                        
                                        pid_t p = fork();
                                        if (p == 0) {
                                            execl(op_path, op_path, "xlector", dir_str, project_id, NULL);
                                            exit(1);
                                        } else waitpid(p, NULL, 0);
                                        moved = 1;
                                    }

                                    if (moved) {
                                        save_manager_state();
                                        trigger_render();
                                    }
                                } else {
                                    route_input(key_code);
                                }
                            }
                        }
                    }
                    last_pos = ftell(f);
                    fclose(f);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    pthread_t t1, t2;
    pthread_create(&t1, NULL, input_thread, NULL);
    pthread_create(&t2, NULL, gltpm_input_thread, NULL);

    printf("Piececraft-3D Manager (Sovereign) initialized.\n");
    save_manager_state();
    trigger_render();

    while (!g_shutdown) { usleep(1000000); }
    return 0;
}
