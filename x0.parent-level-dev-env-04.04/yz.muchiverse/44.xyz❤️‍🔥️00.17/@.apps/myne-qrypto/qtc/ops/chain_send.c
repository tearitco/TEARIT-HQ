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

#define MAX_LINE 1024
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

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

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: chain_send.+x <from_wallet_id> <to_wallet_id> <amount_millicones>\n");
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
    snprintf(tx_line, sizeof(tx_line), "TX|%s|%s|%ld|%ld|%s", from, to, amount, ts, tx_id);

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
