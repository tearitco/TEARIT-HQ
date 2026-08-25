#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>

#define MAX_LINE 4096
#define POLL_INTERVAL 16667  /* 16ms = ~60fps, x0.moke standard */

volatile int running = 1;
long last_relay_pos = 0;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void log_mgr(const char* fmt, ...) {
    FILE *f = fopen("pieces/system/manager.log", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        fprintf(f, "[%ld] ", time(NULL));
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

void pulse_frame_marker(void) {
    FILE *f = fopen("pieces/display/frame_changed.txt", "a");
    if (f) { fputc('G', f); fflush(f); fclose(f); }
}

int run_op(const char *path, const char *arg1, const char *arg2) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (arg2) execl(path, path, arg1, arg2, NULL);
        else if (arg1) execl(path, path, arg1, NULL);
        else execl(path, path, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int keycode_to_action(int keycode) {
    switch (keycode) {
        case 1000: return 5;
        case 1001: return 4;
        case 1002: return 6;
        case 1003: return 7;
        case 119:  return 1;
        case 101:  return 3;
        default:   return -1;
    }
}

/* Read current_turn, total_actors, and current player's type from config */
static void read_game_state(int *turn, int *total, char *player_type, int type_sz) {
    FILE *f = fopen("config.txt", "r");
    if (!f) { *turn = 0; *total = 4; snprintf(player_type, type_sz, "human"); return; }
    int t = 0, n = 4, cp = 1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "current_turn=%d", &t);
        sscanf(line, "num_players=%d", &n);
    }
    cp = (t % n) + 1;
    /* Re-read to find player_N_type */
    char key[64];
    snprintf(key, sizeof(key), "player_%d_type=", cp);
    rewind(f);
    snprintf(player_type, type_sz, "human");
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            char *val = line + strlen(key);
            /* trim newline */
            char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
            snprintf(player_type, type_sz, "%s", val);
            break;
        }
    }
    fclose(f);
    *turn = t;
    *total = n;
}

void execute_action(int action) {
    if (action < 0) return;
    char arg[16];
    snprintf(arg, sizeof(arg), "%d", action);
    log_mgr("Executing action %d", action);
    run_op("./ops/game_turn_input", arg, NULL);
    run_op("./ops/game_compose_frame", NULL, NULL);
}

/* NPC auto-play: execute moves for computer players until it's a human's turn */
void npc_auto_play(void) {
    int max_loops = 32;
    while (max_loops-- > 0 && running) {
        int turn, total;
        char ptype[32];
        read_game_state(&turn, &total, ptype, sizeof(ptype));
        if (strcmp(ptype, "computer") != 0) return; /* human's turn */
        log_mgr("NPC auto-play: turn %d, player type=computer", turn);
        /* Pick random move direction: 4=right, 5=left, 6=up, 7=down */
        int action = 4 + (rand() % 4);
        execute_action(action);
        usleep(300000); /* 300ms delay so human can see NPC moves */
    }
}

void poll_relay(void) {
    FILE *hf = fopen("pieces/apps/player_app/interact_relay.txt", "r");
    if (!hf) return;
    fseek(hf, last_relay_pos, SEEK_SET);
    char line[MAX_LINE];
    int any_action = 0;
    while (fgets(line, sizeof(line), hf)) {
        int keycode = -1;
        if (sscanf(line, "%d", &keycode) == 1) {
            log_mgr("Read keycode: %d from relay", keycode);
            int action = keycode_to_action(keycode);
            if (action >= 0) {
                execute_action(action);
                any_action = 1;
            }
        }
    }
    last_relay_pos = ftell(hf);
    fclose(hf);
    FILE *trunc = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (trunc) fclose(trunc);
    last_relay_pos = 0;

    /* After human action, run NPC auto-play */
    if (any_action) {
        npc_auto_play();
    }
}

void *polling_thread(void *arg) {
    (void)arg;
    log_mgr("Polling thread started");
    while (running) {
        poll_relay();
        usleep(POLL_INTERVAL);
    }
    log_mgr("Polling thread exiting");
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    log_mgr("=== Game Manager Started ===");
    srand(time(NULL) ^ getpid());

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    FILE *relay = fopen("pieces/apps/player_app/interact_relay.txt", "a");
    if (relay) fclose(relay);

    pthread_t poll_tid;
    if (pthread_create(&poll_tid, NULL, polling_thread, NULL) != 0) {
        log_mgr("ERROR: pthread_create failed");
        return 1;
    }

    log_mgr("Polling thread created");

    while (running) {
        sleep(1);
    }

    pthread_join(poll_tid, NULL);
    log_mgr("=== Game Manager Stopped ===");
    return 0;
}
