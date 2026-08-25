#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#define usleep(us) Sleep((us)/1000)
#define getcwd _getcwd
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <dirent.h>
#include <signal.h>
#ifndef _WIN32
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

// TPM CHTPM Parser (v3.6 - PROJECT LOADER FIX)
// Responsibility: 100% Warning-free, app-aware routing and variables.

/* Cross-platform box-drawing characters - ASCII for consistent display */
#define BOX_TL "+"
#define BOX_TR "+"
#define BOX_BL "+"
#define BOX_BR "+"
#define BOX_H "="
#define BOX_V "|"
#define BOX_T_RIGHT "+"
#define BOX_T_LEFT "+"
#define BOX_T_DOWN "+"
#define BOX_T_UP "+"
#define BOX_CROSS "+"
#define BOX_ROW_PREFIX "| "
#define BOX_ROW_SUFFIX " |"

#define MAX_LINE 65536
#define MAX_VAR_NAME 64
#define MAX_VAR_VALUE 65536
#define MAX_VARS 300
#define MAX_ELEMENTS 400
#define MAX_BUFFER 1048576
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#define MAX_CHILDREN 100
#define MAX_LABEL_LEN 65536
#define MAX_ATTR_LEN 1024

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27,
    JOY_BUTTON_0 = 2000, JOY_BUTTON_1 = 2001, JOY_BUTTON_2 = 2002, JOY_BUTTON_3 = 2003,
    JOY_BUTTON_4 = 2004, JOY_BUTTON_5 = 2005, JOY_BUTTON_6 = 2006, JOY_BUTTON_7 = 2007,
    JOY_BUTTON_8 = 2008, JOY_LEFT = 2100, JOY_RIGHT = 2101, JOY_UP = 2102, JOY_DOWN = 2103
};

typedef struct {
    char type[32]; char label[MAX_LABEL_LEN]; char href[MAX_PATH]; char onClick[128];
    char id[MAX_ATTR_LEN]; char visibility_expr[MAX_ATTR_LEN];
    char input_buffer[256];  /* For cli_io text input */
    int parent_index; int children[MAX_CHILDREN]; int num_children;
    bool visibility; int interactive_idx;
    bool is_folded;
} UIElement;

char current_layout[MAX_PATH] = "pieces/chtpm/layouts/os.chtpm";
char last_active_id[64] = "";
char last_methods_raw[MAX_VAR_VALUE] = "";
int focus_index = 0; int active_index = -1;
int element_count = 0; UIElement elements[MAX_ELEMENTS];
char nav_buffer[512] = {0};
bool clear_nav_on_next = false; bool wait_for_view_change = false;
bool is_time_reactive = false;

typedef struct { char name[MAX_VAR_NAME]; char value[MAX_VAR_VALUE]; } Variable;
Variable vars[MAX_VARS]; int var_count = 0;
long last_history_position = 0; 
long last_state_file_size = 0;
long last_master_pulse_size = 0;
long last_display_pulse_size = 0;
long last_view_file_size = 0;
long last_layout_file_size = 0;
#ifdef _WIN32
intptr_t current_module_pid = -1;
#else
pid_t current_module_pid = -1;
#endif
char current_module_path[MAX_PATH] = "";

char* scratch_substituted = NULL;
char project_root_path[MAX_PATH] = ".";
char interact_history_path[MAX_PATH] = "";  // From <interact> tag

// Digit Accumulation State
static int digit_accum = 0;  // Accumulated digit value (0 = no accumulation)

// --- Forward Declarations ---
void parse_chtm(void);
void compose_frame(void);
void load_vars(void);
void substitute_vars(const char* src, char* dst, int max_len);
void set_var(const char* name, const char* value);
const char* get_var(const char* name);
bool is_navigable(int idx);
void initialize_focus(void);
void export_active_index(void);
void launch_module(const char* path);
void cleanup_module(void);
char* read_file_to_string(const char* filename);
void handle_launch_command(const char* cmd);
void send_command(const char* cmd);
bool evaluate_visibility(UIElement* el);
bool is_interactive(UIElement* el);
void parse_attributes(UIElement* el, const char* attr_str);
void render_element(int idx, char* frame, int* p_global_counter, int* p_scoped_counter);
char* build_path_malloc(const char* rel);
char* trim_pmo(char *str);

#ifdef _WIN32
intptr_t win_spawn(const char* path, char* const args[]) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    /* Convert forward slashes to backslashes for Windows path */
    char win_path[MAX_PATH];
    strncpy(win_path, path, MAX_PATH - 1);
    win_path[MAX_PATH - 1] = '\0';
    for (int i = 0; win_path[i]; i++) if (win_path[i] == '/') win_path[i] = '\\';

    /* Build command line string for CreateProcess */
    char cmd_line[MAX_PATH * 2] = "";
    for (int i = 0; args && args[i]; i++) {
        strcat(cmd_line, "\"");
        strcat(cmd_line, args[i]);
        strcat(cmd_line, "\" ");
    }

    if (CreateProcess(win_path, (cmd_line[0] ? cmd_line : NULL), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        return (intptr_t)pi.hProcess;
    }
    return -1;
}
#endif

static bool root_has_anchors(const char* root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];
    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_root() {
    if (!getcwd(project_root_path, sizeof(project_root_path))) strncpy(project_root_path, ".", sizeof(project_root_path) - 1);
    project_root_path[sizeof(project_root_path) - 1] = '\0';
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                if (strlen(v) > 0) {
                    char candidate[MAX_PATH];
                    strncpy(candidate, v, sizeof(candidate) - 1);
                    candidate[sizeof(candidate) - 1] = '\0';
                    if (root_has_anchors(candidate)) {
                        strncpy(project_root_path, candidate, sizeof(project_root_path) - 1);
                        project_root_path[sizeof(project_root_path) - 1] = '\0';
                    }
                }
                break;
            }
        }
        fclose(kvp);
    }
}

void build_path(char* dst, size_t sz, const char* fmt, ...) {
    va_list args; va_start(args, fmt); vsnprintf(dst, sz, fmt, args); va_end(args);
}

char* trim_pmo(char *str) {
    if (!str) return NULL;
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

char* build_path_malloc(const char* rel) {
    char* p = NULL;
    if (asprintf(&p, "%s/%s", project_root_path, rel) == -1) return NULL;
    return p;
}

void cleanup_module() {
#ifndef _WIN32
    if (current_module_pid > 0) {
        kill(current_module_pid, SIGTERM);
        waitpid(current_module_pid, NULL, WNOHANG);
        current_module_pid = -1;
        current_module_path[0] = '\0';
    }
#else
    if (current_module_pid > 0) {
        TerminateProcess((HANDLE)current_module_pid, 0);
        CloseHandle((HANDLE)current_module_pid);
        current_module_pid = -1;
        current_module_path[0] = '\0';
    }
#endif
}

void handle_sigint(int sig __attribute__((unused))) { 
    cleanup_module(); if (scratch_substituted) free(scratch_substituted); exit(0); 
}

void set_var(const char* name, const char* value) {
    for (int i = 0; i < var_count; i++) { 
        if (strcmp(vars[i].name, name) == 0) { 
            strncpy(vars[i].value, value, MAX_VAR_VALUE - 1); vars[i].value[MAX_VAR_VALUE-1] = '\0';
            return; 
        } 
    }
    if (var_count < MAX_VARS) { 
        strncpy(vars[var_count].name, name, MAX_VAR_NAME - 1); vars[var_count].name[MAX_VAR_NAME-1] = '\0';
        strncpy(vars[var_count].value, value, MAX_VAR_VALUE - 1); vars[var_count].value[MAX_VAR_VALUE-1] = '\0';
        var_count++; 
    }
}

const char* get_var(const char* name) {
    /* Special case: ${desktop_view} reads from view.txt directly */
    if (strcmp(name, "desktop_view") == 0) {
        static char desktop_view_cache[4096] = "";
        static time_t last_read = 0;
        time_t now = time(NULL);
        /* Re-read every 100ms to avoid stale data */
        if (now - last_read >= 1) {
            FILE* f = fopen("pieces/apps/gl_os/session/view.txt", "r");
            if (f) {
                char line[256];
                desktop_view_cache[0] = '\0';
                while (fgets(line, sizeof(line), f)) {
                    line[strcspn(line, "\n")] = '|';
                    if (strlen(desktop_view_cache) + strlen(line) < sizeof(desktop_view_cache) - 50) {
                        strcat(desktop_view_cache, line);
                    }
                }
                fclose(f);
            }
            last_read = now;
        }
        return desktop_view_cache;
    }
    for (int i = 0; i < var_count; i++) { if (strcmp(vars[i].name, name) == 0) return vars[i].value; }
    return "";
}

void substitute_vars_naked(const char* src, char* dst, int max_len) {
    const char *p_src = src; char *p_dst = dst;
    bool in_tag = false;
    while (*p_src && (p_dst - dst) < max_len - 1) {
        if (*p_src == '<') in_tag = true;
        else if (*p_src == '>') in_tag = false;

        if (!in_tag && *p_src == '$' && *(p_src+1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64]; int len = end - (p_src + 2); if (len > 63) len = 63;
                strncpy(var_name, p_src + 2, len); var_name[len] = '\0';
                const char *val = get_var(var_name);
                while (*val && (p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val+1) == 'n') {
                        *p_dst++ = '\n';
                        val += 2;
                    } else {
                        *p_dst++ = *val++;
                    }
                }
                p_src = end + 1; continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

void substitute_vars(const char* src, char* dst, int max_len) {
    const char *p_src = src; char *p_dst = dst;
    while (*p_src && (p_dst - dst) < max_len - 1) {
        if (*p_src == '\\' && (*(p_src+1) == '$' || *(p_src+1) == '{' || *(p_src+1) == '<' || *(p_src+1) == '\\')) {
            *p_dst++ = *(p_src+1); p_src += 2; continue;
        }
        if (*p_src == '$' && *(p_src+1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64]; int len = end - (p_src + 2); if (len > 63) len = 63;
                strncpy(var_name, p_src + 2, len); var_name[len] = '\0';
                const char *val = get_var(var_name);
                while (*val && (p_dst - dst) < max_len - 1) {
                    if (*val == '\\' && *(val+1) == 'n') {
                        *p_dst++ = '\n';
                        val += 2;
                    } else {
                        *p_dst++ = *val++;
                    }
                }
                p_src = end + 1; continue;
            }
        }
        *p_dst++ = *p_src++;
    }
    *p_dst = '\0';
}

char* read_file_to_string(const char* filename) {
    FILE* file = fopen(filename, "r"); if (!file) return NULL;
    fseek(file, 0, SEEK_END); long length = ftell(file); fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1); if (!buffer) { fclose(file); return NULL; }
    size_t n = fread(buffer, 1, length, file); buffer[n] = '\0'; fclose(file); return buffer;
}

void load_state_file(const char* rel_path, const char* prefix) {
    char *path = build_path_malloc(rel_path);
    FILE *f = fopen(path, "r");
    if (f) {
        char *line = malloc(MAX_LINE);
        if (!line) { fclose(f); free(path); return; }
        while (fgets(line, MAX_LINE, f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *var_name = NULL;
                if (prefix) asprintf(&var_name, "%s_%s", prefix, trim_pmo(line));
                else asprintf(&var_name, "%s", trim_pmo(line));
                if (var_name) { 
                    set_var(var_name, trim_pmo(eq + 1)); 
                    free(var_name); 
                }
            }
        }
        free(line);
        fclose(f);
    }
    free(path);
}

void save_to_gui_state(const char* name, const char* value) {
    const char* project_id = get_var("project_id");
    if (strlen(project_id) == 0) return;
    
    char *path = NULL;
    asprintf(&path, "%s/projects/%s/manager/gui_state.txt", project_root_path, project_id);
    if (!path) return;

    /* Use heap for these to avoid stack overflow with large MAX_VAR_VALUE/MAX_VARS */
    typedef char VarName[MAX_VAR_NAME];
    typedef char VarValue[MAX_VAR_VALUE];
    VarName *names = malloc(sizeof(VarName) * MAX_VARS);
    VarValue *values = malloc(sizeof(VarValue) * MAX_VARS);
    if (!names || !values) {
        if (names) free(names);
        if (values) free(values);
        free(path);
        return;
    }

    int count = 0;
    
    FILE *f = fopen(path, "r");
    if (f) {
        char *line = malloc(MAX_LINE);
        if (line) {
            while (fgets(line, MAX_LINE, f) && count < MAX_VARS) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    strncpy(names[count], trim_pmo(line), MAX_VAR_NAME - 1);
                    names[count][MAX_VAR_NAME-1] = '\0';
                    strncpy(values[count], trim_pmo(eq + 1), MAX_VAR_VALUE - 1);
                    values[count][MAX_VAR_VALUE-1] = '\0';
                    count++;
                }
            }
            free(line);
        }
        fclose(f);
    }

    /* Update or add variable */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            strncpy(values[i], value, MAX_VAR_VALUE - 1);
            values[i][MAX_VAR_VALUE-1] = '\0';
            found = 1;
            break;
        }
    }
    if (!found && count < MAX_VARS) {
        strncpy(names[count], name, MAX_VAR_NAME - 1);
        names[count][MAX_VAR_NAME-1] = '\0';
        strncpy(values[count], value, MAX_VAR_VALUE - 1);
        values[count][MAX_VAR_VALUE-1] = '\0';
        count++;
    }

    /* Write back all variables */
    f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            fprintf(f, "%s=%s\n", names[i], values[i]);
        }
        fclose(f);
    }
    free(names);
    free(values);
    free(path);
}

// Count available projects for digit accumulation bounds checking
static int count_projects() {
    char *projects_dir_path = build_path_malloc("projects");
    if (!projects_dir_path) return 0;
    DIR *dir = opendir(projects_dir_path);
    if (!dir) { free(projects_dir_path); return 0; }
    
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char *abs_p = NULL;
        if (asprintf(&abs_p, "%s/%s", projects_dir_path, entry->d_name) == -1) continue;
        struct stat st;
        if (stat(abs_p, &st) == 0 && S_ISDIR(st.st_mode)) {
            count++;
        }
        free(abs_p);
    }
    closedir(dir);
    free(projects_dir_path);
    return count;
}

static char* resolve_dynamic_pdl_path(const char* active_id) {
    char *path = NULL;
    const char *project_id = get_var("project_id");

    if (!active_id || active_id[0] == '\0') return NULL;

    if (project_id && project_id[0] != '\0') {
        asprintf(&path, "%s/projects/%s/pieces/%s/piece.pdl", project_root_path, project_id, active_id);
        if (path && access(path, F_OK) == 0) return path;
        free(path); path = NULL;

        asprintf(&path, "%s/projects/%s/pieces/%s/%s.pdl", project_root_path, project_id, active_id, active_id);
        if (path && access(path, F_OK) == 0) return path;
        free(path); path = NULL;

        asprintf(&path, "%s/pieces/apps/%s/pieces/%s/piece.pdl", project_root_path, project_id, active_id);
        if (path && access(path, F_OK) == 0) return path;
        free(path); path = NULL;

        asprintf(&path, "%s/pieces/apps/%s/pieces/%s/%s.pdl", project_root_path, project_id, active_id, active_id);
        if (path && access(path, F_OK) == 0) return path;
        free(path); path = NULL;
    }

    asprintf(&path, "%s/pieces/apps/playrm/%s/%s.pdl", project_root_path, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path); path = NULL;

    asprintf(&path, "%s/pieces/world/map_01/%s/%s.pdl", project_root_path, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path); path = NULL;

    asprintf(&path, "%s/pieces/%s/%s.pdl", project_root_path, active_id, active_id);
    if (path && access(path, F_OK) == 0) return path;
    free(path);

    return NULL;
}

void load_dynamic_methods(const char* active_id) {
    char *path = NULL;
    char *methods_buf = malloc(MAX_VAR_VALUE);
    char *line = NULL;
    FILE *f = NULL;
    int method_idx;

    if (!methods_buf) return;
    methods_buf[0] = '\0';

    path = resolve_dynamic_pdl_path(active_id);
    if (!path) {
        set_var("piece_methods", "[No Methods]");
        free(methods_buf);
        return;
    }

    f = fopen(path, "r");
    if (!f) {
        free(path);
        set_var("piece_methods", "[No Methods]");
        free(methods_buf);
        return;
    }

    line = malloc(MAX_LINE);
    if (!line) {
        fclose(f);
        free(path);
        free(methods_buf);
        return;
    }

    method_idx = (strcmp(active_id, "loader") == 0) ? 1 : 2;
    while (fgets(line, MAX_LINE, f)) {
        char *key_start;
        char *val_start;
        char *cmd_val;
        char *trimmed_key;
        char *btn = NULL;
        bool is_parser_cmd;

        if (strncmp(line, "METHOD", 6) != 0) continue;

        key_start = strchr(line, '|');
        if (!key_start) continue;
        key_start++;
        val_start = strchr(key_start, '|');
        if (!val_start) continue;

        cmd_val = trim_pmo(val_start + 1);
        *val_start = '\0';
        trimmed_key = trim_pmo(key_start);

        if (strcmp(trimmed_key, "move") == 0 || strcmp(trimmed_key, "select") == 0 ||
            strcmp(trimmed_key, "interact") == 0 || strcmp(trimmed_key, "stat_decay") == 0 ||
            strcmp(trimmed_key, "on_turn_end") == 0 || trimmed_key[0] == '_') {
            continue;
        }

        is_parser_cmd = (strncmp(cmd_val, "LOAD_PROJECT:", 13) == 0 ||
                         strncmp(cmd_val, "LAUNCH:", 7) == 0 ||
                         strncmp(cmd_val, "MP3:", 4) == 0 ||
                         strcmp(cmd_val, "BACK") == 0 ||
                         strcmp(cmd_val, "RELEASE") == 0);

        if (strcmp(cmd_val, "void") == 0 || !is_parser_cmd) {
            asprintf(&btn, "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", trimmed_key, method_idx++);
        } else {
            asprintf(&btn, "<button label=\"%s\" onClick=\"%s\" /><br/>", trimmed_key, cmd_val);
        }

        if (btn) {
            if (strlen(methods_buf) + strlen(btn) < MAX_VAR_VALUE - 100) strcat(methods_buf, btn);
            free(btn);
        }
    }

    fclose(f);
    free(line);
    free(path);

    if (strlen(methods_buf) == 0) set_var("piece_methods", "[No Methods]");
    else set_var("piece_methods", methods_buf);
    free(methods_buf);
}

bool is_modern_layout(const char* layout) {
    if (!layout) return false;
    return (strstr(layout, "playrm") != NULL || strstr(layout, "player") != NULL || 
            strstr(layout, "editor") != NULL || strstr(layout, "man-add") != NULL || 
            strstr(layout, "man-ops") != NULL || strstr(layout, "man-pal") != NULL ||
            strstr(layout, "fuzz-op") != NULL || strstr(layout, "op-ed") != NULL ||
            strstr(layout, "desktop") != NULL || strstr(layout, "user") != NULL ||
            strstr(layout, "ai-labs") != NULL || strstr(layout, "p2p-net") != NULL ||
            strstr(layout, "lsr") != NULL || strstr(layout, "blank") != NULL ||
            strstr(layout, "cyoa") != NULL || strstr(layout, "quiz") != NULL ||
            strstr(layout, "mp3") != NULL);
}

void load_vars() {
    var_count = 0;  /* Clear stale variables on every reload */
    set_var("app_title", ""); // Clear stale title early

    // Set modern_layout variable for engine/UI use
    if (is_modern_layout(current_layout)) {
        set_var("modern_layout", "true");
    } else {
        set_var("modern_layout", "false");
    }

    // PRIORITY 2: Contextual Focus Loading
    if (is_modern_layout(current_layout)) {
        load_state_file("pieces/apps/player_app/state.txt", NULL);
        load_state_file("pieces/apps/player_app/manager/state.txt", NULL);
        printf("DEBUG: Loaded directory_listing: %s\n", get_var("directory_listing"));
        printf("DEBUG: Loaded last_key: %s\n", get_var("last_key"));
    } else {
        load_state_file("pieces/apps/fuzzpet_app/manager/state.txt", NULL);
    }
    
    // GL-OS SESSION STATE (loads bg_color, window_count, desktop_view, etc.)
    if (strstr(current_layout, "pieces/apps/gl_os/layouts/desktop.chtpm") != NULL) {
        load_state_file("pieces/apps/gl_os/session/state.txt", NULL);
    }
    
    // APP LAYOUT DETECTION (Pitfall #31: prevent stale project_id from previous session)
    // MUST be AFTER load_state_file to override any stale project_id in state.txt
    if (strstr(current_layout, "projects/") != NULL) {
        const char *start = strstr(current_layout, "projects/") + 9;
        const char *end = strchr(start, '/');
        if (start && end && end > start) {
            char proj_name[64]; int len = (int)(end - start);
            if (len > 0 && len < 64) { memcpy(proj_name, start, len); proj_name[len] = '\0'; set_var("project_id", proj_name); }
        }
    } else if (strstr(current_layout, "pieces/apps/") != NULL) {
        const char *start = strstr(current_layout, "pieces/apps/") + 12;
        const char *end = strchr(start, '/');
        if (start && end && end > start) {
            char app_name[64]; int len = (int)(end - start);
            if (len > 0 && len < 64) { memcpy(app_name, start, len); app_name[len] = '\0'; set_var("project_id", app_name); }
        }
    }

    load_state_file("pieces/system/clock_daemon/state.txt", "clock");
    if (strlen(get_var("turn_count")) == 0 && strlen(get_var("clock_turn")) > 0) set_var("turn_count", get_var("clock_turn"));
    if (strlen(get_var("game_time")) == 0 && strlen(get_var("clock_time")) > 0) set_var("game_time", get_var("clock_time"));
    
    // Set dynamic module path based on project
    const char* proj_id = get_var("project_id");

    if (strlen(proj_id) > 0 && strcmp(proj_id, "template") != 0) {
        char project_state_path[MAX_PATH];
        snprintf(project_state_path, sizeof(project_state_path), "projects/%s/manager/state.txt", proj_id);
        load_state_file(project_state_path, NULL);
    }

    // Load project or app-specific gui_state.txt if it exists
    if (strlen(proj_id) > 0 && strcmp(proj_id, "template") != 0) {
        char gui_path[MAX_PATH];
        /* Search list for gui_state.txt */
        char *g_paths[] = {
            "projects/%s/manager/gui_state.txt",
            "pieces/apps/%s/manager/gui_state.txt",
            "pieces/apps/%s/loader/gui_state.txt",
            "pieces/apps/%s/session/gui_state.txt"
        };
        for (int i = 0; i < 4; i++) {
            snprintf(gui_path, sizeof(gui_path), g_paths[i], proj_id);
            char *abs_g = build_path_malloc(gui_path);
            if (access(abs_g, F_OK) == 0) { 
                load_state_file(gui_path, NULL); 
                free(abs_g); 
                break; 
            }
            free(abs_g);
        }
    }

    const char* active_id = get_var("active_target_id");
    const char* existing_methods = get_var("piece_methods");

    if (strstr(current_layout, "playrm/layouts/loader.chtpm") != NULL) {
        load_dynamic_methods("loader");
    } else if (strlen(active_id) > 0 && (strlen(existing_methods) == 0 || strcmp(existing_methods, "[No Methods]") == 0)) {
        load_dynamic_methods(active_id);
        if (strstr(active_id, "pet") != NULL) set_var("pet_active", "true"); else set_var("pet_active", "false");
        if (strcmp(active_id, "selector") == 0) set_var("selector_active", "true"); else set_var("selector_active", "false");
    }

    // ── GENERIC PROJECT RESOLUTION: Read from project.pdl or construct from convention ──
    // Only run this if gui_state didn't already set the module_path
    if (strlen(get_var("module_path")) == 0 && strlen(proj_id) > 0 && strcmp(proj_id, "template") != 0) {
        // Try to read project.pdl metadata
        char pdl_path[4096];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root_path, proj_id);
        FILE *pdl_f = fopen(pdl_path, "r");
        char pdl_layout[512] = "";
        char pdl_title[256] = "";

        if (pdl_f) {
            char line[1024];
            while (fgets(line, sizeof(line), pdl_f)) {
                // Extract entry_layout from META section
                if (strstr(line, "entry_layout")) {
                    char *pipe = strchr(line, '|');
                    if (pipe) {
                        pipe = strchr(pipe + 1, '|');
                        if (pipe) {
                            char *val = trim_pmo(pipe + 1);
                            char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
                            strncpy(pdl_layout, val, sizeof(pdl_layout) - 1);
                        }
                    }
                }
                // Extract title from STATE section
                if (strstr(line, "title")) {
                    char *pipe = strchr(line, '|');
                    if (pipe) {
                        pipe = strchr(pipe + 1, '|');
                        if (pipe) {
                            char *val = trim_pmo(pipe + 1);
                            char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
                            strncpy(pdl_title, val, sizeof(pdl_title) - 1);
                        }
                    }
                }
            }
            fclose(pdl_f);
        }

        // Construct module path from convention: projects/<id>/manager/+x/<id>_manager.+x
        char module_path[1024];
        snprintf(module_path, sizeof(module_path), "projects/%s/manager/+x/%s_manager.+x", proj_id, proj_id);

        set_var("module_path", module_path);

        // Use PDL layout if available, otherwise construct from convention
        if (strlen(pdl_layout) > 0) {
            set_var("active_layout_id", pdl_layout);
        } else {
            char layout_name[256];
            snprintf(layout_name, sizeof(layout_name), "%s.chtpm", proj_id);
            set_var("active_layout_id", layout_name);
        }

        // Use PDL title if available, otherwise derive from project_id
        if (strlen(pdl_title) > 0) {
            set_var("app_title", pdl_title);
        } else {
            // Convert project_id to title case (e.g., "fuzz-op" → "FUZZ-OP PET SIM")
            char title[256];
            strncpy(title, proj_id, sizeof(title) - 1);
            title[sizeof(title) - 1] = '\0';
            // Uppercase
            for (int i = 0; title[i]; i++) title[i] = toupper((unsigned char)title[i]);
            // Replace hyphens with spaces for display
            for (int i = 0; title[i]; i++) { if (title[i] == '-') title[i] = ' '; }
            set_var("app_title", title);
        }
    }
    // ── FALLBACK: Template or unknown project → playrm generic ──
    else if (strlen(get_var("module_path")) == 0) {
        set_var("module_path", "pieces/apps/playrm/plugins/+x/playrm_module.+x");
        set_var("active_layout_id", "playrm/game.chtpm");
    }
    
    // Load last response
    char *resp_path = build_path_malloc("pieces/apps/fuzzpet_app/fuzzpet/last_response.txt");
    if (strcmp(proj_id, "fuzz-op") == 0) {
        free(resp_path);
        resp_path = build_path_malloc("projects/fuzz-op/pieces/fuzzpet/last_response.txt");
    }
    FILE *rf = fopen(resp_path, "r");
    if (rf) {
        char *resp_buf = malloc(MAX_LINE);
        if (resp_buf && fgets(resp_buf, MAX_LINE, rf)) set_var("last_response", trim_pmo(resp_buf));
        if (resp_buf) free(resp_buf);
        fclose(rf);
    }

    free(resp_path);
    
    load_state_file("pieces/world/map_01/selector/state.txt", "selector");
    load_state_file("pieces/world/map_01/player/state.txt", "player");

    /* GENERIC VIEW LOADING (Dumb Theater Mode) */
    char *view_path = NULL;
    char *v_paths[] = {
        "projects/%s/manager/view.txt",
        "pieces/apps/%s/manager/view.txt",
        "pieces/apps/%s/session/view.txt",
        "pieces/apps/player_app/view.txt",
        "pieces/apps/fuzzpet_app/fuzzpet/view.txt"
    };
    for (int i = 0; i < 5; i++) {
        char full_v[MAX_PATH];
        if (strstr(v_paths[i], "%s")) snprintf(full_v, sizeof(full_v), v_paths[i], proj_id);
        else strcpy(full_v, v_paths[i]);
        
        char *abs_v = build_path_malloc(full_v);
        if (access(abs_v, F_OK) == 0) {
            view_path = abs_v;
            break;
        }
        free(abs_v);
    }

    if (view_path) {
        FILE *vf = fopen(view_path, "r");
        if (vf) {
            char *map_buf = malloc(MAX_VAR_VALUE); if (!map_buf) { free(view_path); return; }
            map_buf[0] = '\0'; char *line = malloc(MAX_LINE);
            if (line) {
                while (fgets(line, MAX_LINE, vf)) { if (strlen(map_buf) + strlen(line) < MAX_VAR_VALUE - 1) strcat(map_buf, line); }
                free(line);
            }
            set_var("game_map", map_buf);
            set_var("desktop_view", map_buf);
            fclose(vf); free(map_buf);
        }
        free(view_path);
    }
 else {
        set_var("game_map", "[Map Loading...]");
        set_var("desktop_view", "[Desktop Loading...]");
    }
}

void inject_raw_key(int code) {
    FILE *fp;
    
    // PRIORITY 1: <interact> tag specifies path (HIGHEST PRIORITY)
    if (interact_history_path[0] != '\0') {
        char *full_path = build_path_malloc(interact_history_path);
        fp = fopen(full_path, "a");
        if (fp) { fprintf(fp, "%d\n", code); fclose(fp); }
        free(full_path);
        return;
    }
    
    const char* project_id = get_var("project_id");

    // PRIORITY 2: Project-specific history
    if (strlen(project_id) > 0 && strcmp(project_id, "template") != 0) {
        char *path = NULL;
        if (asprintf(&path, "%s/projects/%s/history.txt", project_root_path, project_id) != -1) {
            fp = fopen(path, "a");
            if (fp) { fprintf(fp, "%d\n", code); fclose(fp); }
            free(path);
            return;
        }
    }
    
    // PRIORITY 3: Layout-based fallback
    else if (is_modern_layout(current_layout)) {
        char *path = build_path_malloc("pieces/apps/player_app/history.txt");
        fp = fopen(path, "a");
        if (fp) { fprintf(fp, "%d\n", code); fclose(fp); }
        free(path);
        return;
    }
    
    // PRIORITY 4: Generic player app fallback
    else {
        char *path = build_path_malloc("pieces/apps/player_app/history.txt");
        fp = fopen(path, "a");
        if (fp) { fprintf(fp, "%d\n", code); fclose(fp); }
        free(path);
    }
}

void inject_command(const char* cmd) {
    FILE *fp;
    const char* fmt = "COMMAND: %s\n";
    
    if (interact_history_path[0] != '\0') {
        char *full_path = build_path_malloc(interact_history_path);
        fp = fopen(full_path, "a");
        if (fp) { fprintf(fp, fmt, cmd); fclose(fp); }
        free(full_path);
        return;
    }
    
    const char* project_id = get_var("project_id");
    if (strlen(project_id) > 0 && strcmp(project_id, "template") != 0) {
        char *path = NULL;
        if (asprintf(&path, "%s/projects/%s/history.txt", project_root_path, project_id) != -1) {
            fp = fopen(path, "a");
            if (fp) { fprintf(fp, fmt, cmd); fclose(fp); }
            free(path);
            return;
        }
    }
    
    if (is_modern_layout(current_layout)) {
        char *path = build_path_malloc("pieces/apps/player_app/history.txt");
        fp = fopen(path, "a");
        if (fp) { fprintf(fp, fmt, cmd); fclose(fp); }
        free(path);
    }
}

void launch_module(const char* launch_str) { 
    // DEBUG: Log module launch attempt
    {
        FILE *dbg = fopen("debug.txt", "a");
        if (dbg) {
            time_t t; struct tm *tm;
            time(&t); tm = localtime(&t);
            fprintf(dbg, "[%02d:%02d:%02d] PARSER: launch_module(%s) pid=%d\n",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, launch_str, current_module_pid);
            fclose(dbg);
        }
    }

    if (strcmp(launch_str, current_module_path) == 0 && current_module_pid > 0) return;
    cleanup_module();
    strncpy(current_module_path, launch_str, MAX_PATH - 1);
    
    char* full_cmd = strdup(launch_str); char* args[16]; int arg_count = 0;
    char* token = strtok(full_cmd, " ");
    while (token && arg_count < 15) {
        if (arg_count == 0) args[arg_count++] = (token[0] == '/') ? strdup(token) : build_path_malloc(token);
        else args[arg_count++] = strdup(token);
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

#ifndef _WIN32
    current_module_pid = fork();
    if (current_module_pid == 0) {
        if (project_root_path[0] != '\0') chdir(project_root_path);
        execv(args[0], args); exit(1);
    }
    else if (current_module_pid > 0) {
        // DEBUG: Log successful fork
        {
            FILE *dbg = fopen("debug.txt", "a");
            if (dbg) {
                time_t t; struct tm *tm;
                time(&t); tm = localtime(&t);
                fprintf(dbg, "[%02d:%02d:%02d] PARSER: forked module pid=%d\n",
                        tm->tm_hour, tm->tm_min, tm->tm_sec, current_module_pid);
                fclose(dbg);
            }
        }
        char* pm = build_path_malloc("pieces/os/plugins/+x/proc_manager.+x");
        char* module_name = NULL;

        if (strstr(args[0], "projects/") != NULL) {
            char project_part[64];
            const char* start = strstr(args[0], "projects/") + 9;
            const char* end = strchr(start, '/');
            if (end) {
                int len = (int)(end - start);
                if (len > 63) len = 63;
                strncpy(project_part, start, len); project_part[len] = '\0';
                const char* base = strrchr(args[0], '/');
                if (asprintf(&module_name, "%s_%s", project_part, base ? base + 1 : args[0]) == -1) { free(pm); return; }
            } else {
                const char* base = strrchr(args[0], '/');
                module_name = strdup(base ? base + 1 : args[0]);
            }
        } else {
            const char* base = strrchr(args[0], '/');
            module_name = strdup(base ? base + 1 : args[0]);
        }

        if (module_name) {
            char* dot = strchr(module_name, '.'); if (dot) *dot = '\0';

            pid_t p = fork();
            if (p == 0) {
                char *pid_str = NULL;
                if (asprintf(&pid_str, "%d", current_module_pid) != -1) {
                    execl(pm, pm, "register", module_name, pid_str, NULL);
                    free(pid_str);
                }
                exit(1);
            } else { waitpid(p, NULL, 0); }
            free(module_name);
        }
        free(pm);
    }
#else
    current_module_pid = win_spawn(args[0], args);
    
    // DEBUG: Log Windows spawn result
    {
        FILE *dbg = fopen("debug.txt", "a");
        if (dbg) {
            time_t t; struct tm *tm;
            time(&t); tm = localtime(&t);
            fprintf(dbg, "[%02d:%02d:%02d] PARSER: win_spawn module handle=%p (Path: %s)\n",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, (void*)current_module_pid, args[0]);
            fclose(dbg);
        }
    }

    if (current_module_pid > 0) {
        char* pm = build_path_malloc("pieces/os/plugins/+x/proc_manager.+x");
        char* module_name = NULL;
        if (strstr(args[0], "projects/") != NULL) {
            char project_part[64];
            const char* start = strstr(args[0], "projects/") + 9;
            const char* end = strchr(start, '/');
            if (end) {
                int len = (int)(end - start);
                if (len > 63) len = 63;
                strncpy(project_part, start, len); project_part[len] = '\0';
                const char* base = strrchr(args[0], '/');
                if (asprintf(&module_name, "%s_%s", project_part, base ? base + 1 : args[0]) != -1) {
                    char* dot = strchr(module_name, '.'); if (dot) *dot = '\0';
                    DWORD win_pid = GetProcessId((HANDLE)current_module_pid);
                    
                    /* Background registration using win_spawn */
                    char win_pid_str[32]; snprintf(win_pid_str, sizeof(win_pid_str), "%lu", win_pid);
                    char* pm_args[5];
                    pm_args[0] = pm;
                    pm_args[1] = "register";
                    pm_args[2] = module_name;
                    pm_args[3] = win_pid_str;
                    pm_args[4] = NULL;
                    win_spawn(pm, pm_args);

                    free(module_name);
                }
            }
        }
        if (pm) free(pm);
    }
#endif
    for (int i = 0; i < arg_count; i++) free(args[i]);
    free(full_cmd);
}

void handle_launch_command(const char* cmd) {
    char app_name[64]; strncpy(app_name, cmd + 7, sizeof(app_name)-1); app_name[sizeof(app_name)-1] = '\0';
    
    /* NEW: Support direct CHTPM layout launching (Dumb Theater Mode) */
    if (strstr(app_name, ".chtpm") != NULL) {
        strncpy(current_layout, app_name, MAX_PATH-1);
        active_index = -1;
        focus_index = 0;
        parse_chtm();
        initialize_focus();
        compose_frame();
        return;
    }

    char path_list_path[MAX_PATH];
    build_path(path_list_path, sizeof(path_list_path), "%s/pieces/os/app_list.txt", project_root_path);
    FILE *f = fopen(path_list_path, "r");
    if (f) { 
        char line[MAX_LINE]; 
        while (fgets(line, sizeof(line), f)) { 
            line[strcspn(line, "\n")] = 0; char *eq = strchr(line, '='); 
            if (eq) { 
                *eq = '\0'; 
                if (strcasecmp(line, app_name) == 0) { 
                    char module_path[MAX_PATH];
                    build_path(module_path, sizeof(module_path), "%s/%s", project_root_path, trim_pmo(eq + 1));
                    
                    /* GL-OS background launch fix */
                    if (strcasecmp(app_name, "GL-OS") == 0) {
                        /* GL-OS is an APP, not a PROJECT - don't set project_id */
                        set_var("module_path", "pieces/apps/gl_os/plugins/+x/gl_desktop.+x");
                        strncpy(current_layout, "pieces/apps/gl_os/layouts/desktop.chtpm", MAX_PATH-1);
                        active_index = -1;
                        focus_index = 0;
                        parse_chtm();
                        initialize_focus();
#ifndef _WIN32
                        char bg_cmd[MAX_PATH + 10];
                        snprintf(bg_cmd, sizeof(bg_cmd), "'%s' &", module_path);
                        system(bg_cmd);
#else
                        char *args[2];
                        args[0] = module_path;
                        args[1] = NULL;
                        win_spawn(module_path, args);
#endif
                    } else {

                        launch_module(module_path);
                    }
                    break; 
                } 
            } 
        } 
        fclose(f); 
    }
}

void send_command(const char* cmd) {
    /* Handle KEY:n prefix for injecting key codes */
    if (strncmp(cmd, "KEY:", 4) == 0) {
        int k = atoi(cmd + 4);
        if (k >= 0 && k <= 9) inject_raw_key('0' + k); else inject_raw_key(k);
        return;
    }
    
    if (strncmp(cmd, "LOAD_PROJECT:", 13) == 0) {
        const char *proj_name = cmd + 13;
        char entry_layout[MAX_PATH] = "";
        
        /* 1. Try to read PDL for layout */
        char pdl_path[MAX_PATH];
        snprintf(pdl_path, sizeof(pdl_path), "%s/projects/%s/project.pdl", project_root_path, proj_name);
        FILE *pf = fopen(pdl_path, "r");
        if (pf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), pf)) {
                if (strstr(line, "entry_layout")) {
                    char *pipe = strchr(line, '|');
                    if (pipe) pipe = strchr(pipe + 1, '|');
                    if (pipe) {
                        char *val = trim_pmo(pipe + 1);
                        strncpy(entry_layout, val, sizeof(entry_layout) - 1);
                    }
                }
            }
            fclose(pf);
        }

        /* 2. Fallbacks if PDL empty */
        if (entry_layout[0] == '\0') {
            if (strcmp(proj_name, "op-ed") == 0) strcpy(entry_layout, "projects/op-ed/layouts/op-ed.chtpm");
            else snprintf(entry_layout, sizeof(entry_layout), "projects/%s/layouts/%s.chtpm", proj_name, proj_name);
        }

        /* 3. Update state.txt */
        char state_path[MAX_PATH];
        snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root_path);
        FILE *sf = fopen(state_path, "w");
        if (sf) {
            fprintf(sf, "project_id=%s\n", proj_name);
            fprintf(sf, "current_map=map_01.txt\n");
            fprintf(sf, "active_target_id=selector\n");
            fprintf(sf, "current_z=0\n");
            fprintf(sf, "last_key=None\n");
            fclose(sf);
        }

        /* 4. Execute transition */
        strncpy(current_layout, entry_layout, MAX_PATH-1);
        current_module_path[0] = '\0';
        current_module_pid = 0;
        active_index = -1;
        focus_index = 0;
        parse_chtm();
        initialize_focus();
        export_active_index();
        compose_frame();
        return;
    }
    if (strncmp(cmd, "LAUNCH:", 7) == 0) { handle_launch_command(cmd); return; }
    if (strncmp(cmd, "MP3:", 4) == 0 || strncmp(cmd, "OP:", 3) == 0 || strncmp(cmd, "SET_", 4) == 0) { 
        inject_command(cmd); 
        return; 
    }
    if (strcmp(cmd, "BACK") == 0) {
        if (active_index != -1) {
            int old_active = active_index;
            int p = elements[active_index].parent_index;
            /* Find nearest ACTIVATE ancestor */
            while (p != -1 && strcmp(elements[p].onClick, "ACTIVATE") != 0) p = elements[p].parent_index;
            
            active_index = p;
            focus_index = old_active;
            if (active_index != -1 && !is_navigable(focus_index)) initialize_focus();
            export_active_index();
        } else {
            inject_raw_key('9'); /* Fallback for non-submenu contexts */
        }
        return;
    }
    if (strcmp(cmd, "RELEASE") == 0) { inject_raw_key('9'); return; }

    /* UNRECOGNIZED COMMAND DEBUG SIGNAL (Pitfall #99) */
    if (strlen(cmd) > 0 && strcmp(cmd, "ACTIVATE") != 0 && strcmp(cmd, "INTERACT") != 0) {
        FILE *dbg = fopen("debug.txt", "a");
        if (dbg) {
            time_t t; struct tm *tm;
            time(&t); tm = localtime(&t);
            fprintf(dbg, "[%02d:%02d:%02d] PARSER ERROR: Command '%s' rejected. Missing SET_, OP:, or MP3: prefix (Pitfall #99).\n",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, cmd);
            fclose(dbg);
        }
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "CMD REJECTED: %s (Check prefix)", cmd);
        set_var("PARSER_ERROR", err_msg);
    }
}

bool evaluate_visibility(UIElement* el) {
    if (el->visibility_expr[0] == '\0') return true;
    char substituted[512]; substitute_vars(el->visibility_expr, substituted, 512);
    char *s = trim_pmo(substituted); return (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
}

bool is_interactive(UIElement* el) { return (strcmp(el->type, "button") == 0 || strcmp(el->type, "canvas") == 0 || strcmp(el->type, "cli_io") == 0 || strcmp(el->type, "scroller") == 0); }
bool is_descendant(int child_idx, int parent_idx) {
    if (child_idx < 0 || parent_idx < 0) return false;
    int p = elements[child_idx].parent_index;
    while (p != -1) {
        if (p == parent_idx) return true;
        p = elements[p].parent_index;
    }
    return false;
}

void parse_attributes(UIElement* el, const char* attr_str) {
    if (!attr_str || strlen(attr_str) == 0) return;
    char* attrs = strdup(attr_str);
    char* pos = attrs;
    while (*pos) {
        while (*pos && isspace((unsigned char)*pos)) { pos++; } if (!*pos) break;
        char* name_start = pos; while (*pos && *pos != '=' && !isspace((unsigned char)*pos)) { pos++; }
        char saved = *pos; *pos = '\0'; while (*(++pos) && isspace((unsigned char)*pos));
        if (*pos == '=') { pos++; while (*pos && isspace((unsigned char)*pos)) { pos++; } }
        char* val_start = pos;
        if (*pos == '"' || *pos == '\'') { char quote = *pos++; val_start = pos; while (*pos && *pos != quote) { pos++; } if (*pos) *pos++ = '\0'; }
        else { while (*pos && !isspace((unsigned char)*pos) && *pos != '/') { pos++; } if (*pos) *pos++ = '\0'; }
        
        if (strcmp(name_start, "label") == 0) { strncpy(el->label, val_start, MAX_LABEL_LEN - 1); }
        else if (strcmp(name_start, "href") == 0) { strncpy(el->href, val_start, MAX_PATH - 1); }
        else if (strcmp(name_start, "onClick") == 0) { strncpy(el->onClick, val_start, 127); }
        else if (strcmp(name_start, "id") == 0) { strncpy(el->id, val_start, MAX_ATTR_LEN - 1); }
        else if (strcmp(name_start, "visibility") == 0) { strncpy(el->visibility_expr, val_start, MAX_ATTR_LEN - 1); }
        else if (strcmp(name_start, "time_reactive") == 0) { is_time_reactive = (strcmp(val_start, "true") == 0); }
        
        *(pos - 1) = saved;
    }
    free(attrs);
}

bool is_navigable(int idx) { 
    if (idx < 0 || idx >= element_count) return false; 
    UIElement* el = &elements[idx]; 
    if (!is_interactive(el) || !evaluate_visibility(el)) return false; 
    
    /* Submenu Activation Logic: If a menu is active, ONLY its descendants are navigable */
    if (active_index != -1) {
        UIElement* active_el = &elements[active_index];
        if (active_el->num_children > 0 && strcmp(active_el->onClick, "ACTIVATE") == 0) {
            /* Active menu root itself IS navigable (allows focus/numbering) */
            if (idx == active_index) return true; 
            if (is_descendant(idx, active_index)) {
                /* Standard folding check for descendants within the scope */
                int p = el->parent_index;
                while (p != -1 && p != active_index) {
                    if (elements[p].is_folded) return false;
                    p = elements[p].parent_index;
                }
                return true;
            }
            return false; /* Not active root and not descendant */
        } else {
            /* Active element is not an activation menu (e.g., cli_io) - only it is navigable */
            return (idx == active_index);
        }
    }
    
    /* Global Mode: Check if any ancestor is folded or is an ACTIVATE menu */
    int p = el->parent_index;
    while (p != -1) {
        if (elements[p].is_folded) return false;
        if (strcmp(elements[p].onClick, "ACTIVATE") == 0) return false;
        p = elements[p].parent_index;
    }
    return true;
}

typedef enum { TOKEN_TEXT, TOKEN_OPEN_TAG, TOKEN_CLOSE_TAG, TOKEN_SELFCLOSE_TAG } ChtpmTokenType;

typedef struct {
    ChtpmTokenType type;
 char content[MAX_LABEL_LEN]; char tag_name[64]; char attributes[512]; } Token;

Token* tokenize(const char* content, int* token_count) {
    Token* tokens = malloc(MAX_ELEMENTS * 4 * sizeof(Token)); *token_count = 0; const char* cursor = content;
    while (*cursor && *token_count < MAX_ELEMENTS * 4) {
        const char* tag_start = strchr(cursor, '<');
        if (!tag_start) { 
            Token* t = &tokens[(*token_count)++]; t->type = TOKEN_TEXT; strncpy(t->content, cursor, MAX_LABEL_LEN-1); t->content[MAX_LABEL_LEN-1]='\0'; break; 
        }
        if (tag_start > cursor) { 
            Token* t = &tokens[(*token_count)++]; t->type = TOKEN_TEXT; 
            int len = tag_start - cursor; if (len > MAX_LABEL_LEN-1) len = MAX_LABEL_LEN-1; 
            strncpy(t->content, cursor, len); t->content[len] = '\0'; 
        }
        const char* tag_end = strchr(tag_start, '>'); 
        if (!tag_end) {
             /* Stray '<' found without matching '>'. Treat as text and continue. */
             Token* t = &tokens[(*token_count)++]; t->type = TOKEN_TEXT;
             t->content[0] = '<'; t->content[1] = '\0';
             cursor = tag_start + 1; continue;
        }
        Token* t = &tokens[(*token_count)++]; char tag_body[512]; int body_len = tag_end - tag_start - 1; if (body_len > 511) body_len = 511;
        strncpy(tag_body, tag_start + 1, body_len); tag_body[body_len] = '\0';
        if (tag_body[0] == '/') { t->type = TOKEN_CLOSE_TAG; strncpy(t->tag_name, tag_body + 1, 63); }
        else {
            bool self_closing = false; if (body_len > 0 && tag_body[body_len-1] == '/') { self_closing = true; tag_body[body_len-1] = '\0'; }
            t->type = self_closing ? TOKEN_SELFCLOSE_TAG : TOKEN_OPEN_TAG;
            char* space = strchr(tag_body, ' ');
            if (space) { *space = '\0'; strncpy(t->tag_name, tag_body, 63); strncpy(t->attributes, space + 1, 511); }
            else { strncpy(t->tag_name, tag_body, 63); t->attributes[0] = '\0'; }
        }
        cursor = tag_end + 1;
    }
    return tokens;
}

void parse_chtm() {
    is_time_reactive = false; 
    
    /* EXPORT CURRENT LAYOUT FOR MODULE HEARTBEAT */
    char *cl_path = build_path_malloc("pieces/display/current_layout.txt");
    FILE *cl_f = fopen(cl_path, "w");
    if (cl_f) { fprintf(cl_f, "%s\n", current_layout); fclose(cl_f); }
    free(cl_path);

    load_vars(); char* content = read_file_to_string(current_layout); if (!content) return;
    char* temp_substituted = malloc(MAX_BUFFER); 
    if (!temp_substituted) { free(content); return; }
    
    substitute_vars_naked(content, temp_substituted, MAX_BUFFER);
    free(content);
    
    int tc; Token* tokens = tokenize(temp_substituted, &tc); free(temp_substituted);
    element_count = 0; int stack[50], top = -1, current_interactive = 0;
    for (int i = 0; i < tc && element_count < MAX_ELEMENTS; i++) {
        Token* t = &tokens[i];
        if (t->type == TOKEN_TEXT) {
            if (i > 0 && strcmp(tokens[i-1].tag_name, "module") == 0 && tokens[i-1].type == TOKEN_OPEN_TAG) {
                char substituted[512]; substitute_vars(t->content, substituted, 512);
                char module_path[512]; strncpy(module_path, trim_pmo(substituted), 511); module_path[511] = '\0';
                launch_module(module_path); continue;
            }
            char* trim_text = strdup(t->content); char* p_trim = trim_text; while (*p_trim && isspace((unsigned char)*p_trim)) p_trim++;
            char* end_trim = p_trim + strlen(p_trim) - 1; while (end_trim > p_trim && isspace((unsigned char)*end_trim)) *end_trim-- = '\0';
            if (!*p_trim) { free(trim_text); continue; }
            UIElement* el = &elements[element_count++]; memset(el, 0, sizeof(UIElement));
            strcpy(el->type, "text"); strncpy(el->label, p_trim, MAX_LABEL_LEN - 1); 
            el->parent_index = (top >= 0) ? stack[top] : -1; free(trim_text);
        } else if (t->type == TOKEN_OPEN_TAG || t->type == TOKEN_SELFCLOSE_TAG) {
            if (strcmp(t->tag_name, "panel") == 0 || strcmp(t->tag_name, "layout") == 0) { 
                UIElement dummy; memset(&dummy, 0, sizeof(UIElement)); parse_attributes(&dummy, t->attributes); 
                if (t->type == TOKEN_OPEN_TAG) stack[++top] = -1; continue; 
            }
            
            UIElement* el = &elements[element_count++]; memset(el, 0, sizeof(UIElement));
            strncpy(el->type, t->tag_name, 31);
            
            if (strcmp(t->tag_name, "br") == 0) { 
                /* No attributes for br */
            }
            else if (strcmp(t->tag_name, "module") == 0) {
                /* First check src attribute */
                char* path = strstr(t->attributes, "src=\"");
                if (path) {
                    path += 5; char* end = strchr(path, '"');
                    if (end) {
                        char raw_path[512]; int len = end - path;
                        strncpy(raw_path, path, len); raw_path[len] = '\0';
                        char sub_path[512];
                        substitute_vars(raw_path, sub_path, 512);
                        launch_module(sub_path);
                    }
                } else if (i + 1 < tc && tokens[i+1].type == TOKEN_TEXT) {
                    /* If no src, check inner text: <module>${module_path}</module> */
                    char sub_path[512];
                    substitute_vars(tokens[i+1].content, sub_path, 512);
                    if (strlen(sub_path) > 0) {
                        launch_module(sub_path);
                    }
                }
            }
            else if (strcmp(t->tag_name, "interact") == 0 && t->type == TOKEN_SELFCLOSE_TAG) {
                char* path_attr = strstr(t->attributes, "src=\"");
                if (path_attr) {
                    path_attr += 5; char* end = strchr(path_attr, '"');
                    if (end) { 
                        int len = end - path_attr;
                        char temp[MAX_PATH];
                        strncpy(temp, path_attr, len);
                        temp[len] = '\0';
                        substitute_vars(temp, interact_history_path, MAX_PATH);
                    }
                }
            }
            else {
                parse_attributes(el, t->attributes);
            }

            /* Native fold support: detect [+] or [-] in label */
            char *p_plus = strstr(el->label, "[+]");
            char *p_minus = strstr(el->label, "[-]");
            if (p_plus || p_minus) {
                char var_name[MAX_ATTR_LEN + 64];
                if (strlen(el->id) > 0) snprintf(var_name, sizeof(var_name), "fold_%s", el->id);
                else {
                    /* Fallback: use sanitized label snippet as ID if no id provided */
                    char sanitized[64] = {0};
                    int j = 0;
                    for (int i = 0; el->label[i] && j < 63; i++) {
                        if (isalnum((unsigned char)el->label[i])) sanitized[j++] = el->label[i];
                    }
                    snprintf(var_name, sizeof(var_name), "fold_%s", sanitized);
                }
                
                const char* state = get_var(var_name);
                if (strcmp(state, "folded") == 0) {
                    el->is_folded = true;
                } else if (strcmp(state, "open") == 0) {
                    el->is_folded = false;
                } else {
                    /* No state yet - use label marker as default */
                    if (p_plus) el->is_folded = true;
                    else el->is_folded = false;
                }

                /* Update label to match current state */
                if (el->is_folded) {
                    if (p_minus) p_minus[1] = '+';
                } else {
                    if (p_plus) p_plus[1] = '-';
                }
            }

            /* Initialize cli_io input_buffer from cli_buffers.txt (last value) */
            if (strcmp(t->tag_name, "cli_io") == 0) {
                FILE *bf = fopen("pieces/apps/player_app/cli_buffers.txt", "r");
                if (bf) {
                    char line[512], last_val[256] = "";
                    char prefix = 'U';
                    if (strstr(el->id, "username")) prefix = 'U';
                    else if (strstr(el->id, "password")) prefix = 'P';
                    else if (strstr(el->id, "answer")) prefix = 'A';
                    
                    while (fgets(line, sizeof(line), bf)) {
                        if (line[0] == prefix) {
                            char* val = line + 1;
                            val[strcspn(val, "\n")] = 0;
                            strncpy(last_val, val, sizeof(last_val) - 1);
                        }
                    }
                    if (strlen(last_val) > 0) {
                        strncpy(el->input_buffer, last_val, sizeof(el->input_buffer) - 1);
                    }
                    fclose(bf);
                }
            }

            /* Initialize cli_io input_buffer from cli_input state (TPM app signal) */
            if (strcmp(t->tag_name, "cli_io") == 0) {
                const char* cli = get_var("cli_input");
                if (cli && strlen(cli) > 0) {
                    /* Extract just the buffer part after "Username: " or "Password: " */
                    const char* colon = strchr(cli, ':');
                    if (colon) {
                        colon++; /* Skip ": " */
                        while (*colon == ' ') colon++;
                        strncpy(el->input_buffer, colon, sizeof(el->input_buffer) - 1);
                    } else {
                        strncpy(el->input_buffer, cli, sizeof(el->input_buffer) - 1);
                    }
                }
            }

            el->parent_index = (top >= 0) ? stack[top] : -1; 
            if (is_interactive(el)) el->interactive_idx = ++current_interactive;
            if (el->parent_index != -1) { 
                UIElement* pa = &elements[el->parent_index]; 
                if (pa->num_children < MAX_CHILDREN) pa->children[pa->num_children++] = element_count - 1;
            }
            if (t->type == TOKEN_OPEN_TAG) stack[++top] = element_count - 1;
        } else if (t->type == TOKEN_CLOSE_TAG) { if (top >= 0) top--; }
    }
    free(tokens);
}

void export_active_index() {
    int active_gui_idx = 0;
    if (active_index != -1) active_gui_idx = elements[active_index].interactive_idx;
    else active_gui_idx = elements[focus_index].interactive_idx;
    
    char *agi_path = build_path_malloc("pieces/display/active_gui_index.txt");
    FILE *agi_f = fopen(agi_path, "w");
    if (agi_f) { fprintf(agi_f, "%d\n", active_gui_idx); fclose(agi_f); }
    free(agi_path);
}

void render_element(int idx, char* frame, int* p_global_counter, int* p_scoped_counter) {
    if (idx < 0 || idx >= element_count) return;
    UIElement* el = &elements[idx]; if (!evaluate_visibility(el)) return;

    /* Visibility Logic: Hide children of ACTIVATE menus unless the menu is active/parent */
    if (el->parent_index != -1) {
        UIElement* pa = &elements[el->parent_index];
        if (strcmp(pa->onClick, "ACTIVATE") == 0) {
            if (active_index != el->parent_index && !is_descendant(active_index, el->parent_index)) {
                return;
            }
        }
    }

    substitute_vars(el->label, scratch_substituted, MAX_LABEL_LEN);
    if (idx == active_index && strcmp(el->onClick, "ACTIVATE") == 0) {
        char *p_plus = strstr(scratch_substituted, "[+]");
        if (p_plus) memmove(p_plus, p_plus + 3, strlen(p_plus + 3) + 1);
        char *p_minus = strstr(scratch_substituted, "[-]");
        if (p_minus) memmove(p_minus, p_minus + 3, strlen(p_minus + 3) + 1);
    }

    /* Indentation Logic: Apply tab indentation to all descendants of an active menu */
    bool in_active_scope = (active_index != -1 && is_descendant(idx, active_index));
    if (in_active_scope) {
        int depth = 0;
        int p = el->parent_index;
        int safety = 0;
        while (p != -1 && p != active_index && safety < 100) {
            depth++;
            p = elements[p].parent_index;
            safety++;
        }
        for (int i = 0; i <= depth; i++) strcat(frame, "    ");
    }

    bool interactive = is_interactive(el);
    bool navigable = is_navigable(idx);
    int display_num = 0;

    if (interactive) {
        if (in_active_scope) {
            if (navigable) {
                (*p_scoped_counter)++;
                display_num = *p_scoped_counter;
            }
        } else {
            (*p_global_counter)++;
            display_num = *p_global_counter;
        }
    }

    bool is_focused = (idx == focus_index);
    bool is_active = (idx == active_index);
    char pref[4];
    if (is_active) strcpy(pref, "[^]");
    else if (is_focused && (active_index == -1 || navigable)) strcpy(pref, "[>]");
    else strcpy(pref, "[ ]");

    if (strcmp(el->type, "br") == 0) strcat(frame, "\n");
    else if (strcmp(el->type, "text") == 0) {
        strcat(frame, scratch_substituted);
    }
    else if (strcmp(el->type, "row") == 0) {
        if (strlen(scratch_substituted) > 0) {
            char *row_buf = malloc(MAX_LABEL_LEN + 128);
            if (row_buf) {
                snprintf(row_buf, MAX_LABEL_LEN + 128, "%s %-57s %s\n", BOX_V, scratch_substituted, BOX_V);
                strcat(frame, row_buf);
                free(row_buf);
            }
        }
    }
    else if (strcmp(el->type, "scroller") == 0) {
        char *line = NULL;
        if (display_num > 0)
            asprintf(&line, "%s %s %d. %-10s: %-33s %s", BOX_V, pref, display_num, el->id, scratch_substituted, BOX_V);
        else
            asprintf(&line, "%s %s %-10s: %-36s %s", BOX_V, pref, el->id, scratch_substituted, BOX_V);
        if (line) { strcat(frame, line); free(line); }
    }
    else if (strcmp(el->type, "cli_io") == 0) {
        char *line = NULL;
        const char* cli_prompt = get_var("cli_prompt");
        int is_password = (strcmp(cli_prompt, "password") == 0);

        if (is_active) {
            if (is_password) {
                char masked[256] = "";
                int mask_len = strlen(el->input_buffer);
                if (mask_len > 250) mask_len = 250;
                for (int i = 0; i < mask_len; i++) masked[i] = '*';
                masked[mask_len] = '\0';
                if (display_num > 0) asprintf(&line, "%s %s %d. [%s_]                                              %s", BOX_V, pref, display_num, masked, BOX_V);
                else asprintf(&line, "%s %s [%s_]                                                 %s", BOX_V, pref, masked, BOX_V);
            } else {
                if (display_num > 0) asprintf(&line, "%s %s %d. [%s_]                                              %s", BOX_V, pref, display_num, el->input_buffer, BOX_V);
                else asprintf(&line, "%s %s [%s_]                                                 %s", BOX_V, pref, el->input_buffer, BOX_V);
            }
        } else {
            const char* display_val = (strlen(el->input_buffer) > 0) ? el->input_buffer : scratch_substituted;
            if (display_num > 0) asprintf(&line, "%s %s %d. [%s]                                                 %s", BOX_V, pref, display_num, display_val, BOX_V);
            else asprintf(&line, "%s %s [%s]                                                    %s", BOX_V, pref, display_val, BOX_V);
        }
        if (line) { strcat(frame, line); free(line); }
    }
    else if (interactive) {
        char *line = NULL;
        if (display_num > 0) asprintf(&line, "%s %d. [%s]", pref, display_num, scratch_substituted);
        else asprintf(&line, "%s [%s]", pref, scratch_substituted);
        if (line) { strcat(frame, line); free(line); }
    }

    int saved_scoped = *p_scoped_counter;
    if (is_active && strcmp(el->onClick, "ACTIVATE") == 0) *p_scoped_counter = 0;

    bool ignore_fold = (strcmp(el->onClick, "ACTIVATE") == 0);
    for (int i = 0; i < el->num_children; i++) {
        if (ignore_fold || !el->is_folded) {
            render_element(el->children[i], frame, p_global_counter, p_scoped_counter);
        }
    }
    *p_scoped_counter = saved_scoped;
}

void compose_frame() {
    load_vars();
    const char* active_id = get_var("active_target_id");
    const char* methods_raw = get_var("piece_methods");

    bool active_changed = (strcmp(active_id, last_active_id) != 0);
    bool methods_changed = (strcmp(methods_raw, last_methods_raw) != 0);

    if (active_changed || methods_changed || wait_for_view_change) {
        if (active_changed) strncpy(last_active_id, active_id, 63);
        if (methods_changed) strncpy(last_methods_raw, methods_raw, MAX_VAR_VALUE - 1);

        parse_chtm(); 
        initialize_focus(); 
        wait_for_view_change = false;
    }

    export_active_index();

    char *frame = malloc(MAX_BUFFER); if (!frame) return; memset(frame, 0, MAX_BUFFER);
    int current_interactive = 0, scoped_interactive = 0; 
    for (int i = 0; i < element_count; i++) {
        if (elements[i].parent_index == -1) render_element(i, frame, &current_interactive, &scoped_interactive);
    }
    strcat(frame, "\n");
    strcat(frame, BOX_BL);
    for (int i = 0; i < 57; i++) strcat(frame, BOX_H);
    strcat(frame, BOX_BR);
    strcat(frame, "\n");
    char *nav_msg = NULL;
    if (active_index == -1) asprintf(&nav_msg, "Nav > %s_", nav_buffer);
    else asprintf(&nav_msg, "Active [^]: %s (ESC to exit)", nav_buffer);
    if (nav_msg) { strcat(frame, nav_msg); strcat(frame, "\n"); free(nav_msg); }
    
    char* cur_f = build_path_malloc("pieces/display/current_frame.txt");
    FILE *out_f = fopen(cur_f, "w");
    if (out_f) {
        fprintf(out_f, "%s", frame);
        fclose(out_f);
        char* renderer_pulse = build_path_malloc("pieces/display/renderer_pulse.txt");
        FILE *marker = fopen(renderer_pulse, "a"); if (marker) { fprintf(marker, "P\n"); fclose(marker); }
        free(renderer_pulse);
    }
    free(cur_f); free(frame); if (clear_nav_on_next) { nav_buffer[0] = '\0'; clear_nav_on_next = false; }
}

bool do_jump(int target_num) { int cn = 0; for (int i = 0; i < element_count; i++) { if (is_navigable(i)) { cn++; if (cn == target_num) { focus_index = i; return true; } } } return false; }
static int count_navigable() { int cn = 0; for (int i = 0; i < element_count; i++) { if (is_navigable(i)) cn++; } return cn; }
void initialize_focus() {
    const char* saved_idx_str = get_var("active_gui_index");
    if (saved_idx_str && strlen(saved_idx_str) > 0) {
        int saved_idx = atoi(saved_idx_str);
        if (saved_idx > 0) {
            for (int i = 0; i < element_count; i++) {
                if (elements[i].interactive_idx == saved_idx && is_navigable(i)) {
                    focus_index = i;
                    return;
                }
            }
        }
    }

    if (element_count > 0) { 
        for (int i = 0; i < element_count; i++) { 
            if (is_navigable(i)) { 
                focus_index = i; 
                return; 
            } 
        } 
    } 
}

void process_key(int key) {
    /* Set last_key variable for display in frame */
    char key_str[32] = "None";
    if (key >= 32 && key <= 126) {
        snprintf(key_str, sizeof(key_str), "%c", key);
    } else if (key == ARROW_UP) {
        strcpy(key_str, "UP");
    } else if (key == ARROW_DOWN) {
        strcpy(key_str, "DOWN");
    } else if (key == ARROW_LEFT) {
        strcpy(key_str, "LEFT");
    } else if (key == ARROW_RIGHT) {
        strcpy(key_str, "RIGHT");
    } else if (key == 10 || key == 13) {
        strcpy(key_str, "ENTER");
    }
    set_var("last_key", key_str);

    /* Normal nav mode */
    if (active_index == -1) {
        if (key == ARROW_UP || key == 'w' || key == 'W' || key == JOY_UP) { 
            int prev = focus_index; do { focus_index--; if (focus_index < 0) focus_index = element_count-1; } while (focus_index != prev && !is_navigable(focus_index)); 
            digit_accum = 0;  // Reset accumulator on navigation
            clear_nav_on_next = true; export_active_index();
        }
        else if (key == ARROW_DOWN || key == 's' || key == 'S' || key == JOY_DOWN) { 
            int prev = focus_index; do { focus_index++; if (focus_index >= element_count) focus_index = 0; } while (focus_index != prev && !is_navigable(focus_index)); 
            digit_accum = 0;  // Reset accumulator on navigation
            clear_nav_on_next = true; export_active_index();
        }
        else if (isdigit(key)) {
            int d = key - '0';
            int total_nav = count_navigable();
            int new_val = digit_accum * 10 + d;

            if (new_val > 0 && new_val <= total_nav) {
                // Valid jump target (single or multi-digit)
                digit_accum = new_val;
                if (do_jump(digit_accum)) {
                    snprintf(nav_buffer, sizeof(nav_buffer), "%d", digit_accum);
                    clear_nav_on_next = true;
                    export_active_index();
                }
                
                // If appending another digit would definitely exceed bounds, reset for next time
                if (digit_accum * 10 > total_nav) {
                    // But don't reset yet - user might want to press Enter to activate
                    // Actually, if we reset now, Enter won't know we have an accum.
                    // Let's reset only on non-digit keys.
                }
            } else {
                // Out of bounds: start over with the new digit if it's valid
                if (d > 0 && d <= total_nav) {
                    digit_accum = d;
                    if (do_jump(digit_accum)) {
                        snprintf(nav_buffer, sizeof(nav_buffer), "%d", digit_accum);
                        clear_nav_on_next = true;
                        export_active_index();
                    }
                } else {
                    digit_accum = 0;
                }
            }
        }
        else if (key == 10 || key == 13 || key == JOY_BUTTON_0) {
            // Execute accumulated value on Enter
            if (digit_accum > 0) {
                do_jump(digit_accum);
                digit_accum = 0;
                // Fall through to execute the button now focused
            }

            if (focus_index >= 0 && focus_index < element_count) {
                UIElement *el = &elements[focus_index];

                /* Native fold toggle */
                if (strstr(el->label, "[+]") || strstr(el->label, "[-]")) {
                    char saved_id[MAX_ATTR_LEN] = "";
                    if (strlen(el->id) > 0) strcpy(saved_id, el->id);

                    char var_name[MAX_ATTR_LEN + 64];
                    if (strlen(el->id) > 0) snprintf(var_name, sizeof(var_name), "fold_%s", el->id);
                    else {
                        char sanitized[64] = {0};
                        int j = 0;
                        for (int i = 0; el->label[i] && j < 63; i++) {
                            if (isalnum((unsigned char)el->label[i])) sanitized[j++] = el->label[i];
                        }
                        snprintf(var_name, sizeof(var_name), "fold_%s", sanitized);
                    }
                    
                    const char* state = get_var(var_name);
                    if (strcmp(state, "folded") == 0) save_to_gui_state(var_name, "open");
                    else save_to_gui_state(var_name, "folded");
                    
                    parse_chtm();
                    
                    /* Restore focus by ID */
                    if (strlen(saved_id) > 0) {
                        for (int i = 0; i < element_count; i++) {
                            if (strcmp(elements[i].id, saved_id) == 0) {
                                focus_index = i;
                                break;
                            }
                        }
                    }
                    if (!is_navigable(focus_index)) initialize_focus();
                    el = &elements[focus_index]; // Refresh pointer after re-parse
                }

                if (strcmp(el->type, "cli_io") == 0) {
                    /* CLI input field - activate on Enter */
                    active_index = focus_index;
                    clear_nav_on_next = true;
                    export_active_index();
                }
                else if (strcmp(el->onClick, "ACTIVATE") == 0) { 
                    active_index = focus_index; 
                    /* Focus first navigable child */
                    for (int i = 0; i < el->num_children; i++) {
                        if (is_navigable(el->children[i])) {
                            focus_index = el->children[i];
                            break;
                        }
                    }
                    clear_nav_on_next = true; 
                    export_active_index(); 
                }
                else if (strlen(el->href) > 0) { 
                    strncpy(current_layout, el->href, MAX_PATH-1); 
                    current_module_path[0] = '\0';  /* Force module re-launch on layout change */
                    current_module_pid = 0; 
                    active_index = -1;
                    focus_index = 0;
                    parse_chtm(); 
                    initialize_focus(); 
                    export_active_index(); 
                    compose_frame(); // Force immediate render on layout switch
                }
                else if (strcmp(el->onClick, "INTERACT") == 0) { active_index = focus_index; clear_nav_on_next = true; export_active_index(); }
                else if (strlen(el->onClick) > 0) { send_command(el->onClick); wait_for_view_change = true; clear_nav_on_next = true; }
            }
        }
 else if (key == 'q' || key == 'Q') exit(0);
        else {
            // Non-digit key resets accumulator
            digit_accum = 0;
        }
    } else {
        /* Active mode - copy reference pattern exactly */
        UIElement *el = &elements[active_index];
        
        /* KISS: ESC just deactivates, NEVER clears input */
        if (key == ESC_KEY || key == JOY_BUTTON_8) { 
            if (active_index != -1) {
                int old_active = active_index;
                int p = elements[active_index].parent_index;
                /* Find nearest ACTIVATE ancestor */
                while (p != -1 && strcmp(elements[p].onClick, "ACTIVATE") != 0) p = elements[p].parent_index;
                
                active_index = p;
                focus_index = old_active;
                if (active_index != -1 && !is_navigable(focus_index)) initialize_focus();
            }
            export_active_index();
        }
        else if (el->num_children > 0) {
            /* Activation Submenu Navigation */
            if (key == ARROW_UP || key == 'w' || key == 'W' || key == JOY_UP) { 
                int prev = focus_index; do { focus_index--; if (focus_index < 0) focus_index = element_count-1; } while (focus_index != prev && !is_navigable(focus_index)); 
                digit_accum = 0; clear_nav_on_next = true; export_active_index();
            }
            else if (key == ARROW_DOWN || key == 's' || key == 'S' || key == JOY_DOWN) { 
                int prev = focus_index; do { focus_index++; if (focus_index >= element_count) focus_index = 0; } while (focus_index != prev && !is_navigable(focus_index)); 
                digit_accum = 0; clear_nav_on_next = true; export_active_index();
            }
            else if (isdigit(key)) {
                int d = key - '0';
                int total_nav = count_navigable();
                int new_val = digit_accum * 10 + d;
                if (new_val > 0 && new_val <= total_nav) {
                    digit_accum = new_val;
                    if (do_jump(digit_accum)) {
                        snprintf(nav_buffer, sizeof(nav_buffer), "%d", digit_accum);
                        clear_nav_on_next = true;
                        export_active_index();
                    }
                } else if (d > 0 && d <= total_nav) {
                    digit_accum = d;
                    if (do_jump(digit_accum)) {
                        snprintf(nav_buffer, sizeof(nav_buffer), "%d", digit_accum);
                        clear_nav_on_next = true;
                        export_active_index();
                    }
                } else { digit_accum = 0; }
            }
            else if (key == 10 || key == 13 || key == JOY_BUTTON_0) {
                if (digit_accum > 0) { do_jump(digit_accum); digit_accum = 0; }
                if (focus_index >= 0 && focus_index < element_count) {
                    UIElement *child_el = &elements[focus_index];
                    if (child_el->num_children > 0 && strcmp(child_el->onClick, "ACTIVATE") == 0) {
                        active_index = focus_index;
                        for (int i = 0; i < child_el->num_children; i++) {
                            if (is_navigable(child_el->children[i])) {
                                focus_index = child_el->children[i];
                                break;
                            }
                        }
                    } else if (strlen(child_el->onClick) > 0) {
                        send_command(child_el->onClick);
                        wait_for_view_change = true;
                    }
                }
                clear_nav_on_next = true;
            }
        }
        else if (strcmp(el->type, "cli_io") == 0) {
            /* CLI text input - use element's input_buffer like reference */
            if (key == 10 || key == 13) { 
                if (strlen(el->input_buffer) > 0) {
                    /* Sync the input text to gui_state.txt so manager can read it */
                    save_to_gui_state("input_text", el->input_buffer);

                    /* Clear the input buffer of the cli_io element */
                    el->input_buffer[0] = '\0';

                    /* Trigger the Send action by dispatching KEY:13 */
                    inject_raw_key(13);

                    /* Ensure a render is triggered */
                    FILE *mf = fopen("pieces/display/frame_changed.txt", "a");
                    if (mf) { fprintf(mf, "E\n"); fclose(mf); }

                    /* STAY ACTIVE: Do NOT deactivate the input element */
                    export_active_index();
                } else {
                    /* Empty buffer, deactivate on second enter? 
                       Actually, let's keep it active for now or allow ESC to exit. */
                }            }
            else if (key == 127 || key == 8) { 
                /* Backspace */
                int len = strlen(el->input_buffer); 
                if (len > 0) {
                    el->input_buffer[len-1] = '\0';
                    save_to_gui_state("input_text", el->input_buffer);
                }
            }
            else if (key >= 32 && key <= 126) { 
                /* Printable char */
                int len = strlen(el->input_buffer); 
                if (len < sizeof(el->input_buffer) - 2) { 
                    el->input_buffer[len] = (char)key; 
                    el->input_buffer[len+1] = '\0';
                    
                    /* Live sync to gui_state.txt for UI visibility */
                    save_to_gui_state("input_text", el->input_buffer);

                    /* Append to cli_buffers.txt - simple and safe fallback */
                    FILE *bf = fopen("pieces/apps/player_app/cli_buffers.txt", "a");
                    if (bf) {
                        if (strstr(el->id, "username")) {
                            fprintf(bf, "U%s\n", el->input_buffer);
                        } else if (strstr(el->id, "password")) {
                            char masked[256] = "";
                            for (int i = 0; i <= len && i < 20; i++) strcat(masked, "*");
                            fprintf(bf, "P%s\n", masked);
                        } else if (strstr(el->id, "answer")) {
                            fprintf(bf, "A%s\n", el->input_buffer);
                        } else if (strlen(el->id) > 0) {
                            /* Generic fallback: First char of ID */
                            fprintf(bf, "%c%s\n", el->id[0], el->input_buffer);
                        }
                        fclose(bf);
                    }
                }
            }
        }
        else if (strcmp(el->onClick, "INTERACT") == 0) {
            int eff = key;
            if (key == ARROW_LEFT) eff = 1000; else if (key == ARROW_RIGHT) eff = 1001; else if (key == ARROW_UP) eff = 1002; else if (key == ARROW_DOWN) eff = 1003;
            else if (key >= JOY_BUTTON_0 && key <= JOY_BUTTON_8) eff = 2000 + (key - JOY_BUTTON_0);
            inject_raw_key(eff); if (key >= 32 && key <= 126) { nav_buffer[0] = (char)key; nav_buffer[1] = 0; } else if (key == 10 || key == 13) strcpy(nav_buffer, "ENTER");
            clear_nav_on_next = true;
        }
    }
    /* NAV MARKER: For ALL layouts, write the marker so compose_frame() fires 
     * through the same single-trigger path. 
     * DO NOT set dirty=1 from keyboard — this is the unified render path. */
    FILE *mf = fopen("pieces/display/frame_changed.txt", "a");
    if (mf) { fprintf(mf, "K\n"); fclose(mf); }
}

int main(int argc, char **argv) {
    resolve_root(); scratch_substituted = malloc(MAX_LABEL_LEN); if (argc > 1) strncpy(current_layout, argv[1], MAX_PATH-1);
    signal(SIGINT, handle_sigint); 
    active_index = -1;
    focus_index = 0;
    parse_chtm(); initialize_focus(); compose_frame();
    struct stat st;

    /* ═══════════════════════════════════════════════════════════════
     * RENDER TRIGGER — MARKER-DRIVEN, SINGLE SOURCE OF TRUTH
     * ═══════════════════════════════════════════════════════════════
     * compose_frame() ONLY fires when frame_changed.txt grows.
     * DO NOT add dirty=1 from keyboard, view, or state changes.
     * The marker file IS the throttle — it prevents redundant renders.
     *
     * Who writes the marker:
     *   Game layouts: render_map.c after deduped view update
     *   Nav layouts:  process_key() after every nav keypress
     *   Clock daemon: only when time is reactive
     *
     * Need a new render trigger? WRITE TO THE MARKER.
     * DO NOT set dirty=1 directly. That caused triple-rendering.
     * ═══════════════════════════════════════════════════════════════ */

    char *master_frame_ch = build_path_malloc("pieces/master_ledger/frame_changed.txt");
    char *display_frame_ch = build_path_malloc("pieces/display/frame_changed.txt");
    char *view_ch = build_path_malloc("pieces/apps/player_app/view_changed.txt");
    char *hist_p = build_path_malloc("pieces/keyboard/history.txt");
    char *layout_ch = build_path_malloc("pieces/display/layout_changed.txt");
    char *state_ch = build_path_malloc("pieces/apps/player_app/state_changed.txt");
    
    if (stat(master_frame_ch, &st) == 0) last_master_pulse_size = st.st_size;
    if (stat(display_frame_ch, &st) == 0) last_display_pulse_size = st.st_size;
    if (stat(view_ch, &st) == 0) last_view_file_size = st.st_size;
    if (stat(layout_ch, &st) == 0) last_layout_file_size = st.st_size;
    if (stat(state_ch, &st) == 0) last_state_file_size = st.st_size;

    while (1) {
#ifndef _WIN32
        int dirty = 0; if (current_module_pid > 0) { int status; if (waitpid(current_module_pid, &status, WNOHANG) != 0) { current_module_pid = -1; dirty = 1; } }
#else
        int dirty = 0;  /* Windows: No fork/waitpid support */
#endif
        FILE *history = fopen(hist_p, "r");
        if (history) {
            fseek(history, last_history_position, SEEK_SET);
            char line[200];
            static int last_raw_key = -1;
            while (fgets(line, sizeof(line), history)) {
                char *kp = strstr(line, "KEY_PRESSED: ");
                if (kp) {
                    char *colon = strchr(kp, ':');
                    if (colon) {
                        int key = atoi(colon + 1);
                        if (key > 0) {
                            /* Windows CRLF Debounce: Skip 10 if it follows 13, or 13 if it follows 10 */
                            if ((key == 10 && last_raw_key == 13) || (key == 13 && last_raw_key == 10)) {
                                last_raw_key = key;
                                continue;
                            }
                            last_raw_key = key;
                            process_key(key);
                        }
                    }
                }
            }
            last_history_position = ftell(history);
            fclose(history);
        }
        /* RENDER TRIGGER: ONLY when marker file grows.
         * DO NOT add: if (key) dirty=1, if (view_changed) dirty=1, etc.
         * All renders MUST go through the marker — keyboard → process_key() → marker → here → compose_frame().
         * Adding extra dirty=1 paths caused triple-rendering (3 compose_frame() calls per keypress). */
        if (stat(display_frame_ch, &st) == 0 && st.st_size > last_display_pulse_size) { last_display_pulse_size = st.st_size; dirty = 1; }
        if (stat(state_ch, &st) == 0 && st.st_size > last_state_file_size) {
            last_state_file_size = st.st_size;
            var_count = 0;
            load_vars();
            const char* new_active_id = get_var("active_target_id");
            if (strcmp(new_active_id, last_active_id) != 0) {
                strncpy(last_active_id, new_active_id, 63);
                active_index = -1;
                focus_index = 0;
                parse_chtm();
                initialize_focus();
            } else {
                /* Active target didn't change, but other state might have. 
                   Re-parse but preserve navigation state. */
                parse_chtm();
                if (focus_index >= element_count || !is_navigable(focus_index)) initialize_focus();
            }
            /* Auto-activate cli_io if cli_input has content (TPM app signal) */
            const char* cli = get_var("cli_input");
            if (cli && strlen(cli) > 0 && active_index == -1) {
                for (int i = 0; i < element_count; i++) {
                    if (strcmp(elements[i].type, "cli_io") == 0) {
                        active_index = i;
                        break;
                    }
                }
            }
            dirty = 1;
        }
        if (stat(layout_ch, &st) == 0 && st.st_size > last_layout_file_size) {
            last_layout_file_size = st.st_size; FILE *lf = fopen(layout_ch, "r");
            if (lf) { 
                char line[MAX_PATH]; char last_line[MAX_PATH] = "";
                while (fgets(line, sizeof(line), lf)) {
                    if (strlen(trim_pmo(line)) > 0) strcpy(last_line, trim_pmo(line));
                }
                if (strlen(last_line) > 0) {
                    strncpy(current_layout, last_line, MAX_PATH-1);
                    active_index = -1;
                    focus_index = 0;
                    parse_chtm(); initialize_focus(); dirty = 1;
                }
                fclose(lf); 
            }
        }
        if (dirty || clear_nav_on_next) { compose_frame(); dirty = 0; } usleep(16667);
    }
    return 0;
}
