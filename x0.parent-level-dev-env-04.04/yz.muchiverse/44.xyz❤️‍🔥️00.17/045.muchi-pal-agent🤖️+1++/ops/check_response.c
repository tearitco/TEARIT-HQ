/* check_response - polls for the in-flight request send_message.+x
 * kicked off. Called every tick from main_loop.pal (self-filters: no-op
 * unless ai_state=THINKING, same "runs unconditionally, checks its own
 * relevance" convention mutaclsym's choice.c/move_player.c already use).
 *
 * Ops have no persistent memory between invocations, so "is curl done
 * yet" can only be answered by checking whether the PID send_message.+x
 * recorded is still alive - this process isn't that child's real parent
 * (send_message.+x already exited), so waitpid() isn't available; a
 * plain kill(pid, 0) liveness probe is the only option, which is enough
 * since connect_op/curl are bounded one-shot subprocesses, not daemons.
 *
 * Parses the response per provider_kind (ollama/gemini/gemma/llamacpp/
 * iqabod) - see send_message.c's header comment for what request shape
 * each one sent. iqabod is the odd one out: its response isn't JSON at
 * all, and (unlike llamacpp) its output is never persona-JSON-shaped
 * either - IQABOD's curricula are raw word-continuation models, not
 * instruction-tuned to emit a tool/args/response schema - so that branch
 * only ever produces plain assistant text. gemma is similar in that
 * regard by direct design choice, not a model limitation being worked
 * around: tool execution for gemma is entirely pre-decided/pre-run by
 * gemma_strategy.c + strategy_execute_a.c BEFORE send_message.c ever
 * builds gemma's request (see !.GRAND-PLAN-TOOL-SCAFFOLD.txt), so its
 * own reply is always plain conversational text, never scanned for a
 * tool call.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: check_response.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static char *read_full_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
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

/* popen()'s a compiled op and returns its full stdout, trimmed of a
 * trailing newline - the same op-calls-op-via-subprocess convention
 * choice.c/pdl_reader.c already use, not a library call (no shared
 * headers to link against). */
static char *run_tool_capture(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    char *buf = malloc(65536);
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, 65536 - total - 1, pipe)) > 0) {
        total += n;
        if (total >= 65535) break;
    }
    buf[total] = '\0';
    pclose(pipe);
    while (total > 0 && (buf[total - 1] == '\n' || buf[total - 1] == '\r')) buf[--total] = '\0';
    return buf;
}

/* trigger_render - ported directly from pal-chat-irc's own ops/
 * chat_inbox_watcher.c trigger_render() (identical shape, identical
 * reasoning - see that file's own header comment, and check_response.c's
 * own call site below for the full citation). Shells out to
 * compose_frame.+x the instant a response is ready, rather than only
 * pinging a marker and trusting some OTHER, later poll to notice. */
static void trigger_render(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/compose_frame.+x >/dev/null 2>&1", project_root);
    if (system(cmd) != 0) { /* best-effort - a failed re-render just means the next real event will catch up */ }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], log_path[PATH_BUF], response_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);
    snprintf(response_path, sizeof(response_path), "%s/pieces/world_01/session_01/chat/llm_response.json", project_root);

    char ai_state[MAX_FIELD];
    read_state_field(state_path, "ai_state", ai_state, sizeof(ai_state));
    if (strcmp(ai_state, "THINKING") != 0) return 0;

    char pid_str[MAX_FIELD];
    read_state_field(state_path, "curl_pid", pid_str, sizeof(pid_str));
    pid_t pid = (pid_t)atoi(pid_str);
    if (pid > 0) {
        if (kill(pid, 0) == 0 || errno != ESRCH) return 0;
    }

    char provider_kind[MAX_FIELD];
    read_state_field(state_path, "provider_kind", provider_kind, sizeof(provider_kind));

    char json_parser_cmd[PATH_BUF * 2];
    char *content = NULL;      /* plain text to show, if no tool call */
    char *tool_name = NULL;
    char *tool_args = NULL;

    /* connect_op.c now records its own curl exit status alongside
     * response_path (see that file's own header comment) specifically so
     * a connection failure (refused/unreachable/timed out - the actual
     * live symptom that exposed this: gemma-local's curl couldn't reach
     * localhost:11434, never got far enough to touch llm_response.json
     * at all, and this used to silently fall through to whatever a
     * PREVIOUS unrelated call had left there) gets an honest, distinct
     * error instead of the generic "empty or unparseable response" a
     * malformed-but-real API reply would produce. iqabod never goes
     * through connect_op/response_path at all - no sidecar to check. */
    if (strcmp(provider_kind, "iqabod") != 0) {
        char status_path[PATH_BUF];
        snprintf(status_path, sizeof(status_path), "%s.status", response_path);
        char *status_str = read_full_file(status_path);
        int curl_exit = status_str ? atoi(status_str) : 0;
        free(status_str);
        remove(status_path);
        if (curl_exit != 0) {
            /* Distinct messages for the two real, expected curl failure
             * modes, matching gem-dev_manager.c's own documented
             * distinction (#.dox/groq-api-handoff.txt: "Curl exit codes
             * are checked - Code 7 = connection fail, Code 28 = timeout")
             * - a timeout (cold model load under load, see connect_op.c's
             * own updated 600s comment) is a completely different, much
             * less alarming situation than the host being unreachable,
             * and telling them apart is exactly what that handoff's own
             * real debugging needed. */
            if (curl_exit == 28) {
                write_state_field(state_path, "sys_msg", "API Error: request timed out (model may be cold-loading - try again).");
            } else if (curl_exit == 7) {
                write_state_field(state_path, "sys_msg", "API Error: could not connect (host unreachable - check the model's api_url).");
            } else {
                char msg[MAX_FIELD];
                snprintf(msg, sizeof(msg), "API Error: curl failed (exit code %d).", curl_exit);
                write_state_field(state_path, "sys_msg", msg);
            }
            write_state_field(state_path, "ai_state", "IDLE");
            char marker_path[PATH_BUF];
            snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chat_screen_changed.txt", project_root);
            FILE *mf = fopen(marker_path, "a");
            if (mf) { fputc('.', mf); fclose(mf); }
            trigger_render();
            return 0;
        }
    }

    if (strcmp(provider_kind, "iqabod") == 0) {
        /* iqabod: plain-text file, not JSON. generation_module.c prints
         * several stdout lines per run (progress, the final line, then
         * an attention-map confirmation after it) - match specifically
         * on the "Final generated text: " prefix rather than assuming
         * it's the last line. That line is prompt + " " + generated
         * tokens all on one line (output starts as strcpy(output,
         * prompt) - see generation_module.c), so strip the prompt back
         * off the front using the exact prompt text send_message.c
         * persisted alongside it, rather than guessing where it ends. */
        char iqabod_response_path[PATH_BUF], iqabod_prompt_path[PATH_BUF];
        snprintf(iqabod_response_path, sizeof(iqabod_response_path), "%s/pieces/world_01/session_01/chat/iqabod_response.txt", project_root);
        snprintf(iqabod_prompt_path, sizeof(iqabod_prompt_path), "%s/pieces/world_01/session_01/chat/iqabod_prompt.tmp", project_root);

        char *raw_out = read_full_file(iqabod_response_path);
        char *sent_prompt = read_full_file(iqabod_prompt_path);
        const char *marker = "Final generated text: ";
        char *hit = raw_out ? strstr(raw_out, marker) : NULL;
        if (hit) {
            char *line_start = hit + strlen(marker);
            char *line_end = strchr(line_start, '\n');
            size_t line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);

            size_t prompt_len = sent_prompt ? strlen(sent_prompt) : 0;
            const char *new_part = line_start;
            size_t new_len = line_len;
            if (prompt_len > 0 && line_len >= prompt_len && strncmp(line_start, sent_prompt, prompt_len) == 0) {
                new_part = line_start + prompt_len;
                new_len = line_len - prompt_len;
                if (new_len > 0 && new_part[0] == ' ') { new_part++; new_len--; }
            }
            content = malloc(new_len + 1);
            memcpy(content, new_part, new_len);
            content[new_len] = '\0';
        }
        free(raw_out);
        free(sent_prompt);
        remove(iqabod_prompt_path);
    } else if (strcmp(provider_kind, "gemma") == 0) {
        /* Gemma NEVER calls tools itself - tool execution is entirely
         * pre-decided/pre-run by gemma_strategy.c + strategy_execute_a.c
         * BEFORE send_message.c ever builds this request (see
         * !.GRAND-PLAN-TOOL-SCAFFOLD.txt's three-layer architecture:
         * intent detection -> pre-execution -> Gemma just wraps the
         * already-logged result conversationally). Scanning Gemma's own
         * reply for a "TOOL:" line and treating it as a real tool call
         * was a leftover pre-scaffolding hack (prompt_keyword.txt used
         * to instruct Gemma to emit that format) - direct instruction:
         * "gemma should never parse tool etc." Whatever Gemma says is
         * always plain conversational text now, no exceptions. */
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.content'", project_root, response_path);
        content = run_tool_capture(json_parser_cmd);
    } else if (strcmp(provider_kind, "gemini") == 0) {
        /* Gemini shape: candidates[0].content.parts[0].text or
         * .functionCall (name/args directly, not array-indexed the way
         * OpenAI-style tool_calls are - a single object, not a list). */
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'candidates[0].content.parts[0].text'", project_root, response_path);
        content = run_tool_capture(json_parser_cmd);

        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'candidates[0].content.parts[0].functionCall'", project_root, response_path);
        char *func_call = run_tool_capture(json_parser_cmd);
        if (func_call && strlen(func_call) > 2) {
            char tmp_path[PATH_BUF];
            snprintf(tmp_path, sizeof(tmp_path), "%s/pieces/world_01/session_01/chat/tool_calls.tmp", project_root);
            FILE *tf = fopen(tmp_path, "w");
            if (tf) { fputs(func_call, tf); fclose(tf); }
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'name'", project_root, tmp_path);
            tool_name = run_tool_capture(json_parser_cmd);
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'args'", project_root, tmp_path);
            tool_args = run_tool_capture(json_parser_cmd);
            remove(tmp_path);
        }
        free(func_call);

        /* error.message fallback, ported directly from gem-dev_manager.c's
         * own check_ai_status() (same field, same reasoning: Gemini
         * returns real 400/403/etc. bodies shaped {"error":{"message":
         * ...}} - e.g. a malformed function-declaration schema, a safety
         * block, an invalid API key - and neither "text" nor
         * "functionCall" will ever be present in that shape, so this
         * project's own code fell straight through to the generic
         * "empty or unparseable response" and hid the REAL reason from
         * the user every time. gem-dev never had this gap - it's the one
         * check_response.c was missing relative to that reference). Only
         * bother checking when both real fields came back empty - a
         * normal successful reply never has an error.message key at all. */
        if ((!content || strlen(content) == 0) && !tool_name) {
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'error.message'", project_root, response_path);
            char *error_msg = run_tool_capture(json_parser_cmd);
            if (error_msg && strlen(error_msg) > 0) {
                free(content);
                content = NULL;
                char errbuf[MAX_LINE];
                snprintf(errbuf, sizeof(errbuf), "[Gemini API error] %s", error_msg);
                write_state_field(state_path, "ai_state", "IDLE");
                write_state_field(state_path, "sys_msg", errbuf);
                char marker_path[PATH_BUF];
                snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chat_screen_changed.txt", project_root);
                FILE *mf = fopen(marker_path, "a");
                if (mf) { fputc('.', mf); fclose(mf); }
                trigger_render();
                free(error_msg);
                free(tool_args);
                return 0;
            }
            free(error_msg);
        }
    } else if (strcmp(provider_kind, "llamacpp") == 0) {
        /* /completion's response is flat {"content": "...", ...} - the
         * model's own content is itself the persona-JSON blob
         * ({"tool":...,"args":...} or {"response":...}), matching
         * cpp-llm's proven prompt-JSON convention (no native tool schema
         * on this path at all). */
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'content'", project_root, response_path);
        char *raw_content = run_tool_capture(json_parser_cmd);
        if (raw_content && strlen(raw_content) > 0) {
            char tmp_path[PATH_BUF];
            snprintf(tmp_path, sizeof(tmp_path), "%s/pieces/world_01/session_01/chat/llm_content.tmp", project_root);
            FILE *tf = fopen(tmp_path, "w");
            if (tf) { fputs(raw_content, tf); fclose(tf); }

            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'tool'", project_root, tmp_path);
            tool_name = run_tool_capture(json_parser_cmd);
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'args'", project_root, tmp_path);
            tool_args = run_tool_capture(json_parser_cmd);

            if (!tool_name || strlen(tool_name) == 0) {
                snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'response'", project_root, tmp_path);
                content = run_tool_capture(json_parser_cmd);
                if (!content || strlen(content) == 0) { free(content); content = strdup(raw_content); }
            }
            remove(tmp_path);
        }
        free(raw_content);
    } else {
        /* Native Ollama /api/chat: message.content / message.tool_calls
         * (array of {"function":{"name":...,"arguments":...}}) - proven
         * in groq-ollama. */
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.content'", project_root, response_path);
        content = run_tool_capture(json_parser_cmd);

        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'message.tool_calls'", project_root, response_path);
        char *tool_calls = run_tool_capture(json_parser_cmd);
        if (tool_calls && strlen(tool_calls) > 2) {
            char tmp_path[PATH_BUF];
            snprintf(tmp_path, sizeof(tmp_path), "%s/pieces/world_01/session_01/chat/tool_calls.tmp", project_root);
            FILE *tf = fopen(tmp_path, "w");
            if (tf) { fputs(tool_calls, tf); fclose(tf); }
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' '[0].function.name'", project_root, tmp_path);
            tool_name = run_tool_capture(json_parser_cmd);
            snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' '[0].function.arguments'", project_root, tmp_path);
            tool_args = run_tool_capture(json_parser_cmd);
            remove(tmp_path);
        }
        free(tool_calls);
    }

    /* Unified from here regardless of provider: same PENDING_PERM /
     * IDLE-with-text / error handling every provider_kind funnels into.
     * Signal frame update by appending to the marker file (still needed -
     * main_loop_chtpm.pal's own read_pos check still watches this) AND,
     * per direct instruction ("do what gem-dev does... doesn't it
     * trigger redraw at that point"), directly invoke compose_frame.+x
     * itself right now, matching two proven references exactly:
     *   - gem-dev_manager.c's own main loop (line ~1408-1427): the SAME
     *     function that just detected the HTTP response calls
     *     write_gui_state()+trigger_render() itself, unconditionally,
     *     right there - it never relies on some OTHER, later poll to
     *     separately notice and redraw.
     *   - pal-chat-irc's ops/chat_inbox_watcher.c own trigger_render()
     *     (that file's own header comment, line 55-88): shells out to
     *     chat_compose_frame.+x DIRECTLY the instant new data is
     *     detected - its own comment explains why the marker-only
     *     version wasn't enough: "that marker only tells [the renderer]
     *     re-read whatever's currently in view.txt - it does NOT
     *     regenerate view.txt itself... the marker got pinged but
     *     view.txt stayed stale." compose_frame.c already pings its own
     *     frame_changed.txt at its own end (see that file's own tail),
     *     so this one call is complete - no separate hit_frame needed. */
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chat_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    if (tool_name && strlen(tool_name) > 0) {
        write_state_field(state_path, "pending_tool_name", tool_name);
        write_state_field(state_path, "pending_tool_args", tool_args ? tool_args : "{}");
        write_state_field(state_path, "ai_state", "PENDING_PERM");
        char msg[MAX_FIELD];
        snprintf(msg, sizeof(msg), "Execute %s? (y/n)", tool_name);
        write_state_field(state_path, "sys_msg", msg);
        append_log_turn(log_path, "assistant", "tool_call", tool_name, tool_args ? tool_args : "{}");
    } else if (content && strlen(content) > 0) {
        append_log_turn(log_path, "assistant", "text", NULL, content);
        write_state_field(state_path, "ai_state", "IDLE");
        write_state_field(state_path, "sys_msg", "Response received.");
    } else {
        write_state_field(state_path, "ai_state", "IDLE");
        write_state_field(state_path, "sys_msg", "API Error: empty or unparseable response.");
    }

    trigger_render();

    free(content);
    free(tool_name);
    free(tool_args);
    return 0;
}
