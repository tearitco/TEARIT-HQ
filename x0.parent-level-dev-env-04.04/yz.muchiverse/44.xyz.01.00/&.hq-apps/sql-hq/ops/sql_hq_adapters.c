/* sql_hq_adapters.c — load/dump .csv and .pdl files as tables for sql-hq.
 *
 * SQL-HQ-DESIGN.md step 1. Standalone, no deps. The engine
 * (sql_hq_engine.c) CREATE TABLEs + bulk-INSERTs from an ShTable, runs
 * the query in an in-memory sqlite3, then (on commit) SELECT *s back
 * into an ShTable and calls sh_dump() to rewrite the source file in the
 * SAME shape it was read in.
 *
 * Kinds:
 *   'c'  CSV        - first line = headers, comma-split (MVP: no quoted
 *                     commas / embedded newlines - see note in sh_load_csv).
 *   'f'  PDL flat   - the SECTION | key | value config shape
 *                     (livedesk_taskbar.pdl, livedesk_theme.pdl):
 *                     -> 2 columns (key, value), one row per SECTION line.
 *   'r'  PDL record - the TAG|id|k=v|k=v shape (clocks.pdl, reminders.pdl):
 *                     -> columns = "_tag", "_id", then the union of every
 *                        k= seen; one row per line.
 * '#'-prefixed and blank lines are skipped (and NOT preserved on dump -
 * see the design doc's open question about comment preservation; sql-hq
 * should only ever dump pdls it created, not hand-authored config).
 *
 * Build the self-test:  gcc -DSH_ADAPTERS_TEST -o /tmp/sh_adapt sql_hq_adapters.c && /tmp/sh_adapt
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "sql_hq_adapters.h"

/* ---- small helpers ---- */

static char *sh_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void sh_rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                 s[n - 1] == ' '  || s[n - 1] == '\t')) s[--n] = '\0';
}

static void sh_trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    sh_rstrip(s);
}

static int sh_is_skippable(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#');
}

/* grow the column list; returns the index of `name` (adds it if new). */
static int sh_col_idx(ShTable *t, const char *name) {
    for (int i = 0; i < t->ncols; i++)
        if (strcmp(t->cols[i], name) == 0) return i;
    if (t->ncols >= SH_MAX_COLS) return -1;
    t->cols = realloc(t->cols, (size_t)(t->ncols + 1) * sizeof(char *));
    t->cols[t->ncols] = sh_strdup(name);
    return t->ncols++;
}

static void sh_row_begin(ShTable *t) {
    t->cells = realloc(t->cells, (size_t)(t->nrows + 1) * sizeof(char **));
    t->cells[t->nrows] = calloc(SH_MAX_COLS, sizeof(char *));
    t->nrows++;
}

static void sh_set(ShTable *t, int row, int col, const char *val) {
    if (col < 0 || col >= SH_MAX_COLS) return;
    free(t->cells[row][col]);
    t->cells[row][col] = sh_strdup(val ? val : "");
}

/* fill any NULL cells with "" and clamp the row arrays to ncols */
static void sh_finish(ShTable *t) {
    for (int r = 0; r < t->nrows; r++) {
        char **fixed = calloc((size_t)(t->ncols > 0 ? t->ncols : 1), sizeof(char *));
        for (int c = 0; c < t->ncols; c++)
            fixed[c] = t->cells[r][c] ? t->cells[r][c] : sh_strdup("");
        free(t->cells[r]);
        t->cells[r] = fixed;
    }
}

/* ---- CSV ---- */

static int sh_load_csv(FILE *f, ShTable *t) {
    /* MVP splitter: split on ',', trim each field. Does NOT handle
     * RFC-4180 quoted fields containing commas/newlines - flagged in the
     * design doc; add a real state-machine splitter before shipping if
     * the house's .csv sprite/data files need it (sprite.csv rows are
     * simple, so this is fine for step 1). */
    char line[SH_MAX_LINE];
    int header_done = 0;
    while (fgets(line, sizeof(line), f)) {
        sh_rstrip(line);
        if (!header_done) {
            if (sh_is_skippable(line)) continue;
            char *save = NULL, *tok = strtok_r(line, ",", &save);
            while (tok) {
                char c[SH_MAX_CELL];
                snprintf(c, sizeof(c), "%s", tok);
                sh_trim(c);
                if (sh_col_idx(t, c) < 0) return 0;
                tok = strtok_r(NULL, ",", &save);
            }
            header_done = 1;
            continue;
        }
        if (line[0] == '\0') continue;
        sh_row_begin(t);
        int col = 0;
        char *save = NULL, *tok = strtok_r(line, ",", &save);
        while (tok && col < t->ncols) {
            char c[SH_MAX_CELL];
            snprintf(c, sizeof(c), "%s", tok);
            sh_trim(c);
            sh_set(t, t->nrows - 1, col++, c);
            tok = strtok_r(NULL, ",", &save);
        }
    }
    t->src_kind = 'c';
    return 1;
}

static int sh_dump_csv(const ShTable *t, FILE *f) {
    for (int c = 0; c < t->ncols; c++)
        fprintf(f, "%s%s", t->cols[c], c < t->ncols - 1 ? "," : "\n");
    for (int r = 0; r < t->nrows; r++)
        for (int c = 0; c < t->ncols; c++)
            fprintf(f, "%s%s", t->cells[r][c] ? t->cells[r][c] : "",
                    c < t->ncols - 1 ? "," : "\n");
    return 1;
}

/* ---- PDL ---- */

/* sniff: read up to 40 non-skippable lines. If most start with
 * "SECTION" and have >=2 '|' -> flat. Else if most have a '|' and at
 * least one "k=v" field after the first -> record. Default flat. */
static char sh_sniff_pdl(FILE *f) {
    char line[SH_MAX_LINE];
    int n = 0, section = 0, record = 0;
    long pos = ftell(f);
    while (n < 40 && fgets(line, sizeof(line), f)) {
        if (sh_is_skippable(line)) continue;
        n++;
        sh_rstrip(line);
        int bars = 0; for (char *p = line; *p; p++) if (*p == '|') bars++;
        char first[64] = ""; sscanf(line, " %63[^ |\t]", first);
        if (strcasecmp(first, "SECTION") == 0 && bars >= 2) { section++; continue; }
        if (bars >= 1 && strchr(line, '=')) {
            /* is the '=' inside a field AFTER the first '|'? */
            char *bar = strchr(line, '|');
            if (bar && strchr(bar, '=')) record++;
        }
    }
    fseek(f, pos, SEEK_SET);
    if (record > section && record > 0) return 'r';
    return 'f';
}

/* split a '|'-delimited line into trimmed fields; returns count */
static int sh_split_bar(char *line, char **out, int max) {
    int n = 0;
    char *save = NULL, *tok = strtok_r(line, "|", &save);
    while (tok && n < max) {
        while (*tok == ' ' || *tok == '\t') tok++;
        sh_rstrip(tok);
        out[n++] = tok;
        tok = strtok_r(NULL, "|", &save);
    }
    return n;
}

/* The house "flat" pdl shape is <TAG> | <key> | <value> (3 pipe-fields;
 * STATE-AND-PDL-CONVENTIONS.md). SECTION is the common tag but not the
 * only one (livedesk_theme.pdl uses COLOR|bg|#1a1a1a). Model it as a
 * 3-column table (tag, key, value). A 2-field line -> tag="". */
static int sh_load_pdl_flat(FILE *f, ShTable *t) {
    sh_col_idx(t, "tag");
    sh_col_idx(t, "key");
    sh_col_idx(t, "value");
    char line[SH_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (sh_is_skippable(line)) continue;
        sh_rstrip(line);
        char *fields[8];
        char work[SH_MAX_LINE]; snprintf(work, sizeof(work), "%s", line);
        int nf = sh_split_bar(work, fields, 8);
        if (nf < 2) continue;
        const char *tag, *k, *v;
        if (nf >= 3) { tag = fields[0]; k = fields[1]; v = fields[2]; }
        else         { tag = "";       k = fields[0]; v = fields[1]; }
        if (!k[0]) continue;
        sh_row_begin(t);
        sh_set(t, t->nrows - 1, 0, tag);
        sh_set(t, t->nrows - 1, 1, k);
        sh_set(t, t->nrows - 1, 2, v);
    }
    t->src_kind = 'f';
    return 1;
}

static int sh_dump_pdl_flat(const ShTable *t, FILE *f) {
    int ti = -1, ki = -1, vi = -1;
    for (int c = 0; c < t->ncols; c++) {
        if      (strcmp(t->cols[c], "tag")   == 0) ti = c;
        else if (strcmp(t->cols[c], "key")   == 0) ki = c;
        else if (strcmp(t->cols[c], "value") == 0) vi = c;
    }
    if (ki < 0 || vi < 0) return 0;
    for (int r = 0; r < t->nrows; r++) {
        const char *tag = (ti >= 0 && t->cells[r][ti]) ? t->cells[r][ti] : "";
        fprintf(f, "%s | %s | %s\n",
                tag[0] ? tag : "SECTION",
                t->cells[r][ki] ? t->cells[r][ki] : "",
                t->cells[r][vi] ? t->cells[r][vi] : "");
    }
    return 1;
}

static int sh_load_pdl_record(FILE *f, ShTable *t) {
    sh_col_idx(t, "_tag");
    sh_col_idx(t, "_id");
    char line[SH_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (sh_is_skippable(line)) continue;
        sh_rstrip(line);
        char *fields[64];
        char work[SH_MAX_LINE]; snprintf(work, sizeof(work), "%s", line);
        int nf = sh_split_bar(work, fields, 64);
        if (nf < 1) continue;
        sh_row_begin(t);
        int row = t->nrows - 1;
        /* field 0: "TAG" or "TAG=..." or a bare id */
        if (strchr(fields[0], '=')) {
            /* no leading tag - treat field 0 as the first k=v */
        } else {
            sh_set(t, row, 0, fields[0]);            /* _tag */
        }
        int fstart = strchr(fields[0], '=') ? 0 : 1;
        int id_taken = 0;
        for (int i = fstart; i < nf; i++) {
            char *eq = strchr(fields[i], '=');
            if (!eq) {
                if (!id_taken) { sh_set(t, row, 1, fields[i]); id_taken = 1; }
                continue;
            }
            *eq = '\0';
            char key[64]; snprintf(key, sizeof(key), "%s", fields[i]);
            sh_trim(key);
            int ci = sh_col_idx(t, key);
            if (ci < 0) continue;
            sh_set(t, row, ci, eq + 1);
        }
    }
    t->src_kind = 'r';
    return 1;
}

static int sh_dump_pdl_record(const ShTable *t, FILE *f) {
    int tag_i = -1, id_i = -1;
    for (int c = 0; c < t->ncols; c++) {
        if (strcmp(t->cols[c], "_tag") == 0) tag_i = c;
        else if (strcmp(t->cols[c], "_id") == 0) id_i = c;
    }
    for (int r = 0; r < t->nrows; r++) {
        const char *tag = (tag_i >= 0 && t->cells[r][tag_i]) ? t->cells[r][tag_i] : "";
        const char *id  = (id_i  >= 0 && t->cells[r][id_i])  ? t->cells[r][id_i]  : "";
        if (tag[0]) fprintf(f, "%s|", tag);
        if (id[0])  fprintf(f, "%s", id);
        for (int c = 0; c < t->ncols; c++) {
            if (c == tag_i || c == id_i) continue;
            const char *v = t->cells[r][c];
            if (v && v[0]) fprintf(f, "|%s=%s", t->cols[c], v);
        }
        fprintf(f, "\n");
    }
    return 1;
}

/* ---- public API ---- */

static const char *sh_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

int sh_load(const char *path, ShTable *t) {
    memset(t, 0, sizeof(*t));
    snprintf(t->src_path, sizeof(t->src_path), "%s", path);
    /* table name = basename without extension */
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    snprintf(t->name, sizeof(t->name), "%s", base);
    char *dot = strrchr(t->name, '.');
    if (dot) *dot = '\0';
    for (char *p = t->name; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_')) *p = '_';

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int ok = 0;
    const char *e = sh_ext(path);
    if (strcasecmp(e, "csv") == 0) {
        ok = sh_load_csv(f, t);
    } else if (strcasecmp(e, "pdl") == 0) {
        char k = sh_sniff_pdl(f);
        ok = (k == 'r') ? sh_load_pdl_record(f, t) : sh_load_pdl_flat(f, t);
    } else {
        /* unknown ext: try csv */
        ok = sh_load_csv(f, t);
    }
    fclose(f);
    if (ok) sh_finish(t);
    return ok;
}

int sh_dump(const ShTable *t, const char *path) {
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.sqlhqtmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return 0;
    int ok = 0;
    switch (t->src_kind) {
        case 'c': ok = sh_dump_csv(t, f); break;
        case 'f': ok = sh_dump_pdl_flat(t, f); break;
        case 'r': ok = sh_dump_pdl_record(t, f); break;
        default:  ok = sh_dump_csv(t, f); break;
    }
    fclose(f);
    if (!ok) { remove(tmp); return 0; }
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

void sh_free(ShTable *t) {
    for (int c = 0; c < t->ncols; c++) free(t->cols[c]);
    free(t->cols);
    for (int r = 0; r < t->nrows; r++) {
        for (int c = 0; c < t->ncols; c++) free(t->cells[r][c]);
        free(t->cells[r]);
    }
    free(t->cells);
    memset(t, 0, sizeof(*t));
}

#ifdef SH_ADAPTERS_TEST
static void dump_stdout(const ShTable *t) {
    printf("[table %s  kind=%c  %dx%d]\n", t->name, t->src_kind, t->nrows, t->ncols);
    for (int c = 0; c < t->ncols; c++) printf("%s%s", t->cols[c], c < t->ncols - 1 ? " | " : "\n");
    for (int r = 0; r < t->nrows; r++)
        for (int c = 0; c < t->ncols; c++)
            printf("%s%s", t->cells[r][c], c < t->ncols - 1 ? " | " : "\n");
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file.csv|file.pdl> [out]\n", argv[0]); return 2; }
    ShTable t;
    if (!sh_load(argv[1], &t)) { fprintf(stderr, "load failed\n"); return 1; }
    dump_stdout(&t);
    if (argc >= 3) {
        if (!sh_dump(&t, argv[2])) { fprintf(stderr, "dump failed\n"); return 1; }
        fprintf(stderr, "dumped -> %s\n", argv[2]);
    }
    sh_free(&t);
    return 0;
}
#endif
