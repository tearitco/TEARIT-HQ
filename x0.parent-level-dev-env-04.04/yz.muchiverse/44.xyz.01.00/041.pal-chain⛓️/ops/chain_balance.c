/* chain_balance - derives a wallet's current balance by replaying
 * data/blockchain.txt from wallet.txt's own last_processed_block
 * onward (PAL-CHAIN-STANDARD.txt sec. 3), NOT by trusting
 * cached_balance blindly - cached_balance/last_processed_block exist
 * purely as an optimization so a long-lived chain doesn't need a full
 * replay from block 0 on every check, always re-derivable from scratch
 * if ever suspected of drifting (reset last_processed_block=0,
 * cached_balance=0 in wallet.txt to force a full replay).
 *
 * Self-contained, no shared headers - the reward-schedule/block-parsing
 * logic here is intentionally duplicated in chain_miner.c and
 * chain_inbox_watcher.c rather than factored into a shared header, per
 * this family's own no-shared-headers convention.
 *
 * chain.pdl (optional, <root>/chain.pdl, PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md
 * sec. 3): REWARD initial_millicones / halving_blocks override the constants
 * above; a root without chain.pdl uses exactly the constants above (legacy).
 *
 * Transaction lines replayed (sec. 4/5), each a +/- on ONE wallet so the
 * per-wallet cached_balance/last_processed_block optimization stays valid
 * (validity of escrow lines is enforced at ISSUE time by chain_escrow and at
 * INCLUSION time by chain_miner; a replay just applies what the chain holds):
 *   TX    |from|to|amount|ts|id                       from -= , to +=
 *   FAUCET|wallet|amount|ts|id                        wallet +=   (mint)
 *   LOCK  |escrow|from|amount|agent|ts|id             from -=     (moved into escrow)
 *   PAYOUT|escrow|by|to|amount|ts|id                  to +=
 *   REFUND|escrow|by|to|amount|ts|id                  to +=
 * The balance printed is therefore the SPENDABLE balance (locked excluded).
 * Wallet "_burn" (the supply sink) needs no wallet.txt: it is replayed in
 * full each time and never cached.
 *
 * Usage: chain_balance.+x <wallet_id>
 * Exit: 0 balance printed on stdout, 1 usage / no such wallet. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

/* PAL-CHAIN-STANDARD.txt sec. 3 - concrete v1 constants. */
#define HALVING_PERIOD_BLOCKS 1000
#define INITIAL_REWARD_MILLICONES 10500LL

static char project_root[MAX_PATH] = ".";
static long g_initial_reward = INITIAL_REWARD_MILLICONES;
static long g_halving_period = HALVING_PERIOD_BLOCKS;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static long long reward_for_block(int block_index) {
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
    char v[128];
    if (pdl_get("REWARD", "initial_millicones", v, sizeof(v)) && atol(v) >= 0) g_initial_reward = atol(v);
    if (pdl_get("REWARD", "halving_blocks", v, sizeof(v)) && atol(v) > 0) g_halving_period = atol(v);
}

static long read_kv_long(const char *path, const char *key, long def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    long val = def;
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            val = atol(line + key_len + 1);
            break;
        }
    }
    fclose(f);
    return val;
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
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

/* Applies every BLOCK line in blockchain.txt with block_index >
 * from_block (exclusive) to *balance, updating *last_block to the
 * highest block_index seen. Field layout (sec. 2):
 *   BLOCK|<index>|<prev_hash>|<nonce>|<hash>|<timestamp>|<miner>|<tx_list>
 * tx_list is semicolon-separated TX entries, each itself pipe-delimited
 * (TX|<from>|<to>|<amount>|<timestamp>|<tx_id>) - tx_list is taken
 * verbatim as everything after the 7th '|', so embedded '|' inside each
 * TX entry doesn't get mis-split against BLOCK's own fields. */
static void apply_chain(const char *wallet_id, long from_block, long *balance, long *last_block) {
    char chain_path[PATH_BUF];
    snprintf(chain_path, sizeof(chain_path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(chain_path, "r");
    if (!f) return;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "BLOCK|", 6) != 0) continue;

        char *fields[7];
        char *cursor = line + 6;
        int nf = 0;
        for (; nf < 6; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 6) continue;
        fields[6] = cursor; /* remaining tx_list, verbatim */

        long block_index = atol(fields[0]);
        const char *miner = fields[5];
        if (block_index <= from_block) continue;

        if (strcmp(miner, wallet_id) == 0) {
            *balance += reward_for_block((int)block_index);
        }

        char tx_list[MAX_LINE];
        snprintf(tx_list, sizeof(tx_list), "%s", fields[6]);
        char *saveptr = NULL;
        char *tx = strtok_r(tx_list, ";", &saveptr);
        while (tx) {
            {
                char tx_copy[MAX_LINE];
                snprintf(tx_copy, sizeof(tx_copy), "%s", tx);
                char *tf[10];
                int tnf = 0;
                char *tc = tx_copy;
                while (tnf < 10) {
                    tf[tnf++] = tc;
                    char *pipe = strchr(tc, '|');
                    if (!pipe) break;
                    *pipe = '\0';
                    tc = pipe + 1;
                }
                if (strcmp(tf[0], "TX") == 0 && tnf >= 6) {
                    long amount = atol(tf[3]);
                    if (strcmp(tf[1], wallet_id) == 0) *balance -= amount;
                    if (strcmp(tf[2], wallet_id) == 0) *balance += amount;
                } else if (strcmp(tf[0], "FAUCET") == 0 && tnf >= 5) {
                    if (strcmp(tf[1], wallet_id) == 0) *balance += atol(tf[2]);
                } else if (strcmp(tf[0], "LOCK") == 0 && tnf >= 7) {
                    if (strcmp(tf[2], wallet_id) == 0) *balance -= atol(tf[3]);
                } else if ((strcmp(tf[0], "PAYOUT") == 0 || strcmp(tf[0], "REFUND") == 0) && tnf >= 7) {
                    if (strcmp(tf[3], wallet_id) == 0) *balance += atol(tf[4]);
                }
            }
            tx = strtok_r(NULL, ";", &saveptr);
        }

        if (block_index > *last_block) *last_block = block_index;
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: chain_balance.+x <wallet_id>\n");
        return 1;
    }
    resolve_root();
    load_chain_pdl();
    const char *wallet_id = argv[1];

    char wallet_path[PATH_BUF];
    snprintf(wallet_path, sizeof(wallet_path), "%s/wallets/%s/wallet.txt", project_root, wallet_id);

    if (strcmp(wallet_id, "_burn") == 0) {
        long b = 0, lb = 0;
        apply_chain(wallet_id, -1, &b, &lb);
        printf("%ld\n", b);
        return 0;
    }

    char id_check[128];
    read_kv_str(wallet_path, "wallet_id", id_check, sizeof(id_check));
    if (!id_check[0]) {
        fprintf(stderr, "No such wallet.\n");
        return 1;
    }

    long balance = read_kv_long(wallet_path, "cached_balance", 0);
    long last_block = read_kv_long(wallet_path, "last_processed_block", 0);
    /* LEGACY QUIRK, kept as-is on a root without chain.pdl: a fresh wallet has
     * last_processed_block=0 and apply_chain skips index <= that, so BLOCK 0
     * (its reward and txs) is never counted. On a chain.pdl chain, "block 0
     * done" with a zero balance is indistinguishable from "nothing done", and
     * replaying block 0 again in that state is idempotent, so start at -1. */
    int has_pdl = 0;
    { char pp[PATH_BUF]; snprintf(pp, sizeof(pp), "%s/chain.pdl", project_root); has_pdl = (access(pp, R_OK) == 0); }
    if (has_pdl && last_block == 0 && balance == 0) last_block = -1;

    apply_chain(wallet_id, last_block, &balance, &last_block);

    FILE *f = fopen(wallet_path, "r");
    char lines[16][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 16 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    f = fopen(wallet_path, "w");
    if (f) {
        for (int i = 0; i < nlines; i++) {
            if (strncmp(lines[i], "cached_balance=", 15) == 0) {
                fprintf(f, "cached_balance=%ld\n", balance);
            } else if (strncmp(lines[i], "last_processed_block=", 21) == 0) {
                fprintf(f, "last_processed_block=%ld\n", last_block);
            } else {
                fputs(lines[i], f);
            }
        }
        fclose(f);
    }

    printf("%ld\n", balance);
    return 0;
}
