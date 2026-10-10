/*
 * market_settle.c - match the auction book, move money, set the price.
 *
 * THE ONLY OP THAT MOVES CASH OR HOLDINGS.
 *
 * market_quote.c posts orders into projects/wsr-pal/data/book_<TICKER>.txt.
 * Nothing else reads that file, so until this op exists the market is a
 * suggestion: orders exist, nothing trades, and there is no price. This op
 * closes the loop. It is the sole writer of:
 *
 *   - any participant's `cash`
 *   - any participant's holdings of a ticker
 *   - `stock_price` and `price_history.txt` for a corporation
 *   - `projects/wsr-pal/data/market_ledger.txt`
 *
 * Quoting never writes balances, so an order cannot create money merely by
 * existing. That separation is the whole reason this is a separate binary
 * rather than a second phase of market_quote: the only way to move money is
 * through a matched trade, and the only way to get a matched trade is through
 * a book that was written by a different op.
 *
 * PRICE FORMATION
 *
 * A trade prints at the price of the order that was RESTING (the passive
 * maker), not at the aggressor's limit. That is the standard convention and it
 * matters: if trades printed at the aggressor's price, a participant could
 * mark its own book by quoting wide, because its own crossing order would set
 * the price. Printing at the resting price means you can only ever get the
 * price somebody else already committed to.
 *
 * Because prints are the resting price, the last matched trade IS the price.
 * No formula writes it. corp_update_price.c still exists and is still called
 * on End Turn - that is a faithful port of the original's analysis_loop.c -
 * but it is a separate, clearly-labelled opinion, not the price of record.
 *
 * WHAT THIS OP REFUSES TO DO
 *
 * It will not let a fill exceed available cash, and it will not let a
 * participant buy from itself. Both are silent money bugs if allowed through:
 * a self-trade moves cash to itself and manufactures a price print out of
 * nothing, and an over-spend drives cash negative and then spends it again
 * next turn. Both are refused loudly instead.
 *
 * Self-contained: no shared project headers, matching the rest of ops/. See the
 * house-standard note in market_quote.c - each op owns its constants, and the
 * contract between these two binaries is the BOOK FILE FORMAT, documented on
 * the parser below, not a link.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#undef MAX_PATH
#endif

#define PATHBUF 1400
#define MAXLINE 512
#define MAXPART 64
#define MAXPIECE 64
#define MAXTICK 32
#define MAXORDERS 512

/* ---- tuning constants, owned by THIS op (see market_quote.c house note) ---- */

/* A printed size below this is dust and is dropped rather than filled. Left
 * as a named constant because the two ops SHOULD agree on what a real order
 * is, and this is where that agreement is written down on this side. */
#define WSR_MIN_FILL_SHARES 0.01f

/* Cash may not fall below this after a fill. Zero, not negative: a participant
 * that cannot pay does not get the shares, and the order is reported as
 * unfilled rather than half-executed. */
#define WSR_CASH_FLOOR 0.0f

/* Every path is PRISC_PROJECT_ROOT plus a fixed suffix, and that root is a very
 * long OneDrive path in this checkout, so gcc's -Wformat-truncation fires on
 * the worst case for every one of them. Suppressed the same way as in
 * corp_apply_finances.c:44/96/189 and econ_calendar.c. snprintf still
 * truncates safely, and resolve_root refuses an over-long root rather than
 * truncating one, because a truncated root would silently point every path
 * below at the wrong file. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static char g_root[PATHBUF];

/* ------------------------------------------------------------------ paths -- */

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr,
                    "market_settle: PRISC_PROJECT_ROOT is too long (%zu bytes, "
                    "max %zu) - refusing to run rather than truncate a path\n",
                    n, sizeof(g_root) - 129);
            exit(1);
        }
        memcpy(g_root, env, n + 1);
    } else {
        snprintf(g_root, sizeof(g_root), ".");
    }
}

static void piece_state(char *out, size_t n, const char *piece_id) {
    snprintf(out, n, "%s/projects/wsr-pal/pieces/%s/state.txt", g_root, piece_id);
}

static void piece_holdings(char *out, size_t n, const char *piece_id) {
    snprintf(out, n, "%s/projects/wsr-pal/pieces/%s/holdings.txt", g_root, piece_id);
}

/* ------------------------------------------------------- state read/write -- */
/* Same read-all/rewrite-all shape as corp_buy_stake.c:33-75, which is the
 * house idiom for key=value state files. */

static void read_field(const char *path, const char *key, char *out, size_t n) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAXLINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, n, "%s", v);
            break;
        }
    }
    fclose(f);
}

static double field_d(const char *path, const char *key) {
    char buf[MAXLINE];
    read_field(path, key, buf, sizeof(buf));
    return atof(buf);
}

static void write_field(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    static char lines[64][MAXLINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAXLINE, f)) nlines++;
        fclose(f);
    }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "market_settle: cannot write %s\n", path); return; }
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

/* Holdings are "target_id|shares", ints, negatives allowed for shorts. Same
 * shape as corp_buy_stake.c:81-118. */
static int get_holding(const char *path, const char *target_id) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAXLINE];
    size_t tl = strlen(target_id);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, target_id, tl) == 0 && line[tl] == '|') {
            fclose(f);
            return atoi(line + tl + 1);
        }
    }
    fclose(f);
    return 0;
}

/* Holdings are "target_id|shares", ints, NEGATIVES ALLOWED for shorts - the
 * registry documents that convention and shareholder_registry.c keeps shorts.
 *
 * The zero test below is "!=", not "> 0", and that distinction is load-bearing.
 * The sibling helper in corp_buy_stake.c:110 tests `shares > 0`, which is
 * correct THERE because that op only ever buys, and a row falling to zero is
 * genuinely gone. Copying that guard here silently DROPPED every short: the
 * buyer's +N was written while the seller's -N vanished, so each fill minted
 * shares out of nothing. Measured on a scratch tree, ACME's total went
 * 5000 -> 5823 and ZETA's 0 -> 2807 after a handful of ticks, with cash
 * perfectly conserved throughout - which is exactly why only a share-total
 * check catches this and a cash check never will. A short is a real position
 * and must survive a rewrite. */
static void set_holding(const char *path, const char *target_id, int shares) {
    FILE *f = fopen(path, "r");
    static char lines[256][MAXLINE];
    int nlines = 0;
    if (f) {
        while (nlines < 256 && fgets(lines[nlines], MAXLINE, f)) nlines++;
        fclose(f);
    }
    size_t tl = strlen(target_id);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], target_id, tl) == 0 && lines[i][tl] == '|') {
            if (shares != 0) fprintf(f, "%s|%d\n", target_id, shares);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found && shares != 0) fprintf(f, "%s|%d\n", target_id, shares);
    fclose(f);
}

/* ---------------------------------------------------------------- ledger -- */

/* The original's own row format, MSR-DEPRACATED/financing.c:256. One line
 * carries both sides of the entry, so one line IS the balanced record; this is
 * a double-entry ledger, not a pair of half-entries. The share leg is not a
 * cash row and is deliberately absent - it moves a position, not money. */
static FILE *g_ledger = NULL;

static void ledger_open(void) {
    if (g_ledger) return;
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/market_ledger.txt", g_root);
    g_ledger = fopen(path, "a");
    if (!g_ledger)
        fprintf(stderr, "market_settle: cannot append to %s - refusing to trade "
                        "without a ledger\n", path);
}

static void ledger_fill(const char *buyer, const char *seller,
                        double amount, const char *ticker,
                        double price, double shares) {
    if (!g_ledger) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", t);
    fprintf(g_ledger,
            "Time: %s | Debit: %s | Credit: %s | Amount: %.2f Dollars | "
            "Event: market_settle.+x | Ticker: %s | Price: %.4f | Shares: %.2f\n",
            date_str, buyer, seller, amount, ticker, price, shares);
}

/* ----------------------------------------------------------------- orders -- */

/* THE BOOK FILE CONTRACT. market_quote.c writes these rows and this is the
 * only thing that parses them:
 *
 *   bid|1|<price>|<shares>|<participant_id>
 *   ask|-1|<price>|<shares>|<participant_id>
 *
 * The side word and the sign are redundant on purpose: the word is what a
 * human reads in the file, the sign is what a parser can test without
 * strcmp. They are written together and must be written together. */
typedef struct {
    int    side;     /* +1 bid, -1 ask */
    double price;
    double shares;
    char   who[MAXPIECE];
} Order;

static int cmp_bid_desc(const void *a, const void *b) {
    double d = ((const Order *)b)->price - ((const Order *)a)->price;
    if (d > 0) return 1;
    if (d < 0) return -1;
    return 0;
}

static int cmp_ask_asc(const void *a, const void *b) {
    double d = ((const Order *)a)->price - ((const Order *)b)->price;
    if (d > 0) return 1;
    if (d < 0) return -1;
    return 0;
}

/* ------------------------------------------------------------- one ticker -- */

static int settle_ticker(const char *ticker) {
    char bookpath[PATHBUF], corp_state[PATHBUF];
    snprintf(bookpath, sizeof(bookpath), "%s/projects/wsr-pal/data/book_%s.txt",
             g_root, ticker);

    static Order bids[MAXORDERS], asks[MAXORDERS];
    int nb = 0, na = 0;

    FILE *f = fopen(bookpath, "r");
    if (!f) return 0;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), f)) {
        char side_word[16], who[MAXPIECE];
        int sign;
        double price, shares;
        if (sscanf(line, "%15[^|]|%d|%lf|%lf|%63s",
                   side_word, &sign, &price, &shares, who) != 5)
            continue;
        if (price <= 0.0 || shares <= 0.0) continue;
        if (sign != 1 && sign != -1) continue;

        Order *slot = (sign == 1) ? &bids[nb] : &asks[na];
        int *count  = (sign == 1) ? &nb : &na;
        if (*count >= MAXORDERS) continue;
        slot->side = sign;
        slot->price = price;
        slot->shares = shares;
        snprintf(slot->who, MAXPIECE, "%s", who);
        (*count)++;
    }
    fclose(f);

    if (nb == 0 || na == 0) {
        /* No crossing possible. This is a valid, common outcome, not an error:
         * an illiquid name simply does not trade, and it keeps its last price.
         * Saying so keeps "0 fills" from reading like a failure. */
        printf("%-6s no trade: %d bid(s), %d ask(s)%s\n", ticker, nb, na,
               (nb == 0 && na == 0) ? " (empty book)" : "");
        return 0;
    }

    /* Price priority: best bid first, best (lowest) ask first. */
    qsort(bids, (size_t)nb, sizeof(Order), cmp_bid_desc);
    qsort(asks, (size_t)na, sizeof(Order), cmp_ask_asc);

    int bi = 0, ai = 0, fills = 0, refused = 0;
    double last_price = 0.0, last_shares = 0.0;

    while (bi < nb && ai < na) {
        Order *b = &bids[bi];
        Order *a = &asks[ai];

        if (b->price < a->price) break;   /* book is crossed out: no more trades */

        if (strcmp(b->who, a->who) == 0) {
            /* Self-trade. Refused, and said out loud, because allowing it would
             * let a participant print a price against itself - which is a price
             * with no other party's consent behind it. Skip the resting order
             * that is blocking and keep going. */
            refused++;
            if (b->shares <= a->shares) bi++; else ai++;
            continue;
        }

        double qty = (b->shares < a->shares) ? b->shares : a->shares;
        if (qty < WSR_MIN_FILL_SHARES) break;

        /* Print at the RESTING (passive) price - the ask - not the
         * aggressor's limit. See the header comment: this is what stops a
         * participant marking its own book. */
        double price = a->price;
        double amount = price * qty;

        char buyer_state[PATHBUF], seller_state[PATHBUF];
        piece_state(buyer_state, sizeof(buyer_state), b->who);
        piece_state(seller_state, sizeof(seller_state), a->who);

        double buyer_cash = field_d(buyer_state, "cash");

        if (buyer_cash < amount + WSR_CASH_FLOOR) {
            /* Cannot pay. Not a half fill: a half fill would still need the
             * money. Refuse the whole order and report it, so an unaffordable
             * quote is visible instead of silently shrinking the market. */
            refused++;
            if (b->shares <= a->shares) bi++; else ai++;
            continue;
        }

        double seller_cash = field_d(seller_state, "cash");

        char val[MAXLINE];
        snprintf(val, sizeof(val), "%.2f", buyer_cash - amount);
        write_field(buyer_state, "cash", val);
        snprintf(val, sizeof(val), "%.2f", seller_cash + amount);
        write_field(seller_state, "cash", val);

        /* Shares. A sale may take a holding negative - that is a short, and the
         * registry already stores negatives - and the sum across all holders is
         * unchanged, so this cannot create shares.
         *
         * THE KEY IS THE ISSUER'S PIECE ID, "corp_<TICKER>", NOT THE BARE
         * TICKER. holdings.txt rows are keyed by piece id (corp_buy_stake.c
         * uses target_id, and shareholders.txt uses corp_). An earlier draft
         * looked them up by bare ticker, which matched no row, returned 0, and
         * then wrote -60, which set_holding drops because it only writes
         * positive values - so the SELLER'S LINE NEVER MOVED while the buyer's
         * did. That silently manufactured 60 shares per fill, which is exactly
         * the money-from-nothing bug this op exists to make impossible. */
        char asset[MAXPIECE];
        snprintf(asset, sizeof(asset), "corp_%s", ticker);

        int whole = (int)(qty + 0.5);
        if (whole < 1) whole = 1;

        char hpath[PATHBUF];
        piece_holdings(hpath, sizeof(hpath), b->who);
        int bh = get_holding(hpath, asset);
        set_holding(hpath, asset, bh + whole);

        piece_holdings(hpath, sizeof(hpath), a->who);
        int ah = get_holding(hpath, asset);
        set_holding(hpath, asset, ah - whole);

        /* Conservation is a claim worth checking rather than assuming. Both
         * legs are read back, because a fill is balanced only if the buyer's
         * +N and the seller's -N BOTH landed: checking one side passes while
         * shares are being minted, which is precisely the bug the short-drop
         * above caused. Cash is checked for the same reason. */
        char chk[PATHBUF];
        piece_holdings(chk, sizeof(chk), b->who);
        int got_b = get_holding(chk, asset);
        piece_holdings(chk, sizeof(chk), a->who);
        int got_a = get_holding(chk, asset);
        if (got_b != bh + whole || got_a != ah - whole) {
            fprintf(stderr, "market_settle: HOLDINGS WRITE FAILED on %s/%s "
                            "(buyer read %d want %d, seller read %d want %d) - "
                            "aborting rather than leaving a half-applied fill\n",
                    b->who, a->who, got_b, bh + whole, got_a, ah - whole);
            return -1;
        }
        double got_bc = field_d(buyer_state, "cash");
        double got_sc = field_d(seller_state, "cash");
        if (fabs(got_bc - (buyer_cash - amount)) > 0.01 ||
            fabs(got_sc - (seller_cash + amount)) > 0.01) {
            fprintf(stderr, "market_settle: CASH WRITE FAILED on %s/%s - "
                            "aborting rather than leaving a half-applied fill\n",
                    b->who, a->who);
            return -1;
        }

        ledger_fill(b->who, a->who, amount, ticker, price, qty);

        last_price = price;
        last_shares = qty;
        fills++;

        b->shares -= qty;
        a->shares -= qty;
        if (b->shares < WSR_MIN_FILL_SHARES) bi++;
        if (a->shares < WSR_MIN_FILL_SHARES) ai++;
    }

    if (fills > 0) {
        /* The price of record is the last matched trade. Written here and
         * nowhere else in the matching path - no formula touches it. */
        snprintf(corp_state, sizeof(corp_state), "%s/projects/wsr-pal/pieces/corp_%s/state.txt",
                 g_root, ticker);
        char val[MAXLINE];
        double prev = field_d(corp_state, "stock_price");
        snprintf(val, sizeof(val), "%.4f", last_price);
        write_field(corp_state, "stock_price", val);

        char hist[PATHBUF];
        snprintf(hist, sizeof(hist), "%s/projects/wsr-pal/pieces/corp_%s/price_history.txt",
                 g_root, ticker);
        FILE *hf = fopen(hist, "a");
        if (hf) {
            double pct = (prev > 0.0) ? ((last_price - prev) / prev * 100.0) : 0.0;
            fprintf(hf, "%.0f,%.4f,%.4f,%.2f%%\n", 0.0, prev, last_price, pct);
            fclose(hf);
        }

        printf("%-6s %d fill(s) last=%.4f x %.2f\n", ticker, fills, last_price, last_shares);
    } else if (refused > 0) {
        /* Distinguish "nobody wanted to deal" from "someone wanted to and was
         * stopped". Saying "not crossed" after refusing a crossed order reads
         * as a calm market when the truth is that trade was attempted and
         * denied, which is the case an operator needs to see. */
        printf("%-6s no trade: %d order(s) refused (self-trade or insufficient "
               "cash) out of %d bid(s), %d ask(s)\n", ticker, refused, nb, na);
    } else {
        printf("%-6s no trade: book not crossed (%d bid(s), %d ask(s))\n",
               ticker, nb, na);
    }
    if (refused && fills > 0)
        printf("%-6s %d order(s) refused (self-trade or insufficient cash)\n",
               ticker, refused);
    return fills;
}

int main(void) {
    resolve_root();

    char datadir[PATHBUF];
    snprintf(datadir, sizeof(datadir), "%s/projects/wsr-pal/data", g_root);
    DIR *d = opendir(datadir);
    if (!d) {
        fprintf(stderr, "market_settle: cannot read %s\n", datadir);
        return 1;
    }

    static char tickers[MAXORDERS][MAXTICK];
    int nt = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && nt < MAXORDERS) {
        const char *pfx = "book_";
        size_t pl = strlen(pfx), nl = strlen(e->d_name);
        if (nl <= pl + 4) continue;
        if (strncmp(e->d_name, pfx, pl) != 0) continue;
        const char *sfx = ".txt";
        size_t sl = strlen(sfx);
        if (strcmp(e->d_name + nl - sl, sfx) != 0) continue;
        size_t tlen = nl - pl - sl;
        if (tlen >= MAXTICK) continue;
        memcpy(tickers[nt], e->d_name + pl, tlen);
        tickers[nt][tlen] = '\0';
        nt++;
    }
    closedir(d);

    if (nt == 0) {
        printf("market_settle: no book_<TICKER>.txt found - run market_quote first\n");
        return 0;
    }

    ledger_open();
    if (!g_ledger) {
        /* Without a ledger this op would move real balances with no record.
         * That is the one failure mode worse than doing nothing. */
        return 1;
    }

    int total = 0;
    for (int i = 0; i < nt; i++) {
        int n = settle_ticker(tickers[i]);
        if (n < 0) {
            /* A fill could not be applied cleanly. Stop rather than carry on:
             * continuing would leave cash moved against shares that did not
             * move, which is the exact corruption this op must not create. */
            fprintf(stderr, "market_settle: aborting at %s - balances are "
                            "partially applied and the run should be "
                            "reconciled from the ledger\n", tickers[i]);
            if (g_ledger) fclose(g_ledger);
            return 1;
        }
        total += n;
    }

    if (g_ledger) fclose(g_ledger);

    /* Keep the derived shareholder registry in step with the holdings this op
     * just moved. shareholders.txt is not a second store of ownership - it is a
     * reverse index REBUILT from every piece's holdings.txt (see
     * shareholder_registry.c:247 and its `rebuild` mode). Leaving it stale after
     * a fill is how the two disagree: found on a real playthrough, where the
     * registry still advertised player_you holding 55 shares after settlement
     * had traded them away, and a dividend would have been paid to the wrong
     * holder. Rebuilding is cheap (one pass over the pieces directory) and it is
     * the only thing that makes the index trustworthy.
     *
     * Invoked as another .+x, the established inter-op path. Windows quoting
     * matches system/prisc+x.c:1092-1097, because cmd.exe does not treat ' as a
     * quote character, so the single-quoted form silently fails to spawn. */
    if (total > 0) {
#ifdef _WIN32
        int rc = system("\"ops\\+x\\shareholder_registry.+x\" rebuild");
#else
        int rc = system("'./ops/+x/shareholder_registry.+x' rebuild");   /* run from the project root, like every other op call (was './+x/...', which only exists inside ops/) */
#endif
        if (rc != 0)
            fprintf(stderr, "market_settle: WARNING - could not rebuild the "
                            "shareholder registry; dividends will use a stale "
                            "index until it is rebuilt\n");
    }

    printf("market_settle: %d ticker(s) considered, %d fill(s) total\n", nt, total);
    return 0;
}

#pragma GCC diagnostic pop
