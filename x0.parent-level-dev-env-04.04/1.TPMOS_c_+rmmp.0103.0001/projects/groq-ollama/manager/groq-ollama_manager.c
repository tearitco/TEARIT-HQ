#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <signal.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#define access(p, m) _access(p, m)
/* windows.h defines MAX_PATH as 260; this file's own MAX_PATH is 4096 and is
   used pervasively for path buffers below, so the macro has to yield to the
   file's definition. Same collision agy-text-editor_manager.c and
   cpp-llm_manager.c both had to resolve for the same reason. */
#undef MAX_PATH
#else
#include <sys/file.h>
#endif

#ifdef _WIN32
/* MinGW has no POSIX getline(). Same signature and semantics -- read one line,
   growing *lineptr as needed, return the length or -1 at EOF -- so the call
   sites below are unchanged. The growth is load-bearing, not incidental:
   gui_state.txt values here include the AI response area, so a fixed fgets
   buffer would silently truncate the model's reply. */
static long win_getline(char **lineptr, size_t *n, FILE *stream) {
    size_t used = 0;
    int c;
    if (!lineptr || !n || !stream) return -1;
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) { *n = 0; return -1; }
    }
    while ((c = fgetc(stream)) != EOF) {
        if (used + 2 > *n) {
            size_t newn = *n * 2;
            char *tmp = (char *)realloc(*lineptr, newn);
            if (!tmp) return -1;
            *lineptr = tmp; *n = newn;
        }
        (*lineptr)[used++] = (char)c;
        if (c == '\n') break;
    }
    if (used == 0) return -1;
    (*lineptr)[used] = '\0';
    return (long)used;
}
#define getline win_getline
/* MinGW declares mkdir() with one argument; the POSIX form's mode is ignored
   on Windows, where _mkdir() creates the directory with default attributes. */
#define mkdir(path, mode) _mkdir(path)
/* MinGW has no usleep(); Sleep() takes milliseconds. */
#define usleep(usec) Sleep((usec) / 1000)
#endif

#define PROJECT_ID "groq-ollama"
#define MAX_PATH 4096
#define MAX_LINE 1024

// PID tracking for CPU Safety
/* Takes a plain long rather than pid_t so the same function accepts a POSIX
   pid and a Windows process id. On Windows the process id is only ever used
   for this log line: there is no pid_t there is nothing portable to wait() on
   for a child that was reparented away, so every wait site below goes through
   the HANDLE instead. */
static void log_pid(long pid, const char* name) {
    FILE *f = fopen("pieces/os/proc_list.txt", "a");
    if (f) {
        int fd = fileno(f);
#ifndef _WIN32
        /* flock() is POSIX advisory locking and has no Windows equivalent that
           MinGW provides. This is why sys/file.h is not included there. The
           append below is opened with "a", so the FILE is already positioned
           for the write; the lock only serialised concurrent managers. */
        flock(fd, LOCK_EX);
#endif
        fprintf(f, "%d %s\n", pid, name);
#ifndef _WIN32
        flock(fd, LOCK_UN);
#endif
        fclose(f);
    }
}

// Persistent Global UI State
static char g_ai_state[64] = "IDLE";
static char g_api_url[256] = "http://10.0.0.144:11434";
static char g_current_model[256] = "llama3-groq-tool-use:8b";
static char g_ctx_pct[16] = "0%";
static char g_fsm_state[64] = "IDLE";
static char g_resp_area[8192] = "║ Ready for input...                                                         ║";
static char g_sys_msg[256] = "Initialized.";
static int g_thinking_secs = 0;
static time_t g_thinking_start = 0;
static char g_ai_status_line[128] = "";
static char g_sandbox_root[MAX_PATH] = "projects/groq-ollama/sandbox";

// API Menu State
typedef struct {
    char name[64];
    char url[256];
} APIEntry;
static APIEntry g_api_list[16];
static int g_api_count = 0;
static char g_menu_area[4096] = "";

static char project_root[MAX_PATH] = ".";
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static volatile sig_atomic_t g_shutdown = 0;
#ifdef _WIN32
static HANDLE g_ai_handle = NULL;
static int g_ai_pid = 0;
/* The gemini branch used to fork(), and the child inherited the parent's
   api_key/curl command for free. A thread shares one address space instead, so
   those inputs are staged into these globals before CreateThread and consumed
   by the worker. The command line is assembled by the parent rather than the
   worker on purpose: g_api_url and g_current_model are mutable UI state, and
   the manager's loop can change them while the request is in flight, so
   building the URL late would let a request go to a model the user switched
   away from after pressing enter. */
static char g_win_curl_cmd[8192] = "";
static char g_win_tmp_llm[MAX_PATH] = "";
/* Whether g_ai_handle currently holds a thread handle (gemini branch) or a
   process handle (ollama/llamacpp branch). Both are HANDLEs and both satisfy
   the WaitForSingleObject poll, but their exit codes are read with different
   calls, so the kind has to be tracked explicitly. */
static int g_ai_handle_is_thread = 0;
#else
static pid_t g_ai_pid = -1;
#endif

static char *g_pending_input = NULL;
static bool g_completion_mode = false;
static char g_last_buf_input[1024] = "";
static char g_input_text_state[1024] = "";
static long g_buf_last_pos = 0;
static char g_comp_labels[5][256];
static bool g_comp_vis[5];
static bool g_comp_root_vis = false;
static int g_completion_return_gui_index = -1;

// Pulses a TPMOS-shared "something changed" signal and reads/writes the
// shared active-widget-focus index, so the completion suggestion list can
// grab keyboard focus and hand it back afterward (matches gem-dev's
// implementation - without this, pressing 2-6 to pick a suggestion may
// just type those digits into the input field instead).
static void pulse_state_changed(void) {
    FILE *f = fopen("pieces/apps/player_app/state_changed.txt", "a");
    if (f) { fputs("S\n", f); fclose(f); }
}

static int read_active_gui_index(void) {
    FILE *f = fopen("pieces/display/active_gui_index.txt", "r");
    if (!f) return -1;
    char line[64] = "";
    int idx = -1;
    if (fgets(line, sizeof(line), f)) idx = atoi(line);
    fclose(f);
    return idx;
}

static int get_active_gui_is_typing(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_is_typing.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return 0;
    char line[64] = "";
    int typing = 0;
    if (fgets(line, sizeof(line), f)) typing = (atoi(line) != 0);
    fclose(f);
    return typing;
}

// Pending Tool State for 'y/n' Permissions (mirrors gem-dev's PENDING_PERM
// flow). check_ai_status() parks a discovered tool call here instead of
// running it immediately; execute_pending_tool()/deny_pending_tool() are
// invoked from process_input_trigger() once the user answers y/n.
static char g_pending_tool_name[128] = "";
static char *g_pending_args_json = NULL;
static char *g_pending_content_json = NULL;

static void handle_sig(int s) { (void)s; g_shutdown = 1; }
static char* trim_str(char *str);
void write_gui_state(void);
void trigger_render(void);

static int read_config_kv(const char *path, const char *key, char *out, size_t out_size) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = trim_str(trimmed);
        char *v = trim_str(eq + 1);
        if (strcmp(k, key) == 0) {
            snprintf(out, out_size, "%s", v);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static void load_sandbox_root(const char *config_path, char *out, size_t out_size) {
    snprintf(out, out_size, "projects/groq-ollama/sandbox");
    if (config_path) read_config_kv(config_path, "sandbox_root", out, out_size);
}

static bool web_search_ignored(void) {
    FILE *f = fopen("projects/groq-ollama/config/tool_flags.txt", "r");
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
    FILE *f = fopen("projects/groq-ollama/config/tool_flags.txt", "r");
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

static void set_input_text_override(const char *value) {
    if (value) {
        snprintf(g_input_text_state, sizeof(g_input_text_state), "%s", value);
    } else {
        g_input_text_state[0] = '\0';
    }
}

static void clear_completion_state(void) {
    g_completion_mode = false;
    g_comp_root_vis = false;
    for (int i = 0; i < 5; i++) {
        g_comp_labels[i][0] = '\0';
        g_comp_vis[i] = false;
    }
}

static void reset_session_ui(void) {
    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_ctx_pct, sizeof(g_ctx_pct), "0%%");
    snprintf(g_resp_area, sizeof(g_resp_area), "║ Ready for input...                                                         ║");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Initialized.");
    g_thinking_secs = 0;
    g_thinking_start = 0;
    g_buf_last_pos = 0;
    g_last_buf_input[0] = '\0';
    g_input_text_state[0] = '\0';
    clear_completion_state();
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

#ifdef _WIN32
/* Windows run_tool, replacing four POSIX mechanisms at once:
     fork/dup2/pipe + the blocking read() loop -> CreatePipe/CreateProcess +
       ReadFile, which returns 0/ERROR_BROKEN_PIPE at EOF instead of n<=0.
     chdir() before exec -> the lpCurrentDirectory argument, so the child's
       working directory is set without this process ever changing its own.
     execvp's PATH search -> CreateProcess does NOT resolve a bare name against
       PATH the way execvp does, so the executable is located explicitly first
       (see win_resolve_tool), including the .exe suffix MinGW binaries carry
       and the ".+x" name this house actually ships. */
/* Tries one base path against every suffix a binary here can be named, and
   returns the first that exists. The suffix list is the whole point: the POSIX
   side calls its ops by bare name ("groq-ollama_bridge") and lets execvp find
   them, but on Windows there is no file called plain "groq-ollama_bridge" --
   compile_all.ps1 writes "groq-ollama_bridge.+x", and MinGW-built tools also
   carry .exe. Without the ".+x" arm every op lookup returns NULL and the app
   silently does nothing while still appearing to run. */
static const char *win_try_suffixes(const char *base) {
    static char buf[MAX_PATH];
    static const char *sfx[] = { "", ".exe", ".+x", NULL };
    for (int i = 0; sfx[i]; i++) {
        snprintf(buf, sizeof(buf), "%s%s", base, sfx[i]);
        if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES) return buf;
    }
    return NULL;
}

static char* win_resolve_tool(const char* tool_name, const char *prefix) {
    static char resolved[MAX_PATH];
    const char *hit;

    if (strchr(tool_name, '/') || strchr(tool_name, '\\') || tool_name[0] == '.') {
        hit = win_try_suffixes(tool_name);
        if (hit) { snprintf(resolved, sizeof(resolved), "%s", hit); return resolved; }
        return NULL;
    }

    {
        char *base = NULL;
        if (asprintf(&base, "%s/%s", prefix, tool_name) != -1) {
            hit = win_try_suffixes(base);
            if (hit) { snprintf(resolved, sizeof(resolved), "%s", hit); free(base); return resolved; }
            free(base);
        }
    }

    /* Not a project op, so it must be a system tool -- run_tool("curl", ...)
       below. execvp searched PATH for these; CreateProcess does not, so PATH
       is walked explicitly. Without this the Windows build returns NULL for
       every curl call and the AI never gets a reply. */
    {
        const char *path_env = getenv("PATH");
        char *dup_path = NULL;
        if (path_env && asprintf(&dup_path, "%s", path_env) != -1) {
            char *saveptr = NULL;
            for (char *dir = strtok_r(dup_path, ";", &saveptr); dir; dir = strtok_r(NULL, ";", &saveptr)) {
                char *base = NULL;
                if (asprintf(&base, "%s/%s", dir, tool_name) != -1) {
                    hit = win_try_suffixes(base);
                    if (hit) { snprintf(resolved, sizeof(resolved), "%s", hit); free(base); free(dup_path); return resolved; }
                    free(base);
                }
            }
            free(dup_path);
        }
    }
    return NULL;
}

/* Quotes one argument for a Windows command line: wrap in double quotes and
   backslash-escape any embedded quote, per the CRT's parsing rules. Without
   this an argument containing a space -- the API URL, the model name, the
   output path -- arrives at the child already split. */
static void win_append_quoted(char *dst, size_t dst_sz, const char *arg) {
    size_t used = strlen(dst);
    if (used + 3 >= dst_sz) return;
    dst[used++] = ' ';
    dst[used++] = '"';
    for (const char *p = arg; *p && used + 2 < dst_sz; p++) {
        if (*p == '"') { dst[used++] = '\\'; if (used + 1 >= dst_sz) break; }
        dst[used++] = *p;
    }
    dst[used++] = '"';
    dst[used] = '\0';
}
#endif

static char* run_tool(const char* tool_name, char* const args[], bool sandbox) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    HANDLE rd, wr;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmd[8192];
    char cwd[MAX_PATH];
    const char *prefix;
    const char *exe;
    char *output;
    size_t total = 0;
    DWORD n;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return NULL;
    /* The read end must not be inheritable, or the child keeps a duplicate of
       it and the read loop below never sees EOF -- it would block forever on a
       pipe the child itself is still holding open. */
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    prefix = sandbox ? "../ops/+x" : "projects/groq-ollama/ops/+x";
    exe = win_resolve_tool(tool_name, prefix);
    if (!exe) { CloseHandle(rd); CloseHandle(wr); return NULL; }

    snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
    for (int i = 0; args[i]; i++) win_append_quoted(cmd, sizeof(cmd), args[i]);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    memset(&pi, 0, sizeof(pi));

    if (sandbox) snprintf(cwd, sizeof(cwd), "%s/%s", project_root, g_sandbox_root);
    else snprintf(cwd, sizeof(cwd), "%s", project_root);

    if (!CreateProcessA(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr);
        return NULL;
    }
    log_pid((long)GetProcessId(pi.hProcess), "groq-ollama-tool");
    CloseHandle(wr);

    output = malloc(ctx_limit);
    if (!output) { CloseHandle(rd); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return NULL; }
    for (;;) {
        char buf[1024];
        if (!ReadFile(rd, buf, (DWORD)sizeof(buf), &n, NULL) || n == 0) break;
        if (total + n < ctx_limit) { memcpy(output + total, buf, n); total += n; }
    }
    output[total] = '\0';
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    { size_t len = strlen(output); while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) output[--len] = '\0'; }
    return output;
#else
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
        char *full_path = NULL;
        if (sandbox) {
            if (chdir(g_sandbox_root) != 0) _exit(1);
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "../ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        } else {
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "projects/groq-ollama/ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        }
        _exit(127);
    }
    if (pid > 0) log_pid(pid, "groq-ollama-tool");
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
#endif
}

static void load_apis(void) {
    FILE *f = fopen("projects/groq-ollama/config/apis.txt", "r");
    if (!f) { snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: Could not open config/apis.txt"); return; }
    g_api_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && g_api_count < 16) {
        char *sep = strchr(line, '|');
        if (sep) {
            *sep = '\0';
            strncpy(g_api_list[g_api_count].name, trim_str(line), 63);
            strncpy(g_api_list[g_api_count].url, trim_str(sep + 1), 255);
            g_api_count++;
        }
    }
    fclose(f);
    char *msg = NULL; if (asprintf(&msg, "Loaded %d APIs.", g_api_count) != -1) { snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", msg); free(msg); }
}

static void update_menu_markup(void) {
    char buf[4096] = "";
    for (int i = 0; i < g_api_count; i++) {
        char line[512];
        snprintf(line, sizeof(line), "        <button label=\"%s\" onClick=\"SET_API:%s\" /><br/>", g_api_list[i].name, g_api_list[i].url);
        strcat(buf, line);
    }
    snprintf(g_menu_area, sizeof(g_menu_area), "%s", buf);
}

static void update_completions(void) {
    char *star = strrchr(g_last_buf_input, '*');
    if (!star) {
        clear_completion_state();
        return;
    }

    char query[MAX_PATH];
    char *prefix = star + 1;
    if (*prefix == '\0') {
        snprintf(query, sizeof(query), "%s/", g_sandbox_root);
    } else {
        snprintf(query, sizeof(query), "%s/%s", g_sandbox_root, prefix);
    }

    char *p_args[] = {"projects/groq-ollama/ops/+x/complete_path", query, NULL};
    char *matches = run_tool(p_args[0], p_args, false);
    clear_completion_state();
    if (!matches || strlen(matches) == 0) {
        free(matches);
        return;
    }

    g_completion_mode = true;
    g_comp_root_vis = true;
    char *copy = strdup(matches);
    if (!copy) {
        free(matches);
        return;
    }

    size_t root_len = strlen(g_sandbox_root);
    char *token = strtok(copy, " \t\r\n");
    int count = 0;
    while (token && count < 5) {
        // Strip the sandbox_root prefix so suggestions show clean relative
        // paths instead of the full "projects/.../sandbox/..." text.
        const char *label = token;
        if (strncmp(label, g_sandbox_root, root_len) == 0) {
            label += root_len;
            if (*label == '/') label++;
        }
        strncpy(g_comp_labels[count], label, sizeof(g_comp_labels[count]) - 1);
        g_comp_labels[count][sizeof(g_comp_labels[count]) - 1] = '\0';
        g_comp_vis[count] = true;
        token = strtok(NULL, " \t\r\n");
        count++;
    }

    free(copy);
    free(matches);

    // Jump keyboard focus to the suggestion list (index 1) so pressing
    // 2-6 picks a suggestion instead of typing those digits into the
    // input field, and pulse an out-of-cycle render.
    g_completion_return_gui_index = read_active_gui_index();
    FILE *agi_f = fopen("pieces/display/active_gui_index.txt", "w");
    if (agi_f) { fprintf(agi_f, "1\n"); fclose(agi_f); }
    write_gui_state();
    pulse_state_changed();
    trigger_render();
}

static void handle_choose_path(int index) {
    if (index < 2 || index > 6) return;
    int slot = index - 2;
    if (!g_completion_mode || slot < 0 || slot >= 5 || !g_comp_vis[slot]) return;

    char *star = strrchr(g_last_buf_input, '*');
    if (!star) return;

    *star = '\0';
    char new_input[2048];
    snprintf(new_input, sizeof(new_input), "%s%s", g_last_buf_input, g_comp_labels[slot]);
    snprintf(g_last_buf_input, sizeof(g_last_buf_input), "%s", new_input);
    set_input_text_override(new_input);

    // Re-inject the completed text into the shared keystroke-buffer file so
    // the frontend's own input tracking doesn't fall out of sync with what
    // we just set here.
    //
    // Two corrections to what this used to do. It opened the file with "w",
    // which TRUNCATES -- and cli_buffers.txt is a single global file shared
    // by every project on the desktop, so accepting one autocomplete
    // suggestion here silently destroyed every other app's field history
    // (gem-dev polls it by file offset, agy/op-ed/slop-ed-dev scan it for
    // their "s"/"f"-prefixed lines). It now appends.
    //
    // And it wrote the value with NO prefix character, which is the format
    // the parser's own cli_io writer never emits and that every prefix-keyed
    // consumer ignores -- so the line could not actually be matched by the
    // "frontend" this comment is trying to keep in sync, defeating the
    // stated purpose. chtpm_parser.c keys each line by the cli_io element's
    // id (username->U, password->P, answer->A, otherwise the first character
    // of the id); this layout's field is id="input_text", so the prefix is
    // 'i'. Verified live against the parser: typing "abc" into a
    // <cli_io id="input_text"> publishes exactly 'i' followed by the text.
    FILE *bfw = fopen("pieces/apps/player_app/cli_buffers.txt", "a");
    if (bfw) { fprintf(bfw, "i%s\n", new_input); fclose(bfw); }

    clear_completion_state();

    // Restore keyboard focus to wherever it was before the suggestion list
    // grabbed it.
    int restore_idx = (g_completion_return_gui_index > 0) ? g_completion_return_gui_index : 2;
    FILE *agi_f = fopen("pieces/display/active_gui_index.txt", "w");
    if (agi_f) { fprintf(agi_f, "%d\n", restore_idx); fclose(agi_f); }
    g_completion_return_gui_index = -1;

    write_gui_state();
    pulse_state_changed();
    trigger_render();
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

void audit_log(const char* user, const char* assistant) {
    char *path = "projects/groq-ollama/pieces/world_01/map_01/iqabel/memories/history.txt";
    mkdir("projects/groq-ollama/pieces/world_01/map_01/iqabel/memories", 0755);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        char *ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s]\nUSER: %s\nAGENT: %s\n\n", ts, user, assistant);
        fclose(f);
    }
}

static void resolve_local_model(void) {
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;
    if (is_gemini) {
        strncpy(g_current_model, "gemini-1.5-flash", 255);
        return;
    }

    char* tags_file = "projects/groq-ollama/state/ollama_tags.json";
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

    // The path-aware json_parser needs one exact path per lookup (no
    // wildcard-array support), so walk /api/tags' "models" array index by
    // index instead of relying on a single shallow "first match" scan.
    // Response shape: {"models":[{"name":"llama3-groq-tool-use:8b",...}, ...]}
    // (llama.cpp's /v1/models uses {"data":[{"id":"..."}, ...]} instead.)
    const char* array_key = is_llamacpp ? "data" : "models";
    const char* name_key = is_llamacpp ? "id" : "name";
    char* first_model = NULL;
    char* groq_model = NULL;
    char* groq_tool_use_model = NULL;
    for (int i = 0; i < 32; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s[%d].%s", array_key, i, name_key);
        char* find_args[] = {"projects/groq-ollama/ops/+x/json_parser", tags_file, path, NULL};
        char* name = run_tool(find_args[0], find_args, false);
        if (!name || strlen(name) == 0) { if (name) free(name); break; }
        if (!first_model) first_model = strdup(name);
        if (!groq_model && strstr(name, "groq")) groq_model = strdup(name);
        if (!groq_tool_use_model && strstr(name, "groq-tool-use")) groq_tool_use_model = strdup(name);
        free(name);
        if (groq_tool_use_model) break;
    }
    const char* chosen = groq_tool_use_model ? groq_tool_use_model : (groq_model ? groq_model : first_model);
    if (chosen) strncpy(g_current_model, chosen, 255);
    free(first_model); free(groq_model); free(groq_tool_use_model);
}

static void handle_gemini_stream(const char* chunk, char** acc_text, char** acc_func) {
    const char* p = chunk;
    
    // Robust pattern search: find "text" then skip to the start of the string
    const char* t = strstr(p, "\"text\"");
    if (t) {
        t = strchr(t, ':');
        if (t) {
            t = strchr(t, '"');
            if (t) {
                t++; // Start of content
                const char* start = t;
                while (*t && (*t != '"' || (*(t-1) == '\\'))) {
                    if (*t == '\\' && *(t+1)) t += 2;
                    else t++;
                }
                size_t len = t - start;
                size_t old_len = *acc_text ? strlen(*acc_text) : 0;
                *acc_text = realloc(*acc_text, old_len + len + 1);
                memcpy(*acc_text + old_len, start, len);
                (*acc_text)[old_len + len] = '\0';
                
                // Update projection in real-time
                char *unescaped = malloc(old_len + len + 1);
                char *s = *acc_text, *d = unescaped;
                while (*s) {
                    if (*s == '\\' && *(s+1)) {
                        s++;
                        if (*s == 'n') *d++ = '\n';
                        else if (*s == 't') *d++ = '\t';
                        else if (*s == '"') *d++ = '"';
                        else if (*s == '\\') *d++ = '\\';
                        else *d++ = *s;
                        s++;
                    } else *d++ = *s++;
                }
                *d = '\0';
                void format_response(const char* src);
                format_response(unescaped);
                free(unescaped);
                
                // Signal change via file for parent
                FILE *fs = fopen("projects/groq-ollama/state/stream_resp.txt", "w");
                if (fs) { fputs(*acc_text, fs); fclose(fs); }
                void trigger_render(void);
                trigger_render();
            }
        }
    }

    if (!*acc_func) {
        const char* f = strstr(p, "\"functionCall\"");
        if (f) {
            f = strchr(f, ':');
            if (f) {
                while (*f && *f != '{') f++;
                if (*f == '{') {
                    const char* start = f; int depth = 0;
                    while (*f) {
                        if (*f == '{') depth++; else if (*f == '}') depth--;
                        f++; if (depth == 0) break;
                    }
                    size_t len = f - start;
                    *acc_func = malloc(len + 1);
                    memcpy(*acc_func, start, len);
                    (*acc_func)[len] = '\0';
                }
            }
        }
    }
}

static char* get_gemini_key(void) {
    char *key = getenv("GEMINI_API_KEY");
    return key ? strdup(key) : NULL;
}

// Ollama-native (OpenAI-shape) tool schema. Sent as "tools" in the request
// body so llama3-groq-tool-use:8b actually engages its tool-calling path
// instead of relying on the persona prompt alone (confirmed live: without
// this field the model ignores tool instructions and just echoes prompt
// text back). Keep this in sync with the dispatch table in
// execute_pending_tool().
static const char* OLLAMA_TOOLS_JSON =
"[\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"exec_cmd\",\"description\":\"Run a shell command on the host system\",\"parameters\":{\"type\":\"object\",\"properties\":{\"cmd\":{\"type\":\"string\",\"description\":\"The shell command line string to execute\"}},\"required\":[\"cmd\"]}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"read_file\",\"description\":\"Read the contents of a file from the local filesystem\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"The absolute or relative path to the file to read\"}},\"required\":[\"path\"]}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"write_file\",\"description\":\"Create a new file or overwrite an existing file with content\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"The path to write the file to\"},\"content\":{\"type\":\"string\",\"description\":\"The full string content to write to the file\"}},\"required\":[\"path\",\"content\"]}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"list_dir\",\"description\":\"List all files and subdirectories in a directory\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"The directory path to list (defaults to . if empty)\"}}}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"search_in_files\",\"description\":\"Search local files recursively for a specific text query/pattern\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The text query to search for\"}},\"required\":[\"query\"]}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"edit_file\",\"description\":\"Surgically search and replace a block of text in an existing file\",\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"The file path to edit\"},\"search\":{\"type\":\"string\",\"description\":\"The exact contiguous block of code/text to match\"},\"replace\":{\"type\":\"string\",\"description\":\"The new replacement text block\"}},\"required\":[\"path\",\"search\",\"replace\"]}}},\n"
"  {\"type\":\"function\",\"function\":{\"name\":\"web_search\",\"description\":\"Search the internet using DuckDuckGo for up-to-date information\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The search query\"}},\"required\":[\"query\"]}}}\n"
"]";

#ifdef _WIN32
/* Body of the old forked child, verbatim apart from popen->_popen and
   pclose->_pclose. Returns an exit code rather than calling _exit(), because
   check_ai_status() reads it with GetExitCodeThread() and reports a non-zero
   result as "Curl failed (code N)" -- the same contract the POSIX version had,
   where a curl that could not be exec'd returned 127. */
static DWORD WINAPI win_gemini_worker(LPVOID unused) {
    (void)unused;
    FILE *stream_fp = _popen(g_win_curl_cmd, "r");
    char *acc_text = NULL, *acc_func = NULL;
    if (stream_fp) {
        char line[8192];
        FILE *flog = fopen("projects/groq-ollama/state/gemini_stream.log", "w");
        while (fgets(line, sizeof(line), stream_fp)) {
            if (flog) { fputs(line, flog); fflush(flog); }
            if (strncmp(line, "data: ", 6) == 0) handle_gemini_stream(line + 6, &acc_text, &acc_func);
            else if (strstr(line, "error") || strstr(line, "message")) {
                // Likely a direct JSON error response instead of SSE
                handle_gemini_stream(line, &acc_text, &acc_func);
            }
        }
        if (flog) fclose(flog);
        _pclose(stream_fp);
    }
    FILE *fllm = fopen(g_win_tmp_llm, "w");
    if (fllm) {
        // Flattened structure for the shallow json_parser
        fprintf(fllm, "{\"text\":\"%s\", \"functionCall\":%s}", 
                acc_text ? acc_text : "", acc_func ? acc_func : "null");
        fclose(fllm);
    }
    if (acc_text) free(acc_text);
    if (acc_func) free(acc_func);
    return 0;
}
#endif

void start_ai_query(const char* input) {
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_prompt = "projects/groq-ollama/state/prompt.json";
    char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
    char *tmp_content = "projects/groq-ollama/state/llm_content.json";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;

    if (g_ai_pid > 0) return;
    unlink(tmp_llm); unlink(tmp_content);
    unlink("projects/groq-ollama/state/stream_resp.txt");
    struct stat st;
    if (stat(ctx_file, &st) != 0 || st.st_size < 10) {
        char *persona = read_full_file("projects/groq-ollama/config/persona.txt");
        if (persona && strlen(persona) > 5) {
            char *s_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", persona, NULL};
            run_tool(s_args[0], s_args, false);
            free(persona);
        } else {
            if (persona) free(persona);
            char *default_persona = "You are Aida, a technical agent. Respond in JSON with 'response' or 'tool' keys.";
            char *s_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", default_persona, NULL};
            run_tool(s_args[0], s_args, false);
        }
    }
    if (input) {
        if (g_pending_input) free(g_pending_input);
        g_pending_input = strdup(input);
        char *u_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "user", (char*)input, NULL};
        run_tool(u_args[0], u_args, false);
    }
    snprintf(g_ai_state, sizeof(g_ai_state), "THINKING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "THINKING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Querying AI...");
    g_thinking_start = time(NULL);
    g_thinking_secs = 0;

    if (is_gemini) {
        char *api_key = get_gemini_key();
        if (!api_key) { snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: GEMINI_API_KEY not found in ENV"); return; }
        
        char *build_args[] = {"projects/groq-ollama/ops/+x/gemini_payload_builder", ctx_file, tmp_prompt, NULL};
        run_tool(build_args[0], build_args, false);

#ifdef _WIN32
        /* The POSIX version forked here and the child ran the curl stream,
           parsed it and wrote the flattened response before _exit(0). Windows
           has no fork(), so the identical body runs on a worker thread and
           g_ai_handle holds the THREAD handle -- which is still a HANDLE, so
           the existing WaitForSingleObject(g_ai_handle, 0) poll in
           check_ai_status() works unchanged and the manager loop stays
           responsive while the stream is still arriving. _popen is the MinGW
           equivalent of popen() and, like the POSIX original, hands the command
           to a shell -- so "2>&1" still means the same thing here. */
        {
            char *curl_url = NULL; asprintf(&curl_url, "%s/v1beta/models/%s:streamGenerateContent?alt=sse", g_api_url, g_current_model);
            char *key_hdr = NULL; asprintf(&key_hdr, "x-goog-api-key: %s", api_key);
            char *cmd_buf = NULL;
            if (curl_url && key_hdr &&
                asprintf(&cmd_buf, "curl -s -N -H \"Content-Type: application/json\" -H \"%s\" -d @%s \"%s\" 2>&1",
                         key_hdr, tmp_prompt, curl_url) != -1) {
                snprintf(g_win_curl_cmd, sizeof(g_win_curl_cmd), "%s", cmd_buf);
                snprintf(g_win_tmp_llm, sizeof(g_win_tmp_llm), "%s", tmp_llm);
                DWORD tid = 0;
                g_ai_handle = CreateThread(NULL, 0, win_gemini_worker, NULL, 0, &tid);
                if (g_ai_handle) { g_ai_pid = 0; g_ai_handle_is_thread = 1; }
                else snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: could not start gemini stream thread");
            }
            free(cmd_buf); free(curl_url); free(key_hdr);
        }
        free(api_key);
#else
        g_ai_pid = fork();
        if (g_ai_pid == 0) {
            setpgid(0, 0);
            char *curl_url = NULL; asprintf(&curl_url, "%s/v1beta/models/%s:streamGenerateContent?alt=sse", g_api_url, g_current_model);
            char *key_hdr = NULL; asprintf(&key_hdr, "x-goog-api-key: %s", api_key);
            char *curl_cmd = NULL; asprintf(&curl_cmd, "curl -s -N -H \"Content-Type: application/json\" -H \"%s\" -d @%s \"%s\" 2>&1", key_hdr, tmp_prompt, curl_url);
            
            FILE *stream_fp = popen(curl_cmd, "r");
            char *acc_text = NULL, *acc_func = NULL;
            if (stream_fp) {
                char line[8192];
                FILE *flog = fopen("projects/groq-ollama/state/gemini_stream.log", "w");
                while (fgets(line, sizeof(line), stream_fp)) {
                    if (flog) { fputs(line, flog); fflush(flog); }
                    if (strncmp(line, "data: ", 6) == 0) handle_gemini_stream(line + 6, &acc_text, &acc_func);
                    else if (strstr(line, "error") || strstr(line, "message")) {
                        // Likely a direct JSON error response instead of SSE
                        handle_gemini_stream(line, &acc_text, &acc_func);
                    }
                }
                if (flog) fclose(flog);
                pclose(stream_fp);
            }
            FILE *fllm = fopen(tmp_llm, "w");
            if (fllm) {
                // Flattened structure for the shallow json_parser
                fprintf(fllm, "{\"text\":\"%s\", \"functionCall\":%s}", 
                        acc_text ? acc_text : "", acc_func ? acc_func : "null");
                fclose(fllm);
            }
            _exit(0);
        }
        if (g_ai_pid > 0) log_pid(g_ai_pid, "groq-ollama-gemini");
        free(api_key);
#endif
    } else {
        char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
        char *context = run_tool(r_args[0], r_args, false);
        FILE *pf = fopen(tmp_prompt, "w");
        if (pf) { 
            if (is_llamacpp) {
                fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":%s}\n", g_current_model, context ? context : "[]");
            } else {
                // No "format":"json" here on purpose - it fights with native
                // tool_calls (confirmed live against the Mac's Ollama: with
                // format:json + no tools[], the model just echoes prompt
                // text; with tools[] present, the model reliably returns a
                // real message.tool_calls[].function entry instead).
                fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":%s,\"tools\":%s}\n", g_current_model, context ? context : "[]", OLLAMA_TOOLS_JSON);
            }
            fclose(pf); 
        }
        if (context) free(context);
        char *body_arg = NULL; char *api_path = NULL;
        
        if (is_llamacpp) {
            asprintf(&api_path, "%s/v1/chat/completions", g_api_url);
        } else {
            asprintf(&api_path, "%s/api/chat", g_api_url);
        }

        if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
#ifdef _WIN32
            /* curl writes the reply straight to tmp_llm with -o, so nothing
               needs capturing here: this is fire-and-poll, and the existing
               WaitForSingleObject(g_ai_handle, 0) in check_ai_status() picks
               up the exit code. curl's -v trace goes to curl_debug.log through
               a real file HANDLE rather than a dup2'd fd 2, which does not
               exist on Windows. */
            const char *exe = win_resolve_tool("curl", NULL);
            if (!exe) {
                snprintf(g_sys_msg, sizeof(g_sys_msg), "API Error: curl not found on PATH");
                snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
                snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
            } else {
                char cmd[8192];
                char *argvv[] = {"-v", "-s", "--max-time", "600",
                                 "-H", "Content-Type: application/json",
                                 api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
                for (int i = 0; argvv[i]; i++) win_append_quoted(cmd, sizeof(cmd), argvv[i]);

                HANDLE dbg = CreateFileA("projects/groq-ollama/state/curl_debug.log",
                                         GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                STARTUPINFOA si; PROCESS_INFORMATION pi;
                memset(&si, 0, sizeof(si)); memset(&pi, 0, sizeof(pi));
                si.cb = sizeof(si);
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                si.hStdError = (dbg != INVALID_HANDLE_VALUE) ? dbg : GetStdHandle(STD_ERROR_HANDLE);
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                if (CreateProcessA(exe, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                    g_ai_handle = pi.hProcess;
                    g_ai_handle_is_thread = 0;
                    g_ai_pid = (int)GetProcessId(pi.hProcess);
                    log_pid(g_ai_pid, "groq-ollama-curl");
                    CloseHandle(pi.hThread);
                } else {
                    if (dbg != INVALID_HANDLE_VALUE) CloseHandle(dbg);
                    snprintf(g_sys_msg, sizeof(g_sys_msg), "API Error: could not start curl");
                    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
                    snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
                }
            }
#else
            g_ai_pid = fork();
            if (g_ai_pid == 0) {
                setpgid(0, 0);
                char *curl_args[] = {"curl", "-v", "-s", "--max-time", "600", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                int fd = open("projects/groq-ollama/state/curl_debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
                execvp("curl", curl_args);
                _exit(127);
            }
            if (g_ai_pid > 0) log_pid(g_ai_pid, "groq-ollama-curl");
#endif
        }
        free(body_arg); free(api_path);
    }
}

void check_ai_status(void) {
#ifdef _WIN32
    if (!g_ai_handle) return;
    /* Zero timeout = poll, do not block: the POSIX original called
       waitpid(..., WNOHANG) for exactly this reason, and the manager's main
       loop must stay responsive while the request is still in flight. */
    if (WaitForSingleObject(g_ai_handle, 0) == WAIT_TIMEOUT) return;
    {
        DWORD exit_code = 0;
        /* The gemini branch puts a THREAD handle in g_ai_handle while the
           ollama/llamacpp branch puts a PROCESS handle there, so the exit code
           has to be read with the matching call -- GetExitCodeThread on a
           process handle, or the reverse, returns a meaningless value while
           looking like it worked. */
        if (g_ai_handle_is_thread) GetExitCodeThread(g_ai_handle, &exit_code);
        else                       GetExitCodeProcess(g_ai_handle, &exit_code);
        CloseHandle(g_ai_handle);
        g_ai_handle = NULL;
        g_ai_handle_is_thread = 0;
        g_ai_pid = 0;
        if (exit_code != 0) {
            snprintf(g_sys_msg, sizeof(g_sys_msg), "API Error: Curl failed (code %u)", (unsigned)exit_code);
            snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
            snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
            if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
            return;
        }
    }
#else
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
#endif
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
    char *tmp_content = "projects/groq-ollama/state/llm_content.json";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;

    char *content_json = NULL;
    char *tool_calls = NULL;

    if (is_gemini) {
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "text", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "functionCall", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    } else if (is_llamacpp) {
        // OpenAI-compat (llama.cpp) structure: choices[0].message.content / choices[0].message.tool_calls.
        // NOTE: cpp-llm/llama.cpp tool-calling is being tackled separately later -
        // these paths are fixed for correctness (the old bare "content"/"tool_calls"
        // keys silently returned nothing against the new path-aware parser) but are
        // not yet live-tested end to end.
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "choices[0].message.content", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "choices[0].message.tool_calls", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    } else {
        // Native Ollama /api/chat structure: message.content / message.tool_calls
        // (confirmed live against 10.0.0.144:11434 with llama3-groq-tool-use:8b).
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "message.content", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "message.tool_calls", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    }
    char *tool_name = NULL; char *args_json = NULL;
    if (tool_calls && strlen(tool_calls) > 2) {
        // tool_calls.tmp now holds the raw tool_calls ARRAY, e.g.
        // [{"id":"...","function":{"name":"list_dir","arguments":{"path":"."}}}]
        // so paths below index into element 0 directly.
        FILE *fcalls = fopen("projects/groq-ollama/state/tool_calls.tmp", "w");
        if (fcalls) { fputs(tool_calls, fcalls); fclose(fcalls); }
        char *p_tn[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "[0].function.name", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "[0].function.arguments", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
        if (!args_json || strlen(args_json) == 0) {
            if (args_json) free(args_json);
            char *p_ta2[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "[0].function.args", NULL};
            args_json = run_tool(p_ta2[0], p_ta2, false);
        }
        unlink("projects/groq-ollama/state/tool_calls.tmp");
    } else if (content_json && strlen(content_json) > 0) {
        FILE *fcont = fopen(tmp_content, "w");
        if (fcont) { fputs(content_json, fcont); fclose(fcont); }
        char *p_tn[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "tool", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "args", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
    }
    if (tool_name && strlen(tool_name) > 0) {
        // Defer execution and request permission (mirrors gem-dev's
        // PENDING_PERM/WAIT_PERM flow) instead of running the tool
        // immediately.
        strncpy(g_pending_tool_name, tool_name, sizeof(g_pending_tool_name) - 1);
        g_pending_tool_name[sizeof(g_pending_tool_name) - 1] = '\0';
        if (g_pending_args_json) free(g_pending_args_json);
        g_pending_args_json = args_json; // transfer ownership
        if (g_pending_content_json) free(g_pending_content_json);
        g_pending_content_json = content_json; // transfer ownership

        snprintf(g_ai_state, sizeof(g_ai_state), "PENDING_PERM");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "WAIT_PERM");
        snprintf(g_sys_msg, sizeof(g_sys_msg), "Execute %s? (y/n)", tool_name);

        char format_buf[4096];
        snprintf(format_buf, sizeof(format_buf), "Tool Execution Request\nTool: %s\nArgs: %s\n\nType 'y' to allow or 'n' to deny.", tool_name, args_json ? args_json : "{}");
        format_response(format_buf);

        free(tool_name); free(tool_calls);
        return;
    }
    char *p_resp[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "response", NULL};
    char *resp = run_tool(p_resp[0], p_resp, false);
    if (!resp || strlen(resp) == 0) { 
        if (resp) free(resp); 
        resp = strdup((content_json && strlen(content_json) > 0) ? content_json : "(empty)"); 
    }
    
    // Final fallback for Gemini: if still empty, check the stream file
    if (is_gemini && (strcmp(resp, "(empty)") == 0 || strlen(resp) == 0)) {
        char *stream_data = read_full_file("projects/groq-ollama/state/stream_resp.txt");
        if (stream_data && strlen(stream_data) > 0) {
            free(resp);
            // Unescape stream data
            char *unescaped = malloc(strlen(stream_data) + 1);
            char *s = stream_data, *d = unescaped;
            while (*s) {
                if (*s == '\\' && *(s+1)) {
                    s++;
                    if (*s == 'n') *d++ = '\n';
                    else if (*s == 't') *d++ = '\t';
                    else if (*s == '"') *d++ = '"';
                    else if (*s == '\\') *d++ = '\\';
                    else *d++ = *s;
                    s++;
                } else *d++ = *s++;
            }
            *d = '\0';
            resp = unescaped;
            free(stream_data);
        }
    }
    audit_log(g_pending_input, resp); format_response(resp);
    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Response received.");
    char *r_args_app[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", resp, NULL};
    run_tool(r_args_app[0], r_args_app, false);
    free(resp); free(tool_name); free(args_json); free(content_json); free(tool_calls);
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

void trigger_render(void) { char *path = "pieces/display/frame_changed.txt"; FILE *f = fopen(path, "a"); if (f) { fprintf(f, "M\n"); fclose(f); } }

char* get_gui_var(const char* id) {
    char *path = "projects/groq-ollama/manager/gui_state.txt"; char *line = NULL; size_t len = 0; FILE *f = fopen(path, "r");
    if (!f) return NULL;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, id, strlen(id)) == 0 && line[strlen(id)] == '=') {
            char *val = strdup(line + strlen(id) + 1); if (val[strlen(val)-1] == '\n') val[strlen(val)-1] = '\0';
            fclose(f); free(line); return val;
        }
    }
    fclose(f); free(line); return NULL;
}

void write_gui_state(void) {
    char *path = "projects/groq-ollama/manager/gui_state.txt";

    if (strcmp(g_ai_state, "THINKING") == 0) {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s (%ds) | [API]: %s", g_ai_state, g_thinking_secs, g_api_url);
    } else {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s | [API]: %s", g_ai_state, g_api_url);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/groq-ollama/manager/+x/groq-ollama_manager.+x\n");
        fprintf(f, "active_layout_id=groq-ollama.chtpm\n");
        fprintf(f, "ai_state=%s\n", g_ai_state);
        fprintf(f, "active_api=%s\n", g_api_url);
        fprintf(f, "ai_status_line=%s\n", g_ai_status_line);
        fprintf(f, "ctx_pct=%s\n", g_ctx_pct);
        fprintf(f, "iqabel_fsm=%s\n", g_fsm_state);
        fprintf(f, "groq-ollama_response_area=%s\n", g_resp_area);
        fprintf(f, "groq-ollama_api_menu=%s\n", g_menu_area);
        fprintf(f, "comp_root_vis=%s\n", g_comp_root_vis ? "true" : "false");
        for (int i = 0; i < 5; i++) {
            fprintf(f, "comp_%d_lbl=%s\n", i + 1, g_comp_labels[i]);
            fprintf(f, "comp_%d_vis=%s\n", i + 1, g_comp_vis[i] ? "true" : "false");
        }
        fprintf(f, "sys_msg=%s\n", g_sys_msg);
        fprintf(f, "thinking_secs=%d\n", g_thinking_secs);
        fprintf(f, "input_text=%s\n", g_input_text_state);
        fclose(f);
    }
}

void execute_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_args = "projects/groq-ollama/state/args.tmp";

    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "%s", g_pending_tool_name);
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Executing %s...", g_pending_tool_name);

    FILE *fargs = fopen(tmp_args, "w");
    if (fargs) { fputs(g_pending_args_json ? g_pending_args_json : "{}", fargs); fclose(fargs); }

    char *result = NULL;
    if (strcmp(g_pending_tool_name, "read_file") == 0) {
        char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false);
        if (path && strlen(path) > 0) { char *ta[] = {"file_ops", "read", path, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "write_file") == 0) {
        char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "content", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *cont = run_tool(p_c[0], p_c, false);
        if (path && strlen(path) > 0 && cont) { char *ta[] = {"file_ops", "write", path, cont, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path);
        if (cont) free(cont);
    } else if (strcmp(g_pending_tool_name, "list_dir") == 0) {
        char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false);
        char *ta[] = {"list_dir", (path && strlen(path) > 0) ? path : ".", NULL};
        result = run_tool("list_dir", ta, true);
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "exec_cmd") == 0) {
        char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "cmd", NULL};
        char *cmd = run_tool(p_c[0], p_c, false); if (cmd && strlen(cmd) > 0) { char *ta[] = {"cmd_exec", cmd, NULL}; result = run_tool("cmd_exec", ta, true); }
        if (cmd) free(cmd);
    } else if (strcmp(g_pending_tool_name, "search_in_files") == 0) {
        char *p_q[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "query", NULL};
        char *query = run_tool(p_q[0], p_q, false); if (query && strlen(query) > 0) { char *ta[] = {"search_in_files", query, NULL}; result = run_tool("search_in_files", ta, true); }
        if (query) free(query);
    } else if (strcmp(g_pending_tool_name, "web_search") == 0) {
        if (web_search_ignored()) {
            result = strdup("Web search is disabled by config (config/tool_flags.txt: ignore_web_search=1).");
        } else {
            char *p_q[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "query", NULL};
            char *query = run_tool(p_q[0], p_q, false); if (query && strlen(query) > 0) { char *ta[] = {"web_search", query, NULL}; result = run_tool("web_search", ta, true); }
            if (query) free(query);
        }
    } else if (strcmp(g_pending_tool_name, "edit_file") == 0) {
        char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_s[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "search", NULL};
        char *p_r[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "replace", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *search = run_tool(p_s[0], p_s, false); char *replace = run_tool(p_r[0], p_r, false);
        if (path && strlen(path) > 0 && search && replace) { char *ta[] = {"edit_file", path, search, replace, NULL}; result = run_tool("edit_file", ta, true); }
        if (path) free(path);
        if (search) free(search);
        if (replace) free(replace);
    }

    if (result) {
        char *a_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", (g_pending_content_json && strlen(g_pending_content_json) > 0) ? g_pending_content_json : "{\"tool\":\"...\"}", NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "tool", result, NULL};
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
            snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
            snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
            snprintf(g_sys_msg, sizeof(g_sys_msg), "Tool output ready.");
            char *r_args_app[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", g_resp_area, NULL};
            run_tool(r_args_app[0], r_args_app, false);
            free(result);
            if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
            if (g_pending_content_json) { free(g_pending_content_json); g_pending_content_json = NULL; }
            g_pending_tool_name[0] = '\0';
            if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
            return;
        }
        free(result);
    } else {
        char *a_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", (g_pending_content_json && strlen(g_pending_content_json) > 0) ? g_pending_content_json : "{\"tool\":\"...\"}", NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "tool", "Error: Tool execution failed or unknown tool.", NULL};
        run_tool(t_args[0], t_args, false);
    }

    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_content_json) { free(g_pending_content_json); g_pending_content_json = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

void deny_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/groq-ollama/state/context.json";
    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Tool execution denied by user.");

    char *a_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", (g_pending_content_json && strlen(g_pending_content_json) > 0) ? g_pending_content_json : "{\"tool\":\"...\"}", NULL};
    run_tool(a_args[0], a_args, false);
    char *t_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "tool", "Error: Permission denied by user.", NULL};
    run_tool(t_args[0], t_args, false);

    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_content_json) { free(g_pending_content_json); g_pending_content_json = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

void process_input_trigger(void) {
    char *input = get_gui_var("input_text");
    if (input && strlen(input) > 0) {
        if (strcmp(g_ai_state, "PENDING_PERM") == 0) {
            char *trimmed = trim_str(input);
            set_input_text_override("");
            g_last_buf_input[0] = '\0';
            write_gui_state();
            trigger_render();
            if (strcasecmp(trimmed, "y") == 0 || strcasecmp(trimmed, "yes") == 0) {
                execute_pending_tool();
            } else if (strcasecmp(trimmed, "n") == 0 || strcasecmp(trimmed, "no") == 0) {
                deny_pending_tool();
            } else {
                snprintf(g_sys_msg, sizeof(g_sys_msg), "Invalid response. Type 'y' or 'n' for %s.", g_pending_tool_name);
            }
            if (input) free(input);
            return;
        }
        set_input_text_override("");
        g_last_buf_input[0] = '\0';
        clear_completion_state();
        write_gui_state();
        trigger_render();
        start_ai_query(input);
    }
    if (input) free(input);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sig); signal(SIGTERM, handle_sig);
#ifndef _WIN32
    /* setpgid(0,0) puts the manager in its own process group so a Ctrl-C in
       the terminal's foreground group does not also signal its children.
       Windows has no process groups to join, and CREATE_NO_WINDOW children
       are already isolated from console control events. */
    setpgid(0, 0);
    log_pid(getpid(), "groq-ollama-manager");
#else
    log_pid((long)GetCurrentProcessId(), "groq-ollama-manager");
#endif

    resolve_paths(argc > 1 ? argv[1] : NULL);
    if (chdir(project_root) != 0) perror("chdir project_root failed");
    load_sandbox_root("projects/groq-ollama/config/context.txt", g_sandbox_root, sizeof(g_sandbox_root));
    mkdir("projects/groq-ollama/state", 0755);
    mkdir(g_sandbox_root, 0755);
    {
        char sandbox_cfg[MAX_PATH];
        snprintf(sandbox_cfg, sizeof(sandbox_cfg), "%s/config", g_sandbox_root);
        mkdir(sandbox_cfg, 0755);
        char sandbox_ctx[MAX_PATH];
        snprintf(sandbox_ctx, sizeof(sandbox_ctx), "%s/config/context.txt", g_sandbox_root);
        FILE *sf = fopen(sandbox_ctx, "w");
        if (sf) {
            fprintf(sf, "project_id=groq-ollama\nsandbox_root=%s\n", g_sandbox_root);
            fclose(sf);
        }
    }
    load_apis(); update_menu_markup();
    resolve_local_model();
    reset_session_ui();
    set_input_text_override("");
    write_gui_state();
    trigger_render();
    char *hist_path = "pieces/apps/player_app/history.txt"; long last_pos = 0; struct stat st; if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        char *ctx_file = "projects/groq-ollama/state/context.json";
        int pct = 0; char *r_args_pct[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
        char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
        if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
        snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);
        int state_changed = 0;
        {
            char *live_input = get_gui_var("input_text");
            if (live_input) {
                if (strcmp(live_input, g_last_buf_input) != 0) {
                    snprintf(g_last_buf_input, sizeof(g_last_buf_input), "%s", live_input);
                    set_input_text_override(live_input);
                    if (strchr(g_last_buf_input, '*')) update_completions();
                    else clear_completion_state();
                    state_changed = 1;
                }
                free(live_input);
            }
        }
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[1024];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd_ptr = strstr(line, "SET_API:");
                        char *choose_ptr = strstr(line, "CHOOSE:");
                        if (choose_ptr) {
                            handle_choose_path(atoi(trim_str(choose_ptr + 7)));
                            state_changed = 1;
                        } else if (cmd_ptr) {
                            char *url = trim_str(cmd_ptr + 8);
                            strncpy(g_api_url, url, 255);
                            resolve_local_model();
                            snprintf(g_resp_area, sizeof(g_resp_area), "║ Connected to %-61.61s ║", g_api_url);
                            char *msg = NULL; if (asprintf(&msg, "Switched to %s", g_current_model) != -1) { snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", msg); free(msg); }
                            state_changed = 1;
                            // Debug log to manager_debug.log
                            FILE *df = fopen("manager_debug.log", "a");
                            if (df) { fprintf(df, "SET_API Triggered: %s\n", url); fclose(df); }
                        } else {
                            int key = 0;
                            char *bracket = strrchr(line, ']');
                            if (bracket) key = atoi(bracket + 1);
                            else key = atoi(line);

                            if (key == 10 || key == 13) {
                                if (!get_active_gui_is_typing()) {
                                    process_input_trigger(); state_changed = 1;
                                }
                            }
                            else if (g_completion_mode && key >= '2' && key <= '6') { handle_choose_path(key - '0'); state_changed = 1; }
                            else if (key == '1') { unlink(ctx_file); snprintf(g_resp_area, sizeof(g_resp_area), "║ Context cleared.                                                           ║"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Context Reset."); state_changed = 1; }
                            else if (key == '2') { load_apis(); update_menu_markup(); state_changed = 1; }
                            else if (key == '3') {
                                snprintf(g_ai_state, sizeof(g_ai_state), "SUMMARIZING"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Please wait...");
                                write_gui_state(); trigger_render();
                                char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL}; char *context = run_tool(r_args[0], r_args, false);
                                char *tmp_prompt = "projects/groq-ollama/state/prompt.json"; char *tmp_llm = "projects/groq-ollama/state/llm_response.json"; char *tmp_content = "projects/groq-ollama/state/llm_content.json";
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
                                    asprintf(&api_path, "%s/v1/chat/completions", g_api_url);
                                } else {
                                    asprintf(&api_path, "%s/api/chat", g_api_url);
                                }
                                if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
                                    char *curl_args[] = {"curl", "-s", "--max-time", "600", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
#ifdef _WIN32
                                    /* Summarise is fire-and-forget on the POSIX
                                       side only in the sense that it forks; the
                                       waitpid() on the next line means the
                                       caller blocks for the reply anyway. So
                                       the Windows version runs curl to
                                       completion in-process via
                                       StartProcess/WaitFor, keeping the same
                                       blocking contract the code below relies
                                       on -- g_ai_handle is deliberately NOT
                                       used here, because this curl is not the
                                       tracked AI request and must not be
                                       mistaken for it by check_ai_status(). */
                                    const char *cexe = win_resolve_tool("curl", NULL);
                                    if (cexe) {
                                        char ccmd[8192];
                                        snprintf(ccmd, sizeof(ccmd), "\"%s\"", cexe);
                                        for (int i = 0; curl_args[i]; i++) win_append_quoted(ccmd, sizeof(ccmd), curl_args[i]);
                                        STARTUPINFOA csi; PROCESS_INFORMATION cpi;
                                        memset(&csi, 0, sizeof(csi)); memset(&cpi, 0, sizeof(cpi));
                                        csi.cb = sizeof(csi);
                                        csi.dwFlags = STARTF_USESTDHANDLES;
                                        csi.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                                        csi.hStdError = GetStdHandle(STD_ERROR_HANDLE);
                                        csi.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                                        if (CreateProcessA(cexe, ccmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &csi, &cpi)) {
                                            log_pid((long)GetProcessId(cpi.hProcess), "groq-ollama-summarize");
                                            WaitForSingleObject(cpi.hProcess, INFINITE);
                                            CloseHandle(cpi.hProcess);
                                            CloseHandle(cpi.hThread);
                                        }
                                    }
#else
                                    pid_t cpid = fork(); if (cpid == 0) { setpgid(0, 0); execvp("curl", curl_args); _exit(127); } if (cpid > 0) log_pid(cpid, "groq-ollama-summarize");
                                    waitpid(cpid, NULL, 0);
#endif
                                }
                                free(body_arg); free(api_path);
                                char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, is_llamacpp ? "choices[0].message.content" : "message.content", NULL}; char *content_json = run_tool(p_ext[0], p_ext, false);
                                if (content_json) {
                                    FILE *cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                                    char *p_sum[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "summary", NULL}; char *summary = run_tool(p_sum[0], p_sum, false);
                                    if (summary) {
                                        unlink(ctx_file); char *init_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", summary, NULL};
                                        run_tool(init_args[0], init_args, false); format_response(summary);
                                        snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Summary complete."); free(summary);
                                    }
                                    free(content_json);
                                }
                                state_changed = 1;
                            }
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        int old_pid = g_ai_pid;
        char old_state[64]; strncpy(old_state, g_ai_state, 63);

        if (g_ai_pid > 0 && strcmp(g_ai_state, "THINKING") == 0) {
            char *stream_data = read_full_file("projects/groq-ollama/state/stream_resp.txt");
            if (stream_data) {
                // Unescape for display
                char *unescaped = malloc(strlen(stream_data) + 1);
                char *src = stream_data, *dst = unescaped;
                while (*src) {
                    if (*src == '\\' && *(src+1)) {
                        src++;
                        if (*src == 'n') *dst++ = '\n';
                        else if (*src == 't') *dst++ = '\t';
                        else if (*src == '"') *dst++ = '"';
                        else if (*src == '\\') *dst++ = '\\';
                        else *dst++ = *src;
                        src++;
                    } else *dst++ = *src++;
                }
                *dst = '\0';
                format_response(unescaped);
                free(unescaped); free(stream_data);
                state_changed = 1;
            }
        }

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

        write_gui_state(); if (state_changed) trigger_render();
        usleep(g_completion_mode ? 20000 : 100000);
    }
    return 0;
}
