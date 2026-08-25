/*
 * gl_desktop.c - GL-OS Desktop Environment
 */

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dirent.h>

#include "../../../../libraries/stb_image.h"
#ifndef _WIN32
#include <sys/time.h>
#include <sys/wait.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <process.h>
#include <direct.h>
#define F_OK 0
#define access _access
#endif
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#ifdef _WIN32
/* Simple gettimeofday for Windows */
int gettimeofday(struct timeval* tp, struct timezone* tzp) {
    static const uint64_t EPOCH_VAL = (uint64_t)116444736000000000ULL;
    SYSTEMTIME system_time;
    FILETIME file_time;
    uint64_t time_val;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time_val = ((uint64_t)file_time.dwLowDateTime);
    time_val += ((uint64_t)file_time.dwHighDateTime) << 32;
    tp->tv_sec = (long)((time_val - EPOCH_VAL) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
static int win_kill(pid_t pid, int sig) {
    (void)sig; if (pid <= 0) return -1;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) return -1;
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result ? 0 : -1;
}
#define kill win_kill

#define WNOHANG 1
static int win_waitpid(pid_t pid, int* status, int options) {
    (void)status; (void)options;
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess == NULL) return -1;
    DWORD result = WaitForSingleObject(hProcess, (options & WNOHANG) ? 0 : INFINITE);
    CloseHandle(hProcess);
    if (result == WAIT_OBJECT_0) return pid;
    return 0;
}
#define waitpid win_waitpid
#endif

#define MAX_LINE 1024
#define MAX_WIN_TITLE 64
#define MAX_WINDOWS 15
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MAX_TERM_OPTIONS 60
#define MAX_OPTION_LEN 80

/* Menu Contexts */
#define CTX_MAIN 0
#define CTX_DESKTOP_APPS 1
#define CTX_TERMINAL_APPS 2
#define CTX_GLTPM_APPS 3
#define CTX_SETTINGS 4 /* New context for settings menu */

#define MAX_GLTPM_TILES 1024
#define MAX_GLTPM_SPRITES 128
#define MAX_GLTPM_BUTTONS 128
#define MAX_GLTPM_TEXTS 64
#define MAX_GLTPM_NODES 128

typedef struct {
    float x, y, z;
    float extrude;
    float color[3];
    char tile_id[64];
    char ascii[16];
    char unicode[16];
    int parent; /* Parent node index (-1 for root) */
} GLTPMTile;

typedef struct {
    float x, y, z;
    float color[3];
    char sprite_id[64];
    char label[64];
    char ascii[16];
    char unicode[16];
    unsigned char artifact_mask[8][8];
    float face_colors[6][3]; /* Top, Bottom, Front, Back, Right, Left */
    int has_face_colors;
    int parent;
} GLTPMSprite;

typedef struct {
    char id[64];
    char label[128];
    char onClick[128];
    float x, y, w, h; /* Structural positioning (-1 = default) */
    int parent;
} GLTPMButton;

typedef struct {
    char label[256];
    float x, y;
    int parent;
} GLTPMText;

/* CHTMGL Structural Node */
typedef struct {
    char tag[32];
    char id[64];
    float x, y, w, h;
    float color[4];
    int parent;
    unsigned int texture_id;
    int has_texture;
} GLTPMNode;

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

char project_root[MAX_PATH] = ".";

typedef struct {
    char id[64];
    char name[128];
} GLProject;

GLProject gl_projects[50];
int gl_project_count = 0;

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

void get_pdl_value(const char* project, const char* section, const char* key, char* out_val, size_t out_sz) {
    out_val[0] = '\0';
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/%s/project.pdl", project_root, project);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MAX_LINE];
    int in_section = 0;
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_str(line);
        if (strncmp(trimmed, section, strlen(section)) == 0 && (trimmed[strlen(section)] == ' ' || trimmed[strlen(section)] == '\t' || trimmed[strlen(section)] == '\0')) {
            in_section = 1;
            continue;
        }
        if (strncmp(trimmed, "SECTION", 7) == 0 || (line[0] != ' ' && line[0] != '\t' && line[0] != '\n' && line[0] != '-' && strstr(line, "|") == NULL && strlen(trimmed) > 0)) {
            if (in_section) break;
        }
        if (in_section) {
            char *pipe1 = strchr(line, '|');
            if (pipe1) {
                char k_buf[MAX_LINE];
                char *pipe2 = strchr(pipe1 + 1, '|');
                if (pipe2) {
                    int k_len = pipe2 - pipe1 - 1;
                    if (k_len >= MAX_LINE) k_len = MAX_LINE - 1;
                    strncpy(k_buf, trim_str(pipe1 + 1), k_len);
                    k_buf[k_len] = '\0';
                    if (strcmp(trim_str(k_buf), key) == 0) {
                        strncpy(out_val, trim_str(pipe2 + 1), out_sz - 1);
                        out_val[out_sz - 1] = '\0';
                        fclose(f);
                        return;
                    }
                }
            }
        }
    }
    fclose(f);
}

static int run_gl_project_scan_op(void) {
    char *op_path = NULL;
    int rc = -1;

    if (asprintf(&op_path, "%s/pieces/apps/gl_os/plugins/+x/gl_os_project_scan.+x", project_root) == -1) {
        return -1;
    }

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) > 0 && WIFEXITED(status)) {
            rc = WEXITSTATUS(status);
        }
    }
#else
    rc = _spawnl(_P_WAIT, op_path, op_path, NULL);
#endif

    free(op_path);
    return rc;
}

static int load_gl_projects_from_cache(void) {
    char *cache_path = NULL;
    FILE *f = NULL;
    if (asprintf(&cache_path, "%s/pieces/apps/gl_os/manager/projects_cache.txt", project_root) == -1) {
        return 0;
    }

    f = fopen(cache_path, "r");
    free(cache_path);
    if (!f) return 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && gl_project_count < 50) {
        char *sep = strchr(line, '|');
        if (!sep) continue;
        *sep = '\0';
        char *id = trim_str(line);
        char *name = trim_str(sep + 1);
        if (*id == '\0' || *name == '\0') continue;

        strncpy(gl_projects[gl_project_count].id, id, 63);
        gl_projects[gl_project_count].id[63] = '\0';
        strncpy(gl_projects[gl_project_count].name, name, 127);
        gl_projects[gl_project_count].name[127] = '\0';
        gl_project_count++;
    }

    fclose(f);
    return gl_project_count > 0;
}

void load_gl_projects(void) {
    gl_project_count = 0;

    run_gl_project_scan_op();
    load_gl_projects_from_cache();

    /* Fallback: keep the in-process scan if the op cache is missing. */
    {
        char kvp_path[MAX_PATH];
        snprintf(kvp_path, sizeof(kvp_path), "%s/pieces/apps/gl_os/gl_os_projects.kvp", project_root);
        FILE *kf = fopen(kvp_path, "r");
        if (kf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), kf) && gl_project_count < 50) {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char *id = trim_str(line);
                    strncpy(gl_projects[gl_project_count].id, id, 63);
                    
                    /* Metadata override from PDL */
                    char nice_name[128] = "";
                    get_pdl_value(id, "STATE", "app_title", nice_name, sizeof(nice_name));
                    if (nice_name[0] == '\0') get_pdl_value(id, "STATE", "title", nice_name, sizeof(nice_name));
                    
                    if (nice_name[0] != '\0') strncpy(gl_projects[gl_project_count].name, nice_name, 127);
                    else strncpy(gl_projects[gl_project_count].name, trim_str(eq + 1), 127);
                    
                    gl_project_count++;
                }
            }
            fclose(kf);
        }
        
        char projects_dir[MAX_PATH];
        snprintf(projects_dir, sizeof(projects_dir), "%s/projects", project_root);
        DIR *dir = opendir(projects_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL && gl_project_count < 50) {
                if (entry->d_name[0] == '.' || strcmp(entry->d_name, "trunk") == 0) continue;
                
                /* Skip if already added from KVP */
                int found = 0;
                for (int i = 0; i < gl_project_count; i++) {
                    if (strcmp(gl_projects[i].id, entry->d_name) == 0) { found = 1; break; }
                }
                if (found) continue;
                
                char pdl_path[MAX_PATH];
                snprintf(pdl_path, sizeof(pdl_path), "%s/%s/project.pdl", projects_dir, entry->d_name);
                if (access(pdl_path, F_OK) == 0) {
                    strncpy(gl_projects[gl_project_count].id, entry->d_name, 63);
                    char nice_name[128] = "";
                    get_pdl_value(entry->d_name, "STATE", "app_title", nice_name, sizeof(nice_name));
                    if (nice_name[0] == '\0') get_pdl_value(entry->d_name, "STATE", "title", nice_name, sizeof(nice_name));
                    
                    if (nice_name[0] != '\0') strncpy(gl_projects[gl_project_count].name, nice_name, 127);
                    else strncpy(gl_projects[gl_project_count].name, entry->d_name, 127);
                    
                    gl_project_count++;
                }
            }
            closedir(dir);
        }
    }
}

typedef struct {
    char title[128];
    float w, h;
    float bg_color[3];
    int tile_count;
    GLTPMTile tiles[MAX_GLTPM_TILES];
    int sprite_count;
    GLTPMSprite sprites[MAX_GLTPM_SPRITES];
    int button_count;
    GLTPMButton buttons[MAX_GLTPM_BUTTONS];
    int text_count;
    GLTPMText texts[MAX_GLTPM_TEXTS];
    int node_count;
    GLTPMNode nodes[MAX_GLTPM_NODES];
    char interact_src[MAX_PATH];
    char last_key[32];
    char active_target_id[64];
    int is_map_control;
    int camera_mode;
    float cam_pos[3];
    float cam_rot[3];
    int xelector_pos[3];
    int ui_mode; /* 0=Standard, 1=SOVEREIGN (Hide Xelector/HUD) */
    int loaded;
} GLTPMScene;

/* Window structure */
typedef struct {
    int id;
    int x, y, w, h;
    char title[MAX_WIN_TITLE];
    char type[32];  /* "terminal", "cube", "folder", "project_mirror" */
    char project_id[64]; /* Canonical ID for projects (e.g., "fuzz-op-gl") */
    int active;
    int minimized;

    /* App-specific state */
    char input_buffer[256];
    char output_history[12][80];
    int history_count;
    float rotation_x, rotation_y;

    /* Camera & Map Control (K3/K4) */
    int is_map_control;    /* 1 = WASD/Arrows control camera/map, 0 = Menu Nav */
    int camera_mode;       /* 1=1st, 2=2nd, 3=3rd, 4=Free */
    float cam_pos[3];      /* X, Y, Z */
    float cam_rot[3];      /* Pitch, Yaw, Roll */
    
    int xelector_pos[3];   /* Map grid coordinates (Int) */
    time_t last_xelector_move; /* Timestamp of last local move */
    int follow_entity_id;  /* Entity to track in 3rd person mode */

    /* Window state for restoration */
    int prev_x, prev_y, prev_w, prev_h;
    int is_maximized;
    int is_half_screen; /* 0=None, 1=Left, 2=Right */

    /* Terminal menu navigation state (joystick-first) */
    int selected_index;        /* Currently highlighted option (0-based) */
    int show_menu;             /* 1 = showing menu options, 0 = text input mode */
    int option_count;          /* Number of menu options */
    char menu_options[MAX_TERM_OPTIONS][MAX_OPTION_LEN];  /* Option labels */
    int menu_context;          /* CTX_MAIN, CTX_DESKTOP_APPS, etc. */
    
    /* Thumb scroll state (Linux terminal behavior) */
    int scroll_offset;         /* Lines scrolled UP from bottom (0 = at bottom) */
    int max_scroll;            /* Maximum scroll offset (history_count - visible_lines) */
    
    /* Text selection for copy/paste */
    int is_selecting;          /* 1 = user is selecting text */
    int select_start_x;        /* Selection start X (mouse) */
    int select_start_y;        /* Selection start Y (mouse) */
    int select_end_x;          /* Selection end X (mouse) */
    int select_end_y;          /* Selection end Y (mouse) */
    char selected_text[4096];  /* Selected text buffer */
    int selection_active;      /* 1 = selection is active (mouse up after drag) */

    /* GLTPM state */
    char gltpm_layout_path[MAX_PATH];
    char nav_buffer[32];   /* "Nav > n" command buffer */
    long gltpm_frame_marker_size;
    long gltpm_global_marker_size;
    long gltpm_layout_marker_size;
    pid_t managed_pid;
    GLTPMScene gltpm_scene;
    int active_node_index; /* Index of currently activated panel/div (-1 for root) */

    /* Emoji Studio state */
    RGBA_Pixel *emoji_voxels;
    int emoji_res;
    char last_active_piece[256];
} DesktopWindow;

/* Global display settings */
int g_fullscreen_enabled = 0;  /* Default to windowed */
float g_font_scale_factor = 1.0f; /* Default font scale */

void* get_font_regular() {
    if (g_font_scale_factor < 0.9f) return GLUT_BITMAP_8_BY_13;
    if (g_font_scale_factor < 1.1f) return GLUT_BITMAP_9_BY_15;
    if (g_font_scale_factor < 1.4f) return GLUT_BITMAP_HELVETICA_12;
    if (g_font_scale_factor < 1.7f) return GLUT_BITMAP_HELVETICA_18;
    return GLUT_BITMAP_TIMES_ROMAN_24;
}

void* get_font_small() {
    if (g_font_scale_factor < 1.1f) return GLUT_BITMAP_8_BY_13;
    if (g_font_scale_factor < 1.5f) return GLUT_BITMAP_HELVETICA_10;
    return GLUT_BITMAP_HELVETICA_12;
}

int get_font_width() {
    if (g_font_scale_factor < 0.9f) return 8;
    if (g_font_scale_factor < 1.1f) return 9;
    if (g_font_scale_factor < 1.4f) return 10;
    if (g_font_scale_factor < 1.7f) return 14;
    return 18;
}

int get_line_height() {
    if (g_font_scale_factor < 0.9f) return 15;
    if (g_font_scale_factor < 1.1f) return 18;
    if (g_font_scale_factor < 1.4f) return 22;
    if (g_font_scale_factor < 1.7f) return 28;
    return 35;
}

int get_button_height() {
    return get_line_height() + 4;
}

/* Emoji Studio Global State */
GLuint emoji_atlas_tex = 0;
int emoji_atlas_w = 0, emoji_atlas_h = 0;

int window_width = 1024;
int window_height = 768;

void init_emoji_atlas() {
    if (emoji_atlas_tex != 0) return;
    int channels;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/../#.emoji.xtract.stb]c4/emoji_atlas.png", project_root);
    unsigned char* data = stbi_load(path, &emoji_atlas_w, &emoji_atlas_h, &channels, 4);
    if (!data) return;

    glGenTextures(1, &emoji_atlas_tex);
    glBindTexture(GL_TEXTURE_2D, emoji_atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, emoji_atlas_w, emoji_atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
}
int toolbar_height_fallback = 30;
float bg_color[3] = {0.0f, 0.0f, 0.27f};  /* #000044 */
int cursor_blink = 0;

DesktopWindow windows[MAX_WINDOWS];
int window_count = 0;
int next_window_id = 1;
int active_window_id = -1;
float global_rotation = 0.0f;

/* Joystick state (global for edge-triggering) */
int last_joy_x = 0;
int last_joy_y = 0;
unsigned int last_joy_buttons = 0;

/* Mouse interaction */
int mouse_x = 0, mouse_y = 0;
int is_dragging = 0;
int is_resizing = 0;
int drag_window_id = -1;
int drag_start_x = 0, drag_start_y = 0;
int win_drag_start_x = 0, win_drag_start_y = 0;
int win_drag_start_w = 0, win_drag_start_h = 0;

/* Scroll thumb dragging (Linux terminal behavior) */
int is_scroll_thumb_drag = 0;
int scroll_window_id = -1;
int scroll_track_y = 0;
int scroll_track_h = 0;

/* Idle backoff state */
time_t last_interaction_time = 0;

/* Context menu state */
int context_menu_visible = 0;
int context_menu_x = 0, context_menu_y = 0;
int context_menu_width = 150;
int context_menu_height = 100;
int context_menu_type = 0;  /* 0 = desktop, 1 = terminal */

/* Session paths */
char *session_state_path = NULL;
char *session_history_path = NULL;
char *session_view_path = NULL;
char *session_view_changed_path = NULL;
char *master_ledger_path = NULL;
char *input_focus_lock_path = NULL;  /* Input focus lock (TPM isolation) */
char *gl_os_frame_path = NULL;        /* GL-OS current frame file */
char *gl_os_frame_pulse = NULL;       /* GL-OS frame change marker */
char *gl_os_frame_history = NULL;     /* GL-OS frame history */
char *gl_os_audit_frame_path = NULL;   /* GL-OS audit frame */

static void handle_sigint(int sig) {
    printf("\n[GL-OS] Caught signal %d. Cleaning up and exiting...\n", sig);
    if (input_focus_lock_path) {
        remove(input_focus_lock_path);
        printf("[GL-OS] Removed input focus lock: %s\n", input_focus_lock_path);
    }
    exit(0);
}

long last_history_pos = 0;

unsigned int gltpm_load_texture(const char* path);

#include "gltpm_parser.c"

/* Forward declarations */
void create_terminal_window(void);
void create_cube_window(void);
void create_mirrored_window(const char* project_id, const char* title);
void create_emoji_studio_window(void);
void create_gltpm_window(const char* project_id, const char* layout_path, const char* title);
void write_view(void);
void log_master(const char* event, const char* piece);
void free_paths(void);
void write_session_state(void);
void close_window(int win_id);
void clamp_window_bounds(DesktopWindow* win);
void init_terminal_menu(DesktopWindow* win);
void execute_option(DesktopWindow* win, int index);
void move_selection(DesktopWindow* win, int direction);
void special_keyboard(int key, int x, int y);
void draw_text_clipped(float x, float y, float max_width, const char* text);
void draw_scroll_thumb(float x, float y, float h, int scroll_offset, int max_scroll, int visible_lines);
void write_gl_os_frame(void);  /* Write frame to file (like ASCII-OS compose_frame) */
static int run_gl_os_audit_frame_op(void);
static void write_gl_os_audit_frame_inline(void);
int is_active_layout(void);
void refresh_gltpm_window(DesktopWindow* win);
void dispatch_gltpm_button(DesktopWindow* win, int index);
int get_toolbar_height();

void load_emoji_studio_voxels(DesktopWindow *win) {
    char piece_name[256] = "";
    int res = 8;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/emoji-studio/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "active_piece=", 13) == 0) {
                strncpy(piece_name, line + 13, 255);
                piece_name[strcspn(piece_name, "\n\r")] = 0;
            } else if (strncmp(line, "resolution=", 11) == 0) {
                res = atoi(line + 11);
            }
        }
        fclose(f);
    }
    if (piece_name[0] == '\0') return;
    if (strcmp(win->last_active_piece, piece_name) == 0 && win->emoji_res == res && win->emoji_voxels) return;
    strncpy(win->last_active_piece, piece_name, 255);
    win->emoji_res = res;
    if (win->emoji_voxels) free(win->emoji_voxels);
    win->emoji_voxels = calloc(res * res, sizeof(RGBA_Pixel));
    if (!win->emoji_voxels) return;
    snprintf(path, sizeof(path), "%s/projects/emoji-studio/pieces/%s/voxels.csv", project_root, piece_name);
    f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strstr(line, "r,g,b,a")) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            if (idx < res * res) {
                win->emoji_voxels[idx].r = (unsigned char)r;
                win->emoji_voxels[idx].g = (unsigned char)g;
                win->emoji_voxels[idx].b = (unsigned char)b;
                win->emoji_voxels[idx].a = (unsigned char)a;
                idx++;
            }
        }
    }
    fclose(f);
}

static int root_has_anchors(const char* root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

void resolve_root(void) {
    FILE *kvp = NULL;

    if (getcwd(project_root, sizeof(project_root)) && root_has_anchors(project_root)) {
        return;
    }

    kvp = fopen("pieces/locations/location_kvp", "r");
    if (!kvp) return;

    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *v = line + 13;
            v[strcspn(v, "\n\r")] = 0;
            if (strlen(v) > 0 && root_has_anchors(v)) {
                strncpy(project_root, v, MAX_PATH-1);
            }
            break;
        }
    }
    fclose(kvp);
}

/* Check if GL-OS is currently the active layout in CHTPM */
int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) {
        return 1;  /* Default to active if allocation fails */
    }
    
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 1;  /* Default to active if file missing */ }
    
    char line[1024];
    int active = 0;
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "gl_os") || strstr(line, "desktop.chtpm")) {
            active = 1;
        }
    }
    fclose(f);
    free(path);
    return active;
}

/* Clipboard buffer */
static char clipboard_buffer[4096] = "";

/* Copy to system clipboard */
void copy_to_clipboard(const char* text) {
    if (!text || strlen(text) == 0) {
        printf("[CLIPBOARD] Copy failed - empty text\n");
        return;
    }
    
    /* Store in internal buffer */
    strncpy(clipboard_buffer, text, sizeof(clipboard_buffer) - 1);
    printf("[CLIPBOARD] Internal buffer: '%s'\n", clipboard_buffer);
    
    /* Write to clipboard.txt for sanity/debugging */
    FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
    if (cb) {
        fprintf(cb, "%s\n", text);
        fclose(cb);
        printf("[CLIPBOARD] Written to clipboard.txt\n");
    }
    
    /* Try to copy to system clipboard */
    char cmd[4096];
    #ifdef __APPLE__
        /* macOS */
        snprintf(cmd, sizeof(cmd), "printf '%s' | pbcopy 2>/dev/null", text);
        printf("[CLIPBOARD] Running: %s\n", cmd);
    #else
        /* Linux - try xclip first, then xsel */
        snprintf(cmd, sizeof(cmd), "printf '%s' | xclip -selection clipboard 2>/dev/null || printf '%s' | xsel --clipboard 2>/dev/null", text, text);
        printf("[CLIPBOARD] Running: %s\n", cmd);
    #endif
    
    int result = system(cmd);
    printf("[CLIPBOARD] system() returned: %d\n", result);
}

/* Paste from system clipboard */
void paste_from_clipboard(char* dest, size_t dest_size) {
    if (!dest) return;
    
    /* Try to paste from system clipboard to temp file */
    char temp_file[] = "/tmp/gl_os_cb_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return;
    close(fd);
    
    char cmd[4096];
    #ifdef __APPLE__
        snprintf(cmd, sizeof(cmd), "pbpaste > %s 2>/dev/null", temp_file);
    #else
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard -o > %s 2>/dev/null || xsel --clipboard > %s 2>/dev/null", temp_file, temp_file);
    #endif
    
    system(cmd);
    
    /* Read from temp file */
    FILE *f = fopen(temp_file, "r");
    if (f) {
        char line[4096];
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n\r")] = 0;
            strncat(dest, line, dest_size - strlen(dest) - 1);
        }
        fclose(f);
    }
    
    remove(temp_file);
}

void build_session_path(char* dst, size_t sz, const char* rel) {
    snprintf(dst, sz, "%s/pieces/apps/gl_os/session/%s", project_root, rel);
}

void log_master(const char* event, const char* piece) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    FILE *ledger = fopen(master_ledger_path, "a");
    if (ledger) {
        fprintf(ledger, "[%s] GL-OS: %s on %s\n", timestamp, event, piece);
        fclose(ledger);
    }
}

void write_view(void) {
    FILE *fp = fopen(session_view_path, "w");
    if (!fp) return;
    
    fprintf(fp, "DESKTOP_VIEW\n");
    fprintf(fp, "BG: #000044\n");
    fprintf(fp, "WINDOW_COUNT: %d\n", window_count);
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        fprintf(fp, "WINDOW | %d | %d | %d | %d | %d | %s | %s | %d\n", 
                windows[i].id, windows[i].x, windows[i].y, 
                windows[i].w, windows[i].h, 
                windows[i].title, windows[i].type, windows[i].minimized);
    }
    
    fclose(fp);
    
    /* Touch view_changed marker */
    FILE *marker = fopen(session_view_changed_path, "a");
    if (marker) { 
        fprintf(marker, "G\n"); 
        fclose(marker); 
    }
}

void write_session_state(void) {
    char win_list[MAX_LINE] = "";
    char line[256];
    
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        snprintf(line, sizeof(line), "%d:%s(%d,%d),", 
                 windows[i].id, windows[i].title, windows[i].x, windows[i].y);
        if (strlen(win_list) + strlen(line) < MAX_LINE - 50) {
            strcat(win_list, line);
        }
    }
    
    FILE *f = fopen(session_state_path, "w");
    if (f) {
        fprintf(f, "project_id=gl-os\n");
        fprintf(f, "session_status=active\n");
        fprintf(f, "window_count=%d\n", window_count);
        fprintf(f, "windows=%s\n", win_list);
        fprintf(f, "active_window=%d\n", active_window_id);
        fclose(f);
    }
    
    /* Write frame to file (like ASCII-OS) */
    write_gl_os_frame();
}

static void get_current_layout_name(char *out, size_t out_sz) {
    FILE *f = fopen("pieces/display/current_layout.txt", "r");
    if (!f) {
        strncpy(out, "desktop.chtpm", out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    if (!fgets(out, (int)out_sz, f)) {
        strncpy(out, "desktop.chtpm", out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        out[strcspn(out, "\n\r")] = '\0';
    }
    fclose(f);
}

static void write_gl_os_audit_frame_inline(void) {
    if (!gl_os_audit_frame_path) return;

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    char active_layout[128];
    get_current_layout_name(active_layout, sizeof(active_layout));

    FILE *f = fopen(gl_os_audit_frame_path, "w");
    if (!f) return;

    fprintf(f, "GL_OS_AUDIT_FRAME\n");
    fprintf(f, "timestamp=%s\n", timestamp);
    fprintf(f, "project_id=gl-os\n");
    fprintf(f, "active_layout=%s\n", active_layout);
    fprintf(f, "window_count=%d\n", window_count);
    fprintf(f, "active_window_id=%d\n", active_window_id);

    int selected_idx = 0;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == active_window_id && windows[i].show_menu) {
            selected_idx = windows[i].selected_index;
            break;
        }
    }
    fprintf(f, "selected_index=%d\n", selected_idx);

    fprintf(f, "windows:\n");
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        fprintf(f, "  window|id=%d|title=%s|type=%s|x=%d|y=%d|w=%d|h=%d|minimized=%d\n",
                windows[i].id, windows[i].title, windows[i].type,
                windows[i].x, windows[i].y, windows[i].w, windows[i].h,
                windows[i].minimized);
    }

    fprintf(f, "active_menu:\n");
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id != active_window_id || !windows[i].show_menu) continue;
        fprintf(f, "  option_count=%d\n", windows[i].option_count);
        for (int j = 0; j < windows[i].option_count; j++) {
            fprintf(f, "  option|index=%d|selected=%d|label=%s\n",
                    j, (j == windows[i].selected_index) ? 1 : 0, windows[i].menu_options[j]);
        }
        break;
    }

    fclose(f);
}

static int run_gl_os_audit_frame_op(void) {
    char *op_path = NULL;
    int rc = -1;

    if (asprintf(&op_path, "%s/pieces/apps/gl_os/plugins/+x/gl_os_audit_frame.+x", project_root) == -1) {
        return -1;
    }

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) > 0 && WIFEXITED(status)) {
            rc = WEXITSTATUS(status);
        }
    }
#else
    rc = _spawnl(_P_WAIT, op_path, op_path, NULL);
#endif

    free(op_path);
    return rc;
}

void add_history(DesktopWindow* win, const char* text) {
    if (win->history_count < 12) {
        strncpy(win->output_history[win->history_count++], text, 79);
    } else {
        for (int i = 0; i < 11; i++) strcpy(win->output_history[i], win->output_history[i+1]);
        strncpy(win->output_history[11], text, 79);
    }
}

static void cleanup_managed_window(DesktopWindow* win) {
    if (!win || win->managed_pid <= 0) return;

    kill(win->managed_pid, SIGTERM);
    waitpid(win->managed_pid, NULL, WNOHANG);
    win->managed_pid = -1;
}

static void sync_gltpm_menu_from_scene(DesktopWindow* win) {
    if (!win) return;

    win->show_menu = 1;
    win->option_count = 0;
    
    /* Responsive State Sync: Only clobber host state if it's currently inactive 
       or if the scene specifically mandates a mode change (Sovereign override) */
    if (!win->is_map_control || win->gltpm_scene.is_map_control) {
        win->is_map_control = win->gltpm_scene.is_map_control;
    }
    win->camera_mode = win->gltpm_scene.camera_mode;

    /* Sync dynamic camera/xelector coordinates from scene to window struct */
    for(int i=0; i<3; i++) {
        /* SOVEREIGNTY FIX: In Free Mode (4), the Host owns the camera position.
           Don't overwrite local fly coordinates with stale manager state. */
        if (win->camera_mode != 4) {
            win->cam_pos[i] = win->gltpm_scene.cam_pos[i];
            win->cam_rot[i] = win->gltpm_scene.cam_rot[i];
        }
        
        /* SOVEREIGNTY FIX: Don't clobber local xelector movement if we just moved it.
           Gives the manager 1 second to catch up via history file. */
        if (time(NULL) - win->last_xelector_move > 1) {
            win->xelector_pos[i] = win->gltpm_scene.xelector_pos[i];
        }
    }

    for (int i = 0; i < win->gltpm_scene.button_count && i < MAX_TERM_OPTIONS; i++) {
        strncpy(win->menu_options[i], win->gltpm_scene.buttons[i].label, MAX_OPTION_LEN - 1);
        win->menu_options[i][MAX_OPTION_LEN - 1] = '\0';
        win->option_count++;
    }

    if (win->selected_index >= win->option_count) win->selected_index = 0;
}

static void launch_gltpm_manager(DesktopWindow* win) {
    char *manager_path = NULL;
    if (!win || win->managed_pid > 0) return;

    if (asprintf(&manager_path, "%s/projects/%s/manager/+x/%s_manager.+x",
                 project_root, win->project_id, win->project_id) == -1) {
        return;
    }

    if (access(manager_path, F_OK) != 0) {
        free(manager_path);
        return;
    }

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        chdir(project_root);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        for (int fd = STDERR_FILENO + 1; fd < 256; fd++) close(fd);
        execl(manager_path, manager_path, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        win->managed_pid = pid;
    }
#else
    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "start /B \"\" \"%s\"", manager_path);
    system(cmd);
    win->managed_pid = 9999;
#endif

    free(manager_path);
}

void sync_with_global_project(DesktopWindow* win) {
    /* SOVEREIGNTY CHECK: Use PDL flag for dynamic sovereignty (Technical Debt Fix #1) */
    char sov_flag[32] = "false";
    get_pdl_value(win->project_id, "STATE", "SOVEREIGN", sov_flag, sizeof(sov_flag));
    if (strcmp(sov_flag, "true") == 0) return;

    /* Legacy hardcoded list (to be removed in Stage 3) */
    if (strcmp(win->project_id, "chtmgl-alpha") == 0 ||
        strcmp(win->project_id, "chtmgl-beta") == 0 ||
        strcmp(win->project_id, "media-editor") == 0 ||
        strcmp(win->project_id, "op-ed-gl") == 0) {
        return;
    }

    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    FILE *f = fopen(state_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_id") == 0) {
                    if (strcmp(win->project_id, v) != 0) {
                        /* Project switched in terminal - follow it! */
                        strncpy(win->project_id, v, sizeof(win->project_id)-1);
                        win->gltpm_scene.loaded = 0; /* Force reload */
                        
                        /* Probe for layout */
                        char probe[MAX_PATH];
                        snprintf(probe, sizeof(probe), "projects/%s/layouts/main.gltpm", v);
                        char abs_p[MAX_PATH]; snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                        if (access(abs_p, F_OK) == 0) strcpy(win->gltpm_layout_path, probe);
                        else {
                            snprintf(probe, sizeof(probe), "projects/%s/layouts/%s.gltpm", v, v);
                            snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                            if (access(abs_p, F_OK) == 0) strcpy(win->gltpm_layout_path, probe);
                        }
                    }
                }
            }
        }
        fclose(f);
    }
}

void refresh_gltpm_window(DesktopWindow* win) {
    char *frame_marker = NULL;
    char *layout_marker = NULL;
    struct stat st;
    int should_reload = 0;

    if (!win || strcmp(win->type, "gltpm_app") != 0) return;

    /* SYNC: Follow global project if this window is a mirror/active host */
    sync_with_global_project(win);

    if (!win->gltpm_scene.loaded) {
        should_reload = 1;
    }

    if (asprintf(&frame_marker, "%s/projects/%s/session/frame_changed.txt",
                 project_root, win->project_id) != -1) {
        if (stat(frame_marker, &st) == 0) {
            if (st.st_size != win->gltpm_frame_marker_size) {
                win->gltpm_frame_marker_size = st.st_size;
                should_reload = 1;
            }
        }
    }
    
    /* GLOBAL Pulse Check */
    char *global_marker = NULL;
    if (asprintf(&global_marker, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        if (stat(global_marker, &st) == 0) {
            if (st.st_size != win->gltpm_global_marker_size) {
                 win->gltpm_global_marker_size = st.st_size;
                 should_reload = 1;
            }
        }
        free(global_marker);
    }

    if (asprintf(&layout_marker, "%s/projects/%s/session/layout_changed.txt",
                 project_root, win->project_id) != -1) {
        if (stat(layout_marker, &st) == 0) {
            if (st.st_size != win->gltpm_layout_marker_size) {
                win->gltpm_layout_marker_size = st.st_size;
                should_reload = 1;
            }
        }
    }

    if (should_reload &&
        gltpm_load_scene(&win->gltpm_scene, project_root, win->project_id, win->gltpm_layout_path)) {
        if (strlen(win->gltpm_scene.title) > 0) {
            strncpy(win->title, win->gltpm_scene.title, MAX_WIN_TITLE - 1);
            win->title[MAX_WIN_TITLE - 1] = '\0';
        }
        if (win->gltpm_scene.w > 0) win->w = (int)win->gltpm_scene.w;
        if (win->gltpm_scene.h > 0) win->h = (int)win->gltpm_scene.h + 25; /* Add titlebar */
        sync_gltpm_menu_from_scene(win);
    }

    if (frame_marker) free(frame_marker);
    if (layout_marker) free(layout_marker);
}

void dispatch_gltpm_button(DesktopWindow* win, int index) {
    char *history_path = NULL;
    char audit[200];

    if (!win || strcmp(win->type, "gltpm_app") != 0) return;
    if (index < 0 || index >= win->gltpm_scene.button_count) return;

    /* Responsive INTERACT handling (Concern #3) */
    if (strcmp(win->gltpm_scene.buttons[index].onClick, "INTERACT") == 0) {
        win->is_map_control = 1;
        add_history(win, "Map Control Activated (Host).");

        /* INJECT: Notify manager via project history */
        const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
        if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
            FILE *hf = fopen(history_path, "a");
            if (hf) {
                time_t rawtime; struct tm *timeinfo; char timestamp[64];
                time(&rawtime); timeinfo = localtime(&rawtime);
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                fprintf(hf, "[%s] COMMAND: INTERACT\n", timestamp);
                fclose(hf);
            }
            free(history_path);
        }
        glutPostRedisplay();
        return;
    }

    if (asprintf(&history_path, "%s/projects/%s/session/history.txt",
                 project_root, win->project_id) == -1) {
        return;
    }

    FILE *hf = fopen(history_path, "a");
    if (hf) {
        time_t rawtime;
        struct tm *timeinfo;
        char timestamp[64];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        if (strcmp(win->gltpm_scene.buttons[index].onClick, "INTERACT") == 0) {
            fprintf(hf, "[%s] COMMAND: INTERACT\n", timestamp);
        } else {
            fprintf(hf, "[%s] COMMAND: %s\n", timestamp, win->gltpm_scene.buttons[index].onClick);
        }
        fclose(hf);
    }

    snprintf(audit, sizeof(audit), "[GLTPM] %s", win->gltpm_scene.buttons[index].label);
    add_history(win, audit);
    free(history_path);
    write_gl_os_frame();
}

/* Write state file for parser variable substitution */
void write_gl_os_state(void) {
    FILE *f = fopen("pieces/apps/gl_os/session/state.txt", "w");
    if (!f) return;

    fprintf(f, "window_count=%d\n", window_count);
    fprintf(f, "active_window_id=%d\n", active_window_id);
    /* Desktop state variables for layout substitution */
    fprintf(f, "bg_color=%s\n", "#000044");
    fprintf(f, "name=Desktop\n");
    fprintf(f, "type=desktop\n");
    fprintf(f, "pos_x=0\n");
    fprintf(f, "pos_y=0\n");
    fprintf(f, "width=800\n");
    fprintf(f, "height=600\n");
    fprintf(f, "session_status=active\n");

    /* Get selected index from active menu-driven window */
    int selected_idx = 0;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == active_window_id && windows[i].show_menu) {
            selected_idx = windows[i].selected_index;
            break;
        }
    }
    fprintf(f, "selected_index=%d\n", selected_idx);

    /* Debug info */
    char debug_info[256];
    snprintf(debug_info, sizeof(debug_info), "M(%d,%d) S=%d", mouse_x, mouse_y, 0);
    fprintf(f, "debug_info=%s\n", debug_info);

    /* Build window list with selection indicators */
    char window_list[512] = "";
    for (int i = 0; i < window_count; i++) {
        if (windows[i].active) {
            char line[128];
            const char *indicator = (windows[i].id == active_window_id) ? "[>]" : "[ ]";
            snprintf(line, sizeof(line), "  %s %d. %-20s\n",
                     indicator, i + 1, windows[i].title);
            if (strlen(window_list) + strlen(line) < sizeof(window_list) - 50) {
                strcat(window_list, line);
            }
        }
    }
    fprintf(f, "window_list=%s", window_list);

    /* Build menu options with selection indicator - ASCII-OS style */
    char menu_options_str[1024] = "";
    int opt_count = 0;
    char (*win_opts)[MAX_OPTION_LEN] = NULL;

    /* Get menu options from the active menu-driven window */
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == active_window_id && windows[i].show_menu) {
            opt_count = windows[i].option_count;
            win_opts = windows[i].menu_options;
            break;
        }
    }

    if (win_opts && opt_count > 0) {
        /* Build menu with [>] indicator on selected item (ASCII-OS style) */
        for (int i = 0; i < opt_count; i++) {
            char line[128];
            const char *indicator = (i == selected_idx) ? "[>]" : "[ ]";
            snprintf(line, sizeof(line), "║    %s %d. [%-20s]           ║\n", 
                     indicator, i + 1, win_opts[i]);
            if (strlen(menu_options_str) + strlen(line) < sizeof(menu_options_str) - 1) {
                strcat(menu_options_str, line);
            }
        }
    } else {
        /* Fallback for no active terminal */
        strcpy(menu_options_str, "║    No Active Terminal                      ║\n");
    }
    fprintf(f, "menu_options=%s", menu_options_str);

    /* Read desktop view from view.txt for desktop_view variable */
    FILE *vf = fopen("pieces/apps/gl_os/session/view.txt", "r");
    if (vf) {
        char view_content[4096] = "";
        char vline[256];
        while (fgets(vline, sizeof(vline), vf)) {
            /* Replace newlines with | delimiter for parser */
            vline[strcspn(vline, "\n")] = '|';
            if (strlen(view_content) + strlen(vline) < sizeof(view_content) - 50) {
                strcat(view_content, vline);
            }
        }
        fprintf(f, "desktop_view=%s", view_content);
        fclose(vf);
    }

    /* Default response */
    fprintf(f, "gl_response=\n");

    fclose(f);
}

/* Simple parser - substitute ${var} from state file */
void parse_and_write_frame(void) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    /* Read state file */
    FILE *state = fopen("pieces/apps/gl_os/session/state.txt", "r");
    if (!state) return;
    
    char window_count_str[32] = "0";
    char active_window_str[32] = "0";
    char window_list[512] = "";
    char debug_info_str[256] = "";
    char menu_options_str[1024] = "";

    char line[MAX_LINE];
    char *current_val = NULL;
    int max_len = 0;
    while (fgets(line, sizeof(line), state)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *value = eq + 1;
            
            if (strcmp(key, "window_count") == 0) { strncpy(window_count_str, value, 31); window_count_str[strcspn(window_count_str, "\n\r")] = 0; current_val = NULL; }
            else if (strcmp(key, "active_window_id") == 0) { strncpy(active_window_str, value, 31); active_window_str[strcspn(active_window_str, "\n\r")] = 0; current_val = NULL; }
            else if (strcmp(key, "window_list") == 0) { strncpy(window_list, value, 511); current_val = window_list; max_len = 511; }
            else if (strcmp(key, "debug_info") == 0) { strncpy(debug_info_str, value, 255); debug_info_str[strcspn(debug_info_str, "\n\r")] = 0; current_val = debug_info_str; max_len = 255; }
            else if (strcmp(key, "menu_options") == 0) { strncpy(menu_options_str, value, 1023); current_val = menu_options_str; max_len = 1023; }
            else current_val = NULL;
        } else if (current_val) {
            /* Continuation line */
            strncat(current_val, line, max_len - strlen(current_val));
        }
    }
    fclose(state);
    
    /* Strip trailing newline from menu_options_str if it exists, but the substitution might need it? 
       Actually, the layout has ${menu_options} on its own line. */
    menu_options_str[strcspn(menu_options_str, "\r")] = 0;
    /* We keep the \n in menu_options_str for multi-line */
    
    /* Read layout file and substitute variables */
    FILE *layout = fopen("pieces/apps/gl_os/layouts/terminal.chtml", "r");
    if (!layout) return;
    
    /* Write frame to file */
    FILE *frame_file = fopen(gl_os_frame_path, "w");
    if (!frame_file) { fclose(layout); return; }
    
    /* DEBUG: Also print to stdout */
    printf("\n=== FRAME DEBUG OUTPUT ===\n");
    
    while (fgets(line, sizeof(line), layout)) {
        /* Skip empty lines */
        if (strlen(line) < 2) continue;
        
        /* Substitute ${var} */
        char output[MAX_LINE * 2];
        strcpy(output, line);
        
        /* Simple substitution */
        char *p;
        while ((p = strstr(output, "${window_count}")) != NULL) {
            memmove(p + strlen(window_count_str), p + 15, strlen(p) - 14);
            memcpy(p, window_count_str, strlen(window_count_str));
        }
        while ((p = strstr(output, "${active_window_id}")) != NULL) {
            memmove(p + strlen(active_window_str), p + 19, strlen(p) - 18);
            memcpy(p, active_window_str, strlen(active_window_str));
        }
        while ((p = strstr(output, "${debug_info}")) != NULL) {
            memmove(p + strlen(debug_info_str), p + 13, strlen(p) - 12);
            memcpy(p, debug_info_str, strlen(debug_info_str));
        }
        while ((p = strstr(output, "${window_list}")) != NULL) {
            /* Handle multi-line window_list */
            char rest[MAX_LINE];
            strcpy(rest, p + 14);
            strcpy(p, window_list);
            strcat(p, rest);
        }
        while ((p = strstr(output, "${menu_options}")) != NULL) {
            /* Handle multi-line menu_options */
            char rest[MAX_LINE];
            strcpy(rest, p + 15);
            strcpy(p, menu_options_str);
            strcat(p, rest);
        }

        fprintf(frame_file, "%s", output);
        printf("%s", output);  /* DEBUG: Print frame line */
    }
    
    fclose(layout);
    fclose(frame_file);
    
    printf("=== END FRAME DEBUG ===\n\n");
    fflush(stdout);
    
    /* Append to frame history */
    FILE *history = fopen(gl_os_frame_history, "a");
    if (history) {
        fprintf(history, "\n--- FRAME UPDATE at %s ---\n", timestamp);
        frame_file = fopen(gl_os_frame_path, "r");
        if (frame_file) {
            while (fgets(line, sizeof(line), frame_file)) {
                fprintf(history, "%s", line);
            }
            fclose(frame_file);
        }
        fclose(history);
    }
    
    /* Touch marker file */
    FILE *pulse = fopen(gl_os_frame_pulse, "a");
    if (pulse) {
        fprintf(pulse, "G\n");
        fclose(pulse);
    }
}

void init_terminal_menu(DesktopWindow* win) {
    /* Initialize default menu options (joystick-first UX) */
    win->option_count = 4;
    win->selected_index = 0;
    win->show_menu = 1;
    win->menu_context = CTX_MAIN;
    strcpy(win->menu_options[0], "Project Explorer");
    strcpy(win->menu_options[1], "Debug Layouts (Dev Only)");
    strcpy(win->menu_options[2], "Terminal Apps");
    strcpy(win->menu_options[3], "Settings");

    /* Initialize scroll state (Linux terminal: thumb at bottom = newest) */
    win->scroll_offset = 0;      /* Start at bottom (newest content) */
    win->max_scroll = 0;         /* Will be calculated during render */

    /* Initialize text selection state */
    win->is_selecting = 0;
    win->select_start_x = 0;
    win->select_start_y = 0;
    win->select_end_x = 0;
    win->select_end_y = 0;
    win->selected_text[0] = '\0';
    win->selection_active = 0;

    /* Count frames in history file */
    FILE *hf = fopen(gl_os_frame_history, "r");
    if (hf) {
        char line[MAX_LINE];
        win->history_count = 0;
        while (fgets(line, sizeof(line), hf)) {
            if (strstr(line, "--- FRAME UPDATE at")) {
                win->history_count++;
            }
        }
        fclose(hf);
        printf("[DEBUG] Terminal %d: Found %d frames in history\n", win->id, win->history_count);
    }
}

void execute_option(DesktopWindow* win, int index) {
    if (index < 0 || index >= win->option_count) return;
    
    /* Add selection to history */
    char selection[200];
    snprintf(selection, sizeof(selection), "[EXECUTE] %s", win->menu_options[index]);
    add_history(win, selection);

    if (strcmp(win->type, "gltpm_app") == 0) {
        dispatch_gltpm_button(win, index);
        return;
    }

    if (strcmp(win->type, "emoji_studio") == 0) {
        if (index >= 1 && index <= 4) {
            int res_opts[] = {0, 8, 16, 32, 64};
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "SET_RES %d", res_opts[index]);
            add_history(win, cmd);
            
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s/projects/emoji-studio/session/history.txt", project_root);
            FILE *hf = fopen(path, "a");
            if (hf) {
                fprintf(hf, "COMMAND: %s\n", cmd);
                fclose(hf);
            }
        }
        return;
    }
    
    /* Project Mirror Context Handling */
    if (strcmp(win->type, "project_mirror") == 0) {
        if (index == 0) {
            /* Control Map */
            win->is_map_control = 1;
            add_history(win, "Map Control Activated. WASD to move, ZX for vertical, ESC to exit.");
        } else {
            /* Legacy mirror windows are now display-only fallbacks */
            add_history(win, "Action selected. Note: Use Project Explorer for full interactivity.");
        }
    }
    /* Standard Terminal Contexts */
    else if (win->menu_context == CTX_MAIN) {
        if (index == 0) {
            /* Switch to Project Explorer Context */
            win->menu_context = CTX_DESKTOP_APPS;
            win->selected_index = 0;
            int num_projects = gl_project_count;
            if (num_projects > MAX_TERM_OPTIONS - 2) num_projects = MAX_TERM_OPTIONS - 2;
            win->option_count = num_projects + 2;
            for (int i = 0; i < num_projects; i++) {
                strcpy(win->menu_options[i], gl_projects[i].id);
            }
            strcpy(win->menu_options[num_projects], "3D Cube Demo");
            strcpy(win->menu_options[num_projects + 1], "back");
            add_history(win, "Entering Project Explorer...");
        } else if (index == 1) {
            /* Switch to Debug Layouts Context */
            win->menu_context = CTX_GLTPM_APPS;
            win->selected_index = 0;
            win->option_count = 5;
            strcpy(win->menu_options[0], "demo_world.gltpm");
            strcpy(win->menu_options[1], "fuzz-op-gl.gltpm");
            strcpy(win->menu_options[2], "op-ed-gl.gltpm");
            strcpy(win->menu_options[3], "piececraft-3d.gltpm");
            strcpy(win->menu_options[4], "back");
            add_history(win, "Entering Debug Layouts (Developer Mode)...");
        } else if (index == 2) {
            /* Terminal Apps */
            add_history(win, "Terminal Apps (WIP)");
        } else if (index == 3) {
            /* Settings */
            win->menu_context = CTX_SETTINGS;
            win->selected_index = 0;
            win->option_count = 3;
            snprintf(win->menu_options[0], MAX_OPTION_LEN, "Fullscreen: %s", g_fullscreen_enabled ? "ON" : "OFF");
            snprintf(win->menu_options[1], MAX_OPTION_LEN, "Font Scale: %.1fx", g_font_scale_factor);
            strcpy(win->menu_options[2], "back");
            add_history(win, "Entering Settings...");
        }
    } else if (win->menu_context == CTX_SETTINGS) {
        if (index == 0) {
            /* Toggle Fullscreen */
            g_fullscreen_enabled = !g_fullscreen_enabled;
            if (g_fullscreen_enabled) {
                glutFullScreen();
                add_history(win, "Fullscreen ENABLED.");
            } else {
                glutPositionWindow(100, 100);
                glutReshapeWindow(window_width, window_height);
                add_history(win, "Fullscreen DISABLED.");
            }
            snprintf(win->menu_options[0], MAX_OPTION_LEN, "Fullscreen: %s", g_fullscreen_enabled ? "ON" : "OFF");
        } else if (index == 1) {
            /* Cycle Font Scale */
            g_font_scale_factor += 0.2f;
            if (g_font_scale_factor > 2.1f) g_font_scale_factor = 0.8f;
            snprintf(win->menu_options[1], MAX_OPTION_LEN, "Font Scale: %.1fx", g_font_scale_factor);
            char msg[64];
            snprintf(msg, sizeof(msg), "Font Scale set to %.1fx (Requires Refresh)", g_font_scale_factor);
            add_history(win, msg);
        } else if (index == 2) {
            /* Back to Main Menu */
            win->menu_context = CTX_MAIN;
            win->selected_index = 0;
            win->option_count = 4;
            strcpy(win->menu_options[0], "Project Explorer");
            strcpy(win->menu_options[1], "Debug Layouts (Dev Only)");
            strcpy(win->menu_options[2], "Terminal Apps");
            strcpy(win->menu_options[3], "Settings");
            add_history(win, "Returned to Main Menu.");
        }
    } else if (win->menu_context == CTX_DESKTOP_APPS) {
        if (index < gl_project_count) {
            char history_msg[256];
            snprintf(history_msg, sizeof(history_msg), "Launching %s...", gl_projects[index].id);
            add_history(win, history_msg);
            
            /* DYNAMIC LOADER: Read entry_layout from PDL (ASCII-Parity) */
            char entry_layout[MAX_PATH] = "";
            get_pdl_value(gl_projects[index].id, "META", "entry_layout", entry_layout, sizeof(entry_layout));
            
            if (entry_layout[0] != '\0') {
                create_gltpm_window(gl_projects[index].id, entry_layout, gl_projects[index].name);
            } else {
                /* Intelligent Fallback Convention (Prioritize .chtmgl) */
                char probe[MAX_PATH];
                int found_layout = 0;
                
                /* 1. <id>.chtmgl */
                snprintf(probe, sizeof(probe), "projects/%s/layouts/%s.chtmgl", gl_projects[index].id, gl_projects[index].id);
                char abs_p[MAX_PATH]; snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                if (access(abs_p, F_OK) == 0) { create_gltpm_window(gl_projects[index].id, probe, gl_projects[index].name); found_layout = 1; }
                
                /* 2. index.chtmgl */
                if (!found_layout) {
                    snprintf(probe, sizeof(probe), "projects/%s/layouts/index.chtmgl", gl_projects[index].id);
                    snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                    if (access(abs_p, F_OK) == 0) { create_gltpm_window(gl_projects[index].id, probe, gl_projects[index].name); found_layout = 1; }
                }

                /* 3. main.chtmgl */
                if (!found_layout) {
                    snprintf(probe, sizeof(probe), "projects/%s/layouts/main.chtmgl", gl_projects[index].id);
                    snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                    if (access(abs_p, F_OK) == 0) { create_gltpm_window(gl_projects[index].id, probe, gl_projects[index].name); found_layout = 1; }
                }

                /* 4. Legacy .gltpm probing */
                if (!found_layout) {
                    snprintf(probe, sizeof(probe), "projects/%s/layouts/%s.gltpm", gl_projects[index].id, gl_projects[index].id);
                    snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                    if (access(abs_p, F_OK) == 0) { create_gltpm_window(gl_projects[index].id, probe, gl_projects[index].name); found_layout = 1; }
                }
                
                if (!found_layout) {
                     snprintf(probe, sizeof(probe), "projects/%s/layouts/main.gltpm", gl_projects[index].id);
                     snprintf(abs_p, sizeof(abs_p), "%s/%s", project_root, probe);
                     if (access(abs_p, F_OK) == 0) {
                         create_gltpm_window(gl_projects[index].id, probe, gl_projects[index].name);
                     } else {
                         /* Special Built-in Logic */
                         if (strcmp(gl_projects[index].id, "emoji-studio") == 0) {
                             create_emoji_studio_window();
                         } else {
                             /* Generic Mirror fallback */
                             create_mirrored_window(gl_projects[index].id, gl_projects[index].name);
                         }
                     }
                }
            }
        } else if (index == gl_project_count) {
            add_history(win, "Launching 3D Cube...");
            create_cube_window();
        } else if (index == gl_project_count + 1) {
            /* Back to Main Menu */
            win->menu_context = CTX_MAIN;
            win->selected_index = 0;
            win->option_count = 4;
            strcpy(win->menu_options[0], "Project Explorer");
            strcpy(win->menu_options[1], "Debug Layouts (Dev Only)");
            strcpy(win->menu_options[2], "Terminal Apps");
            strcpy(win->menu_options[3], "Settings");
            add_history(win, "Returned to Main Menu.");
        }
    }
 else if (win->menu_context == CTX_GLTPM_APPS) {
        if (index == 0) {
            add_history(win, "Launching demo_world.gltpm...");
            create_gltpm_window("gltpm-demo",
                                "projects/gltpm-demo/layouts/demo_world.gltpm",
                                "Demo World GLTPM");
        } else if (index == 1) {
            add_history(win, "Launching fuzz-op-gl.gltpm...");
            create_gltpm_window("fuzz-op-gl",
                                "projects/fuzz-op-gl/layouts/main.gltpm",
                                "Fuzz-Op-GL (Sovereign)");
        } else if (index == 2) {
            add_history(win, "Launching op-ed-gl.gltpm...");
            create_gltpm_window("op-ed-gl",
                                "projects/op-ed-gl/layouts/main.gltpm",
                                "GL-Op-Ed (God Mode)");
        } else if (index == 3) {
            add_history(win, "Launching piececraft-3d.gltpm...");
            create_gltpm_window("piececraft-3d",
                                "projects/piececraft-3d/layouts/main.gltpm",
                                "Piececraft-3D (Sovereign)");
        } else if (index == 4) {
            win->menu_context = CTX_MAIN;
            win->selected_index = 0;
            win->option_count = 4;
            strcpy(win->menu_options[0], "Project Explorer");
            strcpy(win->menu_options[1], "Debug Layouts (Dev Only)");
            strcpy(win->menu_options[2], "Terminal Apps");
            strcpy(win->menu_options[3], "Settings");
            add_history(win, "Returned to Main Menu.");
        }
    }
    
    /* Clear input buffer */
    win->input_buffer[0] = '\0';

    /* Refresh frame */
    write_gl_os_state();
    parse_and_write_frame();
}

void move_selection(DesktopWindow* win, int direction) {
    if (!win->show_menu) return;
    
    win->selected_index += direction;
    if (win->selected_index < 0) win->selected_index = 0;
    if (win->selected_index >= win->option_count) win->selected_index = win->option_count - 1;
}

void create_mirrored_window(const char* project_id, const char* title) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 80 + (window_count * 20);
    windows[idx].y = 80 + (window_count * 20);
    windows[idx].w = 400;
    windows[idx].h = 400;
    clamp_window_bounds(&windows[idx]);
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    strncpy(windows[idx].title, title, MAX_WIN_TITLE-1);
    strncpy(windows[idx].type, "project_mirror", sizeof(windows[idx].type)-1);
    strncpy(windows[idx].project_id, project_id, sizeof(windows[idx].project_id)-1);
    
    active_window_id = windows[idx].id;
    
    /* Initialize Camera & Map Control state */
    windows[idx].is_map_control = 0;
    windows[idx].camera_mode = 4; /* Default: Free Camera */
    windows[idx].cam_pos[0] = 0.0f; windows[idx].cam_pos[1] = 0.0f; windows[idx].cam_pos[2] = 0.0f;
    windows[idx].cam_rot[0] = 30.0f; windows[idx].cam_rot[1] = 0.0f; windows[idx].cam_rot[2] = 0.0f;

    /* Initialize Project Menu (Generic Fallback) */
    windows[idx].show_menu = 1;
    windows[idx].selected_index = 0;
    windows[idx].option_count = 1;
    strcpy(windows[idx].menu_options[0], "Control Map");

    log_master("WINDOW_CREATE", project_id);
    write_view();
    write_session_state();
    
    printf("[GL-OS] Launched mirrored window for %s (%s)\n", project_id, title);
}

void create_emoji_studio_window(void) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    memset(&windows[idx], 0, sizeof(windows[idx]));
    windows[idx].id = next_window_id++;
    windows[idx].x = 100;
    windows[idx].y = 100;
    windows[idx].w = 600;
    windows[idx].h = 500;
    clamp_window_bounds(&windows[idx]);
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    strncpy(windows[idx].title, "Emoji Studio", MAX_WIN_TITLE-1);
    strncpy(windows[idx].type, "emoji_studio", sizeof(windows[idx].type)-1);
    strncpy(windows[idx].project_id, "emoji-studio", sizeof(windows[idx].project_id)-1);
    
    active_window_id = windows[idx].id;
    launch_gltpm_manager(&windows[idx]);
    
    windows[idx].is_map_control = 0;
    windows[idx].camera_mode = 4;
    windows[idx].cam_pos[0] = 0.0f; windows[idx].cam_pos[1] = 0.0f; windows[idx].cam_pos[2] = 0.0f;
    windows[idx].cam_rot[0] = 20.0f; windows[idx].cam_rot[1] = 45.0f; windows[idx].cam_rot[2] = 0.0f;

    windows[idx].show_menu = 1;
    windows[idx].selected_index = 0;
    windows[idx].option_count = 5;
    strcpy(windows[idx].menu_options[0], "Select Emoji (WIP)");
    strcpy(windows[idx].menu_options[1], "Res: 8");
    strcpy(windows[idx].menu_options[2], "Res: 16");
    strcpy(windows[idx].menu_options[3], "Res: 32");
    strcpy(windows[idx].menu_options[4], "Res: 64");

    log_master("WINDOW_CREATE", "emoji-studio");
    write_view();
    write_session_state();
}

void create_gltpm_window(const char* project_id, const char* layout_path, const char* title) {
    if (window_count >= MAX_WINDOWS) return;

    int idx = window_count++;
    memset(&windows[idx], 0, sizeof(windows[idx]));
    windows[idx].id = next_window_id++;
    windows[idx].active_node_index = -1;
    windows[idx].x = 100 + (window_count * 20);
    windows[idx].y = 90 + (window_count * 20);
    windows[idx].w = 1024;
    windows[idx].h = 768;
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].managed_pid = -1;
    windows[idx].selected_index = 0;
    windows[idx].show_menu = 1;
    windows[idx].gltpm_frame_marker_size = -1;
    windows[idx].gltpm_global_marker_size = -1;
    windows[idx].gltpm_layout_marker_size = -1;
    memset(&windows[idx].gltpm_scene, 0, sizeof(windows[idx].gltpm_scene));

    strncpy(windows[idx].title, title, MAX_WIN_TITLE - 1);
    strncpy(windows[idx].type, "gltpm_app", sizeof(windows[idx].type) - 1);
    strncpy(windows[idx].project_id, project_id, sizeof(windows[idx].project_id) - 1);
    strncpy(windows[idx].gltpm_layout_path, layout_path, sizeof(windows[idx].gltpm_layout_path) - 1);

    clamp_window_bounds(&windows[idx]);
    active_window_id = windows[idx].id;
    launch_gltpm_manager(&windows[idx]);
    refresh_gltpm_window(&windows[idx]);

    log_master("WINDOW_CREATE", project_id);
    write_view();
    write_session_state();
}

void create_terminal_window(void) {
    if (window_count >= MAX_WINDOWS) return;

    static int term_count = 0;
    term_count++;

    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 50 + (term_count * 20);
    windows[idx].y = 50 + (term_count * 20);
    windows[idx].w = 400;
    windows[idx].h = 300;
    clamp_window_bounds(&windows[idx]);
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].input_buffer[0] = '\0';
    windows[idx].history_count = 0;
    snprintf(windows[idx].title, MAX_WIN_TITLE, "Terminal %d", term_count);
    strncpy(windows[idx].type, "terminal", sizeof(windows[idx].type)-1);

    /* Initialize menu navigation state (joystick-first UX) */
    init_terminal_menu(&windows[idx]);

    /* Add multiple menu options for joystick navigation testing */
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "  CHTPM PROJECT EXPLORER");
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "  1. Project Explorer");
    add_history(&windows[idx], "  2. Standalone Layouts");
    add_history(&windows[idx], "  3. Terminal Apps");
    add_history(&windows[idx], "========================================");
    add_history(&windows[idx], "Use joystick UP/DOWN to navigate");
    add_history(&windows[idx], "Press ENTER to execute selected option");

    active_window_id = windows[idx].id;

    log_master("WINDOW_CREATE", "terminal");
    write_view();
    write_session_state();
}

void create_cube_window(void) {
    if (window_count >= MAX_WINDOWS) return;
    
    int idx = window_count++;
    windows[idx].id = next_window_id++;
    windows[idx].x = 100 + (window_count * 20);
    windows[idx].y = 100 + (window_count * 20);
    windows[idx].w = 300;
    windows[idx].h = 300;
    clamp_window_bounds(&windows[idx]);
    windows[idx].active = 1;
    windows[idx].minimized = 0;
    windows[idx].rotation_x = 0;
    windows[idx].rotation_y = 0;
    snprintf(windows[idx].title, MAX_WIN_TITLE, "3D Cube Test");
    strncpy(windows[idx].type, "cube", sizeof(windows[idx].type)-1);
    
    active_window_id = windows[idx].id;
    log_master("WINDOW_CREATE", "cube");
    write_view();
    write_session_state();
}

void process_terminal_command(DesktopWindow* win) {
    char cmd[256];
    strncpy(cmd, win->input_buffer, 255);
    win->input_buffer[0] = '\0';
    
    char prompt[300];
    snprintf(prompt, sizeof(prompt), "$ %s", cmd);
    add_history(win, prompt);
    
    if (strcmp(cmd, "help") == 0) {
        add_history(win, "Available Categories:");
        add_history(win, "  []1.desktop  - Apps inside GL-OS");
        add_history(win, "  []2.terminal - CHTPM CLI Apps");
    } else if (strcmp(cmd, "1") == 0 || strcmp(cmd, "desktop") == 0) {
        add_history(win, "Desktop Apps:");
        add_history(win, "  - cube (Launch 3D Demo)");
        add_history(win, "  - folder (WIP)");
    } else if (strcmp(cmd, "2") == 0 || strcmp(cmd, "terminal") == 0) {
        add_history(win, "Terminal Apps:");
        add_history(win, "  - fuzzpet");
        add_history(win, "  - projects");
        add_history(win, "  - proc (Monitor)");
    } else if (strcmp(cmd, "cube") == 0) {
        add_history(win, "Launching 3D Cube App...");
        create_cube_window();
    } else if (strcmp(cmd, "clear") == 0) {
        win->history_count = 0;
    } else if (strlen(cmd) > 0) {
        add_history(win, "Unknown command. Type 'help' for options.");
    }
}

void close_window(int win_id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == win_id) {
            cleanup_managed_window(&windows[i]);
            if (windows[i].emoji_voxels) free(windows[i].emoji_voxels);
            windows[i].active = 0;
            log_master("WINDOW_CLOSE", windows[i].title);
            
            if (active_window_id == win_id) {
                active_window_id = -1;
                /* Find next active window */
                for (int j = 0; j < window_count; j++) {
                    if (windows[j].active && !windows[j].minimized) {
                        active_window_id = windows[j].id;
                        break;
                    }
                }
            }
            
            write_view();
            write_session_state();
            return;
        }
    }
}

int find_window_at(int x, int y) {
    /* Search from top (last drawn) to bottom */
    for (int i = window_count - 1; i >= 0; i--) {
        if (!windows[i].active || windows[i].minimized) continue;
        /* y is GLUT (top-down), win->y is GLUT (top-down) */
        if (x >= windows[i].x && x <= windows[i].x + windows[i].w &&
            y >= windows[i].y && y <= windows[i].y + windows[i].h) {
            return i; /* Return index */
        }
    }
    return -1;
}

int is_in_titlebar(int x, int y, DesktopWindow* win) {
    int titlebar_h = 25;
    return (y >= win->y && y <= win->y + titlebar_h);
}

void draw_direction_cube(float size) {
    float s = size / 2.0f;
    glLineWidth(2.0f);
    glBegin(GL_QUADS);
    /* Front (Z+) - Blue */
    glColor3f(0.2f, 0.2f, 1.0f); glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    /* Back (Z-) - Dark Blue */
    glColor3f(0, 0, 0.5f); glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    /* Top (Y+) - Green */
    glColor3f(0.2f, 1.0f, 0.2f); glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    /* Bottom (Y-) - Dark Green */
    glColor3f(0, 0.5f, 0); glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    /* Right (X+) - Red */
    glColor3f(1.0f, 0.2f, 0.2f); glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    /* Left (X-) - Dark Red */
    glColor3f(0.5f, 0, 0); glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();

    /* Draw RGB Axis Arrows */
    glBegin(GL_LINES);
    /* X - Red */
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(s*2, 0, 0);
    /* Y - Green */
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, s*2, 0);
    /* Z - Blue */
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, s*2);
    glEnd();
}

void draw_xelector(float size) {
    float s = size / 2.0f;
    glLineWidth(3.0f);
    
    /* Pulse effect based on time */
    float pulse = 0.7f + 0.3f * (float)sin(global_rotation * 0.2f);
    glColor3f(1.0f * pulse, 1.0f * pulse, 0); /* Yellow glow */
    
    glBegin(GL_LINES);
    /* Bottom */
    glVertex3f(-s, 0, -s); glVertex3f( s, 0, -s);
    glVertex3f( s, 0, -s); glVertex3f( s, 0,  s);
    glVertex3f( s, 0,  s); glVertex3f(-s, 0,  s);
    glVertex3f(-s, 0,  s); glVertex3f(-s, 0, -s);
    /* Top */
    glVertex3f(-s, s*2, -s); glVertex3f( s, s*2, -s);
    glVertex3f( s, s*2, -s); glVertex3f( s, s*2,  s);
    glVertex3f( s, s*2,  s); glVertex3f(-s, s*2,  s);
    glVertex3f(-s, s*2,  s); glVertex3f(-s, s*2, -s);
    /* Verticals */
    glVertex3f(-s, 0, -s); glVertex3f(-s, s*2, -s);
    glVertex3f( s, 0, -s); glVertex3f( s, s*2, -s);
    glVertex3f( s, 0,  s); glVertex3f( s, s*2,  s);
    glVertex3f(-s, 0,  s); glVertex3f(-s, s*2,  s);
    glEnd();
}

void draw_rect(float x, float y, float w, float h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

/* Draw text clipped to max_width - adds "..." if too long */
void draw_text_clipped(float x, float y, float max_width, const char* text) {
    /* Measure text width (approximate: dynamic font width) */
    int f_width = get_font_width();
    int text_width = strlen(text) * f_width;
    void* font = get_font_regular();
    
    if (text_width <= max_width) {
        /* Text fits - draw normally */
        glRasterPos2f(x, y);
        for (const char* p = text; *p; p++)
            glutBitmapCharacter(font, *p);
    } else {
        /* Text too long - clip and add "..." */
        int max_chars = max_width / f_width;
        if (max_chars > 3) {
            char clipped[256];
            strncpy(clipped, text, max_chars - 3);
            clipped[max_chars - 3] = '\0';
            strcat(clipped, "...");
            
            glRasterPos2f(x, y);
            for (const char* p = clipped; *p; p++)
                glutBitmapCharacter(font, *p);
        }
    }
}

/* Draw scroll thumb on right edge (Linux terminal behavior)
 * thumb at BOTTOM = viewing newest content (scroll_offset = 0)
 * thumb at TOP = viewing oldest content (scroll_offset = max_scroll)
 * y is the BOTTOM of the scroll track (OpenGL coords, y increases upward)
 * Stores track position for mouse click detection
 */
void draw_scroll_thumb(float x, float y, float h, int scroll_offset, int max_scroll, int visible_lines) {
    /* Store track position for mouse interaction */
    scroll_track_y = (int)y;
    scroll_track_h = (int)h;
    
    if (max_scroll <= 0) return;  /* No scroll needed */
    
    /* Thumb height proportional to visible/total (min 10%) */
    int total_lines = max_scroll + visible_lines;
    float thumb_h_ratio = (float)visible_lines / (float)total_lines;
    if (thumb_h_ratio < 0.1f) thumb_h_ratio = 0.1f;
    
    int thumb_h = (int)(h * thumb_h_ratio);
    if (thumb_h < 10) thumb_h = 10;
    
    /* Thumb position: 0.0 = bottom (newest), 1.0 = top (oldest) */
    float thumb_pos = (max_scroll > 0) ? ((float)scroll_offset / (float)max_scroll) : 0.0f;
    
    /* Y position: y is BOTTOM, so thumb at bottom when thumb_pos = 0 */
    int track_h = (int)h - thumb_h;
    int thumb_y = (int)(y + track_h * thumb_pos);  /* y + 0 = bottom, y + track_h = top */
    
    /* Draw thumb track (background) */
    glColor3f(0.3f, 0.3f, 0.3f);  /* Dark grey track */
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + 8, y);
    glVertex2f(x + 8, y + h);
    glVertex2f(x, y + h);
    glEnd();
    
    /* Draw thumb */
    if (scroll_offset == 0) {
        glColor3f(0.5f, 0.5f, 0.5f);  /* Grey - at bottom */
    } else {
        glColor3f(0.7f, 0.7f, 0.7f);  /* Lighter grey - scrolled up */
    }
    glBegin(GL_QUADS);
    glVertex2f(x + 1, thumb_y);
    glVertex2f(x + 7, thumb_y);
    glVertex2f(x + 7, thumb_y + thumb_h);
    glVertex2f(x + 1, thumb_y + thumb_h);
    glEnd();
}

void draw_cube(float size) {
    float s = size / 2.0f;
    glBegin(GL_QUADS);
    /* Front */
    glColor3f(1, 0, 0); glVertex3f(-s, -s,  s); glVertex3f( s, -s,  s); glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    /* Back */
    glColor3f(0, 1, 0); glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s); glVertex3f( s, -s, -s);
    /* Top */
    glColor3f(0, 0, 1); glVertex3f(-s,  s, -s); glVertex3f(-s,  s,  s); glVertex3f( s,  s,  s); glVertex3f( s,  s, -s);
    /* Bottom */
    glColor3f(1, 1, 0); glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s); glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    /* Right */
    glColor3f(1, 0, 1); glVertex3f( s, -s, -s); glVertex3f( s,  s, -s); glVertex3f( s,  s,  s); glVertex3f( s, -s,  s);
    /* Left */
    glColor3f(0, 1, 1); glVertex3f(-s, -s, -s); glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    glEnd();
}

void draw_tile(float x, float y, float z, float size, float r, float g, float b) {
    float s = size / 2.0f;
    float h = size * (1.0f + z * 0.5f); /* Z-level extrusion */
    
    glBegin(GL_QUADS);
    /* Top - height depends on Z */
    glColor3f(r, g, b); 
    glVertex3f(-s,  h, -s); glVertex3f(-s,  h,  s); glVertex3f( s,  h,  s); glVertex3f( s,  h, -s);
    
    /* Sides - slightly darker for depth */
    glColor3f(r*0.8f, g*0.8f, b*0.8f);
    glVertex3f(-s, 0,  s); glVertex3f( s, 0,  s); glVertex3f( s,  h,  s); glVertex3f(-s,  h,  s);
    glVertex3f(-s, 0, -s); glVertex3f(-s,  h, -s); glVertex3f( s,  h, -s); glVertex3f( s, 0, -s);
    glVertex3f( s, 0, -s); glVertex3f( s,  h, -s); glVertex3f( s,  h,  s); glVertex3f( s, 0,  s);
    glVertex3f(-s, 0, -s); glVertex3f(-s, 0,  s); glVertex3f(-s,  h,  s); glVertex3f(-s,  h, -s);
    glEnd();
}

/* 
 * PIECE ARTIFACT: 8x8x8 Voxel Bitmasks 
 * Each array is [Z-level][Y-row] with 8 bits per row (X)
 */
unsigned char ARTIFACT_XEL[8][8] = {
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}, // Bottom frame
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}  // Top frame
};

unsigned char ARTIFACT_LEGEND[8][8] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};

void draw_artifact_piece(float x, float y, float z, float size, unsigned char mask[8][8], float r, float g, float b, int wireframe, float face_colors[6][3]) {
    float v_size = size / 8.0f;
    float offset = -size / 2.0f + v_size / 2.0f;
    float s = v_size / 2.0f;

    for (int iz = 0; iz < 8; iz++) {
        for (int iy = 0; iy < 8; iy++) {
            unsigned char row = mask[iz][iy];
            for (int ix = 0; ix < 8; ix++) {
                if (row & (1 << (7 - ix))) {
                    float vx = x + offset + (ix * v_size);
                    float vy = y + (iz * v_size);
                    float vz = z + offset + (iy * v_size);
                    
                    glPushMatrix();
                    glTranslatef(vx, vy, vz);
                    if (wireframe) {
                        glColor3f(r, g, b);
                        glutWireCube(v_size);
                    } else if (face_colors) {
                        glBegin(GL_QUADS);
                        /* Top (Y+) */ glColor3fv(face_colors[0]); glVertex3f(-s, s,-s); glVertex3f(-s, s, s); glVertex3f( s, s, s); glVertex3f( s, s,-s);
                        /* Bottom (Y-) */ glColor3fv(face_colors[1]); glVertex3f(-s,-s,-s); glVertex3f( s,-s,-s); glVertex3f( s,-s, s); glVertex3f(-s,-s, s);
                        /* Front (Z-) */ glColor3fv(face_colors[2]); glVertex3f(-s,-s,-s); glVertex3f(-s, s,-s); glVertex3f( s, s,-s); glVertex3f( s,-s,-s);
                        /* Back (Z+) */ glColor3fv(face_colors[3]); glVertex3f(-s,-s, s); glVertex3f( s,-s, s); glVertex3f( s, s, s); glVertex3f(-s, s, s);
                        /* Right (X+) */ glColor3fv(face_colors[4]); glVertex3f( s,-s,-s); glVertex3f( s, s,-s); glVertex3f( s, s, s); glVertex3f( s,-s, s);
                        /* Left (X-) */ glColor3fv(face_colors[5]); glVertex3f(-s,-s,-s); glVertex3f(-s,-s, s); glVertex3f(-s, s, s); glVertex3f(-s, s,-s);
                        glEnd();
                    } else {
                        draw_tile(0, 0, 0, v_size, r, g, b);
                    }
                    glPopMatrix();
                }
            }
        }
    }
}

void restore_window(DesktopWindow* win) {
    if (win->is_maximized || win->is_half_screen) {
        win->x = win->prev_x;
        win->y = win->prev_y;
        win->w = win->prev_w;
        win->h = win->prev_h;
        win->is_maximized = 0;
        win->is_half_screen = 0;
    }
}

void maximize_window(DesktopWindow* win) {
    if (win->is_maximized) {
        restore_window(win);
    } else {
        win->prev_x = win->x;
        win->prev_y = win->y;
        win->prev_w = win->w;
        win->prev_h = win->h;
        
        win->x = 0;
        win->y = 0;
        win->w = window_width;
        win->h = window_height - get_toolbar_height();
        win->is_maximized = 1;
        win->is_half_screen = 0;
    }
}

void half_screen_window(DesktopWindow* win) {
    if (win->is_half_screen == 1) { /* Toggle from Left to Right */
        win->x = window_width / 2;
        win->y = 0;
        win->w = window_width / 2;
        win->h = window_height - get_toolbar_height();
        win->is_half_screen = 2;
    } else if (win->is_half_screen == 2) { /* Toggle from Right to Restore */
        restore_window(win);
    } else { /* Toggle to Left */
        win->prev_x = win->x;
        win->prev_y = win->y;
        win->prev_w = win->w;
        win->prev_h = win->h;
        
        win->x = 0;
        win->y = 0;
        win->w = window_width / 2;
        win->h = window_height - get_toolbar_height();
        win->is_half_screen = 1;
        win->is_maximized = 0;
    }
}

/* Hierarchical Clipping Stack */
typedef struct {
    int x, y, w, h;
} ScissorRect;

ScissorRect scissor_stack[16];
int scissor_top = -1;

void push_scissor(int x, int y, int w, int h) {
    if (scissor_top >= 15) return;
    
    ScissorRect current = {x, y, w, h};
    if (scissor_top >= 0) {
        /* Intersect with parent scissor */
        ScissorRect parent = scissor_stack[scissor_top];
        int nx1 = (current.x > parent.x) ? current.x : parent.x;
        int ny1 = (current.y > parent.y) ? current.y : parent.y;
        int nx2 = (current.x + current.w < parent.x + parent.w) ? current.x + current.w : parent.x + parent.w;
        int ny2 = (current.y + current.h < parent.y + parent.h) ? current.y + current.h : parent.y + parent.h;
        
        current.x = nx1; current.y = ny1;
        current.w = (nx2 > nx1) ? (nx2 - nx1) : 0;
        current.h = (ny2 > ny1) ? (ny2 - ny1) : 0;
    }
    
    scissor_stack[++scissor_top] = current;
    glScissor(current.x, current.y, current.w, current.h);
}

void pop_scissor() {
    if (scissor_top > 0) {
        scissor_top--;
        ScissorRect prev = scissor_stack[scissor_top];
        glScissor(prev.x, prev.y, prev.w, prev.h);
    } else if (scissor_top == 0) {
        scissor_top--;
        glDisable(GL_SCISSOR_TEST);
    }
}

void draw_gltpm_node(DesktopWindow* win, int node_index, int abs_x, int abs_y) {
    GLTPMScene *scene = &win->gltpm_scene;
    
    if (node_index != -1) {
        GLTPMNode *node = &scene->nodes[node_index];
        glPushMatrix();
        /* Translate to TOP-LEFT of node in parent space */
        glTranslatef(node->x, -node->y, 0);
        
        /* 1. Draw Container Background (Draw DOWN from top-left) */
        if (strcmp(node->tag, "img") == 0) {
            if (node->has_texture) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, node->texture_id);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(1, 1, 1, 1);
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(0, 0);
                glTexCoord2f(1, 0); glVertex2f(node->w, 0);
                glTexCoord2f(1, 1); glVertex2f(node->w, -node->h);
                glTexCoord2f(0, 1); glVertex2f(0, -node->h);
                glEnd();
                glDisable(GL_BLEND);
                glDisable(GL_TEXTURE_2D);
            } else {
                /* Missing Image Placeholder (Magenta) */
                draw_rect(0, -node->h, node->w, node->h, 1.0f, 0.0f, 1.0f);
            }
        } else {
            draw_rect(0, -node->h, node->w, node->h, node->color[0], node->color[1], node->color[2]);
        }
        
        /* 2. Apply Clipping (Absolute Window Coordinates for glScissor) */
        int node_abs_x = abs_x + (int)node->x;
        int node_abs_y = abs_y - (int)node->y - (int)node->h;
        push_scissor(node_abs_x, node_abs_y, (int)node->w, (int)node->h);

        /* Update absolute coordinates for children */
        abs_x = node_abs_x;
        abs_y = abs_y - (int)node->y;
    }

    /* 3. Render Children */
    for (int i = 0; i < scene->text_count; i++) {
        if (scene->texts[i].parent == node_index) {
            GLTPMText *txt = &scene->texts[i];
            glColor3f(0.85f, 0.85f, 0.9f);
            glRasterPos2f(txt->x >= 0 ? txt->x : 10, txt->y >= 0 ? -txt->y - 20 : -25);
            for (char *p = txt->label; *p; p++) glutBitmapCharacter(get_font_small(), *p);
        }
    }

    for (int i = 0; i < scene->button_count; i++) {
        if (scene->buttons[i].parent == node_index) {
            GLTPMButton *btn = &scene->buttons[i];
            int btn_h = (btn->h > 0) ? (int)btn->h : get_button_height();
            int btn_w = (btn->w > 0) ? (int)btn->w : (win->w - 24);
            int btn_x = (btn->x >= 0) ? (int)btn->x : 12;
            int btn_y = (btn->y >= 0) ? -(int)btn->y - btn_h : -110 - (i * (get_button_height() + 4));

            const char *indicator = (i == win->selected_index) ? (win->is_map_control ? "^" : ">") : " ";
            char btn_label[256];
            snprintf(btn_label, sizeof(btn_label), "[%s] %d. [%s]", indicator, i + 1, btn->label);

            float r = (i == win->selected_index) ? 0.72f : 0.22f;
            float g = (i == win->selected_index) ? 0.58f : 0.24f;
            float b = (i == win->selected_index) ? 0.18f : 0.36f;
            
            draw_rect(btn_x, btn_y, btn_w, btn_h, r, g, b);
            glColor3f(1, 1, 1);
            glRasterPos2f(btn_x + 6, btn_y + (btn_h / 2) - 4);
            for (char *p = btn_label; *p; p++) glutBitmapCharacter(get_font_small(), *p);
        }
    }

    /* 4. Recurse into Child Nodes */
    for (int i = 0; i < scene->node_count; i++) {
        if (scene->nodes[i].parent == node_index) {
            draw_gltpm_node(win, i, abs_x, abs_y);
        }
    }

    if (node_index != -1) {
        pop_scissor();
        glPopMatrix();
    }
}

void draw_window(DesktopWindow* win) {
    if (win->minimized) return;

    int x = win->x;
    int y = window_height - win->y - win->h; /* OpenGL y (bottom-up) */
    int w = win->w;
    int h = win->h;

    /* Window shadow */
    draw_rect(x + 3, y - 3, w, h, 0.0f, 0.0f, 0.0f);

    /* Window background */
    if (win->id == active_window_id) {
        draw_rect(x, y, w, h, 0.15f, 0.15f, 0.35f);
    } else {
        draw_rect(x, y, w, h, 0.1f, 0.1f, 0.25f);
    }

    /* Titlebar */
    if (win->id == active_window_id) {
        draw_rect(x, y + h - 25, w, 25, 0.0f, 0.4f, 0.8f);
    } else {
        draw_rect(x, y + h - 25, w, 25, 0.3f, 0.3f, 0.5f);
    }

    /* Title text (clipped to titlebar width) */
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text_clipped(x + 10, y + h - 18, w - 100, win->title);

    /* Buttons: Close(X), Maximize([]), Half(H), Minimize(-) */
    /* Close */
    draw_rect(x + w - 22, y + h - 22, 18, 18, 0.8f, 0.2f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 17, y + h - 18);
    glutBitmapCharacter(get_font_regular(), 'X');

    /* Maximize (Full Screen) */
    draw_rect(x + w - 44, y + h - 22, 18, 18, 0.2f, 0.6f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 42, y + h - 18);
    /* Draw '[]' symbol */
    glutBitmapCharacter(get_font_regular(), '[');
    glutBitmapCharacter(get_font_regular(), ']');

    /* Half Screen */
    draw_rect(x + w - 66, y + h - 22, 18, 18, 0.6f, 0.6f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 61, y + h - 18);
    glutBitmapCharacter(get_font_regular(), 'H');

    /* Minimize */
    draw_rect(x + w - 88, y + h - 22, 18, 18, 0.7f, 0.5f, 0.2f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x + w - 83, y + h - 18);
    glutBitmapCharacter(get_font_regular(), '-');

    /* Resize handle (bottom-right) - Linux style */
    draw_rect(x + w - 15, y, 15, 15, 0.6f, 0.6f, 0.8f);  /* Lighter blue - more visible */
    /* Draw diagonal lines on resize handle */
    glColor3f(0.8f, 0.8f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(x + w - 13, y + 3); glVertex2f(x + w - 3, y + 3);
    glVertex2f(x + w - 13, y + 3); glVertex2f(x + w - 13, y + 13);
    glVertex2f(x + w - 10, y + 6); glVertex2f(x + w - 3, y + 6);
    glVertex2f(x + w - 10, y + 6); glVertex2f(x + w - 10, y + 13);
    glEnd();
    
    /* Window content area */
    if (strcmp(win->type, "terminal") == 0) {
        /* innerHTML Pattern: Read frame from file and display */
        int content_top = y + h - 30;  /* Below titlebar */
        int content_bottom = y + 10;   /* Above window floor */
        int line_height = get_line_height();
        int x_start = x + 10;
        
        /* Calculate scroll state for frame history */
        int history_area_h = content_top - content_bottom;
        int visible_lines = history_area_h / line_height;
        win->max_scroll = (win->history_count > visible_lines) ?
                          (win->history_count - visible_lines) : 0;
        
        /* Clamp scroll offset */
        if (win->scroll_offset > win->max_scroll) win->scroll_offset = win->max_scroll;
        if (win->scroll_offset < 0) win->scroll_offset = 0;
        
        /* innerHTML: Read frame from history file - show MOST RECENT frame */
        FILE *frame_file = fopen(gl_os_frame_history, "r");
        if (frame_file) {
            /* Find the LAST frame in the file (most recent) */
            char line[MAX_LINE];
            char last_frame[MAX_LINE * 20];  /* Buffer for last frame */
            int in_last_frame = 0;
            int last_frame_lines = 0;
            
            last_frame[0] = '\0';
            
            while (fgets(line, sizeof(line), frame_file)) {
                if (strstr(line, "--- FRAME UPDATE at")) {
                    /* Start of new frame - reset buffer */
                    in_last_frame = 1;
                    last_frame_lines = 0;
                    last_frame[0] = '\0';
                }
                else if (in_last_frame && last_frame_lines < 20) {
                    /* Add line to last frame buffer */
                    strcat(last_frame, line);
                    last_frame_lines++;
                }
            }
            fclose(frame_file);
            
            /* Now display the last frame */
            if (last_frame[0] != '\0') {
                printf("[FRAME DEBUG] Displaying last frame (%d lines)\n", last_frame_lines);
                
                /* Parse and display each line */
                char *frame_line = strtok(last_frame, "\n");
                int display_line = 0;
                void* f_reg = get_font_regular();
                while (frame_line && display_line < visible_lines) {
                    int frame_y = content_top - (display_line * line_height);
                    if (frame_y > content_bottom) {
                        glColor3f(0.8f, 0.8f, 0.8f);
                        glRasterPos2f(x_start, frame_y);
                        for (char* p = frame_line; *p; p++)
                            glutBitmapCharacter(f_reg, *p);
                        display_line++;
                    }
                    frame_line = strtok(NULL, "\n");
                }
                printf("[FRAME DEBUG] Displayed %d lines\n", display_line);
            } else {
                printf("[FRAME DEBUG] No frame found in history!\n");
            }
        } else {
            printf("[FRAME DEBUG] Could not open frame history file!\n");
        }

        /* Direct code: Draw text selection highlight */
        if (win->is_selecting || win->selection_active) {
            printf("[DEBUG] Drawing selection highlight for terminal\n");
            
            /* Convert GLUT coordinates (top-down) to OpenGL (bottom-up) */
            int gl_y1 = window_height - win->select_start_y;
            int gl_y2 = window_height - win->select_end_y;
            int sel_x1 = win->select_start_x;
            int sel_x2 = win->select_end_x;
            int sel_y1 = (gl_y1 < gl_y2) ? gl_y1 : gl_y2;
            int sel_y2 = (gl_y1 > gl_y2) ? gl_y1 : gl_y2;
            
            /* Normalize X */
            if (sel_x1 > sel_x2) { int t = sel_x1; sel_x1 = sel_x2; sel_x2 = t; }
            
            printf("[DEBUG] Selection rect: (%d,%d) to (%d,%d)\n", sel_x1, sel_y1, sel_x2, sel_y2);
            
            /* Draw semi-transparent highlight */
            glColor4f(0.5f, 0.5f, 1.0f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(sel_x1, sel_y1);
            glVertex2f(sel_x2, sel_y1);
            glVertex2f(sel_x2, sel_y2);
            glVertex2f(sel_x1, sel_y2);
            glEnd();
            
            /* Extract text from frame based on selection */
            /* TODO: Map pixel coordinates to character positions in frame */
            /* For now, just copy a placeholder */
            if (win->selection_active && strlen(win->selected_text) == 0) {
                strncpy(win->selected_text, "[Selected text extraction - TODO]", sizeof(win->selected_text) - 1);
            }
        }

        /* Direct code: Draw scroll thumb on right edge */
        draw_scroll_thumb(x + w - 12, content_bottom, content_top - content_bottom,
                         win->scroll_offset, win->max_scroll, visible_lines);
    } else if (strcmp(win->type, "cube") == 0) {
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0, 0, -5);
        glRotatef(global_rotation, 1, 1, 0);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);
        draw_cube(2.0f);
        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);
    } else if (strcmp(win->type, "gltpm_app") == 0) {
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w / (double)(h - 25), 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        /* Dynamic Camera Logic */
        float target_x = win->cam_pos[0], target_y = win->cam_pos[1], target_z = win->cam_pos[2];
        if (win->camera_mode == 1 || win->camera_mode == 3) {
            target_x = win->xelector_pos[0] * 1.2f;
            target_y = win->xelector_pos[2] * 0.5f;
            target_z = win->xelector_pos[1] * 1.2f;
        }

        if (win->camera_mode == 1) {
            glRotatef(win->cam_rot[0], 1, 0, 0);
            glRotatef(win->cam_rot[1], 0, 1, 0);
            glTranslatef(-target_x, -(target_y + 1.6f), -target_z + 0.3f);
        } else if (win->camera_mode == 3) {
            glTranslatef(0, -2.5f, -8.0f);
            glRotatef(25.0f, 1, 0, 0);
            glRotatef(win->cam_rot[1], 0, 1, 0);
            glTranslatef(-target_x, -target_y, -target_z);
        } else {
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(win->cam_rot[0] + 55.0f, 1, 0, 0);
            glTranslatef(-target_x, -target_y, -target_z);
            glRotatef(win->cam_rot[1] - 45.0f, 0, 1, 0);
        }

        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);

        /* Render 3D World (Root only) */
        for (int i = 0; i < win->gltpm_scene.tile_count; i++) {
            GLTPMTile *tile = &win->gltpm_scene.tiles[i];
            if (tile->parent != -1) continue;
            glPushMatrix();
            glTranslatef(tile->x * 1.2f, 0.0f, tile->y * 1.2f);
            draw_tile(0, 0, tile->z + (tile->extrude - 1.0f), 1.0f,
                      tile->color[0], tile->color[1], tile->color[2]);
            glPopMatrix();
        }

        for (int i = 0; i < win->gltpm_scene.sprite_count; i++) {
            GLTPMSprite *sprite = &win->gltpm_scene.sprites[i];
            if (sprite->parent != -1) continue;
            if (win->camera_mode == 1 && strcmp(sprite->sprite_id, win->gltpm_scene.active_target_id) == 0) continue;
            draw_artifact_piece(sprite->x * 1.2f, 1.0f + sprite->z, sprite->y * 1.2f, 0.8f, sprite->artifact_mask,
                                sprite->color[0], sprite->color[1], sprite->color[2], 0,
                                sprite->has_face_colors ? sprite->face_colors : NULL);
        }

        /* Draw Xelector (Conditional) */
        if (win->gltpm_scene.ui_mode == 0) {
            float sel_gl_x = win->xelector_pos[0] * 1.2f;
            float sel_gl_z = win->xelector_pos[1] * 1.2f;
            float sel_gl_y = 1.1f + (win->xelector_pos[2] * 0.5f);
            struct timeval tv; gettimeofday(&tv, NULL);
            if (win->camera_mode != 1 && (tv.tv_usec / 200000) % 2 == 0) {
                draw_artifact_piece(sel_gl_x, sel_gl_y, sel_gl_z, 1.2f, ARTIFACT_XEL, 1.0f, 1.0f, 0.0f, 1, NULL);
                float legend_faces[6][3] = {{0,1,0},{0.4,0.2,0},{1,1,1},{0,0,0},{1,0,0},{0,0,1}};
                draw_artifact_piece(sel_gl_x, sel_gl_y + 0.2f, sel_gl_z, 0.4f, ARTIFACT_LEGEND, 1, 1, 1, 0, legend_faces);
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);

        /* 2D UI Overlay - Initiates Tree Traversal */
        glPushMatrix();
        glTranslatef(x, y + h - 25, 0);
        
        scissor_top = -1;
        push_scissor(x, y, w, h - 25);
        
        draw_gltpm_node(win, -1, x, y + h - 25);
        
        pop_scissor();
        glPopMatrix();

        /* Global HUD (Conditional) */
        if (win->gltpm_scene.ui_mode == 0) {
            glColor3f(1, 1, 1);
            glRasterPos2f(x + 15, y + h - 42);
            for (char *p = win->gltpm_scene.title; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
            
            char key_feedback[64];
            snprintf(key_feedback, sizeof(key_feedback), "[KEY]: %s", win->gltpm_scene.last_key);
            glColor3f(1.0f, 1.0f, 0.0f);
            glRasterPos2f(x + w - 120, y + h - 42);
            for (char *p = key_feedback; *p; p++) glutBitmapCharacter(get_font_small(), *p);
        }

        /* Nav Command Prompt */
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "Nav > %s_", win->nav_buffer);
        glColor3f(0.8f, 1.0f, 0.8f);
        glRasterPos2f(x + 15, y + 15);
        for (char *p = prompt; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
    } else if (strcmp(win->type, "project_mirror") == 0) {
        /* Project-Specific 3D Rendering + Menu Overlay */
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        
        /* Dynamic Camera Logic (WASD + Modes) */
        float target_x = win->cam_pos[0];
        float target_y = win->cam_pos[1];
        float target_z = win->cam_pos[2];

        if (win->camera_mode == 1 || win->camera_mode == 3) {
            /* Follow Xelector automatically in 1st/3rd person modes */
            target_x = win->xelector_pos[0] * 1.2f;
            target_y = win->xelector_pos[2] * 0.5f;
            target_z = win->xelector_pos[1] * 1.2f;
        }

        if (strcmp(win->project_id, "aow-2d") == 0) {
            /* Force Top-Down for aow-2d */
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(-90.0f, 1, 0, 0);
        } else if (win->camera_mode == 1) { /* 1st Person - Close/Low */
            glTranslatef(0, -0.5f, -1.0f);
        } else if (win->camera_mode == 3) { /* 3rd Person - Behind/High */
            glTranslatef(0, -3.0f, -10.0f);
            glRotatef(20, 1, 0, 0);
        } else { /* Free/Isometric */
            glTranslatef(0, -2.5f, -15.0f);
            glRotatef(win->cam_rot[0], 1, 0, 0); /* Pitch */
        }
        
        /* Apply Targeted Position & User Rotation */
        glTranslatef(-target_x, -target_y, -target_z);
        glRotatef(win->cam_rot[1], 0, 1, 0); /* Yaw (Q/E) */
        
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);
        
        /* Render 10x10 Mock Grid with Z-Extrusion (K4) */
        for (int row = -5; row <= 5; row++) {
            for (int col = -5; col <= 5; col++) {
                float dist = (float)(abs(row) + abs(col));
                float z_level = (float)((5 - (int)dist) % 4); /* Mock wave pattern */
                if (z_level < 0) z_level = 0;
                
                glPushMatrix();
                glTranslatef(col * 1.2f, 0, row * 1.2f);
                draw_tile(0, 0, z_level, 1.0f, 0.2f, 0.5f, 0.8f); /* Blueish tiles */
                glPopMatrix();
            }
        }

        /* Render Mock Entities (Fuzzball, Zombie, Tree) */
        /* Fuzzball (Player) */
        glPushMatrix();
        glTranslatef(0 * 1.2f, 1.0f, 0 * 1.2f);
        draw_tile(0, 0, 0, 0.8f, 1.0f, 1.0f, 1.0f); /* White cube */
        glPopMatrix();

        /* Zombie */
        glPushMatrix();
        glTranslatef(2 * 1.2f, 1.0f, -3 * 1.2f);
        draw_tile(0, 0, 0, 0.8f, 1.0f, 0.2f, 0.2f); /* Red cube */
        glPopMatrix();

        /* Tree */
        glPushMatrix();
        glTranslatef(-4 * 1.2f, 1.0f, 2 * 1.2f);
        draw_tile(0, 0, 1.0f, 0.8f, 0.2f, 0.8f, 0.2f); /* Green cube */
        glPopMatrix();

        /* Draw Xelector (Grid Cursor) */
        glPushMatrix();
        /* Map Grid Y to OpenGL Depth (Z) and Grid Z to OpenGL Elevation (Y) */
        float sel_gl_x = win->xelector_pos[0] * 1.2f;
        float sel_gl_y = win->xelector_pos[2] * 0.5f; /* Elevation */
        float sel_gl_z = win->xelector_pos[1] * 1.2f; /* Depth */
        glTranslatef(sel_gl_x, sel_gl_y, sel_gl_z);
        
        /* Add Blinking Logic (KISS) */
        if ((time(NULL) % 2) == 0) { /* Toggle visibility every second */
            draw_xelector(1.2f);
        }
        glPopMatrix();
        
        /* Draw Direction Cube in Corner */
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(2.5f, 1.8f, -6.0f); /* Corner of view */
        glRotatef(win->cam_rot[0], 1, 0, 0);
        glRotatef(win->cam_rot[1], 0, 1, 0);
        draw_direction_cube(0.5f);
        glPopMatrix();
        
        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);

        /* Overlay Index-Based Menu (K3/Navigation Parity) */
        int content_top = y + h - 45;
        int x_start = x + 15;
        
        /* Status Bar (Map Control Hint) */
        if (win->is_map_control) {
            draw_rect(x, y + h - 45, w, 20, 0.8f, 0.1f, 0.1f);
            glColor3f(1, 1, 1);
            glRasterPos2f(x_start, y + h - 40);
            const char* hint = "[ MAP CONTROL ACTIVE ] - ESC TO EXIT";
            for (const char* p = hint; *p; p++) glutBitmapCharacter(get_font_small(), *p);
            content_top -= 25;
        }

        glColor3f(1, 1, 1);
        glRasterPos2f(x_start, content_top);
        const char* header = "=== PROJECT MENU ===";
        for (const char* p = header; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
        
        /* Draw Menu Options */
        for (int i = 0; i < win->option_count; i++) {
            int line_y = content_top - 25 - (i * get_line_height());
            if (line_y < y + 10) break;

            char line[128];
            if (i == win->selected_index) {
                glColor3f(1, 1, 0); /* Yellow for selection */
                glRasterPos2f(x_start, line_y);
                const char* indicator = "[>] ";
                for (const char* p = indicator; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
                
                snprintf(line, sizeof(line), "%d. [%s]", i + 1, win->menu_options[i]);
                for (const char* p = line; *p; p++) glutBitmapCharacter(get_font_small(), *p);
            } else {
                glColor3f(0.7f, 0.7f, 0.7f);
                glRasterPos2f(x_start, line_y);
                const char* indicator = "[ ] ";
                for (const char* p = indicator; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
                
                snprintf(line, sizeof(line), "%d. [%s]", i + 1, win->menu_options[i]);
                for (const char* p = line; *p; p++) glutBitmapCharacter(get_font_small(), *p);
            }
        }
    } else if (strcmp(win->type, "emoji_studio") == 0) {
        load_emoji_studio_voxels(win);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0, (double)w/(double)(h-25), 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0, 0, -5.0f);
        glRotatef(win->cam_rot[0], 1, 0, 0);
        glRotatef(win->cam_rot[1], 0, 1, 0);
        
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h - 25);
        
        if (win->emoji_voxels && win->emoji_res > 0) {
            float total_size = 3.0f;
            float v_size = total_size / win->emoji_res;
            float offset = -total_size / 2.0f;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (int ey = 0; ey < win->emoji_res; ey++) {
                for (int ex = 0; ex < win->emoji_res; ex++) {
                    RGBA_Pixel p = win->emoji_voxels[ey * win->emoji_res + ex];
                    if (p.a > 50) {
                        glPushMatrix();
                        glTranslatef(offset + ex * v_size + v_size/2, 0, offset + ey * v_size + v_size/2);
                        glScalef(1, (float)win->emoji_res, 1);
                        draw_tile(0, 0, 0, v_size, (float)p.r/255.0f, (float)p.g/255.0f, (float)p.b/255.0f);
                        glPopMatrix();
                    }
                }
            }
        }
        
        glDisable(GL_SCISSOR_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);

        /* 2D Overlay - Emoji Picker Grid */
        int grid_x_start = x + 15;
        int grid_y_top = y + h - 50;
        int grid_cols = 6;
        int cell_size = 32;
        int spacing = 5;

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, emoji_atlas_tex);
        glColor3f(1, 1, 1);
        
        /* Load condensed list from gui_state.txt (parsed by Host) */
        /* Note: For now we just draw the first 30 emojis for the grid */
        for (int i = 0; i < 30; i++) {
            int row = i / grid_cols;
            int col = i % grid_cols;
            float ex = grid_x_start + col * (cell_size + spacing);
            float ey = grid_y_top - (row + 1) * (cell_size + spacing);
            
            if (ey < y + 150) break; // Don't overlap with menu
            
            float u_min = (float)i * 64.0f / (float)emoji_atlas_w;
            float u_max = (float)(i + 1) * 64.0f / (float)emoji_atlas_w;
            
            glBegin(GL_QUADS);
            glTexCoord2f(u_min, 1); glVertex2f(ex, ey);
            glTexCoord2f(u_max, 1); glVertex2f(ex + cell_size, ey);
            glTexCoord2f(u_max, 0); glVertex2f(ex + cell_size, ey + cell_size);
            glTexCoord2f(u_min, 0); glVertex2f(ex, ey + cell_size);
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);

        /* Overlay Menu (Bottom Section) */
        int content_top = y + 140;
        int x_start = x + 15;
        glColor3f(1, 1, 1);
        glRasterPos2f(x_start, content_top);
        const char* header = "=== EMOJI STUDIO MENU ===";
        for (const char* p = header; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
        for (int i = 0; i < win->option_count; i++) {
            int line_y = content_top - 25 - (i * get_line_height());
            if (line_y < y + 10) break;
            if (i == win->selected_index) glColor3f(1, 1, 0); else glColor3f(0.7f, 0.7f, 0.7f);
            glRasterPos2f(x_start, line_y);
            const char* ind = (i == win->selected_index) ? "[>] " : "[ ] ";
            for (const char* p = ind; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
            char line[128]; snprintf(line, sizeof(line), "%d. %s", i + 1, win->menu_options[i]);
            for (const char* p = line; *p; p++) glutBitmapCharacter(get_font_small(), *p);
        }
    }
}

unsigned int gltpm_load_texture(const char* path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) return 0;

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

int get_toolbar_height() {
    if (g_font_scale_factor < 1.4f) return 30;
    if (g_font_scale_factor < 1.7f) return 40;
    return 50;
}

void draw_toolbar(void) {
    int t_height = get_toolbar_height();
    draw_rect(0, 0, window_width, t_height, 0.1f, 0.1f, 0.2f);
    glColor3f(0.3f, 0.3f, 0.5f);
    glBegin(GL_LINES);
    glVertex2f(0, t_height);
    glVertex2f(window_width, t_height);
    glEnd();
    
    char info[128];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", timeinfo);
    snprintf(info, sizeof(info), "User: guest | Time: %s", timestr);
    
    void* f_reg = get_font_regular();
    void* f_small = get_font_small();

    glColor3f(0.8f, 0.8f, 1.0f);
    glRasterPos2f(10, t_height / 3);
    for (char* p = info; *p; p++) {
        glutBitmapCharacter(f_reg, *p);
    }
    
    int x_off = 350;
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active) continue;
        if (windows[i].minimized) {
            draw_rect(x_off, 4, 120, t_height - 8, 0.2f, 0.3f, 0.5f);
        } else if (windows[i].id == active_window_id) {
            draw_rect(x_off, 4, 120, t_height - 8, 0.3f, 0.4f, 0.7f);
        } else {
            draw_rect(x_off, 4, 120, t_height - 8, 0.15f, 0.2f, 0.35f);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x_off + 5, t_height / 3);
        char short_title[15];
        strncpy(short_title, windows[i].title, 12);
        short_title[12] = '\0';
        for (char* p = short_title; *p; p++) {
            glutBitmapCharacter(f_small, *p);
        }
        x_off += 130;
    }

    /* Main Window Controls (Far Right) */
    int main_btns_x = window_width - 130;
    /* Exit Button */
    draw_rect(main_btns_x + 90, 4, 30, t_height - 8, 0.8f, 0.2f, 0.2f);
    glColor3f(1,1,1); glRasterPos2f(main_btns_x + 98, t_height / 3);
    glutBitmapCharacter(f_reg, 'X');

    /* Fullscreen Toggle */
    draw_rect(main_btns_x + 50, 4, 30, t_height - 8, 0.2f, 0.6f, 0.2f);
    glColor3f(1,1,1); glRasterPos2f(main_btns_x + 58, t_height / 3);
    glutBitmapCharacter(f_reg, 'F');

    /* Minimize Button */
    draw_rect(main_btns_x + 10, 4, 30, t_height - 8, 0.6f, 0.6f, 0.2f);
    glColor3f(1,1,1); glRasterPos2f(main_btns_x + 20, t_height / 3);
    glutBitmapCharacter(f_reg, '-');
}

void draw_context_menu(void) {
    if (!context_menu_visible) return;
    int x = context_menu_x;
    int y = window_height - context_menu_y - context_menu_height;
    int w = context_menu_width;
    int h = context_menu_height;
    draw_rect(x, y, w, h, 0.2f, 0.2f, 0.3f);
    glColor3f(0.5f, 0.5f, 0.7f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    
    /* Different menu items based on context */
    char* items[4];
    int item_count = 0;
    
    if (context_menu_type == 1) {
        /* Terminal context menu - Copy/Paste */
        items[0] = "[1] Copy";
        items[1] = "[2] Paste";
        items[2] = "[3] Clear";
        items[3] = "[ESC] Cancel";
        item_count = 4;
    } else {
        /* Desktop context menu */
        items[0] = "[1] New Terminal";
        items[1] = "[2] 3D Cube App";
        items[2] = "[3] New File";
        items[3] = "[ESC] Cancel";
        item_count = 4;
    }
    
    for (int i = 0; i < item_count; i++) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(x + 10, y + h - 25 - (i * (get_line_height() + 2)));
        for (char* p = items[i]; *p; p++) glutBitmapCharacter(get_font_regular(), *p);
    }
}

void render_scene(void) {
    glClearColor(bg_color[0], bg_color[1], bg_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    for (int i = 0; i < window_count; i++) {
        if (windows[i].active) draw_window(&windows[i]);
    }
    draw_toolbar();
    draw_context_menu();
    glutSwapBuffers();
}

void timer(int value) {
    (void)value;
    global_rotation += 2.0f;
    if (global_rotation > 360.0f) global_rotation -= 360.0f;

    cursor_blink++;
    if (cursor_blink >= 20) cursor_blink = 0;

    /* Force GLUT to poll joystick (required for continuous polling) */
    glutForceJoystickFunc();

    /* Focus and Idle-aware throttling */
    int next_timer = 30;  /* Default: 30ms (~33 FPS) */
    
    if (!is_active_layout()) {
        next_timer = 100; /* Throttled: 100ms (10 FPS) when not in focus */
    } else {
        /* Focused but potentially idle */
        time_t now = time(NULL);
        if (now - last_interaction_time > 5) {
            next_timer = 100; /* Idle backoff: 100ms after 5s of inactivity */
        }
    }

    for (int i = 0; i < window_count; i++) {
        if (!windows[i].active || strcmp(windows[i].type, "gltpm_app") != 0) continue;

        if (windows[i].managed_pid > 0) {
            int status = 0;
            pid_t result = waitpid(windows[i].managed_pid, &status, WNOHANG);
            if (result == windows[i].managed_pid) {
                windows[i].managed_pid = -1;
            }
        }

        refresh_gltpm_window(&windows[i]);
    }

    glutPostRedisplay();
    glutTimerFunc(next_timer, timer, 0);
}

void joystick(unsigned int buttonMask, int x, int y, int z) {
    (void)z;
    last_interaction_time = time(NULL);

    /* Joystick navigation for menu (joystick-first UX) */
    if (active_window_id != -1) {
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                idx = i;
                break;
            }
        }

        if (idx != -1) {
            DesktopWindow* win = &windows[idx];
            int is_navigable = (strcmp(win->type, "terminal") == 0) || 
                               (strcmp(win->type, "project_mirror") == 0 && !win->is_map_control) ||
                               (strcmp(win->type, "gltpm_app") == 0);

            if (is_navigable) {
                int selection_changed = 0;
                
                if (win->show_menu && win->option_count > 0) {
                    /* Menu navigation mode */
                    if (y < -500 && last_joy_y >= -500) {
                        move_selection(win, -1);  /* Up: up */
                        selection_changed = 1;
                    } else if (y > 500 && last_joy_y <= 500) {
                        move_selection(win, 1);   /* Down: down */
                        selection_changed = 1;
                    }

                    /* Button A: execute - edge-triggered */
                    if ((buttonMask & GLUT_JOYSTICK_BUTTON_A) &&
                        !(last_joy_buttons & GLUT_JOYSTICK_BUTTON_A)) {
                        execute_option(win, win->selected_index);
                    }
                } else if (strcmp(win->type, "terminal") == 0) {
                    /* Scroll mode (Terminal only) */
                    if (y < -500 && last_joy_y >= -500) {
                        win->scroll_offset++;
                        if (win->scroll_offset > win->max_scroll)
                            win->scroll_offset = win->max_scroll;
                        selection_changed = 1;
                    } else if (y > 500 && last_joy_y <= 500) {
                        win->scroll_offset--;
                        if (win->scroll_offset < 0)
                            win->scroll_offset = 0;
                        selection_changed = 1;
                    }
                }
                
                if (selection_changed) {
                    write_gl_os_frame();
                }
            }
        }
    }

    /* Update last state globally to ensure edge-triggering works correctly */
    last_joy_x = x;
    last_joy_y = y;
    last_joy_buttons = buttonMask;

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    (void)x; (void)y;
    last_interaction_time = time(NULL);

    /* Ctrl+C detection (ASCII 3) */
    if (key == 3) {
        printf("[GL-OS] Ctrl+C detected. Exiting...\n");
        handle_sigint(SIGINT);
        return;
    }

    /* Check focused window type */
    int active_idx = -1;
    if (active_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                active_idx = i;
                break;
            }
        }
    }

    /* ESC behavior */
    if (key == 27) {
        if (g_fullscreen_enabled) {
            g_fullscreen_enabled = 0;
            glutReshapeWindow(1024, 768);
            glutPositionWindow(100, 100);
            printf("[GL-OS] Fullscreen DISABLED via ESC key.\n");
            glutPostRedisplay();
            return;
        }
        if (context_menu_visible) {
            context_menu_visible = 0;
            glutPostRedisplay();
            return;
        } else if (active_idx != -1 && windows[active_idx].is_map_control) {
            windows[active_idx].is_map_control = 0;
            add_history(&windows[active_idx], "Returned to Menu Navigation.");
            
            /* Also inject ESC to project so manager knows */
            DesktopWindow* win = &windows[active_idx];
            const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
            char *history_path = NULL;
            if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                FILE *hf = fopen(history_path, "a");
                if (hf) {
                    time_t rawtime; struct tm *timeinfo; char timestamp[64];
                    time(&rawtime); timeinfo = localtime(&rawtime);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    fprintf(hf, "[%s] KEY_PRESSED: 27\n", timestamp);
                    fclose(hf);
                }
                free(history_path);
            }
            write_gl_os_frame();
            glutPostRedisplay();
            return;
        }
    }

    /* Priority Routing for gltpm_app (A23) */
    if (active_idx != -1 && strcmp(windows[active_idx].type, "gltpm_app") == 0) {
        DesktopWindow* win = &windows[active_idx];
        int in_map = win->is_map_control;

        if (in_map) {
            float speed = 0.5f;
            int moved = 0;
            int k = tolower(key);

            /* Local Camera Fly (Authority: Host for smooth movement) */
            if (k == '1') { win->camera_mode = 1; moved = 1; }
            else if (k == '2') { win->camera_mode = 2; moved = 1; }
            else if (k == '3') { win->camera_mode = 3; moved = 1; }
            else if (k == '4') { win->camera_mode = 4; moved = 1; }
            else if (k == 'w') { win->cam_pos[2] += speed; moved = 1; }
            else if (k == 's') { win->cam_pos[2] -= speed; moved = 1; }
            else if (k == 'a') { win->cam_pos[0] -= speed; moved = 1; }
            else if (k == 'd') { win->cam_pos[0] += speed; moved = 1; }
            else if (k == 'z') { win->cam_pos[1] += speed; moved = 1; }
            else if (k == 'x') { win->cam_pos[1] -= speed; moved = 1; }
            else if (k == 'q') { win->cam_rot[1] -= 5.0f; moved = 1; }
            else if (k == 'e') { win->cam_rot[1] += 5.0f; moved = 1; }

            char *history_path = NULL;
            const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
            
            if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                FILE *hf = fopen(history_path, "a");
                if (hf) {
                    time_t rawtime; struct tm *timeinfo; char timestamp[64];
                    time(&rawtime); timeinfo = localtime(&rawtime);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    
                    if (moved) {
                        /* SYNC: Inform manager of new camera state so it doesn't overwrite it */
                        fprintf(hf, "[%s] COMMAND: CAMERA_SET:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", 
                                timestamp, win->cam_pos[0], win->cam_pos[1], win->cam_pos[2],
                                win->cam_rot[0], win->cam_rot[1], win->cam_rot[2]);
                        fprintf(hf, "[%s] COMMAND: CAMERA_MODE:%d\n", timestamp, win->camera_mode);
                    } else {
                        fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, (int)tolower(key));
                    }
                    fclose(hf);
                }
                free(history_path);
            }
            glutPostRedisplay();
            return; /* YIELD */
        } else {
            /* Menu Mode: Nav > behavior */
            if (key >= '0' && key <= '9') {
                char temp_buf[32];
                strncpy(temp_buf, win->nav_buffer, 31);
                int len = strlen(temp_buf);
                if (len < 30) {
                    temp_buf[len] = key;
                    temp_buf[len+1] = '\0';
                    int target_idx = atoi(temp_buf) - 1;
                    /* VALID TARGET CHECK: Only accumulate if it points to a real button */
                    if (target_idx >= 0 && target_idx < win->gltpm_scene.button_count) {
                        strcpy(win->nav_buffer, temp_buf);
                        win->selected_index = target_idx;
                    } else {
                        /* Reset buffer if invalid multi-digit attempt, but allow single digits to start over */
                        int single_digit = key - '0' - 1;
                        if (single_digit >= 0 && single_digit < win->gltpm_scene.button_count) {
                            win->nav_buffer[0] = key;
                            win->nav_buffer[1] = '\0';
                            win->selected_index = single_digit;
                        }
                    }
                }
            } else if (key == 8 || key == 127) {
                int len = strlen(win->nav_buffer);
                if (len > 0) win->nav_buffer[len-1] = '\0';
            } else if (key == 13 || key == 10) {
                if (strlen(win->nav_buffer) > 0) {
                    int idx = atoi(win->nav_buffer) - 1;
                    if (idx >= 0 && idx < win->gltpm_scene.button_count) {
                        win->selected_index = idx;
                        dispatch_gltpm_button(win, idx);
                    }
                    win->nav_buffer[0] = '\0';
                } else {
                    dispatch_gltpm_button(win, win->selected_index);
                }
            }
            glutPostRedisplay();
            return;
        }
    }

    /* Project Mirror Controls (Hardcoded POC) */
    if (active_idx != -1 && strcmp(windows[active_idx].type, "project_mirror") == 0) {
        DesktopWindow* win = &windows[active_idx];
        if (win->is_map_control) {
            float speed = 0.5f;
            int moved = 0;
            int k = tolower(key);
            if (k == '1') { win->camera_mode = 1; moved = 1; }
            else if (k == '2') { win->camera_mode = 2; moved = 1; }
            else if (k == '3') { win->camera_mode = 3; moved = 1; }
            else if (k == '4') { win->camera_mode = 4; moved = 1; }
            else if (k == 'w') { win->cam_pos[2] += speed; moved = 1; }
            else if (k == 's') { win->cam_pos[2] -= speed; moved = 1; }
            else if (k == 'a') { win->cam_pos[0] -= speed; moved = 1; }
            else if (k == 'd') { win->cam_pos[0] += speed; moved = 1; }
            else if (k == 'z') { win->cam_pos[1] += speed; moved = 1; }
            else if (k == 'x') { win->cam_pos[1] -= speed; moved = 1; }
            else if (k == 'q') { win->cam_rot[1] -= 5.0f; moved = 1; }
            else if (k == 'e') { win->cam_rot[1] += 5.0f; moved = 1; }

            char *history_path = NULL;
            const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";
            
            if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                FILE *hf = fopen(history_path, "a");
                if (hf) {
                    time_t rawtime; struct tm *timeinfo; char timestamp[64];
                    time(&rawtime); timeinfo = localtime(&rawtime);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    
                    if (moved) {
                        fprintf(hf, "[%s] COMMAND: CAMERA_SET:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", 
                                timestamp, win->cam_pos[0], win->cam_pos[1], win->cam_pos[2],
                                win->cam_rot[0], win->cam_rot[1], win->cam_rot[2]);
                        fprintf(hf, "[%s] COMMAND: CAMERA_MODE:%d\n", timestamp, win->camera_mode);
                    } else {
                        fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, (int)tolower(key));
                    }
                    fclose(hf);
                }
                free(history_path);
            }
            glutPostRedisplay();
            return;
        } else {
            if (key >= '1' && key <= '9') {
                int idx = key - '1';
                if (idx >= 0 && idx < win->option_count) win->selected_index = idx;
            } else if (key == 13) execute_option(win, win->selected_index);
            glutPostRedisplay();
            return;
        }
    }

    /* Terminal/Standard Controls */
    int terminal_focused = (active_idx != -1 && strcmp(windows[active_idx].type, "terminal") == 0);
    if (!terminal_focused) {
        if (key == '1') { create_terminal_window(); context_menu_visible = 0; }
        else if (key == '2') { create_cube_window(); context_menu_visible = 0; }
        else if (key == 'c' || key == 'C') { if (active_window_id != -1) close_window(active_window_id); }
    }

    if (terminal_focused) {
        DesktopWindow* win = &windows[active_idx];
        int len = strlen(win->input_buffer);
        if (win->show_menu && key >= '1' && key <= '9') {
            int idx = key - '1';
            if (idx >= 0 && idx < win->option_count) win->selected_index = idx;
        }
        if (key == 13) {
            if (win->show_menu && win->option_count > 0) execute_option(win, win->selected_index);
            else process_terminal_command(win);
        } else if (key == 8 || key == 127) {
            if (len > 0) win->input_buffer[len-1] = '\0';
        } else if (len < 250 && key >= 32 && key <= 126) {
            win->input_buffer[len] = key;
            win->input_buffer[len+1] = '\0';
        }
    }

    FILE *hf = fopen(session_history_path, "a");
    if (hf) {
        time_t rawtime; struct tm *timeinfo; char timestamp[100];
        time(&rawtime); timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, key); fclose(hf);
    }
    write_session_state();
    glutPostRedisplay();
}
void special_keyboard(int key, int x, int y) {
    (void)x; (void)y;
    last_interaction_time = time(NULL);
    
    if (active_window_id != -1) {
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id) {
                idx = i;
                break;
            }
        }
        
        if (idx != -1) {
            DesktopWindow* win = &windows[idx];
            
            if (strcmp(win->type, "gltpm_app") == 0) {
                if (win->is_map_control) {
                    /* Map Mode: Route arrows to project history AND move local xelector */
                    int moved = 0;
                    if (key == GLUT_KEY_UP) { win->xelector_pos[1]--; moved = 1; }
                    else if (key == GLUT_KEY_DOWN) { win->xelector_pos[1]++; moved = 1; }
                    else if (key == GLUT_KEY_LEFT) { win->xelector_pos[0]--; moved = 1; }
                    else if (key == GLUT_KEY_RIGHT) { win->xelector_pos[0]++; moved = 1; }
                    
                    if (moved) win->last_xelector_move = time(NULL);

                    char *history_path = NULL;
                    const char* target_src = (win->gltpm_scene.interact_src[0] != '\0') ? win->gltpm_scene.interact_src : "pieces/apps/player_app/history.txt";

                    if (asprintf(&history_path, "%s/%s", project_root, target_src) != -1) {
                        FILE *hf = fopen(history_path, "a");
                        if (hf) {
                            time_t rawtime; struct tm *timeinfo; char timestamp[64];
                            time(&rawtime); timeinfo = localtime(&rawtime);
                            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                            int tpm_key = 0;
                            if (key == GLUT_KEY_LEFT) tpm_key = 1000;
                            else if (key == GLUT_KEY_RIGHT) tpm_key = 1001;
                            else if (key == GLUT_KEY_UP) tpm_key = 1002;
                            else if (key == GLUT_KEY_DOWN) tpm_key = 1003;
                            if (tpm_key > 0) fprintf(hf, "[%s] KEY_PRESSED: %d\n", timestamp, tpm_key);
                            fclose(hf);
                        }
                        free(history_path);
                    }
                    glutPostRedisplay();
                    return; /* YIELD */
                } else {
                    /* Menu Mode: Standard Nav arrows */
                    if (key == GLUT_KEY_UP) move_selection(win, -1);
                    else if (key == GLUT_KEY_DOWN) move_selection(win, 1);
                    win->nav_buffer[0] = '\0'; /* Clear jump buffer on arrow move */
                    glutPostRedisplay();
                    return; /* YIELD: Don't fall through to generic nav */
                }
            }
            else if (strcmp(win->type, "project_mirror") == 0 && win->is_map_control) {
                /* Map Control: Arrow keys move Xelector only */
                if (key == GLUT_KEY_UP) { win->xelector_pos[2]--; }
                else if (key == GLUT_KEY_DOWN) { win->xelector_pos[2]++; }
                else if (key == GLUT_KEY_LEFT) { win->xelector_pos[0]--; }
                else if (key == GLUT_KEY_RIGHT) { win->xelector_pos[0]++; }
                glutPostRedisplay();
            } else {
                /* Menu Navigation */
                int is_navigable = (strcmp(win->type, "terminal") == 0) || 
                                   (strcmp(win->type, "project_mirror") == 0 && !win->is_map_control) ||
                                   (strcmp(win->type, "gltpm_app") == 0);
                
                if (is_navigable) {
                    if (key == GLUT_KEY_UP) {
                        move_selection(win, -1);
                        write_gl_os_frame();
                        glutPostRedisplay();
                    } else if (key == GLUT_KEY_DOWN) {
                        move_selection(win, 1);
                        write_gl_os_frame();
                        glutPostRedisplay();
                    }
                }
            }
        }
    }
}

void clamp_window_bounds(DesktopWindow* win) {
    if (!win) return;
    int t_height = get_toolbar_height();
    if (win->w > window_width) win->w = window_width;
    if (win->h > window_height - t_height - 25) win->h = window_height - t_height - 25;
    if (win->w < 100) win->w = 100;
    if (win->h < 50) win->h = 50;
    if (win->x < 0) win->x = 0;
    if (win->x + win->w > window_width) win->x = window_width - win->w;
    if (win->y < 0) win->y = 0;
    int max_y = window_height - t_height - win->h;
    if (win->y > max_y) win->y = max_y;
}

void mouse(int button, int state, int x, int y) {
    mouse_x = x; mouse_y = y;
    last_interaction_time = time(NULL);
    
    printf("[MOUSE DEBUG] button=%d state=%d pos=(%d,%d)\n", button, state, x, y);
    
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        if (y > window_height - get_toolbar_height()) return;
        
        /* Check if right-clicking on a terminal window */
        context_menu_type = 0;  /* Default: desktop menu */
        int idx = find_window_at(x, y);
        if (idx != -1 && strcmp(windows[idx].type, "terminal") == 0) {
            context_menu_type = 1;  /* Terminal menu (Copy/Paste) */
        }
        
        context_menu_visible = 1; context_menu_x = x; context_menu_y = y;
        if (context_menu_x + context_menu_width > window_width) context_menu_x = window_width - context_menu_width;
        if (context_menu_y + context_menu_height > window_height) context_menu_y = window_height - context_menu_height;
        log_master("CONTEXT_MENU", (context_menu_type == 1) ? "terminal" : "desktop");
        glutPostRedisplay();
        return;
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int t_height = get_toolbar_height();
        if (y > window_height - t_height) {
            /* Check Main Window Buttons (Far Right) */
            int main_btns_x = window_width - 130;
            if (x >= main_btns_x + 90 && x <= main_btns_x + 120) {
                printf("[MAIN] Exit requested from toolbar\n");
                handle_sigint(SIGINT); return;
            }
            if (x >= main_btns_x + 50 && x <= main_btns_x + 80) {
                printf("[MAIN] Fullscreen toggle requested\n");
                g_fullscreen_enabled = !g_fullscreen_enabled;
                if (g_fullscreen_enabled) glutFullScreen();
                else { glutReshapeWindow(1024, 768); glutPositionWindow(100, 100); }
                return;
            }
            if (x >= main_btns_x + 10 && x <= main_btns_x + 40) {
                printf("[MAIN] Minimize (Iconify) requested\n");
                glutIconifyWindow(); return;
            }

            int x_off = 350;
            for (int i = 0; i < window_count; i++) {
                if (!windows[i].active) continue;
                if (x >= x_off && x <= x_off + 120) {
                    windows[i].minimized = !windows[i].minimized;
                    if (!windows[i].minimized) {
                        active_window_id = windows[i].id;
                        DesktopWindow tmp = windows[i];
                        for (int j = i; j < window_count - 1; j++) windows[j] = windows[j+1];
                        windows[window_count - 1] = tmp;
                    } else {
                        if (active_window_id == windows[i].id) active_window_id = -1;
                    }
                    write_view(); write_session_state(); glutPostRedisplay(); return;
                }
                x_off += 130;
            }
            return;
        }
        if (context_menu_visible) {
            if (x >= context_menu_x && x <= context_menu_x + context_menu_width && y >= context_menu_y && y <= context_menu_y + context_menu_height) {
                int item = (y - context_menu_y) / 25;
                
                if (context_menu_type == 1) {
                    /* Terminal context menu - Copy/Paste/Clear */
                    if (item == 0) {
                        /* Copy - copy ENTIRE current frame to clipboard.txt */
                        printf("[COPY] Copy menu selected - copying entire frame\n");
                        
                        /* Read current frame and write to clipboard.txt */
                        FILE *frame_file = fopen("pieces/apps/gl_os/session/current_frame.txt", "r");
                        FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
                        
                        if (frame_file && cb) {
                            char line[MAX_LINE];
                            printf("[COPY] Copying frame to clipboard.txt:\n");
                            while (fgets(line, sizeof(line), frame_file)) {
                                fprintf(cb, "%s", line);
                                printf("%s", line);  /* Also print to console for debug */
                            }
                            fclose(cb);
                            printf("[COPY] ✓ Frame copied to clipboard.txt\n");
                        } else {
                            printf("[COPY] ERROR: Could not open files\n");
                            if (frame_file) fclose(frame_file);
                            if (cb) fclose(cb);
                        }
                    }
                    else if (item == 1) {
                        /* Paste - paste from clipboard to input buffer */
                        if (active_window_id != -1) {
                            for (int i = 0; i < window_count; i++) {
                                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                                    paste_from_clipboard(windows[i].input_buffer, sizeof(windows[i].input_buffer));
                                    break;
                                }
                            }
                        }
                    }
                    else if (item == 2) {
                        /* Clear - clear terminal history */
                        if (active_window_id != -1) {
                            for (int i = 0; i < window_count; i++) {
                                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                                    windows[i].history_count = 0;
                                    windows[i].input_buffer[0] = '\0';
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    /* Desktop context menu */
                    if (item == 0) create_terminal_window();
                    else if (item == 1) create_cube_window();
                }
                context_menu_visible = 0; glutPostRedisplay(); return;
            }
            context_menu_visible = 0;
        }
        int idx = find_window_at(x, y);
        printf("[MOUSE DEBUG] find_window_at returned idx=%d\n", idx);
        
        if (idx != -1) {
            printf("[MOUSE DEBUG] Window %d type='%s' pos=(%d,%d) size=(%d,%d)\n",
                   windows[idx].id, windows[idx].type,
                   windows[idx].x, windows[idx].y,
                   windows[idx].w, windows[idx].h);
            
            int win_id = windows[idx].id;
            if (idx < window_count - 1) {
                DesktopWindow clicked_win = windows[idx];
                for (int i = idx; i < window_count - 1; i++) windows[i] = windows[i + 1];
                windows[window_count - 1] = clicked_win;
                idx = window_count - 1;
            }
            active_window_id = win_id;

            /* Check if clicking in terminal content area (for text selection) */
            if (strcmp(windows[idx].type, "terminal") == 0) {
                printf("[SELECT DEBUG] Clicked on terminal window\n");

                /* Check if NOT in titlebar */
                int in_titlebar = is_in_titlebar(x, y, &windows[idx]);
                printf("[SELECT DEBUG] in_titlebar=%d\n", in_titlebar);

                if (!in_titlebar) {
                    int res_x = windows[idx].x + windows[idx].w - 15;
                    int res_y = windows[idx].y + windows[idx].h - 15;
                    int thumb_x = windows[idx].x + windows[idx].w - 12;
                    int titlebar_y = windows[idx].y + windows[idx].h - 25;

                    /* Content area: left of thumb, above resize handle, below titlebar */
                    if (x < thumb_x && y > res_y && y < titlebar_y) {
                        windows[idx].is_selecting = 1;
                        windows[idx].select_start_x = x;
                        windows[idx].select_start_y = y;
                        windows[idx].select_end_x = x;
                        windows[idx].select_end_y = y;
                        windows[idx].selection_active = 0;
                        windows[idx].selected_text[0] = '\0';
                    }
                }
            }
            /* INTEGRATED EMOJI STUDIO PICKER */
            else if (strcmp(windows[idx].type, "emoji_studio") == 0) {
                int rel_x = x - windows[idx].x;
                int rel_y = (window_height - y) - (window_height - windows[idx].y - windows[idx].h);
                
                int grid_x_start = 15;
                int grid_y_top = windows[idx].h - 50;
                int grid_cols = 6;
                int cell_size = 32;
                int spacing = 5;

                if (rel_x >= grid_x_start && rel_y <= grid_y_top && rel_y >= 150) {
                    int col = (rel_x - grid_x_start) / (cell_size + spacing);
                    int row = (grid_y_top - rel_y) / (cell_size + spacing);
                    int emoji_idx = row * grid_cols + col;
                    
                    if (emoji_idx >= 0 && emoji_idx < 30) {
                        printf("[EMOJI] Selected index %d via integrated picker\n", emoji_idx);
                        char cmd[64]; snprintf(cmd, sizeof(cmd), "SELECT %d", emoji_idx);
                        
                        char path[MAX_PATH];
                        snprintf(path, sizeof(path), "%s/projects/emoji-studio/session/history.txt", project_root);
                        FILE *hf = fopen(path, "a");
                        if (hf) {
                            fprintf(hf, "COMMAND: %s\n", cmd);
                            fclose(hf);
                        }
                    }
                }
            }

            /* Check if clicking on scroll thumb (right edge of terminal window) */
            if (strcmp(windows[idx].type, "terminal") == 0) {
                int thumb_x = windows[idx].x + windows[idx].w - 12;
                int thumb_right = windows[idx].x + windows[idx].w - 4;
                /* Check if click is in thumb track area */
                if (x >= thumb_x && x <= thumb_right && y >= scroll_track_y && y <= scroll_track_y + scroll_track_h) {
                    is_scroll_thumb_drag = 1;
                    scroll_window_id = win_id;
                    glutPostRedisplay();
                    return;
                }
            }
            
            if (is_in_titlebar(x, y, &windows[idx])) {
                int close_x = windows[idx].x + windows[idx].w - 22;
                int max_x = windows[idx].x + windows[idx].w - 44;
                int half_x = windows[idx].x + windows[idx].w - 66;
                int min_x = windows[idx].x + windows[idx].w - 88;
                int btn_y = windows[idx].y + 4;
                
                if (x >= close_x && x <= close_x + 18 && y >= btn_y && y <= btn_y + 18) close_window(win_id);
                else if (x >= max_x && x <= max_x + 18 && y >= btn_y && y <= btn_y + 18) maximize_window(&windows[idx]);
                else if (x >= half_x && x <= half_x + 18 && y >= btn_y && y <= btn_y + 18) half_screen_window(&windows[idx]);
                else if (x >= min_x && x <= min_x + 18 && y >= btn_y && y <= btn_y + 18) { windows[idx].minimized = 1; active_window_id = -1; }
                else { 
                    is_dragging = 1; drag_window_id = win_id; drag_start_x = x; drag_start_y = y; 
                    win_drag_start_x = windows[idx].x; win_drag_start_y = windows[idx].y;
                    windows[idx].is_maximized = 0; windows[idx].is_half_screen = 0;
                }
            } else {
                int res_x = windows[idx].x + windows[idx].w - 15;  /* Match resize handle size */
                int res_y = windows[idx].y + windows[idx].h - 15;
                if (x >= res_x && x <= res_x + 15 && y >= res_y && y <= res_y + 15) { 
                    is_resizing = 1; drag_window_id = win_id; drag_start_x = x; drag_start_y = y; 
                    win_drag_start_w = windows[idx].w; win_drag_start_h = windows[idx].h;
                    windows[idx].is_maximized = 0; windows[idx].is_half_screen = 0;
                }
            }
            write_session_state();
        } else active_window_id = -1;
        glutPostRedisplay();
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        is_dragging = 0; is_resizing = 0; drag_window_id = -1;
        is_scroll_thumb_drag = 0; scroll_window_id = -1;
        
        /* Finalize text selection */
        if (active_window_id != -1) {
            for (int i = 0; i < window_count; i++) {
                if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                    if (windows[i].is_selecting) {
                        windows[i].is_selecting = 0;
                        windows[i].selection_active = 1;

                        /* Calculate selection dimensions */
                        int dx = windows[i].select_end_x - windows[i].select_start_x;
                        int dy = windows[i].select_end_y - windows[i].select_start_y;

                        printf("[SELECT] FINALIZED selection at (%d,%d) to (%d,%d) delta=(%d,%d)\n",
                               windows[i].select_start_x, windows[i].select_start_y,
                               windows[i].select_end_x, windows[i].select_end_y,
                               dx, dy);

                        /* TODO: Extract text from frame based on selection coordinates */
                        /* For now, just mark selection as active */
                        if (strlen(windows[i].selected_text) == 0) {
                            strncpy(windows[i].selected_text, "[Selected text]", sizeof(windows[i].selected_text) - 1);
                        }
                        
                        /* Write to clipboard.txt immediately */
                        FILE *cb = fopen("pieces/apps/gl_os/session/clipboard.txt", "w");
                        if (cb) {
                            fprintf(cb, "%s\n", windows[i].selected_text);
                            fclose(cb);
                            printf("[SELECT] Written to clipboard.txt\n");
                        }
                    } else {
                        printf("[SELECT DEBUG] Mouse up but is_selecting=0\n");
                    }
                    break;
                }
            }
        }
    }
}

void motion(int x, int y) {
    mouse_x = x; mouse_y = y;
    last_interaction_time = time(NULL);

    /* Handle text selection in terminal */
    if (active_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == active_window_id && strcmp(windows[i].type, "terminal") == 0) {
                if (windows[i].is_selecting) {
                    windows[i].select_end_x = x;
                    windows[i].select_end_y = y;
                    glutPostRedisplay();
                    return;
                }
            }
        }
    }
    
    /* Handle scroll thumb dragging */
    if (is_scroll_thumb_drag && scroll_window_id != -1) {
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == scroll_window_id && strcmp(windows[i].type, "terminal") == 0) {
                /* Calculate scroll based on mouse Y position in thumb track
                 * Linux terminal behavior: drag thumb UP → see OLDER content (scroll_offset increases)
                 * drag thumb DOWN → see NEWER content (scroll_offset decreases to 0)
                 * GLUT y is top-down, so smaller y = higher on screen
                 */
                int track_y = scroll_track_y;
                int track_h = scroll_track_h;
                
                /* Thumb height (same as in draw_scroll_thumb) */
                int visible_lines = (track_h - 20) / 18;
                int total_lines = windows[i].max_scroll + visible_lines;
                float thumb_h_ratio = (float)visible_lines / (float)total_lines;
                if (thumb_h_ratio < 0.1f) thumb_h_ratio = 0.1f;
                int thumb_h = (int)(track_h * thumb_h_ratio);
                if (thumb_h < 10) thumb_h = 10;
                
                /* Calculate new scroll position from mouse Y
                 * Mouse y is GLUT (top-down), track_y is bottom of track
                 * Drag UP (smaller y) → increase scroll_offset (see older)
                 * Drag DOWN (larger y) → decrease scroll_offset (see newer)
                 */
                int track_h_adj = track_h - thumb_h;
                /* Invert: mouse at top (small y) = max_scroll, mouse at bottom (large y) = 0 */
                float mouse_ratio = 1.0f - ((float)(y - track_y - thumb_h/2) / (float)track_h_adj);
                if (mouse_ratio < 0.0f) mouse_ratio = 0.0f;
                if (mouse_ratio > 1.0f) mouse_ratio = 1.0f;
                
                windows[i].scroll_offset = (int)(mouse_ratio * windows[i].max_scroll);
                glutPostRedisplay();
                return;
            }
        }
    }
    
    if (drag_window_id != -1) {
        int dx = x - drag_start_x; int dy = y - drag_start_y;
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == drag_window_id) { idx = i; break; }
        }
        if (idx != -1) {
            if (is_dragging) {
                windows[idx].x = win_drag_start_x + dx;
                windows[idx].y = win_drag_start_y + dy;
            } else if (is_resizing) {
                windows[idx].w = win_drag_start_w + dx;
                windows[idx].h = win_drag_start_h + dy;
            }
            clamp_window_bounds(&windows[idx]);
            write_session_state(); glutPostRedisplay();
        }
    }
}

void reshape(int w, int h) { window_width = w; window_height = h; glViewport(0, 0, w, h); }

void free_paths(void) {
    if (session_state_path) free(session_state_path); if (session_history_path) free(session_history_path);
    if (session_view_path) free(session_view_path); if (session_view_changed_path) free(session_view_changed_path);
    if (master_ledger_path) free(master_ledger_path);
    if (input_focus_lock_path) free(input_focus_lock_path);
    if (gl_os_frame_path) free(gl_os_frame_path);
    if (gl_os_frame_pulse) free(gl_os_frame_pulse);
    if (gl_os_frame_history) free(gl_os_frame_history);
    if (gl_os_audit_frame_path) free(gl_os_audit_frame_path);
}

/* Write GL-OS frame to file (like ASCII-OS compose_frame) */
void write_gl_os_frame(void) {
    /* Write state first (for parser) */
    write_gl_os_state();
    if (run_gl_os_audit_frame_op() != 0) {
        write_gl_os_audit_frame_inline();
    }
    
    /* Parse layout and write frame */
    parse_and_write_frame();
}

int main(int argc, char** argv) {
#ifndef _WIN32
    setpgid(0, 0); // CPU Safety: Become group leader
#endif
    
    /* Register signal handlers for clean exit (Ctrl+C) */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    glutInit(&argc, argv); glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height); glutCreateWindow("GL-OS Desktop");
    if (g_fullscreen_enabled) glutFullScreen();
    
    /* Zero-initialize all window state */
    memset(windows, 0, sizeof(windows));
    
    resolve_root();
    load_gl_projects();
    init_emoji_atlas();
    asprintf(&session_state_path, "%s/pieces/apps/gl_os/session/state.txt", project_root);
    asprintf(&session_history_path, "%s/pieces/apps/gl_os/session/history.txt", project_root);
    asprintf(&session_view_path, "%s/pieces/apps/gl_os/session/view.txt", project_root);
    asprintf(&session_view_changed_path, "%s/pieces/apps/gl_os/session/view_changed.txt", project_root);
    asprintf(&master_ledger_path, "%s/pieces/apps/gl_os/ledger/master_ledger.txt", project_root);
    asprintf(&input_focus_lock_path, "%s/pieces/apps/gl_os/session/input_focus.lock", project_root);
    asprintf(&gl_os_frame_path, "%s/pieces/apps/gl_os/session/current_frame.txt", project_root);
    asprintf(&gl_os_frame_pulse, "%s/pieces/apps/gl_os/session/gl_os_pulse.txt", project_root);
    asprintf(&gl_os_frame_history, "%s/pieces/apps/gl_os/session/frame_history.txt", project_root);
    asprintf(&gl_os_audit_frame_path, "%s/pieces/apps/gl_os/session/audit_frame.txt", project_root);

    last_interaction_time = time(NULL);

    /* Create input focus lock - tells CHTPM daemons to stop writing input */
    FILE *lock = fopen(input_focus_lock_path, "w");
    if (lock) {
        fprintf(lock, "# GL-OS Input Focus Lock\n");
        fprintf(lock, "# CHTPM daemons: DO NOT write to input files while this file exists\n");
        fprintf(lock, "owner=gl_desktop\n");
        fprintf(lock, "session=gl-os\n");
        fclose(lock);
    }

    /* Initialize session files (clear history on each launch) */
    FILE *f; f = fopen(session_state_path, "w"); if (f) fclose(f);  /* Clear state */
    f = fopen(session_history_path, "w"); if (f) fclose(f);        /* Clear history */
    f = fopen(session_view_path, "w"); if (f) fclose(f);           /* Clear view */
    f = fopen(session_view_changed_path, "w"); if (f) fclose(f);   /* Clear marker */
    f = fopen(master_ledger_path, "a"); if (f) fclose(f);          /* Keep ledger (append) */
    f = fopen(gl_os_frame_path, "w"); if (f) fclose(f);            /* Clear frame */
    f = fopen(gl_os_frame_history, "w"); if (f) fclose(f);         /* Clear frame history */
    f = fopen(gl_os_audit_frame_path, "w"); if (f) fclose(f);      /* Clear audit frame */

    create_terminal_window();
    printf("[GL-OS] Desktop Environment Starting... (GLTPM Support ENABLED)\n");
    log_master("DESKTOP_START", "gl-os");
    write_gl_os_frame();  /* Write initial frame to file */
    glutDisplayFunc(render_scene); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard); 
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion); glutTimerFunc(30, timer, 0);
    glutJoystickFunc(joystick, 50);  /* Joystick callback (50ms poll rate) */
    glutMainLoop();
    
    /* Remove input focus lock on exit - CHTPM can resume input */
    if (input_focus_lock_path) {
        remove(input_focus_lock_path);
    }
    
    free_paths(); return 0;
}
