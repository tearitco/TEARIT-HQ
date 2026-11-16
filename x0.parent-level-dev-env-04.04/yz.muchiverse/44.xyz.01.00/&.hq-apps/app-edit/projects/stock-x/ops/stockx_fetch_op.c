/* stockx_fetch_op - one quote from Yahoo (https, no key) onto the quote bus. Port of the old yahoo fetch_stock (which used http and now gets a 301).
 *   stockx_fetch_op quote <SYM> <busdir> [--now N] [--ttl S] [--force] [--file F]
 * Writes <busdir>/yahoo.ledger (append-only rows  Q|n|ts|SYM@yahoo|price_micro|prev_micro|ccy) and rewrites the projection <busdir>/latest.txt
 * (SYM@yahoo|price_micro|prev_micro|market_ts|ccy|fetched_at; nobody reads it back as input). Money is integer micro-units; the double never leaves this file.
 * Cache: a quote FETCHED less than --ttl ago (not its market time: a closed market is days old) (default 60 s) is reused (no request). Budget: <busdir>/yahoo.budget (one request time per line, trimmed to 1 h):
 * at most 300 requests an hour and a 1.5 s gap, else exit 4. --file F reads a canned JSON instead of the network (harness). Exit: 0 ok (stdout: SYM price_micro prev_micro ts ccy cached|fresh)
 * 2 usage/bad symbol, 3 fetch or parse failed, 4 budget. A plain GET: no cookies, no identity, generic user agent, 10 s, 256 KB. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
static long NOW;
static int fetch_url(const char *url, const char *out) {
    pid_t c = fork(); if (c < 0) return 0;
    if (c == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0600); if (fd >= 0) dup2(fd, 1);
        char *av[] = { (char *)"curl", (char *)"-fsS", (char *)"--max-time", (char *)"10", (char *)"--max-filesize", (char *)"262144", (char *)"-A", (char *)"stockx-fetch/1", (char *)"--proto", (char *)"=https", (char *)url, NULL }; execvp("curl", av); _exit(127); }
    int st; waitpid(c, &st, 0); return WIFEXITED(st) && WEXITSTATUS(st) == 0; }
static int jnum(const char *j, const char *key, double *v) { char k[64]; snprintf(k, sizeof k, "\"%s\":", key); const char *p = strstr(j, k); if (!p) return 0; p += strlen(k); char *e; double d = strtod(p, &e); if (e == p) return 0; *v = d; return 1; }
static int jstr(const char *j, const char *key, char *o, size_t n) { char k[64]; snprintf(k, sizeof k, "\"%s\":\"", key); const char *p = strstr(j, k); if (!p) return 0; p += strlen(k); size_t i = 0; while (*p && *p != '"' && i + 1 < n) o[i++] = *p++; o[i] = 0; return i > 0; }
static long long micro(double d) { return (long long)llround(d * 1e6); }
static int count_lines(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 0; int n = 0, c; while ((c = fgetc(f)) != EOF) if (c == '\n') n++; fclose(f); return n; }
int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "quote")) { fprintf(stderr, "usage: stockx_fetch_op quote <SYM> <busdir> [--now N] [--ttl S] [--force] [--file F]\n"); return 2; }
    const char *sym = argv[2], *bus = argv[3], *file = NULL; long ttl = 60; int force = 0; NOW = time(NULL);
    for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--now") && i + 1 < argc) NOW = atol(argv[++i]); else if (!strcmp(argv[i], "--ttl") && i + 1 < argc) ttl = atol(argv[++i]); else if (!strcmp(argv[i], "--force")) force = 1; else if (!strcmp(argv[i], "--file") && i + 1 < argc) file = argv[++i]; }
    size_t sl = strlen(sym); if (sl < 1 || sl > 12) { fprintf(stderr, "bad symbol\n"); return 2; }
    for (size_t i = 0; i < sl; i++) { char c = sym[i]; if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '^')) { fprintf(stderr, "bad symbol\n"); return 2; } }
    char lp[600], bp[600], tmp[600]; snprintf(lp, sizeof lp, "%s/latest.txt", bus); snprintf(bp, sizeof bp, "%s/yahoo.budget", bus);
    if (!force) { FILE *f = fopen(lp, "r"); char ln[256], key[40]; snprintf(key, sizeof key, "%s@yahoo|", sym); while (f && fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, strlen(key))) { long long p, q; long ts, fa = 0; char cc[16] = "?"; if (sscanf(ln + strlen(key), "%lld|%lld|%ld|%15[^|]|%ld", &p, &q, &ts, cc, &fa) >= 5 && NOW - fa < ttl && NOW >= fa) { printf("%s %lld %lld %ld %s cached\n", sym, p, q, ts, cc); fclose(f); return 0; } } if (f) fclose(f); }
    long hist[400]; int nh = 0; { FILE *f = fopen(bp, "r"); long t; while (f && fscanf(f, "%ld", &t) == 1) if (NOW - t < 3600 && nh < 399) hist[nh++] = t; if (f) fclose(f); }
    if (!file) { if (nh >= 300) { fprintf(stderr, "budget: %d requests in the last hour\n", nh); return 4; }
        if (nh > 0 && labs(NOW - hist[nh - 1]) < 2) usleep(1500000); }
    snprintf(tmp, sizeof tmp, "%s/.fetch.%d.json", bus, (int)getpid()); char url[200];
    if (file) snprintf(tmp, sizeof tmp, "%s", file); else { snprintf(url, sizeof url, "https://query2.finance.yahoo.com/v8/finance/chart/%s", sym); if (!fetch_url(url, tmp)) { unlink(tmp); fprintf(stderr, "fetch failed\n"); return 3; }
        hist[nh++] = NOW; FILE *b = fopen(bp, "w"); if (b) { for (int i = 0; i < nh; i++) fprintf(b, "%ld\n", hist[i]); fclose(b); } }
    static char js[262144]; size_t n = 0; { FILE *f = fopen(tmp, "r"); if (!f) { fprintf(stderr, "no response\n"); return 3; } n = fread(js, 1, sizeof js - 1, f); fclose(f); js[n] = 0; if (!file) unlink(tmp); }
    double pr, pv; char ccy[16] = "USD", rs[16] = ""; if (!jnum(js, "regularMarketPrice", &pr) || pr <= 0) { fprintf(stderr, "no price in response\n"); return 3; }
    if (!jnum(js, "chartPreviousClose", &pv)) pv = pr; jstr(js, "currency", ccy, sizeof ccy); (void)rs;
    double mt = 0; long ts = jnum(js, "regularMarketTime", &mt) && mt > 0 ? (long)mt : NOW;
    char lg[600]; snprintf(lg, sizeof lg, "%s/yahoo.ledger", bus); int rows = count_lines(lg);
    FILE *f = fopen(lg, "a"); if (!f) { fprintf(stderr, "cannot write ledger\n"); return 3; } fprintf(f, "Q|%d|%ld|%s@yahoo|%lld|%lld|%s\n", rows + 1, ts, sym, micro(pr), micro(pv), ccy); fclose(f);
    char keep[64][256]; int nk = 0; { FILE *g = fopen(lp, "r"); char ln[256], key[40]; snprintf(key, sizeof key, "%s@yahoo|", sym); while (g && fgets(ln, sizeof ln, g) && nk < 63) if (strncmp(ln, key, strlen(key))) strcpy(keep[nk++], ln); if (g) fclose(g); }
    snprintf(tmp, sizeof tmp, "%s/.latest.%d", bus, (int)getpid()); FILE *o = fopen(tmp, "w"); if (!o) return 3; for (int i = 0; i < nk; i++) fputs(keep[i], o); fprintf(o, "%s@yahoo|%lld|%lld|%ld|%s|%ld\n", sym, micro(pr), micro(pv), ts, ccy, NOW); fclose(o); rename(tmp, lp);
    printf("%s %lld %lld %ld %s fresh\n", sym, micro(pr), micro(pv), ts, ccy); return 0; }
