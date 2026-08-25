#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

static volatile sig_atomic_t g_shutdown = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static char* trim_ws(char *s) {
    char *end = NULL;
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int root_has_anchors(const char *root) {
    char pieces_path[MAX_PATH];
    char projects_path[MAX_PATH];

    snprintf(pieces_path, sizeof(pieces_path), "%s/pieces", root);
    snprintf(projects_path, sizeof(projects_path), "%s/projects", root);
    return access(pieces_path, F_OK) == 0 && access(projects_path, F_OK) == 0;
}

static void resolve_root(char *project_root, size_t size) {
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (!getcwd(project_root, size)) {
        strncpy(project_root, ".", size - 1);
        project_root[size - 1] = '\0';
    }

    if (root_has_anchors(project_root)) {
        if (kvp) fclose(kvp);
        return;
    }

    if (!kvp) return;

    char line[2048];
    while (fgets(line, sizeof(line), kvp)) {
        if (strncmp(line, "project_root=", 13) == 0) {
            char *value = trim_ws(line + 13);
            if (root_has_anchors(value)) {
                strncpy(project_root, value, size - 1);
                project_root[size - 1] = '\0';
            }
            break;
        }
    }
    fclose(kvp);
}

static void ensure_file(const char *path) {
    FILE *f = fopen(path, "a");
    if (f) fclose(f);
}

static void ensure_default_state(const char *state_path) {
    if (access(state_path, F_OK) == 0) return;

    FILE *f = fopen(state_path, "w");
    if (!f) return;

    fprintf(f, "player_x=-1\n");
    fprintf(f, "player_y=-1\n");
    fprintf(f, "move_count=0\n");
    fprintf(f, "camera_mode=4\n");
    fprintf(f, "action_label=Advance Hero\n");
    fprintf(f, "status_text=Moves: 0 | Hero=(-1,-1)\n");
    fclose(f);
}

static void load_state(const char *state_path, int *player_x, int *player_y, int *move_count) {
    FILE *f = fopen(state_path, "r");
    char line[MAX_LINE];

    *player_x = -1;
    *player_y = -1;
    *move_count = 0;

    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *key = NULL;
        char *value = NULL;
        if (!eq) continue;
        *eq = '\0';
        key = trim_ws(line);
        value = trim_ws(eq + 1);

        if (strcmp(key, "player_x") == 0) *player_x = atoi(value);
        else if (strcmp(key, "player_y") == 0) *player_y = atoi(value);
        else if (strcmp(key, "move_count") == 0) *move_count = atoi(value);
    }

    fclose(f);
}

static void write_state(const char *state_path, int player_x, int player_y, int move_count) {
    FILE *f = fopen(state_path, "w");
    char status[128];

    if (!f) return;

    snprintf(status, sizeof(status), "Moves: %d | Hero=(%d,%d)", move_count, player_x, player_y);
    fprintf(f, "player_x=%d\n", player_x);
    fprintf(f, "player_y=%d\n", player_y);
    fprintf(f, "move_count=%d\n", move_count);
    fprintf(f, "camera_mode=4\n");
    fprintf(f, "action_label=Advance Hero\n");
    fprintf(f, "status_text=%s\n", status);
    fclose(f);
}

static void bump_marker(const char *marker_path, char marker) {
    FILE *f = fopen(marker_path, "a");
    if (!f) return;
    fprintf(f, "%c\n", marker);
    fclose(f);
}

static void process_command(const char *cmd, const char *state_path, const char *frame_marker_path) {
    int player_x = -1;
    int player_y = -1;
    int move_count = 0;

    if (!strstr(cmd, "OP gltpm-demo::advance")) return;

    load_state(state_path, &player_x, &player_y, &move_count);

    move_count++;
    player_x++;
    if (player_x > 1) {
        player_x = -1;
        player_y++;
        if (player_y > 1) player_y = -1;
    }

    write_state(state_path, player_x, player_y, move_count);
    bump_marker(frame_marker_path, 'M');
}

int main(void) {
    char project_root[MAX_PATH] = ".";
    char state_path[MAX_PATH];
    char history_path[MAX_PATH];
    char frame_marker_path[MAX_PATH];
    long last_history_pos = 0;

    setpgid(0, 0);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    resolve_root(project_root, sizeof(project_root));

    snprintf(state_path, sizeof(state_path), "%s/projects/gltpm-demo/session/state.txt", project_root);
    snprintf(history_path, sizeof(history_path), "%s/projects/gltpm-demo/session/history.txt", project_root);
    snprintf(frame_marker_path, sizeof(frame_marker_path), "%s/projects/gltpm-demo/session/frame_changed.txt", project_root);

    ensure_default_state(state_path);
    ensure_file(history_path);
    ensure_file(frame_marker_path);

    FILE *history = fopen(history_path, "r");
    if (history) {
        fseek(history, 0, SEEK_END);
        last_history_pos = ftell(history);
        fclose(history);
    }

    while (!g_shutdown) {
        int did_work = 0;
        history = fopen(history_path, "r");
        if (history) {
            char line[MAX_LINE];
            fseek(history, last_history_pos, SEEK_SET);
            while (fgets(line, sizeof(line), history)) {
                char *cmd = strstr(line, "COMMAND:");
                if (cmd) {
                    process_command(trim_ws(cmd + 8), state_path, frame_marker_path);
                    did_work = 1;
                }
            }
            last_history_pos = ftell(history);
            fclose(history);
        }

        usleep(did_work ? 16667 : 100000);
    }

    return 0;
}
