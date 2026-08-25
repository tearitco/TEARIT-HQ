/* game_dispatch.c - One-shot op: read ALL keys from relay, dispatch each,
 * run NPC auto-play, compose frame, signal renderer.
 * This is the PAL equivalent of C game_manager's poll_relay().
 * Architecture: read all -> dispatch all -> NPC -> render -> exit. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE 4096

static int run_op(const char *path, const char *arg1) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (arg1) execl(path, path, arg1, NULL);
        else execl(path, path, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static void read_game_state(int *turn, int *total, char *ptype, int ptype_sz) {
    FILE *f = fopen("config.txt", "r");
    if (!f) { *turn = 0; *total = 4; snprintf(ptype, ptype_sz, "human"); return; }
    int t = 0, n = 4;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "current_turn=%d", &t);
        sscanf(line, "num_players=%d", &n);
    }
    int cp = (t % n) + 1;
    char key[64];
    snprintf(key, sizeof(key), "player_%d_type=", cp);
    rewind(f);
    snprintf(ptype, ptype_sz, "human");
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            char *val = line + strlen(key);
            char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
            snprintf(ptype, ptype_sz, "%s", val);
            break;
        }
    }
    fclose(f);
    *turn = t;
    *total = n;
}

static int keycode_to_action(int keycode) {
    switch (keycode) {
        case 1000: return 5;  /* left */
        case 1001: return 4;  /* right */
        case 1002: return 6;  /* up */
        case 1003: return 7;  /* down */
        case 119:  return 1;  /* word (w) */
        case 101:  return 3;  /* end turn (e) */
        default:   return -1;
    }
}

static void execute_action(int action) {
    if (action < 0) return;
    char arg[16];
    snprintf(arg, sizeof(arg), "%d", action);
    run_op("./ops/game_turn_input", arg);
}

static void npc_auto_play(void) {
    int max_loops = 32;
    while (max_loops-- > 0) {
        int turn, total;
        char ptype[32];
        read_game_state(&turn, &total, ptype, sizeof(ptype));
        if (strcmp(ptype, "computer") != 0) return;
        int action = 4 + (rand() % 4);
        execute_action(action);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand(time(NULL) ^ getpid());

    /* Step 1: Read ALL keys into memory, then truncate immediately.
     * This eliminates the race where parser writes a key between
     * our read and truncation — keys written after truncation
     * are safe for the next cycle. */
    FILE *hf = fopen("pieces/apps/player_app/interact_relay.txt", "r");
    if (!hf) return 0;

    /* Read all lines into buffer */
    char buf[MAX_LINE * 16];
    int buf_used = 0;
    buf[0] = '\0';
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), hf)) {
        int len = strlen(line);
        if (buf_used + len < sizeof(buf)) {
            strcpy(buf + buf_used, line);
            buf_used += len;
        }
    }
    fclose(hf);

    /* Truncate immediately — any keys written after this point
     * are safe for next cycle */
    FILE *trunc = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (trunc) fclose(trunc);

    /* Step 2: Process buffered keys */
    int any_action = 0;
    char *p = buf;
    while (*p) {
        int keycode = -1;
        if (sscanf(p, "%d", &keycode) == 1) {
            int action = keycode_to_action(keycode);
            if (action >= 0) {
                execute_action(action);
                any_action = 1;
            }
        }
        /* Advance past this line */
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
        else break;
    }

    /* Step 3: If any human action, run NPC auto-play */
    if (any_action) {
        npc_auto_play();
    }

    /* Step 4: Only compose frame if something actually changed */
    if (any_action) {
        run_op("./ops/game_compose_frame", NULL);
    }

    return 0;
}
