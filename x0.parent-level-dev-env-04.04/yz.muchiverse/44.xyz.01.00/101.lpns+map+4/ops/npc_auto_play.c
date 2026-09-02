/* npc_auto_play.c - One-shot op: execute random moves for all computer
 * players until it's a human's turn.
 * Called after every human action by PAL script or game_manager.
 * Reuses game_turn_input for each move. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

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

static void read_config(int *turn, int *total, int *cp, char *ptype, int ptype_sz) {
    FILE *f = fopen("config.txt", "r");
    if (!f) { *turn = 0; *total = 4; *cp = 1; snprintf(ptype, ptype_sz, "human"); return; }
    int t = 0, n = 4;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "current_turn=%d", &t);
        sscanf(line, "num_players=%d", &n);
    }
    *turn = t; *total = n;
    int cur = (t % n) + 1;
    *cp = cur;
    char key[64];
    snprintf(key, sizeof(key), "player_%d_type=", cur);
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
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand(time(NULL) ^ getpid());

    int max_loops = 32;
    while (max_loops-- > 0) {
        int turn, total, cp;
        char ptype[32];
        read_config(&turn, &total, &cp, ptype, sizeof(ptype));
        if (strcmp(ptype, "computer") != 0) break;
        int action = 4 + (rand() % 4);
        char arg[16];
        snprintf(arg, sizeof(arg), "%d", action);
        run_op("./ops/game_turn_input", arg);
        usleep(300000);
    }
    return 0;
}
