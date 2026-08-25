/* player_open_derivative - futures AND options, both entirely new
 * (confirmed absent everywhere in the original source - see wsr-pal-
 * final-handoff.txt §2, no "future"/"option" symbol anywhere in the
 * whole codebase). Common-sense simplified design, deliberately no
 * margin calls or early-exit: open a position now, it settles itself
 * automatically N turns later via player_settle_futures.c.
 *
 * pieces/player_you/futures.txt - one line per open position:
 *   future|<ticker>|<long|short>|<shares>|<entry_price>|0|0|<turns_left>
 *   option|<ticker>|<call|put>|<shares>|<entry_price>|<strike>|<premium>|<turns_left>
 * (entry_price recorded for futures' own reference/display; strike is
 * the exercise price for options; premium is what was already paid
 * upfront and is NOT charged again at settlement.)
 *
 * Futures: no upfront cost. Settle payout = (settle_price - entry_price)
 * * shares * (long ? 1 : -1) - a pure directional bet, no margin.
 * Options: premium = 5% of strike * shares, charged NOW. Settle payout
 * = max(0, settle_price - strike) * shares for a call, max(0, strike -
 * settle_price) * shares for a put - the premium is sunk cost either way,
 * exactly like a real option (never a REQUIREMENT to exercise, but here
 * exercise is automatic/costless at expiry so there's no reason not to
 * take a $0 payout instead of a loss).
 *
 * Usage: player_open_derivative.+x future <corp_piece_id> <long|short> <shares> <turns>
 *        player_open_derivative.+x option <corp_piece_id> <call|put> <shares> <turns> */
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

static void write_float(const char *state_path, const char *key, float value);
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
    if (argc < 6) {
        printf("Usage: player_open_derivative.+x future|option <corp_piece_id> <direction> <shares> <turns>\n");
        return 1;
    }
    resolve_root();
    const char *kind = argv[1];
    const char *piece_id = argv[2];
    const char *direction = argv[3];
    int shares = atoi(argv[4]);
    int turns = atoi(argv[5]);
    if (shares <= 0 || turns <= 0) { printf("Invalid shares/turns.\n"); return 1; }

    const char *ticker = (strncmp(piece_id, "corp_", 5) == 0) ? piece_id + 5 : piece_id;

    char corp_state[PATH_BUF], player_state[PATH_BUF], futures_path[PATH_BUF];
    snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
    snprintf(player_state, sizeof(player_state), "%s/projects/wsr-pal/pieces/player_you/state.txt", project_root);
    snprintf(futures_path, sizeof(futures_path), "%s/projects/wsr-pal/pieces/player_you/futures.txt", project_root);

    float price = field_f(corp_state, "stock_price");
    if (price <= 0) { printf("Unknown corporation or no price data.\n"); return 1; }

    FILE *f = fopen(futures_path, "a");
    if (!f) { printf("Could not open futures ledger.\n"); return 1; }

    if (strcmp(kind, "future") == 0) {
        if (strcmp(direction, "long") != 0 && strcmp(direction, "short") != 0) { printf("Direction must be long or short.\n"); fclose(f); return 1; }
        fprintf(f, "future|%s|%s|%d|%.2f|0|0|%d\n", ticker, direction, shares, price, turns);
        printf("Opened %s futures: %d shares of %s at $%.2f, settling in %d turns.\n", direction, shares, ticker, price, turns);
    } else if (strcmp(kind, "option") == 0) {
        if (strcmp(direction, "call") != 0 && strcmp(direction, "put") != 0) { printf("Direction must be call or put.\n"); fclose(f); return 1; }
        float strike = (strcmp(direction, "call") == 0) ? price * 1.10f : price * 0.90f;
        float premium = strike * shares * 0.05f;
        float player_cash = field_f(player_state, "cash");
        if (player_cash < premium) { printf("Insufficient cash for premium: need $%.2f, have $%.2f.\n", premium, player_cash); fclose(f); return 1; }
        write_float(player_state, "cash", player_cash - premium);
        fprintf(f, "option|%s|%s|%d|%.2f|%.2f|%.2f|%d\n", ticker, direction, shares, price, strike, premium, turns);
        printf("Bought %s option: %d shares of %s, strike $%.2f, premium $%.2f, expires in %d turns.\n", direction, shares, ticker, strike, premium, turns);
    } else {
        printf("Unknown kind '%s'.\n", kind);
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}
