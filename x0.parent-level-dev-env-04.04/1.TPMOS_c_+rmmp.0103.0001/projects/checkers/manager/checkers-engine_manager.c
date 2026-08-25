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

#define MODULE_NAME "checkers-engine"
#define MAX_PATH 4096
#define MAX_LINE 1024

typedef enum {
    STATE_SETUP,
    STATE_INIT,
    STATE_PLAY,
    STATE_AI_MOVE,
    STATE_GAME_OVER
} FSMState;

static char project_root[MAX_PATH] = ".";
static FSMState current_state = STATE_SETUP;
static char player_turn[16] = "red";
static char p1_type[16] = "human";
static char p2_type[16] = "ai";

static volatile sig_atomic_t g_shutdown = 0;
static void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

static int run_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static void resolve_paths(void) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "project_root") == 0) {
                    char *v = eq + 1;
                    v[strcspn(v, "\n\r")] = 0;
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

static void update_state(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=checkers\n");
        fprintf(f, "turn=%s\n", player_turn);
        fprintf(f, "fsm_state=%d\n", (int)current_state);
        fprintf(f, "p1_type=%s\n", p1_type);
        fprintf(f, "p2_type=%s\n", p2_type);
        fclose(f);
    }
}

static void process_key(int key) {
    char cmd[MAX_PATH];
    if (current_state == STATE_SETUP) {
        if (key == '1') strcpy(p2_type, "ai");
        else if (key == '2') strcpy(p2_type, "ai"); // AI vs AI logic
        else if (key == '3') strcpy(p2_type, "p2p");
        
        if (key == 10 || key == 13) { // ENTER
            current_state = STATE_INIT;
            snprintf(cmd, sizeof(cmd), "%s/projects/checkers/ops/+x/checkers_init.+x > /dev/null 2>&1", project_root);
            run_command(cmd);
            current_state = STATE_PLAY;
        }
    }
    /* TODO: Add Play state interaction */
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();
    update_state();

    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(hist_path, "r");
            if (hf) {
                fseek(hf, last_pos, SEEK_SET);
                int key;
                while (fscanf(hf, "%d", &key) == 1) process_key(key);
                last_pos = ftell(hf);
                fclose(hf);
                update_state();
            }
        }
        usleep(16667);
    }
    return 0;
}
