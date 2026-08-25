/*
 * gitlet_status.c - Show working tree status
 * Usage: gitlet_status.+x <project-hash>
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

int main(int argc, char *argv[]) {
    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    if (strlen(project_hash) == 0) { fprintf(stderr, "Usage: %s [project-hash]\n  Or create .gitlet-hash file.\n", argv[0]); return 1; }
    resolve_gitlet_root(project_hash);

    char branch[256];
    get_current_branch(branch, sizeof(branch));
    printf("On branch %s\n\n", branch);

    /* Show staged files */
    char index_path[MAX_PATH_LEN];
    snprintf(index_path, sizeof(index_path), "%s/index", GITLET_ROOT);
    FILE *f = fopen(index_path, "r");
    if (f) {
        char line[1024];
        int has_staged = 0;
        while (fgets(line, sizeof(line), f)) {
            char fname[256];
            if (sscanf(line, "%s", fname) == 1) {
                if (!has_staged) { printf("Changes to be committed:\n"); has_staged = 1; }
                printf("\tnew file:   %s\n", fname);
            }
        }
        fclose(f);
        if (has_staged) printf("\n");
    }

    /* Show untracked files */
    printf("Untracked files:\n");
    DIR *d = opendir(".");
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            /* Check if in index */
            FILE *idx = fopen(index_path, "r");
            int found = 0;
            if (idx) {
                char line[1024];
                while (fgets(line, sizeof(line), idx)) {
                    char fname[256];
                    if (sscanf(line, "%s", fname) == 1 && strcmp(fname, entry->d_name) == 0) {
                        found = 1; break;
                    }
                }
                fclose(idx);
            }
            if (!found) printf("\t%s\n", entry->d_name);
        }
        closedir(d);
    }

    return 0;
}
