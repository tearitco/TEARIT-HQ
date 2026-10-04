/*
 * goods_sink.c - the supply/demand cycle: goods leave an owner's hands.
 *
 * Without a sink a market only ever accumulates. Measured before this op
 * existed, units in circulation went 1,000 -> 104,881 over 16 ticks, because
 * goods were produced and sold but never used up. Nothing bounded the stock,
 * nothing forced rebuying, and households quietly became the only place units
 * went to die. This op is where they go instead, and it is what makes the
 * economy a CYCLE rather than a one-off liquidation.
 *
 * THREE SINKS, three different mechanisms, chosen per good by
 * projects/wsr-pal/data/goods_kind.txt:
 *
 *   CONSUMABLE    units destroyed by use. Fast, continuous, and it means demand
 *                 is RECURRING: a household that consumes its stock must buy
 *                 again next period. This is the steady state that production
 *                 can actually chase.
 *
 *   DEPRECIATING  units scrapped as they physically wear out. Deliberately
 *                 SLOW, so replacement demand lags capital formation. That lag
 *                 is the whole reason capital-goods markets cycle in lumps
 *                 rather than pulsing with everything else.
 *
 *   OBSOLESCENT   NOT a schedule. Obsolescence is an EVENT caused by R&D: a
 *                 corp accumulates R&D into rd_pool (booked by
 *                 corp_apply_finances), and when the pool crosses a threshold
 *                 its good's generation advances and the old generation is
 *                 obsolete. Units are not destroyed - they are WORTH LESS, which
 *                 is the part that hurts, because a firm can still not sell
 *                 them at any price. So this op publishes a price multiplier
 *                 per good and ops/goods_quote.c applies it to ask prices.
 *
 *                 A breakthrough in an input cascades to the goods that consume
 *                 it, via data/goods_input.txt, so a semiconductor breakthrough
 *                 partly obsoletes computers and network equipment too.
 *
 * Nothing here assigns a price. The multiplier is an OBSERVATION that the
 * previous generation has been displaced; the new price is still discovered by
 * matched orders in goods_settle.
 *
 * SELF-CONTAINED, like the rest of ops/. Reads pieces/<id>/{state,goods}.txt and
 * data/goods_kind.txt + data/goods_input.txt. Writes data/gobs_<GOOD>.txt, which
 * is goods_quote.c's only contract with this op.
 *
 * DETERMINISM: breakthroughs come from an accumulated threshold, never rand(),
 * so a run replays exactly and a divergence can be diagnosed.
 *
 * ---------------------------------------------------------------------------
 * NEXT SLICE, SHAPE ONLY - NOT YET BUILT: B2B GOODS AND SERVICES
 *
 * Today only HOUSEHOLDS bid for goods. Corporations are producers and never
 * consumers, which is why supply/demand is thinner than it should be: a firm
 * in this world buys its power, its metals and its components out of thin air,
 * and produces nothing but its own single good. Real firms are both.
 *
 * The intent is that corporations offer their goods AND SERVICES to OTHER
 * corporations at billable prices, through the SAME order book and the SAME
 * double-entry ledger the equity market uses - one auction mechanism, one
 * ledger format, one settlement rule, already proven in market_quote /
 * market_settle and goods_quote / goods_settle. Not a second engine.
 *
 * The shape is already opened here in three places, deliberately:
 *
 *   1. data/goods_input.txt already states which goods consume which. That file
 *      currently drives obsolescence cascade only. It is the natural driver of
 *      B2B bids too: COMPUTER consumes SEMICONDUCTOR, so a computer maker should
 *      bid for semiconductors rather than conjure them.
 *   2. Services are already first-class goods (BANKING, INSURANCE, SHIPPING,
 *      telecom and internet content are all CONSUMABLE - a service is consumed
 *      when performed). So "sell a service" needs no new concept, only a
 *      different buyer.
 *   3. Consumption already happens per HOLDER, not per household, so corps
 *      holding intermediates consume them at the same rate pop_* do.
 *
 * What B2B needs when it is built: corps posting bids in goods_quote (currently
 * household-only), an intermediate-goods store distinct from final consumption so
 * a component is not eaten by the same sink as a finished product, and a
 * billable-price concept distinct from unit price for services.
 * ---------------------------------------------------------------------------
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
#define MAXPIECE 64
#define MAXGOODS 64
#define MAXINPUTS 24

/* ---- tuning constants, owned by THIS op ---- */

/* R&D ACCUMULATES LIKE AN RPG LEVEL-UP CURVE, not like a linear drip.
 *
 * Breakthrough cost grows geometrically with a firm's current generation:
 *     cost(n) = WSR_RD_BREAKTHROUGH_BASE * WSR_RD_CURVE ^ n
 *
 * So level 1 is cheap, level 5 is 10x level 1, level 8 is 43x. That curve is
 * the entire point, and it is why a flat threshold would be wrong:
 *
 *  - With a flat threshold, R&D is a repeating chore. Spend, wait, spend again.
 *    No firm ever gets meaningfully better than any other, and no firm is ever
 *    stranded. Nothing about technology changes hands.
 *  - With the curve, breakthroughs are IRREVERSIBLE and PERMANENT. A firm that
 *    once led has an installed base, a reputation and a generation advantage
 *    that a competitor cannot buy its way past cheaply. A firm that stops
 *    researching does not hold its level, it watches the cost of the next one
 *    climb away from it.
 *
 * This is also what makes obsolescence a real hazard rather than ambient noise:
 * the goods you make are displaced by firms who chose to level up, and the
 * whole ladder is priced so that stopping is a decision with a price tag. */
#ifndef WSR_RD_BREAKTHROUGH_BASE
#define WSR_RD_BREAKTHROUGH_BASE 250.0
#endif
#ifndef WSR_RD_CURVE
#define WSR_RD_CURVE 1.6
#endif

/* Price multiplier applied to a good the moment its generation is displaced.
 * A generation is not destroyed, it is undercut - and hard, because the whole
 * point is that the buyer now has a much better option. */
#ifndef WSR_OBSOLETE_PRICE
#define WSR_OBSOLETE_PRICE 0.45
#endif

/* Cascade strength to goods that CONSUME the obsoleted input. Weaker than the
 * direct hit, because a downstream good is not obsolete - it is just built on
 * something that just got cheap and better. */
#ifndef WSR_OBSOLETE_CASCADE
#define WSR_OBSOLETE_CASCADE 0.80
#endif

/* How much of an existing obsolescence multiplier survives one period. The new
 * generation becomes the standard over time, so the discount decays rather than
 * persisting forever. Without this, one breakthrough permanently cripples a
 * good and no amount of R&D could ever recover it. */
#ifndef WSR_OBSOLETE_RECOVERY
#define WSR_OBSOLETE_RECOVERY 0.75
#endif

static char g_root[PATHBUF];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && *env) {
        size_t n = strlen(env);
        if (n >= sizeof(g_root) - 128) {
            fprintf(stderr, "goods_sink: PRISC_PROJECT_ROOT too long (%zu) - "
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
    }
    fclose(f);
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "goods_sink: cannot write %s\n", path); return; }
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

/* ---- good taxonomy ---- */

typedef enum { K_CONSUMABLE, K_DEPRECIATING, K_OBSOLESCENT } Kind;

typedef struct {
    char name[MAXPIECE];
    Kind kind;
    double rate;       /* fraction of units removed per period */
    double mult;       /* current obsolescence price multiplier, 1.0 = clean */
} Good;

static Good goods[MAXGOODS];
static int  ngoods = 0;

static int good_index(const char *name) {
    for (int i = 0; i < ngoods; i++) if (!strcmp(goods[i].name, name)) return i;
    return -1;
}

static void load_kinds(void) {
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/goods_kind.txt", g_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "goods_sink: cannot read %s - refusing to guess how goods "
                        "leave the system\n", path);
        exit(1);
    }
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && ngoods < MAXGOODS) {
        if (line[0] == '#' || strncmp(line, " ", 1) == 0 || line[0] == '\n') continue;
        char name[MAXPIECE], kind[32];
        double rate = 0.0;
        int fields = sscanf(line, "%63[^|]|%31[^|]|%lf", name, kind, &rate);
        if (fields < 2) continue;
        char *e = name + strlen(name);
        while (e > name && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = '\0';
        /* `kind` MUST be trimmed too, and the reason is a trap worth keeping:
         * OBSOLESCENT rows have no rate, so kind is the LAST field on the line
         * and CRLF leaves a trailing \r inside it. Without this trim all 7
         * obsolescing goods fail the strcmp and are dropped SILENTLY - observed
         * as "22 good(s) classified" instead of 29, with obsolescence simply
         * not happening and nothing anywhere reporting a fault. A goods row is
         * never optional, so the classifier verifies its own coverage. */
        e = kind + strlen(kind);
        while (e > kind && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ||
                           e[-1] == '\n')) *--e = '\0';

        if (!strcmp(kind, "CONSUMABLE"))       { goods[ngoods].kind = K_CONSUMABLE;
                                                  if (rate <= 0) rate = 0.80; }
        else if (!strcmp(kind, "DEPRECIATING")) { goods[ngoods].kind = K_DEPRECIATING;
                                                  if (rate <= 0) rate = 0.05; }
        else if (!strcmp(kind, "OBSOLESCENT"))  { goods[ngoods].kind = K_OBSOLESCENT;
                                                  rate = 0.0; }
        else continue;

        snprintf(goods[ngoods].name, MAXPIECE, "%s", name);
        goods[ngoods].rate = rate;
        goods[ngoods].mult = 1.0;
        ngoods++;
    }
    fclose(f);

    /* Coverage check. Every good any corp actually PRODUCES must be classified,
     * or that good silently has no sink and silently never obsolesces. Finding
     * a missing row by noticing the printed count (22 vs 29) is luck; this makes
     * it an error. */
    char dirch[PATHBUF];
    snprintf(dirch, sizeof(dirch), "%s/projects/wsr-pal/pieces", g_root);
    DIR *vd = opendir(dirch);
    int missing = 0;
    if (vd) {
        struct dirent *ve;
        while ((ve = readdir(vd))) {
            if (strncmp(ve->d_name, "corp_", 5) != 0) continue;
            char st[PATHBUF];
            piece_state(st, sizeof(st), ve->d_name);
            char prod[MAXPIECE];
            read_field(st, "produces", prod, sizeof(prod));
            prod[strcspn(prod, "\r\n")] = '\0';
            if (!prod[0]) continue;
            if (good_index(prod) < 0) {
                fprintf(stderr, "goods_sink: produced good '%s' (corp %s) has no "
                                "row in goods_kind.txt - it would never be consumed "
                                "or obsoleted\n", prod, ve->d_name);
                missing++;
            }
        }
        closedir(vd);
    }
    if (missing > 0) exit(1);
}

/* Cascade map: dependent good -> the goods it consumes. */
static char dep_of[MAXGOODS][MAXPIECE];
static char dep_inputs[MAXGOODS][MAXINPUTS][MAXPIECE];
static int  dep_count = 0;

static void load_inputs(void) {
    char path[PATHBUF];
    snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/goods_input.txt", g_root);
    FILE *f = fopen(path, "r");
    if (!f) return;                      /* cascade is optional, not fatal */
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && dep_count < MAXGOODS) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char good[MAXPIECE], inputs[MAXINPUTS * MAXPIECE];
        if (sscanf(line, "%63[^|]|%1900[^\n]", good, inputs) != 2) continue;
        char *e = good + strlen(good);
        while (e > good && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
        snprintf(dep_of[dep_count], MAXPIECE, "%s", good);
        int n = 0;
        char *tok = strtok(inputs, ",");
        while (tok && n < MAXINPUTS) {
            char *t = tok;
            while (*t == ' ' || *t == '\t') t++;
            char *end = t + strlen(t);
            while (end > t && (end[-1] == ' ' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
            if (*t) { snprintf(dep_inputs[dep_count][n], MAXPIECE, "%s", t); n++; }
            tok = strtok(NULL, ",");
        }
        if (n > 0) dep_count++;
    }
    fclose(f);
}

/* ---- inventory ---- */

typedef struct {
    char   good[MAXPIECE];
    long   units;
} Row;

static int load_stock(const char *path, Row *rows, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[MAXLINE];
    while (fgets(line, sizeof(line), f) && n < max) {
        char name[MAXPIECE]; long units;
        if (sscanf(line, "%63[^|]|%ld", name, &units) != 2) continue;
        char *e = name + strlen(name);
        while (e > name && (e[-1] == ' ' || e[-1] == '\r')) *--e = '\0';
        if (units <= 0) continue;
        snprintf(rows[n].good, MAXPIECE, "%s", name);
        rows[n].units = units;
        n++;
    }
    fclose(f);
    return n;
}

static void save_stock(const char *path, Row *rows, int n) {
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "goods_sink: cannot write %s\n", path); return; }
    for (int i = 0; i < n; i++)
        if (rows[i].units > 0) fprintf(f, "%s|%ld\n", rows[i].good, rows[i].units);
    fclose(f);
}

/* ---- obsolescence state ---- */

static void load_mults(void) {
    for (int i = 0; i < ngoods; i++) {
        char path[PATHBUF];
        snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/gobs_%s.txt", g_root,
                 goods[i].name);
        goods[i].mult = field_d(path, "price_multiplier");
        if (goods[i].mult <= 0.0) goods[i].mult = 1.0;
    }
}

static void save_mults(void) {
    for (int i = 0; i < ngoods; i++) {
        char path[PATHBUF];
        snprintf(path, sizeof(path), "%s/projects/wsr-pal/data/gobs_%s.txt", g_root,
                 goods[i].name);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.4f", goods[i].mult);
        write_field(path, "price_multiplier", buf);
    }
}

/* ---------------------------------------------------------------------------
 * GAMIFICATION HOOK - NAMED TECH TIERS. Shape only, no UI yet.
 *
 * `generation` is already the real, load-bearing number: it sets the price of
 * this firm's NEXT breakthrough and it is what obsolescence compares against.
 * It is deliberately the source of truth, not a cosmetic copy.
 *
 * tech_level derives its label from generation so a future UI can show a
 * player something like "ADVANCED" without inventing a second progression that
 * could drift out of sync with the economic one. If a tier is ever added here
 * and not to the curve, the economy and the label will disagree - that is the
 * bug this indirection is designed to make visible.
 *
 * Tier boundaries are gameplay-tunable, not economic: they decide what a player
 * SEES, while the RPG cost curve above decides what actually happens. Keep them
 * separate and a designer can make level 3 feel like an achievement without
 * touching the economy.
 * --------------------------------------------------------------------------- */
static const char *tech_tier(long gen) {
    if (gen <= 0) return "PROTOTYPE";
    if (gen <= 2) return "PRIMARY";
    if (gen <= 4) return "ADVANCED";
    if (gen <= 6) return "STATE_OF_THE_ART";
    return "BLEEDING_EDGE";
}

int main(void) {
    resolve_root();
    load_kinds();
    load_inputs();
    load_mults();

    if (ngoods == 0) { fprintf(stderr, "goods_sink: no goods classified\n"); return 1; }

    /* ---- pass 1: R&D breakthroughs. Done FIRST so a good can be obsoleted and
     * consumed in the same period, which is what actually happens to a firm
     * whose product is displaced the quarter it launches the replacement. ---- */
    int breakthroughs = 0, cascade_hits = 0;
    double rd_spent_total = 0.0;

    char dirpath[PATHBUF];
    snprintf(dirpath, sizeof(dirpath), "%s/projects/wsr-pal/pieces", g_root);
    DIR *d = opendir(dirpath);
    if (!d) { fprintf(stderr, "goods_sink: cannot open %s\n", dirpath); return 1; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "corp_", 5) != 0) continue;
        char st[PATHBUF];
        piece_state(st, sizeof(st), e->d_name);
        char produces[MAXPIECE];
        read_field(st, "produces", produces, sizeof(produces));
        produces[strcspn(produces, "\r\n")] = '\0';
        if (!produces[0]) continue;
        int gi = good_index(produces);
        if (gi < 0) continue;

        double pool = field_d(st, "rd_pool");

        char gb[MAXLINE];
        read_field(st, "generation", gb, sizeof(gb));
        long gen = atol(gb);

        /* Level-up: spend until the pool can no longer afford the NEXT level.
         * A corp sitting on a large pool levels repeatedly in one period
         * rather than idling on a surplus it could have converted. */
        int guard = 0;
        while (pool >= WSR_RD_BREAKTHROUGH_BASE * pow(WSR_RD_CURVE, (double)gen) &&
               guard++ < 8) {
            double cost = WSR_RD_BREAKTHROUGH_BASE * pow(WSR_RD_CURVE, (double)gen);
            pool -= cost;
            gen++;
            breakthroughs++;
            rd_spent_total += cost;

            goods[gi].mult *= WSR_OBSOLETE_PRICE;

            /* Cascade to goods that CONSUME this one. */
            for (int i = 0; i < dep_count; i++) {
                int hit = 0;
                for (int k = 0; k < MAXINPUTS; k++) {
                    if (!dep_inputs[i][k][0]) break;
                    if (!strcmp(dep_inputs[i][k], produces)) { hit = 1; break; }
                }
                if (!hit) continue;
                int dj = good_index(dep_of[i]);
                if (dj < 0) continue;
                goods[dj].mult *= WSR_OBSOLETE_CASCADE;
                cascade_hits++;
            }
        }

        if (guard > 1 || pool != field_d(st, "rd_pool")) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f", pool);
            write_field(st, "rd_pool", buf);
            snprintf(buf, sizeof(buf), "%ld", gen);
            write_field(st, "generation", buf);
            /* Derived label for a future UI. generation stays authoritative. */
            write_field(st, "tech_level", tech_tier(gen));
        }
    }
    closedir(d);

    /* ---- pass 2: consumption and depreciation on every holder's stock ---- */
    static Row rows[64];
    long consumed = 0, scrapped = 0;
    int holders = 0;

    d = opendir(dirpath);
    if (!d) return 1;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "pop_", 4) != 0 && strncmp(e->d_name, "corp_", 5) != 0)
            continue;
        char gp[PATHBUF], st[PATHBUF];
        piece_goods(gp, sizeof(gp), e->d_name);
        piece_state(st, sizeof(st), e->d_name);
        int n = load_stock(gp, rows, 64);
        if (n == 0) continue;
        holders++;

        int changed = 0;
        for (int i = 0; i < n; i++) {
            int gi = good_index(rows[i].good);
            if (gi < 0) continue;
            if (goods[gi].kind == K_OBSOLESCENT) continue;  /* value, not units */
            double rate = goods[gi].rate;
            if (rate <= 0) continue;
            long gone = (long)((double)rows[i].units * rate);
            if (gone < 1) gone = 1;                 /* always progress at the margin */
            if (gone > rows[i].units) gone = rows[i].units;
            rows[i].units -= gone;
            if (goods[gi].kind == K_CONSUMABLE) consumed += gone;
            else scrapped += gone;
            changed = 1;
        }
        if (changed) save_stock(gp, rows, n);

        /* Record the turn's consumption on the holder so starvation and
         * over-consumption are visible rather than inferred. */
        if (strncmp(e->d_name, "pop_", 4) == 0) {
            double prev = field_d(st, "goods_consumed_total");
            double used = 0.0;
            for (int i = 0; i < n; i++) {
                int gi = good_index(rows[i].good);
                if (gi >= 0 && goods[gi].kind == K_CONSUMABLE) used += goods[gi].rate;
            }
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f", prev + used);
            write_field(st, "goods_consumed_total", buf);
        }
    }
    closedir(d);

    /* ---- pass 3: let obsolescence decay toward the new standard ---- */
    for (int i = 0; i < ngoods; i++) {
        if (goods[i].mult < 1.0) {
            goods[i].mult = 1.0 + (goods[i].mult - 1.0) * WSR_OBSOLETE_RECOVERY;
            if (goods[i].mult < 0.05) goods[i].mult = 0.05;
        }
    }
    save_mults();

    int n_obs = 0;
    for (int i = 0; i < ngoods; i++) if (goods[i].mult < 0.999) n_obs++;

    printf("goods_sink: %d good(s) classified, %d holder(s) touched\n", ngoods, holders);
    printf("goods_sink: consumed %ld unit(s), scrapped %ld unit(s)\n", consumed, scrapped);
printf("goods_sink: %d R&D breakthrough(s), %d cascade hit(s), "
           "%d good(s) currently obsoleted\n", breakthroughs, cascade_hits, n_obs);
    return 0;
}
