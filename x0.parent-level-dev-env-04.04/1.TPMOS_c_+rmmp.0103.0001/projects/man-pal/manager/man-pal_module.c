/*
 * man-pal_module.c - Optimized PAL-RELAIER Module (CPU Compliant)
 * Purpose: Prove the PAL pipeline by delegating logic to Prisc VM with batching.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <direct.h>
#include <process.h>
#include <io.h>
#define usleep(us) Sleep((us)/1000)
#define ftruncate _chsize
#define REALPATH(path, resolved) _fullpath(resolved, path, 4096)
#else
#include <unistd.h>
#define REALPATH(path, resolved) realpath(path, resolved)
#endif
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

/* MAX_PATH per compile clean standards - use ifndef to avoid Windows redefinition */
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MAX_LINE 1024

char project_root[MAX_PATH] = ".";
char current_project[MAX_LINE] = "man-pal";

/* State for Display */
int cursor_x = 5, cursor_y = 5, cursor_z = 0;
char last_key_str[32] = "None";

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

void trigger_render() {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/render_map.+x > /dev/null 2>&1", project_root) != -1) {
        system(cmd); free(cmd);
    }
}

void write_pal_state() {
    char *path = NULL;
    // Manager State
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "project_id=%s\nactive_target_id=selector\ncurrent_z=%d\nlast_key=%s\n", current_project, cursor_z, last_key_str); fclose(f); }
        free(path);
    }
    // App State
    if (asprintf(&path, "%s/pieces/apps/player_app/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w"); if (!f) { free(path); return; }
        fprintf(f, "project_id=%s\ngui_focus=1\nlast_key=%s\nselector_pos=(%d,%d,%d)\n", current_project, last_key_str, cursor_x, cursor_y, cursor_z);
        fprintf(f, "editor_response=[RESP]: PAL BRAIN ONLINE\n");
        char stats[MAX_LINE]; snprintf(stats, sizeof(stats), "[POS]: (%d,%d,%d) | [KEY]: %-10s", cursor_x, cursor_y, cursor_z, last_key_str);
        fprintf(f, "editor_status_2=%-57s\n", stats);
        fclose(f); free(path);
    }
}

int is_active_layout() {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    FILE *f = fopen(path, "r"); if (!f) { free(path); return 0; }
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) { fclose(f); int res = (strstr(line, "man-pal.chtpm") != NULL); free(path); return res; }
    fclose(f); free(path); return 0;
}

void update_key_string(int key) {
    if (key >= 32 && key <= 126) snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    else if (key == 10 || key == 13) strcpy(last_key_str, "ENTER");
    else if (key == 27) strcpy(last_key_str, "ESC");
    else if (key == 1000) strcpy(last_key_str, "LEFT");
    else if (key == 1001) strcpy(last_key_str, "RIGHT");
    else if (key == 1002) strcpy(last_key_str, "UP");
    else if (key == 1003) strcpy(last_key_str, "DOWN");
}

void trigger_pal_brain() {
    /* TPM Track 2 Optimization: Trigger Prisc VM to consume the batch */
    char *cmd = NULL;
    // We must ensure PRISC_PROJECT_ROOT is absolute if possible
    char abs_root[MAX_PATH];
    if (REALPATH(project_root, abs_root) == NULL) strcpy(abs_root, project_root);

    if (asprintf(&cmd, "PRISC_PROJECT_ID=%s PRISC_PROJECT_ROOT=%s %s/pieces/system/prisc/prisc+x %s/projects/man-pal/scripts/move.asm > /dev/null 2>&1",
                 current_project, abs_root, abs_root, abs_root) != -1) {
        system(cmd); free(cmd);
    }

    /* Post-Brain Sync: Read the resulting state for UI display */
    char *s_path = NULL;
    if (asprintf(&s_path, "%s/projects/%s/pieces/selector/state.txt", project_root, current_project) != -1) {
        FILE *ssf = fopen(s_path, "r");
        if (ssf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), ssf)) {
                char *eq = strchr(line, '=');
                if (eq) { *eq = '\0'; char *k = trim_str(line), *v = trim_str(eq + 1); 
                    if (strcmp(k, "pos_x") == 0) cursor_x = atoi(v); 
                    else if (strcmp(k, "pos_y") == 0) cursor_y = atoi(v); 
                }
            }
            fclose(ssf);
        }
        free(s_path);
    }
}

int main() {
    resolve_paths(); write_pal_state(); trigger_render();
    long last_pos = 0; struct stat st; char *hist_path = NULL;
    asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root);
    while (1) {
        if (!is_active_layout()) {
#ifdef _WIN32
            Sleep(200);
#else
            usleep(200000);
#endif
            continue;
        }
        if (stat(hist_path, &st) == 0 && st.st_size > 0) {
            FILE *hf = fopen(hist_path, "r+");
            if (hf) {
                int key, processed = 0;
                while (fscanf(hf, "%d", &key) == 1) { 
                    update_key_string(key); 
                    processed = 1; 
                }
                if (processed) { 
                    trigger_pal_brain(); // CPU Efficient: One VM per batch
                    trigger_render(); 
                    write_pal_state(); 
                    // Explicitly truncate to consume the batch we just processed
                    ftruncate(fileno(hf), 0);
                }
                fclose(hf);
            }
        }
#ifdef _WIN32
        Sleep(16);
#else
        usleep(16667);
#endif
    }
    return 0;
}
