#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define MAX_PIDS 1024
#define REFRESH_INTERVAL 100 // Refresh PID list every 100 cycles (~10s)
#define CPU_THRESHOLD 1.0 // Report processes using more CPU than limit
#define REPORT_BACKOFF 180
//#define REPORT_BACKOFF 300 // Seconds to wait before reporting same PID again (5 min)

volatile sig_atomic_t keep_running = 1;

// Structure to track reported PIDs and their last report time
typedef struct {
    int pid;
    time_t last_reported;
} ReportedPid;

ReportedPid reported_pids[MAX_PIDS];
int reported_count = 0;

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
            if (pid != self_pid && pid > 100 && pid != 1) {
                pids[pid_count++] = pid;
            }
        }
    }

    pclose(fp);
    return pid_count;
}

// Get process name from /proc/<pid>/comm
char *get_process_name(int pid) {
    static char comm[256];
    char path[256];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(comm, sizeof(comm), fp)) {
            comm[strcspn(comm, "\n")] = 0;
            fclose(fp);
            return comm;
        }
        fclose(fp);
    }
    return "unknown";
}

// Check if we can report this PID (backoff check)
int can_report_pid(int pid) {
    time_t now = time(NULL);
    for (int i = 0; i < reported_count; i++) {
        if (reported_pids[i].pid == pid) {
            if (now - reported_pids[i].last_reported < REPORT_BACKOFF) {
                return 0; // Too soon to report again
            } else {
                reported_pids[i].last_reported = now; // Update report time
                return 1;
            }
        }
    }
    // New PID, add to reported list
    if (reported_count < MAX_PIDS) {
        reported_pids[reported_count].pid = pid;
        reported_pids[reported_count].last_reported = now;
        reported_count++;
    }
    return 1;
}

// Monitor CPU usage for a PID
float get_cpu_usage(int pid) {
    char path[256];
    FILE *fp;
    unsigned long utime, stime;
    long hz = sysconf(_SC_CLK_TCK);
    static unsigned long prev_total = 0;
    static time_t prev_time = 0;
    time_t now = time(NULL);

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return 0.0;
    }

    char line[512];
    if (fgets(line, sizeof(line), fp)) {
        char *token = strtok(line, " ");
        for (int i = 1; i < 14; i++) token = strtok(NULL, " "); // Skip to utime
        utime = atol(token);
        token = strtok(NULL, " "); // stime
        stime = atol(token);
    }
    fclose(fp);

    unsigned long total_time = utime + stime;
    float cpu_usage = 0.0;
    if (prev_time != 0 && now > prev_time) {
        cpu_usage = 100.0 * (total_time - prev_total) / ((now - prev_time) * hz);
    }
    prev_total = total_time;
    prev_time = now;

    return cpu_usage;
}

int main(int argc, char *argv[]) {
    int pid = -1;
    int limit = -1;
    char *exe_name = NULL;
    int use_top = 0;
    int opt;

    signal(SIGINT, handle_sigint);

    while ((opt = getopt(argc, argv, "p:l:e:t")) != -1) {
        switch (opt) {
            case 'p': pid = atoi(optarg); break;
            case 'l': limit = atoi(optarg); break;
            case 'e': exe_name = optarg; break;
            case 't': use_top = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-p <pid> | -e <exe_name> | -t] -l <limit>\n");
                exit(1);
        }
    }

    if ((pid != -1 && (exe_name != NULL || use_top)) ||
        (exe_name != NULL && (pid != -1 || use_top)) ||
        (use_top && (pid != -1 || exe_name != NULL))) {
        fprintf(stderr, "Specify exactly one of -p, -e, or -t\n");
        exit(1);
    }
    if (limit < 0 || limit > 100) {
        fprintf(stderr, "Invalid limit (0-100)\n");
        exit(1);
    }

    int pids[MAX_PIDS];
    int pid_count = 0;
    int cycle_count = 0;

    if (use_top) {
        pid_count = get_pids_from_top(pids, MAX_PIDS);
        if (pid_count == 0) {
            fprintf(stderr, "No processes found in top output\n");
            exit(1);
        }
    } else if (exe_name) {
        pid = find_pid_by_name(exe_name);
        if (pid == -1) {
            fprintf(stderr, "No process found with name '%s'\n", exe_name);
            exit(1);
        }
        pids[0] = pid;
        pid_count = 1;
    } else if (pid > 0) {
        pids[0] = pid;
        pid_count = 1;
    } else {
        fprintf(stderr, "Must specify -p, -e, or -t\n");
        exit(1);
    }

    double cpu_fraction = limit / 100.0;
    long sleep_time = 200000;
    long active_time = (long)(sleep_time * cpu_fraction);
    long pause_time = sleep_time - active_time;

    while (keep_running) {
        // Allow processes to run
        for (int i = 0; i < pid_count; i++) {
            if (kill(pids[i], SIGCONT) != 0) {
                if (errno == EPERM || errno == ESRCH) continue;
                fprintf(stderr, "Error sending SIGCONT to PID %d: %s\n", pids[i], strerror(errno));
            }
        }
        usleep(active_time);

        // Check CPU usage and report offenders
        for (int i = 0; i < pid_count; i++) {
            float cpu_usage = get_cpu_usage(pids[i]);
            if (cpu_usage > limit * CPU_THRESHOLD && can_report_pid(pids[i])) {
                char *name = get_process_name(pids[i]);
                printf("Process %s (PID %d) exceeds CPU limit: %.1f%% (threshold: %d%%)\n",
                       name, pids[i], cpu_usage, limit);
            }
        }

        // Pause processes
        for (int i = 0; i < pid_count; i++) {
            if (kill(pids[i], SIGSTOP) != 0) {
                if (errno == EPERM || errno == ESRCH) continue;
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
            }
            cycle_count = 0;
        }
    }

    // Resume all processes before exiting
    for (int i = 0; i < pid_count; i++) {
        if (kill(pids[i], SIGCONT) != 0) {
            if (errno == EPERM && errno != ESRCH) {
                fprintf(stderr, "Error sending SIGCONT to PID %d on exit: %s\n", pids[i], strerror(errno));
            }
        }
    }

    printf("Exiting gracefully\n");
    return 0;
}
