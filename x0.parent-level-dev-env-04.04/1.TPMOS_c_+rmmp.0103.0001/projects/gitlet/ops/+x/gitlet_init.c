/*
 * gitlet_init.c - Initialize a gitlet repository
 * Usage: gitlet_init.+x [project-hash]
 * If no hash given, reads .gitlet-hash from current directory.
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096

static char GITLET_ROOT[MAX_PATH_LEN] = {0};

static void resolve_gitlet_root(const char *project_hash) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(GITLET_ROOT, sizeof(GITLET_ROOT), "%s/gitlet/%s/.gitlet", home, project_hash);
}

static void ensure_dirs() {
    char parent[MAX_PATH_LEN];
    char *last_slash = strrchr(GITLET_ROOT, '/');
    if (last_slash) {
        size_t len = last_slash - GITLET_ROOT;
        if (len < sizeof(parent)) {
            strncpy(parent, GITLET_ROOT, len);
            parent[len] = '\0';
            mkdir(parent, 0777);
        }
    }
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/objects", GITLET_ROOT); mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/refs", GITLET_ROOT); mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/refs/heads", GITLET_ROOT); mkdir(path, 0777);
}

static void resolve_project_hash(int argc, char *argv[], char *out_hash, size_t out_size) {
    if (argc >= 2 && strlen(argv[1]) > 0) {
        strncpy(out_hash, argv[1], out_size - 1);
        return;
    }
    /* Auto-detect from .gitlet-hash */
    FILE *f = fopen(".gitlet-hash", "r");
    if (f) {
        if (fgets(out_hash, out_size, f)) {
            out_hash[strcspn(out_hash, "\n")] = '\0';
        }
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    
    if (strlen(project_hash) == 0) {
        fprintf(stderr, "Usage: gitlet_init.+x [project-hash]\n");
        fprintf(stderr, "  Or create .gitlet-hash file in project root.\n");
        return 1;
    }

    resolve_gitlet_root(project_hash);

    /* Create directory structure recursively */
    char path[MAX_PATH_LEN];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(path, sizeof(path), "%s/gitlet", home);
    mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/gitlet/%s", home, project_hash);
    mkdir(path, 0777);
    snprintf(path, sizeof(path), "%s/gitlet/%s/.gitlet", home, project_hash);
    mkdir(path, 0777);

    ensure_dirs();

    /* Create HEAD */
    char head_path[MAX_PATH_LEN];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", GITLET_ROOT);
    FILE *f = fopen(head_path, "w");
    if (!f) { perror("fopen HEAD"); return 1; }
    fprintf(f, "ref: refs/heads/master\n");
    fclose(f);

    /* Create master ref (empty = no commits yet) */
    char master_path[MAX_PATH_LEN];
    snprintf(master_path, sizeof(master_path), "%s/refs/heads/master", GITLET_ROOT);
    f = fopen(master_path, "w");
    if (!f) { perror("fopen master"); return 1; }
    fprintf(f, "\n");
    fclose(f);

    /* Create index (empty staging area) */
    char index_path[MAX_PATH_LEN];
    snprintf(index_path, sizeof(index_path), "%s/index", GITLET_ROOT);
    f = fopen(index_path, "w");
    if (!f) { perror("fopen index"); return 1; }
    fclose(f);

    printf("Initialized gitlet repository: ~/gitlet/%s/.gitlet/\n", project_hash);
    return 0;
}
