/* game_events.c - the S1 event line and resolver shared by df-hq, cdda-hq and mcl-hq (GAME-MECHANICS-SPRINT.md item S1; 2026-10-17).
 *
 * Text-included canonical helper (pure file I/O, no drawing, no game state of its own beyond the replay tables below); prefix ge_.
 *
 * ONE LEDGER, NOT TWO (grok's answer 2026-10-16: grow state/events.txt, do not add a second file; house.txt stays the placement replay).
 * Rows this file owns, all appended to the game's state/events.txt next to the existing STEP / DOOR / DAY / DIG rows (their readers ignore other row types):
 *   EVENT|ts|actor|verb|target|at|queued     what happened, written by ge_append() (a verb, a menu row, or a pet)
 *   EVRES|ts|<byte offset of the EVENT row>|verb|result    written ONCE by ge_scan() after it applied the row
 *   REJECT|ts|<byte offset of the bad row>|reason          written ONCE for an EVENT-looking row that is malformed (never skipped silently)
 * verb is one of give_item take walk dig build craft. at is "-" or "<room>:<x>,<y>". No field may contain '|', CR or LF.
 *
 * NO SEPARATE CURSOR FILE. Resolution status lives in the ledger itself (an EVRES row names the offset of its EVENT row), so a Save, Load, New game or
 * reset, which swap events.txt as a whole, can never leave a cursor that disagrees with the ledger. ge_scan() replays the ledger from byte 0 into the
 * tables below (state is a pure function of the file: replaying gives identical state), appends EVRES / REJECT rows only for rows that have none yet,
 * and is therefore idempotent: a second run appends nothing. Callers run it when the ledger's size grew (marker, not mtime).
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

#define GE_MAX_INV 512
#define GE_MAX_FILE (32L * 1024 * 1024)
static const char *ge_verbs[] = { "give_item", "take", "walk", "dig", "build", "craft", NULL };
typedef struct { char actor[24], item[24]; long n; } GeInv;
static GeInv ge_inv[GE_MAX_INV]; static int ge_ninv;
static char ge_last[48];      /* result of the last legal EVENT row the latest ge_scan applied ("" when none) */
typedef struct { long events, resolved_new, rejected_new, rejects_total; } GeStats;

/* a game may define: static const char *ge_rule_hook(const char *actor, const char *verb, const char *target, const char *at) returning a result text, or NULL for the default */
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
/* split a row in place on '|'; returns the field count (max 8) */
static int ge_split(char *s, char **f, int max) { int n = 0; f[n++] = s; for (char *p = s; *p && n < max; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; } return n; }
static int ge_all_digits(const char *s) { if (!s || !*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }

/* Replay the whole ledger into the tables and, when resolve is 1, append the missing EVRES / REJECT rows (once each). Returns 1, or 0 when the file could not be read. */
static int ge_scan(const char *path, long now, int resolve, GeStats *st) {
    GeStats z = { 0, 0, 0, 0 }; if (st) *st = z; ge_ninv = 0; ge_last[0] = 0;
    FILE *f = fopen(path, "rb"); if (!f) return 1;      /* no ledger = empty state */
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); if (sz < 0 || sz > GE_MAX_FILE) { fclose(f); return 0; }
    char *buf = malloc((size_t)sz + 1); if (!buf) { fclose(f); return 0; } size_t got = fread(buf, 1, (size_t)sz, f); fclose(f); buf[got] = 0; sz = (long)got;
    long *done = NULL; int ndone = 0, cap = 0;      /* offsets that already have an EVRES or REJECT row */
    for (long off = 0; off < sz;) { char *nl = memchr(buf + off, '\n', (size_t)(sz - off)); if (!nl) break; long len = (long)(nl - (buf + off));
        if (!strncmp(buf + off, "EVRES|", 6) || !strncmp(buf + off, "REJECT|", 7)) { char tmp[256]; size_t n = (size_t)len < sizeof tmp - 1 ? (size_t)len : sizeof tmp - 1; memcpy(tmp, buf + off, n); tmp[n] = 0; char *fl[8]; int nf = ge_split(tmp, fl, 8);
            if (nf >= 3 && ge_all_digits(fl[2])) { if (ndone == cap) { cap = cap ? cap * 2 : 64; long *nd = realloc(done, (size_t)cap * sizeof *done); if (!nd) { free(done); free(buf); return 0; } done = nd; } done[ndone++] = atol(fl[2]); if (fl[0][0] == 'R' && fl[0][1] == 'E') z.rejects_total++; } }
        off += len + 1; }
    size_t pcap = 4096, plen = 0; char *pend = malloc(pcap); if (!pend) { free(done); free(buf); return 0; } pend[0] = 0;      /* rows to append after the scan, in one write */
    for (long off = 0; off < sz;) { char *nl = memchr(buf + off, '\n', (size_t)(sz - off)); if (!nl) break; long len = (long)(nl - (buf + off)), row = off; off += len + 1;
        if (strncmp(buf + row, "EVENT|", 6) != 0 && strncmp(buf + row, "EVENT\r", 6) != 0) continue;
        char tmp[300]; int over = (size_t)len >= sizeof tmp; size_t n = over ? sizeof tmp - 1 : (size_t)len; memcpy(tmp, buf + row, n); tmp[n] = 0; if (n && tmp[n - 1] == '\r') tmp[n - 1] = 0;
        char *fl[8]; int nf = over ? 0 : ge_split(tmp, fl, 8); char why[40] = ""; int ok = !over && nf == 7 && ge_all_digits(fl[1]) && ge_check(fl[2], fl[3], fl[4], fl[5], why, sizeof why) && ge_field_ok(fl[6], 24);
        if (!ok && !why[0]) snprintf(why, sizeof why, over ? "row too long" : nf != 7 ? "want 7 fields" : "bad timestamp or result");
        int has = 0; for (int i = 0; i < ndone; i++) if (done[i] == row) { has = 1; break; }
        char line[160];
        if (ok) { z.events++; const char *res = ge_apply(fl[2], fl[3], fl[4], fl[5]); snprintf(ge_last, sizeof ge_last, "%s", res); if (!has) { snprintf(line, sizeof line, "EVRES|%ld|%ld|%s|%s\n", now, row, fl[3], res); z.resolved_new++; } else line[0] = 0; }
        else { if (!has) { snprintf(line, sizeof line, "REJECT|%ld|%ld|%s\n", now, row, why); z.rejected_new++; z.rejects_total++; } else line[0] = 0; }
        size_t ll = strlen(line); if (ll) { if (plen + ll + 1 > pcap) { size_t nc = (plen + ll + 1) * 2; char *np = realloc(pend, nc); if (!np) break; pend = np; pcap = nc; } memcpy(pend + plen, line, ll + 1); plen += ll; } }
    int okw = 1; if (resolve && plen) okw = ge_append_raw(path, pend);
    if (st) *st = z;
    free(pend); free(done); free(buf); return okw;
}
#endif
