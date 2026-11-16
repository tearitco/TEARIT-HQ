/* store_op - the App Store's engine and projector (design: BOOK 16.game/PLUS-PHONE-APP-STORE-AND-APPROVED-REPO-DESIGN-2026-11-16.md). Stage 1: the catalog is the LOCAL app-edit projects folder.
 *   module (the window runs it): store_op <house> <package dir>  with env KHTPM_ARG3 = the phone instance dir; republishes <inst>/.hq_manager/ui.txt when marker.txt grew or every 5 s; ends with its parent. [--once <inst>] = one projection.
 *   verbs:  store_op ui_install <id> <inst>   store_op ui_remove <id> <inst>   (both call phone_op, fork/exec, no shell string; install then builds the app's ops with its own ops/build_<id>.sh through cpu_gate)
 *           store_op catalog <house>          prints  id|title|installed|check   (check = OK or the first app_check failure; the harness reads this)
 * State: none of its own beyond <inst>/.hq_manager/last.txt (the last result line, a projection). What is installed is the folder apps/<id>/ (phone_op owns it). System apps settings and appstore are never offered. Footprint: no resident work except the module's 0.4 s poll. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define P 4096
#define MAXE 32
static char HOUSE[P], HQD[P], INST[P];
static int word(const char *s) { if (!*s || strlen(s) > 40) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '-' || *s == '_')) return 0; return 1; }
static int sys_app(const char *id) { return !strcmp(id, "settings") || !strcmp(id, "appstore"); }
static int house_of(const char *inst) { const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (!h) return 0; snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(h - inst), inst); return 1; }
/* fork/exec with an output file and a 600 s watchdog; returns the exit code, -1 on a signal or timeout */
static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1; if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    int st = 0; for (int i = 0; i < 6000; i++) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); } kill(c, SIGKILL); waitpid(c, &st, 0); return -1; }
static void rd1(const char *f, char *o, size_t cap) { o[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(o, (int)cap, fp)) o[strcspn(o, "\r\n")] = 0; fclose(fp); }
static void wr(const char *f, const char *t) { char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", f); FILE *w = fopen(tmp, "w"); if (!w) return; fputs(t, w); fclose(w); rename(tmp, f); }
static void bumpq(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQD); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }
static long msizeq(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQD); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static int title_of(const char *mf, char *t, size_t cap) { FILE *fp = fopen(mf, "r"); char ln[400]; t[0] = 0; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "TITLE", 5)) { char *b = strchr(ln, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; snprintf(t, cap, "%s", b); } } if (fp) fclose(fp); return t[0] != 0; }
static struct { char id[48], title[64]; int inst; } E[MAXE]; static int NE;
static int cmp_id(const void *a, const void *b) { return strcmp(((const typeof(E[0]) *)a)->id, ((const typeof(E[0]) *)b)->id); }
static void scan(void) { char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/app-edit/projects", HOUSE); NE = 0; DIR *dp = opendir(d); if (!dp) return; struct dirent *e;
    while ((e = readdir(dp)) && NE < MAXE) { if (e->d_name[0] == '.' || !word(e->d_name) || sys_app(e->d_name)) continue; char mf[P]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", d, e->d_name); if (access(mf, R_OK)) continue;
        snprintf(E[NE].id, sizeof E[NE].id, "%s", e->d_name); if (!title_of(mf, E[NE].title, sizeof E[NE].title)) snprintf(E[NE].title, sizeof E[NE].title, "%s", e->d_name);
        char im[P]; snprintf(im, sizeof im, "%s/&.hq-apps/plus-phone/apps/%s/manifest.pdl", HOUSE, e->d_name); E[NE].inst = !access(im, R_OK); NE++; } closedir(dp); qsort(E, NE, sizeof E[0], cmp_id); }
static void say(const char *t) { char f[P]; snprintf(f, sizeof f, "%s/last.txt", HQD); wr(f, t); bumpq(); }
static int ui_verb(const char *verb, const char *id, const char *inst) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst) || !word(id) || sys_app(id)) return 2;
    char pm[P], pkg[P], o[P], line[300]; snprintf(pm, sizeof pm, "%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x", HOUSE); snprintf(o, sizeof o, "%s/store_run.txt", HQD); snprintf(pkg, sizeof pkg, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, id);
    if (!strcmp(verb, "ui_remove")) { char *av[] = { pm, "uninstall", HOUSE, (char *)id, NULL }; int rc = run_out(av, o); char r[200]; rd1(o, r, sizeof r); snprintf(line, sizeof line, "%s: %s", id, rc == 0 ? r : "could not remove"); say(line); return rc == 0 ? 0 : 1; }
    if (strcmp(verb, "ui_install")) return 2; if (access(pkg, R_OK)) { snprintf(line, sizeof line, "%s: no such package in the catalog", id); say(line); return 1; }
    { char *av[] = { pm, "install", HOUSE, pkg, NULL }; int rc = run_out(av, o); FILE *fp = fopen(o, "r"); char l[300], last[300] = ""; while (fp && fgets(l, sizeof l, fp)) { l[strcspn(l, "\r\n")] = 0; if (l[0]) snprintf(last, sizeof last, "%s", l); } if (fp) fclose(fp);
        if (rc != 0) { snprintf(line, sizeof line, "%s not installed: %s", id, last[0] ? last : "refused"); say(line); return 1; } }
    char bs[P]; snprintf(bs, sizeof bs, "%s/&.hq-apps/plus-phone/apps/%s/ops/build_%s.sh", HOUSE, id, id);
    if (!access(bs, R_OK)) { snprintf(line, sizeof line, "%s installed; building it...", id); say(line); char gate[P]; snprintf(gate, sizeof gate, "%s/&.widgits/_shared-lib/ops/+x/cpu_gate.+x", HOUSE); char lab[80]; snprintf(lab, sizeof lab, "store-build-%s", id);
        char *av[] = { gate, HOUSE, "run", lab, "--wait", "300", "--", "/bin/sh", bs, NULL }; int rc = run_out(av, o); if (rc != 0) { snprintf(line, sizeof line, "%s installed, but its build FAILED (exit %d): it will not run", id, rc); say(line); return 1; } }
    snprintf(line, sizeof line, "%s installed", id); say(line); return 0; }
/* STOPGAP (copy of tsc_op's book_vars; remove when phone_projector publishes the phone bar's book:page variables): the book name is the BOOK row of phone_prefs.pdl (default phone); the pages are home (the user's name) plus every installed app; the current page is this app's name. */
static int pref_book(const char *house, char *out, size_t cap) { char f[4096], ln[300]; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", house); FILE *fp = fopen(f, "r"); out[0] = 0; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "BOOK", 4)) { char *b = strchr(ln, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; snprintf(out, cap, "%s", b); } } if (fp) fclose(fp); return out[0] != 0; }
static int book_vars(char *UI, size_t cap, const char *inst, const char *house, const char *page) {
    char b[80] = "", un[80] = "", ln[300]; int n = 0; if (!pref_book(house, b, sizeof b)) snprintf(b, sizeof b, "phone"); n += snprintf(UI + n, cap - n, "book_label=%s\npage_label=%s\n", b, page);
    { char sf[4096]; snprintf(sf, sizeof sf, "%s/#.desktop/strip_ui.txt", house); FILE *pf = fopen(sf, "r"); if (pf) { while (fgets(ln, sizeof ln, pf)) if (!strncmp(ln, "username=", 9)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(un, sizeof un, "%s", ln + 9); break; } fclose(pf); } if (!un[0]) snprintf(un, sizeof un, "home"); }
    char ad[4096]; snprintf(ad, sizeof ad, "%s/&.hq-apps/plus-phone/apps", house); n += snprintf(UI + n, cap - n, "pg_0_label=%s\npg_0_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'home' '%s'\n", un, house, inst); int k = 1;
    DIR *dp = opendir(ad); if (dp) { struct dirent *e; while ((e = readdir(dp)) && k < 12) { if (e->d_name[0] == '.') continue; char mf[4096]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", ad, e->d_name); if (access(mf, R_OK)) continue; n += snprintf(UI + n, cap - n, "pg_%d_label=%s\npg_%d_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'open' '%s' '%s'\n", k, e->d_name, k, house, inst, e->d_name); k++; } closedir(dp); }
    n += snprintf(UI + n, cap - n, "n_pages=%d\n", k); return n; }
static void publish(void) {
    char last[300]; char f[P]; snprintf(f, sizeof f, "%s/last.txt", HQD); rd1(f, last, sizeof last); scan(); static char UI[16384]; int n = 0; n += snprintf(UI + n, sizeof UI - n, "window_title=App Store\nstatus=%s\nn_apps=%d\n", last[0] ? last : "Pick an app to install or remove.", NE);
    n += book_vars(UI + n, sizeof UI - n, INST, HOUSE, "App Store");
    for (int i = 0; i < NE; i++) n += snprintf(UI + n, sizeof UI - n, "app_%d_id=%s\napp_%d_verb=%s\napp_%d_cls=%s\napp_%d_label=  %s  %s  \n", i, E[i].id, i, E[i].inst ? "ui_remove" : "ui_install", i, E[i].inst ? "dimbtn" : "btn", i, E[i].inst ? "remove" : "install", E[i].title);
    char uf[P], t[P], old[16384] = ""; snprintf(uf, sizeof uf, "%s/ui.txt", HQD); FILE *fp = fopen(uf, "r"); if (fp) { size_t r = fread(old, 1, sizeof old - 1, fp); old[r] = 0; fclose(fp); } if (strcmp(old, UI)) { snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); fp = fopen(t, "w"); if (fp) { fputs(UI, fp); fclose(fp); rename(t, uf); } } }
static int module_main(const char *inst, int once) { snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; if (once) { publish(); return 0; }
    long seen = -1; time_t last = 0; for (;;) { long m = msizeq(); time_t now = time(NULL); if (m != seen || now - last >= 5) { publish(); seen = m; last = now; } if (getppid() == 1) return 0; usleep(400000); } }
int main(int argc, char **argv) {
    { const char *ki = getenv("KHTPM_ARG3"); if (argc >= 3 && argv[1][0] == '/') { if (argc >= 5 && !strcmp(argv[3], "--once")) return module_main(argv[4], 1); if (!ki || !ki[0]) { fprintf(stderr, "store_op: no KHTPM_ARG3 (open it through the phone)\n"); return 2; } return module_main(ki, 0); } }
    if (argc >= 4 && !strncmp(argv[1], "ui_", 3)) return ui_verb(argv[1], argv[2], argv[3]);
    if (argc >= 3 && !strcmp(argv[1], "catalog")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); scan(); char chk[P]; snprintf(chk, sizeof chk, "%s/&.hq-apps/plus-phone/ops/+x/app_check.+x", HOUSE);
        for (int i = 0; i < NE; i++) { char pkg[P], o[P], first[300] = "OK"; snprintf(pkg, sizeof pkg, "%s/&.hq-apps/app-edit/projects/%s", HOUSE, E[i].id); snprintf(o, sizeof o, "/tmp/store_chk_%d.txt", (int)getpid()); char *av[] = { chk, pkg, HOUSE, NULL }; int rc = run_out(av, o);
            if (rc != 0) { FILE *fp = fopen(o, "r"); char l[300]; first[0] = 0; while (fp && fgets(l, sizeof l, fp)) if (!strncmp(l, "CHECK-FAIL", 10)) { l[strcspn(l, "\r\n")] = 0; snprintf(first, sizeof first, "%s", l); break; } if (fp) fclose(fp); if (!first[0]) snprintf(first, sizeof first, "refused"); } unlink(o);
            printf("%s|%s|%d|%s\n", E[i].id, E[i].title, E[i].inst, first); } return 0; }
    fprintf(stderr, "usage: store_op ui_install|ui_remove <id> <inst> | catalog <house> | <house> <pkg> (module)\n"); return 2;
}
