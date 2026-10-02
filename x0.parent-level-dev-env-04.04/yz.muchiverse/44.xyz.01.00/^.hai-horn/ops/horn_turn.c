/* horn_turn - HORN_CHAT's one-turn orchestrator.
 *
 * Called by pal/horn_main_loop.pal every time the player submits a line
 * (cli_io Enter -> chtpm saves input_text to gui_state.txt and injects
 * raw key 13 into the interact relay -> the pal loop dispatches here).
 *
 * Reads what the player typed out of gui_state.txt, clears it, appends
 * the user turn to the persistent history, calls the OpenRouter
 * transport, appends the model's turn, then hands off to horn_publish to
 * project the result into the chtpm vars.
 *
 * Chat history is persistent across sessions (chats/HORN_SESSIONS), but
 * deliberately NOT fed back as model context: each request carries only
 * the current turn. Persistent context changes model behaviour turn over
 * turn and makes the transcript in the UI disagree with what the model
 * actually saw, which would poison the side-by-side HORN-vs-HALO
 * comparison the IRL harness is built to make. Revisit when IRL grading
 * has a real signal to attach it to.
 *
 * Usage: horn_turn.+x (no args - reads gui_state.txt)
 * Self-contained: own root resolution, no shared headers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MSG_CAP  65536

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (getcwd(project_root, sizeof(project_root)) == NULL)
        snprintf(project_root, sizeof(project_root), ".");
}

static void ensure_dir(const char *path) {
    if (access(path, F_OK) == 0) return;
    char cmd[PATH_BUF + 32];
    /* Single-quoted: this house's tree has literal '&.widgits' and
     * '^.hai-horn' segments that /bin/sh would mangle unquoted. */
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    int rc = system(cmd);
    (void)rc;
}

/* gui_state.txt path. For a pal-native project PRISC_PROJECT_ID is set,
 * so chtpm's is_modern_layout() is true and its cli_io Enter handler
 * writes to exactly this path (see chtpm_parser_pal.c's
 * save_to_gui_state_impl() modern-layout branch). Keeping the two in
 * step is the whole contract between this op and the layout. */
static void gui_state_path(char *out, size_t sz) {
    snprintf(out, sz, "%s/pieces/apps/player_app/manager/gui_state.txt", project_root);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void trim_in_place(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = '\0';
}

/* Pull one key out of a key=value state file, copying into out. */
static int read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char line[MSG_CAP];
    size_t klen = strlen(key);
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            snprintf(out, out_sz, "%s", line + klen + 1);
            trim_in_place(out);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

/* Write key=value into a state file, preserving every other key already
 * there. Used to clear input_text after consuming it: chtpm's own
 * sync_cli_input_from_gui_state() re-reads this file every render, so
 * leaving a stale value would resurrect the previous turn's text into
 * the input box on the next frame. */
static void set_kv(const char *path, const char *key, const char *value) {
    /* asprintf, not a fixed buffer: `path` is a caller-supplied pointer
     * whose length gcc cannot bound, so a char[n] here is either a
     * truncation risk or a -Wformat-truncation warning. */
    char *tmp_path = NULL;
    if (asprintf(&tmp_path, "%s.tmp", path) < 0 || !tmp_path) return;

    FILE *in = fopen(path, "rb");
    FILE *out = fopen(tmp_path, "wb");
    if (!out) { free(tmp_path); if (in) fclose(in); return; }

    size_t klen = strlen(key);
    int replaced = 0;
    if (in) {
        char line[MSG_CAP];
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                if (!replaced) { fprintf(out, "%s=%s\n", key, value); replaced = 1; }
                continue;
            }
            fputs(line, out);
        }
        fclose(in);
    }
    if (!replaced) fprintf(out, "%s=%s\n", key, value);
    fclose(out);

    rename(tmp_path, path);
    free(tmp_path);
}

/* Session directory, heap-allocated. Returns NULL on allocation failure.
 *
 * Heap rather than a caller buffer because every consumer here composes
 * a further suffix onto this path ("/chat_history.txt"), and project_root
 * is already MAX_PATH wide - a MAX_PATH buffer plus any suffix is a
 * truncation gcc is right to warn about, and quietly truncating a
 * transcript path would write the user's chat history somewhere nobody
 * looks. */
static char *sessions_dir(void) {
    const char *env = getenv("HORN_SESSIONS");
    if (env && env[0]) return strdup(env);
    char *out = NULL;
    if (asprintf(&out, "%s/chats/HORN_SESSIONS", project_root) < 0) return NULL;
    return out;
}

static char *history_path(void) {
    char *dir = sessions_dir();
    if (!dir) return NULL;
    char *out = NULL;
    if (asprintf(&out, "%s/chat_history.txt", dir) < 0) out = NULL;
    free(dir);
    return out;
}

static char *transcript_path(void) {
    char *dir = sessions_dir();
    if (!dir) return NULL;
    char *out = NULL;
    if (asprintf(&out, "%s/transcript.txt", dir) < 0) out = NULL;
    free(dir);
    return out;
}

/* Flatten embedded newlines so one turn is exactly one line. A model
 * reply is routinely multi-paragraph, and an unflattened record would
 * make chat_history.txt unparseable for the IRL harness that is going to
 * read this file turn by turn. */
static char *flatten_newlines(const char *text) {
    size_t n = strlen(text);
    char *out = malloc(n * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (text[i] == '\n')      { out[o++] = ' '; out[o++] = '/'; out[o++] = ' '; }
        else if (text[i] == '\r') { /* dropped: CR+LF pair, the \n covers it */ }
        else if (text[i] == '\t') { out[o++] = ' '; }
        else out[o++] = text[i];
    }
    out[o] = '\0';
    return out;
}

/* Create the sessions dir if missing. Called by every append path: on a
 * first-ever run the dir does not exist yet, and an fopen("ab") that
 * fails on a missing directory is a SILENT no-op - the turn appears to
 * succeed and the reply is simply never recorded anywhere. */
static void ensure_sessions_dir(void) {
    char *dir = sessions_dir();
    if (!dir) return;
    ensure_dir(dir);
    free(dir);
}

static void append_history(const char *role, const char *text) {
    ensure_sessions_dir();

    char *hp = history_path();
    if (!hp) return;
    FILE *f = fopen(hp, "ab");
    free(hp);
    if (!f) return;

    char stamp[64];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    char *flat = flatten_newlines(text);
    fprintf(f, "%s\t%s\t%s\n", stamp, role, flat ? flat : text);
    free(flat);
    fclose(f);
}

static void append_transcript(const char *prefix, const char *text) {
    ensure_sessions_dir();

    char *tp = transcript_path();
    if (!tp) return;
    FILE *f = fopen(tp, "ab");
    free(tp);
    if (!f) return;
    fprintf(f, "%s %s\n", prefix, text);
    fclose(f);
}

/* Run an op by absolute path and wait for it. Shared by the transport and
 * the completion op. Returns exit status, or -1 if it could not be
 * spawned. */
static int run_op(const char *op_name, const char *arg) {
    char *op = NULL;
    if (asprintf(&op, "%s/ops/+x/%s.+x", project_root, op_name) < 0 || !op) return -1;

    pid_t pid = fork();
    if (pid < 0) { free(op); return -1; }
    if (pid == 0) {
        if (arg) execl(op, op_name, arg, (char *)NULL);
        else     execl(op, op_name, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) { free(op); return -1; }
    free(op);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

/* Hand off to horn_publish so the frame that follows this turn already
 * shows the result rather than needing another poll cycle to catch up. */
static void publish(void) {
    run_op("horn_publish", NULL);
}

/* The LLM transport. It writes its reply to pieces/horn/last_reply.txt,
 * which is how multi-line text comes back without a pipe or a temp-file
 * collision between concurrent turns. */
static int run_transport(const char *prompt) {
    return run_op("horn_chat_backend", prompt);
}

/* '@' completion: append the listing to the transcript as a system note. */
static void run_completions(const char *partial) {
    if (run_op("horn_completions", partial) != 0) {
        append_transcript("horn:", "[completion failed]");
        return;
    }
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/horn/completions.txt", project_root) < 0 || !path) return;

    char *list = read_file(path);
    if (!list) { free(path); return; }

    char *line = list;
    int shown = 0;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (*line && shown < 40) {
            char entry[MSG_CAP];
            snprintf(entry, sizeof(entry), "%s", line);
            append_transcript("  path:", entry);
            shown++;
        }
        line = nl ? nl + 1 : NULL;
    }
    if (shown == 0) append_transcript("horn:", "[no matches]");

    free(list);
    free(path);
}


/* Read a JSON string value's body, WITHOUT terminating early on an escaped
 * quote, and without unescaping it.
 *
 * This exists because of `arguments`. The tool call carries
 *   "arguments": "{\"path\":\"./ops\"}"
 * - a JSON string whose content is itself JSON. A reader that stops at the
 * first '"' returns `{\` and every argument silently vanishes, so the tool
 * falls back to its default path. Observed live as list_dir answering with
 * the project root every single time, and the model re-issuing the same
 * useless call until the round limit. Keep the escapes; the caller
 * unescapes once, deliberately. */
static int read_json_string_body(const char *json, const char *key,
                                 char *out, size_t out_sz) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = json ? strstr(json, pat) : NULL;
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    p++;

    size_t o = 0;
    while (*p && o < out_sz - 1) {
        if (*p == '\\' && p[1]) {          /* keep the pair, keep going */
            out[o++] = *p++;
            out[o++] = *p++;
            continue;
        }
        if (*p == '"') break;                /* real terminator */
        out[o++] = *p++;
    }
    out[o] = '\0';
    return 1;
}

/* Unescape in place. The model's `arguments` is a JSON string wrapping a
 * JSON object, so it arrives with \\" for every quote. The tool op expects
 * the bare object, and passing it through verbatim would make every
 * argument key fail to parse. */
static void unescape_json_body(char *s) {
    size_t o = 0;
    for (size_t i = 0; s[i] && o < strlen(s); i++) {
        if (s[i] == '\\' && s[i + 1]) {
            i++;
            switch (s[i]) {
                case 'n': s[o++] = '\n'; break;
                case 't': s[o++] = '\t'; break;
                case 'r': s[o++] = '\r'; break;
                case '"': s[o++] = '"';  break;
                case '\\': s[o++] = '\\'; break;
                case '/': s[o++] = '/';  break;
                default: s[o++] = s[i];  break;
            }
        } else s[o++] = s[i];
    }
    s[o] = '\0';
}


/* JSON-escape a string for embedding in a message object. */
static char *json_escape_local(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n * 6 + 16);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out[o++] = '\\'; out[o++] = '"';  break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
            case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
            case '\t': out[o++] = '\\'; out[o++] = 't';  break;
            default:
                if (c < 0x20) o += (size_t)sprintf(out + o, "\\u%04x", c);
                else out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return out;
}

/* Seed pieces/horn/convo.json with the system prompt plus this user turn.
 *
 * Done HERE, not in the transport: one user turn can be several provider
 * calls (model -> tool -> model -> tool -> answer), and every one of them
 * must see the same growing conversation. The transport used to re-seed on
 * each call, which erased the tool results between rounds. */
static int seed_convo(const char *user_text) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/horn/convo.json", project_root) < 0 || !path) return 0;

    char *u = json_escape_local(user_text);
    if (!u) { free(path); return 0; }

    FILE *f = fopen(path, "wb");
    if (!f) { free(u); free(path); return 0; }
    fprintf(f,
        "[{\"role\":\"system\",\"content\":\"You are HORN, a terminal-based "
        "assistant working in a code project. Answer clearly and concisely. "
        "Plain text only, no markdown fences. You have read-only tools for "
        "listing, reading and searching files; use them when the answer "
        "depends on the actual contents of the code rather than on "
        "assumption.\"},{\"role\":\"user\",\"content\":\"%s\"}]",
        u);
    fclose(f);
    free(u);
    free(path);
    return 1;
}


/* ── human approval gate ──────────────────────────────────────────────
 *
 * Mirrors gem-dev's cmd_exec, which prints
 *   [SAFEGUARD] Run '<cmd>'? (y/n)
 * and aborts unless the answer is y. One necessary adaptation: gem-dev
 * reads that answer from stdin, because its exec is a foreground call in a
 * plain CLI. HORN's terminal is owned by the chtpm window - renderer and
 * keyboard_input hold it in raw mode - so horn_turn has no stdin to read.
 * The prompt therefore renders in the window and the answer comes back
 * through the window's own key path (gem-dev's `<button onClick="KEY:n">`
 * mechanism, which is how its suggestion buttons already work).
 *
 * Same arming mechanism too: a marker file skips the prompt. gem-dev uses
 * config/yolo.flag, so this does too, at the same path.
 *
 * Why the wait cannot deadlock: the pal loop dispatches horn_turn with a
 * BLOCKING exec. If horn_turn sat waiting for an approval that only the pal
 * loop could deliver, that is a deadlock. So horn_turn detaches itself
 * (see main) and the pal loop stays free to service the approve/deny keys
 * while the turn waits. */

static int yolo_armed(void) {
    char *p = NULL;
    if (asprintf(&p, "%s/config/yolo.flag", project_root) < 0 || !p) return 0;
    int armed = (access(p, F_OK) == 0);
    free(p);
    return armed;
}

/* Publish what needs approving and wait for a decision.
 * Returns 1 = approved, 0 = denied, -1 = timed out. */
static int request_approval(const char *tool, const char *detail) {
    char *pend = NULL, *dec = NULL;
    if (asprintf(&pend, "%s/pieces/horn/pending.json", project_root) < 0 || !pend) return -1;
    if (asprintf(&dec, "%s/pieces/horn/decision.txt", project_root) < 0 || !dec) { free(pend); return -1; }

    unlink(dec);   /* a stale decision must never approve a new request */

    char *esc_tool = json_escape_local(tool);
    char *esc_det  = json_escape_local(detail ? detail : "");
    FILE *f = fopen(pend, "wb");
    if (f) {
        fprintf(f, "{\"tool\":\"%s\",\"detail\":\"%s\"}\n", esc_tool ? esc_tool : "", esc_det ? esc_det : "");
        fclose(f);
    }
    free(esc_tool);
    free(esc_det);

    /* Show it. horn_publish reads pending.json into ${horn_pending}. */
    publish();

    int approved = -1;
    /* 60s, then it is a deny. Long enough to read the command and decide,
     * short enough that an ignored prompt does not wedge the turn - the
     * first version waited 300s, which is indistinguishable from a hang. */
    for (int i = 0; i < 60; i++) {
        FILE *d = fopen(dec, "rb");
        if (d) {
            char c[8] = "";
            if (fgets(c, sizeof(c), d)) {
                if (c[0] == 'y' || c[0] == 'Y') approved = 1;
                else approved = 0;
            }
            fclose(d);
            if (approved >= 0) break;
        }
        sleep(1);
        /* Republish periodically so the box keeps showing the prompt even
         * if the first render landed before the file existed. */
        if (i % 10 == 9) publish();
    }

    unlink(pend);
    unlink(dec);
    publish();
    return approved;
}

/* Is this tool one the human must approve? The list lives in
 * tools/horn_tools.json under "gate" so it is reviewable in one place
 * alongside the allowlist itself - a model that can write could otherwise
 * drop a tool out of "gate" and escalate in one edit. */
static int tool_is_gated(const char *name) {
    char *path = NULL;
    if (asprintf(&path, "%s/tools/horn_tools.json", project_root) < 0 || !path) return 0;
    char *buf = read_file(path);
    free(path);
    if (!buf) return 0;

    const char *g = strstr(buf, "\"gate\"");
    int found = 0;
    if (g) {
        const char *p = g + strlen("\"gate\"");
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == ':') {
            p++;
            const char *close = strchr(p, ']');
            if (close) {
                char pat[256];
                snprintf(pat, sizeof(pat), "\"%s\"", name);
                for (const char *q = p; q < close; q++) {
                    if (strncmp(q, pat, strlen(pat)) == 0) { found = 1; break; }
                }
            }
        }
    }
    free(buf);
    return found;
}

/* ── tool loop ────────────────────────────────────────────────────────
 *
 * The transport never executes anything: on exit 10 it has written
 * pieces/horn/tool_calls.json and left the assistant's tool-call message in
 * pieces/horn/convo.json. This is where the model asks get acted on.
 *
 * Loop shape per turn:
 *   transport -> (10, tool_calls.json) -> execute each call
 *             -> append {role:tool, tool_call_id, content} per call
 *             -> transport again -> ... -> (0, final content) -> done
 *
 * MAX_TOOL_ROUNDS is a hard stop. A model that keeps asking for tools
 * would otherwise spin here forever while the player's terminal looks
 * merely slow - the loop has to give up and say so. */

#define MAX_TOOL_ROUNDS 6

/* Append one {"role":"tool",...} message to the conversation. */
static int convo_append_tool(const char *call_id, const char *name, const char *result) {
    char *cid = json_escape_local(call_id ? call_id : "");
    char *nm  = json_escape_local(name ? name : "");
    char *res = json_escape_local(result ? result : "");
    if (!cid || !nm || !res) { free(cid); free(nm); free(res); return 0; }

    char *msg = NULL;
    if (asprintf(&msg,
        "{\"role\":\"tool\",\"tool_call_id\":\"%s\",\"name\":\"%s\","
        "\"content\":\"%s\"}", cid, nm, res) < 0 || !msg) {
        free(cid); free(nm); free(res);
        return 0;
    }

    char *path = NULL;
    int ok = 0;
    if (asprintf(&path, "%s/pieces/horn/convo.json", project_root) >= 0 && path) {
        char *cur = read_file(path);
        if (!cur) cur = strdup("[]");
        char *last = strrchr(cur, ']');
        if (last) {
            int empty = 1;
            for (char *q = cur; q < last; q++) {
                if (*q != ' ' && *q != '\n' && *q != '\r' && *q != '\t' && *q != '[') { empty = 0; break; }
            }
            size_t need = strlen(cur) + strlen(msg) + 8;
            char *out = malloc(need);
            if (out) {
                *last = '\0';
                snprintf(out, need, "%s%s%s]", cur, empty ? "" : ",", msg);
                FILE *f = fopen(path, "wb");
                if (f) { fputs(out, f); fclose(f); ok = 1; }
                free(out);
            }
        }
        free(cur);
        free(path);
    }
    free(msg); free(cid); free(nm); free(res);
    return ok;
}

/* Run ONE tool call through horn_tool_exec and return its stdout.
 * horn_tool_exec is the allowlist gate - it refuses any tool that is not
 * in tools/horn_tools.json - so this does not need its own check. */
static char *execute_tool(const char *name, const char *args_json) {
    char *op = NULL;
    if (asprintf(&op, "%s/ops/+x/horn_tool_exec.+x", project_root) < 0 || !op) return NULL;

    int fds[2];
    if (pipe(fds) != 0) { free(op); return NULL; }

    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); free(op); return NULL; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl(op, "horn_tool_exec", name, args_json, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    char *buf = malloc(MSG_CAP);
    if (!buf) { close(fds[0]); waitpid(pid, NULL, 0); free(op); return NULL; }
    size_t got = 0;
    ssize_t r;
    while (got < MSG_CAP - 1 && (r = read(fds[0], buf + got, MSG_CAP - 1 - got)) > 0)
        got += (size_t)r;
    buf[got] = '\0';
    close(fds[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    free(op);
    return buf;
}

/* One pass over pieces/horn/tool_calls.json: execute every call, append the
 * results, and note what happened for the transcript. Returns the number of
 * calls executed, or -1 if the file could not be read. */
#define SIG_SLOTS 8

/* Per-tool-name execution count for this turn.
 *
 * String-comparing signatures does not work as a stuck-detector: a model
 * that is going in circles re-formats its arguments every round, so the
 * blobs never match and every comparison says "new call". What IS stable
 * is the tool NAME. A model that calls the same tool three times in one
 * turn is not making progress - it is stuck, and the honest thing is to
 * stop and say so rather than spend the rest of the round cap. */
#define MAX_SAME_TOOL 2
static int g_tool_count[16];
static const char *g_tool_name[16];
static int g_tool_n = 0;

static int bump_tool(const char *name) {
    for (int i = 0; i < g_tool_n; i++)
        if (g_tool_name[i] && strcmp(g_tool_name[i], name) == 0)
            return ++g_tool_count[i];
    if (g_tool_n < 16) {
        g_tool_name[g_tool_n] = strdup(name);
        g_tool_count[g_tool_n] = 1;
        g_tool_n++;
        return 1;
    }
    return 1;
}
/* Signatures of every call already executed THIS turn. The first version
 * compared only against the immediately previous round, which is not the
 * same thing: groq/gpt-oss re-issued an identical list_dir across
 * consecutive rounds with a slightly different arguments blob, so the
 * pairwise comparison never matched and the loop burned its full round cap
 * re-running the same command. Remembering every signature makes a repeat
 * detectable however far apart it happens. */
static char g_seen[SIG_SLOTS][2048];
static int  g_seen_n = 0;

/* Has this exact call already been executed this turn? Authoritative check;
 * the pairwise prev/cur comparison could not catch a repeat that was not
 * immediately adjacent. */
static int seen_before(const char *sig) {
    for (int i = 0; i < g_seen_n; i++)
        if (strcmp(g_seen[i], sig) == 0) return 1;
    return 0;
}
static void remember(const char *sig) {
    if (g_seen_n < SIG_SLOTS) snprintf(g_seen[g_seen_n++], sizeof(g_seen[0]), "%s", sig);
}

static int run_tool_calls(const char *prev_sig, char *cur_sig, size_t cur_sz) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/horn/tool_calls.json", project_root) < 0 || !path) return -1;
    char *raw = read_file(path);
    free(path);
    if (!raw) return 0;

    int executed = 0, all_repeated = 1, repeated_tool = 0;
    if (cur_sig && cur_sz) cur_sig[0] = '\0';
    char sig_raw[2048];
    sig_raw[0] = '\0';
    int sig_n = g_seen_n;   /* index the next signature will land at */
    const char *p = raw;
    /* Walk the array element by element, one tool-call object at a time. */
    while ((p = strstr(p, "\"function\"")) != NULL) {
        const char *obj = p;
        /* step back to the enclosing '{' */
        while (obj > raw && *obj != '{') obj--;
        char call[MSG_CAP];
        size_t ci = 0;
        int depth = 0, in_str = 0, esc = 0, done = 0;
        for (const char *q = obj; *q && !done; q++) {
            char c = *q;
            if (in_str) {
                if (esc) esc = 0;
                else if (c == '\\') esc = 1;
                else if (c == '"') in_str = 0;
            } else {
                if (c == '"') in_str = 1;
                else if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) done = 1; }
            }
            if (ci < MSG_CAP - 1) call[ci++] = c;
        }
        call[ci] = '\0';

        char name[256] = "", args[MSG_CAP] = "", id[256] = "";
        if (!read_json_string_body(call, "id", id, sizeof(id))) id[0] = '\0';
        if (read_json_string_body(call, "name", name, sizeof(name))) {
            /* "arguments" is a JSON string containing escaped JSON; pass it
             * through unescaped so the op receives the object it expects. */
            if (!read_json_string_body(call, "arguments", args, sizeof(args))) args[0] = '\0';
            unescape_json_body(args);

            char note[MSG_CAP];
            snprintf(note, sizeof(note), "%s%s", name, args[0] ? " ..." : "");
            append_transcript("  tool:", note);

            size_t nl = strlen(sig_raw);
            snprintf(sig_raw + nl, sizeof(sig_raw) - nl, "%s%s|", name, args);
            { char probe[2048];
              snprintf(probe, sizeof(probe), "%s%s|", name, args);
              if (!seen_before(probe)) all_repeated = 0;
              if (sig_n < SIG_SLOTS)
                  snprintf(g_seen[sig_n], sizeof(g_seen[0]), "%s", probe); }

            /* Gated tools wait for a human unless the session is armed
             * with config/yolo.flag. Deny is the default and a timeout is
             * a deny: silence must never mean consent. */
            if (tool_is_gated(name) && !yolo_armed()) {
                char detail[MSG_CAP];
                snprintf(detail, sizeof(detail), "%s%s%s", name, args[0] ? " " : "", args);
                int ok = request_approval(name, detail);
                if (ok != 1) {
                    char msg[MSG_CAP];
                    snprintf(msg, sizeof(msg), "[%s DENIED by user%s]", name,
                             ok == -1 ? " (timed out)" : "");
                    append_transcript("  tool:", msg);
                    convo_append_tool(id, name,
                        "error: the user denied this tool call. Do not retry it "
                        "and do not look for another route to the same effect - "
                        "report what you cannot do and why.");
                    continue;
                }
                append_transcript("  tool:", "[approved]");
            }

            /* Stuck-detector: refuse to run the same tool a third time in
             * one turn. */
            if (bump_tool(name) > MAX_SAME_TOOL) {
                char msg[MSG_CAP];
                snprintf(msg, sizeof(msg),
                         "[%s called %d times this turn with no progress - stopped]",
                         name, MAX_SAME_TOOL);
                append_transcript("  tool:", msg);
                convo_append_tool(id, name,
                    "error: you have already called this tool repeatedly in this "
                    "turn and are not making progress. Use the result you "
                    "already have, or answer directly.");
                repeated_tool = 1;
                continue;
            }

            char *res = execute_tool(name, args);
            if (res) {
                /* Keep the transcript readable: a full file dump would
                 * scroll the conversation out of a fixed-height box. */
                char one[600];
                snprintf(one, sizeof(one), "%s -> %.400s", name, res);
                for (char *nl = strchr(one, '\n'); nl; nl = strchr(nl, '\n')) *nl = ' ';
                append_transcript("  tool:", one);
                convo_append_tool(id, name, res);
                free(res);
            } else {
                convo_append_tool(id, name, "error: tool execution failed");
            }
            executed++;
        }
        p += 1;
    }
    free(raw);

    /* Fingerprint of everything we ran this round. Compared against the
     * previous round by the caller. */
    if (cur_sig && cur_sz) snprintf(cur_sig, cur_sz, "%s", sig_raw);
    for (int i = sig_n; i < g_seen_n; i++) remember(g_seen[i]);
    /* Every call this round was already executed this turn: a genuine
     * repeat, not a new question. Stop rather than re-running it. */
    if (repeated_tool) return -3;
    if (executed > 0 && all_repeated) return -2;
    (void)prev_sig;
    return executed;
}

int main(void) {
    resolve_root();

    /* Detach unless the caller wants to wait.
     *
     * The pal loop dispatches this op with a BLOCKING exec. A turn that
     * stops at an approval prompt would therefore wedge the only process
     * that can deliver the answer. Forking here makes the pal loop's exec
     * return at once while the real turn keeps running in the background;
     * horn_chat.sh `send` sets HORN_FOREGROUND=1 because it wants to wait
     * and print the transcript. */
    if (!getenv("HORN_FOREGROUND")) {
        pid_t bg = fork();
        if (bg < 0) return 1;
        if (bg > 0) _exit(0);
        setsid();
    }

    char gp[PATH_BUF];
    gui_state_path(gp, sizeof(gp));

    char prompt[MSG_CAP];
    prompt[0] = '\0';

    /* Read under the cli_io's target_id, which is what chtpm's Enter
     * handler saved under (save_cli_io_gui_state(el->target_id[0] ?
     * el->target_id : "input_text", ...)). Accept the bare "input_text"
     * key as a fallback so a hand-written gui_state.txt - which is what
     * `horn_chat.sh send` writes - works without naming the target. */
    if (!read_kv(gp, "horn_prompt", prompt, sizeof(prompt)) || prompt[0] == '\0')
        read_kv(gp, "input_text", prompt, sizeof(prompt));
    if (prompt[0] == '\0') {
        /* Nothing submitted (empty buffer, or a stray Enter). Nothing to do. */
        return 0;
    }

    /* Clear the composer, but ONLY if it still holds what we just consumed.
     *
     * horn_turn detaches (see main) so the pal loop can service an approval
     * prompt, which means this turn is no longer holding the UI hostage -
     * the player can start typing the next message while it runs. An
     * unconditional clear here deleted that: 60s of typing, thrown away by
     * the turn that was already finished reading. Re-read and compare, so
     * new text survives.
     *
     * Clearing is still what stops chtpm's sync_cli_input_from_gui_state()
     * resurrecting the previous turn's text into the box on the next
     * frame, so the clear has to happen - just not unconditionally. */
    {
        char now[MSG_CAP];
        char key[64];
        snprintf(key, sizeof(key), "%s", "horn_prompt");
        int unchanged = read_kv(gp, key, now, sizeof(now)) && strcmp(now, prompt) == 0;
        if (unchanged) {
            set_kv(gp, "horn_prompt", "");
            set_kv(gp, "input_text", "");
        }
    }

    /* '@' at the start of a line is a completion request, not a message -
     * the same convention gem-dev's composer uses. Handled here, in the
     * harness, rather than in the parser: chtpm renders a layout and knows
     * nothing about what text is destined for a model, so prompt-level
     * parsing belongs in this layer.
     *
     * The listing goes to the transcript and the composer ends up EMPTY.
     * That is deliberate and it is chtpm's doing, not ours: its cli_io
     * Enter handler clears the element buffer before the turn ever runs,
     * and its sync_cli_input_from_gui_state() deliberately skips the
     * element that is currently active, so there is no supported way to
     * push text back into the box. Restoring the partial path into
     * gui_state would look like it worked while changing nothing on
     * screen. The player retypes the path they want; v0.1 is not a
     * completion popup. */
    const char *p = prompt;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '@') {
        run_completions(p);
        publish();
        return 0;
    }

    /* '!' forces a tool call this turn. Measured 2026-10-01: with
     * tool_choice:"auto" a model answers 8-15 times out of 10 depending on
     * provider, and forced is 10/10 - but forcing on EVERY request would
     * break ordinary chat, because the model would have to call a tool even
     * for "hello". So the policy is per-turn and explicit: '!' when you want
     * the tool to fire, plain text when you want conversation. */
    int force_tool = 0;
    const char *ask = prompt;
    if (*ask == '!') { force_tool = 1; ask++; while (*ask == ' ') ask++; }

    append_history("user", ask);
    append_transcript("you:", ask);

    if (!seed_convo(ask)) {
        append_transcript("horn:", "[could not start the conversation]");
        publish();
        return 0;
    }

    int rc;
    int rounds = 0;
    char sig_prev[2048] = "", sig_cur[2048];
    for (;;) {
        /* Force only the FIRST request of the turn.
         *
         * tool_choice:"required" means "this response MUST contain a tool
         * call" - on every request, including the one that follows a tool
         * result. Forcing it throughout the loop therefore cannot terminate:
         * the model dutifully calls a tool again, gets a result, is forced
         * to call another, and the turn ends at the round cap with
         * "[stopped: the model repeated the same tool call]". Observed live.
         * Forcing once gets what '!' actually means - "use a tool this
         * turn" - and the rest of the loop is back to auto, so the model
         * can read the result and answer. */
        if (force_tool && rounds == 0) setenv("HORN_TOOL_CHOICE", "required", 1);
        rc = run_transport(ask);
        unsetenv("HORN_TOOL_CHOICE");

        if (rc != 10) break;                 /* 0 = answer, else a failure */
        if (++rounds >= MAX_TOOL_ROUNDS) {
            append_transcript("horn:",
                "[stopped: the model kept asking for tools and hit the "
                "round limit]");
            rc = 2;
            break;
        }
        snprintf(sig_prev, sizeof(sig_prev), "%s", sig_cur);
        int n = run_tool_calls(sig_prev, sig_cur, sizeof(sig_cur));
        if (n == -3) {
            append_transcript("horn:",
                "[stopped: the model kept calling the same tool without making "
                "progress]");
            rc = 2;
            break;
        }
        if (n == -2) {
            /* The model asked for exactly the same thing again. Executing it
             * again would produce the same result and the same answer, so
             * break rather than spin to the round cap looking like a hang. */
            append_transcript("horn:",
                "[stopped: the model repeated the same tool call]");
            rc = 2;
            break;
        }
        if (n <= 0) {
            append_transcript("horn:", "[stopped: no usable tool call]");
            rc = 2;
            break;
        }
    }

    char reply_path[PATH_BUF];
    snprintf(reply_path, sizeof(reply_path), "%s/pieces/horn/last_reply.txt", project_root);

    char reply[MSG_CAP];
    reply[0] = '\0';
    if (rc == 0) {
        char *r = read_file(reply_path);
        if (r) { snprintf(reply, sizeof(reply), "%s", r); trim_in_place(reply); free(r); }
    }

    if (reply[0]) {
        append_history("horn", reply);
        append_transcript("horn:", reply);
    } else if (rc == 3) {
        /* Daily free-tier quota is gone (exit 3). Say exactly that: "all
         * models failed" reads as a broken harness and sends the next
         * person hunting a bug that is not there. */
        append_history("horn", "[OpenRouter daily free-tier quota exhausted - resets at the UTC day boundary]");
        append_transcript("horn:", "[daily free-tier quota exhausted - no model can answer until it resets]");
    } else {
        char msg[MSG_CAP];
        snprintf(msg, sizeof(msg), "[no reply from model%s]",
                 rc == 2 ? " (all models failed)" : "");
        append_history("horn", msg);
        append_transcript("horn:", msg);
    }

    publish();
    return 0;
}