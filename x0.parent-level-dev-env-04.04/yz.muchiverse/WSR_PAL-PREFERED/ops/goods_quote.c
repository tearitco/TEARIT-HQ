/*
 * goods_quote.c - producers make goods and offer them; households bid for them.
 *
 * Second leg of the operating-income model, and the thing that makes a
 * corporation able to earn anything at all. See docs/OPERATING-INCOME.md.
 *
 * THE MARKET ENGINE IS NOT REBUILT HERE. This is the same double auction as
 * market_quote.c, applied to a second asset class. The order row format, the
 * book file convention, the deterministic per-participant view and the
 * crossing rule are deliberately identical, because a second market
 * implementation is a second set of bugs:
 *
 *   bid|1|<price>|<units>|<who>      ->  data/gbook_<GOOD>.txt
 *   ask|-1|<price>|<units>|<who>
 *
 * goods_settle.c is the only writer of balances and the only place a price is
 * decided. As in the equity path, an order cannot create money by existing.
 *
 * WHAT A GOOD IS
 *
 * A good is a corporation's industry group, taken from the real legacy profile
 * line "Industry Group:" (AFL.txt: "Industry Group:  SHIPPING") and stored once
 * at creation as `produces=` in the corp's state.txt. That yields 29 distinct
 * goods across the 50-corp roster, with 6 insurance corps, 4 internet corps, 3
 * air transport and so on.
 *
 * A single FOOD good was the obvious simplification and it was wrong: the roster
 * contains exactly ONE agriculture corp, so a single good gives a 1-producer
 * market - the same one-ticker dead end the equity market already had. Real
 * breadth comes from the real taxonomy.
 *
 * PRODUCTION IS A CASH EXPENDITURE
 *
 * Producing spends cash and creates units. That is the only sense in which a
 * unit comes into existence, and it is why goods conservation is checkable:
 * produced = sold + unsold_inventory, always. A price change never creates a
 * unit, because price is not an input to production.
 *
 * Production follows DEMAND, and steers on sell-through rather than volume:
 * a producer that sold out last tick grows, one that could not move what it
 * offered cuts back, and one with no demand at all stops and spends no cash. A
 * producer nobody buys from therefore runs its inventory down and stops - the
 * correction mechanism the whole model depends on.
 *
 * Self-contained: no shared project headers, matching the rest of ops/. The
 * contract between this op and goods_settle.c is the BOOK FILE FORMAT above.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#undef MAX_PATH
#endif

#define PATHBUF 1400
#define MAXLINE 512
#define MAXGOOD 32
#define MAXPIECE 64
#define MAXGOODS 128
#define MAXPART 256

/* ---- tuning constants, owned by THIS op (see market_quote.c house note) ---- */

/* Cash cost of producing one unit. This is a CONVERSION COST, not a price: it
 * says what it costs a firm to bring a unit into existence, and it is the floor
 * its ask must clear to be worth producing at all. It is deliberately a plain
 * constant rather than anything scaled off book_value or share counts, because
 * the house rule is not to commit to a unit scale until one is needed
 * (OPERATING-INCOME.md §5.1). Override by rebuilding with -DWSR_PRODUCE_COST. */
#ifndef WSR_PRODUCE_COST
#define WSR_PRODUCE_COST 1.0f
#endif

/* How a producer scales production against last tick's sell-through ratio.
 *
 * The obvious rule - "produce half of what I sold" - is not just wrong, it is
 * DEADLY: a firm that sells everything it offers will see produce < sell-through
 * every single tick, so inventory decays 100%, 50%, 25%... and the market is
 * empty by tick 6. That was measured, not assumed.
 *
 * The fix is to steer on the RATIO rather than the level. A producer that sold
 * out gets to grow; one that could not sell what it offered gets cut back.
 * Multiplying last tick's sales by (WSR_SELLTHROUGH_BASE + sell-through) makes
 * that self-correcting, with no fixed point to fall through:
 *   sold out (1.00)      -> x1.50, grows into unmet demand
 *   half sold (0.50)     -> x1.00, holds steady
 *   10% sold (0.10)      -> x0.60, backs off and runs inventory down
 *   no demand (0.00)     -> 0 units, spends no cash (correct: idle producer)
 */
#define WSR_SELLTHROUGH_BASE 0.50f

/* Ceiling on units produced in one tick, so a single tick cannot mint the
 * whole order book in one go and wreck the cash side. */
#define WSR_MAX_PRODUCE 500

/* Mark-up over cost before a producer's own view is applied. A firm that cannot
 * ask above its cost has no business selling, so the floor is cost. */
#define WSR_BASE_MARGIN 0.25f

/* Widest a participant's view may sit from that mark-up, before its own skill
 * scales it. Same role as WSR_VIEW_DISPERSION in market_quote.c: this is the
 * disagreement that makes a market a market. */
#define WSR_GOODS_DISPERSION 0.40f

/* Fraction of a producer's cash it may commit to a production run, so it cannot
 * spend itself into insolvency to fulfil one order. */
#define WSR_MAX_PRODUCE_FRACTION 0.25f

/* Fraction of a household's cash it may commit to any ONE good. Without a cap
 * a household bids its whole balance on the first good it looks at and is then
 * broke for everything else. */
#define WSR_MAX_BUY_FRACTION 0.05f

/* A view within this fraction of the reference counts as no view, and the
 * participant sits the tick out rather than posting a coin flip. */
#define WSR_GOODS_DEADBAND 0.02f

/* ---- piece kinds the model recognises ---- */
#define WSR_IS_CORP 1
#define WSR_IS_POP 2

static char g_root[PATHBUF];

/* Every path is PRISC_PROJECT_ROOT plus a fixed suffix, and that root is a long
 * OneDrive path here, so -Wformat-truncation fires on the worst case for all of
 * them. Suppressed as corp_apply_finances.c:44/96/189 and econ_calendar.c do.
 * resolve_root refuses an over-long root rather than truncating one, because a
 * truncated root silently points every path at the wrong file. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr, "goods_quote: PRISC_PROJECT_ROOT too long (%zu) - "
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
    if (!f) { fprintf(stderr, "goods_quote: cannot write %s\n", path); return; }
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

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

/* ---- inventory: pieces/<id>/goods.txt as <GOOD>|<units> ---- */

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
    return v;
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

/* What this producer sold AND offered last tick. Production steers on the
 * sell-through ratio offered/sold rather than on volume alone, because volume
 * alone is ambiguous: 20 units sold is healthy demand for a firm that offered
 * 20, and total failure for one that offered 200.
 *
 * Written by goods_settle.c as data/gsold_<GOOD>.txt, rows "piece|sold|offered".
 * A two-field row is tolerated so an older file is read as "offered == sold". */
typedef struct { long sold, offered; } SellThrough;

static SellThrough last_sell_through(const char *piece_id, const char *good) {
    SellThrough st; st.sold = 0; st.offered = 0;
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/gsold_%s.txt", g_root, good);
    FILE *f = fopen(path, "r");
    if (!f) return st;
    char line[MAXLINE];
    size_t il = strlen(piece_id);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, piece_id, il) == 0 && line[il] == '|') {
            char *rest = line + il + 1;
            st.sold = atol(rest);
            char *bar = strchr(rest, '|');
            st.offered = bar ? atol(bar + 1) : st.sold;
            break;
        }
    }
    fclose(f);
    if (st.offered < 0) st.offered = 0;
    return st;
}

/* Deterministic per-participant view in [-1,+1]. FNV-1a over "piece|good", NOT
 * rand(): the same participant must hold the same view every run, or a
 * divergence could never be diagnosed and a scenario could not be replayed.
 * Seeded with the good as well as the piece so one participant does not hold
 * the same view of everything at once. */
static float view_bias(const char *piece_id, const char *good, float skill) {
    unsigned h = 2166136261u;
    for (const char *q = piece_id; *q; q++) { h ^= (unsigned char)*q; h *= 16777619u; }
    h ^= 0x5f; h *= 16777619u;
    for (const char *q = good; *q; q++) { h ^= (unsigned char)*q; h *= 16777619u; }
    float u = (float)((h >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
    return ((u * 2.0f) - 1.0f) * WSR_GOODS_DISPERSION * skill;
}

/* Producers know their industry better than a household does. */
static float skill_for(int kind) {
    return (kind == WSR_IS_CORP) ? 0.60f : 1.60f;
}

/* ------------------------------------------------------------------ main -- */

/* A piece, remembered so the tick can be done in two passes. Order rows are
 * appended, so the books have to be truncated BEFORE anything is written -
 * otherwise last tick's unfilled orders linger and get filled again next tick,
 * which is not a market, it is a queue of stale promises. That requires the
 * goods list up front, hence discovery first. */
typedef struct { char id[MAXPIECE]; int kind; char good[MAXGOOD]; } Piece;

int main(void) {
    resolve_root();

    char pieces_root[PATHBUF];
    snprintf(pieces_root, sizeof(pieces_root), "%s/projects/wsr-pal/pieces", g_root);
    DIR *d = opendir(pieces_root);
    if (!d) { fprintf(stderr, "goods_quote: cannot read %s\n", pieces_root); return 1; }

    static char goods[MAXGOODS][MAXGOOD];
    static Piece pieces[MAXPART];
    int ngoods = 0, npieces = 0;

    /* ---- pass 1: discover goods and participants, write nothing ---- */
    struct dirent *e;
    while ((e = readdir(d)) != NULL && npieces < MAXPART) {
        if (e->d_name[0] == '.') continue;
        char full[PATHBUF], state[PATHBUF];
        snprintf(full, sizeof(full), "%s/%s", pieces_root, e->d_name);
        if (!is_dir(full)) continue;

        int kind = 0;
        if (strncmp(e->d_name, "corp_", 5) == 0)    kind = WSR_IS_CORP;
        else if (strncmp(e->d_name, "pop_", 4) == 0) kind = WSR_IS_POP;
        if (!kind) continue;

        piece_state(state, sizeof(state), e->d_name);

        pieces[npieces].kind = kind;
        pieces[npieces].good[0] = '\0';
        snprintf(pieces[npieces].id, MAXPIECE, "%s", e->d_name);

        if (kind == WSR_IS_CORP) {
            read_field(state, "produces", pieces[npieces].good, MAXGOOD);
            pieces[npieces].good[strcspn(pieces[npieces].good, "\r\n")] = '\0';
            if (!pieces[npieces].good[0]) continue;   /* no taxonomy -> not a producer */
            int seen = 0;
            for (int i = 0; i < ngoods; i++)
                if (!strcmp(goods[i], pieces[npieces].good)) { seen = 1; break; }
            if (!seen && ngoods < MAXGOODS)
                snprintf(goods[ngoods++], MAXGOOD, "%s", pieces[npieces].good);
        }
        npieces++;
    }
    closedir(d);

    if (ngoods == 0) {
        printf("goods_quote: no producers found (no corp has produces=) - "
               "nothing to offer\n");
        return 0;
    }

    /* ---- pass 2a: truncate this tick's books ---- */
    for (int gi = 0; gi < ngoods; gi++) {
        char book[PATHBUF];
        snprintf(book, sizeof(book), "%s/projects/wsr-pal/data/gbook_%s.txt", g_root, goods[gi]);
        FILE *bf = fopen(book, "w");
        if (bf) fclose(bf);
    }

    int nprod = 0, noffer = 0, nbid = 0, npop = 0, ncorp = 0;

    /* ---- pass 2b: produce, then offer, then bid ---- */
    for (int pi = 0; pi < npieces; pi++) {
        const char *id = pieces[pi].id;
        char state[PATHBUF], gpath[PATHBUF];
        piece_state(state, sizeof(state), id);
        double cash = field_d(state, "cash");

        if (pieces[pi].kind == WSR_IS_CORP) {
            ncorp++;
            const char *good = pieces[pi].good;
            piece_goods(gpath, sizeof(gpath), id);

            /* PRODUCE. Steer on sell-through, and never spend cash the
             * firm does not have. Production is the ONLY way a unit comes into
             * existence, which is what makes goods conservation checkable. */
            SellThrough st = last_sell_through(id, good);
            double budget = cash * WSR_MAX_PRODUCE_FRACTION;
            long affordable = (long)(budget / WSR_PRODUCE_COST);
            if (affordable < 0) affordable = 0;
            if (affordable > WSR_MAX_PRODUCE) affordable = WSR_MAX_PRODUCE;

            float ratio = 0.0f;
            if (st.offered > 0) {
                ratio = (float)st.sold / (float)st.offered;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
            }
            long want = (long)((double)st.sold * (WSR_SELLTHROUGH_BASE + ratio));
            if (want > affordable) want = affordable;
            if (want > WSR_MAX_PRODUCE) want = WSR_MAX_PRODUCE;

            if (want > 0) {
                long stock = get_stock(gpath, good);
                set_stock(gpath, good, stock + want);
                char val[MAXLINE];
                snprintf(val, sizeof(val), "%.2f", cash - (double)want * WSR_PRODUCE_COST);
                write_field(state, "cash", val);
                cash -= (double)want * WSR_PRODUCE_COST;
                nprod += (int)want;
            }

            /* Offer only what is actually in stock. Offering stock the firm does
             * not have would be a promise settlement cannot keep. */
            long stock = get_stock(gpath, good);
            if (stock <= 0) continue;

            float base = WSR_PRODUCE_COST * (1.0f + WSR_BASE_MARGIN);
            float view = base * (1.0f + view_bias(id, good, skill_for(WSR_IS_CORP)));
            if (view <= WSR_PRODUCE_COST) view = WSR_PRODUCE_COST * 1.01f;

            char book[PATHBUF];
            snprintf(book, sizeof(book), "%s/projects/wsr-pal/data/gbook_%s.txt", g_root, good);
            FILE *bf = fopen(book, "a");
            if (!bf) continue;
            fprintf(bf, "ask|-1|%.4f|%ld|%s\n", view, stock, id);
            fclose(bf);
            noffer++;
        } else {
            npop++;
            if (cash <= 0.0) continue;

            /* Willingness to pay is the household's own dispersed view, so
             * buyers and sellers can genuinely disagree - which is the only
             * reason anything trades at all. */
            for (int gi = 0; gi < ngoods; gi++) {
                float base = WSR_PRODUCE_COST * (1.0f + WSR_BASE_MARGIN);
                float pay = base * (1.0f + view_bias(id, goods[gi], skill_for(WSR_IS_POP)));
                if (fabsf(pay - base) <= base * WSR_GOODS_DEADBAND) continue;
                if (pay <= 0.0f) continue;

                double budget = cash * WSR_MAX_BUY_FRACTION;
                long units = (long)(budget / pay);
                if (units < 1) continue;

                char book[PATHBUF];
                snprintf(book, sizeof(book), "%s/projects/wsr-pal/data/gbook_%s.txt",
                         g_root, goods[gi]);
                FILE *bf = fopen(book, "a");
                if (!bf) continue;
                fprintf(bf, "bid|1|%.4f|%ld|%s\n", pay, units, id);
                fclose(bf);
                nbid++;
            }
        }
    }

    printf("goods_quote: %d corp(s), %d household(s), %d good(s)\n", ncorp, npop, ngoods);
    printf("goods_quote: produced %d unit(s), %d offer(s), %d bid(s)\n",
           nprod, noffer, nbid);
    return 0;
}

#pragma GCC diagnostic pop
