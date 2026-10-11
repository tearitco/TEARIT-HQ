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

#define MAX_LINE 8192
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_W 4096
#define MAX_E 4096

static char project_root[MAX_PATH] = ".";
static long g_initial_reward = 10500, g_halving_period = 1000;
static int g_has_pdl = 0, g_escrow_enabled = 0, g_faucet_enabled = 0;
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
        if (argc < 6) goto usage;
        if (!valid_id(argv[2]) || !valid_id(argv[3]) || !valid_id(argv[5])) { fprintf(stderr, "Ids: letters, digits, _ and - only.\n"); return 3; }
        long amount = atol(argv[4]);
        snprintf(line, sizeof(line), "LOCK|%s|%s|%ld|%s|%ld|%s", argv[2], argv[3], amount, argv[5], ts, tx_id);
        int rc = issue(line);
        if (rc == 0) printf("Locked %ld millicones from %s in escrow %s (agent %s).\n", amount, argv[3], argv[2], argv[5]);
        return rc;
    }
    if (strcmp(verb, "payout") == 0 || strcmp(verb, "refund") == 0) {
        if (argc < 6) goto usage;
        if (!valid_id(argv[2]) || !valid_id(argv[3]) || !valid_id(argv[4])) { fprintf(stderr, "Ids: letters, digits, _ and - only.\n"); return 3; }
        long amount = atol(argv[5]);
        int pay = verb[0] == 'p';
        snprintf(line, sizeof(line), "%s|%s|%s|%s|%ld|%ld|%s", pay ? "PAYOUT" : "REFUND", argv[2], argv[3], argv[4], amount, ts, tx_id);
        int rc = issue(line);
        if (rc == 0) printf("%s %ld millicones from escrow %s to %s.\n", pay ? "Paid out" : "Refunded", amount, argv[2], argv[4]);
        return rc;
    }
usage:
    fprintf(stderr, "Usage: chain_escrow.+x lock <id> <from> <amount> <agent> | payout|refund <id> <by> <to> <amount> | status <id> | audit\n");
    return 1;
}
