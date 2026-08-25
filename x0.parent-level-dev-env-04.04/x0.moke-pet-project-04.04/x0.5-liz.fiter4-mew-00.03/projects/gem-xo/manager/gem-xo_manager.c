#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/file.h>

#define PROJECT_ID "gem-xo"
#define MAX_PATH 4096
#define MAX_LINE 1024

#ifndef SANDBOX_SCOPE_DEFAULT_ROOT
#define SANDBOX_SCOPE_DEFAULT_ROOT "projects/gem-xo/sandbox"
#endif

static void sandbox_scope_trim(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static int sandbox_scope_read_kv(const char *path, const char *key, char *out, size_t out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = line;
        sandbox_scope_trim(trimmed);
        if (*trimmed == '\0' || *trimmed == '#') continue;

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';

        char *k = trimmed;
        char *v = eq + 1;
        sandbox_scope_trim(k);
        sandbox_scope_trim(v);

        if (strcmp(k, key) == 0) {
            snprintf(out, out_size, "%s", v);
            found = 1;
            break;
        }
    }

    fclose(f);
    return found;
}

static void sandbox_scope_join(const char *base, const char *path, char *out, size_t out_size) {
    if (!path || !*path) {
        snprintf(out, out_size, "%s", base && *base ? base : ".");
        return;
    }

    if (path[0] == '/') {
        snprintf(out, out_size, "%s", path);
        return;
    }

    if (!base || !*base) {
        snprintf(out, out_size, "%s", path);
        return;
    }

    if (strcmp(base, "/") == 0) {
        snprintf(out, out_size, "/%s", path);
    } else {
        snprintf(out, out_size, "%s/%s", base, path);
    }
}

static int sandbox_scope_normalize(const char *path, char *out, size_t out_size) {
    if (!path || !*path) {
        if (!getcwd(out, out_size)) return 0;
        return 1;
    }

    char combined[PATH_MAX];
    if (path[0] == '/') {
        snprintf(combined, sizeof(combined), "%s", path);
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return 0;
        sandbox_scope_join(cwd, path, combined, sizeof(combined));
    }

    char scratch[PATH_MAX];
    snprintf(scratch, sizeof(scratch), "%s", combined);

    char *segments[PATH_MAX / 2];
    int depth = 0;
    char *saveptr = NULL;
    for (char *token = strtok_r(scratch, "/", &saveptr); token; token = strtok_r(NULL, "/", &saveptr)) {
        if (strcmp(token, "") == 0 || strcmp(token, ".") == 0) continue;
        if (strcmp(token, "..") == 0) {
            if (depth > 0) depth--;
            continue;
        }
        if (depth < (int)(sizeof(segments) / sizeof(segments[0]))) {
            segments[depth++] = token;
        }
    }

    size_t pos = 0;
    if (pos < out_size) out[pos++] = '/';
    for (int i = 0; i < depth; i++) {
        size_t len = strlen(segments[i]);
        if (pos + len >= out_size) return 0;
        memcpy(out + pos, segments[i], len);
        pos += len;
        if (i + 1 < depth) {
            if (pos + 1 >= out_size) return 0;
            out[pos++] = '/';
        }
    }

    if (depth == 0) {
        if (out_size < 2) return 0;
        out[0] = '/';
        out[1] = '\0';
        return 1;
    }

    out[pos] = '\0';
    return 1;
}

static int sandbox_scope_is_within_root(const char *path, const char *root) {
    char normalized_root[PATH_MAX];
    char normalized_path[PATH_MAX];
    if (!sandbox_scope_normalize(root, normalized_root, sizeof(normalized_root))) return 0;
    if (!sandbox_scope_normalize(path, normalized_path, sizeof(normalized_path))) return 0;

    size_t root_len = strlen(normalized_root);
    if (strncmp(normalized_path, normalized_root, root_len) != 0) return 0;
    return normalized_path[root_len] == '\0' || normalized_path[root_len] == '/';
}

static void sandbox_scope_load_root(const char *config_path, char *out, size_t out_size) {
    snprintf(out, out_size, "%s", SANDBOX_SCOPE_DEFAULT_ROOT);
    if (config_path && sandbox_scope_read_kv(config_path, "sandbox_root", out, out_size)) return;
}

// PID tracking for CPU Safety
static void log_pid(pid_t pid, const char* name) {
    FILE *f = fopen("pieces/os/proc_list.txt", "a");
    if (f) {
        int fd = fileno(f);
        flock(fd, LOCK_EX);
        fprintf(f, "%d %s\n", pid, name);
        flock(fd, LOCK_UN);
        fclose(f);
    }
}

static char* get_gemini_api_key() {
    char* key_env = getenv("GEMINI_API_KEY");
    if (key_env && strlen(key_env) > 0) return strdup(key_env);
    FILE* f = fopen("config/google-lilsol-api-key.txt", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
            size_t len = strlen(p);
            while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r' || p[len-1] == '\n')) {
                p[len-1] = '\0';
                len--;
            }
            fclose(f);
            if (len > 0) return strdup(p);
        } else {
            fclose(f);
        }
    }
    return NULL;
}

// Persistent Global UI State
static char g_ai_state[64] = "IDLE";
static char g_api_url[256] = "https://generativelanguage.googleapis.com";
static char g_current_model[256] = "llama3:latest";
static char g_ctx_pct[16] = "0%";
static char g_fsm_state[128] = "IDLE";
static char g_resp_area[8192] = "║ Ready for input...                                                         ║";
static char g_sys_msg[256] = "Initialized.";
static int g_thinking_secs = 0;
static time_t g_thinking_start = 0;
static char g_ai_status_line[512] = "";

// API Menu State
typedef struct {
    char name[64];
    char url[256];
} APIEntry;
static APIEntry g_api_list[16];
static int g_api_count = 0;
static char g_menu_area[4096] = "";
static char last_frame_methods[4096] = "";

static char project_root[MAX_PATH] = ".";
static char g_sandbox_root[MAX_PATH] = SANDBOX_SCOPE_DEFAULT_ROOT;
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static volatile sig_atomic_t g_shutdown = 0;
static pid_t g_ai_pid = -1;
static char *g_pending_input = NULL;

// Pending Tool State for 'y/n' Permissions
static char g_pending_tool_name[128] = "";
static char *g_pending_args_json = NULL;
static char *g_pending_function_call = NULL;
static bool g_have_tool_result_for_followup = false;
static char g_last_tool_name[128] = "";
static char g_last_tool_result[8192] = "";

// Completion State
static bool g_completion_mode = false;
static char g_piece_methods[4096] = "";
static char g_last_buf_input[1024] = "";
static char g_input_text_state[1024] = "";
static int g_completion_return_gui_index = -1;
static long g_buf_last_pos = 0;
static char g_comp_labels[5][256];
static bool g_comp_vis[5];
static bool g_comp_root_vis = false;

static void handle_sig(int s) { (void)s; g_shutdown = 1; }

static void truncate_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

static void pulse_state_changed(void) {
    FILE *f = fopen("pieces/apps/player_app/state_changed.txt", "a");
    if (f) {
        fputs("S\n", f);
        fclose(f);
    }
}

static int read_active_gui_index(void) {
    FILE *f = fopen("pieces/display/active_gui_index.txt", "r");
    if (!f) return -1;
    char line[64];
    int idx = -1;
    if (fgets(line, sizeof(line), f)) idx = atoi(line);
    fclose(f);
    return idx;
}

static void clear_render_artifacts(void) {
    truncate_file("pieces/display/current_frame.txt");
    truncate_file("pieces/display/frame_changed.txt");
    truncate_file("pieces/display/active_gui_index.txt");
    truncate_file("pieces/chtpm/frame_buffer/current_frame.txt");
    truncate_file("pieces/chtpm/frame_buffer/frame_changed.txt");
}

static void ensure_dir_path(const char *path) {
    if (!path || !*path) return;

    char tmp[MAX_PATH];
    if (!sandbox_scope_normalize(path, tmp, sizeof(tmp))) {
        snprintf(tmp, sizeof(tmp), "%s", path);
    }

    size_t len = strlen(tmp);
    if (len == 0) return;

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void reset_session_ui(void) {
    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_ctx_pct, sizeof(g_ctx_pct), "0%%");
    snprintf(g_resp_area, sizeof(g_resp_area), "║ Ready for input...                                                         ║");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Initialized.");
    g_thinking_secs = 0;
    g_thinking_start = 0;
    g_completion_mode = false;
    g_comp_root_vis = false;
    g_piece_methods[0] = '\0';
    g_last_buf_input[0] = '\0';
    g_input_text_state[0] = '\0';
    g_completion_return_gui_index = -1;
    g_have_tool_result_for_followup = false;
    g_last_tool_name[0] = '\0';
    g_last_tool_result[0] = '\0';
    for (int i = 0; i < 5; i++) {
        g_comp_labels[i][0] = '\0';
        g_comp_vis[i] = false;
    }
}

// Forward Declarations
char* get_gui_var(const char* id);
void write_gui_state(void);
static void set_input_text_override(const char *value);
void start_ai_query(const char* input);
void format_response(const char* src);
void trigger_render(void);
void handle_choose_path(int index);
void update_completions(void);
void audit_log(const char* user, const char* assistant);
static char* read_full_file(const char* path);
static char* extract_text_field_from_raw_json(const char* path);
static char* extract_response_field(const char* text);

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

static int is_quota_error_text(const char* text) {
    return text &&
           (strstr(text, "Quota exceeded") ||
            strstr(text, "quota exceeded") ||
            strstr(text, "RESOURCE_EXHAUSTED") ||
            strstr(text, "rate-limits"));
}

static char* extract_direct_search_query(const char* input) {
    if (!input) return NULL;
    char *copy = strdup(input);
    if (!copy) return NULL;
    char *trimmed = trim_str(copy);
    const char *prefix = NULL;

    if (strncasecmp(trimmed, "internet search ", 16) == 0) {
        prefix = trimmed + 16;
    } else if (strncasecmp(trimmed, "search ", 7) == 0) {
        prefix = trimmed + 7;
    } else if (strncasecmp(trimmed, "web search ", 11) == 0) {
        prefix = trimmed + 11;
    }

    if (!prefix) {
        free(copy);
        return NULL;
    }

    while (*prefix && isspace((unsigned char)*prefix)) prefix++;
    if (!*prefix) {
        free(copy);
        return NULL;
    }

    char *out = strdup(prefix);
    free(copy);
    return out;
}

static bool web_search_ignored(void) {
    FILE *f = fopen("config/tool_flags.txt", "r");
    if (!f) return false;
    char line[256];
    bool ignore = false;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_str(trimmed);
        char *value = trim_str(eq + 1);
        if (strcmp(key, "ignore_web_search") == 0) {
            if (strcasecmp(value, "1") == 0 ||
                strcasecmp(value, "true") == 0 ||
                strcasecmp(value, "yes") == 0 ||
                strcasecmp(value, "on") == 0) {
                ignore = true;
            }
            break;
        }
    }
    fclose(f);
    return ignore;
}

static bool raw_tool_followup_enabled(void) {
    FILE *f = fopen("config/tool_flags.txt", "r");
    if (!f) return false;
    char line[256];
    bool enabled = false;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_str(trimmed);
        char *value = trim_str(eq + 1);
        if (strcmp(key, "raw_tool_followup") == 0) {
            if (strcasecmp(value, "1") == 0 ||
                strcasecmp(value, "true") == 0 ||
                strcasecmp(value, "yes") == 0 ||
                strcasecmp(value, "on") == 0) {
                enabled = true;
            }
            break;
        }
    }
    fclose(f);
    return enabled;
}

static char* json_unescape_segment(const char* s, size_t len) {
    char* out = malloc(len + 1);
    if (!out) return NULL;
    size_t i = 0, j = 0;
    while (i < len) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            switch (s[i]) {
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case '"': out[j++] = '"'; break;
                case '\\': out[j++] = '\\'; break;
                default: out[j++] = s[i]; break;
            }
        } else {
            out[j++] = s[i];
        }
        i++;
    }
    out[j] = '\0';
    return out;
}

static char* extract_text_field_from_raw_json(const char* path) {
    char *raw = read_full_file(path);
    if (!raw) return NULL;
    char *marker = strstr(raw, "\"text\":");
    if (!marker) { free(raw); return NULL; }
    char *q = strchr(marker + 7, '"');
    if (!q) { free(raw); return NULL; }
    q++;
    char *start = q;
    while (*q) {
        if (*q == '"' && (q == start || q[-1] != '\\' || (q - start >= 2 && q[-1] == '\\' && q[-2] == '\\'))) break;
        q++;
    }
    if (*q != '"') { free(raw); return NULL; }
    char *out = json_unescape_segment(start, (size_t)(q - start));
    free(raw);
    return out;
}

static char* extract_response_field(const char* text) {
    if (!text) return NULL;
    const char *json = strstr(text, "```json");
    json = json ? json + 7 : text;
    while (*json && isspace((unsigned char)*json)) json++;

    const char *marker = strstr(json, "\"response\"");
    if (!marker) return NULL;
    const char *colon = strchr(marker, ':');
    if (!colon) return NULL;
    const char *q = strchr(colon, '"');
    if (!q) return NULL;
    q++;
    const char *start = q;
    while (*q) {
        if (*q == '"' && (q == start || q[-1] != '\\' || (q - start >= 2 && q[-1] == '\\' && q[-2] == '\\'))) break;
        q++;
    }
    if (*q != '"') return NULL;
    return json_unescape_segment(start, (size_t)(q - start));
}

static char* run_tool(const char* tool_name, char* const args[], bool sandbox) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
        char tool_path[MAX_PATH];
        if (strchr(tool_name, '/') || tool_name[0] == '.') {
            snprintf(tool_path, sizeof(tool_path), "%s", tool_name);
        } else {
            snprintf(tool_path, sizeof(tool_path), "%s/projects/gem-xo/ops/+x/%s", project_root, tool_name);
        }
        if (sandbox) {
            if (chdir(g_sandbox_root) != 0) _exit(1);
            execvp(tool_path, args);
            execvp(tool_name, args); // Fallback to system path
        } else {
            execvp(tool_path, args);
            execvp(tool_name, args); // Fallback to system path
        }
        _exit(127);
    }
    if (pid > 0) log_pid(pid, "gem-xo-tool");
    close(pipefd[1]);
    char* output = malloc(ctx_limit);
    size_t total = 0;
    while (1) {
        char buf[1024];
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (total + n < ctx_limit) { memcpy(output + total, buf, n); total += n; }
    }
    output[total] = '\0';
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    size_t len = strlen(output); while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) output[--len] = '\0';
    return output;
}

static void load_apis(void) {
    FILE *f = fopen("projects/gem-xo/config/apis.txt", "r");
    if (!f) { snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: Could not open config/apis.txt"); return; }
    g_api_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && g_api_count < 16) {
        char *sep = strchr(line, '|');
        if (sep) {
            *sep = '\0';
            strncpy(g_api_list[g_api_count].name, trim_str(line), sizeof(g_api_list[g_api_count].name) - 1);
            g_api_list[g_api_count].name[sizeof(g_api_list[g_api_count].name) - 1] = '\0';
            strncpy(g_api_list[g_api_count].url, trim_str(sep + 1), sizeof(g_api_list[g_api_count].url) - 1);
            g_api_list[g_api_count].url[sizeof(g_api_list[g_api_count].url) - 1] = '\0';
            g_api_count++;
        }
    }
    fclose(f);
}

static void update_menu_markup(void) {
    char buf[4096] = "";
    for (int i = 0; i < g_api_count; i++) {
    char line[6144];
        snprintf(line, sizeof(line), "        <button label=\"%s\" onClick=\"SET_API:%s\" /><br/>", g_api_list[i].name, g_api_list[i].url);
        strcat(buf, line);
    }
    snprintf(g_menu_area, sizeof(g_menu_area), "%s", buf);
}

static char* read_full_file(const char* path) {
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

void format_response(const char* src) {
    char formatted[8192] = "";
    char line_buf[512];
    const char* p = src;
    while (*p && strlen(formatted) < 7000) {
        int len = 0;
        while (p[len] && p[len] != '\n' && len < 74) len++;
        char wrap[75];
        memcpy(wrap, p, len); wrap[len] = '\0';
        snprintf(line_buf, sizeof(line_buf), "║ %-74.74s ║\\n", wrap);
        strcat(formatted, line_buf);
        p += len;
        if (*p == '\n') p++;
    }
    if (strlen(formatted) > 2) formatted[strlen(formatted)-2] = '\0';
    snprintf(g_resp_area, sizeof(g_resp_area), "%s", formatted);
}

void execute_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/gem-xo/state/context.json";
    char *tmp_args = "projects/gem-xo/state/args.tmp";

    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "%s", g_pending_tool_name);
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Executing %s...", g_pending_tool_name);
    
    FILE *fargs = fopen(tmp_args, "w");
    if (fargs) { fputs(g_pending_args_json ? g_pending_args_json : "{}", fargs); fclose(fargs); }
    
    char *result = NULL;
    if (strcmp(g_pending_tool_name, "read_file") == 0) {
        char *p_p[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false);
        if (path && strlen(path) > 0) { char *ta[] = {"file_ops", "read", path, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "write_file") == 0) {
        char *p_p[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_c[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "content", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *cont = run_tool(p_c[0], p_c, false);
        if (path && strlen(path) > 0 && cont) { char *ta[] = {"file_ops", "write", path, cont, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path);
        if (cont) free(cont);
    } else if (strcmp(g_pending_tool_name, "list_dir") == 0) {
        char *p_p[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false); 
        char *ta[] = {"list_dir", (path && strlen(path) > 0) ? path : ".", NULL}; 
        result = run_tool("list_dir", ta, true); 
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "exec_cmd") == 0) {
        char *p_c[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "cmd", NULL};
        char *cmd = run_tool(p_c[0], p_c, false); if (cmd && strlen(cmd) > 0) { char *ta[] = {"cmd_exec", cmd, NULL}; result = run_tool("cmd_exec", ta, true); }
        if (cmd) free(cmd);
    } else if (strcmp(g_pending_tool_name, "search_in_files") == 0) {
        char *p_q[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "query", NULL};
        char *query = run_tool(p_q[0], p_q, false); if (query && strlen(query) > 0) { char *ta[] = {"search_in_files", query, NULL}; result = run_tool("search_in_files", ta, true); }
        if (query) free(query);
    } else if (strcmp(g_pending_tool_name, "web_search") == 0) {
        if (web_search_ignored()) {
            result = strdup("Web search is disabled by config (config/tool_flags.txt: ignore_web_search=1).");
        } else {
            char *p_q[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "query", NULL};
            char *query = run_tool(p_q[0], p_q, false); if (query && strlen(query) > 0) { char *ta[] = {"web_search", query, NULL}; result = run_tool("web_search", ta, true); }
            if (query) free(query);
        }
    } else if (strcmp(g_pending_tool_name, "edit_file") == 0) {
        char *p_p[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_s[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "search", NULL};
        char *p_r[] = {"projects/gem-xo/ops/+x/json_parser", tmp_args, "replace", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *search = run_tool(p_s[0], p_s, false); char *replace = run_tool(p_r[0], p_r, false);
        if (path && strlen(path) > 0 && search && replace) { char *ta[] = {"edit_file", path, search, replace, NULL}; result = run_tool("edit_file", ta, true); }
        if (path) free(path);
        if (search) free(search);
        if (replace) free(replace);
    }

    if (result) {
        g_have_tool_result_for_followup = true;
        snprintf(g_last_tool_name, sizeof(g_last_tool_name), "%s", g_pending_tool_name);
        snprintf(g_last_tool_result, sizeof(g_last_tool_result), "%s", result);
        char *a_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "tool", result, NULL};
        run_tool(t_args[0], t_args, false);
        if (raw_tool_followup_enabled()) {
            char *fallback = NULL;
            if (asprintf(&fallback,
                         "Tool %s completed.\nShowing raw tool output:\n\n%s",
                         g_pending_tool_name,
                         result) != -1) {
                format_response(fallback);
                free(fallback);
            } else {
                format_response(result);
            }
            FILE *fresp = fopen("projects/gem-xo/state/last_response.txt", "w");
            if (fresp) { fputs(g_resp_area, fresp); fclose(fresp); }
            snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
            snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
            snprintf(g_sys_msg, sizeof(g_sys_msg), "Tool output ready.");
            char *r_args_app[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", g_resp_area, NULL};
            run_tool(r_args_app[0], r_args_app, false);
            free(result);
            if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
            if (g_pending_function_call) { free(g_pending_function_call); g_pending_function_call = NULL; }
            g_pending_tool_name[0] = '\0';
            g_have_tool_result_for_followup = false;
            g_last_tool_name[0] = '\0';
            g_last_tool_result[0] = '\0';
            if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
            return;
        }
        free(result);
    } else {
        g_have_tool_result_for_followup = false;
        g_last_tool_name[0] = '\0';
        g_last_tool_result[0] = '\0';
        char *a_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "tool", "Error: Tool execution failed or unknown tool.", NULL};
        run_tool(t_args[0], t_args, false);
    }

    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_function_call) { free(g_pending_function_call); g_pending_function_call = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

void deny_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/gem-xo/state/context.json";
    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Tool execution denied by user.");
    
    char *a_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
    run_tool(a_args[0], a_args, false);
    char *t_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "tool", "Error: Permission denied by user.", NULL};
    run_tool(t_args[0], t_args, false);
    
    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_function_call) { free(g_pending_function_call); g_pending_function_call = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

void update_completions(void) {
    char *star = strrchr(g_last_buf_input, '*');
    if (!star) { 
        g_completion_mode = false; 
        g_comp_root_vis = false;
        for (int i=0; i<5; i++) g_comp_vis[i] = false;
        return; 
    }
    
    char query[1024];
    char *prefix = star + 1;

    if (strlen(prefix) == 0) {
        snprintf(query, sizeof(query), "%s/", g_sandbox_root);
    } else {
        snprintf(query, sizeof(query), "%s/%s", g_sandbox_root, prefix);
    }

    char *p_args[] = {"projects/gem-xo/ops/+x/complete_path", query, NULL};
    char *matches = run_tool(p_args[0], p_args, false);
    
    for (int i=0; i<5; i++) { g_comp_labels[i][0] = '\0'; g_comp_vis[i] = false; }
    
        if (matches && strlen(matches) > 0) {
        g_comp_root_vis = true;
        g_completion_return_gui_index = read_active_gui_index();
        char *copy = strdup(matches);
        char *token = strtok(copy, "  ");
        int count = 0;
        while (token && count < 5) {
            const char *label = token;
            size_t root_len = strlen(g_sandbox_root);
            if (strncmp(label, g_sandbox_root, root_len) == 0) {
                label += root_len;
                if (*label == '/') label++;
            }
            strncpy(g_comp_labels[count], label, sizeof(g_comp_labels[count]) - 1);
            g_comp_labels[count][sizeof(g_comp_labels[count]) - 1] = '\0';
            g_comp_vis[count] = true;
            token = strtok(NULL, "  ");
            count++;
        }
        free(copy);
        free(matches);
        
        // UX: Force Parser focus to the Suggestions header and expand it.
        char *agi_path = "pieces/display/active_gui_index.txt";
        FILE *agi_f = fopen(agi_path, "w");
        if (agi_f) { fprintf(agi_f, "1\n"); fclose(agi_f); }
        
        // Ensure UI is refreshed with the new active state
        write_gui_state();
        trigger_render();
    } else {
        g_comp_root_vis = false;
    }
}

void handle_choose_path(int index) {
    char *buf_path = "pieces/apps/player_app/cli_buffers.txt";
    FILE *bf = fopen(buf_path, "r");
    char last_line[1024] = "";
    const char *source = g_input_text_state;
    if (!source || strlen(source) == 0) source = g_last_buf_input;
    if (source && strlen(source) > 0) {
        snprintf(last_line, sizeof(last_line), "%s", source);
    }
    if (bf) {
        char line[1024];
        while (fgets(line, sizeof(line), bf)) {
            if (line[0] == 'i') {
                strncpy(last_line, line + 1, sizeof(last_line) - 1);
                last_line[sizeof(last_line) - 1] = '\0';
                size_t l = strlen(last_line); if (l > 0 && last_line[l-1] == '\n') last_line[l-1] = '\0';
            }
        }
        fclose(bf);
    }

    if (strlen(last_line) > 0) {
        char *star = strrchr(last_line, '*');
        if (star) {
            if (index - 2 >= 0 && index - 2 < 5 && g_comp_vis[index - 2]) {
                char *choice = g_comp_labels[index - 2];
                *star = '\0';
                char new_input[2048];
                snprintf(new_input, sizeof(new_input), "i%s%s", last_line, choice);
                // Update internal memory
                snprintf(g_last_buf_input, sizeof(g_last_buf_input), "%s%s", last_line, choice);
                set_input_text_override(g_last_buf_input);

                g_buf_last_pos = 0;
                FILE *bfw = fopen(buf_path, "w");
                if (bfw) { 
                    fprintf(bfw, "%s\n", new_input); 
                    fclose(bfw); 
                    FILE *dbg = fopen("manager_debug.log", "a");
                    if(dbg) { fprintf(dbg, "DEBUG: handle_choose_path injected: %s\n", new_input); fclose(dbg); }
                }
            }
        }
    }
    
    // ATOMIC EXIT: Clear mode and methods, then sync UI immediately
    g_completion_mode = false;
    g_comp_root_vis = false;
    for (int i=0; i<5; i++) { g_comp_labels[i][0] = '\0'; g_comp_vis[i] = false; }

    /* Return focus to the cli_io field so the selected path is shown where the
     * user is actually typing, not inside the completion list. */
    int restore_idx = (g_completion_return_gui_index > 0) ? g_completion_return_gui_index : 2;
    FILE *agi_f = fopen("pieces/display/active_gui_index.txt", "w");
    if (agi_f) { fprintf(agi_f, "%d\n", restore_idx); fclose(agi_f); }
    g_completion_return_gui_index = -1;
    
    write_gui_state();
    pulse_state_changed();
    trigger_render();
    strcpy(last_frame_methods, "");
}

void audit_log(const char* user, const char* assistant) {
    char *path = "projects/gem-xo/pieces/world_01/map_01/iqabel/memories/history.txt";
    mkdir("projects/gem-xo/pieces/world_01/map_01/iqabel/memories", 0755);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        char *ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s]\nUSER: %s\nAGENT: %s\n\n", ts, user, assistant);
        fclose(f);
    }
}

static void resolve_local_model(void) {
    char* tags_file = "projects/gem-xo/state/ollama_tags.json";
    char* api_path = NULL; 
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;

    if (is_llamacpp) {
        if (asprintf(&api_path, "%s/v1/models", g_api_url) == -1) return;
    } else {
        if (asprintf(&api_path, "%s/api/tags", g_api_url) == -1) return;
    }

    char* curl_args[] = {"curl", "-s", "--max-time", "5", api_path, "-o", tags_file, NULL};
    run_tool("curl", curl_args, false);
    free(api_path);

    char* key = is_llamacpp ? "id" : "name";
    char* find_args[] = {"projects/gem-xo/ops/+x/json_parser", tags_file, key, NULL};
    char* models = run_tool(find_args[0], find_args, false);
    if (models) {
        char* copy = strdup(models);
        char* token = strtok(copy, "  ");
        int found = 0;
        while (token) {
            if (strstr(token, "groq-tool-use")) { strncpy(g_current_model, token, sizeof(g_current_model) - 1); g_current_model[sizeof(g_current_model) - 1] = '\0'; found = 1; break; }
            token = strtok(NULL, "  ");
        }
        if (!found) {
            char* second_copy = strdup(models);
            token = strtok(second_copy, "  ");
            while (token) {
                if (strstr(token, "groq")) { strncpy(g_current_model, token, sizeof(g_current_model) - 1); g_current_model[sizeof(g_current_model) - 1] = '\0'; found = 1; break; }
                token = strtok(NULL, "  ");
            }
            free(second_copy);
        }
        if (!found) {
            char* third_copy = strdup(models);
            char* first = strtok(third_copy, "  ");
            if (first) {
                strncpy(g_current_model, first, sizeof(g_current_model) - 1);
                g_current_model[sizeof(g_current_model) - 1] = '\0';
            }
            free(third_copy);
        }
        free(copy);
        free(models);
    }
}

void start_ai_query(const char* input) {
    char *ctx_file = "projects/gem-xo/state/context.json";
    char *tmp_prompt = "projects/gem-xo/state/prompt.json";
    char *tmp_llm = "projects/gem-xo/state/llm_response.json";
    char *tmp_content = "projects/gem-xo/state/llm_content.json";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "generativelanguage") != NULL;

    if (g_ai_pid > 0) return;
    unlink(tmp_llm); unlink(tmp_content);
    struct stat st;
    if (stat(ctx_file, &st) != 0 || st.st_size < 10) {
        char *persona = read_full_file("config/persona.txt");
        if (persona && strlen(persona) > 5) {
            char *s_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "system", persona, NULL};
            run_tool(s_args[0], s_args, false);
            free(persona);
        } else {
            if (persona) free(persona);
            char *default_persona = "You are Aida, a technical agent. Respond in JSON with 'response' or 'tool' keys.";
            char *s_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "system", default_persona, NULL};
            run_tool(s_args[0], s_args, false);
        }
    }
    if (input) {
        g_have_tool_result_for_followup = false;
        g_last_tool_name[0] = '\0';
        g_last_tool_result[0] = '\0';
        if (g_pending_input) free(g_pending_input);
        g_pending_input = strdup(input);
        char *u_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "user", (char*)input, NULL};
        run_tool(u_args[0], u_args, false);
    }
    snprintf(g_ai_state, sizeof(g_ai_state), "THINKING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "THINKING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Querying AI...");
    g_thinking_start = time(NULL);
    g_thinking_secs = 0;
    char *r_args[] = {"projects/gem-xo/ops/+x/json_state", "read", ctx_file, NULL};
    char *context = run_tool(r_args[0], r_args, false);
    
    char *p_args[] = {"projects/gem-xo/ops/+x/gemini_payload_builder", ctx_file, tmp_prompt, NULL};
    run_tool(p_args[0], p_args, false);

    if (context) free(context);
    char *body_arg = NULL; char *api_path = NULL;
    
    if (is_llamacpp) {
        if (asprintf(&api_path, "%s/v1/chat/completions", g_api_url) == -1) api_path = NULL;
    } else if (is_gemini) {
        if (asprintf(&api_path, "%s/v1beta/models/gemini-2.5-flash:generateContent", g_api_url) == -1) api_path = NULL;
    } else {
        if (asprintf(&api_path, "%s/api/chat", g_api_url) == -1) api_path = NULL;
    }

    if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
        g_ai_pid = fork();
        if (g_ai_pid == 0) {
            setpgid(0, 0);
            char *curl_args[20];
            int arg_count = 0;
            curl_args[arg_count++] = "curl";
            curl_args[arg_count++] = "-sS";
            curl_args[arg_count++] = "--max-time";
            curl_args[arg_count++] = "600";
            curl_args[arg_count++] = "-H";
            curl_args[arg_count++] = "Content-Type: application/json";
            
            char *key = NULL;
            if (is_gemini) {
                key = get_gemini_api_key();
                if (key) {
                    char *header = NULL;
                    if (asprintf(&header, "x-goog-api-key: %s", key) != -1) {
                        curl_args[arg_count++] = "-H";
                        curl_args[arg_count++] = header;
                    }
                    free(key);
                }
            }
            
            curl_args[arg_count++] = api_path;
            curl_args[arg_count++] = "-d";
            curl_args[arg_count++] = body_arg;
            curl_args[arg_count++] = "-o";
            curl_args[arg_count++] = tmp_llm;
            curl_args[arg_count++] = NULL;

            int fd = open("projects/gem-xo/state/curl_debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
            execvp("curl", curl_args);
            _exit(127);
        }
        if (g_ai_pid > 0) log_pid(g_ai_pid, "gem-xo-curl");
    }
    free(body_arg); free(api_path);
}

void check_ai_status(void) {
    if (g_ai_pid <= 0) return;
    int status;
    pid_t res = waitpid(g_ai_pid, &status, WNOHANG);
    if (res == 0) return;
    g_ai_pid = -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        snprintf(g_sys_msg, sizeof(g_sys_msg), "API Error: Curl failed (code %d)", WEXITSTATUS(status));
        snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
        if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
        return;
    }
    char *ctx_file = "projects/gem-xo/state/context.json";
    char *tmp_llm = "projects/gem-xo/state/llm_response.json";
    char *tmp_last_txt = "projects/gem-xo/state/last_response.txt";
    char *text_content = NULL;
    char *function_call = NULL;

    // Gemini Response Structure: candidates[0].content.parts[0].text OR candidates[0].content.parts[0].functionCall
    char *p_text[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "candidates[0].content.parts[0].text", NULL};
    text_content = run_tool(p_text[0], p_text, false);
    
    // Fallback for different Gemini versions or if already partially parsed
    if (!text_content || strlen(text_content) == 0) {
        if (text_content) free(text_content);
        char *p_text_fallback[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "text", NULL};
        text_content = run_tool(p_text_fallback[0], p_text_fallback, false);
    }
    if (!text_content || strlen(text_content) == 0) {
        if (text_content) free(text_content);
        text_content = extract_text_field_from_raw_json(tmp_llm);
    }

    char *p_func[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "candidates[0].content.parts[0].functionCall", NULL};
    function_call = run_tool(p_func[0], p_func, false);
    
    if (!function_call || strlen(function_call) == 0) {
        if (function_call) free(function_call);
        char *p_func_fallback[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "functionCall", NULL};
        function_call = run_tool(p_func_fallback[0], p_func_fallback, false);
    }

    // Check for API-level errors if no content or function call
    if ((!text_content || strlen(text_content) == 0) && (!function_call || strlen(function_call) == 0)) {
        char *p_err[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "error.message", NULL};
        char *error_msg = run_tool(p_err[0], p_err, false);
        if (error_msg && strlen(error_msg) > 0) {
            if (text_content) free(text_content);
            if (g_have_tool_result_for_followup && strlen(g_last_tool_result) > 0) {
                char *fallback = NULL;
                if (asprintf(&fallback,
                             "Tool %s completed, but the Gemini follow-up hit quota.\nShowing raw tool output instead:\n\n%s",
                             g_last_tool_name[0] ? g_last_tool_name : "call",
                             g_last_tool_result) != -1) {
                    text_content = fallback;
                } else {
                    text_content = strdup(g_last_tool_result);
                }
                free(error_msg);
            } else if (is_quota_error_text(error_msg) && g_pending_input) {
                char *query = extract_direct_search_query(g_pending_input);
                if (query) {
                    if (web_search_ignored()) {
                        text_content = strdup("Web search is disabled by config (config/tool_flags.txt: ignore_web_search=1).");
                        free(query);
                        free(error_msg);
                    } else {
                        char *ta[] = {"web_search", query, NULL};
                        char *tool_result = run_tool("web_search", ta, true);
                        if (tool_result && strlen(tool_result) > 0) {
                            char *fallback = NULL;
                            if (asprintf(&fallback,
                                         "Gemini quota blocked the initial planner call.\nShowing direct DuckDuckGo search output instead:\n\n%s",
                                         tool_result) != -1) {
                                text_content = fallback;
                            } else {
                                text_content = strdup(tool_result);
                            }
                            char *u_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", "{\"name\":\"web_search\",\"args\":{\"query\":\"fallback\"}}", NULL};
                            run_tool(u_args[0], u_args, false);
                            char *t_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "tool", tool_result, NULL};
                            run_tool(t_args[0], t_args, false);
                            free(tool_result);
                            free(query);
                            free(error_msg);
                        } else {
                            if (tool_result) free(tool_result);
                            free(query);
                            text_content = error_msg;
                        }
                    }
                } else {
                    text_content = error_msg;
                }
            } else {
                text_content = error_msg;
            }
        }
    }

    char *tool_name = NULL; char *args_json = NULL;
    if (function_call && strlen(function_call) > 2) {
        FILE *ffunc = fopen("projects/gem-xo/state/function_call.tmp", "w");
        if (ffunc) { fputs(function_call, ffunc); fclose(ffunc); }
        char *p_tn[] = {"projects/gem-xo/ops/+x/json_parser", "projects/gem-xo/state/function_call.tmp", "name", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/gem-xo/ops/+x/json_parser", "projects/gem-xo/state/function_call.tmp", "args", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
        unlink("projects/gem-xo/state/function_call.tmp");
    }

    if (tool_name && strlen(tool_name) > 0) {
        if (strcmp(tool_name, "web_search") == 0 && web_search_ignored()) {
            if (text_content) free(text_content);
            text_content = strdup("Web search is disabled by config (config/tool_flags.txt: ignore_web_search=1).");
            free(tool_name);
            if (args_json) free(args_json);
            if (function_call) { free(function_call); function_call = NULL; }
        } else {
        // Defer execution and request permission
        strncpy(g_pending_tool_name, tool_name, sizeof(g_pending_tool_name) - 1);
        g_pending_tool_name[sizeof(g_pending_tool_name) - 1] = '\0';
        g_pending_args_json = args_json;         // Transfer ownership
        g_pending_function_call = function_call; // Transfer ownership
        
        snprintf(g_ai_state, sizeof(g_ai_state), "PENDING_PERM");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "WAIT_PERM");
        snprintf(g_sys_msg, sizeof(g_sys_msg), "Execute %s? (y/n)", tool_name);
        
        char format_buf[4096];
        snprintf(format_buf, sizeof(format_buf), "Tool Execution Request\nTool: %s\nArgs: %s\n\nType 'y' to allow or 'n' to deny.", tool_name, args_json ? args_json : "{}");
        format_response(format_buf);
        
        free(tool_name);
        free(text_content);
        return; 
        }
    }

    if (!text_content || strlen(text_content) == 0) {
        if (text_content) free(text_content);
        text_content = strdup("(empty response)");
    } else {
        char *normalized = extract_response_field(text_content);
        if (normalized && strlen(normalized) > 0) {
            free(text_content);
            text_content = normalized;
        } else if (normalized) {
            free(normalized);
        }
    }

    // Write to last_response.txt for audit/read
    FILE *fresp = fopen(tmp_last_txt, "w");
    if (fresp) { fputs(text_content, fresp); fclose(fresp); }

    audit_log(g_pending_input, text_content);
    format_response(text_content);

    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); 
    snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Response received.");

    char *r_args_app[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "assistant", text_content, NULL};
    run_tool(r_args_app[0], r_args_app, false);

    g_have_tool_result_for_followup = false;
    g_last_tool_name[0] = '\0';
    g_last_tool_result[0] = '\0';
    free(text_content); free(function_call);
    if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
}

void resolve_paths(const char* hint) {
    char kvp_path[MAX_PATH]; snprintf(kvp_path, sizeof(kvp_path), "%s/pieces/locations/location_kvp", hint ? hint : ".");
    FILE *kvp = fopen(kvp_path, "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; char *k = trim_str(line); char *v = trim_str(eq + 1); if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v); }
        }
        fclose(kvp);
    } else if (hint) snprintf(project_root, sizeof(project_root), "%s", hint);
}

void trigger_render(void) { 
    char *path = "pieces/display/frame_changed.txt"; 
    FILE *f = fopen(path, "a"); 
    if (f) { 
        fprintf(f, "M\n"); 
        fclose(f); 
    } 
}

static void set_input_text_override(const char *value) {
    if (value) {
        snprintf(g_input_text_state, sizeof(g_input_text_state), "%s", value);
    } else {
        g_input_text_state[0] = '\0';
    }
}

char* get_gui_var(const char* id) {
    char *path = "projects/gem-xo/manager/gui_state.txt"; char *line = NULL; size_t len = 0; FILE *f = fopen(path, "r");
    if (!f) return NULL;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, id, strlen(id)) == 0 && line[strlen(id)] == '=') {
            char *val = strdup(line + strlen(id) + 1);
            if (!val) {
                fclose(f);
                free(line);
                return NULL;
            }
            val[strcspn(val, "\r\n")] = '\0';
            if (strcmp(id, "input_text") != 0) {
                char *trimmed = trim_str(val);
                if (trimmed != val) memmove(val, trimmed, strlen(trimmed) + 1);
            }
            fclose(f); free(line); return val;
        }
    }
    fclose(f); free(line); return NULL;
}

void write_gui_state(void) {
    char *path = "projects/gem-xo/manager/gui_state.txt";

    if (strcmp(g_ai_state, "THINKING") == 0) {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s (%ds) | [API]: %s", g_ai_state, g_thinking_secs, g_api_url);
    } else {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s%s | [API]: %s", g_ai_state, g_completion_mode ? " (COMPLETING)" : "", g_api_url);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/gem-xo/manager/+x/gem-xo_manager.+x\n");
        fprintf(f, "active_layout_id=gem-xo.chtpm\n");
        fprintf(f, "ai_state=%s\n", g_ai_state);
        fprintf(f, "active_api=%s\n", g_api_url);
        fprintf(f, "ai_status_line=%s\n", g_ai_status_line);
        fprintf(f, "ctx_pct=%s\n", g_ctx_pct);
        fprintf(f, "iqabel_fsm=%s\n", g_fsm_state);
        fprintf(f, "gem-xo_response_area=%s\n", g_resp_area);
        
        if (g_completion_mode) {
            fprintf(f, "gem-xo_api_menu=\n");
        } else {
            fprintf(f, "gem-xo_api_menu=%s\n", g_menu_area);
        }

        fprintf(f, "piece_methods=%s\n", g_piece_methods);
        fprintf(f, "comp_root_vis=%s\n", g_comp_root_vis ? "true" : "false");
        for (int i=0; i<5; i++) {
            fprintf(f, "comp_%d_lbl=%s\n", i+1, g_comp_labels[i]);
            fprintf(f, "comp_%d_vis=%s\n", i+1, g_comp_vis[i] ? "true" : "false");
        }
        fprintf(f, "sys_msg=%s\n", g_sys_msg);
        fprintf(f, "thinking_secs=%d\n", g_thinking_secs);
        fprintf(f, "input_text=%s\n", g_input_text_state);
        
        fflush(f);
        fclose(f);
    }
}

void process_input_trigger(void) {
    char *input = get_gui_var("input_text");
    FILE *dbg = fopen("projects/gem-xo/manager/perm_debug.log", "a");
    if (dbg) {
        fprintf(dbg, "process_input_trigger: state=%s raw_input=%s last_buf=%s\n",
                g_ai_state,
                input ? input : "(null)",
                g_last_buf_input);
        fclose(dbg);
    }
    if (input && strlen(input) > 0) {
        char *trimmed = trim_str(input);
        if (strcmp(g_ai_state, "PENDING_PERM") == 0) {
            dbg = fopen("projects/gem-xo/manager/perm_debug.log", "a");
            if (dbg) {
                fprintf(dbg, "pending_perm trimmed=%s\n", trimmed ? trimmed : "(null)");
                fclose(dbg);
            }
            g_last_buf_input[0] = '\0';
            set_input_text_override("");
            write_gui_state();
            trigger_render();
            
            if (strcasecmp(trimmed, "y") == 0 || strcasecmp(trimmed, "yes") == 0) {
                dbg = fopen("projects/gem-xo/manager/perm_debug.log", "a");
                if (dbg) { fprintf(dbg, "permission decision=allow\n"); fclose(dbg); }
                execute_pending_tool();
            } else if (strcasecmp(trimmed, "n") == 0 || strcasecmp(trimmed, "no") == 0) {
                dbg = fopen("projects/gem-xo/manager/perm_debug.log", "a");
                if (dbg) { fprintf(dbg, "permission decision=deny\n"); fclose(dbg); }
                deny_pending_tool();
            } else {
                dbg = fopen("projects/gem-xo/manager/perm_debug.log", "a");
                if (dbg) { fprintf(dbg, "permission decision=invalid\n"); fclose(dbg); }
                snprintf(g_sys_msg, sizeof(g_sys_msg), "Invalid response. Type 'y' or 'n' for %s.", g_pending_tool_name);
            }
        } else {
            // Atomic UI update to clear input
            g_last_buf_input[0] = '\0';
            set_input_text_override("");
            g_completion_mode = false;
            g_comp_root_vis = false;
            g_piece_methods[0] = '\0';
            for (int i=0; i<5; i++) { g_comp_labels[i][0] = '\0'; g_comp_vis[i] = false; }
            
            write_gui_state();
            pulse_state_changed();
            trigger_render();
            
            start_ai_query(input);
        }
    }
    if (input) free(input);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sig); signal(SIGTERM, handle_sig); setpgid(0, 0); log_pid(getpid(), "gem-xo-manager");
    resolve_paths(argc > 1 ? argv[1] : NULL);
    if (chdir(project_root) != 0) perror("chdir project_root failed");
    sandbox_scope_load_root("projects/gem-xo/config/context.txt", g_sandbox_root, sizeof(g_sandbox_root));
    ensure_dir_path("projects/gem-xo/state");
    ensure_dir_path(g_sandbox_root);
    char sandbox_config_dir[MAX_PATH];
    snprintf(sandbox_config_dir, sizeof(sandbox_config_dir), "%s/config", g_sandbox_root);
    ensure_dir_path(sandbox_config_dir);
    char sandbox_ctx_path[MAX_PATH];
    snprintf(sandbox_ctx_path, sizeof(sandbox_ctx_path), "%s/context.txt", sandbox_config_dir);
    FILE *sandbox_ctx = fopen(sandbox_ctx_path, "w");
    if (sandbox_ctx) {
        fputs("project_id=gem-xo\nsandbox_root=.\n", sandbox_ctx);
        fclose(sandbox_ctx);
    }
    char sandbox_yolo_path[MAX_PATH];
    snprintf(sandbox_yolo_path, sizeof(sandbox_yolo_path), "%s/yolo.flag", sandbox_config_dir);
    FILE *sandbox_yolo = fopen(sandbox_yolo_path, "w");
    if (sandbox_yolo) {
        fputs("1\n", sandbox_yolo);
        fclose(sandbox_yolo);
    }
    load_apis(); update_menu_markup();
    resolve_local_model();
    struct stat st;
    char *hist_path = "pieces/apps/player_app/history.txt"; 
    char *buf_path = "pieces/apps/player_app/cli_buffers.txt"; 
    
    // GLOBAL STARTUP CLEAR: hard-reset transient session/runtime files.
    truncate_file(hist_path);
    truncate_file(buf_path);
    truncate_file("projects/gem-xo/manager/gui_state.txt");
    truncate_file("projects/gem-xo/state/context.json");
    truncate_file("projects/gem-xo/state/prompt.json");
    truncate_file("projects/gem-xo/state/llm_response.json");
    truncate_file("projects/gem-xo/state/last_response.txt");
    truncate_file("projects/gem-xo/state/curl_debug.log");
    truncate_file("projects/gem-xo/state/args.tmp");
    truncate_file("pieces/apps/player_app/state_changed.txt");
    unlink("projects/gem-xo/state/function_call.tmp");
    clear_render_artifacts();
    FILE *agi = fopen("pieces/display/active_gui_index.txt", "w");
    if (agi) {
        fputs("1\n", agi);
        fclose(agi);
    }

    long last_pos = 0; 
    g_buf_last_pos = 0; 
    reset_session_ui();
    set_input_text_override("");

    // Flush clean UI immediately
    write_gui_state();
    pulse_state_changed();
    trigger_render();

    while (!g_shutdown) {
        char *ctx_file = "projects/gem-xo/state/context.json";
        int pct = 0; char *r_args_pct[] = {"projects/gem-xo/ops/+x/json_state", "read", ctx_file, NULL};
        char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
        if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
        snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);
        int state_changed = 0;

        // Polling cli_buffers.txt for '*' completion trigger and updates
        char *bp = "pieces/apps/player_app/cli_buffers.txt";
        char last_line[1024];
        strncpy(last_line, g_last_buf_input, sizeof(last_line) - 1);
        last_line[sizeof(last_line) - 1] = '\0';
        
        if (stat(bp, &st) == 0) {
            if (st.st_size > g_buf_last_pos) {
                FILE *bf = fopen(bp, "r");
                if (bf) {
                    fseek(bf, g_buf_last_pos, SEEK_SET);
                    char line[1024];
                    while (fgets(line, sizeof(line), bf)) {
                        if (line[0] == 'i') {
                            strncpy(last_line, line + 1, sizeof(last_line) - 1);
                            last_line[sizeof(last_line) - 1] = '\0';
                            size_t l = strlen(last_line); if (l > 0 && last_line[l-1] == '\n') last_line[l-1] = '\0';
                        }
                    }
                    g_buf_last_pos = ftell(bf);
                    fclose(bf);
                }
            } else if (st.st_size < g_buf_last_pos) {
                g_buf_last_pos = 0; // Handle truncation
            }
        }

        // Detect '*' presence and update completions
        char *star = strrchr(last_line, '*');
        bool has_star = (star != NULL);
        bool text_changed = (strcmp(last_line, g_last_buf_input) != 0);

        // Only update if mode changes or text actually changes
        if (has_star != g_completion_mode || (has_star && text_changed)) {
            strncpy(g_last_buf_input, last_line, sizeof(g_last_buf_input) - 1);
            g_last_buf_input[sizeof(g_last_buf_input) - 1] = '\0';
            set_input_text_override(g_last_buf_input);
            if (has_star) {
                g_completion_mode = true;
                update_completions();
            } else {
                g_completion_mode = false;
                g_comp_root_vis = false;
                for (int i=0; i<5; i++) { g_comp_labels[i][0] = '\0'; g_comp_vis[i] = false; }
            }
            state_changed = 1;
        } else if (text_changed) {
            strncpy(g_last_buf_input, last_line, sizeof(g_last_buf_input) - 1);
            g_last_buf_input[sizeof(g_last_buf_input) - 1] = '\0';
            set_input_text_override(g_last_buf_input);
        }

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[1024];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd_ptr = strstr(line, "SET_API:");
                        if (cmd_ptr) {
                            char *url = trim_str(cmd_ptr + 8);
                            strncpy(g_api_url, url, sizeof(g_api_url) - 1);
                            g_api_url[sizeof(g_api_url) - 1] = '\0';
                            resolve_local_model();
                            snprintf(g_resp_area, sizeof(g_resp_area), "║ Connected to %-61.61s ║", g_api_url);
                            char *msg = NULL; if (asprintf(&msg, "Switched to %s", g_current_model) != -1) { snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", msg); free(msg); }
                            state_changed = 1;
                        } else if ((cmd_ptr = strstr(line, "CHOOSE:"))) {
                            handle_choose_path(atoi(trim_str(cmd_ptr + 7)));
                            state_changed = 1;
                        } else {
                            int key = 0;
                            char *bracket = strrchr(line, ']');
                            if (bracket) key = atoi(bracket + 1);
                            else key = atoi(line);

                            if (key == 10 || key == 13) { process_input_trigger(); state_changed = 1; }
                            else if (g_completion_mode && key >= '2' && key <= '6') {
                                handle_choose_path(key - '0');
                                state_changed = 1;
                            }
                            else if (key == '1') { unlink(ctx_file); snprintf(g_resp_area, sizeof(g_resp_area), "║ Context cleared.                                                           ║"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Context Reset."); state_changed = 1; }
                            else if (key == '2') { load_apis(); update_menu_markup(); state_changed = 1; }
                            else if (key == '3') {
                                snprintf(g_ai_state, sizeof(g_ai_state), "SUMMARIZING"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Please wait...");
                                write_gui_state(); trigger_render();
                                char *r_args[] = {"projects/gem-xo/ops/+x/json_state", "read", ctx_file, NULL}; char *context = run_tool(r_args[0], r_args, false);
                                char *tmp_prompt = "projects/gem-xo/state/prompt.json"; char *tmp_llm = "projects/gem-xo/state/llm_response.json"; char *tmp_content = "projects/gem-xo/state/llm_content.json";
                                FILE *pf = fopen(tmp_prompt, "w"); 
                                bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
                                if (pf) { 
                                    if (is_llamacpp) {
                                        fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", g_current_model, context); 
                                    } else {
                                        fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", g_current_model, context); 
                                    }
                                    fclose(pf); 
                                }
                                free(context); char *body_arg = NULL; char *api_path = NULL;
                                if (is_llamacpp) {
                                    if (asprintf(&api_path, "%s/v1/chat/completions", g_api_url) == -1) api_path = NULL;
                                } else {
                                    if (asprintf(&api_path, "%s/api/chat", g_api_url) == -1) api_path = NULL;
                                }
                                if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
                                    char *curl_args[] = {"curl", "-s", "--max-time", "600", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                                    pid_t cpid = fork(); if (cpid == 0) { setpgid(0, 0); execvp("curl", curl_args); _exit(127); } if (cpid > 0) log_pid(cpid, "gem-xo-summarize");
                                    waitpid(cpid, NULL, 0);
                                }
                                free(body_arg); free(api_path);
                                char *p_ext[] = {"projects/gem-xo/ops/+x/json_parser", tmp_llm, "content", NULL}; char *content_json = run_tool(p_ext[0], p_ext, false);
                                if (content_json) {
                                    FILE *cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                                    char *p_sum[] = {"projects/gem-xo/ops/+x/json_parser", tmp_content, "summary", NULL}; char *summary = run_tool(p_sum[0], p_sum, false);
                                    if (summary) {
                                        unlink(ctx_file); char *init_args[] = {"projects/gem-xo/ops/+x/json_state", "append", ctx_file, "system", summary, NULL};
                                        run_tool(init_args[0], init_args, false); format_response(summary);
                                        snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Summary complete."); free(summary);
                                    }
                                    free(content_json);
                                }
                            }
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        
        int old_pid = g_ai_pid;
        char old_state[64];
        strncpy(old_state, g_ai_state, sizeof(old_state) - 1);
        old_state[sizeof(old_state) - 1] = '\0';

        check_ai_status(); 
        
        if (g_ai_pid > 0) {
            int cur = (int)(time(NULL) - g_thinking_start);
            if (cur != g_thinking_secs) {
                g_thinking_secs = cur;
                state_changed = 1;
            }
        } else {
            if (g_thinking_secs != 0) {
                g_thinking_secs = 0;
                state_changed = 1;
            }
        }
        
        if (old_pid > 0 && g_ai_pid <= 0) state_changed = 1; // AI finished or moved to ACTING
        if (strcmp(old_state, g_ai_state) != 0) state_changed = 1; // State string changed

        write_gui_state(); if (state_changed) {
            trigger_render();
            state_changed = 0; // Reset
        }
        usleep(g_completion_mode ? 20000 : 100000); 
    }

    reset_session_ui();
    truncate_file("projects/gem-xo/state/llm_response.json");
    truncate_file("projects/gem-xo/state/last_response.txt");
    truncate_file("projects/gem-xo/state/curl_debug.log");
    write_gui_state();
    pulse_state_changed();
    trigger_render();
    return 0;
}
