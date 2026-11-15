#define _GNU_SOURCE
/* harness_menu_op - the ONE authority for "which harness can I run from here" (owner 2026-11-07: harness selection in the top taskbar "like edit has, but for LyfeDesk";
 * defaults chosen: rows are DATA, a row names a registered case or op only, CASE and ACTION kinds, a confirm for destructive actions, the last verdict beside each case).
 * Replaces the seven rows that were hard-coded in rpg-edit-hq/ops/pal_side.c (floor, wall, door, roof, bridge, all, clear) and my five `harness_menu_N` rows in livedesk_taskbar.pdl.
 *
 * Data (pipe rows, '#' comments):  HARNESS | id | page | kind | label | target | args | writes | confirm | note
 *   files: <house>/#.desktop/harness_menu.pdl   (global rows: shown everywhere, the page column is ignored)
 *          <house>/@.apps/<book>/harness.pdl     (a book's rows: shown when that book is open; page = `*` (every page) or one page id)
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

#define MAXR 48
#define P 4096
typedef struct { char id[40], page[48], kind[8], label[80], target[P], args[P], writes[8], confirm[8], src[P]; int line, global; } Row;
static Row R[MAXR]; static int nr, nbad;
static char HOUSE[P];

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int word_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }
static void bad(const char *src, int line, const char *why) { nbad++; fprintf(stderr, "harness_menu_op: %s line %d: %s\n", src, line, why); }

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
    while ((e = readdir(dp)) && n < max) { size_t l = strlen(e->d_name); if (l > 10 && !strcmp(e->d_name + l - 9, ".plug.pdl") && l - 9 < 48) { char nm[48]; snprintf(nm, sizeof nm, "%.*s", (int)(l - 9), e->d_name); if (word_ok(nm)) snprintf(names[n++], 48, "%s", nm); } }
    closedir(dp); for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcmp(names[i], names[j]) > 0) { char t[48]; snprintf(t, 48, "%s", names[i]); snprintf(names[i], 48, "%s", names[j]); snprintf(names[j], 48, "%s", t); }
    return n;
}
static int plug_enabled(const char *book, const char *name) {      /* 1 on, 0 off or no ENABLED row */
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

/* PLUGZ WINDOWS (2026-11-14, D76): the three windows (plugs, harnz, tests) read their rows and details through these verbs, so the parsing and the listing rules stay HERE (one authority).
 *   harness_menu_op rows     <house> <book|-> <page|-> <plugs|harnz|tests> <out>        lines `label<TAB>id<TAB>state` (plug: on/off, test: last verdict like `37 ok`, harnz: -)
 *   harness_menu_op describe <house> <book|-> <page|-> <plugs|harnz|tests> <id> <out>   key=value lines: title, status, control (toggle|click|run), confirm, n_info, info_<n>
 * The same hiding rules as `list` apply (a book with nothing of its own lists no global tests; an own row that repeats a global one is hidden). */
static int kind_ok(const char *k) { return !strcmp(k, "plugs") || !strcmp(k, "harnz") || !strcmp(k, "tests"); }
static void one_line(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }
static int plugz_rows(const char *book, const char *page, const char *kind, const char *outf) {
    int named = book[0] && strcmp(book, "-") && word_ok(book); if (!named) book = ""; load_all(book);
    int nown = 0; for (int i = 0; i < nr; i++) if (!R[i].global) nown++;
    static char pn[MAXPLUG][48]; int np = named ? plug_names(book, pn, MAXPLUG) : 0; g_hide_global = named && !nown && !np;
    static char out[MAXR * 200 + MAXPLUG * 100 + 400]; out[0] = 0; size_t l;
    if (!strcmp(kind, "plugs")) { for (int i = 0; i < np; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s\tplug:%s\t%s\n", pn[i], pn[i], plug_enabled(book, pn[i]) ? "on" : "off"); } }
    else for (int i = 0; i < nr; i++) { int is_case = !strcmp(R[i].kind, "CASE"); if (!listed(i, page) || is_case != !strcmp(kind, "tests")) continue;
        char sfx[24]; verdict_suffix(&R[i], sfx, sizeof sfx); const char *s = sfx[0] == ' ' ? sfx + 2 : sfx; char st[24]; snprintf(st, sizeof st, "%s", s[0] ? s : "-"); char *rb = strchr(st, ']'); if (rb) *rb = 0;
        l = strlen(out); snprintf(out + l, sizeof out - l, "%s\t%s\t%s\n", R[i].label, R[i].id, st); }
    return write_atomic(outf, out);
}
static int plugz_describe(const char *book, const char *page, const char *kind, const char *id, const char *outf) {
    int named = book[0] && strcmp(book, "-") && word_ok(book); if (!named) book = ""; static char out[8192]; out[0] = 0; size_t l; int ni = 0; char info[16][240];
    #define PUT(...) do { l = strlen(out); snprintf(out + l, sizeof out - l, __VA_ARGS__); } while (0)
    #define INFO(...) do { if (ni < 16) { snprintf(info[ni], sizeof info[ni], __VA_ARGS__); one_line(info[ni]); ni++; } } while (0)
    if (!strcmp(kind, "plugs")) {
        const char *pname = !strncmp(id, "plug:", 5) ? id + 5 : id; if (!named || !word_ok(pname)) { PUT("title=\nstatus=\ncontrol=none\nconfirm=no\nn_info=0\n"); return write_atomic(outf, out); }
        char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, book, pname); FILE *fp = fopen(f, "r");
        PUT("title=%s\nstatus=%s\ncontrol=toggle\nconfirm=no\n", pname, plug_enabled(book, pname) ? "on" : "off");
        INFO("file @.apps/%s/plugs/%s.plug.pdl", book, pname);
        if (fp) { char ln[600]; while (fgets(ln, sizeof ln, fp)) { char *p = trim(ln); if (!strncmp(p, "PARAM", 5)) { char *fl[7]; int n = 0; for (char *t = strtok(p, "|"); t && n < 7; t = strtok(NULL, "|")) fl[n++] = trim(t); if (n >= 4) INFO("%s = %s  (%s)", fl[1], fl[2], fl[3]); }
                else if (!strncmp(p, "PHASE", 5)) { char *fl[7]; int n = 0; for (char *t = strtok(p, "|"); t && n < 7; t = strtok(NULL, "|")) fl[n++] = trim(t); if (n >= 3) INFO("phase %s from hour %s", fl[1], fl[2]); } } fclose(fp); }
    } else {
        load_all(book); Row *r = NULL; for (int i = 0; i < nr; i++) if (!strcmp(R[i].id, id) && visible(&R[i], page)) r = &R[i];
        int want_case = !strcmp(kind, "tests"); if (!r || ((!strcmp(r->kind, "CASE")) != (want_case != 0))) { PUT("title=\nstatus=\ncontrol=none\nconfirm=no\nn_info=0\n"); return write_atomic(outf, out); }
        char sfx[24]; verdict_suffix(r, sfx, sizeof sfx); PUT("title=%s\nstatus=%s\ncontrol=%s\nconfirm=%s\n", r->label, sfx[0] ? sfx + 1 : "-", want_case ? "run" : "click", r->confirm);
        if (want_case) { INFO("case %s", r->target); INFO("runs behind cpu_gate, one at a time"); } else { INFO("runs %s", r->target); if (strcmp(r->args, "-")) INFO("args %s", r->args); INFO("writes %s", r->writes); }
        INFO("row from %s line %d", r->src, r->line);
    }
    PUT("n_info=%d\n", ni); for (int i = 0; i < ni; i++) PUT("info_%d=%s\n", i, info[i]);
    return write_atomic(outf, out);
}
/* OPEN (D76): `pick open:<kind>` starts the window for that kind. ONE layout (&.hq-apps/plugz-hq/plugz-hq.xhtpm) serves all three kinds through the renderer's generic instance-dir hook (argv[3] = <app>/instances/<kind>,
 * ${ARG3}); the instance's .hq_manager/ holds kind.txt, book.txt, page.txt (what to show), marker.txt (append-only wake-up, never rewritten) and window.pid. Started through the spawn funnel (registered). One window per kind:
 * a second pick only refreshes book and page and wakes the window. Prints WINDOW-OPENED <kind> <pid> | WINDOW-REFRESHED <kind> <pid> | WINDOW-MISSING <what>. */
static int plugz_open(const char *kind, const char *book, const char *page) {
    char app[P], inst[P], hq[P], f[P]; snprintf(app, sizeof app, "%s/&.hq-apps/plugz-hq", HOUSE); snprintf(inst, sizeof inst, "%s/instances/%s", app, kind); snprintf(hq, sizeof hq, "%s/.hq_manager", inst);
    char x[P], exe[P], funnel[P]; snprintf(x, sizeof x, "%s/plugz-hq.xhtpm", app); snprintf(exe, sizeof exe, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", HOUSE); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    if (access(x, R_OK)) { printf("WINDOW-MISSING layout\n"); return 1; } if (access(exe, X_OK)) { printf("WINDOW-MISSING renderer\n"); return 1; } if (access(funnel, R_OK)) { printf("WINDOW-MISSING spawn-funnel\n"); return 1; }
    char d1[P]; snprintf(d1, sizeof d1, "%s/instances", app); mkdir(d1, 0755); mkdir(inst, 0755); mkdir(hq, 0755);
    snprintf(f, sizeof f, "%s/kind.txt", hq); write_atomic(f, kind); snprintf(f, sizeof f, "%s/book.txt", hq); write_atomic(f, book[0] ? book : "-"); snprintf(f, sizeof f, "%s/page.txt", hq); write_atomic(f, page[0] ? page : "-");
    snprintf(f, sizeof f, "%s/marker.txt", hq); { FILE *mf = fopen(f, "a"); if (mf) { fputc('x', mf); fclose(mf); } struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); }      /* capped: a marker over 4 KB is restarted */
    char pf[P]; snprintf(pf, sizeof pf, "%s/window.pid", inst); char pb[32] = ""; FILE *pp = fopen(pf, "r"); if (pp) { if (fgets(pb, sizeof pb, pp)) { } fclose(pp); } long pid = atol(pb);
    if (pid > 1 && !kill((pid_t)pid, 0)) { char cl[P]; snprintf(cl, sizeof cl, "/proc/%ld/cmdline", pid); FILE *cp = fopen(cl, "r"); char c[512] = ""; if (cp) { size_t n = fread(c, 1, sizeof c - 1, cp); c[n] = 0; fclose(cp); } if (strstr(c, "khtpm_core_render")) { printf("WINDOW-REFRESHED %s %ld\n", kind, pid); return 0; } }
    setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "plugz-hq", 1); char nm[64]; snprintf(nm, sizeof nm, "plugz-%s", kind); char tmp[P]; snprintf(tmp, sizeof tmp, "%s/spawn.out", inst);
    char *av[] = { "sh", funnel, HOUSE, "0", nm, "--", exe, HOUSE, x, inst, NULL }; run_child(av, HOUSE, tmp, 20);
    FILE *tp = fopen(tmp, "r"); char out[64] = ""; if (tp) { if (fgets(out, sizeof out, tp)) { } fclose(tp); } unlink(tmp); long np = atol(out); if (np <= 1) { printf("WINDOW-MISSING spawn-failed\n"); return 1; }
    char num[32]; snprintf(num, sizeof num, "%ld\n", np); write_atomic(pf, num); printf("WINDOW-OPENED %s %ld\n", kind, np); return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: harness_menu_op list|pick|check <house> ...\n"); return 2; }
    const char *verb = argv[1]; snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]);
    if (!strcmp(verb, "check")) {
        load_all(NULL); for (int i = 3; i < argc; i++) { char p[P]; snprintf(p, sizeof p, "%s/@.apps/%s/harness.pdl", HOUSE, argv[i]); if (word_ok(argv[i])) load(p, 0); }
        for (int i = 3; i < argc; i++) if (word_ok(argv[i])) { static char pc[MAXPLUG][48]; int n2 = plug_names(argv[i], pc, MAXPLUG);      /* a plug file must say PLUG | name | <its file name> and have an ENABLED row */
            for (int k = 0; k < n2; k++) { char f[P]; snprintf(f, sizeof f, "%s/@.apps/%s/plugs/%s.plug.pdl", HOUSE, argv[i], pc[k]); FILE *fp = fopen(f, "r"); int named = 0, has_en = 0; char ln[600];
                if (fp) { while (fgets(ln, sizeof ln, fp)) { char *p = trim(ln); if (!strncmp(p, "ENABLED", 7) && strchr(p, '|')) has_en = 1; if (!strncmp(p, "PLUG", 4)) { char *f1 = strchr(p, '|'); if (f1) { f1++; char *f2 = strchr(f1, '|'); if (f2) { *f2 = 0; if (!strcmp(trim(f1), "name")) { char *v = trim(f2 + 1); char *f3 = strchr(v, '|'); if (f3) *f3 = 0; if (!strcmp(trim(v), pc[k])) named = 1; } } } } } fclose(fp); }
                if (!named) bad(f, 0, "a plug file needs a row PLUG | name | <the file name>"); if (!has_en) bad(f, 0, "a plug file needs an ENABLED | 1 or 0 row"); } }
        printf("HARNESS-CHECK rows=%d bad=%d\n", nr, nbad); return nbad ? 2 : 0;
    }
    if (!strcmp(verb, "rows") && argc >= 7) { if (!kind_ok(argv[5])) { fprintf(stderr, "harness_menu_op: kind must be plugs, harnz or tests\n"); return 2; } return plugz_rows(argv[3], strcmp(argv[4], "-") ? argv[4] : "", argv[5], argv[6]) ? 2 : 0; }
    if (!strcmp(verb, "describe") && argc >= 8) { if (!kind_ok(argv[5])) { fprintf(stderr, "harness_menu_op: kind must be plugs, harnz or tests\n"); return 2; } return plugz_describe(argv[3], strcmp(argv[4], "-") ? argv[4] : "", argv[5], argv[6], argv[7]) ? 2 : 0; }
    /* LAUNCHER (2026-11-14, owner): the LyfeDesk top-bar Tools dropdown is only three launchers into three windows (plugs, harnz, tests: tests are their own kind, owner agreed); the bar adds notes and cancel. A pick (open:plugs, open:harnz, open:tests) opens that window (plugz_open below, D76); the windows are built. The book and page arguments are accepted for the windows to come. */
    if (!strcmp(verb, "launcher") && argc >= 6) {
        if (write_atomic(argv[5], "plugs\topen:plugs\nharnz\topen:harnz\ntests\topen:tests\n")) { fprintf(stderr, "harness_menu_op: cannot write %s\n", argv[5]); return 2; }
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
        g_hide_global = named && !nown && !np;      /* a named book with nothing of its own: no global tests */
        na = nc = 0; for (int i = 0; i < nr; i++) if (listed(i, page)) { if (!strcmp(R[i].kind, "CASE")) nc++; else na++; }
        if (na) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- this page --\t-\n");
            for (int i = 0; i < nr; i++) if (listed(i, page) && strcmp(R[i].kind, "CASE")) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s\t%s\n", R[i].label, R[i].id); } }
        if (np) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- plugs --\t-\n");
            for (int i = 0; i < np; i++) { l = strlen(out); snprintf(out + l, sizeof out - l, "%s %s\tplug:%s\n", plug_enabled(book, pn[i]) ? "[x]" : "[ ]", pn[i], pn[i]); } }
        if (nc) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- tests --\t-\n");
            for (int i = 0; i < nr; i++) if (listed(i, page) && !strcmp(R[i].kind, "CASE")) { char sfx[24]; verdict_suffix(&R[i], sfx, sizeof sfx); l = strlen(out); snprintf(out + l, sizeof out - l, "%s%s\t%s\n", R[i].label, sfx, R[i].id); } }
        /* NEW GROUP (2026-11-14, owner: "add it just to visually show our work, b4 we continue"): the maker is spec step 5 and is NOT BUILT. These rows only SHOW where it will be, say so in their labels, and a pick changes nothing (id new:plug or new:harnz prints NEW-NOT-BUILT). Shown for any named book. Remove the words "not built yet" when the maker lands. */
        if (named) { l = strlen(out); snprintf(out + l, sizeof out - l, "-- new (not built yet) --\t-\n+ new plug (on/off)\tnew:plug\n+ new harnz (one-shot)\tnew:harnz\n"); }
        if (write_atomic(argv[5], out)) { fprintf(stderr, "harness_menu_op: cannot write %s\n", argv[5]); return 2; }
        return 0;
    }
    if (!strcmp(verb, "pick") && argc >= 6) {
        const char *book = strcmp(argv[3], "-") ? argv[3] : "", *page = strcmp(argv[4], "-") ? argv[4] : "", *id = argv[5]; int confirmed = argc >= 7 && !strcmp(argv[6], "--confirmed");
        if (argc >= 7 && !confirmed) return 2;
        if (!strcmp(id, "-")) { printf("HEADING\n"); return 0; }
        if (!strncmp(id, "open:", 5)) { if (!kind_ok(id + 5)) { fprintf(stderr, "harness_menu_op: no window %s\n", id + 5); return 2; } return plugz_open(id + 5, book, page); }      /* D76: open the window for that kind */
        if (!strcmp(id, "new:plug") || !strcmp(id, "new:harnz")) { printf("NEW-NOT-BUILT %s\n", id + 4); return 0; }      /* the maker is not built: a pick shows the row and changes nothing */      /* a group heading: picking it does nothing */
        if (!strncmp(id, "plug:", 5)) {      /* a plug row: flip its ENABLED row (D74); the book must be open */
            const char *pname = id + 5; if (!book[0] || !word_ok(book) || !word_ok(pname)) { fprintf(stderr, "harness_menu_op: a plug needs an open book and a plain name\n"); return 2; }
            static char pn2[MAXPLUG][48]; int np2 = plug_names(book, pn2, MAXPLUG), found = 0; for (int i = 0; i < np2; i++) if (!strcmp(pn2[i], pname)) found = 1;
            if (!found) { fprintf(stderr, "harness_menu_op: no plug %s in book %s\n", pname, book); return 2; }
            if (plug_toggle(book, pname)) { fprintf(stderr, "harness_menu_op: plug %s has no ENABLED row to flip\n", pname); return 1; }
            printf("PLUG %s %s\n", pname, plug_enabled(book, pname) ? "on" : "off"); return 0; }
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
            write_atomic(last, keep); printf("%s", keep[0] ? keep : "VERDICT|NONE\n"); return strstr(keep, "VERDICT|PASS") ? 0 : 1;
        }
        /* ACTION */
        int needs_page = strstr(r->args, "${CELLS}") || strstr(r->args, "${PAGE}") || strstr(r->args, "${BOOK}") || strstr(r->args, "${APP}") || !strcmp(r->writes, "page");
        if (needs_page && (!book[0] || !page[0])) { fprintf(stderr, "harness_menu_op: %s needs an open page\n", r->id); return 2; }
        char ex[P * 2]; expand(r->args, book, page, ex, sizeof ex); char exe[P]; snprintf(exe, sizeof exe, "%s/%s", HOUSE, r->target);
        char *av[24]; int ac = 0; av[ac++] = exe; char *sp; for (char *t = strtok_r(ex, " ", &sp); t && ac < 22; t = strtok_r(NULL, " ", &sp)) av[ac++] = t; av[ac] = NULL;
        int rc = run_child(av, HOUSE, NULL, 60);
        if (!strcmp(r->writes, "page")) { char mk[P]; snprintf(mk, sizeof mk, "%s/@.apps/%s/%s-pchq/pieces/system/maps/%s/%s/cells.marker", HOUSE, book, book, book, page); FILE *mf = fopen(mk, "a"); if (mf) { fputc('x', mf); fclose(mf); } }
        printf("HARNESS %s %s rc=%d\n", r->id, rc == 0 ? "ok" : "failed", rc); return rc == 0 ? 0 : 1;
    }
    fprintf(stderr, "usage: harness_menu_op list <house> <book|-> <page|-> <outfile> | pick <house> <book|-> <page|-> <id> [--confirmed] | check <house> [book...]\n"); return 2;
}
