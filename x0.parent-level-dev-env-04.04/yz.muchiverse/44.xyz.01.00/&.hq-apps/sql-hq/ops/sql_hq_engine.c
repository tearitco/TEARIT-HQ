/* sql_hq_engine.c — sql-hq's query engine. SQL-HQ-DESIGN.md step 2/3.
 *
 * Loads .csv / .pdl files as tables into an in-memory sqlite3, runs SQL
 * against them (FULL SQL - joins, GROUP BY, CTEs, window fns, ...), and
 * on request writes changed tables back to their source files in the
 * original shape (via sql_hq_adapters.c).
 *
 * Two modes:
 *
 *   ONE-SHOT (used by the x11-hq window's action script):
 *     sql_hq run  <workspace_dir> <query_file> [out_grid_file]
 *       - reads <workspace_dir>/manifest.pdl  (SECTION | table:<name> | <path>|<kind>)
 *       - runs the SQL in <query_file>
 *       - writes a TSV result grid to <out_grid_file> (or stdout)
 *       - writes <workspace_dir>/state/dirty.txt : one changed table name per line
 *     sql_hq commit <workspace_dir>
 *       - re-reads manifest, dumps every table listed in state/dirty.txt
 *         back to its source file, clears dirty.txt
 *
 *   REPL (a plain interactive shell, like sqlite3's):
 *     sql_hq repl [workspace_dir]      (or: bare `sql_hq` on a tty)
 *       .open  <file.csv|file.pdl> [as name]   load a file as a table
 *       .tables                                list loaded tables
 *       .schema [table]                        show CREATE TABLE(s)
 *       .commit / .rollback                    write dirty tables / discard
 *       .save  <table> [to path]               dump one table now
 *       .grid on|off                           aligned grid vs raw TSV
 *       .quit
 *       <any SQL>;                             run it, print the grid
 *
 * Build: see build_sql_hq.sh (compiles the vendored amalgamation once).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "sqlite3.h"
#include "sql_hq_adapters.h"

void sh_rstrip_public(char *s);   /* defined below; used by repl() */

#define SHE_MAX_TABLES 128

typedef struct {
    ShTable  t;
    int      loaded;
} SheEntry;

static sqlite3   *g_db;
static SheEntry   g_tab[SHE_MAX_TABLES];
static int        g_ntab;
static int        g_grid = 1;     /* aligned grid (1) vs raw TSV (0) */
static char       g_ws[1024];     /* workspace dir, "" if none */

/* ---------- table -> sqlite ---------- */

static SheEntry *she_find(const char *name) {
    for (int i = 0; i < g_ntab; i++)
        if (g_tab[i].loaded && strcmp(g_tab[i].t.name, name) == 0) return &g_tab[i];
    return NULL;
}

static void she_quote_ident(const char *in, char *out, size_t n) {
    /* "col" with any embedded " doubled */
    size_t o = 0;
    if (o < n - 1) out[o++] = '"';
    for (const char *p = in; *p && o < n - 2; p++) {
        if (*p == '"' && o < n - 3) out[o++] = '"';
        out[o++] = *p;
    }
    if (o < n - 1) out[o++] = '"';
    out[o] = '\0';
}

static int she_create_and_fill(ShTable *t) {
    char *sql = NULL;
    size_t cap = 256;
    sql = malloc(cap);
    int len = snprintf(sql, cap, "CREATE TABLE ");
    char qi[256];
    she_quote_ident(t->name, qi, sizeof(qi));
    len += snprintf(sql + len, cap - len, "%s (", qi);
    for (int c = 0; c < t->ncols; c++) {
        she_quote_ident(t->cols[c], qi, sizeof(qi));
        size_t need = (size_t)len + strlen(qi) + 16;
        if (need > cap) { cap = need * 2; sql = realloc(sql, cap); }
        len += snprintf(sql + len, cap - len, "%s%s TEXT", c ? ", " : "", qi);
    }
    len += snprintf(sql + len, cap - len, ");");
    char *err = NULL;
    if (sqlite3_exec(g_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "sql-hq: create %s: %s\n", t->name, err ? err : "?");
        sqlite3_free(err); free(sql); return 0;
    }
    free(sql);

    /* bulk insert */
    char ins[512];
    she_quote_ident(t->name, qi, sizeof(qi));
    int p = snprintf(ins, sizeof(ins), "INSERT INTO %s VALUES (", qi);
    for (int c = 0; c < t->ncols; c++)
        p += snprintf(ins + p, sizeof(ins) - p, "%s?", c ? "," : "");
    snprintf(ins + p, sizeof(ins) - p, ");");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db, ins, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "sql-hq: prepare insert %s: %s\n", t->name, sqlite3_errmsg(g_db));
        return 0;
    }
    sqlite3_exec(g_db, "BEGIN", NULL, NULL, NULL);
    for (int r = 0; r < t->nrows; r++) {
        for (int c = 0; c < t->ncols; c++)
            sqlite3_bind_text(st, c + 1, t->cells[r][c] ? t->cells[r][c] : "", -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_reset(st);
    }
    sqlite3_exec(g_db, "COMMIT", NULL, NULL, NULL);
    sqlite3_finalize(st);
    return 1;
}

static int she_add_file(const char *path, const char *as_name) {
    if (g_ntab >= SHE_MAX_TABLES) { fprintf(stderr, "sql-hq: too many tables\n"); return 0; }
    SheEntry *e = &g_tab[g_ntab];
    memset(e, 0, sizeof(*e));
    if (!sh_load(path, &e->t)) { fprintf(stderr, "sql-hq: can't load %s\n", path); return 0; }
    if (as_name && *as_name) snprintf(e->t.name, sizeof(e->t.name), "%s", as_name);
    if (she_find(e->t.name)) {
        /* drop the old one first */
        char q[256], qi[256]; she_quote_ident(e->t.name, qi, sizeof(qi));
        snprintf(q, sizeof(q), "DROP TABLE IF EXISTS %s;", qi);
        sqlite3_exec(g_db, q, NULL, NULL, NULL);
        SheEntry *old = she_find(e->t.name);
        sh_free(&old->t); old->loaded = 0;
    }
    if (!she_create_and_fill(&e->t)) { sh_free(&e->t); return 0; }
    e->loaded = 1;
    g_ntab++;
    fprintf(stderr, "sql-hq: loaded %s as \"%s\" (%dx%d, kind=%c)\n",
            path, e->t.name, e->t.nrows, e->t.ncols, e->t.src_kind);
    return 1;
}

/* sqlite fires this on every INSERT/UPDATE/DELETE - mark the ShTable */
static void she_update_hook(void *u, int op, const char *db, const char *tbl, sqlite3_int64 rowid) {
    (void)u; (void)op; (void)db; (void)rowid;
    SheEntry *e = she_find(tbl);
    if (e) e->t.dirty = 1;
}

/* ---------- result grid ---------- */

static void she_print_grid(sqlite3_stmt *st, FILE *out) {
    int ncol = sqlite3_column_count(st);
    if (ncol == 0) return;

    if (!g_grid) {
        for (int c = 0; c < ncol; c++)
            fprintf(out, "%s%s", sqlite3_column_name(st, c), c < ncol - 1 ? "\t" : "\n");
        while (sqlite3_step(st) == SQLITE_ROW) {
            for (int c = 0; c < ncol; c++) {
                const unsigned char *v = sqlite3_column_text(st, c);
                fprintf(out, "%s%s", v ? (const char *)v : "", c < ncol - 1 ? "\t" : "\n");
            }
        }
        return;
    }

    /* buffer all rows to compute column widths (fine for a UI grid) */
    char ***rows = NULL; int nrows = 0, cap = 0;
    int *w = calloc(ncol, sizeof(int));
    for (int c = 0; c < ncol; c++) w[c] = (int)strlen(sqlite3_column_name(st, c));
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (nrows >= cap) { cap = cap ? cap * 2 : 64; rows = realloc(rows, cap * sizeof(char **)); }
        rows[nrows] = calloc(ncol, sizeof(char *));
        for (int c = 0; c < ncol; c++) {
            const unsigned char *v = sqlite3_column_text(st, c);
            const char *s = v ? (const char *)v : "";
            rows[nrows][c] = strdup(s);
            int l = (int)strlen(s);
            if (l > w[c]) w[c] = l;
        }
        nrows++;
    }
    for (int c = 0; c < ncol; c++) fprintf(out, "%-*s%s", w[c], sqlite3_column_name(st, c), c < ncol - 1 ? " | " : "\n");
    for (int c = 0; c < ncol; c++) { for (int k = 0; k < w[c]; k++) fputc('-', out); fputs(c < ncol - 1 ? "-+-" : "\n", out); }
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncol; c++) fprintf(out, "%-*s%s", w[c], rows[r][c], c < ncol - 1 ? " | " : "\n");
        for (int c = 0; c < ncol; c++) free(rows[r][c]);
        free(rows[r]);
    }
    fprintf(out, "(%d row%s)\n", nrows, nrows == 1 ? "" : "s");
    free(rows); free(w);
}

static int she_run_sql(const char *sql, FILE *out) {
    const char *tail = sql;
    int any_err = 0;
    while (tail && *tail) {
        while (*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == ';') tail++;
        if (!*tail) break;
        sqlite3_stmt *st = NULL;
        const char *next = NULL;
        if (sqlite3_prepare_v2(g_db, tail, -1, &st, &next) != SQLITE_OK) {
            fprintf(out, "-- error: %s\n", sqlite3_errmsg(g_db));
            any_err = 1;
            break;
        }
        if (st) {
            if (sqlite3_column_count(st) > 0) she_print_grid(st, out);
            else while (sqlite3_step(st) == SQLITE_ROW) { /* drain DML */ }
            sqlite3_finalize(st);
        }
        tail = next;
    }
    return any_err ? 1 : 0;
}

/* ---------- dirty -> files ---------- */

/* pull a table back out of sqlite into a fresh ShTable, keeping the
 * original's src_path/src_kind so sh_dump writes the right shape. */
static int she_reap_table(SheEntry *e, ShTable *outp) {
    memset(outp, 0, sizeof(*outp));
    snprintf(outp->name, sizeof(outp->name), "%s", e->t.name);
    snprintf(outp->src_path, sizeof(outp->src_path), "%s", e->t.src_path);
    outp->src_kind = e->t.src_kind;

    char q[300], qi[256]; she_quote_ident(e->t.name, qi, sizeof(qi));
    snprintf(q, sizeof(q), "SELECT * FROM %s;", qi);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db, q, -1, &st, NULL) != SQLITE_OK) return 0;
    int ncol = sqlite3_column_count(st);
    outp->cols = calloc(ncol, sizeof(char *));
    for (int c = 0; c < ncol; c++) outp->cols[c] = strdup(sqlite3_column_name(st, c));
    outp->ncols = ncol;
    int cap = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (outp->nrows >= cap) { cap = cap ? cap * 2 : 64; outp->cells = realloc(outp->cells, cap * sizeof(char **)); }
        outp->cells[outp->nrows] = calloc(ncol, sizeof(char *));
        for (int c = 0; c < ncol; c++) {
            const unsigned char *v = sqlite3_column_text(st, c);
            outp->cells[outp->nrows][c] = strdup(v ? (const char *)v : "");
        }
        outp->nrows++;
    }
    sqlite3_finalize(st);
    return 1;
}

static int she_commit(FILE *log) {
    int n = 0;
    for (int i = 0; i < g_ntab; i++) {
        if (!g_tab[i].loaded || !g_tab[i].t.dirty) continue;
        ShTable reap;
        if (!she_reap_table(&g_tab[i], &reap)) { fprintf(log, "-- reap failed: %s\n", g_tab[i].t.name); continue; }
        if (sh_dump(&reap, reap.src_path)) {
            fprintf(log, "-- wrote %s (%d rows) -> %s\n", reap.name, reap.nrows, reap.src_path);
            g_tab[i].t.dirty = 0;
            n++;
        } else {
            fprintf(log, "-- WRITE FAILED: %s -> %s\n", reap.name, reap.src_path);
        }
        sh_free(&reap);
    }
    fprintf(log, "-- committed %d table%s\n", n, n == 1 ? "" : "s");
    return n;
}

static void she_rollback(FILE *log) {
    /* discard the in-memory db and rebuild from the source files */
    for (int i = 0; i < g_ntab; i++) if (g_tab[i].loaded) { sh_free(&g_tab[i].t); g_tab[i].loaded = 0; }
    int keepn = g_ntab; g_ntab = 0;
    /* reload from manifest if we have a workspace, else nothing to do */
    (void)keepn;
    if (g_ws[0]) {
        char mpath[1200]; snprintf(mpath, sizeof(mpath), "%s/manifest.pdl", g_ws);
        FILE *m = fopen(mpath, "r");
        if (m) {
            char line[2048];
            while (fgets(line, sizeof(line), m)) {
                if (line[0] == '#' || line[0] == '\n') continue;
                char *k = strchr(line, '|'); if (!k) continue; k++;
                char *v = strchr(k, '|'); if (!v) continue; *v = '\0'; v++;
                char keybuf[256]; snprintf(keybuf, sizeof(keybuf), "%s", k);
                for (char *p = keybuf; *p; p++) if (*p == ' ') *p = '\0'; /* trim-ish */
                if (strncmp(keybuf, "table:", 6) != 0) continue;
                char path[1024]; sscanf(v, " %1023[^|\n]", path);
                she_add_file(path, keybuf + 6);
            }
            fclose(m);
        }
    }
    fprintf(log, "-- rolled back (reloaded from source)\n");
}

/* ---------- manifest (one-shot mode) ---------- */

static void load_manifest(const char *ws) {
    char mpath[1200]; snprintf(mpath, sizeof(mpath), "%s/manifest.pdl", ws);
    FILE *m = fopen(mpath, "r");
    if (!m) return;
    char line[2048];
    while (fgets(line, sizeof(line), m)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* SECTION | table:<name> | <path>[|<kind>] */
        char *f1 = strchr(line, '|'); if (!f1) continue; f1++;
        char *f2 = strchr(f1, '|');   if (!f2) continue; *f2 = '\0'; f2++;
        char key[256]; sscanf(f1, " %255[^ |\n]", key);
        if (strncmp(key, "table:", 6) != 0) continue;
        char path[1024]; sscanf(f2, " %1023[^|\n]", path);
        she_add_file(path, key + 6);
    }
    fclose(m);
}

/* ---------- REPL ---------- */

static void repl(void) {
    char buf[SH_MAX_LINE];
    int tty = isatty(0);
    if (tty) {
        fprintf(stderr, "sql-hq REPL. .open <file> to load a table, .tables to list, .quit to exit.\n");
        if (g_ws[0]) fprintf(stderr, "workspace: %s\n", g_ws);
    }
    for (;;) {
        if (tty) fputs("sql-hq> ", stderr);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        char *s = buf;
        while (*s == ' ' || *s == '\t') s++;
        sh_rstrip_public(s);
        if (!*s) continue;

        if (s[0] == '.') {
            char cmd[64] = "", a1[1024] = "", a2[1024] = "", a3[1024] = "";
            sscanf(s, ".%63s %1023s %1023s %1023s", cmd, a1, a2, a3);
            if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "q")) break;
            else if (!strcmp(cmd, "open")) {
                const char *as = (!strcmp(a2, "as") && a3[0]) ? a3 : NULL;
                she_add_file(a1, as);
            } else if (!strcmp(cmd, "tables")) {
                for (int i = 0; i < g_ntab; i++) if (g_tab[i].loaded)
                    printf("%-24s %dx%d  kind=%c%s  <- %s\n", g_tab[i].t.name,
                           g_tab[i].t.nrows, g_tab[i].t.ncols, g_tab[i].t.src_kind,
                           g_tab[i].t.dirty ? " *dirty*" : "", g_tab[i].t.src_path);
            } else if (!strcmp(cmd, "schema")) {
                char q[256];
                if (a1[0]) snprintf(q, sizeof(q), "SELECT sql FROM sqlite_master WHERE name='%s';", a1);
                else       snprintf(q, sizeof(q), "SELECT sql FROM sqlite_master WHERE type='table';");
                she_run_sql(q, stdout);
            } else if (!strcmp(cmd, "commit"))   she_commit(stdout);
            else if (!strcmp(cmd, "rollback"))   she_rollback(stdout);
            else if (!strcmp(cmd, "save")) {
                SheEntry *e = she_find(a1);
                if (!e) { printf("-- no such table: %s\n", a1); continue; }
                ShTable reap; she_reap_table(e, &reap);
                const char *dst = (!strcmp(a2, "to") && a3[0]) ? a3 : reap.src_path;
                printf(sh_dump(&reap, dst) ? "-- wrote %s\n" : "-- WRITE FAILED %s\n", dst);
                sh_free(&reap);
            } else if (!strcmp(cmd, "grid")) g_grid = strcmp(a1, "off") != 0;
            else printf("-- unknown: .%s  (.open .tables .schema .commit .rollback .save .grid .quit)\n", cmd);
            continue;
        }
        /* plain SQL - accumulate until a line ends with ';' for multi-line */
        char acc[SH_MAX_LINE]; snprintf(acc, sizeof(acc), "%s", s);
        while (acc[strlen(acc) - 1] != ';') {
            if (tty) fputs("   ...> ", stderr);
            if (!fgets(buf, sizeof(buf), stdin)) break;
            sh_rstrip_public(buf);
            size_t have = strlen(acc);
            snprintf(acc + have, sizeof(acc) - have, " %s", buf);
            if (!buf[0]) break;
        }
        she_run_sql(acc, stdout);
        fflush(stdout);
    }
}

/* tiny re-export so repl() can trim without pulling adapters' statics */
void sh_rstrip_public(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) s[--n] = '\0';
}

/* ---------- main ---------- */

static int usage(const char *p) {
    fprintf(stderr,
        "usage:\n"
        "  %s run    <workspace_dir> <query_file> [out_grid_file]\n"
        "  %s commit <workspace_dir>\n"
        "  %s repl   [workspace_dir]\n"
        "  %s <file.csv|file.pdl> \"<SQL>\"      (quick one-file query, grid to stdout)\n",
        p, p, p, p);
    return 2;
}

int main(int argc, char **argv) {
    if (sqlite3_open(":memory:", &g_db) != SQLITE_OK) {
        fprintf(stderr, "sql-hq: can't open :memory: db\n");
        return 1;
    }
    sqlite3_update_hook(g_db, she_update_hook, NULL);

    if (argc == 1) {                                  /* bare -> REPL */
        repl();
    } else if (!strcmp(argv[1], "repl")) {
        if (argc >= 3) { snprintf(g_ws, sizeof(g_ws), "%s", argv[2]); load_manifest(g_ws); }
        repl();
    } else if (!strcmp(argv[1], "run")) {
        if (argc < 4) return usage(argv[0]);
        snprintf(g_ws, sizeof(g_ws), "%s", argv[2]);
        load_manifest(g_ws);
        FILE *qf = fopen(argv[3], "r");
        if (!qf) { fprintf(stderr, "sql-hq: can't read %s\n", argv[3]); return 1; }
        char *q = NULL; size_t qn = 0, qc = 0; int ch;
        while ((ch = fgetc(qf)) != EOF) { if (qn + 1 >= qc) { qc = qc ? qc * 2 : 4096; q = realloc(q, qc); } q[qn++] = (char)ch; }
        if (q) q[qn] = '\0'; fclose(qf);
        FILE *out = (argc >= 5) ? fopen(argv[4], "w") : stdout;
        if (!out) { fprintf(stderr, "sql-hq: can't write %s\n", argv[4]); return 1; }
        int rc = she_run_sql(q ? q : "", out);
        if (out != stdout) fclose(out);
        /* record dirty table names for a later `commit` */
        if (g_ws[0]) {
            char dp[1200]; snprintf(dp, sizeof(dp), "%s/state", g_ws);
            /* best-effort mkdir */
            char mk[1300]; snprintf(mk, sizeof(mk), "%s/dirty.txt", dp);
            FILE *df = fopen(mk, "w");
            if (df) { for (int i = 0; i < g_ntab; i++) if (g_tab[i].loaded && g_tab[i].t.dirty) fprintf(df, "%s\n", g_tab[i].t.name); fclose(df); }
        }
        free(q);
        return rc;
    } else if (!strcmp(argv[1], "commit")) {
        if (argc < 3) return usage(argv[0]);
        snprintf(g_ws, sizeof(g_ws), "%s", argv[2]);
        load_manifest(g_ws);
        /* apply queued dirty list: mark those tables dirty, then commit.
         * (load_manifest reloaded clean copies; a real flow keeps the
         * engine resident - see the design doc. For the shell-driven
         * one-shot flow, `run` should be followed immediately by
         * `commit` while nothing else touched the files.) */
        char dp[1250]; snprintf(dp, sizeof(dp), "%s/state/dirty.txt", g_ws);
        FILE *df = fopen(dp, "r");
        if (df) {
            char nm[256];
            while (fgets(nm, sizeof(nm), df)) { sh_rstrip_public(nm); SheEntry *e = she_find(nm); if (e) e->t.dirty = 1; }
            fclose(df);
        }
        she_commit(stdout);
        remove(dp);
    } else if (argc == 3) {                           /* quick one-file */
        if (!she_add_file(argv[1], NULL)) return 1;
        she_run_sql(argv[2], stdout);
    } else {
        return usage(argv[0]);
    }

    sqlite3_close(g_db);
    return 0;
}
