/* strategy_execute_a.c - Strategy A handler: pre-execute detected tool
 *
 * When gemma_strategy.c sets selected_strategy=A and detected_tool=X,
 * this op:
 * 1. Parses the user message to extract arguments for the tool
 * 2. Calls the appropriate tool op (list_dir, read_file, exec_cmd, etc.)
 * 3. Captures the output
 * 4. Appends result to context_log.txt as a "tool" result entry
 *
 * Usage: strategy_execute_a.+x (reads state.txt, gui_state.txt)
 * Side effects: appends to context_log.txt, modifies state.txt
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/wait.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_CMD 8192

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_gui_state(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/muchi-pal-agent/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void read_state_field(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *key, const char *value) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *pipe_escape(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (s[i] == '|') { *p++ = '\\'; *p++ = '|'; }
        else if (s[i] == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else *p++ = s[i];
    }
    *p = '\0';
    return out;
}

static void append_log_turn(const char *log_path, const char *role, const char *kind, const char *tool_name, const char *content) {
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    char *esc_content = pipe_escape(content);
    fprintf(f, "%s|%s|%s|%s\n", role, kind, tool_name ? tool_name : "", esc_content);
    free(esc_content);
    fclose(f);
}

static char *run_tool_exec(const char **argv) {
    pid_t pid = fork();
    if (pid == -1) return strdup("Error: fork failed");

    if (pid == 0) {
        execvp(argv[0], (char * const *)argv);
        perror("execvp");
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return strdup("Tool executed successfully");
    }
    return strdup("Tool execution failed");
}

static char *extract_argument_after(const char *msg, const char *keyword) {
    char *hit = strcasestr(msg, keyword);
    if (!hit) return strdup("");

    char *p = hit + strlen(keyword);
    while (*p && isspace(*p)) p++;

    if (!*p) return strdup("");

    size_t len = strlen(p);
    char *result = malloc(len + 1);
    strcpy(result, p);
    return result;
}

/* --- 5.tool-scaffold-gemma-agentic helpers (2026-07-31) ---
 * Deterministic natural-language → argv parsing for the agentic tools.
 * Rules from the design doc: filenames = first whitespace token after the
 * file-phrase; content = everything after the content-phrase; quotes
 * stripped when the whole argument is quoted. All paths are session-scoped
 * (the dispatcher chdir's to project_root before executing). */

static void trim_ws(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void strip_outer_quotes(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static int first_token(char **cursor, char *out, size_t out_sz) {
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    char *end = p;
    while (*end && !isspace((unsigned char)*end)) end++;
    size_t len = end - p;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    *cursor = end;
    return 1;
}

static char *run_capture(const char **argv) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return strdup("Error: pipe failed");
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]); close(pipefd[1]);
        return strdup("Error: fork failed");
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        execvp(argv[0], (char * const *)argv);
        perror("execvp");
        _exit(127);
    }
    close(pipefd[1]);
    char *buf = malloc(4096 + 1);
    size_t n = 0;
    while (n < 4096) {
        ssize_t r = read(pipefd[0], buf + n, 4096 - n);
        if (r <= 0) break;
        n += r;
    }
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    buf[n] = '\0';
    if (n >= 4096) {
        static const char note[] = " ...(output truncated at 4KB)";
        size_t nl = strlen(note);
        if (nl < n) {
            memcpy(buf + n - nl - 1, note, nl);
            buf[n - 1] = '\0';
        }
    }
    return buf;
}

/* write_file: "create file hello.py containing print('hello world')" */
static int parse_write_file(const char *msg, char *file, size_t fsz,
                            char *content, size_t csz) {
    const char *file_phrases[] = {"write file", "create file", "save file",
                                  "make a file", NULL};
    const char *bare_kws[] = {"write", "create", "save", NULL};
    const char *start = NULL;
    for (int i = 0; file_phrases[i]; i++) {
        char *h = strcasestr(msg, file_phrases[i]);
        if (h) { start = h + strlen(file_phrases[i]); break; }
    }
    if (!start) {
        for (int i = 0; bare_kws[i]; i++) {
            char *h = strcasestr(msg, bare_kws[i]);
            if (h) { start = h + strlen(bare_kws[i]); break; }
        }
    }
    if (!start) return 0;
    char *p = (char *)start;
    if (!first_token(&p, file, fsz)) return 0;

    const char *content_phrases[] = {"containing", "that says", " with ",
                                     " as ", " =", ":", NULL};
    const char *best = NULL;
    size_t best_len = 0;
    for (int i = 0; content_phrases[i]; i++) {
        char *h = strcasestr(p, content_phrases[i]);
        if (h && (!best || h < best)) {
            best = h;
            best_len = strlen(content_phrases[i]);
        }
    }
    if (best) {
        char *c = (char *)best + best_len;
        trim_ws(c);
        strip_outer_quotes(c);
        snprintf(content, csz, "%s", c);
    } else {
        content[0] = '\0';
    }
    return 1;
}

/* edit_file: "edit hello.py replace print('hi') with print('hello world')" */
static int parse_edit_file(const char *msg, char *file, size_t fsz,
                           char *search, size_t ssz, char *replace, size_t rsz) {
    char *p = NULL;
    {
        char *h = strcasestr(msg, "edit");
        if (h) p = h + 4;
    }
    if (!p) {
        char *h = strcasestr(msg, "modify");
        if (h) p = h + 6;
    }
    if (!p) return 0;
    if (!first_token(&p, file, fsz)) return 0;

    const char *ops[] = {"replace", "change", NULL};
    char *so = NULL;
    for (int i = 0; ops[i]; i++) {
        char *h = strcasestr(p, ops[i]);
        if (h && (!so || h < so)) so = h;
    }
    if (!so) return 0;
    size_t so_len = strlen(ops[0]);
    if (strncasecmp(so, "change", 6) == 0) so_len = 6;
    char *sc = so + so_len;
    trim_ws(sc);

    char *ws = strcasestr(sc, " with ");
    if (!ws) ws = strcasestr(sc, " to ");
    if (ws) {
        *ws = '\0';
        trim_ws(sc);
        strip_outer_quotes(sc);
        snprintf(search, ssz, "%s", sc);
        char *rp = ws + (strncasecmp(ws, " with ", 6) == 0 ? 6 : 3);
        trim_ws(rp);
        strip_outer_quotes(rp);
        snprintf(replace, rsz, "%s", rp);
    } else {
        trim_ws(sc);
        strip_outer_quotes(sc);
        snprintf(search, ssz, "%s", sc);
        replace[0] = '\0';
    }
    return 1;
}

/* append: "append to book.txt the line ..." or "append <content> to <file>" */
static int parse_append(const char *msg, char *file, size_t fsz,
                        char *content, size_t csz) {
    char *ap = strcasestr(msg, "append");
    if (!ap) return 0;
    char *p = ap + 6;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strncasecmp(p, "to ", 3) == 0) {
        char *fp = p + 3;
        if (!first_token(&fp, file, fsz)) return 0;
        char *line = strcasestr(fp, "the line");
        if (line) {
            char *c = line + 8;
            trim_ws(c);
            strip_outer_quotes(c);
            snprintf(content, csz, "%s", c);
        } else {
            char *text = strcasestr(fp, "the text");
            if (text) {
                char *c = text + 8;
                trim_ws(c);
                strip_outer_quotes(c);
                snprintf(content, csz, "%s", c);
            } else {
                trim_ws(fp);
                strip_outer_quotes(fp);
                snprintf(content, csz, "%s", fp);
            }
        }
    } else {
        char *to = strcasestr(p, " to ");
        if (!to) return 0;
        *to = '\0';
        trim_ws(p);
        strip_outer_quotes(p);
        snprintf(content, csz, "%s", p);
        char *fp = to + 4;
        if (!first_token(&fp, file, fsz)) return 0;
    }
    return 1;
}

/* read_file: path after "read file"/"open"/"cat"/"view" */
static int parse_read_file(const char *msg, char *file, size_t fsz) {
    const char *phrases[] = {"read file", "open file", "cat file", "view file",
                             "read", "open", "cat", "view", "display", NULL};
    const char *best = NULL;
    size_t best_len = 0;
    for (int i = 0; phrases[i]; i++) {
        char *h = strcasestr(msg, phrases[i]);
        if (h && (!best || h < best || (h == best && strlen(phrases[i]) > best_len))) {
            best = h;
            best_len = strlen(phrases[i]);
        }
    }
    if (!best) return 0;
    char *p = (char *)best + best_len;
    if (!first_token(&p, file, fsz)) return 0;
    strip_outer_quotes(file);
    return 1;
}

/* search: "search for <query> in <dir>" / "grep <query>" */
static void parse_search(const char *msg, char *query, size_t qsz,
                         char *target, size_t tsz) {
    const char *phrases[] = {"search for", "search in", "grep for", "grep",
                             "find", "search", NULL};
    const char *best = NULL;
    size_t best_len = 0;
    for (int i = 0; phrases[i]; i++) {
        char *h = strcasestr(msg, phrases[i]);
        if (h && (!best || h < best || (h == best && strlen(phrases[i]) > best_len))) {
            best = h;
            best_len = strlen(phrases[i]);
        }
    }
    if (!best) {
        query[0] = '\0';
        target[0] = '\0';
        return;
    }
    char *p = (char *)best + best_len;
    char *in = strcasestr(p, " in ");
    if (in) {
        *in = '\0';
        trim_ws(p);
        strip_outer_quotes(p);
        snprintf(query, qsz, "%s", p);
        char *tp = in + 4;
        if (!first_token(&tp, target, tsz)) target[0] = '\0';
        strip_outer_quotes(target);
    } else {
        trim_ws(p);
        strip_outer_quotes(p);
        snprintf(query, qsz, "%s", p);
        target[0] = '\0';
    }
}

/* --- W1 LT pipeline progress indicator (2026-08-03) --- */
static void progress_start(const char *tool, const char *detail) {
    char prog_path[PATH_BUF];
    snprintf(prog_path, sizeof(prog_path), "%s/pieces/world_01/session_01/chat/tool_progress.txt", project_root);
    FILE *f = fopen(prog_path, "w");
    if (f) {
        fprintf(f, "tool=%s\nstatus=running\ndetail=%s\nstarted=%ld\n",
                tool, detail ? detail : "", time(NULL));
        fclose(f);
    }
    write_state_field("tool_progress", tool);
}

static void progress_done(const char *tool, const char *result_summary) {
    char prog_path[PATH_BUF];
    snprintf(prog_path, sizeof(prog_path), "%s/pieces/world_01/session_01/chat/tool_progress.txt", project_root);
    FILE *f = fopen(prog_path, "w");
    if (f) {
        fprintf(f, "tool=%s\nstatus=done\nresult=%s\nfinished=%ld\n",
                tool, result_summary ? result_summary : "", time(NULL));
        fclose(f);
    }
    write_state_field("tool_progress", "done");
}

/* Parse "book chapter [cell_id]" from user message for W1 ops.
 * Accepts: "solpen ch01", "solpen ch01 cell_01", "fill cell 01 for solpen ch01",
 *          "fill cell_01 for solpen ch01", "for solpen ch01 cell 01" */
static int parse_w1_args(const char *msg, char *book, size_t bsz,
                         char *chapter, size_t csz,
                         char *cell_id, size_t cell_sz) {
    book[0] = '\0'; chapter[0] = '\0'; cell_id[0] = '\0';

    /* Step 1: Find chapter token (ch + digits) */
    const char *ch_ptr = NULL;
    const char *p = msg;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *word_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t wlen = p - word_start;
        if (wlen >= 3 && strncasecmp(word_start, "ch", 2) == 0 &&
            word_start[2] >= '0' && word_start[2] <= '9') {
            ch_ptr = word_start;
            break;
        }
    }
    if (!ch_ptr) return 0;

    /* Extract chapter — word boundary only (ch_ptr extends into the rest
     * of the message, so measure to the first space, not strlen). */
    const char *w_end = ch_ptr;
    while (*w_end && !isspace((unsigned char)*w_end)) w_end++;
    size_t clen = w_end - ch_ptr;
    if (clen >= csz) clen = csz - 1;
    memcpy(chapter, ch_ptr, clen);
    chapter[clen] = '\0';

    /* Step 2: Find book (word immediately before chapter) */
    const char *bk_end = ch_ptr;
    while (bk_end > msg && isspace((unsigned char)*(bk_end - 1))) bk_end--;
    const char *bk_start = bk_end;
    while (bk_start > msg && !isspace((unsigned char)*(bk_start - 1))) bk_start--;
    size_t bklen = bk_end - bk_start;
    if (bklen > 0 && bklen < bsz) {
        memcpy(book, bk_start, bklen);
        book[bklen] = '\0';
    }

    /* Step 3: Find cell_id — scan all "cell" occurrences for one followed
     * by (optional "_") + digits: "cell 01", "cell_01", "cell01". */
    const char *scan = msg;
    while ((scan = strcasestr(scan, "cell")) != NULL) {
        const char *cp = scan + 4;
        while (*cp && isspace((unsigned char)*cp)) cp++;
        if (*cp == '_') cp++;
        if (*cp >= '0' && *cp <= '9') {
            char cid[64];
            int ci = 0;
            while (*cp && !isspace((unsigned char)*cp) && ci < 63) {
                cid[ci++] = *cp++;
            }
            cid[ci] = '\0';
            if (cid[0] >= '0' && cid[0] <= '9' && strstr(cid, "_") == NULL) {
                snprintf(cell_id, cell_sz, "cell_%s", cid);
            } else {
                snprintf(cell_id, cell_sz, "%s", cid);
            }
            break;
        }
        scan += 4;
    }

    return (chapter[0] && book[0]) ? 1 : 0;
}

int main(void) {
    resolve_root();

    char detected_tool[256] = "";
    char strategy[32] = "";
    char user_message[MAX_LINE] = "";

    read_state_field("detected_tool", detected_tool, sizeof(detected_tool));
    read_state_field("selected_strategy", strategy, sizeof(strategy));
    read_gui_state("message_input", user_message, sizeof(user_message));

    if (strcmp(strategy, "A") != 0 || strcmp(detected_tool, "none") == 0 || !user_message[0]) {
        return 0;
    }

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);

    char strategy_msg[256];
    snprintf(strategy_msg, sizeof(strategy_msg), "[Strategy A] Detected tool: %s", detected_tool);

    /* OUTPUT PATH 1: Append to context_log (appears in chat history) */
    append_log_turn(log_path, "system", "strategy", detected_tool, strategy_msg);

    /* OUTPUT PATH 2: Append to strategy_messages.txt */
    char strat_path[PATH_BUF];
    snprintf(strat_path, sizeof(strat_path), "%s/pieces/world_01/session_01/chat/strategy_messages.txt", project_root);
    FILE *sf = fopen(strat_path, "a");
    if (sf) {
        fprintf(sf, "%s\n", strategy_msg);
        fclose(sf);
    }

    /* OUTPUT PATH 3: Write to debug file */
    char debug_path[PATH_BUF];
    snprintf(debug_path, sizeof(debug_path), "%s/pieces/world_01/session_01/chat/strategy_debug.txt", project_root);
    FILE *dbg = fopen(debug_path, "a");
    if (dbg) {
        fprintf(dbg, "[strategy_execute_a] %s at %ld\n", strategy_msg, time(NULL));
        fclose(dbg);
    }

    /* OUTPUT PATH 4: Write to all_debug.txt (catch-all) */
    char all_debug[PATH_BUF];
    snprintf(all_debug, sizeof(all_debug), "%s/pieces/world_01/session_01/chat/all_debug.txt", project_root);
    FILE *adb = fopen(all_debug, "a");
    if (adb) {
        fprintf(adb, "[strategy_execute_a] %s\n", strategy_msg);
        fclose(adb);
    }

    /* OUTPUT PATH 5: Update state.txt sys_msg (shown in status line) */
    write_state_field("sys_msg", strategy_msg);

    /* OUTPUT PATH 6: Also write to detected_tool field so it's visible in state */
    write_state_field("detected_tool_strategy", strategy_msg);

    char *result = NULL;

    /* Session-scoped file work: ops taking relative paths (file_ops,
     * edit_file, search_in_files) plus popen'd shell commands resolve
     * against the session dir (PRISC_PROJECT_ROOT), not the launcher's
     * CWD - so pin CWD here so "create file hello.py" lands in the
     * session. All state/log paths above/below are absolute (project_root)
     * and unaffected by this. */
    chdir(project_root);

    if (strcmp(detected_tool, "list_dir") == 0) {
        char cmd[MAX_CMD];
        snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/list_dir.+x'", project_root, project_root);
        FILE *pipe = popen(cmd, "r");
        if (pipe) {
            char buf[4096] = "";
            size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
            buf[n] = '\0';
            pclose(pipe);
            result = strdup(buf);
        } else {
            result = strdup("Error listing files");
        }
    } else if (strcmp(detected_tool, "read_file") == 0) {
        char file[512];
        if (parse_read_file(user_message, file, sizeof(file))) {
            char tool_path[PATH_BUF];
            snprintf(tool_path, sizeof(tool_path), "%s/ops/+x/file_ops.+x", project_root);
            const char *argv[] = { tool_path, "read", file, NULL };
            result = run_capture(argv);
        } else {
            result = strdup("Missing filename");
        }
    } else if (strcmp(detected_tool, "write_file") == 0) {
        char file[512], content[2048];
        if (parse_write_file(user_message, file, sizeof(file), content, sizeof(content))) {
            char tool_path[PATH_BUF];
            snprintf(tool_path, sizeof(tool_path), "%s/ops/+x/file_ops.+x", project_root);
            const char *argv[] = { tool_path, "write", file, content, NULL };
            result = run_capture(argv);
        } else {
            result = strdup("Missing filename");
        }
    } else if (strcmp(detected_tool, "edit_file") == 0) {
        if (strcasestr(user_message, "append")) {
            char file[512], content[2048];
            if (parse_append(user_message, file, sizeof(file), content, sizeof(content))) {
                FILE *af = fopen(file, "a");
                if (af) {
                    fprintf(af, "\n%s", content);
                    fclose(af);
                    char out[512];
                    snprintf(out, sizeof(out), "Appended %zu bytes to %s", strlen(content), file);
                    result = strdup(out);
                } else {
                    result = strdup("Error: cannot append to file");
                }
            } else {
                result = strdup("Missing filename for append");
            }
        } else {
            char file[512], search[1024], replace[2048];
            if (parse_edit_file(user_message, file, sizeof(file), search, sizeof(search), replace, sizeof(replace))) {
                char tool_path[PATH_BUF];
                snprintf(tool_path, sizeof(tool_path), "%s/ops/+x/edit_file.+x", project_root);
                const char *argv[] = { tool_path, file, search, replace, NULL };
                result = run_capture(argv);
            } else {
                result = strdup("Missing edit arguments");
            }
        }
    } else if (strcmp(detected_tool, "exec_cmd") == 0) {
        char *cmd_str = extract_argument_after(user_message, "run");
        if (!cmd_str[0]) {
            free(cmd_str);
            cmd_str = extract_argument_after(user_message, "execute");
        }
        if (cmd_str && strlen(cmd_str) > 0) {
            FILE *pipe = popen(cmd_str, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error executing command");
            }
        }
        free(cmd_str);
    } else if (strcmp(detected_tool, "speak") == 0) {
        char *text = extract_argument_after(user_message, "speak");
        if (text && strlen(text) > 0) {
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/tts_speak.+x' '%s' 2>&1", project_root, project_root, text);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[1024] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: TTS failed");
            }
        }
        free(text);
    } else if (strcmp(detected_tool, "search_in_files") == 0) {
        char query[1024] = "", target[512] = "";
        parse_search(user_message, query, sizeof(query), target, sizeof(target));
        if (!query[0]) {
            result = strdup("Missing search query");
        } else {
            char cmd[MAX_CMD];
            if (target[0]) {
                snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/search_in_files.+x' '%s' '%s' 2>&1 | head -20", project_root, project_root, query, target);
            } else {
                snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/search_in_files.+x' '%s' . 2>&1 | head -20", project_root, project_root, query);
            }
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: search failed");
            }
        }
    } else if (strcmp(detected_tool, "plan_cells") == 0) {
        char book[256], chapter[256], cell_id[256];
        if (parse_w1_args(user_message, book, sizeof(book), chapter, sizeof(chapter), cell_id, sizeof(cell_id))) {
            progress_start("plan_cells", chapter);
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/plan_cells.+x' '%s' '%s' 2>&1",
                     project_root, project_root, book, chapter);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: plan_cells failed");
            }
            progress_done("plan_cells", result);
        } else {
            result = strdup("Usage: plan cells for <book> <chapter>");
        }
    } else if (strcmp(detected_tool, "fill_cell") == 0) {
        char book[256], chapter[256], cell_id[256];
        if (parse_w1_args(user_message, book, sizeof(book), chapter, sizeof(chapter), cell_id, sizeof(cell_id))) {
            char detail[512];
            snprintf(detail, sizeof(detail), "%s/%s %s", book, chapter, cell_id);
            progress_start("fill_cell", detail);
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/fill_cell.+x' '%s' '%s' '%s' 2>&1",
                     project_root, project_root, book, chapter, cell_id);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: fill_cell failed");
            }
            progress_done("fill_cell", result);
        } else {
            result = strdup("Usage: fill cell <id> for <book> <chapter>");
        }
    } else if (strcmp(detected_tool, "verify_cell") == 0) {
        char book[256], chapter[256], cell_id[256];
        if (parse_w1_args(user_message, book, sizeof(book), chapter, sizeof(chapter), cell_id, sizeof(cell_id))) {
            char detail[512];
            snprintf(detail, sizeof(detail), "%s/%s %s", book, chapter, cell_id);
            progress_start("verify_cell", detail);
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/verify_cell.+x' '%s' '%s' '%s' 2>&1",
                     project_root, project_root, book, chapter, cell_id);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: verify_cell failed");
            }
            progress_done("verify_cell", result);
        } else {
            result = strdup("Usage: verify cell <id> for <book> <chapter>");
        }
    } else if (strcmp(detected_tool, "apply_cell") == 0) {
        char book[256], chapter[256], cell_id[256];
        if (parse_w1_args(user_message, book, sizeof(book), chapter, sizeof(chapter), cell_id, sizeof(cell_id))) {
            progress_start("apply_cell", chapter);
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/apply_cell.+x' '%s' '%s' 2>&1",
                     project_root, project_root, book, chapter);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: apply_cell failed");
            }
            progress_done("apply_cell", result);
        } else {
            result = strdup("Usage: apply cells for <book> <chapter>");
        }
    } else if (strcmp(detected_tool, "grade_chapter") == 0) {
        char book[256], chapter[256], cell_id[256];
        if (parse_w1_args(user_message, book, sizeof(book), chapter, sizeof(chapter), cell_id, sizeof(cell_id))) {
            progress_start("grade_chapter", chapter);
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/grade_chapter.+x' '%s' '%s' 2>&1",
                     project_root, project_root, book, chapter);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: grade_chapter failed");
            }
            progress_done("grade_chapter", result);
        } else {
            result = strdup("Usage: grade chapter <book> <chapter>");
        }
    }

    if (result && strlen(result) > 0) {
        /* 2&3-jul31-sprint (jul-31 fix): do NOT append tool|result to
         * context_log here - this op runs BEFORE send_message appends
         * the user's own message, so doing so rendered the listing ABOVE
         * the "You: ..." line that triggered it (read as "the tool
         * didn't answer"). Stash the raw result in a pending file
         * instead; send_message flushes it into context_log right AFTER
         * the user turn, then skips the LLM call entirely unless
         * model_after_tool=yes (default no). Pending file layout:
         * line 1 = tool name, remainder = raw result (un-escaped;
         * send_message's append_log_turn pipe-escapes on flush). */
        char pending_path[PATH_BUF];
        snprintf(pending_path, sizeof(pending_path), "%s/pieces/world_01/session_01/chat/tool_result.pending", project_root);
        FILE *pf = fopen(pending_path, "w");
        if (pf) {
            fprintf(pf, "%s\n", detected_tool);
            fwrite(result, 1, strlen(result), pf);
            fclose(pf);
            write_state_field("tool_result_pending", "1");
        }
        char msg[256];
        snprintf(msg, sizeof(msg), "[Tool: %s] %zu bytes", detected_tool, strlen(result));
        write_state_field("sys_msg", msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "[Tool: %s] No output or error", detected_tool);
        write_state_field("sys_msg", msg);
    }

    write_state_field("detected_tool", "none");
    write_state_field("selected_strategy", "");
    free(result);
    return 0;
}
