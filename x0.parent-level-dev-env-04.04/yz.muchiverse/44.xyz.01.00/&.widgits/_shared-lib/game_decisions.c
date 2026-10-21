/* game_decisions.c - M3 decision ledger (GAMES-ROADMAP M3): one append-only line per actor decision, plus a daily review file.
 * Text-included (pure logic, 2+ consumers: df-hq, cdda-hq, mcl-hq), like game_events.c. No globals but one static cursor; every path comes from the caller.
 * Doc: BOOK/16.game/DECISION-LEDGER-AND-DAILY-REVIEW.md
 *
 * Row (state/decisions.txt):  DEC|<unix>|<game stamp e.g. D2 08:20, may be empty>|<actor>|<state>|<action>|<target>|<outcome>
 *   actor   who decided (pet id, "zed")        state   what it was doing/knew before (idle, goal, adjacent)
 *   action  what it chose (pick, walk, idle, greet, strike, chase)   target  where/who ("x,y", a name, "-")
 *   outcome what happened (ok, blocked, hit, miss, moved, none)
 * One write() per row with O_APPEND, so a crash tears at most the last line and readers ignore a row that does not end in a newline.
 * Size growth is the change marker (never mtime): a reader keeps the byte offset it has consumed. The file rotates to decisions.archive.txt past
 * DEC_MAX_BYTES (rename is atomic; the previous archive is replaced, the owner can raise the cap). The ledger is for review and learning input
 * (the behavior bank), so it may be cut; the event ledger (game_events.c) is the authority for game state.
 * Fields never contain '|' or a newline (replaced by '/' and ' ').
 */
#ifndef GAME_DECISIONS_C
#define GAME_DECISIONS_C
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef DEC_MAX_BYTES
#define DEC_MAX_BYTES (2L * 1024 * 1024)
#endif

static void dec_clean(char *o, size_t cap, const char *s) {
    size_t n = 0; if (!s || !*s) s = "-";
    for (; *s && n + 1 < cap && n < 40; s++) o[n++] = (*s == '|') ? '/' : (*s == '\n' || *s == '\r' || *s == '\t') ? ' ' : *s;
    o[n] = 0; if (!n) { o[0] = '-'; o[1] = 0; }
}

/* append one decision; returns 1 when written. path = state/decisions.txt */
static int dec_append(const char *path, const char *stamp, const char *actor, const char *state, const char *action, const char *target, const char *outcome) {
    char a[48], s[48], c[48], t[48], o[48], st[32], ln[400]; struct stat sb;
    dec_clean(a, sizeof a, actor); dec_clean(s, sizeof s, state); dec_clean(c, sizeof c, action); dec_clean(t, sizeof t, target); dec_clean(o, sizeof o, outcome);
    if (stamp && *stamp) dec_clean(st, sizeof st, stamp); else st[0] = 0;
    if (stat(path, &sb) == 0 && sb.st_size > DEC_MAX_BYTES) { char ap[4200]; snprintf(ap, sizeof ap, "%.*s", (int)(strlen(path) > 4 ? strlen(path) - 4 : strlen(path)), path); strncat(ap, ".archive.txt", sizeof ap - strlen(ap) - 1); rename(path, ap); }
    int n = snprintf(ln, sizeof ln, "DEC|%ld|%s|%s|%s|%s|%s|%s\n", (long)time(NULL), st, a, s, c, t, o); if (n <= 0 || n >= (int)sizeof ln) return 0;
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return 0;
    int ok = write(fd, ln, (size_t)n) == n; close(fd); return ok;
}

/* the daily review: reads the ledger from the start (bounded to the last 4 MB), keeps the rows of one game day (day < 0 = the latest day seen),
 * counts rows per actor and per (action, outcome), and writes the review file atomically (tmp + rename). Returns the number of rows reviewed, -1 on error.
 * Review format (plain text, key=value + table rows, the same style as the other state files):
 *   day=D2   rows=37   actors=3   first=D2 00:05   last=D2 23:50
 *   ACTOR|<name>|<rows>|<ok rows>|<blocked rows>
 *   ACTION|<action>|<outcome>|<count>
 */
#define DEC_MAXKEY 48
typedef struct { char a[DEC_MAXKEY], b[DEC_MAXKEY]; int n, ok, blocked; } DecTally;
static int dec_review(const char *path, const char *out, int day) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); long from = sz > 4L * 1024 * 1024 ? sz - 4L * 1024 * 1024 : 0; fseek(f, from, SEEK_SET);
    enum { MAXT = 96 }; static DecTally ac[MAXT], ax[MAXT]; int nac = 0, nax = 0; memset(ac, 0, sizeof ac); memset(ax, 0, sizeof ax);
    char ln[512], first[32] = "", last[32] = ""; int rows = 0, latest = -1; long keep_pos = ftell(f);
    if (from > 0 && !fgets(ln, sizeof ln, f)) { fclose(f); return -1; }                         /* skip the partial first line of a tail read */
    keep_pos = ftell(f);
    for (int pass = 0; pass < 2; pass++) {
        fseek(f, keep_pos, SEEK_SET);
        while (fgets(ln, sizeof ln, f)) {
            size_t L = strlen(ln); if (!L || ln[L - 1] != '\n') continue;            /* torn last line: ignored */
            ln[L - 1] = 0; if (strncmp(ln, "DEC|", 4)) continue;
            char *fld[8]; int nf = 0; char *p = ln + 4; fld[nf++] = p;
            for (; *p && nf < 8; p++) if (*p == '|') { *p = 0; fld[nf++] = p + 1; }
            if (nf < 7) continue;                                                     /* unix, stamp, actor, state, action, target, outcome */
            int d = (fld[1][0] == 'D') ? atoi(fld[1] + 1) : 0;
            if (pass == 0) { if (d > latest) latest = d; continue; }
            if (d != (day < 0 ? latest : day)) continue;
            rows++; if (!first[0]) snprintf(first, sizeof first, "%s", fld[1]); snprintf(last, sizeof last, "%s", fld[1]);
            int blocked = !strcmp(fld[6], "blocked") || !strcmp(fld[6], "miss") || !strcmp(fld[6], "refused"), ok = !blocked;
            int k; for (k = 0; k < nac; k++) if (!strcmp(ac[k].a, fld[2])) break;
            if (k == nac && nac < MAXT) { snprintf(ac[nac].a, DEC_MAXKEY, "%s", fld[2]); nac++; } if (k < MAXT) { ac[k].n++; ac[k].ok += ok; ac[k].blocked += blocked; }
            for (k = 0; k < nax; k++) if (!strcmp(ax[k].a, fld[4]) && !strcmp(ax[k].b, fld[6])) break;
            if (k == nax && nax < MAXT) { snprintf(ax[nax].a, DEC_MAXKEY, "%s", fld[4]); snprintf(ax[nax].b, DEC_MAXKEY, "%s", fld[6]); nax++; } if (k < MAXT) ax[k].n++;
        }
    }
    fclose(f);
    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", out); FILE *o = fopen(tmp, "w"); if (!o) return -1;
    int shown = day < 0 ? latest : day;
    fprintf(o, "day=D%d\nrows=%d\nactors=%d\nfirst=%s\nlast=%s\n", shown < 0 ? 0 : shown, rows, nac, first, last);
    for (int k = 0; k < nac; k++) fprintf(o, "ACTOR|%s|%d|%d|%d\n", ac[k].a, ac[k].n, ac[k].ok, ac[k].blocked);
    for (int k = 0; k < nax; k++) fprintf(o, "ACTION|%s|%s|%d\n", ax[k].a, ax[k].b, ax[k].n);
    if (fflush(o) != 0 || ferror(o)) { fclose(o); unlink(tmp); return -1; }
    fsync(fileno(o)); fclose(o); if (rename(tmp, out) != 0) { unlink(tmp); return -1; }
    return rows;
}

/* ---- the behavior-bank wiring (HARNESS-AS-GAME-BRAIN.md): DEC rows -> one bank entry per action -> a weight the actor reads back ----
 * dec_bank_update(dec_path, bank_dir, game) reads the decision ledger from the byte offset in <bank_dir>/dec_cursor.txt (append-only marker, never mtime),
 * adds one reward per row whose outcome is not blocked/miss/refused and one punish per row whose outcome is, per ACTION, and rewrites
 * <bank_dir>/dec_<game>_<action>.behavior.pdl in the house behavior-bank sidecar shape (BEHAVIOR / KEYWORDS / SENTENCE / SOURCE / COUNTS / WEIGHT) so that
 * `harness_bank_op find <bank_dir> <word>` ranks the actions with no new reader. WEIGHT is the live Laplace estimate (reward+1)/(reward+punish+2).
 * Each file is written tmp+rename. The cursor is written last: a crash between the two double-counts at most one batch (at-least-once, stated in the doc).
 * Returns the number of rows consumed, -1 on error. A ledger shorter than the cursor (it rotated) restarts the cursor at 0; counts are kept. */
static int dec_is_bad(const char *o) { return !strcmp(o, "blocked") || !strcmp(o, "miss") || !strcmp(o, "refused"); }
static void dec_read_counts(const char *path, long *rw, long *pn) {
    *rw = *pn = 0; FILE *f = fopen(path, "r"); if (!f) return; char ln[300];
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "COUNTS", 6)) { char *a = strstr(ln, "reward="), *b = strstr(ln, "punish="); if (a) *rw = atol(a + 7); if (b) *pn = atol(b + 7); break; }
    fclose(f);
}
static int dec_bank_update(const char *dec_path, const char *bank_dir, const char *game) {
    char cp[4200], ln[512]; snprintf(cp, sizeof cp, "%s/dec_cursor.txt", bank_dir); mkdir(bank_dir, 0755);
    long cursor = 0; { FILE *f = fopen(cp, "r"); if (f) { if (fscanf(f, "%ld", &cursor) != 1) cursor = 0; fclose(f); } }
    FILE *f = fopen(dec_path, "r"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); if (cursor < 0 || cursor > sz) cursor = 0;
    fseek(f, cursor, SEEK_SET);
    struct { char act[DEC_MAXKEY]; long rw, pn; } t[32]; int nt = 0, rows = 0; long pos = cursor;
    while (fgets(ln, sizeof ln, f)) {
        size_t L = strlen(ln); if (!L || ln[L - 1] != '\n') break;                 /* torn last line: stop BEFORE it, the cursor stays on it */
        pos += (long)L; ln[L - 1] = 0; if (strncmp(ln, "DEC|", 4)) continue;
        char *fld[8]; int nf = 0; char *q = ln + 4; fld[nf++] = q; for (; *q && nf < 8; q++) if (*q == '|') { *q = 0; fld[nf++] = q + 1; }
        if (nf < 7) continue;
        if (strpbrk(fld[4], "/.\\")) continue;                                     /* the action becomes part of a file name */
        int k; for (k = 0; k < nt; k++) if (!strcmp(t[k].act, fld[4])) break;
        if (k == nt) { if (nt == 32) continue; snprintf(t[nt].act, DEC_MAXKEY, "%s", fld[4]); t[nt].rw = t[nt].pn = 0; nt++; }
        if (dec_is_bad(fld[6])) t[k].pn++; else t[k].rw++;
        rows++;
    }
    fclose(f);
    for (int k = 0; k < nt; k++) {
        char fp[4200], tp[4300]; snprintf(fp, sizeof fp, "%s/dec_%s_%s.behavior.pdl", bank_dir, game, t[k].act); snprintf(tp, sizeof tp, "%s.tmp", fp);
        long rw, pn; dec_read_counts(fp, &rw, &pn); rw += t[k].rw; pn += t[k].pn;
        FILE *o = fopen(tp, "w"); if (!o) continue;
        fprintf(o, "BEHAVIOR | dec.%s.%s\nKEYWORDS | %s, %s, decision\nSENTENCE | How often the %s choice '%s' ends well (outcome not blocked, miss or refused) in %s's decision ledger.\n"
                   "SOURCE | measured from state/decisions.txt by dec_bank_update; not hand-authored\nCOUNTS | reward=%ld | punish=%ld | cursor=%ld\nWEIGHT | %.4f\n",
                game, t[k].act, t[k].act, game, game, t[k].act, game, rw, pn, pos, (double)(rw + 1) / (double)(rw + pn + 2));
        if (fflush(o) != 0 || ferror(o)) { fclose(o); unlink(tp); continue; }
        fsync(fileno(o)); fclose(o); rename(tp, fp);
    }
    char tc[4300]; snprintf(tc, sizeof tc, "%s.tmp", cp); FILE *c = fopen(tc, "w"); if (!c) return -1;
    fprintf(c, "%ld\n", pos); fflush(c); fsync(fileno(c)); fclose(c); if (rename(tc, cp) != 0) return -1;
    return rows;
}
/* the weight of one action for this game, or 0.5 (no evidence = no change) when the entry does not exist yet */
static double dec_weight(const char *bank_dir, const char *game, const char *action) {
    char fp[4200], ln[300]; snprintf(fp, sizeof fp, "%s/dec_%s_%s.behavior.pdl", bank_dir, game, action); FILE *f = fopen(fp, "r"); if (!f) return 0.5;
    double w = 0.5; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "WEIGHT", 6)) { char *b = strchr(ln, '|'); if (b) w = atof(b + 1); break; }
    fclose(f); return w < 0 ? 0 : w > 1 ? 1 : w;
}
#endif
