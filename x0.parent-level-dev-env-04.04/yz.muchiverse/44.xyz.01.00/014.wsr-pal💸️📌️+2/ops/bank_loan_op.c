/* bank_loan_op - loans, entirely new (confirmed absent from the
 * original source's actual working code - see wsr-pal-final-handoff.
 * txt §2). Common-sense design: any corp tagged `industry=bank` in its
 * own state.txt can lend to the player's owned corp. Approval rule is
 * simple and observable: the bank must hold at least 2x the requested
 * amount in its own cash (a basic reserve-safety heuristic, not a real
 * underwriting model) - tune this via playtesting, not analysis.
 *
 * Usage: bank_loan_op.+x request <borrower_piece_id> <bank_piece_id> <amount>
 *        bank_loan_op.+x repay   <borrower_piece_id> <amount> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void write_float(const char *state_path, const char *key, float value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", value);
    write_state_field(state_path, key, buf);
}

static float field_f(const char *state_path, const char *key) {
    char buf[MAX_LINE];
    read_state_field(state_path, key, buf, sizeof(buf));
    return buf[0] ? atof(buf) : 0.0f;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: bank_loan_op.+x request|repay ...\n"); return 1; }
    resolve_root();

    if (strcmp(argv[1], "request") == 0) {
        if (argc < 5) { printf("Usage: bank_loan_op.+x request <borrower> <bank> <amount>\n"); return 1; }
        const char *borrower = argv[2], *bank = argv[3];
        float amount = atof(argv[4]);

        /* Real bug, found by direct interface testing (2026-07-16):
         * if the player's own owned/active corp happens to BE the
         * designated bank (e.g. they Elect themselves CEO of corp_AFL,
         * which the financing menu also hardcodes as the lender), a
         * loan request let a corp lend to itself with no real effect
         * beyond moving cash between fields for no reason. Guard it. */
        if (strcmp(borrower, bank) == 0) {
            printf("%s can't take out a loan from itself.\n", borrower);
            return 1;
        }

        char borrower_state[PATH_BUF], bank_state[PATH_BUF];
        snprintf(borrower_state, sizeof(borrower_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, borrower);
        snprintf(bank_state, sizeof(bank_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, bank);

        char owned_by[MAX_LINE];
        read_state_field(borrower_state, "owned_by", owned_by, sizeof(owned_by));
        if (strcmp(owned_by, "player_you") != 0) { printf("You don't control %s - can't take a loan on its behalf.\n", borrower); return 1; }

        char industry[MAX_LINE];
        read_state_field(bank_state, "industry", industry, sizeof(industry));
        if (strcmp(industry, "bank") != 0) { printf("%s is not a bank.\n", bank); return 1; }

        float bank_cash = field_f(bank_state, "cash");
        if (bank_cash < amount * 2) { printf("Loan denied - %s doesn't have sufficient reserves (has $%.2f, needs $%.2f).\n", bank, bank_cash, amount * 2); return 1; }

        write_float(bank_state, "cash", bank_cash - amount);
        write_float(borrower_state, "cash", field_f(borrower_state, "cash") + amount);
        write_float(borrower_state, "loan_balance", field_f(borrower_state, "loan_balance") + amount);
        write_state_field(borrower_state, "loan_rate", "0.08");
        write_state_field(borrower_state, "loan_bank", bank);
        printf("Loan approved: $%.2f from %s to %s at 8%%.\n", amount, bank, borrower);

    } else if (strcmp(argv[1], "repay") == 0) {
        if (argc < 4) { printf("Usage: bank_loan_op.+x repay <borrower> <amount>\n"); return 1; }
        const char *borrower = argv[2];
        float amount = atof(argv[3]);

        char borrower_state[PATH_BUF];
        snprintf(borrower_state, sizeof(borrower_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, borrower);

        float loan_balance = field_f(borrower_state, "loan_balance");
        if (loan_balance <= 0) { printf("%s has no outstanding loan.\n", borrower); return 1; }
        float pay = (amount > loan_balance) ? loan_balance : amount;
        float cash = field_f(borrower_state, "cash");
        if (cash < pay) { printf("Not enough cash ($%.2f) to repay $%.2f.\n", cash, pay); return 1; }

        write_float(borrower_state, "cash", cash - pay);
        write_float(borrower_state, "loan_balance", loan_balance - pay);

        char bank[MAX_LINE];
        read_state_field(borrower_state, "loan_bank", bank, sizeof(bank));
        if (bank[0]) {
            char bank_state[PATH_BUF];
            snprintf(bank_state, sizeof(bank_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, bank);
            write_float(bank_state, "cash", field_f(bank_state, "cash") + pay);
        }
        printf("Repaid $%.2f of %s's loan (balance now $%.2f).\n", pay, borrower, loan_balance - pay);

    } else {
        printf("Unknown sub-command '%s'.\n", argv[1]);
        return 1;
    }
    return 0;
}
