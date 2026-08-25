#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

/*
 * 2026-07-11: full editor port from agy-text-editor's UX (real cursor
 * editing, File Menu, File Browser+search, Save/Load) -- the earlier
 * version of this file only scrolled a fixed, read-only test document
 * (a Phase-1 emoji-rendering test harness, not an actual editor).
 *
 * Per Bible section 11's REUSE RULE, the actual editing/file/directory
 * logic is NOT reimplemented here -- it's the same four standalone Ops
 * under pieces/system/file_ops/ that agy-text-editor's manager was
 * refactored to use (text_edit_key.+x, text_editor_view.+x, file_copy.+x,
 * dir_browse.+x). This file's job is purely orchestration: read this
 * project's own persisted state, dispatch each queued command/keystroke
 * to the right Op, and publish the result through Wraith's two rendering
 * surfaces (manager/gui_state.txt for ASCII's render_project_layout_body(),
 * session/wraith_body.txt for GL's append_project_probe_body() -- the same
 * dual-write pattern settings' own ops uses for its multi-page support).
 *
 * Page switching (editor / file_menu / file_browser) uses PROJECT_PAGE:,
 * the newly-generalized sibling of settings' own SETTINGS_PAGE:
 * mechanism (wraith-alpha_manager.c's route_command() now scopes it to
 * whichever project window is active instead of hardcoding settings).
 *
 * cli_io fields in file_browser.chtpm each declare their own target_id
 * (search_query / file_path_input) -- NOT left to the "input_text"
 * fallback key. Two fields sharing that fallback is a real, confirmed
 * bug found live this same session in agy-text-editor's own
 * file_browser.chtpm (every keystroke into either field forced a full
 * layout re-parse via compose_frame()'s input_changed check, wiping the
 * in-progress buffer) -- fixed there too, and avoided here from the start.
 */

typedef struct {
    char active_page[32];      /* "editor" | "file_menu" | "file_browser" */
    int browser_mode;          /* 0 = load, 1 = save-as */
    char active_file_path[MAX_PATH];
    char current_dir[MAX_PATH];
    char response[256];
    /* 2026-07-12 (agy-vs-wrai.txt bug #2/#3): set by handle_command()'s
       SET_PATH: case (clicking a file in Save-As mode should populate
       this field, NOT load/overwrite the working buffer -- mirrors
       agy-text-editor's set_file_path_input()). Not persisted to
       session/state.txt; only lives for the one write_gui_state_and_body()
       call that follows, which writes it into manager/gui_state.txt's
       file_path_input key so chtpm_parser's cli_io sync picks it up on
       its next re-parse. Empty means "don't override the live typed
       value". */
    char pending_file_path_input[MAX_PATH];
} EditorState;

static char g_root[MAX_PATH];

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

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    snprintf(out, out_sz, "%s/%s", a, b);
}

/* Repo-root-relative -- this ops binary is fork+exec'd by
   wraith-alpha_manager.c's run_active_project_input_op() WITHOUT a
   chdir(), so it inherits the manager's own cwd (confirmed repeatedly
   elsewhere this session: the manager itself opens plenty of its own
   paths as plain "pieces/..." relative strings). */
static int run_op(const char *op_name, char *const op_argv[]) {
    char full_path[MAX_PATH];
    pid_t pid;
    int status;

    snprintf(full_path, sizeof(full_path), "pieces/system/file_ops/+x/%s", op_name);
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
    path_join(out, sz, g_root, "session/document.txt");
}

static void get_cursor_path(char *out, size_t sz) {
    path_join(out, sz, g_root, "session/cursor.txt");
}

static void reset_cursor(void) {
    char path[MAX_PATH];
    FILE *f;
    get_cursor_path(path, sizeof(path));
    f = fopen(path, "w");
    if (f) { fprintf(f, "cursor_x=0\ncursor_y=0\n"); fclose(f); }
}

static void ensure_document_seeded(void) {
    char doc_path[MAX_PATH], cursor_path[MAX_PATH], seed_path[MAX_PATH];
    struct stat st;
    get_document_path(doc_path, sizeof(doc_path));
    get_cursor_path(cursor_path, sizeof(cursor_path));

    if (stat(doc_path, &st) != 0) {
        /* First run: seed from the project's own default test document
           (the same file the old scroll-only version always loaded
           fresh) so a new session doesn't start on a totally blank
           editor. */
        path_join(seed_path, sizeof(seed_path), g_root, "pieces/document.txt");
        char *op_argv[] = { "file_copy.+x", seed_path, doc_path, NULL };
        if (run_op("file_copy.+x", op_argv) != 0) {
            FILE *f = fopen(doc_path, "w");
            if (f) { fprintf(f, "\n"); fclose(f); }
        }
    }
    if (stat(cursor_path, &st) != 0) {
        reset_cursor();
    }
}

static void read_state(EditorState *st) {
    char path[MAX_PATH];
    FILE *f;
    char line[MAX_LINE];

    strcpy(st->active_page, "editor");
    st->browser_mode = 0;
    strcpy(st->active_file_path, "none");
    strcpy(st->current_dir, "projects/wraith-alpha/wraith-projects/wrai-text-editor/pieces");
    strcpy(st->response, "Ready.");
    st->pending_file_path_input[0] = '\0';

    path_join(path, sizeof(path), g_root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "active_page=", 12) == 0) {
            strncpy(st->active_page, trim_str(line + 12), sizeof(st->active_page) - 1);
        } else if (strncmp(line, "browser_mode=", 13) == 0) {
            st->browser_mode = atoi(line + 13);
        } else if (strncmp(line, "active_file_path=", 18) == 0) {
            strncpy(st->active_file_path, trim_str(line + 18), sizeof(st->active_file_path) - 1);
        } else if (strncmp(line, "current_dir=", 12) == 0) {
            strncpy(st->current_dir, trim_str(line + 12), sizeof(st->current_dir) - 1);
        } else if (strncmp(line, "response=", 9) == 0) {
            strncpy(st->response, trim_str(line + 9), sizeof(st->response) - 1);
        }
    }
    fclose(f);
}

/* 2026-07-12 (agy-vs-wrai.txt bug #4, the real root cause of "not
   accepting input"/"arrow key isn't moving the cursor"): this used to
   hardcode is_map_control=0 unconditionally on every write -- the EXACT
   same bug class already found and fixed for piececraft-wraith earlier
   this session (that one hardcoded =1 instead). is_map_control is a
   SHARED field: wraith-alpha_manager.c's set_project_map_control_in_dir()
   (fired by this project's own EDIT TEXT (INTERACT) button / by ESC) is
   the real owner and writes 0 or 1 into this SAME session/state.txt.
   Because this ops runs SYNCHRONOUSLY as part of processing the very
   INTERACT command that sets it to 1 (run_active_project_input_op() is
   called from inside that same route_command() branch), hardcoding 0
   here immediately clobbered it back before the very next keystroke
   arrived -- so INTERACT mode died after exactly one command, and every
   subsequent key (arrows, typed characters) fell outside
   route_input()'s map-control branch entirely and never reached this
   project at all. Fixed the same way piececraft-wraith's save_state()
   was fixed: read the CURRENT value out of the existing state.txt first
   and preserve it. */
static int read_current_is_map_control(void) {
    char path[MAX_PATH];
    FILE *f;
    char line[128];
    int value = 0;
    path_join(path, sizeof(path), g_root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "is_map_control=", 15) == 0) {
            value = atoi(line + 15);
        }
    }
    fclose(f);
    return value;
}

static void write_state(const EditorState *st) {
    char path[MAX_PATH], tmp_path[MAX_PATH];
    FILE *f;
    int is_map_control = read_current_is_map_control();
    path_join(path, sizeof(path), g_root, "session/state.txt");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    fprintf(f, "project_id=wraith-alpha/wraith-projects/wrai-text-editor\n");
    fprintf(f, "is_map_control=%d\n", is_map_control);
    fprintf(f, "active_page=%s\n", st->active_page);
    fprintf(f, "browser_mode=%d\n", st->browser_mode);
    fprintf(f, "active_file_path=%s\n", st->active_file_path);
    fprintf(f, "current_dir=%s\n", st->current_dir);
    fprintf(f, "response=%s\n", st->response);
    fclose(f);
    rename(tmp_path, path);
}

/* target_id-keyed cli_io values chtpm_parser.c's save_to_gui_state()
   writes into THIS project's own manager/gui_state.txt on every
   keystroke (the same generic mechanism window-geom's cli_io fields use
   -- see this session's emit_embedded_line_objects() generalization from
   is_cli_io to has_target_id). Read back here so the ops that use them
   (search filtering, save-as/load path) see the live typed value. */
static void read_gui_state_value(const char *key, char *out, size_t out_sz) {
    char path[MAX_PATH];
    FILE *f;
    char line[512];
    size_t key_len = strlen(key);

    out[0] = '\0';
    path_join(path, sizeof(path), g_root, "manager/gui_state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            strncpy(out, trim_str(line + key_len + 1), out_sz - 1);
            out[out_sz - 1] = '\0';
        }
    }
    fclose(f);
}

static void build_editor_map(char *out, size_t max_sz) {
    char doc_path[MAX_PATH], cursor_path[MAX_PATH], view_path[MAX_PATH];
    FILE *vf;
    char line[512];

    out[0] = '\0';
    get_document_path(doc_path, sizeof(doc_path));
    get_cursor_path(cursor_path, sizeof(cursor_path));
    path_join(view_path, sizeof(view_path), g_root, "session/view.txt");

    char *op_argv[] = { "text_editor_view.+x", doc_path, cursor_path, "40", "8", view_path, NULL };
    run_op("text_editor_view.+x", op_argv);

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

static int get_digits(int num) {
    if (num >= 100) return 3;
    if (num >= 10) return 2;
    return 1;
}

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num;
    int digits = get_digits(num);
    int label_len = (int)strlen(label);
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

static int save_to_path(EditorState *st, const char *rel_path) {
    char full_path[MAX_PATH], doc_path[MAX_PATH];
    strncpy(st->active_file_path, rel_path, sizeof(st->active_file_path) - 1);
    snprintf(full_path, sizeof(full_path), "%s/%s", g_root, rel_path);
    get_document_path(doc_path, sizeof(doc_path));

    char *op_argv[] = { "file_copy.+x", doc_path, full_path, NULL };
    int rc = run_op("file_copy.+x", op_argv);
    if (rc != 0) {
        snprintf(st->response, sizeof(st->response), "Error: cannot write target file");
        return -1;
    }
    snprintf(st->response, sizeof(st->response), "Saved to %s", st->active_file_path);
    return 0;
}

static int load_from_path(EditorState *st, const char *rel_path) {
    char full_path[MAX_PATH], doc_path[MAX_PATH];
    strncpy(st->active_file_path, rel_path, sizeof(st->active_file_path) - 1);
    snprintf(full_path, sizeof(full_path), "%s/%s", g_root, rel_path);
    get_document_path(doc_path, sizeof(doc_path));

    char *op_argv[] = { "file_copy.+x", full_path, doc_path, NULL };
    int rc = run_op("file_copy.+x", op_argv);
    if (rc != 0) {
        snprintf(st->response, sizeof(st->response), "Error: file not found");
        return -1;
    }
    reset_cursor();
    snprintf(st->response, sizeof(st->response), "Loaded %s", st->active_file_path);
    return 0;
}

/* Dispatches a PROJECT_ACTION:-suffixed command (arrives here as the raw
   suffix, e.g. "NEW_FILE", "SET_DIR:foo") -- mirrors agy-text-editor's
   handle_command() exactly, adapted to Wraith's PROJECT_PAGE:-based page
   switching instead of href/layout_changed.txt. */
static void handle_command(EditorState *st, const char *cmd) {
    if (strcmp(cmd, "NEW_FILE") == 0) {
        char doc_path[MAX_PATH];
        get_document_path(doc_path, sizeof(doc_path));
        FILE *f = fopen(doc_path, "w");
        if (f) {
            fclose(f);
            strcpy(st->active_file_path, "none");
            strcpy(st->response, "New file created (buffer cleared).");
            reset_cursor();
        } else {
            strcpy(st->response, "Error creating new file.");
        }
    } else if (strcmp(cmd, "CLEAR_FILE") == 0) {
        char doc_path[MAX_PATH];
        get_document_path(doc_path, sizeof(doc_path));
        FILE *f = fopen(doc_path, "w");
        if (f) {
            fclose(f);
            strcpy(st->response, "File cleared.");
            reset_cursor();
        } else {
            strcpy(st->response, "Error clearing file.");
        }
    } else if (strcmp(cmd, "SAVE") == 0) {
        if (strcmp(st->active_file_path, "none") == 0) {
            st->browser_mode = 1;
            strcpy(st->current_dir, "projects/wraith-alpha/wraith-projects/wrai-text-editor/pieces");
            strcpy(st->active_page, "file_browser");
            strcpy(st->response, "Specify Save-As path.");
        } else {
            save_to_path(st, st->active_file_path);
        }
    } else if (strcmp(cmd, "SAVE_AS") == 0) {
        st->browser_mode = 1;
        strcpy(st->active_page, "file_browser");
        strcpy(st->response, "Enter Save-As path.");
    } else if (strcmp(cmd, "LOAD_MENU") == 0) {
        st->browser_mode = 0;
        strcpy(st->active_page, "file_browser");
        strcpy(st->response, "Select file to load.");
    } else if (strcmp(cmd, "BROWSER_ACTION") == 0) {
        char file_path_input[MAX_PATH];
        read_gui_state_value("file_path_input", file_path_input, sizeof(file_path_input));
        if (file_path_input[0] == '\0') {
            strcpy(st->response, "Enter a file path first.");
            return;
        }
        if (st->browser_mode == 0) load_from_path(st, file_path_input);
        else save_to_path(st, file_path_input);
        strcpy(st->active_page, "file_menu");
    } else if (strncmp(cmd, "SET_DIR:", 8) == 0) {
        strncpy(st->current_dir, cmd + 8, sizeof(st->current_dir) - 1);
        if (strlen(st->current_dir) == 0) strcpy(st->current_dir, ".");
    } else if (strncmp(cmd, "SET_LOAD_FILE:", 14) == 0) {
        load_from_path(st, cmd + 14);
        strcpy(st->active_page, "file_menu");
    } else if (strncmp(cmd, "SET_PATH:", 9) == 0) {
        /* agy-vs-wrai.txt bug #2/#3: clicking a file in Save-As mode
           must only populate the path field, never load/overwrite the
           working buffer (that's what SET_LOAD_FILE: is for, and it's
           reserved for LOAD mode -- see build_directory_browser_markup()'s
           browser_mode branch). Stash the value; write_gui_state_and_body()
           publishes it into gui_state.txt's file_path_input key so
           chtpm_parser's cli_io sync picks it up as the field's displayed
           value on its next re-parse. */
        strncpy(st->pending_file_path_input, cmd + 9, sizeof(st->pending_file_path_input) - 1);
    }
}

static void apply_editor_key(int key) {
    char doc_path[MAX_PATH], cursor_path[MAX_PATH], key_str[16];
    get_document_path(doc_path, sizeof(doc_path));
    get_cursor_path(cursor_path, sizeof(cursor_path));
    snprintf(key_str, sizeof(key_str), "%d", key);
    char *op_argv[] = { "text_edit_key.+x", doc_path, cursor_path, key_str, NULL };
    run_op("text_edit_key.+x", op_argv);
}

/* Shared by both the "KEY_PRESSED: " and bare-number history-line
   shapes (see process_history()'s own comments for why both exist).
   editor.chtpm's INTERACT-mode keys edit the document directly.
   file_browser.chtpm's cli_io fields handle their OWN backspace/typing
   generically inside chtpm_parser (target_id-keyed gui_state.txt, no
   help needed from this project) -- but Enter specifically needs a
   nudge: chtpm_parser's cli_io Enter-handling saves the buffer then
   injects a raw 13 (mirrors agy-text-editor's own
   read_file_path_input()-then-SET_LOAD_ACTION/SET_SAVE_ACTION dispatch
   on Enter), which without this would just vanish -- the field's value
   was saved, but nothing ever acted on it. Triggering BROWSER_ACTION
   here is safe even if Enter came from the search field instead of the
   path field: BROWSER_ACTION reads file_path_input itself and reports
   "Enter a file path first." if it's empty, matching agy's own
   graceful no-op shape for an unrelated Enter. */
static void dispatch_raw_key(EditorState *st, int key) {
    if (strcmp(st->active_page, "editor") == 0) {
        apply_editor_key(key);
    } else if (strcmp(st->active_page, "file_browser") == 0 && (key == 10 || key == 13)) {
        handle_command(st, "BROWSER_ACTION");
    }
}

static void process_history(EditorState *st) {
    char hist_path[MAX_PATH], cursor_file_path[MAX_PATH];
    FILE *history;
    long cursor, end_pos;
    char line[1024];

    path_join(hist_path, sizeof(hist_path), g_root, "session/history.txt");
    path_join(cursor_file_path, sizeof(cursor_file_path), g_root, "session/history.cursor");

    history = fopen(hist_path, "r");
    if (!history) return;

    cursor = 0;
    {
        FILE *cf = fopen(cursor_file_path, "r");
        if (cf) {
            if (fscanf(cf, "%ld", &cursor) != 1) cursor = 0;
            fclose(cf);
        }
    }

    fseek(history, 0, SEEK_END);
    end_pos = ftell(history);
    if (cursor < 0 || cursor > end_pos) cursor = 0;
    fseek(history, cursor, SEEK_SET);

    while (fgets(line, sizeof(line), history)) {
        char *cmd = strstr(line, "COMMAND: ");
        char *kpress = strstr(line, "KEY_PRESSED: ");
        if (cmd) {
            handle_command(st, trim_str(cmd + 9));
        } else if (kpress) {
            int key = atoi(kpress + 13);
            if (key > 0) {
                dispatch_raw_key(st, key);
            }
        } else {
            /* agy-vs-wrai.txt bug #1 (the "not accepting input" bug):
               chtpm_parser.c's inject_raw_key() -- called for every
               keystroke while an onClick="INTERACT" element is active,
               PRIORITY 1 branch, `if (interact_history_path[0] != '\0')`
               -- writes ONLY the bare numeric key code ("%d\n", code),
               no "KEY_PRESSED: " prefix. editor.chtpm's own
               <interact src=".../session/history.txt" /> means every
               character typed while editing lands here as a bare
               number. agy-text-editor's own main loop has always had
               this exact fallback (`int key = atoi(line); if (key != 0
               || line[0]=='0') process_key(key);`); this file never
               did, so every typed keystroke was silently dropped before
               it ever reached text_edit_key.+x. */
            int key = atoi(line);
            if (key != 0 || line[0] == '0') {
                dispatch_raw_key(st, key);
            }
        }
    }

    cursor = ftell(history);
    fclose(history);

    {
        FILE *cf = fopen(cursor_file_path, "w");
        if (cf) { fprintf(cf, "%ld\n", cursor); fclose(cf); }
    }
}

static void build_directory_browser_markup(EditorState *st, char *out, size_t max_sz, int *next_display_num) {
    char full_dir[MAX_PATH], listing_path[MAX_PATH], search_query[MAX_LINE];
    FILE *lf;
    char line[512];
    int items_displayed = 0, total_entries = 0;
    const int limit = 8;

    out[0] = '\0';
    read_gui_state_value("search_query", search_query, sizeof(search_query));

    if (strcmp(st->current_dir, ".") != 0 && strlen(st->current_dir) > 0) {
        char parent_dir[MAX_PATH];
        char *last_slash = strrchr(st->current_dir, '/');
        if (last_slash) {
            size_t plen = last_slash - st->current_dir;
            if (plen >= sizeof(parent_dir)) plen = sizeof(parent_dir) - 1;
            memcpy(parent_dir, st->current_dir, plen);
            parent_dir[plen] = '\0';
        } else {
            strcpy(parent_dir, ".");
        }
        char action[MAX_PATH + 24];
        snprintf(action, sizeof(action), "PROJECT_ACTION:SET_DIR:%s", parent_dir);
        append_aligned_button_attr(out, max_sz, "<- BACK", "onClick", action, next_display_num);
    }

    snprintf(full_dir, sizeof(full_dir), "%s/%s", g_root, st->current_dir);
    path_join(listing_path, sizeof(listing_path), g_root, "session/browser_listing.txt");
    {
        char *op_argv[] = { "dir_browse.+x", full_dir, search_query, listing_path, NULL };
        run_op("dir_browse.+x", op_argv);
    }

    lf = fopen(listing_path, "r");
    if (lf) {
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
                char next_dir_path[MAX_PATH];
                if (strcmp(st->current_dir, ".") == 0) {
                    snprintf(next_dir_path, sizeof(next_dir_path), "%s", rest);
                } else {
                    snprintf(next_dir_path, sizeof(next_dir_path), "%s/%s", st->current_dir, rest);
                }
                char btn_label[300];
                snprintf(btn_label, sizeof(btn_label), "[DIR] %s/", rest);
                char display_label[256];
                if (strlen(btn_label) > 28) {
                    snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
                } else {
                    strcpy(display_label, btn_label);
                }
                char action[MAX_PATH + 24];
                snprintf(action, sizeof(action), "PROJECT_ACTION:SET_DIR:%s", next_dir_path);
                append_aligned_button_attr(out, max_sz, display_label, "onClick", action, next_display_num);
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
                if (strcmp(st->current_dir, ".") == 0) {
                    snprintf(target_file_path, sizeof(target_file_path), "%s", name);
                } else {
                    snprintf(target_file_path, sizeof(target_file_path), "%s/%s", st->current_dir, name);
                }
                char btn_label[300];
                snprintf(btn_label, sizeof(btn_label), "[FIL] %s (%s)", name, size_str);
                char display_label[256];
                if (strlen(btn_label) > 28) {
                    snprintf(display_label, sizeof(display_label), "...%s", btn_label + strlen(btn_label) - 25);
                } else {
                    strcpy(display_label, btn_label);
                }
                /* agy-vs-wrai.txt bug #2: was always SET_LOAD_FILE:
                   regardless of mode -- in Save-As mode, clicking an
                   existing file immediately overwrote the working
                   buffer with that file's contents instead of just
                   populating the path field. Mirrors agy-text-editor's
                   own update_gui_state() branch exactly. */
                char action[MAX_PATH + 32];
                if (st->browser_mode == 0) {
                    snprintf(action, sizeof(action), "PROJECT_ACTION:SET_LOAD_FILE:%s", target_file_path);
                } else {
                    snprintf(action, sizeof(action), "PROJECT_ACTION:SET_PATH:%s", target_file_path);
                }
                append_aligned_button_attr(out, max_sz, display_label, "onClick", action, next_display_num);
            }
            items_displayed++;
        }
        fclose(lf);
    }

    if (items_displayed == 0) {
        strncat(out, "<text label=\"║  [Empty Directory]                         ║\" /><br/>", max_sz - strlen(out) - 1);
    } else if (total_entries - items_displayed > 0) {
        char temp_msg[128];
        snprintf(temp_msg, sizeof(temp_msg), "... and %d more items ...", total_entries - items_displayed);
        char remaining_line[256];
        snprintf(remaining_line, sizeof(remaining_line), "<text label=\"║  %-40.40s ║\" /><br/>", temp_msg);
        strncat(out, remaining_line, max_sz - strlen(out) - 1);
    }
}

/* Publishes VALUES to manager/gui_state.txt (ASCII, via
   render_project_layout_body()) and the SAME substituted markup into
   session/wraith_body.txt (GL, via append_project_probe_body()) -- the
   exact dual-write pattern settings' own ops uses. active_layout picks
   which .chtpm this page's values get merged into. */
/* 2026-07-11: session/wraith_body.txt (GL's read path, via
   append_project_probe_body()) is read LINE BY LINE with a 256-byte
   buffer -- each PHYSICAL line becomes one markup chunk. editor_map/
   directory_browser_markup, by contrast, are built as one continuous
   string with multiple <br/>-terminated chunks joined WITHOUT embedded
   newlines (correct for ${var} substitution into a .chtpm template,
   where chtpm_parser processes one substituted source line at a time
   regardless of how many tags it contains). Writing that same
   single-line blob directly into wraith_body.txt overflowed GL's
   256-byte read buffer, truncating mid-tag and desyncing every line
   read after it -- confirmed live as the reason GL's INTERACT button
   never activated and nothing in the editor was clickable. This splits
   on "<br/>" boundaries and writes each chunk as its own physical line,
   matching the exact real-newlines-for-wraith_body.txt-vs-<br/>-joined-
   for-gui_state.txt distinction this session already established while
   building settings' multi-page support -- missed here on the first
   pass. */
static void write_markup_as_lines(FILE *out, const char *markup) {
    const char *p = markup;
    const char *marker = "<br/>";
    size_t marker_len = strlen(marker);
    while (*p) {
        const char *hit = strstr(p, marker);
        if (!hit) {
            if (*p) fprintf(out, "%s\n", p);
            break;
        }
        size_t chunk_len = (size_t)(hit - p) + marker_len;
        fprintf(out, "%.*s\n", (int)chunk_len, p);
        p = hit + marker_len;
    }
}

/* 2026-07-12 (agy-vs-wrai.txt bug #5): session/scene.objects.pdl was a
   leftover, hand-written, all-nav=0 status panel from the original
   Phase-1 emoji test-harness version of this project ("Status: Ready
   with emoji rendering enabled", "Font: Noto Color Emoji", etc.). This
   project's editor uses ONLY the wraith_body.txt dual-write mechanism
   now, never scene.objects.pdl -- but nothing ever cleared the old
   file, and append_project_scene_objects() (wraith-alpha_manager.c)
   unconditionally appends whatever it finds there on top of the body
   content for GL, regardless of what wraith_body.txt says. Confirmed
   live: GL was rendering this stale panel overlaid on the real editor
   content while ASCII correctly showed only the current UI -- the
   ASCII/GL mismatch reported live. Explicitly rewriting this file empty
   (a comment only) on every run means it can never go stale like this
   again, matching how every other actively-maintained Wraith project's
   own ops owns and rewrites its own scene.objects.pdl each time. */
static void write_empty_scene_objects(void) {
    char path[MAX_PATH], tmp_path[MAX_PATH];
    FILE *f;
    path_join(path, sizeof(path), g_root, "session/scene.objects.pdl");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    fprintf(f, "# wrai-text-editor uses wraith_body.txt for all content --\n");
    fprintf(f, "# intentionally empty, kept rewritten so it can't go stale.\n");
    fclose(f);
    rename(tmp_path, path);
}

static void write_gui_state_and_body(EditorState *st) {
    char gui_state_path[MAX_PATH], gui_state_tmp[MAX_PATH];
    char body_path[MAX_PATH], body_tmp[MAX_PATH];
    FILE *gf, *bf;
    char editor_map[8192] = "";
    char directory_browser_markup[8192] = "";
    int next_display_num = 3;
    char active_layout[MAX_PATH];

    snprintf(active_layout, sizeof(active_layout),
        "projects/wraith-alpha/wraith-projects/wrai-text-editor/layouts/%s.chtpm", st->active_page);

    if (strcmp(st->active_page, "editor") == 0) {
        build_editor_map(editor_map, sizeof(editor_map));
    } else if (strcmp(st->active_page, "file_browser") == 0) {
        build_directory_browser_markup(st, directory_browser_markup, sizeof(directory_browser_markup), &next_display_num);
    }

    path_join(gui_state_path, sizeof(gui_state_path), g_root, "manager/gui_state.txt");
    snprintf(gui_state_tmp, sizeof(gui_state_tmp), "%s.tmp", gui_state_path);
    gf = fopen(gui_state_tmp, "w");
    if (gf) {
        fprintf(gf, "module_path=projects/wraith-alpha/wraith-projects/wrai-text-editor/manager/+x/wrai-text-editor_manager.+x\n");
        fprintf(gf, "app_title=WRAI TEXT EDITOR\n");
        fprintf(gf, "project_id=wraith-alpha/wraith-projects/wrai-text-editor\n");
        fprintf(gf, "active_layout=%s\n", active_layout);
        fprintf(gf, "active_file_path=%s\n", st->active_file_path);
        fprintf(gf, "current_dir=%s\n", st->current_dir);
        fprintf(gf, "response=%s\n", st->response);
        fprintf(gf, "editor_map=%s\n", editor_map);
        fprintf(gf, "browser_mode_header=%s\n", st->browser_mode == 0 ? "║  MODE: LOAD FILE" : "║  MODE: SAVE FILE AS");
        fprintf(gf, "directory_browser_markup=%s\n", directory_browser_markup);
        /* file_browser.chtpm's cli_io tags use ${search_query_val}/
           ${file_path_input_val} as their empty-field placeholder label
           -- unresolved without these, chtpm_parser would show the
           literal "${...}" text instead of a blank/hint placeholder.
           Not one of the three functional bugs in agy-vs-wrai.txt, but
           directly adjacent to the cli_io plumbing being fixed here, so
           closed in the same pass. */
        fprintf(gf, "search_query_val=type to filter\n");
        fprintf(gf, "file_path_input_val=path\n");
        if (st->pending_file_path_input[0]) {
            /* agy-vs-wrai.txt bug #2/#3: publishes SET_PATH:'s value so
               chtpm_parser's cli_io sync (which restores a field's
               input_buffer from this exact target_id-keyed gui_state
               key on reparse) shows it as the file_path_input field's
               current value, mirroring agy's set_file_path_input(). */
            fprintf(gf, "file_path_input=%s\n", st->pending_file_path_input);
        }
        fclose(gf);
        rename(gui_state_tmp, gui_state_path);
    }

    /* GL fallback: the same substituted markup, written as real
       <-prefixed lines so append_project_probe_body() parses them as
       genuine buttons/text (Pitfall #56), matching settings' own
       dual-write. Kept intentionally minimal (page title + response +
       the same real markup already computed above) rather than
       hand-duplicating the entire per-page layout twice -- editor_map/
       directory_browser_markup already ARE real markup and get embedded
       directly. */
    path_join(body_path, sizeof(body_path), g_root, "session/wraith_body.txt");
    snprintf(body_tmp, sizeof(body_tmp), "%s.tmp", body_path);
    bf = fopen(body_tmp, "w");
    if (bf) {
        fprintf(bf, "<text label=\"WRAI TEXT EDITOR\" /><br/>\n");
        if (strcmp(st->active_page, "editor") == 0) {
            fprintf(bf, "<text label=\"ACTIVE: %s\" /><br/>\n", st->active_file_path);
            fprintf(bf, "<button label=\"EDIT TEXT (INTERACT)\" onClick=\"INTERACT\" /><br/>\n");
            write_markup_as_lines(bf, editor_map);
            fprintf(bf, "<button label=\"NEW FILE\" onClick=\"PROJECT_ACTION:NEW_FILE\" /><br/>\n");
            fprintf(bf, "<button label=\"CLEAR FILE\" onClick=\"PROJECT_ACTION:CLEAR_FILE\" /><br/>\n");
            fprintf(bf, "<button label=\"FILE MENU\" onClick=\"PROJECT_PAGE:file_menu\" /><br/>\n");
        } else if (strcmp(st->active_page, "file_menu") == 0) {
            fprintf(bf, "<text label=\"ACTIVE: %s\" /><br/>\n", st->active_file_path);
            fprintf(bf, "<button label=\"NEW FILE\" onClick=\"PROJECT_ACTION:NEW_FILE\" /><br/>\n");
            fprintf(bf, "<button label=\"SAVE FILE\" onClick=\"PROJECT_ACTION:SAVE\" /><br/>\n");
            fprintf(bf, "<button label=\"SAVE AS...\" onClick=\"PROJECT_ACTION:SAVE_AS\" /><br/>\n");
            fprintf(bf, "<button label=\"LOAD FILE...\" onClick=\"PROJECT_ACTION:LOAD_MENU\" /><br/>\n");
            fprintf(bf, "<button label=\"BACK TO EDITOR\" onClick=\"PROJECT_PAGE:editor\" /><br/>\n");
        } else if (strcmp(st->active_page, "file_browser") == 0) {
            fprintf(bf, "<text label=\"DIR: %s\" /><br/>\n", st->current_dir);
            fprintf(bf, "<text label=\"SEARCH: \" /><cli_io id=\"search_query\" target_id=\"search_query\" label=\"search\" /><br/>\n");
            fprintf(bf, "<text label=\"FILE: \" /><cli_io id=\"file_path_input\" target_id=\"file_path_input\" label=\"path\" /><br/>\n");
            write_markup_as_lines(bf, directory_browser_markup);
            fprintf(bf, "<button label=\"LOAD/SAVE\" onClick=\"PROJECT_ACTION:BROWSER_ACTION\" /><br/>\n");
            fprintf(bf, "<button label=\"CANCEL\" onClick=\"PROJECT_PAGE:file_menu\" /><br/>\n");
        }
        fprintf(bf, "<text label=\"%s\" /><br/>\n", st->response);
        fclose(bf);
        rename(body_tmp, body_path);
    }
}

int main(int argc, char *argv[]) {
    EditorState state;

    if (argc < 2) return 2;
    strncpy(g_root, argv[1], sizeof(g_root) - 1);
    g_root[sizeof(g_root) - 1] = '\0';

    ensure_document_seeded();
    read_state(&state);

    /* PROJECT_PAGE: transitions arrive via session/state_changed.txt
       (route_command()'s generalized handler), same convention settings
       uses -- pick up the latest requested page before processing this
       invocation's own history/commands.

       2026-07-11: this is a ONE-SHOT event queue, not a persisted value
       -- it must be cleared immediately after being consumed here, or
       it silently re-applies on every future invocation for the rest of
       the window's life, clobbering state.active_page right back to
       whatever page was last explicitly requested via PROJECT_PAGE:
       even after the user has since navigated elsewhere (e.g. via
       PROJECT_ACTION:SAVE_AS or BROWSER_ACTION). Confirmed live:
       pressing Enter on file_browser's Save-As path field never fired
       BROWSER_ACTION because this block had already overwritten
       active_page back to "file_menu" (a stale, never-cleared queue
       entry from an earlier FILE MENU click) BEFORE
       dispatch_raw_key()'s "== file_browser" check ran -- same bug
       class as bug #4's is_map_control clobber, different field. */
    {
        char changed_path[MAX_PATH];
        FILE *f;
        char last_page[64] = "";
        char line[128];
        path_join(changed_path, sizeof(changed_path), g_root, "session/state_changed.txt");
        f = fopen(changed_path, "r");
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                char *trimmed = trim_str(line);
                if (trimmed[0]) strncpy(last_page, trimmed, sizeof(last_page) - 1);
            }
            fclose(f);
        }
        if (last_page[0]) {
            strncpy(state.active_page, last_page, sizeof(state.active_page) - 1);
            f = fopen(changed_path, "w");
            if (f) fclose(f);
        }
    }

    process_history(&state);
    write_state(&state);
    write_empty_scene_objects();
    write_gui_state_and_body(&state);

    return 0;
}
