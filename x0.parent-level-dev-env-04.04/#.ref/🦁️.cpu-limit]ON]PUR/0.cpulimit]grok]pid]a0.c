#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int pid = -1;
    int limit = -1;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "p:l:")) != -1) {
        switch (opt) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 'l':
                limit = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -p <pid> -l <limit>\n", argv[0]);
                exit(1);
        }
    }

    // Validate arguments
    if (pid <= 0 || limit < 0 || limit > 100) {
        fprintf(stderr, "Invalid PID or limit (0-100)\n");
        exit(1);
    }

    // Convert limit to a fraction of CPU time (e.g., 10% = 0.1)
    double cpu_fraction = limit / 100.0;
    long sleep_time = 100000; // Base sleep time in microseconds (0.1s)
    long active_time = (long)(sleep_time * cpu_fraction); // Time process runs
    long pause_time = sleep_time - active_time; // Time process is paused

    while (1) {
        // Allow process to run
        if (kill(pid, SIGCONT) != 0) {
            perror("Error sending SIGCONT");
            exit(1);
        }
        usleep(active_time); // Let process run for active_time

        // Pause process
        if (kill(pid, SIGSTOP) != 0) {
            perror("Error sending SIGSTOP");
            exit(1);
        }
        usleep(pause_time); // Keep process paused for pause_time
    }

    return 0;
}
