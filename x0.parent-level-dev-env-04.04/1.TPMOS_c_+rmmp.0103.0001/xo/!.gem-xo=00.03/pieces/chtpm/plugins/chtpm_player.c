#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#define usleep(us) Sleep((us)/1000)
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#endif
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>

// TPM chtpm_player.c (v1.0 - SOVEREIGN RUNNER)
// Responsibility: Pure Theater. Parses layouts and bridges to Modules.

/* Cross-platform box-drawing characters - ASCII for consistent display */
#define BOX_TL "+"
#define BOX_TR "+"
#define BOX_BL "+"
#define BOX_BR "+"
#define BOX_H "="
#define BOX_V "|"
#define BOX_ROW_PREFIX "| "
#define BOX_ROW_SUFFIX " |"

#define MAX_LINE 4096
#define MAX_VAR_NAME 64
#define MAX_VAR_VALUE 65536
#define MAX_VARS 500
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
    int parent_index; int children[MAX_CHILDREN]; int num_children;
    bool visibility; int interactive_idx;
} UIElement;

char current_layout[MAX_PATH] = "pieces/chtpm/layouts/player_test.chtpm";
int focus_index = 0; int active_index = -1; 
int element_count = 0; UIElement elements[MAX_ELEMENTS];
char nav_buffer[512] = {0};
bool clear_nav_on_next = false;
bool is_time_reactive = false;

typedef struct { char name[MAX_VAR_NAME]; char value[MAX_VAR_VALUE]; } Variable;
Variable vars[MAX_VARS]; int var_count = 0;
long last_history_position = 0; 
long last_master_pulse_size = 0;
long last_display_pulse_size = 0;
long last_view_file_size = 0;
long last_layout_file_size = 0;

// Module IPC
pid_t child_module_pid = -1;
int module_in[2];  // Runner -> Module
int module_out[2]; // Module -> Runner

char* scratch_substituted = NULL;
char project_root_path[MAX_PATH] = ".";

// Forward Decls
void parse_chtm(void);
void compose_frame(void);
void substitute_vars(const char* src, char* dst, int max_len);
void set_var(const char* name, const char* value);
const char* get_var(const char* name);
bool is_navigable(int idx);
void initialize_focus(void);
void cleanup_module(void);
char* read_file_to_string(const char* filename);
void send_command(const char* cmd);
bool evaluate_visibility(UIElement* el);
bool is_interactive(UIElement* el);
void parse_attributes(UIElement* el, const char* attr_str);
void render_element(int idx, char* frame, int* p_current_interactive);
char* build_path_malloc(const char* rel);
char* trim_pmo(char *str);

// --- Utilities ---

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
    asprintf(&p, "%s/%s", project_root_path, rel);
    return p;
}

void resolve_root() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[4096];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                strncpy(project_root_path, trim_pmo(v), MAX_PATH - 1); 
                break;
            }
        }
        fclose(kvp);
    }
}

void cleanup_module() { 
    if (child_module_pid > 0) { 
#ifndef _WIN32
        kill(child_module_pid, SIGTERM); 
        waitpid(child_module_pid, NULL, WNOHANG); 
#endif
        child_module_pid = -1; 
    } 
}

void handle_sigint(int sig __attribute__((unused))) { 
    cleanup_module(); 
    if (scratch_substituted) free(scratch_substituted); 
    exit(0); 
}

void set_var(const char* name, const char* value) {
    for (int i = 0; i < var_count; i++) { 
        if (strcmp(vars[i].name, name) == 0) { 
            strncpy(vars[i].value, value, MAX_VAR_VALUE - 1); 
            return; 
        } 
    }
    if (var_count < MAX_VARS) { 
        strncpy(vars[var_count].name, name, MAX_VAR_NAME - 1); 
        strncpy(vars[var_count].value, value, MAX_VAR_VALUE - 1); 
        var_count++; 
    }
}

const char* get_var(const char* name) { 
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].value; 
    }
    return ""; 
}

void substitute_vars(const char* src, char* dst, int max_len) {
    const char *p_src = src; char *p_dst = dst;
    while (*p_src && (p_dst - dst) < max_len - 1) {
        if (*p_src == '$' && *(p_src+1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64]; int len = end - (p_src + 2); if (len > 63) len = 63;
                strncpy(var_name, p_src + 2, len); var_name[len] = '\0';
                const char *val = get_var(var_name);
                while (*val && (p_dst - dst) < max_len - 1) *p_dst++ = *val++;
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

void handle_module_output() {
#ifndef _WIN32
    char line[MAX_BUFFER];
    ssize_t n = read(module_out[0], line, sizeof(line)-1);
    if (n > 0) {
        line[n] = '\0';
        if (strncmp(line, "VARS:", 5) == 0) {
            char *p = line + 5;
            char *pair = strtok(p, ",;");
            while (pair) {
                char *eq = strchr(pair, '=');
                if (eq) {
                    *eq = '\0';
                    set_var(trim_pmo(pair), trim_pmo(eq + 1));
                }
                pair = strtok(NULL, ",;");
            }
        } else {
            // Treat as DISPLAY content
            set_var("module_display", line);
        }
    }
#endif
}

void launch_module(const char* path) {
    cleanup_module();
#ifndef _WIN32
    pipe(module_in); pipe(module_out);
    child_module_pid = fork();
    if (child_module_pid == 0) {
        dup2(module_in[0], STDIN_FILENO);
        dup2(module_out[1], STDOUT_FILENO);
        close(module_in[1]); close(module_out[0]);
        char* abs_path = (path[0] == '/') ? strdup(path) : build_path_malloc(path);
        execl(abs_path, abs_path, NULL);
        exit(1);
    }
    close(module_in[0]); close(module_out[1]);
    // Set non-blocking for stdout
    int flags = fcntl(module_out[0], F_GETFL, 0);
    fcntl(module_out[0], F_SETFL, flags | O_NONBLOCK);
#else
    char* abs_path = (path[0] == '/') ? strdup(path) : build_path_malloc(path);
    if (abs_path) {
        // Run as simple system command (blocking on Windows for now)
        system(abs_path);
        free(abs_path);
    }
#endif
}

void send_to_module(const char* fmt, ...) {
    if (child_module_pid <= 0) return;
    char buf[MAX_LINE];
    va_list args; va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
#ifndef _WIN32
    write(module_in[1], buf, strlen(buf));
    write(module_in[1], "\n", 1);
#endif
}

void send_command(const char* cmd) {
    if (strncmp(cmd, "KEY:", 4) == 0) {
        send_to_module("%s", cmd);
    } else {
        // Fallback or generic message
        send_to_module("CMD:%s", cmd);
    }
}

bool evaluate_visibility(UIElement* el) {
    if (el->visibility_expr[0] == '\0') return true;
    char substituted[512]; substitute_vars(el->visibility_expr, substituted, 512);
    char *s = trim_pmo(substituted);
    return (strcmp(s, "true") == 0 || strcmp(s, "1") == 0);
}

bool is_interactive(UIElement* el) { return (strcmp(el->type, "button") == 0 || strcmp(el->type, "canvas") == 0); }
bool is_navigable(int idx) { 
    if (idx < 0 || idx >= element_count) return false; 
    UIElement* el = &elements[idx]; 
    return is_interactive(el) && evaluate_visibility(el); 
}

void parse_attributes(UIElement* el, const char* attr_str) {
    if (!attr_str) return; 
    char* attrs = strdup(attr_str); char* pos = attrs;
    while (*pos) {
        while (*pos && isspace((unsigned char)*pos)) pos++; if (!*pos) break;
        char* name_start = pos; while (*pos && *pos != '=' && !isspace((unsigned char)*pos)) pos++;
        char saved = *pos; *pos = '\0'; while (*(++pos) && isspace((unsigned char)*pos));
        if (*pos == '=') { pos++; while (*pos && isspace((unsigned char)*pos)) pos++; }
        char* val_start = pos;
        if (*pos == '"' || *pos == '\'') { char quote = *pos++; val_start = pos; while (*pos && *pos != quote) pos++; if (*pos) *pos++ = '\0'; }
        else { while (*pos && !isspace((unsigned char)*pos) && *pos != '/') pos++; if (*pos) *pos++ = '\0'; }
        
        if (strcmp(name_start, "label") == 0) strncpy(el->label, val_start, MAX_LABEL_LEN - 1);
        else if (strcmp(name_start, "href") == 0) strncpy(el->href, val_start, MAX_PATH - 1);
        else if (strcmp(name_start, "onClick") == 0) strncpy(el->onClick, val_start, 127);
        else if (strcmp(name_start, "id") == 0) strncpy(el->id, val_start, MAX_ATTR_LEN - 1);
        else if (strcmp(name_start, "visibility") == 0) strncpy(el->visibility_expr, val_start, MAX_ATTR_LEN - 1);
        else if (strcmp(name_start, "time_reactive") == 0) is_time_reactive = (strcmp(val_start, "true") == 0);
        
        *(pos - 1) = saved;
    }
    free(attrs);
}

typedef enum { TOKEN_TEXT, TOKEN_OPEN_TAG, TOKEN_CLOSE_TAG, TOKEN_SELFCLOSE_TAG } ChtpmTokenType;
typedef struct { ChtpmTokenType type; char content[MAX_LABEL_LEN]; char tag_name[64]; char attributes[512]; } Token;

Token* tokenize(const char* content, int* token_count) {
    Token* tokens = malloc(MAX_ELEMENTS * 4 * sizeof(Token)); *token_count = 0; const char* cursor = content;
    while (*cursor && *token_count < MAX_ELEMENTS * 4) {
        const char* tag_start = strchr(cursor, '<');
        if (!tag_start) { 
            Token* t = &tokens[(*token_count)++]; t->type = TOKEN_TEXT; 
            strncpy(t->content, cursor, MAX_LABEL_LEN-1); break; 
        }
        if (tag_start > cursor) { 
            Token* t = &tokens[(*token_count)++]; t->type = TOKEN_TEXT; 
            int len = tag_start - cursor; if (len > MAX_LABEL_LEN-1) len = MAX_LABEL_LEN-1; 
            strncpy(t->content, cursor, len); t->content[len] = '\0'; 
        }
        const char* tag_end = strchr(tag_start, '>'); if (!tag_end) break;
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
    char* content = read_file_to_string(current_layout); if (!content) return;
    int tc; Token* tokens = tokenize(content, &tc); free(content);
    element_count = 0; int stack[50], top = -1, current_interactive = 0;
    for (int i = 0; i < tc && element_count < MAX_ELEMENTS; i++) {
        Token* t = &tokens[i];
        if (t->type == TOKEN_TEXT) {
            UIElement* el = &elements[element_count++]; memset(el, 0, sizeof(UIElement));
            strcpy(el->type, "text"); strncpy(el->label, t->content, MAX_LABEL_LEN - 1); 
            el->parent_index = (top >= 0) ? stack[top] : -1;
        } else if (t->type == TOKEN_OPEN_TAG || t->type == TOKEN_SELFCLOSE_TAG) {
            if (strcmp(t->tag_name, "module") == 0) {
                if (i + 1 < tc && tokens[i+1].type == TOKEN_TEXT) { launch_module(trim_pmo(tokens[i+1].content)); }
                continue;
            }
            if (strcmp(t->tag_name, "panel") == 0) { 
                UIElement dummy; parse_attributes(&dummy, t->attributes);
                if (t->type == TOKEN_OPEN_TAG) stack[++top] = -1; continue; 
            }
            UIElement* el = &elements[element_count++]; memset(el, 0, sizeof(UIElement));
            strncpy(el->type, t->tag_name, 31); parse_attributes(el, t->attributes);
            el->parent_index = (top >= 0) ? stack[top] : -1; if (is_interactive(el)) el->interactive_idx = ++current_interactive;
            if (el->parent_index != -1) { UIElement* pa = &elements[el->parent_index]; if (pa->num_children < MAX_CHILDREN) pa->children[pa->num_children++] = element_count - 1; }
            if (t->type == TOKEN_OPEN_TAG) stack[++top] = element_count - 1;
        } else if (t->type == TOKEN_CLOSE_TAG) { if (top >= 0) top--; }
    }
    free(tokens);
}

void render_element(int idx, char* frame, int* p_current_interactive) {
    if (idx < 0 || idx >= element_count) return; 
    UIElement* el = &elements[idx]; if (!evaluate_visibility(el)) return;
    substitute_vars(el->label, scratch_substituted, MAX_LABEL_LEN);
    if (strcmp(el->type, "br") == 0) strcat(frame, "\n");
    else if (strcmp(el->type, "text") == 0) strcat(frame, scratch_substituted);
    else if (is_interactive(el)) {
        (*p_current_interactive)++; char *line = NULL; 
        bool is_focused = (active_index == -1 && idx == focus_index);
        if (idx == active_index) asprintf(&line, "[^] %d. [%s] ", *p_current_interactive, scratch_substituted);
        else if (is_focused) asprintf(&line, "[>] %d. [%s] ", *p_current_interactive, scratch_substituted);
        else asprintf(&line, "[ ] %d. [%s] ", *p_current_interactive, scratch_substituted);
        if (line) { strcat(frame, line); free(line); }
    }
    for (int i = 0; i < el->num_children; i++) render_element(el->children[i], frame, p_current_interactive);
}

void compose_frame() {
    char *frame = malloc(MAX_BUFFER); if (!frame) return; memset(frame, 0, MAX_BUFFER);
    int current_interactive = 0; for (int i = 0; i < element_count; i++) if (elements[i].parent_index == -1) render_element(i, frame, &current_interactive);
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
        fprintf(out_f, "%s", frame); fclose(out_f);
        char* renderer_pulse = build_path_malloc("pieces/display/renderer_pulse.txt");
        FILE *marker = fopen(renderer_pulse, "a"); if (marker) { fprintf(marker, "P\n"); fclose(marker); }
        free(renderer_pulse);
    }
    free(cur_f); free(frame); if (clear_nav_on_next) { nav_buffer[0] = '\0'; clear_nav_on_next = false; }
}

void initialize_focus() { if (element_count > 0) { for (int i = 0; i < element_count; i++) { if (is_navigable(i)) { focus_index = i; return; } } } }

void process_key(int key) {
    if (active_index == -1) {
        if (key == ARROW_UP || key == 'w' || key == 'W' || key == JOY_UP) { 
            int prev = focus_index; do { focus_index--; if (focus_index < 0) focus_index = element_count-1; } while (focus_index != prev && !is_navigable(focus_index)); clear_nav_on_next = true; 
        }
        else if (key == ARROW_DOWN || key == 's' || key == 'S' || key == JOY_DOWN) { 
            int prev = focus_index; do { focus_index++; if (focus_index >= element_count) focus_index = 0; } while (focus_index != prev && !is_navigable(focus_index)); clear_nav_on_next = true; 
        }
        else if (key == 10 || key == 13 || key == JOY_BUTTON_0) {
            if (focus_index >= 0 && focus_index < element_count) {
                UIElement *el = &elements[focus_index];
                if (strlen(el->href) > 0) { strncpy(current_layout, el->href, MAX_PATH-1); parse_chtm(); focus_index = 0; initialize_focus(); }
                else if (strlen(el->onClick) > 0) { send_command(el->onClick); clear_nav_on_next = true; }
            }
        } else if (key == 'q' || key == 'Q') {
            exit(0); // Exit sovereign app and return to OS
        }
    } else {
        if (key == ESC_KEY || key == JOY_BUTTON_8) active_index = -1;
        else send_to_module("KEY:%d", key);
    }
}

int main(int argc, char **argv) {
    resolve_root(); scratch_substituted = malloc(MAX_LABEL_LEN);
    if (argc > 1) strncpy(current_layout, argv[1], MAX_PATH-1);
    signal(SIGINT, handle_sigint); parse_chtm(); initialize_focus(); compose_frame();
    struct stat st; 
    char *master_frame_ch = build_path_malloc("pieces/master_ledger/frame_changed.txt");
    char *display_frame_ch = build_path_malloc("pieces/display/frame_changed.txt");
    char *hist_p = build_path_malloc("pieces/keyboard/history.txt");
    char *layout_ch = build_path_malloc("pieces/display/layout_changed.txt");
    
    if (stat(master_frame_ch, &st) == 0) last_master_pulse_size = st.st_size;
    if (stat(display_frame_ch, &st) == 0) last_display_pulse_size = st.st_size;
    if (stat(layout_ch, &st) == 0) last_layout_file_size = st.st_size;

    while (1) {
        int dirty = 0; 
        handle_module_output(); // Check for module updates
        
        FILE *history = fopen(hist_p, "r");
        if (history) { 
            fseek(history, last_history_position, SEEK_SET); char line[200]; 
            while (fgets(line, sizeof(line), history)) { 
                if (strstr(line, "KEY_PRESSED: ")) { 
                    int key = atoi(strstr(line, "KEY_PRESSED: ") + 13); if (key > 0) { process_key(key); dirty = 1; } 
                } 
            } 
            last_history_position = ftell(history); fclose(history); 
        }
        
        if (stat(master_frame_ch, &st) == 0 && st.st_size > last_master_pulse_size) {
            last_master_pulse_size = st.st_size; if (is_time_reactive) dirty = 1; 
        }
        if (stat(display_frame_ch, &st) == 0 && st.st_size > last_display_pulse_size) {
            last_display_pulse_size = st.st_size; dirty = 1; 
        }
        if (stat(layout_ch, &st) == 0 && st.st_size > last_layout_file_size) {
            last_layout_file_size = st.st_size; FILE *lf = fopen(layout_ch, "r");
            if (lf) { 
                fseek(lf, -MAX_PATH, SEEK_END); char new_l[MAX_PATH]; 
                if (fgets(new_l, sizeof(new_l), lf)) { strncpy(current_layout, trim_pmo(new_l), MAX_PATH-1); parse_chtm(); initialize_focus(); dirty = 1; } 
                fclose(lf); 
            }
        }

        if (dirty || clear_nav_on_next) { compose_frame(); dirty = 0; }
        usleep(16667);
    }
    return 0;
}
