/*
 * gitlet_add.c - Stage a file for commit (TRUNK + DELTA)
 * Usage: gitlet_add.+x <project-hash> <filename>
 * 
 * First commit of a file: stores full content as TRUNK
 * Subsequent commits: stores only the DIFF as DELTA
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

/* Find previous hash for a filename from the current commit's tree */
static char* find_prev_hash_from_commit(const char *filename, char *out_hash, size_t out_size, char *out_type, size_t out_type_size) {
    out_hash[0] = '\0';
    out_type[0] = '\0';
    
    /* Get current commit hash */
    char head_path[MAX_PATH_LEN];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", GITLET_ROOT);
    FILE *hf = fopen(head_path, "r");
    if (!hf) return out_hash;
    
    char head_line[1024];
    char commit_hash[16] = {0};
    if (fgets(head_line, sizeof(head_line), hf)) {
        if (strncmp(head_line, "ref: refs/heads/", 16) == 0) {
            char *b = head_line + 16;
            b[strcspn(b, "\n")] = '\0';
            char ref_path[MAX_PATH_LEN];
            snprintf(ref_path, sizeof(ref_path), "%s/refs/heads/%s", GITLET_ROOT, b);
            fclose(hf);
            hf = fopen(ref_path, "r");
            if (hf) {
                if (fgets(commit_hash, sizeof(commit_hash), hf)) {
                    commit_hash[strcspn(commit_hash, "\n")] = '\0';
                }
                fclose(hf);
            }
        } else {
            fclose(hf);
        }
    } else {
        fclose(hf);
    }
    
    if (strlen(commit_hash) == 0) return out_hash;
    
    /* Read commit to get tree hash */
    char commit_path[MAX_PATH_LEN];
    snprintf(commit_path, sizeof(commit_path), "%s/objects/%s", GITLET_ROOT, commit_hash);
    FILE *cf = fopen(commit_path, "r");
    if (!cf) return out_hash;
    
    char tree_hash[16] = {0};
    char line[1024];
    while (fgets(line, sizeof(line), cf)) {
        if (strncmp(line, "tree ", 5) == 0) {
            char *t = line + 5;
            t[strcspn(t, "\n")] = '\0';
            strncpy(tree_hash, t, sizeof(tree_hash) - 1);
        }
    }
    fclose(cf);
    
    if (strlen(tree_hash) == 0) return out_hash;
    
    /* Read tree (which is the index snapshot) */
    char tree_path[MAX_PATH_LEN];
    snprintf(tree_path, sizeof(tree_path), "%s/objects/%s", GITLET_ROOT, tree_hash);
    FILE *tf = fopen(tree_path, "r");
    if (!tf) return out_hash;
    
    char tree_content[MAX_CONTENT] = {0};
    while (fgets(line, sizeof(line), tf)) {
        strcat(tree_content, line);
    }
    fclose(tf);
    
    /* Find filename in tree */
    char *entry = strtok(tree_content, "\n");
    while (entry) {
        char fname[256], fhash[16], ftype[16];
        if (sscanf(entry, "%s %s %s", fname, fhash, ftype) >= 2) {
            if (strcmp(fname, filename) == 0) {
                strncpy(out_hash, fhash, out_size - 1);
                out_hash[out_size - 1] = '\0';
                if (strlen(ftype) > 0) {
                    strncpy(out_type, ftype, out_type_size - 1);
                    out_type[out_type_size - 1] = '\0';
                } else {
                    strcpy(out_type, "trunk");
                }
                return out_hash;
            }
        }
        entry = strtok(NULL, "\n");
    }
    
    return out_hash;
}

/* Compute a simple diff string (unified diff-like) */
static char* compute_diff(const char *old_content, const char *new_content) {
    if (!old_content) return strdup(new_content);
    if (!new_content) return strdup("");
    
    if (strcmp(old_content, new_content) == 0) return NULL; /* No change */
    
    /* Simple line-by-line diff */
    char *diff = malloc(MAX_CONTENT);
    if (!diff) return NULL;
    diff[0] = '\0';
    
    char *old_copy = strdup(old_content);
    char *new_copy = strdup(new_content);
    
    char *old_line = strtok(old_copy, "\n");
    char *new_line = strtok(new_copy, "\n");
    
    while (old_line || new_line) {
        if (!old_line) {
            char add_line[512];
            snprintf(add_line, sizeof(add_line), "+%s\n", new_line);
            strncat(diff, add_line, MAX_CONTENT - strlen(diff) - 1);
            new_line = strtok(NULL, "\n");
        } else if (!new_line) {
            char del_line[512];
            snprintf(del_line, sizeof(del_line), "-%s\n", old_line);
            strncat(diff, del_line, MAX_CONTENT - strlen(diff) - 1);
            old_line = strtok(NULL, "\n");
        } else if (strcmp(old_line, new_line) != 0) {
            char del_line[512], add_line[512];
            snprintf(del_line, sizeof(del_line), "-%s\n", old_line);
            snprintf(add_line, sizeof(add_line), "+%s\n", new_line);
            strncat(diff, del_line, MAX_CONTENT - strlen(diff) - 1);
            strncat(diff, add_line, MAX_CONTENT - strlen(diff) - 1);
            old_line = strtok(NULL, "\n");
            new_line = strtok(NULL, "\n");
        } else {
            old_line = strtok(NULL, "\n");
            new_line = strtok(NULL, "\n");
        }
    }
    
    free(old_copy);
    free(new_copy);
    return diff;
}

int main(int argc, char *argv[]) {
    char project_hash[256] = {0};
    const char *filename = NULL;
    
    if (argc >= 3) {
        strncpy(project_hash, argv[1], sizeof(project_hash) - 1);
        filename = argv[2];
    } else if (argc == 2) {
        resolve_project_hash(0, NULL, project_hash, sizeof(project_hash));
        filename = argv[1];
    }
    
    if (strlen(project_hash) == 0) {
        fprintf(stderr, "Usage: gitlet_add.+x [project-hash] <filename>\n");
        fprintf(stderr, "  Or create .gitlet-hash file in project root.\n");
        return 1;
    }
    if (!filename) {
        fprintf(stderr, "Usage: gitlet_add.+x [project-hash] <filename>\n");
        return 1;
    }
    
    resolve_gitlet_root(project_hash);

    /* Read new file */
    long new_size;
    char *new_content = read_file_contents(filename, &new_size);
    if (!new_content) { perror(filename); return 1; }

    /* Check if we have a previous version from the last commit */
    char prev_hash[16] = {0};
    char prev_type[16] = {0};
    find_prev_hash_from_commit(filename, prev_hash, sizeof(prev_hash), prev_type, sizeof(prev_type));

    char stored_hash[16];
    char storage_type[16]; /* "trunk" or "delta" */

    if (strlen(prev_hash) > 0) {
        /* File exists in last commit - reconstruct full content, then compute delta */
        char *old_content = NULL;
        
        if (strcmp(prev_type, "trunk") == 0) {
            /* Previous was a trunk - read it directly */
            char prev_path[MAX_PATH_LEN];
            snprintf(prev_path, sizeof(prev_path), "%s/objects/%s", GITLET_ROOT, prev_hash);
            long old_size;
            old_content = read_file_contents(prev_path, &old_size);
        } else {
            /* Previous was a delta - need to find trunk and apply delta */
            /* For now, scan objects for the trunk (simplified) */
            /* TODO: Store trunk hash reference in delta metadata */
            char prev_path[MAX_PATH_LEN];
            snprintf(prev_path, sizeof(prev_path), "%s/deltas/%s", GITLET_ROOT, prev_hash);
            long delta_size;
            char *delta = read_file_contents(prev_path, &delta_size);
            
            /* Find trunk by scanning objects */
            char trunk_path[MAX_PATH_LEN];
            snprintf(trunk_path, sizeof(trunk_path), "%s/objects", GITLET_ROOT);
            DIR *obj_dir = opendir(trunk_path);
            if (obj_dir && delta) {
                struct dirent *entry;
                while ((entry = readdir(obj_dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;
                    /* Try this as trunk - apply delta and see if it matches expected size */
                    char try_path[MAX_PATH_LEN];
                    snprintf(try_path, sizeof(try_path), "%s/objects/%s", GITLET_ROOT, entry->d_name);
                    long try_size;
                    char *try_content = read_file_contents(try_path, &try_size);
                    if (try_content) {
                        /* Simple check: if applying delta gives us something close to new content size */
                        /* For now, just use the first object as trunk (simplified) */
                        old_content = try_content;
                        break;
                    }
                }
                closedir(obj_dir);
            }
            if (delta) free(delta);
        }
        
        if (old_content) {
            char *diff = compute_diff(old_content, new_content);
            if (!diff || strlen(diff) == 0) {
                /* No change */
                if (diff) free(diff);
                free(old_content);
                free(new_content);
                printf("Unchanged: %s\n", filename);
                return 0;
            }
            
            /* Hash and store delta */
            hash_contents(diff, strlen(diff), stored_hash);
            strcpy(storage_type, "delta");
            
            /* Create deltas dir if needed */
            char delta_dir[MAX_PATH_LEN];
            snprintf(delta_dir, sizeof(delta_dir), "%s/deltas", GITLET_ROOT);
            mkdir(delta_dir, 0777);
            
            char delta_path[MAX_PATH_LEN];
            snprintf(delta_path, sizeof(delta_path), "%s/deltas/%s", GITLET_ROOT, stored_hash);
            FILE *f = fopen(delta_path, "w");
            if (!f) { perror("fopen delta"); free(diff); free(old_content); free(new_content); return 1; }
            fwrite(diff, 1, strlen(diff), f);
            fclose(f);
            
            printf("Delta: %s (%s) - %ld bytes diff (was %s)\n", filename, stored_hash, (long)strlen(diff), prev_type);
            free(diff);
            free(old_content);
        } else {
            /* Previous content not found - store as trunk */
            hash_contents(new_content, new_size, stored_hash);
            strcpy(storage_type, "trunk");
            
            char trunk_path[MAX_PATH_LEN];
            snprintf(trunk_path, sizeof(trunk_path), "%s/objects/%s", GITLET_ROOT, stored_hash);
            FILE *f = fopen(trunk_path, "w");
            if (!f) { perror("fopen trunk"); free(new_content); return 1; }
            fwrite(new_content, 1, new_size, f);
            fclose(f);
            
            printf("Trunk (prev not found): %s (%s)\n", filename, stored_hash);
        }
    } else {
        /* New file - store as trunk */
        hash_contents(new_content, new_size, stored_hash);
        strcpy(storage_type, "trunk");
        
        char trunk_path[MAX_PATH_LEN];
        snprintf(trunk_path, sizeof(trunk_path), "%s/objects/%s", GITLET_ROOT, stored_hash);
        FILE *f = fopen(trunk_path, "w");
        if (!f) { perror("fopen trunk"); free(new_content); return 1; }
        fwrite(new_content, 1, new_size, f);
        fclose(f);
        
        printf("Trunk: %s (%s) - %ld bytes\n", filename, stored_hash, new_size);
    }

    /* Update index: filename hash type */
    char index_path[MAX_PATH_LEN];
    snprintf(index_path, sizeof(index_path), "%s/index", GITLET_ROOT);

    /* Read existing index, remove old entry for this file */
    char index_content[MAX_CONTENT] = {0};
    char *idx = read_file_contents(index_path, NULL);
    if (idx) {
        char *line = strtok(idx, "\n");
        char new_index[MAX_CONTENT] = {0};
        while (line) {
            char fname[256], fhash[16], ftype[16];
            if (sscanf(line, "%s %s %s", fname, fhash, ftype) >= 2) {
                if (strcmp(fname, filename) != 0) {
                    strcat(new_index, line);
                    strcat(new_index, "\n");
                }
            }
            line = strtok(NULL, "\n");
        }
        strncpy(index_content, new_index, sizeof(index_content) - 1);
        free(idx);
    }

    /* Append new entry with type and optional trunk reference */
    FILE *f = fopen(index_path, "w");
    if (!f) { perror("fopen index"); free(new_content); return 1; }
    fprintf(f, "%s", index_content);
    if (strcmp(storage_type, "delta") == 0 && strlen(prev_hash) > 0) {
        /* Delta stores reference to its trunk */
        fprintf(f, "%s %s %s %s\n", filename, stored_hash, storage_type, prev_hash);
    } else {
        fprintf(f, "%s %s %s\n", filename, stored_hash, storage_type);
    }
    fclose(f);

    free(new_content);
    return 0;
}
