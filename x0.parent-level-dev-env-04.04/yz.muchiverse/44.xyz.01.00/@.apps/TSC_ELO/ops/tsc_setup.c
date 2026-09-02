/* tsc_setup - the host's widget cmd-bus drainer. Called in a background
 * loop by button.sh (drainer pattern from text-editor-xyz's own
 * editor_widget_cmds.+x): reads lines from
 * pieces/system/widget_cmds/inbox.txt (the Match Setup WIDGIT's inbox),
 * applies each to config.txt, then truncates the inbox.
 *
 * Commands (design doc §3 cmd bus + §5 W1):
 *   MATCH:HvH | MATCH:HvC | MATCH:CvC   pick the duel mode
 *   RATING:<n>                          opponent ELO (= difficulty); in
 *                                       CvC it applies to both bots
 *   PLAYER:<name>                       the human's name (player 1)
 *   START                               finalize: set player types by
 *                                       mode, snapshot ratings, reset
 *                                       HP/mana/status, game_state=playing
 *   PING                                liveness probe
 *
 * PvP commands (TSC_P2P_PVP.md, driven over the palnet_peer mesh):
 *   PVP:CHALLENGE                       broadcast CHALLENGE:<name> to
 *                                       net/outbox.txt (the peer relays
 *                                       it; the remote host's tsc_net
 *                                       records it)
 *   PVP:ACCEPT                          accept a received challenge:
 *                                       broadcast ACCEPT, set this
 *                                       host's config to playing
 *                                       (mode=PvP, both human)
 *   MOVE:<strike|heavy|heal|block>      perform a real Mana-Challenge
 *                                       action (PvP only, turn-gated by
 *                                       pieces/system/pvp_role.txt) and
 *                                       broadcast MOVE:<action>
 * The acting host ALSO appends every broadcast to its PER-SESSION game
 * ledger (pieces/system/games/<game_id>/ledger.txt, via=local); the
 * remote host's tsc_net appends the same line with via=net, so both
 * ledgers converge - the network proof (PITFALL 21).
 *
 * On START the host resolves the human's real rating from their xyzfs
 * via tsc_elo (default 1000 for a new player) - the rating number IS
 * the difficulty, and it gets copied into config.txt as a snapshot per
 * design doc §6.
 *
 * Self-contained, no shared headers.
 * Usage: tsc_setup.+x [max_lines]   (default 8, like text-editor-xyz) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

static int resolve_human_rating(const char *name) {
    /* Ask tsc_elo for the player's real xyzfs rating (default 1000). */
    char cmd[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' %s/ops/+x/tsc_elo.+x get '%s'",
             project_root, project_root, name);
#pragma GCC diagnostic pop
    FILE *p = popen(cmd, "r");
    if (!p) return 1000;
    char buf[64] = "1000";
    if (fgets(buf, sizeof(buf), p)) buf[strcspn(buf, "\r\n")] = '\0';
    pclose(p);
    int r = atoi(buf);
    return r > 0 ? r : 1000;
}

static void ack(const char *root, const char *msg) {
    char status_path[PATH_BUF];
    snprintf(status_path, sizeof(status_path), "%s/pieces/system/widget_cmds/status.txt", root);
    FILE *f = fopen(status_path, "w");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

static void ensure_dir(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *last = strrchr(tmp, '/');
    if (!last) return;
    *last = '\0';
    if (!tmp[0]) return;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
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
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int fallback) {
    char v[64];
    read_kv_str(path, key, v, sizeof(v));
    if (!v[0]) return fallback;
    return atoi(v);
}

/* Broadcast one app action on the wire: appends
 * MSG|<seq>|<game_id>|<user>|<ts>|<action> to net/outbox.txt (the
 * palnet_peer companion process relays it to every peer). seq is a
 * per-host monotonically increasing counter persisted in net/seq.txt
 * (PAL-NET-STANDARD sec. 0.4: outbox is append-only, one line per
 * event). Returns the seq via `out_seq` for the local ledger record. */
static void broadcast_action(const char *root, const char *game_id,
                             const char *user, const char *action,
                             char *out_seq, size_t out_seq_sz) {
    char seq_path[PATH_BUF];
    snprintf(seq_path, sizeof(seq_path), "%s/net/seq.txt", root);
    long counter = 0;
    FILE *sf = fopen(seq_path, "r");
    if (sf) {
        if (fscanf(sf, "%ld", &counter) != 1) counter = 0;
        fclose(sf);
    }
    counter++;
    sf = fopen(seq_path, "w");
    if (sf) { fprintf(sf, "%ld\n", counter); fclose(sf); }

    char seq[128];
    snprintf(seq, sizeof(seq), "%ld-%d-%ld", (long)time(NULL), (int)getpid(), counter);

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/net/outbox.txt", root);
    FILE *f = fopen(out_path, "a");
    if (f) {
        char ts[64];
        time_t now = time(NULL);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
        fprintf(f, "MSG|%s|%s|%s|%s|%s\n", seq, game_id, user, ts, action);
        fclose(f);
    }
    if (out_seq) snprintf(out_seq, out_seq_sz, "%s", seq);
}

/* Per-session game ledger: the acting host records every broadcast with
 * via=local (this op) and the remote host records the same line with
 * via=net (tsc_net), so both subharnesses' ledgers converge to the same
 * ordered action sequence - the network proof. */
static void game_ledger_append(const char *root, const char *game_id,
                               const char *seq, const char *sender,
                               const char *user, const char *action,
                               const char *via) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/games/%s/ledger.txt", root, game_id);
    ensure_dir(path);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s|%s|%s|%s|%s\n", seq, sender, user, action, via);
    fclose(f);
}

static void write_opponent(const char *root, const char *opponent,
                           const char *state) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/opponent.txt", root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "opponent=%s\nstate=%s\n", opponent, state);
    fclose(f);
}

static void read_role(const char *root, char *role, size_t role_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/pvp_role.txt", root);
    read_kv_str(path, "role", role, role_sz);
    if (!role[0]) snprintf(role, role_sz, "player_1");
}

static void write_role(const char *root, const char *role) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/pvp_role.txt", root);
    write_kv(path, "role", role);
}

/* Duplicates tsc_deal's effect math by design (the family does not
 * share headers): applies a Mana-Challenge action for player index cp
 * (1 or 2), mutating config in place and returning 1 if the action was
 * performed (turn should advance). Local PvP moves take this path;
 * tsc_deal stays the host-keyboard path. */
static int pvp_apply_move(const char *config_path, int cp, const char *action,
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
        if (*actor_hp > 100) *actor_hp = 100;
    } else if (strcmp(use_action, "block") == 0) {
        *actor_mana -= 1;
        snprintf(actor_status, 32, "block");
    } else {
        return 0;
    }

    *actor_mana += 1;
    if (*actor_mana > 10) *actor_mana = 10;

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

/* The widget's MATCH/RATING/PLAYER/START commands arrive in SEPARATE
 * enqueue calls (each widget keypress enqueues one line), but the
 * drainer tick (every 0.2s) may split them across invocations. pending_*
 * must therefore survive between calls: persist them to a small state
 * file that each invocation re-reads, so START always finalizes with the
 * FULLY accumulated setup (live-test: MATCH landed in an earlier tick
 * than START, silently leaving mode=HvH and rating=1000). */
static void read_pending(const char *pending_path, char *mode, size_t mode_sz,
                         int *rating, char *name, size_t name_sz) {
    FILE *f = fopen(pending_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "mode=", 5) == 0) {
            char *v = line + 5;
            v[strcspn(v, "\r\n")] = '\0';
            if (v[0]) snprintf(mode, mode_sz, "%s", v);
        } else if (strncmp(line, "rating=", 7) == 0) {
            *rating = atoi(line + 7);
        } else if (strncmp(line, "name=", 5) == 0) {
            char *v = line + 5;
            v[strcspn(v, "\r\n")] = '\0';
            if (v[0]) snprintf(name, name_sz, "%s", v);
        }
    }
    fclose(f);
}

static void write_pending(const char *pending_path, const char *mode,
                          int rating, const char *name) {
    FILE *f = fopen(pending_path, "w");
    if (!f) return;
    fprintf(f, "mode=%s\nrating=%d\nname=%s\n", mode, rating, name);
    fclose(f);
}

int main(int argc, char **argv) {
    resolve_root();

    int max_lines = 8;
    if (argc >= 2) max_lines = atoi(argv[1]);
    if (max_lines < 1) max_lines = 8;

    char config_path[PATH_BUF], inbox_path[PATH_BUF], pending_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(inbox_path, sizeof(inbox_path), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    snprintf(pending_path, sizeof(pending_path), "%s/pieces/system/widget_cmds/pending.txt", project_root);

    FILE *f = fopen(inbox_path, "r");
    if (!f) return 0;
    char buf[MAX_LINE * 16];
    int used = 0;
    buf[0] = '\0';
    char line[MAX_LINE];
    int lines_read = 0;
    while (lines_read < max_lines && fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        if (used + len < (int)sizeof(buf)) {
            memcpy(buf + used, line, (size_t)len);
            used += len;
        }
        lines_read++;
    }
    fclose(f);

    if (used == 0) return 0;

    /* Truncate the inbox now that we've buffered everything. */
    FILE *trunc = fopen(inbox_path, "w");
    if (trunc) fclose(trunc);

    char *p = buf;
    char pending_mode[MAX_LINE] = "HvH";
    int pending_rating = -1;
    char pending_player[MAX_LINE] = "Player1";

    read_pending(pending_path, pending_mode, sizeof(pending_mode),
                 &pending_rating, pending_player, sizeof(pending_player));

    while (*p && lines_read > 0) {
        char cmd[MAX_LINE];
        int i = 0;
        while (*p && *p != '\n' && i < (int)sizeof(cmd) - 1) cmd[i++] = *p++;
        cmd[i] = '\0';
        if (*p == '\n') p++;
        if (i == 0) continue;

        if (strncmp(cmd, "MATCH:", 6) == 0) {
            snprintf(pending_mode, sizeof(pending_mode), "%s", cmd + 6);
            write_pending(pending_path, pending_mode, pending_rating, pending_player);
        } else if (strncmp(cmd, "RATING:", 7) == 0) {
            int r = atoi(cmd + 7);
            if (r > 0) pending_rating = r;
            write_pending(pending_path, pending_mode, pending_rating, pending_player);
        } else if (strncmp(cmd, "PLAYER:", 7) == 0) {
            if (cmd[7]) snprintf(pending_player, sizeof(pending_player), "%s", cmd + 7);
            write_pending(pending_path, pending_mode, pending_rating, pending_player);
        } else if (strcmp(cmd, "START") == 0) {
            write_kv(config_path, "mode", pending_mode);
            write_kv(config_path, "game_state", "playing");
            write_kv_int(config_path, "current_epoch", 1);
            write_kv_int(config_path, "current_turn", 0);
            write_kv_int(config_path, "num_players", 2);

            int opp_rating = (pending_rating > 0) ? pending_rating : 1000;
            int human_rating = resolve_human_rating(pending_player);

            write_kv(config_path, "player_1_name", pending_player);
            write_kv_int(config_path, "player_1_rating", human_rating);
            write_kv_int(config_path, "player_1_hp", 100);
            write_kv_int(config_path, "player_1_mana", 0);
            write_kv(config_path, "player_1_status", "none");

            if (strcmp(pending_mode, "HvC") == 0 || strcmp(pending_mode, "CvC") == 0) {
                write_kv(config_path, "player_1_type", "human");
                write_kv(config_path, "player_2_type", "computer");
                write_kv(config_path, "player_2_name", "SKYNET");
            }
            if (strcmp(pending_mode, "HvH") == 0) {
                write_kv(config_path, "player_1_type", "human");
                write_kv(config_path, "player_2_type", "human");
                write_kv(config_path, "player_2_name", "Player2");
            }
            if (strcmp(pending_mode, "CvC") == 0) {
                write_kv(config_path, "player_1_type", "computer");
                write_kv(config_path, "player_2_type", "computer");
                write_kv(config_path, "player_1_name", "TERMINATOR");
                write_kv(config_path, "player_2_name", "SKYNET");
            }

            write_kv_int(config_path, "player_2_rating", opp_rating);
            write_kv_int(config_path, "player_2_hp", 100);
            write_kv_int(config_path, "player_2_mana", 0);
            write_kv(config_path, "player_2_status", "none");
            if (strcmp(pending_mode, "CvC") == 0) {
                write_kv_int(config_path, "player_1_rating", opp_rating);
            }

            char note[128];
            snprintf(note, sizeof(note), "setup:%s:%d:%d", pending_mode, human_rating, opp_rating);
            ledger_append(project_root, 1, 0, "referee", "setup", note);

            char msg[256];
            snprintf(msg, sizeof(msg),
                     "MATCH STARTED: %s (you %d vs %d). Player 1: %s. Get ready!",
                     pending_mode, human_rating, opp_rating, pending_player);
            ack(project_root, msg);
            write_pending(pending_path, "HvH", -1, "Player1");
        } else if (strcmp(cmd, "PING") == 0) {
            ack(project_root, "PONG host alive");
        } else if (strcmp(cmd, "PVP:CHALLENGE") == 0) {
            char gid[128] = "tsc_elo-001";
            read_kv_str(config_path, "game_id", gid, sizeof(gid));
            /* The challenger IS player_1 in the shared config; its own
             * name lives in this host's pending.txt (per-session). */
            char act[192];
            snprintf(act, sizeof(act), "CHALLENGE:%s", pending_player);
            char seq[128];
            broadcast_action(project_root, gid, pending_player, act, seq, sizeof(seq));
            game_ledger_append(project_root, gid, seq, "local", pending_player, act, "local");
            write_kv(config_path, "player_1_name", pending_player);
            write_role(project_root, "player_1");
            write_opponent(project_root, pending_player, "challenging");
            ack(project_root, "PVP CHALLENGE SENT");
        } else if (strcmp(cmd, "PVP:ACCEPT") == 0) {
            char gid[128] = "tsc_elo-001";
            read_kv_str(config_path, "game_id", gid, sizeof(gid));
            /* The acceptor IS player_2 in the shared config; its own
             * name is this host's pending_player (per-session). */
            char act[64] = "ACCEPT";
            char seq[128];
            broadcast_action(project_root, gid, pending_player, act, seq, sizeof(seq));
            game_ledger_append(project_root, gid, seq, "local", pending_player, act, "local");
            write_kv(config_path, "player_2_name", pending_player);
            write_kv(config_path, "mode", "PvP");
            write_kv(config_path, "game_state", "playing");
            write_kv_int(config_path, "current_epoch", 1);
            write_kv_int(config_path, "current_turn", 0);
            write_kv_int(config_path, "num_players", 2);
            write_kv(config_path, "player_1_type", "human");
            write_kv(config_path, "player_2_type", "human");
            write_kv_int(config_path, "player_1_hp", 100);
            write_kv_int(config_path, "player_1_mana", 0);
            write_kv(config_path, "player_1_status", "none");
            write_kv_int(config_path, "player_2_hp", 100);
            write_kv_int(config_path, "player_2_mana", 0);
            write_kv(config_path, "player_2_status", "none");
            write_kv_int(config_path, "player_2_rating", 1000);
            write_role(project_root, "player_2");
            char opp[128] = "Player1";
            read_kv_str(config_path, "player_1_name", opp, sizeof(opp));
            write_opponent(project_root, opp, "playing");
            ack(project_root, "PVP ACCEPTED - PLAYING");
        } else if (strncmp(cmd, "MOVE:", 5) == 0) {
            char mode[32] = "", gstate[32] = "", gid[128] = "tsc_elo-001";
            read_kv_str(config_path, "mode", mode, sizeof(mode));
            read_kv_str(config_path, "game_state", gstate, sizeof(gstate));
            read_kv_str(config_path, "game_id", gid, sizeof(gid));
            if (strcmp(mode, "PvP") == 0 && strcmp(gstate, "playing") == 0) {
                int turn = read_kv_int(config_path, "current_turn", 0);
                int num_players = read_kv_int(config_path, "num_players", 2);
                if (num_players < 1) num_players = 2;
                int cp = (turn % num_players) + 1;
                char role[32] = "player_1";
                read_role(project_root, role, sizeof(role));
                int my_cp = (strcmp(role, "player_2") == 0) ? 2 : 1;

                if (cp == my_cp) {
                    char hpkey[32], manakey[32], statkey[32];
                    char opphpkey[32], oppstatkey[32];
                    snprintf(hpkey, sizeof(hpkey), "player_%d_hp", cp);
                    snprintf(manakey, sizeof(manakey), "player_%d_mana", cp);
                    snprintf(statkey, sizeof(statkey), "player_%d_status", cp);
                    snprintf(opphpkey, sizeof(opphpkey), "player_%d_hp", 3 - cp);
                    snprintf(oppstatkey, sizeof(oppstatkey), "player_%d_status", 3 - cp);

                    char actor_status[32] = "none", opp_status[32] = "none";
                    read_kv_str(config_path, statkey, actor_status, sizeof(actor_status));
                    read_kv_str(config_path, oppstatkey, opp_status, sizeof(opp_status));
                    int actor_hp = read_kv_int(config_path, hpkey, 100);
                    int actor_mana = read_kv_int(config_path, manakey, 0);
                    int opp_hp = read_kv_int(config_path, opphpkey, 100);

                    const char *raw = cmd + 5;
                    char action[64] = "";
                    if (strcmp(raw, "strike") == 0 || strcmp(raw, "heavy") == 0 ||
                        strcmp(raw, "heal") == 0 || strcmp(raw, "block") == 0) {
                        snprintf(action, sizeof(action), "%.63s", raw);
                    }

                    if (pvp_apply_move(config_path, cp, action, &actor_hp,
                                       &actor_mana, actor_status, &opp_hp, opp_status)) {
                        write_kv_int(config_path, "current_turn", turn + 1);

                        char pkey[32], pname[128] = "Player1";
                        snprintf(pkey, sizeof(pkey), "player_%d_name", cp);
                        read_kv_str(config_path, pkey, pname, sizeof(pname));

                        char wire[64];
                        snprintf(wire, sizeof(wire), "MOVE:%s", action);
                        char seq[128];
                        broadcast_action(project_root, gid, pname, wire, seq, sizeof(seq));
                        game_ledger_append(project_root, gid, seq, "local", pname,
                                           wire, "local");
                        ack(project_root, "MOVE PLAYED");
                    }
                } else {
                    ack(project_root, "NOT YOUR TURN");
                }
            }
        }
    }

    return 0;
}
