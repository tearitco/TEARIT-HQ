#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <stdbool.h>

#define TPM_PIECE_MANAGER_REL_PATH "pieces/master_ledger/plugins/+x/piece_manager.+x"
static char* pm_path_for(const char *project_root) {
    char *result = NULL;
    if (asprintf(&result, "%s/%s", project_root, TPM_PIECE_MANAGER_REL_PATH) == -1)
        return NULL;
    return result;
}

// Clock Daemon - Silent Background Timekeeper (60FPS MANDATE COMPLIANT)
// Efficiency Pattern: Active Pulse with 1-second intervals (sleep(1))
// Only hits frame marker when state actually changes to prevent unnecessary rendering

#define MAX_LINE 256
#define MAX_BUF 2048

char project_root[MAX_BUF] = "";

void resolve_root() {
    // Try current directory first if project_root is empty
    if (project_root[0] == '\0') strcpy(project_root, ".");
    
    FILE *kvp = fopen("pieces/locations/location_kvp", "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            if (strncmp(line, "project_root=", 13) == 0) {
                char *v = line + 13;
                v[strcspn(v, "\n\r")] = 0;
                snprintf(project_root, sizeof(project_root), "%s", v);
                break;
            }
        }
        fclose(kvp);
    }
}

void read_state(const char *state_path, char *turn, char *time_str, char *mode) {
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n\r")] = 0;
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = line;
            char *v = eq + 1;
            if (strcmp(k, "turn") == 0) strncpy(turn, v, 63);
            else if (strcmp(k, "time") == 0) strncpy(time_str, v, 63);
            else if (strcmp(k, "mode") == 0) strncpy(mode, v, 63);
        }
    }
    fclose(f);
}

void write_state(const char *state_path, const char *turn, const char *time_str, const char *mode) {
    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "turn=%s\n", turn);
        fprintf(f, "time=%s\n", time_str);
        fprintf(f, "mode=%s\n", mode);
        fclose(f);
    }
}

void increment_time_str(char *time_str) {
    int h, m, s;
    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        s++;
        if (s >= 60) { s = 0; m++; }
        if (m >= 60) { m = 0; h++; }
        if (h >= 24) { h = 0; }
        snprintf(time_str, 64, "%02d:%02d:%02d", h, m, s);
    }
}

void increment_hour_str(char *time_str) {
    int h, m, s;
    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        h++;
        if (h >= 24) { h = 0; }
        snprintf(time_str, 64, "%02d:%02d:%02d", h, m, s);
    }
}

void increment_turn(char *turn_str) {
    int turn = atoi(turn_str);
    turn++;
    snprintf(turn_str, 64, "%d", turn);
}

void hit_frame_marker() {
    char *path;
    asprintf(&path, "%s/pieces/master_ledger/frame_changed.txt", project_root);
    if (path) {
        FILE *f = fopen(path, "a");
        if (f) {
            fprintf(f, "X\n");
            fclose(f);
        }
        free(path);
    }
}

void call_piece_manager(const char *piece_id, const char *key, const char *value) {
    char *pm_path = pm_path_for(project_root);
    if (!pm_path) return;
    
#ifndef _WIN32
    pid_t p = fork();
    if (p == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(pm_path, pm_path, piece_id, "set-state", key, value, NULL);
        exit(1);
    } else if (p > 0) {
        waitpid(p, NULL, 0);
    }
#else
    char cmd[MAX_BUF];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s set-state %s %s > NUL 2>&1", pm_path, piece_id, key, value);
    system(cmd);
#endif
    free(pm_path);
}

int main(int argc, char *argv[]) {
    resolve_root();
    
    char *state_path;
    asprintf(&state_path, "%s/pieces/system/clock_daemon/state.txt", project_root);
    if (!state_path) return 1;
    
    char turn[64] = "0";
    char time_str[64] = "08:00:00";
    char mode[64] = "auto";
    
    if (argc > 1) {
        // OP MODE: Handle command and exit
        const char *cmd = argv[1];
        read_state(state_path, turn, time_str, mode);
        
        if (strcmp(cmd, "tick") == 0) {
            increment_turn(turn);
            write_state(state_path, turn, time_str, mode);
            call_piece_manager("clock_daemon", "turn", turn);
            hit_frame_marker();
            printf("Clock Ticked: Turn %s\n", turn);
        }
        else if (strcmp(cmd, "set-mode") == 0 && argc > 2) {
            strncpy(mode, argv[2], 63);
            write_state(state_path, turn, time_str, mode);
            printf("Clock Mode Set: %s\n", mode);
        }
        else if (strcmp(cmd, "increment-time") == 0) {
            increment_time_str(time_str);
            write_state(state_path, turn, time_str, mode);
            call_piece_manager("clock_daemon", "time", time_str);
            hit_frame_marker();
        }
        else if (strcmp(cmd, "increment-hour") == 0) {
            increment_hour_str(time_str);
            write_state(state_path, turn, time_str, mode);
            call_piece_manager("clock_daemon", "time", time_str);
            hit_frame_marker();
            printf("Clock Advanced: 1 Hour\n");
        }
        
        free(state_path);
        return 0;
    }

    // DAEMON MODE: Background loop with 1-second pulse (60fps mandate compliant)
    // sleep(1) prevents CPU spiking - only wakes once per second
    int auto_tick_count = 0;
    char last_turn[64] = "";
    char last_time_str[64] = "";
    
    while (1) {
        read_state(state_path, turn, time_str, mode);
        
        // Skip processing if state hasn't changed (prevents redundant updates)
        // Manual mode shouldn't auto-increment
        if (strcmp(mode, "auto") != 0) {
            strncpy(last_turn, turn, sizeof(last_turn) - 1);
            strncpy(last_time_str, time_str, sizeof(last_time_str) - 1);
            sleep(1);
            continue;
        }
        
        if (strcmp(mode, "auto") == 0) {
            increment_time_str(time_str);
            auto_tick_count++;
            
            bool turn_changed = false;
            if (auto_tick_count >= 60) {
                auto_tick_count = 0;
                increment_turn(turn);
                turn_changed = true;
            }
            
            write_state(state_path, turn, time_str, mode);
            
            // Only call piece_manager and hit frame marker when state changes
            if (turn_changed || (auto_tick_count % 10 == 0)) {
                call_piece_manager("clock_daemon", "turn", turn);
                call_piece_manager("clock_daemon", "time", time_str);
                hit_frame_marker();
            }
            
            strncpy(last_turn, turn, sizeof(last_turn) - 1);
            strncpy(last_time_str, time_str, sizeof(last_time_str) - 1);
        }
        
        sleep(1);
    }
    
    free(state_path);
    return 0;
}
