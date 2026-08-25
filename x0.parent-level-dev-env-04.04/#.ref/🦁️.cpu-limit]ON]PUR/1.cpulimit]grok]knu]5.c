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
#define REPORT_BACKOFF 300 // Seconds to wait before reporting same PID again (5 min)
#define MIN_INTERVAL 3 // Minimum seconds between CPU measurements
#define DEBUG 0 // Set to 1 for debug logs

volatile sig_atomic_t keep_running = 1;

typedef struct {
    int pid;
    time_t last_reported;
} ReportedPid;

typedef struct {
    int pid;
    unsigned long prev_total;
    time_t prev_time;
} CpuState;

ReportedPid reported_pids[MAX_PIDS] = {0};
int reported_count = 0;
CpuState cpu_states[MAX_PIDS] = {0};
int cpu_state_count = 0;

void handle_sigint(int sig) {
    keep_running = 0;
}

int is_firefox_process(int pid, int parent_pid) {
    char path[256];
    char comm[256] = {0};
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (DEBUG) fprintf(stderr, "Debug: Opening %s\n", path);
    fp = fopen(path, "r");
    if (!fp) return 0;
    if (!fgets(comm, sizeof(comm) - 1, fp)) {
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (fgets failed)\n", path);
        fclose(fp);
        return 0;
    }
    comm[strcspn(comm, "\n")] = 0;
    if (DEBUG) fprintf(stderr, "Debug: Closing %s (comm read)\n", path);
    fclose(fp);

    if (strncmp(comm, "kworker", 7) == 0 || strcmp(comm, "upowerd") == 0 ||
        strcmp(comm, "systemd") == 0 || strcmp(comm, "dbus-daemon") == 0) {
        return 0;
    }

    if (strcmp(comm, "firefox") == 0 || strcmp(comm, "firefox-bin") == 0) {
        return 1;
    }

    if (parent_pid > 0) {
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        if (DEBUG) fprintf(stderr, "Debug: Opening %s\n", path);
        fp = fopen(path, "r");
        if (!fp) return 0;
        char line[512];
        if (!fgets(line, sizeof(line), fp)) {
            if (DEBUG) fprintf(stderr, "Debug: Closing %s (fgets failed)\n", path);
            fclose(fp);
            return 0;
        }
        int ppid;
        char *token = strtok(line, " ");
        for (int i = 1; i < 4 && token; i++) token = strtok(NULL, " ");
        ppid = token ? atoi(token) : 0;
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (stat read)\n", path);
        fclose(fp);
        if (ppid == parent_pid) return 1;
    }
    return 0;
}

int find_firefox_pids(int *pids, int max_pids, int use_top) {
    int pid_count = 0;
    int main_pids[MAX_PIDS] = {0};
    int main_pid_count = 0;

    // Scan /proc for Firefox processes
    for (int i = 200; i < 99999 && main_pid_count < max_pids; i++) {
        if (is_firefox_process(i, 0)) {
            main_pids[main_pid_count++] = i;
        }
    }

    // Add main PIDs and their children
    for (int i = 0; i < main_pid_count && pid_count < max_pids; i++) {
        pids[pid_count++] = main_pids[i];
        for (int j = 200; j < 99999 && pid_count < max_pids; j++) {
            if (j == main_pids[i]) continue;
            if (is_firefox_process(j, main_pids[i])) {
                pids[pid_count++] = j;
            }
        }
    }

    return pid_count;
}

char *get_process_name(int pid) {
    char comm[256] = {0};
    char path[256];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (DEBUG) fprintf(stderr, "Debug: Opening %s\n", path);
    fp = fopen(path, "r");
    if (!fp) return "unknown";
    if (fgets(comm, sizeof(comm) - 1, fp)) {
        comm[strcspn(comm, "\n")] = 0;
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (comm read)\n", path);
        fclose(fp);
        static char result[256];
        strncpy(result, comm, sizeof(result) - 1);
        result[sizeof(result) - 1] = '\0';
        return result;
    }
    if (DEBUG) fprintf(stderr, "Debug: Closing %s (fgets failed)\n", path);
    fclose(fp);
    return "unknown";
}

int can_report_pid(int pid) {
    time_t now = time(NULL);
    for (int i = 0; i < reported_count; i++) {
        if (reported_pids[i].pid == pid) {
            if (now - reported_pids[i].last_reported < REPORT_BACKOFF) {
                return 0;
            } else {
                reported_pids[i].last_reported = now;
                return 1;
            }
        }
    }
    if (reported_count < MAX_PIDS) {
        reported_pids[reported_count].pid = pid;
        reported_pids[reported_count].last_reported = now;
        reported_count++;
    }
    return 1;
}

float get_cpu_usage(int pid) {
    char path[256];
    FILE *fp;
    unsigned long utime, stime;
    long hz = sysconf(_SC_CLK_TCK);
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    time_t now = time(NULL);

    int state_idx = -1;
    for (int i = 0; i < cpu_state_count; i++) {
        if (cpu_states[i].pid == pid) {
            state_idx = i;
            break;
        }
    }
    if (state_idx == -1 && cpu_state_count < MAX_PIDS) {
        state_idx = cpu_state_count++;
        cpu_states[state_idx].pid = pid;
        cpu_states[state_idx].prev_total = 0;
        cpu_states[state_idx].prev_time = 0;
    }
    if (state_idx == -1) return 0.0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (DEBUG) fprintf(stderr, "Debug: Opening %s\n", path);
    fp = fopen(path, "r");
    if (!fp) return 0.0;

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (fgets failed)\n", path);
        fclose(fp);
        return 0.0;
    }
    char *token = strtok(line, " ");
    for (int i = 1; i < 14 && token; i++) token = strtok(NULL, " ");
    if (!token) {
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (token failed)\n", path);
        fclose(fp);
        return 0.0;
    }
    utime = atol(token);
    token = strtok(NULL, " ");
    if (!token) {
        if (DEBUG) fprintf(stderr, "Debug: Closing %s (token failed)\n", path);
        fclose(fp);
        return 0.0;
    }
    stime = atol(token);
    if (DEBUG) fprintf(stderr, "Debug: Closing %s (stat read)\n", path);
    fclose(fp);

    unsigned long total_time = utime + stime;
    float cpu_usage = 0.0;
    if (cpu_states[state_idx].prev_time != 0 && now - cpu_states[state_idx].prev_time >= MIN_INTERVAL) {
        unsigned long delta_time = total_time - cpu_states[state_idx].prev_total;
        time_t delta_secs = now - cpu_states[state_idx].prev_time;
        cpu_usage = (100.0 * delta_time) / (delta_secs * hz * num_cores);
        if (cpu_usage > 100.0) cpu_usage = 100.0;
        if (cpu_usage < 0.0) cpu_usage = 0.0;
    }

    cpu_states[state_idx].prev_total = total_time;
    cpu_states[state_idx].prev_time = now;
    return cpu_usage;
}

int main(int argc, char *argv[]) {
printf("🔝️");
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

    int pids[MAX_PIDS] = {0};
    int pid_count = 0;
    int cycle_count = 0;

    if (use_top) {
        pid_count = find_firefox_pids(pids, MAX_PIDS, 1);
        if (pid_count == 0) {
            fprintf(stderr, "No Firefox processes found\n");
            exit(1);
        }
    } else if (exe_name) {
        pid_count = find_firefox_pids(pids, MAX_PIDS, 0);
        if (pid_count == 0) {
            fprintf(stderr, "No process found with name '%s'\n", exe_name);
            exit(1);
        }
    } else if (pid > 0) {
        if (is_firefox_process(pid, 0)) {
            pids[0] = pid;
            pid_count = 1;
        } else {
            fprintf(stderr, "PID %d is not a Firefox process\n", pid);
            exit(1);
        }
    } else {
        fprintf(stderr, "Must specify -p, -e, or -t\n");
        exit(1);
    }

    double cpu_fraction = limit / 100.0;
    long sleep_time = 200000;
    long active_time = (long)(sleep_time * cpu_fraction);
    long pause_time = sleep_time - active_time;

    while (keep_running) {
        for (int i = 0; i < pid_count; i++) {
            if (pids[i] == 0) continue;
            if (kill(pids[i], SIGCONT) != 0) {
                if (errno == EPERM || errno == ESRCH) continue;
                fprintf(stderr, "Error sending SIGCONT to PID %d: %s\n", pids[i], strerror(errno));
            }
        }
        usleep(active_time);

        for (int i = 0; i < pid_count; i++) {
            if (pids[i] == 0) continue;
            float cpu_usage = get_cpu_usage(pids[i]);
            if (cpu_usage > limit * CPU_THRESHOLD && can_report_pid(pids[i])) {
                char *name = get_process_name(pids[i]);
                printf("Process %s (PID %d) exceeds CPU limit: %.1f%% (threshold: %d%%)\n",
                       name, pids[i], cpu_usage, limit);
            }
        }

        for (int i = 0; i < pid_count; i++) {
            if (pids[i] == 0) continue;
            if (kill(pids[i], SIGSTOP) != 0) {
                if (errno == EPERM || errno == ESRCH) continue;
                fprintf(stderr, "Error sending SIGSTOP to PID %d: %s\n", pids[i], strerror(errno));
            }
        }
        usleep(pause_time);

        if (use_top && ++cycle_count >= REFRESH_INTERVAL) {
            cpu_state_count = 0;
            pid_count = find_firefox_pids(pids, MAX_PIDS, 1);
            if (pid_count == 0) {
                fprintf(stderr, "No Firefox processes found\n");
                keep_running = 0;
            }
            cycle_count = 0;
        }
    }

    for (int i = 0; i < pid_count; i++) {
        if (pids[i] == 0) continue;
        if (kill(pids[i], SIGCONT) != 0 && errno != EPERM && errno != ESRCH) {
            fprintf(stderr, "Error sending SIGCONT to PID %d on exit: %s\n", pids[i], strerror(errno));
        }
    }

    printf("Exiting gracefully\n");
    return 0;
}
