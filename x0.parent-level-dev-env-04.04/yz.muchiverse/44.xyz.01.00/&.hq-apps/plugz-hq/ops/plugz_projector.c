/* plugz_projector.c - the <module> UI projector of plugz-hq.xhtpm (D76, 2026-11-14): ONE window layout, three kinds (plugs, harnz, tests).
 *
 * Pattern = events-hq's evhq_projector (static template + a small C projector that writes <instance>/.hq_manager/ui.txt only when it CHANGES). No logic about plugs or tests lives here:
 * the rows and the selected row's detail come from `harness_menu_op rows|describe` (the one authority for the rows and the hiding rules); this file only formats them for the template.
 *
 * Instance dir ($KHTPM_ARG3, the renderer's argv[3] hook) = <app>/instances/<kind>; inside it .hq_manager/ holds
 *   kind.txt book.txt page.txt   what to show (written by harness_menu_op when it opens the window: `pick open:<kind>`)
 *   sel.txt                      the selected row index (plugz_act select)
 *   pending.txt                  the id waiting for a confirm (plugz_act control, rows with confirm=yes)
 *   last.txt                     the last result line (plugz_act)
 *   marker.txt                   append-only wake-up: every writer appends one byte; this loop republishes when its SIZE grew (house rule: markers, not mtime)
 *   rows.txt desc.txt            scratch copies of the op's answers (plugz_act reads rows.txt to find the selected id)
 *   ui.txt                       the projection the template reads
 * Cost: 2 stats per 400 ms while idle; a full republish (two forks of the op) only on a marker change or every 5 s (so a plug switched in the editor shows within 5 s). Exits when its parent is gone.
 * argv: <house_root> <package_dir> [--once <instance_dir>].   Build: gcc -std=gnu11 -Wall -O2 -o ops/+x/plugz_projector.+x ops/plugz_projector.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define P 4096
static char HOUSE[P], HQ[P];

static void rd1(const char *name, char *out, size_t cap) {      /* first line of <hq>/<name>, trimmed; empty when missing */
    char f[P]; snprintf(f, sizeof f, "%s/%s", HQ, name); out[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(out, (int)cap, fp)) { out[strcspn(out, "\r\n")] = 0; } fclose(fp);
}
static long msize(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQ); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static int runop(char *const av[], const char *outf) {      /* fork+exec, stdout to outf, 10 s cap: the op is a short reader */
    pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    for (int i = 0; i < 100; i++) { int st; if (waitpid(c, &st, WNOHANG) == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); }
    kill(c, SIGKILL); waitpid(c, NULL, 0); return -1;
}
static void put(char *b, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void put(char *b, size_t cap, const char *fmt, ...) { size_t l = strlen(b); va_list ap; va_start(ap, fmt); vsnprintf(b + l, cap - l, fmt, ap); va_end(ap); }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }

static char last_ui[48 * 1024];
static void publish(void) {
    char kind[32], book[128], page[128], sel_s[16], pend[96], last[400];
    rd1("kind.txt", kind, sizeof kind); rd1("book.txt", book, sizeof book); rd1("page.txt", page, sizeof page); rd1("sel.txt", sel_s, sizeof sel_s); rd1("pending.txt", pend, sizeof pend); rd1("last.txt", last, sizeof last);
    if (strcmp(kind, "plugs") && strcmp(kind, "harnz") && strcmp(kind, "tests")) return;
    char op[P], rowsf[P], descf[P]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/harness_menu_op.+x", HOUSE); snprintf(rowsf, sizeof rowsf, "%s/rows.txt", HQ); snprintf(descf, sizeof descf, "%s/desc.txt", HQ);
    char tmpr[P]; snprintf(tmpr, sizeof tmpr, "%s/rows.new", HQ);
    char *a1[] = { op, "rows", HOUSE, book, page, kind, tmpr, NULL }; char nul1[P]; snprintf(nul1, sizeof nul1, "/dev/null"); runop(a1, nul1);
    rename(tmpr, rowsf);
    static char ids[64][96], txt[64][200], state[64][40]; int n = 0; FILE *fp = fopen(rowsf, "r"); char ln[512];
    while (fp && n < 64 && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; char *t1 = strchr(ln, '\t'); if (!t1) continue; *t1++ = 0; char *t2 = strchr(t1, '\t'); if (!t2) continue; *t2++ = 0;
        snprintf(ids[n], sizeof ids[n], "%s", t1); snprintf(state[n], sizeof state[n], "%s", t2);
        if (!strcmp(kind, "plugs")) snprintf(txt[n], sizeof txt[n], "[%c] %s", !strcmp(t2, "on") ? 'x' : ' ', ln); else if (strcmp(t2, "-")) snprintf(txt[n], sizeof txt[n], "%s  [%s]", ln, t2); else snprintf(txt[n], sizeof txt[n], "%s", ln);
        clean(txt[n]); n++; }
    if (fp) fclose(fp);
    int sel = atoi(sel_s); if (sel < 0 || sel >= n) sel = 0;
    char dt[200] = "", ds[200] = "", dc[24] = "", dconf[8] = "no", lfi[500] = "(none)", lby[500] = "(none)"; int ni = 0; static char info[24][240];
    if (n > 0) { char *a2[] = { op, "describe", HOUSE, book, page, kind, ids[sel], descf, NULL }; runop(a2, nul1);
        fp = fopen(descf, "r"); while (fp && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; char *eq = strchr(ln, '='); if (!eq) continue; *eq++ = 0;
            if (!strcmp(ln, "title")) snprintf(dt, sizeof dt, "%s", eq); else if (!strcmp(ln, "status")) snprintf(ds, sizeof ds, "%s", eq); else if (!strcmp(ln, "control")) snprintf(dc, sizeof dc, "%s", eq); else if (!strcmp(ln, "confirm")) snprintf(dconf, sizeof dconf, "%s", eq); else if (!strcmp(ln, "fires")) snprintf(lfi, sizeof lfi, "%s", eq); else if (!strcmp(ln, "firedby")) snprintf(lby, sizeof lby, "%s", eq);
            else if (!strncmp(ln, "info_", 5)) { int k = atoi(ln + 5); if (k >= 0 && k < 24) { snprintf(info[k], sizeof info[k], "%s", eq); if (k + 1 > ni) ni = k + 1; } } }
        if (fp) fclose(fp); }
    char ui[sizeof last_ui]; ui[0] = 0; (void)dconf;
    const char *what = !strcmp(kind, "plugs") ? "plugs" : !strcmp(kind, "harnz") ? "harnz" : "tests";
    put(ui, sizeof ui, "window_title=%s  :  book %s, page %s\n", what, book, page);
    put(ui, sizeof ui, "list_title=%s of this %s\n", what, !strcmp(kind, "plugs") ? "book" : !strcmp(kind, "harnz") ? "page" : "book");
    put(ui, sizeof ui, "rows_count=%d\n", n);
    for (int i = 0; i < n; i++) { put(ui, sizeof ui, "row_%d_text=%s\n", i, txt[i]); put(ui, sizeof ui, "row_%d_idx=%d\n", i, i); put(ui, sizeof ui, "row_%d_cls=%s\n", i, i == sel ? "data-item active" : "data-item"); }
    put(ui, sizeof ui, "no_rows=%d\n", n == 0 ? 1 : 0);
    put(ui, sizeof ui, "empty_text=%s\n", !strcmp(kind, "plugs") ? "no plugs in this book yet" : !strcmp(kind, "harnz") ? "no harnz on this page yet" : "no tests for this book");
    put(ui, sizeof ui, "detail_title=%s\n", n ? dt : "(nothing selected)");
    put(ui, sizeof ui, "detail_status=%s\n", n ? (!strcmp(kind, "plugs") ? (!strcmp(ds, "on") ? "ON  (the plug is active)" : "OFF  (the plug is not active)") : !strcmp(kind, "tests") ? (strcmp(ds, "-") ? ds : "not run yet") : "ready") : "");
    int has_ctl = n > 0 && dc[0] && strcmp(dc, "none");
    put(ui, sizeof ui, "show_control=%d\n", has_ctl ? 1 : 0);
    put(ui, sizeof ui, "control_label=%s\n", !strcmp(dc, "toggle") ? (!strcmp(ds, "on") ? "  turn OFF  " : "  turn ON  ") : !strcmp(dc, "run") ? "  run this test  " : "  click: run  ");
    int pend_here = n > 0 && pend[0] && !strcmp(pend, ids[sel]);
    put(ui, sizeof ui, "show_confirm=%d\n", pend_here ? 1 : 0);
    put(ui, sizeof ui, "confirm_label=  confirm: really run %s  \n", dt);
    clean(last); put(ui, sizeof ui, "last_result=%s\n", last[0] ? last : "");
    put(ui, sizeof ui, "n_info=%d\n", ni); for (int i = 0; i < ni; i++) put(ui, sizeof ui, "inf_%d_text=%s\n", i, info[i]);
    /* LINKS (D79): read from the book's triggers.pdl by harness_menu_op describe (fires = what this item starts, firedby = what starts it). Edit the file to change them; the window only shows them. */
    clean(lfi); clean(lby); put(ui, sizeof ui, "links_fires=%s\nlinks_firedby=%s\nlinks_note=from triggers.pdl of this book (a trigger never confirms, loops are refused)\n", lfi, lby);
    if (strcmp(ui, last_ui)) { char f[P], t[P]; snprintf(f, sizeof f, "%s/ui.txt", HQ); snprintf(t, sizeof t, "%s/ui.txt.tmp", HQ); FILE *w = fopen(t, "w"); if (w) { fputs(ui, w); fclose(w); rename(t, f); } snprintf(last_ui, sizeof last_ui, "%s", ui); }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: plugz_projector <house_root> <package_dir>   (env KHTPM_ARG3 = the instance dir)\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); const char *inst = getenv("KHTPM_ARG3"); if (argc >= 5 && !strcmp(argv[3], "--once")) { inst = argv[4]; }      /* the harness case passes the instance dir directly */ if (!inst || !inst[0]) { fprintf(stderr, "plugz_projector: no KHTPM_ARG3 (the window must be opened with an instance dir)\n"); return 2; }
    snprintf(HQ, sizeof HQ, "%s/.hq_manager", inst); mkdir(HQ, 0755);
    if (argc >= 4 && !strcmp(argv[3], "--once")) { publish(); return 0; }      /* one projection and exit (the harness case uses it) */
    long seen = -1; time_t last_full = 0;
    for (;;) {
        long m = msize(); time_t now = time(NULL);
        if (m != seen || now - last_full >= 5) { publish(); seen = m; last_full = now; }
        if (getppid() == 1) return 0;
        usleep(400000);
    }
}
