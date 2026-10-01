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

int main(void) {
    resolve_root();

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

    /* Clear BOTH keys before the network call, not after: the transport can
     * take up to 60s across the model ladder, and chtpm re-renders
     * throughout. Clearing first means the input box is empty and ready for
     * the next message the whole time the player is waiting, instead of
     * still showing the message already in flight.
     *
     * Clearing the target_id key is also what stops chtpm's own
     * sync_cli_input_from_gui_state() from resurrecting the previous turn's
     * text into the box on the next frame. */
    set_kv(gp, "horn_prompt", "");
    set_kv(gp, "input_text", "");

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

    append_history("user", prompt);
    append_transcript("you:", prompt);

    int rc = run_transport(prompt);

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