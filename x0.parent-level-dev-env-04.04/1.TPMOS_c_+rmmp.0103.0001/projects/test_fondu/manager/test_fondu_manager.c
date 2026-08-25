/*
 * test_fondu_manager.c - Simple counter test manager
 * Polls input history, calls ops based on key presses
 * CPU-safe pattern: signal handling, setpgid, fork/exec/waitpid
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define HISTORY_FILE "pieces/apps/player_app/history.txt"
#define STATE_FILE "projects/test_fondu/pieces/state.txt"
#define LAYOUT_FILE "pieces/display/current_layout.txt"
#define PROJECT_PATH "projects/test_fondu"
#define OPS_PATH "projects/test_fondu/ops/+x"

static volatile sig_atomic_t g_shutdown = 0;

void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

int setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    return 0;
}

int is_active_layout(void) {
    FILE *fp = fopen(LAYOUT_FILE, "r");
    if (!fp) return 0;
    
    char line[512];
    int is_active = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "test_fondu.chtpm")) {
            is_active = 1;
            break;
        }
    }
    fclose(fp);
    return is_active;
}

int read_history(int *key) {
    FILE *fp = fopen(HISTORY_FILE, "r");
    if (!fp) return 0;
    
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d", key) == 1) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    
    /* Clear history after reading */
    if (found) {
        fp = fopen(HISTORY_FILE, "w");
        if (fp) fclose(fp);
    }
    
    return found;
}

int call_op(const char *op_name) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/%s.+x %s", OPS_PATH, op_name, PROJECT_PATH);
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        setpgid(0, 0);
        execl(cmd, cmd, NULL);
        _exit(1);
    }
    
    /* Parent waits for child */
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return -1;
}

int read_counter(int *counter) {
    FILE *fp = fopen(STATE_FILE, "r");
    if (!fp) return -1;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "counter=", 8) == 0) {
            *counter = atoi(line + 8);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

int main(void) {
    /* Setup */
    setpgid(0, 0);
    setup_signal_handlers();
    
    printf("test_fondu_manager started\n");
    
    while (!g_shutdown) {
        /* Sleep if not active layout */
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        
        int key;
        if (read_history(&key)) {
            switch (key) {
                case 49: /* '1' - increment */
                    call_op("increment_counter");
                    break;
                case 50: /* '2' - decrement */
                    call_op("decrement_counter");
                    break;
                case 51: /* '3' - reset */
                    call_op("reset_counter");
                    break;
                case 52: /* '4' - status */
                    call_op("get_status");
                    break;
                case 27: /* ESC - exit */
                    g_shutdown = 1;
                    break;
            }
        }
        
        usleep(50000); /* 50ms poll rate */
    }
    
    printf("test_fondu_manager shutting down\n");
    return 0;
}
