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
typedef struct { char pkg[P], label[64], mode[8], id[12], type[40], params[400]; } Undo;
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
static void load_nodes(void) {
    nN = 0; if (!nE) return;
    char p[P], l[900]; snprintf(p, P, "%s/event_pkg/pages/page_1/event.ir.pdl", E[sel].pkg);
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
/* run bp_edit_op for the selected event; returns 0 when the manager applied it */
static int do_edit(const char *mode, const char *id, const char *type, const char *params) {
    char bin[P], out[P]; snprintf(bin, P, "%s/ops/+x/bp_edit_op.+x", g_pkg); join(out, g_state, "edit.out");
    char pkg[P]; snprintf(pkg, P, "%s/event_pkg", E[sel].pkg);
    char *av[10]; int n = 0; av[n++] = bin; av[n++] = g_target; av[n++] = pkg; av[n++] = E[sel].name; av[n++] = (char *)mode;
    if (!strcmp(mode, "edit")) { av[n++] = (char *)id; av[n++] = (char *)type; av[n++] = (char *)params; }
    else if (!strcmp(mode, "append")) { av[n++] = (char *)type; av[n++] = (char *)params; }
    else av[n++] = (char *)id;
    av[n] = NULL;
    return run(av, out, 30);
}
static void push_undo(const char *mode, const char *id, const char *type, const char *params) {
    if (nU == MAXU) { memmove(&U[0], &U[1], sizeof(Undo) * (MAXU - 1)); nU--; }
    Undo *u = &U[nU++]; snprintf(u->pkg, P, "%s", E[sel].pkg); snprintf(u->label, 64, "%s", E[sel].name);
    snprintf(u->mode, 8, "%s", mode); snprintf(u->id, 12, "%s", id); snprintf(u->type, 40, "%s", type); snprintf(u->params, 400, "%s", params);
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
    if (!strcmp(verb, "UNDO")) {
        if (!nU) { snprintf(msg, sizeof msg, "nothing to undo"); return; }
        Undo u = U[--nU]; int found = -1; for (int i = 0; i < nE; i++) if (!strcmp(E[i].pkg, u.pkg)) found = i;
        if (found < 0) { snprintf(msg, sizeof msg, "undo: the event %s is not on this page any more", u.label); return; }
        sel = found;
        int rc = do_edit(u.mode, u.id, u.type, u.params);
        if (rc) snprintf(msg, sizeof msg, "undo %s on %s FAILED (rc=%d), see state/edit.out", u.mode, u.label, rc);
        else snprintf(msg, sizeof msg, "undone: %s on %s", u.mode, u.label);
        load_nodes(); return;
    }
    Nd *cur = node_by_id(csel); char type[40], params[400], idb[12]; snprintf(idb, sizeof idb, "%d", csel);
    if (!strcmp(verb, "CDEL")) {
        if (!cur) { snprintf(msg, sizeof msg, "no command selected"); return; }
        Nd old = *cur; int rc = do_edit("delete", idb, "", "");
        if (rc) { snprintf(msg, sizeof msg, "delete FAILED (rc=%d), see state/edit.out", rc); return; }
        push_undo("append", "", old.type, old.params); load_nodes();
        snprintf(msg, sizeof msg, "deleted command %d (%s %s); undo puts it back at the end of the list", old.id, old.type, old.params); return;
    }
    int is_add = !strncmp(verb, "ADD:", 4); const char *typed = verb + (is_add ? 4 : 5);
    if (!is_add && strncmp(verb, "EDIT:", 5)) { snprintf(msg, sizeof msg, "unknown verb %s", verb); return; }
    if (!*typed) { snprintf(msg, sizeof msg, "type a value first"); return; }
    if (!clean(typed, 1)) { snprintf(msg, sizeof msg, "refused: only letters, digits and _ - . : , = | / space are allowed"); return; }
    if (!split_typed(typed, is_add ? NULL : cur, type, sizeof type, params, sizeof params) || !*type || !clean(type, 0)) { snprintf(msg, sizeof msg, "need  type|params  (or key=value|key=value for the selected command)"); return; }
    if (!known_type(type)) { snprintf(msg, sizeof msg, "refused: '%s' is not a command type in the events-hq registry (#.ref/menu/event_commands.registry.pdl)", type); return; }
    if (is_add) {
        int before = max_id(), rc = do_edit("append", "", type, params);
        if (rc) { snprintf(msg, sizeof msg, "add FAILED (rc=%d), see state/edit.out", rc); return; }
        load_nodes(); int nid = max_id(); if (nid > before) { char b[12]; snprintf(b, sizeof b, "%d", nid); push_undo("delete", b, "", ""); csel = nid; }
        snprintf(msg, sizeof msg, "added command %d: %s %s", nid, type, params); return;
    }
    if (!cur) { snprintf(msg, sizeof msg, "no command selected"); return; }
    Nd old = *cur; int rc = do_edit("edit", idb, type, params);
    if (rc) { snprintf(msg, sizeof msg, "edit FAILED (rc=%d), see state/edit.out", rc); return; }
    push_undo("edit", idb, old.type, old.params); load_nodes();
    snprintf(msg, sizeof msg, "command %d: %s %s  ->  %s %s", old.id, old.type, old.params, type, params);
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
    PUT("n_none=%s\n", nN ? "" : "this event has no commands on page_1");
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
    if (!strncmp(verb, "SEL:", 4)) { sel = atoi(verb + 4); csel = -1; snprintf(msg, sizeof msg, "selected"); return; }
    if (!strncmp(verb, "LOAD:", 5)) { load_game(verb + 5); return; }
    if (!strncmp(verb, "CSEL:", 5) || !strncmp(verb, "EDIT:", 5) || !strncmp(verb, "ADD:", 4) || !strcmp(verb, "CDEL") || !strcmp(verb, "UNDO")) { edit_verb(verb); return; }
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
        time_t now = time(NULL);
        if (dirty || now != last_scan) { scan(); status(); write_ui(); last_scan = now; }
        struct timespec ts = { 0, 150 * 1000000L }; nanosleep(&ts, NULL);
    }
    return 0;
}
