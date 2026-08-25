#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#define MODULE_NAME "agy-text-editor"
#define MAX_PATH 4096
#define MAX_LINE 1024

/*
 * 2026-07-11: refactored to comply with Bible section 11 ("Standalone
 * Operational Architecture" -- "NEVER hardcode functional logic...
 * directly into manager modules... Managers orchestrate; Ops execute")
 * and its REUSE RULE ("If logic is shared across managers or projects,
 * prefer a reusable Op first"). This manager used to hardcode cursor-based
 * text editing, file save/load, and directory scanning directly inline --
 * all genuinely reusable logic that wrai-text-editor (a second project
 * needing the exact same capabilities) would otherwise have had to
 * duplicate wholesale. That logic now lives in four standalone,
 * independently CLI-testable Ops under pieces/system/file_ops/ (not under
 * any one project, so any project needing text editing/file copy/
 * directory browsing can reuse them, not just text editors):
 *   - text_edit_key.+x   -- apply one keystroke to a document+cursor
 *   - text_editor_view.+x -- render a scrolled/cursor-marked viewport
 *   - file_copy.+x        -- generic file copy (load/save)
 *   - dir_browse.+x       -- generic directory listing with search filter
 * This file now only orchestrates: which Op to call for a given key/
 * command, and how to present the results (this project's own box-drawing
 * markup) -- exactly the manager/Op boundary section 11 describes.
 *
 * Also fixes the rendering corruption bug reported live on the editor's
 * map screen: the ORIGINAL build_editor_map() copied document lines with
 * strncpy(buf, src, N-1) and never explicitly null-terminated afterward.
 * strncpy does not null-terminate when the source is >= N-1 bytes, so any
 * line at or beyond a buffer's capacity left it unterminated and the
 * strlen()/strcpy() calls that followed read past the buffer. Fixed inside
 * text_editor_view.+x (see that file's own header comment) -- every buffer
 * there is explicitly terminated after every copy now.
 */

static char project_root[MAX_PATH] = ".";
static char active_file_path[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char file_path_input_buffer[MAX_PATH] = "projects/agy-text-editor/pieces/document.txt";
static char search_query_buffer[MAX_LINE] = "";
static char current_dir[MAX_PATH] = "projects/agy-text-editor";
static int browser_mode = 0; // 0 = Load, 1 = Save As

static char response_buffer[256] = "Ready.";
static char input_line_buffer[MAX_LINE] = "";

static volatile sig_atomic_t g_shutdown = 0;

static void handle_command(const char *cmd);
static void handle_interact_key(int key);
static int process_key(int key);

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static void resolve_paths(void) {
    strcpy(project_root, "."); // Default to current directory
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    struct stat st;
                    if (stat(v, &st) == 0 && S_ISDIR(st.st_mode)) {
                        snprintf(project_root, sizeof(project_root), "%s", v);
                    }
                }
            }
        }
        fclose(kvp);
    }
}

static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    int result = 0;
    if (fgets(line, sizeof(line), f)) {
        result = (strstr(line, "editor.chtpm") != NULL ||
                  strstr(line, "file_menu.chtpm") != NULL ||
                  strstr(line, "file_browser.chtpm") != NULL ||
                  strstr(line, "load_slot.chtpm") != NULL);
    }
    fclose(f);
    free(path);
    return result;
}

static void get_current_layout_name(char *buf, size_t sz) {
    buf[0] = '\0';
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), f)) {
            char *trimmed = trim_str(line);
            char *last_slash = strrchr(trimmed, '/');
            if (last_slash) {
                strncpy(buf, last_slash + 1, sz - 1);
            } else {
                strncpy(buf, trimmed, sz - 1);
            }
            buf[sz - 1] = '\0';
        }
        fclose(f);
    }
    free(path);
}

static int get_active_gui_index(void) {
    char *path = NULL;
    int idx = 0;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &idx) != 1) {
            idx = 0;
        }
        fclose(f);
    }
    free(path);
    return idx;
}

static void read_editor_line(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'e') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                strncpy(input_line_buffer, line + 1, sizeof(input_line_buffer) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void read_file_path_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'f') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                char *val = trim_str(line + 1);
                if (strlen(val) > 0 || strlen(file_path_input_buffer) == 0) {
                    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
                }
            }
        }
        fclose(f);
    }
    free(path);
}

static void set_file_path_input(const char *val) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "f%s\n", val);
            fclose(f);
        }
        free(path);
    }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

static void clear_editor_line(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "e\n");
            fclose(f);
        }
        free(path);
    }
    input_line_buffer[0] = '\0';
}

static void read_search_query_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 's') {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void clear_search_query(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "s\n");
            fclose(f);
        }
        free(path);
    }
    search_query_buffer[0] = '\0';
}

static void trigger_render(void) {
    char pulse[MAX_PATH];
    snprintf(pulse, sizeof(pulse), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(pulse, "a");
    if (f) { fprintf(f, "L\n"); fclose(f); }

    snprintf(pulse, sizeof(pulse), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    f = fopen(pulse, "a");
    if (f) { fprintf(f, "S\n"); fclose(f); }
}

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL;
    if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(lp, "a");
        if (f) {
            fprintf(f, "%s\n", layout_path);
            fclose(f);
        }
        free(lp);
    }
}

/* ---- Op invocation helper (Bible section 11: "Managers orchestrate; Ops
   execute" -- this is the ONE place that knows how to run one) ---- */
static int run_op(const char *op_rel_path, char *const op_argv[]) {
    char full_path[MAX_PATH];
    pid_t pid;
    int status;

    snprintf(full_path, sizeof(full_path), "%s/%s", project_root, op_rel_path);
    pid = fork();
    if (pid == 0) {
        execv(full_path, op_argv);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static void get_document_path(char *out, size_t sz) {
    snprintf(out, sz, "%s/projects/agy-text-editor/pieces/document.txt", project_root);
}

static void get_cursor_path(char *out, size_t sz) {
    snprintf(out, sz, "%s/projects/agy-text-editor/pieces/cursor.txt", project_root);
}

static int count_document_lines(void) {
    char doc_path[MAX_PATH];
    FILE *f;
    int count = 0;
    char line[MAX_LINE];
    get_document_path(doc_path, sizeof(doc_path));
    f = fopen(doc_path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) count++;
    fclose(f);
    return count > 0 ? count : 1;
}

static void read_cursor(int *cx, int *cy) {
    char cursor_path[MAX_PATH];
    FILE *f;
    char line[128];
    *cx = 0;
    *cy = 0;
    get_cursor_path(cursor_path, sizeof(cursor_path));
    f = fopen(cursor_path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cursor_x=", 9) == 0) *cx = atoi(trim_str(line + 9));
        else if (strncmp(line, "cursor_y=", 9) == 0) *cy = atoi(trim_str(line + 9));
    }
    fclose(f);
}

static void reset_cursor(void) {
    char cursor_path[MAX_PATH];
    FILE *f;
    get_cursor_path(cursor_path, sizeof(cursor_path));
    f = fopen(cursor_path, "w");
    if (f) { fprintf(f, "cursor_x=0\ncursor_y=0\n"); fclose(f); }
}

/* Delegates the actual keystroke-to-document editing to the shared Op --
   see this file's own header comment. Replaces what used to be ~85 lines
   of inline cursor/line-editing logic. */
static void handle_interact_key(int key) {
    char doc_path[MAX_PATH], cursor_path[MAX_PATH], key_str[16];
    get_document_path(doc_path, sizeof(doc_path));
    get_cursor_path(cursor_path, sizeof(cursor_path));
    snprintf(key_str, sizeof(key_str), "%d", key);
    char *op_argv[] = { "text_edit_key.+x", doc_path, cursor_path, key_str, NULL };
    run_op("pieces/system/file_ops/+x/text_edit_key.+x", op_argv);
}

/* Delegates viewport rendering to the shared Op, then wraps each plain
   line in this project's own box-drawing markup -- the presentation
   choice stays here, the view/cursor logic is shared. */
static void build_editor_map(char *out, size_t max_sz) {
    char doc_path[MAX_PATH], cursor_path[MAX_PATH], view_path[MAX_PATH];
    FILE *vf;
    char line[512];

    out[0] = '\0';
    get_document_path(doc_path, sizeof(doc_path));
    get_cursor_path(cursor_path, sizeof(cursor_path));
    snprintf(view_path, sizeof(view_path), "%s/projects/agy-text-editor/pieces/view.txt", project_root);

    char *op_argv[] = { "text_editor_view.+x", doc_path, cursor_path, "40", "8", view_path, NULL };
    run_op("pieces/system/file_ops/+x/text_editor_view.+x", op_argv);

    vf = fopen(view_path, "r");
    if (!vf) return;
    while (fgets(line, sizeof(line), vf)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char formatted[600];
        snprintf(formatted, sizeof(formatted), "<text label=\"║  %-40.40s ║\" /><br/>", line);
        strncat(out, formatted, max_sz - strlen(out) - 1);
    }
    fclose(vf);
}

static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* Reuses the shared directory-listing Op for autocomplete too (substring
   match against whatever the user has typed so far, scoped to the
   directory implied by that input) rather than a second bespoke scan. */
static int find_autocomplete_matches(const char *input, const char *current_browser_dir, char matches[][256], int max_matches) {
    char dir_to_scan[MAX_PATH];
    const char *prefix = "";
    char full_scan_path[MAX_PATH];
    char listing_path[MAX_PATH];
    FILE *lf;
    char line[512];
    int count = 0;

    const char *last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input;
        if (dir_len >= sizeof(dir_to_scan)) dir_len = sizeof(dir_to_scan) - 1;
        strncpy(dir_to_scan, input, dir_len);
        dir_to_scan[dir_len] = '\0';
        prefix = last_slash + 1;
        if (strlen(dir_to_scan) == 0) strcpy(dir_to_scan, ".");
    } else {
        prefix = input;
        strcpy(dir_to_scan, current_browser_dir);
    }

    snprintf(full_scan_path, sizeof(full_scan_path), "%s/%s", project_root, dir_to_scan);
    snprintf(listing_path, sizeof(listing_path), "%s/projects/agy-text-editor/pieces/autocomplete_listing.txt", project_root);

    char *op_argv[] = { "dir_browse.+x", full_scan_path, (char *)prefix, listing_path, NULL };
    run_op("pieces/system/file_ops/+x/dir_browse.+x", op_argv);

    lf = fopen(listing_path, "r");
    if (!lf) return 0;
    while (count < max_matches && fgets(line, sizeof(line), lf)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char kind[8], name[256];
        char *bar1 = strchr(line, '|');
        if (!bar1) continue;
        size_t klen = bar1 - line;
        if (klen >= sizeof(kind)) continue;
        memcpy(kind, line, klen);
        kind[klen] = '\0';
        char *name_start = bar1 + 1;
        char *bar2 = strchr(name_start, '|');
        size_t nlen = bar2 ? (size_t)(bar2 - name_start) : strlen(name_start);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, name_start, nlen);
        name[nlen] = '\0';
        int is_dir = (strcmp(kind, "DIR") == 0);

        if (last_slash) {
            snprintf(matches[count], 256, "%.*s/%s%s", (int)(last_slash - input), input, name, is_dir ? "/" : "");
        } else {
            snprintf(matches[count], 256, "%s%s", name, is_dir ? "/" : "");
        }
        count++;
    }
    fclose(lf);
    return count;
}

static int get_digits(int num) {
    if (num >= 100) return 3;
    if (num >= 10) return 2;
    return 1;
}

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num;
    int digits = get_digits(num);
    int label_len = strlen(label);

    int visual_len = 8 + digits + label_len;
    int padding = 40 - visual_len;
    if (padding < 0) padding = 0;

    char btn_markup[1024];
    snprintf(btn_markup, sizeof(btn_markup), "<text label=\"║  \" /><button label=\"%s\" %s=\"%s\" />", label, attr_name, attr_val);
    strncat(out, btn_markup, max_sz - strlen(out) - 1);

    if (padding > 0) {
        char pad_str[128];
        snprintf(pad_str, sizeof(pad_str), "<text label=\"%.*s\" />", padding, "                                                                                ");
        strncat(out, pad_str, max_sz - strlen(out) - 1);
    }
    strncat(out, "<text label=\" ║\" /><br/>", max_sz - strlen(out) - 1);

    (*p_display_num)++;
}

/* Delegates to the shared file_copy Op. Replaces what used to be an
   inline fopen/fread/fwrite loop. */
static int save_to_path(const char *rel_path) {
    if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
        strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
    } else {
        snprintf(active_file_path, sizeof(active_file_path), "%s/%s", current_dir, rel_path);
    }
    snprintf(response_buffer, sizeof(response_buffer), "Saving to %s...", active_file_path);

    char full_path[MAX_PATH], doc_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", project_root, active_file_path);
    get_document_path(doc_path, sizeof(doc_path));

    char *op_argv[] = { "file_copy.+x", doc_path, full_path, NULL };
    int rc = run_op("pieces/system/file_ops/+x/file_copy.+x", op_argv);
    if (rc != 0) {
        snprintf(response_buffer, sizeof(response_buffer), "Error: cannot write target file");
        return -1;
    }

    snprintf(response_buffer, sizeof(response_buffer), "Saved to %s", active_file_path);
    return 0;
}

/* Delegates to the shared file_copy Op, then resets cursor to (0,0). */
static int load_from_path(const char *rel_path) {
    if (rel_path[0] == '/' || strstr(rel_path, "projects/") == rel_path) {
        strncpy(active_file_path, rel_path, sizeof(active_file_path) - 1);
    } else {
        snprintf(active_file_path, sizeof(active_file_path), "%s/%s", current_dir, rel_path);
    }
    snprintf(response_buffer, sizeof(response_buffer), "Loading %s...", active_file_path);

    char full_path[MAX_PATH], doc_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", project_root, active_file_path);
    get_document_path(doc_path, sizeof(doc_path));

    char *op_argv[] = { "file_copy.+x", full_path, doc_path, NULL };
    int rc = run_op("pieces/system/file_ops/+x/file_copy.+x", op_argv);
    if (rc != 0) {
        snprintf(response_buffer, sizeof(response_buffer), "Error: file not found");
        return -1;
    }

    reset_cursor();
    snprintf(response_buffer, sizeof(response_buffer), "Loaded %s", active_file_path);
    return 0;
}

static void handle_command(const char *cmd) {
    if (strncmp(cmd, "SET_DIR:", 8) == 0) {
        strncpy(current_dir, cmd + 8, sizeof(current_dir) - 1);
        if (strlen(current_dir) == 0) strcpy(current_dir, ".");
        clear_search_query();
    } else if (strncmp(cmd, "SET_PATH:", 9) == 0) {
        set_file_path_input(cmd + 9);
    } else if (strncmp(cmd, "SET_AUTOCOMPLETE:", 17) == 0) {
        set_file_path_input(cmd + 17);
    } else if (strncmp(cmd, "SET_LOAD_FILE:", 14) == 0) {
        set_file_path_input(cmd + 14);
        load_from_path(cmd + 14);
    } else if (strcmp(cmd, "SET_LOAD_ACTION") == 0) {
        read_file_path_input();
        load_from_path(file_path_input_buffer);
    } else if (strcmp(cmd, "SET_SAVE_ACTION") == 0) {
        read_file_path_input();
        save_to_path(file_path_input_buffer);
    } else if (strcmp(cmd, "SET_NEW_FILE") == 0) {
        char doc_path[MAX_PATH];
        get_document_path(doc_path, sizeof(doc_path));
        FILE *f = fopen(doc_path, "w");
        if (f) {
            fclose(f);
            strcpy(active_file_path, "none");
            strcpy(response_buffer, "New file created (buffer cleared).");
            reset_cursor();
        } else {
            strcpy(response_buffer, "Error creating new file.");
        }
    } else if (strcmp(cmd, "SET_CLEAR_FILE") == 0) {
        char doc_path[MAX_PATH];
        get_document_path(doc_path, sizeof(doc_path));
        FILE *f = fopen(doc_path, "w");
        if (f) {
            fclose(f);
            strcpy(response_buffer, "File cleared.");
            reset_cursor();
        } else {
            strcpy(response_buffer, "Error clearing file.");
        }
    }
}

static int process_key(int key) {
    int processed = 0;

    char layout[MAX_LINE];
    get_current_layout_name(layout, sizeof(layout));

    if (key == 127 || key == 8) {
        if (strcmp(layout, "editor.chtpm") == 0) {
            read_editor_line();
            int len = strlen(input_line_buffer);
            if (len > 0) {
                input_line_buffer[len - 1] = '\0';
                char *path = NULL;
                if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                    FILE *bf = fopen(path, "a");
                    if (bf) {
                        fprintf(bf, "e%s\n", input_line_buffer);
                        fclose(bf);
                    }
                    free(path);
                }
                processed = 1;
            }
        } else if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            if (active_idx == 1) { // search_query
                read_search_query_input();
                int len = strlen(search_query_buffer);
                if (len > 0) {
                    search_query_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) {
                            fprintf(bf, "s%s\n", search_query_buffer);
                            fclose(bf);
                        }
                        free(path);
                    }
                    processed = 1;
                }
            } else if (active_idx == 2) { // file_path_input
                read_file_path_input();
                int len = strlen(file_path_input_buffer);
                if (len > 0) {
                    file_path_input_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) {
                            fprintf(bf, "f%s\n", file_path_input_buffer);
                            fclose(bf);
                        }
                        free(path);
                    }
                    processed = 1;
                }
            }
        }
    } else if (key == 10 || key == 13) {
        if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            if (active_idx == 2) { // file_path_input
                read_file_path_input();
                if (strlen(file_path_input_buffer) > 0) {
                    if (browser_mode == 0) handle_command("SET_LOAD_ACTION");
                    else handle_command("SET_SAVE_ACTION");
                    processed = 1;
                }
            } else if (active_idx == 1) { // search_query
                read_search_query_input();
                processed = 1;
            }
        } else if (strcmp(layout, "editor.chtpm") == 0) {
            handle_interact_key(key);
            return 1;
        }
    } else if (strcmp(layout, "editor.chtpm") == 0) {
        if (key == '6') {
            handle_command("SET_NEW_FILE");
            processed = 1;
        } else if (key == '2') {
            handle_command("SET_CLEAR_FILE");
            processed = 1;
        } else {
            handle_interact_key(key);
            return 1;
        }
    } else if (strcmp(layout, "file_menu.chtpm") == 0) {
        if (key == '6') {
            handle_command("SET_NEW_FILE");
            processed = 1;
        } else if (key == '7') {
            if (strcmp(active_file_path, "none") == 0 || strlen(active_file_path) == 0) {
                browser_mode = 1;
                clear_search_query();
                set_file_path_input("projects/agy-text-editor/pieces/document.txt");
                transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
                strcpy(response_buffer, "Specify Save-As path.");
            } else {
                save_to_path(active_file_path);
            }
            processed = 1;
        } else if (key == '8') {
            browser_mode = 1;
            clear_search_query();
            set_file_path_input(active_file_path);
            transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
            strcpy(response_buffer, "Enter Save-As path.");
            processed = 1;
        } else if (key == '9') {
            browser_mode = 0;
            clear_search_query();
            set_file_path_input("");
            transition_to_layout("projects/agy-text-editor/layouts/file_browser.chtpm");
            strcpy(response_buffer, "Select file to load.");
            processed = 1;
        }
    }
    return processed;
}

static void update_gui_state(void) {
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/agy-text-editor/manager/gui_state.txt", project_root);

    char editor_map[8192] = "";
    build_editor_map(editor_map, sizeof(editor_map));

    read_editor_line();
    read_file_path_input();
    read_search_query_input();

    char browser_mode_header_val[256];
    if (browser_mode == 0) {
        strcpy(browser_mode_header_val, "<text label=\"║  MODE: LOAD FILE                            ║\" /><br/>");
    } else {
        strcpy(browser_mode_header_val, "<text label=\"║  MODE: SAVE FILE AS                         ║\" /><br/>");
    }

    char browser_current_dir_line_val[512];
    snprintf(browser_current_dir_line_val, sizeof(browser_current_dir_line_val),
             "<text label=\"║  DIR: %-35.35s ║\" /><br/>", current_dir);

    char autocomplete_markup[8192] = "";
    char suggestions[4][256];
    int num_suggestions = find_autocomplete_matches(file_path_input_buffer, current_dir, suggestions, 4);

    int next_display_num = 3; // search_query is 1, file_path_input is 2

    if (num_suggestions > 0) {
        strcat(autocomplete_markup, "<text label=\"║  SUGGESTIONS:                               ║\" /><br/>");
        for (int i = 0; i < num_suggestions; i++) {
            char display_label[256];
            if (strlen(suggestions[i]) > 28) {
                snprintf(display_label, sizeof(display_label), "...%s", suggestions[i] + strlen(suggestions[i]) - 25);
            } else {
                strcpy(display_label, suggestions[i]);
            }
            char action[512];
            snprintf(action, sizeof(action), "SET_AUTOCOMPLETE:%s", suggestions[i]);
            append_aligned_button_attr(autocomplete_markup, sizeof(autocomplete_markup), display_label, "onClick", action, &next_display_num);
        }
    } else {
        strcat(autocomplete_markup, "<text label=\"║  (Type to see autocompletions)              ║\" /><br/>");
    }

    /* Directory browser tree -- delegates the actual scan to the shared
       dir_browse Op (see this file's own header comment), replacing what
       used to be ~65 lines of inline opendir()/readdir()/qsort() here. */
    char browser_markup[8192] = "";

    if (strcmp(current_dir, ".") != 0 && strlen(current_dir) > 0) {
        char parent_dir[MAX_PATH];
        char *last_slash = strrchr(current_dir, '/');
        if (last_slash) {
            strncpy(parent_dir, current_dir, last_slash - current_dir);
            parent_dir[last_slash - current_dir] = '\0';
        } else {
            strcpy(parent_dir, ".");
        }
        char action[MAX_PATH + 10];
        snprintf(action, sizeof(action), "SET_DIR:%s", parent_dir);
        append_aligned_button_attr(browser_markup, sizeof(browser_markup), "<- BACK", "onClick", action, &next_display_num);
    }

    char full_current_dir[MAX_PATH];
    snprintf(full_current_dir, sizeof(full_current_dir), "%s/%s", project_root, current_dir);
    char listing_path[MAX_PATH];
    snprintf(listing_path, sizeof(listing_path), "%s/projects/agy-text-editor/pieces/browser_listing.txt", project_root);

    {
        char *op_argv[] = { "dir_browse.+x", full_current_dir, search_query_buffer, listing_path, NULL };
        run_op("pieces/system/file_ops/+x/dir_browse.+x", op_argv);
    }

    int items_displayed = 0;
    int total_entries = 0;
    int limit = 8;
    FILE *lf = fopen(listing_path, "r");
    if (lf) {
        char line[512];
        while (fgets(line, sizeof(line), lf)) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            total_entries++;
            if (items_displayed >= limit) continue;

            char *bar1 = strchr(line, '|');
            if (!bar1) continue;
            *bar1 = '\0';
            const char *kind = line;
            char *rest = bar1 + 1;

            if (strcmp(kind, "DIR") == 0) {
                char *name = rest;
                char next_dir_path[MAX_PATH];
                if (strcmp(current_dir, ".") == 0) {
                    snprintf(next_dir_path, sizeof(next_dir_path), "%s", name);
                } else {
                    snprintf(next_dir_path, sizeof(next_dir_path), "%s/%s", current_dir, name);
                }
                char btn_label[300];
                snprintf(btn_label, sizeof(btn_label), "[DIR] %s/", name);
                char display_label[256];
                if (strlen(btn_label) > 28) {
                    snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
                } else {
                    strcpy(display_label, btn_label);
                }
                char action[MAX_PATH + 10];
                snprintf(action, sizeof(action), "SET_DIR:%s", next_dir_path);
                append_aligned_button_attr(browser_markup, sizeof(browser_markup), display_label, "onClick", action, &next_display_num);
            } else {
                char *bar2 = strchr(rest, '|');
                char name[256], size_str_raw[64];
                if (bar2) {
                    size_t nlen = bar2 - rest;
                    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
                    memcpy(name, rest, nlen);
                    name[nlen] = '\0';
                    strncpy(size_str_raw, bar2 + 1, sizeof(size_str_raw) - 1);
                    size_str_raw[sizeof(size_str_raw) - 1] = '\0';
                } else {
                    strncpy(name, rest, sizeof(name) - 1);
                    name[sizeof(name) - 1] = '\0';
                    size_str_raw[0] = '\0';
                }
                long size = atol(size_str_raw);
                char size_str[32];
                if (size < 1024) snprintf(size_str, sizeof(size_str), "%ldB", size);
                else if (size < 1024 * 1024) snprintf(size_str, sizeof(size_str), "%ldKB", size / 1024);
                else snprintf(size_str, sizeof(size_str), "%.1fMB", (float)size / (1024 * 1024));

                char target_file_path[MAX_PATH];
                if (strcmp(current_dir, ".") == 0) {
                    snprintf(target_file_path, sizeof(target_file_path), "%s", name);
                } else {
                    snprintf(target_file_path, sizeof(target_file_path), "%s/%s", current_dir, name);
                }

                char btn_label[300];
                snprintf(btn_label, sizeof(btn_label), "[FIL] %s (%s)", name, size_str);
                char display_label[256];
                if (strlen(btn_label) > 28) {
                    snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
                } else {
                    strcpy(display_label, btn_label);
                }
                char action[MAX_PATH + 20];
                if (browser_mode == 0) {
                    snprintf(action, sizeof(action), "SET_LOAD_FILE:%s", target_file_path);
                } else {
                    snprintf(action, sizeof(action), "SET_PATH:%s", target_file_path);
                }
                append_aligned_button_attr(browser_markup, sizeof(browser_markup), display_label, "onClick", action, &next_display_num);
            }
            items_displayed++;
        }
        fclose(lf);
    }

    if (items_displayed == 0) {
        strcat(browser_markup, "<text label=\"║  [Empty Directory]                         ║\" /><br/>");
    } else {
        int total_remaining = total_entries - items_displayed;
        if (total_remaining > 0) {
            char temp_msg[128];
            snprintf(temp_msg, sizeof(temp_msg), "... and %d more items ...", total_remaining);
            char remaining_line[256];
            snprintf(remaining_line, sizeof(remaining_line), "<text label=\"║  %-40.40s ║\" /><br/>", temp_msg);
            strcat(browser_markup, remaining_line);
        }
    }

    char action_markup[8192] = "";
    if (browser_mode == 0) {
        append_aligned_button_attr(action_markup, sizeof(action_markup), "LOAD FILE", "onClick", "SET_LOAD_ACTION", &next_display_num);
    } else {
        append_aligned_button_attr(action_markup, sizeof(action_markup), "SAVE FILE", "onClick", "SET_SAVE_ACTION", &next_display_num);
    }
    append_aligned_button_attr(action_markup, sizeof(action_markup), "CANCEL", "href", "projects/agy-text-editor/layouts/file_menu.chtpm", &next_display_num);

    char active_file_info_line_val[512];
    snprintf(active_file_info_line_val, sizeof(active_file_info_line_val),
             "<text label=\"║  FILE: %-34.34s ║\" /><br/>", active_file_path);

    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "module_path=projects/agy-text-editor/manager/+x/agy-text-editor_manager.+x\n");
        fprintf(f, "active_layout_id=editor.chtpm\n");
        fprintf(f, "project_id=agy-text-editor\n");
        fprintf(f, "active_project=%s\n", active_file_path);
        fprintf(f, "editor_map=%s\n", editor_map);
        fprintf(f, "input_line=%s\n", input_line_buffer);

        fprintf(f, "active_file_info_line=%s\n", active_file_info_line_val);
        fprintf(f, "editor_response_line=║  %-40.40s ║\n", response_buffer);

        fprintf(f, "browser_mode_header=%s\n", browser_mode_header_val);
        fprintf(f, "browser_current_dir_line=%s\n", browser_current_dir_line_val);
        fprintf(f, "search_query_val=%s\n", search_query_buffer);
        fprintf(f, "file_path_input_val=%s\n", file_path_input_buffer);
        fprintf(f, "autocomplete_suggestions_markup=%s\n", autocomplete_markup);
        fprintf(f, "directory_browser_markup=%s\n", browser_markup);
        fprintf(f, "browser_action_buttons_markup=%s\n", action_markup);

        fprintf(f, "slot_name_input=default\n");
        fprintf(f, "slot_info_line=║  CURRENT SLOT: default                    ║\n");
        fprintf(f, "slot_selection_markup=<text label=\"║  [Slots deprecated]                    ║\" /><br/>\n");
        fclose(f);
    }

    char main_state_path[MAX_PATH];
    snprintf(main_state_path, sizeof(main_state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *rf = fopen(main_state_path, "r");
    char lines[200][MAX_LINE];
    int lc = 0;
    int found_proj = 0;
    if (rf) {
        while (fgets(lines[lc], MAX_LINE, rf) && lc < 190) {
            if (strncmp(lines[lc], "project_id=", 11) == 0) {
                snprintf(lines[lc], MAX_LINE, "project_id=agy-text-editor\n");
                found_proj = 1;
            }
            lc++;
        }
        fclose(rf);
    }
    if (!found_proj && lc < 190) {
        snprintf(lines[lc++], MAX_LINE, "project_id=agy-text-editor\n");
    }
    FILE *wf = fopen(main_state_path, "w");
    if (wf) {
        for (int i = 0; i < lc; i++) fputs(lines[i], wf);
        fclose(wf);
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();

    /* Seed the working document/cursor files on first run if they don't
       exist yet, so a fresh checkout behaves the same as before this
       refactor (a ready, non-empty default document). */
    {
        char doc_path[MAX_PATH], cursor_path[MAX_PATH];
        struct stat st;
        get_document_path(doc_path, sizeof(doc_path));
        get_cursor_path(cursor_path, sizeof(cursor_path));
        if (stat(doc_path, &st) != 0) {
            FILE *f = fopen(doc_path, "w");
            if (f) { fprintf(f, "\n"); fclose(f); }
        }
        if (stat(cursor_path, &st) != 0) {
            reset_cursor();
        }
    }

    update_gui_state();

    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;

    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    int processed = 0;
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: ");
                        char *kpress = strstr(line, "KEY_PRESSED: ");
                        int processed_this_line = 0;
                        if (cmd) {
                            handle_command(trim_str(cmd + 9));
                            processed_this_line = 1;
                        } else if (kpress) {
                            int key = atoi(kpress + 13);
                            if (process_key(key)) processed_this_line = 1;
                        } else {
                            int key = atoi(line);
                            if (key != 0 || line[0] == '0') {
                                if (process_key(key)) processed_this_line = 1;
                            }
                        }

                        if (processed_this_line) {
                            update_gui_state();
                            trigger_render();
                            processed = 1;
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        usleep(16667);
    }

    free(hist_path);
    return 0;
}
