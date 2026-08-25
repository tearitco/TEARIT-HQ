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

#define MODULE_NAME "exo-bot"
#define MAX_PATH 4096
#define MAX_LINE 1024

static char project_root[MAX_PATH] = ".";
static char last_action[MAX_LINE] = "Initialized";
static char bot_response[MAX_LINE] = "Awaiting orders.";

static volatile sig_atomic_t g_shutdown = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

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
                    while(isspace(*v)) v++;
                    char *end = v + strlen(v) - 1;
                    while(end > v && isspace(*end)) end--;
                    end[1] = '\0';
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    }
}

static void process_key(int key) {
    char cmd[MAX_PATH];
    if (key == '1') {
        /* Scan Local Piece */
        snprintf(last_action, MAX_LINE, "Scanned Filesystem");
        snprintf(bot_response, MAX_LINE, "Piece contains %s/piece/state.txt", project_root);
        snprintf(cmd, sizeof(cmd), "./bin/fs_ls.+x . > piece/history.txt");
        run_command(cmd);
    }
    else if (key == '2') {
        /* Generate Status Report */
        snprintf(last_action, MAX_LINE, "Generated Report");
        snprintf(bot_response, MAX_LINE, "Report written to piece/report.txt");
        snprintf(cmd, sizeof(cmd), "./bin/fs_write.+x piece/report.txt W \"EXO-BOT STATUS: ACTIVE\nTIME: %ld\" > /dev/null", (long)time(NULL));
        run_command(cmd);
    }
    else if (key == '3') {
        /* Run Logic Loop */
        snprintf(last_action, MAX_LINE, "Running FSM Logic");
        snprintf(bot_response, MAX_LINE, "Logic executed via prisc+x");
        snprintf(cmd, sizeof(cmd), "./bin/prisc+x piece/fsm/main.asm > /dev/null");
        run_command(cmd);
    }
}

static void update_state(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "project_id=exo-bot\n");
        fprintf(f, "last_action=%s\n", last_action);
        fprintf(f, "bot_response=%s\n", bot_response);
        fclose(f);
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    setpgid(0, 0);
    resolve_paths();
    
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/keyboard/history.txt", project_root);
    
    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    update_state();

    while (!g_shutdown) {
        if (stat(hist_path, &st) == 0 && st.st_size > last_pos) {
            FILE *hf = fopen(hist_path, "r");
            if (hf) {
                fseek(hf, last_pos, SEEK_SET);
                int key;
                while (fscanf(hf, "%d", &key) == 1) {
                    process_key(key);
                    update_state();
                }
                last_pos = ftell(hf);
                fclose(hf);
            }
        }
        usleep(16667);
    }
    return 0;
}
