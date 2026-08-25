/*
 * get_session.+x - User Op: Get current session data
 * Usage: ./get_session.+x <project_path> <username>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 512
#define SESSIONS_DIR "/pieces/session/"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <project_path> <username>\n", argv[0]);
        return 1;
    }

    const char *project_path = argv[1];
    const char *username = argv[2];

    /* Read session file */
    char session_path[MAX_PATH];
    snprintf(session_path, sizeof(session_path), "%s%s/%s.session", project_path, SESSIONS_DIR, username);
    
    FILE *fp = fopen(session_path, "r");
    if (!fp) {
        fprintf(stderr, "NOT_AUTHENTICATED: No active session for '%s'\n", username);
        return 1;
    }

    printf("Session data for '%s':\n", username);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("  %s", line);
    }
    fclose(fp);

    return 0;
}
