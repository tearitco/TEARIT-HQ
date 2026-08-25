/*
 * bots_manager.c - BOTS Project Manager
 * 
 * This template implements basic CPU-safe patterns for a TPMOS module.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

/* ============================================================================
 * CONFIGURATION - Modify these for your module
 * ============================================================================ */
#define MODULE_NAME "bots_manager"
#define MAX_PATH 4096
#define MAX_LINE 1024

/* Module-specific state variables */
static char project_root[MAX_PATH] = ".";
static char current_project[MAX_LINE] = "BOTS";
static char active_target_id[64] = "selector";
static char last_key_str[32] = "None";
static int last_key_pressed = 0;

/* ============================================================================
 * CPU-SAFE: Signal Handling & Process Management
 * ============================================================================ */
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int run_command(const char* cmd) {
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
    return -1;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

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

static void resolve_paths(void) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

/* ============================================================================
 * MODULE-SPECIFIC FUNCTIONS - Customize these for your module
 * ============================================================================ */

static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    
    char line[MAX_LINE];
    int result = 0;
    
    if (fgets(line, sizeof(line), f)) {
        result = (strstr(line, MODULE_NAME ".chtpm") != NULL ||
                  strstr(line, "game.chtpm") != NULL);
    }
    
    fclose(f);
    free(path);
    return result;
}

static void process_key(int key) {
    last_key_pressed = key;
    if (key >= 32 && key <= 126) {
        snprintf(last_key_str, sizeof(last_key_str), "%c", (char)key);
    } else if (key == 10 || key == 13) {
        strcpy(last_key_str, "ENTER");
    } else if (key == 27) {
        strcpy(last_key_str, "ESC");
    } else if (key == 1000) {
        strcpy(last_key_str, "LEFT");
    } else if (key == 1001) {
        strcpy(last_key_str, "RIGHT");
    } else if (key == 1002) {
        strcpy(last_key_str, "UP");
    } else if (key == 1003) {
        strcpy(last_key_str, "DOWN");
    } else {
        snprintf(last_key_str, sizeof(last_key_str), "Key:%d", key);
    }
    
    /* TODO: Add game-specific key handling here */
    if (key == '1') { /* Example: Placeholder for a game action */ }
}

static void update_state(void) {
    char *path = NULL;
    
    /* Update manager state */
    if (asprintf(&path, "%s/projects/%s/manager/state.txt", project_root, current_project) == -1) {
        return;
    }
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=%s
", current_project);
        fprintf(f, "active_target_id=%s
", active_target_id);
        fprintf(f, "last_key=%s
", last_key_str);
        fprintf(f, "current_z=0
"); /* Example default */
        fprintf(f, "current_map=map_01_z0.txt
"); /* Example default */
        fclose(f);
    }
    free(path);
    
    /* TODO: Add game-specific state updates here */
}

static void trigger_render(void) {
    char *cmd = NULL;
    if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/render_map.+x > /dev/null 2>&1", project_root) != -1) {
        run_command(cmd);
        free(cmd);
    }
}

/* ============================================================================
 * MAIN LOOP - CPU-Safe Pattern
 * ============================================================================ */
int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    
    resolve_paths();
    update_state();
    trigger_render();
    
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/projects/%s/history.txt", project_root, current_project) == -1) {
        return 1;
    }
    
    long last_pos = 0;
    struct stat st;
    
    while (!g_shutdown) {
        if (!is_active_layout()) {
            usleep(100000); 
            continue;
        }
        
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        process_key(key);
                        update_state();
                        trigger_render();
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        
        usleep(16667); /* ~60 FPS */
    }
    
    free(hist_path);
    return 0;
}
