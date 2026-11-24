/* tear_chain_op - the C side of the tearit WEB chain (same rules, same bytes as &.hq-apps/tearit-web/docs/chain.js; graded against it by harness case tear_offline).
 * Block line (the pal-chain format): BLOCK|index|prev_hash|nonce|hash|timestamp|miner|tx_list ; hash = SHA-256("index|prev|nonce|tx_list"), N leading hex zeros.
 * Rows in tx_list: TX|from|to|amount|ts|id (balance checked)  MSG|from|channel|text|ts  FAUCET|wallet|amount|ts (<= faucetMax, faucetGap seconds apart per wallet).
 * Reward halves; supply capped. Fork rule: both copies verified, most work wins, a tie keeps the first. No signatures yet (owner 2026-11-24).
 *
 * Usage (every command prints ONE result line on stdout, exit 0 ok / 1 refused / 2 usage; add `--zeros N` anywhere, default 2):
 *   tear_chain_op verify   <file>                      OK height=H tip=<hash> work=W   |   BAD at=I why=<text>
 *   tear_chain_op balance  <file> <wallet>             <millicones>                     (exit 1 if the chain does not verify)
 *   tear_chain_op mine     <file> <miner> <ts> [row..] mines ONE block with the rows, verifies the result, appends it   -> MINED index=I tries=N
 *   tear_chain_op apply    <file> <miner> <ts> <rowsfile>  steward rule: each row tried alone, accepted rows in one block -> APPLIED accepted=A rejected=R mined=0|1
 *   tear_chain_op merge    <a> <b> <out>               fork rule, writes the winner to <out>            -> MERGE winner=a|b reason=<text>
 *   tear_chain_op messages <file> <channel|*>          one `from<TAB>text` line per message (verified chains only)
 *   tear_chain_op tail     <file> <n>                  the last n blocks as `#index hash10 miner rows`
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_chain_op.+x tear_chain_op.c -lcrypto */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <openssl/sha.h>

#define MAXW 512
#define NAMEL 64
typedef struct { int zeros; long initial, halving, supply, fmax, fgap; } Cfg;
static Cfg C = { 2, 10500, 1000, 21000000, 1000, 3600 };
typedef struct { long index; char prev[72], hash[72], nonce[40], miner[NAMEL]; long ts; char *txl; } Blk;
typedef struct { char name[NAMEL]; long bal, last; int has_last; } W;
typedef struct { W w[MAXW]; int n; } Bal;
typedef struct { int ok; long height; double work; char tip[72]; char why[160]; long at; Bal bal; char **mf, **mc, **mt; long *mts; int nm; } Ver;

static void sha_hex(const char *s, char out[65]) { unsigned char d[32]; SHA256((const unsigned char *)s, strlen(s), d); for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]); out[64] = 0; }
static const char ZERO[] = "0000000000000000000000000000000000000000000000000000000000000000";
static W *wal(Bal *b, const char *n) { for (int i = 0; i < b->n; i++) if (!strcmp(b->w[i].name, n)) return &b->w[i]; if (b->n >= MAXW) return NULL; W *w = &b->w[b->n++]; memset(w, 0, sizeof *w); snprintf(w->name, NAMEL, "%s", n); return w; }
static long reward(long i) { long e = i / C.halving; if (e > 40) return 0; return (long)floor((double)C.initial / pow(2, (double)e)); }
static int meets(const char *h, int z) { for (int i = 0; i < z; i++) if (h[i] != '0') return 0; return 1; }

static char *slurp(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) { *n = 0; return NULL; } fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc((size_t)l + 1); size_t r = fread(b, 1, (size_t)l, f); b[r] = 0; fclose(f); *n = r; return b; }

/* parse text into blocks; returns count, or -1 with err set */
static long parse(char *text, Blk **out, char *err, size_t es) {
    long cap = 64, n = 0; Blk *bl = malloc(sizeof(Blk) * (size_t)cap); long ln = 0;
    for (char *p = text; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; ln++;
        if (*p) { char *f[8]; int k = 0; char *q = p; while (k < 7) { f[k++] = q; char *bar = strchr(q, '|'); if (!bar) break; *bar = 0; q = bar + 1; }
            if (k < 7 || strcmp(f[0], "BLOCK")) { snprintf(err, es, "line %ld is not a BLOCK row", ln); free(bl); return -1; }
            if (n == cap) { cap *= 2; bl = realloc(bl, sizeof(Blk) * (size_t)cap); }
            Blk *b = &bl[n++]; b->index = atol(f[1]); snprintf(b->prev, 72, "%s", f[2]); snprintf(b->nonce, 40, "%s", f[3]); snprintf(b->hash, 72, "%s", f[4]); b->ts = atol(f[5]); snprintf(b->miner, NAMEL, "%s", f[6]); b->txl = q; }
        if (!e) { break; }
        p = e + 1; }
    *out = bl; return n; }

/* split tx_list into rows (copy into rows[], each malloc'd); matches TX| MSG| FAUCET| anywhere, like the JS regex */
static int split_rows(const char *t, char ***rows) {
    int cap = 8, n = 0; char **r = malloc(sizeof(char *) * (size_t)cap); long pos[512]; int np = 0; size_t L = strlen(t);
    for (size_t i = 0; i < L && np < 512; i++) if ((!strncmp(t + i, "TX|", 3)) || (!strncmp(t + i, "MSG|", 4)) || (!strncmp(t + i, "FAUCET|", 7))) pos[np++] = (long)i;
    for (int i = 0; i < np; i++) { long s = pos[i], e = i + 1 < np ? pos[i + 1] : (long)L; while (e > s && (t[e - 1] == ';' || t[e - 1] == ',' || isspace((unsigned char)t[e - 1]))) e--;
        if (n == cap) { cap *= 2; r = realloc(r, sizeof(char *) * (size_t)cap); } r[n] = strndup(t + s, (size_t)(e - s)); n++; }
    *rows = r; return n; }

static int fields(char *s, char **f, int max) { int k = 0; f[k++] = s; for (char *p = s; *p && k < max; p++) if (*p == '|') { *p = 0; f[k++] = p + 1; } return k; }
static void fail(Ver *v, long at, const char *why) { v->ok = 0; v->at = at; snprintf(v->why, sizeof v->why, "%s", why); }

/* ranch rows (PET FEED TRAIN REST COMPETE UPGRADE FREEZE THAW TRANSFER) are NOT ported to C yet: refuse such a block loudly (silently skipping them would compute wrong balances) */
static int has_ranch_row(const char *t) { static const char *V[] = { "PET|", "FEED|", "TRAIN|", "REST|", "COMPETE|", "UPGRADE|", "FREEZE|", "THAW|", "TRANSFER|" }; for (int k = 0; k < 9; k++) { size_t l = strlen(V[k]); for (const char *p = t; (p = strstr(p, V[k])); p++) { if (p == t || p[-1] == ';') { return 1; } } (void)l; } return 0; }
static void verify(char *text, Ver *v) {
    memset(v, 0, sizeof *v); v->ok = 1; snprintf(v->tip, 72, "%s", ZERO);
    char err[160]; Blk *bl; long n = parse(text, &bl, err, sizeof err); if (n < 0) { fail(v, 0, err); return; }
    long minted = 0; char pre[8192]; v->mf = calloc(256, sizeof(char *)); v->mc = calloc(256, sizeof(char *)); v->mt = calloc(256, sizeof(char *)); v->mts = calloc(256, sizeof(long));
    for (long i = 0; i < n && v->ok; i++) { Blk *b = &bl[i]; v->height = i;
        if (b->index != i) { snprintf(err, sizeof err, "index %ld expected %ld", b->index, i); fail(v, i, err); break; }
        if (strcmp(b->prev, i ? bl[i - 1].hash : ZERO)) { fail(v, i, "prev_hash does not match the tip"); break; }
        snprintf(pre, sizeof pre, "%ld|%s|%s|%s", i, b->prev, b->nonce, b->txl); char h[65]; sha_hex(pre, h);
        if (strcmp(h, b->hash)) { fail(v, i, "hash does not recompute"); break; }
        if (!meets(b->hash, C.zeros)) { fail(v, i, "difficulty not met"); break; }
        if (has_ranch_row(b->txl)) { fail(v, i, "ranch rows are not supported by this build yet (web chain only)"); break; }
        char **rows; int nr = split_rows(b->txl, &rows);
        for (int j = 0; j < nr && v->ok; j++) { char tmp[4096]; snprintf(tmp, sizeof tmp, "%s", rows[j]); char *f[8]; int k = fields(tmp, f, 8);
            if (!strcmp(f[0], "TX")) { char *e; double amt = k > 3 ? strtod(f[3], &e) : 0; if (k < 4 || !(amt > 0) || floor(amt) != amt) { fail(v, i, "bad amount"); break; }
                W *a = wal(&v->bal, f[1]); if (!a || a->bal < (long)amt) { snprintf(err, sizeof err, "insufficient balance for %s", f[1]); fail(v, i, err); break; } a->bal -= (long)amt; W *c = wal(&v->bal, f[2]); if (c) c->bal += (long)amt; }
            else if (!strcmp(f[0], "FAUCET")) { double fa = k > 2 ? atof(f[2]) : 0; long ft = k > 3 ? atol(f[3]) : 0; if (!(fa > 0) || fa > (double)C.fmax) { fail(v, i, "faucet amount over the cap"); break; }
                W *w = wal(&v->bal, f[1]); if (!w) break; if (w->has_last && ft - w->last < C.fgap) { fail(v, i, "faucet cooldown"); break; } w->last = ft; w->has_last = 1; w->bal += (long)fa; }
            else if (!strcmp(f[0], "MSG")) { if (k != 5) { fail(v, i, "bad MSG row"); break; } if (v->nm < 256) { v->mf[v->nm] = strdup(f[1]); v->mc[v->nm] = strdup(f[2]); v->mt[v->nm] = strdup(f[3]); v->mts[v->nm] = atol(f[4]); v->nm++; } } }
        for (int j = 0; j < nr; j++) { free(rows[j]); }
        free(rows); if (!v->ok) break;
        long r = reward(i); if (minted + r > C.supply) r = C.supply - minted; minted += r; W *m = wal(&v->bal, b->miner); if (m) m->bal += r; v->work += pow(16, C.zeros); }
    if (v->ok) { v->height = n; if (n) snprintf(v->tip, 72, "%s", bl[n - 1].hash); }
}

static int do_mine(const char *file, const char *miner, long ts, char **rows, int nr, long *idx_out, long *tries_out) {
    size_t n; char *t = slurp(file, &n); if (!t) { t = strdup(""); } char *copy = strdup(t); Blk *bl; char err[160]; long nb = parse(copy, &bl, err, sizeof err); if (nb < 0) return 1;
    char prev[72]; snprintf(prev, 72, "%s", nb ? bl[nb - 1].hash : ZERO); char txl[8192] = ""; for (int i = 0; i < nr; i++) { if (i) strcat(txl, ";"); strncat(txl, rows[i], sizeof txl - strlen(txl) - 1); }
    char pre[9000], h[65]; long nonce; for (nonce = 0; nonce < 4000000; nonce++) { snprintf(pre, sizeof pre, "%ld|%s|%ld|%s", nb, prev, nonce, txl); sha_hex(pre, h); if (meets(h, C.zeros)) break; }
    if (nonce >= 4000000) return 1;
    char line[10000]; snprintf(line, sizeof line, "BLOCK|%ld|%s|%ld|%s|%ld|%s|%s\n", nb, prev, nonce, h, ts, miner, txl);
    char *nt = malloc(strlen(t) + strlen(line) + 1); strcpy(nt, t); strcat(nt, line); Ver v; verify(nt, &v); if (!v.ok) { snprintf(err, sizeof err, "%s", v.why); fprintf(stderr, "refused: %s\n", err); return 1; }
    FILE *f = fopen(file, "a"); if (!f) return 1; fputs(line, f); fclose(f); *idx_out = nb; *tries_out = nonce + 1; return 0; }

static int ok_id(const char *s) { size_t l = strlen(s); if (!l || l >= NAMEL) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_' || *s == '-')) return 0; return 1; }

int main(int argc, char **argv) {
    char *av[64]; int ac = 0; for (int i = 1; i < argc && ac < 64; i++) { if (!strcmp(argv[i], "--zeros") && i + 1 < argc) { C.zeros = atoi(argv[++i]); continue; } av[ac++] = argv[i]; }
    if (ac < 2) { fprintf(stderr, "usage: tear_chain_op verify|balance|mine|apply|merge|messages|tail <file> ...\n"); return 2; }
    const char *cmd = av[0], *file = av[1]; size_t n; Ver v;
    if (!strcmp(cmd, "verify") || !strcmp(cmd, "balance") || !strcmp(cmd, "messages") || !strcmp(cmd, "tail")) {
        char *t = slurp(file, &n); if (!t) t = strdup(""); char *keep = strdup(t); verify(t, &v);
        if (!strcmp(cmd, "verify")) { if (v.ok) { printf("OK height=%ld tip=%s work=%.0f\n", v.height, v.tip, v.work); return 0; } printf("BAD at=%ld why=%s\n", v.at, v.why); return 1; }
        if (!v.ok) { printf("BAD at=%ld why=%s\n", v.at, v.why); return 1; }
        if (!strcmp(cmd, "balance")) { if (ac < 3) return 2; W *w = wal(&v.bal, av[2]); printf("%ld\n", w ? w->bal : 0); return 0; }
        if (!strcmp(cmd, "messages")) { if (ac < 3) return 2; for (int i = 0; i < v.nm; i++) if (!strcmp(av[2], "*") || !strcmp(av[2], v.mc[i])) printf("%s\t%s\n", v.mf[i], v.mt[i]); return 0; }
        long want = ac > 2 ? atol(av[2]) : 10; Blk *bl; char err[160]; long nb = parse(keep, &bl, err, sizeof err); for (long i = nb > want ? nb - want : 0; i < nb; i++) printf("#%ld %.10s %s %.80s\n", bl[i].index, bl[i].hash, bl[i].miner, bl[i].txl); return 0; }
    if (!strcmp(cmd, "mine")) { if (ac < 4 || !ok_id(av[2])) return 2; long idx, tries; if (do_mine(file, av[2], atol(av[3]), av + 4, ac - 4, &idx, &tries)) { printf("REFUSED\n"); return 1; } printf("MINED index=%ld tries=%ld\n", idx, tries); return 0; }
    if (!strcmp(cmd, "apply")) { if (ac < 5 || !ok_id(av[2])) return 2; size_t rn; char *rt = slurp(av[4], &rn); if (!rt) return 2; char *good[20]; int ng = 0, nbad = 0; long ts = atol(av[3]), idx, tries;
        char *tmpf = strdup("/tmp/tear_chain_apply.XXXXXX"); int fd = mkstemp(tmpf); if (fd < 0) return 2; close(fd);
        for (char *p = rt; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; while (isspace((unsigned char)*p)) p++; size_t l = strlen(p); while (l && isspace((unsigned char)p[l - 1])) p[--l] = 0;
            if (ng + nbad < 20 && l) { int okrow = (!strncmp(p, "TX|", 3) || !strncmp(p, "MSG|", 4) || !strncmp(p, "FAUCET|", 7)) && l <= 400 && !strchr(p, ';');
                if (!okrow) nbad++; else { size_t cn; char *cur = slurp(file, &cn); FILE *f = fopen(tmpf, "w"); if (cur) fputs(cur, f); fclose(f); char *rows[21]; for (int i = 0; i < ng; i++) rows[i] = good[i]; rows[ng] = p;
                    if (do_mine(tmpf, av[2], ts, rows, ng + 1, &idx, &tries)) nbad++; else good[ng++] = p; } }
            if (!e) { break; }
        p = e + 1; }
        remove(tmpf); int mined = 0; if (ng) { if (!do_mine(file, av[2], ts, good, ng, &idx, &tries)) mined = 1; }
        printf("APPLIED accepted=%d rejected=%d mined=%d\n", ng, nbad, mined); return 0; }
    if (!strcmp(cmd, "merge")) { if (ac < 4) return 2; size_t na, nb2; char *a = slurp(file, &na); if (!a) a = strdup(""); char *b = slurp(av[2], &nb2); if (!b) b = strdup(""); char *ak = strdup(a), *bk = strdup(b); Ver va, vb; verify(a, &va); verify(b, &vb);
        const char *win, *why; if (!va.ok && !vb.ok) { printf("REFUSED neither copy verifies\n"); return 1; } if (!vb.ok) { win = "a"; why = "b rejected"; } else if (!va.ok) { win = "b"; why = "a rejected"; } else if (vb.work > va.work) { win = "b"; why = "more work"; } else { win = "a"; why = va.work == vb.work ? "tie keeps a" : "more work"; }
        FILE *f = fopen(av[3], "w"); if (!f) return 1; fputs(*win == 'a' ? ak : bk, f); fclose(f); printf("MERGE winner=%s reason=%s\n", win, why); return 0; }
    fprintf(stderr, "unknown command %s\n", cmd); return 2; }
