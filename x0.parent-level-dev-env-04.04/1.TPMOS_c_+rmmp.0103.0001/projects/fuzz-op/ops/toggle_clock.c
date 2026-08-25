#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

int main() {
    resolve_root();
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/clock_daemon/state.txt", project_root);
    
    char turn[64] = "0", time_str[64] = "00:00:00", mode[64] = "auto";
    FILE *f = fopen(state_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "mode") == 0) strncpy(mode, eq + 1, 63);
            }
        }
        fclose(f);
    }
    
    const char *new_mode = (strcmp(mode, "auto") == 0) ? "manual" : "auto";
    
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "%s/pieces/system/clock_daemon/plugins/+x/clock_daemon.+x set-mode %s", project_root, new_mode);
    system(cmd);
    
    // Log response for fuzz-op manager to display
    char resp_path[MAX_PATH];
    snprintf(resp_path, sizeof(resp_path), "%s/projects/fuzz-op/pieces/fuzzpet/last_response.txt", project_root);
    f = fopen(resp_path, "w");
    if (f) {
        fprintf(f, "Clock: %s", new_mode[0] == 'a' ? "Auto" : "Manual");
        fclose(f);
    }
    
    printf("Clock mode toggled to %s\n", new_mode);
    return 0;
}
