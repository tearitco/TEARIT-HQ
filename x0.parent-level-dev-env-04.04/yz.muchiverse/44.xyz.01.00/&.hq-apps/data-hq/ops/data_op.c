/* data_op - the DATASET LAYER of data-hq (phase P0, 2026-11-09; BOOK 08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md section 3 and 8).
 * No window yet. This op finds tables, works out what their columns are, shows them, summarises them, runs a game's exporter and builds the cross-game index. It only READS game data:
 * the only things it ever writes are a schema/index file you name with --out and (through the exporter it runs) the out folder you name.
 *
 * Usage:  data_op <verb> ...
 *   list <root...> [--registry f.pdl]... [--house dir]   one FOUND row per dataset: FOUND | path | kind | rows | cols | bytes | class | why | provider   (kind csv tsv pdl ledger ledgerdir; class public|private)
 *   sniff <file> [--out schema.pdl] [--against expected.pdl]   print (or write) TABLE + COL | name | role | unit | hint rows; --against compares ROLES with a schema (exit 1 on a difference)
 *   check <file> <schema.pdl>                             the table matches its schema: same columns in order, measure columns numeric, time columns numeric or dates (exit 1 + FAIL rows)
 *   show <file> [--schema s.pdl] [--head N] [--where col=val]... [--cols a,b]   rows as `a | b | c` (--where ops: = != < > <= >=, AND of all)
 *   stats <file> [col]                                    STAT | col | n | min | p05 | q25 | median | q75 | p95 | max | mean | distinct   (numeric columns; others: n and distinct only)
 *   pick <file> <outcol> --where col=val ...              the one value of outcol in the one matching row (exit 1 unless exactly one row matches)
 *   provider <app_dir> <state_dir> <out_dir> [--house d]   run the app's `GAME | export | <program> <args>` row from <app_dir>/game.pdl ({house} {book} {state} {out}); exit 3 = the app has no export row
 *   index <out_dir...> [--out index.csv]                  one row per game from each exporter out dir's game.csv, sorted by id (a projection: delete it and rebuild)
 * Formats: csv/tsv (RFC 4180 quotes), `.pdl` pipe tables (header line + a dashed line), pipe ledgers (`EVENT|round|actor|verb|target|at|result`, no header), a folder holding ledger.txt.
 * Caps (refused with the reason, exit 2): file 8 MiB, 200000 rows, 64 columns; list: 2000 datasets, depth 8; provider: 60 s, 64 KiB of output.
 * Class (DATA, not code): a datasets.pdl registry row `DATASET | id | path | kind | class | provider | note` wins (longest path prefix; paths relative to the house root); else a path under xyzfs/users is private; else a ledger folder
 * with a human seat is private and without one is public; else a folder with an exporter game.csv follows its human_ever column; else PRIVATE (fail closed: only a positive reason makes a dataset public).
 * Exit: 0 ok | 1 a check failed | 2 usage, bad data or a refusal | 3 provider: no export row.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/data_op.+x ops/data_op.c -lm   (one line in &.widgits/_shared-lib/harness/ops/build_harness_ops.sh) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_BYTES (8L * 1024 * 1024)
#define MAX_ROWS 200000
#define MAX_COLS 64
#define MAX_FOUND 2000
#define MAX_DEPTH 8
#define MAX_VISIT 200000
#define PROV_SECS 60
#define PROV_OUT 65536

static void die(int code, const char *fmt, ...) __attribute__((format(printf, 2, 3), noreturn));
static void die(int code, const char *fmt, ...) { va_list a; va_start(a, fmt); fputs("data_op: ", stderr); vfprintf(stderr, fmt, a); fputc('\n', stderr); va_end(a); exit(code); }

/* ---------------------------------------------------------------- tables */
typedef struct { char *buf; char **cell; char **name; int nrows, ncols, header, ragged; char delim; char kind[12]; long bytes; } Tab;

static char *slurp(const char *path, long *len, char *err, size_t ecap) {
    struct stat sb; if (stat(path, &sb) || !S_ISREG(sb.st_mode)) { snprintf(err, ecap, "%s is not a readable file", path); return NULL; }
    if (sb.st_size > MAX_BYTES) { snprintf(err, ecap, "refused: %s is %ld bytes, the cap is %ld", path, (long)sb.st_size, MAX_BYTES); return NULL; }
    FILE *f = fopen(path, "rb"); if (!f) { snprintf(err, ecap, "cannot open %s", path); return NULL; }
    char *b = malloc((size_t)sb.st_size + 2); if (!b) die(2, "out of memory");
    size_t n = fread(b, 1, (size_t)sb.st_size, f); fclose(f); b[n] = 0; *len = (long)n; return b;
}
static char *trims(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int is_num(const char *s, double *v) {
    if (!*s) return 0; char c = s[0]; if (!(isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.')) return 0;
    for (const char *p = s; *p; p++) if (!(isdigit((unsigned char)*p) || *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E')) return 0;
    char *e; double d = strtod(s, &e); if (*e || e == s) return 0; if (v) *v = d; return 1;
}
static int is_int(const char *s) { if (*s == '-' || *s == '+') s++; if (!*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }
static int is_date(const char *s) {
    if (!(isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && isdigit((unsigned char)s[2]) && isdigit((unsigned char)s[3]) && s[4] == '-' && isdigit((unsigned char)s[5]) && isdigit((unsigned char)s[6]) && s[7] == '-' && isdigit((unsigned char)s[8]) && isdigit((unsigned char)s[9]))) return 0;
    return s[10] == 0 || s[10] == 'T' || s[10] == ' ';
}
typedef struct { char **c; int *len; int n, cap, ncell, ccap; } Rows;   /* every row's cells, one after the other */
static void rows_cell(Rows *r, char *s) { if (r->ncell >= r->ccap) { r->ccap = r->ccap ? r->ccap * 2 : 4096; r->c = realloc(r->c, sizeof(char *) * (size_t)r->ccap); if (!r->c) die(2, "out of memory"); } r->c[r->ncell++] = s; }
static void rows_end(Rows *r, int len) { if (r->n >= MAX_ROWS + 2) die(2, "refused: more than %d rows", MAX_ROWS); if (r->n >= r->cap) { r->cap = r->cap ? r->cap * 2 : 1024; r->len = realloc(r->len, sizeof(int) * (size_t)r->cap); if (!r->len) die(2, "out of memory"); } r->len[r->n++] = len; }

/* RFC 4180 style, in place (a field never grows, so the write pointer stays behind the read pointer) */
static void parse_delim(char *buf, char d, Rows *R) {
    char *r = buf, *w = buf;
    while (*r) {
        if (*r == '\n' || *r == '\r') { r++; continue; }   /* blank line */
        int len = 0;
        for (;;) {
            char *fs = w; int quoted = 0;
            if (*r == '"') { quoted = 1; r++; for (;;) { if (!*r) break; if (*r == '"') { if (r[1] == '"') { *w++ = '"'; r += 2; continue; } r++; break; } *w++ = *r++; } }
            while (*r && *r != d && *r != '\n') { if (*r == '\r' && (r[1] == '\n' || !r[1])) { r++; continue; } if (!quoted || *r != '"') *w++ = *r; r++; }
            char c = *r; if (c) r++; *w++ = 0; rows_cell(R, fs); len++;
            if (c != d) break;
        }
        rows_end(R, len);
    }
}
/* pipe tables: '#' lines are comments, a line of dashes marks the line before it as the header, fields are trimmed */
static void parse_pipe(char *buf, Rows *R, int *header_row) {
    *header_row = -1; char *save = NULL;
    for (char *ln = strtok_r(buf, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = trims(ln); if (!*t || *t == '#') continue;
        int dash = strlen(t) >= 3; for (char *p = t; *p && dash; p++) if (*p != '-') dash = 0;
        if (dash) { if (*header_row < 0 && R->n > 0) *header_row = R->n - 1; continue; }
        int len = 0; for (char *p = t;;) { char *bar = strchr(p, '|'); if (bar) *bar = 0; rows_cell(R, trims(p)); len++; if (!bar) break; p = bar + 1; }
        rows_end(R, len);
    }
}
static int row_is_header(char **cells, int n) {   /* a csv header: every cell non-empty, non-numeric, unique */
    for (int i = 0; i < n; i++) { if (!cells[i][0] || is_num(cells[i], NULL)) return 0; for (int j = 0; j < i; j++) if (!strcmp(cells[i], cells[j])) return 0; }
    return n > 0;
}
static void tab_free(Tab *t) { free(t->buf); free(t->cell); free(t->name); memset(t, 0, sizeof *t); }
/* kind: "" = decide from the file name and content */
static int load_table(const char *path, Tab *t, char *err, size_t ecap) {
    memset(t, 0, sizeof *t); long len; char *b = slurp(path, &len, err, ecap); if (!b) return -1; t->buf = b; t->bytes = len;
    const char *ext = strrchr(path, '.'); const char *base = strrchr(path, '/'); base = base ? base + 1 : path; Rows R = {0}; int hr = -1; int pipe = 0;
    if (ext && !strcasecmp(ext, ".csv")) { t->delim = ','; snprintf(t->kind, sizeof t->kind, "csv"); }
    else if (ext && !strcasecmp(ext, ".tsv")) { t->delim = '\t'; snprintf(t->kind, sizeof t->kind, "tsv"); }
    else { pipe = 1; t->delim = '|'; }
    if (!pipe) parse_delim(b, t->delim, &R);
    else {
        parse_pipe(b, &R, &hr);
        if (hr >= 0) snprintf(t->kind, sizeof t->kind, "pdl");
        else if (R.n > 0 && !strcmp(R.c[0], "EVENT")) snprintf(t->kind, sizeof t->kind, "ledger");
        else if (ext && !strcasecmp(ext, ".pdl") && R.n > 0) snprintf(t->kind, sizeof t->kind, "pdl");
        else { snprintf(err, ecap, "%s: not a table (no csv/tsv extension, no dashed header line, no EVENT rows)", path); tab_free(t); free(R.c); free(R.len); return -1; }
    }
    if (R.n == 0) { snprintf(err, ecap, "%s: no rows", path); tab_free(t); free(R.c); free(R.len); return -1; }
    if (R.n > MAX_ROWS + 1) { snprintf(err, ecap, "refused: %s has more than %d rows", path, MAX_ROWS); tab_free(t); free(R.c); free(R.len); return -1; }
    int off0 = 0; int first = 0; (void)base;
    if (!pipe) { hr = row_is_header(R.c, R.len[0]) ? 0 : -1; }
    t->ncols = hr >= 0 ? R.len[hr] : R.len[0];
    if (t->ncols > MAX_COLS) { snprintf(err, ecap, "refused: %s has %d columns, the cap is %d", path, t->ncols, MAX_COLS); tab_free(t); free(R.c); free(R.len); return -1; }
    t->header = hr >= 0; t->name = calloc((size_t)t->ncols, sizeof(char *));
    int *start = malloc(sizeof(int) * (size_t)(R.n + 1)); for (int i = 0; i < R.n; i++) { start[i] = off0; off0 += R.len[i]; }
    static char gen[MAX_COLS][12]; static char nm_kind[] = "kind";
    for (int c = 0; c < t->ncols; c++) {
        if (hr >= 0 && c < R.len[hr]) t->name[c] = R.c[start[hr] + c];
        else if (!strcmp(t->kind, "ledger") && t->ncols == 7) { static const char *lg[7] = { "kind", "round", "actor", "verb", "target", "at", "result" }; t->name[c] = (char *)lg[c]; }
        else { snprintf(gen[c], sizeof gen[c], "c%d", c + 1); t->name[c] = gen[c]; }
    }
    (void)nm_kind;
    /* a pipe table without header whose first column is one repeated word (EVENT, GAME): call it kind */
    int nr = R.n - (hr >= 0 ? 1 : 0);
    t->cell = calloc((size_t)(nr > 0 ? nr : 1) * (size_t)t->ncols, sizeof(char *)); static char empty[] = ""; int o = 0;
    for (int i = 0; i < R.n; i++) {
        if (i == hr) continue; if (pipe && hr >= 0 && i < hr) { nr--; continue; }
        for (int c = 0; c < t->ncols; c++) t->cell[(size_t)o * (size_t)t->ncols + (size_t)c] = c < R.len[i] ? R.c[start[i] + c] : empty;
        if (R.len[i] != t->ncols) t->ragged++; o++;
    }
    (void)first; t->nrows = o; free(start); free(R.c); free(R.len); return 0;
}
static const char *cellat(const Tab *t, int r, int c) { return t->cell[(size_t)r * (size_t)t->ncols + (size_t)c]; }
static int col_index(const Tab *t, const char *name) { for (int c = 0; c < t->ncols; c++) if (!strcmp(t->name[c], name)) return c; return -1; }
static void fmtnum(double x, char *o, size_t cap) { if (x == 0) snprintf(o, cap, "0"); else if (x == (double)(long long)x && fabs(x) < 1e15) snprintf(o, cap, "%.0f", x); else snprintf(o, cap, "%.6g", x); }

/* ---------------------------------------------------------------- schema (TABLE / COL rows) */
typedef struct { char name[48], role[12], unit[24], hint[160]; } SCol;
typedef struct { SCol c[MAX_COLS]; int n; } Schema;
static int has_suffix(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !strcasecmp(s + a - b, suf); }
static const char *base_of(const char *p) { const char *b = strrchr(p, '/'); return b ? b + 1 : p; }
/* all COL rows if the file has no TABLE rows, else the COL rows under the TABLE named `table` */
static int load_schema(const char *path, const char *table, Schema *s, char *err, size_t ecap) {
    long len; char *b = slurp(path, &len, err, ecap); if (!b) return -1; s->n = 0; int any_table = 0, in = 1; char *save = NULL;
    for (char *ln = strtok_r(b, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        char *t = trims(ln); if (!*t || *t == '#') continue; char *f[6] = {0}; int n = 0; for (char *p = t; n < 6;) { char *bar = strchr(p, '|'); if (bar && n < 5) *bar = 0; f[n++] = trims(p); if (!bar || n >= 6) break; p = bar + 1; }
        if (!strcmp(f[0], "TABLE") && n >= 2) { any_table = 1; in = !strcmp(f[1], table); continue; }
        if (!strcmp(f[0], "COL") && n >= 3 && in) { if (s->n >= MAX_COLS) break; SCol *c = &s->c[s->n++]; memset(c, 0, sizeof *c); snprintf(c->name, sizeof c->name, "%s", f[1]); snprintf(c->role, sizeof c->role, "%s", f[2]); snprintf(c->unit, sizeof c->unit, "%s", n > 3 ? f[3] : ""); snprintf(c->hint, sizeof c->hint, "%s", n > 4 ? f[4] : ""); }
    }
    free(b); if (any_table && s->n == 0) { snprintf(err, ecap, "%s has no TABLE section for %s", path, table); return -1; } return 0;
}

/* ---------------------------------------------------------------- sniffing */
typedef struct { int n, nempty, numeric, integer, ascending, date, distinct, unique; double mn, mx, sumlen; } ColInfo;
static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void analyze(const Tab *t, int c, ColInfo *ci) {
    memset(ci, 0, sizeof *ci); ci->numeric = 1; ci->integer = 1; ci->ascending = 1; ci->date = 1; ci->mn = INFINITY; ci->mx = -INFINITY; double prev = -INFINITY;
    char **v = malloc(sizeof(char *) * (size_t)(t->nrows ? t->nrows : 1)); int nv = 0;
    for (int r = 0; r < t->nrows; r++) {
        const char *s = cellat(t, r, c); ci->n++; if (!*s) { ci->nempty++; continue; } v[nv++] = (char *)s; ci->sumlen += (double)strlen(s);
        double d; if (is_num(s, &d)) { if (!is_int(s)) ci->integer = 0; if (d < ci->mn) ci->mn = d; if (d > ci->mx) ci->mx = d; if (d < prev) ci->ascending = 0; prev = d; } else { ci->numeric = 0; ci->integer = 0; ci->ascending = 0; }
        if (!is_date(s)) ci->date = 0;
    }
    if (nv == 0) { ci->numeric = ci->integer = ci->ascending = ci->date = 0; }
    qsort(v, (size_t)nv, sizeof(char *), cmp_str); for (int i = 0; i < nv; i++) if (i == 0 || strcmp(v[i], v[i - 1])) ci->distinct++;
    ci->unique = nv > 0 && ci->distinct == nv; free(v); ci->nempty = ci->n - nv;
}
static int name_in(const char *nm, const char *const *list) { for (; *list; list++) if (!strcasecmp(nm, *list)) return 1; return 0; }
static void role_of(const char *nm, const ColInfo *ci, char *role, size_t rc, char *unit, size_t uc, char *hint, size_t hc) {
    static const char *const timeh[] = { "t", "time", "timestamp", "ts", "tick", "step", "turn", "round", "seq", "frame", "date", "day", "hour", "minute", "second", "epoch", "index", "row", "generation", "gen", "iteration", "year", "month", NULL };
    int nv = ci->n - ci->nempty; const char *r;
    if (nv == 0) r = "text";
    else if (!strcasecmp(nm, "id") || has_suffix(nm, "_id") || has_suffix(nm, "uid") || has_suffix(nm, "hash") || has_suffix(nm, "_key") || has_suffix(nm, "seed") || has_suffix(nm, "uuid")) r = "id";
    else if (ci->date) r = "time";
    else if (ci->numeric) {
        if (ci->integer && ci->ascending && name_in(nm, timeh)) r = "time";
        else if (ci->integer && ci->ascending && ci->unique && nv >= 3) r = "id";   /* the plan says ascending integers are time; without a time-like name they are as likely a row id, so P0 says id */
        else r = "measure";
    } else {
        double avg = ci->sumlen / nv;
        if (avg <= 24 && (ci->distinct <= 32 || ci->distinct * 3 <= nv) && !(ci->unique && nv >= 3 && ci->distinct > 32)) r = "category";
        else if (ci->unique && nv >= 3 && avg <= 40) r = "id";
        else r = "text";
    }
    snprintf(role, rc, "%s", r); unit[0] = 0;
    if (!strcmp(r, "measure")) {
        if (strcasestr(nm, "cash") || strcasestr(nm, "price") || strcasestr(nm, "worth") || strcasestr(nm, "rent") || strcasestr(nm, "cost") || strcasestr(nm, "income")) snprintf(unit, uc, "$");
        else if (strcasestr(nm, "prob")) snprintf(unit, uc, "p"); else if (strcasestr(nm, "pct") || strcasestr(nm, "percent")) snprintf(unit, uc, "%%");
    }
    char a[32], b[32]; if (ci->numeric && nv) { fmtnum(ci->mn, a, sizeof a); fmtnum(ci->mx, b, sizeof b); snprintf(hint, hc, "n=%d distinct=%d min=%s max=%s", nv, ci->distinct, a, b); } else snprintf(hint, hc, "n=%d distinct=%d", nv, ci->distinct);
}
static void sniff_text(const Tab *t, const char *path, char *out, size_t cap) {
    size_t o = 0; o += (size_t)snprintf(out + o, cap - o, "# sniff of %s: kind=%s header=%s rows=%d cols=%d%s\nTABLE | %s | sniffed (roles are guesses; a schema.pdl beside the data wins)\n", base_of(path), t->kind, t->header ? "yes" : "no", t->nrows, t->ncols, t->ragged ? " ragged-rows-padded" : "", base_of(path));
    for (int c = 0; c < t->ncols; c++) { ColInfo ci; analyze(t, c, &ci); char role[12], unit[24], hint[160]; role_of(t->name[c], &ci, role, sizeof role, unit, sizeof unit, hint, sizeof hint); o += (size_t)snprintf(out + o, cap - o, "COL | %s | %s | %s | %s\n", t->name[c], role, unit, hint); }
}
static int cmd_sniff(int argc, char **argv) {
    const char *file = NULL, *outf = NULL, *against = NULL;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--out") && i + 1 < argc) outf = argv[++i]; else if (!strcmp(argv[i], "--against") && i + 1 < argc) against = argv[++i]; else if (!file) file = argv[i]; else die(2, "sniff: unexpected %s", argv[i]); }
    if (!file) die(2, "usage: data_op sniff <file> [--out schema.pdl] [--against expected.pdl]");
    Tab t; char err[300]; if (load_table(file, &t, err, sizeof err)) die(2, "%s", err);
    if (against) {
        Schema s; if (load_schema(against, base_of(file), &s, err, sizeof err)) die(2, "%s", err); int bad = 0;
        if (s.n != t.ncols) { printf("DIFF columns: sniffed %d, schema %d\n", t.ncols, s.n); bad++; }
        for (int c = 0; c < t.ncols && c < s.n; c++) { ColInfo ci; analyze(&t, c, &ci); char role[12], unit[24], hint[160]; role_of(t.name[c], &ci, role, sizeof role, unit, sizeof unit, hint, sizeof hint);
            if (strcmp(t.name[c], s.c[c].name)) { printf("DIFF column %d: name %s vs %s\n", c + 1, t.name[c], s.c[c].name); bad++; } else if (strcmp(role, s.c[c].role)) { printf("DIFF %s: sniffed %s, schema says %s\n", t.name[c], role, s.c[c].role); bad++; } }
        if (bad) return 1; printf("ROLES ok table=%s cols=%d\n", base_of(file), t.ncols); return 0;
    }
    static char buf[MAX_COLS * 300 + 600]; sniff_text(&t, file, buf, sizeof buf);
    if (outf) { char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", outf); FILE *f = fopen(tmp, "w"); if (!f) die(2, "cannot write %s", outf); fputs(buf, f); fclose(f); if (rename(tmp, outf)) die(2, "cannot rename into %s", outf); printf("SNIFF wrote %s cols=%d\n", outf, t.ncols); }
    else fputs(buf, stdout);
    return 0;
}
static int cmd_check(int argc, char **argv) {
    if (argc != 2) die(2, "usage: data_op check <file> <schema.pdl>");
    Tab t; Schema s; char err[300]; if (load_table(argv[0], &t, err, sizeof err)) die(2, "%s", err); if (load_schema(argv[1], base_of(argv[0]), &s, err, sizeof err)) die(2, "%s", err); int bad = 0;
    if (!t.header) { printf("FAIL %s has no header row\n", base_of(argv[0])); bad++; }
    if (s.n != t.ncols) { printf("FAIL columns: the table has %d, the schema %d\n", t.ncols, s.n); bad++; }
    for (int c = 0; c < t.ncols && c < s.n; c++) {
        if (strcmp(t.name[c], s.c[c].name)) { printf("FAIL column %d is %s, the schema says %s\n", c + 1, t.name[c], s.c[c].name); bad++; continue; }
        int isnum = !strcmp(s.c[c].role, "measure"), istime = !strcmp(s.c[c].role, "time");
        if (isnum || istime) for (int r = 0; r < t.nrows; r++) { const char *v = cellat(&t, r, c); if (*v && !is_num(v, NULL) && !(istime && is_date(v))) { printf("FAIL %s row %d: \"%s\" is not %s\n", t.name[c], r + 1, v, isnum ? "a number" : "a number or a date"); bad++; break; } }
    }
    if (t.ragged) { printf("FAIL %d ragged rows\n", t.ragged); bad++; }
    if (bad) return 1; printf("CHECK ok table=%s cols=%d rows=%d\n", base_of(argv[0]), t.ncols, t.nrows); return 0;
}

/* ---------------------------------------------------------------- show / pick / stats */
typedef struct { int col; char op[3]; char val[128]; } Where;
static int parse_where(const Tab *t, const char *s, Where *w) {
    const char *p = s; while (*p && !strchr("=!<>", *p)) p++; if (!*p) return -1; size_t nl = (size_t)(p - s); char nm[64]; if (nl >= sizeof nm) return -1; memcpy(nm, s, nl); nm[nl] = 0;
    int ol = (p[1] == '=' && p[0] != '=') ? 2 : 1; if (p[0] == '!' && ol == 1) return -1; memcpy(w->op, p, (size_t)ol); w->op[ol] = 0; snprintf(w->val, sizeof w->val, "%s", p + ol);
    w->col = col_index(t, nm); return w->col < 0 ? -2 : 0;
}
static int where_ok(const Tab *t, int r, const Where *w) {
    const char *v = cellat(t, r, w->col); double a, b; int c;
    if (is_num(v, &a) && is_num(w->val, &b)) c = a < b ? -1 : a > b ? 1 : 0; else c = strcmp(v, w->val);
    switch (w->op[0]) { case '=': return c == 0; case '!': return c != 0; case '<': return w->op[1] ? c <= 0 : c < 0; case '>': return w->op[1] ? c >= 0 : c > 0; }
    return 0;
}
static int cmd_show(int argc, char **argv, int pick) {
    const char *file = NULL, *schema = NULL, *cols = NULL, *outcol = NULL; int head = pick ? 2 : 10; Where wh[8]; int nw = 0; char err[300];
    Tab t; int loaded = 0; const char *wargs[8]; int nwargs = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--schema") && i + 1 < argc) schema = argv[++i]; else if (!strcmp(argv[i], "--head") && i + 1 < argc) head = atoi(argv[++i]); else if (!strcmp(argv[i], "--cols") && i + 1 < argc) cols = argv[++i];
        else if (!strcmp(argv[i], "--where") && i + 1 < argc) { if (nwargs >= 8) die(2, "at most 8 --where"); wargs[nwargs++] = argv[++i]; }
        else if (!file) file = argv[i]; else if (pick && !outcol) outcol = argv[i]; else die(2, "unexpected %s", argv[i]);
    }
    if (!file || (pick && !outcol)) die(2, pick ? "usage: data_op pick <file> <outcol> --where col=val ..." : "usage: data_op show <file> [--schema s.pdl] [--head N] [--where col=val]... [--cols a,b]");
    if (load_table(file, &t, err, sizeof err)) die(2, "%s", err); loaded = 1; (void)loaded;
    for (int i = 0; i < nwargs; i++) { int rc = parse_where(&t, wargs[i], &wh[nw]); if (rc == -1) die(2, "--where %s: use col=val (ops = != < > <= >=)", wargs[i]); if (rc == -2) die(2, "--where %s: no such column", wargs[i]); nw++; }
    int sel[MAX_COLS], ns = 0;
    if (pick) { int c = col_index(&t, outcol); if (c < 0) die(2, "no column %s", outcol); sel[ns++] = c; }
    else if (cols) { char b[512]; snprintf(b, sizeof b, "%s", cols); char *sv = NULL; for (char *p = strtok_r(b, ",", &sv); p; p = strtok_r(NULL, ",", &sv)) { int c = col_index(&t, p); if (c < 0) die(2, "no column %s", p); if (ns < MAX_COLS) sel[ns++] = c; } }
    else for (int c = 0; c < t.ncols; c++) sel[ns++] = c;
    int matches = 0, shown = 0; int firstrow = -1;
    if (!pick) { for (int i = 0; i < ns; i++) printf("%s%s", i ? " | " : "", t.name[sel[i]]); printf("\n");
        if (schema) { Schema s; if (load_schema(schema, base_of(file), &s, err, sizeof err)) die(2, "%s", err); printf("# roles:"); for (int i = 0; i < ns; i++) { const char *ro = "?"; for (int k = 0; k < s.n; k++) if (!strcmp(s.c[k].name, t.name[sel[i]])) ro = s.c[k].role; printf(" %s=%s", t.name[sel[i]], ro); } printf("\n"); } }
    for (int r = 0; r < t.nrows; r++) {
        int ok = 1; for (int k = 0; k < nw; k++) if (!where_ok(&t, r, &wh[k])) { ok = 0; break; } if (!ok) continue;
        matches++; if (pick) { if (firstrow < 0) firstrow = r; continue; }
        if (shown < head) { for (int i = 0; i < ns; i++) printf("%s%s", i ? " | " : "", cellat(&t, r, sel[i])); printf("\n"); shown++; }
    }
    if (pick) { if (matches != 1) { fprintf(stderr, "data_op: pick matched %d rows, need exactly 1\n", matches); return 1; } printf("%s\n", cellat(&t, firstrow, sel[0])); return 0; }
    if (matches > shown) printf("# %d more rows (of %d matching)\n", matches - shown, matches);
    return 0;
}
static int cmp_dbl(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double quant(const double *v, int n, double q) { double p = q * (n - 1); int lo = (int)floor(p); int hi = (int)ceil(p); return v[lo] + (v[hi] - v[lo]) * (p - lo); }
static int cmd_stats(int argc, char **argv) {
    if (argc < 1 || argc > 2) die(2, "usage: data_op stats <file> [col]"); Tab t; char err[300]; if (load_table(argv[0], &t, err, sizeof err)) die(2, "%s", err);
    int only = -1; if (argc == 2) { only = col_index(&t, argv[1]); if (only < 0) die(2, "no column %s", argv[1]); }
    double *v = malloc(sizeof(double) * (size_t)(t.nrows ? t.nrows : 1));
    for (int c = 0; c < t.ncols; c++) {
        if (only >= 0 && c != only) continue; ColInfo ci; analyze(&t, c, &ci); int nv = ci.n - ci.nempty;
        if (ci.numeric && nv) {
            int n = 0; double sum = 0; for (int r = 0; r < t.nrows; r++) { double d; const char *s = cellat(&t, r, c); if (*s && is_num(s, &d)) { v[n++] = d; sum += d; } }
            qsort(v, (size_t)n, sizeof(double), cmp_dbl); char f[10][32]; double q[10] = { v[0], quant(v, n, 0.05), quant(v, n, 0.25), quant(v, n, 0.5), quant(v, n, 0.75), quant(v, n, 0.95), v[n - 1], sum / n }; for (int i = 0; i < 8; i++) fmtnum(q[i], f[i], sizeof f[i]);
            printf("STAT | %s | n=%d | min=%s | p05=%s | q25=%s | median=%s | q75=%s | p95=%s | max=%s | mean=%s | distinct=%d\n", t.name[c], n, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], ci.distinct);
        } else printf("STAT | %s | n=%d | distinct=%d | not numeric\n", t.name[c], nv, ci.distinct);
    }
    free(v); return 0;
}

/* ---------------------------------------------------------------- registry, class, list */
typedef struct { char id[48], path[PATH_MAX], kind[16], cls[12], provider[160]; } Reg;
static Reg REG[256]; static int nreg; static char HOUSE[PATH_MAX];
static void find_house(const char *given) {
    if (given) { snprintf(HOUSE, sizeof HOUSE, "%s", given); return; }
    char ex[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", ex, sizeof ex - 1); if (n <= 0) { HOUSE[0] = 0; return; } ex[n] = 0;
    for (;;) { char *sl = strrchr(ex, '/'); if (!sl || sl == ex) { HOUSE[0] = 0; return; } *sl = 0; char q[PATH_MAX + 16]; snprintf(q, sizeof q, "%s/xyzfs", ex); struct stat sb; if (!stat(q, &sb)) { snprintf(HOUSE, sizeof HOUSE, "%s", ex); return; } }
}
static void load_registry(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) die(2, "cannot read registry %s", path); char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *t = trims(ln); if (!*t || *t == '#') continue; char *fl[8] = {0}; int n = 0; for (char *p = t; n < 8;) { char *bar = strchr(p, '|'); if (bar) *bar = 0; fl[n++] = trims(p); if (!bar) break; p = bar + 1; }
        if (n < 5 || strcmp(fl[0], "DATASET")) continue; if (strcmp(fl[4], "public") && strcmp(fl[4], "private")) die(2, "registry %s: class of %s must be public or private", path, fl[1]);
        if (nreg >= 256) die(2, "registry: more than 256 rows"); Reg *r = &REG[nreg++]; snprintf(r->id, sizeof r->id, "%s", fl[1]);
        if (fl[2][0] == '/') snprintf(r->path, sizeof r->path, "%s", fl[2]); else snprintf(r->path, sizeof r->path, "%s/%s", HOUSE, fl[2]);
        snprintf(r->kind, sizeof r->kind, "%s", fl[3]); snprintf(r->cls, sizeof r->cls, "%s", fl[4]); snprintf(r->provider, sizeof r->provider, "%s", n > 5 ? fl[5] : "-");
    }
    fclose(f);
}
static int path_under(const char *p, const char *pre) { size_t n = strlen(pre); while (n > 1 && pre[n - 1] == '/') n--; return !strncmp(p, pre, n) && (p[n] == 0 || p[n] == '/'); }
static int file_has_human(const char *path) {
    long len; char err[100]; char *b = slurp(path, &len, err, sizeof err); if (!b) return 0; int h = 0; char *save = NULL;
    for (char *ln = strtok_r(b, "\n", &save); ln && !h; ln = strtok_r(NULL, "\n", &save)) { if (strstr(ln, "driver=human")) h = 1; else if (strstr(ln, "|seat|")) { size_t l = strlen(ln); if (l >= 6 && !strcmp(ln + l - 6, "|human")) h = 1; } }
    free(b); return h;
}
static char g_prov[160];
static void classify(const char *path, const char *kind, char *cls, size_t cc, char *why, size_t wc) {
    snprintf(g_prov, sizeof g_prov, "-");
    int best = -1; size_t bl = 0; for (int i = 0; i < nreg; i++) if (path_under(path, REG[i].path) && strlen(REG[i].path) >= bl) { best = i; bl = strlen(REG[i].path); }
    if (best >= 0) { snprintf(g_prov, sizeof g_prov, "%s", REG[best].provider); snprintf(cls, cc, "%s", REG[best].cls); snprintf(why, wc, "registry:%s", REG[best].id); return; }
    if (strstr(path, "/xyzfs/users/")) { snprintf(cls, cc, "private"); snprintf(why, wc, "users-path"); return; }
    char dir[PATH_MAX]; snprintf(dir, sizeof dir, "%s", path); if (strcmp(kind, "ledgerdir")) { char *sl = strrchr(dir, '/'); if (sl) *sl = 0; }
    char lp[PATH_MAX + 16]; struct stat sb;
    if (!strcmp(kind, "ledger") && strcmp(base_of(path), "ledger.txt")) snprintf(lp, sizeof lp, "%s", path); else snprintf(lp, sizeof lp, "%s/ledger.txt", dir);
    if (!stat(lp, &sb)) { int h = file_has_human(lp); snprintf(cls, cc, h ? "private" : "public"); snprintf(why, wc, h ? "ledger-human" : "ledger-pets"); return; }
    snprintf(lp, sizeof lp, "%s/game.csv", dir);
    if (!stat(lp, &sb)) { Tab t; char err[200]; if (!load_table(lp, &t, err, sizeof err)) { int c = col_index(&t, "human_ever"); if (c >= 0 && t.nrows > 0) { double d = 0; is_num(cellat(&t, 0, c), &d); snprintf(cls, cc, d > 0 ? "private" : "public"); snprintf(why, wc, d > 0 ? "game.csv-human" : "game.csv-pets"); tab_free(&t); return; } tab_free(&t); } }
    snprintf(cls, cc, "private"); snprintf(why, wc, "default-private");
}
static int nfound, nrefused, nvisit;
static void found_row(const char *path, const char *kind, int rows, int cols, long bytes) {
    char cls[12], why[80]; classify(path, kind, cls, sizeof cls, why, sizeof why); printf("FOUND | %s | %s | %d | %d | %ld | %s | %s | %s\n", path, kind, rows, cols, bytes, cls, why, g_prov); nfound++;
}
static int count_lines(const char *path, long *bytes) { long len; char err[200]; char *b = slurp(path, &len, err, sizeof err); if (!b) return -1; int n = 0; for (long i = 0; i < len; i++) if (b[i] == '\n') n++; if (len && b[len - 1] != '\n') n++; free(b); *bytes = len; return n; }
static void try_file(const char *path, const char *name) {
    const char *ext = strrchr(name, '.'); struct stat sb; if (stat(path, &sb)) return; int isledger = !strcmp(name, "ledger.txt");
    if (isledger) { /* the folder is the dataset */ char dir[PATH_MAX]; snprintf(dir, sizeof dir, "%s", path); char *sl = strrchr(dir, '/'); if (sl) *sl = 0; long by; int n = count_lines(path, &by); if (n < 0) { printf("REFUSED | %s | oversize: %ld bytes > %ld\n", dir, (long)sb.st_size, MAX_BYTES); nrefused++; return; } found_row(dir, "ledgerdir", n, 7, by); return; }
    int cand = ext && (!strcasecmp(ext, ".csv") || !strcasecmp(ext, ".tsv") || !strcasecmp(ext, ".pdl") || !strcasecmp(ext, ".txt")); if (!cand) return;
    if (sb.st_size > MAX_BYTES) { if (ext && strcasecmp(ext, ".txt") && strcasecmp(ext, ".pdl")) { printf("REFUSED | %s | oversize: %ld bytes > %ld\n", path, (long)sb.st_size, MAX_BYTES); nrefused++; } return; }
    if (!strcasecmp(ext, ".txt")) { FILE *f = fopen(path, "r"); if (!f) return; char b[16] = ""; size_t n = fread(b, 1, 6, f); fclose(f); if (n < 6 || strncmp(b, "EVENT|", 6)) return; }
    Tab t; char err[300]; if (load_table(path, &t, err, sizeof err)) return;
    if (!strcasecmp(ext, ".pdl") && !strcmp(t.kind, "pdl") && !t.header) { tab_free(&t); return; }
    if (t.nrows < 1 || t.ncols < 2) { tab_free(&t); return; }
    found_row(path, t.kind, t.nrows, t.ncols, t.bytes); tab_free(&t);
}
static void walk(const char *dir, int depth) {
    if (depth > MAX_DEPTH || nfound >= MAX_FOUND || nvisit > MAX_VISIT) return; DIR *d = opendir(dir); if (!d) return;
    struct dirent *e; char *names[4096]; int nn = 0;
    while ((e = readdir(d)) && nn < 4096) { if (e->d_name[0] == '.') continue; if (!strcmp(e->d_name, "node_modules") || !strcmp(e->d_name, "+x")) continue; names[nn++] = strdup(e->d_name); }
    closedir(d); qsort(names, (size_t)nn, sizeof(char *), cmp_str);
    for (int i = 0; i < nn; i++) {
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", dir, names[i]); struct stat sb; nvisit++;
        if (!lstat(p, &sb)) { if (S_ISDIR(sb.st_mode)) walk(p, depth + 1); else if (S_ISREG(sb.st_mode)) { if (nfound >= MAX_FOUND) { free(names[i]); continue; } try_file(p, names[i]); } }
        free(names[i]);
    }
}
static int cmd_list(int argc, char **argv) {
    const char *house = NULL; const char *regs[16]; int nregs = 0; const char *roots[32]; int nroots = 0;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--registry") && i + 1 < argc && nregs < 16) regs[nregs++] = argv[++i]; else if (!strcmp(argv[i], "--house") && i + 1 < argc) house = argv[++i]; else if (nroots < 32) roots[nroots++] = argv[i]; }
    if (!nroots) die(2, "usage: data_op list <root...> [--registry f.pdl]... [--house dir]");
    find_house(house);
    if (!nregs && HOUSE[0]) { char dflt[PATH_MAX + 64]; snprintf(dflt, sizeof dflt, "%s/&.hq-apps/data-hq/datasets.pdl", HOUSE); if (!access(dflt, R_OK)) load_registry(dflt); }
    for (int i = 0; i < nregs; i++) load_registry(regs[i]);
    for (int i = 0; i < nroots; i++) {
        char rp[PATH_MAX]; if (!realpath(roots[i], rp)) { printf("REFUSED | %s | no such path\n", roots[i]); nrefused++; continue; }
        struct stat sb; if (stat(rp, &sb)) continue;
        if (S_ISDIR(sb.st_mode)) walk(rp, 0); else try_file(rp, base_of(rp));
    }
    if (nfound >= MAX_FOUND) printf("REFUSED | (more) | the list is capped at %d datasets\n", MAX_FOUND);
    printf("TOTAL | datasets=%d | refused=%d\n", nfound, nrefused); return 0;
}

/* ---------------------------------------------------------------- provider (run a game's exporter) */
static int cmd_provider(int argc, char **argv) {
    const char *house = NULL, *pos[3]; int np = 0;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--house") && i + 1 < argc) house = argv[++i]; else if (np < 3) pos[np++] = argv[i]; else die(2, "provider: unexpected %s", argv[i]); }
    if (np != 3) die(2, "usage: data_op provider <app_dir> <state_dir> <out_dir> [--house dir]");
    char app[PATH_MAX], st[PATH_MAX]; if (!realpath(pos[0], app) || !realpath(pos[1], st)) die(2, "provider: app or state folder does not exist");
    char hs[PATH_MAX]; if (house) snprintf(hs, sizeof hs, "%s", house); else { snprintf(hs, sizeof hs, "%s", app); for (int k = 0; k < 2; k++) { char *sl = strrchr(hs, '/'); if (sl) *sl = 0; } }
    char out[PATH_MAX]; if (pos[2][0] == '/') snprintf(out, sizeof out, "%s", pos[2]); else { char cw[PATH_MAX]; if (!getcwd(cw, sizeof cw)) die(2, "getcwd"); snprintf(out, sizeof out, "%s/%s", cw, pos[2]); }
    { char ro[PATH_MAX]; if (realpath(out, ro)) snprintf(out, sizeof out, "%s", ro); if (path_under(out, st) || path_under(st, out)) die(2, "provider refused: the out folder %s and the game folder %s overlap (the studio never writes beside a game)", out, st); }
    char gp[PATH_MAX + 16]; snprintf(gp, sizeof gp, "%s/game.pdl", app); FILE *f = fopen(gp, "r"); if (!f) die(2, "provider: no game.pdl in %s", app);
    char ln[1024], cmd[1024] = ""; while (fgets(ln, sizeof ln, f)) { char *t = trims(ln); if (strncmp(t, "GAME", 4)) continue; char *a = strchr(t, '|'); if (!a) continue; char *b = strchr(a + 1, '|'); if (!b) continue; *b = 0; if (strcmp(trims(a + 1), "export")) continue; snprintf(cmd, sizeof cmd, "%s", trims(b + 1)); break; }
    fclose(f); if (!cmd[0]) { printf("PROVIDER none: %s has no `GAME | export` row\n", app); return 3; }
    char ex[2048] = ""; for (const char *s = cmd; *s && strlen(ex) + 8 < sizeof ex;) { const char *r = NULL; size_t sk = 0;
        if (!strncmp(s, "{house}", 7)) { r = hs; sk = 7; } else if (!strncmp(s, "{book}", 6)) { r = app; sk = 6; } else if (!strncmp(s, "{state}", 7)) { r = st; sk = 7; } else if (!strncmp(s, "{out}", 5)) { r = out; sk = 5; }
        if (r) { strncat(ex, r, sizeof ex - strlen(ex) - 1); s += sk; } else { strncat(ex, s, 1); s++; } }
    if (strchr(ex, '{')) die(2, "provider refused: an unknown placeholder is left in `%s` (only {house} {book} {state} {out})", ex);
    char *av[24]; int n = 0; for (char *t = strtok(ex, " "); t && n < 23; t = strtok(NULL, " ")) av[n++] = t; av[n] = NULL; if (!n || access(av[0], X_OK)) die(2, "provider: the exporter %s is not executable (build it first)", n ? av[0] : "(none)");
    int pf[2]; if (pipe(pf)) die(2, "pipe"); pid_t pid = fork(); if (pid < 0) die(2, "fork");
    if (pid == 0) { setpgid(0, 0); signal(SIGPIPE, SIG_DFL); dup2(pf[1], 1); dup2(pf[1], 2); close(pf[0]); close(pf[1]); execv(av[0], av); _exit(127); }
    close(pf[1]); static char ob[PROV_OUT + 1]; size_t on = 0; int killed = 0; struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0); int status = 0, done = 0;
    while (!done) {
        struct pollfd p = { pf[0], POLLIN, 0 }; int pr = poll(&p, 1, 100);
        if (pr > 0) { ssize_t k = read(pf[0], ob + on, PROV_OUT - on); if (k > 0) on += (size_t)k; else if (k == 0) { waitpid(pid, &status, 0); done = 1; continue; } if (on >= PROV_OUT) { killed = 2; kill(-pid, SIGKILL); waitpid(pid, &status, 0); break; } }
        clock_gettime(CLOCK_MONOTONIC, &t1); if (t1.tv_sec - t0.tv_sec >= PROV_SECS) { killed = 1; kill(-pid, SIGKILL); waitpid(pid, &status, 0); break; }
    }
    close(pf[0]); ob[on] = 0; { size_t shown = on > 4000 ? 4000 : on; fwrite(ob, 1, shown, stdout); if (shown && ob[shown - 1] != 10) putchar(10); if (on > shown) printf("[... %zu more bytes of exporter output not shown]\n", on - shown); }
    if (killed) { printf("PROVIDER failed: %s\n", killed == 1 ? "killed after the 60 s limit" : "killed: more than 64 KiB of output"); return 1; }
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1; int files = 0; DIR *d = opendir(out); if (d) { struct dirent *e; while ((e = readdir(d))) if (e->d_name[0] != '.') files++; closedir(d); }
    printf("PROVIDER %s rc=%d out=%s files=%d\n", rc == 0 ? "ok" : "failed", rc, out, files); return rc == 0 ? 0 : 1;
}

/* ---------------------------------------------------------------- index (one row per game, for history and trends) */
static const char *const IDX_COLS[][2] = { {"id", "game_id"}, {"source", NULL}, {"ruleset", "ruleset"}, {"seed", "seed"}, {"players", "players"}, {"rounds", "rounds"}, {"rows", "ledger_rows"}, {"outcome", "outcome"}, {"winner", "winner"}, {"winner_driver", "winner_driver"}, {"human_ever", "human_ever"}, {"hash", "state_hash"} };
#define NIDX (int)(sizeof IDX_COLS / sizeof *IDX_COLS)
typedef struct { char c[NIDX][96]; } IdxRow;
static int cmp_idx(const void *a, const void *b) { const IdxRow *x = a, *y = b; int c = strcmp(x->c[0], y->c[0]); return c ? c : strcmp(x->c[1], y->c[1]); }
static void csv_cell(FILE *f, const char *s) { if (!strpbrk(s, ",\"\r\n")) { fputs(s, f); return; } fputc('"', f); for (; *s; s++) { if (*s == '"') fputc('"', f); fputc(*s, f); } fputc('"', f); }
static int cmd_index(int argc, char **argv) {
    const char *outf = NULL; const char *dirs[512]; int nd = 0;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "--out") && i + 1 < argc) outf = argv[++i]; else if (nd < 512) dirs[nd++] = argv[i]; else die(2, "index: more than 512 folders"); }
    if (!nd) die(2, "usage: data_op index <exporter out dir...> [--out index.csv]");
    IdxRow *rows = calloc((size_t)nd, sizeof *rows); int n = 0, skipped = 0, dups = 0;
    for (int i = 0; i < nd; i++) {
        char gp[PATH_MAX + 16], err[300]; snprintf(gp, sizeof gp, "%s/game.csv", dirs[i]); Tab t;
        if (load_table(gp, &t, err, sizeof err)) { fprintf(stderr, "data_op: index skips %s: %s\n", dirs[i], err); skipped++; continue; }
        if (t.nrows != 1 || col_index(&t, "game_id") < 0) { fprintf(stderr, "data_op: index skips %s: game.csv must have one row and a game_id column\n", dirs[i]); skipped++; tab_free(&t); continue; }
        IdxRow *r = &rows[n]; for (int k = 0; k < NIDX; k++) { if (!IDX_COLS[k][1]) { { char dd[PATH_MAX]; snprintf(dd, sizeof dd, "%s", dirs[i]); size_t dl = strlen(dd); while (dl > 1 && dd[dl - 1] == '/') dd[--dl] = 0; snprintf(r->c[k], sizeof r->c[k], "%s", base_of(dd)); } continue; } int c = col_index(&t, IDX_COLS[k][1]); snprintf(r->c[k], sizeof r->c[k], "%s", c >= 0 ? cellat(&t, 0, c) : ""); }
        tab_free(&t); n++;
    }
    qsort(rows, (size_t)n, sizeof *rows, cmp_idx);
    int m = 0; for (int i = 0; i < n; i++) { if (m && !strcmp(rows[m - 1].c[0], rows[i].c[0])) { dups++; continue; } rows[m++] = rows[i]; }
    FILE *f = stdout; char tmp[PATH_MAX + 8] = ""; if (outf) { snprintf(tmp, sizeof tmp, "%s.tmp", outf); f = fopen(tmp, "w"); if (!f) die(2, "cannot write %s", outf); }
    for (int k = 0; k < NIDX; k++) { if (k) fputc(',', f); fputs(IDX_COLS[k][0], f); } fputc('\n', f);
    for (int i = 0; i < m; i++) { for (int k = 0; k < NIDX; k++) { if (k) fputc(',', f); csv_cell(f, rows[i].c[k]); } fputc('\n', f); }
    if (outf) { fclose(f); if (rename(tmp, outf)) die(2, "cannot rename into %s", outf); printf("INDEX wrote %s games=%d skipped=%d duplicates=%d\n", outf, m, skipped, dups); }
    else fprintf(stderr, "INDEX games=%d skipped=%d duplicates=%d\n", m, skipped, dups);
    free(rows); return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fputs("usage: data_op list|sniff|check|show|stats|pick|provider|index ...   (see the header of ops/data_op.c)\n", stderr); return 2; }
    const char *v = argv[1];
    if (!strcmp(v, "list")) return cmd_list(argc - 2, argv + 2);
    if (!strcmp(v, "sniff")) return cmd_sniff(argc - 2, argv + 2);
    if (!strcmp(v, "check")) return cmd_check(argc - 2, argv + 2);
    if (!strcmp(v, "show")) return cmd_show(argc - 2, argv + 2, 0);
    if (!strcmp(v, "pick")) return cmd_show(argc - 2, argv + 2, 1);
    if (!strcmp(v, "stats")) return cmd_stats(argc - 2, argv + 2);
    if (!strcmp(v, "provider")) return cmd_provider(argc - 2, argv + 2);
    if (!strcmp(v, "index")) return cmd_index(argc - 2, argv + 2);
    fprintf(stderr, "data_op: unknown verb %s\n", v); return 2;
}
