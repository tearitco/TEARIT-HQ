/* game_events.c - the S1 event line and resolver shared by df-hq, cdda-hq and mcl-hq (GAME-MECHANICS-SPRINT.md item S1; 2026-10-17).
 *
 * Text-included canonical helper (pure file I/O, no drawing, no game state of its own beyond the replay tables below); prefix ge_.
 *
 * ONE LEDGER, NOT TWO (grok's answer 2026-10-16: grow state/events.txt, do not add a second file; house.txt stays the placement replay).
 * Rows this file owns, all appended to the game's state/events.txt next to the existing STEP / DOOR / DAY / DIG rows (their readers ignore other row types):
 *   EVENT|ts|actor|verb|target|at|queued     what happened, written by ge_append() (a verb, a menu row, or a pet)
 *   EVRES|ts|<byte offset of the EVENT row>|verb|result    written ONCE by ge_scan() after it applied the row
 *   REJECT|ts|<byte offset of the bad row>|reason          written ONCE for an EVENT-looking row that is malformed (never skipped silently)
 * verb is one of give_item take walk dig build craft strike. at is "-" or "<room>:<x>,<y>". No field may contain '|', CR or LF.
 *
 * NO SEPARATE CURSOR FILE. Resolution status lives in the ledger itself (an EVRES row names the offset of its EVENT row), so a Save, Load, New game or
 * reset, which swap events.txt as a whole, can never leave a cursor that disagrees with the ledger. ge_scan() replays the ledger from byte 0 into the
 * tables below (state is a pure function of the file: replaying gives identical state), appends EVRES / REJECT rows only for rows that have none yet,
 * and is therefore idempotent: a second run appends nothing. Callers run it when the ledger's size grew (marker, not mtime).
 *
 * COMPACTION (2026-10-17, owner: "fix ledger, that is a must"): ge_compact() folds the ledger into a checkpoint block (CKPT / CKINV / CKG / CKEND rows = the replayed tables) and moves the old file to
 * state/events.archive.<n>.txt; a replay gives identical tables, ge_scan starts from the checkpoint. It runs after a successful resolve once the file reaches GE_COMPACT_BYTES() (tune ledger_compact_kb, 4 MB) and on the
 * `compact` verb. A game's hooks must stay replay-pure and provide GE_CKSAVE_HOOK / GE_CKLOAD_HOOK for their own tables. Full scheme, crash analysis, restore: BOOK 16.game/S1-EVENT-LEDGER.md "Compaction and archives".
 *
 * RULES ARE NOT HERE. Only the inventory effect of give_item (+1) and take (-1, "none" when the actor has none) is built in, because "one scripted list
 * gives the expected state" needs a state. walk / dig / build / craft resolve to "todo:<verb>": the rule bodies (labor, mood, recipes, wounds, mob weights)
 * belong to grok's files and are wired through ge_rule_hook below, which a game may define before including this file.
 *   TODO(grok): dig / build / craft rule bodies - df-hq ops/df_hq.c (dig already exists as DESIGNATE/CLAIM/DIG in jobs.txt + events.txt), cdda-hq ops/cdda_hq.c, mcl-hq ops/mcl_hq.c.
 */
#ifndef GAME_EVENTS_C
#define GAME_EVENTS_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/file.h>
#define PATH_MAX_GE 4096

#define GE_MAX_INV 512
/* HARD cap: ge_scan refuses a ledger above this (it is read whole into memory). It used to be 32 MB and made the game unstartable; compaction (below) keeps real ledgers far under it, and a file between the old cap and this one still loads and compacts. */
#ifndef GE_MAX_FILE      /* a test may build with -DGE_MAX_FILE=<bytes> to see the refusal */
#define GE_MAX_FILE (256L * 1024 * 1024)
#endif
static const char *ge_verbs[] = { "give_item", "take", "walk", "dig", "build", "craft", "strike", NULL };      /* strike: RULES-cdda-hq.md (one wound) */
typedef struct { char actor[24], item[24]; long n; } GeInv;
static GeInv ge_inv[GE_MAX_INV]; static int ge_ninv;
static char ge_last[48];      /* result of the last legal EVENT row the latest ge_scan applied ("" when none) */
typedef struct { long events, resolved_new, rejected_new, rejects_total; } GeStats;

/* a game may define: static const char *ge_rule_hook(const char *actor, const char *verb, const char *target, const char *at) returning a result text, or NULL for the default */
/* a game may also define GE_RESET_HOOK(): called at the start of every ge_scan, before the ledger is replayed, so game tables built from the ledger (blocks) start from their base state */
#ifndef GE_RESET_HOOK
#define GE_RESET_HOOK() ((void)0)
#endif
/* a game may also define GE_LOG_HOOK(actor, verb, target, at, result): called ONCE for every EVENT row the scan resolves now (never on a replay of a row that already has an EVRES, never when resolve is 0). The owner's call 2026-10-17: the output goes to the chat scroll for now; ge_describe() is the one place for the wording (a first draft, grok may change it). */
#ifndef GE_LOG_HOOK
#define GE_LOG_HOOK(a, v, t, at, r) ((void)0)
#endif
/* COMPACTION hooks (BOOK 16.game/S1-EVENT-LEDGER.md, "Compaction and archives"). GE_COMPACT_BYTES(): size at which a successful resolve compacts the ledger (a game maps it to a tune value; 0 = never).
 * GE_CKSAVE_HOOK(FILE *o): write the game's own tables as "CKG|<anything without a newline>" lines; GE_CKLOAD_HOOK(char *payload): read one such line back (payload = text after "CKG|"; GE_RESET_HOOK has already
 * restored the base state). GE_FAIL_HOOK(why): a hard refusal (ledger over the hard cap) was met; the game shows why in its note/log line. */
#ifndef GE_COMPACT_BYTES
#define GE_COMPACT_BYTES() (4L * 1024 * 1024)
#endif
#ifndef GE_CKSAVE_HOOK
#define GE_CKSAVE_HOOK(o) ((void)0)
#endif
#ifndef GE_CKLOAD_HOOK
#define GE_CKLOAD_HOOK(p) ((void)0)
#endif
#ifndef GE_FAIL_HOOK
#define GE_FAIL_HOOK(why) ((void)0)
#endif
static char ge_why[160];      /* why the last ge_scan / ge_compact refused (hard cap, memory) */
#ifndef GE_RULE_HOOK
#define GE_RULE_HOOK(a, v, t, at) ((const char *)NULL)
#endif

static int ge_field_ok(const char *s, size_t max) { size_t n = s ? strlen(s) : 0; if (n == 0 || n >= max) return 0; for (size_t i = 0; i < n; i++) if (s[i] == '|' || s[i] == '\r' || s[i] == '\n') return 0; return 1; }
static int ge_verb_ok(const char *v) { for (int i = 0; ge_verbs[i]; i++) if (!strcmp(v, ge_verbs[i])) return 1; return 0; }
static int ge_at_ok(const char *at) {      /* "-" or "<room>:<x>,<y>" */
    if (!at || !at[0]) return 0;
    if (!strcmp(at, "-")) return 1;
    const char *c = strchr(at, ':'); if (!c || c == at) return 0; const char *p = c + 1; if (!isdigit((unsigned char)*p)) return 0; while (isdigit((unsigned char)*p)) p++; if (*p != ',') return 0; p++; if (!isdigit((unsigned char)*p)) return 0; while (isdigit((unsigned char)*p)) p++; return *p == 0;
}
/* 1 when the four fields make a legal event; else 0 and err says why */
static int ge_check(const char *actor, const char *verb, const char *target, const char *at, char *err, size_t en) {
    if (!ge_field_ok(actor, 24)) { snprintf(err, en, "bad actor"); return 0; }
    if (!ge_field_ok(verb, 16) || !ge_verb_ok(verb)) { snprintf(err, en, "unknown verb"); return 0; }
    if (!ge_field_ok(target, 24)) { snprintf(err, en, "bad target"); return 0; }
    if (!ge_at_ok(at) || strlen(at) >= 40) { snprintf(err, en, "bad at (want - or room:x,y)"); return 0; }
    return 1;
}
static int ge_append_raw(const char *path, const char *line) {      /* one write() on an O_APPEND fd: rows from two processes never interleave */
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return 0; size_t n = strlen(line); ssize_t w = write(fd, line, n); close(fd); return w == (ssize_t)n;
}
/* validate and append one EVENT row; 1 ok, 0 refused (err says why, nothing was written) */
static int ge_append(const char *path, long ts, const char *actor, const char *verb, const char *target, const char *at, char *err, size_t en) {
    if (!ge_check(actor, verb, target, at, err, en)) return 0;
    char b[200]; snprintf(b, sizeof b, "EVENT|%ld|%s|%s|%s|%s|queued\n", ts, actor, verb, target, at);
    if (!ge_append_raw(path, b)) { snprintf(err, en, "ledger not writable"); return 0; } return 1;
}
static GeInv *ge_inv_find(const char *actor, const char *item, int create) {
    for (int i = 0; i < ge_ninv; i++) if (!strcmp(ge_inv[i].actor, actor) && !strcmp(ge_inv[i].item, item)) return &ge_inv[i];
    if (!create || ge_ninv >= GE_MAX_INV) return NULL;
    GeInv *q = &ge_inv[ge_ninv++]; snprintf(q->actor, sizeof q->actor, "%s", actor); snprintf(q->item, sizeof q->item, "%s", item); q->n = 0; return q;
}
static long __attribute__((unused)) ge_inv_get(const char *actor, const char *item) { GeInv *q = ge_inv_find(actor, item, 0); return q ? q->n : 0; }
/* apply one legal event to the tables; returns the result text (static or a literal) */
static const char *ge_apply(const char *actor, const char *verb, const char *target, const char *at) {
    static char res[48]; const char *h = GE_RULE_HOOK(actor, verb, target, at); if (h) return h;
    if (!strcmp(verb, "give_item")) { GeInv *q = ge_inv_find(actor, target, 1); if (!q) return "full"; q->n++; return "ok"; }
    if (!strcmp(verb, "take")) { GeInv *q = ge_inv_find(actor, target, 0); if (!q || q->n <= 0) return "none"; q->n--; return "ok"; }
    snprintf(res, sizeof res, "todo:%s", verb); return res;      /* walk dig build craft: the rule body is grok's */
}
/* the plain-text line for one resolved event ("crafts bandage", "strikes rex: leg_l"); returns 0 when the event deserves no line (walk, the todo:* placeholders) */
static const char *ge_art(const char *w) { return w && strchr("aeiou", w[0]) ? "an" : "a"; }      /* "finds a rag", "finds an apple" */
static int ge_describe(const char *verb, const char *target, const char *res, char *out, size_t n) {
    int ok = !strcmp(res, "ok");
    if (!strncmp(res, "todo:", 5)) return 0;
    if (!strcmp(verb, "give_item")) { if (ok) snprintf(out, n, "finds %s %s", ge_art(target), target); else snprintf(out, n, "cannot carry %s (%s)", target, res); return 1; }
    if (!strcmp(verb, "take")) { if (ok) snprintf(out, n, "puts away %s", target); else snprintf(out, n, "has no %s", target); return 1; }
    if (!strcmp(verb, "dig")) { if (ok) snprintf(out, n, "mines %s", target); else if (!strcmp(res, "none")) snprintf(out, n, "finds no %s there", target); else snprintf(out, n, "cannot mine %s (%s)", target, res); return 1; }
    if (!strcmp(verb, "build")) { if (ok) snprintf(out, n, "places %s", target); else if (!strcmp(res, "none")) snprintf(out, n, "has no %s to place", target); else if (!strcmp(res, "blocked")) snprintf(out, n, "cannot place %s: the cell is taken", target); else snprintf(out, n, "cannot place %s (%s)", target, res); return 1; }
    if (!strcmp(verb, "craft")) { if (ok) snprintf(out, n, "crafts %s", target); else snprintf(out, n, "cannot craft %s: %s", target, !strcmp(res, "missing") ? "need the inputs" : res); return 1; }
    if (!strcmp(verb, "strike")) { if (!strncmp(res, "wound:", 6)) snprintf(out, n, "hits %s: %s", target, res + 6); else snprintf(out, n, "cannot hit %s (%s)", target, res); return 1; }
    return 0;
}
/* split a row in place on '|'; returns the field count (max 8) */
static int ge_split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } return n; }
static int ge_all_digits(const char *s) { if (!s || !*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }

/* ---------- row classes (the compaction scheme is in BOOK 16.game/S1-EVENT-LEDGER.md, "Compaction and archives") ---------- */
static int ge_is_event_row(const char *r) { return !strncmp(r, "EVENT|", 6) || !strncmp(r, "EVENT\r", 6); }
static int ge_is_ck_row(const char *r) { return !strncmp(r, "CKPT|", 5) || !strncmp(r, "CKINV|", 6) || !strncmp(r, "CKG|", 4) || !strncmp(r, "CKEND|", 6); }
static int ge_is_ledger_row(const char *r) { return ge_is_event_row(r) || !strncmp(r, "EVRES|", 6) || !strncmp(r, "REJECT|", 7) || ge_is_ck_row(r); }
/* write-only history rows of the game (no reader in df-hq / cdda-hq / mcl-hq): compaction moves them to the archive instead of carrying them forward. Any other row type (DIG, RESET, unknown) is kept verbatim. */
static int ge_is_history_row(const char *r) {
#ifdef GE_KEEP_HISTORY
    (void)r; return 0;
#else
    return !strncmp(r, "STEP|", 5) || !strncmp(r, "DOOR|", 5) || !strncmp(r, "DAY|", 4);
#endif
}
static void ge_fail(const char *why) { static char lastw[120]; snprintf(ge_why, sizeof ge_why, "%s", why); if (strcmp(lastw, why)) { snprintf(lastw, sizeof lastw, "%s", why); GE_FAIL_HOOK(ge_why); } }

/* read the whole ledger into a malloc'd buffer (NUL at the end). 1 ok, 2 no file (= empty ledger), 0 failed (ge_why says why) */
static int ge_read_all(const char *path, char **out, long *szp) {
    FILE *f = fopen(path, "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    if (sz < 0) { fclose(f); ge_fail("ledger size unreadable"); return 0; }
    if (sz > GE_MAX_FILE) { char w[120]; snprintf(w, sizeof w, "ledger is %ld MB, over the hard cap of %ld MB: move state/events.txt aside (see S1-EVENT-LEDGER.md)", sz >> 20, (long)(GE_MAX_FILE >> 20)); fclose(f); ge_fail(w); return 0; }
    char *buf = malloc((size_t)sz + 1); if (!buf) { fclose(f); ge_fail("out of memory reading the ledger"); return 0; }
    size_t got = fread(buf, 1, (size_t)sz, f); fclose(f); buf[got] = 0; *out = buf; *szp = (long)got; return 1;
}
static int ge_cmp_long(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return x < y ? -1 : x > y; }
static int ge_has_done(const long *done, int n, long off) { return n > 0 && bsearch(&off, done, (size_t)n, sizeof *done, ge_cmp_long) != NULL; }
/* pass 1: the offsets that already have an EVRES / REJECT row (sorted), and the offset of the last COMPLETE checkpoint (CKPT ... CKEND), -1 when none */
static int ge_index(const char *buf, long sz, long **donep, int *ndonep, long *valid_ck) {
    long *done = NULL; int ndone = 0, cap = 0; long ck = -1, ckopen = -1;
    for (long off = 0; off < sz;) { const char *nl = memchr(buf + off, '\n', (size_t)(sz - off)); if (!nl) break; long len = (long)(nl - (buf + off));
        const char *r = buf + off;
        if (!strncmp(r, "EVRES|", 6) || !strncmp(r, "REJECT|", 7)) { char tmp[256]; size_t n = (size_t)len < sizeof tmp - 1 ? (size_t)len : sizeof tmp - 1; memcpy(tmp, r, n); tmp[n] = 0; char *fl[8]; int nf = ge_split(tmp, fl, 8);
            if (nf >= 3 && ge_all_digits(fl[2])) { if (ndone == cap) { cap = cap ? cap * 2 : 64; long *nd = realloc(done, (size_t)cap * sizeof *done); if (!nd) { free(done); return 0; } done = nd; } done[ndone++] = atol(fl[2]); } }
        else if (!strncmp(r, "CKPT|", 5)) ckopen = off; else if (!strncmp(r, "CKEND|", 6) && ckopen >= 0) { ck = ckopen; ckopen = -1; }
        off += len + 1; }
    if (ndone > 1) qsort(done, (size_t)ndone, sizeof *done, ge_cmp_long);
    *donep = done; *ndonep = ndone; *valid_ck = ck; return 1;
}
/* pass 2: replay the rows with offset < limit into the tables. A valid checkpoint row resets the tables and loads them from its block; EVENT rows are applied in order. With resolve 1 the EVRES / REJECT rows still missing are
 * returned in *pend (malloc'd, *plen bytes) and GE_LOG_HOOK fires once per newly resolved event. */
static int ge_replay(const char *buf, long sz, long limit, const long *done, int ndone, long valid_ck, long now, int resolve, GeStats *z, char **pendp, size_t *plenp) {
    size_t pcap = 4096, plen = 0; char *pend = malloc(pcap); if (!pend) return 0; pend[0] = 0; int in_ck = 0;
    for (long off = 0; off < sz && off < limit;) { const char *nl = memchr(buf + off, '\n', (size_t)(sz - off)); if (!nl) break; long len = (long)(nl - (buf + off)), row = off; off += len + 1; const char *r = buf + row;
        if (ge_is_ck_row(r)) {
            char tmp[512]; size_t n = (size_t)len < sizeof tmp - 1 ? (size_t)len : sizeof tmp - 1; memcpy(tmp, r, n); tmp[n] = 0; if (n && tmp[n - 1] == '\r') tmp[n - 1] = 0;
            if (!strncmp(r, "CKPT|", 5)) { in_ck = 0; if (row != valid_ck) continue; char *fl[8]; int nf = ge_split(tmp, fl, 8); if (nf < 5) continue;
                ge_ninv = 0; GE_RESET_HOOK(); z->events = atol(fl[2]); z->rejects_total = atol(fl[3]); snprintf(ge_last, sizeof ge_last, "%s", fl[4]); in_ck = 1; }
            else if (!strncmp(r, "CKEND|", 6)) in_ck = 0;
            else if (in_ck && !strncmp(r, "CKINV|", 6)) { char *fl[8]; int nf = ge_split(tmp, fl, 8); if (nf >= 4 && ge_field_ok(fl[1], 24) && ge_field_ok(fl[2], 24) && ge_ninv < GE_MAX_INV) { GeInv *q = &ge_inv[ge_ninv++]; snprintf(q->actor, sizeof q->actor, "%s", fl[1]); snprintf(q->item, sizeof q->item, "%s", fl[2]); q->n = atol(fl[3]); } }
            else if (in_ck && !strncmp(r, "CKG|", 4)) { GE_CKLOAD_HOOK(tmp + 4); }
            continue; }
        if (!strncmp(r, "REJECT|", 7)) { z->rejects_total++; continue; }
        if (!ge_is_event_row(r)) continue;
        char tmp[300]; int over = (size_t)len >= sizeof tmp; size_t n = over ? sizeof tmp - 1 : (size_t)len; memcpy(tmp, r, n); tmp[n] = 0; if (n && tmp[n - 1] == '\r') tmp[n - 1] = 0;
        char *fl[8]; int nf = over ? 0 : ge_split(tmp, fl, 8); char why[40] = ""; int ok = !over && nf == 7 && ge_all_digits(fl[1]) && ge_check(fl[2], fl[3], fl[4], fl[5], why, sizeof why) && ge_field_ok(fl[6], 24);
        if (!ok && !why[0]) snprintf(why, sizeof why, over ? "row too long" : nf != 7 ? "want 7 fields" : "bad timestamp or result");
        int has = ge_has_done(done, ndone, row);
        char line[160];
        if (ok) { z->events++; const char *res = ge_apply(fl[2], fl[3], fl[4], fl[5]); snprintf(ge_last, sizeof ge_last, "%s", res); if (!has) { if (resolve) GE_LOG_HOOK(fl[2], fl[3], fl[4], fl[5], res); snprintf(line, sizeof line, "EVRES|%ld|%ld|%s|%s\n", now, row, fl[3], res); z->resolved_new++; } else line[0] = 0; }
        else { if (!has) { snprintf(line, sizeof line, "REJECT|%ld|%ld|%s\n", now, row, why); z->rejected_new++; z->rejects_total++; } else line[0] = 0; }
        size_t ll = strlen(line); if (ll) { if (plen + ll + 1 > pcap) { size_t nc = (plen + ll + 1) * 2; char *np = realloc(pend, nc); if (!np) break; pend = np; pcap = nc; } memcpy(pend + plen, line, ll + 1); plen += ll; } }
    *pendp = pend; *plenp = plen; return 1;
}

static int ge_compact_busy;      /* set while a compaction runs: its own resolve pass must not start another one */
typedef struct { long before, after; int archive; char why[120]; } GeCompact;
static int ge_compact(const char *path, long now, GeCompact *cs);

/* Replay the whole ledger into the tables and, when resolve is 1, append the missing EVRES / REJECT rows (once each). Returns 1, or 0 when the file could not be read (ge_why says why).
 * A successful resolve that leaves the ledger at GE_COMPACT_BYTES() or more compacts it (never a replay-only scan). */
static int ge_scan(const char *path, long now, int resolve, GeStats *st) {
    GeStats z = { 0, 0, 0, 0 }; if (st) *st = z; ge_ninv = 0; ge_last[0] = 0; GE_RESET_HOOK();
    char *buf = NULL; long sz = 0; int rr = ge_read_all(path, &buf, &sz); if (rr == 2) return 1; if (!rr) return 0;      /* no ledger = empty state */
    long *done = NULL; int ndone = 0; long vck = -1; if (!ge_index(buf, sz, &done, &ndone, &vck)) { free(buf); ge_fail("out of memory indexing the ledger"); return 0; }
    char *pend = NULL; size_t plen = 0; if (!ge_replay(buf, sz, sz, done, ndone, vck, now, resolve, &z, &pend, &plen)) { free(done); free(buf); ge_fail("out of memory replaying the ledger"); return 0; }
    int okw = 1; if (resolve && plen) okw = ge_append_raw(path, pend);
    if (st) *st = z;
    free(pend); free(done); free(buf);
    if (okw && resolve && !ge_compact_busy) { long lim = (long)GE_COMPACT_BYTES(); struct stat s2; if (lim > 0 && !stat(path, &s2) && s2.st_size >= lim) { GeCompact cs; ge_compact(path, now, &cs); } }      /* ge_compact leaves the tables rebuilt from the new file (identical by construction); the caller's ge_last and stats stay */
    return okw;
}

/* ---------- compaction ---------- */
/* crash points for XO/test_ledger_compact.sh, compiled in only with -DGE_TEST_CRASH: GE_CRASH_AT=1 after the new file is written, 2 after the old ledger is linked as the archive, 3 after the rename */
#ifdef GE_TEST_CRASH
#define GE_CRASH(n) do { const char *ce_ = getenv("GE_CRASH_AT"); if (ce_ && atoi(ce_) == (n)) raise(SIGKILL); } while (0)
#else
#define GE_CRASH(n) ((void)0)
#endif
static void ge_dir_sync(const char *path) { char d[PATH_MAX_GE]; snprintf(d, sizeof d, "%s", path); char *sl = strrchr(d, '/'); if (!sl) return; *sl = 0; int fd = open(d, O_RDONLY); if (fd >= 0) { fsync(fd); close(fd); } }
static int ge_write_file(const char *p, const char *data, size_t n) {
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) return 0; size_t o = 0;
    while (o < n) { ssize_t w = write(fd, data + o, n - o); if (w <= 0) { close(fd); return 0; } o += (size_t)w; }
    int ok = fsync(fd) == 0; close(fd); return ok;
}
/* Compact the ledger now (the trigger in ge_scan and the games' `compact` verb both land here). 1 ok, 0 not done (cs->why says why; the old ledger is untouched). */
static int ge_compact(const char *path, long now, GeCompact *cs) {
    GeCompact local; if (!cs) cs = &local; memset(cs, 0, sizeof *cs);
    char lockp[PATH_MAX_GE], tmpp[PATH_MAX_GE]; snprintf(lockp, sizeof lockp, "%s.lock", path); snprintf(tmpp, sizeof tmpp, "%s.compact.tmp", path);
    int lfd = open(lockp, O_RDWR | O_CREAT, 0644); if (lfd < 0 || flock(lfd, LOCK_EX | LOCK_NB) != 0) { snprintf(cs->why, sizeof cs->why, "another compaction holds the lock"); if (lfd >= 0) close(lfd); return 0; }      /* flock dies with the process: a kill -9 leaves no stale lock */
    ge_compact_busy = 1; ge_scan(path, now, 1, NULL);      /* resolve what is pending first (logs fire, EVRES rows land) so the cut is normally the end of the file */
    int result = 0;
    for (int attempt = 0; attempt < 4 && !result; attempt++) {
        char *buf = NULL; long sz = 0; int rr = ge_read_all(path, &buf, &sz); if (rr != 1) { snprintf(cs->why, sizeof cs->why, rr == 2 ? "no ledger" : "%s", rr == 2 ? "" : ge_why); break; }
        long *done = NULL; int ndone = 0, nok = 1; long vck = -1; if (!ge_index(buf, sz, &done, &ndone, &vck)) { free(buf); snprintf(cs->why, sizeof cs->why, "out of memory"); break; }
        long full = 0; { long off = 0; while (off < sz) { const char *nl = memchr(buf + off, '\n', (size_t)(sz - off)); if (!nl) break; off = (long)(nl - buf) + 1; } full = off; }      /* end of the last complete line */
        long cut = full; for (long off = 0; off < full;) { const char *nl = memchr(buf + off, '\n', (size_t)(full - off)); long len = (long)(nl - (buf + off)); if (ge_is_event_row(buf + off) && !ge_has_done(done, ndone, off)) { cut = off; break; } off += len + 1; }      /* first EVENT row with no EVRES / REJECT yet */
        GeStats z = { 0, 0, 0, 0 }; ge_ninv = 0; ge_last[0] = 0; GE_RESET_HOOK(); char *pend = NULL; size_t plen = 0;
        if (!ge_replay(buf, sz, cut, done, ndone, vck, now, 0, &z, &pend, &plen)) { free(done); free(buf); snprintf(cs->why, sizeof cs->why, "out of memory"); break; } free(pend);
        /* the tail (rows from the cut on) is copied; its EVENT rows move, so its EVRES / REJECT offsets are rewritten; a done row whose EVENT is before the cut is counted in the checkpoint and dropped */
        long *tev = NULL; int ntev = 0, tcap = 0; long tail_rej_dropped = 0;
        for (long off = cut; off < full;) { const char *nl = memchr(buf + off, '\n', (size_t)(full - off)); long len = (long)(nl - (buf + off)); if (ge_is_event_row(buf + off)) { if (ntev == tcap) { tcap = tcap ? tcap * 2 : 64; long *nt = realloc(tev, (size_t)tcap * sizeof *tev); if (!nt) { nok = 0; break; } tev = nt; } tev[ntev++] = off; } off += len + 1; }
        for (long off = cut; nok && off < full;) { const char *nl = memchr(buf + off, '\n', (size_t)(full - off)); long len = (long)(nl - (buf + off));
            if (!strncmp(buf + off, "REJECT|", 7)) { char tmp[256]; size_t n = (size_t)len < sizeof tmp - 1 ? (size_t)len : sizeof tmp - 1; memcpy(tmp, buf + off, n); tmp[n] = 0; char *fl[8]; int nf = ge_split(tmp, fl, 8); long ref = nf >= 3 && ge_all_digits(fl[2]) ? atol(fl[2]) : -1; if (!(ref >= cut && bsearch(&ref, tev, (size_t)ntev, sizeof *tev, ge_cmp_long))) tail_rej_dropped++; }
            off += len + 1; }
        if (!nok) { free(tev); free(done); free(buf); snprintf(cs->why, sizeof cs->why, "out of memory"); break; }
        char *obuf = NULL; size_t olen = 0; FILE *o = open_memstream(&obuf, &olen); if (!o) { free(tev); free(done); free(buf); snprintf(cs->why, sizeof cs->why, "out of memory"); break; }
        fprintf(o, "CKPT|%ld|%ld|%ld|%s\n", now, z.events, z.rejects_total + tail_rej_dropped, ge_last);
        long ckn = 1; for (int i = 0; i < ge_ninv; i++) { fprintf(o, "CKINV|%s|%s|%ld\n", ge_inv[i].actor, ge_inv[i].item, ge_inv[i].n); ckn++; }
        GE_CKSAVE_HOOK(o); fflush(o); fprintf(o, "CKEND|%ld\n", ckn);      /* CKEND ends the block (its count is the CKINV rows plus the CKPT row; a hook's CKG rows are in between) */
        for (long off = 0; off < cut;) { const char *nl = memchr(buf + off, '\n', (size_t)(cut - off)); long len = (long)(nl - (buf + off)); const char *r = buf + off;
            if (!ge_is_ledger_row(r) && !ge_is_history_row(r)) fwrite(r, 1, (size_t)len + 1, o); off += len + 1; }
        long *tnew = ntev ? malloc((size_t)ntev * sizeof *tnew) : NULL; int ti = 0; fflush(o);
        for (long off = cut; off < full;) { const char *nl = memchr(buf + off, '\n', (size_t)(full - off)); long len = (long)(nl - (buf + off)); const char *r = buf + off;
            if (ge_is_event_row(r)) { fflush(o); if (tnew) tnew[ti] = (long)ftell(o); ti++; fwrite(r, 1, (size_t)len + 1, o); }
            else if (!strncmp(r, "EVRES|", 6) || !strncmp(r, "REJECT|", 7)) {
                char tmp[256]; size_t n = (size_t)len < sizeof tmp - 1 ? (size_t)len : sizeof tmp - 1; memcpy(tmp, r, n); tmp[n] = 0; char *fl[8]; int nf = ge_split(tmp, fl, 8); long ref = nf >= 3 && ge_all_digits(fl[2]) ? atol(fl[2]) : -1;
                long *hit = ref >= cut ? bsearch(&ref, tev, (size_t)ntev, sizeof *tev, ge_cmp_long) : NULL;
                if (hit && tnew) { const char *p2 = strchr(strchr(r, '|') + 1, '|'); const char *p3 = p2 ? strchr(p2 + 1, '|') : NULL; if (p2 && p3 && p3 < r + len) fprintf(o, "%.*s|%ld%.*s\n", (int)(p2 - r), r, tnew[hit - tev], (int)(r + len - p3), p3); else if (p2) fprintf(o, "%.*s|%ld\n", (int)(p2 - r), r, tnew[hit - tev]); } }
            else if (!ge_is_ck_row(r) && !ge_is_history_row(r)) fwrite(r, 1, (size_t)len + 1, o);
            off += len + 1; }
        if (full < sz) fwrite(buf + full, 1, (size_t)(sz - full), o);      /* a torn last line stays exactly as it was */
        fclose(o); free(tnew); free(tev);
        if (!obuf || !ge_write_file(tmpp, obuf, olen)) { free(obuf); free(done); free(buf); snprintf(cs->why, sizeof cs->why, "could not write %s", tmpp); break; }
        struct stat s2; if (stat(path, &s2) != 0 || s2.st_size != sz) { free(obuf); free(done); free(buf); continue; }      /* the ledger grew while we worked: redo from the new size */
        GE_CRASH(1);
        char arch[PATH_MAX_GE], dir[PATH_MAX_GE]; snprintf(dir, sizeof dir, "%s", path); char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
        int n = 1, linked = 0; for (; n < 100000 && !linked; n++) { snprintf(arch, sizeof arch, "%sevents.archive.%d.txt", dir, n); if (link(path, arch) == 0) { linked = 1; break; } if (errno != EEXIST) break; }
        if (!linked) {      /* no hard links here: write the archive from the buffer (a .part file first, then link it into place, never overwriting) */
            for (n = 1; n < 100000; n++) { char part[PATH_MAX_GE]; snprintf(arch, sizeof arch, "%sevents.archive.%d.txt", dir, n); if (access(arch, F_OK) == 0) continue; snprintf(part, sizeof part, "%s.part", arch); if (ge_write_file(part, buf, (size_t)sz) && link(part, arch) == 0) { unlink(part); linked = 1; } break; } }
        if (!linked) { free(obuf); free(done); free(buf); snprintf(cs->why, sizeof cs->why, "could not archive the old ledger"); break; }
        GE_CRASH(2);
        if (rename(tmpp, path) != 0) { free(obuf); free(done); free(buf); snprintf(cs->why, sizeof cs->why, "rename failed"); break; }
        GE_CRASH(3);
        ge_dir_sync(path); cs->before = sz; cs->after = (long)olen; cs->archive = n; result = 1; free(obuf); free(done); free(buf); }
    if (!result && !cs->why[0]) snprintf(cs->why, sizeof cs->why, "the ledger kept growing; not compacted");
    ge_compact_busy = 0; flock(lfd, LOCK_UN); close(lfd);
    ge_scan(path, now, 0, NULL);      /* leave the tables as the (new) ledger says */
    return result;
}
#endif
