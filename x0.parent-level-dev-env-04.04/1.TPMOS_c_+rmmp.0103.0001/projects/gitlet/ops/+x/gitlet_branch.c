/*
 * gitlet_branch.c - Create or list branches
 * Usage: gitlet_branch.+x <project-hash> [branch-name]
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
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

static void get_current_branch(char *branch, size_t size) {
    char head_path[MAX_PATH_LEN];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", GITLET_ROOT);
    FILE *f = fopen(head_path, "r");
    if (!f) { strncpy(branch, "master", size - 1); return; }
    char line[1024];
    if (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ref: refs/heads/", 16) == 0) {
            char *b = line + 16;
            b[strcspn(b, "\n")] = '\0';
            strncpy(branch, b, size - 1);
        } else {
            strncpy(branch, "master", size - 1);
        }
    } else {
        strncpy(branch, "master", size - 1);
    }
    fclose(f);
}

static void get_current_commit(char *commit, size_t size) {
    char branch[256];
    get_current_branch(branch, sizeof(branch));
    char ref_path[MAX_PATH_LEN];
    snprintf(ref_path, sizeof(ref_path), "%s/refs/heads/%s", GITLET_ROOT, branch);
    FILE *f = fopen(ref_path, "r");
    if (!f) { commit[0] = '\0'; return; }
    if (fgets(commit, size, f)) {
        commit[strcspn(commit, "\n")] = '\0';
    } else {
        commit[0] = '\0';
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    if (strlen(project_hash) == 0) { fprintf(stderr, "Usage: %s [project-hash] [branch-name]\n  Or create .gitlet-hash file.\n", argv[0]); return 1; }
    resolve_gitlet_root(project_hash);

    if (argc < 3) {
        /* List branches */
        char refs_path[MAX_PATH_LEN];
        snprintf(refs_path, sizeof(refs_path), "%s/refs/heads", GITLET_ROOT);
        DIR *d = opendir(refs_path);
        if (!d) { printf("No branches found.\n"); return 0; }

        char current_branch[256];
        get_current_branch(current_branch, sizeof(current_branch));

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, current_branch) == 0) {
                printf("* %s\n", entry->d_name);
            } else {
                printf("  %s\n", entry->d_name);
            }
        }
        closedir(d);
    } else {
        /* Create branch */
        const char *branch_name = argv[2];
        char commit_hash[MAX_PATH_LEN];
        get_current_commit(commit_hash, sizeof(commit_hash));

        if (strlen(commit_hash) == 0) {
            fprintf(stderr, "Cannot create branch: no commits yet.\n");
            return 1;
        }

        char branch_path[MAX_PATH_LEN];
        snprintf(branch_path, sizeof(branch_path), "%s/refs/heads/%s", GITLET_ROOT, branch_name);
        FILE *f = fopen(branch_path, "w");
        if (!f) { perror("fopen branch"); return 1; }
        fprintf(f, "%s\n", commit_hash);
        fclose(f);
        printf("Created branch '%s' at %s\n", branch_name, commit_hash);
    }

    return 0;
}
