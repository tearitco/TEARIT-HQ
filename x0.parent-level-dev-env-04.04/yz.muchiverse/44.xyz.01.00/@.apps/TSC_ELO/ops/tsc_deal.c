/* tsc_deal - apply the current human player's Mana Challenge action.
 * Called from pal/main_loop_chtpm.pal every tick; consumes the handoff
 * file written by tsc_input (pieces/system/player_action.txt) and is a
 * strict no-op when there is nothing to consume, the match isn't
 * playing, or it isn't the human's turn. Computer turns are handled by
 * tsc_ai (dispatched from tsc_tick), so in CvC this op never fires.
 *
 * Effect math (duplicated in tsc_ai.c by design - the family does not
 * share headers): each action grants the actor +1 mana (cap 10);
 * costs: strike 0, heavy 2, heal 2, block 1; heavy deals 6, strike 3;
 * heal restores 5 (HP cap 100); block sets status=block which absorbs
 * 3 damage from the next hit (then clears). A target with
 * status=block halves... no - absorbs a flat 3, floored at 0.
 *
 * Each performed action appends a "deal:<action>" line to the game
 * master ledger (the durable record) and advances current_turn, which
 * flips the current player to the opponent ((turn % num_players) + 1).
 *
 * Self-contained, no shared headers.
 * Usage: tsc_deal.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define HP_CAP 100
#define MANA_CAP 10

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(l, sizeof(l), f)) {
        if (strncmp(l, key, key_len) == 0 && l[key_len] == '=') {
            char *v = l + key_len + 1;
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

/* Applies the effect of `action` for the actor at player index `cp` (1 or 2).
 * Returns 1 if the action was performed (turn should advance), else 0.
 * Must be given read/written keys. Kept local so tsc_ai can't share it. */
static int perform_action(const char *config_path, int cp, const char *action,
                          int *actor_hp, int *actor_mana, char *actor_status,
                          int *opp_hp, char *opp_status) {
    const char *use_action = action;
    int dmg = 0;

    if (strcmp(action, "heavy") == 0 && *actor_mana < 2) use_action = "strike";
    if (strcmp(action, "heal") == 0 && *actor_mana < 2) use_action = "strike";
    if (strcmp(action, "block") == 0 && *actor_mana < 1) use_action = "strike";

    if (strcmp(use_action, "strike") == 0) {
        dmg = 3;
    } else if (strcmp(use_action, "heavy") == 0) {
        *actor_mana -= 2;
        dmg = 6;
    } else if (strcmp(use_action, "heal") == 0) {
        *actor_mana -= 2;
        *actor_hp += 5;
        if (*actor_hp > HP_CAP) *actor_hp = HP_CAP;
    } else if (strcmp(use_action, "block") == 0) {
        *actor_mana -= 1;
        snprintf(actor_status, 32, "block");
    } else {
        return 0;
    }

    *actor_mana += 1;
    if (*actor_mana > MANA_CAP) *actor_mana = MANA_CAP;

    if (dmg > 0) {
        if (strcmp(opp_status, "block") == 0) {
            dmg -= 3;
            snprintf(opp_status, 32, "none");
        }
        if (dmg < 0) dmg = 0;
        *opp_hp -= dmg;
        if (*opp_hp < 0) *opp_hp = 0;
    }

    char hpkey[32], manakey[32], statkey[32], opphpkey[32], oppstatkey[32];
    snprintf(hpkey, sizeof(hpkey), "player_%d_hp", cp);
    snprintf(manakey, sizeof(manakey), "player_%d_mana", cp);
    snprintf(statkey, sizeof(statkey), "player_%d_status", cp);
    snprintf(opphpkey, sizeof(opphpkey), "player_%d_hp", 3 - cp);
    snprintf(oppstatkey, sizeof(oppstatkey), "player_%d_status", 3 - cp);
    write_kv_int(config_path, hpkey, *actor_hp);
    write_kv_int(config_path, manakey, *actor_mana);
    write_kv(config_path, statkey, actor_status);
    write_kv_int(config_path, opphpkey, *opp_hp);
    write_kv(config_path, oppstatkey, opp_status);
    return 1;
}

int main(void) {
    resolve_root();

    char config_path[PATH_BUF], handoff_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(handoff_path, sizeof(handoff_path), "%s/pieces/system/player_action.txt", project_root);

    FILE *hf = fopen(handoff_path, "r");
    if (!hf) return 0;
    char action[MAX_LINE] = "";
    if (!fgets(action, sizeof(action), hf)) {
        fclose(hf);
        return 0;
    }
    fclose(hf);
    action[strcspn(action, "\r\n")] = '\0';
    if (!action[0]) return 0;

    /* Consume the handoff regardless of validity - stale actions must
     * not sit around and get applied on a later, wrong turn. */
    FILE *cf = fopen(handoff_path, "w");
    if (cf) fclose(cf);

    char game_state[32] = "";
    read_kv_str(config_path, "game_state", game_state, sizeof(game_state));
    if (strcmp(game_state, "playing") != 0) return 0;

    int turn = read_kv_int(config_path, "current_turn", 0);
    int epoch = read_kv_int(config_path, "current_epoch", 1);
    int num_players = read_kv_int(config_path, "num_players", 2);
    if (num_players < 1) num_players = 2;
    int cp = (turn % num_players) + 1;

    char tkey[32];
    snprintf(tkey, sizeof(tkey), "player_%d_type", cp);
    char ptype[16] = "human";
    read_kv_str(config_path, tkey, ptype, sizeof(ptype));
    if (strcmp(ptype, "human") != 0) return 0;

    char pkey[32], hpkey[32], manakey[32], statkey[32];
    snprintf(pkey, sizeof(pkey), "player_%d_name", cp);
    snprintf(hpkey, sizeof(hpkey), "player_%d_hp", cp);
    snprintf(manakey, sizeof(manakey), "player_%d_mana", cp);
    snprintf(statkey, sizeof(statkey), "player_%d_status", cp);

    char player_name[64] = "Player1";
    read_kv_str(config_path, pkey, player_name, sizeof(player_name));
    char actor_status[32] = "none";
    read_kv_str(config_path, statkey, actor_status, sizeof(actor_status));

    int actor_hp = read_kv_int(config_path, hpkey, 100);
    int actor_mana = read_kv_int(config_path, manakey, 0);

    int opp_cp = 3 - cp;
    char opphpkey[32], oppstatkey[32];
    snprintf(opphpkey, sizeof(opphpkey), "player_%d_hp", opp_cp);
    snprintf(oppstatkey, sizeof(oppstatkey), "player_%d_status", opp_cp);
    char opp_status[32] = "none";
    read_kv_str(config_path, oppstatkey, opp_status, sizeof(opp_status));
    int opp_hp = read_kv_int(config_path, opphpkey, 100);

    if (!perform_action(config_path, cp, action, &actor_hp, &actor_mana,
                        actor_status, &opp_hp, opp_status)) {
        return 0;
    }

    char details[128];
    snprintf(details, sizeof(details), "action:%s:hp:%d:mana:%d:opphp:%d",
             action, actor_hp, actor_mana, opp_hp);
    ledger_append(project_root, epoch, turn, player_name, "deal", details);

    write_kv_int(config_path, "current_turn", turn + 1);
    return 0;
}
