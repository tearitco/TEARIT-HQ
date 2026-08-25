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

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_shutdown = 0;

/* Game State */
int current_player_idx = 0; // 0=Alpha, 1=Bravo, 2=Charlie, 3=Delta
const char *player_names[] = {"alpha", "bravo", "charlie", "delta"};
int actions_remaining = 1;
char last_log[MAX_LINE] = "Game Started.";

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
    if (getcwd(project_root, sizeof(project_root))) {
        return;
    }
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) kvp = fopen("../../../pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    strncpy(project_root, v, MAX_PATH - 1);
                }
            }
        }
        fclose(kvp);
    }
}

int get_piece_int(const char* piece_id, const char* key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/aow-2d/pieces/%s/state.txt", project_root, piece_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int val = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq && strncmp(line, key, strlen(key)) == 0) {
            val = atoi(eq + 1);
            break;
        }
    }
    fclose(f);
    return val;
}

void set_piece_int(const char* piece_id, const char* key, int val) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/aow-2d/pieces/%s/state.txt", project_root, piece_id);
    char temp[MAX_PATH];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    FILE *f = fopen(path, "r");
    FILE *tf = fopen(temp, "w");
    if (!tf) return;
    int found = 0;
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq && strncmp(line, key, strlen(key)) == 0) {
                fprintf(tf, "%s=%d\n", key, val);
                found = 1;
            } else fputs(line, tf);
        }
        fclose(f);
    }
    if (!found) fprintf(tf, "%s=%d\n", key, val);
    fclose(tf);
    rename(temp, path);
}

void trigger_render(int z_level) {
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "%s/projects/aow-2d/ops/+x/render_2d_map.+x %d", project_root, z_level);
    system(cmd);

    char view_path[MAX_PATH];
    snprintf(view_path, sizeof(view_path), "%s/projects/aow-2d/manager/map_view.txt", project_root);
    FILE *vf = fopen(view_path, "r");
    if (vf) {
        char state_path[MAX_PATH];
        snprintf(state_path, sizeof(state_path), "%s/projects/aow-2d/manager/state.txt", project_root);
        FILE *sf = fopen(state_path, "a");
        if (sf) {
            fprintf(sf, "map_view=");
            char line[MAX_LINE * 4];
            while (fgets(line, sizeof(line), vf)) {
                // Remove newline and replace with literal \n for parser if needed, 
                // but TPMOS parser often handles multi-line if properly formatted.
                // For simple substitution, we'll try literal content first.
                fputs(line, sf);
            }
            fclose(sf);
        }
        fclose(vf);
    }
}

void update_manager_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/aow-2d/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");

    char cap_id[64];
    snprintf(cap_id, sizeof(cap_id), "capital_%s", player_names[current_player_idx]);
    int z_level = get_piece_int(cap_id, "z_level");

    if (f) {
        fprintf(f, "active_player=%s\n", player_names[current_player_idx]);
        fprintf(f, "actions_left=%d\n", actions_remaining);
        fprintf(f, "last_log=%s\n", last_log);
        fprintf(f, "z_level=%d\n", z_level);
        // Export player specific info for UI
        fprintf(f, "credits=%d\n", get_piece_int(cap_id, "credits"));
        fprintf(f, "units=%d\n", get_piece_int(cap_id, "units"));
        fclose(f);
    }
    trigger_render(z_level);
}

void end_turn() {
    current_player_idx = (current_player_idx + 1) % 4;
    actions_remaining = 1;
    snprintf(last_log, sizeof(last_log), "Turn passed to %s.", player_names[current_player_idx]);
    update_manager_state();
}

void perform_tax() {
    if (actions_remaining <= 0) return;
    char cap_id[64];
    snprintf(cap_id, sizeof(cap_id), "capital_%s", player_names[current_player_idx]);
    int credits = get_piece_int(cap_id, "credits");
    int units = get_piece_int(cap_id, "units");
    int income = 10 - units;
    if (income < 0) income = 0;
    set_piece_int(cap_id, "credits", credits + income);
    actions_remaining--;
    snprintf(last_log, sizeof(last_log), "%s taxed the land for %d credits.", player_names[current_player_idx], income);
    update_manager_state();
}

int main(void) {
    signal(SIGINT, handle_sigint);
    setpgid(0, 0);
    resolve_paths();
    update_manager_state();

    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0;
    struct stat st;

    // Declare cap_id for use in the main loop
    char cap_id[64];

    while (!g_shutdown) {
        // Ensure cap_id matches the current player at the start of each check
        snprintf(cap_id, sizeof(cap_id), "capital_%s", player_names[current_player_idx]);

        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *f = fopen(hist_path, "r");
            if (f) {
                fseek(f, last_pos, SEEK_SET);
                int key;
                while (fscanf(f, "%d", &key) == 1) {
                    if (key == 't' || key == 'T') perform_tax();
                    else if (key == 'x') { // Cycle Z-level up
                        int current_z = get_piece_int(cap_id, "z_level");
                        set_piece_int(cap_id, "z_level", current_z + 1);
                        snprintf(last_log, sizeof(last_log), "%s changed Z-level to %d.", player_names[current_player_idx], current_z + 1);
                    } else if (key == 'z') { // Cycle Z-level down
                        int current_z = get_piece_int(cap_id, "z_level");
                        if (current_z > 0) set_piece_int(cap_id, "z_level", current_z - 1);
                        else snprintf(last_log, sizeof(last_log), "%s is at the lowest Z-level.", player_names[current_player_idx]);
                    }
                    if (key == 'e' || key == 'E') end_turn();
                }
                last_pos = ftell(f);
                fclose(f);
            }
        }
        usleep(16667);
    }
    return 0;
}
