#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH 4096

static void ensure_parent(const char *path) {
    char buf[MAX_PATH];
    size_t len;
    char *slash = NULL;

    if (!path || !path[0]) return;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    slash = strrchr(buf, '/');
    if (!slash) return;
    *slash = '\0';
    len = strlen(buf);
    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
    mkdir(buf, 0755);
}

static void timestamp(char *dst, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(dst, size, "%Y-%m-%d %H:%M:%S", t);
}

static void update_state(const char *state_path, const char *goal, int amount, int reward_total, int punish_total) {
    FILE *f = fopen(state_path, "w");
    if (!f) return;
    fprintf(f, "reward_total=%d\n", reward_total);
    fprintf(f, "punish_total=%d\n", punish_total);
    fprintf(f, "net_score=%d\n", reward_total - punish_total);
    fprintf(f, "last_goal=%s\n", goal ? goal : "");
    fprintf(f, "last_event=reward\n");
    fprintf(f, "last_amount=%d\n", amount);
    fclose(f);
}

int main(int argc, char **argv) {
    char log_path[MAX_PATH];
    char state_path[MAX_PATH];
    char ts[64];
    const char *project_root;
    const char *goal;
    const char *reason = "";
    int amount;
    int reward_total = 0;
    int punish_total = 0;
    FILE *log;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <project_root> <goal> <amount> [reason]\n", argv[0]);
        return 1;
    }

    project_root = argv[1];
    goal = argv[2];
    amount = atoi(argv[3]);
    if (argc > 4) reason = argv[4];

    snprintf(log_path, sizeof(log_path), "%s/pieces/rl/reward.log", project_root);
    snprintf(state_path, sizeof(state_path), "%s/pieces/rl/state.kvp", project_root);
    ensure_parent(log_path);
    ensure_parent(state_path);

    log = fopen(log_path, "a");
    if (log) {
        timestamp(ts, sizeof(ts));
        fprintf(log, "[%s] REWARD|goal=%s|amount=%d|reason=%s\n", ts, goal, amount, reason);
        fclose(log);
    }

    {
        FILE *state = fopen(state_path, "r");
        char line[256];
        if (state) {
            while (fgets(line, sizeof(line), state)) {
                if (sscanf(line, "reward_total=%d", &reward_total) == 1) continue;
                if (sscanf(line, "punish_total=%d", &punish_total) == 1) continue;
            }
            fclose(state);
        }
    }

    reward_total += amount;
    update_state(state_path, goal, amount, reward_total, punish_total);
    printf("reward=%d goal=%s\n", amount, goal);
    return 0;
}
