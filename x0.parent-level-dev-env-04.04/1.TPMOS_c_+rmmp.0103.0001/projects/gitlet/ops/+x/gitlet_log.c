/*
 * gitlet_log.c - Show commit history
 * Usage: gitlet_log.+x <project-hash>
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096

static void resolve_project_hash(int argc, char *argv[], char *out_hash, size_t out_size) {
    if (argc >= 2 && strlen(argv[1]) > 0) {
        strncpy(out_hash, argv[1], out_size - 1);
        return;
    }
    FILE *f = fopen(".gitlet-hash", "r");
    if (f) {
        if (fgets(out_hash, out_size, f)) {
            out_hash[strcspn(out_hash, "\n")] = '\0';
        }
        fclose(f);
    }
}

static char GITLET_ROOT[MAX_PATH_LEN] = {0};

static void resolve_gitlet_root(const char *project_hash) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(GITLET_ROOT, sizeof(GITLET_ROOT), "%s/gitlet/%s/.gitlet", home, project_hash);
}

static void get_current_commit(char *commit, size_t size) {
    char head_path[MAX_PATH_LEN];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", GITLET_ROOT);
    FILE *f = fopen(head_path, "r");
    if (!f) { commit[0] = '\0'; return; }
    char line[1024];
    if (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ref: refs/heads/", 16) == 0) {
            char *b = line + 16;
            b[strcspn(b, "\n")] = '\0';
            char ref_path[MAX_PATH_LEN];
            snprintf(ref_path, sizeof(ref_path), "%s/refs/heads/%s", GITLET_ROOT, b);
            fclose(f);
            f = fopen(ref_path, "r");
            if (f) {
                if (fgets(commit, size, f)) {
                    commit[strcspn(commit, "\n")] = '\0';
                } else {
                    commit[0] = '\0';
                }
                fclose(f);
            } else {
                commit[0] = '\0';
            }
        } else {
            commit[0] = '\0';
            fclose(f);
        }
    } else {
        commit[0] = '\0';
        fclose(f);
    }
}

static void print_commit(const char *commit_hash) {
    if (!commit_hash || strlen(commit_hash) == 0) return;

    char commit_path[MAX_PATH_LEN];
    snprintf(commit_path, sizeof(commit_path), "%s/objects/%s", GITLET_ROOT, commit_hash);
    FILE *f = fopen(commit_path, "r");
    if (!f) return;

    char line[1024];
    char parent_hash[MAX_PATH_LEN] = {0};
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "parent ", 7) == 0) {
            char *p = line + 7;
            p[strcspn(p, "\n")] = '\0';
            strncpy(parent_hash, p, sizeof(parent_hash) - 1);
        }
        printf("%s", line);
    }
    fclose(f);
    printf("\n");

    if (strlen(parent_hash) > 0) {
        print_commit(parent_hash);
    }
}

int main(int argc, char *argv[]) {
    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    if (strlen(project_hash) == 0) { fprintf(stderr, "Usage: %s [project-hash]\n  Or create .gitlet-hash file.\n", argv[0]); return 1; }
    resolve_gitlet_root(project_hash);

    char commit_hash[MAX_PATH_LEN];
    get_current_commit(commit_hash, sizeof(commit_hash));

    if (strlen(commit_hash) == 0) {
        printf("No commits yet.\n");
        return 0;
    }

    print_commit(commit_hash);
    return 0;
}
