/*
 * reset_counter.+x - Test Op: Reset counter to 0
 * Usage: ./reset_counter.+x <project_path>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 512
#define STATE_FILE "/pieces/state.txt"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <project_path>\n", argv[0]);
        return 1;
    }

    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s%s", argv[1], STATE_FILE);

    /* Read and update state (non-destructive - preserve other fields) */
    FILE *fp = fopen(state_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot read %s\n", state_path);
        return 1;
    }

    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", state_path);
    FILE *out = fopen(temp_path, "w");
    if (!out) {
        fclose(fp);
        fprintf(stderr, "Error: Cannot write temp file\n");
        return 1;
    }

    int wrote_counter = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "counter=", 8) == 0) {
            fprintf(out, "counter=0\n");
            wrote_counter = 1;
        } else {
            fputs(line, out);
        }
    }
    if (!wrote_counter) {
        fprintf(out, "counter=0\n");
    }
    fclose(fp);
    fclose(out);

    rename(temp_path, state_path);

    printf("Counter reset to 0\n");
    return 0;
}
