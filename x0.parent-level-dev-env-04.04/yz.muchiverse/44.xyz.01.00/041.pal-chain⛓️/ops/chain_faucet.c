/* chain_faucet - claim test coins on a chain that has a faucet
 * (PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md sec. 4).
 *
 * Refuses (exit 2) unless <root>/chain.pdl exists, its CHAIN kind is not `cones`, and
 * FAUCET enabled = 1: a root with no chain.pdl (the real cones chain) never has a
 * faucet. Limits come from the append-only data/faucet_ledger.txt (one line per claim:
 * `CLAIM|wallet|amount|ts`; read whole, appended with one write, never rewritten, no
 * mtime): FAUCET cooldown_seconds since this wallet's last claim (0 = none) and
 * daily_cap_millicones_per_wallet over the current UTC day (0 = none; a claim that would
 * push today's total above it is refused). On success: appends the CLAIM row, then a
 * mint line `FAUCET|wallet|amount|ts|tx_id` to data/pending_tx.txt and net/outbox.txt;
 * a miner includes it (chain_miner re-checks the amount against chain.pdl), and
 * chain_balance counts it as +amount. Faucet coins are inflation by design on test
 * chains (not part of the reward schedule). The wallet must exist locally
 * (wallets/<id>/wallet.txt) so a typo cannot mint into the void; `_burn` and any id
 * starting with `_` are refused.
 *
 * Usage: chain_faucet.+x <wallet_id>   (root = PRISC_PROJECT_ROOT, default .)
 * Exit: 0 claimed (prints the amount), 1 usage, 2 refused: no faucet / bad or unknown
 *       wallet, 3 refused: cooldown or daily cap, 4 could not write. */
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

static char project_root[MAX_PATH] = ".";

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

static int valid_wallet_id(const char *id) {
    if (!id[0] || strlen(id) > 100) return 0;
    for (const char *p = id; *p; p++) if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: chain_faucet.+x <wallet_id>\n"); return 1; }
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
    const char *wallet = argv[1];

    char v[128], pp[PATH_BUF];
    snprintf(pp, sizeof(pp), "%s/chain.pdl", project_root);
    if (access(pp, R_OK) != 0) { fprintf(stderr, "No faucet on this chain (no chain.pdl).\n"); return 2; }
    if (pdl_get("CHAIN", "kind", v, sizeof(v)) && strcmp(v, "cones") == 0) { fprintf(stderr, "No faucet on the cones chain.\n"); return 2; }
    if (!(pdl_get("FAUCET", "enabled", v, sizeof(v)) && atoi(v) == 1)) { fprintf(stderr, "Faucet is disabled on this chain.\n"); return 2; }
    long amount = pdl_get("FAUCET", "amount_millicones", v, sizeof(v)) ? atol(v) : 0;
    long cooldown = pdl_get("FAUCET", "cooldown_seconds", v, sizeof(v)) ? atol(v) : 0;
    long daily = pdl_get("FAUCET", "daily_cap_millicones_per_wallet", v, sizeof(v)) ? atol(v) : 0;
    if (amount <= 0) { fprintf(stderr, "Faucet amount is not set.\n"); return 2; }

    if (!valid_wallet_id(wallet) || wallet[0] == '_') { fprintf(stderr, "Invalid wallet_id for the faucet.\n"); return 2; }
    char wp[PATH_BUF];
    snprintf(wp, sizeof(wp), "%s/wallets/%s/wallet.txt", project_root, wallet);
    if (access(wp, R_OK) != 0) { fprintf(stderr, "No such wallet on this chain.\n"); return 2; }

    long now = (long)time(NULL);
    long last_ts = 0, today_total = 0;
    char lp[PATH_BUF];
    snprintf(lp, sizeof(lp), "%s/data/faucet_ledger.txt", project_root);
    FILE *lf = fopen(lp, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            char *f[4]; int n = 0; char *c = line;
            line[strcspn(line, "\n")] = '\0';
            while (n < 4) { f[n++] = c; char *p = strchr(c, '|'); if (!p) break; *p = '\0'; c = p + 1; }
            if (n < 4 || strcmp(f[0], "CLAIM") != 0 || strcmp(f[1], wallet) != 0) continue;
            long ts = atol(f[3]);
            if (ts > last_ts) last_ts = ts;
            if (ts / 86400 == now / 86400) today_total += atol(f[2]);
        }
        fclose(lf);
    }
    if (cooldown > 0 && last_ts > 0 && now - last_ts < cooldown) {
        fprintf(stderr, "Faucet cooldown: %ld s left.\n", cooldown - (now - last_ts));
        return 3;
    }
    if (daily > 0 && today_total + amount > daily) {
        fprintf(stderr, "Faucet daily cap reached (%ld of %ld millicones today).\n", today_total, daily);
        return 3;
    }

    char tx_id[160];
    snprintf(tx_id, sizeof(tx_id), "faucet-%s-%ld-%d", wallet, now, (int)getpid());
    FILE *f = fopen(lp, "a");
    if (!f) { fprintf(stderr, "Could not append faucet ledger.\n"); return 4; }
    fprintf(f, "CLAIM|%s|%ld|%ld\n", wallet, amount, now);
    fclose(f);

    char tx_line[MAX_LINE];
    snprintf(tx_line, sizeof(tx_line), "FAUCET|%s|%ld|%ld|%s", wallet, amount, now, tx_id);
    char pend[PATH_BUF], outb[PATH_BUF];
    snprintf(pend, sizeof(pend), "%s/data/pending_tx.txt", project_root);
    snprintf(outb, sizeof(outb), "%s/net/outbox.txt", project_root);
    f = fopen(pend, "a");
    if (!f) { fprintf(stderr, "Could not append pending_tx.txt.\n"); return 4; }
    fprintf(f, "%s\n", tx_line); fclose(f);
    {   struct stat st;
        if (stat(outb, &st) == 0 && st.st_size > 2560 * 1024) { FILE *z = fopen(outb, "w"); if (z) fclose(z); } }
    f = fopen(outb, "a");
    if (f) { fprintf(f, "%s\n", tx_line); fclose(f); }

    printf("Faucet: %ld millicones queued for %s.\n", amount, wallet);
    return 0;
}
