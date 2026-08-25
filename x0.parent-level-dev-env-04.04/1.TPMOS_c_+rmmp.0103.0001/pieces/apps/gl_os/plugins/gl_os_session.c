/*
 * gl_os_session.c - GL-OS Desktop Session Manager
 * Manages windows, context menus, and desktop state
 * Separate from CHTPM terminal session
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <windows.h>
#include <process.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif
#include <signal.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#define usleep(us) Sleep((us)/1000)
#ifndef _vscprintf
/* Simple asprintf for Windows */
#include <stdarg.h>
int asprintf(char** strp, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    if (len < 0) return -1;
    *strp = (char*)malloc(len + 1);
    if (!*strp) return -1;
    int result = vsprintf(*strp, fmt, args);
    va_end(args);
    return result;
}
#endif
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MAX_CMD 16384
#define MAX_LINE 1024
#define MAX_WINDOWS 15
#define MAX_WINDOWS_STR 8192

/* CPU-SAFE shutdown flag */
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/* Run command with fork/exec/waitpid */
static int run_command(const char* cmd) {
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
#else
    /* Windows simple version */
    return system(cmd);
#endif
    return -1;
}

/* Global state */
char project_root[MAX_PATH] = ".";
char *session_state_path = NULL;
char *session_history_path = NULL;
char *session_view_path = NULL;
char *session_view_changed_path = NULL;

/* Window structure */
typedef struct {
    int id;
    int x, y, w, h;
    char title[64];
    char type[32];  /* "terminal", "folder", "menu", "file" */
    char content_source[MAX_PATH];
    int active;
} DesktopWindow;

DesktopWindow windows[MAX_WINDOWS];
int window_count = 0;
int next_window_id = 1;
int active_window_id = -1;

/* Desktop state */
char bg_color[16] = "#000044";
long last_history_pos = 0;

void resolve_root(void) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;
    
    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0) strncpy(project_root, v, MAX_PATH-1);
            break;
        }
    }
    fclose(kvp);
}

void log_master(const char* event, const char* piece) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/gl_os/ledger/master_ledger.txt", project_root) != -1) {
        FILE *ledger = fopen(path, "a");
        if (ledger) {
            fprintf(ledger, "[%s] GL-OS: %s on %s\n", timestamp, event, piece);
            fclose(ledger);
        }
        free(path);
    }
}

void write_view(void) {
    FILE *fp = fopen(session_view_path, "w");
    if (!fp) return;
    
    fprintf(fp, "DESKTOP_VIEW\n");
    fprintf(fp, "BG: %s\n", bg_color);
    fprintf(fp, "WINDOW_COUNT: %d\n", window_count);
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        fprintf(fp, "WINDOW | %d | %d | %d | %d | %d | %s | %s | %s\n", 
                windows[i].id, windows[i].x, windows[i].y, 
                windows[i].w, windows[i].h, 
                windows[i].title, windows[i].type,
                windows[i].content_source);
    }
    
    fclose(fp);
    
    /* Touch view_changed marker */
    FILE *marker = fopen(session_view_changed_path, "a");
    if (marker) { 
        fprintf(marker, "G\n"); 
        fclose(marker); 
    }
}

void write_gui_state(void) {
    /* Write gui_state.txt for CHTPM Parser */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/gl_os/manager/gui_state.txt", project_root);
    
    /* Ensure directory exists */
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/pieces/apps/gl_os/manager", project_root);
    mkdir(dir, 0755);

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=pieces/apps/gl_os/plugins/+x/gl_os_session.+x\n");
        fprintf(f, "active_layout_id=desktop.chtpm\n");
        fprintf(f, "app_title=GL-OS DESKTOP\n");
        
        char win_list[MAX_WINDOWS_STR] = "";
        char line[256];
        for (int i = 0; i < window_count; i++) {
            if (!windows[i].active) continue;
            snprintf(line, sizeof(line), "%d:%s,", windows[i].id, windows[i].title);
            if (strlen(win_list) + strlen(line) < MAX_WINDOWS_STR - 50) strcat(win_list, line);
        }
        fprintf(f, "windows=%s\n", win_list);
        fprintf(f, "bg_color=%s\n", bg_color);
        fprintf(f, "active_window=%d\n", active_window_id);
        
        fclose(f);
    }
}

void write_desktop_state(void) {
    char win_list[MAX_WINDOWS_STR] = "";
    char line[256];
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        snprintf(line, sizeof(line), "%d:%s,", windows[i].id, windows[i].title);
        if (strlen(win_list) + strlen(line) < MAX_WINDOWS_STR - 50) {
            strcat(win_list, line);
        }
    }
    
    FILE *f = fopen(session_state_path, "w");
    if (f) {
        fprintf(f, "name=Desktop\n");
        fprintf(f, "type=desktop\n");
        fprintf(f, "pos_x=0\n");
        fprintf(f, "pos_y=0\n");
        fprintf(f, "width=800\n");
        fprintf(f, "height=600\n");
        fprintf(f, "bg_color=%s\n", bg_color);
        fprintf(f, "windows=%s\n", win_list);
        fprintf(f, "active_window=%d\n", active_window_id);
        fprintf(f, "session_status=active\n");
        fclose(f);
    }
    write_gui_state();
}

void open_window(const char* title, const char* type, int x, int y, int w, int h, const char* content) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = x;
    windows[idx].y = y;
    windows[idx].w = w;
    windows[idx].h = h;
    windows[idx].active = 1;
    strncpy(windows[idx].title, title, sizeof(windows[idx].title)-1);
    strncpy(windows[idx].type, type, sizeof(windows[idx].type)-1);
    strncpy(windows[idx].content_source, content, sizeof(windows[idx].content_source)-1);
    
    active_window_id = windows[idx].id;
    
    log_master("WINDOW_OPEN", title);
    write_view();
    write_desktop_state();
}

void close_window(int win_id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == win_id) {
            windows[i].active = 0;
            log_master("WINDOW_CLOSE", windows[i].title);
            
            if (active_window_id == win_id) {
                active_window_id = -1;
                /* Find next active window */
                for (int j = 0; j < window_count; j++) {
                    if (windows[j].active) {
                        active_window_id = windows[j].id;
                        break;
                    }
                }
            }
            
            write_view();
            write_desktop_state();
            return;
        }
    }
}

void create_terminal_window(void) {
    static int term_count = 0;
    char title[32], content[MAX_PATH];
    
    term_count++;
    snprintf(title, sizeof(title), "Terminal %d", term_count);
    snprintf(content, sizeof(content), "pieces/apps/gl_os/session/terminal_%d.txt", term_count);
    
    /* Create terminal content file */
    FILE *f = fopen(content, "w");
    if (f) {
        fprintf(f, "GL-OS Terminal %d\n", term_count);
        fprintf(f, "Type commands below:\n\n");
        fclose(f);
    }
    
    /* Cascade window position */
    int x = 10 + (term_count * 15);
    int y = 10 + (term_count * 5);
    
    open_window(title, "terminal", x, y, 60, 20, content);
}

void create_folder_window(const char* name) {
    char title[64], content[MAX_PATH];
    
    snprintf(title, sizeof(title), "Folder: %s", name ? name : "New Folder");
    snprintf(content, sizeof(content), "FOLDER:%s", name ? name : "new_folder");
    
    open_window(title, "folder", 50, 50, 40, 15, content);
}

void create_file_window(const char* name) {
    char title[64], content[MAX_PATH];
    
    snprintf(title, sizeof(title), "File: %s", name ? name : "New File");
    snprintf(content, sizeof(content), "FILE:%s", name ? name : "new_file.txt");
    
    open_window(title, "file", 100, 100, 50, 20, content);
}

void show_context_menu(int x, int y) {
    /* For now, just log - context menu will be handled via key bindings */
    log_master("CONTEXT_MENU", "desktop");
}

void process_key(int key) {
    /* Key bindings for GL-OS */
    switch (key) {
        case 116:  /* 't' - New Terminal */
            create_terminal_window();
            break;
        case 102:  /* 'f' - New Folder */
            create_folder_window(NULL);
            break;
        case 110:  /* 'n' - New File */
            create_file_window(NULL);
            break;
        case 99:   /* 'c' - Close active window */
            if (active_window_id != -1) {
                close_window(active_window_id);
            }
            break;
        case 27:   /* ESC - Deselect window */
            active_window_id = -1;
            write_desktop_state();
            break;
        default:
            break;
    }
}

void poll_history(void) {
    FILE *hf = fopen(session_history_path, "r");
    if (!hf) return;
    
    struct stat st;
    if (stat(session_history_path, &st) != 0) {
        fclose(hf);
        return;
    }
    
    if (st.st_size <= last_history_pos) {
        fclose(hf);
        return;
    }
    
    fseek(hf, last_history_pos, SEEK_SET);
    
    char line[256];
    while (fgets(line, sizeof(line), hf)) {
        if (strstr(line, "KEY_PRESSED: ")) {
            int key = atoi(strstr(line, "KEY_PRESSED: ") + 13);
            if (key > 0) {
                process_key(key);
            }
        }
    }
    
    last_history_pos = ftell(hf);
    fclose(hf);
}

void init_paths(void) {
    if (asprintf(&session_state_path, "%s/pieces/apps/gl_os/session/state.txt", project_root) == -1) session_state_path = NULL;
    if (asprintf(&session_history_path, "%s/pieces/apps/gl_os/session/history.txt", project_root) == -1) session_history_path = NULL;
    if (asprintf(&session_view_path, "%s/pieces/apps/gl_os/session/view.txt", project_root) == -1) session_view_path = NULL;
    if (asprintf(&session_view_changed_path, "%s/pieces/apps/gl_os/session/view_changed.txt", project_root) == -1) session_view_changed_path = NULL;
    if (asprintf(&session_state_path, "%s/pieces/apps/gl_os/session/state.txt", project_root) == -1) session_state_path = NULL;
}

void ensure_session_files(void) {
    /* Create session files if they don't exist */
    FILE *f;
    
    f = fopen(session_state_path, "a");
    if (f) fclose(f);
    
    f = fopen(session_history_path, "a");
    if (f) fclose(f);
    
    f = fopen(session_view_path, "a");
    if (f) fclose(f);
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    resolve_root();
    init_paths();
    ensure_session_files();
    
    /* Initialize with desktop background */
    write_view();
    write_desktop_state();
    
    log_master("SESSION_START", "gl-os desktop");
    
    while (!g_shutdown) {
        poll_history();
        
        /* Small sleep to prevent CPU hogging */
        usleep(50000);  /* 50ms */
    }
    
    log_master("SESSION_END", "gl-os desktop");
    
    return 0;
}
