/* chain_send - PAL-CHAIN-STANDARD.txt sec. 3. Checks the sender's own
 * CURRENT derived balance (shells out to chain_balance.+x - the single
 * authoritative derivation, not re-implemented here, matching
 * wsr_menu_input.c's own "shell out to the real op" precedent rather
 * than duplicating the replay logic a third time), refuses if it
 * doesn't cover the amount, otherwise appends a new TX line to
 * data/pending_tx.txt AND to this node's own palnet_peer outbox.txt so
 * it propagates to the mesh for other nodes' own miners to include.
 *
 * tx_id is a local dedup key only (sec. 2) - NOT a signature; v1 has no
 * transaction signing (sec. 7's own named gap) - a malicious local
 * process could still forge a TX claiming to be from any wallet_id.
 *
 * REAL BUG, LIVE-CAUGHT during a 2-node test: this used to require
 * wallets/<to>/wallet.txt to exist ON THIS NODE before allowing a send.
 * That's wrong for a p2p system - a wallet's login credentials
 * (wallet.txt, with its password hash) live only on whichever node(s)
 * chain_create_wallet.+x was actually run on (a real, named v1
 * limitation - see PAL-CHAIN-STANDARD.txt), but the balance LEDGER
 * (blockchain.txt) is global across the mesh. Requiring the recipient's
 * wallet.txt to exist locally on the SENDER's own node made it
 * impossible to send to a wallet that was only ever created on a
 * different node - exactly the "send/receive across headless nodes"
 * scenario this was built to support. Fixed to just validate the
 * recipient wallet_id's own FORMAT (same charset chain_create_wallet.c
 * enforces at creation), not its local presence.
 *
 * Self-contained, no shared headers.
 * Usage: chain_send.+x <from_wallet_id> <to_wallet_id> <amount_millicones> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";
static char g_sigtail[256] = "";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static long current_balance(const char *wallet_id) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/chain_balance.+x %s 2>/dev/null", project_root, wallet_id);
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    long balance = -1;
    if (fscanf(p, "%ld", &balance) != 1) balance = -1;
    pclose(p);
    return balance;
}

static int valid_wallet_id(const char *id) {
    if (!id[0]) return 0;
    for (const char *p = id; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}


/* ---- signed transactions (signatures step 2-3; design 16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 19) ----
 * chain.pdl row `SIGNING | required | 1` makes a signature mandatory on that chain; the real chain (no chain.pdl) stays unsigned until the owner flips it.
 * Optional trailing args: <nonce> <sig_hex>. Signed message: TX|<chain_id>|<from>|<to>|<amount>|<nonce> under context chain:<chain_id> (sig_op: a signature
 * for another chain, another amount or another recipient does not verify). The sender's public key is the `pubkey=` line of its wallet.txt. A (from, nonce)
 * pair already present in pending or the ledger is refused (replay). The TX line gets two trailing fields `n=<nonce>|s=<sig>`; chain_balance and the miner ignore them.
 * The miner re-verifies signed TX lines at inclusion (chain_miner.c). NOT yet: escrow rows, key binding on the chain, the real chain. */
static void trim_ends(char **p) {
    while (**p == ' ') (*p)++;
    char *e = *p + strlen(*p);
    while (e > *p && (e[-1] == ' ' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
}
static int pdl_row(const char *section, const char *key, char *out, size_t n) {
    char path[PATH_BUF]; snprintf(path, sizeof path, "%s/chain.pdl", project_root); FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[MAX_LINE]; int found = 0;
    while (!found && fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        char *a = strchr(line, '|'); if (!a) continue;
        char *b = strchr(a + 1, '|'); if (!b) continue;
        *a = 0; *b = 0;
        char *sec = line, *k = a + 1, *v = b + 1;
        trim_ends(&sec); trim_ends(&k); trim_ends(&v);
        if (!strcmp(sec, section) && !strcmp(k, key)) { snprintf(out, n, "%s", v); found = 1; }
    }
    fclose(f); return found;
}
static int wallet_pubkey(const char *w, char *out, size_t n) {
    char path[PATH_BUF], line[MAX_LINE]; snprintf(path, sizeof path, "%s/wallets/%s/wallet.txt", project_root, w); FILE *f = fopen(path, "r"); if (!f) return 0;
    int ok = 0; while (fgets(line, sizeof line, f)) if (!strncmp(line, "pubkey=", 7)) { line[strcspn(line, "\r\n")] = 0; snprintf(out, n, "%.79s", line + 7); ok = strlen(out) == 64; }
    fclose(f); return ok; }
/* The first TX / LOCK / PAYOUT / REFUND row inside a ledger segment, matched at a field boundary (a BLOCK line must not match "LOCK|" inside "BLOCK|"). esc = 0 for TX (signer = field 1), 1 for escrow rows (signer = field 2). */
static char *find_row(char *tx, int *esc) {
    static const char *K[4] = { "TX|", "LOCK|", "PAYOUT|", "REFUND|" };
    for (char *p = tx; *p; p++) { if (p != tx && p[-1] != '|') continue;
        for (int k = 0; k < 4; k++) if (!strncmp(p, K[k], strlen(K[k]))) { *esc = k > 0; return p; } }
    return NULL; }
static int nonce_used(const char *from, const char *nonce) {      /* one nonce space per signer across TX and LOCK/PAYOUT/REFUND rows (the miner keeps the same one) */
    const char *files[2] = { "data/pending_tx.txt", "data/blockchain.txt" }; char needle[256]; snprintf(needle, sizeof needle, "|n=%s|", nonce);
    for (int i = 0; i < 2; i++) { char path[PATH_BUF]; snprintf(path, sizeof path, "%s/%s", project_root, files[i]); FILE *f = fopen(path, "r"); if (!f) continue; static char line[1 << 16];
        while (fgets(line, sizeof line, f)) { char *sp = NULL; for (char *tx = strtok_r(line, ";\n", &sp); tx; tx = strtok_r(NULL, ";\n", &sp)) { int esc = 0; char *row = find_row(tx, &esc); if (!row) continue; char copy[1 << 12]; snprintf(copy, sizeof copy, "%s", row);
                char *fld[4]; int n = 0; char *c = copy; while (n < 4) { fld[n++] = c; char *q = strchr(c, '|'); if (!q) break; *q = 0; c = q + 1; }
                if (n < 3) continue; const char *who = esc ? fld[2] : fld[1]; if (strcmp(who, from) != 0) continue;
                char *back = strstr(row, needle); if (back) { fclose(f); return 1; } } }
        fclose(f); }
    return 0; }
static int verify_sig(const char *pub, const char *chain, const char *msg, const char *sig) {   /* fork+exec sig_op verify with a watchdog (house rule: no popen) */
    char exe[PATH_BUF], ctx[128]; snprintf(exe, sizeof exe, "%s/ops/+x/sig_op.+x", project_root); if (access(exe, X_OK)) return -1; snprintf(ctx, sizeof ctx, "chain:%s", chain);
    pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int nul = open("/dev/null", O_WRONLY); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); } execl(exe, exe, "verify", pub, ctx, msg, sig, (char *)NULL); _exit(97); }
    time_t t0 = time(NULL); int st;
    for (;;) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 1 : 0; if (time(NULL) - t0 > 10) { kill(c, SIGKILL); waitpid(c, &st, 0); return -1; } usleep(20000); } }
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: chain_send.+x <from_wallet_id> <to_wallet_id> <amount_millicones> [<nonce> <sig_hex>]\n");
        return 1;
    }
    resolve_root();
    const char *from = argv[1];
    const char *to = argv[2];
    long amount = atol(argv[3]);

    if (amount <= 0) {
        fprintf(stderr, "Amount must be positive.\n");
        return 1;
    }
    if (!valid_wallet_id(to)) {
        fprintf(stderr, "Invalid recipient wallet_id - letters, digits, _ and - only.\n");
        return 1;
    }

    if (strcmp(from, "_burn") == 0) {
        fprintf(stderr, "_burn is the supply sink and can never be spent.\n");
        return 1;
    }


    {   char req[16] = "0", chain[64] = "cones"; pdl_row("SIGNING", "required", req, sizeof req); pdl_row("CHAIN", "id", chain, sizeof chain);
        const char *nonce = argc > 5 ? argv[4] : NULL, *sig = argc > 5 ? argv[5] : NULL;
        if (argc == 5) { fprintf(stderr, "A signed send needs <nonce> <sig_hex> together.\n"); return 1; }
        if (!strcmp(req, "1") && !sig) { fprintf(stderr, "This chain requires signed transactions (SIGNING | required).\n"); return 3; }
        if (sig) {
            for (const char *p = nonce; *p; p++) if (!isdigit((unsigned char)*p) || strlen(nonce) > 18) { fprintf(stderr, "nonce must be a whole number.\n"); return 1; }
            char pub[80] = ""; if (!wallet_pubkey(from, pub, sizeof pub)) { fprintf(stderr, "Sender has no public key on this node (wallet.txt pubkey=).\n"); return 3; }
            char msg[512]; snprintf(msg, sizeof msg, "TX|%s|%s|%s|%ld|%s", chain, from, to, amount, nonce);
            int v = verify_sig(pub, chain, msg, sig); if (v < 0) { fprintf(stderr, "Could not run sig_op (ops/+x/sig_op.+x).\n"); return 3; }
            if (!v) { fprintf(stderr, "Signature does not verify.\n"); return 3; }
            if (nonce_used(from, nonce)) { fprintf(stderr, "Nonce already used by this wallet (replay).\n"); return 3; }
            snprintf(g_sigtail, sizeof g_sigtail, "|n=%s|s=%s", nonce, sig); } }
    long balance = current_balance(from);
    if (balance < 0) {
        fprintf(stderr, "Could not determine sender balance.\n");
        return 1;
    }
    if (balance < amount) {
        fprintf(stderr, "Insufficient balance: have %ld, need %ld.\n", balance, amount);
        return 1;
    }

    long ts = (long)time(NULL);
    char tx_id[128];
    snprintf(tx_id, sizeof(tx_id), "%s-%ld-%d", from, ts, rand() % 1000000);

    char tx_line[MAX_LINE];
    snprintf(tx_line, sizeof(tx_line), "TX|%s|%s|%ld|%ld|%s%s", from, to, amount, ts, tx_id, g_sigtail);

    char pending_path[PATH_BUF];
    snprintf(pending_path, sizeof(pending_path), "%s/data/pending_tx.txt", project_root);
    FILE *pf = fopen(pending_path, "a");
    if (pf) { fprintf(pf, "%s\n", tx_line); fclose(pf); }

    char outbox_path[PATH_BUF];
    snprintf(outbox_path, sizeof(outbox_path), "%s/net/outbox.txt", project_root);
    {   struct stat ob_st;
        if (stat(outbox_path, &ob_st) == 0 && ob_st.st_size > 2560 * 1024) {
            FILE *zf = fopen(outbox_path, "w");
            if (zf) fclose(zf);
        }
    }
    FILE *of = fopen(outbox_path, "a");
    if (of) { fprintf(of, "%s\n", tx_line); fclose(of); }

    /* REAL BUG, LIVE-CAUGHT: this used to also repeat "from %s" and the
     * full tx_id in the confirmation text. chain_compose_frame.c renders
     * this inside a fixed BOX_W=60 box, and the sender wallet is already
     * shown on its own "Wallet: ..." line right above - the old, longer
     * message silently lost characters off the recipient wallet ID once
     * both wallet IDs got long enough (see proof/harness-20260730-215522/
     * walletA_send_result.txt). tx_id is still recorded in full in
     * pending_tx.txt/outbox.txt - it never needed to be in the rendered
     * confirmation line. */
    printf("Sent %ld millicones to %s.\n", amount, to);
    return 0;
}
