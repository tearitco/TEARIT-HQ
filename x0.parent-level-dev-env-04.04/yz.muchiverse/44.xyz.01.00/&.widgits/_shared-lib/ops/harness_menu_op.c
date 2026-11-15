#define _GNU_SOURCE
/* harness_menu_op - the ONE authority for "which harness can I run from here" (owner 2026-11-07: harness selection in the top taskbar "like edit has, but for LyfeDesk";
 * defaults chosen: rows are DATA, a row names a registered case or op only, CASE and ACTION kinds, a confirm for destructive actions, the last verdict beside each case).
 * Replaces the seven rows that were hard-coded in rpg-edit-hq/ops/pal_side.c (floor, wall, door, roof, bridge, all, clear) and my five `harness_menu_N` rows in livedesk_taskbar.pdl.
 *
 * Data (pipe rows, '#' comments):  HARNESS | id | page | kind | label | target | args | writes | confirm | note
 *   files: <house>/#.desktop/harness_menu.pdl   (global rows: shown everywhere, the page column is ignored)
  *          <house>/@.apps/<book>/harness.pdl     (a book's rows: shown when that book is open; page = `*` (every page) or one page id)
 *                                         a book may also say `GLOBAL | no` (one row, anywhere in its file): the global rows are then NOT listed for that book (2026-11-14: the office page does not want the centroid generators); with no GLOBAL row they are listed as before
 *   id       a word, unique across both files          kind     CASE = run one harness case (&.widgits/_shared-lib/harness/cases/<target>.pdl) and keep its verdict
 *                                                                ACTION = run one compiled op on the open page
 *   target   CASE: the case name, the file must exist.  ACTION: a path relative to the house of an executable under an ops/+x/ folder (no `..`, nothing outside the house)
 *   args     ACTION only: space-separated, each token taken whole (NO shell); ${HOUSE} ${BOOK} ${PAGE} ${APP} (= @.apps/<book>) ${CELLS} (= the open page's cells.txt) are replaced
 *   writes   `page` = the action changes the open page (the page's cells.marker is bumped afterwards so the window redraws) | `-`      confirm   yes = the first pick only asks (exit 3), pick again with --confirmed
 * Verbs:   harness_menu_op list  <house> <book|-> <page|-> <outfile>   writes `label<TAB>id` lines (CASE rows carry the last verdict: ` [15 ok]` / ` [2 FAIL]`, read from the case's results file and
 *                                                                      only when that file exists); only valid rows are listed
 *          harness_menu_op pick  <house> <book|-> <page|-> <id> [--confirmed]   run one row: exit 0 ran ok, 1 it ran and failed, 2 refused (unknown id, bad row, no open page), 3 needs confirm
 *          harness_menu_op check <house> [book...]                     validate every row of the global file and the named books: exit 0 all good, 2 with one line per bad row
 * A CASE runs behind cpu_gate (one heavy job at a time, niced, waits up to 120 s) and its verdict line is written to <house>/#.desktop/harness_last.txt (ONE file, overwritten, never grows).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/harness_menu_op.+x ops/harness_menu_op.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>
#include "../plug_ref.c"      /* the engine's plug rules (value types and bounds, row splitting): the plug check uses the SAME code */

#define MAXR 48
#define P 4096
typedef struct { char id[40], page[48], kind[8], label[80], target[P], args[P], writes[8], confirm[8], src[P]; int line, global; } Row;
static Row R[MAXR]; static int nr, nbad;
static int g_book_no_global;      /* a book's harness.pdl may say `GLOBAL | no`: the global rows (#.desktop/harness_menu.pdl) are not listed for that book (D76 addendum, 2026-11-14) */
static char HOUSE[P];

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int word_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }
static void bad(const char *src, int line, const char *why) { nbad++; printf("harness_menu_op: %s line %d: %s\n", src, line, why); }      /* stdout since 2026-11-14: one line per bad row, readable by a human and by a case */

static int under_ops(const char *rel) {          /* relative, no .., an executable regular file under an ops/+x/ folder inside the house */
    if (!rel[0] || rel[0] == '/' || strstr(rel, "..") || !strstr(rel, "/ops/+x/")) return 0;
    char full[P]; snprintf(full, sizeof full, "%s/%s", HOUSE, rel); struct stat st; return stat(full, &st) == 0 && S_ISREG(st.st_mode) && access(full, X_OK) == 0;
}
static int case_ok(const char *c) { if (!word_ok(c) || strlen(c) > 60) return 0; char f[P]; snprintf(f, sizeof f, "%s/&.widgits/_shared-lib/harness/cases/%s.pdl", HOUSE, c); return access(f, R_OK) == 0; }

static void load(const char *path, int global) {
    FILE *fp = fopen(path, "r"); if (!fp) return; char ln[2 * P]; int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; if (ln[0] == '#') continue; char *f[12]; int n = 0; char *p = ln; f[n++] = p;
        for (; *p && n < 12; p++) if (*p == '|') { *p = 0; f[n++] = p + 1; }
        for (int i = 0; i < n; i++) f[i] = trim(f[i]);
        if (!global && n >= 2 && !strcmp(f[0], "GLOBAL")) { if (!strcmp(f[1], "no")) g_book_no_global = 1; continue; }
        if (n < 1 || strcmp(f[0], "HARNESS")) continue;
        if (n < 10) { bad(path, lineno, "a HARNESS row needs id | page | kind | label | target | args | writes | confirm | note"); continue; }
        if (nr >= MAXR) { bad(path, lineno, "too many rows"); continue; }
        Row *r = &R[nr]; memset(r, 0, sizeof *r); r->line = lineno; r->global = global; snprintf(r->src, sizeof r->src, "%s", path);
        snprintf(r->id, sizeof r->id, "%s", f[1]); snprintf(r->page, sizeof r->page, "%s", global ? "*" : f[2]); snprintf(r->kind, sizeof r->kind, "%s", f[3]);
        snprintf(r->label, sizeof r->label, "%s", f[4]); snprintf(r->target, sizeof r->target, "%s", f[5]); snprintf(r->args, sizeof r->args, "%s", f[6]);
        snprintf(r->writes, sizeof r->writes, "%s", f[7]); snprintf(r->confirm, sizeof r->confirm, "%s", f[8]);
        if (!word_ok(r->id) || strlen(f[1]) > 39) { bad(path, lineno, "id must be one word"); continue; }
        if (!r->label[0] || strchr(r->label, '\t')) { bad(path, lineno, "label is empty or has a tab"); continue; }
        if (!global && strcmp(r->page, "*") && !word_ok(r->page)) { bad(path, lineno, "page must be * or one page id"); continue; }
        if (strcmp(r->writes, "page") && strcmp(r->writes, "-")) { bad(path, lineno, "writes must be page or -"); continue; }
        if (strcmp(r->confirm, "yes") && strcmp(r->confirm, "no")) { bad(path, lineno, "confirm must be yes or no"); continue; }
        int dup = 0; for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, r->id)) dup = 1; if (dup) { bad(path, lineno, "id is declared twice"); continue; }
        if (!strcmp(r->kind, "CASE")) { if (!case_ok(r->target)) { bad(path, lineno, "CASE target is not an existing harness case"); continue; } }
        else if (!strcmp(r->kind, "ACTION")) { if (!under_ops(r->target)) { bad(path, lineno, "ACTION target must be an executable under an ops/+x folder of the house"); continue; } if (strstr(r->args, "..")) { bad(path, lineno, "args must not contain .."); continue; } }
        else { bad(path, lineno, "kind must be CASE or ACTION"); continue; }
        nr++;
    }
    fclose(fp);
}
static void load_all(const char *book) {
    g_book_no_global = 0; nr = 0;      /* idempotent: a trigger calls this again inside a run, and appending twice made every id "declared twice" (found 2026-11-14) */
    char p[P]; snprintf(p, sizeof p, "%s/#.desktop/harness_menu.pdl", HOUSE); load(p, 1);
    if (book && book[0] && strcmp(book, "-") && word_ok(book)) { snprintf(p, sizeof p, "%s/@.apps/%s/harness.pdl", HOUSE, book); load(p, 0); }
}
static int visible(const Row *r, const char *page) { return !strcmp(r->page, "*") || (page && !strcmp(r->page, page)); }
/* LIST ONLY (2026-11-14, owner): a row is LISTED when it is visible AND (a) it is not a global row of a book that has no tools of its own (a page with no harnesses or plugs shows no tests, only the generic notes rows the bar adds) and (b) it is not an own row that repeats a global one (same kind and target: `crew` and `enter house` showed twice). pick does not use this. */
static int g_hide_global;
static int listed(int i, const char *page) { if (!visible(&R[i], page)) return 0; if (R[i].global) return !g_hide_global;
    for (int k = 0; k < nr; k++) if (R[k].global && !strcmp(R[k].kind, R[i].kind) && !strcmp(R[k].target, R[i].target)) return 0; return 1; }

static void verdict_suffix(const Row *r, char *out, size_t cap) {      /* ` [15 ok]` / ` [2 FAIL]` from the case's results file, and nothing when the file does not exist */
    out[0] = 0; if (strcmp(r->kind, "CASE")) return;
    char f[P]; snprintf(f, sizeof f, "%s/&.widgits/_shared-lib/harness/results/%s.txt", HOUSE, r->target); FILE *fp = fopen(f, "r"); if (!fp) return;
    int ok = 0, fail = 0; char ln[1024]; while (fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, "RUN|", 4)) { ok = 0; fail = 0; }   /* the file is append-only: only the LAST run counts, or two runs read as a doubled total */
        else if (!strncmp(ln, "PASS|", 5)) ok++; else if (!strncmp(ln, "FAIL|", 5)) fail++; } fclose(fp);
    if (fail) snprintf(out, cap, " [%d FAIL]", fail); else if (ok) snprintf(out, cap, " [%d ok]", ok);
}
static int write_atomic(const char *path, const char *text) { char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return 1; fputs(text, f); if (fclose(f)) return 1; return rename(tmp, path) ? 1 : 0; }

/* PLUGS (D74, 2026-11-14): the open book's own <app>/plugs/<name>.plug.pdl files are listed in the same dropdown as plug rows (`[x] name` on, `[ ] name` off, id `plug:<name>`); picking one flips its
 * `ENABLED | 1/0` row and appends one row to <app>/state/plug_log.txt (TOGGLE | epoch | plug | from | to; capped at 32 KB, then the oldest half is dropped). Only the toggle lives here: what a plug DOES is its own file. */
#define MAXPLUG 16
static int plug_names(const char *book, char names[][48], int max) {
    char d[P]; snprintf(d, sizeof d, "%s/@.apps/%s/plugs", HOUSE, book); DIR *dp = opendir(d); if (!dp) return 0; int n = 0; struct dirent *e;
    while ((e = readdir(dp)) && n < max) { size_t l = strlen(e->d_name); if (l > 9 && !strcmp(e->d_name + l - 9, ".plug.pdl") && l - 9 < 48) { char nm[48]; snprintf(nm, sizeof nm, "%.*s", (int)(l - 9), e->d_name); if (word_ok(nm)) snprintf(names[n++], 48, "%s", nm); } }
    closedir(dp); for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcmp(names[i], names[j]) > 0) { char t[48]; snprintf(t, 48, "%s", names[i]); snprintf(names[i], 48, "%s", names[j]); snprintf(names[j], 48, "%s", t); }
    return n;
}
static const char *g_sim_name; static int g_sim_val = -1;      /* the loader's what-if: treat this plug as ON or OFF without touching the file */
static int plug_enabled(const char *book, const char *name) {
    if (g_sim_name && !strcmp(name, g_sim_name)) { return g_sim_val; }      /* 1 on, 0 off or no ENABLED row */
    char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, name); FILE *fp = fopen(f, "r"); if (!fp) return 0; char ln[600]; int on = 0;
    while (fgets(ln, sizeof ln, fp)) { char *p = trim(ln); if (!strncmp(p, "ENABLED", 7)) { char *bar = strchr(p, '|'); if (bar) on = atoi(bar + 1) == 1; } } fclose(fp); return on;
}
static int plug_toggle(const char *book, const char *name) {      /* 0 ok, 1 failed (no file, or no ENABLED row to flip) */
    char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, name); FILE *fp = fopen(f, "r"); if (!fp) return 1;
    static char buf[60000], out[60500]; size_t n = fread(buf, 1, sizeof buf - 1, fp); buf[n] = 0; fclose(fp); size_t o = 0; int hit = 0, from = 0;
    for (char *l = buf; *l; ) { char *nl = strchr(l, '\n'); size_t ll = nl ? (size_t)(nl - l) : strlen(l); char line[700]; snprintf(line, sizeof line, "%.*s", (int)(ll < 699 ? ll : 699), l);
        char *q = line; while (*q == ' ') q++;
        if (!hit && !strncmp(q, "ENABLED", 7) && strchr(q, '|')) { from = atoi(strchr(q, '|') + 1) == 1; o += (size_t)snprintf(out + o, sizeof out - o, "ENABLED | %d\n", from ? 0 : 1); hit = 1; }
        else { o += (size_t)snprintf(out + o, sizeof out - o, "%.*s\n", (int)ll, l); }
        if (!nl) { break; } l = nl + 1; }
    if (!hit) { return 1; } if (write_atomic(f, out)) { return 1; }
    char lg[P]; snprintf(lg, sizeof lg, "%s/@.apps/%s/state/plug_log.txt", HOUSE, book); FILE *lf = fopen(lg, "a");
    if (lf) { fprintf(lf, "TOGGLE | %ld | %s | %d | %d\n", (long)time(NULL), name, from, from ? 0 : 1); fclose(lf); struct stat st; if (stat(lg, &st) == 0 && st.st_size > 32768) { FILE *rf = fopen(lg, "r"); if (rf) { static char kb[40000]; size_t kn = fread(kb, 1, sizeof kb - 1, rf); fclose(rf); kb[kn] = 0; char *mid = strchr(kb + kn / 2, '\n'); if (mid) write_atomic(lg, mid + 1); } } }
    return 0;
}
static int run_child(char *const argv[], const char *cwd, const char *outfile, int cap_s) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { if (cwd && chdir(cwd)) _exit(126); int fd = open(outfile ? outfile : "/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) { dup2(fd, 1); dup2(open("/dev/null", O_WRONLY), 2); } execv(argv[0], argv); _exit(127); }
    for (int i = 0; i < cap_s * 10; i++) { int st; pid_t r = waitpid(pid, &st, WNOHANG); if (r == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0); return -1;
}
static void expand(const char *in, const char *book, const char *page, char *out, size_t cap) {
    char app[P], cells[P]; snprintf(app, sizeof app, "%s/@.apps/%s", HOUSE, book);
    snprintf(cells, sizeof cells, "%s/@.apps/%s/%s-pchq/pieces/system/maps/%s/%s/cells.txt", HOUSE, book, book, book, page);
    size_t o = 0; for (const char *p = in; *p && o + 1 < cap; ) {
        const char *nm[] = { "${HOUSE}", "${BOOK}", "${PAGE}", "${APP}", "${CELLS}" }; const char *vl[] = { HOUSE, book, page, app, cells }; int hit = 0;
        for (int i = 0; i < 5; i++) { size_t l = strlen(nm[i]); if (!strncmp(p, nm[i], l)) { for (const char *v = vl[i]; *v && o + 1 < cap; v++) out[o++] = *v; p += l; hit = 1; break; } }
        if (!hit) out[o++] = *p++; }
    out[o] = 0;
}

/* PLUG HEADER (spec step 3, 2026-11-14): ONE scanner for a plug file, used by `check` (every problem is a bad row) and by the plugs window (`describe` shows the header and the problems).
 * Header rows (all optional except name and ENABLED):  PLUG | name | <file name>   PLUG | version | <whole number >= 1>   ENABLED | 0 or 1   AFFECTS | view or outcome (at most one)
 *   REQUIRES | <plug>   CONFLICTS | <plug>   USES | <verb>      and typed rows  PARAM | key | value | type | min | max | help  (value, type and bounds are checked by the SAME rules the engine uses, plug_ref_value_ok).
 * Cross-plug rules inside one book: a REQUIRES must name an installed plug (and it must be ON while this one is ON); a CONFLICTS must not be ON while this one is ON; a plug may not require or conflict with itself.
 * USES is validated as a word only: refusing an undeclared verb needs the engine (spec step 4, not built). Returns the number of problems written to msgs (at most maxm). */
typedef struct { int has_name, has_enabled, enabled, version, nreq, ncon, nuses, nparams, ncmd, nphase; char affects[16]; char req[8][48], con[8][48], uses[12][48], cmd[12][48]; int phase_hour[12]; } PlugInfo;
static int plug_scan(const char *book, const char *name, PlugInfo *pi, char msgs[][200], int maxm) {
    memset(pi, 0, sizeof *pi); int nm = 0;
#define PMSG(...) do { if (nm < maxm) { snprintf(msgs[nm], 200, __VA_ARGS__); } nm++; } while (0)
    char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, name); FILE *fp = fopen(f, "r"); if (!fp) { PMSG("the plug file is missing"); return nm > maxm ? maxm : nm; }
    static char pn[MAXPLUG][48]; int np = plug_names(book, pn, MAXPLUG); char keys[40][48]; int nk = 0, n_aff = 0; char ln[700];
    while (fgets(ln, sizeof ln, fp)) { char *p = trim(ln); if (!*p || *p == '#' || *p == '-') continue; char *fl[7]; int nf = plug_ref_split(p, fl, 7); if (nf < 2) continue;
        if (!strcmp(fl[0], "PLUG")) {
            if (!strcmp(fl[1], "name") && nf >= 3) { if (!strcmp(fl[2], name)) pi->has_name = 1; else PMSG("PLUG name is %s but the file is %s.plug.pdl", fl[2], name); }
            else if (!strcmp(fl[1], "version") && nf >= 3) { char *e; long v = strtol(fl[2], &e, 10); if (!*fl[2] || *e || v < 1 || v > 9999) PMSG("PLUG version must be a whole number from 1 to 9999"); else pi->version = (int)v; }
        } else if (!strcmp(fl[0], "ENABLED")) { pi->has_enabled = 1; if (!strcmp(fl[1], "1")) pi->enabled = 1; else if (strcmp(fl[1], "0")) PMSG("ENABLED must be 0 or 1"); }
        else if (!strcmp(fl[0], "AFFECTS")) { if (strcmp(fl[1], "view") && strcmp(fl[1], "outcome")) PMSG("AFFECTS must be view or outcome"); else snprintf(pi->affects, sizeof pi->affects, "%s", fl[1]); if (++n_aff == 2) PMSG("more than one AFFECTS row"); }
        else if (!strcmp(fl[0], "REQUIRES") || !strcmp(fl[0], "CONFLICTS") || !strcmp(fl[0], "USES")) {
            int is_req = !strcmp(fl[0], "REQUIRES"), is_con = !strcmp(fl[0], "CONFLICTS"); if (!word_ok(fl[1]) || strlen(fl[1]) > 40) { PMSG("%s %s: not a plain name", fl[0], fl[1]); continue; }
            if (is_req || is_con) { if (!strcmp(fl[1], name)) { PMSG("%s %s: a plug cannot %s itself", fl[0], fl[1], is_req ? "require" : "conflict with"); continue; }
                if (is_req && pi->nreq < 8) { snprintf(pi->req[pi->nreq++], 48, "%s", fl[1]); }
                if (is_con && pi->ncon < 8) { snprintf(pi->con[pi->ncon++], 48, "%s", fl[1]); }
                int found = 0; for (int i = 0; i < np; i++) if (!strcmp(pn[i], fl[1])) found = 1;
                if (is_req && !found) PMSG("REQUIRES %s: no plug %s in this book", fl[1], fl[1]); }
            else if (pi->nuses < 12) snprintf(pi->uses[pi->nuses++], 48, "%s", fl[1]);
        } else if (!strcmp(fl[0], "COMMAND")) {      /* COMMAND | name | event or verb | args: a named thing the plug can do (the loader checks the names do not collide across plugs) */
            if (nf < 3 || !plug_ref_word(fl[1], 0) || strlen(fl[1]) > 40 || !word_ok(fl[2])) { PMSG("COMMAND needs name | event or verb (plain names)"); continue; }
            int dc = 0; for (int i = 0; i < pi->ncmd; i++) if (!strcmp(pi->cmd[i], fl[1])) dc = 1; if (dc) { PMSG("COMMAND %s: declared twice", fl[1]); continue; } if (pi->ncmd < 12) snprintf(pi->cmd[pi->ncmd++], 48, "%s", fl[1]);
        } else if (!strcmp(fl[0], "PHASE")) {      /* PHASE | name | from_hour | ...: a plug that installs a clock reminder at from_hour (day-night) */
            if (nf >= 3 && pi->nphase < 12) { char *e2; long h = strtol(fl[2], &e2, 10); if (*fl[2] && !*e2 && h >= 0 && h <= 23) pi->phase_hour[pi->nphase++] = (int)h; }
        } else if (!strcmp(fl[0], "PARAM")) {
            pi->nparams++; if (nf < 4) { PMSG("PARAM needs key | value | type"); continue; }
            if (!plug_ref_word(fl[1], 0) || strlen(fl[1]) > 40) { PMSG("PARAM %s: the key must be letters, digits and _", fl[1]); continue; }
            int dup = 0; for (int i = 0; i < nk; i++) if (!strcmp(keys[i], fl[1])) dup = 1; if (dup) { PMSG("PARAM %s: declared twice", fl[1]); continue; } if (nk < 40) snprintf(keys[nk++], 48, "%s", fl[1]);
            char why[200]; if (plug_ref_value_ok(name, fl[1], fl[3], fl[2], nf > 4 ? fl[4] : "", nf > 5 ? fl[5] : "", why, sizeof why)) PMSG("PARAM %s", why);
        } }
    fclose(fp);
    if (!pi->has_name) { int mism = 0; for (int i = 0; i < nm && i < maxm; i++) if (!strncmp(msgs[i], "PLUG name is", 12)) mism = 1; if (!mism) PMSG("a plug file needs a row PLUG | name | <the file name>"); }
    if (g_sim_name && !strcmp(name, g_sim_name)) { pi->enabled = g_sim_val; }
    if (!pi->has_enabled) PMSG("a plug file needs an ENABLED | 1 or 0 row");
    if (pi->enabled) { for (int i = 0; i < pi->nreq; i++) { int found = 0; for (int k = 0; k < np; k++) if (!strcmp(pn[k], pi->req[i])) found = 1; if (found && !plug_enabled(book, pi->req[i])) PMSG("is ON but REQUIRES %s which is OFF", pi->req[i]); }
        for (int i = 0; i < pi->ncon; i++) if (plug_enabled(book, pi->con[i])) PMSG("is ON and CONFLICTS with %s which is also ON", pi->con[i]); }
#undef PMSG
    return nm > maxm ? maxm : nm;
}

/* PLUG LOADER (spec step 3, the other half, D78, 2026-11-14): which plugs are ON, in what order they load, and what collides. `harness_menu_op load <house> <book>` writes the PROJECTION <app>/state/plugs_active.txt
 * (ORDER rows, then PROBLEM rows; safe to delete, regenerate with this verb; nothing reads it as authority) and exits 2 when there is a problem. Rules, all data: a plug that is ON and has a header problem is reported (the same scan as `check`);
 * the ON plugs are ordered so a plug comes after every ON plug it REQUIRES (a cycle is reported); two ON plugs may not declare the same COMMAND name, and may not install a clock reminder (PHASE from_hour) at the same hour.
 * A `pick plug:<name>` that would switch a plug ON is REFUSED when loading with it ON gives a problem that names it (nothing is changed, no log row): the loader runs on the would-be state through g_sim_name/g_sim_val.
 * Switching OFF is always allowed (a plug that REQUIRES it is then reported, not blocked). Page-based commands on high-frequency triggers are refused by the trigger check (D79); no such trigger source exists yet. */
typedef struct { int n, nprob; char order[MAXPLUG][48]; int ver[MAXPLUG]; char aff[MAXPLUG][16]; char prob[24][200]; } PlugLoad;
static void plug_load(const char *book, PlugLoad *L) {
    memset(L, 0, sizeof *L); static char pn[MAXPLUG][48]; int np = plug_names(book, pn, MAXPLUG); static PlugInfo A[MAXPLUG]; static char an[MAXPLUG][48]; int na = 0; static char pm[24][200];
#define LPROB(...) do { if (L->nprob < 24) { snprintf(L->prob[L->nprob], 200, __VA_ARGS__); } L->nprob++; } while (0)
    for (int i = 0; i < np; i++) { PlugInfo pi; int nm = plug_scan(book, pn[i], &pi, pm, 24); if (!(pi.has_enabled && pi.enabled)) continue; for (int m = 0; m < nm; m++) LPROB("plug %s: %s", pn[i], pm[m]); if (na < MAXPLUG) { A[na] = pi; snprintf(an[na++], 48, "%s", pn[i]); } }
    int placed[MAXPLUG] = { 0 }, nplaced = 0, progress = 1;
    while (progress && nplaced < na) { progress = 0; for (int i = 0; i < na; i++) { if (placed[i]) continue; int ok = 1;
            for (int r = 0; r < A[i].nreq; r++) for (int j = 0; j < na; j++) if (!strcmp(an[j], A[i].req[r]) && !placed[j]) ok = 0;
            if (ok) { placed[i] = 1; nplaced++; progress = 1; snprintf(L->order[L->n], 48, "%s", an[i]); L->ver[L->n] = A[i].version; snprintf(L->aff[L->n], 16, "%s", A[i].affects[0] ? A[i].affects : "-"); L->n++; } } }
    if (nplaced < na) { char t[200] = ""; for (int i = 0; i < na; i++) if (!placed[i]) { size_t l = strlen(t); snprintf(t + l, sizeof t - l, "%s%s", t[0] ? ", " : "", an[i]); } LPROB("CYCLE: plugs that require each other cannot load: %s", t); }
    for (int i = 0; i < na; i++) for (int j = i + 1; j < na; j++) {
        for (int a = 0; a < A[i].ncmd; a++) for (int b = 0; b < A[j].ncmd; b++) if (!strcmp(A[i].cmd[a], A[j].cmd[b])) LPROB("COLLISION: COMMAND %s is declared by both %s and %s", A[i].cmd[a], an[i], an[j]);
        for (int a = 0; a < A[i].nphase; a++) for (int b = 0; b < A[j].nphase; b++) if (A[i].phase_hour[a] == A[j].phase_hour[b]) LPROB("COLLISION: %s and %s both install a clock reminder at hour %d", an[i], an[j], A[i].phase_hour[a]); }
#undef LPROB
}
static int plug_load_verb(const char *book) {
    if (!word_ok(book)) { fprintf(stderr, "harness_menu_op: load needs a plain book name\n"); return 2; }
    PlugLoad L; plug_load(book, &L); static char out[8192]; out[0] = 0; size_t l;
    l = strlen(out); snprintf(out + l, sizeof out - l, "# plugs_active.txt - a PROJECTION written by `harness_menu_op load` (D78): the ON plugs of this book in load order, then any problems. Regenerate it with that verb; never edit it; nothing reads it as authority.\n");
    for (int i = 0; i < L.n; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "ORDER | %d | %s | %d | %s\n", i + 1, L.order[i], L.ver[i], L.aff[i]); }
    for (int i = 0; i < L.nprob && i < 24; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "PROBLEM | %s\n", L.prob[i]); }
    char d[P], f[P]; snprintf(d, sizeof d, "%s/@.apps/%s", HOUSE, book); struct stat sb; if (!stat(d, &sb) && S_ISDIR(sb.st_mode)) { snprintf(d, sizeof d, "%s/@.apps/%s/state", HOUSE, book); mkdir(d, 0755); snprintf(f, sizeof f, "%s/plugs_active.txt", d); write_atomic(f, out); }
    printf("LOAD active=%d problems=%d\n", L.n, L.nprob); for (int i = 0; i < L.nprob && i < 24; i++) printf("%s\n", L.prob[i]); return L.nprob ? 2 : 0;
}

static void trig_links(const char *book, int is_plug, const char *name, char *fires, size_t fc, char *by, size_t bc);      /* defined with the triggers, below */
static int nt_;      /* the trigger count; defined with the trigger table below */
/* PLUGZ WINDOWS (2026-11-14, D76): the three windows (plugs, harnz, tests) read their rows and details through these verbs, so the parsing and the listing rules stay HERE (one authority).
 *   harness_menu_op rows     <house> <book|-> <page|-> <plugs|harnz|tests> <out>        lines `label<TAB>id<TAB>state` (plug: on/off, test: last verdict like `37 ok`, harnz: -)
 *   harness_menu_op describe <house> <book|-> <page|-> <plugs|harnz|tests> <id> <out>   key=value lines: title, status, control (toggle|click|run), confirm, n_info, info_<n>
 * The same hiding rules as `list` apply (a book with nothing of its own lists no global tests; an own row that repeats a global one is hidden). */
static void clean_line(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r') *s = ' '; }
static int kind_ok(const char *k) { return !strcmp(k, "plugs") || !strcmp(k, "harnz") || !strcmp(k, "tests"); }
static void one_line(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }
static int plugz_rows(const char *book, const char *page, const char *kind, const char *outf) {
    int named = book[0] && strcmp(book, "-") && word_ok(book); if (!named) book = ""; load_all(book);
    int nown = 0; for (int i = 0; i < nr; i++) if (!R[i].global) nown++;
    static char pn[MAXPLUG][48]; int np = named ? plug_names(book, pn, MAXPLUG) : 0; g_hide_global = named && ((!nown && !np) || g_book_no_global);
    static char out[MAXR * 200 + MAXPLUG * 100 + 400]; out[0] = 0; size_t l;
    if (!strcmp(kind, "plugs")) { for (int i = 0; i < np; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s\tplug:%s\t%s\n", pn[i], pn[i], plug_enabled(book, pn[i]) ? "on" : "off"); } }
    else for (int i = 0; i < nr; i++) { int is_case = !strcmp(R[i].kind, "CASE"); if (!listed(i, page) || is_case != !strcmp(kind, "tests")) continue;
        char sfx[24]; verdict_suffix(&R[i], sfx, sizeof sfx); const char *s = sfx[0] == ' ' ? sfx + 2 : sfx; char st[24]; snprintf(st, sizeof st, "%s", s[0] ? s : "-"); char *rb = strchr(st, ']'); if (rb) *rb = 0;
        l = strlen(out); snprintf(out + l, sizeof out - l, "%s\t%s\t%s\n", R[i].label, R[i].id, st); }
    return write_atomic(outf, out);
}
static int plugz_describe(const char *book, const char *page, const char *kind, const char *id, const char *outf) {
    int named = book[0] && strcmp(book, "-") && word_ok(book); if (!named) book = ""; static char out[8192]; out[0] = 0; size_t l; int ni = 0; char info[24][240];
    #define PUT(...) do { l = strlen(out); snprintf(out + l, sizeof out - l, __VA_ARGS__); } while (0)
    #define INFO(...) do { if (ni < 24) { snprintf(info[ni], sizeof info[ni], __VA_ARGS__); one_line(info[ni]); ni++; } } while (0)
    if (!strcmp(kind, "plugs")) {
        const char *pname = !strncmp(id, "plug:", 5) ? id + 5 : id; if (!named || !word_ok(pname)) { PUT("title=\nstatus=\ncontrol=none\nconfirm=no\nn_info=0\n"); return write_atomic(outf, out); }
        char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, pname); FILE *fp = fopen(f, "r");
        PUT("title=%s\nstatus=%s\ncontrol=toggle\nconfirm=no\n", pname, plug_enabled(book, pname) ? "on" : "off");
        INFO("file @.apps/%s/plugs/%s.plug.pdl", book, pname);
        { static char pm[24][200]; PlugInfo pi; int nmsg = plug_scan(book, pname, &pi, pm, 24);      /* the header (spec step 3) and any problems, first, so they are never cut off */
          if (pi.version || pi.affects[0]) INFO("version %d   affects %s", pi.version, pi.affects[0] ? pi.affects : "(not declared)");
          if (pi.nreq) { char t[200] = ""; for (int i = 0; i < pi.nreq; i++) { size_t l2 = strlen(t); snprintf(t + l2, sizeof t - l2, "%s%s", i ? ", " : "", pi.req[i]); } INFO("requires %s", t); }
          if (pi.ncon) { char t[200] = ""; for (int i = 0; i < pi.ncon; i++) { size_t l2 = strlen(t); snprintf(t + l2, sizeof t - l2, "%s%s", i ? ", " : "", pi.con[i]); } INFO("conflicts with %s", t); }
          if (pi.nuses) { char t[200] = ""; for (int i = 0; i < pi.nuses; i++) { size_t l2 = strlen(t); snprintf(t + l2, sizeof t - l2, "%s%s", i ? ", " : "", pi.uses[i]); } INFO("uses %s", t); }
          for (int m = 0; m < nmsg && m < 4; m++) INFO("PROBLEM: %s", pm[m]); }
        if (fp) { char ln[600]; while (fgets(ln, sizeof ln, fp)) { char *p = trim(ln); if (!strncmp(p, "PARAM", 5)) { char *fl[7]; int n = 0; for (char *t = strtok(p, "|"); t && n < 7; t = strtok(NULL, "|")) fl[n++] = trim(t); if (n >= 4) INFO("%s = %s  (%s)", fl[1], fl[2], fl[3]); }
                else if (!strncmp(p, "PHASE", 5)) { char *fl[7]; int n = 0; for (char *t = strtok(p, "|"); t && n < 7; t = strtok(NULL, "|")) fl[n++] = trim(t); if (n >= 3) INFO("phase %s from hour %s", fl[1], fl[2]); } } fclose(fp); }
    } else {
        load_all(book); Row *r = NULL; for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, id) && visible(&R[i], page)) r = &R[i];
        int want_case = !strcmp(kind, "tests"); if (!r || ((!strcmp(r->kind, "CASE")) != (want_case != 0))) { PUT("title=\nstatus=\ncontrol=none\nconfirm=no\nn_info=0\n"); return write_atomic(outf, out); }
        char sfx[24]; verdict_suffix(r, sfx, sizeof sfx); PUT("title=%s\nstatus=%s\ncontrol=%s\nconfirm=%s\n", r->label, sfx[0] ? sfx + 1 : "-", want_case ? "run" : "click", r->confirm);
        if (want_case) { INFO("case %s", r->target); INFO("runs behind cpu_gate, one at a time"); } else { INFO("runs %s", r->target); if (strcmp(r->args, "-")) INFO("args %s", r->args); INFO("writes %s", r->writes); }
        { size_t hl = strlen(HOUSE); INFO("row from %s line %d", (!strncmp(r->src, HOUSE, hl) && r->src[hl] == '/') ? r->src + hl + 1 : r->src, r->line); }
    }
    { char fi[500], by[500]; const char *nm_ = !strcmp(kind, "plugs") ? (!strncmp(id, "plug:", 5) ? id + 5 : id) : id; trig_links(named ? book : "", !strcmp(kind, "plugs"), nm_, fi, sizeof fi, by, sizeof by); clean_line(fi); clean_line(by); PUT("fires=%s\nfiredby=%s\nn_trig=%d\n", fi, by, nt_); }
    PUT("n_info=%d\n", ni); for (int i = 0; i < ni; i++) PUT("info_%d=%s\n", i, info[i]);
    return write_atomic(outf, out);
}
/* OPEN (D76): `pick open:<kind>` starts the window for that kind. ONE layout (&.hq-apps/plugz-hq/plugz-hq.xhtpm) serves all three kinds through the renderer's generic instance-dir hook (argv[3] = <app>/instances/<kind>,
 * ${ARG3}); the instance's .hq_manager/ holds kind.txt, book.txt, page.txt (what to show), marker.txt (append-only wake-up, never rewritten) and window.pid. Started through the spawn funnel (registered). One window per kind:
 * a second pick only refreshes book and page and wakes the window. Prints WINDOW-OPENED <kind> <pid> | WINDOW-REFRESHED <kind> <pid> | WINDOW-MISSING <what>. */
static int plugz_open(const char *kind, const char *book, const char *page) {
    char app[P], inst[P], hq[P], f[P]; snprintf(app, sizeof app, "%s/&.hq-apps/plugz-hq", HOUSE); snprintf(inst, sizeof inst, "%s/instances/%s", app, kind); snprintf(hq, sizeof hq, "%s/.hq_manager", inst);
    char x[P], exe[P], funnel[P]; snprintf(x, sizeof x, "%s/plugz-hq.xhtpm", app); snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", HOUSE); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    char d1[P]; snprintf(d1, sizeof d1, "%s/instances", app); mkdir(d1, 0755); mkdir(inst, 0755); mkdir(hq, 0755);
    /* a window opened for a DIFFERENT book or page than the instance last showed starts clean: selection, result line and any pending confirm belong to the old one (seen live 2026-11-14: a result from another book showed in an empty book's window) */
    { char ob[160] = "", op_[160] = ""; FILE *q; snprintf(f, sizeof f, "%s/book.txt", hq); if ((q = fopen(f, "r"))) { if (fgets(ob, sizeof ob, q)) { ob[strcspn(ob, "\r\n")] = 0; } fclose(q); }
      snprintf(f, sizeof f, "%s/page.txt", hq); if ((q = fopen(f, "r"))) { if (fgets(op_, sizeof op_, q)) { op_[strcspn(op_, "\r\n")] = 0; } fclose(q); }
      if (strcmp(ob, book[0] ? book : "-") || strcmp(op_, page[0] ? page : "-")) { const char *gone[] = { "sel.txt", "last.txt", "pending.txt" }; for (int i = 0; i < 3; i++) { snprintf(f, sizeof f, "%s/%s", hq, gone[i]); unlink(f); } } }
    snprintf(f, sizeof f, "%s/kind.txt", hq); write_atomic(f, kind); snprintf(f, sizeof f, "%s/book.txt", hq); write_atomic(f, book[0] ? book : "-"); snprintf(f, sizeof f, "%s/page.txt", hq); write_atomic(f, page[0] ? page : "-");
    snprintf(f, sizeof f, "%s/marker.txt", hq); { FILE *mf = fopen(f, "a"); if (mf) { fputc('x', mf); fclose(mf); } struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); }      /* capped: a marker over 4 KB is restarted */
    if (access(x, R_OK)) { printf("WINDOW-MISSING layout\n"); return 1; } if (access(exe, X_OK)) { printf("WINDOW-MISSING renderer\n"); return 1; } if (access(funnel, R_OK)) { printf("WINDOW-MISSING spawn-funnel\n"); return 1; }      /* checked AFTER the instance files are written, so the files are right even when nothing can start */
    char pf[P]; snprintf(pf, sizeof pf, "%s/window.pid", inst); char pb[32] = ""; FILE *pp = fopen(pf, "r"); if (pp) { if (fgets(pb, sizeof pb, pp)) { } fclose(pp); } long pid = atol(pb);
    if (pid > 1 && !kill((pid_t)pid, 0)) { char cl[P]; snprintf(cl, sizeof cl, "/proc/%ld/cmdline", pid); FILE *cp = fopen(cl, "r"); char c[512] = ""; if (cp) { size_t n = fread(c, 1, sizeof c - 1, cp); c[n] = 0; fclose(cp); } if (strstr(c, "khtpm_core_render")) { printf("WINDOW-REFRESHED %s %ld\n", kind, pid); return 0; } }
    setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "plugz-hq", 1); char nm[64]; snprintf(nm, sizeof nm, "plugz-%s", kind); char tmp[P]; snprintf(tmp, sizeof tmp, "%s/spawn.out", inst);
    char *av[] = { "/bin/sh", funnel, HOUSE, "0", nm, "--", exe, HOUSE, x, inst, NULL }; run_child(av, HOUSE, tmp, 20);
    FILE *tp = fopen(tmp, "r"); char out[64] = ""; if (tp) { if (fgets(out, sizeof out, tp)) { } fclose(tp); } unlink(tmp); long np = atol(out); if (np <= 1) { printf("WINDOW-MISSING spawn-failed\n"); return 1; }
    char num[32]; snprintf(num, sizeof num, "%ld\n", np); write_atomic(pf, num); printf("WINDOW-OPENED %s %ld\n", kind, np); return 0;
}
/* DIR (2026-11-14, owner: "add dir to tools option, in edit also, so a human can easily open the tools dir for that book"): `pick open:dir` opens the book's own folder (<house>/@.apps/<book>/, where harness.pdl and plugs/ live)
 * in the system file manager, the way the HQ menu's own `dir` row does (xdg-open). Needs an open book; a book with no folder yet prints DIR-MISSING <book> and opens nothing (the folder is never created here). */
static int open_dir(const char *book) {
    if (!book[0] || !word_ok(book)) { printf("DIR-MISSING no open book\n"); return 1; }
    char d[P]; snprintf(d, sizeof d, "%s/@.apps/%s", HOUSE, book); struct stat sb; if (stat(d, &sb) || !S_ISDIR(sb.st_mode)) { printf("DIR-MISSING %s\n", book); return 1; }
    const char *opener = getenv("HARNESS_MENU_OPENER"); if (!opener || !opener[0]) opener = "/usr/bin/xdg-open";      /* the env var is a TEST HOOK (a case swaps in a harmless program so no file manager opens) */
    if (access(opener, X_OK)) { printf("DIR-MISSING opener\n"); return 1; }
    char *av[] = { (char *)opener, d, NULL }; run_child(av, HOUSE, "/dev/null", 10); printf("DIR-OPENED %s\n", d); return 0;
}
/* TRIGGERS (spec step 4b, D79, 2026-11-14): a plug and a harnz can fire each other. Config, one file per book: <app>/triggers.pdl, rows   TRIGGER | id | on | run | note
 *   on   toggle:<plug>:on | toggle:<plug>:off   a plug was switched          done:<id> | failed:<id>   a harnz or test row finished (ok / not ok)
 *   run  harnz:<id>   an ACTION row    test:<id>   a CASE row    plug:<plug>:on | plug:<plug>:off   switch a plug
 * Guards (all enforced at run time, and the first two also by `check`): a target already in the chain of this fire is REFUSED (a loop); the depth is capped at 4; at most 12 fires per run; a row that asks for confirmation is never
 * confirmed by a trigger (refused); a plug switched ON through a trigger goes through the same loader refusal as a click. Every fire and every refusal is ONE row of <app>/state/trigger_log.txt (capped, 32 KB):
 *   FIRE | epoch | trigger id | on | run | depth | ok | failed | refused: <why>        (that row is the verdict row of an automated run; a test's own verdict is still in its results file)
 * No triggers.pdl = nothing changes for the book. Only click-like sources exist (no timer, no per-tick source), so a page-based command can never run at high frequency; a clock source would need the spec's refusal rule first. */
#define MAXTRIG 32
#define TRIG_DEPTH 4
#define TRIG_FIRES 12
typedef struct { char id[48], on[96], run[96], note[120]; int line; } Trig;
static Trig T_[MAXTRIG]; static int nt_; static int g_fires;
static int trig_split(const char *s, char out[3][48]) { int n = 0; const char *p = s; while (n < 3) { const char *c = strchr(p, ':'); size_t l = c ? (size_t)(c - p) : strlen(p); if (l >= 48) return 0; memcpy(out[n], p, l); out[n][l] = 0; n++; if (!c) break; p = c + 1; } return n; }
static int trig_on_ok(const char *on, char *why, size_t wc) {
    char x[3][48]; int n = trig_split(on, x);
    if (n == 3 && !strcmp(x[0], "toggle") && word_ok(x[1]) && (!strcmp(x[2], "on") || !strcmp(x[2], "off"))) return 1;
    if (n == 2 && (!strcmp(x[0], "done") || !strcmp(x[0], "failed")) && word_ok(x[1])) return 1;
    snprintf(why, wc, "on %s: want toggle:<plug>:on|off, done:<id> or failed:<id>", on); return 0;
}
static int trig_run_ok(const char *run, char *why, size_t wc) {
    char x[3][48]; int n = trig_split(run, x);
    if (n == 2 && (!strcmp(x[0], "harnz") || !strcmp(x[0], "test")) && word_ok(x[1])) return 1;
    if (n == 3 && !strcmp(x[0], "plug") && word_ok(x[1]) && (!strcmp(x[2], "on") || !strcmp(x[2], "off"))) return 1;
    snprintf(why, wc, "run %s: want harnz:<id>, test:<id> or plug:<plug>:on|off", run); return 0;
}
/* read <app>/triggers.pdl: the valid rows go to T_ (what fires at run time), every problem goes to msgs (what `check` prints); runs with maxm = 0 at run time */
static int trig_scan(const char *book, char msgs[][200], int maxm) {
    nt_ = 0; int nm = 0;
#define TMSG(...) do { if (nm < maxm) { snprintf(msgs[nm], 200, __VA_ARGS__); } nm++; } while (0)
    char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/triggers.pdl", HOUSE, book); FILE *fp = fopen(f, "r"); if (!fp) return 0; char ln[700]; int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) { lineno++; char *p = trim(ln); if (!*p || *p == '#') continue; char *fl[6]; int nf = plug_ref_split(p, fl, 5); if (strcmp(fl[0], "TRIGGER")) continue;
        char why[200]; if (nf < 4) { TMSG("line %d: a TRIGGER row needs id | on | run | note", lineno); continue; }
        if (!word_ok(fl[1]) || strlen(fl[1]) > 40) { TMSG("line %d: the trigger id must be a plain word", lineno); continue; }
        if (!trig_on_ok(fl[2], why, sizeof why)) { TMSG("line %d: %s", lineno, why); continue; } if (!trig_run_ok(fl[3], why, sizeof why)) { TMSG("line %d: %s", lineno, why); continue; }
        int dup = 0; for (int i = 0; i < nt_; i++) if (!strcmp(T_[i].id, fl[1])) dup = 1; if (dup) { TMSG("line %d: trigger id %s is declared twice", lineno, fl[1]); continue; }
        if (nt_ >= MAXTRIG) { TMSG("line %d: too many triggers (at most %d)", lineno, MAXTRIG); continue; }
        Trig *t = &T_[nt_++]; snprintf(t->id, sizeof t->id, "%s", fl[1]); snprintf(t->on, sizeof t->on, "%s", fl[2]); snprintf(t->run, sizeof t->run, "%s", fl[3]); snprintf(t->note, sizeof t->note, "%s", nf > 4 ? fl[4] : ""); t->line = lineno; }
    fclose(fp);
#undef TMSG
    return nm > maxm ? maxm : nm;
}
/* the events a `run` produces when it is carried out: a row gives done and failed, a plug switch gives its toggle */
static int trig_produces(const char *run, char ev[2][100]) { char x[3][48]; int n = trig_split(run, x); if (n == 2) { snprintf(ev[0], 100, "done:%s", x[1]); snprintf(ev[1], 100, "failed:%s", x[1]); return 2; } if (n == 3) { snprintf(ev[0], 100, "toggle:%s:%s", x[1], x[2]); return 1; } return 0; }
static int trig_dfs(const char *e, char path[][100], int depth, char *loop, size_t lc) {
    for (int i = 0; i < nt_; i++) if (!strcmp(T_[i].on, e)) { char ev[2][100]; int ne = trig_produces(T_[i].run, ev);
        for (int k = 0; k < ne; k++) { for (int d = 0; d < depth; d++) if (!strcmp(path[d], ev[k])) { snprintf(loop, lc, "%s", ev[k]); return 1; } if (depth >= 12) continue; snprintf(path[depth], 100, "%s", ev[k]); if (trig_dfs(ev[k], path, depth + 1, loop, lc)) return 1; } }
    return 0;
}
/* `check` for one book: the file's own problems, then every id must exist (a plug, a row of the right kind), then no loop among the triggers (the static twin of the runtime chain guard) */
static void trig_verify(const char *book, const char *src) {
    static char pm[24][200]; int nm = trig_scan(book, pm, 24); for (int i = 0; i < nm; i++) bad(src, 0, pm[i]);
    static char pn[MAXPLUG][48]; int np = plug_names(book, pn, MAXPLUG);
    for (int i = 0; i < nt_; i++) { char x[3][48]; char msg[200]; const char *two[2] = { T_[i].on, T_[i].run };
        for (int w = 0; w < 2; w++) { int n = trig_split(two[w], x); const char *kind = x[0]; if (n < 2) continue; int is_plug = !strcmp(kind, "toggle") || !strcmp(kind, "plug");
            if (is_plug) { int found = 0; for (int k = 0; k < np; k++) if (!strcmp(pn[k], x[1])) found = 1; if (!found) { snprintf(msg, sizeof msg, "trigger %s: no plug %s in this book", T_[i].id, x[1]); bad(src, T_[i].line, msg); } }
            else { Row *r = NULL; for (int k = 0; k < nr; k++) if (!strcmp(R[k].id, x[1])) r = &R[k]; if (!r) { snprintf(msg, sizeof msg, "trigger %s: no harnz or test row %s", T_[i].id, x[1]); bad(src, T_[i].line, msg); }
                else if (w == 1 && !strcmp(kind, "harnz") && !strcmp(r->kind, "CASE")) { snprintf(msg, sizeof msg, "trigger %s: %s is a test, not a harnz (use test:%s)", T_[i].id, x[1], x[1]); bad(src, T_[i].line, msg); }
                else if (w == 1 && !strcmp(kind, "test") && strcmp(r->kind, "CASE")) { snprintf(msg, sizeof msg, "trigger %s: %s is a harnz, not a test (use harnz:%s)", T_[i].id, x[1], x[1]); bad(src, T_[i].line, msg); } } } }
    static char path[12][100]; char loop[100] = "";
    for (int i = 0; i < nt_ && !loop[0]; i++) { snprintf(path[0], 100, "%s", T_[i].on); trig_dfs(T_[i].on, path, 1, loop, sizeof loop); }
    if (loop[0]) { char msg[200]; snprintf(msg, sizeof msg, "the triggers form a loop through %s (a chain that returns to itself is refused at run time)", loop); bad(src, 0, msg); }
}
static void trig_log(const char *book, const char *tid, const char *on, const char *run, int depth, const char *result) {
    char d[P], f[P]; snprintf(d, sizeof d, "%s/@.apps/%s/state", HOUSE, book); mkdir(d, 0755); snprintf(f, sizeof f, "%s/trigger_log.txt", d); FILE *lf = fopen(f, "a"); if (!lf) return;
    fprintf(lf, "FIRE | %ld | %s | %s | %s | %d | %s\n", (long)time(NULL), tid, on, run, depth, result); fclose(lf);
    struct stat st; if (!stat(f, &st) && st.st_size > 32768) { FILE *rf = fopen(f, "r"); if (rf) { static char kb[40000]; size_t kn = fread(kb, 1, sizeof kb - 1, rf); fclose(rf); kb[kn] = 0; char *half = kb + kn / 2; char *nl = strchr(half, '\n'); if (nl) write_atomic(f, nl + 1); } }      /* capped: the older half is dropped */
}
static int chain_has(const char *chain, const char *tid) { size_t l = strlen(tid); for (const char *p = chain; (p = strstr(p, tid)); p += l) { int start = (p == chain || p[-1] == '>'); int end = (p[l] == 0 || p[l] == '>'); if (start && end) return 1; } return 0; }
/* switch a plug on or off, for a click and for a trigger alike: 0 done (or already so), 1 refused with the loader's reason in why, 2 failed (no such plug, no ENABLED row). *changed = 1 when the file was flipped */
static int plug_switch(const char *book, const char *pname, int want_on, char *why, size_t wc, int *changed) {
    *changed = 0; why[0] = 0; static char pn2[MAXPLUG][48]; int np2 = plug_names(book, pn2, MAXPLUG), found = 0; for (int i = 0; i < np2; i++) if (!strcmp(pn2[i], pname)) found = 1;
    if (!found) { snprintf(why, wc, "no plug %s in book %s", pname, book); return 2; }
    if ((plug_enabled(book, pname) != 0) == (want_on != 0)) return 0;
    if (want_on) { g_sim_name = pname; g_sim_val = 1; PlugLoad LL; plug_load(book, &LL); g_sim_name = NULL; g_sim_val = -1;      /* switching ON: refuse when the loader finds a problem that names this plug (D78); nothing is changed */
        for (int i = 0; i < LL.nprob && i < 24; i++) if (strstr(LL.prob[i], pname)) { snprintf(why, wc, "%s", LL.prob[i]); return 1; } }
    if (plug_toggle(book, pname)) { snprintf(why, wc, "plug %s has no ENABLED row to flip", pname); return 2; }
    *changed = 1; return 0;
}
static int run_row(const char *book, const char *page, const char *id, int confirmed, int depth, const char *chain);
static void fire_triggers(const char *book, const char *page, const char *event, int depth, const char *chain);
/* a harnz or test row finished: fire what hangs on `done:<id>` or `failed:<id>` */
static void fire_after(const char *book, const char *page, const char *rid, int rc, int depth, const char *chain) {
    if (!book[0] || !word_ok(book) || rc > 1) return; char ev[120], tid[80], nc[400]; snprintf(ev, sizeof ev, "%s:%s", rc == 0 ? "done" : "failed", rid); snprintf(tid, sizeof tid, "row:%s", rid);
    if (chain_has(chain, tid)) snprintf(nc, sizeof nc, "%s", chain); else snprintf(nc, sizeof nc, "%s%s%s", chain, chain[0] ? ">" : "", tid);
    fire_triggers(book, page, ev, depth, nc);
}
static void fire_triggers(const char *book, const char *page, const char *event, int depth, const char *chain) {
    if (!book[0] || !word_ok(book)) return; trig_scan(book, NULL, 0); static Trig m[TRIG_DEPTH + 2][MAXTRIG]; int lvl = depth < TRIG_DEPTH + 1 ? depth : TRIG_DEPTH + 1; int nm = 0;      /* copied: a nested fire reloads T_ */
    for (int i = 0; i < nt_; i++) if (!strcmp(T_[i].on, event)) m[lvl][nm++] = T_[i];
    for (int i = 0; i < nm; i++) { Trig t = m[lvl][i]; char x[3][48]; int n = trig_split(t.run, x); char tid[100], nc[500], result[260] = "";
        if (n == 3) snprintf(tid, sizeof tid, "plug:%s", x[1]); else snprintf(tid, sizeof tid, "row:%s", x[1]);
        if (chain_has(chain, tid)) snprintf(result, sizeof result, "refused: loop (%s is already in the chain %s)", tid, chain);
        else if (depth + 1 > TRIG_DEPTH) snprintf(result, sizeof result, "refused: the chain is deeper than %d", TRIG_DEPTH);
        else if (++g_fires > TRIG_FIRES) snprintf(result, sizeof result, "refused: more than %d fires in one run", TRIG_FIRES);
        else { snprintf(nc, sizeof nc, "%s%s%s", chain, chain[0] ? ">" : "", tid);
            if (n == 3) { char why[200]; int ch = 0; int rc = plug_switch(book, x[1], !strcmp(x[2], "on"), why, sizeof why, &ch);
                if (rc == 0) { snprintf(result, sizeof result, "ok"); } else if (rc == 1) snprintf(result, sizeof result, "refused: %s", why); else snprintf(result, sizeof result, "failed: %s", why);
                trig_log(book, t.id, t.on, t.run, depth + 1, result); printf("TRIGGER %s %s\n", t.id, result);
                if (rc == 0 && ch) { char ev[120]; snprintf(ev, sizeof ev, "toggle:%s:%s", x[1], x[2]); fire_triggers(book, page, ev, depth + 1, nc); } continue; }
            load_all(book); Row *r = NULL; for (int k = 0; k < nr; k++) if (!strcmp(R[k].id, x[1]) && visible(&R[k], page)) r = &R[k];
            if (!r) snprintf(result, sizeof result, "refused: no row %s here", x[1]);
            else if ((!strcmp(r->kind, "CASE")) != (!strcmp(x[0], "test"))) snprintf(result, sizeof result, "refused: %s is a %s", x[1], !strcmp(r->kind, "CASE") ? "test" : "harnz");
            else { int rc = run_row(book, page, x[1], 0, depth + 1, nc); snprintf(result, sizeof result, "%s", rc == 0 ? "ok" : rc == 1 ? "failed" : rc == 3 ? "refused: that row asks for confirmation and a trigger never confirms" : "refused: it could not run here"); } }
        trig_log(book, t.id, t.on, t.run, depth + 1, result); printf("TRIGGER %s %s\n", t.id, result); }
}
/* the row a click runs, moved out of `pick` unchanged so a trigger runs the very same code: 0 ran ok, 1 ran and failed, 2 refused, 3 needs confirm */
static int run_row(const char *book, const char *page, const char *id, int confirmed, int depth, const char *chain) {
    load_all(book);
    Row *r = NULL; for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, id) && visible(&R[i], page)) r = &R[i];
    if (!r) { fprintf(stderr, "harness_menu_op: no row %s here\n", id); return 2; }
    if (!strcmp(r->confirm, "yes") && !confirmed) { printf("CONFIRM %s\n", r->id); return 3; }
    if (!strcmp(r->kind, "CASE")) {
        char hd[P], gate[P], cf[P], last[P], rf[P]; snprintf(hd, sizeof hd, "%s/&.widgits/_shared-lib/harness", HOUSE); snprintf(gate, sizeof gate, "%s/&.widgits/_shared-lib/ops/+x/cpu_gate.+x", HOUSE);
        snprintf(cf, sizeof cf, "cases/%s.pdl", r->target); snprintf(last, sizeof last, "%s/#.desktop/harness_last.txt", HOUSE); snprintf(rf, sizeof rf, "%s/results/%s.txt", hd, r->target); unlink(rf);
        char lab[64]; snprintf(lab, sizeof lab, "harness-%s", r->target);
        char *a1[] = { gate, HOUSE, "run", lab, "--wait", "120", "--", "./ops/+x/harness_case_op.+x", cf, NULL }; run_child(a1, hd, NULL, 900);
        char vp[P], tmp[P]; snprintf(vp, sizeof vp, "%s/ops/+x/harness_verdict_op.+x", hd); snprintf(tmp, sizeof tmp, "%s.tmp", last);
        char *a2[] = { vp, cf, NULL }; run_child(a2, hd, tmp, 30);
        /* keep only the verdict line (the last), so the file is one line and never grows */
        FILE *fp = fopen(tmp, "r"); char ln[1024], keep[1024] = ""; if (fp) { while (fgets(ln, sizeof ln, fp)) snprintf(keep, sizeof keep, "%s", ln); fclose(fp); } unlink(tmp);
        write_atomic(last, keep); printf("%s", keep[0] ? keep : "VERDICT|NONE\n"); { int crc = strstr(keep, "VERDICT|PASS") ? 0 : 1; char rid[48]; snprintf(rid, sizeof rid, "%s", r->id); fire_after(book, page, rid, crc, depth, chain); return crc; }
    }
    /* ACTION */
    int needs_page = strstr(r->args, "${CELLS}") || strstr(r->args, "${PAGE}") || strstr(r->args, "${BOOK}") || strstr(r->args, "${APP}") || !strcmp(r->writes, "page");
    if (needs_page && (!book[0] || !page[0])) { fprintf(stderr, "harness_menu_op: %s needs an open page\n", r->id); return 2; }
    char ex[P * 2]; expand(r->args, book, page, ex, sizeof ex); char exe[P]; snprintf(exe, sizeof exe, "%s/%s", HOUSE, r->target);
    char *av[24]; int ac = 0; av[ac++] = exe; char *sp; for (char *t = strtok_r(ex, " ", &sp); t && ac < 22; t = strtok_r(NULL, " ", &sp)) av[ac++] = t; av[ac] = NULL;
    int rc = run_child(av, HOUSE, NULL, 60);
    if (!strcmp(r->writes, "page")) { char mk[P]; snprintf(mk, sizeof mk, "%s/@.apps/%s/%s-pchq/pieces/system/maps/%s/%s/cells.marker", HOUSE, book, book, book, page); FILE *mf = fopen(mk, "a"); if (mf) { fputc('x', mf); fclose(mf); } }
    printf("HARNESS %s %s rc=%d\n", r->id, rc == 0 ? "ok" : "failed", rc); { int arc = rc == 0 ? 0 : 1; char rid[48]; snprintf(rid, sizeof rid, "%s", r->id); fire_after(book, page, rid, arc, depth, chain); return arc; }
}
/* LINKS of one plug or row for the window: `fires` = the triggers whose source is this item (item -> what it runs), `firedby` = the triggers that run this item (when). (none) when empty. */
static void trig_links(const char *book, int is_plug, const char *name, char *fires, size_t fc, char *by, size_t bc) {
    fires[0] = 0; by[0] = 0; if (!book[0] || !word_ok(book)) { snprintf(fires, fc, "(none)"); snprintf(by, bc, "(none)"); return; } trig_scan(book, NULL, 0);
    for (int i = 0; i < nt_; i++) { char x[3][48]; int n = trig_split(T_[i].on, x); int src = n >= 2 && !strcmp(x[1], name) && (is_plug ? !strcmp(x[0], "toggle") : (!strcmp(x[0], "done") || !strcmp(x[0], "failed")));
        if (src) { size_t l = strlen(fires); snprintf(fires + l, fc - l, "%s%s -> %s", fires[0] ? "; " : "", T_[i].on, T_[i].run); }
        n = trig_split(T_[i].run, x); int tgt = n >= 2 && !strcmp(x[1], name) && (is_plug ? !strcmp(x[0], "plug") : (!strcmp(x[0], "harnz") || !strcmp(x[0], "test")));
        if (tgt) { size_t l = strlen(by); snprintf(by + l, bc - l, "%s%s (%s)", by[0] ? "; " : "", T_[i].on, T_[i].id); } }
    if (!fires[0]) snprintf(fires, fc, "(none)"); if (!by[0]) snprintf(by, bc, "(none)");
}
/* THE MAKER (spec step 5, D80, 2026-11-14): ONE shared op writes new plug files and harnz rows, data only (the game_new_op pattern: no per-plug C). A person (the `+ new` rows), a window and an AI all call this verb, so a made file is
 * checked by the same code as a hand-written one. Rules: the book folder must exist; a name is a plain word of at most 40 characters; NOTHING is overwritten (an existing plug file or row id is refused); a harnz must name a real executable under ops/+x
 * (the same test the loader uses) and its args may not contain "..". After writing, the book is checked; if the check finds a problem that was not there before, the new file is removed again (a plug) or the harness.pdl is put back (a harnz).
 *   make <house> <book> plug <name>                         writes @.apps/<book>/plugs/<name>.plug.pdl: version 1, ENABLED 0 (off until a person switches it on), AFFECTS view, one example PARAM
 *   make <house> <book> harnz <name> <target> [args...]     appends   HARNESS | <name> | * | ACTION | <name> | <target> | <args or -> | - | no | made by the maker
 * Limit (spec 6): a made plug composes parameters and commands; a new engine rule still needs C. */
static int make_problems(const char *book) {      /* the number of problems in the book's plug files, harness rows and triggers (a fresh count, nothing printed) */
    int saved = dup(1); int nul = open("/dev/null", O_WRONLY); if (nul >= 0) { dup2(nul, 1); close(nul); } int n = 0;
    static char pn[MAXPLUG][48]; static char pm[24][200]; int np = plug_names(book, pn, MAXPLUG); for (int i = 0; i < np; i++) { PlugInfo pi; n += plug_scan(book, pn[i], &pi, pm, 24); }
    n += trig_scan(book, pm, 24); PlugLoad L; plug_load(book, &L); n += L.nprob;
    fflush(stdout); if (saved >= 0) { dup2(saved, 1); close(saved); } return n;
}
static int make_verb(const char *book, const char *kind, const char *name, char **rest, int nrest) {
    if (!word_ok(book) || !word_ok(name) || strlen(name) > 40) { fprintf(stderr, "harness_menu_op: make needs a plain book and a plain name (at most 40 characters)\n"); return 2; }
    char app[P]; snprintf(app, sizeof app, "%s/@.apps/%s", HOUSE, book); struct stat sb; if (stat(app, &sb) || !S_ISDIR(sb.st_mode)) { printf("MAKE-REFUSED no book folder @.apps/%s\n", book); return 1; }
    int before = make_problems(book);
    if (!strcmp(kind, "plug")) {
        char d[P], f[P]; snprintf(d, sizeof d, "%s/plugs", app); snprintf(f, sizeof f, "%s/%s.plug.pdl", d, name);
        if (!access(f, F_OK)) { printf("MAKE-REFUSED plug %s already exists (nothing was overwritten)\n", name); return 1; }
        mkdir(d, 0755); char txt[1500]; snprintf(txt, sizeof txt,
            "# %s.plug.pdl - made by `harness_menu_op make` (D80). A PLUG: optional, with parameters and an on/off switch. It starts OFF; the Tools > plugs window switches it on.\n# Header rows: PLUG | version | N, ENABLED | 1 or 0, AFFECTS | view or outcome (own line, no trailing comment), REQUIRES | other plug, CONFLICTS | other plug, USES | verb.\n# PARAM | key | value | type | min | max | help   (type int, tone, word or text; an event page reads it as @plug:%s:<key>)\n# COMMAND | name | event or verb | args   (a named thing this plug can do; two ON plugs may not share a name)\nPLUG | name | %s\nPLUG | version | 1\nENABLED | 0\nAFFECTS | view\nPARAM | example | 5 | int | 1 | 100 | an example number, replace it\n", name, name, name);
        if (write_atomic(f, txt)) { printf("MAKE-FAILED could not write the plug file\n"); return 1; }
        if (make_problems(book) > before) { unlink(f); printf("MAKE-UNDONE plug %s: the book check found a problem it did not have before\n", name); return 1; }
        printf("MADE plug %s plugs/%s.plug.pdl (off; switch it on in the plugs window)\n", name, name); return 0;
    }
    if (!strcmp(kind, "harnz")) {
        if (nrest < 1) { fprintf(stderr, "harness_menu_op: make harnz <name> <target under ops/+x> [args...]\n"); return 2; }
        const char *target = rest[0]; if (!under_ops(target)) { printf("MAKE-REFUSED target %s is not an executable under an ops/+x folder of the house\n", target); return 1; }
        char args[600] = ""; for (int i = 1; i < nrest; i++) { if (strchr(rest[i], '|') || strchr(rest[i], '\n')) { printf("MAKE-REFUSED an argument may not contain | or a newline\n"); return 1; } size_t l = strlen(args); snprintf(args + l, sizeof args - l, "%s%s", i > 1 ? " " : "", rest[i]); }
        if (strstr(args, "..") || strchr(target, '|')) { printf("MAKE-REFUSED the target and args may not contain .. or |\n"); return 1; }
        char f[P]; snprintf(f, sizeof f, "%s/harness.pdl", app); load_all(book); for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, name)) { printf("MAKE-REFUSED a row %s already exists (nothing was overwritten)\n", name); return 1; }
        static char old[60000]; size_t on = 0; FILE *fp = fopen(f, "r"); if (fp) { on = fread(old, 1, sizeof old - 1, fp); fclose(fp); } old[on] = 0;
        static char nw[61000]; snprintf(nw, sizeof nw, "%s%sHARNESS | %s | * | ACTION | %s | %s | %s | - | no | made by the maker (D80)\n", old, on && old[on - 1] != '\n' ? "\n" : "", name, name, target, args[0] ? args : "-");
        if (write_atomic(f, nw)) { printf("MAKE-FAILED could not write harness.pdl\n"); return 1; }
        load_all(book); int seen = 0; for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, name)) seen = 1;
        if (!seen || make_problems(book) > before) { if (on) write_atomic(f, old); else unlink(f); printf("MAKE-UNDONE harnz %s: the row does not pass the check, harness.pdl is as it was\n", name); return 1; }
        printf("MADE harnz %s -> %s (a button on every page of %s)\n", name, target, book); return 0;
    }
    fprintf(stderr, "harness_menu_op: make plug|harnz\n"); return 2;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: harness_menu_op list|pick|check <house> ...\n"); return 2; }
    const char *verb = argv[1]; snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]);
    if (!strcmp(verb, "check")) {
        load_all(NULL); for (int i = 3; i < argc; i++) { char p[P]; snprintf(p, sizeof p, "%s/@.apps/%s/harness.pdl", HOUSE, argv[i]); if (word_ok(argv[i])) load(p, 0); }
        for (int i = 3; i < argc; i++) if (word_ok(argv[i])) { static char pc[MAXPLUG][48]; int n2 = plug_names(argv[i], pc, MAXPLUG);      /* every plug file: the header and the typed rows (plug_scan, spec step 3) */
            for (int k = 0; k < n2; k++) { char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, argv[i], pc[k]); static char pm[24][200]; PlugInfo pi; int nmsg = plug_scan(argv[i], pc[k], &pi, pm, 24); for (int m = 0; m < nmsg; m++) bad(f, 0, pm[m]); }
            { char tf[P]; snprintf(tf, sizeof tf, "%s/@.apps/%s/triggers.pdl", HOUSE, argv[i]); trig_verify(argv[i], tf); } }      /* the book's triggers (D79): syntax, ids, and no loop */
        printf("HARNESS-CHECK rows=%d bad=%d\n", nr, nbad); return nbad ? 2 : 0;
    }
    if (!strcmp(verb, "make") && argc >= 6) { return make_verb(argv[3], argv[4], argv[5], argv + 6, argc - 6); }      /* D80: the maker */
    if (!strcmp(verb, "load") && argc >= 4) { return plug_load_verb(argv[3]); }      /* D78: the plug loader */
    if (!strcmp(verb, "rows") && argc >= 7) { if (!kind_ok(argv[5])) { fprintf(stderr, "harness_menu_op: kind must be plugs, harnz or tests\n"); return 2; } return plugz_rows(argv[3], strcmp(argv[4], "-") ? argv[4] : "", argv[5], argv[6]) ? 2 : 0; }
    if (!strcmp(verb, "describe") && argc >= 8) { if (!kind_ok(argv[5])) { fprintf(stderr, "harness_menu_op: kind must be plugs, harnz or tests\n"); return 2; } return plugz_describe(argv[3], strcmp(argv[4], "-") ? argv[4] : "", argv[5], argv[6], argv[7]) ? 2 : 0; }
    /* LAUNCHER (2026-11-14, owner): the LyfeDesk top-bar Tools dropdown is only three launchers into three windows (plugs, harnz, tests: tests are their own kind, owner agreed); the bar adds notes and cancel. A pick (open:plugs, open:harnz, open:tests) opens that window (plugz_open below, D76); the windows are built. The book and page arguments are accepted for the windows to come. */
    if (!strcmp(verb, "launcher") && argc >= 6) {
        if (write_atomic(argv[5], "plugs\topen:plugs\nharnz\topen:harnz\ntests\topen:tests\ndir\topen:dir\n")) { fprintf(stderr, "harness_menu_op: cannot write %s\n", argv[5]); return 2; }
        return 0;
    }
    if (!strcmp(verb, "list") && argc >= 6) {
        const char *book = argv[3], *page = strcmp(argv[4], "-") ? argv[4] : ""; load_all(book);
        int named = book[0] && strcmp(book, "-") && word_ok(book), nown = 0; for (int i = 0; i < nr; i++) if (!R[i].global) nown++;
        static char out[MAXR * 200 + MAXPLUG * 100 + 400]; out[0] = 0; size_t l;
        /* THREE GROUPS (D74, 2026-11-14), one list for the editor's Tools tab and the LyfeDesk Tools cell: what this page can DO (ACTION rows), the book's PLUGS (on/off), and the TESTS (CASE rows, agents).
         * A group with no rows is left out. A heading is a line with the id `-`: picking it does nothing. */
        int na = 0, nc = 0; for (int i = 0; i < nr; i++) if (visible(&R[i], page)) { if (!strcmp(R[i].kind, "CASE")) nc++; else na++; }
        static char pn[MAXPLUG][48]; int np = (book[0] && strcmp(book, "-") && word_ok(book)) ? plug_names(book, pn, MAXPLUG) : 0;
        g_hide_global = named && ((!nown && !np) || g_book_no_global);      /* a named book with nothing of its own: no global tests */
        na = nc = 0; for (int i = 0; i < nr; i++) if (listed(i, page)) { if (!strcmp(R[i].kind, "CASE")) nc++; else na++; }
        if (named) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- windows --\t-\nplugs\topen:plugs\nharnz\topen:harnz\ntests\topen:tests\n"); }      /* D81: the editor tab gets the same three window launchers the bar has, above its full list (the one-click rows stay) */
        if (na) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- this page --\t-\n");
            for (int i = 0; i < nr; i++) if (listed(i, page) && strcmp(R[i].kind, "CASE")) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s\t%s\n", R[i].label, R[i].id); } }
        if (np) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- plugs --\t-\n");
            for (int i = 0; i < np; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s %s\tplug:%s\n", plug_enabled(book, pn[i]) ? "[x]" : "[ ]", pn[i], pn[i]); } }
        if (nc) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- tests --\t-\n");
            for (int i = 0; i < nr; i++) if (listed(i, page) && !strcmp(R[i].kind, "CASE")) { char sfx[24]; verdict_suffix(&R[i], sfx, sizeof sfx); l = strlen(out); snprintf(out + l, sizeof out - l, "%s%s\t%s\n", R[i].label, sfx, R[i].id); } }
        /* NEW GROUP (2026-11-14, owner: "add it just to visually show our work, b4 we continue"): the maker is spec step 5 and is NOT BUILT. These rows only SHOW where it will be, say so in their labels, and a pick changes nothing (id new:plug or new:harnz prints NEW-NOT-BUILT). Shown for any named book. Remove the words "not built yet" when the maker lands. */
        if (named) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- folder --\t-\ndir\topen:dir\n"); }      /* open the book's tools folder (D76 addendum, 2026-11-14) */
        if (named) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- new --\t-\n+ new plug (starter, off)\tnew:plug\n+ new harnz (needs a target)\tnew:harnz\n"); }
        if (write_atomic(argv[5], out)) { fprintf(stderr, "harness_menu_op: cannot write %s\n", argv[5]); return 2; }
        return 0;
    }
    if (!strcmp(verb, "pick") && argc >= 6) {
        const char *book = strcmp(argv[3], "-") ? argv[3] : "", *page = strcmp(argv[4], "-") ? argv[4] : "", *id = argv[5]; int confirmed = argc >= 7 && !strcmp(argv[6], "--confirmed");
        if (argc >= 7 && !confirmed) return 2;
        if (!strcmp(id, "-")) { printf("HEADING\n"); return 0; }
        if (!strncmp(id, "open:", 5)) { if (!strcmp(id + 5, "dir")) { return open_dir(book); } if (!kind_ok(id + 5)) { fprintf(stderr, "harness_menu_op: no window %s\n", id + 5); return 2; } return plugz_open(id + 5, book, page); }      /* D76: open the window for that kind */
        if (!strcmp(id, "new:plug")) {      /* D80: a click on `+ new plug` makes a starter plug with the next free name new-plug-N in the open book */
            if (!book[0] || !word_ok(book)) { printf("MAKE-REFUSED open a book first\n"); return 1; } char nm[40]; for (int k = 1; k < 100; k++) { char pf[P]; snprintf(nm, sizeof nm, "new-plug-%d", k); snprintf(pf, sizeof pf, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, nm); if (access(pf, F_OK)) break; }
            return make_verb(book, "plug", nm, NULL, 0); }
        if (!strcmp(id, "new:harnz")) { printf("NEW-HARNZ needs a target: harness_menu_op make <house> <book> harnz <name> <target under ops/+x> [args]\n"); return 0; }
        if (!strncmp(id, "plug:", 5)) {      /* a plug row: flip its ENABLED row (D74); the book must be open */
            const char *pname = id + 5; if (!book[0] || !word_ok(book) || !word_ok(pname)) { fprintf(stderr, "harness_menu_op: a plug needs an open book and a plain name\n"); return 2; }
            static char pn2[MAXPLUG][48]; int np2 = plug_names(book, pn2, MAXPLUG), found = 0; for (int i = 0; i < np2; i++) if (!strcmp(pn2[i], pname)) found = 1;
            if (!found) { fprintf(stderr, "harness_menu_op: no plug %s in book %s\n", pname, book); return 2; }
            char why[200]; int chg = 0; int want_on = !plug_enabled(book, pname); int src = plug_switch(book, pname, want_on, why, sizeof why, &chg);
            if (src == 1) { printf("REFUSED plug %s: %s\n", pname, why); return 1; } if (src == 2) { fprintf(stderr, "harness_menu_op: %s\n", why); return 1; }
            printf("PLUG %s %s\n", pname, plug_enabled(book, pname) ? "on" : "off");
            { char ev[120], ch0[80]; snprintf(ev, sizeof ev, "toggle:%s:%s", pname, want_on ? "on" : "off"); snprintf(ch0, sizeof ch0, "plug:%s", pname); fire_triggers(book, page, ev, 0, ch0); } return 0; }
        return run_row(book, page, id, confirmed, 0, "");      /* the same code a trigger runs (D79) */
    }
    fprintf(stderr, "usage: harness_menu_op list <house> <book|-> <page|-> <outfile> | pick <house> <book|-> <page|-> <id> [--confirmed] | check <house> [book...]\n"); return 2;
}
