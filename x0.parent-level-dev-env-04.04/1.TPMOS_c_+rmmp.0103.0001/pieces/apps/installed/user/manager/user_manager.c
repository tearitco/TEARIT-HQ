/*
 * user_manager.c - User Profile Manager
 * Polls input history, calls user ops based on key presses
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
#define LAYOUT_FILE "pieces/display/current_layout.txt"
#define PROJECT_PATH "projects/user"
#define OPS_PATH "projects/user/ops/+x"

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
        if (strstr(line, "user.chtpm")) {
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

int call_op(const char *op_name, const char *arg) {
    char cmd[1024];
    if (arg) {
        snprintf(cmd, sizeof(cmd), "%s/%s.+x %s %s", OPS_PATH, op_name, PROJECT_PATH, arg);
    } else {
        snprintf(cmd, sizeof(cmd), "%s/%s.+x %s", OPS_PATH, op_name, PROJECT_PATH);
    }
    
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

int main(void) {
    /* Setup */
    setpgid(0, 0);
    setup_signal_handlers();
    
    printf("user_manager started\n");
    
    while (!g_shutdown) {
        /* Sleep if not active layout */
        if (!is_active_layout()) {
            usleep(100000);
            continue;
        }
        
        int key;
        if (read_history(&key)) {
            switch (key) {
                case 49: /* '1' - create profile */
                    printf("Enter username: ");
                    fflush(stdout);
                    /* In real implementation, would use CLI input */
                    call_op("create_profile", "testuser");
                    break;
                case 50: /* '2' - auth user */
                    call_op("auth_user", "testuser");
                    break;
                case 51: /* '3' - get session */
                    call_op("get_session", "testuser");
                    break;
                case 27: /* ESC - exit */
                    g_shutdown = 1;
                    break;
            }
        }
        
        usleep(50000); /* 50ms poll rate */
    }
    
    printf("user_manager shutting down\n");
    return 0;
}
