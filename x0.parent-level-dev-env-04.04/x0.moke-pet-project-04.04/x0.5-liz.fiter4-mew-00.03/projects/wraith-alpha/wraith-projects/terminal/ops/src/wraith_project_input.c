#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 1024
#define MAX_INPUT_LEN 512
#define MAX_SCROLLBACK_LINES 400
#define SCROLLBACK_TRIM_KEEP 250
#define BODY_MAX_LINES 15
#define CMD_TIMEOUT_SECONDS 8

/*
 * Real hot-path logic for the "terminal" Wraith project. Invoked fresh on
 * every keystroke by wraith-alpha_manager.c's run_active_project_input_op()
 * while this project's is_map_control=1 (see route_input() in the
 * top-level manager: every raw key gets forwarded here as a KEY_PRESSED
 * history line instead of being consumed as desktop digit-nav, exactly
 * the same mechanism piececraft-wraith/wraith-ed use for WASD movement --
 * this project just types text instead of moving a character). Builds up
 * a command line character by character, and on Enter runs it through a
 * real shell (fork/exec /bin/sh -c, not system() -- matches this repo's
 * standing fork/exec/waitpid convention), capturing combined
 * stdout+stderr into the scrollback transcript displayed via
 * session/wraith_body.txt (the generic per-project body-render fallback
 * every hosted project gets for free -- see
 * append_project_probe_body()/project_probe_body_lines() in
 * wraith-alpha_manager.c; no ${...} chtpm template wiring needed, unlike
 * piececraft-wraith's game_map= special case).
 *
 * Known v1 scope limits (documented, not oversights):
 *  - Commands run synchronously with an 8s timeout (CMD_TIMEOUT_SECONDS)
 *    and are then SIGKILLed -- run_active_project_input_op() blocks the
 *    whole desktop input loop until this process exits, so a genuinely
 *    long-running or interactive program (vim, top, less, a dev server)
 *    would otherwise freeze the entire Wraith desktop. This is a real
 *    shell for quick commands, not a full async job runner or a pty.
 *  - No command history recall (up/down arrows) and no tab completion.
 *  - `cd` is a builtin (a subshell's chdir can't persist across the
 *    fork/exec-per-command model here); everything else is delegated to
 *    /bin/sh -c verbatim.
 */

typedef struct {
    int is_map_control;
    char project_id[128];
    char title[64];
    char mode[64];
    char status[128];
    char cwd[PATH_MAX];
    char input_line[MAX_INPUT_LEN];
    char last_action[256];
} TermState;

static void trim_newline(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = '\0';
}

static void copy_value(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
}

static void trim_both_ends(char *s) {
    char *start = s;
    size_t len;
    if (!s) return;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void path_join(char *out, size_t out_sz, const char *a, const char *b) {
    if (!a || !a[0]) {
        snprintf(out, out_sz, "%s", b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        snprintf(out, out_sz, "%s", a);
        return;
    }
    if (a[strlen(a) - 1] == '/') snprintf(out, out_sz, "%s%s", a, b);
    else snprintf(out, out_sz, "%s/%s", a, b);
}

static void derive_repo_root(const char *project_root, char *repo_root, size_t repo_root_sz) {
    const char *needle = "/projects/wraith-alpha/wraith-projects/";
    const char *p = strstr(project_root, needle);
    if (p) {
        size_t len = (size_t)(p - project_root);
        if (len >= repo_root_sz) len = repo_root_sz - 1;
        memcpy(repo_root, project_root, len);
        repo_root[len] = '\0';
        return;
    }
    copy_value(repo_root, repo_root_sz, project_root);
}

static long read_cursor(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;
    long cursor = 0;

    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fscanf(f, "%ld", &cursor) != 1) cursor = 0;
    fclose(f);
    return cursor;
}

static void write_cursor(const char *root, long cursor) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/history.cursor");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%ld\n", cursor);
    fclose(f);
}

static int key_from_history_line(const char *line) {
    const char *p = strstr(line, "KEY_PRESSED:");
    if (!p) return -1;
    p += strlen("KEY_PRESSED:");
    while (*p && isspace((unsigned char)*p)) p++;
    return atoi(p);
}

static const char *command_from_history_line(const char *line) {
    const char *p = strstr(line, "COMMAND:");
    if (!p) return NULL;
    p += strlen("COMMAND:");
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static void set_defaults(TermState *st, const char *repo_root) {
    memset(st, 0, sizeof(*st));
    st->is_map_control = 0;
    snprintf(st->project_id, sizeof(st->project_id), "wraith-alpha/wraith-projects/terminal");
    snprintf(st->title, sizeof(st->title), "WRAITH TERMINAL");
    snprintf(st->mode, sizeof(st->mode), "shell");
    snprintf(st->status, sizeof(st->status), "ready");
    copy_value(st->cwd, sizeof(st->cwd), repo_root);
    st->input_line[0] = '\0';
    snprintf(st->last_action, sizeof(st->last_action), "Initialized");
}

static void load_state(const char *root, TermState *st, const char *repo_root) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;

    set_defaults(st, repo_root);
    path_join(path, sizeof(path), root, "session/state.txt");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strncmp(line, "project_id=", 11) == 0) copy_value(st->project_id, sizeof(st->project_id), line + 11);
        else if (strncmp(line, "title=", 6) == 0) copy_value(st->title, sizeof(st->title), line + 6);
        else if (strncmp(line, "mode=", 5) == 0) copy_value(st->mode, sizeof(st->mode), line + 5);
        else if (strncmp(line, "status=", 7) == 0) copy_value(st->status, sizeof(st->status), line + 7);
        else if (strncmp(line, "is_map_control=", 15) == 0) st->is_map_control = atoi(line + 15);
        else if (strncmp(line, "cwd=", 4) == 0) copy_value(st->cwd, sizeof(st->cwd), line + 4);
        else if (strncmp(line, "input_line=", 11) == 0) copy_value(st->input_line, sizeof(st->input_line), line + 11);
        else if (strncmp(line, "last_action=", 12) == 0) copy_value(st->last_action, sizeof(st->last_action), line + 12);
    }
    fclose(f);
    if (st->cwd[0] == '\0') copy_value(st->cwd, sizeof(st->cwd), repo_root);
}

/* Atomic write (temp+rename) -- matches wraith-alpha_manager.c's own
   convention for this exact file. piececraft-wraith_manager.c's
   save_state_txt() documents WHY: a non-atomic write to state.txt was
   the root cause of is_map_control intermittently reading as false. This
   project toggles is_map_control the same way, so it gets the same
   atomic write, not the plain fopen("w") fs_manager.c's sibling ops file
   uses (that project never dynamically flips is_map_control, so it never
   hit the bug -- but there's no reason to risk it here). */
static void save_state(const char *root, const TermState *st) {
    char path[MAX_PATH_LEN], tmp_path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/state.txt");
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    f = fopen(tmp_path, "w");
    if (!f) return;
    fprintf(f, "project_id=%s\n", st->project_id);
    fprintf(f, "title=%s\n", st->title);
    fprintf(f, "mode=%s\n", st->mode);
    fprintf(f, "status=%s\n", st->status);
    fprintf(f, "is_map_control=%d\n", st->is_map_control);
    fprintf(f, "cwd=%s\n", st->cwd);
    fprintf(f, "input_line=%s\n", st->input_line);
    fprintf(f, "last_action=%s\n", st->last_action);
    fclose(f);
    rename(tmp_path, path);
}

static int count_lines_in_file(const char *path) {
    FILE *f = fopen(path, "r");
    char line[600];
    int n = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) n++;
    fclose(f);
    return n;
}

/* Caps total scrollback at MAX_SCROLLBACK_LINES by rewriting down to the
   last SCROLLBACK_TRIM_KEEP lines -- same "don't let a session file grow
   unbounded forever" discipline already applied elsewhere in this repo
   (session/history.txt caps, debug.txt caps). Two-pass (count, then
   copy-past-skip) rather than buffering the whole file, so this stays
   cheap regardless of how chatty a session gets. */
static void trim_scrollback_if_needed(const char *root) {
    char path[MAX_PATH_LEN], tmp_path[MAX_PATH_LEN];
    FILE *f, *out;
    char line[600];
    long total, skip, idx;

    path_join(path, sizeof(path), root, "session/scrollback.txt");
    total = count_lines_in_file(path);
    if (total <= MAX_SCROLLBACK_LINES) return;

    f = fopen(path, "r");
    if (!f) return;
    skip = total - SCROLLBACK_TRIM_KEEP;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    out = fopen(tmp_path, "w");
    if (!out) {
        fclose(f);
        return;
    }
    idx = 0;
    while (fgets(line, sizeof(line), f)) {
        idx++;
        if (idx > skip) fputs(line, out);
    }
    fclose(f);
    fclose(out);
    rename(tmp_path, path);
}

static void append_scrollback(const char *root, const char *text) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/scrollback.txt");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%.550s\n", text);
    fclose(f);
    trim_scrollback_if_needed(root);
}

static void clear_scrollback(const char *root) {
    char path[MAX_PATH_LEN];
    FILE *f;

    path_join(path, sizeof(path), root, "session/scrollback.txt");
    f = fopen(path, "w");
    if (f) fclose(f);
}

static void handle_cd(const char *root, const char *repo_root, TermState *st, const char *command) {
    const char *target = command + 2;
    char resolved[PATH_MAX];
    char real[PATH_MAX];
    struct stat st_buf;

    while (*target == ' ') target++;

    if (!target[0]) {
        copy_value(resolved, sizeof(resolved), repo_root);
    } else if (target[0] == '/') {
        copy_value(resolved, sizeof(resolved), target);
    } else {
        snprintf(resolved, sizeof(resolved), "%s/%s", st->cwd, target);
    }

    if (realpath(resolved, real) && stat(real, &st_buf) == 0 && S_ISDIR(st_buf.st_mode)) {
        copy_value(st->cwd, sizeof(st->cwd), real);
        snprintf(st->last_action, sizeof(st->last_action), "Changed directory to %.200s", real);
    } else {
        char msg[PATH_MAX + 32];
        snprintf(msg, sizeof(msg), "cd: no such directory: %.200s", target);
        append_scrollback(root, msg);
        snprintf(st->last_action, sizeof(st->last_action), "cd: no such directory");
    }
}

/* Runs `command` via a real shell (fork + execl("/bin/sh","-c",...), not
   system() -- this repo's standing convention), cwd'd into st->cwd,
   combined stdout+stderr captured to a scratch file and folded into the
   scrollback line by line. Polls waitpid with WNOHANG rather than
   blocking outright so a runaway command can be killed after
   CMD_TIMEOUT_SECONDS instead of freezing the desktop input loop
   forever (run_active_project_input_op() in the top-level manager is
   synchronous -- see this file's top-of-file note). */
static void exec_and_capture(const char *root, TermState *st, const char *command) {
    char out_path[MAX_PATH_LEN];
    pid_t pid;
    int status = 0;
    int elapsed_ms = 0;
    int killed = 0;
    FILE *f;
    char line[1024];
    int line_count = 0;

    path_join(out_path, sizeof(out_path), root, "session/cmd_output.tmp");

    pid = fork();
    if (pid == 0) {
        int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        if (chdir(st->cwd) != 0) {
            _exit(127);
        }
        execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) {
        append_scrollback(root, "[failed to start shell]");
        return;
    }

    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (elapsed_ms >= CMD_TIMEOUT_SECONDS * 1000) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            killed = 1;
            break;
        }
        usleep(50000);
        elapsed_ms += 50;
    }

    f = fopen(out_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) && line_count < 200) {
            trim_newline(line);
            append_scrollback(root, line);
            line_count++;
        }
        fclose(f);
    }
    remove(out_path);

    if (killed) {
        char msg[128];
        snprintf(msg, sizeof(msg), "[command timed out after %ds and was killed]", CMD_TIMEOUT_SECONDS);
        append_scrollback(root, msg);
        snprintf(st->last_action, sizeof(st->last_action), "Command timed out");
    } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "[exit %d]", WEXITSTATUS(status));
        append_scrollback(root, msg);
        snprintf(st->last_action, sizeof(st->last_action), "Command exited %d", WEXITSTATUS(status));
    } else {
        snprintf(st->last_action, sizeof(st->last_action), "Ran command");
    }
}

static void execute_command(const char *root, const char *repo_root, TermState *st, const char *raw_command) {
    char command[MAX_INPUT_LEN];
    char prompt_line[MAX_INPUT_LEN + PATH_MAX + 8];

    copy_value(command, sizeof(command), raw_command);
    trim_both_ends(command);

    snprintf(prompt_line, sizeof(prompt_line), "%s $ %s", st->cwd, command);
    append_scrollback(root, prompt_line);

    if (command[0] == '\0') {
        return;
    }
    if (strcmp(command, "clear") == 0) {
        clear_scrollback(root);
        snprintf(st->last_action, sizeof(st->last_action), "Cleared screen");
        return;
    }
    if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        st->is_map_control = 0;
        snprintf(st->status, sizeof(st->status), "ready");
        snprintf(st->last_action, sizeof(st->last_action), "Exited terminal input mode");
        append_scrollback(root, "[terminal input mode closed -- select the interact control to resume]");
        return;
    }
    if (strcmp(command, "cd") == 0 || strncmp(command, "cd ", 3) == 0) {
        handle_cd(root, repo_root, st, command);
        return;
    }
    exec_and_capture(root, st, command);
}

static void process_key(const char *root, const char *repo_root, TermState *st, int key) {
    if (key <= 0) return;

    if (key == 10 || key == 13) {
        execute_command(root, repo_root, st, st->input_line);
        st->input_line[0] = '\0';
        return;
    }
    if (key == 8 || key == 127) {
        size_t len = strlen(st->input_line);
        if (len > 0) st->input_line[len - 1] = '\0';
        return;
    }
    if (key >= 32 && key < 127) {
        size_t len = strlen(st->input_line);
        if (len + 1 < sizeof(st->input_line)) {
            st->input_line[len] = (char)key;
            st->input_line[len + 1] = '\0';
        }
        return;
    }
    /* Arrow keys / function keys / other synthetic codes (>=1000, per the
       desktop's own key-translation convention -- see fs's sibling ops
       file) are silently ignored: no command-history recall yet, a
       documented v1 scope limit, not an oversight. */
}

static void write_body(const char *root, const TermState *st) {
    char scrollback_path[MAX_PATH_LEN], body_path[MAX_PATH_LEN];
    FILE *in, *out;
    char line[600];
    long total, skip, idx;
    long shown = BODY_MAX_LINES - 2; /* header line + prompt line */

    path_join(scrollback_path, sizeof(scrollback_path), root, "session/scrollback.txt");
    path_join(body_path, sizeof(body_path), root, "session/wraith_body.txt");

    total = count_lines_in_file(scrollback_path);
    skip = total - shown;
    if (skip < 0) skip = 0;

    out = fopen(body_path, "w");
    if (!out) return;
    fprintf(out, "WRAITH TERMINAL -- %s\n", st->status);

    in = fopen(scrollback_path, "r");
    if (in) {
        idx = 0;
        while (fgets(line, sizeof(line), in)) {
            idx++;
            if (idx > skip) {
                trim_newline(line);
                fprintf(out, "%.200s\n", line);
            }
        }
        fclose(in);
    }

    fprintf(out, "%.150s $ %.200s_\n", st->cwd, st->input_line);
    fclose(out);
}

int main(int argc, char **argv) {
    const char *root;
    char repo_root[PATH_MAX];
    char history_path[MAX_PATH_LEN];
    FILE *history;
    long cursor, end_pos;
    char line[MAX_LINE_LEN];
    TermState st;

    if (argc < 2) return 2;
    root = argv[1];

    derive_repo_root(root, repo_root, sizeof(repo_root));
    load_state(root, &st, repo_root);

    cursor = read_cursor(root);
    path_join(history_path, sizeof(history_path), root, "session/history.txt");
    history = fopen(history_path, "r");
    if (history) {
        fseek(history, 0, SEEK_END);
        end_pos = ftell(history);
        if (cursor < 0 || cursor > end_pos) cursor = 0;
        fseek(history, cursor, SEEK_SET);
        while (fgets(line, sizeof(line), history)) {
            const char *command = command_from_history_line(line);
            int key;

            if (command) {
                if (strncmp(command, "CLEAR", 5) == 0) {
                    clear_scrollback(root);
                }
                continue;
            }

            key = key_from_history_line(line);
            if (key > 0) {
                process_key(root, repo_root, &st, key);
            }
        }
        cursor = ftell(history);
        fclose(history);
        write_cursor(root, cursor);
    }

    save_state(root, &st);
    write_body(root, &st);
    return 0;
}
