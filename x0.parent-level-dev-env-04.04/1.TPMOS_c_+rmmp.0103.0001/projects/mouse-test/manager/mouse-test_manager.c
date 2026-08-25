#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_PATH 4096

char project_root[MAX_PATH] = ".";
int mouse_x = 0;
int mouse_y = 0;
int mouse_btn = 0;
char mouse_lock_path[MAX_PATH] = "";
char legacy_mouse_lock_path[MAX_PATH] = "";

void remove_mouse_lock(void);
void handle_signal(int sig);
void enable_mouse_mode(void);

char* trim_str(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v);
            }
        }
        fclose(kvp);
    }
}

void remove_mouse_lock(void) {
    if (mouse_lock_path[0] != '\0') {
        remove(mouse_lock_path);
    }
    if (legacy_mouse_lock_path[0] != '\0') {
        remove(legacy_mouse_lock_path);
    }
}

void handle_signal(int sig) {
    (void)sig;
    remove_mouse_lock();
    exit(0);
}

void enable_mouse_mode(void) {
    char mouse_dir[MAX_PATH];

    snprintf(mouse_dir, sizeof(mouse_dir), "%s/pieces/mouse", project_root);
    mkdir(mouse_dir, 0777);

    snprintf(mouse_lock_path, sizeof(mouse_lock_path), "%s/pieces/mouse/mouse_enabled.lock", project_root);
    snprintf(legacy_mouse_lock_path, sizeof(legacy_mouse_lock_path), "%s/projects/mouse-test/session/mouse_enabled.lock", project_root);

    FILE *lock = fopen(mouse_lock_path, "w");
    if (lock) {
        fprintf(lock, "mouse-test\n");
        fclose(lock);
    }

    /* Transitional compatibility: keep the legacy project lock until all launch/kill paths are normalized. */
    lock = fopen(legacy_mouse_lock_path, "w");
    if (lock) {
        fprintf(lock, "mouse-test\n");
        fclose(lock);
    }
}

void write_gui_state() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/projects/mouse-test/manager/gui_state.txt", project_root);
    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return;
    fprintf(f, "app_title=Mouse Test\n");
    fprintf(f, "mouse_x=%d\n", mouse_x);
    fprintf(f, "mouse_y=%d\n", mouse_y);
    fprintf(f, "mouse_btn=%d\n", mouse_btn);
    fclose(f);
    rename(tmp_path, path);
}

void pulse_marker() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "M\n");
        fclose(f);
    }
}

int main() {
    setpgid(0, 0);
    resolve_paths();
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(remove_mouse_lock);
    enable_mouse_mode();
    if (chdir(project_root) != 0) { /* Fallback to current dir */ }

    char history_path[MAX_PATH];
    snprintf(history_path, sizeof(history_path), "projects/mouse-test/session/history.txt");
    
    long last_pos = 0;
    struct stat st;

    /* Initialize GUI state */
    write_gui_state();

    while (1) {
        if (stat(history_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(history_path, "r");
            if (hf) {
                fseek(hf, last_pos, SEEK_SET);
                char line[MAX_LINE];
                int updated = 0;
                while (fgets(line, sizeof(line), hf)) {
                    if (strstr(line, "COMMAND: MOUSE_MOVE ")) {
                        char *p = strstr(line, "MOUSE_MOVE ") + 11;
                        if (sscanf(p, "%d %d %d", &mouse_btn, &mouse_x, &mouse_y) == 3) {
                            updated = 1;
                        }
                    }
                }
                last_pos = ftell(hf);
                fclose(hf);
                
                if (updated) {
                    write_gui_state();
                    pulse_marker();
                }
            }
        } else if (st.st_size < last_pos) {
            last_pos = 0;
        }
        
        usleep(16667); /* 60 FPS */
    }
    return 0;
}
