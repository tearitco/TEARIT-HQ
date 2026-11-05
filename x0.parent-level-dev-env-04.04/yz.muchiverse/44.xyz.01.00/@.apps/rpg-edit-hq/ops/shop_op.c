#define _GNU_SOURCE
/* shop_op - early-survivor step 6, the shop slice (16.game/SHOP-SLICE-SPEC-2026-11-04.md), QUARANTINED in the editor's own book (2026-11-04). Coins are ledger items; every sale is ONE atomic ledger `trade`; this op never edits a count and never mints.
 *   shop_op <book> list                         each priced item: buy, sell, the shop's stock
 *   shop_op <book> wallet <actor>               what the actor holds (coins and items), one line
 *   shop_op <book> buy  <actor> <item> <n>      actor pays n*buy coins, the shop hands over n items
 *   shop_op <book> sell <actor> <item> <n>      the shop pays n*sell coins, the actor hands over n items
 * There is NO verb that swaps one item id for another: cdda / df / mcl stay three ids; an exchange across ids is two trades through coins. Files: shop.pdl (COIN / SHOPKEEPER / PRICE rows; any sell >= buy is refused), state/shop_events.txt (log, capped 64 KB -> 32 KB).
 * Runs only in a folder that holds QUARANTINE, OR a folder whose RELEASE file names shop_op (the owner's one released caller, 2026-11-04; exit 4 otherwise). Exit 0 ok, 2 usage, 3 refused (reason on stdout, nothing written). Env SHOP_LEDGER_OP overrides the ledger op path. COST: one short process, 3-4 ledger processes per call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
static char BOOK[PATH_MAX], LOP[PATH_MAX], COIN[48] = "", KEEPER[48] = "shop";
#define MAXP 40
static struct { char item[48]; long buy, sell; } pr[MAXP]; static int np;
static void P(char *o, const char *rel) { snprintf(o, PATH_MAX, "%s/%s", BOOK, rel); }
static void sanit(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r' || *s == ' ') *s = '_'; }
static void logev(const char *what) { char p[PATH_MAX]; P(p, "state/shop_events.txt"); struct stat st; if (stat(p, &st) == 0 && st.st_size > 65536) { FILE *f = fopen(p, "r"); if (f) { static char b[32768]; fseek(f, -32768, SEEK_END); size_t n = fread(b, 1, sizeof b, f); fclose(f); char *nl = memchr(b, '\n', n); f = fopen(p, "w"); if (f) { if (nl) fwrite(nl + 1, 1, n - (size_t)(nl + 1 - b), f); fclose(f); } } } FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%ld|%s\n", (long)time(NULL), what); fclose(f); } }
static int ledger(char *out, size_t cap, const char *a, const char *b, const char *c, const char *d, const char *e, const char *f, const char *g, const char *h) {
    const char *av[14]; int n = 0; av[n++] = LOP; av[n++] = BOOK; const char *x[8] = { a, b, c, d, e, f, g, h }; for (int i = 0; i < 8 && x[i]; i++) av[n++] = x[i]; av[n] = NULL;
    int pf[2]; if (pipe(pf) != 0) return -1; pid_t k = fork(); if (k < 0) return -1; if (k == 0) { dup2(pf[1], 1); close(pf[0]); execv(LOP, (char *const *)av); _exit(127); }
    close(pf[1]); size_t got = 0; ssize_t r; while (got + 1 < cap && (r = read(pf[0], out + got, cap - 1 - got)) > 0) got += (size_t)r; out[got] = 0; close(pf[0]);
    while (got > 0 && (out[got - 1] == '\n' || out[got - 1] == '\r')) out[--got] = 0; int st = 0; waitpid(k, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static long held(const char *actor, const char *item) { char o[4096]; if (ledger(o, sizeof o, "balance", actor, NULL, NULL, NULL, NULL, NULL, NULL) != 0) return 0; long n = 0;
    for (char *sv = NULL, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char *a = strchr(l, '|'); if (!a) continue; *a++ = 0; char *b = strchr(a, '|'); if (!b) continue; *b++ = 0; if (!strcmp(l, actor) && !strcmp(a, item)) n = atol(b); } return n; }
static int load_shop(void) {
    char p[PATH_MAX], ln[200]; P(p, "shop.pdl"); FILE *f = fopen(p, "r"); if (!f) { printf("REFUSED: no shop.pdl\n"); return 3; } np = 0;
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0] || ln[0] == '#') continue; char *sv = NULL, *t[5] = {0}; int k = 0; for (char *x = strtok_r(ln, "|", &sv); x && k < 5; x = strtok_r(NULL, "|", &sv)) t[k++] = x; if (!k) continue;
        if (!strcmp(t[0], "COIN") && k >= 2) snprintf(COIN, sizeof COIN, "%s", t[1]); else if (!strcmp(t[0], "SHOPKEEPER") && k >= 2) { snprintf(KEEPER, sizeof KEEPER, "%s", t[1]); sanit(KEEPER); }
        else if (!strcmp(t[0], "PRICE") && k >= 4) { long b = atol(t[2]), s = atol(t[3]); if (b < 1 || s < 1 || b > 1000000 || s > 1000000) { printf("REFUSED: price row for %s must be integers 1..1000000\n", t[1]); fclose(f); return 3; } if (s >= b) { printf("REFUSED: %s sells at %ld but buys at %ld: sell must be below buy or the shop can be cycled for free coins\n", t[1], s, b); fclose(f); return 3; } if (np >= MAXP) { printf("REFUSED: too many price rows\n"); fclose(f); return 3; } snprintf(pr[np].item, 48, "%s", t[1]); pr[np].buy = b; pr[np].sell = s; np++; } }
    fclose(f); if (!COIN[0]) { printf("REFUSED: shop.pdl has no COIN row\n"); return 3; } if (!np) { printf("REFUSED: shop.pdl has no PRICE rows\n"); return 3; } return 0;
}
static int find(const char *item) { for (int i = 0; i < np; i++) if (!strcmp(pr[i].item, item)) return i; return -1; }
static int do_trade(int buying, const char *actor, const char *item, const char *ns) {
    char *end; long n = strtol(ns, &end, 10); if (*end || n < 1 || n > 100) { printf("REFUSED: n must be a whole number 1..100\n"); return 3; } int i = find(item); if (i < 0) { printf("REFUSED: %s is not in the shop's price list\n", item); return 3; }
    if (!strcmp(actor, KEEPER)) { printf("REFUSED: the shopkeeper cannot trade with itself\n"); return 3; }
    long coins = (buying ? pr[i].buy : pr[i].sell) * n; char cs[24], nstr[24]; snprintf(cs, sizeof cs, "%ld", coins); snprintf(nstr, sizeof nstr, "%ld", n);
    if (buying) { if (held(actor, COIN) < coins) { printf("REFUSED: %s has %ld coins, %ld x %s costs %ld\n", actor, held(actor, COIN), n, item, coins); return 3; } if (held(KEEPER, item) < n) { printf("REFUSED: the shop has %ld x %s, you asked for %ld\n", held(KEEPER, item), item, n); return 3; } }
    else { if (held(actor, item) < n) { printf("REFUSED: %s holds %ld x %s\n", actor, held(actor, item), item); return 3; } if (held(KEEPER, COIN) < coins) { printf("REFUSED: the shop cannot pay %ld coins (its till holds %ld)\n", coins, held(KEEPER, COIN)); return 3; } }
    char o[400]; int rc = buying ? ledger(o, sizeof o, "trade", actor, KEEPER, COIN, cs, item, nstr, NULL) : ledger(o, sizeof o, "trade", KEEPER, actor, COIN, cs, item, nstr, NULL);
    if (rc != 0) { printf("REFUSED: ledger: %s\n", o); return 3; }
    char d[200]; snprintf(d, sizeof d, "%s %s %ld x %s for %ld coins", actor, buying ? "bought" : "sold", n, item, coins); logev(d); printf("%s %ld x %s for %ld coins\n", buying ? "BOUGHT" : "SOLD", n, item, coins); return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: shop_op <book_dir> list | wallet <actor> | buy|sell <actor> <item> <n>\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/QUARANTINE", BOOK); { char rl[PATH_MAX], ln[80] = ""; snprintf(rl, sizeof rl, "%s/RELEASE", BOOK); FILE *rf = fopen(rl, "r"); int rel = 0; if (rf) { if (fgets(ln, sizeof ln, rf)) { ln[strcspn(ln, "\r\n")] = 0; rel = !strcmp(ln, "shop_op"); } fclose(rf); } if (access(q, F_OK) != 0 && !rel) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file (and no RELEASE naming shop_op)\n", BOOK); return 4; } }
    setenv("ITEM_LEDGER_CALLER", "shop_op", 1);
    const char *lo = getenv("SHOP_LEDGER_OP"); if (lo && lo[0]) snprintf(LOP, sizeof LOP, "%s", lo); else { char self[PATH_MAX]; ssize_t k = readlink("/proc/self/exe", self, sizeof self - 1); if (k < 0) return 2; self[k] = 0; char *sl = strrchr(self, '/'); if (sl) *sl = 0; snprintf(LOP, sizeof LOP, "%s/item_ledger_op.+x", self); }
    char sd[PATH_MAX]; P(sd, "state"); mkdir(sd, 0755); const char *v = argv[2];
    if (!strcmp(v, "list") && argc == 3) { int rc = load_shop(); if (rc) return rc; printf("SHOP %s, coin %s, till %ld\n", KEEPER, COIN, held(KEEPER, COIN)); for (int i = 0; i < np; i++) printf("%s buy %ld sell %ld stock %ld\n", pr[i].item, pr[i].buy, pr[i].sell, held(KEEPER, pr[i].item)); return 0; }
    if (!strcmp(v, "wallet") && argc == 4) { char actor[64]; snprintf(actor, sizeof actor, "%s", argv[3]); sanit(actor); char o[4096]; if (ledger(o, sizeof o, "balance", actor, NULL, NULL, NULL, NULL, NULL, NULL) != 0) { printf("REFUSED: ledger: %s\n", o); return 3; } char line[600] = ""; for (char *sv = NULL, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char *x = strchr(l, '|'); if (!x) continue; *x++ = 0; char *y = strchr(x, '|'); if (!y) continue; *y++ = 0; if (strcmp(l, actor)) continue; size_t n = strlen(line); snprintf(line + n, sizeof line - n, "%s%s %s", n ? ", " : "", x, y); } printf("WALLET %s: %s\n", actor, line[0] ? line : "empty"); return 0; }
    if ((!strcmp(v, "buy") || !strcmp(v, "sell")) && argc == 6) { int rc = load_shop(); if (rc) return rc; char actor[64]; snprintf(actor, sizeof actor, "%s", argv[3]); sanit(actor); return do_trade(!strcmp(v, "buy"), actor, argv[4], argv[5]); }
    fprintf(stderr, "usage: shop_op <book_dir> list | wallet <actor> | buy|sell <actor> <item> <n>\n"); return 2;
}
