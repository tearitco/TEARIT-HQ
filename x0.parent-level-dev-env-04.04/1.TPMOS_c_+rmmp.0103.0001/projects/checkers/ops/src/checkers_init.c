#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *v = eq + 1;
                    v[strcspn(v, "\n\r")] = 0;
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

int main(void) {
    resolve_paths();
    char cmd[MAX_PATH * 2];

    for (int i = 0; i < 32; i++) {
        char piece_id[64];
        snprintf(piece_id, sizeof(piece_id), "q_%02d", i);
        char *piece_dir = NULL;
        asprintf(&piece_dir, "%s/projects/checkers/pieces/board/%s", project_root, piece_id);
        
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", piece_dir);
        system(cmd);

        char *state_path = NULL;
        asprintf(&state_path, "%s/state.txt", piece_dir);
        FILE *f = fopen(state_path, "w");
        if (f) {
            fprintf(f, "piece_id=%s\n", piece_id);
            fprintf(f, "type=checker\n");
            
            int color = 0; // 0=empty, 1=red, -1=black
            if (i < 12) color = -1;
            else if (i >= 20) color = 1;
            
            fprintf(f, "color=%d\n", color);
            fprintf(f, "is_king=0\n");
            fprintf(f, "icon=%s\n", (color == 1) ? "r" : (color == -1) ? "b" : ".");
            fclose(f);
        }
        free(piece_dir);
        free(state_path);
    }

    printf("Checkers board initialized (32 pieces).\n");
    return 0;
}
