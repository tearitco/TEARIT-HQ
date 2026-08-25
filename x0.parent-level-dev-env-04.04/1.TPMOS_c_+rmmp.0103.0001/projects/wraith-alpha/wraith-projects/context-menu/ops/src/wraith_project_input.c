#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 2048
#define MAX_METHODS 32

/*
 * Real hot-path logic for the "context-menu" Wraith project. Invoked
 * fresh on every KEY: press by wraith-alpha_manager.c's
 * run_active_project_input_op() -- same fork/exec pattern as
 * settings/ops/src/wraith_project_input.c, which this mirrors (init-only
 * manager, all real work here, no polling loop).
 *
 * BASICS PASS ONLY (2026-07-13, see context-st8.txt): this build handles
 * ONLY the method-list screen (session/cm_target.txt already set before
 * this window was launched -- see wraith-alpha_manager.c's
 * open_context_menu_for_active_window()). The picker screen (cold
 * launcher-click, cm_target.txt empty) is NOT implemented yet -- shows a
 * plain "no target" message instead of a real picker. Emoji picker,
 * create-folder, change-background are not wired at all in this pass.
 */

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (a[0] && a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static char *trim_str(char *s) {
    char *end;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static int read_last_key_pressed(const char *root) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;
    int last_key = -1;

    path_join(path, sizeof(path), root, "session/history.txt");
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "KEY_PRESSED:");
        if (p) {
            p += strlen("KEY_PRESSED:");
            while (*p && isspace((unsigned char)*p)) p++;
            last_key = atoi(p);
        }
    }
    fclose(f);
    return last_key;
}

/* Copied from settings' own ops file, but that copy has a real bug this
 * project's own use of repo_root (for invoking pdl_reader.+x, which
 * settings' ops never needed to do) actually exposed: wraith-alpha_manager.c
 * invokes this op with an ABSOLUTE project_dir on every KEY:/PROJECT_ACTION:
 * dispatch (run_active_project_input_op() -> active_project_dir() ->
 * project_dir_for_window(), which builds "%s/projects/.../%s" from
 * g_project_root, always absolute) -- but the FIRST invocation, fired
 * directly by this project's own manager (context-menu_manager.c's
 * hardcoded relative PROJECT_DIR macro, same pattern settings_manager.c
 * uses), passes a RELATIVE path with no leading "/projects/..." for the
 * needle below to ever match. The original code silently fell back to
 * `repo_root = project_dir` (wrong -- not the repo root at all) whenever
 * that happened, confirmed live 2026-07-13: pdl_reader.+x got invoked at
 * a nonexistent path on the very first render, silently failing (stderr
 * redirected to /dev/null) and showing "no methods declared" even though
 * the target project genuinely has METHOD entries. Fixed by also
 * recognizing the relative form -- this whole process tree's cwd is
 * always the real repo root (nothing here ever chdir()s), so a relative
 * project_dir starting with "projects/wraith-alpha/wraith-projects/"
 * means repo_root is simply ".". */
static void derive_repo_root(const char *project_dir, char *repo_root, size_t repo_root_sz) {
    const char *abs_needle = "/projects/wraith-alpha/wraith-projects/";
    const char *rel_needle = "projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_dir, abs_needle);
    if (p) {
        size_t len = (size_t)(p - project_dir);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_dir, len);
        repo_root[len] = '\0';
        return;
    }
    if (strncmp(project_dir, rel_needle, strlen(rel_needle)) == 0) {
        snprintf(repo_root, repo_root_sz, ".");
        return;
    }
    snprintf(repo_root, repo_root_sz, "%s", project_dir);
}

/* cm_target.txt is 3 lines as of 2026-07-13 (was 1): project_id,
 * window id (unused here -- only close, on the manager side, needs it
 * to disambiguate multiple open instances), interact_mode (0/1 --
 * whether the target window was in map_control/INTERACT when the menu
 * was opened; see wraith-alpha_manager.c's
 * open_context_menu_for_active_window() for where it's computed). */
static void read_target(const char *root, char *out, size_t out_sz, int *out_interact_mode) {
    char path[MAX_PATH_LEN];
    char line[256];
    FILE *f;
    int line_no = 0;

    out[0] = '\0';
    if (out_interact_mode) *out_interact_mode = 0;
    path_join(path, sizeof(path), root, "session/cm_target.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        line_no++;
        if (line_no == 1) {
            strncpy(out, trimmed, out_sz - 1);
            out[out_sz - 1] = '\0';
        } else if (line_no == 3 && out_interact_mode) {
            *out_interact_mode = atoi(trimmed);
        }
    }
    fclose(f);
}

/* Runs pdl_reader.+x <target> list_methods, splits its newline-separated
 * output into `methods`. Returns the count (0 if the target has no
 * METHOD section, or doesn't resolve at all -- both look the same to the
 * caller, "NONE"/exit 1, per pdl_reader.c's own main()).
 *
 * FILTERED by interact_mode as of 2026-07-13 (emoji picker,
 * context-st8.txt): an "emoji_" name prefix is this pass's convention
 * for "only makes sense while actively typing" METHODs -- when
 * interact_mode is true, ONLY emoji_-prefixed methods are kept (resize/
 * close don't make sense mid-typing); when false, emoji_-prefixed ones
 * are EXCLUDED (don't clutter the normal resize/close menu). Both
 * directions filtered, not just one, so a project can freely mix normal
 * and emoji_ METHODs in the same project.pdl without the wrong ones
 * leaking into either context. */
static int list_target_methods(const char *repo_root, const char *target,
                                char methods[MAX_METHODS][64], int interact_mode) {
    char cmd[MAX_PATH_LEN];
    FILE *p;
    char line[256];
    int count = 0;

    snprintf(cmd, sizeof(cmd), "%s/pieces/system/pdl/+x/pdl_reader.+x %s list_methods 2>/dev/null",
             repo_root, target);
    p = popen(cmd, "r");
    if (!p) return 0;
    while (count < MAX_METHODS && fgets(line, sizeof(line), p)) {
        char *trimmed;
        int is_emoji_method;
        line[strcspn(line, "\r\n")] = '\0';
        trimmed = trim_str(line);
        if (!trimmed[0] || strcmp(trimmed, "NONE") == 0) continue;
        is_emoji_method = (strncmp(trimmed, "emoji_", 6) == 0);
        if (is_emoji_method != (interact_mode != 0)) continue;
        strncpy(methods[count], trimmed, sizeof(methods[0]) - 1);
        methods[count][sizeof(methods[0]) - 1] = '\0';
        count++;
    }
    pclose(p);
    return count;
}

/* Runs pdl_reader.+x <target> get_method <name>, returns the handler
 * string (or empty on failure). */
static void get_target_method_handler(const char *repo_root, const char *target,
                                       const char *name, char *out, size_t out_sz) {
    char cmd[MAX_PATH_LEN];
    FILE *p;
    char line[512];

    out[0] = '\0';
    snprintf(cmd, sizeof(cmd), "%s/pieces/system/pdl/+x/pdl_reader.+x %s get_method %s 2>/dev/null",
             repo_root, target, name);
    p = popen(cmd, "r");
    if (!p) return;
    if (fgets(line, sizeof(line), p)) {
        char *trimmed;
        line[strcspn(line, "\r\n")] = '\0';
        trimmed = trim_str(line);
        if (strcmp(trimmed, "NOT_FOUND") != 0) {
            strncpy(out, trimmed, out_sz - 1);
            out[out_sz - 1] = '\0';
        }
    }
    pclose(p);
}

/* DESKTOP_ACTION:-prefixed handlers are for actions that already exist as
 * FUNCTIONS INSIDE wraith-alpha_manager.c (resize, close, ...) -- this
 * child process cannot call those directly, so it appends a COMMAND: line
 * to the SAME desktop_actions.txt queue every other DESKTOP_ACTION:
 * dispatch in this codebase already uses (confirmed live this session --
 * the parent's 60Hz loop's process_history_file() picks it up on its next
 * tick and runs it through route_command() normally). Everything else is
 * treated as fo-menu-sys.md's plain literal-executable-path convention
 * and fork+exec'd directly, splitting on whitespace for argv. */
static void dispatch_handler(const char *repo_root, const char *handler) {
    if (strncmp(handler, "DESKTOP_ACTION:", 15) == 0) {
        char queue_path[MAX_PATH_LEN];
        FILE *f;
        path_join(queue_path, sizeof(queue_path), repo_root,
                  "projects/wraith-alpha/session/desktop_actions.txt");
        f = fopen(queue_path, "a");
        if (f) {
            fprintf(f, "COMMAND: %s\n", handler);
            fclose(f);
        }
        return;
    }

    {
        char argv_buf[512];
        char *argv_list[16];
        int argc_count = 0;
        char *tok;
        pid_t pid;

        strncpy(argv_buf, handler, sizeof(argv_buf) - 1);
        argv_buf[sizeof(argv_buf) - 1] = '\0';

        tok = strtok(argv_buf, " ");
        while (tok && argc_count < 15) {
            argv_list[argc_count++] = tok;
            tok = strtok(NULL, " ");
        }
        argv_list[argc_count] = NULL;
        if (argc_count == 0) return;

        pid = fork();
        if (pid == 0) {
            execv(argv_list[0], argv_list);
            _exit(127);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

/* Resolves the DISPLAY LABEL for one method button. Bug found live
 * 2026-07-13 (reported directly): showing the bare method NAME
 * ("emoji_grin") as the button label is technically correct but useless
 * for an emoji picker -- the user wants to see the actual glyph (\xf0\x9f\x98\x80),
 * not its internal name. If the handler is an insert_emoji action, the
 * emoji itself (embedded in the handler string, after the colon -- same
 * place dispatch_handler() reads it from) IS the natural label. Falls
 * back to the bare method name for everything else (resize, close, ...),
 * where the name already IS the meaningful label. */
static void resolve_display_label(const char *repo_root, const char *target,
                                   const char *method_name, char *out, size_t out_sz) {
    char handler[512];
    const char *emoji_prefix = "DESKTOP_ACTION:open_context_menu_insert_emoji:";

    get_target_method_handler(repo_root, target, method_name, handler, sizeof(handler));
    if (strncmp(handler, emoji_prefix, strlen(emoji_prefix)) == 0) {
        strncpy(out, handler + strlen(emoji_prefix), out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    strncpy(out, method_name, out_sz - 1);
    out[out_sz - 1] = '\0';
}

/* Writes manager/gui_state.txt (cm_methods_markup=... single-key, fresh
 * overwrite -- no merge-preserving needed, this project has no other
 * persisted gui_state keys yet) and session/wraith_body.txt (what
 * append_project_probe_body() actually reads for an embedded Window's
 * body -- same dual-write shape window-geom's write_wraith_body() uses,
 * see that file's own header comment for why both are written).
 *
 * ONE BUTTON PER PHYSICAL LINE as of 2026-07-13 -- the previous version
 * concatenated ALL buttons into one giant string (<button.../><br/>
 * repeated, no real newline between them) and wrote it as a single
 * fprintf("%s\n", markup) line. Bug found live: wraith-alpha_manager.c's
 * project_probe_body_lines() reads wraith_body.txt through a 256-BYTE
 * fgets() buffer per line -- 10 buttons concatenated is 500+ bytes, so
 * that single logical line got split across multiple 256-byte fgets()
 * reads, each continuation chunk starting mid-tag (not with '<'), which
 * made emit_embedded_line_objects() treat it as plain text instead of
 * markup -- exactly the reported symptom (literal "<br/>" and "onClick="
 * text visible in GL). Fixed by giving each button its own real '\n',
 * matching window-geom's write_wraith_body() own multi-fprintf-calls
 * pattern (that file's comment doesn't call out WHY it does this, but
 * this bug is exactly why it has to). */
static void publish_state(const char *root, const char *repo_root, const char *target,
                           char methods[MAX_METHODS][64], int method_count,
                           const char *last_status) {
    char gui_state_path[MAX_PATH_LEN];
    char body_path[MAX_PATH_LEN];
    char tmp_path[MAX_PATH_LEN];
    char labels[MAX_METHODS][64];
    FILE *f;
    int i;

    if (target[0] && method_count > 0) {
        for (i = 0; i < method_count; i++) {
            resolve_display_label(repo_root, target, methods[i], labels[i], sizeof(labels[0]));
        }
    }

    path_join(gui_state_path, sizeof(gui_state_path), root, "manager/gui_state.txt");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", gui_state_path);
    f = fopen(tmp_path, "w");
    if (f) {
        fprintf(f, "cm_target=%s\n", target);
        fclose(f);
        rename(tmp_path, gui_state_path);
    }

    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", body_path);
    f = fopen(tmp_path, "w");
    if (f) {
        fprintf(f, "CONTEXT MENU\n");
        fprintf(f, "\n");
        if (target[0]) {
            fprintf(f, "Target: %s\n", target);
            fprintf(f, "\n");
        }
        if (!target[0]) {
            fprintf(f, "No target selected (picker screen not built yet -- open this via a\n");
            fprintf(f, "window's chrome icon or Ctrl+Q instead of the launcher list).\n");
        } else if (method_count == 0) {
            fprintf(f, "%s has no methods declared.\n", target);
        } else {
            for (i = 0; i < method_count; i++) {
                fprintf(f, "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>\n", labels[i], i + 1);
            }
        }
        if (last_status && last_status[0]) {
            fprintf(f, "\n%s\n", last_status);
        }
        fclose(f);
        rename(tmp_path, body_path);
    }
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[MAX_PATH_LEN];
    char target[256];
    char methods[MAX_METHODS][64];
    int method_count;
    int interact_mode;
    int key;
    char status[256] = "";

    if (argc < 2) return 2;
    root = argv[1];

    /* Same marker_tick guard as settings' own ops -- see that file's
       comment for the confirmed-live infinite-loop bug it prevents.
       Not currently exercised (this project has no polling watcher), kept
       for consistency and because removing a safety net "because it
       shouldn't matter here" is exactly the kind of unverified assumption
       flagged elsewhere in this doc's history. */
    if (argc > 2 && strcmp(argv[2], "marker_tick") == 0) {
        return 0;
    }

    derive_repo_root(root, repo_root, sizeof(repo_root));
    read_target(root, target, sizeof(target), &interact_mode);

    method_count = target[0] ? list_target_methods(repo_root, target, methods, interact_mode) : 0;

    key = read_last_key_pressed(root);
    if (target[0] && key >= 1 && key <= method_count) {
        char handler[512];
        get_target_method_handler(repo_root, target, methods[key - 1], handler, sizeof(handler));
        if (handler[0]) {
            dispatch_handler(repo_root, handler);
            snprintf(status, sizeof(status), "Ran: %s", methods[key - 1]);
        }
    }

    publish_state(root, repo_root, target, methods, method_count, status);

    /* No trigger_render() here -- same reasoning as settings' own ops:
       wraith-alpha_manager.c's route_command() already calls
       trigger_render() once, right after run_active_project_input_op(),
       for every dispatch path that reaches this op. */
    return 0;
}
