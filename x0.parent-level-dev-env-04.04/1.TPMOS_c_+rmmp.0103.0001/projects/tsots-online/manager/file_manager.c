#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_SLOTS 5
#define MAX_VARS 100

char project_root[MAX_PATH] = ".";
char project_id[64] = "fuzz-op";
char active_tsots_online[64] = "unknown";
char active_slot[64] = "default";
char input_file_name[256] = "";
char file_response[256] = "Ready";

typedef struct {
    char name[64];
    int exists;
} Slot;
Slot slots[MAX_SLOTS];
int num_found_slots = 0;

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                if (strlen(v) > 0) snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
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

void scan_saves() {
    char path[MAX_PATH];
    if (strcmp(active_tsots_online, "unknown") == 0) return;
    
    snprintf(path, sizeof(path), "%s/projects/tsots_online/pieces/profiles/%s/saves/%s", project_root, active_tsots_online, project_id);
    
    DIR *dir = opendir(path);
    num_found_slots = 0;
    for(int i=0; i<MAX_SLOTS; i++) { slots[i].exists = 0; strcpy(slots[i].name, ""); }
    
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && num_found_slots < MAX_SLOTS) {
            if (entry->d_name[0] == '.') continue;
            strncpy(slots[num_found_slots].name, entry->d_name, 63);
            slots[num_found_slots].exists = 1;
            num_found_slots++;
        }
        closedir(dir);
    }
}

void load_context() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_id") == 0 && strlen(v) > 0) strncpy(project_id, v, 63);
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/projects/tsots_online/pieces/session", project_root);
    DIR *dir = opendir(path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".session") == 0) {
                strncpy(active_tsots_online, entry->d_name, dot - entry->d_name);
                active_tsots_online[dot - entry->d_name] = '\0';
                break;
            }
        }
        closedir(dir);
    }
    scan_saves();
}

void sync_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    char lines[MAX_VARS][MAX_LINE];
    int lc = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(lines[lc], MAX_LINE, f) && lc < MAX_VARS-20) {
            if (strncmp(lines[lc], "project_id=", 11) == 0) continue;
            if (strncmp(lines[lc], "current_project=", 16) == 0) continue;
            if (strncmp(lines[lc], "active_tsots_online=", 12) == 0) continue;
            if (strncmp(lines[lc], "active_slot=", 12) == 0) continue;
            if (strncmp(lines[lc], "input_file_name=", 16) == 0) continue;
            if (strncmp(lines[lc], "file_response=", 14) == 0) continue;
            if (strncmp(lines[lc], "slot_", 5) == 0) continue;
            lc++;
        }
        fclose(f);
    }
    
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < lc; i++) fputs(lines[i], f);
        fprintf(f, "project_id=%s\n", project_id);
        fprintf(f, "current_project=%s\n", project_id);
        fprintf(f, "active_tsots_online=%s\n", active_tsots_online);
        fprintf(f, "active_slot=%s\n", active_slot);
        fprintf(f, "input_file_name=%s\n", input_file_name);
        fprintf(f, "file_response=%s\n", file_response);
        for(int i=0; i<MAX_SLOTS; i++) {
            fprintf(f, "slot_%d_name=%s\n", i, slots[i].name);
            fprintf(f, "slot_%d_exists=%d\n", i, slots[i].exists);
        }
        fclose(f);
    }
    
    char pulse[MAX_PATH];
    snprintf(pulse, sizeof(pulse), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    f = fopen(pulse, "a");
    if (f) { fprintf(f, "S\n"); fclose(f); }
}

void read_input_buffer() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "file_name=", 10) == 0) {
                strncpy(input_file_name, trim_str(line + 10), 255);
                break;
            }
        }
        fclose(f);
    }
}

void run_op(const char *cmd, const char *slot) {
    char shell_cmd[MAX_PATH];
    snprintf(shell_cmd, sizeof(shell_cmd), "'%s/projects/tsots_online/ops/+x/file_op.+x' '%s' '%s' '%s'", project_root, cmd, project_id, slot);
    system(shell_cmd);
    if (slot && strcmp(cmd, "delete") != 0) strncpy(active_slot, slot, 63);
    
    snprintf(file_response, sizeof(file_response), "%s %s OK", cmd, slot);
    scan_saves();
    sync_state();
}

void handle_command(const char* cmd) {
    read_input_buffer();
    if (strcmp(cmd, "FILE:NEW") == 0) run_op("new", "default");
    else if (strcmp(cmd, "FILE:SAVE") == 0) run_op("save", active_slot);
    else if (strcmp(cmd, "FILE:SAVE_AS") == 0) {
        if (strlen(input_file_name) > 0) run_op("save", input_file_name);
        else strcpy(file_response, "Error: Name empty");
    }
    else if (strcmp(cmd, "FILE:LOAD") == 0) run_op("load", active_slot);
    else if (strcmp(cmd, "FILE:DELETE") == 0) run_op("delete", active_slot);
    else if (strncmp(cmd, "FILE:SELECT:", 12) == 0) {
        strncpy(active_slot, cmd + 12, 63);
        strncpy(input_file_name, active_slot, 255);
        snprintf(file_response, sizeof(file_response), "Slot Selected: %s", active_slot);
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "file_name=%s\n", active_slot); fclose(f); }
    }
    else if (strcmp(cmd, "FILE:BACK") == 0) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/pieces/display/layout_changed.txt", project_root);
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "projects/%s/layouts/%s.chtpm\n", project_id, project_id); fclose(f); }
        exit(0);
    }
    sync_state();
}

void route_input(int key) {
    /* Map numeric keys to string commands */
    if (key == '1') handle_command("FILE:NEW");
    else if (key == '2') handle_command("FILE:SAVE");
    else if (key == '3') handle_command("FILE:SAVE_AS");
    else if (key == '4') handle_command("FILE:LOAD");
    else if (key == '5') handle_command("FILE:DELETE");
    else if (key == '7') handle_command("FILE:BACK");
    else if (key == '8' || key == '9') {
        int idx = (key == '8') ? 0 : 1;
        if (idx < num_found_slots && slots[idx].exists) {
            char cmd[128]; snprintf(cmd, sizeof(cmd), "FILE:SELECT:%s", slots[idx].name);
            handle_command(cmd);
        }
    }
}

int main() {
    resolve_root(); load_context(); sync_state();
    char history_path[MAX_PATH];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    long last_pos = 0; struct stat st;
    if (stat(history_path, &st) == 0) last_pos = st.st_size;
    while (1) {
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET); char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd_pos = strstr(line, "COMMAND: ");
                        if (cmd_pos) handle_command(trim_str(cmd_pos + 9));
                        else {
                            int k; if (sscanf(line, "%d", &k) == 1) route_input(k);
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return 0;
}
