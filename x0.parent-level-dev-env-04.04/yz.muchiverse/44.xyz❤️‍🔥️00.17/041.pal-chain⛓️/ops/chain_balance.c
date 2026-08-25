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
 * Usage: chain_balance.+x <wallet_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

/* PAL-CHAIN-STANDARD.txt sec. 3 - concrete v1 constants. */
#define HALVING_PERIOD_BLOCKS 1000
#define INITIAL_REWARD_MILLICONES 10500LL

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static long long reward_for_block(int block_index) {
    int epoch = block_index / HALVING_PERIOD_BLOCKS;
    if (epoch >= 62) return 0;
    return INITIAL_REWARD_MILLICONES >> epoch;
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
            if (strncmp(tx, "TX|", 3) == 0) {
                char tx_copy[MAX_LINE];
                snprintf(tx_copy, sizeof(tx_copy), "%s", tx);
                char *tfields[5];
                char *tc = tx_copy + 3;
                int tnf = 0;
                for (; tnf < 4; tnf++) {
                    char *pipe = strchr(tc, '|');
                    if (!pipe) break;
                    *pipe = '\0';
                    tfields[tnf] = tc;
                    tc = pipe + 1;
                }
                if (tnf == 4) {
                    tfields[4] = tc; /* tx_id, unused here */
                    const char *from = tfields[0];
                    const char *to = tfields[1];
                    long amount = atol(tfields[2]);
                    if (strcmp(from, wallet_id) == 0) *balance -= amount;
                    if (strcmp(to, wallet_id) == 0) *balance += amount;
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
    const char *wallet_id = argv[1];

    char wallet_path[PATH_BUF];
    snprintf(wallet_path, sizeof(wallet_path), "%s/wallets/%s/wallet.txt", project_root, wallet_id);

    char id_check[128];
    read_kv_str(wallet_path, "wallet_id", id_check, sizeof(id_check));
    if (!id_check[0]) {
        fprintf(stderr, "No such wallet.\n");
        return 1;
    }

    long balance = read_kv_long(wallet_path, "cached_balance", 0);
    long last_block = read_kv_long(wallet_path, "last_processed_block", 0);

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
