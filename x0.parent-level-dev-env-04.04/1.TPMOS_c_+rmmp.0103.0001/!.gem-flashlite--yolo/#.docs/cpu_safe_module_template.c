/*
 * cpu_safe_module_template.c - CPU-Safe PMO Module Template
 * 
 * This template implements all CPU-safe patterns validated by legacy_fuzzpet:
 * - Signal handling for graceful shutdown
 * - Process group management for clean cleanup
 * - fork()/exec()/waitpid() pattern instead of system()
 * - Focus-aware throttling (is_active_layout)
 * - stat()-first polling for history files
 * - Bounded sleep intervals (no tight loops)
 *
 * USAGE: Copy this template and modify the following:
 *   1. Change MODULE_NAME and module-specific variables
 *   2. Implement process_key() with your input handling
 *   3. Implement update_state() with your state logic
 *   4. Implement is_active_layout() for your layout(s)
 *   5. Add your custom run_command() calls where needed
 *
 * COMPILE: gcc -o +x/MODULE_NAME.+x MODULE_NAME.c -pthread
 *
 * VALIDATION CHECKLIST:
 *   [ ] Signal handler registered in main()
 *   [ ] setpgid(0, 0) called in main()
 *   [ ] Main loop checks g_shutdown flag
 *   [ ] All external commands use run_command()
 *   [ ] Focus throttling implemented
 *   [ ] History polling uses stat() first
 *   [ ] Sleep interval >= 16ms (60 FPS max)
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
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

/* ============================================================================
 * CONFIGURATION - Modify these for your module
 * ============================================================================ */
#define MODULE_NAME "template"
#define MAX_PATH 4096
#define MAX_LINE 1024

/* Module-specific state variables */
static char project_root[MAX_PATH] = ".";
static char current_project[MAX_LINE] = "template";
static char active_target_id[64] = "selector";
static char last_key_str[32] = "None";

/* ============================================================================
 * CPU-SAFE: Signal Handling & Process Management
 * ============================================================================ */
static volatile sig_atomic_t g_shutdown = 0;

/**
 * Signal handler for SIGINT (Ctrl+C) and SIGTERM
 * Sets shutdown flag - main loop will exit gracefully
 */
static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/**
 * CPU-Safe command execution using fork()/exec()/waitpid()
 * 
 * @param cmd Command string to execute
 * @return Exit status of command, or -1 on error
 * 
 * WHY NOT system()?
 * - system() spawns a subshell that can become orphaned on Ctrl+C
 * - fork()/waitpid() gives us full control over child lifecycle
 * - Child processes are always reaped, no zombies
 */
static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process: redirect output and execute */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);  /* execl failed */
    } else if (pid > 0) {
        /* Parent process: wait for child to complete */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;  /* fork failed */
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Trim whitespace from string (in-place)
 */
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

/**
 * Resolve project root from location_kvp
 */
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

/**
 * Check if this module's layout is currently active
 * 
 * CRITICAL FOR CPU SAFETY: Returns 0 when layout is not active,
 * causing main loop to sleep longer (100ms vs 16ms)
 * 
 * @return 1 if active, 0 if not
 */
static int is_active_layout(void) {
    char *path = NULL;
    if (asprintf(&path, "%s/pieces/display/current_layout.txt", project_root) == -1) return 0;
    
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0; }
    
    char line[MAX_LINE];
    int result = 0;
    
    if (fgets(line, sizeof(line), f)) {
        /* MODIFY: Add your layout name here */
        result = (strstr(line, MODULE_NAME ".chtpm") != NULL ||
                  strstr(line, "template.chtpm") != NULL);
    }
    
    fclose(f);
    free(path);
    return result;
}

/**
 * Process a single key input
 * 
 * @param key Key code from history buffer
 */
static void process_key(int key) {
    /* Convert key to string for state */
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
    }
    
    /* Example: Handle quit key (9 or ESC) */
    if (key == '9' || key == 27) {
        /* Return to selector / quit current mode */
        strcpy(active_target_id, "selector");
    }
    
    /* Example: Handle action key with external command */
    if (key == 10 || key == 13) {
        char *cmd = NULL;
        if (asprintf(&cmd, "%s/pieces/apps/playrm/ops/+x/some_op.+x arg1 arg2 > /dev/null 2>&1", project_root) != -1) {
            run_command(cmd);  /* CPU-SAFE: Uses fork/exec/waitpid */
            free(cmd);
        }
    }
    
    /* TODO: Add your module-specific key handling here */
}

/**
 * Update module state files
 */
static void update_state(void) {
    char *path = NULL;
    
    /* Update manager state */
    if (asprintf(&path, "%s/pieces/apps/player_app/manager/state.txt", project_root) != -1) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "project_id=%s\nactive_target_id=%s\nlast_key=%s\n", 
                    current_project, active_target_id, last_key_str);
            fclose(f);
        }
        free(path);
    }
    
    /* TODO: Add your module-specific state updates here */
}

/**
 * Trigger render update (optional)
 */
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
    /* CPU-SAFE STEP 1: Register signal handlers */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    /* CPU-SAFE STEP 2: Set process group for clean cleanup */
    setpgid(0, 0);
    
    /* Initialize module */
    resolve_paths();
    update_state();
    trigger_render();
    
    /* History file polling setup */
    char *hist_path = NULL;
    if (asprintf(&hist_path, "%s/pieces/apps/player_app/history.txt", project_root) == -1) {
        return 1;
    }
    
    long last_pos = 0;
    struct stat st;
    
    /* CPU-SAFE STEP 3: Main loop with shutdown flag check */
    while (!g_shutdown) {
        /* CPU-SAFE STEP 4: Focus-aware throttling */
        if (!is_active_layout()) {
            usleep(100000);  /* 100ms when not in focus (10 FPS) */
            continue;
        }
        
        /* CPU-SAFE STEP 5: stat()-first polling */
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                /* New input available - process incrementally */
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
                /* File was truncated - reset position */
                last_pos = 0;
            }
        }
        
        /* CPU-SAFE STEP 6: Bounded sleep (60 FPS max) */
        usleep(16667);
    }
    
    /* CPU-SAFE STEP 7: Cleanup */
    free(hist_path);
    
    return 0;
}

/* ============================================================================
 * END OF TEMPLATE
 * ============================================================================
 * 
 * REMINDER: Before committing your module:
 * 
 * 1. Update the header comment with your module name and purpose
 * 2. Customize MODULE_NAME and state variables
 * 3. Implement is_active_layout() with your layout names
 * 4. Implement process_key() with your input handling
 * 5. Replace all system() calls with run_command()
 * 6. Test Ctrl+C to verify clean shutdown
 * 7. Run pmo_cpu_health.sh to verify no orphaned processes
 * 
 * REFERENCES:
 * - pieces/apps/editor/plugins/editor_module.c (CPU-safe example)
 * - projects/fuzz_legacy/manager/fuzz_legacy_manager.c (Original reference)
 * - #.docs/!.cpu_health.txt (CPU health documentation)
 * - #.docs/!.cpu_fix.txt (Crash analysis and fixes)
 * ============================================================================
 */
