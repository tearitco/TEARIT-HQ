/* data_hq_manager - the backend of the Data Studio window (data-hq, phase P2, 2026-11-09; BOOK 08-roadmap/design-docs/DATA-HQ-DATA-STUDIO-PLAN-2026-11-09.md sections 2, 4, 8).
 * One binary, three ways to run it, no shell and no Python anywhere:
 *   data_hq_manager.+x <house_root> <package_dir>          the window's <module> (the shared renderer forks it, SIGTERMs it on close, and registers it in the process registry):
 *                                                          builds the window's vars file once, then BLOCKS in poll() on inotify watches of the dataset roots (a game archived or a new
 *                                                          dataset folder wakes it; nothing else does) with a 30 s tick that only checks the parent is alive. No timer work, no stat loop.
 *   data_hq_manager.+x act <verb> [arg] <package_dir> <house>   ONE short process per click (the renderer appends the two trailing args to every action): applies the verb to the view
 *                                                          state, renders the one chart or table page the view needs, rewrites the vars file and exits.
 *   data_hq_manager.+x --once <house_root> <package_dir>   build the vars file once and exit (the harness hook)
 * Files (all under <package_dir>; everything but studio.pdl and datasets.pdl is a PROJECTION or a PREFERENCE: delete state/ and nothing is lost):
 *   studio.pdl      DATA: GROUP | id | header, ROOT | group | path | provider | name (where datasets live, relative to the house root), SET | key | value (sizes and caps)
 *   datasets.pdl    the registry read by `data_op list` (class public|private; no row and no positive reason = PRIVATE)
 *   state/scan.txt  the last scan (one row per dataset)        state/view.txt   what is selected, the mode, the panel, the table page (a window preference, never game state)
 *   state/data_hq_ui.txt  the vars file the window reads        state/out/<key>/  per-dataset cache (export, dash.pdl), state/out/view.raw (+ .receipt.txt) the ONE chart the canvas shows
 * It only READS game data: the exporter (`data_op provider`, which runs the game's own `GAME | export` row) writes only inside state/out. Charts are made only when the selection, the mode or the panel
 * changes, never on a tick. The cache is capped (SET cache_dirs, SET cache_kb); the oldest unselected dataset folders go first.
 * Verbs: toggle <key> | mode dashboard|table | next | prev | pgdn | pgup | colr | coll | refresh | clear | noop
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/data_hq_manager.+x ops/data_hq_manager.c   (one line in &.widgits/_shared-lib/harness/ops/build_harness_ops.sh) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <poll.h>
#include <time.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/inotify.h>

#define MAXDS 400
#define MAXROOT 16
#define MAXGRP 8
#define MAXPAN 160
#define MAXSEL 8
#define MAXTAB 24
#define OUTCAP (2 * 1024 * 1024)
#define PB 1024

typedef struct { char id[24], head[96]; } Group;
typedef struct { char group[24], path[PB], provider[160], name[80]; } Root;
typedef struct { char rel[PB], key[12], name[96], kind[16], cls[12], why[48], provider[160]; long rows, cols, bytes; int grp; } Ds;
typedef struct { char id[96], title[200], table[PB], spec[PB]; int dsi, grouped; } Panel;

static char house[PB], pkg[PB], sdir[PB], odir[PB], uipath[PB], dataop[PB], chartop[PB];
static Group grp[MAXGRP]; static int ngrp;
static Root root[MAXROOT]; static int nroot;
static Ds ds[MAXDS]; static int nds;
static int cfg_cw = 798, cfg_ch = 499, cfg_rows = 16, cfg_cols = 7, cfg_maxsel = 4, cfg_maxside = 200, cfg_cdirs = 24; static long cfg_ckb = 16384;
static volatile sig_atomic_t quit_flag;
static void on_term(int s) { (void)s; quit_flag = 1; }

/* ---- view state (a window preference) */
static char vmode[16] = "dashboard"; static int vpanel, vtable, vpage, vcolpage; static char vsel[MAXSEL][PB]; static int nvsel;
static char msg[240];   /* one line the status row shows after an action */

static char *oputs; static size_t on_, ocap;
static void put(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void put(const char *fmt, ...) { va_list a; va_start(a, fmt); if (on_ < ocap - 1) on_ += (size_t)vsnprintf(oputs + on_, ocap - on_, fmt, a); va_end(a); if (on_ > ocap - 1) on_ = ocap - 1; }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int split_bar(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static unsigned fnv(const char *s) { unsigned h = 2166136261u; for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; } return h; }
static int exists(const char *p) { struct stat sb; return stat(p, &sb) == 0; }
static int isdir(const char *p) { struct stat sb; return stat(p, &sb) == 0 && S_ISDIR(sb.st_mode); }
static long fsize(const char *p) { struct stat sb; return stat(p, &sb) == 0 ? (long)sb.st_size : -1; }
static void mkdirs(const char *p) { char t[PB]; snprintf(t, sizeof t, "%s", p); for (char *c = t + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(t, 0775); *c = '/'; } mkdir(t, 0775); }
static char *slurp(const char *p, size_t cap) { FILE *f = fopen(p, "rb"); if (!f) return NULL; char *b = malloc(cap + 1); if (!b) { fclose(f); return NULL; } size_t n = fread(b, 1, cap, f); fclose(f); b[n] = 0; return b; }
static int write_atomic(const char *path, const char *data) { char tmp[PB + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return -1; fputs(data, f); if (fclose(f)) { unlink(tmp); return -1; } return rename(tmp, path); }
static int rm_cb(const char *p, const struct stat *sb, int t, struct FTW *w) { (void)sb; (void)t; (void)w; return remove(p); }
static void rmtree(const char *p) { if (strstr(p, "/state/out") || strstr(p, "/export") || strstr(p, "/dash")) nftw(p, rm_cb, 16, FTW_DEPTH | FTW_PHYS); }   /* only ever called on our own cache paths */

/* run argv, capture stdout+stderr (cap bytes) and wait at most `secs`; returns the exit code or -1 on a breach/fork failure */
static int run(char *const av[], char *out, size_t cap, int secs) {
    int pf[2]; if (pipe(pf)) return -1; pid_t pid = fork();
    if (pid < 0) { close(pf[0]); close(pf[1]); return -1; }
    if (pid == 0) { dup2(pf[1], 1); dup2(pf[1], 2); close(pf[0]); close(pf[1]); int dn = open("/dev/null", O_RDONLY); if (dn >= 0) dup2(dn, 0); execv(av[0], av); _exit(127); }
    close(pf[1]); size_t n = 0; time_t t0 = time(NULL); int st = 0, brk = 0;
    for (;;) { struct pollfd p = { pf[0], POLLIN, 0 }; int r = poll(&p, 1, 500);
        if (r > 0) { ssize_t k = read(pf[0], out + n, n + 1 < cap ? cap - n - 1 : 0); if (k > 0) n += (size_t)k; else if (k == 0) break; else if (errno != EINTR) break; if (n + 1 >= cap) { char junk[4096]; while (read(pf[0], junk, sizeof junk) > 0) {} break; } }
        if (time(NULL) - t0 > secs) { kill(pid, SIGKILL); brk = 1; break; } }
    out[n] = 0; close(pf[0]); waitpid(pid, &st, 0); if (brk) return -1; return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* ---- config */
static void load_config(void) {
    char p[PB]; snprintf(p, sizeof p, "%s/studio.pdl", pkg); char *b = slurp(p, 65536); ngrp = nroot = 0; if (!b) return; char *sv = NULL;
    for (char *ln = strtok_r(b, "\n", &sv); ln; ln = strtok_r(NULL, "\n", &sv)) { char *t = trim(ln); if (!*t || *t == '#') continue; char *f[8]; int n = split_bar(t, f, 8);
        if (!strcmp(f[0], "GROUP") && n >= 3 && ngrp < MAXGRP) { snprintf(grp[ngrp].id, sizeof grp[0].id, "%s", f[1]); snprintf(grp[ngrp].head, sizeof grp[0].head, "%s", f[2]); ngrp++; }
        else if (!strcmp(f[0], "ROOT") && n >= 3 && nroot < MAXROOT) { Root *r = &root[nroot++]; snprintf(r->group, sizeof r->group, "%s", f[1]); snprintf(r->path, sizeof r->path, "%s", f[2]); snprintf(r->provider, sizeof r->provider, "%s", n >= 4 && strcmp(f[3], "-") ? f[3] : ""); snprintf(r->name, sizeof r->name, "%s", n >= 5 ? f[4] : ""); }
        else if (!strcmp(f[0], "SET") && n >= 3) { long v = atol(f[2]);
            if (!strcmp(f[1], "canvas_w") && v >= 200 && v <= 4000) cfg_cw = (int)v; else if (!strcmp(f[1], "canvas_h") && v >= 120 && v <= 3000) cfg_ch = (int)v;
            else if (!strcmp(f[1], "table_rows") && v >= 4 && v <= 60) cfg_rows = (int)v; else if (!strcmp(f[1], "table_cols") && v >= 2 && v <= 20) cfg_cols = (int)v;
            else if (!strcmp(f[1], "max_selected") && v >= 1 && v <= MAXSEL) cfg_maxsel = (int)v; else if (!strcmp(f[1], "max_sidebar") && v >= 10 && v <= MAXDS) cfg_maxside = (int)v;
            else if (!strcmp(f[1], "cache_dirs") && v >= 2 && v <= 200) cfg_cdirs = (int)v; else if (!strcmp(f[1], "cache_kb") && v >= 256 && v <= 1048576) cfg_ckb = v; } }
    free(b);
}
static int group_of(const char *id) { for (int i = 0; i < ngrp; i++) if (!strcmp(grp[i].id, id)) return i; return -1; }

/* ---- scan: one `data_op list` per existing root; the scan is saved so a click does not walk the disk again */
static void rel_of(const char *abs, char *o, size_t n) { size_t hl = strlen(house); if (!strncmp(abs, house, hl) && abs[hl] == '/') snprintf(o, n, "%s", abs + hl + 1); else snprintf(o, n, "%s", abs); }
static void abs_of(const char *rel, char *o, size_t n) { if (rel[0] == '/') snprintf(o, n, "%s", rel); else snprintf(o, n, "%s/%s", house, rel); }
static char scan_err[240];   /* why a root was not listed (a refused registry row, a root that failed): shown in the status line, never silent, and nothing from that root is listed (fail closed) */
static int scan(void) {
    char *out = malloc(OUTCAP); if (!out) return -1; nds = 0; scan_err[0] = 0; char reg[PB]; snprintf(reg, sizeof reg, "%s/datasets.pdl", pkg);
    for (int ri = 0; ri < nroot; ri++) { char ab[PB]; abs_of(root[ri].path, ab, sizeof ab); if (!exists(ab)) continue;
        char *av[] = { dataop, "list", ab, "--registry", reg, "--house", house, NULL }; int rc = run(av, out, OUTCAP, 30); if (rc != 0 && rc != 1) { if (!scan_err[0]) { char *l = strstr(out, "data_op:"); const char *why = l ? l + 9 : out; char *c = strstr(why, ".pdl: "); if (c) why = c + 6; snprintf(scan_err, sizeof scan_err, "SCAN REFUSED (%s): %.150s", root[ri].group, why); clean(scan_err); } continue; }
        char *sv = NULL; for (char *ln = strtok_r(out, "\n", &sv); ln && nds < MAXDS; ln = strtok_r(NULL, "\n", &sv)) { if (strncmp(ln, "FOUND |", 7)) continue; char *f[10]; int n = split_bar(ln, f, 10); if (n < 8) continue;
            Ds *d = &ds[nds]; memset(d, 0, sizeof *d); rel_of(f[1], d->rel, sizeof d->rel); int dup = 0; for (int i = 0; i < nds; i++) if (!strcmp(ds[i].rel, d->rel)) dup = 1; if (dup) continue;
            snprintf(d->kind, sizeof d->kind, "%s", f[2]); d->rows = atol(f[3]); d->cols = atol(f[4]); d->bytes = atol(f[5]); snprintf(d->cls, sizeof d->cls, "%s", strcmp(f[6], "public") ? "private" : "public"); snprintf(d->why, sizeof d->why, "%s", f[7]);
            snprintf(d->provider, sizeof d->provider, "%s", n >= 9 && strcmp(f[8], "-") ? f[8] : root[ri].provider); d->grp = group_of(root[ri].group);
            char ab2[PB]; abs_of(d->rel, ab2, sizeof ab2); const char *bn = strrchr(ab2, '/'); bn = bn ? bn + 1 : ab2; if (root[ri].name[0] && !strcmp(bn, strrchr(ab, (int)0x2f) ? strrchr(ab, (int)0x2f) + 1 : ab)) snprintf(d->name, sizeof d->name, "%s", root[ri].name); else if (root[ri].name[0]) snprintf(d->name, sizeof d->name, "%s/%s", root[ri].name, bn); else snprintf(d->name, sizeof d->name, "%s", bn);
            snprintf(d->key, sizeof d->key, "%08x", fnv(d->rel)); nds++; } }
    free(out); return 0;
}
static void save_scan(void) { char *b = malloc(nds * 700 + 400); if (!b) return; b[0] = 0; size_t n = 0; if (scan_err[0]) n += (size_t)snprintf(b + n, 300, "ERR\t%s\n", scan_err); char p[PB]; snprintf(p, sizeof p, "%s/scan.txt", sdir);
    for (int i = 0; i < nds; i++) { Ds *d = &ds[i]; n += (size_t)snprintf(b + n, 700, "%s\t%d\t%s\t%ld\t%ld\t%ld\t%s\t%s\t%s\t%s\n", d->rel, d->grp, d->kind, d->rows, d->cols, d->bytes, d->cls, d->why, d->provider[0] ? d->provider : "-", d->name); }
    write_atomic(p, b); free(b); }
static void load_scan(void) { char p[PB]; snprintf(p, sizeof p, "%s/scan.txt", sdir); char *b = slurp(p, 1 << 20); nds = 0; scan_err[0] = 0; if (!b) return; char *sv = NULL;
    for (char *ln = strtok_r(b, "\n", &sv); ln && nds < MAXDS; ln = strtok_r(NULL, "\n", &sv)) { if (!strncmp(ln, "ERR\t", 4)) { snprintf(scan_err, sizeof scan_err, "%s", ln + 4); continue; } char *f[10]; int n = 0; char *q = ln; while (n < 10) { f[n++] = q; char *t = strchr(q, '\t'); if (!t) break; *t = 0; q = t + 1; } if (n < 10) continue;
        Ds *d = &ds[nds]; memset(d, 0, sizeof *d); snprintf(d->rel, sizeof d->rel, "%s", f[0]); d->grp = atoi(f[1]); snprintf(d->kind, sizeof d->kind, "%s", f[2]); d->rows = atol(f[3]); d->cols = atol(f[4]); d->bytes = atol(f[5]);
        snprintf(d->cls, sizeof d->cls, "%s", f[6]); snprintf(d->why, sizeof d->why, "%s", f[7]); snprintf(d->provider, sizeof d->provider, "%s", strcmp(f[8], "-") ? f[8] : ""); snprintf(d->name, sizeof d->name, "%s", f[9]); snprintf(d->key, sizeof d->key, "%08x", fnv(d->rel)); nds++; }
    free(b); }

/* ---- view state */
static void load_view(void) { char p[PB]; snprintf(p, sizeof p, "%s/view.txt", sdir); char *b = slurp(p, 65536); nvsel = 0; if (!b) return; char *sv = NULL;
    for (char *ln = strtok_r(b, "\n", &sv); ln; ln = strtok_r(NULL, "\n", &sv)) { if (!strncmp(ln, "mode=", 5)) snprintf(vmode, sizeof vmode, "%s", !strcmp(ln + 5, "table") ? "table" : "dashboard"); else if (!strncmp(ln, "panel=", 6)) vpanel = atoi(ln + 6); else if (!strncmp(ln, "table=", 6)) vtable = atoi(ln + 6);
        else if (!strncmp(ln, "page=", 5)) vpage = atoi(ln + 5); else if (!strncmp(ln, "colpage=", 8)) vcolpage = atoi(ln + 8); else if (!strncmp(ln, "sel=", 4) && nvsel < MAXSEL) snprintf(vsel[nvsel++], PB, "%s", ln + 4); }
    free(b); }
static void save_view(void) { char b[MAXSEL * PB + 256], p[PB]; size_t n = (size_t)snprintf(b, sizeof b, "mode=%s\npanel=%d\ntable=%d\npage=%d\ncolpage=%d\n", vmode, vpanel, vtable, vpage, vcolpage); for (int i = 0; i < nvsel; i++) n += (size_t)snprintf(b + n, sizeof b - n, "sel=%s\n", vsel[i]);
    snprintf(p, sizeof p, "%s/view.txt", sdir); write_atomic(p, b); }
static int is_sel(const char *rel) { for (int i = 0; i < nvsel; i++) if (!strcmp(vsel[i], rel)) return 1; return 0; }
static void prune_sel(void) { int k = 0; for (int i = 0; i < nvsel; i++) { int ok = 0; for (int j = 0; j < nds; j++) if (!strcmp(ds[j].rel, vsel[i])) ok = 1; if (ok) { if (k != i) snprintf(vsel[k], PB, "%s", vsel[i]); k++; } } nvsel = k; }

/* ---- per-dataset cache: export (ledger folders), dash.pdl (autodash --no-render) */
typedef struct { Ds *d; char dir[PB], tsrc[PB], err[200]; int ok; } Prep;
static Prep prep[MAXSEL]; static int nprep;
static void touch_use(const char *dir) { static long seq; if (!seq) seq = (long)time(NULL) * 1000; char p[PB], b[32]; snprintf(p, sizeof p, "%s/use", dir); snprintf(b, sizeof b, "%ld\n", ++seq); write_atomic(p, b); }
static void ensure(Prep *pr) {
    Ds *d = pr->d; char ab[PB]; abs_of(d->rel, ab, sizeof ab); snprintf(pr->dir, sizeof pr->dir, "%s/%s", odir, d->key); mkdirs(pr->dir); pr->ok = 0; pr->err[0] = 0;
    int ledger = !strcmp(d->kind, "ledgerdir"); char src[PB]; if (ledger) snprintf(src, sizeof src, "%s/ledger.txt", ab); else snprintf(src, sizeof src, "%s", ab);
    char mk[96], old[96] = "", mp[PB], ep[PB], dp[PB], xp[PB]; snprintf(mk, sizeof mk, "size=%ld prov=%s", fsize(src), d->provider); snprintf(mp, sizeof mp, "%s/mark", pr->dir); snprintf(ep, sizeof ep, "%s/err.txt", pr->dir); snprintf(dp, sizeof dp, "%s/dash/dash.pdl", pr->dir); snprintf(xp, sizeof xp, "%s/export", pr->dir);
    { char *m = slurp(mp, 90); if (m) { snprintf(old, sizeof old, "%s", trim(m)); free(m); } }
    if (ledger && d->provider[0]) snprintf(pr->tsrc, sizeof pr->tsrc, "%s", xp); else snprintf(pr->tsrc, sizeof pr->tsrc, "%s", ledger ? src : ab);
    if (strcmp(old, mk) || (!exists(dp) && !exists(ep))) {      /* new, or the source changed (a live ledger grew): rebuild this dataset only */
        char *out = malloc(OUTCAP); if (!out) { snprintf(pr->err, sizeof pr->err, "out of memory"); return; } char dd[PB]; snprintf(dd, sizeof dd, "%s/dash", pr->dir); rmtree(xp); rmtree(dd); unlink(ep); int rc = 0; char eb[PB];
        if (ledger && d->provider[0]) { char app[PB]; abs_of(d->provider, app, sizeof app); char *av[] = { dataop, "provider", app, ab, xp, "--house", house, NULL }; rc = run(av, out, OUTCAP, 70); if (rc != 0) { char *l = strstr(out, "refused"); snprintf(eb, sizeof eb, "exporter failed (rc=%d): %.120s", rc, l ? l : out); clean(eb); snprintf(pr->err, sizeof pr->err, "%s", eb); } }
        if (!pr->err[0]) { char *av[] = { dataop, "autodash", pr->tsrc, dd, "--no-render", NULL }; rc = run(av, out, OUTCAP, 40); if (rc != 0) { char *l = strstr(out, "autodash:"); snprintf(eb, sizeof eb, "no panels: %.150s", l ? l : out); clean(eb); snprintf(pr->err, sizeof pr->err, "%s", eb); } }
        if (pr->err[0]) { FILE *f = fopen(ep, "w"); if (f) { fprintf(f, "%s\n", pr->err); fclose(f); } }
        { FILE *f = fopen(mp, "w"); if (f) { fprintf(f, "%s\n", mk); fclose(f); } } free(out);
    } else { char *e = slurp(ep, 199); if (e) { snprintf(pr->err, sizeof pr->err, "%s", trim(e)); free(e); } }
    pr->ok = pr->err[0] == 0; touch_use(pr->dir);
}

/* ---- the cache cap: oldest unselected dataset folder first, by the number in its `use` file (a counter, not a clock) */
static long du(const char *p) { long t = 0; DIR *dd = opendir(p); if (!dd) return 0; struct dirent *e; while ((e = readdir(dd))) { if (e->d_name[0] == '.') continue; char q[PB * 2]; snprintf(q, sizeof q, "%s/%s", p, e->d_name); struct stat sb; if (lstat(q, &sb)) continue; if (S_ISDIR(sb.st_mode)) t += du(q); else t += (long)sb.st_size; } closedir(dd); return t; }
static void cache_prune(void) {
    for (int guard = 0; guard < 400; guard++) { DIR *dd = opendir(odir); if (!dd) return; struct dirent *e; int n = 0; long kb = 0; char best[PB] = ""; long bestuse = LONG_MAX;
        while ((e = readdir(dd))) { if (e->d_name[0] == '.' || strlen(e->d_name) != 8) continue; char q[PB * 2]; snprintf(q, sizeof q, "%s/%s", odir, e->d_name); if (!isdir(q)) continue; n++; kb += du(q) / 1024; int selected = 0;
            for (int i = 0; i < nprep; i++) if (!strcmp(prep[i].d->key, e->d_name)) selected = 1; if (selected) continue; char up[PB * 2]; snprintf(up, sizeof up, "%s/use", q); char *u = slurp(up, 30); long uv = u ? atol(u) : 0; free(u); if (uv < bestuse) { bestuse = uv; snprintf(best, sizeof best, "%s", q); } }
        closedir(dd); if ((n <= cfg_cdirs && kb <= cfg_ckb) || !best[0]) return; rmtree(best); }
}

/* ---- panels: the chosen datasets' autodash panels, the same panel of each dataset side by side so two games can be compared by pressing next */
static Panel pan[MAXPAN]; static int npan;
static void collect_panels(void) {
    npan = 0; static Panel tmp[MAXPAN]; int nt = 0;
    for (int i = 0; i < nprep && nt < MAXPAN; i++) { if (!prep[i].ok) continue; char dp[PB]; snprintf(dp, sizeof dp, "%s/dash/dash.pdl", prep[i].dir); char *b = slurp(dp, 1 << 20); if (!b) continue; int first = nt; char *sv = NULL;
        for (char *ln = strtok_r(b, "\n", &sv); ln && nt < MAXPAN; ln = strtok_r(NULL, "\n", &sv)) { char *f[10]; int n; if (!strncmp(ln, "CHART |", 7)) { n = split_bar(ln, f, 10); if (n < 3) continue; Panel *p = &tmp[nt++]; memset(p, 0, sizeof *p); snprintf(p->id, sizeof p->id, "%s", f[1]); snprintf(p->title, sizeof p->title, "%s", f[2]); snprintf(p->spec, sizeof p->spec, "%s", dp); p->dsi = i; p->grouped = n >= 7 && strcmp(f[6], "-") != 0; } }
        sv = NULL; char *b2 = slurp(dp, 1 << 20); if (!b2) { free(b); continue; }
        for (char *ln = strtok_r(b2, "\n", &sv); ln; ln = strtok_r(NULL, "\n", &sv)) { char *f[10]; if (strncmp(ln, "PANEL |", 7)) continue; int n = split_bar(ln, f, 10); if (n < 6) continue; for (int k = first; k < nt; k++) if (tmp[k].dsi == i && !strcmp(tmp[k].id, f[1])) snprintf(tmp[k].table, sizeof tmp[k].table, "%s", f[5]); }
        free(b2); free(b); }
    { static Panel s2[MAXPAN]; int k = 0; for (int pass = 1; pass >= 0; pass--) for (int a = 0; a < nt; a++) if (tmp[a].grouped == pass) s2[k++] = tmp[a]; memcpy(tmp, s2, (size_t)k * sizeof tmp[0]); }   /* a line per group (a series key) before the others: the first panel is the useful one */
    int used[MAXPAN] = {0}; for (int a = 0; a < nt; a++) { if (used[a]) continue; for (int b2 = a; b2 < nt; b2++) if (!used[b2] && !strcmp(tmp[b2].id, tmp[a].id)) { used[b2] = 1; pan[npan++] = tmp[b2]; } }
}

/* ---- tables of the chosen datasets */
typedef struct { char file[PB], label[160]; int dsi; } Tab;
static Tab tabs[MAXTAB]; static int ntab;
/* a table with at least one data row (two lines): the ones with rows come first, an empty table (no trades in this game) after them */
static int has_rows(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 0; int nl = 0, c; long n = 0; while (nl < 2 && (c = fgetc(f)) != EOF && n++ < 1000000) if (c == 10) nl++; fclose(f); return nl >= 2; }
static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static void collect_tables(void) {
    ntab = 0;
    for (int i = 0; i < nprep; i++) { if (!prep[i].ok && !prep[i].tsrc[0]) continue; if (!exists(prep[i].tsrc)) continue;
        if (isdir(prep[i].tsrc)) { DIR *dd = opendir(prep[i].tsrc); if (!dd) continue; char *names[64]; int nn = 0; struct dirent *e; while ((e = readdir(dd)) && nn < 64) { size_t l = strlen(e->d_name); if (l > 4 && (!strcmp(e->d_name + l - 4, ".csv") || !strcmp(e->d_name + l - 4, ".tsv"))) names[nn++] = strdup(e->d_name); } closedir(dd); qsort(names, (size_t)nn, sizeof *names, cmpstr);
            for (int pass = 0; pass < 2; pass++) for (int k = 0; k < nn && ntab < MAXTAB; k++) { char fp[PB * 2]; snprintf(fp, sizeof fp, "%s/%s", prep[i].tsrc, names[k]); if (has_rows(fp) != (pass == 0)) continue; Tab *t = &tabs[ntab++]; snprintf(t->file, sizeof t->file, "%s/%s", prep[i].tsrc, names[k]); snprintf(t->label, sizeof t->label, "%s of %s", names[k], prep[i].d->name); t->dsi = i; } for (int k = 0; k < nn; k++) free(names[k]); }
        else if (ntab < MAXTAB) { Tab *t = &tabs[ntab++]; snprintf(t->file, sizeof t->file, "%s", prep[i].tsrc); const char *bn = strrchr(t->file, '/'); snprintf(t->label, sizeof t->label, "%s of %s", bn ? bn + 1 : t->file, prep[i].d->name); t->dsi = i; } }
}

/* split a `data_op show` row at " | " (the exact separator, so a lone bar inside a value survives) */
static int split_cells(char *line, char **c, int max) { int n = 0; char *p = line; while (n < max) { c[n++] = p; char *b = strstr(p, " | "); if (!b) break; *b = 0; p = b + 3; } return n; }

/* ---- the vars file */
typedef struct { char text[260]; char cls[24]; } TRow;
static TRow trow[64]; static int ntrow; static char title[300], status[400]; static int show_dash;
static void addrow(const char *cls, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void addrow(const char *cls, const char *fmt, ...) { if (ntrow >= 64) return; va_list a; va_start(a, fmt); vsnprintf(trow[ntrow].text, sizeof trow[0].text, fmt, a); va_end(a); clean(trow[ntrow].text); snprintf(trow[ntrow].cls, sizeof trow[0].cls, "%s", cls); ntrow++; }

static int render_panel(Panel *p) {     /* the ONE chart the canvas shows; remade only when the (panel, size, source) tuple changed */
    char key[PB * 3], kp[PB], vp[PB]; snprintf(vp, sizeof vp, "%s/view.raw", odir); snprintf(kp, sizeof kp, "%s/view.key", odir);
    snprintf(key, sizeof key, "%s|%s|%dx%d|%ld|%ld", p->spec, p->id, cfg_cw, cfg_ch, fsize(p->table), fsize(p->spec)); char *old = slurp(kp, sizeof key - 1); if (old && !strcmp(trim(old), key) && exists(vp)) { free(old); return 0; } free(old);
    char sz[32]; snprintf(sz, sizeof sz, "%dx%d", cfg_cw, cfg_ch); char *out = malloc(65536); if (!out) return -1; char *av[] = { chartop, p->spec, p->table, vp, "--id", p->id, "--size", sz, NULL }; int rc = run(av, out, 65536, 30); free(out);
    if (rc != 0) return -1; strcat(key, "\n"); write_atomic(kp, key); return 0;
}

static void build_tables_text(void) {
    if (!ntab) { addrow("ds-hint", "no tables for the selected datasets (see the status line)"); return; }
    if (vtable < 0) vtable = ntab - 1; if (vtable >= ntab) vtable = 0; Tab *t = &tabs[vtable]; int N = cfg_rows; if (vpage < 0) vpage = 0; int K = (vpage + 1) * N; if (K > 2000) { K = 2000; vpage = 2000 / N - 1; }
    char hd[16]; snprintf(hd, sizeof hd, "%d", K); char *out = malloc(OUTCAP); if (!out) return; char *av[] = { dataop, "show", t->file, "--head", hd, NULL }; int rc = run(av, out, OUTCAP, 20);
    if (rc != 0) { addrow("ds-hint", "cannot show %s: %.150s", t->label, out); free(out); return; }
    static char *ln[2100]; int nl = 0; char *sv = NULL; for (char *l = strtok_r(out, "\n", &sv); l && nl < 2100; l = strtok_r(NULL, "\n", &sv)) ln[nl++] = l;
    long total = 0; int nrow = nl - 1; if (nl > 0 && !strncmp(ln[nl - 1], "# ", 2) && strstr(ln[nl - 1], "more rows")) { char *o = strstr(ln[nl - 1], "(of "); if (o) total = atol(o + 4); nrow--; } else total = nrow;
    if (nl == 0) { addrow("ds-hint", "%s is empty", t->label); free(out); return; }
    char *hc[64]; char hline[4096]; snprintf(hline, sizeof hline, "%s", ln[0]); int nc = split_cells(hline, hc, 64); int W = cfg_cols; int ncp = (nc + W - 1) / W; if (ncp < 1) ncp = 1; if (vcolpage < 0) vcolpage = 0; if (vcolpage >= ncp) vcolpage = ncp - 1; int c0 = vcolpage * W, c1 = c0 + W > nc ? nc : c0 + W;
    long maxpage = total > 0 ? (total - 1) / N : 0; if (vpage > maxpage) vpage = (int)maxpage; int r0 = vpage * N, r1 = r0 + N; if (r1 > nrow) r1 = nrow; if (r0 > nrow) r0 = nrow;
    snprintf(title, sizeof title, "table %d/%d: %s   rows %d-%ld of %ld   columns %d-%d of %d", vtable + 1, ntab, t->label, r0 + 1 > r1 ? 0 : r0 + 1, (long)r1, total, c0 + 1, c1, nc);
    int wd[20] = {0}; static char cells[64][64][40]; char *rc_[64]; for (int c = c0; c < c1; c++) wd[c - c0] = (int)strlen(hc[c]);
    for (int r = r0; r < r1; r++) { char row[4096]; snprintf(row, sizeof row, "%s", ln[r + 1]); int n2 = split_cells(row, rc_, 64); for (int c = c0; c < c1; c++) { const char *v = c < n2 ? rc_[c] : ""; snprintf(cells[r - r0][c - c0], 40, "%s", v); int l = (int)strlen(cells[r - r0][c - c0]); if (l > wd[c - c0]) wd[c - c0] = l; } }
    char line[512]; size_t k = 0; k += (size_t)snprintf(line + k, sizeof line - k, "%-6s ", "row"); for (int c = c0; c < c1; c++) { int w = wd[c - c0] > 18 ? 18 : wd[c - c0]; k += (size_t)snprintf(line + k, sizeof line - k, "| %-*.*s ", w, w, hc[c]); } addrow("ds-th", "%s", line);
    for (int r = r0; r < r1; r++) { k = 0; k += (size_t)snprintf(line + k, sizeof line - k, "%-6d ", r + 1); for (int c = c0; c < c1; c++) { int w = wd[c - c0] > 18 ? 18 : wd[c - c0]; k += (size_t)snprintf(line + k, sizeof line - k, "| %-*.*s ", w, w, cells[r - r0][c - c0]); } addrow("ds-tr", "%s", line); }
    free(out);
}

static void build_ui(void) {
    ntrow = 0; title[0] = 0; status[0] = 0; show_dash = 0; nprep = 0;
    int nsel = 0; int anyprivate = 0; for (int i = 0; i < nds && nprep < MAXSEL; i++) if (is_sel(ds[i].rel)) { prep[nprep].d = &ds[i]; nprep++; nsel++; if (strcmp(ds[i].cls, "public")) anyprivate = 1; }
    for (int i = 0; i < nprep; i++) ensure(&prep[i]);
    cache_prune();
    int table_mode = !strcmp(vmode, "table");
    char what[200] = "";
    if (!nprep) { addrow("ds-hint", "Select one or more datasets in SESSIONS (left); [x] marks a selected one.");
        addrow("ds-hint", "Dashboard: panels are chosen from the column roles of the data; prev / next page through them (two games: the same panel side by side).");
        addrow("ds-hint", "Table: paged rows (in file order; sorting is not offered yet), rows up / rows down, cols < / cols >, prev / next = the next table.");
        addrow("ds-hint", "PRIVATE = a human played it, or nothing says it is safe: shown only here, never sent anywhere. public = pets only or a fixture.");
        addrow("ds-hint", "SQL + declarative cells + C stats; no Python/R/JS cells; notebooks come later.");
        snprintf(status, sizeof status, "%s%sno dataset selected", msg, msg[0] ? "  |  " : ""); }
    else if (!table_mode) {
        collect_panels(); if (npan == 0) { for (int i = 0; i < nprep; i++) if (!prep[i].ok) addrow("ds-warn", "%s: %s", prep[i].d->name, prep[i].err); if (!ntrow) addrow("ds-warn", "no panels for the selected datasets");
            snprintf(status, sizeof status, "%.60s no panels", msg); }
        else { if (vpanel < 0) vpanel = npan - 1; if (vpanel >= npan) vpanel = 0; Panel *p = &pan[vpanel];
            if (render_panel(p) == 0) { show_dash = 1; snprintf(what, sizeof what, "panel %d/%d  %s  (%s)", vpanel + 1, npan, p->title, prep[p->dsi].d->name); }
            else { addrow("ds-warn", "the chart for %s could not be made (chart_op failed)", p->id); snprintf(what, sizeof what, "panel %d/%d %s: chart failed", vpanel + 1, npan, p->id); }
            for (int i = 0; i < nprep; i++) if (!prep[i].ok) addrow("ds-warn", "%s: %s", prep[i].d->name, prep[i].err); snprintf(title, sizeof title, "%s", what); snprintf(status, sizeof status, "%.80s%s%s", msg, msg[0] ? "  |  " : "", what); }
    } else {
        collect_tables(); for (int i = 0; i < nprep; i++) if (!prep[i].ok && prep[i].err[0]) addrow("ds-warn", "%s: %s", prep[i].d->name, prep[i].err); build_tables_text();
        snprintf(status, sizeof status, "%.80s%s%s", msg, msg[0] ? "  |  " : "", title); }
    /* sidebar */
    on_ = 0; oputs[0] = 0; put("head=Data Studio\nmode=%s\nlbl_dash=%s\nlbl_table=%s\n", vmode, table_mode ? "Dashboard" : "[ Dashboard ]", table_mode ? "[ Table ]" : "Table");
    int rows = 0; static char sb[MAXDS + MAXGRP][400]; static char sc[MAXDS + MAXGRP][40], sa[MAXDS + MAXGRP][40]; int shown = 0, hidden = 0;
    for (int g = 0; g < ngrp; g++) { int cnt = 0; for (int i = 0; i < nds; i++) if (ds[i].grp == g) cnt++; if (!cnt) continue; snprintf(sb[rows], sizeof sb[0], "%s  (%d)", grp[g].head, cnt); snprintf(sc[rows], sizeof sc[0], "ds-grp"); snprintf(sa[rows], sizeof sa[0], "noop"); rows++;
        for (int i = 0; i < nds; i++) { if (ds[i].grp != g) continue; if (shown >= cfg_maxside) { hidden++; continue; } int sel = is_sel(ds[i].rel); int pub = !strcmp(ds[i].cls, "public"); shown++;
            snprintf(sb[rows], sizeof sb[0], "[%c] %s  %ld rows  %s", sel ? 'x' : ' ', ds[i].name, ds[i].rows, pub ? "public" : "PRIVATE"); snprintf(sc[rows], sizeof sc[0], "%s%s", pub ? "ds-pub" : "ds-priv", sel ? " ds-sel" : ""); snprintf(sa[rows], sizeof sa[0], "toggle %s", ds[i].key); rows++; } }
    put("n_ds=%d\n", rows); for (int i = 0; i < rows; i++) { clean(sb[i]); put("d_%d_label=%s\nd_%d_cls=%s\nd_%d_act=%s\n", i, sb[i], i, sc[i], i, sa[i]); }
    put("show_dash=%s\nshow_text=%s\ncanvas_raw=%s/view.raw\n", show_dash ? "1" : "", show_dash ? "" : "1", odir); put("title=%s\n", title);
    put("n_rows=%d\n", ntrow); for (int i = 0; i < ntrow; i++) put("r_%d_text=%s\nr_%d_cls=%s\n", i, trow[i].text, i, trow[i].cls);
    char st[500]; snprintf(st, sizeof st, "%d selected%s%s", nsel, anyprivate ? "  |  PRIVATE data selected: it stays on this machine" : "", hidden ? "  |  sidebar shows the first part of a long list" : "");
    char all[1000]; snprintf(all, sizeof all, "%s%s%s  |  %s", scan_err, scan_err[0] ? "  |  " : "", status[0] ? status : "ready", st); clean(all); put("status=%s\nnotice=SQL + declarative cells + C stats; no Python/R/JS cells; notebooks come later.\n", all);
    char *old = slurp(uipath, ocap); if (!old || strcmp(old, oputs)) write_atomic(uipath, oputs); free(old);
}

/* ---- verbs */
static int apply(const char *verb, const char *arg) {
    msg[0] = 0;
    if (!strcmp(verb, "noop")) return 0;
    if (!strcmp(verb, "toggle")) { int di = -1; for (int i = 0; i < nds; i++) if (!strcmp(ds[i].key, arg)) di = i; if (di < 0) { snprintf(msg, sizeof msg, "that dataset is gone: press Refresh"); return 1; }
        if (is_sel(ds[di].rel)) { int k = 0; for (int i = 0; i < nvsel; i++) if (strcmp(vsel[i], ds[di].rel)) { if (k != i) snprintf(vsel[k], PB, "%s", vsel[i]); k++; } nvsel = k; }
        else if (nvsel >= cfg_maxsel) { snprintf(msg, sizeof msg, "at most %d datasets at a time: untick one first", cfg_maxsel); return 1; } else snprintf(vsel[nvsel++], PB, "%s", ds[di].rel);
        vpanel = vtable = vpage = vcolpage = 0; return 1; }
    if (!strcmp(verb, "mode")) { snprintf(vmode, sizeof vmode, "%s", !strcmp(arg, "table") ? "table" : "dashboard"); return 1; }
    int table_mode = !strcmp(vmode, "table");
    if (!strcmp(verb, "next")) { if (table_mode) { vtable++; vpage = vcolpage = 0; } else vpanel++; return 1; }
    if (!strcmp(verb, "prev")) { if (table_mode) { vtable--; vpage = vcolpage = 0; } else vpanel--; return 1; }
    if (!strcmp(verb, "pgdn")) { if (table_mode) vpage++; else snprintf(msg, sizeof msg, "rows up / down page the Table mode"); return 1; }
    if (!strcmp(verb, "pgup")) { if (table_mode) vpage--; else snprintf(msg, sizeof msg, "rows up / down page the Table mode"); return 1; }
    if (!strcmp(verb, "colr")) { if (table_mode) vcolpage++; return 1; }
    if (!strcmp(verb, "coll")) { if (table_mode) vcolpage--; return 1; }
    if (!strcmp(verb, "clear")) { nvsel = 0; vpanel = vtable = vpage = vcolpage = 0; return 1; }
    if (!strcmp(verb, "refresh")) { scan(); save_scan(); prune_sel(); snprintf(msg, sizeof msg, "rescanned: %d datasets", nds); return 1; }
    snprintf(msg, sizeof msg, "unknown verb %.40s", verb); return 1;
}

static void setup(const char *h, const char *p) {
    snprintf(house, sizeof house, "%s", h); snprintf(pkg, sizeof pkg, "%s", p); snprintf(sdir, sizeof sdir, "%s/state", pkg); snprintf(odir, sizeof odir, "%s/state/out", pkg); snprintf(uipath, sizeof uipath, "%s/data_hq_ui.txt", sdir);
    snprintf(dataop, sizeof dataop, "%s/ops/+x/data_op.+x", pkg); snprintf(chartop, sizeof chartop, "%s/ops/+x/chart_op.+x", pkg); mkdirs(odir); ocap = 1 << 20; oputs = malloc(ocap); load_config();
}
static int lock_fd = -1;
static void lock(void) { char p[PB]; snprintf(p, sizeof p, "%s/.lock", sdir); lock_fd = open(p, O_CREAT | O_RDWR, 0664); if (lock_fd >= 0) flock(lock_fd, LOCK_EX); }
static void unlock(void) { if (lock_fd >= 0) { flock(lock_fd, LOCK_UN); close(lock_fd); lock_fd = -1; } }

static void full_build(int rescan) { lock(); if (rescan || !exists(uipath)) { scan(); save_scan(); } else load_scan(); load_view(); prune_sel(); msg[0] = 0; build_ui(); save_view(); unlock(); }

/* ---- the daemon: block on inotify; a created or removed folder under a dataset root (or the parent of a root that does not exist yet) rescans, twice (after 0.7 s and 4 s: a game being archived is moved in pieces) */
static int add_watches(int fd) {
    int n = 0; for (int i = 0; i < nroot; i++) { char ab[PB]; abs_of(root[i].path, ab, sizeof ab); char w[PB]; snprintf(w, sizeof w, "%s", ab);
        while (!isdir(w) && strlen(w) > strlen(house) + 1) { char *s = strrchr(w, '/'); if (!s) break; *s = 0; }      /* a root not made yet: watch the nearest folder that exists, so its creation is seen */
        if (isdir(w) && inotify_add_watch(fd, w, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM) >= 0) n++; }
    return n;
}
static long mono_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int daemon_main(void) {
    pid_t owner = getppid(); prctl(PR_SET_PDEATHSIG, SIGTERM); if (getppid() != owner) return 0; signal(SIGTERM, on_term); signal(SIGINT, on_term);
    full_build(1); int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC); if (fd >= 0) add_watches(fd); long due1 = 0, due2 = 0;
    while (!quit_flag) {
        long now = mono_ms(), wait = 30000; if (due1 && due1 - now < wait) wait = due1 - now; if (due2 && due2 - now < wait) wait = due2 - now; if (wait < 0) wait = 0;
        struct pollfd p = { fd, POLLIN, 0 }; int r = poll(&p, fd >= 0 ? 1 : 0, (int)wait); if (r < 0 && errno != EINTR) break;
        now = mono_ms();
        if (r > 0) { char buf[4096]; while (read(fd, buf, sizeof buf) > 0) {} due1 = now + 700; due2 = now + 4000; }
        else if (r == 0 && !due1 && !due2) { if (getppid() != owner) break; if (fd < 0) full_build(1); continue; }      /* the 30 s tick: only the owner check (and the no-inotify fallback rescan) */
        if (due1 && now >= due1) { due1 = 0; full_build(1); if (fd >= 0) add_watches(fd); }
        if (due2 && now >= due2) { due2 = 0; full_build(1); if (fd >= 0) add_watches(fd); } }
    return 0;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    if (argc >= 4 && !strcmp(argv[1], "act")) {        /* act <verb> [arg] <package_dir> <house>: the renderer appends the last two */
        const char *p = argv[argc - 2], *h = argv[argc - 1]; const char *verb = argv[2], *arg = argc >= 6 ? argv[3] : ""; setup(h, p); lock(); load_scan(); if (!nds && !exists(uipath)) { scan(); save_scan(); } load_view(); int ch = apply(verb, arg); if (ch) save_view(); else if (exists(uipath)) { unlock(); return 0; } prune_sel(); build_ui(); save_view(); unlock(); return 0; }
    if (argc >= 4 && !strcmp(argv[1], "--once")) { setup(argv[2], argv[3]); full_build(1); return 0; }
    if (argc >= 3) { setup(argv[1], argv[2]); return daemon_main(); }
    fprintf(stderr, "usage: data_hq_manager <house_root> <package_dir> | act <verb> [arg] <package_dir> <house> | --once <house_root> <package_dir>\n"); return 2;
}
