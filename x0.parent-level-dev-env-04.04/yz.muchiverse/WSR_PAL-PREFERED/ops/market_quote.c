/*
 * market_quote.c - analysts post orders into the auction book.
 *
 * This is the quoting half of the auction market; market_settle.c is the
 * matching half and is the ONLY thing that moves cash or holdings. Quoting never
 * writes balances, so an order can never create or destroy money by existing.
 *
 * WHO QUOTES
 *
 * Every discovered participant quotes against every traded ticker. Banks
 * (corporations whose industry is "bank") are the analysts in the sense that
 * they carry the valuation view and also lend (bank_loan_op.c), so an analyst
 * with a thin balance sheet can still take a position on credit - which is how
 * a short gets funded without magic money.
 *
 * Discovery is a directory scan, never a roster. analysis_loop.c:288-318 uses
 * opendir for the same reason: corporations and banks are added over time, and
 * a hardcoded list would silently exclude every entity added after it was
 * written. Adding corps and banks needs no change here.
 *
 * HOW A QUOTE IS FORMED
 *
 *     fair     = book_value / shares_outstanding   (the analyst's own view)
 *     momentum = recent price direction, from price_history.txt
 *     target   = fair * (1 + MOM_WEIGHT * momentum)
 *     bid      = target * (1 - spread)
 *     ask      = target * (1 + spread)
 *
 * The spread widens when the book is thin, which is how illiquidity becomes
 * visible in the price rather than being hidden by a formula.
 *
 * VALUATION IS A CEILING ON QUOTES, NOT THE PRICE
 *
 * Quotes are clamped to within WSR_VAL_ANCHOR of `fair`. A price may therefore
 * run above book when analysts and the herd bid it up, and may sit below book
 * when nobody wants it - both are reachable. What is NOT reachable is a quote
 * the analysts would never rationally make, which is the boundary that stops
 * positive feedback compounding to absurdity over a few hundred ticks. That is
 * a bound on the ANALYST'S REASONABLENESS, not a clamp on the price: a stock
 * whose last trade is far outside the band still reports that last trade, and
 * the gap is exactly the signal that analysts think it is over/undervalued.
 *
 * Self-contained, matching the rest of ops/.
 *
 * HOUSE STANDARD: NO SHARED HEADER, OPS COMMUNICATE VIA .+x AND STATE FILES
 *
 * The first draft of this op put its tuning constants in a shared
 * ops/wsr_market.h and #included it. That was wrong and is removed. The house
 * rule, stated in the khtpm-house-standards skill as "no linking to share
 * behavior across binaries - this house's standing rule everywhere, not just
 * here", means each op is its own self-contained binary that owns its own
 * constants. ops/ contained no header at all before this one; the siblings all
 * inline their own literals (bank_loan_op.c:120 "0.08", corp_action.c:136
 * "0.06"), and that duplication is intentional, not technical debt.
 *
 * The cost of that choice is real and accepted: a constant that must agree
 * across two ops (the spread here, and the one market_settle.c will need) can
 * drift. The house answer is that the shared truth is the DATA, not the code -
 * ops coordinate through the state files under projects/wsr-pal/, and
 * market_quote.c's book file is the contract between quoting and settlement. So
 * the constants below are private to this op, and anything genuinely shared
 * across the boundary travels in the files, not through a compile-time link.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#undef MAX_PATH
#endif

/* ---- tuning constants, owned by THIS op (see house-standard note above) ---- */

/* An analyst's quotes are bounded to within this fraction of their own
 * fair-value view, however strong the momentum. This bounds the ANALYST's
 * reasonableness, not the price: a last trade outside the band still reports,
 * and the gap is the signal that the market disagrees with the analysts.
 *
 * The value is not free. Momentum is clamped to [-1,+1] and enters at
 * WSR_MOM_WEIGHT, so `target` can only ever reach fair * (1 +/- WSR_MOM_WEIGHT).
 * For this clamp to be a real circuit-breaker rather than dead code, it MUST
 * be smaller than WSR_MOM_WEIGHT. At 0.35 it never fired, because the widest
 * reachable target was fair*1.25 against a band of fair*1.35 - the safety net
 * could not catch anything. 0.20 makes it bind: any average move beyond +80%
 * over the lookback pins the quote, so a runaway print cannot drag analysts'
 * quotes out with it. Keep WSR_VAL_ANCHOR < WSR_MOM_WEIGHT or delete it. */
#define WSR_VAL_ANCHOR 0.20f

/* Momentum contribution as a fraction of the recent move. */
#define WSR_MOM_WEIGHT 0.25f

/* Bid-ask spread as a fraction of price, widened when the book is thin. */
#define WSR_SPREAD_BASE 0.01f
#define WSR_SPREAD_THIN 0.08f

/* Resting shares below this on a side makes the book "thin", which widens the
 * spread - this is how illiquidity becomes visible in the price. */
#define WSR_THIN_DEPTH 50

/* Largest fraction of a participant's cash one order may commit, so no single
 * participant can clear the market in a tick. */
#define WSR_MAX_ORDER_FRACTION 0.25f

/* Widest a single participant's view may sit from the analyst fair value,
 * before its own skill scales it. This is the dispersion that makes the market
 * a market: with every participant holding an identical view there is nothing
 * to disagree about, every bid lands below every ask, and the book can never
 * cross - the auction direction is inert. The dispersion is what creates
 * genuine buyers and sellers.
 *
 * A second purpose, and the reason it is not simply zero: the spread and the
 * momentum anchor alone cannot express uncertainty, so they cannot express a
 * participant who is unsure. Skill carries that. */
#define WSR_VIEW_DISPERSION 0.30f

/* A view within this fraction of fair value counts as no view at all, and the
 * participant sits the tick out rather than posting a coin-flip order. */
#define WSR_VIEW_DEADBAND 0.01f

/* How many prior prints inform the momentum term. */
#define WSR_MOMENTUM_LOOKBACK 3

/* Order sides and event names. Private to this op; the settlement op parses the
 * book file's own field layout, which is the actual contract between them. */
#define WSR_SIDE_BID 1
#define WSR_SIDE_ASK (-1)

#define PATHBUF 1400
#define MAXLINE 1024
#define MAXTICK 32
#define MAXPIECE 128
#define MAXPART 256
#define MAXTICKERS 512

static char g_root[PATHBUF];

/* ---------- small path/state helpers (same conventions as sibling ops) ------ */

/* Every path in this op is built from PRISC_PROJECT_ROOT plus a fixed relative
 * suffix, and PRISC_PROJECT_ROOT is an arbitrarily long OneDrive path in this
 * checkout. gcc's -Wformat-truncation therefore fires on the *worst case* of a
 * root that could not fit, which is true of every op in this family and is
 * suppressed the same way in corp_apply_finances.c:44/96/189 and
 * econ_calendar.c. snprintf still truncates safely; the concern is only that a
 * truncated path would silently point at the wrong file, so the root is capped
 * well below the buffer below and any path that would overflow is reported
 * rather than used. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        /* Refuse an over-long root rather than truncating it. A truncated root
         * would silently point every path below at the wrong file, which is far
         * worse than refusing to run: the market would appear to quote against
         * nothing. */
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr,
                    "market_quote: PRISC_PROJECT_ROOT is too long (%zu bytes, max %zu) - "
                    "refusing to run rather than truncate a path\n",
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

static int read_field(const char *path, const char *key, char *out, size_t n) {
    FILE *f = fopen(path, "r");
    char line[MAXLINE];
    size_t klen = strlen(key);
    out[0] = '\0';
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            char *nl = strpbrk(v, "\r\n");
            if (nl) *nl = '\0';
            snprintf(out, n, "%s", v);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static float field_f(const char *path, const char *key) {
    char buf[64];
    if (!read_field(path, key, buf, sizeof(buf))) return 0.0f;
    return (float)atof(buf);
}

static int is_dir(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* ---------- participants and tickers, both discovered, never hardcoded ----- */

typedef struct {
    char id[MAXPIECE];
    float cash;
    int is_bank;
    float skill;   /* how tight this participant's view is, 1.0 = baseline */
  } Participant;

static Participant g_parts[MAXPART];
static int g_nparts = 0;

static char g_tickers[MAXTICKERS][MAXTICK];
static int g_ntickers = 0;

static int is_traded_ticker(const char *t) {
    for (int i = 0; i < g_ntickers; i++)
        if (strcmp(g_tickers[i], t) == 0) return 1;
    return 0;
}

static int ticker_exists_on_disk(const char *t) {
    char p[PATHBUF];
    snprintf(p, sizeof(p), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", g_root, t);
    FILE *f = fopen(p, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Tickers come from the shareholders cap table, which is itself built by
 * scanning every piece directory. Anything another participant actually holds is
 * therefore automatically tradeable, including corps added after this op was
 * written. */
static void load_tickers(void) {
    char path[PATHBUF];
    FILE *f;
    char line[MAXLINE];

    snprintf(path, sizeof(path), "%s/projects/wsr-pal/shareholders.txt", g_root);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && g_ntickers < MAXTICKERS) {
        char ticker[MAXTICK];
        /* rows are "<ticker>|<shares>" or "<corp_ticker>|<shares>" */
        if (sscanf(line, " %31[^|]", ticker) != 1) continue;
        if (ticker[0] == '#' || ticker[0] == '\n') continue;
        if (strncmp(ticker, "corp_", 5) == 0) memmove(ticker, ticker + 5, strlen(ticker) - 5 + 1);
        if (!*ticker) continue;
        if (!is_traded_ticker(ticker) && ticker_exists_on_disk(ticker))
            snprintf(g_tickers[g_ntickers++], MAXTICK, "%s", ticker);
    }
    fclose(f);
}

static void load_participants(void) {
    char pieces_root[PATHBUF];
    static const char *prefixes[] = {"corp_", "player_", "pop_", "gov_", NULL};
    DIR *d;
    struct dirent *e;

    snprintf(pieces_root, sizeof(pieces_root), "%s/projects/wsr-pal/pieces", g_root);
    d = opendir(pieces_root);
    if (!d) return;

    while ((e = readdir(d)) != NULL && g_nparts < MAXPART) {
        int match = 0;
        char state[PATHBUF], industry[64];
        float skill;
        for (int i = 0; prefixes[i]; i++) {
            size_t pl = strlen(prefixes[i]);
            if (strncmp(e->d_name, prefixes[i], pl) == 0) { match = 1; break; }
        }
        if (!match || e->d_name[0] == '.') continue;

        char full[PATHBUF];
        snprintf(full, sizeof(full), "%s/%s", pieces_root, e->d_name);
        if (!is_dir(full)) continue;

        piece_state(state, sizeof(state), e->d_name);
        if (!read_field(state, "current_state", industry, sizeof(industry))) continue; /* must be a live piece */

        read_field(state, "industry", industry, sizeof(industry));

        snprintf(g_parts[g_nparts].id, MAXPIECE, "%s", e->d_name);
        g_parts[g_nparts].cash = field_f(state, "cash");
        g_parts[g_nparts].is_bank = (strcmp(industry, "bank") == 0);

        /* Information asymmetry, by participant type. A bank runs a real
         * valuation model; a population piece has no model at all and is
         * guessing from rumour. ECONOMY-INTENT.md calls for exactly this -
         * "disagreement and information asymmetry create alpha" - so the
         * quality of a participant's view is a property of WHO IS ASKING, not
         * a global constant applied to everyone. Lower skill means the view
         * wanders further from the fundamental. */
        g_parts[g_nparts].skill = 1.0f;
        if (g_parts[g_nparts].is_bank)                    skill = 0.45f;
        else if (strncmp(e->d_name, "corp_", 5) == 0)     skill = 0.80f;
        else if (strncmp(e->d_name, "player_", 7) == 0)  skill = 1.50f;
        else if (strncmp(e->d_name, "pop_", 4) == 0)     skill = 2.20f;
        else                                              skill = 1.20f;  /* gov_ */
        g_parts[g_nparts].skill = skill;

        g_nparts++;
    }
    closedir(d);
}

/* ---------- valuation, momentum, spread ------------------------------------ */

/* Recent price direction, in [-1, +1], from the tail of price_history.txt.
 * Rows are: turn,old,new,change% (written by corp_update_price). Positive
 * momentum pulls quotes UP, so a hot name attracts further bids. */
static float read_momentum(const char *ticker) {
    char path[PATHBUF];
    FILE *f;
    char line[MAXLINE];
    float changes[WSR_MOMENTUM_LOOKBACK];
    int n = 0;

    snprintf(path, sizeof(path), "%s/projects/wsr-pal/pieces/corp_%s/price_history.txt",
             g_root, ticker);
    f = fopen(path, "r");
    if (!f) return 0.0f;
    while (fgets(line, sizeof(line), f) && n < WSR_MOMENTUM_LOOKBACK) {
        double t, oldp, newp, pct;
        if (sscanf(line, "%lf,%lf,%lf,%lf%%", &t, &oldp, &newp, &pct) == 4)
            changes[n++] = (float)pct / 100.0f;
    }
    fclose(f);
    if (n == 0) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += changes[i];
    float avg = sum / (float)n;
    if (avg > 1.0f) avg = 1.0f;
    if (avg < -1.0f) avg = -1.0f;
    return avg;
}

/* Fundamentally fair value per share. This is the analyst's VIEW, an input to
 * quoting. It is never written to the price. */
static float fair_value(const char *ticker) {
    char state[PATHBUF];
    float book, shares;
    piece_state(state, sizeof(state), "CORP_PLACEHOLDER");
    snprintf(state, sizeof(state), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", g_root, ticker);
    book = field_f(state, "book_value");
    shares = field_f(state, "shares_outstanding");
    /* shares_outstanding is a MILLIONS-scaled market-profile figure, not a whole
     * share count (PORT-FIDELITY.md gap 8). The ratio is nonetheless the
     * original's own basis - analysis_loop.c uses exactly book_value /
     * shares_outstanding for its fundamental - so it is kept as-is rather than
     * "corrected" into a different unit. */
    if (shares <= 0.0f) return 0.0f;
    return book / shares;
}

/* Size of resting interest on one side of a ticker, used for the thin-book
 * spread. Read back from the book file so quoting sees the market that is
 * actually there rather than the one it hoped for. */
static float resting_depth(const char *ticker, int side) {
    char path[PATHBUF];
    FILE *f;
    char line[MAXLINE];
    float total = 0.0f;

    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/book_%s.txt", g_root, ticker);
    f = fopen(path, "r");
    if (!f) return 0.0f;
    while (fgets(line, sizeof(line), f)) {
        char side_s[8];
        float sz;
        int t;
        char who[MAXPIECE];
        if (sscanf(line, "%7[^|]|%d|%*[^|]|%f|%127s", side_s, &t, &sz, who) == 4) {
            if (t == side) total += sz;
        }
    }
    fclose(f);
    return total;
}

/* Order size: bounded so no single participant clears the market in one tick. */
static float order_size(float cash, float price) {
    if (price <= 0.0f) return 0.0f;
    float budget = cash * WSR_MAX_ORDER_FRACTION;
    float shares = budget / price;
    if (shares < 1.0f) return 0.0f;
    return shares;
}

/* The suppression runs to end of file on purpose: main() builds the same
 * root-relative paths as the helpers above, so narrowing the scope to the
 * helpers just moves the warnings rather than fixing anything. */

/* This participant's OWN view of a ticker, as a multiplier on the analyst
 * fair value. Dispersion is a deterministic hash of the piece id, NOT a random
 * number: the same participant must hold the same view on every run, or the
 * market would be unfalsifiable and a divergence could never be diagnosed. The
 * hash is stable across machines and runs, which rand()/time() would not be.
 *
 * The hash is folded to [-1,+1] and scaled by both the global dispersion
 * ceiling and this participant's skill, so a bank's view sits close to the
 * fundamental and a population piece's is wide. */
static float view_multiplier(const char *id, float skill) {
    unsigned h = 2166136261u;
    for (const char *q = id; *q; q++) {
        h ^= (unsigned char)*q;
        h *= 16777619u;
    }
    /* Top 24 bits -> [0,1), then to [-1,+1]. */
    float u = (float)((h >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
    float signed_bias = (u * 2.0f) - 1.0f;
    float m = 1.0f + signed_bias * WSR_VIEW_DISPERSION * skill;
    return (m < 0.05f) ? 0.05f : m;
}

int main(void) {
    FILE *bf;
    int posted = 0, skipped = 0;

    resolve_root();

    load_tickers();
    load_participants();

    if (g_ntickers == 0) {
        printf("market_quote: no traded tickers found (shareholders.txt empty?)\n");
        return 0;
    }
    if (g_nparts == 0) {
        printf("market_quote: no live participants found under pieces/\n");
        return 0;
    }

    /* No single book file is opened here. The book is per-ticker and is opened
     * inside the loop below with "w", so each tick replaces it wholesale: it
     * holds ORDERS, not balances, and every order here is new this tick. The
     * ledger is the durable record; the book is only a snapshot of intent.
     *
     * An earlier draft kept a "truncate the book" fopen here against a path
     * buffer that was declared and never assigned, so it opened an
     * uninitialized stack buffer as a filename. Undefined behaviour with a
     * real downside - "w" truncates whatever that garbage happens to name. */

    for (int t = 0; t < g_ntickers; t++) {
        const char *ticker = g_tickers[t];
        char path[PATHBUF], state[PATHBUF];
        float fair, mom, target, spread, lo, hi;
        float last_price;
        /* best_ask must start ABOVE every possible price. Starting it at 0 made
         * the "keep the smaller" comparison below never fire, so the summary
         * printed ask=0.00 on a book that plainly had asks on it. best_bid
         * starts at 0 because bids are positive and "keep the larger" works
         * from there. */
        float best_bid = 0.0f, best_ask = 1e30f;
        int n_bid = 0, n_ask = 0, no_view = 0;

        fair = fair_value(ticker);
        if (fair <= 0.0f) {
            /* Say so instead of dropping the row. A held ticker with no
             * quotable value is a real state - a freshly founded corporation
             * has no book_value yet, and a bank has no corp_ fair-value basis
             * at all - and reporting "0 orders posted" with no explanation
             * reads like a crash rather than an unvaluable name. */
            skipped++;
            printf("%-6s SKIPPED: no fair value (no book_value/shares_outstanding "
                   "in pieces/corp_%s/state.txt)\n", ticker, ticker);
            continue;
        }

        mom = read_momentum(ticker);

        target = fair * (1.0f + WSR_MOM_WEIGHT * mom);
        if (target <= 0.0f) continue;

        /* Ceiling on how far a quote may sit from the analyst's own view. */
        lo = fair * (1.0f - WSR_VAL_ANCHOR);
        hi = fair * (1.0f + WSR_VAL_ANCHOR);
        if (target < lo) target = lo;
        if (target > hi) target = hi;

        /* Thin book -> wide spread. This is the mechanism that lets a stock sit
         * below book: if no one has shown a bid, the spread opens up and the
         * offer lands far under fair value. It is applied to each participant's
         * OWN view below, not to one shared price. */
        float bid_depth = resting_depth(ticker, WSR_SIDE_BID);
        float ask_depth = resting_depth(ticker, WSR_SIDE_ASK);
        spread = (bid_depth < WSR_THIN_DEPTH || ask_depth < WSR_THIN_DEPTH)
                     ? WSR_SPREAD_THIN
                     : WSR_SPREAD_BASE;

        snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/book_%s.txt", g_root, ticker);
        bf = fopen(path, "w");
        if (!bf) {
            fprintf(stderr, "market_quote: cannot write %s\n", path);
            continue;
        }

        snprintf(state, sizeof(state), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", g_root, ticker);
        last_price = field_f(state, "stock_price");

        for (int i = 0; i < g_nparts; i++) {
            /* Each participant quotes from ITS OWN view, and posts on the ONE
             * side that view implies. A participant that thinks the stock is
             * cheap bids; one that thinks it is dear asks. It does not post
             * both sides at once: that is how the previous version ended up
             * with a book where every bid sat below every ask and nothing
             * could ever trade. One side, chosen by conviction, is also what
             * actually happens - a participant is either a buyer or a seller,
             * and market_settle refuses self-trades anyway. */
            float m = view_multiplier(g_parts[i].id, g_parts[i].skill);
            float view = target * m;

            if (fabsf(view - target) <= target * WSR_VIEW_DEADBAND) {
                no_view++;
                continue;   /* genuinely undecided - not a view, so no order */
            }

            int side = (view > target) ? 1 : -1;
            float px = (side == 1) ? view * (1.0f - spread)
                                   : view * (1.0f + spread);
            if (px < 0.01f) px = 0.01f;

            float sz = order_size(g_parts[i].cash, px);
            if (sz <= 0.0f) continue;

            fprintf(bf, "%s|%d|%.4f|%.2f|%s\n",
                    (side == 1) ? "bid" : "ask", side, px, sz, g_parts[i].id);
            posted++;
            if (side == 1) {
                n_bid++;
                if (px > best_bid) best_bid = px;
            } else {
                n_ask++;
                if (px < best_ask) best_ask = px;
            }
        }
        fclose(bf);

        /* Report the crossing explicitly. "Could these orders trade?" is the
         * single most useful thing to know about a book, and when the answer
         * is no the reason is worth naming rather than leaving the operator to
         * work out that every bid is below every ask. */
        printf("%-6s fair=%8.2f target=%8.2f mom=%+6.2f%% bid=%8.2f ask=%8.2f "
               "n=%d/%d last=%8.2f%s\n",
               ticker, fair, target, mom * 100.0f,
               (n_bid > 0) ? best_bid : 0.0f,
               (n_ask > 0) ? best_ask : 0.0f,
               n_bid, n_ask, last_price,
               (last_price > 0.0f && last_price < fair * 0.98f) ? "  [below BVP]" : "");
        if (no_view)
            printf("%-6s   %d participant(s) had no view and sat the tick out\n",
                   ticker, no_view);
        if (n_bid > 0 && n_ask > 0 && best_bid < best_ask)
            printf("%-6s   book is NOT crossed (best bid %.2f < best ask %.2f) "
                   "- nothing will trade\n", ticker, best_bid, best_ask);
    }

    printf("market_quote: %d participant(s), %d ticker(s) quoted, %d skipped, "
           "%d order(s) posted\n",
           g_nparts, g_ntickers - skipped, skipped, posted);
    return 0;
}

#pragma GCC diagnostic pop
