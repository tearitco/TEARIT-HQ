/* redit_op - the rpg-edit phone app's screen engine (2026-11-17; owner: "the editor should open in the window ... the phone hardware and size should still be present"; design BOOK 16.game/PLUS-PHONE-RPG-EDIT-EMBEDDED-DESIGN-2026-11-17.md).
 * The EDITOR RUNS INSIDE THE PHONE SCREEN: this op is the host. It gives the phone its OWN editor instance (a pc-hq data folder <inst>/phone<N>-rpg-edit-pchq made by pchq_instance_init.sh, the book folders LINKED from the desktop editor's
 * data so edits to a book are shared), starts that instance's engine session without a window (open_pchq_board.sh with PCHQ_ENGINE_ONLY=1, through cpu_gate, owned by the phone window), runs the board projector (pchq_board_projector, PCHQ_PKG = the
 * data folder) as a child, and BRIDGES its variables into the phone's <inst>/.hq_manager/ui.txt: bv_session and canvas_raw as they are (the renderer reads bv_session by that name and routes the canvas size and clicks into that session), every
 * other projector variable with the prefix ed_ (so none collides with the phone's own book_label / page_label / n_pages).
 *   module (the window runs it): redit_op <house> <package dir> with env KHTPM_ARG3 = the phone instance dir; [--once <inst>] projects once WITHOUT starting anything, bridging the projector ui.txt if one exists (harness).
 *   verbs (each button runs ONE):  ui_open <inst> (open the DESKTOP editor window; never a second one)   ui_refresh <inst>
 * Load order of a fresh phone editor (a small state machine in the 0.4 s loop): data folder + links -> projector + engine start (background) -> CONFIRM_START_MAP:<last book> to the engine inbox -> wait until world_01 names the book, settle 6 s
 * (START_MAP regenerates the book on its default page AFTER a same-instant SET_DESK, pitfall 35) -> CONFIRM_SET_DESK:<last page>. The last book:page is the desktop editor's state/last_book.txt (read only).
 * Footprint: the engine session and the projector while the screen is open (measure before claiming a number), no resident work after the window is gone (the registry ends the engine, the module kills the projector). Exit: 0 ok, 2 usage. */
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
#include <sys/types.h>
#include <limits.h>
static char DATA[P], EUI[P];            /* the phone's editor data folder; the projector's state dir (<inst>/editor_ui) */
static pid_t PROJ = 0, ENGL = 0;        /* projector child, background engine launch */
static int STAGE = 0, ONCE = 0; static time_t STAGE_T = 0; static char WANT_BOOK[80], WANT_PAGE[80];
/* pid of a running DESKTOP editor window / launch of this house, 0 if none: scans /proc for a command line holding "<house>/@.apps/rpg-edit-hq/board/" (never ourselves) */
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
static void kv_of(const char *file, const char *key, char *out, size_t cap) { out[0] = 0; FILE *fp = fopen(file, "r"); if (!fp) return; char ln[600]; size_t kl = strlen(key); while (fgets(ln, sizeof ln, fp)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { snprintf(out, cap, "%s", ln + kl + 1); out[strcspn(out, "\r\n")] = 0; break; } fclose(fp); }
/* a `KEY | value` row of the phone config (phone.txt) */
static void row_of(const char *file, const char *key, char *out, size_t cap) { out[0] = 0; FILE *fp = fopen(file, "r"); if (!fp) return; char ln[300]; size_t kl = strlen(key); while (fgets(ln, sizeof ln, fp)) if (!strncmp(ln, key, kl) && !strncmp(ln + kl, " | ", 3)) { snprintf(out, cap, "%s", ln + kl + 3); out[strcspn(out, "\r\n")] = 0; break; } fclose(fp); }
static int window_pid(void) { char f[P], b[32]; snprintf(f, sizeof f, "%s/window.pid", INST); rd1(f, b, sizeof b); return atoi(b); }
/* ---- starting the phone's own editor instance ---- */
static void ensure_data(void) {
    const char *base = strrchr(INST, '/'); base = base ? base + 1 : INST; snprintf(DATA, sizeof DATA, "%s/phone%s-rpg-edit-pchq", INST, base); snprintf(EUI, sizeof EUI, "%s/editor_ui", INST);
    char init[P], of[P]; snprintf(init, sizeof init, "%s/@.apps/piececraft-hq/ops/pchq_instance_init.sh", HOUSE); snprintf(of, sizeof of, "%s/init.out", HQD);
    char rdy[P]; snprintf(rdy, sizeof rdy, "%s/pieces/system/.instance-ready", DATA);
    if (access(rdy, F_OK) != 0) { char *av[] = { "/bin/sh", init, DATA, NULL }; run_out(av, of); }
    char md[P], src[P]; snprintf(md, sizeof md, "%s/pieces/system/maps", DATA); mkdir(md, 0755); snprintf(src, sizeof src, "%s/@.apps/rpg-edit-hq/rpg-edit-hq-pchq/pieces/system/maps", HOUSE);
    DIR *dp = opendir(src); if (dp) { struct dirent *e; while ((e = readdir(dp))) { if (e->d_name[0] == '.') continue; char from[P], to[P], tgt[P]; snprintf(from, sizeof from, "%s/%s", src, e->d_name); snprintf(to, sizeof to, "%s/%s", md, e->d_name);
            struct stat sb; if (lstat(to, &sb) == 0) continue; ssize_t n = readlink(from, tgt, sizeof tgt - 1); if (n > 0) { tgt[n] = 0; if (symlink(tgt, to) != 0) { /* best effort */ } } else if (symlink(from, to) != 0) { /* best effort */ } } closedir(dp); }
    snprintf(of, sizeof of, "%s/pieces/system/widget_cmds", DATA); mkdir(of, 0755); mkdir(EUI, 0755);
}
static pid_t child(char *const av[], const char *env_pkg, int engine_only) {
    pid_t c = fork(); if (c != 0) return c;
    int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); }
    setenv("PCHQ_PKG", env_pkg, 1); if (engine_only) { char ow[24]; snprintf(ow, sizeof ow, "%d", window_pid()); setenv("PCHQ_ENGINE_ONLY", "1", 1); setenv("PCHQ_OWNER_PID", ow, 1); }
    execv(av[0], av); _exit(127);
}
static void inbox(const char *line) { char f[P]; snprintf(f, sizeof f, "%s/pieces/system/widget_cmds/inbox.txt", DATA); FILE *fp = fopen(f, "a"); if (fp) { fprintf(fp, "%s\n", line); fclose(fp); } }
static void stop_children(void) { if (PROJ > 0) { kill(PROJ, SIGTERM); waitpid(PROJ, NULL, 0); PROJ = 0; } }
static void on_term(int s) { (void)s; if (PROJ > 0) kill(PROJ, SIGTERM); _exit(0); }
static void start_projector(void) {
    char pj[P], host[80]; snprintf(pj, sizeof pj, "%s/@.apps/piececraft-hq/ops/+x/pchq_board_projector.+x", HOUSE); const char *hb = strrchr(DATA, '/'); snprintf(host, sizeof host, "%s", hb ? hb + 1 : DATA);
    char *av[] = { pj, HOUSE, EUI, host, NULL }; if (access(pj, X_OK) == 0) PROJ = child(av, DATA, 0);
}
/* one step of the load: returns after at most a few file reads */
static void step(void) {
    time_t now = time(NULL); char wf[P], mid[80];
    if (STAGE == 0) {
        last_page(WANT_BOOK, sizeof WANT_BOOK, WANT_PAGE, sizeof WANT_PAGE); ensure_data();
        char gate[P], opb[P];      /* the projector is started AFTER the engine launch: open_pchq_board.sh reaps every projector of the same instance first (found 2026-11-17, it killed ours) */
        snprintf(gate, sizeof gate, "%s/&.widgits/_shared-lib/ops/+x/cpu_gate.+x", HOUSE); snprintf(opb, sizeof opb, "%s/@.apps/piececraft-hq/open_pchq_board.sh", HOUSE);
        { char *av1[] = { gate, HOUSE, "run", "phone_rpg_edit_engine", "--wait", "150", "--", "/bin/sh", opb, HOUSE, NULL }; char *av2[] = { "/bin/sh", opb, HOUSE, NULL }; ENGL = child(access(gate, X_OK) == 0 ? av1 : av2, DATA, 1); }
        STAGE = 1; STAGE_T = now; return; }
    if (STAGE == 1) { int st; pid_t r = waitpid(ENGL, &st, WNOHANG); if (r == ENGL) { ENGL = 0; if (WIFEXITED(st) && WEXITSTATUS(st) == 0) { start_projector(); STAGE = WANT_BOOK[0] ? 2 : 5; STAGE_T = now; } else { STAGE = 9; STAGE_T = now; } }
        else if (now - STAGE_T > 240) { if (ENGL > 0) kill(ENGL, SIGKILL); STAGE = 9; STAGE_T = now; } return; }
    if (STAGE == 2) { char l[200]; snprintf(l, sizeof l, "CONFIRM_START_MAP:%s", WANT_BOOK); inbox(l); STAGE = 3; STAGE_T = now; return; }
    if (STAGE == 3) { snprintf(wf, sizeof wf, "%s/pieces/world_01/state.txt", DATA); kv_of(wf, "map_id", mid, sizeof mid);
        if ((!strcmp(mid, WANT_BOOK) && now - STAGE_T >= 6) || now - STAGE_T > 40) { if (WANT_PAGE[0]) { char l[200]; snprintf(l, sizeof l, "CONFIRM_SET_DESK:%s", WANT_PAGE); inbox(l); } STAGE = 5; STAGE_T = now; } return; }
}
/* the projector's variables into the phone's ui text: the ones the RENDERER reads by name (bv_session, canvas_raw, bv_h1, bv_h2, interact_class, interact_armed: canvas routing and the In: key relay) unchanged, everything else prefixed ed_ */
static int bridge(char *UI, size_t cap) {
    char f[P], ln[800]; int n = 0, fix = 0; snprintf(f, sizeof f, "%s/state/ui.txt", EUI); FILE *fp = fopen(f, "r"); if (!fp) return 0;
    while (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; char *eq = strchr(ln, '='); if (!eq || eq == ln) continue; *eq = 0; const char *k = ln, *v = eq + 1;
        /* the book list is the desktop menu: its "Open File Explorer" row would start a real desktop window from the phone, so it becomes an inert row (verb menu, arg close) */
        if (!strncmp(k, "f_", 2) && strlen(k) >= 8 && !strcmp(k + strlen(k) - 5, "_verb") && !strcmp(v, "file-hq")) { v = "menu"; fix = 1; }
        else if (fix && !strncmp(k, "f_", 2) && strlen(k) >= 6 && !strcmp(k + strlen(k) - 4, "_arg")) { v = "close"; fix = 0; }
        else if (!strncmp(k, "f_", 2) && !strncmp(v, "Open File Explorer", 18)) v = "(desktop only)";
        if (!strcmp(k, "bv_session") || !strcmp(k, "canvas_raw") || !strcmp(k, "bv_h1") || !strcmp(k, "bv_h2") || !strcmp(k, "interact_class") || !strcmp(k, "interact_armed")) n += snprintf(UI + n, cap - n, "%s=%s\n", k, v); else n += snprintf(UI + n, cap - n, "ed_%s=%s\n", k, v);
        if (n > (int)cap - 1000) break; }
    fclose(fp); return n;
}
static void publish(void) {
    static char UI[65536]; int n = 0; char last[300], f[P], book[80], page[80], status[400], pl[200], el[200]; int pid = editor_pid();
    snprintf(f, sizeof f, "%s/last.txt", HQD); rd1(f, last, sizeof last); last_page(book, sizeof book, page, sizeof page);
    const char *stg = STAGE == 0 ? "starting the editor..." : STAGE == 1 ? "starting the editor engine..." : STAGE == 2 || STAGE == 3 ? "opening your last page..." : STAGE == 9 ? "the editor could not start (see the desktop editor)" : "";
    snprintf(status, sizeof status, "%s", last[0] ? last : (stg[0] ? stg : (book[0] ? "" : "no last page yet")));      /* ready = empty: the screen shows the editor's own book label */
    char ptitle[120] = "phone", pn[16] = "", pp[16] = ""; { char pf[P]; snprintf(pf, sizeof pf, "%s/phone.txt", INST); row_of(pf, "PHONE", pn, sizeof pn); row_of(pf, "PORT", pp, sizeof pp); if (pn[0]) snprintf(ptitle, sizeof ptitle, "+%s :%s", pn, pp); }      /* the window is the PHONE: its title is the phone's name, the app is only what it has open (owner 2026-11-17) */
    snprintf(pl, sizeof pl, "last page: %s", book[0] ? book : "none"); if (book[0] && page[0]) snprintf(pl, sizeof pl, "last page: %s : %s", book, page);
    snprintf(el, sizeof el, "%s", pid ? "editor window: open" : "editor window: not open");
    clean(status); clean(pl);
    n += snprintf(UI + n, sizeof UI - n, "window_title=%s\nstatus=%s\nhint=%s\nopen_label=%s\npage_line=%s\neditor_line=%s\neditor_open=%d\nstage=%d\n", ptitle, status, pid ? "The desktop editor is open too." : "Opens the editor on the last page.", pid ? "  editor is open  " : "  open the editor  ", pl, el, pid ? 1 : 0, STAGE);
    n += book_vars(UI + n, sizeof UI - n, INST, HOUSE, "rpg-edit");
    n += bridge(UI + n, sizeof UI - n);      /* reads the projector's ui.txt if there is one (harness: a fake one) */
    char uf[P], t[P]; static char old[65536]; old[0] = 0; snprintf(uf, sizeof uf, "%s/ui.txt", HQD); FILE *fp = fopen(uf, "r"); if (fp) { size_t r = fread(old, 1, sizeof old - 1, fp); old[r] = 0; fclose(fp); }
    if (strcmp(old, UI)) { snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); fp = fopen(t, "w"); if (fp) { fputs(UI, fp); fclose(fp); rename(t, uf); } }
}
static int module_main(const char *inst, int once) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; ONCE = once; snprintf(EUI, sizeof EUI, "%s/editor_ui", inst); if (once) { publish(); return 0; }
    signal(SIGTERM, on_term); signal(SIGHUP, on_term);
    long seen = -1; time_t last = 0; pid_t pp = getppid();      /* end when the window that started us is gone (an orphan is adopted by the user systemd, so ppid == 1 never fires) */
    for (;;) { step(); long m = msizeq(); time_t now = time(NULL); if (m != seen || now - last >= 1) { publish(); seen = m; last = now; } if (getppid() != pp || pp <= 1) { stop_children(); return 0; } usleep(400000); }
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
