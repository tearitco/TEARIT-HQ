/* exchange_manager - Exchange HQ v2 (X11-HQ window module): an order book with bid/ask spread and matching, a loans market, candle and depth
 * charts, and the averaged preferred-value rates. Same three-part shape as chain-hq: template + this module + thin shims.
 * Publishes exchange_ui.txt (key=value, atomic) and chart.raw (via ops/+x/exchange_chart.+x), polls exchange_action.txt (seq=/cmd=).
 * ALL state is append-only files; every view is DERIVED by replaying them (never stored twice):
 *   exchange.pdl           settings (edited in the Settings tab, rewritten atomically)
 *   ledger/orders.txt      ORDER|id|ts|actor|side|unit|amount_mc|price_bp      CANCEL|id|ts|actor
 *   ledger/trades.txt      TRADE|n|ts|taker|side|unit|amount|price_bp|value|fee|maker|buy_id|sell_id      (one row per fill)
 *   ledger/rates.txt       RATE|unit|avg_bp|window_n|trades|ts                 (a row whenever a unit's averaged rate moves)
 *   ledger/loans.txt       OFFER|id|ts|lender|unit|amount|rate_bp_day|days     WITHDRAW|id|ts|lender
 *                          TAKE|loan_id|ts|borrower|offer_id                   REPAY|loan_id|ts|borrower|amount
 * Matching: price-time priority, fills at the RESTING order's price, no self-trade, taker fee (fee.percent) and maker fee (fee.maker_percent) in
 * preferred. Market orders take what is there and drop the remainder. PAPER LEDGER: balances are not checked and no chain coins move yet
 * (settlement comes with the lease layer, CHAIN-ECONOMY-DESIGN 3/10). The rate of a unit = volume-weighted price of its last N fills.
 * Cmds (cmd=...): TAB:<market|loans|orders|feed|settings>  UNIT:<id>  CHART  REFRESH  SET:<key> <+|->
 *   CMD:<actor>|<buy|sell unit amount [price_bp]> | <cancel id> | <lend unit amount rate_bp days> | <borrow offer_id> | <repay loan_id amount> | <withdraw offer_id>
 *   QUICK:<buy_mkt|sell_mkt|bid|ask|cancel|lend|borrow>      (hotbar presets, use the selected unit and quick.lot_mc)
 * Self-contained, no shared headers. Usage: exchange_manager.+x <house_root> <package_dir> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdarg.h>
#define PL 4096
#define MAXU 8
#define MAXT 4000
#define MAXO 2000
#define MAXL 600
#define LADDER 4
#define FEEDN 14
#define ROWS 14
#define CW 728
#define CH 526
static char pkg[PL], house[PL], cur_tab[16] = "market", sel_unit[16] = "mined", chart_mode[8] = "price", msg[320] = "";
typedef struct { char id[16], label[64]; long seed, avg, prev_avg, last, vol; int trades; } Unit;
static Unit U[MAXU]; static int nU = 0;
static double fee_pct = 2, maker_pct = 0; static int win_n = 20, band = 50; static long floor_bp = 10, ceil_bp = 1000000;
static long day_s = 600, max_rate = 500, max_days = 30, def_rate = 100, def_days = 2, lot = 1000;
typedef struct { long n, ts; char taker[48], side[8], unit[16], maker[48]; long amount, price, value, fee, buy_id, sell_id; } Trade;
static Trade T[MAXT]; static int nT = 0; static long fee_income = 0;
typedef struct { long id, ts, amount, price, filled; char actor[48], side[8], unit[16]; int canceled; } Order;
static Order O[MAXO]; static int nO = 0; static long next_oid = 1;
typedef struct { long id, ts, amount, rate, days, taken_by_loan; char lender[48], unit[16]; int withdrawn; } Offer;
typedef struct { long id, ts, principal, rate, days, offer_id, repaid; char borrower[48], lender[48], unit[16]; } Loan;
static Offer OF[MAXL]; static int nOF = 0; static Loan LN[MAXL]; static int nLN = 0; static long next_lid = 1;

static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0; }
static void path_of(char *o, const char *rel) { snprintf(o, PL, "%s/%s", pkg, rel); }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) trim(f[i]); return n; }
static void append_line(const char *rel, const char *line) { char p[PL]; path_of(p, rel); FILE *f = fopen(p, "a"); if (f) { fputs(line, f); fclose(f); } }
static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap); }

/* ---------------- settings ---------------- */
static void load_settings(void) {
    char p[PL], line[512]; path_of(p, "exchange.pdl"); FILE *f = fopen(p, "r"); nU = 0; if (!f) return;
    while (fgets(line, sizeof line, f)) { if (line[0] == '#') continue; char *c = strstr(line, "   #"); if (c) *c = 0; char *fl[6]; int n = split(line, fl, 6); if (n < 3) continue;
        if (!strcmp(fl[0], "FEE") && !strcmp(fl[1], "percent")) fee_pct = atof(fl[2]);
        else if (!strcmp(fl[0], "FEE") && !strcmp(fl[1], "maker_percent")) maker_pct = atof(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "window_trades")) win_n = atoi(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "band_pct")) band = atoi(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "floor_bp")) floor_bp = atol(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "ceiling_bp")) ceil_bp = atol(fl[2]);
        else if (!strcmp(fl[0], "LOAN") && !strcmp(fl[1], "day_seconds")) day_s = atol(fl[2]);
        else if (!strcmp(fl[0], "LOAN") && !strcmp(fl[1], "max_rate_bp")) max_rate = atol(fl[2]);
        else if (!strcmp(fl[0], "LOAN") && !strcmp(fl[1], "max_days")) max_days = atol(fl[2]);
        else if (!strcmp(fl[0], "LOAN") && !strcmp(fl[1], "default_rate_bp")) def_rate = atol(fl[2]);
        else if (!strcmp(fl[0], "LOAN") && !strcmp(fl[1], "default_days")) def_days = atol(fl[2]);
        else if (!strcmp(fl[0], "QUICK") && !strcmp(fl[1], "lot_mc")) lot = atol(fl[2]);
        else if (!strcmp(fl[0], "UNIT") && n >= 4 && nU < MAXU) { snprintf(U[nU].id, 16, "%s", fl[1]); snprintf(U[nU].label, 64, "%s", fl[2]); U[nU].seed = atol(fl[3]); nU++; } }
    fclose(f); if (win_n < 1) win_n = 1; if (win_n > 500) win_n = 500; if (day_s < 1) day_s = 1; if (lot < 1) lot = 1; }
static int set_setting(const char *key, int dir) {
    struct { const char *key, *sec, *kk; long step, lo, hi; } M[] = { {"fee_percent","FEE","percent",1,0,50}, {"maker_percent","FEE","maker_percent",1,0,50}, {"window_trades","RATE","window_trades",5,1,500}, {"band_pct","RATE","band_pct",10,5,500},
        {"seed_mined","UNIT","mined",50,1,100000000}, {"day_seconds","LOAN","day_seconds",60,1,86400}, {"lot_mc","QUICK","lot_mc",500,1,100000000}, {"default_rate","LOAN","default_rate_bp",25,1,500} };
    int mi = -1; for (int i = 0; i < (int)(sizeof M / sizeof M[0]); i++) if (!strcmp(M[i].key, key)) mi = i; if (mi < 0) return 0;
    char p[PL], tp[PL], line[512]; path_of(p, "exchange.pdl"); snprintf(tp, PL, "%s.tmp%d", p, (int)getpid()); FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!f || !o) { if (f) fclose(f); if (o) fclose(o); return 0; }
    int done = 0; while (fgets(line, sizeof line, f)) { char cp[512], cm[300] = "", w[512]; snprintf(cp, sizeof cp, "%s", line); char *c = strstr(cp, "   #"); if (c) { snprintf(cm, sizeof cm, "%s", c); *c = 0; } snprintf(w, sizeof w, "%s", cp); char *fl[6]; int n = line[0] == '#' ? 0 : split(w, fl, 6);
        if (n >= 3 && !strcmp(fl[0], M[mi].sec) && !strcmp(fl[1], M[mi].kk)) { int isu = !strcmp(M[mi].sec, "UNIT"); long v = atol(isu ? fl[3] : fl[2]) + dir * M[mi].step; if (v < M[mi].lo) v = M[mi].lo; if (v > M[mi].hi) v = M[mi].hi;
            /* replace ONLY the number, keep every other byte of the line (column padding, comment) as it was */
            const char *q = line; int bars = isu ? 3 : 2; while (bars-- > 0 && (q = strchr(q, '|'))) q++; if (!q) { fputs(line, o); done = 1; continue; } while (*q == ' ') q++; const char *e = q; while (*e >= '0' && *e <= '9') e++;
            fprintf(o, "%.*s%ld%s", (int)(q - line), line, v, e); done = 1; } else fputs(line, o); }
    fclose(f); fclose(o); if (!done) { remove(tp); return 0; } return rename(tp, p) == 0; }

/* ---------------- replay: fills, orders, rates ---------------- */
static void load_trades(void) {
    char p[PL], line[600]; path_of(p, "ledger/trades.txt"); FILE *f = fopen(p, "r"); nT = 0; fee_income = 0; if (!f) return;
    while (fgets(line, sizeof line, f) && nT < MAXT) { char *fl[14]; int n = split(line, fl, 14); if (n < 10 || strcmp(fl[0], "TRADE")) continue; Trade *t = &T[nT++]; memset(t, 0, sizeof *t); t->n = atol(fl[1]); t->ts = atol(fl[2]); snprintf(t->taker, 48, "%s", fl[3]); snprintf(t->side, 8, "%s", fl[4]); snprintf(t->unit, 16, "%s", fl[5]); t->amount = atol(fl[6]); t->price = atol(fl[7]); t->value = atol(fl[8]); t->fee = atol(fl[9]);
        if (n >= 13) { snprintf(t->maker, 48, "%s", fl[10]); t->buy_id = atol(fl[11]); t->sell_id = atol(fl[12]); } fee_income += t->fee; }
    fclose(f); }
static Order *find_order(long id) { for (int i = 0; i < nO; i++) if (O[i].id == id) return &O[i]; return NULL; }
static void load_orders(void) {
    char p[PL], line[400]; path_of(p, "ledger/orders.txt"); FILE *f = fopen(p, "r"); nO = 0; next_oid = 1; if (f) {
        while (fgets(line, sizeof line, f)) { char *fl[10]; int n = split(line, fl, 10); if (n < 4) continue;
            if (!strcmp(fl[0], "ORDER") && n >= 8 && nO < MAXO) { Order *o = &O[nO++]; memset(o, 0, sizeof *o); o->id = atol(fl[1]); o->ts = atol(fl[2]); snprintf(o->actor, 48, "%s", fl[3]); snprintf(o->side, 8, "%s", fl[4]); snprintf(o->unit, 16, "%s", fl[5]); o->amount = atol(fl[6]); o->price = atol(fl[7]); if (o->id >= next_oid) next_oid = o->id + 1; }
            else if (!strcmp(fl[0], "CANCEL")) { Order *o = find_order(atol(fl[1])); if (o) o->canceled = 1; } } fclose(f); }
    for (int i = 0; i < nT; i++) { if (T[i].buy_id) { Order *o = find_order(T[i].buy_id); if (o) o->filled += T[i].amount; } if (T[i].sell_id) { Order *o = find_order(T[i].sell_id); if (o) o->filled += T[i].amount; } } }
static long clampbp(long v) { if (v < floor_bp) v = floor_bp; if (v > ceil_bp) v = ceil_bp; return v; }
static int unit_index(const char *id) { for (int i = 0; i < nU; i++) if (!strcmp(U[i].id, id)) return i; return -1; }
static long vwap(int ui, int upto, int *cnt) { double sv = 0, sa = 0; int c = 0; for (int i = upto - 1; i >= 0 && c < win_n; i--) if (!strcmp(T[i].unit, U[ui].id)) { sv += (double)T[i].price * T[i].amount; sa += T[i].amount; c++; } if (cnt) *cnt = c; if (sa <= 0) return U[ui].seed; return clampbp((long)(sv / sa + 0.5)); }
static void derive(void) { for (int u = 0; u < nU; u++) { int c = 0; U[u].avg = vwap(u, nT, &c); U[u].prev_avg = nT ? vwap(u, nT - 1, NULL) : U[u].seed; U[u].trades = 0; U[u].vol = 0; U[u].last = U[u].seed;
        for (int i = 0; i < nT; i++) if (!strcmp(T[i].unit, U[u].id)) { U[u].trades++; U[u].vol += T[i].amount; U[u].last = T[i].price; } if (!strcmp(U[u].id, "cones")) U[u].avg = 10000; } }
/* book of the selected unit */
typedef struct { long price, amount, cum; } Lvl;
static Lvl BIDS[64], ASKS[64]; static int nBID = 0, nASK = 0; static long best_bid = 0, best_ask = 0;
static int cmp_lvl_desc(const void *a, const void *b) { long x = ((const Lvl *)a)->price, y = ((const Lvl *)b)->price; return x < y ? 1 : x > y ? -1 : 0; }
static int cmp_lvl_asc(const void *a, const void *b) { return -cmp_lvl_desc(a, b); }
static void build_book(void) { nBID = nASK = 0; best_bid = best_ask = 0;
    for (int i = 0; i < nO; i++) { Order *o = &O[i]; if (o->canceled || strcmp(o->unit, sel_unit)) continue; long rem = o->amount - o->filled; if (rem <= 0 || o->price <= 0) continue; int bid = !strcmp(o->side, "buy"); Lvl *L = bid ? BIDS : ASKS; int *n = bid ? &nBID : &nASK; int k;
        for (k = 0; k < *n; k++) if (L[k].price == o->price) break; if (k == *n) { if (*n >= 64) continue; L[k].price = o->price; L[k].amount = 0; (*n)++; } L[k].amount += rem; }
    qsort(BIDS, nBID, sizeof(Lvl), cmp_lvl_desc); qsort(ASKS, nASK, sizeof(Lvl), cmp_lvl_asc); long c = 0; for (int i = 0; i < nBID; i++) { c += BIDS[i].amount; BIDS[i].cum = c; } c = 0; for (int i = 0; i < nASK; i++) { c += ASKS[i].amount; ASKS[i].cum = c; }
    if (nBID) best_bid = BIDS[0].price; if (nASK) best_ask = ASKS[0].price; }
static void load_all(void) { load_trades(); load_orders(); derive(); build_book(); }

/* ---------------- orders and matching ---------------- */
static void place_order(const char *actor, const char *side, const char *unit, long amount, long price) {
    int ui = unit_index(unit); if (ui < 0) { say("unknown unit %s", unit); return; } if (!strcmp(unit, "cones")) { say("cones are the preferred unit; trade another unit against them"); return; } if (amount <= 0) { say("amount must be above 0"); return; }
    int buy = !strcmp(side, "buy"); load_all(); long avg = U[ui].avg; int market = price <= 0;
    if (!market && labs(price - avg) * 100 > avg * band) { say("refused: %ld bp is more than %d%% from the %ld bp average", price, band, avg); return; }
    long oid = next_oid; char line[400]; time_t now = time(NULL); snprintf(line, sizeof line, "ORDER|%ld|%ld|%s|%s|%s|%ld|%ld\n", oid, (long)now, actor, side, unit, amount, market ? 0L : price); append_line("ledger/orders.txt", line);
    load_all(); Order *me = find_order(oid); long left = amount, filled_total = 0, notional = 0; int maker_fills = 0;
    while (left > 0) { Order *best = NULL; for (int i = 0; i < nO; i++) { Order *o = &O[i]; if (o->canceled || o->id == oid || strcmp(o->unit, unit) || !strcmp(o->side, side) || !strcmp(o->actor, actor)) continue; if (o->amount - o->filled <= 0 || o->price <= 0) continue;
            if (!market && (buy ? o->price > price : o->price < price)) continue; if (!best || (buy ? o->price < best->price : o->price > best->price) || (o->price == best->price && o->id < best->id)) best = o; }
        if (!best) break; long q = best->amount - best->filled; if (q > left) q = left; long px = best->price, value = (long)((double)q * px / 10000.0 + 0.5); long tfee = (long)ceil((double)value * fee_pct / 100.0), mfee = (long)ceil((double)value * maker_pct / 100.0);
        long nn = (nT ? T[nT - 1].n : 0) + 1; snprintf(line, sizeof line, "TRADE|%ld|%ld|%s|%s|%s|%ld|%ld|%ld|%ld|%s|%ld|%ld\n", nn, (long)now, actor, side, unit, q, px, value, tfee + mfee, best->actor, buy ? oid : best->id, buy ? best->id : oid); append_line("ledger/trades.txt", line);
        left -= q; filled_total += q; notional += (long)((double)q * px); maker_fills++; load_trades(); load_orders(); me = find_order(oid); }
    long before = U[ui].avg; load_all(); if (U[ui].avg != before) { snprintf(line, sizeof line, "RATE|%s|%ld|%d|%d|%ld\n", unit, U[ui].avg, win_n, U[ui].trades, (long)now); append_line("ledger/rates.txt", line); }
    if (market && left > 0) { snprintf(line, sizeof line, "CANCEL|%ld|%ld|%s\n", oid, (long)now, actor); append_line("ledger/orders.txt", line); load_all(); }
    (void)me; if (filled_total) say("%s %s %ld of %ld mc %s, avg %.4f%s", actor, buy ? "bought" : "sold", filled_total, amount, unit, (double)notional / filled_total / 10000.0, left > 0 ? (market ? " (rest dropped, no liquidity)" : " (rest resting in the book)") : "");
    else if (market) say("%s: no liquidity to %s %s against", actor, side, unit); else say("%s: %s %ld mc %s @ %.4f is resting in the book (#%ld)", actor, side, amount, unit, price / 10000.0, oid); }
static void cancel_order(const char *actor, long id) { load_all(); Order *o = find_order(id); if (!o) { say("no such order #%ld", id); return; } if (strcmp(o->actor, actor)) { say("only %s can cancel #%ld", o->actor, id); return; } if (o->canceled || o->amount - o->filled <= 0) { say("#%ld is not open", id); return; }
    char line[200]; snprintf(line, sizeof line, "CANCEL|%ld|%ld|%s\n", id, (long)time(NULL), actor); append_line("ledger/orders.txt", line); say("#%ld cancelled", id); load_all(); }

/* ---------------- loans ---------------- */
static void load_loans(void) { char p[PL], line[400]; path_of(p, "ledger/loans.txt"); FILE *f = fopen(p, "r"); nOF = nLN = 0; next_lid = 1; if (!f) return;
    while (fgets(line, sizeof line, f)) { char *fl[9]; int n = split(line, fl, 9); if (n < 4) continue;
        if (!strcmp(fl[0], "OFFER") && n >= 8 && nOF < MAXL) { Offer *o = &OF[nOF++]; memset(o, 0, sizeof *o); o->id = atol(fl[1]); o->ts = atol(fl[2]); snprintf(o->lender, 48, "%s", fl[3]); snprintf(o->unit, 16, "%s", fl[4]); o->amount = atol(fl[5]); o->rate = atol(fl[6]); o->days = atol(fl[7]); if (o->id >= next_lid) next_lid = o->id + 1; }
        else if (!strcmp(fl[0], "WITHDRAW")) { for (int i = 0; i < nOF; i++) if (OF[i].id == atol(fl[1])) OF[i].withdrawn = 1; }
        else if (!strcmp(fl[0], "TAKE") && n >= 5 && nLN < MAXL) { Offer *o = NULL; for (int i = 0; i < nOF; i++) if (OF[i].id == atol(fl[4])) o = &OF[i]; if (!o) continue; Loan *l = &LN[nLN++]; memset(l, 0, sizeof *l); l->id = atol(fl[1]); l->ts = atol(fl[2]); snprintf(l->borrower, 48, "%s", fl[3]); snprintf(l->lender, 48, "%s", o->lender); snprintf(l->unit, 16, "%s", o->unit); l->principal = o->amount; l->rate = o->rate; l->days = o->days; l->offer_id = o->id; o->taken_by_loan = l->id; if (l->id >= next_lid) next_lid = l->id + 1; }
        else if (!strcmp(fl[0], "REPAY") && n >= 5) { for (int i = 0; i < nLN; i++) if (LN[i].id == atol(fl[1])) LN[i].repaid += atol(fl[4]); } } fclose(f); }
static double loan_interest(const Loan *l, time_t now) { double days = (double)(now - l->ts) / (double)day_s; return (double)l->principal * l->rate / 10000.0 * days; }
static long loan_owed(const Loan *l, time_t now) { double owed = l->principal + loan_interest(l, now) - l->repaid; return owed < 0 ? 0 : (long)(owed + 0.5); }
static const char *loan_state(const Loan *l, time_t now) { long owed = loan_owed(l, now); if (owed <= 0) return "repaid"; if (now > l->ts + l->days * day_s) return "OVERDUE"; return "open"; }
static Offer *find_offer(long id) { for (int i = 0; i < nOF; i++) if (OF[i].id == id) return &OF[i]; return NULL; }
static Loan *find_loan(long id) { for (int i = 0; i < nLN; i++) if (LN[i].id == id) return &LN[i]; return NULL; }
static void do_lend(const char *actor, const char *unit, long amount, long rate, long days) { load_loans(); if (unit_index(unit) < 0) { say("unknown unit %s", unit); return; } if (amount <= 0) { say("amount must be above 0"); return; } if (rate < 1 || rate > max_rate) { say("rate must be 1..%ld bp per day", max_rate); return; } if (days < 1 || days > max_days) { say("days must be 1..%ld", max_days); return; }
    char line[300]; snprintf(line, sizeof line, "OFFER|%ld|%ld|%s|%s|%ld|%ld|%ld\n", next_lid, (long)time(NULL), actor, unit, amount, rate, days); append_line("ledger/loans.txt", line); say("#%ld: %s offers %ld mc %s at %.2f%% per day for %ld days", next_lid, actor, amount, unit, rate / 100.0, days); }
static void do_borrow(const char *actor, long oid) { load_loans(); Offer *o = find_offer(oid); if (!o) { say("no such offer #%ld", oid); return; } if (o->withdrawn || o->taken_by_loan) { say("offer #%ld is not available", oid); return; } if (!strcmp(o->lender, actor)) { say("you cannot borrow your own offer"); return; }
    char line[300]; snprintf(line, sizeof line, "TAKE|%ld|%ld|%s|%ld\n", next_lid, (long)time(NULL), actor, oid); append_line("ledger/loans.txt", line); say("loan #%ld: %s borrowed %ld mc %s from %s at %.2f%% per day", next_lid, actor, o->amount, o->unit, o->lender, o->rate / 100.0); }
static void do_repay(const char *actor, long lid, long amt) { load_loans(); Loan *l = find_loan(lid); if (!l) { say("no such loan #%ld", lid); return; } if (strcmp(l->borrower, actor)) { say("only %s can repay loan #%ld", l->borrower, lid); return; } long owed = loan_owed(l, time(NULL)); if (owed <= 0) { say("loan #%ld is already repaid", lid); return; } if (amt <= 0) { say("amount must be above 0"); return; } if (amt > owed) amt = owed;
    char line[300]; snprintf(line, sizeof line, "REPAY|%ld|%ld|%s|%ld\n", lid, (long)time(NULL), actor, amt); append_line("ledger/loans.txt", line); say("loan #%ld: %s repaid %ld, still owes %ld", lid, actor, amt, owed - amt); }
static void do_withdraw(const char *actor, long oid) { load_loans(); Offer *o = find_offer(oid); if (!o) { say("no such offer #%ld", oid); return; } if (strcmp(o->lender, actor)) { say("only %s can withdraw offer #%ld", o->lender, oid); return; } if (o->withdrawn || o->taken_by_loan) { say("offer #%ld is not open", oid); return; }
    char line[200]; snprintf(line, sizeof line, "WITHDRAW|%ld|%ld|%s\n", oid, (long)time(NULL), actor); append_line("ledger/loans.txt", line); say("offer #%ld withdrawn", oid); }

/* ---------------- commands ---------------- */
static const char *me_name(void) { const char *u = getenv("USER"); return u && *u ? u : "human"; }
static void do_cmd_line(const char *arg) { char b[400]; snprintf(b, sizeof b, "%s", arg); char actor[48]; snprintf(actor, 48, "%s", me_name()); char *bar = strchr(b, '|'); char *line = b; if (bar) { *bar = 0; snprintf(actor, 48, "%.40s", b); line = bar + 1; } trim(line);
    char *ap = strstr(line, "actor="); if (ap) { snprintf(actor, 48, "%.40s", ap + 6); *ap = 0; char *sp = strchr(actor, ' '); if (sp) *sp = 0; trim(line); }
    char verb[16] = "", unit[32] = ""; long a = 0, p = 0, d = 0; sscanf(line, "%15s", verb); char *rest = line + strlen(verb);
    if (!strcmp(verb, "buy") || !strcmp(verb, "sell")) { int n = sscanf(rest, "%31s %ld %ld", unit, &a, &p); if (n < 2) { say("usage: buy|sell <unit> <amount_mc> [price_bp]"); return; } place_order(actor, verb, unit, a, n >= 3 ? p : 0); }
    else if (!strcmp(verb, "cancel")) { if (sscanf(rest, "%ld", &a) < 1) { say("usage: cancel <order id>"); return; } cancel_order(actor, a); }
    else if (!strcmp(verb, "lend")) { if (sscanf(rest, "%31s %ld %ld %ld", unit, &a, &p, &d) < 4) { say("usage: lend <unit> <amount_mc> <rate_bp_per_day> <days>"); return; } do_lend(actor, unit, a, p, d); }
    else if (!strcmp(verb, "borrow")) { if (sscanf(rest, "%ld", &a) < 1) { say("usage: borrow <offer id>"); return; } do_borrow(actor, a); }
    else if (!strcmp(verb, "repay")) { if (sscanf(rest, "%ld %ld", &a, &p) < 2) { say("usage: repay <loan id> <amount>"); return; } do_repay(actor, a, p); }
    else if (!strcmp(verb, "withdraw")) { if (sscanf(rest, "%ld", &a) < 1) { say("usage: withdraw <offer id>"); return; } do_withdraw(actor, a); }
    else say("verbs: buy sell cancel lend borrow repay withdraw"); }
static void do_quick(const char *q) { const char *me = me_name(); load_all(); load_loans();
    if (!strcmp(q, "buy_mkt")) place_order(me, "buy", sel_unit, lot, 0); else if (!strcmp(q, "sell_mkt")) place_order(me, "sell", sel_unit, lot, 0);
    else if (!strcmp(q, "bid")) { if (best_bid) place_order(me, "buy", sel_unit, lot, best_bid); else { int ui = unit_index(sel_unit); place_order(me, "buy", sel_unit, lot, ui >= 0 ? U[ui].avg * 99 / 100 : 0); } }
    else if (!strcmp(q, "ask")) { if (best_ask) place_order(me, "sell", sel_unit, lot, best_ask); else { int ui = unit_index(sel_unit); place_order(me, "sell", sel_unit, lot, ui >= 0 ? U[ui].avg * 101 / 100 : 0); } }
    else if (!strcmp(q, "cancel")) { long last = 0; for (int i = 0; i < nO; i++) if (!strcmp(O[i].actor, me) && !O[i].canceled && O[i].amount - O[i].filled > 0 && O[i].id > last) last = O[i].id; if (last) cancel_order(me, last); else say("you have no open order to cancel"); }
    else if (!strcmp(q, "lend")) do_lend(me, sel_unit, lot, def_rate, def_days);
    else if (!strcmp(q, "borrow")) { long best = 0, br = 1 << 30; for (int i = 0; i < nOF; i++) if (!OF[i].withdrawn && !OF[i].taken_by_loan && strcmp(OF[i].lender, me) != 0 && OF[i].rate < br) { br = OF[i].rate; best = OF[i].id; } if (best) do_borrow(me, best); else say("no open loan offer to take"); } }

/* ---------------- chart ---------------- */
static void run_chart(void) { char in[PL], base[PL]; path_of(in, "chart_in.txt"); path_of(base, "chart"); FILE *f = fopen(in, "w"); if (!f) return; int ui = unit_index(sel_unit);
    fprintf(f, "MODE %s\nUNIT %s\nSIZE %d %d\nPADB 100\nBID %ld\nASK %ld\nAVG %ld\n", chart_mode, sel_unit, CW, CH, best_bid, best_ask, ui >= 0 ? U[ui].avg : 0L);
    int start = 0, cnt = 0; for (int i = nT - 1; i >= 0 && cnt < 300; i--) if (!strcmp(T[i].unit, sel_unit)) { start = i; cnt++; } for (int i = start; i < nT; i++) if (!strcmp(T[i].unit, sel_unit)) fprintf(f, "T %ld %ld\n", T[i].price, T[i].amount);
    for (int i = 0; i < nBID && i < 40; i++) fprintf(f, "B %ld %ld\n", BIDS[i].price, BIDS[i].cum); for (int i = 0; i < nASK && i < 40; i++) fprintf(f, "A %ld %ld\n", ASKS[i].price, ASKS[i].cum); fclose(f);
    char bin[PL]; path_of(bin, "ops/+x/exchange_chart.+x"); pid_t pid = fork(); if (pid == 0) { setsid(); int dn = open("/dev/null", 0); (void)dn; execl(bin, bin, in, base, (char *)NULL); _exit(127); }
    if (pid > 0) { for (int i = 0; i < 100; i++) { int st; if (waitpid(pid, &st, WNOHANG) == pid) return; usleep(20000); } kill(pid, SIGKILL); waitpid(pid, NULL, 0); } }

/* ---------------- ui ---------------- */
static void fmt_bp(char *o, size_t n, long bp) { snprintf(o, n, "%ld.%04ld", bp / 10000, labs(bp % 10000)); }
static void hhmmss(char *o, size_t n, long ts) { struct tm tm; time_t t = ts; localtime_r(&t, &tm); strftime(o, n, "%H:%M:%S", &tm); }
static void dur(char *o, size_t n, long s) { if (s < 0) s = 0; if (s >= 3600) snprintf(o, n, "%ldh%02ldm", s / 3600, (s % 3600) / 60); else snprintf(o, n, "%ldm%02lds", s / 60, s % 60); }
static void write_ui(void) {
    char tmp[PL], dst[PL]; path_of(dst, "exchange_ui.txt"); snprintf(tmp, PL, "%s.tmp", dst); FILE *f = fopen(tmp, "w"); if (!f) return; time_t now = time(NULL);
    const char *tabs[] = { "market", "loans", "orders", "feed", "settings" }; for (int i = 0; i < 5; i++) { int a = !strcmp(cur_tab, tabs[i]); fprintf(f, "tab_%s=%s\ncls_%s=%s\n", tabs[i], a ? "1" : "", tabs[i], a ? "active" : ""); }
    int ui = unit_index(sel_unit); const char *ul = ui >= 0 ? U[ui].label : sel_unit; char a[24], b[24], c[24];
    fprintf(f, "status=%s\n", msg[0] ? msg : "paper ledger: no chain coins move yet (settlement comes with the lease layer)");
    fprintf(f, "title=Exchange  ·  %s  ·  %d fills\n", sel_unit, nT); fprintf(f, "chart_raw=%s/chart.raw\nchart_mode=%s\nsel_unit=%s\nunit_label=%s\nhb_title=%s chart  ·  %s  ·  lot %ld mc\n", pkg, chart_mode, sel_unit, ul, chart_mode, sel_unit, lot);
    fprintf(f, "cls_u_mined=%s\ncls_u_play=%s\n", !strcmp(sel_unit, "mined") ? "sel" : "", !strcmp(sel_unit, "play") ? "sel" : "");
    long avg = ui >= 0 ? U[ui].avg : 0, last = ui >= 0 ? U[ui].last : 0; long spread = (best_bid && best_ask) ? best_ask - best_bid : 0; fmt_bp(a, 24, last); fmt_bp(b, 24, avg);
    fprintf(f, "sb_unit=%s\nsb_last=last %s\nsb_avg=avg %s (pref)\n", ul, a, b);
    if (best_bid) { fmt_bp(a, 24, best_bid); fprintf(f, "sb_bid=bid  %s  x %ld\n", a, BIDS[0].amount); } else fprintf(f, "sb_bid=bid  -\n");
    if (best_ask) { fmt_bp(a, 24, best_ask); fprintf(f, "sb_ask=ask  %s  x %ld\n", a, ASKS[0].amount); } else fprintf(f, "sb_ask=ask  -\n");
    if (spread) { long mid = (best_bid + best_ask) / 2; fmt_bp(a, 24, spread); fmt_bp(b, 24, mid); fprintf(f, "sb_spread=spread %s (%.2f%%)\nsb_mid=mid  %s\n", a, mid ? 100.0 * spread / mid : 0.0, b); } else fprintf(f, "sb_spread=spread -\nsb_mid=mid  -\n");
    long hi = 0, lo = 0, vol = 0; int cnt = 0; for (int i = nT - 1; i >= 0 && cnt < win_n; i--) if (!strcmp(T[i].unit, sel_unit)) { if (!hi || T[i].price > hi) hi = T[i].price; if (!lo || T[i].price < lo) lo = T[i].price; vol += T[i].amount; cnt++; }
    if (cnt) { fmt_bp(a, 24, hi); fmt_bp(b, 24, lo); fprintf(f, "sb_hilo=hi %s  lo %s\nsb_vol=vol %ld mc (%d fills)\n", a, b, vol, cnt); } else fprintf(f, "sb_hilo=no fills yet\nsb_vol=\n");
    fprintf(f, "sb_fee=fee: taker %.0f%%  maker %.0f%%\nsb_inc=fee income %ld preferred mc\n", fee_pct, maker_pct, fee_income);
    /* ladder: 5 asks (worst first), spread, 5 bids */
    int k = 0; for (int i = LADDER - 1; i >= 0; i--) { if (i < nASK) { fmt_bp(a, 24, ASKS[i].price); fprintf(f, "ob_%d=ASK %s %7ld\n", k++, a, ASKS[i].amount); } else fprintf(f, "ob_%d=ASK      -\n", k++); }
    if (spread) { fmt_bp(a, 24, spread); fprintf(f, "ob_%d=-- spread %s --\n", k++, a); } else fprintf(f, "ob_%d=-- no spread --\n", k++);
    for (int i = 0; i < LADDER; i++) { if (i < nBID) { fmt_bp(a, 24, BIDS[i].price); fprintf(f, "ob_%d=BID %s %7ld\n", k++, a, BIDS[i].amount); } else fprintf(f, "ob_%d=BID      -\n", k++); }
    /* orders tab: open orders of all units, newest first */
    int no = 0; for (int i = nO - 1; i >= 0 && no < ROWS; i--) { Order *o = &O[i]; long rem = o->amount - o->filled; if (o->canceled || rem <= 0) continue; fmt_bp(a, 24, o->price); fprintf(f, "o_%d_text=#%-4ld %-9.9s %-4s %-6.6s %6ld/%-6ld @ %s\n", no++, o->id, o->actor, o->side, o->unit, rem, o->amount, o->price ? a : "mkt"); }
    fprintf(f, "n_o=%d\norders_hdr=%-5s %-9s %-4s %-6s %13s   price\norders_empty=%s\n", no, "id", "actor", "side", "unit", "left/total", no ? "" : "No open orders. Type: buy mined 2000 540   or use the hotbar on the Market tab");
    /* loans tab */
    load_loans(); int nl = 0; for (int i = nOF - 1; i >= 0 && nl < 6; i--) { Offer *o = &OF[i]; if (o->withdrawn || o->taken_by_loan) continue; fprintf(f, "l_%d_text=OFFER #%-3ld %-8.8s lends %7ld %-5s %.2f%%/day  %ldd\n", nl++, o->id, o->lender, o->amount, o->unit, o->rate / 100.0, o->days); }
    int nl2 = 0; for (int i = nLN - 1; i >= 0 && nl2 < 6; i--) { Loan *l = &LN[i]; char dd[16]; dur(dd, 16, l->ts + l->days * day_s - now); fprintf(f, "ln_%d_text=LOAN  #%-3ld %-8.8s owes %-8.8s %7ld %-5s  %-7s due %s\n", nl2++, l->id, l->borrower, l->lender, loan_owed(l, now), l->unit, loan_state(l, now), dd); }
    fprintf(f, "n_l=%d\nn_ln=%d\nloans_empty=%s\n", nl, nl2, (nl + nl2) ? "" : "No loans yet. Type: lend mined 5000 100 2   (unit amount rate_bp_per_day days)   then borrow <offer id>");
    /* feed */
    int nf = 0; for (int i = nT - 1; i >= 0 && nf < FEEDN; i--, nf++) { hhmmss(c, 24, T[i].ts); fmt_bp(a, 24, T[i].price); fprintf(f, "f_%d_text=%s %-9.9s %-4s %-6.6s %7ld @ %s fee %ld%s%s\n", nf, c, T[i].taker, T[i].side, T[i].unit, T[i].amount, a, T[i].fee, T[i].maker[0] ? " vs " : "", T[i].maker); }
    fprintf(f, "n_f=%d\nfeed_empty=%s\n", nf, nf ? "" : "No fills yet. Orders that cross the book make fills.");
    /* settings */
    struct { const char *key, *label; double v; } S[] = { {"fee_percent","Taker fee percent (preferred)",fee_pct}, {"maker_percent","Maker fee percent",maker_pct}, {"window_trades","Averaging window (fills)",win_n}, {"band_pct","Price band (percent from average)",band},
        {"day_seconds","Loan day length (seconds)",(double)day_s}, {"lot_mc","Hotbar lot size (mc)",(double)lot}, {"default_rate","Hotbar lend rate (bp per day)",(double)def_rate}, {"seed_mined","Seed value of mined cones (bp)",0} };
    for (int u = 0; u < nU; u++) if (!strcmp(U[u].id, "mined")) S[7].v = (double)U[u].seed; int ns = (int)(sizeof S / sizeof S[0]); fprintf(f, "n_set=%d\n", ns); for (int i = 0; i < ns; i++) fprintf(f, "s_%d_label=%s   %.0f\ns_%d_key=%s\n", i, S[i].label, S[i].v, i, S[i].key);
    fclose(f); rename(tmp, dst); }
static void refresh(void) { load_settings(); load_all(); load_loans(); if (!strcmp(cur_tab, "market")) run_chart(); write_ui(); }
static void do_cmd(const char *cmd) {
    if (!strncmp(cmd, "TAB:", 4)) { snprintf(cur_tab, sizeof cur_tab, "%s", cmd + 4); msg[0] = 0; }
    else if (!strncmp(cmd, "UNIT:", 5)) { if (unit_index(cmd + 5) >= 0 && strcmp(cmd + 5, "cones")) snprintf(sel_unit, sizeof sel_unit, "%s", cmd + 5); msg[0] = 0; }
    else if (!strcmp(cmd, "CHART")) snprintf(chart_mode, sizeof chart_mode, "%s", !strcmp(chart_mode, "price") ? "depth" : "price");
    else if (!strncmp(cmd, "CMD:", 4)) do_cmd_line(cmd + 4);
    else if (!strncmp(cmd, "QUICK:", 6)) do_quick(cmd + 6);
    else if (!strncmp(cmd, "SET:", 4)) { char k[48] = "", d[4] = ""; sscanf(cmd + 4, "%47s %3s", k, d); if (set_setting(k, d[0] == '-' ? -1 : 1)) say("saved %s", k); else say("could not save %s", k); }
    refresh(); }
static void poll(int *last) { char p[PL], buf[1500]; path_of(p, "exchange_action.txt"); FILE *f = fopen(p, "r"); if (!f) return; size_t nr = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[nr] = 0; int seq = 0; char cmd[1024] = "";
    for (char *ls = buf; *ls;) { char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls); if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4); else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof cmd) cl = sizeof cmd - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = 0; } if (!le) break; ls = le + 1; }
    if (seq > *last && cmd[0]) { *last = seq; do_cmd(cmd); } }
static void bye(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "--once")) { /* harness/script mode: apply ONE command (house, package, cmd), write the ui + chart, print the status line, exit */
        snprintf(house, PL, "%s", argv[2]); snprintf(pkg, PL, "%s", argv[3]); { char p[PL]; path_of(p, "ledger"); mkdir(p, 0755); } refresh(); do_cmd(argv[4]); printf("%s\n", msg); return 0; }
    if (argc < 3) { fprintf(stderr, "Usage: %s <house_root> <package_dir>   or   %s --once <house_root> <package_dir> <cmd>\n", argv[0], argv[0]); return 1; }
    snprintf(house, PL, "%s", argv[1]); snprintf(pkg, PL, "%s", argv[2]); signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
    { char p[PL]; path_of(p, "ledger"); mkdir(p, 0755); path_of(p, "exchange_action.txt"); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } }
    refresh(); int last = 0, slow = 0; for (;;) { usleep(50000); poll(&last); if (++slow >= 100) { slow = 0; refresh(); } } }
