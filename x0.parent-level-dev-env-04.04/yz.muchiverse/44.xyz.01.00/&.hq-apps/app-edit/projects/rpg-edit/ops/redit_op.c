/* redit_op - the rpg-edit phone app's screen engine (2026-11-17; owner: "make sure rpg edit opens as a phone app"; guide BOOK 16.game/PLUS-PHONE-PLATFORM-DEV-GUIDE-2026-11-15.md section 2). v1 = a LAUNCHER for the existing rpg-edit-hq editor window.
 *   module (the window runs it): redit_op <house> <package dir> with env KHTPM_ARG3 = the phone instance dir; republishes <inst>/.hq_manager/ui.txt when marker.txt grew or every 5 s; ends with its parent. [--once <inst>] publishes once (harness).
 *   verbs (each button runs ONE):  ui_open <inst>   ui_refresh <inst>
 * ui_open: if an editor window is already running (a process whose command line holds <house>/@.apps/rpg-edit-hq/board/, which also covers a launch in progress) it does NOT start another (the editor is single-instance and relaunching reaps the
 * owner's session); it says so and the phone's bottom bar lists that window as a cell. Otherwise it runs `<house>/@.apps/rpg-edit-hq/button.sh run` (through cpu_gate when the house has it: a launch starts the 3D engine) which opens the editor on the
 * last book and page (the launcher saves the open page itself, pitfall 34). The screen shows that last book:page from <app>/state/last_book.txt (read only: a projection, safe to delete) and whether the editor is open.
 * Footprint: no resident work beyond the module's 0.4 s poll and ONE /proc scan every 5 s while the screen is shown. Exit: 0 ok, 2 usage. */
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
#include <signal.h>
#define P 4096
#define MAXE 32
static char HOUSE[P], HQD[P], INST[P];
static int word(const char *s) { if (!*s || strlen(s) > 40) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '-' || *s == '_')) return 0; return 1; }
static int house_of(const char *inst) { const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (!h) return 0; snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(h - inst), inst); return 1; }
/* fork/exec with an output file and a 600 s watchdog; returns the exit code, -1 on a signal or timeout */
static int run_out(char *const av[], const char *outf) { pid_t c = fork(); if (c < 0) return -1; if (c == 0) { int fd = open(outf, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 2); dup2(nul, 0); } execv(av[0], av); _exit(127); }
    int st = 0; for (int i = 0; i < 6000; i++) { pid_t r = waitpid(c, &st, WNOHANG); if (r == c) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(100000); } kill(c, SIGKILL); waitpid(c, &st, 0); return -1; }
static void rd1(const char *f, char *o, size_t cap) { o[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(o, (int)cap, fp)) o[strcspn(o, "\r\n")] = 0; fclose(fp); }
static void wr(const char *f, const char *t) { char tmp[P]; snprintf(tmp, sizeof tmp, "%s.tmp", f); FILE *w = fopen(tmp, "w"); if (!w) return; fputs(t, w); fclose(w); rename(tmp, f); }
static void bumpq(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQD); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }
static long msizeq(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQD); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static void say(const char *t) { char f[P]; snprintf(f, sizeof f, "%s/last.txt", HQD); wr(f, t); bumpq(); }
static int pref_book(const char *house, char *out, size_t cap) { char f[4096], ln[300]; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", house); FILE *fp = fopen(f, "r"); out[0] = 0; while (fp && fgets(ln, sizeof ln, fp)) if (!strncmp(ln, "BOOK", 4)) { char *b = strchr(ln, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n")] = 0; snprintf(out, cap, "%s", b); } } if (fp) fclose(fp); return out[0] != 0; }
static int book_vars(char *UI, size_t cap, const char *inst, const char *house, const char *page) {
    char b[80] = "", un[80] = "", ln[300]; int n = 0; if (!pref_book(house, b, sizeof b)) snprintf(b, sizeof b, "phone"); n += snprintf(UI + n, cap - n, "book_label=%s\npage_label=%s\n", b, page);
    { char sf[4096]; snprintf(sf, sizeof sf, "%s/#.desktop/strip_ui.txt", house); FILE *pf = fopen(sf, "r"); if (pf) { while (fgets(ln, sizeof ln, pf)) if (!strncmp(ln, "username=", 9)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(un, sizeof un, "%s", ln + 9); break; } fclose(pf); } if (!un[0]) snprintf(un, sizeof un, "home"); }
    char ad[4096]; snprintf(ad, sizeof ad, "%s/&.hq-apps/plus-phone/apps", house); n += snprintf(UI + n, cap - n, "pg_0_label=%s\npg_0_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'home' '%s'\n", un, house, inst); int k = 1;
    DIR *dp = opendir(ad); if (dp) { struct dirent *e; while ((e = readdir(dp)) && k < 12) { if (e->d_name[0] == '.') continue; char mf[4096]; snprintf(mf, sizeof mf, "%s/%s/manifest.pdl", ad, e->d_name); if (access(mf, R_OK)) continue; n += snprintf(UI + n, cap - n, "pg_%d_label=%s\npg_%d_act='%s/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'open' '%s' '%s'\n", k, e->d_name, k, house, inst, e->d_name); k++; } closedir(dp); }
    n += snprintf(UI + n, cap - n, "n_pages=%d\n", k); return n; }

/* ---- the editor ---- */
/* pid of a running editor window / launch of this house, 0 if none: scans /proc for a command line holding "<house>/@.apps/rpg-edit-hq/board/" (never ourselves) */
static int editor_pid(void) {
    char pat[P]; snprintf(pat, sizeof pat, "%s/@.apps/rpg-edit-hq/board/", HOUSE); DIR *d = opendir("/proc"); if (!d) return 0; struct dirent *e; int found = 0; pid_t me = getpid();
    while (!found && (e = readdir(d))) { if (e->d_name[0] < '1' || e->d_name[0] > '9') continue; int pid = atoi(e->d_name); if (pid == me) continue; char f[64], buf[8192]; snprintf(f, sizeof f, "/proc/%d/cmdline", pid);
        FILE *fp = fopen(f, "r"); if (!fp) continue; size_t n = fread(buf, 1, sizeof buf - 1, fp); fclose(fp); for (size_t i = 0; i < n; i++) if (buf[i] == 0) buf[i] = ' '; buf[n] = 0;
        if (strstr(buf, pat) && !strstr(buf, "redit_op")) found = pid; }
    closedir(d); return found;
}
static void last_page(char *book, size_t bc, char *page, size_t pc) {
    char f[P], ln[300]; snprintf(f, sizeof f, "%s/@.apps/rpg-edit-hq/state/last_book.txt", HOUSE); book[0] = page[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return;
    while (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (!strncmp(ln, "book=", 5)) snprintf(book, bc, "%s", ln + 5); else if (!strncmp(ln, "page=", 5)) snprintf(page, pc, "%s", ln + 5); } fclose(fp);
}
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '=') *s = ' '; }
static void publish(void) {
    static char UI[16384]; int n = 0; char last[300], f[P], book[80], page[80], status[400], pl[200], el[200]; int pid = editor_pid();
    snprintf(f, sizeof f, "%s/last.txt", HQD); rd1(f, last, sizeof last); last_page(book, sizeof book, page, sizeof page);
    snprintf(status, sizeof status, "%s", last[0] ? last : (pid ? "the editor is open" : "the editor is closed"));
    snprintf(pl, sizeof pl, "last page: %s", book[0] ? book : "none"); if (book[0] && page[0]) snprintf(pl, sizeof pl, "last page: %s : %s", book, page);
    snprintf(el, sizeof el, "%s", pid ? "editor window: open" : "editor window: not open");
    clean(status); clean(pl);
    n += snprintf(UI + n, sizeof UI - n, "window_title=rpg-edit\nstatus=%s\nhint=%s\nopen_label=%s\npage_line=%s\neditor_line=%s\neditor_open=%d\n", status, pid ? "Tap its cell below to show it." : "Opens the editor on the last page.", pid ? "  editor is open  " : "  open the editor  ", pl, el, pid ? 1 : 0);
    n += book_vars(UI + n, sizeof UI - n, INST, HOUSE, "rpg-edit");
    char uf[P], t[P], old[16384] = ""; snprintf(uf, sizeof uf, "%s/ui.txt", HQD); FILE *fp = fopen(uf, "r"); if (fp) { size_t r = fread(old, 1, sizeof old - 1, fp); old[r] = 0; fclose(fp); }
    if (strcmp(old, UI)) { snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); fp = fopen(t, "w"); if (fp) { fputs(UI, fp); fclose(fp); rename(t, uf); } }
}
static int module_main(const char *inst, int once) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; if (once) { publish(); return 0; }
    long seen = -1; time_t last = 0; pid_t pp = getppid();      /* end when the window that started us is gone (an orphan is adopted by the user systemd, so ppid == 1 never fires) */
    for (;;) { long m = msizeq(); time_t now = time(NULL); if (m != seen || now - last >= 5) { publish(); seen = m; last = now; } if (getppid() != pp || pp <= 1) return 0; usleep(400000); }
}
static int ui_verb(const char *verb, const char *inst) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2;
    if (!strcmp(verb, "ui_refresh")) { say("refreshed"); return 0; }
    if (!strcmp(verb, "ui_open")) {
        if (editor_pid()) { say("the editor is already open (tap its cell in the bottom bar)"); return 0; }
        char btn[P], gate[P]; snprintf(btn, sizeof btn, "%s/@.apps/rpg-edit-hq/button.sh", HOUSE); snprintf(gate, sizeof gate, "%s/&.widgits/_shared-lib/ops/+x/cpu_gate.+x", HOUSE);
        if (access(btn, R_OK) != 0) { say("the editor launcher is missing"); return 2; }
        char of[P]; snprintf(of, sizeof of, "%s/open.out", HQD); char *av[12]; int k = 0;
        if (access(gate, X_OK) == 0) { av[k++] = gate; av[k++] = HOUSE; av[k++] = "run"; av[k++] = "phone_rpg_edit"; av[k++] = "--wait"; av[k++] = "150"; av[k++] = "--"; }      /* a launch starts the 3D engine: the house way is one heavy job at a time */
        av[k++] = "/bin/sh"; av[k++] = btn; av[k++] = "run"; av[k] = NULL;
        say("opening the editor...");
        int rc = run_out(av, of);
        say(rc == 0 ? "the editor was started (it takes up to a minute)" : "the editor launch reported a problem (see open.out)");
        return rc == 0 ? 0 : 1;
    }
    fprintf(stderr, "usage: redit_op ui_open|ui_refresh <inst> | <house> <pkg> (module)\n"); return 2;
}
int main(int argc, char **argv) {
    { const char *ki = getenv("KHTPM_ARG3"); if (argc >= 3 && argv[1][0] == '/') { if (argc >= 5 && !strcmp(argv[3], "--once")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); return module_main(argv[4], 1); }
        snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); if (ki && ki[0]) return module_main(ki, 0); } }
    if (argc >= 3 && !strncmp(argv[1], "ui_", 3)) { const char *h = strstr(argv[2], "/&.hq-apps/plus-phone/instances/"); if (h) snprintf(HOUSE, sizeof HOUSE, "%.*s", (int)(h - argv[2]), argv[2]); return ui_verb(argv[1], argv[2]); }
    fprintf(stderr, "usage: redit_op ui_open|ui_refresh <inst> | <house> <pkg> (module)\n"); return 2;
}
