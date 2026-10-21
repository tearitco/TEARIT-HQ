/* playthrough_op - the PHASE-TABLE front end of the scripted playthrough harnesses (df-hq, cdda-hq, mcl-hq; BOOK 06-testing/GAME-PLAYTHROUGH-HARNESSES.md).
 *
 * A playthrough is DATA: a phase table (play/phases/<name>.phases.pdl, format in the doc and in the header of each table). This op does the three jobs a pal cannot:
 *   compile <phases.pdl> [<case_dir>]   writes <case_dir>/play_<name>.pdl, an ordinary harness_case_op case file (default case_dir = play/cases; the three shipped pals pass `cases`),
 *                                       plus one mini case + one behavior-bank seed per phase under play/cases and play/bank (seeds are never overwritten: measured counts live in play/bank/live)
 *   ledger  <phases.pdl> [<case_dir>]   after harness_case_op ran: reads this run's PASS/FAIL rows (after the last RUN row), groups them by phase, takes the time of each phase from the
 *                                       marks file, appends one PH row per phase and one PLAY row to the append-only RESULT ledger play/results/play_ledger.txt, writes one PASS/FAIL row
 *                                       per phase to play/results/ph/<game>__<phase>.txt and runs harness_bank_op update on it, so every phase has a LIVE weight = (passes+1)/(runs+2)
 *   run     <phases.pdl> [<case_dir>]   compile + harness_case_op + harness_verdict_op + ledger in one call (what a game AI calls with its own proposed phase list); prints one
 *                                       PLAYTHROUGH line and one line per phase with its weight; exit 0 = every check passed, 1 = a check failed, 2 = usage / bad table
 *   mark    <file> <label>              appends `epoch_ms|label` (the compiled case calls this at each phase start; the only timing source a case file has)
 *
 * RESULT ledger rows (play/results/play_ledger.txt, append-only, git-ignored):
 *   PH|<run id ms>|<game>|<name>|<phase>|<PASS|FAIL|NOCHECKS>|<ms>|pass=N|fail=M|<first failed label or ->
 *   PLAY|<run id ms>|<game>|<name>|<PASS|FAIL>|<total ms>|phases=N|failed=M
 * The run id is the epoch ms of the harness_case_op RUN row, so a ledger call is idempotent (a PLAY row with the same run id and name is never written twice).
 *
 * Build: gcc -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o +x/playthrough_op.+x playthrough_op.c
 * No shell anywhere: children are fork/exec'ed with an argv. Never touches a live game: the compiled case works only under its own /tmp/hc_* scratch. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <libgen.h>
#include <glob.h>
#include <limits.h>

#define P 4096
#define MAXF 40
#define MAXPH 64
#define MAXL 256
static char HARN[P];                      /* .../_shared-lib/harness */

typedef struct { char *f[MAXF]; int n; } Row;
typedef struct { char id[64], title[256], kw[256]; Row body[MAXL]; int nb; } Phase;
typedef struct { char name[128], game[64], bin[64]; int events, common; Row init[MAXL], needs[32]; int ninit, nneeds; Phase ph[MAXPH]; int nph; char path[P]; } Table;

static long long now_ms(void) { struct timeval tv; gettimeofday(&tv, NULL); return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000; }
static char *trim(char *s) { char *e; while (*s == ' ' || *s == '\t') s++; e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static void mkdirs(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }
static void mkparent(const char *file) { char t[P]; snprintf(t, sizeof t, "%s", file); char *s = strrchr(t, '/'); if (s) { *s = 0; mkdirs(t); } }
static int exists(const char *p) { return access(p, F_OK) == 0; }

static void split_row(char *line, Row *r) {
    char *q = line; r->n = 0;
    for (;;) { char *bar = strchr(q, '|'); r->f[r->n++] = q; if (!bar || r->n == MAXF) break; *bar = 0; q = bar + 1; }
    for (int i = 0; i < r->n; i++) r->f[i] = strdup(trim(r->f[i]));
}
/* the rest of a row from field i on, rejoined with " | " (text fields may hold bars) */
static void rejoin(const Row *r, int i, char *out, size_t n) {
    size_t o = 0; out[0] = 0;
    for (; i < r->n; i++) o += (size_t)snprintf(out + o, n - o, "%s%s", o ? " | " : "", r->f[i]);
}
static int valid_id(const char *s) { if (!*s) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return 0; return 1; }

static int parse_table(const char *path, Table *t) {
    FILE *f = fopen(path, "r"); char line[P], rp[P]; Phase *cur = NULL;
    memset(t, 0, sizeof *t);
    if (!f) { fprintf(stderr, "playthrough_op: cannot open %s\n", path); return -1; }
    if (!realpath(path, rp)) snprintf(rp, sizeof rp, "%s", path);
    snprintf(t->path, sizeof t->path, "%s", rp);
    { char b[P]; snprintf(b, sizeof b, "%s", rp); char *bn = basename(b); snprintf(t->name, sizeof t->name, "%s", bn); char *d = strstr(t->name, ".phases.pdl"); if (d) *d = 0; else { d = strrchr(t->name, '.'); if (d) *d = 0; } }
    while (fgets(line, sizeof line, f)) {
        char *p = trim(line); Row r; if (!*p || *p == '#') continue;
        split_row(p, &r); const char *v = r.f[0];
        if (!strcmp(v, "GAME") && r.n >= 3) { snprintf(t->game, sizeof t->game, "%s", r.f[1]); snprintf(t->bin, sizeof t->bin, "%s", r.f[2]); }
        else if (!strcmp(v, "EVENTS") && r.n >= 2) t->events = !strcmp(r.f[1], "yes");      /* the game folder keeps events/ and the scratch house gets the real event runner, the manager and bp_edit_op (blueprint-programming tables) */
        else if (!strcmp(v, "COMMON") && r.n >= 2) t->common = !strcmp(r.f[1], "yes");      /* with EVENTS: common_events/ in the scratch house, the fixed call_event_op, mr_change_gold, and a caller entity <app>/ce_caller (BPCE, PLAYCE rows) */
        else if (!strcmp(v, "INIT") && r.n >= 3 && t->ninit < MAXL) t->init[t->ninit++] = r;
        else if (!strcmp(v, "NEEDS") && r.n >= 2 && t->nneeds < 32) t->needs[t->nneeds++] = r;
        else if (!strcmp(v, "PHASE") && r.n >= 2) {
            if (t->nph >= MAXPH) { fprintf(stderr, "playthrough_op: more than %d phases\n", MAXPH); return -1; }
            if (!valid_id(r.f[1]) || !strcmp(r.f[1], "setup")) { fprintf(stderr, "playthrough_op: bad phase id '%s' (letters digits _ - only, not 'setup')\n", r.f[1]); return -1; }
            for (int i = 0; i < t->nph; i++) if (!strcmp(t->ph[i].id, r.f[1])) { fprintf(stderr, "playthrough_op: duplicate phase id %s\n", r.f[1]); return -1; }
            cur = &t->ph[t->nph++]; snprintf(cur->id, sizeof cur->id, "%s", r.f[1]);
            snprintf(cur->title, sizeof cur->title, "%s", r.n >= 3 ? r.f[2] : r.f[1]); snprintf(cur->kw, sizeof cur->kw, "%s", r.n >= 4 ? r.f[3] : "");
        } else if (cur) { if (cur->nb < MAXL) cur->body[cur->nb++] = r; else { fprintf(stderr, "playthrough_op: phase %s has more than %d rows\n", cur->id, MAXL); return -1; } }
        else { fprintf(stderr, "playthrough_op: row '%s' before the first PHASE\n", v); return -1; }
    }
    fclose(f);
    if (!t->game[0] || !t->bin[0]) { fprintf(stderr, "playthrough_op: no `GAME | <folder> | <binary base>` row\n"); return -1; }
    if (!t->nph) { fprintf(stderr, "playthrough_op: no PHASE rows\n"); return -1; }
    return 0;
}

/* ---------- compile ---------- */
static FILE *OUTF; static const Table *CT; static const Phase *CP; static char CBIN[P], CAPP[P], PTOP[P], MARKS[P];
static void emit(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void emit(const char *fmt, ...) { __builtin_va_list ap; __builtin_va_start(ap, fmt); vfprintf(OUTF, fmt, ap); __builtin_va_end(ap); fputc('\n', OUTF); }
static int bad_row(const char *why, const Row *r) { fprintf(stderr, "playthrough_op: phase %s: %s (row %s)\n", CP ? CP->id : "?", why, r->f[0]); return -1; }

static int emit_body_row(const Row *r) {
    const char *v = r->f[0]; char lab[P], tx[P];
    #define NEED(k) do { if (r->n < (k)) return bad_row("too few fields", r); } while (0)
    #define LAB(i) snprintf(lab, sizeof lab, "%s.%s", CP->id, r->f[i])
    if (!strcmp(v, "DO")) {
        NEED(2); if (r->n > 13) return bad_row("DO takes at most 12 arguments", r);
        char row[P]; size_t o = (size_t)snprintf(row, sizeof row, "RUN | nice | -n | 15 | %s", CBIN);
        for (int i = 1; i < r->n; i++) o += (size_t)snprintf(row + o, sizeof row - o, " | %s", r->f[i]);
        emit("%s", row);
    } else if (!strcmp(v, "PUT")) { NEED(3); rejoin(r, 2, tx, sizeof tx); emit("WRITE | %s/%s | %s", CAPP, r->f[1], tx); }
    else if (!strcmp(v, "SAY")) { NEED(3); LAB(2); emit("EXPECT_OUT | %s | %s", r->f[1], lab); }
    else if (!strcmp(v, "NOSAY")) { NEED(3); LAB(2); emit("EXPECT_OUT_LACKS | %s | %s", r->f[1], lab); }
    else if (!strcmp(v, "SAYIS")) { NEED(3); LAB(2); emit("EXPECT_OUT_IS | %s | %s", r->f[1], lab); }
    else if (!strcmp(v, "EXIT")) { NEED(3); LAB(2); emit("EXPECT_EXIT | %s | %s", r->f[1], lab); }
    else if (!strcmp(v, "HAS")) { NEED(4); LAB(3); emit("EXPECT_HAS | %s/%s | %s | %s", CAPP, r->f[1], r->f[2], lab); }
    else if (!strcmp(v, "LACKS")) { NEED(4); LAB(3); emit("EXPECT_LACKS | %s/%s | %s | %s", CAPP, r->f[1], r->f[2], lab); }
    else if (!strcmp(v, "FILEIS")) { NEED(4); LAB(3); emit("EXPECT_FILE | %s/%s | %s | %s", CAPP, r->f[1], r->f[2], lab); }
    else if (!strcmp(v, "EXISTS")) { NEED(3); LAB(2); emit("EXPECT_EXISTS | %s/%s | %s", CAPP, r->f[1], lab); }
    else if (!strcmp(v, "NOFILE")) { NEED(3); LAB(2); emit("EXPECT_NOFILE | %s/%s | %s", CAPP, r->f[1], lab); }
    else if (!strcmp(v, "UI")) {   /* a whole line key=value of state/ui.txt (grep -x -F: independent of the file size, an empty value is allowed) */
        NEED(4); LAB(3);
        emit("RUN | grep | -c | -x | -F | %s=%s | $T/%s/state/ui.txt", r->f[1], r->f[2], CAPP); emit("EXPECT_OUT_IS | 1 | %s", lab);
    } else if (!strcmp(v, "UIKEY")) {   /* the key exists with any value */
        NEED(3); LAB(2);
        emit("RUN | grep | -c | ^%s= | $T/%s/state/ui.txt", r->f[1], CAPP); emit("EXPECT_OUT_IS | 1 | %s", lab);
    } else if (!strcmp(v, "COUNT")) {   /* number of lines of a file that start with a prefix (\\p = a bar) */
        NEED(5); LAB(4);
        emit("RUN | grep | -c | ^%s | $T/%s/%s", r->f[2], CAPP, r->f[1]); emit("EXPECT_OUT_IS | %s | %s", r->f[3], lab);
    } else if (!strcmp(v, "MARK")) {   /* var <- value of key in a key=value file */
        NEED(4);
        emit("RUN | awk | -F= | -v | k=%s | $1==k{print $2} | $T/%s/%s", r->f[3], CAPP, r->f[2]); emit("CAPTURE | %s", r->f[1]);
    } else if (!strcmp(v, "DELTA")) {   /* after - before == expected */
        NEED(5); LAB(4);
        emit("RUN | awk | BEGIN{print $%s - $%s} ", r->f[2], r->f[1]); emit("EXPECT_OUT_IS | %s | %s", r->f[3], lab);
    } else if (!strcmp(v, "CAP")) { NEED(2); emit("CAPTURE | %s", r->f[1]); }
    else if (!strcmp(v, "SUM")) { NEED(3); emit("SUMTREE | %s | %s/%s", r->f[1], CAPP, r->f[2]); }
    else if (!strcmp(v, "EQ")) { NEED(4); LAB(3); emit("EXPECT_EQ | $%s | $%s | %s", r->f[1], r->f[2], lab); }
    else if (!strcmp(v, "NE")) { NEED(4); LAB(3); emit("EXPECT_NE | $%s | $%s | %s", r->f[1], r->f[2], lab); }
    else if (!strcmp(v, "CP")) { NEED(3); emit("RUN | cp | $T/%s/%s | $T/%s/%s", CAPP, r->f[1], CAPP, r->f[2]); }
    else if (!strcmp(v, "RM")) { NEED(2); emit("RUN | rm | -f | $T/%s/%s", CAPP, r->f[1]); }
    else if (!strcmp(v, "SLEEP")) { NEED(2); emit("SLEEP | %s", r->f[1]); }
    else if (!strcmp(v, "BP")) {   /* BP | <event> | edit|append|delete|show | args...: drive one blueprint edit through the real events-hq manager (bp_edit_op); \p = a bar in the params */
        NEED(3); if (!CT->events) return bad_row("BP needs an `EVENTS | yes` row", r);
        char row[P]; size_t o = (size_t)snprintf(row, sizeof row, "RUN | $T/&.hq-apps/blueprintz/ops/+x/bp_edit_op.+x | $T | $T/%s/events/%s/event_pkg | %s | %s", CAPP, r->f[1], r->f[1], r->f[2]);
        for (int i = 3; i < r->n; i++) o += (size_t)snprintf(row + o, sizeof row - o, " | %s", r->f[i]);
        emit("%s", row);
    }
    else if (!strcmp(v, "BPWIN")) {   /* BPWIN | REQ:<request> | EXPECT:<ui key>~<text> | EXPECTNOT:<ui key>~<text> | ...: drive the blueprintz WINDOW layer (its real manager, the same request lines a click writes) in one session, so undo history is kept between steps; \p = a bar */
        NEED(2); if (!CT->events) return bad_row("BPWIN needs an `EVENTS | yes` row", r);
        char row[P]; size_t o = (size_t)snprintf(row, sizeof row, "RUN | $T/&.hq-apps/blueprintz/ops/+x/bp_win_op.+x | $T | $T/&.hq-apps/blueprintz");
        for (int i = 1; i < r->n; i++) o += (size_t)snprintf(row + o, sizeof row - o, " | %s", r->f[i]);
        emit("%s", row);
    }
    else if (!strcmp(v, "BPCE")) {   /* BPCE | <event>[@<page>] | edit|append|delete|show|new_page|trigger | args...: one edit of a COMMON event (common_events/<event>) through the real events-hq manager (bp_edit_op); \p = a bar */
        NEED(3); if (!CT->common) return bad_row("BPCE needs `EVENTS | yes` and `COMMON | yes` rows", r);
        char ev[96], pg[32] = "page_1"; snprintf(ev, sizeof ev, "%s", r->f[1]); char *at = strchr(ev, '@'); if (at) { *at = 0; snprintf(pg, sizeof pg, "%s", at + 1); }
        char row[P]; size_t o = (size_t)snprintf(row, sizeof row, "RUN | env | BP_PAGE=%s | $T/&.hq-apps/blueprintz/ops/+x/bp_edit_op.+x | $T | $T/common_events/%s/event_pkg | %s | %s", pg, ev, ev, r->f[2]);
        for (int i = 3; i < r->n; i++) o += (size_t)snprintf(row + o, sizeof row - o, " | %s", r->f[i]);
        emit("%s", row);
    }
    else if (!strcmp(v, "PLAYCE")) {   /* PLAYCE | <trigger>: play_event.sh on the caller entity <app>/ce_caller with that trigger (the way a Play does: the entity's pages, then every common event with the trigger) */
        NEED(2); if (!CT->common) return bad_row("PLAYCE needs `EVENTS | yes` and `COMMON | yes` rows", r);
        emit("RUN | bash | $T/&.widgits/events-hq/ops/play_event.sh | $T/%s/ce_caller | $T | %s", CAPP, r->f[1]);
    }
    else if (!strcmp(v, "HASROOT")) { NEED(4); LAB(3); emit("EXPECT_HAS | %s | %s | %s", r->f[1], r->f[2], lab); }      /* HASROOT | <path relative to the scratch house root> | <text> | <label> */
    else if (!strcmp(v, "MKDIRROOT")) { NEED(2); emit("MKDIR | %s", r->f[1]); }      /* MKDIRROOT | <dir relative to the scratch house root> */
    else if (!strcmp(v, "PUTROOT")) { NEED(3); rejoin(r, 2, tx, sizeof tx); emit("WRITE | %s | %s", r->f[1], tx); }      /* PUTROOT | <file relative to the scratch house root> | <one line of text> */
    else if (!strcmp(v, "NOFILEROOT")) { NEED(3); LAB(2); emit("EXPECT_NOFILE | %s | %s", r->f[1], lab); }
    else if (!strcmp(v, "NOTE")) { /* a comment that travels with the table */ }
    else return bad_row("unknown row verb", r);
    return 0;
}

static void write_seed(const Table *t, const Phase *p, const char *bankdir) {
    char path[P]; FILE *f; char verbs[8][48]; int nv = 0;
    snprintf(path, sizeof path, "%s/%s__%s.behavior.pdl", bankdir, t->game, p->id);
    if (exists(path)) return;
    for (int i = 0; i < p->nb && nv < 8; i++) if (!strcmp(p->body[i].f[0], "DO") && p->body[i].n >= 2) {
        int dup = 0; for (int k = 0; k < nv; k++) if (!strcmp(verbs[k], p->body[i].f[1])) dup = 1;
        if (!dup) snprintf(verbs[nv++], 48, "%s", p->body[i].f[1]);
    }
    mkparent(path); if (!(f = fopen(path, "w"))) return;
    fprintf(f, "# Behavior Bank entry for one PHASE of a playthrough harness, generated by playthrough_op from the phase table (seed only: never overwritten; measured counts live in bank/live/).\n");
    fprintf(f, "# Weights on SYNONYM / SLOT / CORPUS rows are guesses; only WEIGHT (reward = runs where the phase passed, punish = runs where it failed) is measured.\n");
    fprintf(f, "BEHAVIOR | play.%s.%s\nKIND | spoke\nKEYWORDS | %s, %s, playthrough, phase", t->game, p->id, t->game, p->id);
    for (int i = 0; i < nv; i++) fprintf(f, ", %s", verbs[i]);
    if (p->kw[0]) fprintf(f, ", %s", p->kw);
    fprintf(f, "\n");
    for (int i = 0; i < nv; i++) fprintf(f, "SYNONYM | %s step | 0.50\n", verbs[i]);
    fprintf(f, "SENTENCE | %s\nSENTENCE | One phase of the %s scripted playthrough (%s): real game verbs, then checks of the ledger, ui.txt and files.\n", p->title, t->game, t->name);
    fprintf(f, "SLOT | 0 | POINTS_TO=%s | WEIGHT=0.60\n", t->game);
    for (int i = 0; i < nv && i < 3; i++) fprintf(f, "SLOT | %d | POINTS_TO=%s | WEIGHT=0.50\n", i + 1, verbs[i]);
    fprintf(f, "CORPUS | game-engine | WEIGHT=0.50\nSEQUENCE | 1 | playthrough_op run %s\n", t->path);
    fprintf(f, "SOURCE | generated by playthrough_op from %s\nCOUNTS | reward=0 | punish=0 | cursor=0\nWEIGHT | 0.5000\n", t->path);
    fclose(f);
}

static int cmd_compile(const char *phases, const char *casedir_arg) {
    Table *t = calloc(1, sizeof *t); char cdir[P], out[P], rel[P];
    if (parse_table(phases, t) != 0) return 2;
    snprintf(cdir, sizeof cdir, "%s", casedir_arg ? casedir_arg : "");
    if (!cdir[0]) snprintf(cdir, sizeof cdir, "%s/play/cases", HARN);
    else if (cdir[0] != '/') { char b[P]; snprintf(b, sizeof b, "%s/%s", HARN, cdir); snprintf(cdir, sizeof cdir, "%s", b); }
    mkdirs(cdir); snprintf(out, sizeof out, "%s/play_%s.pdl", cdir, t->name);
    snprintf(PTOP, sizeof PTOP, "$HOUSE/&.widgits/_shared-lib/harness/ops/+x/playthrough_op.+x");
    { char rd[P]; snprintf(rd, sizeof rd, "%s/../results", cdir); char rr[P]; mkdirs(rd); if (realpath(rd, rr)) snprintf(MARKS, sizeof MARKS, "%s/play_%s.marks.txt", rr, t->name); else snprintf(MARKS, sizeof MARKS, "%s/play_%s.marks.txt", rd, t->name); }
    snprintf(CAPP, sizeof CAPP, "@.apps/%s", t->game); snprintf(CBIN, sizeof CBIN, "$T/@.apps/%s/ops/+x/%s.+x", t->game, t->bin);
    snprintf(rel, sizeof rel, "../results/play_%s.txt", t->name);
    if (!(OUTF = fopen(out, "w"))) { fprintf(stderr, "playthrough_op: cannot write %s\n", out); return 2; }
    CT = t; CP = NULL;
    emit("# GENERATED by ops/+x/playthrough_op.+x compile from %s - do not edit; edit the phase table.", t->path);
    emit("RESULTS | %s", rel); emit("CASE | %s", t->game); emit("SCRATCH |");
    emit("RUN | %s | mark | %s | RUN", PTOP, MARKS); emit("RUN | %s | mark | %s | setup", PTOP, MARKS);
    emit("# setup: a minimal house skeleton under $T (the game finds its house by walking up to &.widgits and #.NNEST_ASSETS), the game folder without runtime state, the text-included sources, the pet DB, lc_clock");
    emit("COPYDIR | %s | %s | state,board,audit,+x,saves,%s-pchq,.hq_manager%s", CAPP, CAPP, t->game, t->events ? "" : ",events");
    emit("MKDIR | %s/ops/+x", CAPP); emit("MKDIR | %s/state", CAPP); emit("MKDIR | #.NNEST_ASSETS/rmmv-www-img");
    emit("COPY | &.widgits/_shared-lib/stb_image.h | &.widgits/_shared-lib/stb_image.h");
    emit("COPY | &.widgits/_shared-lib/stb_image_write.h | &.widgits/_shared-lib/stb_image_write.h");
    emit("COPY | &.widgits/_shared-lib/game_events.c | &.widgits/_shared-lib/game_events.c");
    emit("COPY | &.widgits/livedesk-clock/ops/+x/lc_clock.+x | &.widgits/livedesk-clock/ops/+x/lc_clock.+x");
    emit("COPY | &.widgits/db-hq/data/system.pdl | &.widgits/db-hq/data/system.pdl");
    emit("COPY | &.widgits/db-hq/data/actors.pdl | &.widgits/db-hq/data/actors.pdl");
    if (t->events) {
        emit("# EVENTS: a scratch house that can run and edit event pages: xyzfs marker (the pages find the game by walking up to it), the real play_event.sh, the command registry, the events-hq manager, bp_edit_op, and a prisc VM in a 101.mutaclsym*/system dir (play_event.sh walks up to find one)");
        emit("MKDIR | xyzfs"); emit("MKDIR | #.ref/menu"); emit("MKDIR | 101.mutaclsym-scratch/system");
        emit("COPY | &.widgits/events-hq/ops/play_event.sh | &.widgits/events-hq/ops/play_event.sh");
        emit("COPY | #.ref/menu/event_commands.registry.pdl | #.ref/menu/event_commands.registry.pdl");
        emit("COPY | &.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x | &.widgits/events-hq/ops/+x/khtpm_events_hq_manager.+x");
        emit("COPY | &.hq-apps/blueprintz/ops/+x/bp_edit_op.+x | &.hq-apps/blueprintz/ops/+x/bp_edit_op.+x");
        emit("COPY | &.hq-apps/blueprintz/ops/+x/bp_win_op.+x | &.hq-apps/blueprintz/ops/+x/bp_win_op.+x");      /* the blueprintz window layer: its manager and the ops it forks */
        emit("COPY | &.hq-apps/blueprintz/+x/blueprintz_manager.+x | &.hq-apps/blueprintz/+x/blueprintz_manager.+x");
        emit("COPY | &.hq-apps/blueprintz/ops/+x/bp_scan.+x | &.hq-apps/blueprintz/ops/+x/bp_scan.+x");
        emit("COPY | &.hq-apps/blueprintz/ops/+x/bp_ctl.+x | &.hq-apps/blueprintz/ops/+x/bp_ctl.+x");
        { glob_t g; char pat[P], house[P]; snprintf(pat, sizeof pat, "%s/../../../101.mutaclsym*/system/prisc+x", HARN);
          if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc >= 1 && realpath(g.gl_pathv[0], house)) {      /* the directory name has emoji in it: take it from the disk, never type it */
              char *sl = strstr(house, "/101.mutaclsym"); char rel[P]; char root[P]; snprintf(root, sizeof root, "%s/../../..", HARN); char rr[P]; if (sl && realpath(root, rr) && !strncmp(house, rr, strlen(rr))) {
                  snprintf(rel, sizeof rel, "%s", house + strlen(rr) + 1); char dir[P]; snprintf(dir, sizeof dir, "%s", rel); char *ls = strrchr(dir, '/'); if (ls) *ls = 0;
                  emit("COPY | %s | 101.mutaclsym-scratch/system/prisc+x", rel); (void)dir; } }      /* no default_op.txt in the house system dir: prisc prints its usual "could not open ops file" line and the page still runs (known, same in the live house) */
          globfree(&g); }
        if (t->common) {
            emit("# COMMON: common_events/ + the call_event op (fixed 2026-10-21), its op registry in the VM's system dir, mr_change_gold, and a caller entity with its own gold");
            glob_t g2; char pat2[P], real2[P];
            snprintf(pat2, sizeof pat2, "%s/../../../101.mutaclsym*/default_op.txt", HARN);
            if (glob(pat2, 0, NULL, &g2) == 0 && g2.gl_pathc >= 1 && realpath(g2.gl_pathv[0], real2)) {
                char root2[P], rr2[P]; snprintf(root2, sizeof root2, "%s/../../..", HARN);
                if (realpath(root2, rr2) && !strncmp(real2, rr2, strlen(rr2))) {
                    char rel2[P]; snprintf(rel2, sizeof rel2, "%s", real2 + strlen(rr2) + 1); char md[P]; snprintf(md, sizeof md, "%s", rel2); char *sl2 = strrchr(md, '/'); if (sl2) *sl2 = 0;
                    emit("MKDIR | 101.mutaclsym-scratch/system/ops/+x");
                    emit("COPY | %s | 101.mutaclsym-scratch/system/default_op.txt", rel2);
                    emit("COPY | %s/ops/+x/call_event_op.+x | 101.mutaclsym-scratch/system/ops/+x/call_event_op.+x", md); } }
            globfree(&g2);
            emit("MKDIR | common_events"); emit("COPY | &.widgits/events-hq/ops/+x/mr_change_gold.+x | &.widgits/events-hq/ops/+x/mr_change_gold.+x");
            emit("MKDIR | %s/ce_caller", CAPP); emit("WRITE | %s/ce_caller/inventory.txt | qolq=35", CAPP);
        }
    }
    for (int i = 0; i < t->nneeds; i++) emit("COPY | %s | %s", t->needs[i].f[1], t->needs[i].n >= 3 ? t->needs[i].f[2] : t->needs[i].f[1]);
    emit("RUN | nice | -n | 15 | gcc | -std=gnu11 | -O0 | -w | -o | %s | $T/%s/ops/%s.c | -lm", CBIN, CAPP, t->bin);
    emit("EXPECT_EXIT | 0 | setup.build"); emit("EXPECT_EXISTS | %s/ops/+x/%s.+x | setup.binary-exists", CAPP, t->bin);
    emit("# a stand-in window: the game writes ui.txt only while a process named khtpm_core_render with <game>.xhtpm in its command line exists (a copy of tail, no GUI)");
    emit("RUN | cp | /usr/bin/tail | $T/khtpm_core_render"); emit("SPAWN | viewer | $T/khtpm_core_render | -f | /dev/null | %s.xhtpm", t->game); emit("EXPECT_ALIVE | $viewer | setup.viewer-alive");
    for (int i = 0; i < t->ninit; i++) { char tx[P]; rejoin(&t->init[i], 2, tx, sizeof tx); emit("WRITE | %s/%s | %s", CAPP, t->init[i].f[1], tx); }
    for (int p = 0; p < t->nph; p++) {
        const Phase *ph = &t->ph[p]; int checks = 0; CP = ph;
        emit("# ---- phase %s: %s", ph->id, ph->title); emit("RUN | %s | mark | %s | %s", PTOP, MARKS, ph->id);
        for (int i = 0; i < ph->nb; i++) {
            const char *v = ph->body[i].f[0];
            if (strncmp(v, "SAY", 3) == 0 || strcmp(v, "NOSAY") == 0 || !strcmp(v, "EXIT") || !strcmp(v, "HAS") || !strcmp(v, "LACKS") || !strcmp(v, "FILEIS") || !strcmp(v, "EXISTS") || !strcmp(v, "NOFILE") || !strncmp(v, "UI", 2) || !strcmp(v, "COUNT") || !strcmp(v, "DELTA") || !strcmp(v, "EQ") || !strcmp(v, "NE")) checks++;
            if (emit_body_row(&ph->body[i]) != 0) { fclose(OUTF); remove(out); return 2; }
        }
        if (!checks) { fprintf(stderr, "playthrough_op: phase %s has no expectation row (a phase that checks nothing is refused)\n", ph->id); fclose(OUTF); remove(out); return 2; }
    }
    emit("RUN | %s | mark | %s | END", PTOP, MARKS); emit("CLEAN |"); fclose(OUTF);
    /* per-phase mini cases + seeds (play/cases, play/bank) */
    {
        char mc[P], bk[P]; snprintf(mc, sizeof mc, "%s/play/cases", HARN); snprintf(bk, sizeof bk, "%s/play/bank", HARN); mkdirs(mc); mkdirs(bk);
        for (int p = 0; p < t->nph; p++) {
            char path[P]; snprintf(path, sizeof path, "%s/%s__%s.pdl", mc, t->game, t->ph[p].id);
            if (!exists(path)) { FILE *f = fopen(path, "w"); if (f) { fprintf(f, "# generated by playthrough_op: the result ledger of one phase (PASS/FAIL per run), the file harness_bank_op update reads\nRESULTS | ../results/ph/%s__%s.txt\n", t->game, t->ph[p].id); fclose(f); } }
            write_seed(t, &t->ph[p], bk);
        }
    }
    printf("compiled %s: %d phases -> %s\n", phases, t->nph, out);
    return 0;
}

/* ---------- children ---------- */
static int spawn_wait(char *const argv[], char *out, size_t n) {   /* fork/exec, capture stdout+stderr into out (or pass through when out == NULL); returns exit code */
    int pfd[2] = {-1, -1}, st = 0; pid_t pid; size_t k = 0;
    if (out && pipe(pfd) != 0) return 127;
    fflush(stdout);
    if ((pid = fork()) == 0) { if (out) { dup2(pfd[1], 1); dup2(pfd[1], 2); close(pfd[0]); close(pfd[1]); } execv(argv[0], argv); _exit(127); }
    if (out) { ssize_t r; close(pfd[1]); while (k < n - 1 && (r = read(pfd[0], out + k, n - 1 - k)) > 0) k += (size_t)r; out[k] = 0; close(pfd[0]); }
    waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}
static void sibling(const char *name, char *out, size_t n) { snprintf(out, n, "%s/ops/+x/%s", HARN, name); }

/* ---------- ledger ---------- */
typedef struct { char id[64]; int pass, fail; char firstfail[256]; long long t0, t1; } PS;
static int cmd_ledger(const char *phases, const char *casedir_arg, int quiet) {
    Table *t = calloc(1, sizeof *t); char cdir[P], resp[P], marksp[P], line[8192], ledger[P], rid_s[32] = "0"; FILE *f; PS ps[MAXPH + 1]; int nps = 0; long long rid = 0;
    if (parse_table(phases, t) != 0) return 2;
    snprintf(cdir, sizeof cdir, "%s", casedir_arg && casedir_arg[0] ? casedir_arg : "play/cases");
    if (cdir[0] != '/') { char b[P]; snprintf(b, sizeof b, "%s/%s", HARN, cdir); snprintf(cdir, sizeof cdir, "%s", b); }
    snprintf(resp, sizeof resp, "%s/../results/play_%s.txt", cdir, t->name); snprintf(marksp, sizeof marksp, "%s/../results/play_%s.marks.txt", cdir, t->name);
    snprintf(ledger, sizeof ledger, "%s/play/results/play_ledger.txt", HARN);
    memset(ps, 0, sizeof ps); snprintf(ps[0].id, sizeof ps[0].id, "setup"); nps = 1;
    for (int i = 0; i < t->nph; i++) snprintf(ps[nps++].id, sizeof ps[0].id, "%s", t->ph[i].id);
    if (!(f = fopen(resp, "r"))) { fprintf(stderr, "playthrough_op: no results ledger %s (run harness_case_op first)\n", resp); return 2; }
    long run_off = 0, off = 0; while (fgets(line, sizeof line, f)) { if (!strncmp(line, "RUN|", 4)) { run_off = off; rid = atoll(line + 4); } off = ftell(f); }
    fseek(f, run_off, SEEK_SET); snprintf(rid_s, sizeof rid_s, "%lld", rid);
    while (fgets(line, sizeof line, f)) {
        int ok = !strncmp(line, "PASS|", 5); if (!ok && strncmp(line, "FAIL|", 5)) continue;
        line[strcspn(line, "\r\n")] = 0; char *lab = strchr(line, '/'); char ph[64] = "setup";
        if (lab) { lab++; char *dot = strchr(lab, '.'); if (dot) { size_t l = (size_t)(dot - lab); if (l < sizeof ph) { memcpy(ph, lab, l); ph[l] = 0; } } }
        int idx = 0; for (int i = 0; i < nps; i++) if (!strcmp(ps[i].id, ph)) idx = i;
        if (ok) ps[idx].pass++; else { ps[idx].fail++; if (!ps[idx].firstfail[0]) snprintf(ps[idx].firstfail, sizeof ps[idx].firstfail, "%s", lab ? lab : line); }
    }
    fclose(f);
    /* marks after the last RUN mark: time of each phase = next mark - its own */
    {
        long long mt[MAXPH + 4]; char ml[MAXPH + 4][64]; int nm = 0;
        if ((f = fopen(marksp, "r"))) {
            long long tms[4096]; char lbl[4096][64]; int n = 0, start = 0;
            while (n < 4096 && fgets(line, sizeof line, f)) { char *bar = strchr(line, '|'); if (!bar) continue; *bar = 0; tms[n] = atoll(line); snprintf(lbl[n], 64, "%s", trim(bar + 1)); if (!strcmp(lbl[n], "RUN")) start = n; n++; }
            fclose(f);
            for (int i = start; i < n && nm < MAXPH + 3; i++) { mt[nm] = tms[i]; snprintf(ml[nm], 64, "%s", lbl[i]); nm++; }
            for (int i = 0; i < nm; i++) for (int k = 0; k < nps; k++) if (!strcmp(ml[i], ps[k].id)) { ps[k].t0 = mt[i]; ps[k].t1 = i + 1 < nm ? mt[i + 1] : mt[i]; }
        }
    }
    mkparent(ledger);
    {   /* idempotent: the same run id and name are written once */
        int seen = 0; if ((f = fopen(ledger, "r"))) { char key[256]; snprintf(key, sizeof key, "PLAY|%s|%s|%s|", rid_s, t->game, t->name); while (fgets(line, sizeof line, f)) if (!strncmp(line, key, strlen(key))) seen = 1; fclose(f); }
        if (seen) { if (!quiet) printf("ledger: run %s of %s already recorded, nothing written\n", rid_s, t->name); return 0; }
    }
    int failed_phases = 0; long long total = 0;
    f = fopen(ledger, "a"); if (!f) return 2;
    for (int i = 0; i < nps; i++) {
        long long ms = ps[i].t1 > ps[i].t0 ? ps[i].t1 - ps[i].t0 : 0; const char *vd = ps[i].fail ? "FAIL" : ps[i].pass ? "PASS" : "NOCHECKS"; total += ms; if (ps[i].fail) failed_phases++;
        fprintf(f, "PH|%s|%s|%s|%s|%s|%lld|pass=%d|fail=%d|%s\n", rid_s, t->game, t->name, ps[i].id, vd, ms, ps[i].pass, ps[i].fail, ps[i].firstfail[0] ? ps[i].firstfail : "-");
    }
    fprintf(f, "PLAY|%s|%s|%s|%s|%lld|phases=%d|failed=%d\n", rid_s, t->game, t->name, failed_phases ? "FAIL" : "PASS", total, nps, failed_phases);
    fclose(f);
    if (!quiet) printf("PLAYTHROUGH|%s|%s|%s|%lld ms|phases=%d|failed=%d\n", t->name, t->game, failed_phases ? "FAIL" : "PASS", total, nps, failed_phases);
    /* per-phase result file (one PASS/FAIL per run) and the bank update: this is what turns a playthrough into weights */
    for (int i = 1; i < nps; i++) {
        char rf[P], mc[P]; FILE *g; snprintf(rf, sizeof rf, "%s/play/results/ph/%s__%s.txt", HARN, t->game, ps[i].id); snprintf(mc, sizeof mc, "%s/play/cases/%s__%s.pdl", HARN, t->game, ps[i].id);
        if (!ps[i].pass && !ps[i].fail) { if (!quiet) printf("  %-22s NOCHECKS (never ran; no weight change)\n", ps[i].id); continue; }
        mkparent(rf); if ((g = fopen(rf, "a"))) {
            fprintf(g, "RUN|%s|%s\n", rid_s, t->name);
            if (ps[i].fail) fprintf(g, "FAIL|%s/%s|%s\n", t->game, ps[i].id, ps[i].firstfail); else fprintf(g, "PASS|%s/%s\n", t->game, ps[i].id);
            fclose(g);
        }
        char bank[P], res[512] = ""; sibling("harness_bank_op.+x", bank, sizeof bank);
        char *av1[] = { bank, mc, NULL };
        spawn_wait(av1, res, sizeof res); res[strcspn(res, "\r\n")] = 0;
        if (!quiet) printf("  %-22s %-4s %5lld ms  %s\n", ps[i].id, ps[i].fail ? "FAIL" : "PASS", ps[i].t1 > ps[i].t0 ? ps[i].t1 - ps[i].t0 : 0, res);
    }
    if (!quiet) printf("  %-22s %-4s (build + scratch house + viewer)\n", "setup", ps[0].fail ? "FAIL" : "PASS");
    return 0;
}

static int cmd_run(const char *phases, const char *casedir_arg) {
    char cd[P], op[P], cpath[P], out[8192]; Table *t = calloc(1, sizeof *t);
    if (parse_table(phases, t) != 0) return 2;
    snprintf(cd, sizeof cd, "%s", casedir_arg && casedir_arg[0] ? casedir_arg : "play/cases");
    { char abs[P]; if (cd[0] != '/') snprintf(abs, sizeof abs, "%s/%s", HARN, cd); else snprintf(abs, sizeof abs, "%s", cd); snprintf(cpath, sizeof cpath, "%s/play_%s.pdl", abs, t->name); }
    { char me[P]; sibling("playthrough_op.+x", me, sizeof me); char *av[] = { me, (char *)"compile", (char *)phases, cd, NULL }; if (spawn_wait(av, NULL, 0) != 0) return 2; }
    sibling("harness_case_op.+x", op, sizeof op);   { char *av[] = { op, cpath, NULL }; spawn_wait(av, out, sizeof out); }
    sibling("harness_verdict_op.+x", op, sizeof op); { char *av[] = { op, cpath, NULL }; spawn_wait(av, out, sizeof out); }
    cmd_ledger(phases, cd, 0);
    /* no whole-run bank entry for a proposal (the shipped pals update their own seed); every phase has an entry, updated by the ledger step */
    {   /* exit code from the verdict file harness_verdict_op wrote */
        char vp[P], l[1024], dd[P]; FILE *f; snprintf(dd, sizeof dd, "%s", cpath); { char *d = dirname(dd); snprintf(vp, sizeof vp, "%s/../results/play_%s.txt.verdict.txt", d, t->name); }
        if ((f = fopen(vp, "r")) && fgets(l, sizeof l, f)) { fclose(f); return strncmp(l, "VERDICT|PASS", 12) == 0 ? 0 : 1; }
        return 1;
    }
}

int main(int argc, char **argv) {
    char exe[P];
    { ssize_t k = readlink("/proc/self/exe", exe, sizeof exe - 1); if (k < 0) return 2; exe[k] = 0; }
    for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (s) *s = 0; }     /* .../harness/ops/+x/playthrough_op.+x -> .../harness */
    snprintf(HARN, sizeof HARN, "%s", exe);
    if (argc >= 4 && !strcmp(argv[1], "mark")) {
        FILE *f; mkparent(argv[2]); if (!(f = fopen(argv[2], "a"))) return 1;
        fprintf(f, "%lld|%s\n", now_ms(), argv[3]); fclose(f); return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "compile")) return cmd_compile(argv[2], argc > 3 ? argv[3] : NULL);
    if (argc >= 3 && !strcmp(argv[1], "ledger")) return cmd_ledger(argv[2], argc > 3 ? argv[3] : NULL, 0);
    if (argc >= 3 && !strcmp(argv[1], "run")) return cmd_run(argv[2], argc > 3 ? argv[3] : NULL);
    fprintf(stderr, "usage: playthrough_op compile|ledger|run <phases.pdl> [case_dir]   |   mark <file> <label>\n");
    return 2;
}
