// agent.c - TPMOS CLI Agent Core (Strict JSON Escaping + Debug Fallback)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>

static volatile sig_atomic_t action_interrupted = 0;
static volatile sig_atomic_t do_resize = 0;
static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t spinner_active = 0;
static double spinner_start = 0;
static pthread_t spinner_tid;
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static FILE* debug_fp = NULL;
static int feat_completion = 1;
static int feat_summarize = 1;
static struct termios orig_termios;
static char current_api_url[256] = "https://generativelanguage.googleapis.com";
static char current_model[256] = "llama3:latest";
static char* last_response = NULL;

void sig_handler(int sig) { 
    if (sig == SIGWINCH) do_resize = 1;
    else {
        action_interrupted = 1; 
        spinner_active = 0; 
    }
}

void debug_print(const char* fmt, ...) {
    if (!debug_fp) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(debug_fp, fmt, args);
    va_end(args);
    fflush(debug_fp);
}

static void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static char* run_tool(const char* tool, char* const args[]) {
    debug_print("[DEBUG] Forking: %s", tool);
    for (int i = 1; args && args[i]; i++) debug_print(" %s", args[i]);
    debug_print("\n");
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) { 
        close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO); 
        // Do NOT dup stderr here, so we can see it in our own terminal or ignore it
        close(pipefd[1]); 
        execvp(tool, args); 
        _exit(127); 
    }
    close(pipefd[1]);
    int status = 0; 
    char* output = malloc(ctx_limit);
    size_t total = 0;
    while (1) {
        char buf[1024];
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (total + n < ctx_limit) {
            memcpy(output + total, buf, n);
            total += n;
        }
    }
    output[total] = '\0';
    close(pipefd[0]);
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // Optional: handle error status
    }

    size_t len = strlen(output); while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) output[--len] = '\0';
    return output;
}

static void clear_dropdown() {
    // Move down 1, clear line, move back up 1, CR
    printf("\033[s\033[1B\r\033[K\033[u");
    fflush(stdout);
}


static int get_term_rows() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) return w.ws_row;
    return 24;
}

static void draw_header() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) cwd[0] = '\0';
    // Box-drawing top bar
    printf("\033[H\033[36m┌──────────────────────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[36m│\033[0m \033[1;37mAPI:\033[0m %s \033[1;37mModel:\033[0m %s \033[1;37mPath:\033[0m %s \033[36m│\033[0m\n", current_api_url, current_model, cwd);
    printf("\033[36m└──────────────────────────────────────────────────────────────────────────┘\033[0m\n");
}

static void draw_log_area() {
    // Log area is handled by standard printing, but we demarcate it visually
    printf("\n\033[90m--- [AIDA LOG] ---\033[0m\n");
    if (last_response) {
        printf("\033[37m%s\033[0m\n", last_response);
    }
}

static void draw_prompt_area(int pct, const char* line) {
    int rows = get_term_rows();
    // Top border (Line N-2)
    printf("\033[%d;1H\033[36m┌──────────────────────────────────────────────────────────────────────────┐\033[0m", rows - 2);
    // Prompt line (Line N-1)
    // Prefix length: 17 chars ("│ [  3%] Aida >> ")
    printf("\033[%d;1H\033[36m│\033[0m [%s%3d%%\033[0m] \033[92mAida >> \033[0m%-57.56s", rows - 1, 
           pct < 75 ? "\033[92m" : "\033[91m", pct, line);
    printf("\033[%d;76H\033[36m│\033[0m", rows - 1);
    // Bottom border (Line N)
    printf("\033[%d;1H\033[36m└──────────────────────────────────────────────────────────────────────────┘\033[0m", rows);
}

static void draw_suggestions(const char* suggestions, int selected_idx, const char* sub_suggestions, int rows) {
    if (suggestions && strlen(suggestions) > 0) {
        printf("\033[%d;1H\033[K\033[90m", rows - 3); // Position suggestions above the prompt box
        printf("[tab] ");
        
        char* copy = strdup(suggestions);
        char* token = strtok(copy, "  ");
        int i = 0;
        while (token) {
            if (i == selected_idx) printf("\033[7m %s \033[27m ", token);
            else printf(" %s  ", token);
            token = strtok(NULL, "  ");
            i++;
            if (i > 8) { printf("..."); break; }
        }
        
        if (sub_suggestions && strlen(sub_suggestions) > 0) {
            printf("\n\033[90m       ↳ %s", sub_suggestions);
        }
        printf("\033[0m");
        free(copy);
    }
}

static void log_session_history(const char* user, const char* assistant) {
    FILE* f = fopen("session-history.txt", "a");
    if (!f) return;
    fprintf(f, "[PROMPT]\n%s\n\n[RESPONSE]\n%s\n\n----------------------------------------\n\n", user, assistant);
    fclose(f);
}

static void refresh_ui(int pct, const char* line, const char* suggestions, int selected_idx, const char* sub_suggestions) {
    printf("\033[?25l"); // Hide cursor during redraw
    printf("\033[H\033[2J"); // Full clear
    draw_header();
    draw_log_area();
    draw_prompt_area(pct, line);
    draw_suggestions(suggestions, selected_idx, sub_suggestions, get_term_rows());
    
    // Explicitly place cursor at the end of the input line (Line rows-1)
    // Only show the cursor if we are NOT currently selecting a suggestion
    if (selected_idx == -1) {
        int rows = get_term_rows();
        printf("\033[%d;%zuH", rows - 1, (size_t)(18 + strlen(line)));
        printf("\033[?25h"); 
    }
    
    fflush(stdout);
}

static char* get_line_interactive(int pct) {
    char* line = calloc(1024, 1);
    size_t len = 0;
    char* current_suggestions = NULL;
    int selected_idx = -1;
    
    refresh_ui(pct, line, NULL, -1, NULL);

    enable_raw_mode();
    while (keep_running) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;
        
        if (c == 27) { // Escape sequence
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;
            if (seq[0] == '[' && seq[1] == 'C') { // Right Arrow
                if (selected_idx != -1 && current_suggestions) {
                    char* copy = strdup(current_suggestions);
                    char* token = strtok(copy, "  ");
                    for (int i = 0; i < selected_idx; i++) token = strtok(NULL, "  ");
                    if (token) {
                        char* last_word = strrchr(line, ' ');
                        if (!last_word) last_word = line; else last_word++;
                        if (last_word[0] == '@') {
                            line[last_word - line + 1] = '\0';
                            strcat(line, token);
                            len = strlen(line);
                        }
                    }
                    free(copy);
                    selected_idx = -1;
                    if (current_suggestions) { free(current_suggestions); current_suggestions = NULL; }
                }
            }
            refresh_ui(pct, line, current_suggestions, selected_idx, NULL);
            continue;
        }

        if (c == 13 || c == 10) { // Enter
            int bytes_waiting = 0;
            ioctl(STDIN_FILENO, FIONREAD, &bytes_waiting);
            if (bytes_waiting > 0) {
                // Paste detected: treat newline as content
                if (len < 1023) {
                    line[len++] = '\n'; line[len] = '\0';
                }
                continue;
            }

            if (selected_idx != -1 && current_suggestions) {
                // COMMIT SELECTION
                char* copy = strdup(current_suggestions);
                char* token = strtok(copy, "  ");
                for (int i = 0; i < selected_idx; i++) token = strtok(NULL, "  ");
                if (token) {
                    char* last_word = strrchr(line, ' ');
                    if (!last_word) last_word = line; else last_word++;
                    if (last_word[0] == '@') {
                        line[last_word - line + 1] = '\0';
                        strcat(line, token);
                        len = strlen(line);
                    }
                }
                free(copy);
                selected_idx = -1;
                if (current_suggestions) { free(current_suggestions); current_suggestions = NULL; }
                refresh_ui(pct, line, NULL, -1, NULL);
                continue; // Stay in the interactive loop
            }
            // ELSE SEND MESSAGE
            printf("\r\033[J[%s%d%%\033[0m] \033[92mAida >> \033[0m%s\n", pct < 75 ? "\033[92m" : "\033[91m", pct, line);
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (len > 0) {
                line[--len] = '\0';
                selected_idx = -1;
            }
        } else if (c == 9) { // Tab
            if (feat_completion && current_suggestions && strlen(current_suggestions) > 0) {
                int count = 0;
                char* p = current_suggestions;
                while (p) {
                    count++;
                    p = strstr(p, "  ");
                    if (p) p += 2;
                }
                selected_idx = (selected_idx + 1) % count;
            }
        } else if (c == 3) { // Ctrl+C
            line[0] = '\0'; len = 0;
            printf("\n");
            refresh_ui(pct, line, NULL, -1, NULL);
            continue;
        } else if (c == 4) { // Ctrl+D
            if (len == 0) { free(line); disable_raw_mode(); return NULL; }
        } else if (c >= 32 && c <= 126) {
            if (c == '/' && selected_idx != -1 && current_suggestions) {
                // AUTO-COMMIT ON SLASH
                char* copy = strdup(current_suggestions);
                char* token = strtok(copy, "  ");
                for (int i = 0; i < selected_idx; i++) token = strtok(NULL, "  ");
                if (token) {
                    char* last_word = strrchr(line, ' ');
                    if (!last_word) last_word = line; else last_word++;
                    if (last_word[0] == '@') {
                        line[last_word - line + 1] = '\0';
                        strcat(line, token);
                        if (line[strlen(line)-1] != '/') strcat(line, "/");
                        len = strlen(line);
                    }
                }
                free(copy);
                selected_idx = -1;
            } else {
                if (len < 1023) {
                    line[len++] = c; line[len] = '\0';
                    selected_idx = -1;
                }
            }
        }

        // Pulse and Refresh
        char* sub_suggestions = NULL;
        if (feat_completion) {
            char* last_word = strrchr(line, ' ');
            if (!last_word) last_word = line; else last_word++;
            
            if (last_word[0] == '@') {
                char* prefix = last_word + 1;
                char* comp_args[] = {"./tools/complete_path", prefix, NULL};
                char* new_sug = run_tool(comp_args[0], comp_args);
                if (current_suggestions) free(current_suggestions);
                current_suggestions = new_sug;

                if (selected_idx != -1 && current_suggestions) {
                    char* copy = strdup(current_suggestions);
                    char* tok = strtok(copy, "  ");
                    for (int i = 0; i < selected_idx; i++) tok = strtok(NULL, "  ");
                    if (tok && tok[strlen(tok)-1] == '/') {
                        char* sub_args[] = {"./tools/complete_path", tok, NULL};
                        sub_suggestions = run_tool(sub_args[0], sub_args);
                    }
                    free(copy);
                }
            } else {
                if (current_suggestions) { free(current_suggestions); current_suggestions = NULL; }
                selected_idx = -1;
            }
        }
        refresh_ui(pct, line, current_suggestions, selected_idx, sub_suggestions);
        if (sub_suggestions) free(sub_suggestions);
    }
    if (current_suggestions) free(current_suggestions);
    disable_raw_mode();
    return line;
}



void* spinner_thread(void* arg) {
    (void)arg;
    const char* chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
    int idx = 0;
    while(spinner_active) {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        double elapsed = (ts.tv_sec + ts.tv_nsec/1e9) - spinner_start;
        fprintf(stderr, "\r\033[90m%c Thinking... (%.1fs)\033[0m", chars[idx % 10], elapsed);
        fflush(stderr); idx++; usleep(100000);
    }
    fprintf(stderr, "\r%50s\r", ""); fflush(stderr); return NULL;
}

void start_spinner() { spinner_active = 1; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); spinner_start = ts.tv_sec + ts.tv_nsec/1e9; pthread_create(&spinner_tid, NULL, spinner_thread, NULL); }
static void stop_spinner() { spinner_active = 0; pthread_join(spinner_tid, NULL); }

static void log_session_frame(const char* user, const char* assistant) {

    FILE* f = fopen("debug/session.log", "a");
    if (!f) return;
    fprintf(f, "[USER]\n%s\n\n[AIDA]\n%s\n\n========================================\n\n", user, assistant);
    fclose(f);
}

static char* make_path(const char* base, const char* file) { char* out = NULL; if (asprintf(&out, "%s/%s", base, file) == -1) return NULL; return out; }

static char* get_gemini_api_key() {
    char* key_env = getenv("GEMINI_API_KEY");
    if (key_env && strlen(key_env) > 0) {
        return strdup(key_env);
    }
    FILE* f = fopen("../!.google-api-py-00.01/google-lilsol-api-key.txt", "r");
    if (!f) {
        f = fopen("google-lilsol-api-key.txt", "r");
    }
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

static void resolve_local_model() {
    FILE* f = fopen("config/model.txt", "r");
    if (f) {
        if (fgets(current_model, sizeof(current_model), f)) {
            char* p = current_model;
            while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
            if (p != current_model) {
                memmove(current_model, p, strlen(p) + 1);
            }
            size_t len = strlen(current_model);
            while (len > 0 && (current_model[len-1] == ' ' || current_model[len-1] == '\t' || current_model[len-1] == '\r' || current_model[len-1] == '\n')) {
                current_model[len-1] = '\0';
                len--;
            }
        }
        fclose(f);
    }
    if (strlen(current_model) == 0 || strncmp(current_model, "gemini-", 7) != 0) {
        strncpy(current_model, "gemini-2.5-flash", sizeof(current_model) - 1);
        current_model[sizeof(current_model) - 1] = '\0';
    }
    printf("\033[90m[System] Resolved Model: %s\033[0m\n", current_model);
}

static void handle_stream_chunk(const char* chunk, char** acc_text, char** acc_func) {
    const char* p = chunk;
    
    // Extract text content
    const char* text_ptr = strstr(p, "\"text\": \"");
    if (text_ptr) {
        text_ptr += 9;
        const char* start = text_ptr;
        while (*text_ptr && (*text_ptr != '"' || (*(text_ptr-1) == '\\'))) {
            if (*text_ptr == '\\' && *(text_ptr+1)) {
                text_ptr++;
                if (*text_ptr == 'n') printf("\n");
                else if (*text_ptr == 't') printf("\t");
                else if (*text_ptr == 'r') printf("\r");
                else if (*text_ptr == '"') printf("\"");
                else if (*text_ptr == '\\') printf("\\");
                else printf("%c", *text_ptr);
                text_ptr++;
            } else {
                printf("%c", *text_ptr++);
            }
        }
        size_t len = text_ptr - start;
        size_t old_len = *acc_text ? strlen(*acc_text) : 0;
        *acc_text = realloc(*acc_text, old_len + len + 1);
        memcpy(*acc_text + old_len, start, len);
        (*acc_text)[old_len + len] = '\0';
        fflush(stdout);
    }
    
    // Extract functionCall (only if not already found)
    if (!*acc_func) {
        const char* f = strstr(p, "\"functionCall\":");
        if (f) {
            f += 15; while (*f && *f != '{') f++;
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

static int check_permission(const char* tool_name, const char* details) {
    if (access("config/yolo.flag", F_OK) == 0) return 1;
    printf("\n\033[95m⚠️ [PERMISSION REQUEST] Aida wants to execute: \033[1m%s\033[0m\n", tool_name);
    printf("\033[90m└ Details: %s\033[0m\n", details);
    printf("\033[93mAllow execution? (y/n): \033[0m");
    fflush(stdout);

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    struct termios cooked = raw;
    cooked.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &cooked);
    
    char choice = 'n';
    if (read(STDIN_FILENO, &choice, 1) <= 0) choice = 'n';
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\n");
    return (choice == 'y' || choice == 'Y');
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0); setvbuf(stdin, NULL, _IONBF, 0);
    struct sigaction sa; sa.sa_handler = sig_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL); sigaction(SIGWINCH, &sa, NULL);
    mkdir("state", 0755); mkdir("config", 0755); mkdir("debug", 0755);

    debug_fp = fopen("debug/debug.txt", "w");
    if (!debug_fp) perror("Warning: Could not open debug/debug.txt");

    FILE* sf_init = fopen("debug/session.log", "w");
    if (sf_init) fclose(sf_init);
    FILE* sh_init = fopen("session-history.txt", "w");
    if (sh_init) fclose(sh_init);

    // Resolve Model Dynamically
    resolve_local_model();

    // Load API Selection
    FILE* asf = fopen("state/active_api.txt", "r");
    if (asf) { 
        if (fgets(current_api_url, sizeof(current_api_url), asf)) 
            current_api_url[strcspn(current_api_url, "\r\n")] = 0; 
        fclose(asf); 
    }

    // Load Context Configuration
    FILE* c_file = fopen("config/context.txt", "r");
    if (c_file) {
        char c_line[128];
        while (fgets(c_line, sizeof(c_line), c_file)) {
            if (strncmp(c_line, "limit=", 6) == 0) ctx_limit = atoll(c_line + 6);
            else if (strncmp(c_line, "divisor=", 8) == 0) ctx_divisor = atoi(c_line + 8);
        }
        fclose(c_file);
    }

    // Load Features Configuration
    FILE* f_file = fopen("config/features.txt", "r");
    if (f_file) {
        char f_line[128];
        while (fgets(f_line, sizeof(f_line), f_file)) {
            if (strncmp(f_line, "completion=off", 14) == 0) feat_completion = 0;
            else if (strncmp(f_line, "summarize=off", 13) == 0) feat_summarize = 0;
        }
        fclose(f_file);
    }

    char* ctx_file = make_path("state", "context.json");
    char* yolo_file = make_path("config", "yolo.flag");
    char* tmp_llm = make_path("state", "llm_response.json");
    char* tmp_content = make_path("state", "llm_content.json");
    char* tmp_prompt = make_path("state", "prompt.json");
    char* tmp_args = make_path("state", "args.tmp");

    // Initialize or Sync context with system prompt
    FILE* fpers = fopen("config/persona.txt", "r");
    char* sys_msg = NULL;
    if (fpers) {
        fseek(fpers, 0, SEEK_END); long fsize = ftell(fpers); fseek(fpers, 0, SEEK_SET);
        sys_msg = malloc(fsize + 1);
        if (sys_msg) {
            size_t read_bytes = fread(sys_msg, 1, fsize, fpers);
            sys_msg[read_bytes] = '\0';
        }
        fclose(fpers);
    } else {
        sys_msg = strdup("You are Aida, an expert technical coding agent.");
    }

    int force_sync = 0;
    if (access(ctx_file, F_OK) != 0) {
        force_sync = 1;
    } else {
        // Simple check: does the file contain the sys_msg?
        char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
        char* current_ctx = run_tool(read_args[0], read_args);
        if (current_ctx) {
            if (sys_msg && !strstr(current_ctx, sys_msg)) {
                force_sync = 1;
            }
            free(current_ctx);
        }
    }

    if (force_sync) {
        unlink(ctx_file);
        char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", sys_msg, NULL};
        run_tool(init_args[0], init_args);
    }
    free(sys_msg);

    printf("\033[94m[Aida] Agent Active.\033[0m (exit, /yolo, /clear)\n");
    
    char* line = NULL;
    while (keep_running) {
        action_interrupted = 0;
        int pct = 0;
        
        // Handle Resize Trigger
        if (do_resize) {
            do_resize = 0;
            printf("\033[H\033[2J"); // Clear screen completely
            fflush(stdout);
        }
        
        // Calculate context stats (simple estimation)
        char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
        char* full_ctx = run_tool(read_args[0], read_args);
        if (full_ctx) {
            pct = (strlen(full_ctx) / ctx_divisor); 
            if (pct > 100) pct = 100;
            free(full_ctx);
        }
        
        // frame_clear();

        if (line) free(line);
        line = get_line_interactive(pct);
        if (!line) break;
        if (strlen(line) == 0) continue;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "/exit") == 0) {
            keep_running = 0; break;
        }
        if (strcmp(line, "/yolo") == 0) {
            if (access(yolo_file, F_OK) == 0) { unlink(yolo_file); printf("YOLO: OFF\n"); log_session_frame("/yolo", "OFF"); }
            else { FILE* f = fopen(yolo_file, "w"); if(f){ fputs("1", f); fclose(f); } printf("YOLO: ON\n"); log_session_frame("/yolo", "ON"); }
            continue;
        }
        if (strcmp(line, "/clear") == 0) {
            unlink(ctx_file);
            FILE* fpers = fopen("config/persona.txt", "r");
            char* sys_msg = NULL;
            if (fpers) {
                fseek(fpers, 0, SEEK_END); long fsize = ftell(fpers); fseek(fpers, 0, SEEK_SET);
                sys_msg = malloc(fsize + 1);
                if (sys_msg) {
                    size_t read_bytes = fread(sys_msg, 1, fsize, fpers);
                    sys_msg[read_bytes] = '\0';
                }
                fclose(fpers);
            }
 else {
                sys_msg = strdup("You are a technical coding agent.");
            }
            char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", sys_msg, NULL};
            run_tool(init_args[0], init_args);
            free(sys_msg);
            printf("Memory cleared.\n"); continue;
        }
        if (strcmp(line, "/scan") == 0) {
            char* list_args[] = {"./tools/list_dir", ".", NULL};
            char* snapshot = run_tool(list_args[0], list_args);
            if (snapshot) {
                char* snapshot_msg = NULL;
                if (asprintf(&snapshot_msg, "[Context Snapshot]\n%s", snapshot) != -1) {
                    char* append_args[] = {"./tools/json_state", "append", ctx_file, "system", snapshot_msg, NULL};
                    run_tool(append_args[0], append_args);
                    printf("\033[90m%s\033[0m\n", snapshot_msg);
                    printf("\033[90mContext snapshot injected. Aida now sees this structure.\033[0m\n");
                    log_session_frame("/scan", snapshot_msg);
                    free(snapshot_msg);
                }
                free(snapshot);
            }
            continue;
        }
        if (strcmp(line, "/summarize") == 0) {
            if (!feat_summarize) { printf("Summarization is disabled in config/features.txt\n"); continue; }
            printf("Summarizing conversation...\n");
            log_session_frame("/summarize", "Summarizing conversation...");

            char* api_key = get_gemini_api_key();
            if (!api_key) {
                fprintf(stderr, "\033[91m[ERR] Google Gemini API Key not found in environment or file.\033[0m\n");
                continue;
            }
            char* key_header = NULL;
            if (asprintf(&key_header, "x-goog-api-key: %s", api_key) == -1) { key_header = NULL; }
            free(api_key);

            char* sum_sys_instruction = "Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.";
            char* build_args[] = {"./tools/gemini_payload_builder", ctx_file, tmp_prompt, "0", sum_sys_instruction, NULL};
            char* build_res = run_tool(build_args[0], build_args);
            if (build_res) free(build_res);

            start_spinner();
            unlink(tmp_llm);
            char* curl_url = NULL;
            if (asprintf(&curl_url, "%s/v1beta/models/%s:generateContent", current_api_url, current_model) == -1) { curl_url = NULL; }
            pid_t curl_pid = fork();
            if (curl_pid == 0) {
                char* body_arg = NULL;
                if (asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
                    char* curl_args[] = {
                        "curl", "-s", "--max-time", "120",
                        "-H", "Content-Type: application/json",
                        "-H", key_header,
                        curl_url,
                        "-d", body_arg,
                        "-o", tmp_llm,
                        NULL
                    };
                    execvp(curl_args[0], curl_args);
                }
                _exit(127);
            }
            free(curl_url);
            int curl_status = 0;
            while (1) {
                pid_t r = waitpid(curl_pid, &curl_status, WNOHANG);
                if (r == curl_pid) break;
                if (r == -1 && errno != EINTR) break;
                if (action_interrupted) { kill(curl_pid, SIGTERM); break; }
                usleep(50000);
            }
            stop_spinner();
            free(key_header);

            if (action_interrupted) {
                printf("\n\033[91m[Action Cancelled]\033[0m\n");
                continue;
            }

            if (!WIFEXITED(curl_status) || WEXITSTATUS(curl_status) != 0 || access(tmp_llm, F_OK) != 0) {
                fprintf(stderr, "\033[91m[ERR] LLM call failed (exit: %d).\033[0m\n", 
                        WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1);
                char* p_err[] = {"./tools/json_parser", tmp_llm, "message", NULL};
                char* err_msg = run_tool(p_err[0], p_err);
                if (err_msg && strlen(err_msg) > 0) {
                    fprintf(stderr, "\033[93m[API Error] %s\033[0m\n", err_msg);
                    free(err_msg);
                }
                continue;
            }

            char* p_ext[] = {"./tools/json_parser", tmp_llm, "text", NULL};
            char* content_json = run_tool(p_ext[0], p_ext);
            if (content_json) {
                FILE* cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                char* p_sum[] = {"./tools/json_parser", tmp_content, "summary", NULL};
                char* summary = run_tool(p_sum[0], p_sum);
                if (summary) {
                    unlink(ctx_file);
                    char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", summary, NULL};
                    run_tool(init_args[0], init_args);
                    // printf("\033[92m>> Summary: %s\033[0m\n", summary);
                    if (last_response) free(last_response);
                    last_response = strdup(summary);
                    log_session_frame("system", summary);
                    free(summary);
                }
                free(content_json);
            }
            continue;
        }
        if (strcmp(line, "/api") == 0) {
            FILE* af = fopen("config/apis.txt", "r");
            if (!af) { printf("Error: config/apis.txt not found.\n"); continue; }
            
            char* names = malloc(4096);
            names[0] = '\0';
            char urls[16][256];
            char al[512];
            int ac = 0;
            while (fgets(al, sizeof(al), af) && ac < 16) {
                char* p = strchr(al, '|');
                if (p) {
                    *p = '\0';
                    strcat(names, al); strcat(names, "  ");
                    strcpy(urls[ac++], p + 1);
                    urls[ac-1][strcspn(urls[ac-1], "\r\n")] = 0;
                }
            }
            fclose(af);
            
            printf("Select API (Tab to cycle, Enter to select):\n");
            int sel = 0;
            enable_raw_mode();
            while (1) {
                refresh_ui(pct, "Select", names, sel, NULL);
                char c; if (read(STDIN_FILENO, &c, 1) <= 0) break;
                if (c == 9) sel = (sel + 1) % ac;
                else if (c == 13 || c == 10) {
                    strcpy(current_api_url, urls[sel]);
                    FILE* sf = fopen("state/active_api.txt", "w");
                    if (sf) { fputs(current_api_url, sf); fclose(sf); }
                    clear_dropdown();
                    printf("\033[94m>> API set to: %s\033[0m\n", current_api_url);
                    log_session_frame("/api", current_api_url);
                    break;
                }
                else if (c == 3) { clear_dropdown(); break; }
            }
            disable_raw_mode();
            free(names);
            continue;
        }

        // 1. Append User Message to Context
        char* u_args[] = {"./tools/json_state", "append", ctx_file, "user", line, NULL};
        run_tool(u_args[0], u_args);

        while (keep_running && !action_interrupted) {
            // 2. Read Full Context for API Call and Build Payload
            char* build_args[] = {"./tools/gemini_payload_builder", ctx_file, tmp_prompt, NULL};
            char* build_res = run_tool(build_args[0], build_args);
            if (build_res) free(build_res);

            char* api_key = get_gemini_api_key();
            if (!api_key) {
                fprintf(stderr, "\033[91m[ERR] Google Gemini API Key not found in environment or file.\033[0m\n");
                break;
            }
            char* key_header = NULL;
            if (asprintf(&key_header, "x-goog-api-key: %s", api_key) == -1) { key_header = NULL; }
            free(api_key);

            start_spinner();
            unlink(tmp_llm);
            char* curl_url = NULL;
            if (asprintf(&curl_url, "%s/v1beta/models/%s:streamGenerateContent?alt=sse", current_api_url, current_model) == -1) { curl_url = NULL; }

            char* curl_cmd = NULL;
            if (asprintf(&curl_cmd, "curl -s -N --max-time 120 -H \"Content-Type: application/json\" -H \"%s\" -d @%s \"%s\"", 
                     key_header ? key_header : "", tmp_prompt, curl_url ? curl_url : "") == -1) { curl_cmd = NULL; }

            FILE* stream_fp = popen(curl_cmd, "r");
            char* acc_text = NULL;
            char* acc_func = NULL;
            int first_chunk = 1;

            if (stream_fp) {
                char line[8192];
                while (fgets(line, sizeof(line), stream_fp)) {
                    if (first_chunk) { stop_spinner(); printf("\033[92m>> \033[0m"); first_chunk = 0; }
                    if (strncmp(line, "data: ", 6) == 0) handle_stream_chunk(line + 6, &acc_text, &acc_func);
                    if (action_interrupted) break;
                }
                pclose(stream_fp);
            }
            if (first_chunk) stop_spinner(); else printf("\n");
            free(curl_url); free(curl_cmd); free(key_header);

            if (action_interrupted) {
                printf("\n\033[91m[Action Cancelled]\033[0m\n");
                if (acc_text) free(acc_text);
                if (acc_func) free(acc_func);
                break;
            }

            // Construct valid Gemini-like JSON for the existing tool-calling logic
            FILE* fllm = fopen(tmp_llm, "w");
            if (fllm) {
                fprintf(fllm, "{\"candidates\":[{\"content\":{\"parts\":[");
                if (acc_text && strlen(acc_text) > 0) {
                    fprintf(fllm, "{\"text\":\"%s\"}", acc_text);
                    if (acc_func) fprintf(fllm, ",");
                }
                if (acc_func) {
                    fprintf(fllm, "{\"functionCall\":%s}", acc_func);
                }
                fprintf(fllm, "]}}]}");
                fclose(fllm);
            }
            if (acc_text) free(acc_text);
            if (acc_func) free(acc_func);

            if (access(tmp_llm, F_OK) != 0) {
                fprintf(stderr, "\033[91m[ERR] Failed to create llm_response.json\033[0m\n");
                continue;
            }

            // Check for API-level error first
            char* p_err[] = {"./tools/json_parser", tmp_llm, "message", NULL};
            char* err_msg = run_tool(p_err[0], p_err);
            if (err_msg && strlen(err_msg) > 0) {
                fprintf(stderr, "\033[93m[API Error] %s\033[0m\n", err_msg);
                free(err_msg);
                continue;
            }

            // 3. Extract text response or functionCall
            char* p_ext[] = {"./tools/json_parser", tmp_llm, "text", NULL};
            char* text_response = run_tool(p_ext[0], p_ext);
            
            char* p_calls[] = {"./tools/json_parser", tmp_llm, "functionCall", NULL};
            char* function_call = run_tool(p_calls[0], p_calls);

            if ((!text_response || strlen(text_response) == 0) && (!function_call || strlen(function_call) == 0)) {
                // If both are empty, check if we have any other error indication
                char* p_err_obj[] = {"./tools/json_parser", tmp_llm, "error", NULL};
                char* err_obj = run_tool(p_err_obj[0], p_err_obj);
                if (err_obj && strlen(err_obj) > 0) {
                    fprintf(stderr, "\033[93m[API Error Object] %s\033[0m\n", err_obj);
                    free(err_obj);
                } else {
                    fprintf(stderr, "\033[90m[Debug] Raw response saved to state/llm_response.json\033[0m\n");
                }
            }

            char* tool_name = NULL;
            char* args_json = NULL;

            if (function_call && strlen(function_call) > 2) {
                FILE* fcalls = fopen("state/tool_calls.tmp", "w");
                if (fcalls) { fputs(function_call, fcalls); fclose(fcalls); }
                
                char* p_tn[] = {"./tools/json_parser", "state/tool_calls.tmp", "name", NULL};
                tool_name = run_tool(p_tn[0], p_tn);
                char* p_ta[] = {"./tools/json_parser", "state/tool_calls.tmp", "args", NULL};
                args_json = run_tool(p_ta[0], p_ta);
                unlink("state/tool_calls.tmp");
            } else if (text_response && strlen(text_response) > 0) {
                FILE* fcont = fopen(tmp_content, "w");
                if (fcont) { fputs(text_response, fcont); fclose(fcont); }
                
                char* p_tn[] = {"./tools/json_parser", tmp_content, "tool", NULL};
                tool_name = run_tool(p_tn[0], p_tn);
                char* p_ta[] = {"./tools/json_parser", tmp_content, "args", NULL};
                args_json = run_tool(p_ta[0], p_ta);
            }

            if (tool_name && strlen(tool_name) > 0) {
                FILE* fargs = fopen(tmp_args, "w");
                if (fargs) { fputs(args_json && strlen(args_json) > 0 ? args_json : "{}", fargs); fclose(fargs); }
                
                char* result = NULL;
                if (strcmp(tool_name, "exec_cmd") == 0 || strcmp(tool_name, "run_command") == 0) {
                    char* p_cmd[] = {"./tools/json_parser", tmp_args, "cmd", NULL};
                    char* command = run_tool(p_cmd[0], p_cmd);
                    if (!command || strlen(command) == 0) {
                        free(command);
                        char* p_cmd2[] = {"./tools/json_parser", tmp_args, "command", NULL};
                        command = run_tool(p_cmd2[0], p_cmd2);
                    }
                    if (command && strlen(command) > 0) {
                        if (check_permission(tool_name, command)) {
                            char* exec_args[] = {"./tools/cmd_exec", command, NULL};
                            result = run_tool(exec_args[0], exec_args);
                        } else {
                            result = strdup("Permission Denied: Command execution aborted by user.");
                        }
                        free(command);
                    }
                } else if (strcmp(tool_name, "read_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    if (path && strlen(path) > 0) {
                        char* tool_args[] = {"./tools/file_ops", "read", path, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(path);
                    }
                } else if (strcmp(tool_name, "write_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* p_content[] = {"./tools/json_parser", tmp_args, "content", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* content = run_tool(p_content[0], p_content);
                    if (path && strlen(path) > 0 && content) {
                        if (check_permission(tool_name, path)) {
                            char* tool_args[] = {"./tools/file_ops", "write", path, content, NULL};
                            result = run_tool(tool_args[0], tool_args);
                        } else {
                            result = strdup("Permission Denied: Write aborted by user.");
                        }
                    }
                    free(path); free(content);
                } else if (strcmp(tool_name, "list_dir") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* tool_args[] = {"./tools/list_dir", path && strlen(path) > 0 ? path : ".", NULL};
                    result = run_tool(tool_args[0], tool_args);
                    free(path);
                } else if (strcmp(tool_name, "search_in_files") == 0) {
                    char* p_query[] = {"./tools/json_parser", tmp_args, "query", NULL};
                    char* query = run_tool(p_query[0], p_query);
                    if (query && strlen(query) > 0) {
                        char* tool_args[] = {"./tools/search_in_files", query, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(query);
                    }
                } else if (strcmp(tool_name, "edit_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* p_search[] = {"./tools/json_parser", tmp_args, "search", NULL};
                    char* p_replace[] = {"./tools/json_parser", tmp_args, "replace", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* search = run_tool(p_search[0], p_search);
                    char* replace = run_tool(p_replace[0], p_replace);
                    if (path && search && replace && strlen(path) > 0) {
                        if (check_permission(tool_name, path)) {
                            char* tool_args[] = {"./tools/edit_file", path, search, replace, NULL};
                            result = run_tool(tool_args[0], tool_args);
                        } else {
                            result = strdup("Permission Denied: Edit aborted by user.");
                        }
                    }
                    free(path); free(search); free(replace);
                } else if (strcmp(tool_name, "web_search") == 0) {
                    char* p_query[] = {"./tools/json_parser", tmp_args, "query", NULL};
                    char* query = run_tool(p_query[0], p_query);
                    if (query && strlen(query) > 0) {
                        char* tool_args[] = {"./tools/web_search", query, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(query);
                    }
                }

                if (result) {
                    printf("\033[90m[Action: %s]\033[0m\n", tool_name);
                    printf("\033[92m>> %s\033[0m\n", result);
                    
                    char* assistant_json = NULL;
                    if (asprintf(&assistant_json, "{\"name\":\"%s\",\"args\":%s}", tool_name, args_json && strlen(args_json) > 0 ? args_json : "{}") == -1) { assistant_json = NULL; }
                    char* a_args[] = {"./tools/json_state", "append", ctx_file, "assistant", assistant_json, NULL};
                    run_tool(a_args[0], a_args);
                    if (assistant_json) free(assistant_json);

                    char* t_args[] = {"./tools/json_state", "append", ctx_file, "tool", result, NULL};
                    run_tool(t_args[0], t_args);
                    free(result);
                    free(tool_name); free(args_json); free(function_call); free(text_response);
                    continue; // Loop back for tool result
                }
            }

            char* resp = NULL;
            if (text_response && strlen(text_response) > 0) {
                FILE* fcont = fopen(tmp_content, "w");
                if (fcont) { fputs(text_response, fcont); fclose(fcont); }
                char* p_resp[] = {"./tools/json_parser", tmp_content, "response", NULL};
                resp = run_tool(p_resp[0], p_resp);
            }
            if (!resp || strlen(resp) == 0) {
                if (resp) free(resp);
                resp = (text_response && strlen(text_response) > 0) ? strdup(text_response) : strdup("(empty)");
            }

            // printf("\033[92m>> %s\033[0m\n", resp);
            char* r_args[] = {"./tools/json_state", "append", ctx_file, "assistant", resp, NULL};
            run_tool(r_args[0], r_args);
            
            if (last_response) free(last_response);
            last_response = strdup(resp);
            log_session_frame(line, resp);
            log_session_history(line, resp);

            free(resp); free(tool_name); free(args_json); free(function_call); free(text_response);
            break; // Final response received
        }
    }

    free(ctx_file); free(yolo_file); free(tmp_llm); free(tmp_content); free(tmp_prompt); free(tmp_args);
    if (last_response) free(last_response);
    if (debug_fp) fclose(debug_fp);
    printf("\033[90mGraceful shutdown.\033[0m\n"); return 0;
}