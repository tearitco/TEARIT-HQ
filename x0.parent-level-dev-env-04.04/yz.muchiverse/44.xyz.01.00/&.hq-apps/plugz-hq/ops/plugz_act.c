/* plugz_act.c - the ONLY write path of plugz-hq (D76, 2026-11-14), the action op the template's action= strings run (events-hq's evhq_action.sh, as a compiled op: no shell logic).
 *   plugz_act select  <index>  <instance> [renderer-appended args ignored]   select a row of the list
 *   plugz_act control  -       <instance>                                      the one control button: a plug toggles, a harnz clicks, a test runs
 *   plugz_act confirm  -       <instance>                                      the second step of a row that asks first (confirm = yes in its row)
 *   plugz_act worker   <id> <instance> [--confirmed]                           (internal) run one row to its end and write the result; started detached through the spawn funnel
 * Every write is a file under <instance>/.hq_manager/ followed by ONE appended byte on marker.txt (the projector republishes when the marker's SIZE grows). The rows, the toggling and the running
 * are NOT done here: `harness_menu_op pick` is the one authority, this op only calls it. A plug toggle is quick and runs here; a harnz or test can take minutes (a case runs behind cpu_gate), so it
 * runs in a registered worker (`kh_spawn.sh`, role tool) and the window never blocks; the result line appears in last.txt, the test verdict in the row.
 * The house root is read from the instance path (<house>/&.hq-apps/plugz-hq/instances/<kind>), not from the arguments the renderer appends.
 * Build: gcc -std=gnu11 -Wall -O2 -o ops/+x/plugz_act.+x ops/plugz_act.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define P 4096
static char HOUSE[P], HQ[P];

static void rd1(const char *name, char *out, size_t cap) { char f[P]; snprintf(f, sizeof f, "%s/%s", HQ, name); out[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(out, (int)cap, fp)) out[strcspn(out, "\r\n")] = 0; fclose(fp); }
static void wr(const char *name, const char *text) { char f[P], t[P]; snprintf(f, sizeof f, "%s/%s", HQ, name); snprintf(t, sizeof t, "%s.tmp", f); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fputc('\n', w); fclose(w); rename(t, f); }
static void bump(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQ); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }      /* capped at 4 KB: restarted, never grows */
static int run_wait(char *const av[], const char *outf) {      /* fork+exec, wait to the end (the op caps its own children), stdout to outf */
    pid_t c = fork(); if (c < 0) return -1;
    if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static int selected_id(char *id, size_t cap) {      /* the id of the selected row, from the rows.txt the projector wrote */
    char sel_s[16]; rd1("sel.txt", sel_s, sizeof sel_s); int sel = atoi(sel_s); char f[P]; snprintf(f, sizeof f, "%s/rows.txt", HQ); FILE *fp = fopen(f, "r"); if (!fp) return 0;
    char ln[512]; int i = 0, ok = 0; while (fgets(ln, sizeof ln, fp)) { if (i++ != sel) continue; char *t1 = strchr(ln, '\t'); if (!t1) break; t1++; char *t2 = strchr(t1, '\t'); if (t2) *t2 = 0; t1[strcspn(t1, "\r\n")] = 0; snprintf(id, cap, "%s", t1); ok = 1; break; }
    fclose(fp); return ok;
}
static void stamp(char *out, size_t cap) { time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); strftime(out, cap, "%H:%M:%S", &tm); }
static int id_ok(const char *s) { if (!*s || strlen(s) > 90) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_' || *s == '-' || *s == ':')) return 0; return 1; }

static void spawn_worker(const char *id, int confirmed, const char *inst) {
    char self[P]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return; self[n] = 0; char funnel[P]; snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", HOUSE);
    setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "plugz-hq", 1); setenv("KH_LIFETIME", "keep", 1);
    pid_t c = fork(); if (c == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); }
        char *av[12]; int k = 0; av[k++] = "sh"; av[k++] = funnel; av[k++] = HOUSE; av[k++] = "0"; av[k++] = "plugz-run"; av[k++] = "--"; av[k++] = self; av[k++] = "worker"; av[k++] = (char *)id; av[k++] = (char *)inst; if (confirmed) av[k++] = "--confirmed"; av[k] = NULL; execv("/bin/sh", av); _exit(127); }
    if (c > 0) waitpid(c, NULL, 0);      /* the funnel returns at once after starting the worker detached */
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: plugz_act select|control|confirm|worker <arg> <instance> ...\n"); return 2; }
    const char *verb = argv[1], *arg = argv[2], *inst = argv[3]; const char *mark = "/&.hq-apps/plugz-hq/instances/"; const char *at = strstr(inst, mark);
    if (!at || (size_t)(at - inst) >= sizeof HOUSE) { fprintf(stderr, "plugz_act: not a plugz-hq instance dir: %s\n", inst); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(at - inst), inst); snprintf(HQ, sizeof HQ, "%s/.hq_manager", inst);
    char kind[32], book[128], page[128]; rd1("kind.txt", kind, sizeof kind); rd1("book.txt", book, sizeof book); rd1("page.txt", page, sizeof page); if (!kind[0]) { fprintf(stderr, "plugz_act: instance has no kind.txt\n"); return 2; }
    char op[P]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/harness_menu_op.+x", HOUSE);
    if (!strcmp(verb, "select")) { for (const char *c = arg; *c; c++) if (!isdigit((unsigned char)*c)) return 2; wr("sel.txt", arg); char pf[P]; snprintf(pf, sizeof pf, "%s/pending.txt", HQ); unlink(pf); wr("last.txt", ""); bump(); return 0; }
    if (!strcmp(verb, "worker")) {
        if (!id_ok(arg)) { return 2; }
        int conf = argc >= 5 && !strcmp(argv[4], "--confirmed"); char outf[P]; snprintf(outf, sizeof outf, "%s/worker.out", HQ);
        char *av[10]; int k = 0; av[k++] = op; av[k++] = "pick"; av[k++] = HOUSE; av[k++] = book; av[k++] = page; av[k++] = (char *)arg; if (conf) av[k++] = "--confirmed"; av[k] = NULL; int rc = run_wait(av, outf);
        char line[400] = ""; FILE *fp = fopen(outf, "r"); char ln[512]; while (fp && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) snprintf(line, sizeof line, "%s", ln); } if (fp) fclose(fp); unlink(outf);
        char ts[16], res[480]; stamp(ts, sizeof ts); snprintf(res, sizeof res, "%s  %s  (exit %d)  %s", ts, arg, rc, line); wr("last.txt", res); bump(); return rc == 0 ? 0 : 1;
    }
    char id[96]; if (!strcmp(verb, "confirm")) { rd1("pending.txt", id, sizeof id); char pf[P]; snprintf(pf, sizeof pf, "%s/pending.txt", HQ); unlink(pf); if (!id_ok(id)) { wr("last.txt", "nothing is waiting for a confirm"); bump(); return 1; }
        char ts[16], res[200]; stamp(ts, sizeof ts); snprintf(res, sizeof res, "%s  running %s (confirmed) ...", ts, id); wr("last.txt", res); bump(); spawn_worker(id, 1, inst); return 0; }
    if (!strcmp(verb, "control")) {
        if (!selected_id(id, sizeof id) || !id_ok(id)) { wr("last.txt", "nothing is selected"); bump(); return 1; }
        if (!strcmp(kind, "plugs")) { char outf[P]; snprintf(outf, sizeof outf, "%s/act.out", HQ); char *av[] = { op, "pick", HOUSE, book, page, id, NULL }; int rc = run_wait(av, outf);
            char line[200] = ""; FILE *fp = fopen(outf, "r"); if (fp) { if (fgets(line, sizeof line, fp)) line[strcspn(line, "\r\n")] = 0; fclose(fp); } unlink(outf); char ts[16], res[300]; stamp(ts, sizeof ts);
            snprintf(res, sizeof res, "%s  %s", ts, rc == 0 && line[0] ? line : "the switch failed (see plug_log.txt of the book)"); wr("last.txt", res); bump(); return rc == 0 ? 0 : 1; }
        char dconf[16] = ""; { char f[P]; snprintf(f, sizeof f, "%s/desc.txt", HQ); FILE *fp = fopen(f, "r"); char ln[512]; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "confirm=", 8)) { snprintf(dconf, sizeof dconf, "%s", ln + 8); dconf[strcspn(dconf, "\r\n")] = 0; } if (fp) fclose(fp); }
        if (!strcmp(dconf, "yes")) { wr("pending.txt", id); wr("last.txt", "this row asks first: press confirm"); bump(); return 0; }
        char ts[16], res[200]; stamp(ts, sizeof ts); snprintf(res, sizeof res, "%s  running %s ...", ts, id); wr("last.txt", res); bump(); spawn_worker(id, 0, inst); return 0;
    }
    fprintf(stderr, "plugz_act: unknown verb %s\n", verb); return 2;
}
