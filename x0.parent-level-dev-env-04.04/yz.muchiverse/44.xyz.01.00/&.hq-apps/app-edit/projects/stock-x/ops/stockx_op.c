/* stockx_op - the stock+x paper-broker engine (design: BOOK 16.game/STOCK-X-DESIGN-2026-11-15.md). STATE IS THE LEDGER ONLY (no ghost data): <dir>/ledger.txt, one row per action, every row ends with its RESULT:
 *   n|START|cash_cents|res        n|CREDIT|cents|res        n|BUY|SYM@source|qty_micro|price_micro|cents|res        n|SELL|SYM@source|qty_micro|price_micro|cents|res
 * res = cash=<cents>;pos=<SYM@source>:<micro-shares> (a semicolon: the row is split on bars) (what the engine computes after the row). `state` and `replay` REPLAY every row through the SAME interpreter and REFUSE the ledger (exit 3) when any row's
 * numbers or result differ (a hand-edited ledger). MONEY IS INTEGER: cash in cents, shares in micro-shares (1e-6, so fractional shares are exact), prices in micro-dollars; cents = round-half-up(qty * price / 1e10);
 * no float touches money. The fill price comes from the quote bus (<bus>/latest.txt, written by stockx_fetch_op) and must be younger than 300 s (exit 4 when stale or missing); a replay never fetches.
 *   stockx_op <dir> new [cash_cents]                 start a ledger (default 100000 = $1,000.00); refuses to overwrite
 *   stockx_op <dir> credit <cents>                   add cash (the old add_credit)
 *   stockx_op <dir> buy|sell <SYM> <qty> [--bus D] [--source S] [--now N]     qty is decimal text with up to 6 decimals ("2.5"); SYM is uppercase letters, digits . - ^
 *   stockx_op <dir> state [--bus D]                  verify the ledger, print cash, holdings (valued at the bus quotes when present) and the total
 *   stockx_op <dir> replay                           verify only
 * Exit: 0 ok, 1 refused (no cash, no shares, too small, over), 2 usage or bad input, 3 the ledger does not replay, 4 no fresh quote. Footprint: no resident process; one short run per verb. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#define P 4096
#define NPOS 64
static char LEDGER[P], DIR_[P]; static long long CASH; static struct { char sym[40]; long long q; } POS[NPOS]; static int NP;
static long long cents_of(long long q, long long p) { __int128 v = (__int128)q * p; return (long long)((v + 5000000000LL) / 10000000000LL); }
static int sym_ok(const char *s) { size_t l = strlen(s); if (l < 3 || l > 30) return 0; const char *at = strchr(s, '@'); if (!at || at == s || !at[1]) return 0; for (const char *c = s; *c; c++) if (!(isupper((unsigned char)*c) || isdigit((unsigned char)*c) || *c == '.' || *c == '-' || *c == '^' || *c == '@' || (c > at && islower((unsigned char)*c)))) return 0; return 1; }
static int pos_i(const char *sym, int make) { for (int i = 0; i < NP; i++) if (!strcmp(POS[i].sym, sym)) return i; if (!make || NP >= NPOS) return -1; snprintf(POS[NP].sym, sizeof POS[NP].sym, "%s", sym); POS[NP].q = 0; return NP++; }
static void result(char *res, size_t cap, const char *sym) { int i = sym ? pos_i(sym, 0) : -1; if (i >= 0) snprintf(res, cap, "cash=%lld;pos=%s:%lld", CASH, sym, POS[i].q); else snprintf(res, cap, "cash=%lld", CASH); }
/* THE row interpreter (live and replay): applies one row to CASH/POS; returns 0 ok, 1 refused; *cents = the money of a BUY/SELL (computed here), res = the result text */
static int exec_row(const char *kind, const char *sym, long long a, long long b, long long *cents, char *res, size_t cap) {
    if (!strcmp(kind, "START")) { if (a < 0) return 1; CASH = a; NP = 0; result(res, cap, NULL); return 0; }
    if (!strcmp(kind, "CREDIT")) { if (a <= 0 || CASH + a < CASH) return 1; CASH += a; result(res, cap, NULL); return 0; }
    int buy = !strcmp(kind, "BUY"); if (!buy && strcmp(kind, "SELL")) return 1; if (!sym_ok(sym) || a <= 0 || b <= 0) return 1;
    long long c = cents_of(a, b); if (c <= 0) return 1; *cents = c; int i = pos_i(sym, buy); if (i < 0) return 1;
    if (buy) { if (c > CASH) return 1; CASH -= c; POS[i].q += a; } else { if (a > POS[i].q) return 1; CASH += c; POS[i].q -= a; }
    result(res, cap, sym); return 0;
}
static int split(char *ln, char **f, int max) { int n = 0; char *p = ln; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } return n; }
/* replay: 0 ok, 2 no ledger, 3 a row is wrong; fills CASH/POS */
static int replay(int quiet) {
    FILE *fp = fopen(LEDGER, "r"); if (!fp) { if (!quiet) fprintf(stderr, "stockx_op: no ledger %s\n", LEDGER); return 2; } char ln[1024]; int seq = 0, rc = 0; CASH = 0; NP = 0;
    while (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; seq++; char *f[10]; int nf = split(ln, f, 10); int ok = nf >= 4 && atoi(f[0]) == seq; char res[200] = "", exp_[200] = ""; long long cents = 0;
        if (ok) { const char *k = f[1]; if (!strcmp(k, "START") || !strcmp(k, "CREDIT")) { ok = nf == 4 && seq == (strcmp(k, "START") ? seq : 1) && !exec_row(k, NULL, atoll(f[2]), 0, &cents, res, sizeof res); snprintf(exp_, sizeof exp_, "%s", nf > 3 ? f[3] : ""); }
            else if (!strcmp(k, "BUY") || !strcmp(k, "SELL")) { ok = nf == 7 && !exec_row(k, f[2], atoll(f[3]), atoll(f[4]), &cents, res, sizeof res) && cents == atoll(f[5]); snprintf(exp_, sizeof exp_, "%s", nf > 6 ? f[6] : ""); } else { ok = 0; }
            if (ok && strcmp(res, exp_)) ok = 0; }
        if (!ok) { rc = 3; if (!quiet) fprintf(stderr, "stockx_op: ledger row %d does not replay\n", seq); break; } }
    fclose(fp); if (!rc && seq == 0) rc = 3; return rc;
}
static int append(const char *row) { FILE *fp = fopen(LEDGER, "a"); if (!fp) return 2; fprintf(fp, "%s\n", row); fclose(fp); return 0; }
static int rows(void) { FILE *fp = fopen(LEDGER, "r"); int n = 0, c; while (fp && (c = fgetc(fp)) != EOF) if (c == '\n') n++; if (fp) fclose(fp); return n; }
static int parse_qty(const char *s, long long *out) { long long ip = 0, fp = 0; int nd = 0, seen = 0; const char *c = s; for (; isdigit((unsigned char)*c); c++) { ip = ip * 10 + (*c - '0'); seen = 1; if (ip > 1000000000LL) return 0; }
    if (*c == '.') { c++; for (; isdigit((unsigned char)*c); c++) { if (nd >= 6) return 0; fp = fp * 10 + (*c - '0'); nd++; seen = 1; } } if (*c || !seen) return 0; while (nd < 6) { fp *= 10; nd++; } *out = ip * 1000000LL + fp; return *out > 0; }
/* the quote for SYM@source from the bus: latest.txt rows  SYM@source|price_micro|prev_micro|market_ts|ccy|fetched_at ; 1 found (and fresh enough if maxage>0), 0 not */
static int quote(const char *bus, const char *sym, long long now, long maxage, long long *price, long long *prev) {
    char f[P], ln[300], key[64]; snprintf(f, sizeof f, "%s/latest.txt", bus); snprintf(key, sizeof key, "%s|", sym); FILE *fp = fopen(f, "r"); int ok = 0;
    while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, key, strlen(key))) { long long p = 0, q = 0, ts = 0, fa = 0; char cc[16]; if (sscanf(ln + strlen(key), "%lld|%lld|%lld|%15[^|]|%lld", &p, &q, &ts, cc, &fa) >= 5 && p > 0) { if (maxage > 0 && (now - fa > maxage || now < fa)) ok = -1; else { *price = p; *prev = q; ok = 1; } } }
    if (fp) fclose(fp); return ok > 0; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: stockx_op <dir> new|credit|buy|sell|state|replay ...\n"); return 2; } snprintf(DIR_, sizeof DIR_, "%s", argv[1]); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", DIR_); const char *v = argv[2]; const char *bus = NULL, *src = "yahoo"; long long now = time(NULL);
    for (int i = 3; i + 1 < argc; i++) { if (!strcmp(argv[i], "--bus")) bus = argv[i + 1]; else if (!strcmp(argv[i], "--source")) src = argv[i + 1]; else if (!strcmp(argv[i], "--now")) now = atoll(argv[i + 1]); }
    if (!strcmp(v, "new")) { if (!access(LEDGER, F_OK)) { fprintf(stderr, "stockx_op: a ledger already exists\n"); return 1; } long long cash = argc >= 4 && isdigit((unsigned char)argv[3][0]) ? atoll(argv[3]) : 100000; mkdir(DIR_, 0755); char res[100], row[200]; long long c = 0; CASH = 0; NP = 0; if (exec_row("START", NULL, cash, 0, &c, res, sizeof res)) return 2;
        snprintf(row, sizeof row, "1|START|%lld|%s", cash, res); if (append(row)) return 2; printf("%s\n", res); return 0; }
    int rc = replay(0); if (rc) return rc; if (!strcmp(v, "replay")) { printf("REPLAY-OK rows=%d cash=%lld\n", rows(), CASH); return 0; }
    if (!strcmp(v, "credit")) { if (argc < 4) return 2; char *e; long long a = strtoll(argv[3], &e, 10); if (*e || a <= 0) return 2; char res[200], row[300]; long long c; if (exec_row("CREDIT", NULL, a, 0, &c, res, sizeof res)) return 1; snprintf(row, sizeof row, "%d|CREDIT|%lld|%s", rows() + 1, a, res); if (append(row)) return 2; printf("%s\n", res); return 0; }
    if (!strcmp(v, "buy") || !strcmp(v, "sell")) { if (argc < 5 || !bus) { fprintf(stderr, "stockx_op: %s <SYM> <qty> --bus <dir>\n", v); return 2; } char sym[64]; snprintf(sym, sizeof sym, "%s@%s", argv[3], src); long long q, p, pv; if (!sym_ok(sym) || !parse_qty(argv[4], &q)) { fprintf(stderr, "stockx_op: bad symbol or quantity\n"); return 2; }
        if (!quote(bus, sym, now, 300, &p, &pv)) { fprintf(stderr, "stockx_op: no fresh quote for %s (older than 300 s or missing)\n", sym); return 4; }
        char res[200], row[400]; long long c = 0; if (exec_row(!strcmp(v, "buy") ? "BUY" : "SELL", sym, q, p, &c, res, sizeof res)) { fprintf(stderr, "stockx_op: refused (%s)\n", !strcmp(v, "buy") ? "not enough cash, or too small" : "not enough shares, or too small"); return 1; }
        snprintf(row, sizeof row, "%d|%s|%s|%lld|%lld|%lld|%s", rows() + 1, !strcmp(v, "buy") ? "BUY" : "SELL", sym, q, p, c, res); if (append(row)) return 2; printf("%s %s qty_micro=%lld price_micro=%lld cents=%lld %s\n", v, sym, q, p, c, res); return 0; }
    if (!strcmp(v, "state")) { long long total = CASH; printf("cash_cents=%lld\n", CASH); for (int i = 0; i < NP; i++) { if (POS[i].q <= 0) continue; long long p = 0, pv = 0, val = 0; int have = bus && quote(bus, POS[i].sym, now, 0, &p, &pv); if (have) { val = cents_of(POS[i].q, p); total += val; } printf("pos %s qty_micro=%lld price_micro=%lld value_cents=%lld\n", POS[i].sym, POS[i].q, have ? p : 0LL, val); } printf("total_cents=%lld\nrows=%d\n", total, rows()); return 0; }
    fprintf(stderr, "stockx_op: unknown verb %s\n", v); return 2;
}
