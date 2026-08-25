#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

// GL-Op-Ed Manager (v1.0 - Sovereign God Mode)
// Responsibility: Route input for gl-op-ed, manage editor camera, and dispatch place_tile Ops.

char current_project[64] = "op-ed-gl";
char active_target_id[64] = "xlector";
char last_key_str[64] = "None";
char armed_glyph[16] = "#";
int current_z_val = 0;
int is_map_control = 0;

float cam_pos[3] = {0.0f, 2.0f, 10.0f};
float cam_rot[3] = {30.0f, 0.0f, 0.0f};
int camera_mode = 4; // Free

char project_root[2048] = ".";
float speed = 0.5f;

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

void resolve_paths() {
    if (!getcwd(project_root, sizeof(project_root))) strncpy(project_root, ".", sizeof(project_root) - 1);
    
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[2048];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0 && *v) {
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

void sync_camera_piece() {
    char path[4096];
    char dir_path[4096];
    snprintf(dir_path, sizeof(dir_path), "%s/projects/%s/pieces/camera", project_root, current_project);
    snprintf(path, sizeof(path), "%s/state.txt", dir_path);
    
    mkdir(dir_path, 0755);
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "name=EditorCamera\ntype=camera\n");
        fprintf(f, "pos_x=%.2f\npos_y=%.2f\npos_z=%.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
        fprintf(f, "pitch=%.2f\nyaw=%.2f\nroll=%.2f\n", cam_rot[0], cam_rot[1], cam_rot[2]);
        fprintf(f, "mode=%d\nis_map_control=%d\n", camera_mode, is_map_control);
        fclose(f);
    }
}

void save_manager_state() {
    char path[4096];
    // 1. PROJECT-SPECIFIC session state (GLTPM Compliance)
    snprintf(path, sizeof(path), "%s/projects/%s/session/state.txt", project_root, current_project);
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s\n", current_project);
        fprintf(f, "active_target_id=%s\n", active_target_id);
        fprintf(f, "last_key=%s\n", last_key_str);
        fprintf(f, "is_map_control=%d\n", is_map_control);
        fprintf(f, "camera_mode=%d\n", camera_mode);
        fprintf(f, "cam_x=%.2f\ncam_y=%.2f\ncam_z=%.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
        fprintf(f, "cam_pitch=%.2f\ncam_yaw=%.2f\ncam_roll=%.2f\n", cam_rot[0], cam_rot[1], cam_rot[2]);
        fprintf(f, "armed_glyph=%s\n", armed_glyph);
        fprintf(f, "current_z=%d\n", current_z_val);
        fprintf(f, "current_map=map_01_z0.txt\n");
        
        // Expose coordinates for HUD
        int sel_x = 12, sel_y = 2; // Default if file not found
        fprintf(f, "xelector_x=%d\nxelector_y=%d\n", sel_x, sel_y);
        fclose(f);
    }
    
    sync_camera_piece();
}

void trigger_render() {
    char gl_pulse[4096];
    snprintf(gl_pulse, sizeof(gl_pulse), "%s/projects/%s/session/frame_changed.txt", project_root, current_project);
    FILE *pf = fopen(gl_pulse, "a");
    if (pf) { fprintf(pf, "G\n"); fclose(pf); }
}

void* gltpm_input_thread(void* arg) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/projects/%s/session/history.txt", project_root, current_project);
    long last_pos = 0;
    struct stat st;
    
    while (1) {
        if (stat(path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *f = fopen(path, "r");
                if (f) {
                    fseek(f, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), f)) {
                        char *kpress = strstr(line, "KEY_PRESSED:");
                        char *cmd = strstr(line, "COMMAND:");
                        
                        if (cmd && strstr(cmd, "INTERACT")) {
                            is_map_control = !is_map_control;
                            save_manager_state();
                            trigger_render();
                        } else if (kpress) {
                            int key = atoi(kpress + 12);
                            if (key == 27) { is_map_control = 0; }
                            else if (is_map_control) {
                                if (key == 'w') cam_pos[2] += speed;
                                else if (key == 's') cam_pos[2] -= speed;
                                else if (key == 'a') cam_pos[0] -= speed;
                                else if (key == 'd') cam_pos[0] += speed;
                                else if (key == 'z') cam_pos[1] += speed;
                                else if (key == 'x') cam_pos[1] -= speed;
                                else if (key == 'q') cam_rot[1] -= 5.0f;
                                else if (key == 'e') cam_rot[1] += 5.0f;
                            }
                            snprintf(last_key_str, 63, "%d", key);
                            save_manager_state();
                            trigger_render();
                        }
                    }
                    last_pos = ftell(f);
                    fclose(f);
                }
            }
        }
        usleep(16667);
    }
    return NULL;
}

int main() {
    resolve_paths();
    save_manager_state();
    trigger_render();
    
    pthread_t t1;
    pthread_create(&t1, NULL, gltpm_input_thread, NULL);
    
    while (1) { usleep(1000000); }
    return 0;
}
