/*
 * file_explorer_manager - Directory browsing backend process
 * Communicates with GUI frontend via text files in package_dir
 * Usage: file_explorer_manager <house_root> <package_dir> <mode>
 *
 * REAL, NEW 2026-09-05 - argv order matches khtpm_core_render.c's own
 * launch_module() convention exactly (house_root, then package_dir,
 * then a <module>'s own id= as a single extra_arg) - this file was
 * originally speced/written with a different, hypothetical argv
 * shape (package_dir, start_dir, mode); adjusted here, once it's
 * actually being wired into the real launcher, rather than inventing
 * a start_dir argv slot launch_module() has no way to fill. Browsing
 * always starts at house_root itself - a real, honest v1 default, not
 * a placeholder; a configurable start_dir is real, separate future
 * work if a consumer ever needs one.
 */
#define _DEFAULT_SOURCE /* usleep() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_ENTRIES 512
#define MAX_NAME 300
#define MAX_CMD_BUFFER 4096

typedef struct {
    char name[MAX_NAME];
    char type[4];
    char size[16];
} Entry;

typedef struct {
    Entry entries[MAX_ENTRIES];
    int count;
    char current_dir[MAX_PATH];
    char mode[10];
    char pending_filename[MAX_NAME];
    int last_seq;
} State;

int is_readable_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

void get_parent_dir(const char *path, char *parent) {
    strcpy(parent, path);
    char *last_slash = strrchr(parent, '/');
    if (last_slash == NULL || last_slash == parent) {
        strcpy(parent, "/");
    } else {
        *last_slash = '\0';
    }
}

void format_size(off_t size, char *buf) {
    if (size < 1024) {
        snprintf(buf, 16, "%ldB", (long)size);
    } else if (size < 1024LL * 1024) {
        snprintf(buf, 16, "%ldKB", (long)(size / 1024));
    } else if (size < 1024LL * 1024 * 1024) {
        snprintf(buf, 16, "%ldMB", (long)(size / (1024LL * 1024)));
    } else {
        snprintf(buf, 16, "%ldGB", (long)(size / (1024LL * 1024 * 1024)));
    }
}

int entry_cmp(const void *a, const void *b) {
    const Entry *ea = (const Entry *)a;
    const Entry *eb = (const Entry *)b;

    if (strcmp(ea->type, "DIR") == 0 && strcmp(eb->type, "FIL") == 0) return -1;
    if (strcmp(ea->type, "FIL") == 0 && strcmp(eb->type, "DIR") == 0) return 1;

    return strcmp(ea->name, eb->name);
}

void list_directory(const char *dir, State *state) {
    DIR *d = opendir(dir);
    if (!d) return;

    state->count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && state->count < MAX_ENTRIES) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        strncpy(state->entries[state->count].name, entry->d_name, MAX_NAME - 1);
        state->entries[state->count].name[MAX_NAME - 1] = '\0';

        if (S_ISDIR(st.st_mode)) {
            strcpy(state->entries[state->count].type, "DIR");
            strcpy(state->entries[state->count].size, "");
        } else {
            strcpy(state->entries[state->count].type, "FIL");
            format_size(st.st_size, state->entries[state->count].size);
        }

        state->count++;
    }

    closedir(d);

    qsort(state->entries, state->count, sizeof(Entry), entry_cmp);

    if (strcmp(dir, "/") != 0 && state->count < MAX_ENTRIES) {
        for (int i = state->count; i > 0; i--) {
            state->entries[i] = state->entries[i-1];
        }
        strcpy(state->entries[0].name, "..");
        strcpy(state->entries[0].type, "DIR");
        strcpy(state->entries[0].size, "");
        state->count++;
    }
}

void write_ui_file(const char *package_dir, State *state,
                   const char *result, const char *result_action) {
    char ui_path[MAX_PATH];
    snprintf(ui_path, MAX_PATH, "%s/file_explorer_ui.txt", package_dir);

    FILE *f = fopen(ui_path, "w");
    if (!f) return;

    fprintf(f, "mode=%s\n", state->mode);
    /* REAL, NEW 2026-09-05 - file-explorer-pal.xhtpm's own Save row
     * (the cli_io filename field + Save button) uses show="${show_
     * save_row}" to stay hidden entirely in LOAD mode. */
    fprintf(f, "show_save_row=%d\n", strcmp(state->mode, "SAVE") == 0 ? 1 : 0);
    fprintf(f, "show_load_hint=%d\n", strcmp(state->mode, "SAVE") == 0 ? 0 : 1);
    fprintf(f, "dir=%s\n", state->current_dir);
    fprintf(f, "n_entries=%d\n", state->count);

    for (int i = 0; i < state->count; i++) {
        fprintf(f, "entry_%d_name=%s\n", i, state->entries[i].name);
        fprintf(f, "entry_%d_type=%s\n", i, state->entries[i].type);
        fprintf(f, "entry_%d_size=%s\n", i, state->entries[i].size);
    }

    fprintf(f, "filename=%s\n", state->pending_filename);
    fprintf(f, "result=%s\n", result ? result : "");
    fprintf(f, "result_action=%s\n", result_action ? result_action : "");

    fclose(f);
}

void read_action_file(const char *package_dir, int *seq, char *cmd) {
    char action_path[MAX_PATH];
    snprintf(action_path, MAX_PATH, "%s/file_explorer_action.txt", package_dir);

    FILE *f = fopen(action_path, "r");
    if (!f) {
        *seq = 0;
        cmd[0] = '\0';
        return;
    }

    char buffer[MAX_CMD_BUFFER];
    memset(buffer, 0, MAX_CMD_BUFFER);
    size_t bytes = fread(buffer, 1, MAX_CMD_BUFFER - 1, f);
    fclose(f);

    if (bytes == 0) {
        *seq = 0;
        cmd[0] = '\0';
        return;
    }

    buffer[bytes] = '\0';

    *seq = 0;
    cmd[0] = '\0';

    char *line1_end = strchr(buffer, '\n');
    if (line1_end) {
        *line1_end = '\0';
    }
    if (strncmp(buffer, "seq=", 4) == 0) {
        *seq = atoi(buffer + 4);
    }

    if (line1_end) {
        char *line2 = line1_end + 1;
        char *line2_end = strchr(line2, '\n');
        if (line2_end) {
            *line2_end = '\0';
        }
        if (strncmp(line2, "cmd=", 4) == 0) {
            strncpy(cmd, line2 + 4, MAX_CMD_BUFFER - 1);
            cmd[MAX_CMD_BUFFER - 1] = '\0';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> <mode>\n", argv[0]);
        return 1;
    }

    const char *house_root = argv[1];
    const char *package_dir = argv[2];
    const char *mode = argv[3];
    const char *start_dir = house_root; /* real v1 default - see this file's own top-of-file comment */

    State state;
    memset(&state, 0, sizeof(state));
    strncpy(state.mode, mode, 9);
    state.last_seq = 0;

    if (!is_readable_dir(start_dir)) {
        start_dir = package_dir;
    }

    if (!is_readable_dir(start_dir)) {
        fprintf(stderr, "Error: cannot access start directory\n");
        return 1;
    }

    strncpy(state.current_dir, start_dir, MAX_PATH - 1);
    state.current_dir[MAX_PATH - 1] = '\0';

    char action_path[MAX_PATH];
    snprintf(action_path, MAX_PATH, "%s/file_explorer_action.txt", package_dir);
    FILE *f = fopen(action_path, "w");
    if (f) {
        fprintf(f, "seq=0\ncmd=\n");
        fclose(f);
    }

    list_directory(state.current_dir, &state);
    write_ui_file(package_dir, &state, "", "");

    while (1) {
        usleep(50000);

        int seq;
        char cmd[MAX_CMD_BUFFER];
        read_action_file(package_dir, &seq, cmd);

        if (seq <= state.last_seq || cmd[0] == '\0') {
            continue;
        }

        state.last_seq = seq;

        if (strncmp(cmd, "ENTRY:", 6) == 0) {
            int idx = atoi(cmd + 6);
            if (idx < 0 || idx >= state.count) {
                continue;
            }

            Entry *e = &state.entries[idx];

            if (strcmp(e->type, "DIR") == 0) {
                char new_dir[MAX_PATH];
                if (strcmp(e->name, "..") == 0) {
                    get_parent_dir(state.current_dir, new_dir);
                } else {
                    snprintf(new_dir, MAX_PATH, "%s/%s", state.current_dir, e->name);
                }

                if (is_readable_dir(new_dir)) {
                    strncpy(state.current_dir, new_dir, MAX_PATH - 1);
                    state.current_dir[MAX_PATH - 1] = '\0';
                    list_directory(state.current_dir, &state);
                    write_ui_file(package_dir, &state, "", "");
                }
            } else {
                if (strcmp(state.mode, "LOAD") == 0) {
                    char result[MAX_PATH];
                    snprintf(result, MAX_PATH, "%s/%s", state.current_dir, e->name);
                    write_ui_file(package_dir, &state, result, "LOAD");
                    return 0;
                } else if (strcmp(state.mode, "SAVE") == 0) {
                    strncpy(state.pending_filename, e->name, MAX_NAME - 1);
                    state.pending_filename[MAX_NAME - 1] = '\0';
                    write_ui_file(package_dir, &state, "", "");
                }
            }
        } else if (strncmp(cmd, "SAVEAS:", 7) == 0) {
            const char *name = cmd + 7;
            if (name[0] != '\0') {
                char result[MAX_PATH];
                snprintf(result, MAX_PATH, "%s/%s", state.current_dir, name);
                write_ui_file(package_dir, &state, result, "SAVE");
                return 0;
            }
        } else if (strcmp(cmd, "CANCEL") == 0) {
            write_ui_file(package_dir, &state, "", "CANCEL");
            return 0;
        }
    }

    return 0;
}
