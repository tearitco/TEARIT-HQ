#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#define MAX_PATH 4096
#define MAX_LINE 1024

// TPM FuzzGL Manager (v1.1 - SOVEREIGN GLTPM)
// Responsibility: Route input for fuzz-op-gl, manage camera/xelector state.

char current_project[64] = "fuzz-op-gl";
char active_target_id[64] = "xlector";
char last_key_str[64] = "None";
int emoji_mode = 0;
int current_z_val = 0;  /* Track Z-level for save_manager_state() */

/* Global state for camera and input */
int is_map_control = 0;
float cam_pos[3] = {0.0f, 0.0f, 0.0f};
float cam_rot[3] = {30.0f, 0.0f, 0.0f};
int camera_mode = 4; // Free

// Path cache
char project_root[2048] = ".";
char history_path[4096] = "pieces/apps/player_app/history.txt";
char debug_log_path[4096] = "projects/fuzz-op-gl/manager/debug_log.txt";

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

char* build_path_malloc(const char* rel) {
    size_t sz = strlen(project_root) + strlen(rel) + 2;
    char* p = (char*)malloc(sz);
    if (p) snprintf(p, sz, "%s/%s", project_root, rel);
    return p;
}

int root_has_anchors(const char* root) {
    char pieces_path[4096], projects_path[4096];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_paths() {
    if (!getcwd(project_root, sizeof(project_root))) strncpy(project_root, ".", sizeof(project_root) - 1);
    project_root[sizeof(project_root) - 1] = '\0';

    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    if (root_has_anchors(v)) snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }

    // Resolve project_id from manager state (Commented out to prevent GL drift)
    /*
    char *mgr_state = build_path_malloc("pieces/apps/player_app/manager/state.txt");
    FILE *mf = fopen(mgr_state, "r");
    if (mf) {
        char line[1024];
        while (fgets(line, sizeof(line), mf)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_id") == 0) strncpy(current_project, trim_str(eq + 1), 63);
            }
        }
        fclose(mf);
    }
    free(mgr_state);
    */
    // Explicitly set for this sovereign manager
    strcpy(current_project, "fuzz-op-gl");

    snprintf(debug_log_path, sizeof(debug_log_path), "%s/projects/%s/manager/debug_log.txt", project_root, current_project);
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
}

int read_exec_output(const char* path, char* out_buf, size_t out_sz, const char* a1, const char* a2, const char* a3, const char* a4) {
#ifndef _WIN32
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fd[0]); dup2(pipe_fd[1], STDOUT_FILENO); close(pipe_fd[1]);
        execl(path, path, a1, a2, a3, a4, NULL); exit(1);
    } else if (pid > 0) {
        close(pipe_fd[1]);
        ssize_t n = read(pipe_fd[0], out_buf, out_sz - 1);
        if (n >= 0) out_buf[n] = '\0';
        close(pipe_fd[0]); waitpid(pid, NULL, 0); return 0;
    }
#else
    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s %s %s %s", path, a1?a1:"", a2?a2:"", a3?a3:"", a4?a4:"");
    FILE *pf = popen(cmd, "r");
    if (pf) {
        size_t n = fread(out_buf, 1, out_sz - 1, pf);
        out_buf[n] = '\0';
        pclose(pf);
        return 0;
    }
#endif
    return -1;
}

void log_debug(const char* fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args; va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL)); 
        vfprintf(f, fmt, args); 
        fprintf(f, "\n");
        va_end(args); fclose(f);
    }
}

void log_resp(const char* msg) {
    // Write to project piece's last_response.txt
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/fuzzpet/last_response.txt", project_root, current_project);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%-57s", msg); fclose(f); }
}

void get_state_fast(const char* piece_id, const char* key, char* out_val) {
    char path[4096], line[4096];
    // Prioritize project-specific pieces
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id);
    if (access(path, F_OK) != 0) {
        // Fallback to global world
        snprintf(path, sizeof(path), "%s/pieces/world/map_01/%s/state.txt", project_root, piece_id);
    }
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), key) == 0) {
                    strncpy(out_val, trim_str(eq + 1), 255);
                    fclose(f); return;
                }
            }
        }
        fclose(f);
    }
    strcpy(out_val, "unknown");
}

int get_state_int_fast(const char* piece_id, const char* key) {
    char val[256]; get_state_fast(piece_id, key, val);
    if (strcmp(val, "unknown") == 0 || val[0] == '\0') return -1;
    return atoi(val);
}

void sync_emoji_mode() {
    /* TPM Direct Mirror Access: Write emoji_mode to project's sovereign state */
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/emoji_mode.txt", project_root, current_project);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "emoji_mode=%d\n", emoji_mode);
        fclose(f);
    }

    /* Also write to manager state for global visibility */
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    char lines[100][256];
    int lc = 0, found = 0;
    FILE *mf = fopen(path, "r");
    if (mf) {
        while (fgets(lines[lc], sizeof(lines[0]), mf) && lc < 99) {
            if (strncmp(lines[lc], "emoji_mode=", 11) == 0) {
                snprintf(lines[lc], sizeof(lines[0]), "emoji_mode=%d\n", emoji_mode);
                found = 1;
            }
            lc++;
        }
        fclose(mf);
    }
    if (!found && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "emoji_mode=%d\n", emoji_mode);
    
    mf = fopen(path, "w");
    if (mf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], mf);
        fclose(mf);
    }
}

void perform_mirror_sync() {
    char path[4096];
    // Sync to UI mirror file (NOT to a piece directory - that would overwrite actual piece state)
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/fuzzpet_mirror/state.txt", project_root, current_project);
    
    // Create mirror directory if it doesn't exist
    char dir_path[4096];
    snprintf(dir_path, sizeof(dir_path), "%s/projects/%s/pieces/fuzzpet_mirror", project_root, current_project);
    mkdir(dir_path, 0755);

    FILE *f = fopen(path, "w");
    if (!f) return;

    if (strcmp(active_target_id, "xlector") == 0) {
        /* Get xlector Z-level from state */
        int sz = get_state_int_fast("xlector", "pos_z");
        if (sz == -1) sz = 0;
        fprintf(f, "name=XLECTOR\nhunger=0\nhappiness=0\nenergy=0\nlevel=0\nstatus=active\nemoji_mode=%d\nz_level=%d\n", emoji_mode, sz);
    } else {
        char name[64], hunger[64], happiness[64], energy[64], level[64];
        get_state_fast(active_target_id, "name", name);
        get_state_fast(active_target_id, "hunger", hunger);
        get_state_fast(active_target_id, "happiness", happiness);
        get_state_fast(active_target_id, "energy", energy);
        get_state_fast(active_target_id, "level", level);
        fprintf(f, "name=%s\nhunger=%s\nhappiness=%s\nenergy=%s\nlevel=%s\nstatus=active\nemoji_mode=%d\n",
            name, hunger, happiness, energy, level, emoji_mode);
    }
    fclose(f);
}

void sync_camera_piece() {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/camera/state.txt", project_root, current_project);
    
    // Ensure directory exists
    char dir_path[4096];
    snprintf(dir_path, sizeof(dir_path), "%s/projects/%s/pieces/camera", project_root, current_project);
    mkdir(dir_path, 0755);

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "name=Camera\ntype=camera\n");
        fprintf(f, "pos_x=%.2f\npos_y=%.2f\npos_z=%.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
        fprintf(f, "pitch=%.2f\nyaw=%.2f\nroll=%.2f\n", cam_rot[0], cam_rot[1], cam_rot[2]);
        fprintf(f, "mode=%d\nis_map_control=%d\n", camera_mode, is_map_control);
        fclose(f);
    }
}

void save_manager_state() {
    char path[4096];
    // 1. Update legacy player_app manager state
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    char lines[100][256];
    int lc = 0;
    /* current_z_val is global */
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], sizeof(lines[0]), f) && lc < 99) {
            lc++;
        }
        fclose(f);
    }
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s\nactive_target_id=%s\nlast_key=%s\ncurrent_z=%d\n", current_project, active_target_id, last_key_str, current_z_val);
        fprintf(f, "current_map=map_01_z%d.txt\n", current_z_val);
        fclose(f);

        char *pulse_path = build_path_malloc("pieces/apps/player_app/state_changed.txt");
        FILE *pf = fopen(pulse_path, "a");
        if (pf) { fprintf(pf, "S\n"); fclose(pf); }
        free(pulse_path);
    }

    // 2. Update PROJECT-SPECIFIC session state (GLTPM Compliance)
    snprintf(path, sizeof(path), "%s/projects/%s/session/state.txt", project_root, current_project);
    char session_dir[4096];
    snprintf(session_dir, sizeof(session_dir), "%s/projects/%s/session", project_root, current_project);
    mkdir(session_dir, 0755);
    
    f = fopen(path, "w");
    if (f) {
        char status_buf[256];
        char pet_x[32], pet_y[32], zom_x[32], zom_y[32], sel_x[32], sel_y[32];
        char pet_act[32] = "1", zom_act[32] = "1";
        
        get_state_fast("pet_01", "pos_x", pet_x);
        get_state_fast("pet_01", "pos_y", pet_y);
        get_state_fast("zombie_01", "pos_x", zom_x);
        get_state_fast("zombie_01", "pos_y", zom_y);
        get_state_fast("xlector", "pos_x", sel_x);
        get_state_fast("xlector", "pos_y", sel_y);

        if (strcmp(pet_x, "unknown") == 0) strcpy(pet_act, "0");
        if (strcmp(zom_x, "unknown") == 0) strcpy(zom_act, "0");

        snprintf(status_buf, sizeof(status_buf), "Pet(%s,%s) | Zom(%s,%s) | Sel(%s,%s) %s",
            pet_x, pet_y, zom_x, zom_y, sel_x, sel_y, is_map_control ? "[MAP]" : "[MENU]");

        fprintf(f, "project_id=%s\n", current_project);
        fprintf(f, "active_target_id=%s\n", active_target_id);
        fprintf(f, "last_key=%s\n", last_key_str);
        fprintf(f, "is_map_control=%d\n", is_map_control);
        fprintf(f, "camera_mode=%d\n", camera_mode);
        fprintf(f, "cam_x=%.2f\ncam_y=%.2f\ncam_z=%.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
        fprintf(f, "cam_pitch=%.2f\ncam_yaw=%.2f\ncam_roll=%.2f\n", cam_rot[0], cam_rot[1], cam_rot[2]);
        fprintf(f, "pet_01_pos_x=%s\npet_01_pos_y=%s\npet_01_active=%s\n", pet_x, pet_y, pet_act);
        fprintf(f, "zombie_01_pos_x=%s\nzombie_01_pos_y=%s\nzombie_01_active=%s\n", zom_x, zom_y, zom_act);
        fprintf(f, "xelector_pos_x=%s\nxelector_pos_y=%s\n", sel_x, sel_y);
        fprintf(f, "status_text=%s\n", status_buf);
        fprintf(f, "current_z=%d\n", current_z_val);
        fclose(f);
    }
    
    // 3. Sync Camera Piece (Fizz Standard)
    sync_camera_piece();
}

static void ensure_xlector_state_defaults(void) {
    char sel_state[4096];
    snprintf(sel_state, sizeof(sel_state), "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project);
    char lines[128][256];
    int lc = 0;
    int has_name = 0, has_type = 0, has_on_map = 0, has_map_id = 0, has_x = 0, has_y = 0, has_z = 0;
    
    FILE *sf = fopen(sel_state, "r");
    if (sf) {
        while (lc < 127 && fgets(lines[lc], sizeof(lines[0]), sf)) {
            if (strncmp(lines[lc], "name=", 5) == 0) has_name = 1;
            else if (strncmp(lines[lc], "type=", 5) == 0) has_type = 1;
            else if (strncmp(lines[lc], "on_map=", 7) == 0) has_on_map = 1;
            else if (strncmp(lines[lc], "map_id=", 7) == 0) has_map_id = 1;
            else if (strncmp(lines[lc], "pos_x=", 6) == 0) has_x = 1;
            else if (strncmp(lines[lc], "pos_y=", 6) == 0) has_y = 1;
            else if (strncmp(lines[lc], "pos_z=", 6) == 0) has_z = 1;
            lc++;
        }
        fclose(sf);
    }

    if (!has_name && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "name=Xlector\n");
    if (!has_type && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "type=xlector\n");
    if (!has_on_map && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "on_map=1\n");
    if (!has_map_id && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "map_id=map_01_z0.txt\n");
    if (!has_x && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "pos_x=12\n");
    if (!has_y && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "pos_y=2\n");
    if (!has_z && lc < 128) snprintf(lines[lc++], sizeof(lines[0]), "pos_z=0\n");

    sf = fopen(sel_state, "w");
    if (sf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], sf);
        fclose(sf);
    }
}
float speed = 0.5f;
void trigger_render() {
    char* render = build_path_malloc("pieces/apps/playrm/ops/+x/render_map.+x");
    pid_t p = fork();
    if (p == 0) {
        freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
        execl(render, render, NULL); exit(1);
    } else if (p > 0) waitpid(p, NULL, 0);
    free(render);

    // SYNC: After render_map (which writes player_app/view.txt),
    // we must hit the pulse to ensure parser sees it.
    char *p_path = NULL; asprintf(&p_path, "%s/pieces/display/frame_changed.txt", project_root);
    FILE *pf = fopen(p_path, "a"); if (pf) { fprintf(pf, "M\n"); fclose(pf); } if (p_path) free(p_path);

    /* Signal GLTPM frame change */
    char gl_pulse[4096];
    snprintf(gl_pulse, sizeof(gl_pulse), "%s/projects/%s/session/frame_changed.txt", project_root, current_project);
    pf = fopen(gl_pulse, "a"); if (pf) { fprintf(pf, "G\n"); fclose(pf); }
}

void route_input(int key) {
    if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000 || key == 2100) strcpy(last_key_str, "LEFT");
    else if (key == 1001 || key == 2101) strcpy(last_key_str, "RIGHT");
    else if (key == 1002 || key == 2102) strcpy(last_key_str, "UP");
    else if (key == 1003 || key == 2103) strcpy(last_key_str, "DOWN");

    if (key == '9' || key == 27 || key == 2008) {
        strcpy(active_target_id, "xlector");
        log_resp("Returned to Xlector.");
        save_manager_state();
    }

    if (strcmp(active_target_id, "xlector") == 0 && (key == 10 || key == 13 || key == 2000)) {
        int cx = get_state_int_fast("xlector", "pos_x");
        int cy = get_state_int_fast("xlector", "pos_y");
        char pieces_dir[4096];
        snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/%s/pieces", project_root, current_project);
        DIR *dir = opendir(pieces_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
                /* Can't select zombies - they are AI controlled */
                if (strstr(entry->d_name, "zombie") != NULL) continue;
                
                int tx = get_state_int_fast(entry->d_name, "pos_x");
                int ty = get_state_int_fast(entry->d_name, "pos_y");
                if (tx == cx && ty == cy) {
                    strncpy(active_target_id, entry->d_name, 63);
                    fprintf(stderr, "[DEBUG] Selected entity: %s\n", active_target_id);
                    char *msg = NULL;
                    if (asprintf(&msg, "Selected %s.", entry->d_name) != -1) {
                        log_resp(msg);
                        free(msg);
                    }

                    /* ARCHITECTURE FIX: Xlector inherits entity's map_id */
                    char entity_map_id[256] = "";
                    char xlector_state[4096];
                    snprintf(xlector_state, sizeof(xlector_state), "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project);
                    
                    /* Read entity's map_id */
                    char *path = NULL;
                    if (asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, entry->d_name) != -1) {
                        FILE *f = fopen(path, "r");
                        if (f) {
                            char line[256];
                            while (fgets(line, sizeof(line), f)) {
                                if (strncmp(line, "map_id=", 7) == 0) {
                                    strncpy(entity_map_id, trim_str(line + 7), sizeof(entity_map_id) - 1);
                                    entity_map_id[sizeof(entity_map_id) - 1] = '\0';
                                    break;
                                }
                            }
                            fclose(f);
                        }
                        free(path);
                    }

                    /* Update xlector's map_id to match entity */
                    if (strlen(entity_map_id) > 0) {
                        char lines[100][256];
                        int line_count = 0, found = 0;
                        FILE *sf = fopen(xlector_state, "r");
                        if (sf) {
                            while (fgets(lines[line_count], sizeof(lines[0]), sf) && line_count < 99) {
                                if (strncmp(lines[line_count], "map_id=", 7) == 0) {
                                    snprintf(lines[line_count], sizeof(lines[0]), "map_id=%.247s\n", entity_map_id);
                                    found = 1;
                                }
                                line_count++;
                            }
                            fclose(sf);
                        }
                        if (!found && line_count < 100) snprintf(lines[line_count++], sizeof(lines[0]), "map_id=%.247s\n", entity_map_id);
                        
                        sf = fopen(xlector_state, "w");
                        if (sf) {
                            for (int i = 0; i < line_count; i++) fputs(lines[i], sf);
                            fclose(sf);
                        }
                    }
                    save_manager_state();

                    /* PULSE: Notify parser that active_target_id changed (Trait Menu Sync) */
                    char* pulse_path = build_path_malloc("pieces/apps/player_app/state_changed.txt");
                    FILE *pf = fopen(pulse_path, "a");
                    if (pf) { fprintf(pf, "S\n"); fclose(pf); }
                    free(pulse_path);

                    break;
                }
            }
            closedir(dir);
        }
    }

    /* Handle Z-level changes (x = up, z = down) - exactly like fuzzpet_app */
    if (key == 'x' || key == 'X') {
        int cz = get_state_int_fast(active_target_id, "pos_z"); if (cz == -1) cz = 0;
        int nz = cz + 1; if (nz > 2) nz = 2;  /* Max Z=2 for sky map */
        
        /* Update active entity pos_z via piece_manager */
        char *cmd = NULL;
        asprintf(&cmd, "'%s/pieces/master_ledger/plugins/+x/piece_manager.+x' %s set-state pos_z %d", project_root, active_target_id, nz);
        if (cmd) { system(cmd); free(cmd); }

        /* Update xlector pos_z DIRECTLY */
        char sel_state[4096];
        snprintf(sel_state, sizeof(sel_state), "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project);
        char lines[100][256]; int lc = 0, fz = 0;
        FILE *sf = fopen(sel_state, "r");
        if (sf) {
            while (fgets(lines[lc], sizeof(lines[0]), sf) && lc < 99) {
                if (strncmp(lines[lc], "pos_z=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_z=%d\n", nz); fz = 1; }
                lc++;
            }
            fclose(sf);
        }
        if (!fz && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_z=%d\n", nz);
        sf = fopen(sel_state, "w");
        if (sf) { for (int i = 0; i < lc; i++) fputs(lines[i], sf); fclose(sf); }

        /* Update manager's current_z for save_manager_state() */
        current_z_val = nz;

        /* Hit frame marker for render */
        char *p_path = NULL; asprintf(&p_path, "%s/pieces/display/frame_changed.txt", project_root);
        FILE *pf = fopen(p_path, "a"); if (pf) { fprintf(pf, "Z\n"); fclose(pf); } if (p_path) free(p_path);
    }
    else if (key == 'z' || key == 'Z') {
        int cz = get_state_int_fast(active_target_id, "pos_z"); if (cz == -1) cz = 0;
        int nz = cz - 1; if (nz < 0) nz = 0;
        
        /* Update active entity pos_z via piece_manager */
        char *cmd = NULL;
        asprintf(&cmd, "'%s/pieces/master_ledger/plugins/+x/piece_manager.+x' %s set-state pos_z %d", project_root, active_target_id, nz);
        if (cmd) { system(cmd); free(cmd); }

        /* Update xlector pos_z DIRECTLY */
        char sel_state[4096];
        snprintf(sel_state, sizeof(sel_state), "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project);
        char lines[100][256]; int lc = 0, fz = 0;
        FILE *sf = fopen(sel_state, "r");
        if (sf) {
            while (fgets(lines[lc], sizeof(lines[0]), sf) && lc < 99) {
                if (strncmp(lines[lc], "pos_z=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_z=%d\n", nz); fz = 1; }
                lc++;
            }
            fclose(sf);
        }
        if (!fz && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_z=%d\n", nz);
        sf = fopen(sel_state, "w");
        if (sf) { for (int i = 0; i < lc; i++) fputs(lines[i], sf); fclose(sf); }

        /* Update manager's current_z for save_manager_state() */
        current_z_val = nz;

        /* Hit frame marker for render */
        char *p_path = NULL; asprintf(&p_path, "%s/pieces/display/frame_changed.txt", project_root);
        FILE *pf = fopen(p_path, "a"); if (pf) { fprintf(pf, "Z\n"); fclose(pf); } if (p_path) free(p_path);
    }

    /* Handle emoji toggle (key '6') - BEFORE method_key handler */
    if (key == '6' || key == 2006) {
        emoji_mode = !emoji_mode;
        char msg[64];
        snprintf(msg, sizeof(msg), "Emoji Mode %s", emoji_mode ? "ON" : "OFF");
        log_resp(msg);
        sync_emoji_mode();  /* TPM Mirror Sync */
        /* Save state immediately */
        perform_mirror_sync();
        save_manager_state();
        trigger_render();
        return;  /* Don't fall through to method_key handler */
    }

    /* Handle method hotkeys (2-8) - works for xlector AND entities */
    int method_key = -1;
    if (key >= '2' && key <= '8') method_key = key - '0';
    else if (key >= 2002 && key <= 2008) method_key = key - 2000;  /* Joystick buttons */
    
    if (method_key >= 2 && method_key <= 8) {
        /* Get method name from PDL using pdl_reader */
        char pdl_cmd[4096];
        snprintf(pdl_cmd, sizeof(pdl_cmd), "%s/pieces/system/pdl/+x/pdl_reader.+x %s list_methods", project_root, active_target_id);
        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char methods[1024] = "";
            char line[64];
            int idx = 2;  /* Methods start at index 2 (skip move, select) */
            char target_method[64] = "";
            while (fgets(line, sizeof(line), pf)) {
                line[strcspn(line, "\r\n")] = 0;
                if (idx == method_key) {
                    strncpy(target_method, trim_str(line), sizeof(target_method) - 1);
                    break;
                }
                idx++;
            }
            pclose(pf);
            
            if (strlen(target_method) > 0) {
                /* Get handler for this method */
                snprintf(pdl_cmd, sizeof(pdl_cmd), "%s/pieces/system/pdl/+x/pdl_reader.+x %s get_method %s", project_root, active_target_id, target_method);
                pf = popen(pdl_cmd, "r");
                if (pf) {
                    char handler[4096] = "";
                    if (fgets(handler, sizeof(handler), pf)) {
                        handler[strcspn(handler, "\r\n")] = 0;
                        if (strlen(handler) > 0 && strcmp(handler, "NOT_FOUND") != 0) {
                            /* Execute handler */
                            system(handler);
                            
                            /* Log response with irregular verb conjugation */
                            char resp_key[64];
                            if (strcmp(target_method, "feed") == 0) strcpy(resp_key, "fed");
                            else if (strcmp(target_method, "sleep") == 0) strcpy(resp_key, "slept");
                            else {
                                strcpy(resp_key, target_method);
                                int len = strlen(resp_key);
                                if (len > 0 && resp_key[len-1] == 'y') {
                                    resp_key[len-1] = 'i';
                                    strcat(resp_key, "ied");
                                } else if (len > 0 && resp_key[len-1] == 'e') {
                                    strcat(resp_key, "d");
                                } else {
                                    strcat(resp_key, "ed");
                                }
                            }
                            log_resp(resp_key);
                        }
                    }
                    pclose(pf);
                }
            }
        }
    }

    if (key == 'w' || key == 'a' || key == 's' || key == 'd' || (key >= 1000 && key <= 1003)) {
        char* trait = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
        char dir_str[16];
        if (key == 'w' || key == 1002) strcpy(dir_str, "up");
        else if (key == 's' || key == 1003) strcpy(dir_str, "down");
        else if (key == 'a' || key == 1000) strcpy(dir_str, "left");
        else if (key == 'd' || key == 1001) strcpy(dir_str, "right");
        
        pid_t p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
            char proj_env[128]; snprintf(proj_env, sizeof(proj_env), "PRISC_PROJECT_ID=%s", current_project);
            putenv(proj_env);
            execl(trait, trait, active_target_id, dir_str, NULL); exit(1);
        } else waitpid(p, NULL, 0);
        free(trait);

        /* SHADOW XLECTOR SYNC: Update project xlector to match entity position */
        if (strcmp(active_target_id, "xlector") != 0) {
            int px = get_state_int_fast(active_target_id, "pos_x");
            int py = get_state_int_fast(active_target_id, "pos_y");
            char px_s[32], py_s[32];
            snprintf(px_s, sizeof(px_s), "%d", px);
            snprintf(py_s, sizeof(py_s), "%d", py);

            /* Update project xlector directly */
            char sel_state[4096];
            snprintf(sel_state, sizeof(sel_state), "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project);
            char lines[100][256]; int lc = 0, fx = 0, fy = 0;
            FILE *sf = fopen(sel_state, "r");
            if (sf) {
                while (fgets(lines[lc], sizeof(lines[0]), sf) && lc < 99) {
                    if (strncmp(lines[lc], "pos_x=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_x=%s\n", px_s); fx = 1; }
                    else if (strncmp(lines[lc], "pos_y=", 6) == 0) { snprintf(lines[lc], sizeof(lines[0]), "pos_y=%s\n", py_s); fy = 1; }
                    lc++;
                }
                fclose(sf);
            }
            if (!fx && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_x=%s\n", px_s);
            if (!fy && lc < 100) snprintf(lines[lc++], sizeof(lines[0]), "pos_y=%s\n", py_s);
            
            sf = fopen(sel_state, "w");
            if (sf) { for (int i = 0; i < lc; i++) fputs(lines[i], sf); fclose(sf); }
        }

        /* Call zombie AI to chase player */
        char* zombie_cmd = build_path_malloc("pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x");
        p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
            char proj_env[128]; snprintf(proj_env, sizeof(proj_env), "PRISC_PROJECT_ID=%s", current_project);
            putenv(proj_env);
            execl(zombie_cmd, zombie_cmd, "zombie_01", active_target_id, NULL); exit(1);
        } else waitpid(p, NULL, 0);
        free(zombie_cmd);
    }

    perform_mirror_sync();
    save_manager_state();
    trigger_render();
}

int is_active_layout() {
    char *path = build_path_malloc("pieces/display/current_layout.txt");
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 1; }
    char line[1024];
    if (fgets(line, sizeof(line), f)) {
        fclose(f);
        int res = (strstr(line, "game.chtpm") != NULL ||
                   strstr(line, "fuzz-op.chtpm") != NULL ||
                   strstr(line, "gl_os") != NULL);
        free(path);
        return res;
    }
    fclose(f); free(path);
    return 1;
}

void* input_thread(void* arg __attribute__((unused))) {
    long last_pos = 0; struct stat st;
    if (stat(history_path, &st) == 0) last_pos = st.st_size;
    
    while (1) {
        if (!is_active_layout()) { usleep(100000); continue; }
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) { 
                    fseek(hf, last_pos, SEEK_SET); 
                    int key; 
                    while (fscanf(hf, "%d", &key) == 1) route_input(key); 
                    last_pos = ftell(hf); 
                    fclose(hf); 
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return NULL;
}

void* gltpm_input_thread(void* arg __attribute__((unused))) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/session/history.txt", project_root, current_project);
    long last_pos = 0;
    struct stat st;
    if (stat(path, &st) == 0) last_pos = st.st_size;
    
    while (1) {
        if (stat(path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(path, "r");
                  
                if (f) {
                    fseek(f, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), f)) {
                        char *cmd = strstr(line, "COMMAND:");
                        char *kpress = strstr(line, "KEY_PRESSED:");
                        if (cmd) {
                            char *action = trim_str(cmd + 8);
                            if (strcmp(action, "OP fuzz-op::scan") == 0) route_input('1');
                            else if (strcmp(action, "OP fuzz-op::collect") == 0) route_input('2');
                            else if (strcmp(action, "OP fuzz-op::place") == 0) route_input('3');
                            else if (strcmp(action, "INTERACT") == 0) { // Host signals absolute map control
                                is_map_control = 1;
                                log_resp("Map Control ACTIVE");
                                save_manager_state();
                                trigger_render();
                            }
                            else if (strcmp(action, "OP fuzz-op-gl::toggle_map_mode") == 0) {
                                is_map_control = !is_map_control;
                                log_resp(is_map_control ? "Map Control ACTIVE" : "Menu Mode ACTIVE");
                                save_manager_state();
                                trigger_render();
                            }
                        } else if (kpress) {
                            int key_code = atoi(kpress + 12);
                            if (key_code > 0) {
                                if (key_code == 27) { // ESC
                                    is_map_control = 0;
                                    log_resp("Menu Mode ACTIVE");
                                    save_manager_state();
                                    trigger_render();
                                } else if (is_map_control) {
                                    /* Handle Camera/Xelector movement */
                                    if (key_code >= 32 && key_code <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key_code);
                                    else if (key_code == 1000) strcpy(last_key_str, "LEFT");
                                    else if (key_code == 1001) strcpy(last_key_str, "RIGHT");
                                    else if (key_code == 1002) strcpy(last_key_str, "UP");
                                    else if (key_code == 1003) strcpy(last_key_str, "DOWN");
                                    else if (key_code == 10 || key_code == 13) strcpy(last_key_str, "ENTER");

                                    /* 1. Camera Fly/Mode Controls (Authority: Manager) */
                                    int moved = 0;
                                    if (key_code == '1') { camera_mode = 1; moved = 1; }
                                    else if (key_code == '2') { camera_mode = 2; moved = 1; }
                                    else if (key_code == '3') { camera_mode = 3; moved = 1; }
                                    else if (key_code == '4') { camera_mode = 4; moved = 1; }
                                    /* Note: WASD Fly is handled by Host for smoothness */
                                    
                                    /* 2. Xelector Piece Muscle (Arrows) - Direct Op Delegation */
                                    if (key_code >= 1000 && key_code <= 1003) {
                                        /* FORCE: In Map Control mode, arrows always move the Xlector */
                                        char* trait = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
                                        char dir_str[16];
                                        if (key_code == 1002) strcpy(dir_str, "up");
                                        else if (key_code == 1003) strcpy(dir_str, "down");
                                        else if (key_code == 1000) strcpy(dir_str, "left");
                                        else if (key_code == 1001) strcpy(dir_str, "right");
                                        
                                        pid_t p = fork();
                                        if (p == 0) {
                                            freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
                                            char proj_env[128]; snprintf(proj_env, sizeof(proj_env), "PRISC_PROJECT_ID=%s", current_project);
                                            putenv(proj_env);
                                            execl(trait, trait, "xlector", dir_str, NULL); exit(1);
                                        } else waitpid(p, NULL, 0);
                                        free(trait);
                                        moved = 1;
                                    }

                                    if (moved) {
                                        save_manager_state();
                                        trigger_render();
                                    }
                                } else { // Not map control, route to normal input
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

int main() {
    resolve_paths();
    ensure_xlector_state_defaults();
    save_manager_state(); perform_mirror_sync(); trigger_render();
    
    pthread_t t1, t2;
    pthread_create(&t1, NULL, input_thread, NULL);
    pthread_create(&t2, NULL, gltpm_input_thread, NULL);
    
    while (1) { usleep(1000000); }
    return 0;
}
