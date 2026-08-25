#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdarg.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ENTITIES 64
#define MAX_VAR_VALUE 65536

// TPM XO-PET Manager (Legacy PoC, split controller/world roots)
char control_root[MAX_PATH] = ".";
char world_root[MAX_PATH] = ".";
char project_id[64] = "xo-pet-v1";
char active_target_id[64] = "xolector";
int sim_active = 0;
int step_pending = 0;
int step_count = 0;
char last_response[256] = "System Initialized.";
char turn_summary[MAX_VAR_VALUE] = "";
char nav_buffer[32] = "";
pthread_mutex_t control_lock = PTHREAD_MUTEX_INITIALIZER;

// Simulation Paths (Relative to system_root)
char world_path[MAX_PATH];
char state_path_world[MAX_PATH];
char log_dir[MAX_PATH];
char xelector_pdl_path[MAX_PATH];

char* trim_str(char *str);
static int is_switcher_target(const char *id) {
    return id && (strcmp(id, "xolector") == 0 || strcmp(id, "xelector") == 0);
}

static void write_gui_state(void);
static void set_sim_active(int value);
static void request_single_step(void);
static int resolve_method_path_from_pdl(const char* pdl_path, const char* method, char* out_path, size_t out_sz);

static void build_piece_pdl_path(const char* piece_id, char* out_path, size_t out_sz) {
    snprintf(out_path, out_sz, "%s/%s/piece.pdl", world_root, piece_id);
}

static void write_color_kvp(FILE *f, const char *key, const char *value) {
    fprintf(f, "%s=%s\n", key, value ? value : "");
}

static void append_button_markup(char *dst, size_t dst_sz, const char *label, const char *on_click, const char *fg, const char *bg) {
    char chunk[1024];
    if (fg && fg[0] && bg && bg[0]) {
        snprintf(chunk, sizeof(chunk), "<button label=\"%s\" onClick=\"%s\" fg=\"%s\" bg=\"%s\" /> ", label, on_click, fg, bg);
    } else if (fg && fg[0]) {
        snprintf(chunk, sizeof(chunk), "<button label=\"%s\" onClick=\"%s\" fg=\"%s\" /> ", label, on_click, fg);
    } else if (bg && bg[0]) {
        snprintf(chunk, sizeof(chunk), "<button label=\"%s\" onClick=\"%s\" bg=\"%s\" /> ", label, on_click, bg);
    } else {
        snprintf(chunk, sizeof(chunk), "<button label=\"%s\" onClick=\"%s\" /> ", label, on_click);
    }
    strncat(dst, chunk, dst_sz - strlen(dst) - 1);
}

static void write_session_state(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/xo-pet-v1/session/state.txt", control_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=%s\n", project_id);
    fprintf(f, "active_target_id=%s\n", active_target_id);
    fprintf(f, "sim_active=%d\n", sim_active);
    fprintf(f, "step_pending=%d\n", step_pending);
    fprintf(f, "step_count=%d\n", step_count);
    fprintf(f, "epoch=%d\n", step_count + 1);
    fprintf(f, "last_response=%s\n", last_response);
    fprintf(f, "turn_summary=%s\n", turn_summary);
    fclose(f);
}

static void write_epoch_snapshot(int epoch) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/xo-pet-v1/session/frame_%04d.txt", control_root, epoch);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "project_id=%s\n", project_id);
    fprintf(f, "epoch=%d\n", epoch);
    fprintf(f, "step_count=%d\n", step_count);
    fprintf(f, "active_target_id=%s\n", active_target_id);
    fprintf(f, "sim_active=%d\n", sim_active);
    fprintf(f, "last_response=%s\n", last_response);
    fprintf(f, "turn_summary=%s\n", turn_summary);
    fclose(f);
}

static int read_methods_from_pdl(const char* pdl_path, char* out_methods, size_t out_sz, int skip_possess) {
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;

    out_methods[0] = '\0';
    int count = 0;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;

        char *pipe1 = strchr(line, '|');
        if (!pipe1) continue;
        char *pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) continue;

        char parsed_event[64];
        size_t event_len = (size_t)(pipe2 - pipe1 - 1);
        if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
        strncpy(parsed_event, pipe1 + 1, event_len);
        parsed_event[event_len] = '\0';
        char *method = trim_str(parsed_event);

        if (skip_possess && strncmp(method, "possess_", 8) == 0) continue;

        if (count > 0) {
            strncat(out_methods, "\n", out_sz - strlen(out_methods) - 1);
        }
        strncat(out_methods, method, out_sz - strlen(out_methods) - 1);
        count++;
    }

    fclose(f);
    return count;
}

static void pulse_frame_markers(void) {
    const char *marker_paths[] = {
        "pieces/display/frame_changed.txt",
        "pieces/chtpm/frame_buffer/frame_changed.txt",
        NULL
    };

    for (int i = 0; marker_paths[i] != NULL; i++) {
        FILE *mf = fopen(marker_paths[i], "a");
        if (mf) {
            fputs("M\n", mf);
            fclose(mf);
        }
    }
}

static void reset_turn_summary(void) {
    turn_summary[0] = '\0';
}

static void append_turn_summary(const char *fmt, ...) {
    char chunk[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(chunk, sizeof(chunk), fmt, ap);
    va_end(ap);

    if (turn_summary[0] != '\0') {
        strncat(turn_summary, " | ", sizeof(turn_summary) - strlen(turn_summary) - 1);
    }
    strncat(turn_summary, chunk, sizeof(turn_summary) - strlen(turn_summary) - 1);
}

static void clear_nav_buffer(void) {
    nav_buffer[0] = '\0';
}

static void append_nav_digit(int digit) {
    size_t len = strlen(nav_buffer);
    if (len + 1 >= sizeof(nav_buffer)) return;
    nav_buffer[len] = (char)('0' + digit);
    nav_buffer[len + 1] = '\0';
}

static int consume_nav_buffer(void) {
    if (nav_buffer[0] == '\0') return 0;
    int idx = atoi(nav_buffer);
    clear_nav_buffer();
    return idx;
}

static void handle_committed_selection(int idx) {
    if (idx == 1) {
        set_sim_active(1);
        snprintf(last_response, sizeof(last_response), "Simulation resumed.");
        write_gui_state();
        return;
    }
    if (idx == 2) {
        set_sim_active(0);
        pthread_mutex_lock(&control_lock);
        step_pending = 0;
        pthread_mutex_unlock(&control_lock);
        snprintf(last_response, sizeof(last_response), "Simulation paused.");
        write_gui_state();
        return;
    }
    if (idx == 3) {
        request_single_step();
        snprintf(last_response, sizeof(last_response), "Step requested.");
        write_gui_state();
        return;
    }

    char method_name[64] = "";
    char pdl_path[MAX_PATH];
    if (is_switcher_target(active_target_id)) {
        strncpy(pdl_path, xelector_pdl_path, sizeof(pdl_path) - 1);
        pdl_path[sizeof(pdl_path) - 1] = '\0';
    } else {
        snprintf(pdl_path, sizeof(pdl_path), "%s/%s/%s/piece.pdl", world_root, world_path, active_target_id);
    }
    char methods[MAX_VAR_VALUE] = "";
    if (read_methods_from_pdl(pdl_path, methods, sizeof(methods), 0) > 0) {
        char *cursor = methods;
        int current = 2;
        while (cursor && *cursor) {
            char *next = strchr(cursor, '\n');
            if (next) *next = '\0';
            if (current == idx) {
                strncpy(method_name, cursor, sizeof(method_name) - 1);
                method_name[sizeof(method_name) - 1] = '\0';
                break;
            }
            cursor = next ? next + 1 : NULL;
            current++;
        }
    }

    if (strlen(method_name) > 0) {
        if (strcmp(method_name, "release") == 0) {
            strncpy(active_target_id, "xolector", sizeof(active_target_id) - 1);
            active_target_id[sizeof(active_target_id) - 1] = '\0';
            snprintf(last_response, sizeof(last_response), "Returned to xolector.");
            write_gui_state();
            return;
        }

        if (strcmp(method_name, "ai") == 0) {
            if (!sim_active) request_single_step();
            snprintf(last_response, sizeof(last_response), "AI turn requested.");
            write_gui_state();
            return;
        }

        char op_rel_path[MAX_PATH] = "";
        if (resolve_method_path_from_pdl(pdl_path, method_name, op_rel_path, sizeof(op_rel_path)) == 0) {
            char full_op[MAX_PATH * 2];
            if (is_switcher_target(active_target_id)) {
                snprintf(full_op, sizeof(full_op), "%s/%s", control_root, op_rel_path);
                char controller_cwd[MAX_PATH * 2];
                snprintf(controller_cwd, sizeof(controller_cwd), "%s/projects/%s", control_root, project_id);
                char cmd[MAX_PATH * 4];
                snprintf(cmd, sizeof(cmd), "cd '%s' && '%s'", controller_cwd, full_op);
                system(cmd);
            } else {
                snprintf(full_op, sizeof(full_op), "%s/%s", world_root, op_rel_path);
                system(full_op);
            }
            snprintf(last_response, sizeof(last_response), "Executed %s.", method_name);
        }
    }
    write_gui_state();
}

static int resolve_method_path_from_pdl(const char* pdl_path, const char* method, char* out_path, size_t out_sz) {
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;

        char *pipe1 = strchr(line, '|');
        char *pipe2 = pipe1 ? strchr(pipe1 + 1, '|') : NULL;
        if (!pipe1 || !pipe2) continue;

        char parsed_event[64];
        size_t event_len = (size_t)(pipe2 - pipe1 - 1);
        if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
        strncpy(parsed_event, pipe1 + 1, event_len);
        parsed_event[event_len] = '\0';
        char *method_name = trim_str(parsed_event);

        if (strcmp(method_name, method) == 0) {
            char parsed_handler[MAX_PATH];
            strncpy(parsed_handler, pipe2 + 1, sizeof(parsed_handler) - 1);
            parsed_handler[sizeof(parsed_handler) - 1] = '\0';
            char *handler = trim_str(parsed_handler);
            strncpy(out_path, handler, out_sz - 1);
            out_path[out_sz - 1] = '\0';
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return -1;
}

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
    if (!getcwd(control_root, sizeof(control_root))) strcpy(control_root, ".");

    // Prefer the executable path because the controller can be launched
    // from multiple directories during testing.
    char exe_path[MAX_PATH];
    ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (exe_len > 0) {
        exe_path[exe_len] = '\0';
        char *marker = strstr(exe_path, "/projects/xo-pet-v1/pieces/manager/");
        if (marker) {
            *marker = '\0';
            strncpy(control_root, exe_path, MAX_PATH - 1);
            control_root[MAX_PATH - 1] = '\0';
        }
    }
    
    // Resolve via location_kvp if available (Standard TPMOS)
    // Check multiple relative paths based on common execution contexts
    char *kvp_attempts[] = {
        "pieces/locations/location_kvp",
        "../pieces/locations/location_kvp",
        "../../pieces/locations/location_kvp",
        "../../../pieces/locations/location_kvp",
        "../../../../x0.moke-pet-project-2.2/x0.5-liz.fiter4-mew-00.02/pieces/locations/location_kvp",
        "../../../../x0.moke-pet-project-2.2/x0.5-liz.fiter4-mew-00.02/pieces/buttons/shared/pieces/locations/location_kvp"
    };
    
    FILE *kvp = NULL;
    for (int i = 0; i < 4; i++) {
        kvp = fopen(kvp_attempts[i], "r");
        if (kvp) break;
    }

    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(trim_str(line), "project_root") == 0) {
                    strncpy(control_root, trim_str(eq + 1), MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }

    strncpy(world_root, control_root, MAX_PATH - 1);
    world_root[MAX_PATH - 1] = '\0';
    for (int up = 0; up < 4; up++) {
        char probe[MAX_PATH];
        snprintf(probe, sizeof(probe), "%s/pieces/world_tank_01/map_enclosure", world_root);
        if (access(probe, F_OK) == 0) break;
        char *slash = strrchr(world_root, '/');
        if (!slash) break;
        *slash = '\0';
    }
    
    // Construct internal paths (bundle-level tank enclosure)
    snprintf(world_path, MAX_PATH, "pieces/world_tank_01/map_enclosure");
    snprintf(state_path_world, MAX_PATH, "pieces/world_tank_01/state.txt");
    snprintf(log_dir, MAX_PATH, "pieces/world_tank_01/logs");
    snprintf(xelector_pdl_path, MAX_PATH, "%s/projects/xo-pet-v1/pieces/xelector/piece.pdl", control_root);
    
    printf("[Manager] Control Root: %s\n", control_root);
    printf("[Manager] World Root: %s\n", world_root);
}

// --- Simulation Logic ---

int get_epoch() {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", world_root, state_path_world);
    FILE* f = fopen(full_path, "r");
    if (!f) return 1;
    char line[MAX_LINE];
    int epoch = 1;
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "epoch=", 6) == 0) {
            epoch = atoi(line + 6);
            break;
        }
    }
    fclose(f);
    return epoch;
}

void increment_epoch(int current) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", world_root, state_path_world);
    FILE* f = fopen(full_path, "w");
    if (f) {
        fprintf(f, "epoch=%d\nstatus=active\n", current + 1);
        fclose(f);
    }
}

void log_event(int epoch, const char* entity, const char* action, const char* target) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s/epoch_%d.txt", world_root, log_dir, epoch);
    FILE* f = fopen(full_path, "a");
    if (f) {
        time_t now; time(&now); char* ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s] %s | %s | %s\n", ts, entity, action, target ? target : "NONE");
        fclose(f);
    }
}

int get_type(const char* id, char* type) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s/%s/state.txt", world_root, world_path, id);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, f)) {
        if (strncmp(line, "type | ", 7) == 0) {
            strncpy(type, line + 7, 63);
            type[strcspn(type, "\n\r")] = 0;
            fclose(f); return 1;
        }
    }
    fclose(f); return 0;
}

void resolve_op_path(const char* entity, const char* method, char* out_path) {
    char pdl_path[MAX_PATH];
    snprintf(pdl_path, MAX_PATH, "%s/%s/%s/piece.pdl", world_root, world_path, entity);
    if (resolve_method_path_from_pdl(pdl_path, method, out_path, MAX_PATH) == 0) return;

    if (is_switcher_target(entity)) {
        if (resolve_method_path_from_pdl(xelector_pdl_path, method, out_path, MAX_PATH) == 0) return;
    }
}

void dispatch_sim_op(const char* entity, const char* method, const char* arg1, const char* arg2) {
    char op_rel_path[MAX_PATH] = {0};
    resolve_op_path(entity, method, op_rel_path);
    if (strlen(op_rel_path) == 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        char full_op[MAX_PATH];
        snprintf(full_op, MAX_PATH, "%s/%s", world_root, op_rel_path);
        execl(full_op, full_op, arg1, arg2, NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void run_simulation_epoch() {
    int epoch = get_epoch();
    reset_turn_summary();
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s/%s", world_root, world_path);
    DIR* d = opendir(search_path);
    if (!d) return;

    char lizard_ids[MAX_ENTITIES][64];
    int lizard_count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) && lizard_count < MAX_ENTITIES) {
        if (entry->d_name[0] == '.') continue;
        char type[64];
        if (get_type(entry->d_name, type) && strcmp(type, "lizard") == 0) {
            strncpy(lizard_ids[lizard_count++], entry->d_name, 63);
        }
    }
    closedir(d);

    for (int i = 0; i < lizard_count; i++) {
        const char* liz = lizard_ids[i];
        if (strcmp(liz, active_target_id) == 0) {
            append_turn_summary("%s acts: breathe()", liz);
            dispatch_sim_op(liz, "breathe", NULL, NULL);
            append_turn_summary("%s acts: check_death()", liz);
            dispatch_sim_op(liz, "check_death", NULL, NULL);
            continue;
        }
        append_turn_summary("%s acts: breathe()", liz);
        dispatch_sim_op(liz, "breathe", NULL, NULL);
        append_turn_summary("%s acts: scan()", liz);
        dispatch_sim_op(liz, "scan", NULL, NULL);
        
        char obs_path[MAX_PATH];
        snprintf(obs_path, MAX_PATH, "%s/%s/%s/memory/observations.txt", world_root, world_path, liz);
        FILE* f = fopen(obs_path, "r");
        if (!f) {
            append_turn_summary("%s acts: rest()", liz);
            dispatch_sim_op(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
            append_turn_summary("%s acts: check_death()", liz);
            dispatch_sim_op(liz, "check_death", NULL, NULL);
            continue;
        }

        char target_food[64] = "";
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, f)) {
            char* pipe = strchr(line, '|');
            if (pipe) {
                *pipe = '\0';
                char* id = trim_str(line);
                char* type = trim_str(pipe + 1);
                if (strstr(type, "food")) {
                    strncpy(target_food, id, 63); break;
                }
            }
        }
        fclose(f);

        if (strlen(target_food) > 0) {
            append_turn_summary("%s acts: eat(%s)", liz, target_food);
            dispatch_sim_op(liz, "eat", target_food, NULL);
            log_event(epoch, liz, "eat", target_food);
        } else {
            append_turn_summary("%s acts: rest()", liz);
            dispatch_sim_op(liz, "rest", NULL, NULL);
            log_event(epoch, liz, "rest", NULL);
        }
        append_turn_summary("%s acts: check_death()", liz);
        dispatch_sim_op(liz, "check_death", NULL, NULL);
    }

    char epoch_log[MAX_PATH];
    snprintf(epoch_log, MAX_PATH, "pieces/world_tank_01/logs/epoch_%d.txt", epoch);
    for (int i = 0; i < lizard_count; i++) {
        append_turn_summary("%s acts: train(%s, %s)", lizard_ids[i], epoch_log, lizard_ids[i]);
        dispatch_sim_op(lizard_ids[i], "train", epoch_log, lizard_ids[i]);
    }
    increment_epoch(epoch);
    step_count++;
    write_epoch_snapshot(epoch);
    snprintf(last_response, sizeof(last_response), "Epoch %d Complete.", epoch);
    if (turn_summary[0] == '\0') {
        snprintf(turn_summary, sizeof(turn_summary), "No entity actions recorded.");
    }
}

// --- GUI & Input Logic ---

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/xo-pet-v1/pieces/manager/gui_state.txt", control_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/xo-pet-v1/pieces/manager/+x/xo-pet-v1_manager.+x\n");
        fprintf(f, "app_title=XO-PET V1 (PoC)\n");
        write_color_kvp(f, "theme_title_fg", "cyan");
        write_color_kvp(f, "theme_title_bg", "");
        write_color_kvp(f, "theme_header_fg", "yellow");
        write_color_kvp(f, "theme_header_bg", "");
        write_color_kvp(f, "theme_text_fg", "white");
        write_color_kvp(f, "theme_text_bg", "");
        write_color_kvp(f, "theme_value_fg", "green");
        write_color_kvp(f, "theme_value_bg", "");
        write_color_kvp(f, "theme_button_fg", "black");
        write_color_kvp(f, "theme_button_bg", "cyan");
        write_color_kvp(f, "theme_button_muted_fg", "black");
        write_color_kvp(f, "theme_button_muted_bg", "yellow");
        write_color_kvp(f, "theme_status_fg", "white");
        write_color_kvp(f, "theme_status_bg", sim_active ? "green" : "red");
        fprintf(f, "active_target_id=%s\n", active_target_id);
        fprintf(f, "active_target=%s\n", is_switcher_target(active_target_id) ? "xolector" : active_target_id);
        fprintf(f, "sim_active=%d\n", sim_active);
        fprintf(f, "sim_status=%s\n", sim_active ? "RUNNING" : "PAUSED");
        fprintf(f, "last_response=%s\n", last_response);
        fprintf(f, "turn_summary=%s\n", turn_summary);
        fprintf(f, "epoch=%d\n", get_epoch());
        fprintf(f, "step_count=%d\n", step_count);
        
        if (!is_switcher_target(active_target_id)) {
            char stats_path[MAX_PATH];
            snprintf(stats_path, sizeof(stats_path), "%s/%s/%s/memory/stats.txt", world_root, world_path, active_target_id);
            FILE *sf = fopen(stats_path, "r");
            if (sf) {
                char line[MAX_LINE];
                while (fgets(line, MAX_LINE, sf)) {
                    if (strncmp(line, "hp=", 3) == 0) fprintf(f, "pet_hp=%s\n", trim_str(line + 3));
                    if (strncmp(line, "hunger=", 7) == 0) fprintf(f, "pet_hunger=%s\n", trim_str(line + 7));
                }
                fclose(sf);
            }
        } else {
             fprintf(f, "pet_hp=N/A\npet_hunger=N/A\n");
        }

        fprintf(f, "project_controls=");
        char project_controls[MAX_VAR_VALUE] = "";
        if (sim_active) {
            append_button_markup(project_controls, sizeof(project_controls), "[ START ]", "COMMAND: START", "${theme_button_muted_fg}", "${theme_button_muted_bg}");
            append_button_markup(project_controls, sizeof(project_controls), "[ PAUSE ]", "COMMAND: PAUSE", "${theme_button_fg}", "${theme_button_bg}");
        } else {
            append_button_markup(project_controls, sizeof(project_controls), "[ START ]", "COMMAND: START", "${theme_button_fg}", "${theme_button_bg}");
            append_button_markup(project_controls, sizeof(project_controls), "[ PAUSE ]", "COMMAND: PAUSE", "${theme_button_muted_fg}", "${theme_button_muted_bg}");
        }
        append_button_markup(project_controls, sizeof(project_controls), "[ STEP / END TURN ]", "COMMAND: STEP", "${theme_button_fg}", "${theme_button_bg}");
        fprintf(f, "%s", project_controls);
        fprintf(f, "\n");

        // 1. Build Pet List
        char pet_list[MAX_VAR_VALUE] = "";
        char search_path[MAX_PATH];
        snprintf(search_path, MAX_PATH, "%s/%s", world_root, world_path);
        DIR* d = opendir(search_path);
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d))) {
                if (entry->d_name[0] == '.') continue;
                char type[64];
                if (get_type(entry->d_name, type) && strcmp(type, "lizard") == 0) {
                    char on_click[128];
                    char label[256];
                    snprintf(on_click, sizeof(on_click), "SET_POSSESS:%s", entry->d_name);
                    snprintf(label, sizeof(label), "Possess %s", entry->d_name);
                    append_button_markup(pet_list, sizeof(pet_list), label, on_click, "${theme_button_fg}", "${theme_button_bg}");
                }
            }
            closedir(d);
        }
        fprintf(f, "pet_list=%s\n", pet_list);

        // 2. Build Methods
        char methods_raw[MAX_VAR_VALUE] = "";
        char pdl_path[MAX_PATH];
        if (is_switcher_target(active_target_id)) {
            strncpy(pdl_path, xelector_pdl_path, sizeof(pdl_path) - 1);
            pdl_path[sizeof(pdl_path) - 1] = '\0';
        } else {
            snprintf(pdl_path, sizeof(pdl_path), "%s/%s/%s/piece.pdl", world_root, world_path, active_target_id);
        }

        int method_count = read_methods_from_pdl(pdl_path, methods_raw, sizeof(methods_raw), 0);
        fprintf(f, "piece_methods=");
        if (method_count > 0) {
            char methods_markup[MAX_VAR_VALUE] = "";
            int idx = 2;
            char *cursor = methods_raw;
            while (cursor && *cursor) {
                char *next = strchr(cursor, '\n');
                if (next) *next = '\0';
                if (*cursor) {
                    char on_click[64];
                    char label[256];
                    snprintf(on_click, sizeof(on_click), "KEY:%d", idx++);
                    snprintf(label, sizeof(label), "%s", cursor);
                    append_button_markup(methods_markup, sizeof(methods_markup), label, on_click, "${theme_button_fg}", "${theme_button_bg}");
                }
                cursor = next ? next + 1 : NULL;
            }
            fprintf(f, "%s", methods_markup);
        }
        fprintf(f, "\n");
        fprintf(f, "turn_summary=%s\n", turn_summary);
        fclose(f);
    }
    write_session_state();
    pulse_frame_markers();
}

static void set_sim_active(int value) {
    pthread_mutex_lock(&control_lock);
    sim_active = value;
    pthread_mutex_unlock(&control_lock);
}

static void request_single_step(void) {
    pthread_mutex_lock(&control_lock);
    step_pending = 1;
    pthread_mutex_unlock(&control_lock);
}

static int consume_step_request(int *was_continuous) {
    int should_run = 0;
    int continuous = 0;
    pthread_mutex_lock(&control_lock);
    if (sim_active || step_pending) {
        should_run = 1;
        continuous = sim_active;
        if (step_pending) step_pending = 0;
    }
    pthread_mutex_unlock(&control_lock);
    if (was_continuous) *was_continuous = continuous;
    return should_run;
}

void route_input(int key) {
    if (key == 13 || key == '\n') {
        int idx = consume_nav_buffer();
        if (idx > 0) {
            handle_committed_selection(idx);
        }
        return;
    }

    if (key >= '0' && key <= '9') {
        append_nav_digit(key - '0');
        snprintf(last_response, sizeof(last_response), "Nav buffered: %s", nav_buffer);
        write_gui_state();
        return;
    }

    // Check internal state updates from Ops
    char pos_path[MAX_PATH], sim_path[MAX_PATH];
    snprintf(pos_path, MAX_PATH, "%s/projects/xo-pet-v1/pieces/manager/active_target.txt", control_root);
    FILE *f = fopen(pos_path, "r");
    if (f) {
        char next_target[64];
        if (fscanf(f, "%63s", next_target) == 1) {
            if (strcmp(next_target, "xelector") == 0) {
                strncpy(active_target_id, "xolector", sizeof(active_target_id) - 1);
            } else {
                strncpy(active_target_id, next_target, sizeof(active_target_id) - 1);
            }
            active_target_id[sizeof(active_target_id) - 1] = '\0';
        }
        fclose(f);
        remove(pos_path);
    }

    snprintf(sim_path, MAX_PATH, "%s/projects/xo-pet-v1/pieces/manager/sim_control.txt", control_root);
    f = fopen(sim_path, "r");
    if (f) { int next_sim = 0; fscanf(f, "%d", &next_sim); fclose(f); remove(sim_path); set_sim_active(next_sim); }

    write_gui_state();
}

void* simulation_thread(void* arg) {
    (void)arg;
    while (1) {
        int continuous = 0;
        if (consume_step_request(&continuous)) {
            run_simulation_epoch();
            write_gui_state();
            if (continuous) {
                usleep(180000);
            }
            continue;
        }
        usleep(50000);
    }
    return NULL;
}

void* input_thread(void* arg) {
    char kb_path[MAX_PATH];
    char proj_hist_path[MAX_PATH];
    char player_hist_path[MAX_PATH];
    snprintf(proj_hist_path, sizeof(proj_hist_path), "%s/projects/xo-pet-v1/history.txt", control_root);
    snprintf(player_hist_path, sizeof(player_hist_path), "%s/pieces/apps/player_app/history.txt", control_root);
    snprintf(kb_path, sizeof(kb_path), "%s/pieces/keyboard/history.txt", control_root);

    struct stat st;
    long last_pos[3] = {0, 0, 0};
    while (1) {
        const char *paths[] = { proj_hist_path, player_hist_path, kb_path };
        for (int i = 0; i < 3; i++) {
            if (stat(paths[i], &st) != 0) continue;
            if (st.st_size < last_pos[i]) last_pos[i] = 0;
            if (st.st_size <= last_pos[i]) continue;

            FILE *f = fopen(paths[i], "r");
            if (!f) continue;
            fseek(f, last_pos[i], SEEK_SET);
            char line[MAX_LINE];
            while (fgets(line, MAX_LINE, f)) {
                if (strstr(line, "KEY_PRESSED:")) {
                    int key;
                    if (sscanf(line, "[%*[^]]] KEY_PRESSED: %d", &key) == 1) {
                        route_input(key);
                        snprintf(last_response, sizeof(last_response), "Input consumed: %d", key);
                        write_gui_state();
                    }
                } else if (strstr(line, "COMMAND: START")) {
                    set_sim_active(1);
                    snprintf(last_response, sizeof(last_response), "Simulation resumed.");
                    write_gui_state();
                } else if (strstr(line, "COMMAND: PAUSE")) {
                    set_sim_active(0);
                    pthread_mutex_lock(&control_lock);
                    step_pending = 0;
                    pthread_mutex_unlock(&control_lock);
                    snprintf(last_response, sizeof(last_response), "Simulation paused.");
                    write_gui_state();
                } else if (strstr(line, "COMMAND: STEP")) {
                    request_single_step();
                    snprintf(last_response, sizeof(last_response), "Step requested.");
                    write_gui_state();
                } else if (strstr(line, "COMMAND: SET_POSSESS:")) {
                    char *val = strstr(line, "SET_POSSESS:") + 12;
                    char next_target[64];
                    strncpy(next_target, trim_str(val), sizeof(next_target) - 1);
                    next_target[sizeof(next_target) - 1] = '\0';
                    if (strcmp(next_target, "xelector") == 0) {
                        strncpy(active_target_id, "xolector", sizeof(active_target_id) - 1);
                    } else {
                        strncpy(active_target_id, next_target, sizeof(active_target_id) - 1);
                    }
                    active_target_id[sizeof(active_target_id) - 1] = '\0';
                    snprintf(last_response, sizeof(last_response), "Possessed %s.", active_target_id);
                    write_gui_state();
                } else {
                    char *end = NULL;
                    long raw_key = strtol(trim_str(line), &end, 10);
                    if (end != trim_str(line) && raw_key > 0) {
                        route_input((int)raw_key);
                        snprintf(last_response, sizeof(last_response), "Input consumed: %ld", raw_key);
                        write_gui_state();
                    }
                }
            }
            last_pos[i] = ftell(f);
            fclose(f);
        }
        usleep(16667);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    resolve_paths();
    write_gui_state();
    pthread_t t;
    pthread_create(&t, NULL, input_thread, NULL);
    pthread_t sim_t;
    pthread_create(&sim_t, NULL, simulation_thread, NULL);
    printf("=== XO-PET V1 Manager Active ===\n");
    while (1) sleep(1);
    return 0;
}
