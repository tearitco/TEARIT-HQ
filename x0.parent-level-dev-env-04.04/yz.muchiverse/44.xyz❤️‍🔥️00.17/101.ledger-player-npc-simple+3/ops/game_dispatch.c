/* game_dispatch.c - One-shot op: read ALL keys from relay, dispatch each,
 * run NPC auto-play, compose frame, signal renderer.
 * This is the PAL equivalent of C game_manager's poll_history().
 * Word game variant: Enter submits word from gui_state.txt, 'e' ends turn. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE 4096

static int run_op(const char *op_path, int argc, const char **argv) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        char *args[32];
        args[0] = (char *)op_path;
        for (int i = 0; i < argc && i < 30; i++) args[i + 1] = (char *)argv[i];
        args[argc + 1] = NULL;
        execv(op_path, args);
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

static void read_input_word(char *buf, size_t bufsz) {
    buf[0] = '\0';
    FILE *f = fopen("pieces/apps/player_app/manager/gui_state.txt", "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *val = eq + 1;
            val[strcspn(val, "\r\n")] = '\0';
            if (strcmp(line, "input_text") == 0) {
                strncpy(buf, val, bufsz - 1);
                buf[bufsz - 1] = '\0';
                break;
            }
        }
    }
    fclose(f);
}

static void clear_input_text(void) {
    FILE *f = fopen("pieces/apps/player_app/manager/gui_state.txt", "w");
    if (f) { fprintf(f, "input_text=\n"); fclose(f); }
    FILE *bf = fopen("pieces/apps/player_app/cli_buffers.txt", "w");
    if (bf) fclose(bf);
}

static void npc_pick_word(char *buf, size_t bufsz) {
    static const char *words[] = {
        "hello", "world", "adventure", "serendipity", "quantum",
        "melody", "eclipse", "thunder", "whisper", "crystal",
        "voyage", "radiant", "phoenix", "cascade", "cipher",
        "aurora", "sentinel", "luminous", "tempest", "zenith"
    };
    srand(time(NULL) ^ getpid());
    int idx = rand() % 20;
    strncpy(buf, words[idx], bufsz - 1);
    buf[bufsz - 1] = '\0';
}

static void get_current_player_name(char *buf, size_t bufsz) {
    int turn, total;
    char ptype[32];
    read_game_state(&turn, &total, ptype, sizeof(ptype));
    int cp = (turn % total) + 1;
    char key[64];
    snprintf(key, sizeof(key), "player_%d_name=", cp);
    FILE *f = fopen("config.txt", "r");
    buf[0] = '\0';
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, key, strlen(key)) == 0) {
                char *val = line + strlen(key);
                char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
                strncpy(buf, val, bufsz - 1);
                buf[bufsz - 1] = '\0';
                break;
            }
        }
        fclose(f);
    }
    if (buf[0] == '\0') snprintf(buf, bufsz, "player%d", cp);
}

static void execute_action(int keycode) {
    if (keycode == 13) {
        char word[256] = "hello";
        read_input_word(word, sizeof(word));
        if (word[0] == '\0') strncpy(word, "hello", sizeof(word) - 1);
        const char *args[] = {"1", word, "human", NULL};
        run_op("./ops/word_turn_input", 3, args);
        clear_input_text();
    } else if (keycode == 101) {
        const char *args[] = {"3", NULL};
        run_op("./ops/word_turn_input", 1, args);
    }
}

static void npc_auto_play(void) {
    int max_loops = 32;
    while (max_loops-- > 0) {
        int turn, total;
        char ptype[32];
        read_game_state(&turn, &total, ptype, sizeof(ptype));
        if (strcmp(ptype, "computer") != 0) return;

        char word[256];
        npc_pick_word(word, sizeof(word));

        const char *args[] = {"1", word, "computer", NULL};
        run_op("./ops/word_turn_input", 3, args);
        usleep(300000);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand(time(NULL) ^ getpid());

    /* Step 1: Read ALL keys into memory, then truncate immediately.
     * This eliminates the race where parser writes a key between
     * our read and the truncation. */
    FILE *hf = fopen("pieces/apps/player_app/interact_relay.txt", "r");
    if (!hf) return 0;

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

    /* Truncate immediately */
    FILE *trunc = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (trunc) fclose(trunc);

    /* Step 2: Process buffered keys */
    int any_action = 0;
    char *p = buf;
    while (*p) {
        int keycode = -1;
        if (sscanf(p, "%d", &keycode) == 1) {
            execute_action(keycode);
            any_action = 1;
        }
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
        const char *compose_args[] = {NULL};
        run_op("./ops/word_compose_frame", 0, compose_args);
    }

    return 0;
}
