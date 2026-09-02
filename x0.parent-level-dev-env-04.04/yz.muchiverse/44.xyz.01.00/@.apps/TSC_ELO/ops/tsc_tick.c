/* tsc_tick - the True Swords Clash referee tick, called from
 * pal/main_loop_chtpm.pal every 16667us (101's own tick cadence).
 * Mirrors 101's game_dispatch/game_tick roles:
 *   1. read config.txt
 *   2. if waiting_setup: nothing to do yet (no match started)
 *   3. if not playing: battle over, do nothing
 *   4. current player = (current_turn % num_players) + 1
 *   5. apply that player's status effects at start of their turn
 *      (bleeding -1 HP, poison -2 HP, locust -1 mana, blessing +1 mana),
 *      appending status lines to the game master ledger when nonzero
 *   6. win check (any HP <= 0 -> game_state=victory + ledger win line)
 *   7. if the current player is a computer, dispatch tsc_ai (the AI
 *      widget's brain op) to take its turn
 *
 * Human turn input (Mana Challenge digits / Miracle picks) lands in P3
 * via tsc_input/tsc_miracle - until then the referee waits on the
 * current player's action. This is honest incremental scope, matching
 * the design doc's build order.
 *
 * Self-contained, no shared headers.
 * Usage: tsc_tick.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static void ledger_append(const char *root, int epoch, int turn,
                          const char *player, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%d|%s|%s\n", ts, epoch, player, turn, details, action_type);
    fclose(f);
}

static int run_op(const char *op_path) {
    char resolved[PATH_BUF];
    snprintf(resolved, sizeof(resolved), "%s/%s", project_root, op_path);
    pid_t pid = fork();
    if (pid == 0) {
        setenv("PRISC_PROJECT_ROOT", project_root, 1);
        execl(resolved, resolved, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int main(void) {
    resolve_root();

    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    char game_state[32] = "";
    read_kv_str(config_path, "game_state", game_state, sizeof(game_state));
    if (!game_state[0]) return 0;
    if (strcmp(game_state, "waiting_setup") == 0) return 0;
    if (strcmp(game_state, "playing") != 0) return 0;

    char mode[32] = "HvH";
    read_kv_str(config_path, "mode", mode, sizeof(mode));

    int turn = read_kv_int(config_path, "current_turn", 0);
    int epoch = read_kv_int(config_path, "current_epoch", 1);
    int num_players = read_kv_int(config_path, "num_players", 2);
    if (num_players < 1) num_players = 2;
    int cp = (turn % num_players) + 1;

    char pkey[32], tkey[32], hpkey[32], manakey[32], statkey[32];
    snprintf(pkey, sizeof(pkey), "player_%d_name", cp);
    snprintf(tkey, sizeof(tkey), "player_%d_type", cp);
    snprintf(hpkey, sizeof(hpkey), "player_%d_hp", cp);
    snprintf(manakey, sizeof(manakey), "player_%d_mana", cp);
    snprintf(statkey, sizeof(statkey), "player_%d_status", cp);

    char player_name[64] = "Player1";
    read_kv_str(config_path, pkey, player_name, sizeof(player_name));

    int hp = read_kv_int(config_path, hpkey, 100);
    int mana = read_kv_int(config_path, manakey, 0);

    /* Step 5: apply status effects at the start of the current turn. */
    char status[32] = "none";
    read_kv_str(config_path, statkey, status, sizeof(status));
    if (status[0] && strcmp(status, "none") != 0) {
        int applied = 0;
        char note[96] = "";
        if (strcmp(status, "bleeding") == 0) {
            hp -= 1; applied = 1;
            snprintf(note, sizeof(note), "bleeding:-1");
        } else if (strcmp(status, "poison") == 0) {
            hp -= 2; applied = 1;
            snprintf(note, sizeof(note), "poison:-2");
        } else if (strcmp(status, "locust") == 0) {
            mana -= 1; applied = 1;
            snprintf(note, sizeof(note), "locust:-1");
        } else if (strcmp(status, "blessing") == 0) {
            mana += 1; applied = 1;
            snprintf(note, sizeof(note), "blessing:+1");
        }
        if (applied) {
            if (hp < 0) hp = 0;
            if (mana < 0) mana = 0;
            write_kv_int(config_path, hpkey, hp);
            write_kv_int(config_path, manakey, mana);
            ledger_append(project_root, epoch, turn, player_name,
                          "tick", note);
        }
    }

    /* Step 6: win check. */
    if (hp <= 0) {
        write_kv(config_path, "game_state", "victory");
        char win_note[96];
        snprintf(win_note, sizeof(win_note), "win:%s", player_name);
        ledger_append(project_root, epoch, turn, player_name, "referee", win_note);
        return 0;
    }

    /* Step 7: if the current player is a computer, dispatch the AI. */
    char ptype[16] = "human";
    read_kv_str(config_path, tkey, ptype, sizeof(ptype));
    if (strcmp(ptype, "computer") == 0) {
        run_op("ops/+x/tsc_ai.+x");
    }

    return 0;
}
