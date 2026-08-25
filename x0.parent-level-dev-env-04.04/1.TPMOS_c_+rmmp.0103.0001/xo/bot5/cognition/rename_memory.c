/*
 * rename_memory.c - XO Bot Memory Labeling Utility
 * Purpose: Allows users to rename timestamped memory folders to logical goals.
 * Usage: ./rename_memory.+x <project_root> <timestamp_or_old_name> <new_name>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE 1024

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <project_root> <old_name> <new_name>\n", argv[0]);
        return 1;
    }

    char *project_root = argv[1];
    char *old_name = argv[2];
    char *new_name = argv[3];

    char *old_path, *new_path;
    asprintf(&old_path, "%s/xo/bot5/pieces/memories/%s", project_root, old_name);
    asprintf(&new_path, "%s/xo/bot5/pieces/memories/%s", project_root, new_name);

    // 1. Verify existence
    struct stat st;
    if (stat(old_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Source directory %s does not exist.\n", old_path);
        return 1;
    }

    // 2. Perform Rename
    printf("[XO-MEM] Renaming %s to %s...\n", old_name, new_name);
    char *cmd;
    asprintf(&cmd, "mv %s %s", old_path, new_path);
    
    if (run_command(cmd) == 0) {
        printf("✓ Rename successful.\n");
        
        // 3. Update Registry (Optional mapping file for logical lookup)
        char *map_path;
        asprintf(&map_path, "%s/xo/bot5/pieces/memory_map.kvp", project_root);
        FILE *f = fopen(map_path, "a");
        if (f) {
            fprintf(f, "%s=%s\n", new_name, new_path);
            fclose(f);
        }
        free(map_path);
    } else {
        fprintf(stderr, "✗ Rename FAILED.\n");
    }

    free(cmd);
    free(old_path);
    free(new_path);

    return 0;
}
