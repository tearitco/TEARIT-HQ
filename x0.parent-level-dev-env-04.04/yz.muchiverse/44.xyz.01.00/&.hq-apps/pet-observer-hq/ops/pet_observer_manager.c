#define _GNU_SOURCE
/* pet_observer_manager - backend of the pet observer window (Phase 2, 2026-11-04): WATCH pets talk on the IRC rooms, read-only, with the pet_post guard's state next to it.
 * <module> launched by the shared khtpm_core_render.+x against pet-observer-hq.xhtpm. Invocation: <house_root> <package_dir>      Harness hook (no window): --dump <root> [room]
 * Reads (never writes) the chat node's ledgers: <root>/rooms/<room>/messages.txt (rows `MSG|id|room|user|ts|text`), <root>/pet_post_audit.txt, pet_post_pending.txt, pet_post.pdl, pet_post_kill.txt.
 * Root: env PET_OBS_ROOT, else the first line of <package_dir>/observer_root.txt, else the real 044.pal-chat-irc app folder of the house.
 * The ONLY things it can do to the world are the three guard buttons, and each goes through the guard op itself (pet_post.+x): kill switch toggle, approve the oldest held line, veto the oldest held line.
 * Publishes <package_dir>/pet_observer_ui.txt (vars for the template); polls <package_dir>/pet_observer_action.txt (seq/cmd: ROOM:<name> KILL APPROVE VETO).
 * COST: one wake per second, five stat() calls; the ui file is rewritten only when a ledger size changed (marker by size, not mtime) or a command arrived. Idle = nothing else.
 * No renderer C, no new global in khtpm_core_render. Rows: name row + wrapped body rows are NOT used here: one row per message line, wrapped to the pane width by this file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAXR 24
#define MAXM 400
#define MAXA 60
#define WRAP 74
static char house[PATH_MAX], pkg[PATH_MAX], root[PATH_MAX], cur_room[96] = "";
static char rooms[MAXR][96]; static int nrooms;
typedef struct { char text[WRAP + 40]; char cls[24]; } Row;
static Row msgs[MAXM], aud[MAXA]; static int nmsgs, naud;
static char kill_label[64], hold_label[64], pend_label[96], status[256];

static long fsz(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : -1; }
static void P(char *o, const char *n) { snprintf(o, PATH_MAX, "%s/%s", root, n); }
static int killed(void) { const char *e = getenv("PET_POST_KILL"); char p[PATH_MAX]; if (e && e[0]) snprintf(p, sizeof p, "%s", e); else P(p, "pet_post_kill.txt"); return access(p, F_OK) == 0; }
static void resolve_root(void) {
    const char *e = getenv("PET_OBS_ROOT"); if (e && e[0]) { snprintf(root, sizeof root, "%s", e); return; }
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/observer_root.txt", pkg); FILE *f = fopen(p, "r");
    if (f) { if (fgets(root, sizeof root, f)) root[strcspn(root, "\r\n")] = 0; fclose(f); if (root[0]) return; }
    snprintf(root, sizeof root, "%s/044.pal-chat-irc\xF0\x9F\x91\xA5\xEF\xB8\x8F+2", house);      /* the real app folder, literal UTF-8 like the irc window */
}
static int cmpstr(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void scan_rooms(void) {
    char d[PATH_MAX]; P(d, "rooms"); nrooms = 0; DIR *dd = opendir(d); if (!dd) return; struct dirent *e;
    while ((e = readdir(dd)) && nrooms < MAXR) { if (e->d_name[0] == '.') continue; char m[PATH_MAX]; snprintf(m, sizeof m, "%s/%s/messages.txt", d, e->d_name); if (access(m, F_OK) == 0) snprintf(rooms[nrooms++], 96, "%s", e->d_name); }
    closedir(dd); qsort(rooms, (size_t)nrooms, 96, cmpstr);
    int ok = 0; for (int i = 0; i < nrooms; i++) if (!strcmp(rooms[i], cur_room)) ok = 1;
    if (!ok) snprintf(cur_room, sizeof cur_room, "%s", nrooms ? rooms[0] : "");
}
static int is_pet(const char *u) { return !strncmp(u, "pet-", 4); }
static void add_wrapped(Row *rows, int *n, int max, const char *prefix, const char *text, const char *cls) {
    char first[WRAP + 8]; int pl = (int)strlen(prefix); int width = WRAP - pl; if (width < 20) width = 20; const char *p = text; int line = 0;
    while (*p && *n < max) { while (*p == ' ') p++; int len = (int)strlen(p), take = len < width ? len : width;
        if (take < len) { int sp = take; while (sp > width / 2 && p[sp] != ' ') sp--; if (sp > width / 2) take = sp; }
        if (!line) snprintf(first, sizeof first, "%s", prefix); else snprintf(first, sizeof first, "%*s", pl, "");
        snprintf(rows[*n].text, sizeof rows[0].text, "%s%.*s", first, take, p); snprintf(rows[*n].cls, sizeof rows[0].cls, "%s", cls); (*n)++; p += take; line++; }
}
static void read_msgs(void) {
    nmsgs = 0; if (!cur_room[0]) return; char p[PATH_MAX]; snprintf(p, sizeof p, "%s/rooms/%s/messages.txt", root, cur_room); long n = fsz(p); if (n <= 0) return;
    FILE *f = fopen(p, "r"); if (!f) return; long off = n > 65536 ? n - 65536 : 0; fseek(f, off, SEEK_SET); char ln[2300];
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char *fl[6]; int k = 0; char *q = ln; while (k < 6) { fl[k++] = q; char *b = strchr(q, '|'); if (!b || k == 6) break; *b = 0; q = b + 1; }
        if (k < 6 || strcmp(fl[0], "MSG")) continue; char pre[80]; time_t t = (time_t)atol(fl[4]); struct tm tm; localtime_r(&t, &tm); snprintf(pre, sizeof pre, "%02d:%02d  %-12.12s  ", tm.tm_hour, tm.tm_min, fl[3]);
        if (nmsgs > MAXM - 8) { memmove(msgs, msgs + MAXM / 2, (size_t)(nmsgs - MAXM / 2) * sizeof msgs[0]); nmsgs -= MAXM / 2; }
        add_wrapped(msgs, &nmsgs, MAXM, pre, fl[5], is_pet(fl[3]) ? "msg-pet" : "msg-human"); }
    fclose(f);
}
static void read_guard(void) {
    char p[PATH_MAX], ln[1400]; naud = 0; int sent = 0, deny = 0, held = 0;
    P(p, "pet_post_audit.txt"); long n = fsz(p); static Row all[MAXA * 8]; int na = 0;
    if (n > 0) { FILE *f = fopen(p, "r"); if (f) { fseek(f, n > 32768 ? n - 32768 : 0, SEEK_SET);
        while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char *fl[5]; int k = 0; char *q = ln; while (k < 5) { fl[k++] = q; char *b = strchr(q, '|'); if (!b || k == 5) break; *b = 0; q = b + 1; } if (k < 5) continue;
            time_t t = (time_t)atol(fl[0]); struct tm tm; localtime_r(&t, &tm); const char *cls = !strncmp(fl[1], "DENY", 4) || !strcmp(fl[1], "VETOED") || !strcmp(fl[1], "ERROR") ? "aud-deny" : !strcmp(fl[1], "HELD") ? "aud-held" : "aud-sent";
            if (!strcmp(fl[1], "SENT") || !strcmp(fl[1], "APPROVED")) sent++; else if (!strcmp(fl[1], "HELD")) held++; else deny++;
            if (na >= MAXA * 8 - 1) { memmove(all, all + MAXA * 4, (size_t)MAXA * 4 * sizeof all[0]); na -= MAXA * 4; }
            snprintf(all[na].text, sizeof all[0].text, "%02d:%02d:%02d %-9s %.14s #%.10s %.34s", tm.tm_hour, tm.tm_min, tm.tm_sec, fl[1], fl[2], fl[3], fl[4]); snprintf(all[na].cls, sizeof all[0].cls, "%s", cls); na++; }
        fclose(f); } }
    int from = na > MAXA ? na - MAXA : 0; for (int i = from; i < na; i++) aud[naud++] = all[i];
    int pend = 0; P(p, "pet_post_pending.txt"); { FILE *f = fopen(p, "r"); if (f) { while (fgets(ln, sizeof ln, f)) pend++; fclose(f); } }
    char c[PATH_MAX], hv[8] = "0"; P(c, "pet_post.pdl"); { FILE *f = fopen(c, "r"); if (f) { while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "hold", 4)) { char *b = strchr(ln, '|'); if (b) snprintf(hv, sizeof hv, "%d", atoi(b + 1)); } } fclose(f); } }
    snprintf(kill_label, sizeof kill_label, "Kill switch: %s   (press to toggle)", killed() ? "ON - all pet posts refused" : "off");
    snprintf(hold_label, sizeof hold_label, "Human approval: %s", atoi(hv) ? "ON - pet lines wait" : "off");
    snprintf(pend_label, sizeof pend_label, "%d line%s waiting for a human", pend, pend == 1 ? "" : "s");
    snprintf(status, sizeof status, "#%s   -   %d pet posts sent, %d refused, %d held   -   %s", cur_room[0] ? cur_room : "(no room)", sent, deny, held, root);
}
static void write_ui(void) {
    char dst[PATH_MAX], tmp[PATH_MAX + 8]; snprintf(dst, sizeof dst, "%s/pet_observer_ui.txt", pkg); snprintf(tmp, sizeof tmp, "%s.tmp", dst); FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "n_rooms=%d\n", nrooms); for (int i = 0; i < nrooms; i++) { fprintf(f, "r_%d_name=%s\n", i, rooms[i]); fprintf(f, "r_%d_cls=%s\n", i, !strcmp(rooms[i], cur_room) ? "room-active" : ""); }
    fprintf(f, "cur_room=%s\nstatus=%s\nkill_label=%s\nhold_label=%s\npend_label=%s\n", cur_room, status, kill_label, hold_label, pend_label);
    fprintf(f, "n_msgs=%d\n", nmsgs); for (int i = 0; i < nmsgs; i++) { fprintf(f, "m_%d_text=%s\n", i, msgs[i].text); fprintf(f, "m_%d_cls=%s\n", i, msgs[i].cls); }
    fprintf(f, "empty_hint=%s\n", nmsgs ? "" : "Nobody has said anything in this room yet.");
    fprintf(f, "n_aud=%d\n", naud); for (int i = 0; i < naud; i++) { fprintf(f, "a_%d_text=%s\n", i, aud[i].text); fprintf(f, "a_%d_cls=%s\n", i, aud[i].cls); }
    fclose(f); rename(tmp, dst);
}
static void guard(const char *verb) {      /* the only write path: the guard op itself */
    char op[PATH_MAX]; snprintf(op, sizeof op, "%s/&.hq-apps/irc-chat-hq/ops/+x/pet_post.+x", house); setenv("PRISC_PROJECT_ROOT", root, 1);
    pid_t pid = fork(); if (pid == 0) { execl(op, op, verb, "first", (char *)NULL); _exit(127); } if (pid > 0) { int st; waitpid(pid, &st, 0); }
}
static void do_cmd(const char *cmd) {
    if (!strncmp(cmd, "ROOM:", 5)) snprintf(cur_room, sizeof cur_room, "%s", cmd + 5);
    else if (!strcmp(cmd, "KILL")) { char p[PATH_MAX]; P(p, "pet_post_kill.txt"); if (access(p, F_OK) == 0) remove(p); else { FILE *f = fopen(p, "w"); if (f) { fputs("stop\n", f); fclose(f); } } }
    else if (!strcmp(cmd, "APPROVE")) guard("approve"); else if (!strcmp(cmd, "VETO")) guard("veto");
}
static void refresh(void) { scan_rooms(); read_msgs(); read_guard(); write_ui(); }
static void poll_action(int *last) {
    char p[PATH_MAX], b[1024]; snprintf(p, sizeof p, "%s/pet_observer_action.txt", pkg); FILE *f = fopen(p, "r"); if (!f) return; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    int seq = 0; char cmd[256] = ""; for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "seq=", 4)) seq = atoi(l + 4); else if (!strncmp(l, "cmd=", 4)) snprintf(cmd, sizeof cmd, "%s", l + 4); }
    if (seq > *last && cmd[0]) { *last = seq; do_cmd(cmd); refresh(); }
}
static void on_term(int s) { (void)s; _exit(0); }
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--dump")) {      /* harness hook: print what the window would show for a node root, no files written */
        snprintf(root, sizeof root, "%s", argv[2]); if (argc >= 4) snprintf(cur_room, sizeof cur_room, "%s", argv[3]); scan_rooms(); read_msgs(); read_guard();
        printf("rooms=%d room=%s msgs=%d aud=%d\n", nrooms, cur_room, nmsgs, naud); for (int i = 0; i < nrooms; i++) printf("ROOM %s\n", rooms[i]);
        for (int i = 0; i < nmsgs; i++) printf("%s|%s\n", msgs[i].cls, msgs[i].text); for (int i = 0; i < naud; i++) printf("%s|%s\n", aud[i].cls, aud[i].text);
        printf("%s\n%s\n%s\n%s\n", kill_label, hold_label, pend_label, status); return 0; }
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <package_dir> | --dump <root> [room]\n", argv[0]); return 1; }
    snprintf(house, sizeof house, "%s", argv[1]); snprintf(pkg, sizeof pkg, "%s", argv[2]); { char *s = strrchr(pkg, '/'); if (s && strstr(s, ".xhtpm")) *s = 0; }      /* the renderer passes the template file: use its folder */
    resolve_root(); signal(SIGTERM, on_term); signal(SIGINT, on_term); signal(SIGHUP, on_term);
    refresh(); int last = 0; char mp[PATH_MAX], ap[PATH_MAX], pp[PATH_MAX], kp[PATH_MAX]; long sm = -2, sa = -2, sp = -2; int sk = -1; strcpy(kp, "");
    for (;;) {
        for (int i = 0; i < 10; i++) { usleep(100000); poll_action(&last); }
        snprintf(mp, sizeof mp, "%s/rooms/%s/messages.txt", root, cur_room); P(ap, "pet_post_audit.txt"); P(pp, "pet_post_pending.txt");
        long m = fsz(mp), a = fsz(ap), pe = fsz(pp); int k = killed(); if (m != sm || a != sa || pe != sp || k != sk) { sm = m; sa = a; sp = pe; sk = k; refresh(); }
    }
}
