/*
 * decrement_counter.+x - Test Op: Decrement counter by 1
 * Usage: ./decrement_counter.+x <project_path>
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

    /* Read current counter value */
    FILE *fp = fopen(state_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot read %s\n", state_path);
        return 1;
    }

    int counter = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "counter=", 8) == 0) {
            counter = atoi(line + 8);
            break;
        }
    }
    fclose(fp);

    /* Decrement counter */
    counter--;

    /* Write updated state (non-destructive - preserve other fields) */
    fp = fopen(state_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot reopen %s\n", state_path);
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
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "counter=", 8) == 0) {
            fprintf(out, "counter=%d\n", counter);
            wrote_counter = 1;
        } else {
            fputs(line, out);
        }
    }
    if (!wrote_counter) {
        fprintf(out, "counter=%d\n", counter);
    }
    fclose(fp);
    fclose(out);

    rename(temp_path, state_path);

    printf("Counter decremented to %d\n", counter);
    return 0;
}
