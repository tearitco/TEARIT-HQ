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
#else
#include <windows.h>
#include <process.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define popen _popen
#define pclose _pclose
#endif
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>

#ifdef _WIN32
#define usleep(us) Sleep((us)/1000)
#ifndef _vscprintf
/* Simple asprintf for Windows */
int asprintf(char** strp, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if (len < 0) return -1;
    *strp = (char*)malloc(len + 1);
    if (!*strp) return -1;
    int result = vsprintf(*strp, fmt, args);
    va_end(args);
    return result;
}
#endif
#endif

#define MAX_PATH 4096
#define MAX_LINE 1024

// TPM FuzzLegacy Manager (v1.0 - PROJECT AWARE)
// Responsibility: Route input for legacy projects, sync project mirrors.

char current_project[64] = "fuzz-op";
char active_target_id[64] = "xlector";
char last_key_str[64] = "None";
int emoji_mode = 0;
int current_z_val = 0;  /* Track Z-level for save_manager_state() */
int gui_focus_index = 0;
int last_key_code = 0;

// Path cache
char project_root[2048] = ".";
char history_path[4096] = "pieces/apps/player_app/history.txt";
char debug_log_path[4096] = "projects/fuzz-op/manager/debug_log.txt";

char last_response_str[256] = "None";
char method_response_str[256] = "None";

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
    
    // Resolve project_id from manager state
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

    snprintf(debug_log_path, sizeof(debug_log_path), "%s/projects/%s/manager/debug_log.txt", project_root, current_project);
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
}

void log_debug(const char* fmt, ...) {
    FILE *f = fopen(debug_log_path, "a");
    if (f) {
        va_list args; va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL)); vfprintf(f, fmt, args); fprintf(f, "\n");
        va_end(args); fclose(f);
    }
}

void log_resp(const char* msg) {
    if (!msg) return;
    strncpy(last_response_str, msg, 255);
    last_response_str[255] = '\0';
    
    // Write to project piece's last_response.txt
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/fuzzpet/last_response.txt", project_root, current_project);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%-57s", msg); fclose(f); }
}

int find_piece_state_recursive(const char* dir_path, const char* piece_id, char* out_path, size_t out_size, int depth) {
    DIR *dir;
    struct dirent *entry;

    if (!dir_path || !piece_id || !out_path || depth > 8) return 0;

    dir = opendir(dir_path);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char child[4096];
        struct stat st;

        if (entry->d_name[0] == '.') continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if (stat(child, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (strcmp(entry->d_name, piece_id) == 0) {
            char state_path[4096];
            snprintf(state_path, sizeof(state_path), "%s/state.txt", child);
            if (access(state_path, F_OK) == 0) {
                strncpy(out_path, state_path, out_size - 1);
                out_path[out_size - 1] = '\0';
                closedir(dir);
                return 1;
            }
        }

        if (find_piece_state_recursive(child, piece_id, out_path, out_size, depth + 1)) {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

void get_state_fast(const char* piece_id, const char* key, char* out_val) {
    char path[4096], line[4096];
    // Prioritize project-specific pieces
    snprintf(path, sizeof(path), "%s/projects/%s/pieces/%s/state.txt", project_root, current_project, piece_id);
    
    if (access(path, F_OK) != 0) {
        char pieces_root[4096];
        /* 1. Try project-specific recursive search */
        snprintf(pieces_root, sizeof(pieces_root), "%s/projects/%s/pieces", project_root, current_project);
        if (!find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0)) {
            /* 2. Fallback to global pieces recursive search */
            snprintf(pieces_root, sizeof(pieces_root), "%s/pieces", project_root);
            if (!find_piece_state_recursive(pieces_root, piece_id, path, sizeof(path), 0)) {
                /* 3. Final legacy fallback */
                snprintf(path, sizeof(path), "%s/pieces/world/map_01/%s/state.txt", project_root, piece_id);
            }
        }
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

void sync_focus() {
    char path[4096];
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_index.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        int active_idx = 0;
        if (fscanf(f, "%d", &active_idx) == 1) {
            if (active_idx > 0) gui_focus_index = active_idx;
        }
        fclose(f);
    }
}

void write_gui_state() {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/manager/gui_state.txt", project_root, current_project);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/fuzz-op/manager/+x/fuzz-op_manager.+x\n");
        fprintf(f, "active_layout_id=fuzz-op.chtpm\n");
        fprintf(f, "app_title=FUZZ-OP PET SIM\n");
        fprintf(f, "project_id=%s\n", current_project);
        fprintf(f, "active_gui_index=%d\n", gui_focus_index);

        char name[64], hunger[64], happiness[64], energy[64], level[64], status[64];
        get_state_fast("fuzzpet", "name", name);
        get_state_fast("fuzzpet", "hunger", hunger);
        get_state_fast("fuzzpet", "happiness", happiness);
        get_state_fast("fuzzpet", "energy", energy);
        get_state_fast("fuzzpet", "level", level);
        get_state_fast("fuzzpet", "status", status);
        if (strcmp(name, "unknown") == 0) get_state_fast("pet_01", "name", name);
        if (strcmp(hunger, "unknown") == 0) get_state_fast("pet_01", "hunger", hunger);
        if (strcmp(happiness, "unknown") == 0) get_state_fast("pet_01", "happiness", happiness);
        if (strcmp(energy, "unknown") == 0) get_state_fast("pet_01", "energy", energy);
        if (strcmp(level, "unknown") == 0) get_state_fast("pet_01", "level", level);
        if (strcmp(status, "unknown") == 0 || status[0] == '\0') strcpy(status, "active");

        fprintf(f, "pet_name=%s\n", name);
        fprintf(f, "pet_status=%s\n", status);
        fprintf(f, "hunger=%s\n", hunger);
        fprintf(f, "happiness=%s\n", happiness);
        fprintf(f, "energy=%s\n", energy);
        fprintf(f, "level=%s\n", level);
        fprintf(f, "active_target=%s\n", active_target_id);
        fprintf(f, "last_key=%s\n", last_key_str);
        fprintf(f, "last_response=%s\n", last_response_str);
        fprintf(f, "method_response=%s\n", method_response_str);

        /* NEW: Update dynamic methods (Dumb Theater Mode) */
        char *op_path = NULL;
        asprintf(&op_path, "'%s/pieces/chtpm/ops/+x/get_piece_methods_op.+x' %s %s", project_root, active_target_id, current_project);
        FILE *pf = popen(op_path, "r");
        if (pf) {
            char methods[16384];
            size_t n = fread(methods, 1, sizeof(methods)-1, pf);
            methods[n] = '\0';
            pclose(pf);
            fprintf(f, "piece_methods=%s\n", methods);
        }
        free(op_path);

        /* Load clock state for turn_count and game_time */
        char clock_path[4096];
        snprintf(clock_path, sizeof(clock_path), "%s/pieces/system/clock_daemon/state.txt", project_root);
        FILE *cf = fopen(clock_path, "r");
        if (cf) {
            char line[1024];
            while (fgets(line, sizeof(line), cf)) {
                if (strncmp(line, "clock_turn=", 11) == 0) fprintf(f, "turn_count=%s", line + 11);
                else if (strncmp(line, "clock_time=", 11) == 0) fprintf(f, "game_time=%s", line + 11);
                else if (strncmp(line, "turn=", 5) == 0) fprintf(f, "turn_count=%s", line + 5);
                else if (strncmp(line, "time=", 5) == 0) fprintf(f, "game_time=%s", line + 5);
            }
            fclose(cf);
        }
        
        fprintf(f, "debug_key_code=%d\n", last_key_code);

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
        fprintf(f, "project_id=%s\n", current_project);
        // Use the potentially updated active_target_id
        fprintf(f, "active_target_id=%s\n", active_target_id); 
        fprintf(f, "last_key=%s\n", last_key_str);
        fprintf(f, "current_z=%d\n", current_z_val);
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
        
        get_state_fast("pet_01", "pos_x", pet_x); // Assuming pet_01 is the default target
        get_state_fast("pet_01", "pos_y", pet_y);
        get_state_fast("zombie_01", "pos_x", zom_x);
        get_state_fast("zombie_01", "pos_y", zom_y);
        get_state_fast("xlector", "pos_x", sel_x);
        get_state_fast("xlector", "pos_y", sel_y);

        if (strcmp(pet_x, "unknown") == 0) strcpy(pet_act, "0");
        if (strcmp(zom_x, "unknown") == 0) strcpy(zom_act, "0");

        snprintf(status_buf, sizeof(status_buf), "Pet(%s,%s) | Zom(%s,%s) | Sel(%s,%s)", 
                 pet_x, pet_y, zom_x, zom_y, sel_x, sel_y);

        fprintf(f, "project_id=%s\n", current_project);
        // Ensure active_target_id is correctly written here if it's relevant for session state
        fprintf(f, "active_target_id=%s\n", active_target_id); 
        fprintf(f, "camera_mode=4\n");
        fprintf(f, "pet_01_pos_x=%s\npet_01_pos_y=%s\npet_01_active=%s\n", pet_x, pet_y, pet_act);
        fprintf(f, "zombie_01_pos_x=%s\nzombie_01_pos_y=%s\nzombie_01_active=%s\n", zom_x, zom_y, zom_act);
        fprintf(f, "selector_pos_x=%s\nselector_pos_y=%s\n", sel_x, sel_y);
        fprintf(f, "status_text=%s\n", status_buf);
        fprintf(f, "current_z=%d\n", current_z_val);
        fclose(f);
    }
    write_gui_state();
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

void trigger_render() {
    char* render = build_path_malloc("pieces/apps/playrm/ops/+x/render_map.+x");
#ifndef _WIN32
    pid_t p = fork();
    if (p == 0) {
        freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
        execl(render, render, NULL); exit(1);
    } else if (p > 0) waitpid(p, NULL, 0);
#else
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "\"%s\"", render);
    system(cmd);
#endif
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

void tick_clock_if_manual() {
    if (strcmp(active_target_id, "xlector") == 0) return;
    
    char state_path[4096];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/clock_daemon/state.txt", project_root);
    FILE *f = fopen(state_path, "r");
    if (f) {
        char line[1024];
        int manual = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "mode=manual", 11) == 0) {
                manual = 1;
                break;
            }
        }
        fclose(f);
        if (manual) {
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "'%s/pieces/system/clock_daemon/plugins/+x/clock_daemon.+x' tick", project_root);
            system(cmd);
        }
    }
}

void route_input(int key) {
    last_key_code = key;
    log_debug("Key received: %d", key);
    if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000 || key == 2100) strcpy(last_key_str, "LEFT");
    else if (key == 1001 || key == 2101) strcpy(last_key_str, "RIGHT");
    else if (key == 1002 || key == 2102) strcpy(last_key_str, "UP");
    else if (key == 1003 || key == 2103) strcpy(last_key_str, "DOWN");
    
    if (key == '9' || key == 27 || key == 2008) { 
        if (strcmp(active_target_id, "xlector") != 0) {
            strcpy(active_target_id, "xlector");
            log_resp("Returned to Xlector.");
            save_manager_state();
            return;
        }
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

    /* Handle method hotkeys (2-9) - works for xlector AND entities */
    int method_key = -1;
    if (key >= '2' && key <= '9') method_key = key - '0';
    else if (key >= 2002 && key <= 2009) method_key = key - 2000;  /* Joystick buttons */

    if (method_key >= 2 && method_key <= 9) {
        /* Get method name from PDL using pdl_reader */
        char pdl_cmd[4096];
        snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s list_methods", project_root, active_target_id);
        FILE *pf = popen(pdl_cmd, "r");
        if (pf) {
            char line[64];
            int idx = 2;  /* Methods start at index 2 (skip move, select) */
            char target_method[64] = "";
            
            while (fgets(line, sizeof(line), pf)) {
                line[strcspn(line, "\n\r")] = 0;
                if (idx == method_key) {
                    strncpy(target_method, trim_str(line), sizeof(target_method) - 1);
                    break;
                }
                idx++;
            }
            pclose(pf);
            
            if (strlen(target_method) > 0) {
                /* Get handler for this method */
                snprintf(pdl_cmd, sizeof(pdl_cmd), "'%s/pieces/system/pdl/+x/pdl_reader.+x' %s get_method %s", project_root, active_target_id, target_method);
                pf = popen(pdl_cmd, "r");
                if (pf) {
                    char handler[4096] = "";
                    if (fgets(handler, sizeof(handler), pf)) {
                        handler[strcspn(handler, "\n\r")] = 0;
                        if (strlen(handler) > 0 && strcmp(handler, "NOT_FOUND") != 0) {
                            /* Execute handler - quote ONLY the binary path part */
                            char *quoted_handler = NULL;
                            char *space = strchr(handler, ' ');
                            if (space) {
                                *space = '\0';
                                asprintf(&quoted_handler, "'%s/%s' %s", project_root, handler, space + 1);
                                *space = ' '; // Restore
                            } else {
                                asprintf(&quoted_handler, "'%s/%s'", project_root, handler);
                            }
                            
                            if (quoted_handler) {
                                log_debug("Executing hotkey %d: %s", method_key, quoted_handler);
                                FILE *hpf = popen(quoted_handler, "r");
                                if (hpf) {
                                    if (fgets(method_response_str, sizeof(method_response_str), hpf)) {
                                        method_response_str[strcspn(method_response_str, "\n\r")] = 0;
                                    }
                                    pclose(hpf);
                                }
                                free(quoted_handler);
                            }
                            tick_clock_if_manual();
                            /* Log response with irregular verb conjugation */
                            char resp_key[64];
                            if (strcmp(target_method, "feed") == 0) strcpy(resp_key, "fed");
                            else if (strcmp(target_method, "sleep") == 0) strcpy(resp_key, "slept");
                            else {
                                strcpy(resp_key, target_method);
                                int len = strlen(resp_key);
                                if (len > 0 && resp_key[len-1] == 'y') {
                                    resp_key[len-1] = 'i';
                                    strcat(resp_key, "iied");
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
    
    if (key == 'w' || key == 'a' || key == 's' || key == 'd' || (key >= 1000 && key <= 1003) || (key >= 2100 && key <= 2103)) {
        char* trait = build_path_malloc("pieces/apps/playrm/ops/+x/move_entity.+x");
        char dir_str[16];
        if (key == 'w' || key == 1002 || key == 2102) strcpy(dir_str, "up");
        else if (key == 's' || key == 1003 || key == 2103) strcpy(dir_str, "down");
        else if (key == 'a' || key == 1000 || key == 2100) strcpy(dir_str, "left");
        else if (key == 'd' || key == 1001 || key == 2101) strcpy(dir_str, "right");
        
#ifndef _WIN32
        pid_t p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
            execl(trait, trait, active_target_id, dir_str, NULL); exit(1);
        } else waitpid(p, NULL, 0);
#else
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "\"%s\" %s %s", trait, active_target_id, dir_str);
        system(cmd);
#endif
        free(trait);

        tick_clock_if_manual();
        
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
#ifndef _WIN32
        p = fork();
        if (p == 0) {
            freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr);
            execl(zombie_cmd, zombie_cmd, "zombie_01", active_target_id, NULL); exit(1);
        } else waitpid(p, NULL, 0);
#else
        char cmd_z[4096];
        snprintf(cmd_z, sizeof(cmd_z), "\"%s\" zombie_01 %s", zombie_cmd, active_target_id);
        system(cmd_z);
#endif
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
        sync_focus();
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) { fseek(hf, last_pos, SEEK_SET); int key; while (fscanf(hf, "%d", &key) == 1) route_input(key); last_pos = ftell(hf); fclose(hf); }
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
        sync_focus();
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
                        } else if (kpress) {
                            int key_code = atoi(kpress + 12);
                            if (key_code > 0) route_input(key_code);
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
