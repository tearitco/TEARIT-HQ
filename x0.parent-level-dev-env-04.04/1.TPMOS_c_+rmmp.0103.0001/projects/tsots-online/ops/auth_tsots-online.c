/*
 * auth_tsots_online.+x - tsots_online Op: Authenticate a tsots_online session
 * Usage: ./auth_tsots_online.+x <project_path> <tsots_onlinename>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_PATH 512
#define PROFILES_DIR "/pieces/profiles/"
#define SESSIONS_DIR "/pieces/session/"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <project_path> <tsots_onlinename>\n", argv[0]);
        return 1;
    }

    const char *project_path = argv[1];
    const char *tsots_onlinename = argv[2];

    /* Check if profile exists */
    char profile_path[MAX_PATH];
    snprintf(profile_path, sizeof(profile_path), "%s%s/%s/state.txt", project_path, PROFILES_DIR, tsots_onlinename);
    
    FILE *fp = fopen(profile_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Profile '%s' not found\n", tsots_onlinename);
        return 1;
    }
    fclose(fp);

    /* Create session file */
    char session_path[MAX_PATH];
    snprintf(session_path, sizeof(session_path), "%s%s/%s.session", project_path, SESSIONS_DIR, tsots_onlinename);
    
    fp = fopen(session_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create session file\n");
        return 1;
    }

    time_t now = time(NULL);
    fprintf(fp, "tsots_onlinename=%s\n", tsots_onlinename);
    fprintf(fp, "login_time=%ld\n", (long)now);
    fprintf(fp, "status=active\n");
    fclose(fp);

    /* Update profile session count */
    fp = fopen(profile_path, "r");
    if (!fp) return 1;

    int session_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "session_count=", 14) == 0) {
            session_count = atoi(line + 14);
            break;
        }
    }
    fclose(fp);
    session_count++;

    /* Write updated count */
    fp = fopen(profile_path, "r");
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", profile_path);
    FILE *out = fopen(temp_path, "w");
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "session_count=", 14) == 0) {
            fprintf(out, "session_count=%d\n", session_count);
        } else {
            fputs(line, out);
        }
    }
    fclose(fp);
    fclose(out);
    rename(temp_path, profile_path);

    printf("tsots_online '%s' authenticated (session #%d)\n", tsots_onlinename, session_count);
    return 0;
}
