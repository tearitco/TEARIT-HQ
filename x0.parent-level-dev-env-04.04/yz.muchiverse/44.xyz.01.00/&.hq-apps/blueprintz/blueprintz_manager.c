/* blueprintz_manager - the blueprintz window (page view + debugger). Design: BOOK BLUEPRINTZ-DESIGN.md sections 12.3-12.15.
 * Forked by the renderer (module tag in blueprintz.xhtpm) as: blueprintz_manager <house_root> [package_dir]. Writes state/ui.txt.
 * WHAT IT SHOWS: every event of one page (state/bp_sources.pdl rows  EVENT | name | pkg_dir ), with trigger, state (waiting / queued /
 *   running / ran) and problem flags, all read by ops/bp_scan; the debugger status and the last trace rows from bp_ctl.
 * VERBS (state/request.txt, appended by ops/bp_req, read by size growth): SEL:<i> PLAY STOPPLAY PAUSE RESUME DETACH STEPEV STEPCMD RUNEND
 *   ABORT FIRE REFRESH  CSEL:<id> EDIT:<typed> ADD:<typed> CDEL UNDO  (B4, design 12.22: edit the selected event's page_1 commands).  PLAY = the desktop play switch (mode=on in <target>/#.desktop/khtpm_play_mode.state.txt, the same file the player
 *   menu toggles) and fires the page's Autorun events once; events with other triggers stay "waiting" until a trigger comes or FIRE.
 * TARGET: state/target.pdl  (TARGET | house | <path>) points the controls and the scan at another house root, so a test never touches the live
 *   desktop switch or the real common events; missing = the house the window runs in.
 * Style: state files only, size-growth marker for requests, children by fork+exec with a time limit (never popen), no logging. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>

#define P 4400
#define MAXE 48
#define MAXF 200
static char g_house[4096], g_pkg[P], g_state[P], g_target[P];

typedef struct { char name[64], pkg[P], trig[200], state[16]; int nflags; } Ev;
typedef struct { char ev[64], page[24], code[24], detail[240]; } Fl;
static Ev E[MAXE]; static int nE = 0, sel = 0;
static Fl F[MAXF]; static int nF = 0;
static char msg[300] = "ready", ctl_status[600] = "", trace_rows[6][240]; static int ntrace = 0;
static char last_ui[24000];

/* ---- B4 editing: the commands (NODE rows) of the selected event's page_1, and an undo stack of inverse edits ---- */
typedef struct { int id; char type[40], params[400]; } Nd;
/* pages of the selected event (B4 multi-page, 2026-10-21): name + trigger read from pages/<name>/condition.pdl; cpage = the page being edited */
typedef struct { char name[32], trig[64], sw[64]; } Pg;
#define MAXPG 24
static Pg PG[MAXPG]; static int nPG = 0; static char cpage[32] = "";
typedef struct { char pkg[P], label[64], page[32], mode[8], id[12], type[40], params[400]; } Undo;
#define MAXN 64
#define MAXU 16
static Nd N[MAXN]; static int nN = 0, csel = -1;
static Undo U[MAXU]; static int nU = 0;

static void join(char *o, const char *a, const char *b) { snprintf(o, P, "%s/%s", a, b); }
static long fsize(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_size : 0; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }

static void read_target(void) {
    char p[P]; join(p, g_state, "target.pdl"); snprintf(g_target, sizeof g_target, "%s", g_house);
    FILE *f = fopen(p, "r"); if (!f) return;
    char l[P];
    while (fgets(l, sizeof l, f)) if (!strncmp(l, "TARGET | house |", 16)) { char *v = trim(l + 16); if (*v) snprintf(g_target, sizeof g_target, "%s", v); }
    fclose(f);
}

/* run argv with stdout+stderr to outfile, up to limit_s seconds. returns exit status or -1 */
static int run(char *const argv[], const char *outfile, int limit_s) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) {
        if (outfile && (!freopen(outfile, "w", stdout) || !freopen(outfile, "a", stderr))) _exit(126);
        if (!freopen("/dev/null", "r", stdin)) _exit(126);
        execv(argv[0], argv); _exit(127);
    }
    for (int t = 0; t < limit_s * 20; t++) {
        int st; pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        struct timespec ts = { 0, 50 * 1000000L }; nanosleep(&ts, NULL);
    }
    kill(pid, SIGKILL); waitpid(pid, NULL, 0); return -1;
}
static void ops_path(char *o, const char *name) { snprintf(o, P, "%s/ops/+x/%s.+x", g_pkg, name); }

static int first_line(const char *file, char *out, size_t n) {
    FILE *f = fopen(file, "r"); if (!f) { out[0] = 0; return 0; }
    int got = fgets(out, (int)n, f) != NULL; fclose(f); out[strcspn(out, "\r\n")] = 0; return got;
}

/* ---- scan: bp_scan output -> E[] / F[] ---- */
static void load_sources(void) {
    char p[P]; join(p, g_state, "bp_sources.pdl"); nE = 0;
    FILE *f = fopen(p, "r"); if (!f) return;
    char l[P];
    while (fgets(l, sizeof l, f) && nE < MAXE) {
        if (strncmp(l, "EVENT |", 7)) continue;
        l[strcspn(l, "\r\n")] = 0;
        char *a = strchr(l + 7, '|'); if (!a) continue; *a = 0;
        char *name = trim(l + 7), *path = trim(a + 1);
        snprintf(E[nE].name, 64, "%s", name);
        if (path[0] == '/') snprintf(E[nE].pkg, P, "%s", path); else snprintf(E[nE].pkg, P, "%s/%s", g_target, path);
        snprintf(E[nE].trig, 200, "-"); snprintf(E[nE].state, 16, "waiting"); E[nE].nflags = 0; nE++;
    }
    fclose(f);
}
static void scan(void) {
    load_sources(); nF = 0;
    if (!nE) return;
    char bin[P], src[P], out[P]; ops_path(bin, "bp_scan"); join(src, g_state, "bp_sources.pdl"); join(out, g_state, "scan.out");
    char *av[] = { bin, g_target, "--sources", src, NULL };
    /* the sources file's relative paths are resolved against the TARGET house, so the scan sees the same events the controls act on */
    run(av, out, 15);
    FILE *f = fopen(out, "r"); if (!f) return;
    char l[P]; int cur = -1;
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0;
        if (!strncmp(l, "EV_BEGIN | ", 11)) { cur++; continue; }
        if (!strncmp(l, "EV | ", 5) && cur >= 0 && cur < nE) {
            char *p = l + 5, *f2 = strstr(p, " | "); if (!f2) continue; p = f2 + 3;          /* skip name */
            char *f3 = strstr(p, " | "); if (!f3) continue; *f3 = 0; snprintf(E[cur].trig, 200, "%s", p); p = f3 + 3;
            char *f4 = strstr(p, " | "); if (!f4) continue; *f4 = 0; snprintf(E[cur].state, 16, "%s", p); E[cur].nflags = atoi(f4 + 3);
        } else if (!strncmp(l, "FLAG | ", 7) && nF < MAXF) {
            char *p = l + 7, *s1 = strstr(p, " | "); if (!s1) continue; *s1 = 0; snprintf(F[nF].ev, 64, "%s", p); p = s1 + 3;
            char *s2 = strstr(p, " | "); if (!s2) continue; *s2 = 0; snprintf(F[nF].page, 24, "%s", p); p = s2 + 3;
            char *s3 = strstr(p, " | "); if (!s3) continue; *s3 = 0; snprintf(F[nF].code, 24, "%s", p); snprintf(F[nF].detail, 240, "%s", s3 + 3); nF++;
        }
    }
    fclose(f);
    if (sel >= nE) sel = nE ? nE - 1 : 0;
}

/* ---- bp_ctl helper ---- */
static int ctl(const char *cmd, const char *a1, const char *a2) {
    char bin[P], out[P]; ops_path(bin, "bp_ctl"); join(out, g_state, "ctl.out");
    char *av[8]; int n = 0; av[n++] = bin; av[n++] = g_target; av[n++] = (char *)cmd; if (a1) av[n++] = (char *)a1; if (a2) av[n++] = (char *)a2; av[n] = NULL;
    int rc = run(av, out, 20);
    char l[300]; first_line(out, l, sizeof l);
    snprintf(msg, sizeof msg, "%s: %s%s", cmd, l[0] ? l : "(no output)", rc ? "  [exit nonzero]" : "");
    return rc;
}
static void status(void) {
    char bin[P], out[P]; ops_path(bin, "bp_ctl"); join(out, g_state, "status.out");
    char *av[] = { bin, g_target, "status", NULL }; run(av, out, 10);
    FILE *f = fopen(out, "r"); ctl_status[0] = 0; if (!f) return;
    char l[300], paused[8] = "0", attached[8] = "0", pend[16] = "0", vm[200] = "-";
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\r\n")] = 0;
        if (!strncmp(l, "paused=", 7)) snprintf(paused, sizeof paused, "%s", l + 7);
        else if (!strncmp(l, "attached=", 9)) snprintf(attached, sizeof attached, "%s", l + 9);
        else if (!strncmp(l, "queue_pending=", 14)) snprintf(pend, sizeof pend, "%s", l + 14);
        else if (!strncmp(l, "vm_next=", 8)) snprintf(vm, sizeof vm, "%s", l + 8);
    }
    fclose(f);
    snprintf(ctl_status, sizeof ctl_status, "debugger=%s  queued=%s  held=%s", !strcmp(attached, "0") ? "detached" : (!strcmp(paused, "1") ? "PAUSED" : "attached"), pend, vm);
    /* last trace rows */
    char tp[P]; join(tp, g_state, "trace.txt");
    if (g_target[0]) { snprintf(tp, P, "%s/&.hq-apps/blueprintz/state/trace.txt", g_target); }
    ntrace = 0; f = fopen(tp, "r");
    if (f) { char all[6][240]; int n = 0; char t[240];
        while (fgets(t, sizeof t, f)) { t[strcspn(t, "\r\n")] = 0; snprintf(all[n % 6], 240, "%s", t); n++; }
        fclose(f); int start = n > 6 ? n - 6 : 0;
        for (int i = start; i < n; i++) snprintf(trace_rows[ntrace++], 240, "%s", all[i % 6]);
    }
}
static int play_mode_on(void) {
    char p[P], l[64]; snprintf(p, P, "%s/#.desktop/khtpm_play_mode.state.txt", g_target); return first_line(p, l, sizeof l) && !strncmp(l, "mode=on", 7);
}
static void set_play(int on) {
    char p[P], d[P]; snprintf(d, P, "%s/#.desktop", g_target); mkdir(d, 0755); snprintf(p, P, "%s/khtpm_play_mode.state.txt", d);
    FILE *f = fopen(p, "w"); if (f) { fprintf(f, "mode=%s\n", on ? "on" : "off"); fclose(f); }
}
static const char *first_trigger(const Ev *e, char *out, size_t n) {   /* "page_1:on-click,page_2:pp6" -> on-click */
    const char *c = strchr(e->trig, ':'); if (!c) { out[0] = 0; return out; }
    snprintf(out, n, "%s", c + 1); out[strcspn(out, ",")] = 0; return out;
}


/* ---- B4: command list + edits (every write goes through ops/bp_edit_op -> the events-hq manager; this file never writes an IR) ---- */
static int pgnum(const char *n) { return atoi(n + 5); }
static void load_pages(void) {
    nPG = 0; if (!nE) return;
    char root[P]; snprintf(root, P, "%s/event_pkg/pages", E[sel].pkg); DIR *d = opendir(root); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && nPG < MAXPG) { if (strncmp(de->d_name, "page_", 5) || strlen(de->d_name) >= 32) continue; snprintf(PG[nPG].name, 32, "%s", de->d_name); PG[nPG].trig[0] = 0; PG[nPG].sw[0] = 0; nPG++; }
    closedir(d);
    for (int i = 1; i < nPG; i++) { Pg t = PG[i]; int j = i - 1; while (j >= 0 && pgnum(PG[j].name) > pgnum(t.name)) { PG[j + 1] = PG[j]; j--; } PG[j + 1] = t; }
    for (int i = 0; i < nPG; i++) {
        char c[P], l[300]; snprintf(c, P, "%s/%s/condition.pdl", root, PG[i].name); FILE *f = fopen(c, "r"); if (!f) continue;
        while (fgets(l, sizeof l, f)) if (!strncmp(l, "COND", 4)) {      /* "COND | trigger | x" and the padded "COND         | trigger              | x" both occur */
            char *t = strstr(l, "trigger"), *w = strstr(l, "switch"); char *k = t ? t : w; if (!k) continue; char *bar = strchr(k, '|'); if (!bar) continue;
            snprintf(t ? PG[i].trig : PG[i].sw, 64, "%s", trim(bar + 1)); }
        fclose(f);
    }
    int ok = 0; for (int i = 0; i < nPG; i++) if (!strcmp(PG[i].name, cpage)) ok = 1;
    if (!ok) snprintf(cpage, sizeof cpage, "%s", nPG ? PG[0].name : "");
}
static const char *page_trig(const char *name) { for (int i = 0; i < nPG; i++) if (!strcmp(PG[i].name, name)) return PG[i].trig; return ""; }
static const char *page_sw(const char *name) { for (int i = 0; i < nPG; i++) if (!strcmp(PG[i].name, name)) return PG[i].sw; return ""; }
static void load_nodes(void) {
    nN = 0; if (!nE) return;
    load_pages(); if (!cpage[0]) return;
    char p[P], l[900]; snprintf(p, P, "%s/event_pkg/pages/%s/event.ir.pdl", E[sel].pkg, cpage);
    FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(l, sizeof l, f) && nN < MAXN) {
        if (strncmp(l, "NODE", 4)) continue;
        l[strcspn(l, "\r\n")] = 0;
        char *idp = strstr(l, "id="), *tp = strstr(l, "type="), *bar = tp ? strstr(tp, " | ") : NULL;
        if (!idp || !tp) continue;
        N[nN].id = atoi(idp + 3);
        snprintf(N[nN].type, sizeof N[nN].type, "%.*s", (int)(bar ? bar - (tp + 5) : (long)strlen(tp + 5)), tp + 5);
        snprintf(N[nN].params, sizeof N[nN].params, "%s", bar ? trim(bar + 3) : "");
        nN++;
    }
    fclose(f);
    int ok = 0; for (int i = 0; i < nN; i++) if (N[i].id == csel) ok = 1;
    if (!ok) csel = nN ? N[0].id : -1;
}
static Nd *node_by_id(int id) { for (int i = 0; i < nN; i++) if (N[i].id == id) return &N[i]; return NULL; }
/* what a hand-typed value may contain: it ends up inside a single-quoted word of cmd_N.sh, so no quote, $, backtick, ; & or newline */
static int clean(const char *s, int allow_params) {
    for (; *s; s++) { unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') continue;
        if (allow_params && (c == ':' || c == ',' || c == '=' || c == '|' || c == '/' || c == ' ')) continue;
        return 0; }
    return 1;
}
/* a command type must be one the events-hq registry knows: an unknown type compiles to a page with no exec line (the silent failure of 2026-10-20) */
static int known_type(const char *type) {
    char p[P], l[600], want[80]; snprintf(p, P, "%s/#.ref/menu/event_commands.registry.pdl", g_target); snprintf(want, sizeof want, "COMMAND %s", type);
    FILE *f = fopen(p, "r"); if (!f) return 1;      /* no registry readable: do not block, the manager decides */
    int ok = 0; while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (!strcmp(l, want)) { ok = 1; break; } }
    fclose(f); return ok;
}
/* An edit runs as a background child (bp_edit_op takes 2-3 s: it starts the events-hq manager, writes the request, waits for the recompile), so the
 * window keeps updating and the click returns at once. One edit at a time; poll_pending() (main loop) finishes it. */
typedef struct { pid_t pid; long t0; char kind[8], mode[12], id[12], type[40], params[400], pkg[P], label[64], page[32]; Nd old; int before; } Pend;
static Pend pend;
static void push_undo(const char *pkg, const char *label, const char *page, const char *mode, const char *id, const char *type, const char *params) {
    if (nU == MAXU) { memmove(&U[0], &U[1], sizeof(Undo) * (MAXU - 1)); nU--; }
    Undo *u = &U[nU++]; snprintf(u->pkg, P, "%s", pkg); snprintf(u->label, 64, "%s", label); snprintf(u->page, 32, "%s", page);
    snprintf(u->mode, 8, "%s", mode); snprintf(u->id, 12, "%s", id); snprintf(u->type, 40, "%s", type); snprintf(u->params, 400, "%s", params);
}
static long now_s(void) { return (long)time(NULL); }
static int max_id(void);
/* start bp_edit_op for the selected event in the background; kind = edit | add | del | undo (what finish_edit does with the result) */
static int start_edit(const char *kind, const char *mode, const char *id, const char *type, const char *params, const Nd *old) {
    memset(&pend, 0, sizeof pend);
    snprintf(pend.kind, 8, "%s", kind); snprintf(pend.mode, 12, "%s", mode); snprintf(pend.page, 32, "%s", cpage); snprintf(pend.id, 12, "%s", id); snprintf(pend.type, 40, "%s", type); snprintf(pend.params, 400, "%s", params);
    snprintf(pend.pkg, P, "%s", E[sel].pkg); snprintf(pend.label, 64, "%s", E[sel].name); if (old) pend.old = *old; pend.before = max_id();
    char bin[P], out[P], pkg[P]; snprintf(bin, P, "%s/ops/+x/bp_edit_op.+x", g_pkg); join(out, g_state, "edit.out"); snprintf(pkg, P, "%s/event_pkg", pend.pkg);
    char *av[10]; int n = 0; av[n++] = bin; av[n++] = g_target; av[n++] = pkg; av[n++] = pend.label; av[n++] = pend.mode;
    if (!strcmp(mode, "edit")) { av[n++] = pend.id; av[n++] = pend.type; av[n++] = pend.params; }
    else if (!strcmp(mode, "append")) { av[n++] = pend.type; av[n++] = pend.params; }
    else if (!strcmp(mode, "trigger") || !strcmp(mode, "switch")) { av[n++] = pend.params; }
    else if (!strcmp(mode, "delete_page")) { }
    else if (!strcmp(mode, "new_page")) { }
    else av[n++] = pend.id;
    av[n] = NULL;
    pid_t pid = fork(); if (pid < 0) { pend.pid = 0; return -1; }
    if (pid == 0) {
        if (!freopen(out, "w", stdout) || !freopen(out, "a", stderr)) _exit(126);
        if (!freopen("/dev/null", "r", stdin)) _exit(126);
        setenv("BP_PAGE", pend.page[0] ? pend.page : "page_1", 1);
        execv(av[0], av); _exit(127);
    }
    pend.pid = pid; pend.t0 = now_s(); return 0;
}
/* the child ended (rc) or was killed: do what the synchronous code used to do after the edit */
static void finish_edit(int rc) {
    Pend p = pend; pend.pid = 0;
    if (!strcmp(p.kind, "undo")) {
        if (rc) { snprintf(msg, sizeof msg, "undo %s on %s FAILED (rc=%d), see state/edit.out", p.mode, p.label, rc); push_undo(p.pkg, p.label, p.page, p.mode, p.id, p.type, p.params); }
        else snprintf(msg, sizeof msg, "undone: %s on %s", p.mode, p.label);
        load_nodes(); return;
    }
    if (rc) { snprintf(msg, sizeof msg, "%s FAILED (rc=%d), see state/edit.out", p.kind, rc); return; }
    if (!strcmp(p.kind, "pdel")) {
        load_pages(); cpage[0] = 0; load_nodes();
        snprintf(msg, sizeof msg, "deleted %s of %s (now %d page%s); a deleted page cannot be brought back from here", p.page, p.label, nPG, nPG == 1 ? "" : "s"); return;
    }
    if (!strcmp(p.kind, "page")) {
        load_pages(); if (nPG) snprintf(cpage, sizeof cpage, "%s", PG[nPG - 1].name); load_nodes();
        snprintf(msg, sizeof msg, "added %s (trigger on-click, no commands); a page cannot be removed from here, so this step has no undo", cpage); return;
    }
    load_nodes();
    if (!strcmp(p.kind, "sw")) {
        push_undo(p.pkg, p.label, p.page, "switch", "", "", p.old.type);       /* old switch kept in old.type */
        snprintf(msg, sizeof msg, "%s switch: %s  ->  %s", p.page, p.old.type[0] ? p.old.type : "(none)", p.params[0] ? p.params : "(none)"); return;
    }
    if (!strcmp(p.kind, "trig")) {
        push_undo(p.pkg, p.label, p.page, "trigger", "", "", p.old.type);       /* old trigger kept in old.type */
        snprintf(msg, sizeof msg, "%s trigger: %s  ->  %s", p.page, p.old.type[0] ? p.old.type : "(none)", p.params); return;
    }
    if (!strcmp(p.kind, "del")) {
        push_undo(p.pkg, p.label, p.page, "append", "", p.old.type, p.old.params);
        snprintf(msg, sizeof msg, "deleted command %d (%s %s); undo puts it back at the end of the list", p.old.id, p.old.type, p.old.params);
    } else if (!strcmp(p.kind, "add")) {
        int nid = max_id(); if (nid > p.before) { char b[12]; snprintf(b, sizeof b, "%d", nid); push_undo(p.pkg, p.label, p.page, "delete", b, "", ""); csel = nid; }
        snprintf(msg, sizeof msg, "added command %d: %s %s", nid, p.type, p.params);
    } else {
        push_undo(p.pkg, p.label, p.page, "edit", p.id, p.old.type, p.old.params);
        snprintf(msg, sizeof msg, "command %d: %s %s  ->  %s %s", p.old.id, p.old.type, p.old.params, p.type, p.params);
    }
}
/* main loop: has the background edit finished? (30 s limit) */
static int poll_pending(void) {
    if (!pend.pid) return 0;
    int st; pid_t r = waitpid(pend.pid, &st, WNOHANG);
    if (r == pend.pid) { finish_edit(WIFEXITED(st) ? WEXITSTATUS(st) : -1); return 1; }
    if (now_s() - pend.t0 > 30) { kill(pend.pid, SIGKILL); waitpid(pend.pid, NULL, 0); finish_edit(-1); return 1; }
    return 0;
}
static int max_id(void) { int m = 0; for (int i = 0; i < nN; i++) if (N[i].id > m) m = N[i].id; return m; }
/* typed value -> type + params. "verb=advance|arg=" (first field has '=') keeps the selected command's type; "type|params" sets both. */
static int split_typed(const char *t, const Nd *cur, char *type, size_t tn, char *params, size_t pn) {
    const char *bar = strchr(t, '|'), *eq = strchr(t, '=');
    if (eq && (!bar || eq < bar)) { if (!cur) return 0; snprintf(type, tn, "%s", cur->type); snprintf(params, pn, "%s", t); return 1; }
    if (!bar) { snprintf(type, tn, "%s", t); params[0] = 0; return 1; }
    snprintf(type, tn, "%.*s", (int)(bar - t), t); snprintf(params, pn, "%s", bar + 1); return 1;
}
/* LOAD:<app> - make the page "every verb page of one game": rows EVENT | verb | @.apps/<app>/events/<verb> for each events/<verb>/event_pkg, sorted */
static int cmp_s(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
/* rows for bp_sources.pdl from one folder of event folders (each with event_pkg/pages): sorted, relative to the target house */
static void write_rows(const char *dir, const char *rel_prefix, const char *what) {
    DIR *d = opendir(dir); if (!d) { snprintf(msg, sizeof msg, "no events folder for %s (%s)", what, dir); return; }
    static char names[MAXE][64]; int n = 0; struct dirent *de;
    while ((de = readdir(d)) && n < MAXE) {
        if (de->d_name[0] == '.' || strlen(de->d_name) >= 64) continue;
        char t[P]; snprintf(t, P, "%s/%s/event_pkg/pages", dir, de->d_name); if (access(t, F_OK) == 0) snprintf(names[n++], 64, "%s", de->d_name);
    }
    closedir(d); qsort(names, (size_t)n, 64, cmp_s);
    char out[P], tmp[P]; join(out, g_state, "bp_sources.pdl"); snprintf(tmp, P, "%s.tmp", out);
    FILE *f = fopen(tmp, "w"); if (!f) { snprintf(msg, sizeof msg, "cannot write bp_sources.pdl"); return; }
    for (int i = 0; i < n; i++) fprintf(f, "EVENT | %s | %s/%s\n", names[i], rel_prefix, names[i]);
    fclose(f); rename(tmp, out); sel = 0; csel = -1; cpage[0] = 0; nU = 0;
    snprintf(msg, sizeof msg, "loaded %d event(s) of %s (undo history cleared)", n, what);
}
static void load_common(void) {
    char dir[P]; snprintf(dir, P, "%s/common_events", g_target);
    write_rows(dir, "common_events", "common events");
}
static void load_game(const char *app) {
    if (!app[0] || !clean(app, 0)) { snprintf(msg, sizeof msg, "refused: bad game name"); return; }
    char dir[P]; snprintf(dir, P, "%s/@.apps/%s/events", g_target, app);
    DIR *d = opendir(dir); if (!d) { snprintf(msg, sizeof msg, "no events folder for %s (%s)", app, dir); return; }
    static char names[MAXE][64]; int n = 0; struct dirent *de;
    while ((de = readdir(d)) && n < MAXE) {
        if (de->d_name[0] == '.' || strlen(de->d_name) >= 64) continue;
        char t[P]; snprintf(t, P, "%s/%s/event_pkg/pages", dir, de->d_name); if (access(t, F_OK) == 0) snprintf(names[n++], 64, "%s", de->d_name);
    }
    closedir(d); qsort(names, (size_t)n, 64, cmp_s);
    char out[P], tmp[P]; join(out, g_state, "bp_sources.pdl"); snprintf(tmp, P, "%s.tmp", out);
    FILE *f = fopen(tmp, "w"); if (!f) { snprintf(msg, sizeof msg, "cannot write bp_sources.pdl"); return; }
    for (int i = 0; i < n; i++) fprintf(f, "EVENT | %s | @.apps/%s/events/%s\n", names[i], app, names[i]);
    fclose(f); rename(tmp, out); sel = 0; csel = -1; nU = 0;
    snprintf(msg, sizeof msg, "loaded %d verb pages of %s (undo history cleared)", n, app);
}
static void edit_verb(const char *verb) {
    if (!nE) { snprintf(msg, sizeof msg, "no event selected"); return; }
    load_nodes();
    if (!strncmp(verb, "CSEL:", 5)) { csel = atoi(verb + 5); snprintf(msg, sizeof msg, "command %d selected", csel); return; }
    if (!strncmp(verb, "PSEL:", 5)) {
        load_pages(); int ok = 0; for (int i = 0; i < nPG; i++) if (!strcmp(PG[i].name, verb + 5)) ok = 1;
        if (!ok) { snprintf(msg, sizeof msg, "no such page: %s", verb + 5); return; }
        snprintf(cpage, sizeof cpage, "%s", verb + 5); csel = -1; load_nodes(); snprintf(msg, sizeof msg, "%s selected (trigger %s)", cpage, page_trig(cpage)); return;
    }
    if (pend.pid) { snprintf(msg, sizeof msg, "an edit is still running (%s on %s): wait for it", pend.kind, pend.label); return; }
    if (!strcmp(verb, "PNEW")) {
        if (start_edit("page", "new_page", "", "", "", NULL)) snprintf(msg, sizeof msg, "new page: could not start the edit");
        else snprintf(msg, sizeof msg, "adding a page to %s ...", E[sel].name);
        return;
    }
    if (!strncmp(verb, "PDEL:", 5)) {      /* typed confirmation: the page name; a deleted page cannot be undone */
        load_pages(); if (!cpage[0]) { snprintf(msg, sizeof msg, "no page selected"); return; }
        if (strcmp(verb + 5, cpage)) { snprintf(msg, sizeof msg, "not deleted: to delete the selected page type its name exactly (%s)", cpage); return; }
        if (nPG <= 1) { snprintf(msg, sizeof msg, "not deleted: %s is the only page of %s", cpage, E[sel].name); return; }
        if (start_edit("pdel", "delete_page", "", "", "", NULL)) snprintf(msg, sizeof msg, "delete page: could not start the edit");
        else snprintf(msg, sizeof msg, "deleting %s ...", cpage);
        return;
    }
    if (!strncmp(verb, "SWITCH", 6)) {
        const char *sv = verb[6] == ':' ? verb + 7 : ""; load_pages();      /* a blank field arrives as a bare SWITCH: clears the switch */
        if (!clean(sv, 0)) { snprintf(msg, sizeof msg, "refused: a switch name may only contain letters, digits and _ - ."); return; }
        if (!cpage[0]) { snprintf(msg, sizeof msg, "no page selected"); return; }
        Nd old; memset(&old, 0, sizeof old); snprintf(old.type, sizeof old.type, "%s", page_sw(cpage));
        if (start_edit("sw", "switch", "", "", sv, &old)) snprintf(msg, sizeof msg, "switch: could not start the edit");
        else snprintf(msg, sizeof msg, "setting the switch of %s ...", cpage);
        return;
    }
    if (!strncmp(verb, "TRIG:", 5)) {
        const char *tv = verb + 5; load_pages();
        if (!*tv) { snprintf(msg, sizeof msg, "type a trigger first (for example on-click, player-touch, Autorun)"); return; }
        if (!clean(tv, 0)) { snprintf(msg, sizeof msg, "refused: a trigger may only contain letters, digits and _ - ."); return; }
        if (!cpage[0]) { snprintf(msg, sizeof msg, "no page selected"); return; }
        Nd old; memset(&old, 0, sizeof old); snprintf(old.type, sizeof old.type, "%s", page_trig(cpage));
        if (start_edit("trig", "trigger", "", "", tv, &old)) snprintf(msg, sizeof msg, "trigger: could not start the edit");
        else snprintf(msg, sizeof msg, "setting the trigger of %s ...", cpage);
        return;
    }
    if (!strcmp(verb, "UNDO")) {
        if (!nU) { snprintf(msg, sizeof msg, "nothing to undo"); return; }
        Undo u = U[--nU]; int found = -1; for (int i = 0; i < nE; i++) if (!strcmp(E[i].pkg, u.pkg)) found = i;
        if (found < 0) { snprintf(msg, sizeof msg, "undo: the event %s is not on this page any more", u.label); return; }
        sel = found; snprintf(cpage, sizeof cpage, "%s", u.page);
        if (start_edit("undo", u.mode, u.id, u.type, u.params, NULL)) snprintf(msg, sizeof msg, "undo: could not start the edit");
        else snprintf(msg, sizeof msg, "undoing %s on %s ...", u.mode, u.label);
        return;
    }
    Nd *cur = node_by_id(csel); char type[40], params[400], idb[12]; snprintf(idb, sizeof idb, "%d", csel);
    if (!strcmp(verb, "CDEL")) {
        if (!cur) { snprintf(msg, sizeof msg, "no command selected"); return; }
        Nd old = *cur;
        if (start_edit("del", "delete", idb, "", "", &old)) snprintf(msg, sizeof msg, "delete: could not start the edit");
        else snprintf(msg, sizeof msg, "deleting command %d ...", old.id);
        return;
    }
    int is_add = !strncmp(verb, "ADD:", 4); const char *typed = verb + (is_add ? 4 : 5);
    if (!is_add && strncmp(verb, "EDIT:", 5)) { snprintf(msg, sizeof msg, "unknown verb %s", verb); return; }
    if (!*typed) { snprintf(msg, sizeof msg, "type a value first"); return; }
    if (!clean(typed, 1)) { snprintf(msg, sizeof msg, "refused: only letters, digits and _ - . : , = | / space are allowed"); return; }
    if (!split_typed(typed, is_add ? NULL : cur, type, sizeof type, params, sizeof params) || !*type || !clean(type, 0)) { snprintf(msg, sizeof msg, "need  type|params  (or key=value|key=value for the selected command)"); return; }
    if (!known_type(type)) { snprintf(msg, sizeof msg, "refused: '%s' is not a command type in the events-hq registry (#.ref/menu/event_commands.registry.pdl)", type); return; }
    if (is_add) {
        if (start_edit("add", "append", "", type, params, NULL)) snprintf(msg, sizeof msg, "add: could not start the edit");
        else snprintf(msg, sizeof msg, "adding %s ...", type);
        return;
    }
    if (!cur) { snprintf(msg, sizeof msg, "no command selected"); return; }
    Nd old = *cur;
    if (start_edit("edit", "edit", idb, type, params, &old)) snprintf(msg, sizeof msg, "edit: could not start the edit");
    else snprintf(msg, sizeof msg, "editing command %d ...", old.id);
}

/* ---- who calls whom: call_common_event nodes of every loaded event (all pages). Computed by scanning, never stored (design 12.22, section 4 of the design). ---- */
typedef struct { char from[64], page[32], to[64]; } Edge;
#define MAXEDGE 256
static Edge EG[MAXEDGE]; static int nEG = 0; static time_t eg_at = 0;
static int is_loaded(const char *name) { for (int i = 0; i < nE; i++) if (!strcmp(E[i].name, name)) return 1; return 0; }
static void edges_of(const char *name, const char *pkg) {
    char root[P]; snprintf(root, P, "%s/event_pkg/pages", pkg); DIR *d = opendir(root); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (strncmp(de->d_name, "page_", 5)) continue;
        char ir[P], l[900]; snprintf(ir, P, "%s/%s/event.ir.pdl", root, de->d_name); FILE *f = fopen(ir, "r"); if (!f) continue;
        while (fgets(l, sizeof l, f) && nEG < MAXEDGE) {
            if (strncmp(l, "NODE", 4) || !strstr(l, "type=call_common_event")) continue;
            char *en = strstr(l, "event_name="); if (!en) continue; en += 11; size_t k = strcspn(en, "|\r\n ");
            snprintf(EG[nEG].from, 64, "%s", name); snprintf(EG[nEG].page, 32, "%s", de->d_name); snprintf(EG[nEG].to, 64, "%.*s", (int)(k < 63 ? k : 63), en); nEG++;
        }
        fclose(f);
    }
    closedir(d);
}
/* who calls whom: call_common_event nodes of every loaded event AND of every common event of the target house that is not loaded (marked "not loaded" in the list) */
static void scan_edges(void) {
    if (time(NULL) == eg_at) return; eg_at = time(NULL); nEG = 0;
    for (int i = 0; i < nE; i++) edges_of(E[i].name, E[i].pkg);
    char dir[P]; snprintf(dir, P, "%s/common_events", g_target); DIR *d = opendir(dir); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) { if (de->d_name[0] == '.' || is_loaded(de->d_name)) continue; char pk[P]; snprintf(pk, P, "%s/%s", dir, de->d_name); edges_of(de->d_name, pk); }
    closedir(d);
}

/* ---- ui.txt ---- */
static void write_ui(void) {
    static char buf[24000]; size_t o = 0;
#define PUT(...) do { if (o < sizeof buf - 600) o += (size_t)snprintf(buf + o, sizeof buf - o, __VA_ARGS__); } while (0)
    char act[P]; snprintf(act, P, "'%s/ops/+x/bp_req.+x'", g_pkg);
    int totflags = 0; for (int i = 0; i < nE; i++) totflags += E[i].nflags;
    PUT("status_line=BLUEPRINTZ  page: %d events, %d flags  play=%s  %s\n", nE, totflags, play_mode_on() ? "ON" : "off", ctl_status);
    PUT("msg=%s\n", msg);
    PUT("target_line=target house: %s\n", g_target);
    PUT("e_count=%d\n", nE);
    for (int i = 0; i < nE; i++) {
        const char *cls = !strcmp(E[i].state, "queued") ? "st-queued" : !strcmp(E[i].state, "running") ? "st-run" : !strcmp(E[i].state, "ran") ? "st-ran" : "st-wait";
        PUT("e_%d_label=%s%s   %s   %s%s\n", i, i == sel ? "* " : "  ", E[i].name, E[i].trig, E[i].state, E[i].nflags ? "   !! flags" : "");
        PUT("e_%d_class=ms-row %s%s\n", i, cls, i == sel ? " active" : "");
        PUT("e_%d_action=%s 'SEL:%d'\n", i, act, i);
    }
    if (nE) {
        PUT("sel_line=selected: %s   (%s)\n", E[sel].name, E[sel].pkg);
        int k = 0; for (int i = 0; i < nF; i++) if (!strcmp(F[i].ev, E[sel].name)) k++;
        PUT("f_count=%d\n", k);
        int j = 0; for (int i = 0; i < nF; i++) if (!strcmp(F[i].ev, E[sel].name)) { PUT("f_%d_label=FLAG %s  %s: %s\n", j, F[i].code, F[i].page, F[i].detail); j++; }
        if (!k) PUT("f_none=no problems found for this event\n"); else PUT("f_none=\n");
    } else { PUT("sel_line=no events: put EVENT rows in state/bp_sources.pdl\nf_count=0\nf_none=\n"); }
    load_nodes();
    PUT("n_count=%d\n", nN);
    for (int i = 0; i < nN; i++) {
        PUT("n_%d_label=%s%d  %s  %s\n", i, N[i].id == csel ? "* " : "  ", N[i].id, N[i].type, N[i].params);
        PUT("n_%d_class=%s\n", i, N[i].id == csel ? "bp-node active" : "bp-node");
        PUT("n_%d_action=%s 'CSEL:%d'\n", i, act, N[i].id);
    }
    PUT("n_none=%s\n", nN ? "" : "this page has no commands");
    PUT("pg_count=%d\n", nPG);
    for (int i = 0; i < nPG; i++) {
        int on = !strcmp(PG[i].name, cpage);
        PUT("pg_%d_label=%s%s   trigger: %s%s%s\n", i, on ? "* " : "  ", PG[i].name, PG[i].trig[0] ? PG[i].trig : "(none)", PG[i].sw[0] ? "   switch: " : "", PG[i].sw);
        PUT("pg_%d_class=%s\n", i, on ? "bp-node active" : "bp-node");
        PUT("pg_%d_action=%s 'PSEL:%s'\n", i, act, PG[i].name);
    }
    PUT("page_line=editing %s of %s   (%d page%s)\n", cpage[0] ? cpage : "-", nE ? E[sel].name : "-", nPG, nPG == 1 ? "" : "s");
    scan_edges();
    { int xi = 0, total = 0;
      for (int i = 0; i < nEG; i++) {
        if (nE && !strcmp(EG[i].from, E[sel].name)) { PUT("x_%d_label=calls  %s   (from %s)%s\n", xi, EG[i].to, EG[i].page, is_loaded(EG[i].to) ? "" : "   [not loaded]"); xi++; }
        else if (nE && !strcmp(EG[i].to, E[sel].name)) { PUT("x_%d_label=called by  %s   (%s)%s\n", xi, EG[i].from, EG[i].page, is_loaded(EG[i].from) ? "" : "   [not loaded]"); xi++; }
      }
      total = xi; PUT("x_count=%d\n", total); PUT("x_none=%s\n", total ? "" : "no calls to or from this event (loaded events and the house's common events)"); }
    PUT("pgnew_action=%s 'PNEW'\ntrig_action=%s 'TRIG'\nloadce_action=%s 'LOADCE'\nbook_action=%s 'BOOK'\npdel_action=%s 'PDEL'\nsw_action=%s 'SWITCH'\n", act, act, act, act, act, act);
    PUT("n_hint=%s\n", nN && csel >= 0 ? "type below: key=value|key=value  edits the selected command;  type|key=value  sets both" : "");
    PUT("load_gdh_action=%s 'LOAD:game-dev-hq'\nload_rpg_action=%s 'LOAD:rpg-pvp-hq'\n", act, act);
    PUT("edit_action=%s 'EDIT'\nadd_action=%s 'ADD'\ncdel_action=%s 'CDEL'\nundo_action=%s 'UNDO'\nundo_line=undo steps: %d\n", act, act, act, act, nU);
    PUT("t_count=%d\n", ntrace);
    for (int i = 0; i < ntrace; i++) PUT("t_%d_label=%s\n", i, trace_rows[i]);
    static const char *verbs[][2] = { {"play","PLAY"}, {"stopplay","STOPPLAY"}, {"pause","PAUSE"}, {"resume","RESUME"}, {"stepev","STEPEV"}, {"stepcmd","STEPCMD"},
                                      {"runend","RUNEND"}, {"abort","ABORT"}, {"fire","FIRE"}, {"detach","DETACH"}, {"refresh","REFRESH"} };
    for (size_t i = 0; i < sizeof verbs / sizeof verbs[0]; i++) PUT("%s_action=%s '%s'\n", verbs[i][0], act, verbs[i][1]);
#undef PUT
    if (strcmp(buf, last_ui)) {
        char tmp[P], dst[P]; join(dst, g_state, "ui.txt"); snprintf(tmp, P, "%s.tmp", dst);
        FILE *f = fopen(tmp, "w"); if (!f) return; fputs(buf, f); fclose(f); rename(tmp, dst);
        snprintf(last_ui, sizeof last_ui, "%s", buf);
    }
}

static void handle(const char *verb) {
    if (!strncmp(verb, "SEL:", 4)) { sel = atoi(verb + 4); csel = -1; cpage[0] = 0; snprintf(msg, sizeof msg, "selected"); return; }
    if (!strcmp(verb, "BOOK")) {   /* B4b: compile the loaded events plus the house's common events into per-trigger programs under state/book/ (generated, never the source) */
        char bin[P], src[P], out[P], res[P]; ops_path(bin, "bp_book_op"); join(src, g_state, "bp_sources.pdl"); join(out, g_state, "book"); join(res, g_state, "book.out");
        char *av[] = { bin, g_target, src, out, NULL }; int rc = run(av, res, 20); char line[300] = "";
        if (!first_line(res, line, sizeof line)) snprintf(line, sizeof line, "no output");
        if (rc == 0) snprintf(msg, sizeof msg, "%s", line); else snprintf(msg, sizeof msg, "book compile failed (%d): %s", rc, line);
        return;
    }
    if (!strcmp(verb, "LOADCE")) { load_common(); return; }
    if (!strncmp(verb, "LOAD:", 5)) { load_game(verb + 5); return; }
    if (!strncmp(verb, "CSEL:", 5) || !strncmp(verb, "EDIT:", 5) || !strncmp(verb, "ADD:", 4) || !strcmp(verb, "CDEL") || !strcmp(verb, "UNDO") || !strncmp(verb, "PSEL:", 5) || !strcmp(verb, "PNEW") || !strncmp(verb, "TRIG:", 5) || !strncmp(verb, "PDEL:", 5) || !strncmp(verb, "SWITCH", 6)) { edit_verb(verb); return; }
    if (!strcmp(verb, "REFRESH")) { snprintf(msg, sizeof msg, "refreshed"); return; }
    if (!strcmp(verb, "PLAY")) {
        set_play(1); int n = 0;
        for (int i = 0; i < nE; i++) if (strstr(E[i].trig, ":Autorun")) { ctl("fire", E[i].pkg, "Autorun"); n++; }
        snprintf(msg, sizeof msg, "play ON; %d Autorun event(s) fired; the others wait for their trigger", n); return;
    }
    if (!strcmp(verb, "STOPPLAY")) { set_play(0); snprintf(msg, sizeof msg, "play off"); return; }
    if (!strcmp(verb, "PAUSE")) { ctl("pause", NULL, NULL); return; }
    if (!strcmp(verb, "RESUME")) { ctl("resume", NULL, NULL); return; }
    if (!strcmp(verb, "DETACH")) { ctl("detach", NULL, NULL); return; }
    if (!strcmp(verb, "STEPEV")) { ctl("step-event", NULL, NULL); return; }
    if (!strcmp(verb, "STEPCMD")) { ctl("step-cmd", NULL, NULL); return; }
    if (!strcmp(verb, "RUNEND")) { ctl("run-to-end", NULL, NULL); return; }
    if (!strcmp(verb, "ABORT")) { ctl("abort", NULL, NULL); return; }
    if (!strcmp(verb, "FIRE")) {
        if (!nE) { snprintf(msg, sizeof msg, "no event selected"); return; }
        char t[200]; first_trigger(&E[sel], t, sizeof t);
        if (!t[0]) { snprintf(msg, sizeof msg, "%s has no trigger to send", E[sel].name); return; }
        ctl("fire", E[sel].pkg, t); return;
    }
    snprintf(msg, sizeof msg, "unknown verb %s", verb);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: blueprintz_manager <house_root> [package_dir]\n"); return 1; }
    snprintf(g_house, sizeof g_house, "%s", argv[1]);
    if (argc > 2 && argv[2][0]) snprintf(g_pkg, sizeof g_pkg, "%s", argv[2]);
    else { char exe[P]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1); if (n < 0) return 1; exe[n] = 0;
           char *s = strrchr(exe, '/'); if (s) *s = 0; s = strrchr(exe, '/'); if (s) *s = 0; snprintf(g_pkg, sizeof g_pkg, "%s", exe); }
    snprintf(g_state, sizeof g_state, "%s/state", g_pkg); mkdir(g_state, 0755);
    char req[P]; join(req, g_state, "request.txt");
    long seen = fsize(req);                       /* only requests that arrive after start */
    time_t last_scan = 0; last_ui[0] = 0;
    for (;;) {
        read_target();
        int dirty = 0;
        long sz = fsize(req);
        if (sz < seen) seen = 0;
        if (sz > seen) {
            FILE *f = fopen(req, "r");
            if (f) { fseek(f, seen, SEEK_SET); char l[P];
                while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; if (l[0]) { scan(); handle(l); dirty = 1; } }
                fclose(f); }
            seen = sz;
        }
        if (poll_pending()) dirty = 1;
        time_t now = time(NULL);
        if (dirty || now != last_scan) { scan(); status(); write_ui(); last_scan = now; }
        struct timespec ts = { 0, 150 * 1000000L }; nanosleep(&ts, NULL);
    }
    return 0;
}
