// orchestrator.c for Exo-Bot v5
// CONCURRENT MODE: Runs TPMOS in background, launches muscle immediately, monitors until SIGINT.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/file.h>
#include <errno.h>

#define PROC_LIST "proc_list.txt"
static char g_project_root[1024] = ".";
static volatile sig_atomic_t should_exit = 0;

void handle_sigint(int sig) { (void)sig; should_exit = 1; }

static void log_pid(pid_t pid, const char* name) {
    FILE* f = fopen(PROC_LIST, "a");
    if (!f) return;
    flock(fileno(f), LOCK_EX);
    fprintf(f, "%d %s\n", (int)pid, name);
    fflush(f);
    flock(fileno(f), LOCK_UN);
    fclose(f);
}

// 🔥 ASYNC LAUNCHER: Starts a process and returns immediately (no waitpid)
static pid_t launch_background(const char* script, const char* arg) {
    fprintf(stderr, "[Orchestrator] Launching background process: %s %s\n", script, arg);
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(g_project_root) != 0) { perror("chdir setup failed"); exit(1); }
        execl("/bin/bash", "bash", script, arg, (char*)NULL);
        perror("execl failed"); exit(1);
    } else if (pid > 0) {
        return pid;
    }
    return -1;
}

static void cleanup_children(pid_t tpmos_pid, pid_t muscle_pid) {
    fprintf(stderr, "[Orchestrator] Sending SIGTERM to tracked processes...\n");
    if (tpmos_pid > 0) kill(tpmos_pid, SIGTERM);
    if (muscle_pid > 0) kill(muscle_pid, SIGTERM);
}

int main(int argc, char *argv[]) {
    // 1️⃣ Resolve root
    if (argc > 1) {
        strncpy(g_project_root, argv[1], sizeof(g_project_root)-1);
        g_project_root[sizeof(g_project_root)-1] = '\0';
    } else {
        getcwd(g_project_root, sizeof(g_project_root));
    }
    fprintf(stderr, "[Orchestrator] STARTUP. Root: %s\n", g_project_root);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // Clear proc list
    FILE* f = fopen(PROC_LIST, "w"); if(f) fclose(f);

    // 2️⃣ Launch TPMOS in BACKGROUND (non-blocking)
    pid_t tpmos_pid = launch_background("button.sh", "r");
    if (tpmos_pid < 0) { fprintf(stderr, "[Orchestrator] FATAL: Failed to launch TPMOS\n"); return 1; }
    log_pid(tpmos_pid, "TPMOS_CLI");
    fprintf(stderr, "[Orchestrator] TPMOS launched (PID %d)\n", (int)tpmos_pid);

    // 3️⃣ Launch Muscle IMMEDIATELY (concurrent)
    pid_t muscle_pid = fork();
    if (muscle_pid == 0) {
        execl("./bot5_keyboard_muscle", "bot5_keyboard_muscle", g_project_root, (char*)NULL);
        perror("[Orchestrator] Muscle execl failed");
        exit(1);
    } else if (muscle_pid > 0) {
        log_pid(muscle_pid, "Keyboard_Hand");
        fprintf(stderr, "[Orchestrator] Muscle launched (PID %d)\n", (int)muscle_pid);
    }

    // 4️⃣ MONITORING LOOP (STAYS ALIVE)
    fprintf(stderr, "[Orchestrator] Entering concurrent monitoring (Ctrl+C to exit)...\n");
    while (!should_exit) {
        int status;
        // Reap any exited children without blocking
        while (waitpid(-1, &status, WNOHANG) > 0);
        sleep(1);
    }

    cleanup_children(tpmos_pid, muscle_pid);
    fprintf(stderr, "[Orchestrator] Clean shutdown complete.\n");
    return 0;
}
