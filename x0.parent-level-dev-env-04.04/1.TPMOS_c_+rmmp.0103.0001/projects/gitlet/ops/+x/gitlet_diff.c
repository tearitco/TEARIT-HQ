/*
 * gitlet_diff.c - Show diff between working tree and last commit
 * Usage: gitlet_diff.+x <project-hash> [filename]
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_CONTENT 65536

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

static char* read_file_contents(const char *path, long *out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
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

static void diff_files(const char *old_path, const char *new_path, const char *filename) {
    long old_size = 0, new_size = 0;
    char *old_content = read_file_contents(old_path, &old_size);
    char *new_content = read_file_contents(new_path, &new_size);

    if (!old_content && !new_content) return;

    printf("--- a/%s\n", filename);
    printf("+++ b/%s\n", filename);

    if (!old_content) {
        printf("@@ -0,0 +1 @@\n");
        printf("+%s\n", new_content ? new_content : "(empty)");
    } else if (!new_content) {
        printf("@@ -1 +0,0 @@\n");
        printf("-%s\n", old_content);
    } else {
        if (strcmp(old_content, new_content) != 0) {
            printf("@@ -1 +1 @@\n");
            char *old_line = strtok(old_content, "\n");
            char *new_line = strtok(new_content, "\n");
            while (old_line || new_line) {
                if (!old_line) {
                    printf("+%s\n", new_line);
                    new_line = strtok(NULL, "\n");
                } else if (!new_line) {
                    printf("-%s\n", old_line);
                    old_line = strtok(NULL, "\n");
                } else if (strcmp(old_line, new_line) != 0) {
                    printf("-%s\n", old_line);
                    printf("+%s\n", new_line);
                    old_line = strtok(NULL, "\n");
                    new_line = strtok(NULL, "\n");
                } else {
                    old_line = strtok(NULL, "\n");
                    new_line = strtok(NULL, "\n");
                }
            }
        }
    }

    if (old_content) free(old_content);
    if (new_content) free(new_content);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: gitlet_diff.+x <project-hash> [filename]\n");
        return 1;
    }

    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    if (strlen(project_hash) == 0) { fprintf(stderr, "Usage: %s [project-hash]\n  Or create .gitlet-hash file.\n", argv[0]); return 1; }
    resolve_gitlet_root(project_hash);

    /* Get last commit */
    char commit_hash[MAX_PATH_LEN];
    get_current_commit(commit_hash, sizeof(commit_hash));

    if (strlen(commit_hash) == 0) {
        printf("No commits yet. Showing all files as new.\n");
        DIR *d = opendir(".");
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (argc >= 3 && strcmp(entry->d_name, argv[2]) != 0) continue;
                printf("--- /dev/null\n");
                printf("+++ b/%s\n", entry->d_name);
                printf("@@ -0,0 +1 @@\n");
                printf("+[new file]\n\n");
            }
            closedir(d);
        }
        return 0;
    }

    /* Get tree from commit */
    char commit_path[MAX_PATH_LEN];
    snprintf(commit_path, sizeof(commit_path), "%s/objects/%s", GITLET_ROOT, commit_hash);
    FILE *f = fopen(commit_path, "r");
    if (!f) { printf("Commit not found.\n"); return 1; }

    char line[1024];
    char tree_hash[9] = {0};
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "tree ", 5) == 0) {
            char *t = line + 5;
            t[strcspn(t, "\n")] = '\0';
            strncpy(tree_hash, t, sizeof(tree_hash) - 1);
        }
    }
    fclose(f);

    if (strlen(tree_hash) == 0) return 1;

    /* Read tree */
    char tree_path[MAX_PATH_LEN];
    snprintf(tree_path, sizeof(tree_path), "%s/objects/%s", GITLET_ROOT, tree_hash);
    f = fopen(tree_path, "r");
    if (!f) return 1;

    char tree_content[MAX_CONTENT] = {0};
    while (fgets(line, sizeof(line), f)) {
        strcat(tree_content, line);
    }
    fclose(f);

    /* Diff each file in tree */
    char *entry = strtok(tree_content, "\n");
    while (entry) {
        char filename[256], file_hash[16];
        if (sscanf(entry, "%s %s", filename, file_hash) == 2) {
            if (argc >= 3 && strcmp(filename, argv[2]) != 0) {
                entry = strtok(NULL, "\n");
                continue;
            }

            char blob_path[MAX_PATH_LEN];
            snprintf(blob_path, sizeof(blob_path), "%s/objects/%s", GITLET_ROOT, file_hash);
            diff_files(blob_path, filename, filename);
            printf("\n");
        }
        entry = strtok(NULL, "\n");
    }

    return 0;
}
