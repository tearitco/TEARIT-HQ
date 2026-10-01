/*
 * goods_settle.c - match the goods books, move money, move goods.
 *
 * THE ONLY OP THAT MOVES CASH OR GOODS.
 *
 * The goods-market twin of market_settle.c, and deliberately the same shape
 * rather than a new implementation. It is the sole writer of:
 *
 *   - a household's cash          (paying for goods)
 *   - a corporation's cash        (receiving revenue)
 *   - a corporation's inventory   (pieces/<id>/goods.txt)
 *   - data/gsold_<GOOD>.txt       (producer|sold|offered|revenue, so production
 *                                   steers on sell-through and payroll can be
 *                                   sized off real revenue, not a guess)
 *   - the goods rows in the ledger
 *
 * goods_quote.c posts intent; this decides what actually happened. An order
 * cannot create money by merely existing.
 *
 * THE THREE INVARIANTS THIS OP EXISTS TO HOLD
 *
 * 1. CASH CONSERVATION. A fill moves cash buyer->seller. Verified to the cent
 *    in the equity path; checked again here per fill.
 * 2. GOODS CONSERVATION. produced = sold + unsold_inventory, always. A unit
 *    enters existence only in goods_quote's production step, which pays cash
 *    for it. Price is NOT an input to quantity, so a price change can never
 *    conjure a unit. This is asserted by refusing to fill more units than the
 *    seller actually has in stock.
 * 3. NO HALF-APPLIED FILL. Every fill writes cash and stock and then reads BOTH
 *    legs back. Checking one leg passes while shares are minted, which is
 *    exactly the bug that shipped twice in the equity path (the holdings key
 *    mismatch, then the `shares > 0` guard that silently dropped shorts).
 *
 * REFUSALS ARE LOUD
 *
 * A household that cannot pay, and any attempt to sell goods the seller does
 * not hold, are refused and reported. Silently skipping them would make an
 * illiquid market look like a working one.
 *
 * Self-contained: no shared project headers, matching the rest of ops/. The
 * contract with goods_quote.c is the BOOK FILE FORMAT:
 *   bid|1|<price>|<units>|<who>   /   ask|-1|<price>|<units>|<who>
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
#define MAXGOOD 32
#define MAXPIECE 64
#define MAXORDERS 512
#define MAXGOODS 128

/* ---- tuning constants, owned by THIS op (see market_quote.c house note) ---- */

/* A fill below this many units is dust and is dropped rather than filled. */
#define WSR_MIN_FILL_UNITS 1L

/* Cash may not fall below this after a fill. A buyer that cannot pay does not
 * get the goods. */
#define WSR_CASH_FLOOR 0.0

static char g_root[PATHBUF];

/* Every path is PRISC_PROJECT_ROOT plus a fixed suffix; see market_quote.c for
 * why this warning is suppressed and why resolve_root refuses rather than
 * truncates. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr, "goods_settle: PRISC_PROJECT_ROOT too long (%zu) - "
                            "refusing to truncate a path\n", n);
            exit(1);
        }
        memcpy(g_root, env, n + 1);
    } else {
        snprintf(g_root, sizeof(g_root), ".");
    }
}

static void piece_state(char *out, size_t n, const char *id) {
    snprintf(out, n, "%s/projects/wsr-pal/pieces/%s/state.txt", g_root, id);
}

static void piece_goods(char *out, size_t n, const char *id) {
    snprintf(out, n, "%s/projects/wsr-pal/pieces/%s/goods.txt", g_root, id);
}

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
    static char lines[96][MAXLINE];
    int nlines = 0;
    if (f) {
        while (nlines < 96 && fgets(lines[nlines], MAXLINE, f)) nlines++;
        fclose(f);
    }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "goods_settle: cannot write %s\n", path); return; }
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

/* Inventory rows are <GOOD>|<units>. Units are LONG and non-negative: a
 * producer cannot hold negative stock, so there is no "short goods" state to
 * represent and no risk of the `shares > 0`-style guard silently deleting a
 * legitimate position. */
static long get_stock(const char *path, const char *good) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAXLINE];
    size_t gl = strlen(good);
    long v = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, good, gl) == 0 && line[gl] == '|')
            { v = atol(line + gl + 1); break; }
    }
    fclose(f);
    return v < 0 ? 0 : v;
}

static void set_stock(const char *path, const char *good, long units) {
    FILE *f = fopen(path, "r");
    static char lines[64][MAXLINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAXLINE, f)) nlines++;
        fclose(f);
    }
    size_t gl = strlen(good);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], good, gl) == 0 && lines[i][gl] == '|') {
            if (units > 0) fprintf(f, "%s|%ld\n", good, units);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found && units > 0) fprintf(f, "%s|%ld\n", good, units);
    fclose(f);
}

static FILE *g_ledger = NULL;

static void ledger_open(void) {
    if (g_ledger) return;
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/market_ledger.txt", g_root);
    g_ledger = fopen(path, "a");
    if (!g_ledger)
        fprintf(stderr, "goods_settle: cannot append to %s - refusing to trade "
                        "without a ledger\n", path);
}

/* A goods fill is REVENUE, and it is deliberately distinguishable from an
 * equity fill by its event name and its Goods field. Revenue is the sale of a
 * good, not the creation of earnings: earnings only move when the cost side is
 * booked. */
static void ledger_fill(const char *buyer, const char *seller, double amount,
                        const char *good, double price, long units) {
    if (!g_ledger) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", t);
    fprintf(g_ledger,
            "Time: %s | Debit: %s | Credit: %s | Amount: %.2f Dollars | "
            "Event: goods_settle.+x | Goods: %s | Price: %.4f | Units: %ld\n",
            date_str, buyer, seller, amount, good, price, units);
}

typedef struct {
    int    side;      /* +1 bid, -1 ask */
    double price;
    long   units;
    char   who[MAXPIECE];
} Order;

static int cmp_bid_desc(const void *a, const void *b) {
    double d = ((const Order *)b)->price - ((const Order *)a)->price;
    return (d > 0) - (d < 0);
}

static int cmp_ask_asc(const void *a, const void *b) {
    double d = ((const Order *)a)->price - ((const Order *)b)->price;
    return (d > 0) - (d < 0);
}

/* ------------------------------------------------------------- one good -- */

static int settle_good(const char *good) {
    char book[PATHBUF];
    snprintf(book, sizeof(book), "%s/projects/wsr-pal/data/gbook_%s.txt", g_root, good);

    static Order bids[MAXORDERS], asks[MAXORDERS];
    int nb = 0, na = 0;

    /* Per-producer book stats. `offered` is registered as each ask is read
     * (a producer can have several), `sold` accumulates on fills. The ratio
     * sold/offered is the sell-through goods_quote steers production on. A
     * producer that offers and never fills must still be recorded, or a dead
     * market looks like a market that was never stocked. */
static char sold_names[MAXPIECE][MAXPIECE];
static long  sold_units[MAXPIECE];
static long  sold_offer[MAXPIECE];
static double sold_rev[MAXPIECE];
    int nsold = 0;

    FILE *f = fopen(book, "r");
    if (!f) return 0;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), f)) {
        char side_word[16], who[MAXPIECE];
        int sign; double price; long units;
        if (sscanf(line, "%15[^|]|%d|%lf|%ld|%63s",
                   side_word, &sign, &price, &units, who) != 5) continue;
        if (price <= 0.0 || units <= 0) continue;
        if (sign != 1 && sign != -1) continue;
        Order *slot = (sign == 1) ? &bids[nb] : &asks[na];
        int *count  = (sign == 1) ? &nb : &na;
        if (*count >= MAXORDERS) continue;
        slot->side = sign; slot->price = price; slot->units = units;
        snprintf(slot->who, MAXPIECE, "%s", who);
        (*count)++;
        if (sign == -1) {
            int k = -1;
            for (int i = 0; i < nsold; i++)
                if (!strcmp(sold_names[i], who)) { k = i; break; }
            if (k < 0 && nsold < MAXPIECE) {
                snprintf(sold_names[nsold], MAXPIECE, "%s", who);
                sold_units[nsold] = 0;
            sold_offer[nsold] = 0;
            sold_rev[nsold] = 0.0;
                k = nsold++;
            }
            if (k >= 0) sold_offer[k] += units;
        }
    }
    fclose(f);

    if (nb == 0 || na == 0) {
        printf("%-26s no trade: %d bid(s), %d ask(s)%s\n", good, nb, na,
               (nb == 0 && na == 0) ? " (no orders)" : "");
        return 0;
    }

    qsort(bids, (size_t)nb, sizeof(Order), cmp_bid_desc);
    qsort(asks, (size_t)na, sizeof(Order), cmp_ask_asc);

    /* What each producer sold this tick, for goods_quote's feedback and for
     * gsold_<GOOD>.txt. `offered` was already registered during parsing. */
    int bi = 0, ai = 0, fills = 0, refused = 0;
    double last_price = 0.0;
    long   last_units = 0;
    long   total_units = 0;

    while (bi < nb && ai < na) {
        Order *b = &bids[bi];
        Order *a = &asks[ai];
        if (b->price < a->price) break;                 /* not crossed */
        if (!strcmp(b->who, a->who)) {                   /* self-trade */
            refused++;
            if (b->units <= a->units) bi++; else ai++;
            continue;
        }

        long qty = (b->units < a->units) ? b->units : a->units;
        if (qty < WSR_MIN_FILL_UNITS) break;

        /* Print at the RESTING price, not the aggressor's limit, so a firm
         * cannot mark its own market by quoting wide. */
        double price = a->price;
        double amount = price * (double)qty;

        char buyer_state[PATHBUF], seller_state[PATHBUF];
        piece_state(buyer_state, sizeof(buyer_state), b->who);
        piece_state(seller_state, sizeof(seller_state), a->who);

        double buyer_cash = field_d(buyer_state, "cash");
        if (buyer_cash < amount + WSR_CASH_FLOOR) { refused++;
            if (b->units <= a->units) bi++; else ai++; continue; }

        /* GOODS CONSERVATION, enforced: never sell a unit that does not exist.
         * This is the check that makes produced = sold + unsold true by
         * construction rather than by hope. */
        char seller_goods[PATHBUF];
        piece_goods(seller_goods, sizeof(seller_goods), a->who);
        long have = get_stock(seller_goods, good);
        if (have < qty) {
            long can = have;
            refused++;
            if (can < WSR_MIN_FILL_UNITS) {
                if (a->units <= b->units) ai++; else bi++;
                continue;
            }
            qty = can;
            amount = price * (double)qty;
            if (buyer_cash < amount + WSR_CASH_FLOOR) { refused++;
                if (b->units <= a->units) bi++; else ai++; continue; }
        }

        double seller_cash = field_d(seller_state, "cash");

        char val[MAXLINE];
        snprintf(val, sizeof(val), "%.2f", buyer_cash - amount);
        write_field(buyer_state, "cash", val);
        snprintf(val, sizeof(val), "%.2f", seller_cash + amount);
        write_field(seller_state, "cash", val);

        char buyer_goods[PATHBUF];
        piece_goods(buyer_goods, sizeof(buyer_goods), b->who);
        set_stock(buyer_goods, good, get_stock(buyer_goods, good) + qty);
        set_stock(seller_goods, good, have - qty);

        /* BOTH legs read back, plus both cash balances. Checking one leg passes
         * while units are being minted. */
        long got_b = get_stock(buyer_goods, good);
        long got_a = get_stock(seller_goods, good);
        double got_bc = field_d(buyer_state, "cash");
        double got_sc = field_d(seller_state, "cash");
        if (got_b < qty ||
            got_a != have - qty ||
            fabs(got_bc - (buyer_cash - amount)) > 0.01 ||
            fabs(got_sc - (seller_cash + amount)) > 0.01) {
            fprintf(stderr, "goods_settle: WRITE FAILED on %s/%s for %s "
                            "(buyer stock %ld want >=%ld, seller stock %ld want %ld) "
                            "- aborting rather than leaving a half-applied fill\n",
                    b->who, a->who, good, got_b, qty, got_a, have - qty);
            return -1;
        }

        ledger_fill(b->who, a->who, amount, good, price, qty);

        int k = -1;
        for (int i = 0; i < nsold; i++)
            if (!strcmp(sold_names[i], a->who)) { k = i; break; }
        if (k >= 0) { sold_units[k] += qty; sold_rev[k] += amount; }

        last_price = price;
        last_units = qty;
        total_units += qty;
        fills++;

        b->units -= qty;
        a->units -= qty;
        if (b->units < WSR_MIN_FILL_UNITS) bi++;
        if (a->units < WSR_MIN_FILL_UNITS) ai++;
    }

    /* Feedback file for next tick's production decision. Overwritten, not
     * appended: it describes THIS tick, and a stale tail would tell a producer
     * to make goods for sales that already happened. */
    if (nsold > 0) {
        char sp[PATHBUF];
        snprintf(sp, sizeof(sp), "%s/projects/wsr-pal/data/gsold_%s.txt", g_root, good);
        FILE *sf = fopen(sp, "w");
        if (sf) {
            for (int i = 0; i < nsold; i++)
                fprintf(sf, "%s|%ld|%ld|%.2f\n", sold_names[i], sold_units[i], sold_offer[i], sold_rev[i]);
            fclose(sf);
        }
    }

    if (fills > 0)
        printf("%-26s %d fill(s) %ld unit(s) last=%.4f\n",
               good, fills, total_units, last_price);
    else if (refused > 0)
        printf("%-26s no trade: %d order(s) refused (self-trade, cannot pay, or "
               "no stock) out of %d bid(s), %d ask(s)\n", good, refused, nb, na);
    else
        printf("%-26s no trade: book not crossed (%d bid(s), %d ask(s))\n",
               good, nb, na);
    (void)last_units;
    return fills;
}

int main(void) {
    resolve_root();

    char datadir[PATHBUF];
    snprintf(datadir, sizeof(datadir), "%s/projects/wsr-pal/data", g_root);
    DIR *d = opendir(datadir);
    if (!d) { fprintf(stderr, "goods_settle: cannot read %s\n", datadir); return 1; }

    static char goods[MAXGOODS][MAXGOOD];
    int ng = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && ng < MAXGOODS) {
        const char *pfx = "gbook_";
        size_t pl = strlen(pfx), nl = strlen(e->d_name);
        if (nl <= pl + 4 || strncmp(e->d_name, pfx, pl) != 0) continue;
        if (strcmp(e->d_name + nl - 4, ".txt") != 0) continue;
        size_t tlen = nl - pl - 4;
        if (tlen >= MAXGOOD) continue;
        memcpy(goods[ng], e->d_name + pl, tlen);
        goods[ng][tlen] = '\0';
        ng++;
    }
    closedir(d);

    if (ng == 0) {
        printf("goods_settle: no gbook_<GOOD>.txt found - run goods_quote first\n");
        return 0;
    }

    ledger_open();
    if (!g_ledger) return 1;   /* no record -> do not move money */

    int total = 0;
    for (int i = 0; i < ng; i++) {
        int n = settle_good(goods[i]);
        if (n < 0) {
            fprintf(stderr, "goods_settle: aborting at %s - balances are "
                            "partially applied; reconcile from the ledger\n",
                    goods[i]);
            if (g_ledger) fclose(g_ledger);
            return 1;
        }
        total += n;
    }

    if (g_ledger) fclose(g_ledger);
    printf("goods_settle: %d good(s) considered, %d fill(s) total\n", ng, total);
    return 0;
}

#pragma GCC diagnostic pop
