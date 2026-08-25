/*
 * op-ed_module.c - Advanced RMMP Editor (Unified Trait Edition)
 * CPU-SAFE: Signal handling + fork/exec/waitpid pattern
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
/* Simple strcasestr for Windows */
char* strcasestr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (toupper(*haystack) == toupper(*needle)) {
            const char *h, *n;
            for (h = haystack, n = needle; *h && *n; h++, n++) {
                if (toupper(*h) != toupper(*n)) break;
            }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}
#endif

#define MAX_PATH 4096
#define MAX_CMD 16384
#define MAX_LINE 1024
#define MAX_ITEMS 50

/* CPU-SAFE: Global shutdown flag and signal handler */
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/* CPU-SAFE: Helper to run external commands with fork/exec/waitpid */
static int run_command(const char* cmd) {
#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: redirect stdout/stderr to /dev/null */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        /* Parent: wait for child */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
#else
    return system(cmd);
#endif
    return -1;
}

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "op-ed"; // Default to op-ed project
char active_game_name[MAX_LINE] = "none";

/* Browser State (Agy-inspired) */
static char current_browser_dir[MAX_PATH] = "projects/op-ed/games";
static char file_path_input_buffer[MAX_PATH] = "";
static char search_query_buffer[MAX_LINE] = "";
static int browser_mode = 0; // 0 = Load, 1 = Save As

/* Map Palette */
char project_maps[MAX_ITEMS][MAX_LINE];
int map_count = 0;
int map_idx = 0;
int stage_map_idx = -1; /* Tracks which map is actually active for editing */

/* Glyph Palette */
const char *ascii_glyphs[] = {"#", ".", "R", "T", "@", "&", "Z", "X", "?", "!"};
const char *emoji_glyphs[] = {"🧱", "🟩", "🌲", "🏰", "🎯", "🐶", "🧟", "💰", "🏠", "🔥"};
int glyph_idx = 0;
int emoji_mode = 0;

/* State */
int gui_focus_index = 1;
int cursor_x = 5, cursor_y = 5, cursor_z = 0;
char active_target_id[64] = "xlector";
char last_key_str[32] = "None";
int last_sync_idx = 0;
char current_world_id[64] = "world_01";
char current_map_dir[64] = "map_01";

/* Prototypes */
static void load_project_metadata(void);
static void save_project(void);

static void transition_to_layout(const char *layout_path) {
    char *lp = NULL;
    if (asprintf(&lp, "%s/pieces/display/layout_changed.txt", project_root) != -1) {
        FILE *f = fopen(lp, "a");
        if (f) { fprintf(f, "%s\n", layout_path); fclose(f); }
        free(lp);
    }
}
static void scan_maps(void);
static void sync_current_map(void);
static void update_current_map_dir(void);
static void write_gui_state(void);
static int process_key(int key);

char* trim_str(char *str) {
    char *end;
    if(!str) return str;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static int get_active_gui_index(void) {
    char *path = NULL;
    int idx = 0;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r");
    if (f) { if (fscanf(f, "%d", &idx) != 1) idx = 0; fclose(f); }
    free(path);
    return idx;
}

static void get_current_layout_name(char *buf, size_t sz) {
    buf[0] = '\0';
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        if (fgets(line, sizeof(line), f)) {
            char *trimmed = trim_str(line);
            char *last_slash = strrchr(trimmed, '/');
            if (last_slash) strncpy(buf, last_slash + 1, sz - 1);
            else strncpy(buf, trimmed, sz - 1);
            buf[sz - 1] = '\0';
        }
        fclose(f);
    }
    free(path);
}

static int get_digits(int num) {
    if (num >= 100) return 3;
    if (num >= 10) return 2;
    return 1;
}

static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void append_aligned_button_attr(char *out, size_t max_sz, const char *label, const char *attr_name, const char *attr_val, int *p_display_num) {
    int num = *p_display_num;
    int digits = get_digits(num);
    int label_len = strlen(label);
    
    int visual_len = 8 + digits + label_len;
    int padding = 40 - visual_len;
    if (padding < 0) padding = 0;
    
    char btn_markup[1024];
    snprintf(btn_markup, sizeof(btn_markup), "<text label=\"║  \" /><button label=\"%s\" %s=\"%s\" />", label, attr_name, attr_val);
    strncat(out, btn_markup, max_sz - strlen(out) - 1);
    
    if (padding > 0) {
        char pad_str[128];
        snprintf(pad_str, sizeof(pad_str), "<text label=\"%.*s\" />", padding, "                                                                                ");
        strncat(out, pad_str, max_sz - strlen(out) - 1);
    }
    strncat(out, "<text label=\" ║\" /><br/>", max_sz - strlen(out) - 1);
    
    (*p_display_num)++;
}

static int find_autocomplete_matches(const char *input, const char *cur_dir, char matches[][256], int max_matches) {
    char dir_to_scan[MAX_PATH];
    const char *prefix = "";
    
    const char *last_slash = strrchr(input, '/');
    if (last_slash) {
        size_t dir_len = last_slash - input;
        if (dir_len >= sizeof(dir_to_scan)) dir_len = sizeof(dir_to_scan) - 1;
        strncpy(dir_to_scan, input, dir_len);
        dir_to_scan[dir_len] = '\0';
        prefix = last_slash + 1;
        if (strlen(dir_to_scan) == 0) strcpy(dir_to_scan, ".");
    } else {
        prefix = input;
        strcpy(dir_to_scan, cur_dir);
    }
    
    char full_scan_path[MAX_PATH];
    snprintf(full_scan_path, sizeof(full_scan_path), "%s/%s", project_root, dir_to_scan);
    
    DIR *d = opendir(full_scan_path);
    if (!d) return 0;
    
    struct dirent *entry;
    int count = 0;
    size_t prefix_len = strlen(prefix);
    
    while ((entry = readdir(d)) != NULL && count < max_matches) {
        if (entry->d_name[0] == '.') continue;
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            char entry_path[MAX_PATH];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", full_scan_path, entry->d_name);
            struct stat st;
            int is_dir = 0;
            if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
            
            if (last_slash) snprintf(matches[count], 256, "%.*s/%s%s", (int)(last_slash - input), input, entry->d_name, is_dir ? "/" : "");
            else snprintf(matches[count], 256, "%s%s", entry->d_name, is_dir ? "/" : "");
            count++;
        }
    }
    closedir(d);
    return count;
}

void derive_map_dir_name(const char* map_file, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!map_file || !*map_file) { snprintf(out, out_sz, "map_01"); return; }
    snprintf(out, out_sz, "%s", map_file);
    char *dot = strstr(out, ".txt");
    if (dot) *dot = '\0';
    char *z = strstr(out, "_z");
    if (z) {
        char *p = z + 2;
        int digits = 0;
        while (*p && isdigit((unsigned char)*p)) { digits = 1; p++; }
        if (digits && *p == '\0') *z = '\0';
    }
}

int resolve_piece_dir(const char* piece_id, char* out_dir, size_t out_sz) {
    if (!piece_id || !out_dir || out_sz == 0) return 0;

    if (strlen(current_world_id) > 0 && strlen(current_map_dir) > 0) {
        snprintf(out_dir, out_sz, "%s/projects/%s/pieces/%s/%s/%s",
                 project_root, current_project, current_world_id, current_map_dir, piece_id);
        if (access(out_dir, F_OK) == 0) return 1;
    }

    snprintf(out_dir, out_sz, "%s/projects/%s/pieces/%s", project_root, current_project, piece_id);
    if (access(out_dir, F_OK) == 0) return 1;

    out_dir[0] = '\0';
    return 0;
}

void update_current_map_dir() {
    if (stage_map_idx >= 1 && stage_map_idx < map_count) {
        derive_map_dir_name(project_maps[stage_map_idx], current_map_dir, sizeof(current_map_dir));
    }
}

/* PDL Reader Utility - reads METHOD from piece.pdl files */
static int get_method(const char* piece_id, const char* event, 
               char* out_handler, size_t out_size,
               const char* project_root, const char* current_project) {
    char pdl_path[MAX_PATH];
    char piece_dir[MAX_PATH];
    if (!resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir))) return -1;
    snprintf(pdl_path, sizeof(pdl_path), "%.*s/piece.pdl", 4000, piece_dir);
    
    FILE *f = fopen(pdl_path, "r");
    if (!f) return -1;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) == 0) {
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;
            
            char parsed_event[64];
            size_t event_len = pipe2 - pipe1 - 1;
            if (event_len >= sizeof(parsed_event)) event_len = sizeof(parsed_event) - 1;
            strncpy(parsed_event, pipe1 + 1, event_len);
            parsed_event[event_len] = '\0';
            char* trimmed_event = trim_str(parsed_event);
            
            char parsed_handler[MAX_PATH];
            strncpy(parsed_handler, pipe2 + 1, sizeof(parsed_handler) - 1);
            parsed_handler[sizeof(parsed_handler) - 1] = '\0';
            char* trimmed_handler = trim_str(parsed_handler);
            
            if (strcmp(trimmed_event, event) == 0) {
                strncpy(out_handler, trimmed_handler, out_size - 1);
                out_handler[out_size - 1] = '\0';
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line), *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void scan_maps() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/maps", project_root, current_project) == -1) return;
    DIR *dir = opendir(path);
    if (!dir) { 
        map_count = 1;
        strcpy(project_maps[0], "ADD");
        free(path); return; 
    }
    map_count = 0;
    strcpy(project_maps[map_count++], "ADD");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && map_count < MAX_ITEMS) {
        if (strstr(entry->d_name, ".txt")) strncpy(project_maps[map_count++], entry->d_name, MAX_LINE - 1);
    }
    closedir(dir); free(path);
}

void trigger_render() {
    char *cmd = NULL;
    if (asprintf(&cmd, "'%s/pieces/apps/playrm/ops/+x/render_map.+x' > /dev/null 2>&1", project_root) != -1) {
        run_command(cmd); free(cmd);
    }
}

void hit_frame_marker() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/frame_changed.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "M\n"); fclose(f); }
        free(path);
    }
}

void set_response(const char* msg) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/editor/response.txt", project_root) != -1) {
        FILE *f = fopen(path, "w"); if (f) { fprintf(f, "%-57s", msg); fclose(f); }
        free(path);
    }
}

void sync_current_map() {
    if (stage_map_idx < 1 || stage_map_idx >= map_count) return;
    update_current_map_dir();
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 49) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line_buf), "current_map") == 0) {
                        asprintf(&lines[line_count], "current_map=%s\n", project_maps[stage_map_idx]);
                        found = 1;
                    } else {
                        *eq = '=';
                        asprintf(&lines[line_count], "%s\n", line_buf);
                    }
                } else {
                    asprintf(&lines[line_count], "%s", line_buf);
                }
                line_count++;
            }
            fclose(f);
        }
        if (!found && line_count < 50) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            asprintf(&lines[line_count++], "current_map=%s\n", project_maps[stage_map_idx]);
        }
        f = fopen(path, "w");
        if (f) { for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); } fclose(f); }
        free(lines);
        free(path);
    }
}

void save_xlector_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "pos_x=%d\npos_y=%d\npos_z=%d\nactive=1\nicon=X\ntype=xlector\n", cursor_x, cursor_y, cursor_z);
            fclose(f);
        }
        free(path);
    }
}

void create_new_map() {
    int max_num = 0;
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/maps", project_root, current_project) == -1) return;
    DIR *dir = opendir(path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "map_", 4) == 0) {
                int num = atoi(entry->d_name + 4);
                if (num > max_num) max_num = num;
            }
        }
        closedir(dir);
    }
    char new_map[MAX_LINE]; snprintf(new_map, sizeof(new_map), "map_%04d.txt", max_num + 1);
    char *full_path = NULL;
    if (asprintf(&full_path, "%s/%s", path, new_map) != -1) {
        FILE *f = fopen(full_path, "w");
        if (f) {
            for (int y = 0; y < 10; y++) {
                for (int x = 0; x < 20; x++) fputc('.', f);
                fputc('\n', f);
            }
            fclose(f); set_response("New Map Created");
        }
        free(full_path);
    }
    free(path); scan_maps();
    for (int i = 1; i < map_count; i++) {
        if (strcmp(project_maps[i], new_map) == 0) { map_idx = i; break; }
    }
}

int get_state_int_fast(const char* piece_id, const char* key) {
    char piece_dir[MAX_PATH];
    if (!resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir))) return -1;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%.*s/state.txt", 4000, piece_dir);
    FILE *f = fopen(path, "r");
    char line[MAX_LINE]; int val = -1;
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; if (strcmp(trim_str(line), key) == 0) { val = atoi(trim_str(eq + 1)); break; } }
        }
        fclose(f);
    }
    return val;
}

void sync_emoji_mode() {
    /* TPM Direct Mirror Access: Write emoji_mode to project's sovereign state */
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/pieces/emoji_mode.txt", project_root, current_project) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "emoji_mode=%d\n", emoji_mode);
            fclose(f);
        }
        free(path);
    }
    
    /* Also write to manager state for global visibility */
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        /* Read existing state */
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 99) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(trim_str(line_buf), "emoji_mode") == 0) {
                        asprintf(&lines[line_count], "emoji_mode=%d\n", emoji_mode);
                        found = 1;
                    } else {
                        *eq = '=';
                        asprintf(&lines[line_count], "%s\n", line_buf);
                    }
                } else {
                    asprintf(&lines[line_count], "%s", line_buf);
                }
                line_count++;
            }
            fclose(f);
        }
        if (!found && line_count < 100) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            asprintf(&lines[line_count++], "emoji_mode=%d\n", emoji_mode);
        }
        f = fopen(path, "w");
        if (f) {
            for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); }
            fclose(f);
        }
        free(lines);
        free(path);
    }
}

static void read_file_path_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'f') { // id="file_path_input" -> prefix 'f'
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                char *val = trim_str(line + 1);
                if (strlen(val) > 0 || strlen(file_path_input_buffer) == 0) {
                    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
                }
            }
        }
        fclose(f);
    }
    free(path);
}

static void read_search_query_input(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 's') { // id="search_query" -> prefix 's'
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                strncpy(search_query_buffer, trim_str(line + 1), sizeof(search_query_buffer) - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

static void set_file_path_input(const char *val) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "f%s\n", val); fclose(f); }
        free(path);
    }
    strncpy(file_path_input_buffer, val, sizeof(file_path_input_buffer) - 1);
}

static void clear_search_query(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
        FILE *f = fopen(path, "a");
        if (f) { fprintf(f, "s\n"); fclose(f); }
        free(path);
    }
    search_query_buffer[0] = '\0';
}

static int save_game_to_path(const char *rel_path) {
    char full_dest[MAX_PATH];
    snprintf(full_dest, sizeof(full_dest), "%s/%s", project_root, rel_path);
    
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", full_dest);
    run_command(cmd);

    save_project();

    snprintf(cmd, sizeof(cmd), "cp -r '%s/projects/op-ed/maps' '%s/'", project_root, full_dest);
    run_command(cmd);
    snprintf(cmd, sizeof(cmd), "cp -r '%s/projects/op-ed/pieces' '%s/'", project_root, full_dest);
    run_command(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/projects/op-ed/project.pdl' '%s/'", project_root, full_dest);
    run_command(cmd);

    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Game saved to %s", rel_path);
    set_response(msg);
    strncpy(active_game_name, rel_path, sizeof(active_game_name) - 1);
    return 0;
}

static int load_game_from_path(const char *rel_path) {
    char full_src[MAX_PATH];
    snprintf(full_src, sizeof(full_src), "%s/%s", project_root, rel_path);
    
    if (access(full_src, F_OK) != 0) {
        set_response("Error: Game folder not found");
        return -1;
    }

    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/projects/op-ed/maps'/* '%s/projects/op-ed/pieces'/*", project_root, project_root);
    run_command(cmd);

    snprintf(cmd, sizeof(cmd), "cp -r '%s/maps'/* '%s/projects/op-ed/maps/'", full_src, project_root);
    run_command(cmd);
    snprintf(cmd, sizeof(cmd), "cp -r '%s/pieces'/* '%s/projects/op-ed/pieces/'", full_src, project_root);
    run_command(cmd);
    snprintf(cmd, sizeof(cmd), "cp '%s/project.pdl' '%s/projects/op-ed/'", full_src, project_root);
    run_command(cmd);

    load_project_metadata();
    scan_maps();
    
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Loaded game: %s", rel_path);
    set_response(msg);
    strncpy(active_game_name, rel_path, sizeof(active_game_name) - 1);
    return 0;
}

void handle_command(const char* cmd) {
    if (strncmp(cmd, "KEY:", 4) == 0) {
        process_key(atoi(cmd + 4));
        return;
    }
    if (strncmp(cmd, "SET_DIR:", 8) == 0) {
        strncpy(current_browser_dir, cmd + 8, sizeof(current_browser_dir) - 1);
        if (strlen(current_browser_dir) == 0) strcpy(current_browser_dir, ".");
        clear_search_query();
    } else if (strncmp(cmd, "SET_PATH:", 9) == 0) {
        set_file_path_input(cmd + 9);
    } else if (strncmp(cmd, "SET_AUTOCOMPLETE:", 17) == 0) {
        set_file_path_input(cmd + 17);
    } else if (strcmp(cmd, "SET_LOAD_ACTION") == 0) {
        read_file_path_input();
        load_game_from_path(file_path_input_buffer);
        transition_to_layout("projects/op-ed/layouts/op-ed.chtpm");
    } else if (strcmp(cmd, "SET_SAVE_ACTION") == 0) {
        read_file_path_input();
        save_game_to_path(file_path_input_buffer);
        transition_to_layout("projects/op-ed/layouts/op-ed.chtpm");
    }
}

void write_gui_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/op-ed/manager/gui_state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "module_path=projects/op-ed/manager/+x/op-ed_manager.+x\n");
            fprintf(f, "active_layout_id=op-ed.chtpm\n");
            fprintf(f, "app_title=OP-ED ADVANCED EDITOR\n");
            fprintf(f, "project_id=%s\n", current_project);
            fprintf(f, "active_game=%s\n", active_game_name);
            fprintf(f, "active_gui_index=%d\n", gui_focus_index);
            fprintf(f, "last_key=%s\n", last_key_str);
            fprintf(f, "selector_pos=(%d,%d,%d)\n", cursor_x, cursor_y, cursor_z);
            
            read_file_path_input();
            read_search_query_input();
            
            if (browser_mode == 0) fprintf(f, "browser_mode_header=<text label=\"║  MODE: LOAD GAME                            ║\" /><br/>\n");
            else fprintf(f, "browser_mode_header=<text label=\"║  MODE: SAVE GAME AS                         ║\" /><br/>\n");
            
            fprintf(f, "browser_current_dir_line=<text label=\"║  DIR: %-35.35s ║\" /><br/>\n", current_browser_dir);
            fprintf(f, "search_query_val=%s\n", search_query_buffer);
            fprintf(f, "file_path_input_val=%s\n", file_path_input_buffer);

            char autocomplete_markup[8192] = "";
            char suggestions[4][256];
            int num_suggestions = find_autocomplete_matches(file_path_input_buffer, current_browser_dir, suggestions, 4);
            int next_display_num = 3;
            if (num_suggestions > 0) {
                strcat(autocomplete_markup, "<text label=\"║  SUGGESTIONS:                               ║\" /><br/>");
                for (int i = 0; i < num_suggestions; i++) {
                    char display_label[256];
                    if (strlen(suggestions[i]) > 28) snprintf(display_label, sizeof(display_label), "...%s", suggestions[i] + strlen(suggestions[i]) - 25);
                    else strcpy(display_label, suggestions[i]);
                    char action[512]; snprintf(action, sizeof(action), "SET_AUTOCOMPLETE:%s", suggestions[i]);
                    append_aligned_button_attr(autocomplete_markup, sizeof(autocomplete_markup), display_label, "onClick", action, &next_display_num);
                }
            } else {
                strcat(autocomplete_markup, "<text label=\"║  (Type to see autocompletions)              ║\" /><br/>");
            }
            fprintf(f, "autocomplete_suggestions_markup=%s\n", autocomplete_markup);

            char browser_markup[8192] = "";
            if (strcmp(current_browser_dir, ".") != 0 && strlen(current_browser_dir) > 0) {
                char parent_dir[MAX_PATH];
                char *last_slash = strrchr(current_browser_dir, '/');
                if (last_slash) { strncpy(parent_dir, current_browser_dir, last_slash - current_browser_dir); parent_dir[last_slash - current_browser_dir] = '\0'; }
                else strcpy(parent_dir, ".");
                char action[MAX_PATH + 10]; snprintf(action, sizeof(action), "SET_DIR:%s", parent_dir);
                append_aligned_button_attr(browser_markup, sizeof(browser_markup), "<- BACK", "onClick", action, &next_display_num);
            }

            DIR *d = opendir(current_browser_dir);
            if (d) {
                struct dirent *entry;
                char subdirs[MAX_ITEMS][MAX_LINE];
                int subdir_count = 0;
                while ((entry = readdir(d)) != NULL && subdir_count < MAX_ITEMS) {
                    if (entry->d_name[0] == '.') continue;
                    if (strlen(search_query_buffer) > 0 && strcasestr(entry->d_name, search_query_buffer) == NULL) continue;
                    char entry_path[MAX_PATH]; snprintf(entry_path, sizeof(entry_path), "%s/%s", current_browser_dir, entry->d_name);
                    struct stat st;
                    if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) strncpy(subdirs[subdir_count++], entry->d_name, MAX_LINE - 1);
                }
                closedir(d);
                qsort(subdirs, subdir_count, MAX_LINE, compare_names);
                for (int i = 0; i < subdir_count; i++) {
                    char next_dir[MAX_PATH];
                    if (strcmp(current_browser_dir, ".") == 0) snprintf(next_dir, sizeof(next_dir), "%s", subdirs[i]);
                    else snprintf(next_dir, sizeof(next_dir), "%s/%s", current_browser_dir, subdirs[i]);
                    char action[MAX_PATH + 10]; snprintf(action, sizeof(action), "SET_DIR:%s", next_dir);
                    char btn_label[MAX_LINE]; snprintf(btn_label, sizeof(btn_label), "[DIR] %s/", subdirs[i]);
                    append_aligned_button_attr(browser_markup, sizeof(browser_markup), btn_label, "onClick", action, &next_display_num);
                }
            }
            fprintf(f, "directory_browser_markup=%s\n", browser_markup);

            char action_markup[8192] = "";
            if (browser_mode == 0) append_aligned_button_attr(action_markup, sizeof(action_markup), "LOAD GAME", "onClick", "SET_LOAD_ACTION", &next_display_num);
            else append_aligned_button_attr(action_markup, sizeof(action_markup), "SAVE GAME", "onClick", "SET_SAVE_ACTION", &next_display_num);
            append_aligned_button_attr(action_markup, sizeof(action_markup), "CANCEL", "href", "projects/op-ed/layouts/file_menu.chtpm", &next_display_num);
            fprintf(f, "browser_action_buttons_markup=%s\n", action_markup);

            char raw_resp[MAX_LINE] = "";
            char *resp_path = NULL;
            if (asprintf(&resp_path, "%s/pieces/apps/editor/response.txt", project_root) != -1) {
                FILE *rf = fopen(resp_path, "r");
                if (rf) { if (fgets(raw_resp, sizeof(raw_resp), rf)) trim_str(raw_resp); fclose(rf); }
                free(resp_path);
            }
            fprintf(f, "editor_response=[RESP]: %-49s\n", raw_resp);
            
            char stats[MAX_LINE];
            snprintf(stats, sizeof(stats), "[POS]: (%d,%d,%d) | [H-BEAT]: %d | [KEY]: %-10s", cursor_x, cursor_y, cursor_z, last_sync_idx, last_key_str);
            fprintf(f, "editor_status_2=%-57s\n", stats);
            
            char maps_str[MAX_LINE] = "";
            for (int i = 0; i < map_count && strlen(maps_str) < 100; i++) {
                if (i == map_idx) strcat(maps_str, "[");
                strcat(maps_str, project_maps[i]);
                if (i == map_idx) strcat(maps_str, "]");
                strcat(maps_str, " ");
            }
            fprintf(f, "map_palette_view=%s\n", maps_str);
            
            char gly_str[MAX_LINE] = "";
            const char **active_set = emoji_mode ? emoji_glyphs : ascii_glyphs;
            for (int i = 0; i < 10; i++) {
                if (i == glyph_idx) strcat(gly_str, "[");
                strcat(gly_str, active_set[i]);
                if (i == glyph_idx) strcat(gly_str, "]");
                strcat(gly_str, " ");
            }
            fprintf(f, "glyph_palette_view=%s\n", gly_str);
            fprintf(f, "armed_glyph=%s\n", active_set[glyph_idx]);
            
            fclose(f);
        }
        free(path);
    }
}

void write_editor_state() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        char **lines = NULL;
        int line_count = 0, found_proj = 0, found_target = 0, found_z = 0, found_key = 0, found_focus = 0;
        char line_buf[MAX_LINE];
        if (f) {
            while (fgets(line_buf, sizeof(line_buf), f) && line_count < 49) {
                lines = realloc(lines, sizeof(char*) * (line_count + 1));
                char *eq = strchr(line_buf, '=');
                if (eq) {
                    *eq = '\0';
                    char *k = trim_str(line_buf);
                    if (strcmp(k, "project_id") == 0) { asprintf(&lines[line_count], "project_id=%s\n", current_project); found_proj = 1; }
                    else if (strcmp(k, "active_target_id") == 0) { asprintf(&lines[line_count], "active_target_id=%s\n", active_target_id); found_target = 1; }
                    else if (strcmp(k, "current_z") == 0) { asprintf(&lines[line_count], "current_z=%d\n", cursor_z); found_z = 1; }
                    else if (strcmp(k, "last_key") == 0) { asprintf(&lines[line_count], "last_key=%s\n", last_key_str); found_key = 1; }
                    else if (strcmp(k, "gui_focus") == 0) { asprintf(&lines[line_count], "gui_focus=%d\n", gui_focus_index); found_focus = 1; }
                    else { *eq = '='; asprintf(&lines[line_count], "%s\n", line_buf); }
                } else { asprintf(&lines[line_count], "%s", line_buf); }
                line_count++;
            }
            fclose(f);
        }
        if (!found_proj && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "project_id=%s\n", current_project); }
        if (!found_target && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "active_target_id=%s\n", active_target_id); }
        if (!found_z && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "current_z=%d\n", cursor_z); }
        if (!found_key && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "last_key=%s\n", last_key_str); }
        if (!found_focus && line_count < 50) { lines = realloc(lines, sizeof(char*) * (line_count + 1)); asprintf(&lines[line_count++], "gui_focus=%d\n", gui_focus_index); }
        f = fopen(path, "w");
        if (f) { for (int i = 0; i < line_count; i++) { fputs(lines[i], f); free(lines[i]); } fclose(f); }
        free(lines);
        free(path);
    }
    char *sel_path = NULL;
    if (asprintf(&sel_path, "%s/projects/%s/pieces/xlector/state.txt", project_root, current_project) != -1) {
        FILE *sf = fopen(sel_path, "w");
        if (sf) { fprintf(sf, "name=Xlector\ntype=xlector\npos_x=%d\npos_y=%d\npos_z=%d\non_map=1\n", cursor_x, cursor_y, cursor_z); fclose(sf); }
        free(sel_path);
    }
    write_gui_state();
}

char my_layout_path[MAX_PATH] = "pieces/apps/op-ed/layouts/op-ed.chtpm";

void resolve_my_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/apps/op-ed/op-ed.pdl", project_root) == -1) return;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "layout_path")) {
                char *pipe = strrchr(line, '|');
                if (pipe) strncpy(my_layout_path, trim_str(pipe + 1), MAX_PATH - 1);
            }
        }
        fclose(f);
    }
    free(path);
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r"); if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) { 
        fclose(f); 
        char *cur = trim_str(line);
        int res = 0;
        if (cur && strstr(cur, "op-ed.chtpm") != NULL) res = 1;
        else {
            if (cur && strstr(cur, "pal_editor.chtpm") != NULL) res = 0;
            else if (cur && (strstr(cur, "pieces/apps/op-ed/layouts/") != NULL ||
                             strstr(cur, "projects/op-ed/layouts/") != NULL)) res = 1;
        }
        free(path); 
        return res; 
    }
    fclose(f); free(path); return 0;
}

void sync_focus() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/active_gui_index.txt", project_root) != -1) {
        FILE *f = fopen(path, "r");
        if (f) { int active_idx = 0; if (fscanf(f, "%d", &active_idx) == 1) { last_sync_idx = active_idx; if (active_idx > 0) gui_focus_index = active_idx; } fclose(f); }
        free(path);
    }
}

void save_project() {
    char *path = NULL;
    if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, current_project) == -1) return;
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "SECTION      | KEY                | VALUE\n");
        fprintf(f, "----------------------------------------\n");
        fprintf(f, "META         | project_id         | %s\n", current_project);
        fprintf(f, "META         | version            | 1.0\n");
        fprintf(f, "META         | entry_layout       | pieces/apps/op-ed/layouts/op-ed.chtpm\n\n");
        fprintf(f, "STATE        | starting_map       | %s\n", current_map_dir);
        fprintf(f, "STATE        | player_piece       | %s\n", active_target_id);
        fprintf(f, "STATE        | current_world      | %s\n", current_world_id);
        fprintf(f, "STATE        | current_map        | %s\n\n", current_map_dir);
        fprintf(f, "RESPONSE     | default            | Project %s Saved.\n", current_project);
        fclose(f);
        set_response("Project Saved Successfully");
    }
    free(path);
}

void trigger_event(const char* piece_id, const char* event) {
    char handler[MAX_PATH];
    if (get_method(piece_id, event, handler, sizeof(handler), project_root, current_project) == 0) {
        char *cmd = NULL;
        if (strstr(handler, ".asm")) {
            asprintf(&cmd, "PRISC_PROJECT_ID=%s PRISC_PROJECT_ROOT='%s' '%s/pieces/system/prisc/prisc+x' '%s/%s' > /dev/null 2>&1",
                     current_project, project_root, project_root, project_root, handler);
        } else { asprintf(&cmd, "'%s/%s' > /dev/null 2>&1", project_root, handler); }
        if (cmd) { run_command(cmd); char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "Triggered %s on %s", event, piece_id); set_response(msg); free(cmd); }
    } else { char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "No handler for %s on %s", event, piece_id); set_response(msg); }
}

void bind_event(const char* piece_id, const char* event, const char* handler) {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/master_ledger/plugins/+x/piece_manager.+x %s add-method %s %s > /dev/null 2>&1",
                 project_root, piece_id, event, handler) != -1) {
        run_command(cmd); free(cmd);
        char msg[MAX_LINE]; snprintf(msg, sizeof(msg), "Bound %s to %s", event, piece_id);
        set_response(msg);
    }
}


int process_key(int key) {
    char layout[MAX_LINE];
    get_current_layout_name(layout, sizeof(layout));

    if (key == 127 || key == 8) {
        if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            if (active_idx == 1) { // search_query
                read_search_query_input();
                int len = strlen(search_query_buffer);
                if (len > 0) {
                    search_query_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) { fprintf(bf, "s%s\n", search_query_buffer); fclose(bf); }
                        free(path);
                    }
                }
                return 1;
            } else if (active_idx == 2) { // file_path_input
                read_file_path_input();
                int len = strlen(file_path_input_buffer);
                if (len > 0) {
                    file_path_input_buffer[len - 1] = '\0';
                    char *path = NULL;
                    if (asprintf(&path, "%s/pieces/apps/player_app/cli_buffers.txt", project_root) != -1) {
                        FILE *bf = fopen(path, "a");
                        if (bf) { fprintf(bf, "f%s\n", file_path_input_buffer); fclose(bf); }
                        free(path);
                    }
                }
                return 1;
            }
        }
    } else if (key == 10 || key == 13) {
        if (strcmp(layout, "file_browser.chtpm") == 0) {
            int active_idx = get_active_gui_index();
            if (active_idx == 2) { // file_path_input
                read_file_path_input();
                if (strlen(file_path_input_buffer) > 0) {
                    if (browser_mode == 0) handle_command("SET_LOAD_ACTION");
                    else handle_command("SET_SAVE_ACTION");
                    return 1;
                }
            } else if (active_idx == 1) { read_search_query_input(); return 1; }
        }
    }

    if (key >= 32 && key <= 126) {
        if (key == ' ') strcpy(last_key_str, "SPACE");
        else snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    }
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000) strcpy(last_key_str, "LEFT");
    else if (key == 1001) strcpy(last_key_str, "RIGHT");
    else if (key == 1002) strcpy(last_key_str, "UP");
    else if (key == 1003) strcpy(last_key_str, "DOWN");

    if (strcmp(layout, "file_menu.chtpm") == 0) {
        if (key == '7') {
            if (strcmp(active_game_name, "none") == 0) {
                browser_mode = 1; clear_search_query(); set_file_path_input("projects/op-ed/games/new-game");
                transition_to_layout("projects/op-ed/layouts/file_browser.chtpm");
                set_response("Specify Save-As path.");
            } else { save_game_to_path(active_game_name); }
            return 1;
        }
        if (key == '8') { // Save As
            browser_mode = 1; clear_search_query(); set_file_path_input(active_game_name);
            transition_to_layout("projects/op-ed/layouts/file_browser.chtpm");
            set_response("Enter Save-As path.");
            return 1;
        }
        if (key == '9') { // Load
            browser_mode = 0; clear_search_query(); set_file_path_input("projects/op-ed/games/");
            transition_to_layout("projects/op-ed/layouts/file_browser.chtpm");
            set_response("Select game folder to load.");
            return 1;
        }
        if (key == '5') { // BACK
            transition_to_layout("projects/op-ed/layouts/op-ed.chtpm");
            return 1;
        }
        if (key == '4') { // PLAY
            transition_to_layout("pieces/apps/playrm/layouts/game.chtpm");
            set_response("Switching to Play Mode...");
            return 1;
        }
    }

    if (key == 'p' || key == 'P') {
        transition_to_layout("pieces/apps/playrm/layouts/game.chtpm");
        set_response("Switching to Play Mode...");
        return 1;
    }
    if (key == 'e' || key == '7') {
        const char *target = (strcmp(active_target_id, "xlector") == 0) ? "common_event" : active_target_id;
        
        char *mgr_path = NULL;
        if (asprintf(&mgr_path, "%s/projects/op-ed/manager/state.txt", project_root) != -1) {
            FILE *f = fopen(mgr_path, "w");
            if (f) {
                fprintf(f, "project_id=%s\n", current_project);
                fprintf(f, "active_target_id=%s\n", active_target_id);
                fprintf(f, "pal_editor_piece=%s\n", target);
                fprintf(f, "pal_editor_event=on_interact\n");
                fprintf(f, "current_z=%d\n", cursor_z);
                fprintf(f, "last_key=None\n");
                fprintf(f, "current_map=%s\n", current_map_dir);
                fclose(f);
            }
            free(mgr_path);
        }
        transition_to_layout("projects/op-ed/layouts/pal_editor.chtpm");
        hit_frame_marker();
        set_response("Event Builder Opened");
        return 1;
    }

    if (key == '9' || key == 27) { strcpy(active_target_id, "xlector"); set_response("Returned to Xlector"); return 1; }
    if (key >= '1' && key <= '9') {
        if (key == '4') { emoji_mode = !emoji_mode; set_response(emoji_mode ? "Emoji Mode: ON" : "Emoji Mode: OFF"); sync_emoji_mode(); return 1; }
        else if (key == '8') { if (strcmp(active_target_id, "xlector") != 0) bind_event(active_target_id, "interact", "projects/man-pal/scripts/move.asm"); else set_response("Select a piece first!"); return 1; }
        else { gui_focus_index = key - '0'; return 1; }
    }

    if (key == 122) { char *cmd = NULL; if (asprintf(&cmd, "'%s/pieces/apps/playrm/ops/+x/undo_action.+x' > /dev/null 2>&1", project_root) != -1) { run_command(cmd); free(cmd); set_response("Undo performed"); } return 1; }

    if (key == 127 || key == 8) { if (gui_focus_index == 1 && stage_map_idx > 0 && stage_map_idx < map_count) { char *cmd = NULL; if (asprintf(&cmd, "'%s/pieces/apps/playrm/ops/+x/place_tile.+x' %s %d %d . > /dev/null 2>&1", project_root, project_maps[stage_map_idx], cursor_x, cursor_y) != -1) { run_command(cmd); free(cmd); set_response("Tile cleared"); } } return 1; }

    if (gui_focus_index == 1) {
        if (key == 'w' || key == 'W' || key == 1002 || key == 's' || key == 'S' || key == 1003 || key == 'a' || key == 'A' || key == 1000 || key == 'd' || key == 'D' || key == 1001) {
            const char *dir = (key == 'w' || key == 'W' || key == 1002) ? "w" : (key == 's' || key == 'S' || key == 1003) ? "s" : (key == 'a' || key == 'A' || key == 1000) ? "a" : "d";
            if (strcmp(active_target_id, "xlector") == 0) {
                if (strcmp(dir, "w") == 0) cursor_y--; else if (strcmp(dir, "s") == 0) cursor_y++; else if (strcmp(dir, "a") == 0) cursor_x--; else if (strcmp(dir, "d") == 0) cursor_x++;
                if (cursor_x < 0) cursor_x = 0; if (cursor_x >= 100) cursor_x = 99; if (cursor_y < 0) cursor_y = 0; if (cursor_y >= 100) cursor_y = 99;
                save_xlector_state();
            } else {
                char piece_dir[MAX_PATH]; if (!resolve_piece_dir(active_target_id, piece_dir, sizeof(piece_dir))) { strcpy(active_target_id, "xlector"); set_response("Entity not found"); }
                else { char *cmd = NULL; if (asprintf(&cmd, "'%s/pieces/apps/playrm/ops/+x/move_entity.+x' %s %s %s > /dev/null 2>&1", project_root, active_target_id, dir, current_project) != -1) { run_command(cmd); free(cmd); }
                    int px = get_state_int_fast(active_target_id, "pos_x"), py = get_state_int_fast(active_target_id, "pos_y");
                    if (px != -1 && py != -1) { cursor_x = px; cursor_y = py; save_xlector_state(); }
                }
            }
            return 1;
        }
        if (key == ' ') {
            if (strcmp(active_target_id, "xlector") == 0) {
                int found = 0; char map_path[MAX_PATH]; snprintf(map_path, sizeof(map_path), "%s/projects/%s/pieces/%s/%s", project_root, current_project, current_world_id, current_map_dir);
                DIR *dir = opendir(map_path); if (dir) { struct dirent *entry; while ((entry = readdir(dir)) != NULL) { if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue; int tx = get_state_int_fast(entry->d_name, "pos_x"), ty = get_state_int_fast(entry->d_name, "pos_y"); if (tx == cursor_x && ty == cursor_y) { strncpy(active_target_id, entry->d_name, 63); set_response("Selected Piece"); found = 1; break; } } closedir(dir); }
                if (!found) set_response("No piece at cursor");
            } else { trigger_event(active_target_id, "interact"); }
            return 1;
        }
    }
    else if (gui_focus_index == 2) { 
        if (key == 1000 || key == 'a') { map_idx--; if (map_idx < 0) map_idx = map_count - 1; } 
        else if (key == 1001 || key == 'd') { map_idx++; if (map_idx >= map_count) map_idx = 0; } 
        else if (key == 10 || key == 13) { if (map_idx == 0) create_new_map(); stage_map_idx = map_idx; sync_current_map(); set_response("Map Switched"); }
        return 1;
    }
    else if (gui_focus_index == 3) { 
        if (key == 1000 || key == 'a') { glyph_idx--; if (glyph_idx < 0) glyph_idx = 9; } 
        else if (key == 1001 || key == 'd') { glyph_idx++; if (glyph_idx >= 10) glyph_idx = 0; } 
        return 1;
    }

    if ((key == 10 || key == 13) && gui_focus_index == 1) {
        if (stage_map_idx < 1) { set_response("Select a map first!"); return 1; }
        const char **active_set = emoji_mode ? emoji_glyphs : ascii_glyphs; const char *glyph = active_set[glyph_idx];
        int is_entity = (strcmp(glyph, "@") == 0 || strcmp(glyph, "&") == 0 || strcmp(glyph, "Z") == 0 || strcmp(glyph, "T") == 0);
        char *cmd = NULL;
        if (is_entity) {
            const char *type = (strcmp(glyph, "@") == 0) ? "player" : (strcmp(glyph, "&") == 0) ? "npc" : (strcmp(glyph, "Z") == 0) ? "zombie" : "chest";
            asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/create_piece.+x '%s' %d %d %s --world %s --map %s > /dev/null 2>&1", project_root, type, cursor_x, cursor_y, current_project, current_world_id, current_map_dir);
        } else { asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/place_tile.+x %s %d %d '%s' > /dev/null 2>&1", project_root, project_maps[stage_map_idx], cursor_x, cursor_y, glyph); }
        if (cmd) { run_command(cmd); free(cmd); set_response(is_entity ? "Piece created" : "Tile placed"); }
        return 1;
    }
    return 0;
}

static void load_project_metadata(void) {
    char *path = NULL; if (asprintf(&path, "%s/projects/%s/project.pdl", project_root, current_project) == -1) return;
    FILE *f = fopen(path, "r"); if (f) { char line[MAX_LINE]; while (fgets(line, sizeof(line), f)) { char *pipe1 = strchr(line, '|'), *pipe2 = strrchr(line, '|'); if (pipe1 && pipe2 && pipe1 != pipe2) { char key[MAX_LINE]; strncpy(key, trim_str(pipe1 + 1), MAX_LINE-1); char *val = trim_str(pipe2 + 1); if (strstr(key, "current_world")) strncpy(current_world_id, val, 63); else if (strstr(key, "current_map")) strncpy(current_map_dir, val, 63); else if (strstr(key, "player_piece")) strncpy(active_target_id, val, 63); } } fclose(f); }
    free(path);
}

int main() {
    signal(SIGINT, handle_sigint); signal(SIGTERM, handle_sigint);
#ifndef _WIN32
    setpgid(0, 0); 
#endif
    resolve_paths(); resolve_my_layout();
    load_project_metadata(); scan_maps();
    if (stage_map_idx == -1) { if (map_count > 1) stage_map_idx = 1; else stage_map_idx = 0; map_idx = stage_map_idx; }
    write_editor_state(); trigger_render(); hit_frame_marker();
    long last_pos = 0; struct stat st; char *hist_path = NULL; if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) return 1;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }
        sync_focus();
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r"); if (hf) {
                    fseek(hf, last_pos, SEEK_SET); char line[MAX_LINE]; int processed = 0;
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: "), *kpress = strstr(line, "KEY_PRESSED: ");
                        if (cmd) { handle_command(trim_str(cmd + 9)); processed = 1; }
                        else if (kpress) { if (process_key(atoi(kpress + 13))) processed = 1; }
                        else { int key = atoi(line); if (key != 0 || line[0] == '0') { if (process_key(key)) processed = 1; } }
                    }
                    if (processed) { write_editor_state(); trigger_render(); hit_frame_marker(); }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    free(hist_path); return 0;
}
