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

#define MAXR 48
#define P 4096
typedef struct { char id[40], page[48], kind[8], label[80], target[P], args[P], writes[8], confirm[8], src[P]; int line; } Row;
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
        Row *r = &R[nr]; memset(r, 0, sizeof *r); r->line = lineno; snprintf(r->src, sizeof r->src, "%s", path);
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

static void verdict_suffix(const Row *r, char *out, size_t cap) {      /* ` [15 ok]` / ` [2 FAIL]` from the case's results file, and nothing when the file does not exist */
    out[0] = 0; if (strcmp(r->kind, "CASE")) return;
    char f[P]; snprintf(f, sizeof f, "%s/&.widgits/_shared-lib/harness/results/%s.txt", HOUSE, r->target); FILE *fp = fopen(f, "r"); if (!fp) return;
    int ok = 0, fail = 0; char ln[1024]; while (fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, "RUN|", 4)) { ok = 0; fail = 0; }   /* the file is append-only: only the LAST run counts, or two runs read as a doubled total */
        else if (!strncmp(ln, "PASS|", 5)) ok++; else if (!strncmp(ln, "FAIL|", 5)) fail++; } fclose(fp);
    if (fail) snprintf(out, cap, " [%d FAIL]", fail); else if (ok) snprintf(out, cap, " [%d ok]", ok);
}
static int write_atomic(const char *path, const char *text) { char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return 1; fputs(text, f); if (fclose(f)) return 1; return rename(tmp, path) ? 1 : 0; }

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

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: harness_menu_op list|pick|check <house> ...\n"); return 2; }
    const char *verb = argv[1]; snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]);
    if (!strcmp(verb, "check")) {
        load_all(NULL); for (int i = 3; i < argc; i++) { char p[P]; snprintf(p, sizeof p, "%s/@.apps/%s/harness.pdl", HOUSE, argv[i]); if (word_ok(argv[i])) load(p, 0); }
        printf("HARNESS-CHECK rows=%d bad=%d\n", nr, nbad); return nbad ? 2 : 0;
    }
    if (!strcmp(verb, "list") && argc >= 6) {
        const char *book = argv[3], *page = strcmp(argv[4], "-") ? argv[4] : ""; load_all(book);
        static char out[MAXR * 200]; out[0] = 0;
        for (int i = 0; i < nr; i++) if (visible(&R[i], page)) { char sfx[24]; verdict_suffix(&R[i], sfx, sizeof sfx); size_t l = strlen(out); snprintf(out + l, sizeof out - l, "%s%s\t%s\n", R[i].label, sfx, R[i].id); }
        if (write_atomic(argv[5], out)) { fprintf(stderr, "harness_menu_op: cannot write %s\n", argv[5]); return 2; }
        return 0;
    }
    if (!strcmp(verb, "pick") && argc >= 6) {
        const char *book = strcmp(argv[3], "-") ? argv[3] : "", *page = strcmp(argv[4], "-") ? argv[4] : "", *id = argv[5]; int confirmed = argc >= 7 && !strcmp(argv[6], "--confirmed");
        if (argc >= 7 && !confirmed) return 2;
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
