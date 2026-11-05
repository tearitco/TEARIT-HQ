#define _GNU_SOURCE
/* item_ledger_op - the item ledger with a conservation check (2026-11-04). QUARANTINED: it only runs inside a book folder that holds a file named QUARANTINE (exit 4 otherwise), so nothing in a live game book
 * can call it by accident. Built for the editor first (book folder = the editor's own `books/ledger-lab/`); the early-survivor slice (smash, craft, shop) and the pet seller are its callers later.
 *   item_ledger_op <book_dir> define <id> <name>                         id = <game>.<name> e.g. cdda.stone, df.stone, mcl.cobble: THREE ids stay three (owner 2026-11-04)
 *   item_ledger_op <book_dir> mint <to> <item> <n> <source>              the ONLY way an item appears; <source> must be in sources.pdl (default: smash break mine craft spawn gift)
 *   item_ledger_op <book_dir> burn <from> <item> <n> <reason>            the only way one disappears
 *   item_ledger_op <book_dir> move <from> <to> <item> <n>                refused if <from> holds fewer than <n>
 *   item_ledger_op <book_dir> trade <a> <b> <itemA> <nA> <itemB> <nB>    a gives nA of itemA, b gives nB of itemB; both legs are checked first, then written together or not at all
 *   item_ledger_op <book_dir> balance [actor] | total <item> | check | compact
 * Files in <book_dir>: items.pdl (ITEM rows), sources.pdl (optional), state/ledger.txt (append-only: seq|ts|VERB|from|to|item|n|note|hash). The hash is a chain: FNV-1a of the previous hash plus this row's text, so editing ANY earlier
 * row makes `check` fail. `check` replays the whole ledger: contiguous seq, valid chain, defined items, positive counts, allowed mint sources, no balance below zero, and per item id: sum of balances = minted - burned (+ snapshot baseline).
 * Growth cap (RUNTIME-FILE-GROWTH-STANDARD): past 256 KB the ledger is rewritten as SNAP rows (one per non-zero balance, a new chain); `compact` does it on demand. COST: one short-lived process per call, reads the ledger once; nothing runs between calls.
 * Exit: 0 ok, 1 check failed, 2 usage, 3 refused (reason on stdout), 4 not a quarantined book. Env ITEM_LEDGER_TS fixes the timestamp (tests). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>

#define CAP_BYTES (256 * 1024)
#define MAXB 1024
#define MAXI 128
static char BOOK[PATH_MAX], LEDGER[PATH_MAX], ITEMS[PATH_MAX];
typedef struct { char who[64]; char item[48]; long n; } Bal;
static Bal bal[MAXB]; static int nbal;
static char items[MAXI][48]; static int nitems; static long minted[MAXI], burned[MAXI], base[MAXI];
static unsigned prevh = 2166136261u; static long lastseq = 0;
static char srcs[16][24] = { "smash", "break", "mine", "craft", "spawn", "gift" }; static int nsrc = 6;

static void clean(char *s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') *s = '_'; }
static unsigned fnv(unsigned h, const char *s) { for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; } return h; }
static int item_ix(const char *id) { for (int i = 0; i < nitems; i++) if (!strcmp(items[i], id)) return i; return -1; }
static int valid_id(const char *id) { const char *d = strchr(id, '.'); if (!d || d == id || !d[1]) return 0; for (const char *p = id; *p; p++) if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_' || p == d)) return 0; return strlen(id) < 47; }
static Bal *getb(const char *who, const char *item, int create) { for (int i = 0; i < nbal; i++) if (!strcmp(bal[i].who, who) && !strcmp(bal[i].item, item)) return &bal[i]; if (!create || nbal >= MAXB) return NULL; snprintf(bal[nbal].who, 64, "%s", who); snprintf(bal[nbal].item, 48, "%s", item); bal[nbal].n = 0; return &bal[nbal++]; }
static void load_items(void) { nitems = 0; FILE *f = fopen(ITEMS, "r"); if (!f) return; char ln[256]; while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "ITEM", 4)) continue; char *a = strchr(ln, '|'); if (!a) continue; a++; while (*a == ' ') a++; char *b = strchr(a, '|'); if (b) *b = 0; char *e = a + strlen(a); while (e > a && (e[-1] == ' ' || e[-1] == '\n')) *--e = 0; if (nitems < MAXI && valid_id(a)) snprintf(items[nitems++], 48, "%s", a); } fclose(f); }
static void load_sources(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/sources.pdl", BOOK); FILE *f = fopen(p, "r"); if (!f) return; nsrc = 0; char ln[128]; while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "SOURCE", 6)) continue; char *a = strchr(ln, '|'); if (!a) continue; a++; while (*a == ' ') a++; a[strcspn(a, " \n|")] = 0; if (a[0] && nsrc < 16) snprintf(srcs[nsrc++], 24, "%s", a); } fclose(f); }
static int src_ok(const char *s) { for (int i = 0; i < nsrc; i++) if (!strcmp(srcs[i], s)) return 1; return 0; }

/* replay: verify = 1 enforces every rule and fills *why; returns 0 ok, 1 broken. Always rebuilds balances/supply/prevh/lastseq. */
static int replay(int verify, char *why, size_t wl) {
    nbal = 0; memset(minted, 0, sizeof minted); memset(burned, 0, sizeof burned); memset(base, 0, sizeof base); prevh = 2166136261u; lastseq = 0;
    FILE *f = fopen(LEDGER, "r"); if (!f) return 0; char ln[1024]; long row = 0;
    while (fgets(ln, sizeof ln, f)) { row++; ln[strcspn(ln, "\n")] = 0; char *last = strrchr(ln, '|'); if (!last) { snprintf(why, wl, "row %ld: no hash", row); fclose(f); return 1; }
        char body[1024]; snprintf(body, sizeof body, "%.*s", (int)(last - ln), ln); char hx[16]; snprintf(hx, sizeof hx, "%08x", fnv(prevh, body));
        if (strcmp(hx, last + 1)) { snprintf(why, wl, "row %ld: hash chain broken (a row before or at this one was edited)", row); fclose(f); return 1; }
        prevh = fnv(prevh, body); char cp[1024]; snprintf(cp, sizeof cp, "%s", body); char *fl[8]; int k = 0; char *q = cp; while (k < 8) { fl[k++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; }
        if (k < 8) { snprintf(why, wl, "row %ld: %d fields", row, k); fclose(f); return 1; }
        long seq = atol(fl[0]); const char *vb = fl[2], *from = fl[3], *to = fl[4], *it = fl[5]; long n = atol(fl[6]);
        if (seq != lastseq + 1 && !(lastseq == 0)) { snprintf(why, wl, "row %ld: seq %ld after %ld (gap or reorder)", row, seq, lastseq); fclose(f); return 1; } lastseq = seq;
        int ix = item_ix(it); if (verify && ix < 0) { snprintf(why, wl, "row %ld: item %s is not defined", row, it); fclose(f); return 1; }
        if (verify && n <= 0) { snprintf(why, wl, "row %ld: count %ld is not positive", row, n); fclose(f); return 1; }
        if (ix < 0) continue;
        if (!strcmp(vb, "MINT")) { if (verify && !src_ok(fl[7])) { snprintf(why, wl, "row %ld: mint source '%s' is not allowed", row, fl[7]); fclose(f); return 1; } getb(to, it, 1)->n += n; minted[ix] += n; }
        else if (!strcmp(vb, "SNAP")) { getb(to, it, 1)->n += n; base[ix] += n; }
        else if (!strcmp(vb, "BURN") || !strcmp(vb, "MOVE")) { Bal *b = getb(from, it, 1); if (!b) { snprintf(why, wl, "row %ld: too many holders", row); fclose(f); return 1; } b->n -= n; if (verify && b->n < 0) { snprintf(why, wl, "row %ld: %s goes below zero in %s (%ld)", row, from, it, b->n); fclose(f); return 1; }
            if (!strcmp(vb, "BURN")) burned[ix] += n; else getb(to, it, 1)->n += n; }
        else if (verify) { snprintf(why, wl, "row %ld: unknown verb %s", row, vb); fclose(f); return 1; } }
    fclose(f);
    if (verify) for (int i = 0; i < nitems; i++) { long s = 0; for (int j = 0; j < nbal; j++) if (!strcmp(bal[j].item, items[i])) s += bal[j].n; if (s != base[i] + minted[i] - burned[i]) { snprintf(why, wl, "item %s: balances sum to %ld but minted %ld - burned %ld + baseline %ld", items[i], s, minted[i], burned[i], base[i]); return 1; } }
    return 0;
}
static void append_rows(const char *rows) { int fd = open(LEDGER, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd >= 0) { if (write(fd, rows, strlen(rows)) < 0) perror("write"); close(fd); } }
static long ts(void) { const char *e = getenv("ITEM_LEDGER_TS"); return e ? atol(e) : (long)time(NULL); }
static const char *row(char *out, size_t cap, const char *vb, const char *from, const char *to, const char *it, long n, const char *note) {      /* builds one chained row, advances prevh and lastseq */
    char body[900]; snprintf(body, sizeof body, "%ld|%ld|%s|%s|%s|%s|%ld|%s", lastseq + 1, ts(), vb, from, to, it, n, note); unsigned h = fnv(prevh, body); snprintf(out, cap, "%s|%08x\n", body, h); prevh = h; lastseq++; return out;
}
static int compact(void) {
    char why[200]; if (replay(1, why, sizeof why)) { printf("REFUSED: will not compact a broken ledger (%s)\n", why); return 3; }
    char tmp[PATH_MAX + 8], rows[MAXB * 160]; rows[0] = 0; snprintf(tmp, sizeof tmp, "%s.tmp", LEDGER); Bal keep[MAXB]; int nk = 0; for (int i = 0; i < nbal; i++) if (bal[i].n > 0) keep[nk++] = bal[i];
    prevh = 2166136261u; lastseq = 0; for (int i = 0; i < nk; i++) { char r[400]; row(r, sizeof r, "SNAP", "-", keep[i].who, keep[i].item, keep[i].n, "compacted"); strncat(rows, r, sizeof rows - strlen(rows) - 1); }
    FILE *f = fopen(tmp, "w"); if (!f) return 2; fputs(rows, f); fclose(f); rename(tmp, LEDGER); printf("COMPACTED %d snapshot rows\n", nk); return 0;
}
static int maybe_compact(void) { struct stat st; if (stat(LEDGER, &st) == 0 && st.st_size > CAP_BYTES) return compact(); return 0; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: item_ledger_op <book_dir> define|mint|burn|move|trade|balance|total|check|compact ...\n"); return 2; }
    snprintf(BOOK, sizeof BOOK, "%s", argv[1]); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/QUARANTINE", BOOK);
    if (access(q, F_OK) != 0) { printf("NOT A QUARANTINED BOOK: %s has no QUARANTINE file (this op never runs in a live game book)\n", BOOK); return 4; }
    snprintf(ITEMS, sizeof ITEMS, "%s/items.pdl", BOOK); snprintf(LEDGER, sizeof LEDGER, "%s/state/ledger.txt", BOOK); { char d[PATH_MAX]; snprintf(d, sizeof d, "%s/state", BOOK); mkdir(d, 0755); }
    const char *v = argv[2]; load_items(); load_sources(); char why[300], rb[1000];
    int lk = open(LEDGER, O_RDWR | O_CREAT, 0644); if (lk >= 0) { struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET }; fcntl(lk, F_SETLKW, &fl); }      /* one writer at a time; released at exit */
    if (!strcmp(v, "define") && argc == 5) { char id[64], nm[96]; snprintf(id, sizeof id, "%s", argv[3]); snprintf(nm, sizeof nm, "%s", argv[4]); clean(nm); if (!valid_id(id)) { printf("REFUSED: id must be <game>.<name> in lowercase, e.g. cdda.stone\n"); return 3; } if (item_ix(id) >= 0) { printf("REFUSED: %s is already defined\n", id); return 3; }
        FILE *f = fopen(ITEMS, "a"); if (!f) return 2; fprintf(f, "ITEM | %s | %s\n", id, nm); fclose(f); printf("DEFINED %s\n", id); return 0; }
    if (!strcmp(v, "check")) { if (replay(1, why, sizeof why)) { printf("CHECK FAILED: %s\n", why); return 1; } printf("CHECK OK rows=%ld items=%d\n", lastseq, nitems); return 0; }
    if (!strcmp(v, "compact")) return compact();
    if (!strcmp(v, "balance")) { if (replay(0, why, sizeof why)) return 1; for (int i = 0; i < nbal; i++) if (bal[i].n != 0 && (argc < 4 || !strcmp(argv[3], bal[i].who))) printf("%s|%s|%ld\n", bal[i].who, bal[i].item, bal[i].n); return 0; }
    if (!strcmp(v, "total") && argc == 4) { replay(0, why, sizeof why); int ix = item_ix(argv[3]); if (ix < 0) { printf("REFUSED: %s is not defined\n", argv[3]); return 3; } printf("%ld\n", base[ix] + minted[ix] - burned[ix]); return 0; }
    if (replay(1, why, sizeof why)) { printf("REFUSED: the ledger is already broken (%s); fix or restore it before writing\n", why); return 3; }      /* never write on top of a broken ledger */
    if (!strcmp(v, "mint") && argc == 7) { char to[64], it[48], sr[32]; snprintf(to, 64, "%s", argv[3]); snprintf(it, 48, "%s", argv[4]); snprintf(sr, 32, "%s", argv[6]); clean(to); long n = atol(argv[5]);
        if (item_ix(it) < 0) { printf("REFUSED: item %s is not defined\n", it); return 3; } if (n <= 0 || n > 1000000) { printf("REFUSED: count must be 1..1000000\n"); return 3; } if (!src_ok(sr)) { printf("REFUSED: mint source '%s' is not allowed (a mint needs a real source)\n", sr); return 3; }
        append_rows(row(rb, sizeof rb, "MINT", "-", to, it, n, sr)); printf("MINTED %ld %s to %s\n", n, it, to); return maybe_compact(); }
    if (!strcmp(v, "burn") && argc == 7) { char fr[64], it[48], rs[64]; snprintf(fr, 64, "%s", argv[3]); snprintf(it, 48, "%s", argv[4]); snprintf(rs, 64, "%s", argv[6]); clean(fr); clean(rs); long n = atol(argv[5]);
        if (item_ix(it) < 0) { printf("REFUSED: item %s is not defined\n", it); return 3; } Bal *b = getb(fr, it, 0); if (n <= 0 || !b || b->n < n) { printf("REFUSED: %s holds %ld %s, cannot burn %ld\n", fr, b ? b->n : 0, it, n); return 3; }
        append_rows(row(rb, sizeof rb, "BURN", fr, "-", it, n, rs)); printf("BURNED %ld %s from %s\n", n, it, fr); return maybe_compact(); }
    if (!strcmp(v, "move") && argc == 7) { char fr[64], to[64], it[48]; snprintf(fr, 64, "%s", argv[3]); snprintf(to, 64, "%s", argv[4]); snprintf(it, 48, "%s", argv[5]); clean(fr); clean(to); long n = atol(argv[6]);
        if (item_ix(it) < 0) { printf("REFUSED: item %s is not defined\n", it); return 3; } Bal *b = getb(fr, it, 0); if (n <= 0 || !b || b->n < n) { printf("REFUSED: %s holds %ld %s, cannot move %ld\n", fr, b ? b->n : 0, it, n); return 3; } if (!strcmp(fr, to)) { printf("REFUSED: from and to are the same holder\n"); return 3; }
        append_rows(row(rb, sizeof rb, "MOVE", fr, to, it, n, "move")); printf("MOVED %ld %s %s -> %s\n", n, it, fr, to); return maybe_compact(); }
    if (!strcmp(v, "trade") && argc == 9) { char a[64], b2[64], ia[48], ib[48]; snprintf(a, 64, "%s", argv[3]); snprintf(b2, 64, "%s", argv[4]); snprintf(ia, 48, "%s", argv[5]); snprintf(ib, 48, "%s", argv[7]); clean(a); clean(b2); long na = atol(argv[6]), nb = atol(argv[8]);
        if (item_ix(ia) < 0 || item_ix(ib) < 0) { printf("REFUSED: an item is not defined\n"); return 3; } if (!strcmp(a, b2)) { printf("REFUSED: a trade needs two holders\n"); return 3; }
        Bal *x = getb(a, ia, 0), *y = getb(b2, ib, 0); if (na <= 0 || nb <= 0 || !x || x->n < na) { printf("REFUSED: %s cannot give %ld %s\n", a, na, ia); return 3; } if (!y || y->n < nb) { printf("REFUSED: %s cannot give %ld %s\n", b2, nb, ib); return 3; }
        char two[2000], r1[500], r2[500]; row(r1, sizeof r1, "MOVE", a, b2, ia, na, "trade"); row(r2, sizeof r2, "MOVE", b2, a, ib, nb, "trade"); snprintf(two, sizeof two, "%s%s", r1, r2); append_rows(two); printf("TRADED %ld %s <-> %ld %s\n", na, ia, nb, ib); return maybe_compact(); }
    fprintf(stderr, "usage: item_ledger_op <book_dir> define|mint|burn|move|trade|balance|total|check|compact ...\n"); return 2;
}
