/* execute_tool - the "y" verb. Dispatched from main_loop.pal when
 * input_buffer is exactly "y"/"yes" while ai_state=PENDING_PERM. Ports
 * the same dispatch-by-tool-name logic already proven three times over
 * in gem-dev/groq-ollama/cpp-llm's execute_pending_tool(), as a
 * standalone op instead of a manager function.
 *
 * v1 shows the raw tool result immediately (ai_state -> IDLE) rather than
 * auto-continuing the conversation with a follow-up model call - a
 * deliberate simplification given how slow/variable this project's target
 * hardware has proven to be in practice (see groq-ollama/cpp-llm's own
 * EMERGENCY docs): chaining another THINKING wait immediately after the
 * tool call would double the latency of every tool use. Matches those two
 * projects' raw_tool_followup=1 config default, just without the config
 * toggle - there's only one behavior in v1, not two.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: execute_tool.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], log_path[PATH_BUF], args_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);
    snprintf(args_path, sizeof(args_path), "%s/pieces/world_01/session_01/chat/args.tmp", project_root);

    char tool_name[MAX_FIELD], tool_args[MAX_LINE];
    read_state_field(state_path, "pending_tool_name", tool_name, sizeof(tool_name));
    read_state_field(state_path, "pending_tool_args", tool_args, sizeof(tool_args));
    if (strlen(tool_name) == 0) return 0;

    FILE *af = fopen(args_path, "w");
    if (af) { fputs(strlen(tool_args) > 0 ? tool_args : "{}", af); fclose(af); }

    /* file_ops/cmd_exec/edit_file each read config/context.txt relative to
     * their own CWD for sandbox_depth (same convention gem-dev/groq-ollama/
     * cpp-llm's run_tool(sandbox=true) already established) - chdir into
     * sandbox/ first via the shell so a fresh `pieces` tree, source, etc.
     * can never be touched directly by a tool call. */
    char sandbox_dir[PATH_BUF];
    snprintf(sandbox_dir, sizeof(sandbox_dir), "%s/sandbox", project_root);

    char json_parser_cmd[PATH_BUF * 2];
    char *result = NULL;

    if (strcmp(tool_name, "read_file") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'path'", project_root, args_path);
        char *path = run_tool_capture(json_parser_cmd);
        if (path && strlen(path) > 0) {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/file_ops.+x' read '%s'", sandbox_dir, project_root, path);
            result = run_tool_capture(cmd);
        }
        free(path);
    } else if (strcmp(tool_name, "write_file") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'path'", project_root, args_path);
        char *path = run_tool_capture(json_parser_cmd);
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'content'", project_root, args_path);
        char *content = run_tool_capture(json_parser_cmd);
        if (path && content) {
            char cmd[PATH_BUF * 4];
            snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/file_ops.+x' write '%s' '%s'", sandbox_dir, project_root, path, content);
            result = run_tool_capture(cmd);
        }
        free(path); free(content);
    } else if (strcmp(tool_name, "list_dir") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'path'", project_root, args_path);
        char *path = run_tool_capture(json_parser_cmd);
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/list_dir.+x' '%s'", sandbox_dir, project_root, (path && strlen(path) > 0) ? path : ".");
        result = run_tool_capture(cmd);
        free(path);
    } else if (strcmp(tool_name, "exec_cmd") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'cmd'", project_root, args_path);
        char *shell_cmd = run_tool_capture(json_parser_cmd);
        if (shell_cmd && strlen(shell_cmd) > 0) {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/cmd_exec.+x' '%s'", sandbox_dir, project_root, shell_cmd);
            result = run_tool_capture(cmd);
        }
        free(shell_cmd);
    } else if (strcmp(tool_name, "edit_file") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'path'", project_root, args_path);
        char *path = run_tool_capture(json_parser_cmd);
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'search'", project_root, args_path);
        char *search = run_tool_capture(json_parser_cmd);
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'replace'", project_root, args_path);
        char *replace = run_tool_capture(json_parser_cmd);
        if (path && search && replace) {
            char cmd[PATH_BUF * 6];
            snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/edit_file.+x' '%s' '%s' '%s'", sandbox_dir, project_root, path, search, replace);
            result = run_tool_capture(cmd);
        }
        free(path); free(search); free(replace);
    } else if (strcmp(tool_name, "search_in_files") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'query'", project_root, args_path);
        char *query = run_tool_capture(json_parser_cmd);
        if (query && strlen(query) > 0) {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "cd '%s' && '%s/ops/+x/search_in_files.+x' '%s'", sandbox_dir, project_root, query);
            result = run_tool_capture(cmd);
        }
        free(query);
    } else if (strcmp(tool_name, "web_search") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'query'", project_root, args_path);
        char *query = run_tool_capture(json_parser_cmd);
        if (query && strlen(query) > 0) {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "'%s/ops/+x/web_search.+x' '%s'", project_root, query);
            result = run_tool_capture(cmd);
        }
        free(query);
    } else if (strcmp(tool_name, "speak") == 0) {
        snprintf(json_parser_cmd, sizeof(json_parser_cmd), "'%s/ops/+x/json_parser.+x' '%s' 'text'", project_root, args_path);
        char *text = run_tool_capture(json_parser_cmd);
        if (text && strlen(text) > 0) {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "'%s/ops/+x/tts_speak.+x' '%s'", project_root, text);
            result = run_tool_capture(cmd);
        }
        free(text);
    }

    remove(args_path);

    append_log_turn(log_path, "tool", "text", tool_name, result ? result : "Error: tool execution failed or produced no output.");

    write_state_field(state_path, "pending_tool_name", "");
    write_state_field(state_path, "pending_tool_args", "");
    write_state_field(state_path, "ai_state", "IDLE");
    char msg[MAX_FIELD + 32];
    snprintf(msg, sizeof(msg), "Tool %s completed.", tool_name);
    write_state_field(state_path, "sys_msg", msg);

    free(result);
    return 0;
}
