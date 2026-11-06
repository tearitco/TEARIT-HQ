#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
/* pal_side - the editor's left side bar switch: pets <-> palettes (BOOK RPG-EDIT-HQ-PALETTES-SIDEBAR-RESEARCH-2026-11-02.md).
 * Usage: pal_side <house> <verb> [arg]
 *   open        show the palettes bar (hides the pets bar: they share the left slot), list view
 *   pets        show the pets bar (hides the palettes bar)
 *   none        hide both (the bars' "_" buttons)
 *   tiles <key> palettes bar, tile view of one category (key = a CATEGORY row of &.widgits/palettes/pallets.pdl)
 *   back        palettes bar, back to the category list
 *   arm <i>     arm tile i of the open category as the editor's brush: board/state/brush.txt (key, glyph, sprite) + pal_armed in pal.txt; a piececraft tile also goes
 *               through palettes_menu.sh arm-pc (the house's own brush file, palettes/state/pc_brush.txt)
 *   list        only rewrite pal.txt (category list for the window)
 * Files (board/state/): sides.txt (side_left / side_pal: "1" = shown, EMPTY = hidden, other keys kept; same convention as layout-studio side_toggle) and pal.txt
 * (what the window binds: n_pcat, pcat_<i>_key/_label, pal_is_list, pal_is_tiles, pal_key, pal_title). Both written atomically. A state-key switch, not an href stack (owner OK 2026-11-02). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <dirent.h>
#define MAXC 40
static char KEY[MAXC][64], LAB[MAXC][96]; static int nc, SIDE[MAXC];
static void trim(char *s) { size_t n = strlen(s); while (n && (s[n-1]==' '||s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]=='\t')) s[--n]=0; char *p=s; while (*p==' '||*p=='\t') p++; if (p!=s) memmove(s,p,strlen(p)+1); }
static void load_cats(const char *house) {
    char p[4200]; snprintf(p, sizeof p, "%s/&.widgits/palettes/pallets.pdl", house);
    FILE *f = fopen(p, "r"); if (!f) return; char ln[512];
    while (nc < MAXC && fgets(ln, sizeof ln, f)) {
        if (strncmp(ln, "CATEGORY", 8)) continue;
        char *fld[4]; int k = 0; char *c = ln;
        while (k < 4) { fld[k++] = c; char *b = strchr(c, '|'); if (!b) break; *b = 0; c = b + 1; }
        if (k < 3) continue;
        trim(fld[1]); trim(fld[2]);
        if (!fld[1][0] || !fld[2][0]) continue;
        snprintf(KEY[nc], sizeof KEY[0], "%s", fld[1]); snprintf(LAB[nc], sizeof LAB[0], "%s", fld[2]);
        /* My Pals is the Pals tab. Harness is not a bar row. My Palettes stays on this list. */
        SIDE[nc] = strcmp(fld[1], "my-pals") != 0 && strcmp(fld[1], "harness") != 0;
        nc++;
    }
    fclose(f);
}
/* set key=value ("" = hidden) in a kv file, keeping every other line */
static void kv_set(const char *path, const char *key, const char *val) {
    static char L[640][256]; int n = 0, at = -1; size_t kl = strlen(key); FILE *f = fopen(path, "r");
    if (f) { while (n < 640 && fgets(L[n], sizeof L[0], f)) { if (!strncmp(L[n], key, kl) && L[n][kl] == '=') at = n; n++; } fclose(f); }
    char row[256]; snprintf(row, sizeof row, "%s=%s\n", key, val);
    if (at >= 0) memcpy(L[at], row, strlen(row) + 1); else if (n < 640) { memcpy(L[n], row, strlen(row) + 1); n++; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", path); f = fopen(tmp, "w"); if (!f) return;
    for (int i = 0; i < n; i++) { fputs(L[i], f); }
    fclose(f); rename(tmp, path);
}
#define MAXT 48
static char TG[MAXT][64], TS[MAXT][1024]; static int nt;
static void kv_get(const char *path, const char *key, char *out, size_t n) {
    out[0] = 0; FILE *f = fopen(path, "r"); if (!f) return; static char ln[2048]; size_t kl = strlen(key);
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { snprintf(out, n, "%s", ln + kl + 1); out[strcspn(out, "\r\n")] = 0; }
    fclose(f);
}
/* ---- palette levels (owner 2026-11-03: "in tiles there is a tile type screen for some palette types, look out for that") ----
 * list (0) -> tile type (1: the palette's directories, d_) -> sheet (2: RPG Maker A/B/C.., c_) -> tileset (3: s_) -> tiles (4). A level is shown only when it has MORE than one choice;
 * picking one calls the palette window's own verb (palettes_menu.sh set-<kind>-dir / -tab / -tileset|-mod, so the choice is the same state the LyfeDesk palette windows use), reruns the
 * projector once and reads what it wrote. Nav state: board/state/pal_nav.txt (kind=, level=). */
#define MAXR 60
static char RK[3][MAXR][96], RL[3][MAXR][96]; static int RN[3];   /* 0 types, 1 sheets, 2 sets */
static int g_busy = 0;                                            /* set when a refresh was skipped because the machine is busy */
static int g_off = 0;                                             /* first tile shown: page * MAXT (paging past the first 48, 2026-11-03) */
static int ntot = 0;                                              /* tiles in the active tileset (only MAXT are shown) */
/* Bounded helper runs (CPU: the first version ran `timeout 3 manager` and `timeout 2 projector` per pick: both are polling daemons that never exit by themselves, so EVERY pick burned the full
 * 5 s; 14 picks in a row next to a browser were part of the 2026-11-03 02:49 guard kill, a finding in our own footprint). Now: (1) not started at all when the machine is already busy
 * (over 70 % of the CPUs in a 250 ms sample: the old view stays, a note says so), (2) niced, (3) stopped the moment the file it exists to write has changed, (4) a hard cap as the backstop. */
static int cpu_times(unsigned long long *busy, unsigned long long *all) {   /* /proc/stat first line: user nice system idle iowait irq softirq steal */
    FILE *f = fopen("/proc/stat", "r"); if (!f) return 0; unsigned long long v[8] = {0}; int n = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]); fclose(f);
    if (n < 4) return 0; unsigned long long idle = v[3] + v[4], tot = 0; for (int i = 0; i < 8; i++) tot += v[i]; *busy = tot - idle; *all = tot; return 1;
}
/* busy = over 70 % of all CPUs in the last 250 ms (a short sample, not the 1-minute load average, which stays high for minutes after a spike) */
static int machine_busy(void) {
    unsigned long long b0, a0, b1, a1; if (!cpu_times(&b0, &a0)) return 0; usleep(250000); if (!cpu_times(&b1, &a1) || a1 <= a0) return 0;
    return (b1 - b0) * 100 > 70 * (a1 - a0);
}
static long long sig_of(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec + st.st_size : 0; }
static int run_until_written(const char *exe, char *const argv[], const char *w1, const char *w2, int cap_ms) {
    long long b1 = w1 ? sig_of(w1) : 0, b2 = w2 ? sig_of(w2) : 0; pid_t c = fork(); if (c < 0) return 0;
    if (c == 0) { setpriority(PRIO_PROCESS, 0, 15); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); } execv(exe, argv); _exit(127); }
    int done = 0, waited = 0;
    while (waited < cap_ms) { usleep(40000); waited += 40; int st; if (waitpid(c, &st, WNOHANG) == c) { c = 0; done = 1; break; }
        if ((w1 && sig_of(w1) != b1) || (w2 && sig_of(w2) != b2)) { usleep(150000); done = 1; break; } }   /* changed: give the writer one short grace to finish its file, then stop it */
    if (c > 0) { kill(c, SIGTERM); int st, k = 0; while (waitpid(c, &st, WNOHANG) == 0 && k++ < 25) usleep(20000); if (k >= 25) { kill(c, SIGKILL); waitpid(c, &st, 0); } }
    return done;
}
/* the palette window's manager rewrites palettes-<kind>_state.txt and <kind>_options.txt when <kind>_active.txt changes (and at its first pass): without running it a tile type / tileset pick
 * made here is not reflected (found 2026-11-03: cdda_active said giant_96x96/fields while the grid still showed normal_32x32/furniture). */
static void run_manager(const char *house, const char *key) {
    char mgr[4200], pkg[4200], st[4300], op[4300]; snprintf(mgr, sizeof mgr, "%s/_.monads/_.livedesk-taskbar/ops/+x/palettes_manager.+x", house); snprintf(pkg, sizeof pkg, "%s/&.widgits/palettes", house);
    if (access(mgr, X_OK) != 0) return;
    snprintf(st, sizeof st, "%s/palettes-%s_state.txt", pkg, key); snprintf(op, sizeof op, "%s/%s_options.txt", pkg, key);
    char *av[] = { mgr, (char *)house, pkg, (char *)key, NULL }; run_until_written(mgr, av, st, op, 2000);
}
static void run_projector(const char *house, const char *key) {
    char proj[4200], pkg[4200], ui[4300]; snprintf(proj, sizeof proj, "%s/&.widgits/palettes/ops/+x/palettes_projector.+x", house); snprintf(pkg, sizeof pkg, "%s/&.widgits/palettes", house);
    if (access(proj, X_OK) != 0) return;
    snprintf(ui, sizeof ui, "%s/state/palettes-%s_ui.txt", pkg, key);
    char *av[] = { proj, (char *)house, pkg, (char *)key, NULL }; run_until_written(proj, av, ui, NULL, 1500);
}
/* read palettes/state/palettes-<key>_ui.txt (run the projector first when it is missing or `force`: it is a polling daemon in the taskbar windows; here it is bounded to 2 s and gone again) */
/* "<pre><n><suf>" with the suffix matched EXACTLY (sscanf("d_%d_key") alone also accepts d_0_label and d_0_active) */
static int idx_of(const char *ln, const char *pre, const char *suf, int *i) { size_t pl = strlen(pre); if (strncmp(ln, pre, pl)) return 0; char *e; long v = strtol(ln + pl, &e, 10); if (e == ln + pl || strcmp(e, suf)) return 0; *i = (int)v; return 1; }
static void load_ui(const char *house, const char *key, int force) {
    char p[4200]; snprintf(p, sizeof p, "%s/&.widgits/palettes/state/palettes-%s_ui.txt", house, key);
    if (!machine_busy()) { if (force) run_manager(house, key); if (force || access(p, R_OK) != 0) run_projector(house, key); } else g_busy = 1;   /* busy: keep what is on file, say so */
    nt = 0; ntot = 0; RN[0] = RN[1] = RN[2] = 0;
    FILE *f = fopen(p, "r"); if (!f) return; static char ln[2048];
    while (fgets(ln, sizeof ln, f)) {
        int i; char *eq = strchr(ln, '='); if (!eq) continue; *eq = 0; char *v = eq + 1; v[strcspn(v, "\r\n")] = 0;
        if (sscanf(ln, "t_%d_glyph", &i) == 1 && !strstr(ln, "sprite") && (i -= g_off) >= 0 && i < MAXT) { snprintf(TG[i], sizeof TG[0], "%.14s", v); if (i + 1 > nt) nt = i + 1; }
        else if (sscanf(ln, "t_%d_sprite", &i) == 1 && (i -= g_off) >= 0 && i < MAXT) { snprintf(TS[i], sizeof TS[0], "%s", v); if (i + 1 > nt) nt = i + 1; }
        else if (!strcmp(ln, "n_tiles")) ntot = atoi(v);
        else if (idx_of(ln, "d_", "_key", &i) && i >= 0 && i < MAXR) { snprintf(RK[0][i], 96, "%s", v); if (i + 1 > RN[0]) RN[0] = i + 1; }
        else if (idx_of(ln, "d_", "_label", &i) && i >= 0 && i < MAXR) snprintf(RL[0][i], 96, "%s", v);
        else if (idx_of(ln, "c_", "_letter", &i) && i >= 0 && i < MAXR) { snprintf(RK[1][i], 96, "%s", v); if (i + 1 > RN[1]) RN[1] = i + 1; }
        else if (idx_of(ln, "c_", "_label", &i) && i >= 0 && i < MAXR) snprintf(RL[1][i], 96, "%s", v);
        else if (idx_of(ln, "s_", "_key", &i) && i >= 0 && i < MAXR) { snprintf(RK[2][i], 96, "%s", v); if (i + 1 > RN[2]) RN[2] = i + 1; }
        else if (idx_of(ln, "s_", "_label", &i) && i >= 0 && i < MAXR) snprintf(RL[2][i], 96, "%s", v);
    }
    fclose(f);
}
/* does level L (1 types, 2 sheets, 3 sets) have a real choice for the loaded palette? */
static int applicable(int L) { return L >= 1 && L <= 3 && RN[L - 1] > 1; }
static int descend(int from) { for (int L = from + 1; L <= 3; L++) if (applicable(L)) return L; return 4; }
static int ascend(int from) { for (int L = from - 1; L >= 1; L--) if (applicable(L)) return L; return 0; }
/* the palette window's own verbs for a kind: dir / tab (sheet) / tileset; "" = this kind has no such level */
static void kind_verbs(const char *k, char *dv, char *tv, char *mv, size_t n) {
    dv[0] = tv[0] = mv[0] = 0; const char *st = NULL;
    if (!strcmp(k, "rmmv")) { snprintf(dv, n, "set-rmmv-dir"); snprintf(tv, n, "set-rmmv-tab"); snprintf(mv, n, "set-rmmv-tileset"); return; }
    if (!strcmp(k, "piececraft")) st = "pc"; else if (!strcmp(k, "cdda")) st = "cdda"; else if (!strcmp(k, "df")) st = "df"; else if (!strcmp(k, "doom")) st = "doom";
    else if (!strcmp(k, "tiled")) st = "tiled"; else if (!strcmp(k, "ohrrpgce")) st = "ohr"; else if (!strcmp(k, "my-palettes")) st = "mypal"; else if (!strcmp(k, "emojis")) st = "emoji";
    if (st) { snprintf(dv, n, "set-%s-dir", st); snprintf(mv, n, "set-%s-mod", st); }
}
static void run_menu(const char *house, const char *verb, const char *val) {
    char pm[4200], pkg[4200]; snprintf(pm, sizeof pm, "%s/&.widgits/palettes/palettes_menu.sh", house); snprintf(pkg, sizeof pkg, "%s/&.widgits/palettes", house);
    pid_t c = fork(); if (c == 0) { execlp("sh", "sh", pm, verb, pkg, val, (char *)NULL); _exit(127); } if (c > 0) { int st; waitpid(c, &st, 0); }
}
/* Sidebar layout history + start preference (owner 2026-11-03: "set in a .pdl history like the other thing, changeable by preference").
 * state/sides_history.pdl: append-only rows `SIDES | <epoch> | pets|palettes|none`, one per change (capped: past 8 KB the last 20 rows are kept; read by identity, last row wins).
 * Start layout, applied by `pal_side <house> restore` when the window opens: state/prefs.txt (set from the editor's Settings tab) `sidebar_start=last|pets|palettes|none`; default `last`; with no history yet, SIDE_FIRST_RUN. */
#define SIDE_FIRST_RUN "palettes"
static void hist_path(const char *house, char *o, size_t n) { snprintf(o, n, "%s/@.apps/rpg-edit-hq/state/sides_history.pdl", house); }
static void hist_add(const char *house, const char *mode) {
    char p[4200]; hist_path(house, p, sizeof p); struct stat st;
    if (stat(p, &st) == 0 && st.st_size > 8192) { static char rows[64][64]; int nr = 0; FILE *r = fopen(p, "r"); if (r) { static char ln[256]; while (fgets(ln, sizeof ln, r)) { snprintf(rows[nr % 64], 64, "%.63s", ln); nr++; } fclose(r); }
        FILE *w = fopen(p, "w"); if (w) { int from = nr > 20 ? nr - 20 : 0; for (int i = from; i < nr; i++) fputs(rows[i % 64], w); fclose(w); } }
    FILE *f = fopen(p, "a"); if (f) { fprintf(f, "SIDES | %ld | %s\n", (long)time(NULL), mode); fclose(f); }
}
static void hist_last(const char *house, char *o, size_t n) {
    char p[4200]; hist_path(house, p, sizeof p); o[0] = 0; FILE *f = fopen(p, "r"); if (!f) return; static char ln[256];
    while (fgets(ln, sizeof ln, f)) { char *b = strrchr(ln, '|'); if (b && !strncmp(ln, "SIDES", 5)) { b++; while (*b == ' ') b++; b[strcspn(b, "\r\n ")] = 0; snprintf(o, n, "%s", b); } }
    fclose(f);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pal_side <house> open|pets|none|tiles <key>|back|list\n"); return 2; }
    const char *house = argv[1], *verb = argv[2], *arg = argc > 3 ? argv[3] : "";
    /* toparm <key> <index>: the top Pals / Palettes row. Same as picking that library from an empty nav, then arming that thumb. */
    int topix = -1;
    if (!strcmp(verb, "hpick") && arg[0]) {
        char palp[4300]; snprintf(palp, sizeof palp, "%s/@.apps/rpg-edit-hq/board/state/pal.txt", house);
        kv_set(palp, "harness_sel", arg);
        printf("HARNESS %s\n", arg);
        /* centroid/clear wipes the open page's stamp. Every other row runs that pal from the harness folder. */
        if (strstr(arg, "/clear")) {
            char ui[4200], bl[80] = "", pl[80] = "", cells[4300];
            snprintf(ui, sizeof ui, "%s/@.apps/rpg-edit-hq/board/state/ui.txt", house);
            kv_get(ui, "book_label", bl, sizeof bl); kv_get(ui, "page_label", pl, sizeof pl);
            char *bc = strchr(bl, ':'); if (bc) memmove(bl, bc + 1, strlen(bc + 1) + 1);
            char *pc = strchr(pl, ':'); if (pc) memmove(pl, pc + 1, strlen(pc + 1) + 1);
            if (bl[0] && pl[0]) {
                snprintf(cells, sizeof cells, "%s/@.apps/centroid/centroid-pchq/pieces/system/maps/%s/%s/cells.txt", house, bl, pl);
                char op[4300]; snprintf(op, sizeof op, "%s/@.apps/centroid/ops/+x/centroid_clear_op.+x", house);
                pid_t c = fork(); if (c == 0) { execl(op, op, cells, (char *)NULL); _exit(127); }
                if (c > 0) { int st; waitpid(c, &st, 0); }
            }
        } else {
            char dir[4300], paln[4300], pr[4200];
            snprintf(dir, sizeof dir, "%s/&.widgits/_shared-lib/harness", house);
            snprintf(paln, sizeof paln, "%s.pal", arg);
            snprintf(pr, sizeof pr, "/tmp/prisc");
            pid_t c = fork();
            if (c == 0) { if (chdir(dir) != 0) _exit(127); execl(pr, pr, paln, (char *)NULL); _exit(127); }
            if (c > 0) { int st; waitpid(c, &st, 0); }
        }
        return 0;
    }
    if (!strcmp(verb, "toparm") && arg[0]) {
        char np0[4300]; snprintf(np0, sizeof np0, "%s/@.apps/rpg-edit-hq/board/state/pal_nav.txt", house);
        kv_set(np0, "kind", ""); kv_set(np0, "level", "0");
        topix = argc > 4 ? atoi(argv[4]) : 0; verb = "pick";
    }
    char sides[4200], pal[4200]; snprintf(sides, sizeof sides, "%s/@.apps/rpg-edit-hq/board/state/sides.txt", house); snprintf(pal, sizeof pal, "%s/@.apps/rpg-edit-hq/board/state/pal.txt", house);
    load_cats(house);
    int log_hist = 1; char title[96] = "";
    if (!strcmp(verb, "restore")) {   /* the window just opened: apply the start layout (preference, else the last one, else the first-run default) without logging a new row */
        char pr[4200], want[32], last[32]; snprintf(pr, sizeof pr, "%s/@.apps/rpg-edit-hq/state/prefs.txt", house); kv_get(pr, "sidebar_start", want, sizeof want); hist_last(house, last, sizeof last);
        char ll[32] = "", lr[32] = "";      /* layout.pdl (default): LAYOUT | left | palettes|pets|none, LAYOUT | right | hai|edit|network|none */
        if (!want[0] || !strcmp(want, "layout")) { char lp[4200], lb[2000] = ""; snprintf(lp, sizeof lp, "%s/@.apps/rpg-edit-hq/layout.pdl", house); FILE *lf = fopen(lp, "r");
            if (lf) { char row[200]; while (fgets(row, sizeof row, lf)) { char a[24], b[24], c[32]; if (row[0] == '#') continue; if (sscanf(row, " LAYOUT | %23[a-z] | %31[a-z]", a, c) == 2) { (void)b; (void)lb; if (!strcmp(a, "left")) snprintf(ll, sizeof ll, "%s", c); else if (!strcmp(a, "right")) snprintf(lr, sizeof lr, "%s", c); } } fclose(lf); } }
        if (ll[0] && strcmp(ll, "palettes") && strcmp(ll, "pets") && strcmp(ll, "none")) ll[0] = 0;
        if (lr[0]) { char cm[4200]; snprintf(cm, sizeof cm, "%s/@.apps/rpg-edit-hq/state/flag_chatmode.txt", house);
            if (!strcmp(lr, "none")) kv_set(sides, "side_right", "");
            else if (!strcmp(lr, "hai") || !strcmp(lr, "edit") || !strcmp(lr, "network")) { kv_set(sides, "side_right", "1"); FILE *cf = fopen(cm, "w"); if (cf) { fprintf(cf, "%s\n", lr); fclose(cf); } } }
        const char *m = ll[0] ? ll : (!strcmp(want, "pets") || !strcmp(want, "palettes") || !strcmp(want, "none")) ? want : (last[0] ? last : SIDE_FIRST_RUN);
        static char mv[16]; snprintf(mv, sizeof mv, "%s", !strcmp(m, "palettes") ? "open" : !strcmp(m, "pets") ? "pets" : !strcmp(m, "none") ? "none" : "open"); verb = mv; log_hist = 0;
    }
    if (!strcmp(verb, "open")) { if (log_hist) hist_add(house, "palettes"); kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", ""); }
    else if (!strcmp(verb, "pets")) { if (log_hist) hist_add(house, "pets"); kv_set(sides, "side_left", "1"); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "none")) { if (log_hist) hist_add(house, "none"); kv_set(sides, "side_left", ""); kv_set(sides, "side_pal", ""); }
    else if (!strcmp(verb, "tiles") || !strcmp(verb, "pick")) { if (log_hist) hist_add(house, "palettes");
        /* pick <key>: at the list = open that palette (at its first level with a choice), at a level = choose it and go down */
        char np[4300]; snprintf(np, sizeof np, "%s/@.apps/rpg-edit-hq/board/state/pal_nav.txt", house); char nk[64], nl[16]; kv_get(np, "kind", nk, sizeof nk); kv_get(np, "level", nl, sizeof nl);
        int lvl = nk[0] && nl[0] ? atoi(nl) : 0;
        g_off = 0;      /* a pick starts at the first page of whatever it opens */
        if (lvl == 0) {
            int at = -1; for (int i = 0; i < nc; i++) if (!strcmp(KEY[i], arg)) at = i;
            if (at < 0) { fprintf(stderr, "pal_side: no such category %s\n", arg); return 1; }
            snprintf(nk, sizeof nk, "%s", KEY[at]); load_ui(house, nk, 0); lvl = descend(0);
        } else {
            char dv[40], tv[40], mv[40]; kind_verbs(nk, dv, tv, mv, sizeof dv);
            const char *vb = lvl == 1 ? dv : lvl == 2 ? tv : mv;
            if (vb[0]) { run_menu(house, vb, arg); load_ui(house, nk, 1); } else load_ui(house, nk, 0);
            lvl = descend(lvl);
        }
        char sl[16]; snprintf(sl, sizeof sl, "%d", lvl); kv_set(np, "kind", nk); kv_set(np, "level", sl); kv_set(np, "page", "0");
        kv_set(sides, "side_pal", "1"); kv_set(sides, "side_left", "");
    } else if (!strcmp(verb, "arm")) {
        int i = atoi(arg); char g[64], sp[1024], k[64], pm[4200];
        char kk[64]; snprintf(kk, sizeof kk, "ptile_%d_glyph", i); kv_get(pal, kk, g, sizeof g); snprintf(kk, sizeof kk, "ptile_%d_sprite", i); kv_get(pal, kk, sp, sizeof sp); kv_get(pal, "pal_key", k, sizeof k);
        if (!g[0] && !sp[0]) { fprintf(stderr, "pal_side: no tile %d\n", i); return 1; }
        char bp[4200], bt[4300]; snprintf(bp, sizeof bp, "%s/@.apps/rpg-edit-hq/board/state/brush.txt", house); snprintf(bt, sizeof bt, "%s.tmp", bp);
        FILE *o = fopen(bt, "w"); if (!o) return 1; fprintf(o, "key=%s\nglyph=%s\nsprite=%s\n", k, g, sp); fclose(o); rename(bt, bp);
        kv_set(pal, "pal_armed", g[0] ? g : "tile");
        {   /* engine side: brush mode + the green placer armed at the selector (the Move placer's shape; Enter then places, Esc leaves) */
            char ui[4200], sess[4200], xp[4300], xb[512] = "", bm[4400], pp[4400]; snprintf(ui, sizeof ui, "%s/@.apps/rpg-edit-hq/board/state/ui.txt", house); kv_get(ui, "bv_session", sess, sizeof sess);
            if (sess[0] && access(sess, F_OK) == 0) {
                int px = 8, py = 8, pz = 0; snprintf(xp, sizeof xp, "%s/@.apps/rpg-edit-hq/rpg-edit-hq-pchq/pieces/xelector_01/state.txt", house);
                char v[32]; kv_get(xp, "pos_x", v, sizeof v); if (v[0]) px = atoi(v); kv_get(xp, "pos_y", v, sizeof v); if (v[0]) py = atoi(v); kv_get(xp, "pos_z", v, sizeof v); if (v[0]) pz = atoi(v); (void)xb;
                snprintf(bm, sizeof bm, "%s/pieces/display/brush_mode.txt", sess); FILE *bf = fopen(bm, "w"); if (bf) { fprintf(bf, "glyph=%s\n", g); fclose(bf); }
                snprintf(pp, sizeof pp, "%s/pieces/display/placer.txt", sess); char nb[16]; snprintf(nb, sizeof nb, "%d", px); kv_set(pp, "x", nb); snprintf(nb, sizeof nb, "%d", py); kv_set(pp, "y", nb); snprintf(nb, sizeof nb, "%d", pz); kv_set(pp, "z", nb); kv_set(pp, "armed", "1");
            } }
        {   /* the palette window's own arm verb for this kind (the same call its tile buttons make): pc/cdda/df/doom <sprite> <glyph>; rmmv <sprite> <tileset> <category> <glyph>. The other kinds have none. */
            const char *av = !strcmp(k, "piececraft") ? "arm-pc" : !strcmp(k, "cdda") ? "arm-cdda" : !strcmp(k, "df") ? "arm-df" : !strcmp(k, "doom") ? "arm-doom" : !strcmp(k, "rmmv") ? "arm-rmmv" : "";
            if (av[0]) { char ui[4200], ats[128] = "", acat[64] = ""; snprintf(pm, sizeof pm, "%s/&.widgits/palettes/palettes_menu.sh", house);
                if (!strcmp(k, "rmmv")) { snprintf(ui, sizeof ui, "%s/&.widgits/palettes/state/palettes-rmmv_ui.txt", house); kv_get(ui, "active_tileset", ats, sizeof ats); kv_get(ui, "active_category", acat, sizeof acat); }
                pid_t c = fork(); if (c == 0) { if (!strcmp(k, "rmmv")) execlp("sh", "sh", pm, av, sp, ats, acat, g, (char *)NULL); else execlp("sh", "sh", pm, av, sp, g, (char *)NULL); _exit(127); }
                if (c > 0) { int st; waitpid(c, &st, 0); } } }
        return 0;
    } else if (strcmp(verb, "back") && strcmp(verb, "list") && strcmp(verb, "next") && strcmp(verb, "prev")) { fprintf(stderr, "pal_side: unknown verb %s\n", verb); return 2; }
    /* the view = the nav state (kind, level): back goes up to the previous level that has a choice, list resets, every other verb (open / pets / none / restore / pick) shows where you are */
    char np[4300]; snprintf(np, sizeof np, "%s/@.apps/rpg-edit-hq/board/state/pal_nav.txt", house); char nk[64] = "", nl[16] = ""; kv_get(np, "kind", nk, sizeof nk); kv_get(np, "level", nl, sizeof nl);
    int lvl = nk[0] && nl[0] ? atoi(nl) : 0; if (lvl < 0 || lvl > 4) lvl = 0;
    if (!strcmp(verb, "list")) lvl = 0;
    int page = 0; { char pg[16]; kv_get(np, "page", pg, sizeof pg); page = atoi(pg); if (page < 0) page = 0; }
    if (lvl > 0) {
        g_off = (strcmp(verb, "pick") && strcmp(verb, "tiles")) ? page * MAXT : 0; load_ui(house, nk, 0);
        if (lvl == 4 && (!strcmp(verb, "next") || !strcmp(verb, "prev"))) {      /* page within bounds, then read that page */
            int np2 = page + (!strcmp(verb, "next") ? 1 : -1); if (np2 >= 0 && np2 * MAXT < ntot) { page = np2; g_off = page * MAXT; load_ui(house, nk, 0); } }
        if (!strcmp(verb, "back")) { lvl = ascend(lvl); page = 0; }
        if (strcmp(verb, "pick") == 0 || strcmp(verb, "tiles") == 0) page = 0;
        { char pg[16]; snprintf(pg, sizeof pg, "%d", page); kv_set(np, "page", pg); }
    } else { RN[0] = RN[1] = RN[2] = nt = ntot = 0; g_off = 0; }
    { char sl[16]; snprintf(sl, sizeof sl, "%d", lvl); kv_set(np, "kind", lvl ? nk : ""); kv_set(np, "level", sl); }
    int kl = -1; for (int i = 0; i < nc; i++) if (!strcmp(KEY[i], nk)) kl = i; const char *kname = kl >= 0 ? LAB[kl] : nk;
    static const char *LV[] = { "palettes", "tile type", "sheet", "tileset", "tiles" };
    snprintf(title, sizeof title, "%s", lvl == 0 ? "palettes" : lvl == 4 ? kname : LV[lvl]);   /* a level shows its own name; the palette name goes on the line below (pal_sub) */
    int nrow = lvl == 0 ? nc : (lvl >= 1 && lvl <= 3) ? RN[lvl - 1] : 0;
    if (lvl == 0) { int s = 0; for (int i = 0; i < nc; i++) if (SIDE[i]) s++; nrow = s; }
    char tmp[4300]; snprintf(tmp, sizeof tmp, "%s.tmp", pal); FILE *o = fopen(tmp, "w"); if (!o) return 1;
    fprintf(o, "n_pcat=%d\npal_is_list=%s\npal_is_tiles=%s\npal_can_back=%s\npal_key=%s\npal_title=%s\npal_sub=%s\npal_can_prev=%s\npal_can_next=%s\n", nrow, lvl != 4 ? "1" : "", lvl == 4 ? "1" : "", lvl > 0 ? "1" : "", nk, title, (lvl >= 1 && lvl <= 3) ? kname : "", (lvl == 4 && g_off > 0) ? "1" : "", (lvl == 4 && g_off + MAXT < ntot) ? "1" : "");
    if (lvl == 0) { int w = 0; for (int i = 0; i < nc; i++) if (SIDE[i]) { fprintf(o, "pcat_%d_key=%s\npcat_%d_label=%s\n", w, KEY[i], w, LAB[i]); w++; } }
    else for (int i = 0; i < nrow; i++) { const char *k2 = RK[lvl - 1][i], *l2 = RL[lvl - 1][i][0] ? RL[lvl - 1][i] : RK[lvl - 1][i]; fprintf(o, "pcat_%d_key=%s\npcat_%d_label=%s\n", i, k2, i, l2); }
    /* Pals tab: the published my-pals thumbs. Palettes tab: the same rows as LyfeDesk's palettes cell (#.desktop/livedesk_taskbar.pdl palettes_menu_N). */
    {
        char sp[4200]; snprintf(sp, sizeof sp, "%s/&.widgits/palettes/palettes-my-pals_state.txt", house);
        FILE *sf = fopen(sp, "r"); int n = 0; char sln[1024];
        if (sf) while (n < 24 && fgets(sln, sizeof sln, sf)) {
            char *t1 = strchr(sln, '\t'); if (!t1) continue; t1++;
            char *t2 = strchr(t1, '\t'); char lab[96];
            size_t L = t2 ? (size_t)(t2 - t1) : strlen(t1); if (L > 90) L = 90; memcpy(lab, t1, L); lab[L] = 0; trim(lab);
            if (!lab[0]) continue;
            fprintf(o, "tbpals_%d_label=%s\n", n, lab); n++;
        }
        if (sf) fclose(sf);
        fprintf(o, "n_tbpals=%d\n", n);
        char pd[4200]; snprintf(pd, sizeof pd, "%s/#.desktop/livedesk_taskbar.pdl", house);
        char plab[24][64]; int have[24]; memset(have, 0, sizeof have); memset(plab, 0, sizeof plab);
        char pkey[24][64]; memset(pkey, 0, sizeof pkey);
        FILE *pf = fopen(pd, "r");
        if (pf) while (fgets(sln, sizeof sln, pf)) {
            char *a = strchr(sln, '|'); if (!a) continue; a++;
            char *b = strchr(a, '|'); if (!b) continue; *b = 0; char *val = b + 1;
            trim(a); trim(val);
            int idx = 0; char suf[16] = "";
            if (sscanf(a, "palettes_menu_%d_%15s", &idx, suf) != 2 || idx < 1 || idx > 24) continue;
            if (!strcmp(suf, "label")) { snprintf(plab[idx - 1], 64, "%s", val); have[idx - 1] = 1; }
            else { const char *c = strrchr(val, ':'); snprintf(pkey[idx - 1], 64, "%s", c ? c + 1 : val); }
        }
        if (pf) fclose(pf);
        n = 0;
        for (int i = 0; i < 24; i++) {
            if (!have[i] || !plab[i][0] || !strcmp(plab[i], "cancel")) continue;
            /* terumon art lives in my-pals now; the taskbar row still names the old library */
            if (!strcmp(plab[i], "terumon")) snprintf(pkey[i], 64, "my-pals");
            fprintf(o, "tbpal_%d_label=%s\ntbpal_%d_key=%s\n", n, plab[i], n, pkey[i]);
            n++;
        }
        fprintf(o, "n_tbpal=%d\n", n);
        /* Harness tab: the pals for the open book. all runs those cases. clear is last. */
        {
            char ui[4200], bl[80] = ""; snprintf(ui, sizeof ui, "%s/@.apps/rpg-edit-hq/board/state/ui.txt", house);
            kv_get(ui, "book_label", bl, sizeof bl);
            char *bc = strchr(bl, ':'); if (bc) memmove(bl, bc + 1, strlen(bc + 1) + 1);
            char hd[4300]; snprintf(hd, sizeof hd, "%s/&.widgits/_shared-lib/harness/%s", house, bl);
            char names[16][64]; int nn = 0;
            DIR *d = bl[0] ? opendir(hd) : NULL;
            if (d) {
                struct dirent *de;
                while ((de = readdir(d)) && nn < 16) {
                    size_t L = strlen(de->d_name);
                    if (L < 5 || strcmp(de->d_name + L - 4, ".pal")) continue;
                    if (!strcmp(de->d_name, "all.pal") || !strcmp(de->d_name, "clear.pal")) continue;
                    snprintf(names[nn], 64, "%s", de->d_name); names[nn][L - 4] = 0; nn++;
                }
                closedir(d);
            }
            for (int a = 0; a + 1 < nn; a++) for (int b = a + 1; b < nn; b++) if (strcmp(names[a], names[b]) > 0) { char t[64]; memcpy(t, names[a], 64); memcpy(names[a], names[b], 64); memcpy(names[b], t, 64); }
            int nhr = 0;
            for (int i = 0; i < nn; i++) { fprintf(o, "tbhar_%d_label=%s/%s\n", nhr, bl, names[i]); nhr++; }
            if (bl[0] && nn) {
                fprintf(o, "tbhar_%d_label=%s/all\n", nhr, bl); nhr++;
                fprintf(o, "tbhar_%d_label=%s/clear\n", nhr, bl); nhr++;
            }
            fprintf(o, "n_tbhar=%d\n", nhr);
        }
    }
    char more[64] = ""; if (lvl == 4 && ntot > MAXT) snprintf(more, sizeof more, "tiles %d-%d of %d", g_off + 1, g_off + nt, ntot);
    if (g_busy) snprintf(more, sizeof more, "machine busy: shown as before");
    fprintf(o, "n_ptile=%d\npal_empty=%s\npal_more=%s\n", lvl == 4 ? nt : 0, (lvl == 4 && nt == 0) ? "1" : "", more);
    if (lvl == 4) for (int i = 0; i < nt; i++) fprintf(o, "ptile_%d_glyph=%s\nptile_%d_sprite=%s\n", i, TG[i], i, TS[i]);
    fclose(o); rename(tmp, pal);
    if (topix >= 0 && topix < nt && (TG[topix][0] || TS[topix][0])) {
        char bp[4200], bt[4300]; snprintf(bp, sizeof bp, "%s/@.apps/rpg-edit-hq/board/state/brush.txt", house); snprintf(bt, sizeof bt, "%s.tmp", bp);
        FILE *bf = fopen(bt, "w"); if (bf) { fprintf(bf, "key=%s\nglyph=%s\nsprite=%s\n", arg, TG[topix], TS[topix]); fclose(bf); rename(bt, bp); }
        kv_set(pal, "pal_armed", TG[topix][0] ? TG[topix] : "tile");
    }
    return 0;
}
