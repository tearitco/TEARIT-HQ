/* texting_op - the Texting app's screen engine (BOOK 16.game/PLUS-PHONE-TEXTING-APP-DESIGN-2026-11-16.md). A base phone app.
 *   module (the window runs it): texting_op <house> <package dir> with env KHTPM_ARG3 = the phone instance dir; republishes <inst>/.hq_manager/ui.txt when marker.txt grew or every 5 s; ends with its parent. [--once <inst>] = one projection.
 *   verbs (each button runs ONE):  ui_threads|ui_contacts|ui_searchview <inst>   ui_open <inst> <number>   ui_cmd <inst> <dir> <house> <text>   (the last one is what the renderer's cli_io field calls)
 * ALL data comes from the core op &.widgits/_shared-lib/ops/text_op.c (fork/exec, no shell): messages are lines of MY phone's inbox/outbox, contacts come from phones.index and the address book (trunk_op). This op keeps only the VIEW (<inst>/texting/view.txt: view, peer, query),
 * a projection that is safe to delete, and <inst>/.hq_manager/last.txt (the last result line). Footprint: no resident work beyond the module's 0.4 s poll and ONE text_op run (a few forks) every 5 s while the screen is open. */
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
/* ---- text_op ---- */
static char TOP[P], VW[P];
static int top_run(const char *outf, const char *verb, const char *a1, const char *a2, const char *a3) {
    char *av[8]; int n = 0; av[n++] = TOP; av[n++] = HOUSE; av[n++] = (char *)verb; if (a1) av[n++] = (char *)a1; if (a2) av[n++] = (char *)a2; if (a3) av[n++] = (char *)a3; av[n] = NULL; return run_out(av, outf);
}
static void set_paths(void) { snprintf(TOP, sizeof TOP, "%s/&.widgits/_shared-lib/ops/+x/text_op.+x", HOUSE); char d[P]; snprintf(d, sizeof d, "%s/texting", INST); mkdir(d, 0755); snprintf(VW, sizeof VW, "%s/view.txt", d); }
static void get_view(char *view, size_t vc, char *peer, size_t pc, char *query, size_t qc) {
    view[0] = peer[0] = query[0] = 0; FILE *fp = fopen(VW, "r"); char ln[600]; if (fp) { if (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(view, vc, "%s", ln); } if (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(peer, pc, "%s", ln); } if (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; snprintf(query, qc, "%s", ln); } fclose(fp); }
    if (strcmp(view, "contacts") && strcmp(view, "thread") && strcmp(view, "search")) snprintf(view, vc, "threads");
}
static void set_view(const char *view, const char *peer, const char *query) { char t[1400]; snprintf(t, sizeof t, "%s\n%s\n%s\n", view, peer, query); wr(VW, t); bumpq(); }
static int number_ok(const char *n) { if (strlen(n) != 13) return 0; for (int i = 0; i < 13; i++) { if (i == 3 || i == 8) { if (n[i] != '-') return 0; } else if (n[i] < '0' || n[i] > '9') return 0; } return 1; }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '=') *s = ' '; }
/* read text_op's rows of one kind into lines; returns the count */
static char (*ROW)[700]; static int NROW;
static void load_out(const char *f) { static char buf[200][700]; ROW = buf; NROW = 0; FILE *fp = fopen(f, "r"); if (!fp) return; while (NROW < 200 && fgets(buf[NROW], 700, fp)) { buf[NROW][strcspn(buf[NROW], "\r\n")] = 0; NROW++; } fclose(fp); }
static int field(const char *ln, int idx, char *out, size_t cap) { const char *p = ln; for (int i = 0; i < idx; i++) { p = strchr(p, '|'); if (!p) { out[0] = 0; return 0; } p++; } const char *e = strchr(p, '|'); size_t n = e ? (size_t)(e - p) : strlen(p); if (n >= cap) n = cap - 1; memcpy(out, p, n); out[n] = 0; return 1; }
static const char *rest_after(const char *ln, int idx) { const char *p = ln; for (int i = 0; i < idx; i++) { p = strchr(p, '|'); if (!p) return ""; p++; } return p; }
static void trunc_to(char *s, size_t n) { if (strlen(s) > n) { s[n - 1] = '.'; s[n] = 0; } }

static void publish(void) {
    static char UI[65536]; int n = 0; char view[40], peer[40], query[400], last[300], f[P], of[P]; get_view(view, sizeof view, peer, sizeof peer, query, sizeof query);
    snprintf(f, sizeof f, "%s/last.txt", HQD); rd1(f, last, sizeof last); snprintf(of, sizeof of, "%s/texting/out.txt", INST);
    char me[100] = ""; top_run(of, "me", NULL, NULL, NULL); load_out(of); if (NROW && !strncmp(ROW[0], "ME|", 3)) { char num[20], lab[80]; field(ROW[0], 1, num, sizeof num); field(ROW[0], 2, lab, sizeof lab); snprintf(me, sizeof me, "%s (%s)", lab, num); }
    int err = NROW && !strncmp(ROW[0], "REFUSED", 7);
    char hint[300] = "", cmdl[100] = "", status[400]; int show_cmd = 0, nt = 0, nc = 0, nl = 0, ns = 0; static char TL[64][300], TP[64][20], CL[100][300], CP[100][20], LT[64][700], SL[64][400], SP[64][20];
    if (err) { snprintf(status, sizeof status, "%s", ROW[0]); }
    else {
        snprintf(status, sizeof status, "%s", last[0] ? last : (me[0] ? me : "Texting")); char nm[200][130]; int nn = 0;
        top_run(of, "contacts", NULL, NULL, NULL); load_out(of); static char names[200][100], nums[200][20]; for (int i = 0; i < NROW && nn < 200; i++) if (!strncmp(ROW[i], "CONTACT|", 8)) { field(ROW[i], 1, nums[nn], 20); field(ROW[i], 2, names[nn], 100); nn++; }
        (void)nm;
        if (!strcmp(view, "threads")) { snprintf(hint, sizeof hint, "Newest first. * = unread."); top_run(of, "threads", NULL, NULL, NULL); load_out(of);
            for (int i = 0; i < NROW && nt < 64; i++) { if (strncmp(ROW[i], "THREAD|", 7)) continue; char pe[20], na[100], un[10], tx[300]; field(ROW[i], 1, pe, sizeof pe); field(ROW[i], 2, na, sizeof na); field(ROW[i], 5, un, sizeof un); snprintf(tx, sizeof tx, "%s", rest_after(ROW[i], 6)); clean(tx); trunc_to(tx, 38); snprintf(TP[nt], 20, "%s", pe); snprintf(TL[nt], 300, "  %s%s: %s  ", atoi(un) > 0 ? "* " : "", na, tx); nt++; }
            if (!nt) snprintf(hint, sizeof hint, "No conversations yet. Use contacts."); }
        else if (!strcmp(view, "contacts")) { snprintf(hint, sizeof hint, "add NUMBER Name  or  note NUMBER text"); show_cmd = 1; snprintf(cmdl, sizeof cmdl, "Contacts: ");
            load_out(of); for (int i = 0; i < NROW && nc < 100; i++) { if (strncmp(ROW[i], "CONTACT|", 8)) continue; char pe[20], na[100], ow[12]; field(ROW[i], 1, pe, sizeof pe); field(ROW[i], 2, na, sizeof na); field(ROW[i], 3, ow, sizeof ow); snprintf(CP[nc], 20, "%s", pe); snprintf(CL[nc], 300, "  %s  [%s]  %s  ", na, ow, pe); nc++; } }
        else if (!strcmp(view, "thread")) { char pn[100]; snprintf(pn, sizeof pn, "%s", peer); for (int i = 0; i < nn; i++) if (!strcmp(nums[i], peer)) { snprintf(pn, sizeof pn, "%s", names[i]); break; }
            snprintf(hint, sizeof hint, "%s  %s", pn, peer); show_cmd = 1; snprintf(cmdl, sizeof cmdl, "Message: "); top_run(of, "thread", peer, NULL, NULL); load_out(of);
            /* newest first, so the list opens at the top with the newest line and the field */
            for (int i = NROW - 1; i >= 0 && nl < 64; i--) { if (strncmp(ROW[i], "LINE|", 5)) continue; char dr[8], tx[600]; field(ROW[i], 2, dr, sizeof dr); snprintf(tx, sizeof tx, "%s", rest_after(ROW[i], 3)); clean(tx); snprintf(LT[nl], 700, "%s: %s", !strcmp(dr, "out") ? "me" : pn, tx); nl++; }
            if (!nl) snprintf(hint, sizeof hint, "No messages with %s yet.", pn); }
        else { snprintf(hint, sizeof hint, "Contacts, notes and messages."); show_cmd = 1; snprintf(cmdl, sizeof cmdl, "Search: ");
            if (query[0]) { top_run(of, "search", query, NULL, NULL); load_out(of); snprintf(hint, sizeof hint, "Results for \"%s\"", query);
                for (int i = 0; i < NROW && ns < 64; i++) { char kd[12], pe[20]; if (strncmp(ROW[i], "HIT|", 4)) continue; field(ROW[i], 1, kd, sizeof kd); field(ROW[i], 2, pe, sizeof pe); snprintf(SP[ns], 20, "%s", pe); if (!strcmp(kd, "contact")) { char na[100]; field(ROW[i], 3, na, sizeof na); snprintf(SL[ns], 400, "  contact: %s  %s  ", na, pe); } else { char dr[8], tx[300]; field(ROW[i], 4, dr, sizeof dr); snprintf(tx, sizeof tx, "%s", rest_after(ROW[i], 5)); clean(tx); trunc_to(tx, 40); char pn[100]; snprintf(pn, sizeof pn, "%s", pe); for (int j = 0; j < nn; j++) if (!strcmp(nums[j], pe)) { snprintf(pn, sizeof pn, "%s", names[j]); break; } snprintf(SL[ns], 400, "  %s %s: %s  ", !strcmp(dr, "out") ? "to" : "from", pn, tx); } ns++; }
                if (!ns) snprintf(hint, sizeof hint, "Nothing found for \"%s\".", query); } }
    }
    clean(status); clean(hint);
    n += snprintf(UI + n, sizeof UI - n, "window_title=Texting\nstatus=%s\nhint=%s\nshow_top=%d\nshow_bot=%d\ncmd_label=%s\ncls_threads=%s\ncls_contacts=%s\ncls_search=%s\nn_t=%d\nn_c=%d\nn_l=%d\nn_s=%d\n", status, hint, show_cmd, 0, cmdl, !strcmp(view, "threads") ? "dimbtn" : "btn", !strcmp(view, "contacts") ? "dimbtn" : "btn", !strcmp(view, "search") ? "dimbtn" : "btn", nt, nc, nl, ns);
    n += book_vars(UI + n, sizeof UI - n, INST, HOUSE, "Texting");
    for (int i = 0; i < nt; i++) n += snprintf(UI + n, sizeof UI - n, "t_%d_label=%s\nt_%d_peer=%s\n", i, TL[i], i, TP[i]);
    for (int i = 0; i < nc; i++) n += snprintf(UI + n, sizeof UI - n, "c_%d_label=%s\nc_%d_peer=%s\n", i, CL[i], i, CP[i]);
    for (int i = 0; i < nl; i++) n += snprintf(UI + n, sizeof UI - n, "l_%d_text=%s\n", i, LT[i]);
    for (int i = 0; i < ns; i++) n += snprintf(UI + n, sizeof UI - n, "s_%d_label=%s\ns_%d_peer=%s\n", i, SL[i], i, SP[i]);
    char uf[P], t[P], old[65536] = ""; snprintf(uf, sizeof uf, "%s/ui.txt", HQD); FILE *fp = fopen(uf, "r"); if (fp) { size_t r = fread(old, 1, sizeof old - 1, fp); old[r] = 0; fclose(fp); }
    if (strcmp(old, UI)) { snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); fp = fopen(t, "w"); if (fp) { fputs(UI, fp); fclose(fp); rename(t, uf); } }
}
static int module_main(const char *inst, int once) {
    snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; set_paths(); if (once) { publish(); return 0; }
    long seen = -1; time_t last = 0; pid_t pp = getppid();      /* end when the window that started us is gone: on this desktop an orphan is adopted by the user systemd (ppid != 1), so "ppid == 1" never fires and the module leaked (found 2026-11-16, two stale modules) */
    for (;;) { long m = msizeq(); time_t now = time(NULL); if (m != seen || now - last >= 5) { publish(); seen = m; last = now; } if (getppid() != pp || pp <= 1) return 0; usleep(400000); }
}
static int ui_verb(int argc, char **argv) {
    const char *verb = argv[1], *inst = argv[2]; snprintf(INST, sizeof INST, "%s", inst); snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); if (!house_of(inst)) return 2; set_paths();
    char view[40], peer[40], query[400], of[P]; get_view(view, sizeof view, peer, sizeof peer, query, sizeof query); snprintf(of, sizeof of, "%s/texting/verb.txt", INST);
    if (!strcmp(verb, "ui_threads")) { set_view("threads", "", ""); say("threads"); return 0; }
    if (!strcmp(verb, "ui_contacts")) { set_view("contacts", "", ""); say("contacts"); return 0; }
    if (!strcmp(verb, "ui_searchview")) { set_view("search", "", ""); say("search"); return 0; }
    if (!strcmp(verb, "ui_open") && argc >= 4) { if (!number_ok(argv[3])) { say("not a phone number"); return 2; } set_view("thread", argv[3], ""); top_run(of, "thread", argv[3], "--read", NULL); say("conversation opened"); return 0; }
    if (!strcmp(verb, "ui_cmd") && argc >= 6) { const char *text = argv[5]; char r[300] = "";
        if (!text[0]) { say("nothing typed"); return 0; }
        if (!strcmp(view, "thread")) { int rc = top_run(of, "send", peer, text, NULL); rd1(of, r, sizeof r); if (rc == 0) { top_run(of, "thread", peer, "--read", NULL); snprintf(r, sizeof r, "sent"); } say(r[0] ? r : "could not send"); return rc == 0 ? 0 : 1; }
        if (!strcmp(view, "search")) { set_view("search", "", text); say("search"); return 0; }
        if (!strcmp(view, "contacts")) { char w[16], num[40], rest[300]; w[0] = num[0] = rest[0] = 0; sscanf(text, "%15s %39s %299[^\n]", w, num, rest); if ((!strcmp(w, "add") || !strcmp(w, "name") || !strcmp(w, "note")) && number_ok(num) && rest[0]) { char *av[9]; int k = 0; av[k++] = TOP; av[k++] = HOUSE; av[k++] = "contact"; av[k++] = !strcmp(w, "note") ? "note" : "add"; av[k++] = num; av[k++] = rest; av[k] = NULL; int rc = run_out(av, of); rd1(of, r, sizeof r); say(r[0] ? r : "done"); return rc == 0 ? 0 : 1; }
            say("type: add 123-4567-8901 Name   or   note 123-4567-8901 text"); return 2; }
        say("open contacts or a conversation first"); return 2; }
    fprintf(stderr, "usage: texting_op ui_threads|ui_contacts|ui_searchview <inst> | ui_open <inst> <number> | ui_cmd <inst> <dir> <house> <text> | <house> <pkg> (module)\n"); return 2;
}
int main(int argc, char **argv) {
    { const char *ki = getenv("KHTPM_ARG3"); if (argc >= 3 && argv[1][0] == '/') { if (argc >= 5 && !strcmp(argv[3], "--once")) { snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); return module_main(argv[4], 1); } if (!ki || !ki[0]) { fprintf(stderr, "texting_op: no KHTPM_ARG3 (open it through the phone)\n"); return 2; } snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); return module_main(ki, 0); } }
    if (argc >= 3 && !strncmp(argv[1], "ui_", 3)) return ui_verb(argc, argv);
    fprintf(stderr, "usage: texting_op ui_... <inst> | <house> <pkg> (module)\n"); return 2;
}
