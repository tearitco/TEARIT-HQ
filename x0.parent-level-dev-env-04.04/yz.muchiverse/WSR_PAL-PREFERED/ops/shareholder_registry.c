/* shareholder_registry - the corp -> holders reverse index, and the only
 * place a dividend is ever paid out.
 *
 * WHY THIS EXISTS
 *   `corp_apply_finances.c` used to debit the corporation by dividend_pct
 *   and credit the money ONLY if `owned_by=player_you`, silently destroying
 *   the payout on every AI/independent corp. The underlying data was
 *   already there and already correct - player_trade.c maintains
 *   `pieces/player_you/holdings.txt` as `<ticker>|<shares>` (negatives for
 *   shorts), and corp_buy_stake.c maintains the same file inside a CORP's
 *   piece directory for cross-corp ownership. Nothing was inverting those
 *   rows into "who owns what", so nothing could pay a pro-rata dividend.
 *
 * DESIGN - ANSWER TO THE "do AI corps hold shares?" QUESTION
 *   The registry is HOLDER-KIND-AGNOSTIC on purpose. It scans every piece
 *   directory under projects/wsr-pal/pieces/ for a holdings.txt and
 *   records whoever is named there, classifying it as player / corp /
 *   other from its id prefix. corp_buy_stake.c is documented as
 *   "<buyer> acquires shares IN another corp", so a corp holding a stake
 *   in a rival is already a supported position and a corp-held row
 *   receives its dividend into its own `cash` field exactly like a
 *   player's. Today only player_you has a holdings.txt (verified by
 *   scanning the live tree), but the schema does not lock that out, and
 *   it does not need editing when the first AI corp goes long.
 *
 * DESIGN - WHY shares_outstanding CANNOT RECONCILE THIS
 *   `shares_outstanding` in a corp's state.txt is NOT a share count. It
 *   is the first decimal scraped off the "Shares of Stock Outstanding:"
 *   line of the market profile by scripts/ensure_entities.{sh,ps1}, in
 *   millions - the live value is 15.15 for corp_AFL while the registry
 *   counts whole shares (player_you holds 55). market_cap is consistent
 *   with it only because market_cap = stock_price x shares_outstanding,
 *   i.e. both are in the same synthetic "millions" unit. Reconciling
 *   whole-share holdings against a millions-scaled profile number is
 *   meaningless, so this op reconciles against what it CAN check: the
 *   per-corp sum of registered rows, and the invariant that a corp's
 *   payouts always sum to the amount that left the corp. It prints the
 *   registered total and the profile's shares_outstanding side by side
 *   and labels them as different units rather than quietly pretending
 *   they agree.
 *
 * SINGLE WRITER
 *   This op is the only writer of projects/wsr-pal/shareholders.txt, and
 *   the only code path that moves dividend money. corp_apply_finances.c
 *   computes the payout amount and hands it here; nothing else credits a
 *   holder. The index is fully derived (a pure function of every
 *   holdings.txt), so "rebuild before every read" is cheap and removes
 *   any possibility of it going stale.
 *
 * USAGE
 *   shareholder_registry.+x summary
 *       one line, for the Shareholder List menu row (see cmd_summary)
 *   shareholder_registry.+x list [<corp_id>]
 *       rebuild the index, then print it (one corp if given)
 *   shareholder_registry.+x holders <corp_id>
 *       rebuild, print the holder count only; exit 2 if nobody is
 *       registered for that corp
 *   shareholder_registry.+x dividend <corp_id> <amount>
 *       rebuild, then pay <amount> out of the corp pro-rata to every
 *       registered holder, debit the corp, append to the corp's
 *       dividends.txt audit trail. Exit 1 if the corp cannot afford it.
 *
 * Self-contained, no shared headers (matching the rest of ops/). */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_FIELD 256
#define MAX_PIECES 512
#define MAX_ROWS 4096

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

/* Returns 1 on success. corp_apply_finances.c and the dividend path both
 * need to know whether the write landed, because a debit that silently
 * did nothing is exactly the money-destroying bug this op exists to fix. */
static int write_state_field(const char *state_path, const char *key, const char *value) {
    FILE *f = fopen(state_path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(state_path, "w");
    if (!f) return 0;
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
    return fclose(f) == 0;
}

static float field_f(const char *state_path, const char *key) {
    char buf[MAX_LINE];
    read_state_field(state_path, key, buf, sizeof(buf));
    return buf[0] ? (float)atof(buf) : 0.0f;
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void state_path_for(char *out, size_t out_sz, const char *piece_id) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s/projects/wsr-pal/pieces/%s/state.txt", project_root, piece_id);
#pragma GCC diagnostic pop
}

/* A holder is classified from its piece id prefix rather than from a
 * hardcoded list, so a future player_N, corp_N or gov_N holder lands in
 * the registry without this file changing. */
static const char *holder_kind(const char *piece_id) {
    if (strncmp(piece_id, "player_", 7) == 0) return "player";
    if (strncmp(piece_id, "corp_", 5) == 0) return "corp";
    if (strncmp(piece_id, "gov_", 4) == 0) return "gov";
    return "other";
}

/* A holdings.txt row names the corporation it is a stake in, but the
 * two writers use DIFFERENT keys for that name and the registry has to
 * accept both:
 *   player_trade.c      writes the bare ticker     - `AFL|55`
 *   corp_buy_stake.c    writes the target's piece id - `corp_AFL|55`
 * The first run of this op against the live tree returned an EMPTY
 * registry precisely because it assumed the second form, so this
 * resolves either spelling to a real corp piece id and returns 0 if no
 * such corporation exists (a stale row for a delisted ticker, which is
 * then reported rather than indexed). */
static int resolve_corp_piece(const char *key, char *out, size_t out_sz) {
    char cand[64];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (strncmp(key, "corp_", 5) == 0) snprintf(cand, sizeof(cand), "%s", key);
    else snprintf(cand, sizeof(cand), "corp_%s", key);
#pragma GCC diagnostic pop
    char state[PATH_BUF];
    state_path_for(state, sizeof(state), cand);
    if (!file_exists(state)) return 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s", cand);
#pragma GCC diagnostic pop
    return 1;
}

typedef struct {
    char corp_piece[64];
    char corp_ticker[16];
    char holder_piece[64];
    char holder_kind[8];
    int shares;
    double pct;
} Row;

static Row rows[MAX_ROWS];
static int n_rows = 0;

/* Stakes pointing at something that is not a live corp piece. Tracked so
 * the listing can say "3 rows ignored" instead of quietly showing a
 * registry that looks complete and is not. */
#define MAX_ORPHANS 64
typedef struct {
    char holder_piece[64];
    char key[64];
    int shares;
} Orphan;
static Orphan orphans[MAX_ORPHANS];
static int n_orphan_rows = 0;

/* Dynamic discovery, in the same spirit as the "for dir in
 * projects/wsr-pal/pieces/corp_XXX/" loop in scripts/tick_all.sh: no
 * hardcoded entity list anywhere, and the same reason opendir/readdir
 * are used instead of a .pal op - prisc+x resolves a piece's literal id
 * at PARSE time, so a .pal op cannot enumerate pieces at runtime (see
 * tick_all.sh's own header).
 *
 * Note the scan is over EVERY piece directory, not just corp_* ones: a
 * holder's stake is recorded in the HOLDER's own holdings.txt, and the
 * overwhelming majority of holders are players, whose directories are
 * named player_*. Filtering the scan to corp_* (the first version of
 * this function) finds only corp-held stakes and reported the whole
 * registry empty. */
static int collect_rows(void) {
    char pieces_dir[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root);
#pragma GCC diagnostic pop

    DIR *d = opendir(pieces_dir);
    if (!d) {
        fprintf(stderr, "cannot open pieces dir: %s\n", pieces_dir);
        return -1;
    }

    n_rows = 0;
    n_orphan_rows = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char holdings_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(holdings_path, sizeof(holdings_path), "%s/%s/holdings.txt", pieces_dir, ent->d_name);
#pragma GCC diagnostic pop
        if (!file_exists(holdings_path)) continue;

        FILE *f = fopen(holdings_path, "r");
        if (!f) continue;
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f) && n_rows < MAX_ROWS) {
            char key[64];
            int shares = 0;
            if (sscanf(line, "%63[^|]|%d", key, &shares) != 2) continue;
            key[strcspn(key, " \t\r\n")] = '\0';
            if (key[0] == '\0' || shares == 0) continue;

            char corp_piece[64];
            if (!resolve_corp_piece(key, corp_piece, sizeof(corp_piece))) {
                /* A stake in something that is not a live corporation.
                 * Counted and reported, never silently dropped and never
                 * paid - paying it would be inventing a counterparty. */
                if (n_orphan_rows < MAX_ORPHANS) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(orphans[n_orphan_rows].holder_piece, sizeof(orphans[0].holder_piece), "%s", ent->d_name);
                    snprintf(orphans[n_orphan_rows].key, sizeof(orphans[0].key), "%s", key);
#pragma GCC diagnostic pop
                    orphans[n_orphan_rows].shares = shares;
                    n_orphan_rows++;
                }
                continue;
            }

            Row *r = &rows[n_rows++];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(r->corp_piece, sizeof(r->corp_piece), "%s", corp_piece);
            snprintf(r->corp_ticker, sizeof(r->corp_ticker), "%s", corp_piece + 5);
            snprintf(r->holder_piece, sizeof(r->holder_piece), "%s", ent->d_name);
            snprintf(r->holder_kind, sizeof(r->holder_kind), "%s", holder_kind(ent->d_name));
#pragma GCC diagnostic pop
            r->shares = shares;
            r->pct = 0.0;
        }
        fclose(f);
    }
    closedir(d);

    /* Fold duplicate (corp, holder) pairs into one net row. The two
     * current writers both maintain a single row per holding, so this
     * never fires on data the game itself produced - it matters for a
     * hand-edited or merged holdings.txt that names the same corp twice
     * (once long, once short). A cap table is one row per holder per
     * corporation, and paying both rows separately is arithmetically
     * identical, but showing the same holder twice with an opaque
     * double percentage reads as a bug in the registry rather than in
     * the input. */
    int merged = 0;    for (int i = 0; i < n_rows; i++) {
        for (int j = i + 1; j < n_rows; ) {
            if (strcmp(rows[j].corp_piece, rows[i].corp_piece) == 0 &&
                strcmp(rows[j].holder_piece, rows[i].holder_piece) == 0) {
                rows[i].shares += rows[j].shares;
                for (int m = j; m < n_rows - 1; m++) rows[m] = rows[m + 1];
                n_rows--;
                merged++;
            } else {
                j++;
            }
        }
    }
    /* A holder who has fully closed out nets to zero - they own nothing,
     * are owed nothing, and listing them would be noise. */
    for (int i = 0; i < n_rows; ) {
        if (rows[i].shares == 0) {
            for (int m = i; m < n_rows - 1; m++) rows[m] = rows[m + 1];
            n_rows--;
        } else {
            i++;
        }
    }

    /* Percentages are per-corp, from the summed registered share count -
     * the one quantity that can actually be reconciled (see the header
     * on why shares_outstanding cannot). Negatives (shorts) are kept in
     * the denominator so a short genuinely dilutes the long's share of
     * the payout, and a holder who is net short across the whole corp
     * ends up paying in rather than receiving. */
    for (int i = 0; i < n_rows; i++) {
        long long total = 0;
        for (int j = 0; j < n_rows; j++) {
            if (strcmp(rows[j].corp_piece, rows[i].corp_piece) == 0) total += rows[j].shares;
        }
        rows[i].pct = (total != 0) ? (100.0 * rows[i].shares / (double)total) : 0.0;
    }
    return n_rows;
}

static void index_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/projects/wsr-pal/shareholders.txt", project_root);
}

static void write_index(void) {
    char path[PATH_BUF];
    index_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write index: %s\n", path);
        return;
    }
    fprintf(f, "# shareholder registry - generated by ops/shareholder_registry.c\n");
    fprintf(f, "# single writer: do not hand-edit, and do not write this file anywhere else\n");
    fprintf(f, "# format: <corp_piece>|<ticker>|<holder_piece>|<holder_kind>|<shares>|<pct_of_registered>\n");
    fprintf(f, "# pct is of the summed registered rows for that corp. shares_outstanding is a\n");
    fprintf(f, "# market-profile figure in MILLIONS (scripts/ensure_entities) and is NOT the\n");
    fprintf(f, "# denominator here - it is carried on each row for reference only.\n");
    for (int i = 0; i < n_rows; i++) {
        char corp_state[PATH_BUF];
        state_path_for(corp_state, sizeof(corp_state), rows[i].corp_piece);
        char so[64];
        read_state_field(corp_state, "shares_outstanding", so, sizeof(so));
        long long total = 0;
        for (int j = 0; j < n_rows; j++) {
            if (strcmp(rows[j].corp_piece, rows[i].corp_piece) == 0) total += rows[j].shares;
        }
        fprintf(f, "%s|%s|%s|%s|%d|%.4f|registered=%lld|profile_shares_outstanding=%s\n",
                rows[i].corp_piece, rows[i].corp_ticker, rows[i].holder_piece,
                rows[i].holder_kind, rows[i].shares, rows[i].pct, total,
                so[0] ? so : "0");
    }
    fclose(f);
}

static int rows_for(const char *corp_piece, int *idx, int max) {
    int n = 0;
    for (int i = 0; i < n_rows && n < max; i++) {
        if (strcmp(rows[i].corp_piece, corp_piece) == 0) idx[n++] = i;
    }
    return n;
}

/* One-line mode, for the Shareholder List MENU row.
 *
 * The menu surfaces an op's captured stdout as the single `last_message=`
 * value in wsr_main_menu/state.txt, and that file is a line-based
 * key=value store - a multi-line table written into it would be read back
 * as garbage state on the next parse. So the menu asks for `summary` and
 * the full table stays available on stdout/`list` and in the index file,
 * whose path the summary names. */
static int cmd_summary(void) {
    char index[PATH_BUF];
    index_path(index, sizeof(index));
    write_index();

    if (n_rows == 0) {
        printf("Shareholder List: empty - no piece holds a stake in any corp yet (index: projects/wsr-pal/shareholders.txt)");
        return 0;
    }

    /* Compact: "<ticker> <n> holder(s): name/shares, name/shares | full
     * registry: projects/wsr-pal/shareholders.txt" - capped so a heavily
     * held world cannot overflow the message field either. */
    char line[MAX_LINE * 2];
    line[0] = '\0';
    const char *last_corp = NULL;
    int shown = 0;
    for (int i = 0; i < n_rows; i++) {
        if (last_corp && strcmp(last_corp, rows[i].corp_piece) == 0) continue;
        last_corp = rows[i].corp_piece;
        if (shown) strncat(line, "; ", sizeof(line) - strlen(line) - 1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s: ",
                 rows[i].corp_ticker);
#pragma GCC diagnostic pop
        int idx[MAX_ROWS];
        int n = rows_for(rows[i].corp_piece, idx, MAX_ROWS);
        for (int k = 0; k < n; k++) {
            char part[128];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(part, sizeof(part), "%s%s %d sh (%.1f%%)", k ? ", " : "",
                     rows[idx[k]].holder_piece, rows[idx[k]].shares, rows[idx[k]].pct);
#pragma GCC diagnostic pop
            strncat(line, part, sizeof(line) - strlen(line) - 1);
        }
        shown++;
        if (shown >= 4) { strncat(line, "; ...", sizeof(line) - strlen(line) - 1); break; }
    }
    strncat(line, " | full registry: projects/wsr-pal/shareholders.txt",
            sizeof(line) - strlen(line) - 1);
    printf("%s", line);
    return 0;
}

static int cmd_list(const char *corp_piece) {
    char index[PATH_BUF];
    index_path(index, sizeof(index));
    write_index();

    if (corp_piece) {
        int idx[MAX_ROWS];
        int n = rows_for(corp_piece, idx, MAX_ROWS);
        printf("SHAREHOLDERS OF %s\n", corp_piece);
        if (n == 0) {
            printf("  (none registered - no holdings.txt row points at this corp)\n");
            return 0;
        }
        for (int k = 0; k < n; k++) {
            Row *r = &rows[idx[k]];
            printf("  %-24s %-6s %8d shares  %7.2f%%\n", r->holder_piece, r->holder_kind, r->shares, r->pct);
        }
        return 0;
    }

    printf("SHAREHOLDER REGISTRY\n");
    if (n_rows == 0) {
        printf("  (empty - no piece holds a stake in any corp)\n");
        return 0;
    }
    printf("%-10s %-24s %-6s %10s %9s   %s\n", "Ticker", "Holder", "Kind", "Shares", "Pct", "Owned_by");
    printf("----------  ------------------------  ------  ----------  ---------  ---------\n");
    const char *last_corp = NULL;
    for (int i = 0; i < n_rows; i++) {
        if (last_corp && strcmp(last_corp, rows[i].corp_piece) == 0) continue;
        last_corp = rows[i].corp_piece;
        char owned_by[MAX_FIELD];
        char corp_state[PATH_BUF];
        state_path_for(corp_state, sizeof(corp_state), rows[i].corp_piece);
        read_state_field(corp_state, "owned_by", owned_by, sizeof(owned_by));
        if (!owned_by[0]) snprintf(owned_by, sizeof(owned_by), "(none)");

        int idx[MAX_ROWS];
        int n = rows_for(rows[i].corp_piece, idx, MAX_ROWS);
        long long total = 0;
        for (int k = 0; k < n; k++) total += rows[idx[k]].shares;
        for (int k = 0; k < n; k++) {
            Row *r = &rows[idx[k]];
            printf("%-10s %-24s %-6s %10d %8.2f%%   %s\n", r->corp_ticker, r->holder_piece,
                   r->holder_kind, r->shares, r->pct, owned_by);
        }
        char profile_so[64];
        read_state_field(corp_state, "shares_outstanding", profile_so, sizeof(profile_so));
        printf("%-10s  total registered: %lld shares   (profile shares_outstanding=%s, millions - different unit)\n",
               rows[i].corp_ticker, total, profile_so[0] ? profile_so : "0");
    }

    if (n_orphan_rows > 0) {
        printf("\nIGNORED %d stake row%s naming a ticker with no live corp piece (never paid):\n",
               n_orphan_rows, n_orphan_rows == 1 ? "" : "s");
        for (int i = 0; i < n_orphan_rows; i++) {
            printf("  %-24s %-8s %d shares\n", orphans[i].holder_piece, orphans[i].key, orphans[i].shares);
        }
    }
    return 0;
}

static int cmd_holders(const char *corp_piece) {
    int idx[MAX_ROWS];
    int n = rows_for(corp_piece, idx, MAX_ROWS);
    if (n == 0) {
        printf("no registered holders for %s\n", corp_piece);
        return 2;
    }
    printf("%d\n", n);
    return 0;
}

/* Renders a signed cent amount as a plain dollar string, e.g. 7421 ->
 * "74.21", -4286 -> "-42.86". Kept as one helper so the printed rows and
 * the written state.txt values are formatted by the same code and cannot
 * disagree. */
static const char *cents_str(long long cents) {
    static char buf[4][64];
    static int slot = 0;
    char *out = buf[slot];
    slot = (slot + 1) % 4;
    snprintf(out, sizeof(buf[0]), "%lld.%02lld", cents / 100, llabs(cents % 100));
    return out;
}

static void append_audit(const char *corp_piece, double amount, int n_holders) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/pieces/%s/dividends.txt", project_root, corp_piece);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%.2f|%d|dividend_paid_to_n_holders\n", amount, n_holders);
    fclose(f);
}

/* Pays <amount> out of <corp_piece> pro-rata to every registered holder.
 *
 * Money is placed BEFORE the corp is debited, and the corp is only
 * debited if every credit landed. That ordering is deliberate: the
 * failure this op replaces destroyed money (payout left the corp and
 * reached nobody), so on any write failure this version destroys money
 * in the opposite direction - the holders are short by at most one
 * period's dividend and the corp keeps its cash, which is a visible,
 * recoverable, one-line-fix state rather than a silent leak.
 *
 * Cent-exact: each holder's share is computed in double, rounded to
 * cents, and the residual is given to the largest positive position, so
 * sum(credits) == amount to the cent on every platform. */
static int cmd_dividend(const char *corp_piece, double amount) {
    if (amount <= 0) {
        fprintf(stderr, "dividend amount must be positive (got %.2f)\n", amount);
        return 1;
    }

    int idx[MAX_ROWS];
    int n = rows_for(corp_piece, idx, MAX_ROWS);
    if (n == 0) {
        printf("dividend WITHHELD: no registered holders for %s - no cash moved\n", corp_piece);
        return 2;
    }

    long long total_shares = 0;
    for (int k = 0; k < n; k++) total_shares += rows[idx[k]].shares;
    if (total_shares <= 0) {
        printf("dividend WITHHELD: %s has only short/zero registered positions - no cash moved\n", corp_piece);
        return 2;
    }

    char corp_state[PATH_BUF];
    state_path_for(corp_state, sizeof(corp_state), corp_piece);
    float corp_cash = field_f(corp_state, "cash");
    if ((double)corp_cash + 0.005 < amount) {
        printf("dividend REFUSED: %s has $%.2f, cannot pay $%.2f\n", corp_piece, corp_cash, amount);
        return 1;
    }

    /* Pass 1: allocate the payout in whole CENTS, exactly.
     *
     * The first version of this used doubles and clamped any negative
     * credit to zero, on the assumption that a holder can only receive.
     * That is wrong twice over: a short holder must PAY IN on a dividend
     * (that is what being short means), and the clamp made the credits
     * stop summing to the amount that left the corp. Verified against a
     * 100-share long against a 30-share short on a $100.00 payout: the
     * long is owed 142.86 and the short owes -42.86, but the clamp
     * zeroed the short and then the residual correction still left the
     * long 1 cent short of the corp's debit - a 1-cent-per-period leak
     * of exactly the kind of silent money-destruction this op exists
     * to end.
     *
     * Instead: floor each holder's exact share toward negative infinity
     * using integer arithmetic, then hand the leftover cents out one
     * cent at a time to the largest positions. Because every term is
     * floored, the leftover is >= 0 and strictly less than the holder
     * count, so the loop always terminates and
     * sum(credits) == payout exactly, for long-only, short-only and
     * mixed books alike. */
    long long total_cents = (long long)(amount * 100.0 + 0.5);
    long long cents[MAX_ROWS];
    long long cents_sum = 0;
    for (int k = 0; k < n; k++) {
        long long numer = total_cents * (long long)rows[idx[k]].shares;
        long long q = numer / total_shares;      /* total_shares > 0, checked above */
        if (numer % total_shares != 0 && ((numer < 0) != (total_shares < 0))) q--;  /* toward -inf */
        cents[k] = q;
        cents_sum += q;
    }
    long long leftover = total_cents - cents_sum;
    while (leftover > 0) {
        int best = -1;
        for (int k = 0; k < n; k++) {
            if (cents[k] <= 0) continue;
            if (best < 0 || rows[idx[k]].shares > rows[idx[best]].shares) best = k;
        }
        if (best < 0) break;   /* only reachable if total_shares is net-short, already refused */
        cents[best]++;
        leftover--;
    }
    if (leftover != 0) {
        /* Unreachable while total_shares > 0, but never let a rounding
         * surprise pay out a different total than the corp is debited. */
        fprintf(stderr, "internal: %lld cents unallocated for %s; payout cancelled\n", leftover, corp_piece);
        return 1;
    }

    /* DIVIDEND IS A DISTRIBUTION FROM EQUITY, NOT ONLY A CASH OUTFLOW.
     *
     * Under GAAP a dividend is paid out of retained earnings, which is part of
     * shareholders' equity. Crediting only the cash side leaves equity
     * untouched, so book value per share never falls.
     *
     * That is not cosmetic in this sim - it is fatal. Valuation is BVP-based
     * (ECONOMY-INTENT.md sections 7 and 9: every bank's own fair-value view is
     * built from book value), so a dividend that leaves equity alone inflates
     * every future valuation. A company could pay dividends indefinitely while
     * its book value climbed, and the auction would converge on a number that
     * drifts upward with every payout. The whole market would slowly become
     * fiction.
     *
     * ORDER IS LOAD-BEARING: this runs BEFORE any money moves, not after. If
     * equity cannot be written, the payout is not a dividend at all - it is
     * cash leaving the balance sheet with no offsetting reduction in equity,
     * which is a broken balance sheet rather than a distribution. Doing it
     * first means a failure aborts before a single cent has been credited or
     * debited, so there is no partial state to reconcile.
     *
     * `book_value` is this sim's common-equity carrier; there is no separate
     * retained_earnings field yet (ROADMAP 2.2, ECONOMY-INTENT.md section 6).
     * Clamped at zero: a corporation cannot distribute more equity than it
     * holds, and an un-clamped negative would put book value - and so every
     * valuation - below zero.
     */
    {
        char eqbuf[64];
        float book_before = field_f(corp_state, "book_value");
        float book_after = book_before - (float)amount;
        if (book_after < 0.0f) book_after = 0.0f;
        snprintf(eqbuf, sizeof(eqbuf), "%.4f", book_after);
        if (!write_state_field(corp_state, "book_value", eqbuf)) {
            fprintf(stderr,
                    "internal: could not reduce equity for %s - dividend of $%.2f "
                    "ABORTED, no cash moved (equity must fall with the payout or "
                    "book value per share never declines)\n",
                    corp_piece, amount);
            return 1;
        }
        printf("dividend: %s equity reduced by $%.2f (book_value $%.2f -> $%.2f)\n",
               corp_piece, amount, book_before, book_after);
    }

    /* Pass 2: apply every cent allocation, tracking the first failure. */
    long long applied_cents = 0;
    int failed = 0;
    for (int k = 0; k < n; k++) {
        if (cents[k] == 0) continue;
        int i = idx[k];
        char holder_state[PATH_BUF];
        state_path_for(holder_state, sizeof(holder_state), rows[i].holder_piece);
        /* Read the on-disk value in cents rather than reusing the float
         * field helper, so the holder's new balance is their old balance
         * in whole cents plus their allocation in whole cents - no
         * float round-trip error compounding across periods. */
        char cur[64];
        read_state_field(holder_state, "cash", cur, sizeof(cur));
        long long cur_cents = cur[0] ? (long long)(atof(cur) * 100.0 + 0.5) : 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld.%02lld", (cur_cents + cents[k]) / 100,
                 llabs((cur_cents + cents[k]) % 100));
        if (!write_state_field(holder_state, "cash", buf)) {
            fprintf(stderr, "failed to credit %s for %s\n", rows[i].holder_piece, corp_piece);
            failed = 1;
            break;
        }
        applied_cents += cents[k];
    }
    if (failed) {
        printf("dividend ABORTED partway: %s was NOT debited, $%.2f of $%.2f reached holders\n",
               corp_piece, applied_cents / 100.0, amount);
        return 1;
    }

    /* The invariant this op exists to make true: the sum of what left the
     * holders' side equals what left the corp, to the cent. */
    if (applied_cents != total_cents) {
        fprintf(stderr, "internal: applied %lld cents, expected %lld; payout cancelled\n",
                applied_cents, total_cents);
        return 1;
    }

    {
        char buf[64];
        long long new_cents = (long long)(corp_cash * 100.0 + 0.5) - total_cents;
        snprintf(buf, sizeof(buf), "%lld.%02lld", new_cents / 100, llabs(new_cents % 100));
        write_state_field(corp_state, "cash", buf);
    }
    append_audit(corp_piece, amount, n);

    printf("dividend: %s paid $%.2f to %d registered holder%s (corp cash $%.2f -> $%.2f)\n",
           corp_piece, amount, n, n == 1 ? "" : "s", corp_cash, corp_cash - (float)amount);
    for (int k = 0; k < n; k++) {
        if (cents[k] == 0) continue;
        printf("  %-24s %-6s %10s  (%.2f%% of $%.2f)\n", rows[idx[k]].holder_piece,
               rows[idx[k]].holder_kind, cents_str(cents[k]), rows[idx[k]].pct, amount);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    resolve_root();

    const char *mode = (argc > 1) ? argv[1] : "list";
    const char *arg2 = (argc > 2) ? argv[2] : NULL;

    if (collect_rows() < 0) return 1;

    if (strcmp(mode, "list") == 0) {
        return cmd_list(arg2);
    }
    if (strcmp(mode, "summary") == 0) {
        return cmd_summary();
    }
    if (strcmp(mode, "holders") == 0) {
        if (!arg2) { fprintf(stderr, "Usage: shareholder_registry.+x holders <corp_id>\n"); return 1; }
        return cmd_holders(arg2);
    }
    if (strcmp(mode, "dividend") == 0) {
        if (!arg2 || argc < 4) {
            fprintf(stderr, "Usage: shareholder_registry.+x dividend <corp_id> <amount>\n");
            return 1;
        }
        return cmd_dividend(arg2, atof(argv[3]));
    }
    if (strcmp(mode, "rebuild") == 0) {
        write_index();
        printf("registry rebuilt: %d holder row%s -> projects/wsr-pal/shareholders.txt\n",
               n_rows, n_rows == 1 ? "" : "s");
        return 0;
    }

    fprintf(stderr,
            "Usage: shareholder_registry.+x list [<corp_id>]\n"
            "                             summary\n"
            "                             holders <corp_id>\n"
            "                             dividend <corp_id> <amount>\n"
            "                             rebuild\n");
    return 1;
}
