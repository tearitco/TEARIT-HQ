#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

static int walk_dir(FILE *out, const char *path, int depth, int max_depth) {
    DIR *dir = opendir(path);
    struct dirent *ent = NULL;
    int dir_count = 0;
    int file_count = 0;

    if (!dir) {
        fprintf(out, "ERROR|depth=%d|path=%s|errno=%d|msg=%s\n", depth, path, errno, strerror(errno));
        return -1;
    }

    fprintf(out, "DIR|depth=%d|path=%s\n", depth, path);
    while ((ent = readdir(dir)) != NULL) {
        char child[MAX_PATH];
        struct stat st;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        if (stat(child, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            dir_count++;
            if (max_depth < 0 || depth + 1 <= max_depth) {
                walk_dir(out, child, depth + 1, max_depth);
            } else {
                fprintf(out, "DIR|depth=%d|path=%s|note=depth_limit\n", depth + 1, child);
            }
        } else {
            file_count++;
            fprintf(out, "FILE|depth=%d|path=%s|size=%lld\n", depth + 1, child, (long long)st.st_size);
        }
    }

    closedir(dir);
    fprintf(out, "SUMMARY|depth=%d|path=%s|dirs=%d|files=%d\n", depth, path, dir_count, file_count);
    return 0;
}

int main(int argc, char **argv) {
    const char *project_root = NULL;
    const char *start_rel = "";
    const char *out_path = NULL;
    int max_depth = -1;
    FILE *out = stdout;
    char start_path[MAX_PATH];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <project_root> [start_rel] [max_depth] [out_file]\n", argv[0]);
        return 1;
    }

    project_root = argv[1];
    if (argc > 2) start_rel = argv[2];
    if (argc > 3) max_depth = atoi(argv[3]);
    if (argc > 4) out_path = argv[4];

    if (out_path && out_path[0]) {
        out = fopen(out_path, "w");
        if (!out) {
            perror("traverse_dirs fopen");
            return 1;
        }
    }

    if (start_rel[0]) snprintf(start_path, sizeof(start_path), "%s/%s", project_root, start_rel);
    else snprintf(start_path, sizeof(start_path), "%s", project_root);

    walk_dir(out, start_path, 0, max_depth);

    if (out != stdout) fclose(out);
    return 0;
}
