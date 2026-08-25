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

int main(int argc, char **argv) {
    char current_goal_path[MAX_PATH];
    char label_log_path[MAX_PATH];
    char state_path[MAX_PATH];
    char ts[64];
    FILE *f;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <project_root> <session_label> [goal]\n", argv[0]);
        return 1;
    }

    snprintf(current_goal_path, sizeof(current_goal_path), "%s/pieces/goals/current_goal.txt", argv[1]);
    snprintf(label_log_path, sizeof(label_log_path), "%s/pieces/goals/label_history.log", argv[1]);
    snprintf(state_path, sizeof(state_path), "%s/pieces/sessions/state.kvp", argv[1]);
    ensure_parent(current_goal_path);
    ensure_parent(label_log_path);
    ensure_parent(state_path);

    timestamp(ts, sizeof(ts));
    f = fopen(current_goal_path, "w");
    if (f) {
        fprintf(f, "session_label=%s\n", argv[2]);
        fprintf(f, "updated_at=%s\n", ts);
        if (argc > 3) fprintf(f, "goal=%s\n", argv[3]);
        fclose(f);
    }

    f = fopen(label_log_path, "a");
    if (f) {
        fprintf(f, "[%s] session_label=%s", ts, argv[2]);
        if (argc > 3) fprintf(f, " goal=%s", argv[3]);
        fprintf(f, "\n");
        fclose(f);
    }

    f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "session_label=%s\n", argv[2]);
        fprintf(f, "updated_at=%s\n", ts);
        if (argc > 3) fprintf(f, "goal=%s\n", argv[3]);
        fclose(f);
    }

    printf("session_label=%s\n", argv[2]);
    return 0;
}
