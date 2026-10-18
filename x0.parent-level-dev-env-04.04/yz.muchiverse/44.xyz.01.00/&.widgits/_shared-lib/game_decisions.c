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
#endif
