#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define MAX_MP3S 50
#define MAX_PATH 4096

typedef struct {
    char name[256];
    char filename[256];
} MP3File;

MP3File library[MAX_MP3S];
int mp3_count = 0;
char current_mp3[256] = "None";
char player_status[64] = "Stopped";
int is_paused = 0;

char project_root[MAX_PATH] = ".";
char mp3_library_root[MAX_PATH] = "";
char current_project[64] = "mp3-store";

static volatile sig_atomic_t g_shutdown = 0;
void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

static char* trim_pmo(char *s) {
    char *p = s;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
    return s;
}

char* build_path_malloc(const char* rel) {
    size_t sz = strlen(project_root) + strlen(rel) + 2;
    char* p = (char*)malloc(sz);
    if (p) snprintf(p, sz, "%s/%s", project_root, rel);
    return p;
}

void resolve_paths() {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[1024];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_pmo(line);
                char *v = trim_pmo(eq + 1);
                if (strcmp(k, "project_root") == 0) strncpy(project_root, v, MAX_PATH - 1);
            }
        }
        fclose(kvp);
    }

    char config_path[MAX_PATH];
    snprintf(config_path, sizeof(config_path), "%s/projects/mp3-store/pieces/user-config.txt", project_root);
    FILE *cfg = fopen(config_path, "r");
    if (cfg) {
        char line[MAX_PATH];
        if (fgets(line, sizeof(line), cfg)) {
            char *path = trim_pmo(line);
            if (path[0] == '/') strncpy(mp3_library_root, path, MAX_PATH - 1);
            else snprintf(mp3_library_root, MAX_PATH, "%s/%s", project_root, path);
        }
        fclose(cfg);
    } else {
        snprintf(mp3_library_root, MAX_PATH, "%s/#.mp3-library", project_root);
    }
}

void hit_frame_marker() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

void write_app_context() {
    char path[MAX_PATH], mpath[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", project_root);
    snprintf(mpath, sizeof(mpath), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=mp3-store\n");
        fprintf(f, "app_title=MP3 STORE PLAYER\n");
        fprintf(f, "active_target_id=mp3_player\n");
        fprintf(f, "current_mp3=%s\n", current_mp3);
        fprintf(f, "player_status=%s\n", player_status);
        fclose(f);
    }
    f = fopen(mpath, "w");
    if (f) {
        fprintf(f, "project_id=mp3-store\n");
        fprintf(f, "active_target_id=mp3_player\n");
        fclose(f);
    }
}

void write_pdl() {
    char dir_path[MAX_PATH], pdl_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s/projects/mp3-store/pieces/mp3_player", project_root);
    mkdir(dir_path, 0755);
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", dir_path);
    
    FILE *f = fopen(pdl_path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | mp3_player\n");
    fprintf(f, "STATE        | name               | MP3 Player Engine\n\n");
    
    for (int i = 0; i < mp3_count; i++) {
        fprintf(f, "METHOD       | %s | MP3:PLAY:%s\n", library[i].name, library[i].filename);
    }
    fprintf(f, "METHOD       | %s | MP3:TOGGLE_PAUSE\n", is_paused ? "Resume Audio" : "Pause Audio");
    fprintf(f, "METHOD       | Stop Audio | MP3:STOP\n");
    fclose(f);
}

void scan_library() {
    mp3_count = 0;
    DIR *dir = opendir(mp3_library_root);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".mp3")) {
            strncpy(library[mp3_count].filename, entry->d_name, 255);
            strncpy(library[mp3_count].name, entry->d_name, 255);
            char *ext = strrchr(library[mp3_count].name, '.');
            if (ext) *ext = '\0';
            mp3_count++;
            if (mp3_count >= MAX_MP3S) break;
        }
    }
    closedir(dir);
}

void play_mp3(const char* filename) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", mp3_library_root, filename);
    char op_path[MAX_PATH];
    snprintf(op_path, sizeof(op_path), "%s/projects/mp3-store/ops/+x/play_mp3.+x", project_root);
    
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, full_path, NULL);
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        strncpy(current_mp3, filename, 255);
        strcpy(player_status, "Playing");
        is_paused = 0;
    }
}

void stop_mp3() {
    char op_path[MAX_PATH];
    snprintf(op_path, sizeof(op_path), "%s/projects/mp3-store/ops/+x/play_mp3.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, "--stop", NULL);
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        system("killall -9 mpg123 2>/dev/null");
        strcpy(current_mp3, "None");
        strcpy(player_status, "Stopped");
        is_paused = 0;
    }
}

void toggle_pause() {
    char op_path[MAX_PATH];
    snprintf(op_path, sizeof(op_path), "%s/projects/mp3-store/ops/+x/play_mp3.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        execl(op_path, op_path, is_paused ? "--resume" : "--pause", NULL);
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        is_paused = !is_paused;
        strcpy(player_status, is_paused ? "Paused" : "Playing");
    }
}

void process_command(const char* cmd) {
    FILE *df = fopen("debug.txt", "a");
    if (df) { fprintf(df, "MP3_MANAGER: process_command(%s)\n", cmd); fclose(df); }

    if (strcmp(cmd, "MP3:STOP") == 0) {
        stop_mp3();
    } else if (strcmp(cmd, "MP3:TOGGLE_PAUSE") == 0) {
        toggle_pause();
    } else if (strncmp(cmd, "MP3:PLAY:", 9) == 0) {
        play_mp3(cmd + 9);
    }
    write_app_context();
    write_pdl();
    hit_frame_marker();
}

void process_input(int key) {
    FILE *df = fopen("debug.txt", "a");
    if (df) { fprintf(df, "MP3_MANAGER: process_input(key=%d)\n", key); fclose(df); }

    if (key == 27) return; 
    
    if (key == '9' || key == 9) {
        stop_mp3();
        char *layout_marker = build_path_malloc("pieces/display/layout_changed.txt");
        FILE *lf = fopen(layout_marker, "w");
        if (lf) { fprintf(lf, "pieces/chtpm/layouts/os.chtpm"); fclose(lf); }
        free(layout_marker);
        return;
    }

    if (key == 32) { // Spacebar
        toggle_pause();
    } else {
        int num = -1;
        if (key >= '1' && key <= '9') num = key - '0';
        else if (key >= 1 && key <= 9) num = key;
        
        if (num != -1) {
            int idx = num - 1;
            if (idx >= 0 && idx < mp3_count) {
                play_mp3(library[idx].filename);
            } else if (idx == mp3_count) { // Toggle Pause button
                toggle_pause();
            } else if (idx == mp3_count + 1) { // Stop button
                stop_mp3();
            }
        }
    }
    write_app_context();
    write_pdl();
    hit_frame_marker();
}

int is_active_layout() {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    int res = 0;
    if (fgets(line, sizeof(line), f)) res = (strstr(line, "mp3-store.chtpm") != NULL);
    fclose(f);
    return res;
}

int main() {
    signal(SIGINT, handle_sigint);
    resolve_paths();
    scan_library();
    write_app_context();
    write_pdl();
    hit_frame_marker();

    long last_pos = 0; struct stat st;
    char history_path[MAX_PATH];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    if (stat(history_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[512];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd = strstr(line, "COMMAND: ");
                        if (cmd) process_command(trim_pmo(cmd + 9));
                        else {
                            char *kp = strstr(line, "KEY_PRESSED: ");
                            if (kp) {
                                int key = atoi(kp + 13);
                                if (key > 0) process_input(key);
                            } else {
                                int key = atoi(line);
                                if (key > 0) process_input(key);
                            }
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        usleep(16667);
    }
    return 0;
}
