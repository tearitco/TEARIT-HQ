/* chain_new - create a test/user chain: a new project ROOT under
 * <PRISC_PROJECT_ROOT>/chains/<chain_id>/ (PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN.md
 * sec. 2/7). A chain is just a root: every chain_* op run with PRISC_PROJECT_ROOT
 * pointed at it is that chain. Creates data/ wallets/ net/, empty data/blockchain.txt,
 * pending_tx.txt, faucet_ledger.txt, a `chain.pdl`, and an `ops` symlink to the parent
 * root's ops/ (chain_send shells out to ./ops/+x/chain_balance.+x relative to its root).
 *
 * Never overwrites: an existing chain id is refused. Chain ids are [a-z0-9_-]+ only
 * (max 48); `cones` is reserved for the real chain (the root itself, no chain.pdl).
 * Escrow trust note: without signing, an escrow's agent/by field is an honor field,
 * so test/user chains are the only place escrow is meant to be used.
 *
 * Usage: chain_new.+x <chain_id> [--kind test|user] [--difficulty n] [--cap n]
 *                     [--faucet-amount n] [--cooldown s] [--faucet-daily-cap n]
 *   defaults: kind test, difficulty 1 (1..15), cap 20 blocks/wallet/UTC day (0 = none),
 *   faucet-amount 1000, cooldown 3600, faucet-daily-cap 5000 (0 = none).
 * Exit: 0 created (prints the new root path), 1 usage / bad option,
 *       2 refused (bad or reserved id, already exists), 4 could not write. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static int all_digits(const char *s) {
    if (!s[0]) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static int touch_new(const char *dir, const char *name) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/%s", dir, name);
    int fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: chain_new.+x <chain_id> [--kind test|user] [--difficulty n] [--cap n] [--faucet-amount n] [--cooldown s] [--faucet-daily-cap n]\n");
        return 1;
    }
    const char *id = argv[1];
    const char *kind = "test";
    long difficulty = 1, cap = 20, famount = 1000, cooldown = 3600, fdaily = 5000;
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) { fprintf(stderr, "Option %s needs a value.\n", argv[i]); return 1; }
        const char *v = argv[i + 1];
        if (strcmp(argv[i], "--kind") == 0) {
            if (strcmp(v, "test") != 0 && strcmp(v, "user") != 0) { fprintf(stderr, "--kind must be test or user.\n"); return 1; }
            kind = v;
        } else if (strcmp(argv[i], "--difficulty") == 0 || strcmp(argv[i], "--cap") == 0 || strcmp(argv[i], "--faucet-amount") == 0 ||
                   strcmp(argv[i], "--cooldown") == 0 || strcmp(argv[i], "--faucet-daily-cap") == 0) {
            if (!all_digits(v) || strlen(v) > 9) { fprintf(stderr, "%s needs a non-negative whole number.\n", argv[i]); return 1; }
            long n = atol(v);
            if (strcmp(argv[i], "--difficulty") == 0) { if (n < 1 || n > 15) { fprintf(stderr, "--difficulty must be 1..15.\n"); return 1; } difficulty = n; }
            else if (strcmp(argv[i], "--cap") == 0) cap = n;
            else if (strcmp(argv[i], "--faucet-amount") == 0) famount = n;
            else if (strcmp(argv[i], "--cooldown") == 0) cooldown = n;
            else fdaily = n;
        } else { fprintf(stderr, "Unknown option %s.\n", argv[i]); return 1; }
    }

    size_t n = strlen(id);
    int ok = n > 0 && n <= 48;
    for (const char *p = id; ok && *p; p++) if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_' || *p == '-')) ok = 0;
    if (!ok) { fprintf(stderr, "Invalid chain id - lowercase letters, digits, _ and - only (max 48).\n"); return 2; }
    if (strcmp(id, "cones") == 0) { fprintf(stderr, "'cones' is reserved for the real chain.\n"); return 2; }

    const char *env = getenv("PRISC_PROJECT_ROOT");
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%s", (env && env[0]) ? env : ".");

    char chains_dir[PATH_BUF], croot[PATH_BUF], sub[PATH_BUF + 16];
    snprintf(chains_dir, sizeof(chains_dir), "%s/chains", root);
    snprintf(croot, sizeof(croot), "%s/chains/%s", root, id);
    if (mkdir(chains_dir, 0755) != 0 && errno != EEXIST) { fprintf(stderr, "Could not create chains/.\n"); return 4; }
    if (mkdir(croot, 0755) != 0) {
        if (errno == EEXIST) { fprintf(stderr, "Chain '%s' already exists.\n", id); return 2; }
        fprintf(stderr, "Could not create chain directory.\n");
        return 4;
    }
    const char *subs[] = { "data", "wallets", "net" };
    for (int i = 0; i < 3; i++) {
        snprintf(sub, sizeof(sub), "%s/%s", croot, subs[i]);
        if (mkdir(sub, 0755) != 0) { fprintf(stderr, "Could not create %s.\n", subs[i]); return 4; }
    }
    snprintf(sub, sizeof(sub), "%s/data", croot);
    if (!touch_new(sub, "blockchain.txt") || !touch_new(sub, "pending_tx.txt") || !touch_new(sub, "faucet_ledger.txt")) { fprintf(stderr, "Could not create data files.\n"); return 4; }

    /* ops symlink: relative, so the chain keeps working if the house moves. Best-effort. */
    snprintf(sub, sizeof(sub), "%s/ops", croot);
    if (symlink("../../ops", sub) != 0) { /* chain_send needs it; the other ops do not */ }

    char pdl[PATH_BUF + 32];
    snprintf(pdl, sizeof(pdl), "%s/chain.pdl", croot);
    int fd = open(pdl, O_WRONLY | O_CREAT | O_EXCL, 0644);
    FILE *f = fd >= 0 ? fdopen(fd, "w") : NULL;
    if (!f) { fprintf(stderr, "Could not write chain.pdl.\n"); return 4; }
    fprintf(f, "# chain.pdl - written by chain_new; rows are `SECTION | key | value`\n");
    fprintf(f, "CHAIN   | id                         | %s\n", id);
    fprintf(f, "CHAIN   | kind                       | %s\n", kind);
    fprintf(f, "REWARD  | initial_millicones         | 10500\n");
    fprintf(f, "REWARD  | halving_blocks             | 1000\n");
    fprintf(f, "REWARD  | total_supply_millicones    | 21000000\n");
    fprintf(f, "MINING  | difficulty_hex_zeros       | %ld\n", difficulty);
    fprintf(f, "MINING  | daily_cap_blocks_per_wallet| %ld\n", cap);
    fprintf(f, "FAUCET  | enabled                    | 1\n");
    fprintf(f, "FAUCET  | amount_millicones          | %ld\n", famount);
    fprintf(f, "FAUCET  | cooldown_seconds           | %ld\n", cooldown);
    fprintf(f, "FAUCET  | daily_cap_millicones_per_wallet | %ld\n", fdaily);
    fprintf(f, "ESCROW  | enabled                    | 1\n");
    fclose(f);

    printf("Chain '%s' created at %s\n", id, croot);
    return 0;
}
