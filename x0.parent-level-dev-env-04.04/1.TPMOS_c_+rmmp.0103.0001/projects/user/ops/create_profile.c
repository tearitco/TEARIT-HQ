/*
 * create_profile.+x - User Op: Create a new user profile
 * Usage: ./create_profile.+x <project_path> <username>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define PROFILES_DIR "/pieces/profiles/"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <project_path> <username>\n", argv[0]);
        return 1;
    }

    const char *project_path = argv[1];
    const char *username = argv[2];

    /* Validate username (alphanumeric only) */
    for (int i = 0; username[i]; i++) {
        if (!isalnum((unsigned char)username[i]) && username[i] != '_') {
            fprintf(stderr, "Error: Invalid username (alphanumeric and _ only)\n");
            return 1;
        }
    }

    /* Create profile directory */
    char *profile_path = NULL;
    asprintf(&profile_path, "%s%s/%s", project_path, PROFILES_DIR, username);
    
    if (mkdir(profile_path, 0755) != 0) {
        /* Check if already exists */
        struct stat st;
        if (stat(profile_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("Profile '%s' already exists\n", username);
            free(profile_path);
            return 0;
        }
        fprintf(stderr, "Error: Cannot create profile directory\n");
        free(profile_path);
        return 1;
    }

    /* Create initial state file */
    char *state_path = NULL;
    asprintf(&state_path, "%s/state.txt", profile_path);
    
    FILE *fp = fopen(state_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create state file\n");
        free(state_path);
        free(profile_path);
        return 1;
    }

    fprintf(fp, "username=%s\n", username);
    fprintf(fp, "created_at=now\n");
    fprintf(fp, "session_count=0\n");
    fprintf(fp, "last_login=never\n");
    fclose(fp);

    printf("Profile '%s' created successfully\n", username);
    free(state_path);
    free(profile_path);
    return 0;
}
