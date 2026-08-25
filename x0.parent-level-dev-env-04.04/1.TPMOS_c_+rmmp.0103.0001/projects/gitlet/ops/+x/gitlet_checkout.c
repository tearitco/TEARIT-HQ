/*
 * gitlet_checkout.c - Restore files from a commit (TRUNK + DELTA reconstruction)
 * Usage: gitlet_checkout.+x <project-hash> <commit-hash|branch-name>
 * 
 * Reconstructs files by: trunk + all deltas in order
 * STANDALONE — no headers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

/* Apply a delta string to the base content */
static char* apply_delta(const char *base, const char *delta) {
    if (!delta || strlen(delta) == 0) return strdup(base ? base : "");
    if (!base) return strdup(delta);
    
    /* Check if delta has diff markers (-/+) */
    int has_diff_markers = 0;
    const char *p = delta;
    while (*p) {
        if (*p == '\n') p++;
        else if (*p == '-' && *(p+1) && *(p+1) != '\n') { has_diff_markers = 1; break; }
        else if (*p == '+' && *(p+1) && *(p+1) != '\n') { has_diff_markers = 1; break; }
        else { p++; }
    }
    
    if (!has_diff_markers) {
        /* No diff markers - delta is the full replacement */
        return strdup(delta);
    }
    
    /* Apply line-by-line diff */
    char *result = malloc(MAX_CONTENT);
    if (!result) return NULL;
    result[0] = '\0';
    
    /* Simple approach: start with base, remove deleted lines, add inserted lines */
    /* Copy base lines that aren't deleted */
    char *base_copy = strdup(base);
    char *delta_copy = strdup(delta);
    
    /* First pass: collect all lines to keep from base */
    char *base_lines[1000];
    int base_count = 0;
    char *line = strtok(base_copy, "\n");
    while (line && base_count < 999) {
        base_lines[base_count++] = line;
        line = strtok(NULL, "\n");
    }
    
    /* Second pass: mark which base lines to delete */
    int delete[1000] = {0};
    line = strtok(delta_copy, "\n");
    int base_idx = 0;
    while (line) {
        if (line[0] == '-') {
            /* Delete next base line that matches */
            char *del_content = line + 1;
            for (int i = 0; i < base_count; i++) {
                if (!delete[i] && strcmp(base_lines[i], del_content) == 0) {
                    delete[i] = 1;
                    break;
                }
            }
        } else if (line[0] == '+') {
            /* Insert after current base_idx */
            char *add_content = line + 1;
            /* Find insertion point */
            int insert_at = base_idx;
            /* Shift remaining lines and insert */
            if (base_count < 999) {
                for (int i = base_count; i > insert_at; i--) {
                    base_lines[i] = base_lines[i-1];
                    delete[i] = delete[i-1];
                }
                base_lines[insert_at] = strdup(add_content);
                delete[insert_at] = 0;
                base_count++;
            }
        } else {
            /* Context line - advance base_idx */
            base_idx++;
        }
        line = strtok(NULL, "\n");
    }
    
    /* Build result */
    for (int i = 0; i < base_count; i++) {
        if (!delete[i]) {
            if (strlen(result) > 0) strcat(result, "\n");
            strcat(result, base_lines[i]);
        }
    }
    
    free(base_copy);
    free(delta_copy);
    return result;
}

static void restore_from_commit(const char *commit_hash) {
    char commit_path[MAX_PATH_LEN];
    snprintf(commit_path, sizeof(commit_path), "%s/objects/%s", GITLET_ROOT, commit_hash);
    FILE *f = fopen(commit_path, "r");
    if (!f) { fprintf(stderr, "Commit not found: %s\n", commit_hash); return; }

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

    if (strlen(tree_hash) == 0) return;

    /* Read tree to get index snapshot */
    char tree_path[MAX_PATH_LEN];
    snprintf(tree_path, sizeof(tree_path), "%s/objects/%s", GITLET_ROOT, tree_hash);
    f = fopen(tree_path, "r");
    if (!f) return;

    char tree_content[MAX_CONTENT] = {0};
    while (fgets(line, sizeof(line), f)) {
        strcat(tree_content, line);
    }
    fclose(f);

    /* Restore each file from index */
    char *entry = strtok(tree_content, "\n");
    while (entry) {
        char filename[256], file_hash[16], file_type[16], trunk_hash[16];
        trunk_hash[0] = '\0';
        int fields = sscanf(entry, "%s %s %s %s", filename, file_hash, file_type, trunk_hash);
        if (fields >= 2) {
            if (fields < 3) strcpy(file_type, "trunk");
            
            char *content = NULL;
            
            if (strcmp(file_type, "trunk") == 0) {
                /* Read trunk directly */
                char trunk_path[MAX_PATH_LEN];
                snprintf(trunk_path, sizeof(trunk_path), "%s/objects/%s", GITLET_ROOT, file_hash);
                long size;
                content = read_file_contents(trunk_path, &size);
            } else if (strcmp(file_type, "delta") == 0) {
                /* Reconstruct from trunk + delta */
                char *base = NULL;
                
                /* Use trunk_hash from tree entry if available */
                if (strlen(trunk_hash) > 0) {
                    char trunk_path[MAX_PATH_LEN];
                    snprintf(trunk_path, sizeof(trunk_path), "%s/objects/%s", GITLET_ROOT, trunk_hash);
                    long size;
                    base = read_file_contents(trunk_path, &size);
                }
                
                /* Read delta */
                char delta_path[MAX_PATH_LEN];
                snprintf(delta_path, sizeof(delta_path), "%s/deltas/%s", GITLET_ROOT, file_hash);
                long delta_size;
                char *delta = read_file_contents(delta_path, &delta_size);
                
                if (base && delta) {
                    content = apply_delta(base, delta);
                    free(base);
                } else if (delta) {
                    content = strdup(delta);
                } else if (base) {
                    content = base;
                }
                
                if (delta) free(delta);
            }
            
            if (content) {
                /* Create parent directories if needed */
                char *last_slash = strrchr(filename, '/');
                if (last_slash) {
                    char dir[256];
                    size_t len = last_slash - filename;
                    if (len < sizeof(dir)) {
                        strncpy(dir, filename, len);
                        dir[len] = '\0';
                        mkdir(dir, 0777);
                    }
                }
                
                FILE *out = fopen(filename, "w");
                if (out) {
                    fwrite(content, 1, strlen(content), out);
                    fclose(out);
                    printf("Restored: %s (%s)\n", filename, file_type);
                }
                free(content);
            }
        }
        entry = strtok(NULL, "\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: gitlet_checkout.+x <project-hash> <commit|branch>\n");
        return 1;
    }

    char project_hash[256] = {0};
    resolve_project_hash(argc, argv, project_hash, sizeof(project_hash));
    if (strlen(project_hash) == 0) { fprintf(stderr, "Usage: %s [project-hash]\n  Or create .gitlet-hash file.\n", argv[0]); return 1; }
    resolve_gitlet_root(project_hash);
    const char *target = argv[2];

    /* Check if target is a branch */
    char branch_path[MAX_PATH_LEN];
    snprintf(branch_path, sizeof(branch_path), "%s/refs/heads/%s", GITLET_ROOT, target);
    FILE *f = fopen(branch_path, "r");

    if (f) {
        /* It's a branch - switch HEAD */
        char commit_hash[MAX_PATH_LEN];
        if (fgets(commit_hash, sizeof(commit_hash), f)) {
            commit_hash[strcspn(commit_hash, "\n")] = '\0';
        }
        fclose(f);

        /* Update HEAD */
        char head_path[MAX_PATH_LEN];
        snprintf(head_path, sizeof(head_path), "%s/HEAD", GITLET_ROOT);
        f = fopen(head_path, "w");
        if (f) {
            fprintf(f, "ref: refs/heads/%s\n", target);
            fclose(f);
        }

        /* Restore files */
        restore_from_commit(commit_hash);
        printf("Switched to branch '%s'\n", target);
    } else {
        /* It's a commit hash - detached HEAD */
        restore_from_commit(target);
        printf("Checked out commit %s\n", target);
    }

    return 0;
}
