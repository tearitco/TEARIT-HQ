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
 * Usage: chain_miner.+x <miner_wallet_id> */
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
static char miner_wallet_id[128] = "";
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int difficulty_hex_zeros(void) {
    const char *env = getenv("CHAIN_DIFFICULTY_HEX_ZEROS");
    if (env && env[0]) {
        int v = atoi(env);
        if (v > 0 && v < 16) return v;
    }
    return 5;
}

static long long reward_for_block(long block_index) {
    long epoch = block_index / HALVING_PERIOD_BLOCKS;
    if (epoch >= 62) return 0;
    return INITIAL_REWARD_MILLICONES >> epoch;
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
static void scan_chain(long *last_index, char *last_hash, size_t last_hash_sz, long long *total_minted) {
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
        char *fields[6];
        char *cursor = copy;
        int nf = 0;
        for (; nf < 5; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 5) continue;
        long idx = atol(fields[0]);
        const char *hash = fields[3];
        *total_minted += reward_for_block(idx);
        if (idx > *last_index) {
            *last_index = idx;
            snprintf(last_hash, last_hash_sz, "%s", hash);
        }
    }
    fclose(f);
}

static void read_pending_tx(char *tx_list, size_t tx_list_sz) {
    tx_list[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/pending_tx.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        size_t cur_len = strlen(tx_list);
        size_t add_len = strlen(line) + 2;
        if (cur_len + add_len >= tx_list_sz) break;
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
        fprintf(stderr, "Usage: chain_miner.+x <miner_wallet_id>\n");
        return 1;
    }
    resolve_root();
    snprintf(miner_wallet_id, sizeof(miner_wallet_id), "%s", argv[1]);
    int zeros = difficulty_hex_zeros();

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

        if (total_minted >= TOTAL_SUPPLY_MILLICONES || reward <= 0) {
            write_status(blocks_mined, last_index, last_hash, total_minted);
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

        /* Included tx's are now mined - remove them from our own
         * pending_tx.txt (best-effort exact-line removal). */
        if (tx_list[0]) {
            char pending_path[PATH_BUF];
            snprintf(pending_path, sizeof(pending_path), "%s/data/pending_tx.txt", project_root);
            FILE *rf = fopen(pending_path, "r");
            char remaining[64][MAX_LINE];
            int nremain = 0;
            if (rf) {
                char l[MAX_LINE];
                while (fgets(l, sizeof(l), rf)) {
                    l[strcspn(l, "\n")] = '\0';
                    if (!l[0]) continue;
                    if (strstr(tx_list, l)) continue; /* was included */
                    if (nremain < 64) snprintf(remaining[nremain++], MAX_LINE, "%s", l);
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
    return 0;
}
