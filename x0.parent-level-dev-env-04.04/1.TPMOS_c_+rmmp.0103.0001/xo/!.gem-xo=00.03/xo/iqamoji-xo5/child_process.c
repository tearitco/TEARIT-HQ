// child_process.c for Exo-Bot v5
// STANDALONE MUSCLE: Compiles to its own binary.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t should_exit = 0;

void handle_sigterm(int sig) {
    (void)sig;
    should_exit = 1;
}

int main(int argc, char* argv[]) {
    const char* name = (argc > 1) ? argv[1] : "Unnamed_Muscle";
    pid_t pid = getpid();

    printf("[%s] Muscle binary started (PID %d)\n", name, (int)pid);

    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, SIG_IGN); // Orchestrator handles main INT

    int count = 0;
    while (!should_exit) {
        printf("[%s] Pulse... %d\n", name, ++count);
        sleep(2);
        if (count >= 10) break; // Auto-exit after 20s
    }

    printf("[%s] Muscle binary shutting down gracefully.\n", name);
    return 0;
}
