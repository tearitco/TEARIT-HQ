/* dsr_day_tick - DSR step 1: ONE game day of the economy for a scratch game root (the `common:dsr_day_tick` common event body).
 *
 * Usage:  dsr_day_tick <pkg_dir> <house_root>        callable as an lc_clock event runner (LC_CLOCK_EVENT_RUNNER=<this>; lc_clock calls `<bin> <pkg> <house>`)
 *         dsr_day_tick --game <game_root>            same, game root given directly
 *   game root = flag > env DSR_GAME_ROOT > `<pkg>/event_pkg/target.pdl` row `TARGET | state | <path>` (absolute, or relative to the house root; the
 *   digipet convention) > <house>/dsr_game.  One call = exactly one day: the world variable `day` is the count of ticks applied, so the lc_clock schedule
 *   ledger (one SCHED row per occurrence, catch-up fires every missed day in order) is the cursor; this op keeps no clock state of its own.
 *
 * Why a compiled op (and not registered commands): the economy needs logs, rounding, sorting and a double auction. The registry's PAL has addi/beq/bne and
 * integer key=value reads only (see harness/digipet README "idioms"); `control_variable`/`change_variable` add literals, `if` tests switches. Nothing registered
 * can compute log10 or a margin. State, however, IS the registry convention: every number lives in the entity's `variables.txt` (`key=int`, money in whole
 * cents), so events/pages can read it.
 *
 * ORDER OF A DAY (WSR order per corp_apply_finances.c: finances, then price):
 *   1 production + quotes (goods_quote.c): each store produces by last day's sell-through, then asks its whole stock; each population bids per good
 *   2 settle (goods_settle.c): per good a double auction, bids high->low vs asks low->high, trades print at the RESTING (ask) price; a store's `price` is the
 *     last price it traded at (WSR: a goods price only ever comes from a matched trade); shared market across civs (backlog decision 12)
 *   3 payroll (corp_payroll.c): each store with a margin pays 60% of its GROSS MARGIN (revenue - units sold x cost) to its civ's population, if it keeps the floor
 *   4 loan interest (corp_apply_finances.c loan *= 1+rate, bank_loan_op.c repay): daily interest on the store's loan; paid in cash to the bank when affordable,
 *     else compounded onto the balance (bank loans_out grows by the same amount)
 *   5 tax (tax_loop.c): the civ's castle takes tax_rate of each store's cash (DSR: basis points per day; WSR: percent points per year)
 *   6 reprice (corp_update_price.c calculate_new_stock_price, float math as WSR): `share_price`
 *   7 population (pop_update.c weighted mode): count grows by births - deaths scaled by food adequacy (units of good 1 bought today vs need)
 *   8 history rows (append-only `dsr_history.txt` per entity, `DAY|n|...`) + world ledger rows (`DAY|n|total|sink|loans_out|loans_owed|wages|tax|interest`,
 *     `MARKET|n|good|fills|units|last_price`), then the variable files, the world `day` last.
 * MONEY: production cost is paid to the world `sink` account (WSR: cost just vanishes; here it is booked, so the closed system is exact). Conserved:
 *   sum(store cash + bank reserves + castle treasury + population cash) + sink. Debt (store loan_balance == bank loans_out) is conserved separately. The op
 *   checks the total in memory BEFORE writing anything and aborts (exit 3, nothing written) on a mismatch.
 * DSR ADAPTATIONS of WSR (said once, here): WSR hashes `piece|good` only, so its views never change; the FNV also takes the game seed and the day, so views
 *   (and therefore asks, bids and prices) vary by day and by seed and the run is reproducible. Population is ONE aggregate bidder per civ (WSR households are
 *   districts). Money is integer cents. Unemployment (permille) is DSR's own bookkeeping: 1000 - min(1000, wages paid to the pop / (count x target_wage)).
 * Exit: 0 ok, 1 usage/setup, 3 invariant violated (nothing written).  Self-contained (no shared headers). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define P 4096
#define MAXE 128
#define MAXK 48
#define MAXO 64
#define NGOODS 4
static const char *GOODNAME[NGOODS + 1] = { "", "FOOD", "WATER", "CLOTHES", "SHELTER" };

typedef struct { char k[40]; long v; } KV;
typedef struct { KV kv[MAXK]; int n; } Vars;
typedef struct {
    char name[64], dir[P]; char kind; int civ, idx;          /* kind: c castle, b bank, s store, p population */
    Vars v;
} Ent;
static Ent E[MAXE]; static int NE;
static char GAME[P], WORLD[P];
static Vars W;
static char TK[64][48]; static long TV[64]; static int NT;

static long vget(const Vars *v, const char *k) { for (int i = 0; i < v->n; i++) if (!strcmp(v->kv[i].k, k)) return v->kv[i].v; return 0; }
static void vset(Vars *v, const char *k, long x) {
    for (int i = 0; i < v->n; i++) if (!strcmp(v->kv[i].k, k)) { v->kv[i].v = x; return; }
    if (v->n < MAXK) { snprintf(v->kv[v->n].k, 40, "%s", k); v->kv[v->n].v = x; v->n++; }
}
static int vload(Vars *v, const char *path) {
    FILE *f = fopen(path, "r"); char ln[256]; v->n = 0; if (!f) return -1;
    while (fgets(ln, sizeof ln, f)) { char *eq = strchr(ln, '='); if (!eq) continue; *eq = 0; vset(v, ln, atol(eq + 1)); }
    fclose(f); return 0;
}
static int vsave(const Vars *v, const char *path) {         /* temp file + rename: a reader never sees half a file */
    char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w"); if (!f) return -1;
    for (int i = 0; i < v->n; i++) fprintf(f, "%s=%ld\n", v->kv[i].k, v->kv[i].v);
    if (fclose(f)) return -1;
    return rename(tmp, path);
}
static long T(const char *k) { for (int i = NT - 1; i >= 0; i--) if (!strcmp(TK[i], k)) return TV[i];   /* last row wins: a scenario can append an override */ fprintf(stderr, "dsr_day_tick: missing TUNE %s\n", k); exit(1); }
static char *trim(char *s) { char *e; while (isspace((unsigned char)*s)) s++; e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }

static int entcmp(const void *a, const void *b) { return strcmp(((const Ent *)a)->name, ((const Ent *)b)->name); }

/* ---- WSR goods_quote.c view_bias: FNV-1a over "who|good" -> [-1,+1] * dispersion * skill. DSR also folds in the game seed and the day (see header). ---- */
static double view_bias(const char *who, const char *good, long skill_pct, long seed, long day) {
    unsigned h = 2166136261u;
    for (const char *q = who; *q; q++) { h ^= (unsigned char)*q; h *= 16777619u; }
    h ^= 0x5f; h *= 16777619u;
    for (const char *q = good; *q; q++) { h ^= (unsigned char)*q; h *= 16777619u; }
    for (int i = 0; i < 4; i++) { h ^= (unsigned)((seed >> (8 * i)) & 0xff); h *= 16777619u; }   /* DSR: seed */
    for (int i = 0; i < 4; i++) { h ^= (unsigned)((day >> (8 * i)) & 0xff); h *= 16777619u; }    /* DSR: day */
    double u = (double)((h >> 8) & 0xFFFFFF) / (double)0xFFFFFF;
    return ((u * 2.0) - 1.0) * (T("dispersion_pct") / 100.0) * (skill_pct / 100.0);
}

/* ---- WSR corp_update_price.c calculate_new_stock_price (float math, constant for constant) ---- */
static float calculate_new_stock_price(float book_value, float shares_outstanding, float market_cap, float debt_to_equity, int risk_bias, float current_price) {
    float book_value_per_share = (shares_outstanding > 0) ? book_value / shares_outstanding : 0.0f;
    float market_cap_multiplier = 1.0f;
    if (market_cap > 0) market_cap_multiplier = 1.0f + (log10f(market_cap) - 3) * 0.05f;
    float leverage_factor;
    if (debt_to_equity > 1.0f) leverage_factor = 1.0f - (debt_to_equity - 1.0f) * 0.1f;
    else leverage_factor = 1.0f + (0.5f - debt_to_equity) * 0.05f;
    float bias_factor = 0.5f + (risk_bias / 100.0f) * 1.0f;
    float value = book_value_per_share * market_cap_multiplier * leverage_factor * bias_factor;
    float momentum_factor = 0.7f;
    float new_price = (value * momentum_factor) + (current_price * (1.0f - momentum_factor));
    if (new_price < 0.1f) new_price = 0.1f;
    return new_price;
}

typedef struct { int ent; long price, units; } Ord;
static int cmp_bid(const void *a, const void *b) { const Ord *x = a, *y = b; if (x->price != y->price) return x->price < y->price ? 1 : -1; return x->ent - y->ent; }
static int cmp_ask(const void *a, const void *b) { const Ord *x = a, *y = b; if (x->price != y->price) return x->price > y->price ? 1 : -1; return x->ent - y->ent; }

static long total_cash(void) {
    long t = vget(&W, "sink");
    for (int i = 0; i < NE; i++) {
        if (E[i].kind == 's' || E[i].kind == 'p') t += vget(&E[i].v, "cash");
        else if (E[i].kind == 'b') t += vget(&E[i].v, "reserves");
        else t += vget(&E[i].v, "treasury");
    }
    return t;
}
static int civ_ent(char kind, int civ, int n) {          /* n-th (1-based) entity of kind in civ, or -1 */
    int seen = 0;
    for (int i = 0; i < NE; i++) if (E[i].kind == kind && E[i].civ == civ && ++seen == n) return i;
    return -1;
}

int main(int argc, char **argv) {
    const char *pkg = NULL, *house = NULL, *gameflag = NULL;
    if (argc == 3 && !strcmp(argv[1], "--game")) gameflag = argv[2];
    else if (argc >= 3) { pkg = argv[1]; house = argv[2]; }
    else { fprintf(stderr, "usage: dsr_day_tick <pkg_dir> <house_root> | --game <game_root>\n"); return 1; }

    if (gameflag) snprintf(GAME, sizeof GAME, "%s", gameflag);
    else if (getenv("DSR_GAME_ROOT") && *getenv("DSR_GAME_ROOT")) snprintf(GAME, sizeof GAME, "%s", getenv("DSR_GAME_ROOT"));
    else {
        char tp[P], ln[1024]; GAME[0] = 0;
        snprintf(tp, sizeof tp, "%s/event_pkg/target.pdl", pkg);
        FILE *f = fopen(tp, "r");
        if (f) { while (fgets(ln, sizeof ln, f)) {
            char *a = strtok(ln, "|"), *b = strtok(NULL, "|"), *c = strtok(NULL, "\n");
            if (a && b && c && !strcmp(trim(a), "TARGET") && !strcmp(trim(b), "state")) { c = trim(c); if (c[0] == '/') snprintf(GAME, sizeof GAME, "%s", c); else snprintf(GAME, sizeof GAME, "%s/%s", house, c); break; }
        } fclose(f); }
        if (!GAME[0]) snprintf(GAME, sizeof GAME, "%s/dsr_game", house);
    }
    snprintf(WORLD, sizeof WORLD, "%s/dsr_world", GAME);

    char path[P + 64], ln[1024];
    snprintf(path, sizeof path, "%s/variables.txt", WORLD);
    if (vload(&W, path)) { fprintf(stderr, "dsr_day_tick: no world at %s\n", path); return 1; }
    snprintf(path, sizeof path, "%s/tunables.pdl", WORLD);
    FILE *tf = fopen(path, "r"); if (!tf) { fprintf(stderr, "dsr_day_tick: no tunables\n"); return 1; }
    while (fgets(ln, sizeof ln, tf)) { char *a = strtok(ln, "|"), *b = strtok(NULL, "|"), *c = strtok(NULL, "\n"); if (a && b && c && !strcmp(trim(a), "TUNE") && NT < 64) { snprintf(TK[NT], 48, "%s", trim(b)); TV[NT++] = atol(c); } }
    fclose(tf);

    /* discover entities (sorted by name = deterministic) */
    snprintf(path, sizeof path, "%s/pals", GAME);
    DIR *d = opendir(path); if (!d) { fprintf(stderr, "dsr_day_tick: no pals dir\n"); return 1; }
    struct dirent *de;
    while ((de = readdir(d)) && NE < MAXE) {
        if (strncmp(de->d_name, "dsrtest_", 8)) continue;
        const char *r = de->d_name + 8; Ent *e = &E[NE]; memset(e, 0, sizeof *e);
        if (!strncmp(r, "castle_", 7)) { e->kind = 'c'; e->civ = r[7] - 'a'; e->idx = 1; }
        else if (!strncmp(r, "bank_", 5)) { e->kind = 'b'; e->civ = r[5] - 'a'; e->idx = atoi(r + 6); }
        else if (!strncmp(r, "store_", 6)) { e->kind = 's'; e->civ = r[6] - 'a'; e->idx = atoi(r + 7); }
        else if (!strncmp(r, "population_", 11)) { e->kind = 'p'; e->civ = r[11] - 'a'; e->idx = 1; }
        else continue;
        snprintf(e->name, sizeof e->name, "%s", de->d_name); snprintf(e->dir, sizeof e->dir, "%s/pals/%s", GAME, de->d_name);
        snprintf(path, sizeof path, "%s/variables.txt", e->dir);
        if (vload(&e->v, path)) { fprintf(stderr, "dsr_day_tick: %s has no variables.txt\n", e->name); return 1; }
        NE++;
    }
    closedir(d);
    qsort(E, (size_t)NE, sizeof(Ent), entcmp);

    long seed = vget(&W, "seed"), day = vget(&W, "day") + 1;
    long cost = T("produce_cost_c"), margin = T("base_margin_pct");
    double base = cost * (100 + margin) / 100.0;
    long cash_before = total_cash();

    long wages_total = 0, tax_total = 0, interest_total = 0, produced_cost_total = 0;
    static Ord bids[NGOODS + 1][MAXO], asks[NGOODS + 1][MAXO]; int nb[NGOODS + 1] = {0}, na[NGOODS + 1] = {0};
    long good_fills[NGOODS + 1] = {0}, good_units[NGOODS + 1] = {0}, good_last[NGOODS + 1] = {0};

    /* 1. production + quotes (populations reset their per-day counters first: production wages land on them) */
    for (int i = 0; i < NE; i++) if (E[i].kind == 'p') { vset(&E[i].v, "food_supply", 0); vset(&E[i].v, "wages_today", 0); }
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v;
        if (e->kind == 's') {
            long cash = vget(v, "cash"), stock = vget(v, "stock"), sold = vget(v, "last_sold"), offered = vget(v, "last_offered"), g = vget(v, "good_id");
            if (g < 1 || g > NGOODS) { fprintf(stderr, "dsr_day_tick: %s bad good_id\n", e->name); return 1; }
            long budget = cash * T("max_produce_frac_pct") / 100, affordable = budget / cost;
            if (affordable < 0) affordable = 0; if (affordable > T("max_produce")) affordable = T("max_produce");
            double ratio = 0.0; if (offered > 0) { ratio = (double)sold / (double)offered; if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1; }
            long want = (long)((double)sold * (T("sellthrough_base_pct") / 100.0 + ratio));
            if (want > affordable) want = affordable; if (want > T("max_produce")) want = T("max_produce");
            if (want < 0) want = 0;
            long pw = 0;
            if (want > 0) {
                long pc = want * cost; int pi = civ_ent('p', e->civ, 1);
                stock += want; cash -= pc;
                if (pi >= 0) { pw = pc * T("production_wage_pct") / 100; vset(&E[pi].v, "cash", vget(&E[pi].v, "cash") + pw); vset(&E[pi].v, "wages_today", vget(&E[pi].v, "wages_today") + pw); wages_total += pw; }
                produced_cost_total += pc - pw;                       /* the rest of the input cost leaves to the world sink (WSR: it just vanishes) */
            }
            vset(v, "wage_bill", pw);
            vset(v, "produced_today", want); vset(v, "cash", cash); vset(v, "stock", stock);
            vset(v, "sold_today", 0); vset(v, "rev_today", 0); vset(v, "interest_today", 0); vset(v, "tax_today", 0);
            vset(v, "offered_today", 0);
            if (stock > 0) {
                double view = base * (1.0 + view_bias(e->name, GOODNAME[g], T("skill_corp_pct"), seed, day));
                if (view <= (double)cost) view = cost * 1.01;
                long ask = llround(view);
                if (na[g] < MAXO) { asks[g][na[g]].ent = i; asks[g][na[g]].price = ask; asks[g][na[g]].units = stock; na[g]++; }
                vset(v, "offered_today", stock);
            }
        } else if (e->kind == 'p') {
            long cash = vget(v, "cash");
            if (cash <= 0) continue;
            for (int g = 1; g <= NGOODS; g++) {
                double pay = base * (1.0 + view_bias(e->name, GOODNAME[g], T("skill_pop_pct"), seed, day));
                if (fabs(pay - base) <= base * (T("deadband_pct") / 100.0)) continue;
                if (pay <= 0.0) continue;
                long payc = llround(pay); if (payc <= 0) continue;
                long units = (cash * T("max_buy_frac_pct") / 100) / payc;
                if (units < 1) continue;
                if (nb[g] < MAXO) { bids[g][nb[g]].ent = i; bids[g][nb[g]].price = payc; bids[g][nb[g]].units = units; nb[g]++; }
            }
        }
    }
    /* 2. settle each good */
    for (int g = 1; g <= NGOODS; g++) {
        if (!nb[g] || !na[g]) continue;
        qsort(bids[g], (size_t)nb[g], sizeof(Ord), cmp_bid); qsort(asks[g], (size_t)na[g], sizeof(Ord), cmp_ask);
        int bi = 0, ai = 0;
        while (bi < nb[g] && ai < na[g]) {
            Ord *b = &bids[g][bi], *a = &asks[g][ai];
            if (b->price < a->price) break;
            long qty = b->units < a->units ? b->units : a->units;
            if (qty < 1) break;
            long price = a->price, amount = price * qty;
            Ent *buyer = &E[b->ent], *seller = &E[a->ent];
            long bcash = vget(&buyer->v, "cash"), have = vget(&seller->v, "stock");
            if (bcash < amount) { if (b->units <= a->units) bi++; else ai++; continue; }
            if (have < qty) { qty = have; if (qty < 1) { ai++; continue; } amount = price * qty; if (bcash < amount) { bi++; continue; } }
            vset(&buyer->v, "cash", bcash - amount); vset(&seller->v, "cash", vget(&seller->v, "cash") + amount);
            vset(&seller->v, "stock", have - qty);
            vset(&seller->v, "sold_today", vget(&seller->v, "sold_today") + qty);
            vset(&seller->v, "rev_today", vget(&seller->v, "rev_today") + amount);
            vset(&seller->v, "price", price);                                     /* last matched trade price */
            if (g == 1) vset(&buyer->v, "food_supply", vget(&buyer->v, "food_supply") + qty);
            good_fills[g]++; good_units[g] += qty; good_last[g] = price;
            b->units -= qty; a->units -= qty;
            if (b->units < 1) bi++;
            if (a->units < 1) ai++;
        }
    }
    /* feedback for tomorrow's production + 3. payroll (gross margin share to the civ's population) */
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v;
        if (e->kind != 's') continue;
        vset(v, "last_sold", vget(v, "sold_today")); vset(v, "last_offered", vget(v, "offered_today"));
        long rev = vget(v, "rev_today"), sold = vget(v, "sold_today"), paid = 0;
        long m = rev - sold * cost;
        int pi = civ_ent('p', e->civ, 1);
        if (rev > 0 && m > 0 && pi >= 0) {
            long owed = llround((double)m * T("payroll_rate_pct") / 100.0), cash = vget(v, "cash");
            if (owed > 0 && cash - owed >= T("payroll_floor_c")) {
                vset(v, "cash", cash - owed); vset(&E[pi].v, "cash", vget(&E[pi].v, "cash") + owed);
                vset(&E[pi].v, "wages_today", vget(&E[pi].v, "wages_today") + owed);
                paid = owed; wages_total += owed;
            }
        }
        vset(v, "wage_bill", vget(v, "wage_bill") + paid);
    }
    /* 4. loan interest, 5. tax */
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v;
        if (e->kind != 's') continue;
        long loan = vget(v, "loan_balance"), bn = vget(v, "loan_bank");
        int bi = bn > 0 ? civ_ent('b', e->civ, (int)bn) : -1;
        if (loan > 0 && bi >= 0) {
            long interest = llround((double)loan * (double)vget(&E[bi].v, "rate") / 1000000.0), cash = vget(v, "cash");
            if (interest > 0) {
                if (cash - interest >= 0) { vset(v, "cash", cash - interest); vset(&E[bi].v, "reserves", vget(&E[bi].v, "reserves") + interest); vset(&E[bi].v, "interest_in", vget(&E[bi].v, "interest_in") + interest); interest_total += interest; vset(v, "interest_today", interest); }
                else { vset(v, "loan_balance", loan + interest); vset(&E[bi].v, "loans_out", vget(&E[bi].v, "loans_out") + interest); vset(v, "interest_today", 0); }
            }
        }
        int ci = civ_ent('c', e->civ, 1);
        if (ci >= 0) {
            long cash = vget(v, "cash"), tax = cash * vget(&E[ci].v, "tax_rate") / 10000;
            if (tax > 0) { vset(v, "cash", cash - tax); vset(&E[ci].v, "treasury", vget(&E[ci].v, "treasury") + tax); vset(v, "tax_today", tax); tax_total += tax; }
        }
    }
    for (int i = 0; i < NE; i++) if (E[i].kind == 'c') {          /* castle tax_in = what it took today */
        long tin = 0; for (int j = 0; j < NE; j++) if (E[j].kind == 's' && E[j].civ == E[i].civ) tin += vget(&E[j].v, "tax_today");
        vset(&E[i].v, "tax_in", tin);
    }
    for (int i = 0; i < NE; i++) if (E[i].kind == 'b') {          /* interest_in reset to today's receipts, not a running total */
        long iin = 0; for (int j = 0; j < NE; j++) if (E[j].kind == 's' && E[j].civ == E[i].civ && vget(&E[j].v, "loan_bank") == E[i].idx) iin += vget(&E[j].v, "interest_today");
        vset(&E[i].v, "interest_in", iin);
    }
    /* 6. reprice */
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v;
        if (e->kind != 's') continue;
        double loan = (double)vget(v, "loan_balance") / 100.0;
        double book = ((double)vget(v, "cash") + (double)vget(v, "stock") * (double)cost) / 100.0 - loan;
        double shares = (double)vget(v, "shares"), cur = (double)vget(v, "share_price") / 100.0;
        double mcap = cur * shares;
        double de = book > 0 ? loan / book : 10.0;
        float np = calculate_new_stock_price((float)book, (float)shares, (float)mcap, (float)de, (int)T("store_risk_bias"), (float)cur);
        long npc = llround((double)np * 100.0); if (npc < 10) npc = 10;
        vset(v, "share_price", npc);
    }
    /* 7. population */
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v;
        if (e->kind != 'p') continue;
        long count = vget(v, "count"), wages = vget(v, "wages_today");
        double demand = (double)count * (double)T("pop_food_need_ppm") / 1000000.0, supply = (double)vget(v, "food_supply");
        double adequacy = demand > 0 ? supply / demand : 1.0;
        double births = (T("pop_birth_milli_pct") / 1000.0) * (adequacy < 1.0 ? adequacy : 1.0);
        double shortage = 1.0 - adequacy; if (shortage < 0) shortage = 0;
        double deaths = (T("pop_death_milli_pct") / 1000.0) + shortage * (T("pop_death_milli_pct") / 1000.0) * 2.0;
        long ncount = llround((double)count * (1.0 + (births - deaths) / 100.0)); if (ncount < 0) ncount = 0;
        vset(v, "avg_wage", count > 0 ? wages / count : 0);
        long den = count * T("target_wage_c"), emp = den > 0 ? wages * 1000 / den : 0; if (emp > 1000) emp = 1000;
        vset(v, "unemployment", 1000 - emp);
        vset(v, "count", ncount);
    }
    /* conservation check BEFORE any write */
    long sink_after = vget(&W, "sink") + produced_cost_total;
    vset(&W, "sink", sink_after);
    long cash_after = total_cash();
    long lo = 0, lw = 0;
    for (int i = 0; i < NE; i++) { if (E[i].kind == 'b') lo += vget(&E[i].v, "loans_out"); if (E[i].kind == 's') lw += vget(&E[i].v, "loan_balance"); }
    if (cash_after != cash_before || lo != lw) {
        fprintf(stderr, "dsr_day_tick: INVARIANT VIOLATED day %ld: total cash %ld -> %ld, loans_out %ld vs loan_balance %ld; nothing written\n", day, cash_before, cash_after, lo, lw);
        return 3;
    }
    /* 8. history + world ledger + state */
    for (int i = 0; i < NE; i++) {
        Ent *e = &E[i]; Vars *v = &e->v; char row[256];
        if (e->kind == 's') snprintf(row, sizeof row, "DAY|%ld|%ld|%ld|%ld|%ld|%ld|%ld|%ld\n", day, vget(v, "cash"), vget(v, "price"), vget(v, "share_price"), vget(v, "stock"), vget(v, "sold_today"), vget(v, "wage_bill"), vget(v, "loan_balance"));
        else if (e->kind == 'b') snprintf(row, sizeof row, "DAY|%ld|%ld|%ld|%ld|%ld\n", day, vget(v, "reserves"), vget(v, "loans_out"), vget(v, "rate"), vget(v, "interest_in"));
        else if (e->kind == 'c') snprintf(row, sizeof row, "DAY|%ld|%ld|%ld|%ld\n", day, vget(v, "treasury"), vget(v, "tax_rate"), vget(v, "tax_in"));
        else snprintf(row, sizeof row, "DAY|%ld|%ld|%ld|%ld|%ld\n", day, vget(v, "count"), vget(v, "cash"), vget(v, "avg_wage"), vget(v, "unemployment"));
        snprintf(path, sizeof path, "%s/dsr_history.txt", e->dir);
        FILE *hf = fopen(path, "a"); if (!hf) { fprintf(stderr, "dsr_day_tick: cannot append %s\n", path); return 1; }
        fputs(row, hf); fclose(hf);
    }
    snprintf(path, sizeof path, "%s/world_ledger.txt", WORLD);
    FILE *lf = fopen(path, "a"); if (!lf) { fprintf(stderr, "dsr_day_tick: cannot append ledger\n"); return 1; }
    for (int g = 1; g <= NGOODS; g++) fprintf(lf, "MARKET|%ld|%s|%ld|%ld|%ld\n", day, GOODNAME[g], good_fills[g], good_units[g], good_last[g]);
    fprintf(lf, "DAY|%ld|%ld|%ld|%ld|%ld|%ld|%ld|%ld\n", day, cash_after, sink_after, lo, lw, wages_total, tax_total, interest_total);
    fclose(lf);
    for (int i = 0; i < NE; i++) { snprintf(path, sizeof path, "%s/variables.txt", E[i].dir); if (vsave(&E[i].v, path)) { fprintf(stderr, "dsr_day_tick: cannot write %s\n", path); return 1; } }
    vset(&W, "day", day);
    snprintf(path, sizeof path, "%s/variables.txt", WORLD);
    if (vsave(&W, path)) { fprintf(stderr, "dsr_day_tick: cannot write world\n"); return 1; }
    printf("dsr_day_tick: day %ld total cash %ld wages %ld tax %ld interest %ld\n", day, cash_after, wages_total, tax_total, interest_total);
    return 0;
}
