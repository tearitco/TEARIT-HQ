/* chain_escrow - lock coins in escrow, pay them out or refund them, and inspect
 * escrow state (PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md sec. 5).
 *
 * Transaction lines it issues (mined like TX by chain_miner, replayed by chain_balance):
 *   LOCK  |escrow_id|from|amount|agent|ts|tx_id        from -= amount (moved into escrow)
 *   PAYOUT|escrow_id|by|to|amount|ts|tx_id             to += amount
 *   REFUND|escrow_id|by|to|amount|ts|tx_id             to += amount
 * Spendable balance excludes locked coins. A rake is just a PAYOUT to wallet `_burn`
 * (never spendable, supply sink). Invariant per escrow: paid out + refunded <= locked.
 *
 * !! WITHOUT SIGNING the `agent` / `by` field is an HONOR FIELD !! anyone who can write
 * data/pending_tx.txt can claim to be the agent. Escrow is trustworthy on local/test
 * chains only; for the real cones chain it stays behind the gate (escrow is also
 * disabled unless chain.pdl says ESCROW enabled = 1, so a root with no chain.pdl refuses).
 *
 * ISSUE-TIME checks (here) run over the MINED chain plus the lines already pending, so
 * a lock and its payout can be queued back to back; chain_miner re-checks every line at
 * INCLUSION time over the same rules and drops an invalid one (a block cannot be
 * un-mined). Checks: escrow enabled, ids [A-Za-z0-9_-]+, amount > 0, LOCK: id unused,
 * from != _burn and spendable >= amount; PAYOUT/REFUND: escrow exists, by == the lock's
 * agent, amount <= remaining.
 *
 * Usage: chain_escrow.+x lock   <escrow_id> <from> <amount> <agent>
 *        chain_escrow.+x payout <escrow_id> <by> <to> <amount>
 *        chain_escrow.+x refund <escrow_id> <by> <to> <amount>
 *        chain_escrow.+x status <escrow_id>
 *            prints escrow=<id> agent=<a> locked=N paid_out=N refunded=N remaining=N
 *            from a full replay of the MINED chain (queued lines are not counted)
 *        chain_escrow.+x audit
 *            full replay of the mined chain: one `bal|<wallet>|<n>` row per wallet
 *            (including _burn), then minted= (block rewards + faucet mints) balances=
 *            (all wallets except _burn) locked= (still in escrow) burn= (the _burn
 *            balance) conserved=1|0 (minted == balances + locked + burn). Extra verb
 *            beyond the spec, so a test can check conservation without arithmetic.
 *   root = PRISC_PROJECT_ROOT (default .)
 * Exit: 0 ok, 1 usage, 2 refused: escrow disabled / unknown escrow (status),
 *       3 refused by an issue-time check, 4 could not write. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_W 4096
#define MAX_E 4096

static char project_root[MAX_PATH] = ".";
static long g_initial_reward = 10500, g_halving_period = 1000;
static int g_has_pdl = 0, g_escrow_enabled = 0, g_faucet_enabled = 0, g_signing = 0;
static char g_chain_id[64] = "cones", g_sigtail[256] = "";
static long g_faucet_amount = 0;
static struct { char id[128]; long long bal; } W[MAX_W]; static int nW;
static struct { char id[128], agent[128]; long long locked, paid, refunded; } E[MAX_E]; static int nE;
static long long g_minted;

/* chain.pdl row reader, duplicated per op (house rule: no shared headers). */
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
    if (pdl_get("FAUCET", "enabled", v, sizeof(v))) g_faucet_enabled = atoi(v) == 1;
    if (pdl_get("FAUCET", "amount_millicones", v, sizeof(v))) g_faucet_amount = atol(v);
    if (pdl_get("ESCROW", "enabled", v, sizeof(v))) g_escrow_enabled = atoi(v) == 1;
    if (pdl_get("SIGNING", "required", v, sizeof(v))) g_signing = atoi(v) == 1;
    if (pdl_get("CHAIN", "id", v, sizeof(v)) && v[0]) snprintf(g_chain_id, sizeof g_chain_id, "%.63s", v);
    if (pdl_get("CHAIN", "kind", v, sizeof(v)) && strcmp(v, "cones") == 0) g_faucet_enabled = 0;
}

static long long reward_for_block(long idx) {
    long epoch = idx / g_halving_period;
    if (epoch >= 62) return 0;
    return g_initial_reward >> epoch;
}

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
static int split_tx(char *tx, char **tf, int max) {
    int n = 0; char *c = tx;
    while (n < max) { tf[n++] = c; char *p = strchr(c, '|'); if (!p) break; *p = '\0'; c = p + 1; }
    return n;
}

/* Same rules as chain_miner's inclusion check (duplicated on purpose). validate=0: replay. */
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
        if (!validate) g_minted += amt;
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
            E[nE].locked = amt; E[nE].paid = 0; E[nE].refunded = 0; nE++;
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
            if (E[ei].paid + E[ei].refunded + amt > E[ei].locked) { snprintf(why, why_sz, "exceeds remaining locked (%lld left)", E[ei].locked - E[ei].paid - E[ei].refunded); return 0; }
        }
        if (ei >= 0) { if (tf[0][0] == 'P') E[ei].paid += amt; else E[ei].refunded += amt; }
        *wallet_bal(tf[3]) += amt;
    }
    return 1;
}

static void replay_chain(void) {
    nW = 0; nE = 0; g_minted = 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/blockchain.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "BLOCK|", 6) != 0) continue;
        char *fields[7]; char *cur = line + 6; int nf = 0;
        for (; nf < 6; nf++) { char *p = strchr(cur, '|'); if (!p) break; *p = '\0'; fields[nf] = cur; cur = p + 1; }
        if (nf < 6) continue;
        fields[6] = cur;
        long idx = atol(fields[0]);
        long long r = reward_for_block(idx);
        *wallet_bal(fields[5]) += r; g_minted += r;
        char *sp = NULL, *tx = strtok_r(fields[6], ";", &sp);
        while (tx) {
            char *tf[10], why[64];
            int tnf = split_tx(tx, tf, 10);
            apply_tx(tf, tnf, 0, why, sizeof(why));
            tx = strtok_r(NULL, ";", &sp);
        }
    }
    fclose(f);
}

static void replay_pending(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/pending_tx.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;
        char *tf[10], why[64];
        int tnf = split_tx(line, tf, 10);
        apply_tx(tf, tnf, 1, why, sizeof(why));   /* a line the miner will drop changes nothing */
    }
    fclose(f);
}


/* ---- signed escrow rows (signatures, escrow slice; design 16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 19/27) ----
 * On a chain with `SIGNING | required | 1` every LOCK / PAYOUT / REFUND must be signed by the wallet that loses the coins (LOCK: from) or by the escrow agent (PAYOUT/REFUND: by).
 * Optional trailing args <nonce> <sig_hex>. Signed message: <KIND>|<chain>|<esc>|<signer>|<a3>|<a4>|<nonce> (LOCK: a3 = amount, a4 = agent; PAYOUT/REFUND: a3 = to, a4 = amount)
 * under context chain:<id>, verified through sig_op against the signer's wallet.txt pubkey=. A (signer, nonce) pair is usable once across TX and escrow rows. The line gets two
 * trailing fields n=<nonce>|s=<sig>. The miner re-verifies at inclusion (chain_miner.c). Copies of chain_send's helpers on purpose (no header+link). */
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
static int nonce_used(const char *signer, const char *nonce) {
    const char *files[2] = { "data/pending_tx.txt", "data/blockchain.txt" }; char needle[256]; snprintf(needle, sizeof needle, "|n=%s|", nonce);
    for (int i = 0; i < 2; i++) { char path[PATH_BUF]; snprintf(path, sizeof path, "%s/%s", project_root, files[i]); FILE *f = fopen(path, "r"); if (!f) continue; static char line[1 << 16];
        while (fgets(line, sizeof line, f)) { char *sp = NULL; for (char *tx = strtok_r(line, ";\n", &sp); tx; tx = strtok_r(NULL, ";\n", &sp)) { int esc = 0; char *row = find_row(tx, &esc); if (!row) continue; char copy[1 << 12]; snprintf(copy, sizeof copy, "%s", row);
                char *fld[4]; int n = 0; char *c = copy; while (n < 4) { fld[n++] = c; char *q = strchr(c, '|'); if (!q) break; *q = 0; c = q + 1; }
                if (n < 3) continue; const char *who = esc ? fld[2] : fld[1]; if (strcmp(who, signer) != 0) continue;
                if (strstr(row, needle)) { fclose(f); return 1; } } }
        fclose(f); }
    return 0; }
static int verify_sig(const char *pub, const char *msg, const char *sig) {   /* fork+exec sig_op verify with a watchdog (house rule: no popen) */
    char exe[PATH_BUF], ctx[128]; snprintf(exe, sizeof exe, "%s/ops/+x/sig_op.+x", project_root); if (access(exe, X_OK)) return -1; snprintf(ctx, sizeof ctx, "chain:%s", g_chain_id);
    pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int nul = open("/dev/null", O_WRONLY); if (nul >= 0) { dup2(nul, 1); dup2(nul, 2); } execl(exe, exe, "verify", pub, ctx, msg, sig, (char *)NULL); _exit(97); }
    time_t t0 = time(NULL); int st;
    for (;;) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 1 : 0; if (time(NULL) - t0 > 10) { kill(c, SIGKILL); waitpid(c, &st, 0); return -1; } usleep(20000); } }
/* 0 = ok (g_sigtail set when signed), nonzero = exit code after printing why. argv_nonce/argv_sig NULL = unsigned. */
static int escrow_sign_check(const char *kind, const char *esc, const char *signer, const char *a3, const char *a4, const char *nonce, const char *sig) {
    g_sigtail[0] = 0;
    if (g_signing && !sig) { fprintf(stderr, "This chain requires signed transactions (SIGNING | required).\n"); return 3; }
    if (!sig) return 0;
    if (!nonce || !nonce[0] || strlen(nonce) > 18) { fprintf(stderr, "nonce must be a whole number.\n"); return 3; }
    for (const char *p = nonce; *p; p++) if (!isdigit((unsigned char)*p)) { fprintf(stderr, "nonce must be a whole number.\n"); return 3; }
    char pub[80] = ""; if (!wallet_pubkey(signer, pub, sizeof pub)) { fprintf(stderr, "Signer has no public key on this node (wallet.txt pubkey=).\n"); return 3; }
    char msg[640]; snprintf(msg, sizeof msg, "%s|%s|%s|%s|%s|%s|%s", kind, g_chain_id, esc, signer, a3, a4, nonce);
    int v = verify_sig(pub, msg, sig); if (v < 0) { fprintf(stderr, "Could not run sig_op (ops/+x/sig_op.+x).\n"); return 3; }
    if (!v) { fprintf(stderr, "Signature does not verify.\n"); return 3; }
    if (nonce_used(signer, nonce)) { fprintf(stderr, "Nonce already used by this wallet (replay).\n"); return 3; }
    snprintf(g_sigtail, sizeof g_sigtail, "|n=%s|s=%s", nonce, sig); return 0; }

static int valid_id(const char *id) {
    if (!id[0] || strlen(id) > 100) return 0;
    for (const char *p = id; *p; p++) if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    return 1;
}

static int issue(char *line) {
    /* validate a copy over mined+pending state, then append the real line */
    replay_chain();
    replay_pending();
    char copy[MAX_LINE], why[96], *tf[10];
    snprintf(copy, sizeof(copy), "%s", line);
    int tnf = split_tx(copy, tf, 10);
    if (!apply_tx(tf, tnf, 1, why, sizeof(why))) { fprintf(stderr, "Refused: %s.\n", why); return 3; }

    char pend[PATH_BUF], outb[PATH_BUF];
    snprintf(pend, sizeof(pend), "%s/data/pending_tx.txt", project_root);
    snprintf(outb, sizeof(outb), "%s/net/outbox.txt", project_root);
    FILE *f = fopen(pend, "a");
    if (!f) { fprintf(stderr, "Could not append pending_tx.txt.\n"); return 4; }
    fprintf(f, "%s\n", line); fclose(f);
    {   struct stat st;
        if (stat(outb, &st) == 0 && st.st_size > 2560 * 1024) { FILE *z = fopen(outb, "w"); if (z) fclose(z); } }
    f = fopen(outb, "a");
    if (f) { fprintf(f, "%s\n", line); fclose(f); }
    return 0;
}

int main(int argc, char **argv) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    load_chain_pdl();
    if (argc < 2) goto usage;
    const char *verb = argv[1];
    long ts = (long)time(NULL);
    char tx_id[200], line[MAX_LINE];
    snprintf(tx_id, sizeof(tx_id), "esc-%ld-%d-%d", ts, (int)getpid(), rand() % 1000000);

    if (strcmp(verb, "audit") == 0) {
        replay_chain();
        long long balances = 0, locked = 0, burn = 0;
        for (int i = 0; i < nW; i++) {
            printf("bal|%s|%lld\n", W[i].id, W[i].bal);
            if (strcmp(W[i].id, "_burn") == 0) burn += W[i].bal; else balances += W[i].bal;
        }
        for (int i = 0; i < nE; i++) locked += E[i].locked - E[i].paid - E[i].refunded;
        printf("minted=%lld balances=%lld locked=%lld burn=%lld conserved=%d\n", g_minted, balances, locked, burn, g_minted == balances + locked + burn);
        return 0;
    }
    if (strcmp(verb, "status") == 0) {
        if (argc < 3) goto usage;
        replay_chain();
        int ei = escrow_find(argv[2]);
        if (ei < 0) { fprintf(stderr, "No such escrow (in the mined chain).\n"); return 2; }
        printf("escrow=%s agent=%s locked=%lld paid_out=%lld refunded=%lld remaining=%lld\n", E[ei].id, E[ei].agent,
               E[ei].locked, E[ei].paid, E[ei].refunded, E[ei].locked - E[ei].paid - E[ei].refunded);
        return 0;
    }
    if (!g_escrow_enabled) { fprintf(stderr, "Escrow is disabled on this chain.\n"); return 2; }
    if (strcmp(verb, "lock") == 0) {
        if (argc < 6 || argc == 7 || argc > 8) goto usage;
        if (!valid_id(argv[2]) || !valid_id(argv[3]) || !valid_id(argv[5])) { fprintf(stderr, "Ids: letters, digits, _ and - only.\n"); return 3; }
        long amount = atol(argv[4]); char am[32]; snprintf(am, sizeof am, "%ld", amount);
        { int sc = escrow_sign_check("LOCK", argv[2], argv[3], am, argv[5], argc == 8 ? argv[6] : NULL, argc == 8 ? argv[7] : NULL); if (sc) return sc; }
        snprintf(line, sizeof(line), "LOCK|%s|%s|%ld|%s|%ld|%s%s", argv[2], argv[3], amount, argv[5], ts, tx_id, g_sigtail);
        int rc = issue(line);
        if (rc == 0) printf("Locked %ld millicones from %s in escrow %s (agent %s).\n", amount, argv[3], argv[2], argv[5]);
        return rc;
    }
    if (strcmp(verb, "payout") == 0 || strcmp(verb, "refund") == 0) {
        if (argc < 6 || argc == 7 || argc > 8) goto usage;
        if (!valid_id(argv[2]) || !valid_id(argv[3]) || !valid_id(argv[4])) { fprintf(stderr, "Ids: letters, digits, _ and - only.\n"); return 3; }
        long amount = atol(argv[5]); char am[32]; snprintf(am, sizeof am, "%ld", amount);
        int pay = verb[0] == 'p';
        { int sc = escrow_sign_check(pay ? "PAYOUT" : "REFUND", argv[2], argv[3], argv[4], am, argc == 8 ? argv[6] : NULL, argc == 8 ? argv[7] : NULL); if (sc) return sc; }
        snprintf(line, sizeof(line), "%s|%s|%s|%s|%ld|%ld|%s%s", pay ? "PAYOUT" : "REFUND", argv[2], argv[3], argv[4], amount, ts, tx_id, g_sigtail);
        int rc = issue(line);
        if (rc == 0) printf("%s %ld millicones from escrow %s to %s.\n", pay ? "Paid out" : "Refunded", amount, argv[2], argv[4]);
        return rc;
    }
usage:
    fprintf(stderr, "Usage: chain_escrow.+x lock <id> <from> <amount> <agent> [<nonce> <sig>] | payout|refund <id> <by> <to> <amount> [<nonce> <sig>] | status <id> | audit\n");
    return 1;
}
