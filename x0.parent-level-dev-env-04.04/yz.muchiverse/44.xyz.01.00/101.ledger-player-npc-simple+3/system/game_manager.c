#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define POLL_INTERVAL 16667  /* 16ms = ~60fps, x0.moke standard */

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) {
        snprintf(project_root, sizeof(project_root), "%s", env);
        return;
    }
    if (!getcwd(project_root, sizeof(project_root))) {
        snprintf(project_root, sizeof(project_root), ".");
    }
}

static volatile int running = 1;
static long last_history_pos = 0;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void log_mgr(const char* fmt, ...) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/manager.log", project_root);
    FILE *f = fopen(path, "a");
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

static void pulse_frame_marker(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fputc('G', f);
        fflush(f);
        fclose(f);
    }
}

static void clear_input_text(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "input_text=\n");
        fclose(f);
    }
    char bp[PATH_BUF];
    snprintf(bp, sizeof(bp), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
    FILE *bf = fopen(bp, "w");
    if (bf) fclose(bf);
    pulse_frame_marker();
}

static int keycode_to_action(int keycode) {
    switch (keycode) {
        case 13:   return 1;  /* Enter from cli_io = submit word */
        case 101:  return 3;  /* 'e' = end_turn */
        case 27:   return -1; /* ESC */
        default:   return -1;
    }
}

static void read_input_word(char *buf, size_t bufsz) {
    buf[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;
            val[strcspn(val, "\r\n")] = '\0';
            if (strcmp(key, "input_text") == 0) {
                strncpy(buf, val, bufsz - 1);
                buf[bufsz - 1] = '\0';
                break;
            }
        }
    }
    fclose(f);
}

static int run_op(const char *op_path, int argc, const char **argv) {
    pid_t pid = fork();
    if (pid == 0) {
        chdir(project_root);
        char *args[32];
        args[0] = (char *)op_path;
        for (int i = 0; i < argc && i < 30; i++) args[i + 1] = (char *)argv[i];
        args[argc + 1] = NULL;
        execv(op_path, args);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════
 * GAME STATE READING — config.txt accessors
 * ═══════════════════════════════════════════════════════════════ */

static int read_config_int(const char *key) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/config.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    int val = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            val = atoi(line + strlen(prefix));
            break;
        }
    }
    fclose(f);
    return val;
}

static void read_config_str(const char *key, char *buf, size_t bufsz) {
    buf[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/config.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            char *val = line + strlen(prefix);
            val[strcspn(val, "\r\n")] = '\0';
            strncpy(buf, val, bufsz - 1);
            buf[bufsz - 1] = '\0';
            break;
        }
    }
    fclose(f);
}

static int get_current_player(void) {
    int turn = read_config_int("current_turn");
    int num = read_config_int("num_players");
    if (num <= 0) num = 1;
    return (turn % num) + 1;
}

static int is_current_player_human(void) {
    int player = get_current_player();
    char key[64];
    snprintf(key, sizeof(key), "player_%d_type", player);
    char type[32];
    read_config_str(key, type, sizeof(type));
    return (strcmp(type, "human") == 0);
}

static int is_game_playing(void) {
    char state[32];
    read_config_str("game_state", state, sizeof(state));
    return (strcmp(state, "playing") == 0);
}

static void get_current_player_name(char *buf, size_t bufsz) {
    int player = get_current_player();
    char key[64];
    snprintf(key, sizeof(key), "player_%d_name", player);
    read_config_str(key, buf, bufsz);
    if (buf[0] == '\0') snprintf(buf, bufsz, "player%d", player);
}

/* ═══════════════════════════════════════════════════════════════
 * NPC WORD PICKING — from word_bank.txt
 * ═══════════════════════════════════════════════════════════════ */

static void npc_pick_word(char *buf, size_t bufsz) {
    /* Hardcoded fallback bank (matches benchmark) */
    static const char *words[] = {
        "hello", "world", "adventure", "serendipity", "quantum",
        "melody", "eclipse", "thunder", "whisper", "crystal",
        "voyage", "radiant", "phoenix", "cascade", "cipher",
        "aurora", "sentinel", "luminous", "tempest", "zenith"
    };
    #define NUM_WORDS 20

    /* Try to read from word_bank.txt first */
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/registry/word_bank.txt", project_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        char *bank_words[100];
        int count = 0;
        fgets(line, sizeof(line), f); /* skip header */
        while (fgets(line, sizeof(line), f) && count < 100) {
            char *p = strchr(line, '|');
            if (p) {
                p++;
                char *end = strchr(p, '|');
                int len = end ? (int)(end - p) : (int)strlen(p);
                if (len > 0 && len < 256) {
                    bank_words[count] = malloc(len + 1);
                    memcpy(bank_words[count], p, len);
                    bank_words[count][len] = '\0';
                    bank_words[count][strcspn(bank_words[count], "\r\n")] = '\0';
                    count++;
                }
            }
        }
        fclose(f);
        if (count > 0) {
            srand(time(NULL));
            int idx = rand() % count;
            strncpy(buf, bank_words[idx], bufsz - 1);
            buf[bufsz - 1] = '\0';
            for (int i = 0; i < count; i++) free(bank_words[i]);
            return;
        }
    }

    /* Fallback to hardcoded bank */
    srand(time(NULL));
    int idx = rand() % NUM_WORDS;
    strncpy(buf, words[idx], bufsz - 1);
    buf[bufsz - 1] = '\0';
}

/* ═══════════════════════════════════════════════════════════════
 * ACTION EXECUTION
 * ═══════════════════════════════════════════════════════════════ */

static void execute_action(int action) {
    if (action < 0) return;

    if (action == 1) {
        char word[256] = "hello";
        read_input_word(word, sizeof(word));
        const char *args[] = {"1", word, "human", NULL};
        log_mgr("Submitting word: %s", word);
        run_op("./ops/word_turn_input", 3, args);
        clear_input_text();
    } else {
        char act_str[16];
        snprintf(act_str, sizeof(act_str), "%d", action);
        const char *args[] = {act_str, NULL};
        log_mgr("Executing action %d", action);
        run_op("./ops/word_turn_input", 1, args);
    }

    log_mgr("Recomposing frame...");
    const char *compose_args[] = {NULL};
    run_op("./ops/word_compose_frame", 0, compose_args);

    pulse_frame_marker();
    log_mgr("Action complete, frame pulsed");
}

static void execute_npc_turn(void) {
    char word[256];
    npc_pick_word(word, sizeof(word));

    char player_name[64];
    get_current_player_name(player_name, sizeof(player_name));

    log_mgr("NPC '%s' auto-playing word: %s", player_name, word);

    const char *args[] = {"1", word, "computer", NULL};
    run_op("./ops/word_turn_input", 3, args);

    log_mgr("Recomposing frame...");
    const char *compose_args[] = {NULL};
    run_op("./ops/word_compose_frame", 0, compose_args);

    pulse_frame_marker();
    log_mgr("NPC turn complete, frame pulsed");
}

/* ═══════════════════════════════════════════════════════════════
 * MAIN POLL LOOP
 * ═══════════════════════════════════════════════════════════════ */

static void poll_history(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *hf = fopen(path, "r");
    if (!hf) return;

    fseek(hf, last_history_pos, SEEK_SET);

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), hf)) {
        int keycode = -1;
        if (sscanf(line, "%d", &keycode) == 1) {
            log_mgr("Read keycode: %d from history", keycode);

            int action = keycode_to_action(keycode);
            if (action >= 0) {
                execute_action(action);
            } else if (keycode == 3) {
                log_mgr("Ctrl+C received, quitting");
                running = 0;
            }
        }
    }

    last_history_pos = ftell(hf);
    fclose(hf);
}

static void *polling_thread(void *arg) {
    (void)arg;
    log_mgr("Manager polling thread started");

    while (running) {
        /* Check if game is still playing */
        if (!is_game_playing()) {
            log_mgr("Game finished, manager idle");
            usleep(1000000); /* Sleep 1s, keep checking */
            continue;
        }

        /* Check if current player is NPC */
        if (!is_current_player_human()) {
            log_mgr("NPC turn detected, auto-playing...");
            execute_npc_turn();
            /* Brief pause so human can see NPC action */
            usleep(500000); /* 0.5s */
            continue;
        }

        /* Human turn — poll for input */
        poll_history();
        usleep(POLL_INTERVAL);
    }

    log_mgr("Manager polling thread exiting");
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    resolve_root();

    log_mgr("=== Game Manager Started (TPMOS) ===");

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    pthread_t poll_tid;
    if (pthread_create(&poll_tid, NULL, polling_thread, NULL) != 0) {
        log_mgr("ERROR: Failed to create polling thread");
        return 1;
    }

    log_mgr("Polling thread created, waiting for signals...");

    while (running) {
        sleep(1);
    }

    pthread_join(poll_tid, NULL);
    log_mgr("=== Game Manager Stopped ===");

    return 0;
}
