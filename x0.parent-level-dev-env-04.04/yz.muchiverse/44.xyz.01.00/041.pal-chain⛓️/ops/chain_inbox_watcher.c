/* chain_inbox_watcher - PERSISTENT process (named but not-yet-built in
 * the first PAL-CHAIN-STANDARD.txt draft - built now, sec. 4). Tails
 * net/inbox.txt (palnet_peer.c's own inbox file - this node's own
 * palnet_peer instance writes every DATA line a peer sends there
 * verbatim) and applies each new TX/BLOCK line to this node's own
 * local files:
 *   - TX line not already known (dedup by tx_id, checked against both
 *     data/pending_tx.txt and data/blockchain.txt) -> appended to
 *     pending_tx.txt so THIS node's own chain_miner.c can include it.
 *   - BLOCK line that is exactly this node's own next expected index,
 *     whose prev_hash matches this node's own current chain tip, AND
 *     whose claimed block_hash is independently RECOMPUTED and verified
 *     (never trusted verbatim) -> appended to blockchain.txt, and any
 *     of its own tx's removed from pending_tx.txt (already mined by the
 *     peer that sent it).
 * v1 has NO fork-resolution (sec. 7) - a BLOCK that doesn't match the
 * expected next index/prev_hash is simply dropped, not queued or
 * re-tried.
 *
 * Tracks its own read position in net/inbox_watcher_state.txt
 * (last_line=N, a plain line-count cursor - inbox.txt is append-only,
 * matching palnet_peer.c's own write-only usage of it) so a restart
 * doesn't re-process the whole file from scratch.
 *
 * Self-contained, no shared headers - block-parsing/reward-schedule
 * logic here is intentionally duplicated from chain_balance.c/
 * chain_miner.c per this family's own no-shared-headers convention.
 *
 * Usage: chain_inbox_watcher.+x (no args, reads net/inbox.txt relative
 * to PRISC_PROJECT_ROOT) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <openssl/sha.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define DEFAULT_DIFFICULTY_HEX_ZEROS 5

static char project_root[MAX_PATH] = ".";
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
    return DEFAULT_DIFFICULTY_HEX_ZEROS;
}

/* Fast hex encode (no per-byte snprintf) - see chain_miner.c's own
 * identical header comment for why: it's a real, measured speedup, not
 * a style preference. */
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

static long read_last_line(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/inbox_watcher_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long v = 0;
    if (fscanf(f, "last_line=%ld", &v) != 1) v = 0;
    fclose(f);
    return v;
}

static void write_last_line(long v) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/net/inbox_watcher_state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "last_line=%ld\n", v); fclose(f); }
}

static int tx_id_of(const char *tx_line, char *out, size_t out_sz) {
    /* TX|<from>|<to>|<amount>|<timestamp>|<tx_id> - tx_id is the field
     * after the 5th pipe. */
    const char *p = tx_line;
    int pipes = 0;
    while (*p && pipes < 5) { if (*p == '|') pipes++; p++; }
    if (pipes < 5) return 0;
    snprintf(out, out_sz, "%s", p);
    return 1;
}

static int chain_contains(const char *needle) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static int pending_contains(const char *needle) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/pending_tx.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static void scan_chain_tip(long *last_index, char *last_hash, size_t last_hash_sz) {
    *last_index = -1;
    snprintf(last_hash, last_hash_sz, "%s", "0000000000000000000000000000000000000000000000000000000000000000");
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "BLOCK|", 6) != 0) continue;
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line + 6);
        char *fields[5];
        char *cursor = copy;
        int nf = 0;
        for (; nf < 4; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 4) continue;
        long idx = atol(fields[0]);
        if (idx > *last_index) {
            *last_index = idx;
            /* REAL BUG, LIVE-CAUGHT during a 2-node propagation test:
             * fields are index[0]/prev_hash[1]/nonce[2]/hash[3] - this
             * used to grab fields[2] (the NONCE) as the chain tip's own
             * hash, so every subsequent received block's prev_hash
             * comparison (this file's own handle_block_line()) failed
             * after the very first block, silently dropping every
             * block after block 0 as "doesn't match our tip" even
             * though it legitimately did. */
            snprintf(last_hash, last_hash_sz, "%s", fields[3]);
        }
    }
    fclose(f);
}

/* PAL-NET-STANDARD.txt sec. 5/6 - REAL BUG, LIVE-CAUGHT (2026-07-20,
 * TWICE, in pal-chat-irc - see that project's own chat_inbox_watcher.c
 * for the full trace): applying a received TX/BLOCK used to update the
 * right file but never told THIS session's own chtpm_parser_pal to
 * redraw. FIRST fix attempt just pinged frame_changed.txt (ported from
 * p2p_manager.c's own trigger_render()) - insufficient, confirmed live:
 * that marker only tells chtpm_parser_pal to re-read whatever's
 * CURRENTLY in view.txt, it doesn't regenerate view.txt itself - and
 * view.txt is only ever written by chain_compose_frame.c, normally
 * invoked by this project's own pal script in response to a local
 * keypress, a path an inbox watcher never goes through. Real fix:
 * actually RUN chain_compose_frame.+x from here so view.txt gets
 * regenerated with current data BEFORE anything gets told to redraw -
 * that op already pings frame_changed.txt itself at the end.
 * PRISC_PROJECT_ROOT here is this watcher's own launching session's
 * directory, so this only ever touches THAT session's own view.txt/
 * frame_changed.txt. */
static void trigger_render(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/chain_compose_frame.+x >/dev/null 2>&1", project_root);
    if (system(cmd) != 0) { /* best-effort - a failed re-render just means the next real event will catch up */ }
}

static void handle_tx_line(const char *line) {
    char tx_id[128];
    if (!tx_id_of(line, tx_id, sizeof(tx_id))) return;
    if (pending_contains(tx_id) || chain_contains(tx_id)) {
        /* Already applied - possibly by a sibling SESSION of this same
         * real project writing the SAME shared data/ files directly
         * (xyzos-standards.txt sec. 23's own "what stays shared" list) -
         * see chat_inbox_watcher.c's own identical fix/comment for the
         * live-caught bug this addresses: "already in the file" is not
         * the same question as "did THIS session already render it." */
        trigger_render();
        return;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/pending_tx.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
    trigger_render();
}

static void handle_block_line(const char *line, int zeros) {
    /* BLOCK|<index>|<prev_hash>|<nonce>|<hash>|<timestamp>|<miner>|<tx_list> */
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
    if (nf < 6) return;
    fields[6] = cursor;

    long block_index = atol(fields[0]);
    const char *prev_hash = fields[1];
    const char *nonce = fields[2];
    const char *claimed_hash = fields[3];
    const char *tx_list = fields[6];

    long last_index;
    char last_hash[80];
    scan_chain_tip(&last_index, last_hash, sizeof(last_hash));

    if (block_index != last_index + 1) return; /* not the expected next block - no fork resolution in v1 */
    if (strcmp(prev_hash, last_hash) != 0) return;

    char preimage[MAX_LINE + 256];
    char recomputed_hash[65];
    snprintf(preimage, sizeof(preimage), "%ld|%s|%s|%s", block_index, prev_hash, nonce, tx_list);
    sha256_hex(preimage, recomputed_hash);

    if (strcmp(recomputed_hash, claimed_hash) != 0) return; /* forged/corrupt - reject */
    if (!meets_difficulty(recomputed_hash, zeros)) return;  /* doesn't satisfy PoW - reject */

    char chain_path[PATH_BUF];
    snprintf(chain_path, sizeof(chain_path), "%s/data/blockchain.txt", project_root);
    FILE *cf = fopen(chain_path, "a");
    if (cf) { fprintf(cf, "%s\n", line); fclose(cf); }
    trigger_render();

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
                if (strstr(tx_list, l)) continue;
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
}

int main(void) {
    resolve_root();
    int zeros = difficulty_hex_zeros();
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    char inbox_path[PATH_BUF];
    snprintf(inbox_path, sizeof(inbox_path), "%s/net/inbox.txt", project_root);

    long last_line = read_last_line();

    while (!g_stop) {
        FILE *f = fopen(inbox_path, "r");
        if (f) {
            char line[MAX_LINE];
            long cur = 0;
            while (fgets(line, sizeof(line), f)) {
                cur++;
                if (cur <= last_line) continue;
                line[strcspn(line, "\n")] = '\0';
                /* palnet_peer.c's own write_inbox() prefixes every line
                 * with "<sender_node_id>|" (PAL-NET-STANDARD.txt sec. 3's
                 * wire format) - strip it before checking content type.
                 * Which peer sent it isn't needed here: a TX/BLOCK line
                 * is self-describing and independently re-verified
                 * (recomputed hash, sec. 4), not trusted because of who
                 * relayed it. */
                char *content = strchr(line, '|');
                content = content ? content + 1 : line;
                if (strncmp(content, "TX|", 3) == 0) handle_tx_line(content);
                else if (strncmp(content, "BLOCK|", 6) == 0) handle_block_line(content, zeros);
            }
            last_line = cur;
            fclose(f);
            write_last_line(last_line);
        }
        sleep(1);
    }
    return 0;
}
