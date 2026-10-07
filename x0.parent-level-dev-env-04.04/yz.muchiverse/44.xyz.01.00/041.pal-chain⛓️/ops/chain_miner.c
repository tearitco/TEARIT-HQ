/* chain_miner - PERSISTENT proof-of-work miner (matches gl_mirror.c/
 * chtpm_rgb_render.c's own persistent-daemon category), per
 * PAL-CHAIN-STANDARD.txt sec. 3. Real SHA-256 PoW (find a nonce whose
 * block_hash has DIFFICULTY_HEX_ZEROS leading hex-zero characters, a
 * multiple-of-4-bits approximation of "leading zero bits" - simpler to
 * implement correctly than a sub-nibble bitmask check, and the doc's
 * own "calibrate empirically" note already treats the exact bit count
 * as tunable, not load-bearing), paying the CURRENT halving-schedule
 * block reward (sec. 3's own formula) to miner_wallet_id.
 *
 * DIFFICULTY_HEX_ZEROS is overridable via CHAIN_DIFFICULTY_HEX_ZEROS
 * (default 5 = 20 bits, ~1M average tries - fast on any modern CPU;
 * raise this to make blocks take longer, matching a real "block time"
 * once this is tuned against actual observed hash rate, not now).
 *
 * Reads whatever is currently in data/pending_tx.txt as the next
 * block's own transaction set (does NOT dedup against other miners'
 * already-mined tx's beyond what chain_inbox_watcher.c already removed
 * on receipt of a peer's own block - sec. 4's own v1 scope, no fork
 * resolution).
 *
 * Self-contained, no shared headers - block-parsing/reward-schedule
 * logic here is intentionally duplicated from chain_balance.c per this
 * family's own no-shared-headers convention.
 *
 * chain.pdl (optional, <root>/chain.pdl; PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md
 * sec. 3): REWARD initial_millicones/halving_blocks/total_supply_millicones
 * replace the constants above; MINING difficulty_hex_zeros (wins over the env
 * var when the file sets it: the chain defines its own difficulty) and
 * MINING daily_cap_blocks_per_wallet (0 = unlimited). No chain.pdl = exactly
 * the legacy behaviour (constants, env difficulty, no cap, no escrow/faucet).
 *
 * DAILY CAP (sec. 6): before each block the miner counts the blocks already
 * in blockchain.txt whose miner field is this wallet and whose block ts falls
 * in the current UTC day (ts / 86400); at the cap it stops mining (daemon
 * mode idles and keeps status fresh, --blocks mode exits 3).
 *
 * INCLUSION-TIME VALIDATION (sec. 5, a block cannot be un-mined): the miner
 * replays the chain into balances + escrow state, then walks pending_tx.txt IN
 * ORDER over that running state. A pending FAUCET / LOCK / PAYOUT / REFUND that
 * is invalid is NOT included; it is removed from pending_tx.txt and appended
 * (with the reason) to data/rejected_tx.txt (append-only). Rules:
 *   FAUCET  chain.pdl faucet enabled and amount == FAUCET amount_millicones
 *   LOCK    ESCROW enabled, escrow id unused, amount > 0, from != _burn, from's
 *           spendable balance (after earlier lines of this block) >= amount
 *   PAYOUT/REFUND  ESCROW enabled, escrow exists, by == the lock's agent,
 *           amount > 0, paid+refunded+amount <= locked
 *   TX      with chain.pdl present, a TX from `_burn` is not included; any other
 *           TX is included exactly as before (legacy; not balance-checked here)
 * WITHOUT SIGNING the agent/by fields are an HONOR FIELD: anyone who can write
 * pending_tx.txt can claim to be the agent. Escrow is trustworthy on
 * local/test chains only.
 *
 * Usage: chain_miner.+x <miner_wallet_id> [--blocks N]
 *   no flag: persistent daemon (as before). --blocks N: mine at most N blocks
 *   and exit (for tests/scripts). Exit: 0 done, 1 usage, 3 --blocks mode and
 *   stopped early by the daily cap or the supply cap. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define HALVING_PERIOD_BLOCKS 1000
#define INITIAL_REWARD_MILLICONES 10500LL
#define TOTAL_SUPPLY_MILLICONES 21000000LL

static char project_root[MAX_PATH] = ".";
static long g_initial_reward = INITIAL_REWARD_MILLICONES;
static long g_halving_period = HALVING_PERIOD_BLOCKS;
static long long g_total_supply = TOTAL_SUPPLY_MILLICONES;
static int g_pdl_difficulty = 0;      /* from chain.pdl, 0 = not set */
static long g_daily_cap = 0;          /* blocks per wallet per UTC day, 0 = none */
static int g_has_pdl = 0, g_faucet_enabled = 0, g_escrow_enabled = 0;
static long g_faucet_amount = 0;
static char miner_wallet_id[128] = "";
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int difficulty_hex_zeros(void) {
    if (g_pdl_difficulty > 0 && g_pdl_difficulty < 16) return g_pdl_difficulty;
    const char *env = getenv("CHAIN_DIFFICULTY_HEX_ZEROS");
    if (env && env[0]) {
        int v = atoi(env);
        if (v > 0 && v < 16) return v;
    }
    return 5;
}

static long long reward_for_block(long block_index) {
    long epoch = block_index / g_halving_period;
    if (epoch >= 62) return 0;
    return g_initial_reward >> epoch;
}

/* chain.pdl row reader: `SECTION | key | value   # comment`. 1 = found. Duplicated
 * in every op that needs it (house rule: no shared headers). */
static int pdl_get(const char *section, const char *key, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/chain.pdl", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (!found && fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char *c = strstr(line, " #"); if (c) *c = '\0';
        char *a = strchr(line, '|'); if (!a) continue;
        char *b = strchr(a + 1, '|'); if (!b) continue;
        *a = '\0'; *b = '\0';
        char *sec = line, *k = a + 1, *v = b + 1;
        while (*sec == ' ' || *sec == '\t') sec++;
        char *e = sec + strlen(sec); while (e > sec && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
        while (*k == ' ' || *k == '\t') k++;
        e = k + strlen(k); while (e > k && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
        while (*v == ' ' || *v == '\t') v++;
        e = v + strlen(v); while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = '\0';
        if (strcmp(sec, section) == 0 && strcmp(k, key) == 0) { snprintf(out, out_sz, "%s", v); found = 1; }
    }
    fclose(f);
    return found;
}

static void load_chain_pdl(void) {
    char v[128], pp[PATH_BUF];
    snprintf(pp, sizeof(pp), "%s/chain.pdl", project_root);
    g_has_pdl = (access(pp, R_OK) == 0);
    if (!g_has_pdl) return;
    if (pdl_get("REWARD", "initial_millicones", v, sizeof(v)) && atol(v) >= 0) g_initial_reward = atol(v);
    if (pdl_get("REWARD", "halving_blocks", v, sizeof(v)) && atol(v) > 0) g_halving_period = atol(v);
    if (pdl_get("REWARD", "total_supply_millicones", v, sizeof(v)) && atoll(v) >= 0) g_total_supply = atoll(v);
    if (pdl_get("MINING", "difficulty_hex_zeros", v, sizeof(v))) g_pdl_difficulty = atoi(v);
    if (pdl_get("MINING", "daily_cap_blocks_per_wallet", v, sizeof(v)) && atol(v) > 0) g_daily_cap = atol(v);
    if (pdl_get("FAUCET", "enabled", v, sizeof(v))) g_faucet_enabled = atoi(v) == 1;
    if (pdl_get("FAUCET", "amount_millicones", v, sizeof(v))) g_faucet_amount = atol(v);
    if (pdl_get("ESCROW", "enabled", v, sizeof(v))) g_escrow_enabled = atoi(v) == 1;
    if (pdl_get("CHAIN", "kind", v, sizeof(v)) && strcmp(v, "cones") == 0) g_faucet_enabled = 0;   /* cones never has a faucet */
}

/* ---- running state: balances + escrows, from a chain replay and then pending lines ---- */
#define MAX_W 4096
#define MAX_E 4096
static struct { char id[128]; long long bal; } W[MAX_W]; static int nW;
static struct { char id[128]; char agent[128]; long long locked, paid; } E[MAX_E]; static int nE;

static long long *wallet_bal(const char *id) {
    for (int i = 0; i < nW; i++) if (strcmp(W[i].id, id) == 0) return &W[i].bal;
    if (nW >= MAX_W) { static long long dummy; dummy = 0; return &dummy; }
    snprintf(W[nW].id, sizeof(W[nW].id), "%s", id); W[nW].bal = 0;
    return &W[nW++].bal;
}
static int escrow_find(const char *id) {
    for (int i = 0; i < nE; i++) if (strcmp(E[i].id, id) == 0) return i;
    return -1;
}

/* Applies one tx (fields already split, tnf fields). validate=0: replay, just
 * apply. validate=1: refuse an invalid line, writing the reason to why. 1 = ok. */
static int apply_tx(char **tf, int tnf, int validate, char *why, size_t why_sz) {
    why[0] = '\0';
    if (strcmp(tf[0], "TX") == 0 && tnf >= 6) {
        if (validate && g_has_pdl && strcmp(tf[1], "_burn") == 0) { snprintf(why, why_sz, "TX from _burn"); return 0; }
        long long amt = atoll(tf[3]);
        *wallet_bal(tf[1]) -= amt; *wallet_bal(tf[2]) += amt;
    } else if (strcmp(tf[0], "FAUCET") == 0 && tnf >= 5) {
        long long amt = atoll(tf[2]);
        if (validate && !(g_has_pdl && g_faucet_enabled && amt == g_faucet_amount && amt > 0)) { snprintf(why, why_sz, "faucet disabled or wrong amount"); return 0; }
        *wallet_bal(tf[1]) += amt;
    } else if (strcmp(tf[0], "LOCK") == 0 && tnf >= 7) {
        long long amt = atoll(tf[3]);
        if (validate) {
            if (!g_escrow_enabled) { snprintf(why, why_sz, "escrow disabled"); return 0; }
            if (amt <= 0) { snprintf(why, why_sz, "amount must be positive"); return 0; }
            if (!tf[1][0] || !tf[4][0]) { snprintf(why, why_sz, "empty escrow id or agent"); return 0; }
            if (escrow_find(tf[1]) >= 0) { snprintf(why, why_sz, "escrow id already used"); return 0; }
            if (strcmp(tf[2], "_burn") == 0) { snprintf(why, why_sz, "lock from _burn"); return 0; }
            if (*wallet_bal(tf[2]) < amt) { snprintf(why, why_sz, "insufficient spendable balance"); return 0; }
        }
        if (nE < MAX_E) {
            snprintf(E[nE].id, sizeof(E[nE].id), "%s", tf[1]); snprintf(E[nE].agent, sizeof(E[nE].agent), "%s", tf[4]);
            E[nE].locked = amt; E[nE].paid = 0; nE++;
        }
        *wallet_bal(tf[2]) -= amt;
    } else if ((strcmp(tf[0], "PAYOUT") == 0 || strcmp(tf[0], "REFUND") == 0) && tnf >= 7) {
        long long amt = atoll(tf[4]);
        int ei = escrow_find(tf[1]);
        if (validate) {
            if (!g_escrow_enabled) { snprintf(why, why_sz, "escrow disabled"); return 0; }
            if (ei < 0) { snprintf(why, why_sz, "no such escrow"); return 0; }
            if (strcmp(tf[2], E[ei].agent) != 0) { snprintf(why, why_sz, "by is not the escrow agent"); return 0; }
            if (amt <= 0) { snprintf(why, why_sz, "amount must be positive"); return 0; }
            if (E[ei].paid + amt > E[ei].locked) { snprintf(why, why_sz, "exceeds remaining locked"); return 0; }
        }
        if (ei >= 0) E[ei].paid += amt;
        *wallet_bal(tf[3]) += amt;
    }
    return 1;
}

static int split_tx(char *tx, char **tf, int max) {
    int n = 0; char *c = tx;
    while (n < max) { tf[n++] = c; char *p = strchr(c, '|'); if (!p) break; *p = '\0'; c = p + 1; }
    return n;
}

/* REAL PERF FIX, live-caught: the obvious per-byte snprintf("%02x", ...)
 * hex encoding costs ~32 sprintf-family calls per hash attempt - at
 * DIFFICULTY_HEX_ZEROS=5 (~1M average tries/block) that's ~32M snprintf
 * calls just to render hex strings nobody looks at unless the hash
 * happens to win, measured live to slow one block down to several
 * seconds instead of the sub-second this difficulty should take. A
 * direct nibble->hex-char lookup table is the same real SHA-256 output,
 * just encoded without the format-string machinery. */
static const char HEX_CHARS[] = "0123456789abcdef";
static void sha256_hex(const char *input, char out_hex[65]) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), digest);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        out_hex[i * 2] = HEX_CHARS[(digest[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = HEX_CHARS[digest[i] & 0xF];
    }
    out_hex[64] = '\0';
}

static int meets_difficulty(const char *hash_hex, int zeros) {
    for (int i = 0; i < zeros; i++) {
        if (hash_hex[i] != '0') return 0;
    }
    return 1;
}

/* Scans blockchain.txt for the last block's index+hash, and the total
 * cumulative reward paid out so far (sum of reward_for_block() over
 * every BLOCK line's own index - a real, enforced running total, not
 * just a documented target, per sec. 3's own "refuses to mine a new
 * block once TOTAL_SUPPLY_MILLICONES has already been fully paid out"
 * requirement). */
static long g_today_blocks = 0;   /* blocks by this miner in the current UTC day, set by scan_chain */

static void scan_chain(long *last_index, char *last_hash, size_t last_hash_sz, long long *total_minted) {
    nW = 0; nE = 0; g_today_blocks = 0;
    long today = (long)time(NULL) / 86400;
    *last_index = -1;
    snprintf(last_hash, last_hash_sz, "%s", "0000000000000000000000000000000000000000000000000000000000000000");
    *total_minted = 0;

    char chain_path[PATH_BUF];
    snprintf(chain_path, sizeof(chain_path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(chain_path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "BLOCK|", 6) != 0) continue;
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line + 6);
        char *fields[7];
        char *cursor = copy;
        int nf = 0;
        for (; nf < 6; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 6) continue;
        fields[6] = cursor;
        long idx = atol(fields[0]);
        const char *hash = fields[3];
        *total_minted += reward_for_block(idx);
        *wallet_bal(fields[5]) += reward_for_block(idx);
        if (strcmp(fields[5], miner_wallet_id) == 0 && atol(fields[4]) / 86400 == today) g_today_blocks++;
        {
            char *sp = NULL, *tx = strtok_r(fields[6], ";", &sp);
            while (tx) {
                char *tf[10]; char why[64];
                int tnf = split_tx(tx, tf, 10);
                apply_tx(tf, tnf, 0, why, sizeof(why));
                tx = strtok_r(NULL, ";", &sp);
            }
        }
        if (idx > *last_index) {
            *last_index = idx;
            snprintf(last_hash, last_hash_sz, "%s", hash);
        }
    }
    fclose(f);
}

/* Pending lines this block will carry (validated over the running state left by
 * scan_chain), plus the exact set of lines to drop from pending afterwards
 * (included + rejected). Rejected lines are appended to data/rejected_tx.txt. */
#define MAX_PEND 512
static char g_pend[MAX_PEND][MAX_LINE]; static int g_pend_n;
static char g_drop[MAX_PEND][MAX_LINE]; static int g_drop_n;

static void read_pending_tx(char *tx_list, size_t tx_list_sz) {
    tx_list[0] = '\0';
    g_pend_n = 0; g_drop_n = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/pending_tx.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        if (g_pend_n < MAX_PEND) snprintf(g_pend[g_pend_n++], MAX_LINE, "%s", line);
        size_t cur_len = strlen(tx_list);
        size_t add_len = strlen(line) + 2;
        if (cur_len + add_len >= tx_list_sz) break;
        char copy[MAX_LINE], why[64], *tf[10];
        snprintf(copy, sizeof(copy), "%s", line);
        int tnf = split_tx(copy, tf, 10);
        if (!apply_tx(tf, tnf, 1, why, sizeof(why))) {
            char rp[PATH_BUF];
            snprintf(rp, sizeof(rp), "%s/data/rejected_tx.txt", project_root);
            FILE *rf = fopen(rp, "a");
            if (rf) { fprintf(rf, "REJECT|%ld|%s|%s\n", (long)time(NULL), why, line); fclose(rf); }
            if (g_drop_n < MAX_PEND) snprintf(g_drop[g_drop_n++], MAX_LINE, "%s", line);
            continue;
        }
        if (g_drop_n < MAX_PEND) snprintf(g_drop[g_drop_n++], MAX_LINE, "%s", line);
        if (!first) strcat(tx_list, ";");
        strcat(tx_list, line);
        first = 0;
    }
    fclose(f);
}

static void write_status(long blocks_mined, long last_index, const char *last_hash, long long total_minted) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/miner_status.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "miner_wallet_id=%s\n", miner_wallet_id);
    fprintf(f, "running=1\n");
    fprintf(f, "blocks_mined_this_session=%ld\n", blocks_mined);
    fprintf(f, "last_block_index=%ld\n", last_index);
    fprintf(f, "last_block_hash=%s\n", last_hash);
    fprintf(f, "total_supply_minted=%lld\n", total_minted);
    fprintf(f, "updated_at=%ld\n", (long)time(NULL));
    fclose(f);
}

static void clear_status_running(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/miner_status.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fclose(f); }
    /* Best-effort: mark not-running so mining_status.chtpm stops
     * showing this miner as active. */
    FILE *rf = fopen(path, "r");
    char lines[16][MAX_LINE];
    int nlines = 0;
    if (rf) {
        while (nlines < 16 && fgets(lines[nlines], MAX_LINE, rf)) nlines++;
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (wf) {
        for (int i = 0; i < nlines; i++) {
            if (strncmp(lines[i], "running=", 8) == 0) fprintf(wf, "running=0\n");
            else fputs(lines[i], wf);
        }
        fclose(wf);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: chain_miner.+x <miner_wallet_id> [--blocks N]\n");
        return 1;
    }
    resolve_root();
    snprintf(miner_wallet_id, sizeof(miner_wallet_id), "%s", argv[1]);
    long max_blocks = 0; /* 0 = daemon */
    if (argc >= 4 && strcmp(argv[2], "--blocks") == 0) max_blocks = atol(argv[3]);
    load_chain_pdl();
    int zeros = difficulty_hex_zeros();
    int stopped_early = 0;

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/net/miner.pid", project_root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    long blocks_mined = 0;

    while (!g_stop) {
        long last_index;
        char last_hash[80];
        long long total_minted;
        scan_chain(&last_index, last_hash, sizeof(last_hash), &total_minted);

        long next_index = last_index + 1;
        long long reward = reward_for_block(next_index);

        if (max_blocks > 0 && blocks_mined >= max_blocks) break;

        if (g_daily_cap > 0 && g_today_blocks >= g_daily_cap) {
            write_status(blocks_mined, last_index, last_hash, total_minted);
            if (max_blocks > 0) { fprintf(stderr, "daily cap reached (%ld blocks today)\n", g_today_blocks); stopped_early = 1; break; }
            sleep(2);
            continue;
        }

        if (total_minted >= g_total_supply || reward <= 0) {
            write_status(blocks_mined, last_index, last_hash, total_minted);
            if (max_blocks > 0) { fprintf(stderr, "supply cap reached\n"); stopped_early = 1; break; }
            /* Supply cap reached - nothing left to mine. Idle rather
             * than exit, so mining_status.chtpm can keep showing this
             * as a real, informative terminal state. */
            sleep(2);
            continue;
        }

        char tx_list[MAX_LINE];
        read_pending_tx(tx_list, sizeof(tx_list));

        char preimage[MAX_LINE + 256];
        char block_hash[65];
        unsigned long long nonce = 0;
        for (;;) {
            snprintf(preimage, sizeof(preimage), "%ld|%s|%llu|%s", next_index, last_hash, nonce, tx_list);
            sha256_hex(preimage, block_hash);
            if (meets_difficulty(block_hash, zeros)) break;
            nonce++;
            if (nonce % 200000 == 0 && g_stop) break;
        }
        if (g_stop) break;

        char chain_path[PATH_BUF];
        snprintf(chain_path, sizeof(chain_path), "%s/data/blockchain.txt", project_root);
        FILE *cf = fopen(chain_path, "a");
        char block_line[MAX_LINE + 512];
        long ts = (long)time(NULL);
        snprintf(block_line, sizeof(block_line), "BLOCK|%ld|%s|%llu|%s|%ld|%s|%s",
                 next_index, last_hash, nonce, block_hash, ts, miner_wallet_id, tx_list);
        if (cf) { fprintf(cf, "%s\n", block_line); fclose(cf); }

        /* Included and rejected tx's are now settled - remove exactly those
         * lines from pending_tx.txt (exact-line match; lines that arrived
         * meanwhile or did not fit stay). */
        if (g_drop_n > 0) {
            char pending_path[PATH_BUF];
            snprintf(pending_path, sizeof(pending_path), "%s/data/pending_tx.txt", project_root);
            FILE *rf = fopen(pending_path, "r");
            static char remaining[4096][MAX_LINE];
            int nremain = 0;
            if (rf) {
                char l[MAX_LINE];
                while (fgets(l, sizeof(l), rf)) {
                    l[strcspn(l, "\n")] = '\0';
                    if (!l[0]) continue;
                    int drop = 0;
                    for (int i = 0; i < g_drop_n; i++) if (strcmp(g_drop[i], l) == 0) { drop = 1; break; }
                    if (drop) continue;
                    if (nremain < 4096) snprintf(remaining[nremain++], MAX_LINE, "%s", l);
                }
                fclose(rf);
            }
            FILE *wf = fopen(pending_path, "w");
            if (wf) {
                for (int i = 0; i < nremain; i++) fprintf(wf, "%s\n", remaining[i]);
                fclose(wf);
            }
        }

        char outbox_path[PATH_BUF];
        snprintf(outbox_path, sizeof(outbox_path), "%s/net/outbox.txt", project_root);
        {   struct stat ob_st;
            if (stat(outbox_path, &ob_st) == 0 && ob_st.st_size > 2560 * 1024) {
                FILE *zf = fopen(outbox_path, "w");
                if (zf) fclose(zf);
            }
        }
        FILE *of = fopen(outbox_path, "a");
        if (of) { fprintf(of, "%s\n", block_line); fclose(of); }

        blocks_mined++;
        total_minted += reward;
        write_status(blocks_mined, next_index, block_hash, total_minted);
    }

    clear_status_running();
    remove(pid_path);
    return stopped_early ? 3 : 0;
}
