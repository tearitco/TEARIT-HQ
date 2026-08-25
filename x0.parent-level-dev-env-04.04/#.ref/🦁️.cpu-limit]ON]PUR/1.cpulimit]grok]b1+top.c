#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define MAX_PIDS 1024
#define REFRESH_INTERVAL 100 // Refresh PID list every 100 cycles (~10s)

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

int find_pid_by_name(const char *exe_name) {
    FILE *fp;
    char path[256];
    char comm[256];
    int pid = -1;

    for (int i = 1; i < 99999; i++) {
        snprintf(path, sizeof(path), "/proc/%d/comm", i);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(comm, sizeof(comm), fp)) {
                comm[strcspn(comm, "\n")] = 0;
                if (strcmp(comm, exe_name) == 0) {
                    pid = i;
                    fclose(fp);
                    break;
                }
            }
            fclose(fp);
        }
    }
    return pid;
}

int get_pids_from_top(int *pids, int max_pids) {
    FILE *fp;
    char line[256];
    int pid_count = 0;
    int self_pid = getpid();

    fp = popen("top -b -n 1 | tail -n +8", "r");
    if (!fp) {
        perror("Error running top");
        return 0;
    }

    while (fgets(line, sizeof(line), fp) && pid_count < max_pids) {
        int pid;
        if (sscanf(line, "%d", &pid) == 1) {
            // Skip own PID, PID 1 (init), and low PIDs (< 100) to avoid system processes
            if (pid != self_pid && pid > 100 && pid != 1) {
                pids[pid_count++] = pid;
            }
        }
    }

    pclose(fp);
    return pid_count;
}

int main(int argc, char *argv[]) {
    int pid = -1;
    int limit = -1;
    char *exe_name = NULL;
    int use_top = 0;
    int opt;

    // Set up SIGINT handler
    signal(SIGINT, handle_sigint);

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "p:l:e:t")) != -1) {
        switch (opt) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 'l':
                limit = atoi(optarg);
                break;
            case 'e':
                exe_name = optarg;
                break;
            case 't':
                use_top = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p <pid> | -e <exe_name> | -t] -l <limit>\n");
                exit(1);
        }
    }

    // Validate arguments
    if ((pid != -1 && (exe_name != NULL || use_top)) ||
        (exe_name != NULL && (pid != -1 || use_top)) ||
        (use_top && (pid != -1 || exe_name != NULL))) {
        fprintf(stderr, "Specify exactly one of -p, -e, or -t\n");
        exit(1);
    }
    if (limit < 0 || limit > 100) {
        fprintf(stderr, "Invalid limit (0-100)\n./toppy.+x -t -l 77\n");
        exit(1);
    }

    int pids[MAX_PIDS];
    int pid_count = 0;
    int cycle_count = 0;

    // Handle -t flag: get PIDs from top
    if (use_top) {
        pid_count = get_pids_from_top(pids, MAX_PIDS);
        if (pid_count == 0) {
            fprintf(stderr, "No processes found in top output\n");
            exit(1);
        }
        printf("Found %d processes in top\n", pid_count);
    }
    // Handle -e flag: find PID by executable name
    else if (exe_name) {
        pid = find_pid_by_name(exe_name);
        if (pid == -1) {
            fprintf(stderr, "No process found with name '%s'\n", exe_name);
            exit(1);
        }
        pids[0] = pid;
        pid_count = 1;
        printf("Found process '%s' with PID %d\n", exe_name, pid);
    }
    // Handle -p flag: use provided PID
    else if (pid > 0) {
        pids[0] = pid;
        pid_count = 1;
    }
    else {
        fprintf(stderr, "Must specify -p, -e, or -t\n");
        exit(1);
    }

    // Convert limit to CPU fraction
    double cpu_fraction = limit / 100.0;
    long sleep_time = 200000; // Increased to 0.2s to reduce load
    long active_time = (long)(sleep_time * cpu_fraction);
    long pause_time = sleep_time - active_time;

    while (keep_running) {
        // Allow processes to run
        for (int i = 0; i < pid_count; i++) {
            if (kill(pids[i], SIGCONT) != 0) {
                // Skip if permission denied or process doesn't exist
                if (errno == EPERM || errno == ESRCH) {
                    continue;
                }
                fprintf(stderr, "Error sending SIGCONT to PID %d: %s\n", pids[i], strerror(errno));
            }
        }
        usleep(active_time);

        // Pause processes
        for (int i = 0; i < pid_count; i++) {
            if (kill(pids[i], SIGSTOP) != 0) {
                if (errno == EPERM || errno == ESRCH) {
                    continue;
                }
                fprintf(stderr, "Error sending SIGSTOP to PID %d: %s\n", pids[i], strerror(errno));
            }
        }
        usleep(pause_time);

        // Refresh PID list periodically if using -t
        if (use_top && ++cycle_count >= REFRESH_INTERVAL) {
            pid_count = get_pids_from_top(pids, MAX_PIDS);
            if (pid_count == 0) {
                fprintf(stderr, "No processes found in top output\n");
                keep_running = 0;
            } else {
                printf("Refreshed: Found %d processes in top\n", pid_count);
            }
            cycle_count = 0;
        }
    }

    // Resume all processes before exiting
    for (int i = 0; i < pid_count; i++) {
        if (kill(pids[i], SIGCONT) != 0) {
            if (errno != EPERM && errno != ESRCH) {
                fprintf(stderr, "Error sending SIGCONT to PID %d on exit: %s\n", pids[i], strerror(errno));
            }
        }
    }

    printf("Exiting gracefully\n");
    return 0;
}
