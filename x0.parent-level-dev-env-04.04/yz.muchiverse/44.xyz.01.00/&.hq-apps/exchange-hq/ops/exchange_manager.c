/* exchange_manager - Exchange HQ (X11-HQ window module). Same three-part shape as chain-hq: template + this module + thin shims.
 * Publishes exchange_ui.txt (key=value, atomic), polls exchange_action.txt (seq=/cmd=). All state is files:
 *   exchange.pdl            settings (edited from the Settings tab, rewritten atomically)
 *   ledger/trades.txt       append-only  TRADE|n|ts|actor|side|unit|amount_mc|price_bp|value_mc|fee_mc
 *   ledger/rates.txt        append-only  RATE|unit|avg_bp|window_n|trades|ts   (a row whenever a unit's averaged rate moves)
 * Rates are DERIVED by replaying trades.txt (volume-weighted over the last window_trades of each unit), never stored twice.
 * Trades are a PAPER ledger for now: nothing here moves chain coins (settlement layer comes with the leases, CHAIN-ECONOMY-DESIGN 3/10).
 * Cmds: TAB:<rates|trade|feed|settings>  REFRESH  TRADE:<side> <unit> <amount_mc> [price_bp] [actor=<name>]  SET:<key> <+|->
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
#define PL 4096
#define MAXU 8
#define MAXT 4000
#define FEEDN 14
static char pkg[PL], house[PL], cur_tab[16] = "rates", msg[300] = "";
typedef struct { char id[16], label[64]; long seed; long avg, prev_avg, last; long vol; int trades; } Unit;
static Unit U[MAXU]; static int nU = 0;
static double fee_pct = 2; static int win_n = 20, band = 50; static long floor_bp = 10, ceil_bp = 1000000;
typedef struct { long n, ts; char actor[48], side[8], unit[16]; long amount, price, value, fee; } Trade;
static Trade T[MAXT]; static int nT = 0; static long fee_income = 0;

static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0; }
static void path_of(char *o, const char *rel) { snprintf(o, PL, "%s/%s", pkg, rel); }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) trim(f[i]); return n; }

static void load_settings(void) {
    char p[PL], line[512]; path_of(p, "exchange.pdl"); FILE *f = fopen(p, "r"); nU = 0; if (!f) return;
    while (fgets(line, sizeof line, f)) { if (line[0] == '#') continue; char *c = strstr(line, "   #"); if (c) *c = 0; char *fl[6]; int n = split(line, fl, 6); if (n < 3) continue;
        if (!strcmp(fl[0], "FEE") && !strcmp(fl[1], "percent")) fee_pct = atof(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "window_trades")) win_n = atoi(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "band_pct")) band = atoi(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "floor_bp")) floor_bp = atol(fl[2]);
        else if (!strcmp(fl[0], "RATE") && !strcmp(fl[1], "ceiling_bp")) ceil_bp = atol(fl[2]);
        else if (!strcmp(fl[0], "UNIT") && n >= 4 && nU < MAXU) { snprintf(U[nU].id, 16, "%s", fl[1]); snprintf(U[nU].label, 64, "%s", fl[2]); U[nU].seed = atol(fl[3]); nU++; } }
    fclose(f); if (win_n < 1) win_n = 1; if (win_n > 500) win_n = 500; }
static int set_setting(const char *key, int dir) { /* rewrite exchange.pdl atomically; key is a bare name (fee_percent, window_trades, band_pct, seed_mined) */
    char p[PL], tp[PL], line[512]; path_of(p, "exchange.pdl"); snprintf(tp, PL, "%s.tmp%d", p, (int)getpid()); FILE *f = fopen(p, "r"), *o = fopen(tp, "w"); if (!f || !o) { if (f) fclose(f); if (o) fclose(o); return 0; }
    const char *sec = "", *kk = ""; long step = 1, lo = 0, hi = 100000000; if (!strcmp(key, "fee_percent")) { sec = "FEE"; kk = "percent"; step = 1; hi = 50; } else if (!strcmp(key, "window_trades")) { sec = "RATE"; kk = "window_trades"; step = 5; lo = 1; hi = 500; } else if (!strcmp(key, "band_pct")) { sec = "RATE"; kk = "band_pct"; step = 10; lo = 5; hi = 500; } else if (!strcmp(key, "seed_mined")) { sec = "UNIT"; kk = "mined"; step = 50; lo = 1; } else { fclose(f); fclose(o); remove(tp); return 0; }
    int done = 0; while (fgets(line, sizeof line, f)) { char cp[512]; snprintf(cp, sizeof cp, "%s", line); char *c = strstr(cp, "   #"); char cm[200] = ""; if (c) { snprintf(cm, sizeof cm, "%s", c); *c = 0; } char *fl[6]; char w[512]; snprintf(w, sizeof w, "%s", cp); int n = line[0] == '#' ? 0 : split(w, fl, 6);
        if (n >= 3 && !strcmp(fl[0], sec) && !strcmp(fl[1], kk)) { if (!strcmp(sec, "UNIT")) { long v = atol(fl[3]) + dir * step; if (v < lo) v = lo; if (v > hi) v = hi; fprintf(o, "UNIT  | %s | %s | %ld%s", fl[1], fl[2], v, cm[0] ? cm : "\n"); } else { long v = atol(fl[2]) + dir * step; if (v < lo) v = lo; if (v > hi) v = hi; fprintf(o, "%-5s | %-14s | %-8ld%s", sec, kk, v, cm[0] ? cm : "\n"); } done = 1; }
        else fputs(line, o); }
    fclose(f); fclose(o); if (!done) { remove(tp); return 0; } return rename(tp, p) == 0; }

static void load_trades(void) {
    char p[PL], line[512]; path_of(p, "ledger/trades.txt"); FILE *f = fopen(p, "r"); nT = 0; fee_income = 0; if (!f) return;
    while (fgets(line, sizeof line, f) && nT < MAXT) { char *fl[12]; int n = split(line, fl, 12); if (n < 10 || strcmp(fl[0], "TRADE")) continue; Trade *t = &T[nT++]; t->n = atol(fl[1]); t->ts = atol(fl[2]); snprintf(t->actor, 48, "%s", fl[3]); snprintf(t->side, 8, "%s", fl[4]); snprintf(t->unit, 16, "%s", fl[5]); t->amount = atol(fl[6]); t->price = atol(fl[7]); t->value = atol(fl[8]); t->fee = atol(fl[9]); fee_income += t->fee; }
    fclose(f); }
static long clampbp(long v) { if (v < floor_bp) v = floor_bp; if (v > ceil_bp) v = ceil_bp; return v; }
static long vwap(int ui, int upto, int *cnt) { /* volume-weighted price of the last win_n trades of unit ui among T[0..upto) */
    double sv = 0, sa = 0; int c = 0; for (int i = upto - 1; i >= 0 && c < win_n; i--) if (!strcmp(T[i].unit, U[ui].id)) { sv += (double)T[i].price * T[i].amount; sa += T[i].amount; c++; } if (cnt) *cnt = c; if (sa <= 0) return U[ui].seed; return clampbp((long)(sv / sa + 0.5)); }
static void derive(void) { for (int u = 0; u < nU; u++) { int c = 0; U[u].avg = vwap(u, nT, &c); U[u].prev_avg = nT ? vwap(u, nT - 1, NULL) : U[u].seed; U[u].trades = 0; U[u].vol = 0; U[u].last = U[u].seed; for (int i = 0; i < nT; i++) if (!strcmp(T[i].unit, U[u].id)) { U[u].trades++; U[u].vol += T[i].amount; U[u].last = T[i].price; } if (!strcmp(U[u].id, "cones")) U[u].avg = 10000; } }
static int unit_index(const char *id) { for (int i = 0; i < nU; i++) if (!strcmp(U[i].id, id)) return i; return -1; }
static void append_line(const char *rel, const char *line) { char p[PL]; path_of(p, rel); FILE *f = fopen(p, "a"); if (f) { fputs(line, f); fclose(f); } }
static void do_trade(const char *arg) {
    char b[300]; snprintf(b, sizeof b, "%s", arg); char actor[48]; const char *u = getenv("USER"); snprintf(actor, 48, "%s", u && *u ? u : "human"); char *ap = strstr(b, "actor="); if (ap) { snprintf(actor, 48, "%.40s", ap + 6); *ap = 0; char *sp = strchr(actor, ' '); if (sp) *sp = 0; }
    char side[16] = "", unit[32] = ""; long amount = 0, price = 0; int n = sscanf(b, "%15s %31s %ld %ld", side, unit, &amount, &price);
    if (n < 3 || (strcmp(side, "buy") && strcmp(side, "sell"))) { snprintf(msg, sizeof msg, "usage: buy|sell <unit> <amount_mc> [price_bp]"); return; }
    int ui = unit_index(unit); if (ui < 0) { snprintf(msg, sizeof msg, "unknown unit %s", unit); return; } if (!strcmp(unit, "cones")) { snprintf(msg, sizeof msg, "cones are the preferred unit; trade another unit against them"); return; } if (amount <= 0) { snprintf(msg, sizeof msg, "amount must be above 0"); return; }
    load_trades(); derive(); long avg = U[ui].avg; if (n < 4 || price <= 0) price = avg; if (labs(price - avg) * 100 > avg * band) { snprintf(msg, sizeof msg, "refused: %ld bp is more than %d%% from the %ld bp average", price, band, avg); return; }
    long value = (long)((double)amount * price / 10000.0 + 0.5), fee = (long)ceil((double)value * fee_pct / 100.0); long nn = (nT ? T[nT-1].n : 0) + 1; char line[400]; time_t now = time(NULL);
    snprintf(line, sizeof line, "TRADE|%ld|%ld|%s|%s|%s|%ld|%ld|%ld|%ld\n", nn, (long)now, actor, side, unit, amount, price, value, fee); append_line("ledger/trades.txt", line);
    long before = avg; load_trades(); derive(); if (U[ui].avg != before) { snprintf(line, sizeof line, "RATE|%s|%ld|%d|%d|%ld\n", unit, U[ui].avg, win_n, U[ui].trades, (long)now); append_line("ledger/rates.txt", line); }
    snprintf(msg, sizeof msg, "%s %s %ld mc of %s @ %ld bp  (value %ld, fee %ld preferred)  paper ledger", actor, side, amount, unit, price, value, fee); }
static void fmt_bp(char *o, size_t n, long bp) { snprintf(o, n, "%ld.%04ld", bp / 10000, labs(bp % 10000)); }
static void write_ui(void) {
    char tmp[PL], dst[PL]; path_of(dst, "exchange_ui.txt"); snprintf(tmp, PL, "%s.tmp", dst); FILE *f = fopen(tmp, "w"); if (!f) return;
    const char *tabs[] = { "rates", "trade", "feed", "settings" }; for (int i = 0; i < 4; i++) { int a = !strcmp(cur_tab, tabs[i]); fprintf(f, "tab_%s=%s\ncls_%s=%s\n", tabs[i], a ? "1" : "", tabs[i], a ? "tab-active" : ""); }
    fprintf(f, "status=%s   (fee income %ld preferred mc)\n", msg[0] ? msg : "paper ledger - chain settlement comes with the lease layer", fee_income);
    fprintf(f, "title=Exchange  ·  fee %.0f%%  ·  %d trades\n", fee_pct, nT);
    char a[24], b[24], c[24]; int nr = 0; fprintf(f, "rates_hdr=%-34s %10s %10s %8s %9s\n", "unit", "avg (pref)", "last", "change", "volume mc");
    for (int u = 0; u < nU; u++) { fmt_bp(a, 24, U[u].avg); fmt_bp(b, 24, U[u].last); long ch = U[u].prev_avg ? (U[u].avg - U[u].prev_avg) * 10000 / U[u].prev_avg : 0; snprintf(c, 24, "%+ld.%02ld%%", ch / 100, labs(ch % 100));
        fprintf(f, "r_%d_text=%-34.34s %10s %10s %8s %9ld\n", nr++, U[u].label, a, b, !strcmp(U[u].id, "cones") ? "-" : c, U[u].vol); }
    fprintf(f, "n_r=%d\nrates_note=average = volume-weighted price of the last %d trades; band %d%%; floor %ld bp\n", nr, win_n, band, floor_bp);
    int nf = 0; for (int i = nT - 1; i >= 0 && nf < FEEDN; i--, nf++) { struct tm tm; time_t ts = T[i].ts; localtime_r(&ts, &tm); char hh[16]; strftime(hh, 16, "%H:%M:%S", &tm); char pb[24]; fmt_bp(pb, 24, T[i].price);
        fprintf(f, "f_%d_text=%s  %-10.10s %-4s %-6s %8ld mc @ %s  fee %ld\n", nf, hh, T[i].actor, T[i].side, T[i].unit, T[i].amount, pb, T[i].fee); }
    fprintf(f, "n_f=%d\nfeed_empty=%s\n", nf, nf ? "" : "No trades yet. Use the Trade tab: sell mined 2000");
    fprintf(f, "n_set=4\ns_0_label=Fee percent (paid in preferred)   %.0f\ns_0_key=fee_percent\ns_1_label=Averaging window (trades)   %d\ns_1_key=window_trades\ns_2_label=Price band (percent from average)   %d\ns_2_key=band_pct\n", fee_pct, win_n, band);
    long sm = 0; for (int u = 0; u < nU; u++) if (!strcmp(U[u].id, "mined")) sm = U[u].seed; fprintf(f, "s_3_label=Seed value of mined cones (bp)   %ld\ns_3_key=seed_mined\n", sm);
    fclose(f); rename(tmp, dst); }
static void refresh(void) { load_settings(); load_trades(); derive(); write_ui(); }
static void do_cmd(const char *cmd) {
    if (!strncmp(cmd, "TAB:", 4)) { snprintf(cur_tab, sizeof cur_tab, "%s", cmd + 4); msg[0] = 0; }
    else if (!strncmp(cmd, "TRADE:", 6)) do_trade(cmd + 6);
    else if (!strncmp(cmd, "SET:", 4)) { char k[48] = ""; char d[4] = ""; sscanf(cmd + 4, "%47s %3s", k, d); if (set_setting(k, d[0] == '-' ? -1 : 1)) snprintf(msg, sizeof msg, "saved %s", k); else snprintf(msg, sizeof msg, "could not save %s", k); }
    refresh(); }
static void poll(int *last) { char p[PL], buf[1500]; path_of(p, "exchange_action.txt"); FILE *f = fopen(p, "r"); if (!f) return; size_t nr = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[nr] = 0; int seq = 0; char cmd[1024] = "";
    for (char *ls = buf; *ls;) { char *le = strchr(ls, '\n'); size_t ll = le ? (size_t)(le - ls) : strlen(ls); if (!strncmp(ls, "seq=", 4)) seq = atoi(ls + 4); else if (!strncmp(ls, "cmd=", 4)) { size_t cl = ll - 4; if (cl >= sizeof cmd) cl = sizeof cmd - 1; memcpy(cmd, ls + 4, cl); cmd[cl] = 0; } if (!le) break; ls = le + 1; }
    if (seq > *last && cmd[0]) { *last = seq; do_cmd(cmd); } }
static void bye(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) { if (argc < 3) { fprintf(stderr, "Usage: %s <house_root> <package_dir>\n", argv[0]); return 1; }
    snprintf(house, PL, "%s", argv[1]); snprintf(pkg, PL, "%s", argv[2]); signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
    { char p[PL]; path_of(p, "ledger"); mkdir(p, 0755); path_of(p, "exchange_action.txt"); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } }
    refresh(); int last = 0, slow = 0; for (;;) { usleep(50000); poll(&last); if (++slow >= 100) { slow = 0; refresh(); } } }
