/*
 * gitlet_commit.c - Create a commit from staged files
 * Usage: gitlet_commit.+x <project-hash> "commit message"
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

static void hash_contents(const char *data, long long size, char *hash_output) {
    unsigned long hash = 5381;
    const unsigned char *str = (const unsigned char *)data;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    hash = hash ^ (unsigned long)size;
    char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    hash_output[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        hash_output[i] = charset[hash % 62];
        hash >>= 5;
    }
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
    const char *message = NULL;
    
    if (argc >= 3) {
        strncpy(project_hash, argv[1], sizeof(project_hash) - 1);
        message = argv[2];
    } else if (argc == 2) {
        resolve_project_hash(0, NULL, project_hash, sizeof(project_hash));
        message = argv[1];
    }
    
    if (strlen(project_hash) == 0) {
        fprintf(stderr, "Usage: gitlet_commit.+x [project-hash] \"message\"\n");
        fprintf(stderr, "  Or create .gitlet-hash file in project root.\n");
        return 1;
    }
    if (!message) {
        fprintf(stderr, "Usage: gitlet_commit.+x [project-hash] \"message\"\n");
        return 1;
    }
    
    resolve_gitlet_root(project_hash);

    /* Read index */
    char index_path[MAX_PATH_LEN];
    snprintf(index_path, sizeof(index_path), "%s/index", GITLET_ROOT);
    FILE *f = fopen(index_path, "r");
    if (!f) { fprintf(stderr, "Nothing staged.\n"); return 1; }

    char tree_content[MAX_CONTENT] = {0};
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        strcat(tree_content, line);
    }
    fclose(f);

    if (strlen(tree_content) == 0) {
        fprintf(stderr, "Nothing staged.\n");
        return 1;
    }

    /* Hash tree */
    char tree_hash[9];
    hash_contents(tree_content, strlen(tree_content), tree_hash);

    /* Store tree */
    char tree_path[MAX_PATH_LEN];
    snprintf(tree_path, sizeof(tree_path), "%s/objects/%s", GITLET_ROOT, tree_hash);
    f = fopen(tree_path, "w");
    if (!f) { perror("fopen tree"); return 1; }
    fwrite(tree_content, 1, strlen(tree_content), f);
    fclose(f);

    /* Get parent commit */
    char parent_hash[MAX_PATH_LEN] = {0};
    get_current_commit(parent_hash, sizeof(parent_hash));

    /* Create commit object */
    char commit_content[MAX_CONTENT];
    time_t now = time(NULL);
    snprintf(commit_content, sizeof(commit_content),
             "tree %s\nparent %s\nDate: %s\n%s\n",
             tree_hash, parent_hash, ctime(&now), message);

    char commit_hash[9];
    hash_contents(commit_content, strlen(commit_content), commit_hash);

    char commit_path[MAX_PATH_LEN];
    snprintf(commit_path, sizeof(commit_path), "%s/objects/%s", GITLET_ROOT, commit_hash);
    f = fopen(commit_path, "w");
    if (!f) { perror("fopen commit"); return 1; }
    fwrite(commit_content, 1, strlen(commit_content), f);
    fclose(f);

    /* Update branch ref */
    char branch[256];
    get_current_branch(branch, sizeof(branch));
    char ref_path[MAX_PATH_LEN];
    snprintf(ref_path, sizeof(ref_path), "%s/refs/heads/%s", GITLET_ROOT, branch);
    f = fopen(ref_path, "w");
    if (!f) { perror("fopen ref"); return 1; }
    fprintf(f, "%s\n", commit_hash);
    fclose(f);

    /* Clear index */
    f = fopen(index_path, "w");
    if (f) fclose(f);

    printf("[%s %s] %s\n", branch, commit_hash, message);
    return 0;
}
