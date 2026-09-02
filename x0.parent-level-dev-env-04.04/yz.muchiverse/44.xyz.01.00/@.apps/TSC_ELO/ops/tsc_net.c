/* tsc_net - host P2P inbox drainer. Companion to the palnet_peer
 * companion process (PAL-NET-STANDARD.txt sec. 0): the peer does 100%
 * of the socket work and appends every received line to this host's
 * net/inbox.txt, sender-prefixed. This op drains that file, one NEW
 * complete line per call, and applies the PvP protocol.
 *
 * Wire line (verbatim, PAL-NET-STANDARD sec. 3 + the app-level MSG
 * shape the house's irc peer already uses):
 *   <sender_node_id>|MSG|<seq>|<game_id>|<user>|<ts>|<action>
 * where action is one of:
 *   CHALLENGE:<name>   - remote offers a duel (name = its player_1)
 *   ACCEPT             - remote confirms the duel; game becomes playing
 *   MOVE:<action>      - remote commits a Mana-Challenge action
 *   RESIGN             - remote concedes
 *
 * Every accepted (deduped) message is recorded into the PER-SESSION
 * game ledger pieces/system/games/<game_id>/ledger.txt, one line per
 * message, format: <seq>|<sender>|<user>|<action>|<via=net>. The
 * acting host records the SAME line with via=local (see tsc_setup), so
 * two subharnesses' ledgers converge to the same ordered action
 * sequence - that is the network proof (PITFALL 21: asserted on
 * per-session files, never the shared config.txt).
 *
 * Dedupe (PAL-NET-STANDARD sec. 4.7): a mesh replays backlog to
 * reconnecting peers, so a message can arrive more than once. The
 * (sender,seq) pair is recorded in net/applied.txt and applied once.
 *
 * Per-session state written:
 *   net/applied.txt         - sender|seq of every applied message
 *   net/opponent.txt        - PvP handshake state (per-session)
 *   net/state.txt           - append-only snapshot of applied remote
 *                             moves (per-session evidence of delivery)
 *   pieces/system/games/<gid>/ledger.txt - converged action record
 *
 * config.txt is mutated ONLY for the handshake (CHALLENGE records the
 * opponent's name; ACCEPT flips game_state to playing). Remote MOVE
 * effects are NOT re-applied to config - it is SHARED between the two
 * local subharnesses and the acting host already advanced it; this op's
 * job for MOVE is the per-session applied record.
 *
 * Self-contained, no shared headers.
 * Usage: tsc_net.+x [max_lines]   (default 8) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 4096
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

static int already_applied(const char *applied_path, const char *sender, const char *seq) {
    FILE *f = fopen(applied_path, "r");
    if (!f) return 0;
    char sender_b[128], seq_b[128];
    snprintf(sender_b, sizeof(sender_b), "%.127s", sender);
    snprintf(seq_b, sizeof(seq_b), "%.127s", seq);
    char key[MAX_LINE];
    snprintf(key, sizeof(key), "%s|%s", sender_b, seq_b);
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, key) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static void record_applied(const char *applied_path, const char *sender, const char *seq) {
    FILE *f = fopen(applied_path, "a");
    if (!f) return;
    fprintf(f, "%.128s|%.128s\n", sender, seq);
    fclose(f);
}

static void game_ledger_append(const char *root, const char *game_id,
                               const char *seq, const char *sender,
                               const char *user, const char *action, const char *via) {
    char gid_b[128], seq_b[128];
    snprintf(gid_b, sizeof(gid_b), "%.127s", game_id);
    snprintf(seq_b, sizeof(seq_b), "%.127s", seq);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/games/%s/ledger.txt", root, gid_b);
    ensure_dir(path);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s|%.128s|%.128s|%.256s|%s\n", seq_b, sender, user, action, via);
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

static void snapshot_state(const char *root, const char *sender, const char *action) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/state.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s|%s\n", sender, action);
    fclose(f);
}

static void ack(const char *root, const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/widget_cmds/status.txt", root);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

/* Splits `s` on '|' into up to max_tok tokens (tokens cannot be empty
 * except the last; a trailing empty token is kept). Returns count. */
static int split_pipe(const char *s, char toks[][MAX_LINE], int max_tok) {
    int n = 0;
    const char *p = s;
    while (n < max_tok) {
        const char *bar = strchr(p, '|');
        if (!bar) {
            snprintf(toks[n], MAX_LINE, "%s", p);
            n++;
            break;
        }
        size_t len = (size_t)(bar - p);
        if (len >= MAX_LINE) len = MAX_LINE - 1;
        memcpy(toks[n], p, len);
        toks[n][len] = '\0';
        n++;
        p = bar + 1;
    }
    return n;
}

int main(int argc, char **argv) {
    resolve_root();

    int max_lines = 8;
    if (argc >= 2) max_lines = atoi(argv[1]);
    if (max_lines < 1) max_lines = 8;

    char inbox_path[PATH_BUF], offset_path[PATH_BUF], applied_path[PATH_BUF];
    char config_path[PATH_BUF];
    snprintf(inbox_path, sizeof(inbox_path), "%s/net/inbox.txt", project_root);
    snprintf(offset_path, sizeof(offset_path), "%s/net/inbox_offset.txt", project_root);
    snprintf(applied_path, sizeof(applied_path), "%s/net/applied.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    FILE *f = fopen(inbox_path, "r");
    if (!f) return 0;

    long offset = 0;
    FILE *of = fopen(offset_path, "r");
    if (of) {
        if (fscanf(of, "%ld", &offset) != 1) offset = 0;
        fclose(of);
    }
    if (fseek(f, offset, SEEK_SET) != 0) {
        offset = 0;
        if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    }

    char lines[16][MAX_LINE];
    int n = 0;
    long consumed = offset;
    char line[MAX_LINE];
    while (n < 16 && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') break;
        line[strcspn(line, "\r\n")] = '\0';
        consumed += (long)len;
        if (line[0] == '\0') continue;
        snprintf(lines[n], MAX_LINE, "%s", line);
        n++;
    }
    fclose(f);

    of = fopen(offset_path, "w");
    if (of) {
        fprintf(of, "%ld\n", consumed);
        fclose(of);
    }

    char game_id[128] = "tsc_elo-001";
    char game_state[32] = "";
    read_kv_str(config_path, "game_id", game_id, sizeof(game_id));
    read_kv_str(config_path, "game_state", game_state, sizeof(game_state));

    for (int li = 0; li < n; li++) {
        /* sender|MSG|seq|game_id|user|ts|action - 7 fields, action is
         * the tail and the last split may be empty (never skip it). */
        char toks[7][MAX_LINE];
        int nt = split_pipe(lines[li], toks, 7);
        if (nt < 7) continue;
        if (strcmp(toks[1], "MSG") != 0) continue;

        const char *sender = toks[0];
        const char *seq = toks[2];
        const char *mgid = toks[3];
        const char *user = toks[4];
        const char *action = toks[6];

        if (already_applied(applied_path, sender, seq)) continue;
        record_applied(applied_path, sender, seq);

        game_ledger_append(project_root, mgid, seq, sender, user, action, "net");

        if (strncmp(action, "CHALLENGE:", 10) == 0) {
            /* The challenger is player_1 (global identity, shared config
             * player_1 = challenger / player_2 = acceptor). Its name
             * arrives in the wire action payload - a REAL net artifact. */
            write_opponent(project_root, action + 10, "pending");
            write_kv(config_path, "player_1_type", "human");
            write_kv(config_path, "player_2_type", "human");
            write_kv(config_path, "player_1_name", action + 10);
            ack(project_root, "DUEL CHALLENGE RECEIVED");
        } else if (strcmp(action, "ACCEPT") == 0) {
            /* The acceptor is player_2; its name is the wire `user`
             * field (PITFALL 21: real wire artifact, not shared-fs). */
            write_opponent(project_root, user, "playing");
            write_kv(config_path, "player_2_name", user);
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
            ack(project_root, "DUEL ACCEPTED - PLAYING");
        } else if (strncmp(action, "MOVE:", 5) == 0) {
            snapshot_state(project_root, sender, action + 5);
        } else if (strcmp(action, "RESIGN") == 0) {
            write_kv(config_path, "game_state", "victory");
            ack(project_root, "OPPONENT RESIGNED");
        }
    }

    (void)game_state;
    return 0;
}
