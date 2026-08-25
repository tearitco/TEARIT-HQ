/*
 * get_status.+x - Test Op: Get status message
 * Usage: ./get_status.+x <project_path>
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

    /* Read status value */
    FILE *fp = fopen(state_path, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot read %s\n", state_path);
        return 1;
    }

    char status[256] = "UNKNOWN";
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "status=", 7) == 0) {
            /* Remove newline */
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            strncpy(status, line + 7, sizeof(status) - 1);
            status[sizeof(status) - 1] = '\0';
            break;
        }
    }
    fclose(fp);

    printf("Status: %s\n", status);
    return 0;
}
