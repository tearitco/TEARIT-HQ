#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>

// Hash function adapted from 69.list+dirs🔦️[.]+#]🔐️i10]CONT.c
void hash_contents(const char *data, long long size, char *hash_output) {
    unsigned long hash = 5381;
    int c;
    const unsigned char *str = (const unsigned char *)data;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // djb2 algorithm
    }
    hash = hash ^ (unsigned long)size; // Incorporate size
    char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    hash_output[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        hash_output[i] = charset[hash % 62];
        hash >>= 5;
    }
}

void init_command(int bare) {
    mkdir(".gitlet", 0777);
    mkdir(".gitlet/objects", 0777);
    mkdir(".gitlet/refs", 0777);
    mkdir(".gitlet/refs/heads", 0777);

    FILE *master_file = fopen(".gitlet/refs/heads/master", "w");
    fprintf(master_file, "\n");
    fclose(master_file);

    FILE *head = fopen(".gitlet/HEAD", "w");
    fprintf(head, "ref: refs/heads/master\n");
    fclose(head);

    FILE *config = fopen(".gitlet/config", "w");
    fprintf(config, "[core]\n\tbare = %s\n", bare ? "true" : "false");
    fclose(config);

    printf("Initialized %s Gitlet repository in .gitlet/\n", bare ? "bare" : "empty");
}

void add_command(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    char content_hash[9];
    hash_contents(buffer, length, content_hash);

    char blob_path[256];
    sprintf(blob_path, ".gitlet/objects/%s", content_hash);
    FILE *blob_file = fopen(blob_path, "w");
    fwrite(buffer, 1, length, blob_file);
    fclose(blob_file);

    FILE *index_file = fopen(".gitlet/index", "a");
    fprintf(index_file, "%s %s\n", filename, content_hash);
    fclose(index_file);

    free(buffer);
}

void commit_command(char *commit_message) {
    // Read the index
    FILE *index_file = fopen(".gitlet/index", "r");
    if (!index_file) {
        perror("fopen");
        return;
    }

    char line[256];
    char tree_content[4096] = {0};
    while (fgets(line, sizeof(line), index_file)) {
        strcat(tree_content, line);
    }
    fclose(index_file);

    // Create tree object
    char tree_hash[9];
    hash_contents(tree_content, strlen(tree_content), tree_hash);
    char tree_path[256];
    sprintf(tree_path, ".gitlet/objects/%s", tree_hash);
    FILE *tree_file = fopen(tree_path, "w");
    fwrite(tree_content, 1, strlen(tree_content), tree_file);
    fclose(tree_file);

    // Get parent commit hash
    char parent_hash[256] = {0};
    FILE *head_file = fopen(".gitlet/HEAD", "r");
    if (head_file) {
        char head_content[256];
        fgets(head_content, sizeof(head_content), head_file);
        fclose(head_file);
        if (strncmp(head_content, "ref: ", 5) == 0) {
            char ref_path[256];
            sprintf(ref_path, ".gitlet/%s", head_content + 5);
            ref_path[strlen(ref_path) - 1] = '\0'; // remove newline
            FILE *ref_file = fopen(ref_path, "r");
            if (ref_file) {
                fgets(parent_hash, sizeof(parent_hash), ref_file);
                fclose(ref_file);
                parent_hash[strlen(parent_hash) - 1] = '\0'; // remove newline
            }
        }
    }

    // Create commit object
    char commit_content[4096];
    time_t now = time(NULL);
    sprintf(commit_content, "tree %s\nparent %s\nDate: %s\n%s\n", tree_hash, parent_hash, ctime(&now), commit_message);

    char commit_hash[9];
    hash_contents(commit_content, strlen(commit_content), commit_hash);
    char commit_path[256];
    sprintf(commit_path, ".gitlet/objects/%s", commit_hash);
    FILE *commit_file = fopen(commit_path, "w");
    fwrite(commit_content, 1, strlen(commit_content), commit_file);
    fclose(commit_file);

    // Update HEAD
    char head_ref[256];
    head_file = fopen(".gitlet/HEAD", "r");
    fgets(head_ref, sizeof(head_ref), head_file);
    fclose(head_file);
    if (strncmp(head_ref, "ref: ", 5) == 0) {
        char ref_path[256];
        sprintf(ref_path, ".gitlet/%s", head_ref + 5);
        ref_path[strlen(ref_path) - 1] = '\0'; // remove newline
        FILE *ref_file = fopen(ref_path, "w");
        fprintf(ref_file, "%s\n", commit_hash);
        fclose(ref_file);
    }

    printf("[%s %s] %s\n", "master", commit_hash, commit_message);
}

void print_commit(char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return;
    }

    char commit_path[256];
    sprintf(commit_path, ".gitlet/objects/%s", commit_hash);
    FILE *commit_file = fopen(commit_path, "r");
    if (!commit_file) {
        return; // Reached the end of the log
    }

    char line[256];
    char parent_hash[256] = {0};
    while (fgets(line, sizeof(line), commit_file)) {
        if (strncmp(line, "parent ", 7) == 0) {
            strcpy(parent_hash, line + 7);
            parent_hash[strlen(parent_hash) - 1] = '\0'; // remove newline
        }
        printf("%s", line);
    }
    fclose(commit_file);

    printf("\n");

    print_commit(parent_hash);
}

void log_command() {
    char head_ref[256];
    FILE *head_file = fopen(".gitlet/HEAD", "r");
    if (!head_file) {
        perror("fopen");
        return;
    }
    fgets(head_ref, sizeof(head_ref), head_file);
    fclose(head_file);

    char commit_hash[256];
    if (strncmp(head_ref, "ref: ", 5) == 0) {
        char ref_path[256];
        sprintf(ref_path, ".gitlet/%s", head_ref + 5);
        ref_path[strlen(ref_path) - 1] = '\0'; // remove newline
        FILE *ref_file = fopen(ref_path, "r");
        if (ref_file) {
            fgets(commit_hash, sizeof(commit_hash), ref_file);
            fclose(ref_file);
            commit_hash[strlen(commit_hash) - 1] = '\0'; // remove newline
        }
    }

    print_commit(commit_hash);
}

void status_command() {
    FILE *head_file = fopen(".gitlet/HEAD", "r");
    if (!head_file) {
        perror("fopen");
        return;
    }

    char head_content[256];
    fgets(head_content, sizeof(head_content), head_file);
    fclose(head_file);

    if (strncmp(head_content, "ref: refs/heads/", 16) == 0) {
        char branch_name[256];
        strcpy(branch_name, head_content + 16);
        branch_name[strlen(branch_name) - 1] = '\0'; // remove newline
        printf("On branch %s\n", branch_name);
    } else {
        printf("Not on any branch\n");
    }

    printf("\nChanges to be committed:\n");

    FILE *index_file = fopen(".gitlet/index", "r");
    if (index_file) {
        char line[256];
        while (fgets(line, sizeof(line), index_file)) {
            char filename[256];
            sscanf(line, "%s", filename);
            printf("\tnew file:   %s\n", filename);
        }
        fclose(index_file);
    }

    printf("\nUntracked files:\n");

    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".gitlet") != 0 && strcmp(dir->d_name, "gitlet") != 0) {
                FILE *index_file_check = fopen(".gitlet/index", "r");
                int found = 0;
                if (index_file_check) {
                    char line_check[256];
                    while (fgets(line_check, sizeof(line_check), index_file_check)) {
                        char filename_check[256];
                        sscanf(line_check, "%s", filename_check);
                        if (strcmp(filename_check, dir->d_name) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    fclose(index_file_check);
                }
                if (!found) {
                    printf("\t%s\n", dir->d_name);
                }
            }
        }
        closedir(d);
    }
}

void branch_command(char *branch_name) {
    if (branch_name == NULL) {
        // List branches
        DIR *d;
        struct dirent *dir;
        d = opendir(".gitlet/refs/heads");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_type == DT_REG) { // Only list regular files
                    printf("%s\n", dir->d_name);
                }
            }
            closedir(d);
        }
    } else {
        // Create new branch
        char head_ref[256];
        FILE *head_file = fopen(".gitlet/HEAD", "r");
        if (!head_file) {
            perror("fopen");
            return;
        }
        fgets(head_ref, sizeof(head_ref), head_file);
        fclose(head_file);

        char commit_hash[256];
        if (strncmp(head_ref, "ref: ", 5) == 0) {
            char ref_path[256];
            sprintf(ref_path, ".gitlet/%s", head_ref + 5);
            ref_path[strlen(ref_path) - 1] = '\0'; // remove newline
            FILE *ref_file = fopen(ref_path, "r");
            if (ref_file) {
                fgets(commit_hash, sizeof(commit_hash), ref_file);
                fclose(ref_file);
                commit_hash[strlen(commit_hash) - 1] = '\0'; // remove newline
            }
        }

        char new_branch_path[256];
        sprintf(new_branch_path, ".gitlet/refs/heads/%s", branch_name);
        FILE *new_branch_file = fopen(new_branch_path, "w");
        fprintf(new_branch_file, "%s\n", commit_hash);
        fclose(new_branch_file);
        printf("Created branch %s\n", branch_name);
    }
}

void checkout_command(char *branch_name) {
    char branch_path[256];
    sprintf(branch_path, ".gitlet/refs/heads/%s", branch_name);
    FILE *branch_file = fopen(branch_path, "r");
    if (!branch_file) {
        printf("Branch %s does not exist.\n", branch_name);
        return;
    }

    char commit_hash[256];
    fgets(commit_hash, sizeof(commit_hash), branch_file);
    commit_hash[strlen(commit_hash) - 1] = '\0'; // remove newline
    fclose(branch_file);

    // Clear the index
    FILE *index_file = fopen(".gitlet/index", "w");
    fclose(index_file);

    // Update working directory
    if (strlen(commit_hash) > 0) {
        char commit_path[256];
        sprintf(commit_path, ".gitlet/objects/%s", commit_hash);
        FILE *commit_file = fopen(commit_path, "r");
        if (commit_file) {
            char line[256];
            char tree_hash[256];
            while (fgets(line, sizeof(line), commit_file)) {
                if (strncmp(line, "tree ", 5) == 0) {
                    strcpy(tree_hash, line + 5);
                    tree_hash[strlen(tree_hash) - 1] = '\0'; // remove newline
                    break;
                }
            }
            fclose(commit_file);

            char tree_path[256];
            sprintf(tree_path, ".gitlet/objects/%s", tree_hash);
            FILE *tree_file = fopen(tree_path, "r");
            if (tree_file) {
                while (fgets(line, sizeof(line), tree_file)) {
                    char filename[256];
                    char file_hash[256];
                    sscanf(line, "%s %s", filename, file_hash);

                    char blob_path[256];
                    sprintf(blob_path, ".gitlet/objects/%s", file_hash);
                    FILE *blob_file = fopen(blob_path, "r");
                    if (blob_file) {
                        FILE *working_file = fopen(filename, "w");
                        char c;
                        while ((c = fgetc(blob_file)) != EOF) {
                            fputc(c, working_file);
                        }
                        fclose(working_file);
                        fclose(blob_file);

                        // Add to index
                        FILE *index_file_add = fopen(".gitlet/index", "a");
                        fprintf(index_file_add, "%s %s\n", filename, file_hash);
                        fclose(index_file_add);
                    }
                }
                fclose(tree_file);
            }
        }
    }

    char new_head_path[256];
    sprintf(new_head_path, "refs/heads/%s", branch_name);

    FILE *head_file = fopen(".gitlet/HEAD", "w");
    if (!head_file) {
        perror("fopen");
        return;
    }
    fprintf(head_file, "ref: %s\n", new_head_path);
    fclose(head_file);

    printf("Switched to branch '%s'\n", branch_name);
}

void rm_command(char *filename) {
    // Remove from index
    FILE *index_file = fopen(".gitlet/index", "r");
    if (!index_file) {
        perror("fopen");
        return;
    }

    char temp_file_path[256];
    sprintf(temp_file_path, ".gitlet/index_temp");
    FILE *temp_file = fopen(temp_file_path, "w");

    char line[256];
    while (fgets(line, sizeof(line), index_file)) {
        char index_filename[256];
        sscanf(line, "%s", index_filename);
        if (strcmp(index_filename, filename) != 0) {
            fputs(line, temp_file);
        }
    }

    fclose(index_file);
    fclose(temp_file);

    remove(".gitlet/index");
    rename(temp_file_path, ".gitlet/index");

    // Remove from working directory
    remove(filename);

    printf("Removed '%s'\n", filename);
}

void push_command(char *remote_path) {
    // Get current branch
    char head_content[256];
    FILE *head = fopen(".gitlet/HEAD", "r");
    if (!head) {
        perror("fopen");
        return;
    }
    fgets(head_content, sizeof(head_content), head);
    fclose(head);

    char current_branch[256];
    if (strncmp(head_content, "ref: refs/heads/", 16) == 0) {
        strcpy(current_branch, head_content + 16);
        current_branch[strlen(current_branch) - 1] = '\0';
    } else {
        printf("Not on a branch\n");
        return;
    }

    // Get current commit hash
    char ref_path[256];
    sprintf(ref_path, ".gitlet/refs/heads/%s", current_branch);
    FILE *ref = fopen(ref_path, "r");
    if (!ref) {
        perror("fopen");
        return;
    }
    char commit_hash[256];
    fgets(commit_hash, sizeof(commit_hash), ref);
    commit_hash[strlen(commit_hash) - 1] = '\0';
    fclose(ref);

    // Copy objects to remote
    char local_objects[] = ".gitlet/objects";
    DIR *local_dir = opendir(local_objects);
    if (!local_dir) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    struct stat st;
    while ((ent = readdir(local_dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            char local_obj[512];
            sprintf(local_obj, "%s/%s", local_objects, ent->d_name);

            char remote_obj[512];
            sprintf(remote_obj, "%s/.gitlet/objects/%s", remote_path, ent->d_name);

            if (stat(remote_obj, &st) != 0) {
                // Copy file
                FILE *src = fopen(local_obj, "r");
                if (!src) continue;
                FILE *dst = fopen(remote_obj, "w");
                if (!dst) {
                    fclose(src);
                    continue;
                }
                char c;
                while ((c = fgetc(src)) != EOF) {
                    fputc(c, dst);
                }
                fclose(src);
                fclose(dst);
            }
        }
    }
    closedir(local_dir);

    // Update remote ref
    char remote_ref[512];
    sprintf(remote_ref, "%s/.gitlet/refs/heads/%s", remote_path, current_branch);
    FILE *rref = fopen(remote_ref, "w");
    if (!rref) {
        perror("fopen");
        return;
    }
    fprintf(rref, "%s\n", commit_hash);
    fclose(rref);

    printf("Pushed to %s\n", remote_path);
}

void pull_command(char *remote_path) {
    // Get current branch
    char head_content[256];
    FILE *head = fopen(".gitlet/HEAD", "r");
    if (!head) {
        perror("fopen");
        return;
    }
    fgets(head_content, sizeof(head_content), head);
    fclose(head);

    char current_branch[256];
    if (strncmp(head_content, "ref: refs/heads/", 16) == 0) {
        strcpy(current_branch, head_content + 16);
        current_branch[strlen(current_branch) - 1] = '\0';
    } else {
        printf("Not on a branch\n");
        return;
    }

    // Copy objects from remote
    char remote_objects[512];
    sprintf(remote_objects, "%s/.gitlet/objects", remote_path);
    DIR *remote_dir = opendir(remote_objects);
    if (!remote_dir) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    struct stat st;
    while ((ent = readdir(remote_dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            char remote_obj[512];
            sprintf(remote_obj, "%s/%s", remote_objects, ent->d_name);

            char local_obj[512];
            sprintf(local_obj, ".gitlet/objects/%s", ent->d_name);

            if (stat(local_obj, &st) != 0) {
                // Copy file
                FILE *src = fopen(remote_obj, "r");
                if (!src) continue;
                FILE *dst = fopen(local_obj, "w");
                if (!dst) {
                    fclose(src);
                    continue;
                }
                char c;
                while ((c = fgetc(src)) != EOF) {
                    fputc(c, dst);
                }
                fclose(src);
                fclose(dst);
            }
        }
    }
    closedir(remote_dir);

    // Get remote commit hash
    char remote_ref[512];
    sprintf(remote_ref, "%s/.gitlet/refs/heads/%s", remote_path, current_branch);
    FILE *rref = fopen(remote_ref, "r");
    if (!rref) {
        perror("fopen");
        return;
    }
    char remote_hash[256];
    fgets(remote_hash, sizeof(remote_hash), rref);
    remote_hash[strlen(remote_hash) - 1] = '\0';
    fclose(rref);

    // Update local ref
    char local_ref[256];
    sprintf(local_ref, ".gitlet/refs/heads/%s", current_branch);
    FILE *lref = fopen(local_ref, "w");
    if (!lref) {
        perror("fopen");
        return;
    }
    fprintf(lref, "%s\n", remote_hash);
    fclose(lref);

    // Update working directory
    checkout_command(current_branch);

    printf("Pulled from %s\n", remote_path);
}

void merge_command(char *branch_name) {
    // Ensure on master branch
    char head_content[256];
    FILE *head = fopen(".gitlet/HEAD", "r");
    if (!head) {
        perror("fopen");
        return;
    }
    fgets(head_content, sizeof(head_content), head);
    fclose(head);
    if (strncmp(head_content, "ref: refs/heads/master", 22) != 0) {
        printf("Merge must be performed on master branch\n");
        return;
    }

    // Get branch commit hash
    char branch_path[256];
    sprintf(branch_path, ".gitlet/refs/heads/%s", branch_name);
    FILE *branch_file = fopen(branch_path, "r");
    if (!branch_file) {
        printf("Branch %s does not exist\n", branch_name);
        return;
    }
    char branch_commit[256];
    fgets(branch_commit, sizeof(branch_commit), branch_file);
    branch_commit[strlen(branch_commit) - 1] = '\0';
    fclose(branch_file);

    // Update master ref
    FILE *master_file = fopen(".gitlet/refs/heads/master", "w");
    if (!master_file) {
        perror("fopen");
        return;
    }
    fprintf(master_file, "%s\n", branch_commit);
    fclose(master_file);

    // Update working directory
    checkout_command("master");

    printf("Merged branch '%s' into master\n", branch_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: gitlet <command> [<args>]\n");
        return 1;
    }

    char *command = argv[1];

    if (strcmp(command, "init") == 0) {
        int bare = 0;
        if (argc > 2 && strcmp(argv[2], "--bare") == 0) {
            bare = 1;
        }
        init_command(bare);
    } else if (strcmp(command, "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet add <file>\n");
            return 1;
        }
        add_command(argv[2]);
    } else if (strcmp(command, "commit") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet commit <message>\n");
            return 1;
        }
        commit_command(argv[2]);
    } else if (strcmp(command, "log") == 0) {
        log_command();
    } else if (strcmp(command, "status") == 0) {
        status_command();
    } else if (strcmp(command, "branch") == 0) {
        if (argc > 2) {
            branch_command(argv[2]);
        } else {
            branch_command(NULL);
        }
    } else if (strcmp(command, "checkout") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet checkout <branch-name>\n");
            return 1;
        }
        checkout_command(argv[2]);
    } else if (strcmp(command, "rm") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet rm <file>\n");
            return 1;
        }
        rm_command(argv[2]);
    } else if (strcmp(command, "push") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet push <remote_path>\n");
            return 1;
        }
        push_command(argv[2]);
    } else if (strcmp(command, "pull") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet pull <remote_path>\n");
            return 1;
        }
        pull_command(argv[2]);
    } else if (strcmp(command, "merge") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: gitlet merge <branch-name>\n");
            return 1;
        }
        merge_command(argv[2]);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }

    return 0;
}
