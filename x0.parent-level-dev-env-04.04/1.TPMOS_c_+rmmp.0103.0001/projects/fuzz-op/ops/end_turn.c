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
    
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "%s/pieces/system/clock_daemon/plugins/+x/clock_daemon.+x increment-hour", project_root);
    system(cmd);
    
    // Log response
    char resp_path[MAX_PATH];
    snprintf(resp_path, sizeof(resp_path), "%s/projects/fuzz-op/pieces/fuzzpet/last_response.txt", project_root);
    FILE *f = fopen(resp_path, "w");
    if (f) {
        fprintf(f, "Turn Ended (+1h)");
        fclose(f);
    }
    
    printf("End Turn: Time advanced by 1 hour\n");
    return 0;
}
