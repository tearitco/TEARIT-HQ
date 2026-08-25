#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>

int find_pid_by_name(const char *exe_name) {
    FILE *fp;
    char path[256];
    char comm[256];
    int pid = -1;

    // Scan /proc for PIDs
    for (int i = 1; i < 99999; i++) {
        snprintf(path, sizeof(path), "/proc/%d/comm", i);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(comm, sizeof(comm), fp)) {
                // Remove newline
                comm[strcspn(comm, "\n")] = 0;
                // Compare process name with exe_name
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

int main(int argc, char *argv[]) {
    int pid = -1;
    int limit = -1;
    char *exe_name = NULL;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "p:l:e:")) != -1) {
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
            default:
                fprintf(stderr, "Usage: %s [-p <pid> | -e <exe_name>] -l <limit>\n", argv[0]);
                exit(1);
        }
    }

    // Validate arguments
    if ((pid == -1 && exe_name == NULL) || (pid != -1 && exe_name != NULL)) {
        fprintf(stderr, "Specify either -p <pid> or -e <exe_name>, not both\n");
        exit(1);
    }
    if (limit < 0 || limit > 100) {
        fprintf(stderr, "Invalid limit (0-100)\n");
        exit(1);
    }

    // If exe_name is provided, find the PID
    if (exe_name) {
        pid = find_pid_by_name(exe_name);
        if (pid == -1) {
            fprintf(stderr, "No process found with name '%s'\n", exe_name);
            exit(1);
        }
        printf("Found process '%s' with PID %d\n", exe_name, pid);
    }

    // Validate PID
    if (pid <= 0) {
        fprintf(stderr, "Invalid PID\n");
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
