/* event_page_op - run an event package's pages the RPG Maker way, WITH variable/switch page conditions.
 *
 * Why it exists (read in play_event.sh, 2026-10-07): the house runner picks pages by `COND | trigger | X` only ("Does NOT yet support
 * switches/variables as conditions"), runs the entity block and then ALL common events again (so `event=common:<name>` through play_event.sh would
 * run every on-click common event twice), and a compiled page has no variable compare (the registry `if` tests a switch only; prisc has beq/bne/addi,
 * no blt). This is the smallest thing that closes those gaps for the digipet; it does not replace play_event.sh.
 *
 * Usage:  event_page_op <pkg_dir> <house_root> [--trigger T] [--state DIR] [--prisc BIN]
 *   Callable as an lc_clock event runner (`LC_CLOCK_EVENT_RUNNER=<this>`; lc_clock calls it `<bin> <pkg> <house>`), so the trigger / state / prisc
 *   also come from env: EVENT_PAGE_TRIGGER (default on-click), EVENT_PAGE_STATE, EVENT_PAGE_PRISC.
 *   Precedence: flag > env > file/default.
 *   state dir  : flag > env > `<pkg>/event_pkg/target.pdl` row `TARGET | state | <path>` (absolute, or relative to the house root) > <pkg> itself.
 *                The pages' event.pal use RELATIVE paths (variables.txt, switches.txt, items.txt, actor_1_stats.txt, actor_states.txt,
 *                needs_tunables.pdl ...) and the pal is run with its cwd = the state dir, so one package can act on any entity.
 *   prisc      : flag > env > <house>/101.mutaclsym* /system/prisc+x
 *   mode       : `<pkg>/event_pkg/mode.pdl` row `MODE | pages | highest` (default; RPG Maker: the highest-numbered page whose conditions hold runs alone)
 *                or `MODE | pages | sequential` (every page in number order whose conditions hold AT THAT MOMENT runs, so a later page sees the
 *                earlier page's changes: the parallel-process form, used by the Day Tick and Feed).
 * Page = <pkg>/event_pkg/pages/page_N/{condition.pdl,event.pal}. condition.pdl rows (all must hold; '#' comments):
 *   COND | trigger | <name>                       must equal the trigger (a page with no trigger row never matches, like play_event.sh)
 *   COND | kv | <file> | <key> | <op> | <rhs>     op: ge gt le lt eq ne;  value read like prisc SYS_GET_KV_INT (`key=int` line, missing = 0), file relative
 *                                                 to the state dir; rhs: an int, or @<file>:<key>[+N|-N] (another kv value, e.g. a tunable)
 * Writes: append-only `<state>/event_page_ledger.txt`  `EVENTPAGE|<pkg name>|<trigger>|page_N|ran|<unix s>` (one row per page run) and appends prisc's
 * output to `<state>/event_page_out.txt`. Exit 0 = at least one page ran and none failed to start, 1 = nothing matched / a page failed, 2 = usage/setup.
 * A page run has a 20 s watchdog (kills it). No shell. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <glob.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define P 4096
static char state[P], prisc[P], trigger[128] = "on-click", pkgname[P];

static int kv_get(const char *file, const char *key, long *out) {
    char path[P + 256]; snprintf(path, sizeof path, "%s/%s", state, file);
    FILE *f = fopen(path, "r"); long v = 0; size_t kl = strlen(key); char ln[1024];
    if (f) { while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { v = atol(ln + kl + 1); break; } fclose(f); }
    *out = v; return 0;
}
static long rhs_val(const char *s) {
    if (s[0] != '@') return atol(s);
    char file[256], key[256]; const char *colon = strchr(s + 1, ':'); long off = 0;
    if (!colon) return 0;
    snprintf(file, sizeof file, "%.*s", (int)(colon - s - 1), s + 1);
    snprintf(key, sizeof key, "%s", colon + 1);
    char *p = key + strcspn(key, "+-");
    if (*p) { off = atol(p); *p = 0; }
    long v; kv_get(file, key, &v); return v + off;
}
static void trim(char *s) { char *a = s; while (*a == ' ' || *a == '\t') a++; memmove(s, a, strlen(a) + 1); size_t n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0; }
/* split a row on '|' into fields (trimmed); returns count */
static int split(char *ln, char **f, int max) { int n = 0; char *p = ln; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) trim(f[i]); return n; }

static int page_ok(const char *cond_path) {
    FILE *f = fopen(cond_path, "r"); if (!f) return 0;
    char ln[1024]; int trig = 0, ok = 1;
    while (ok && fgets(ln, sizeof ln, f)) {
        trim(ln); if (!ln[0] || ln[0] == '#') continue;
        char *fld[8]; int n = split(ln, fld, 8);
        if (n >= 3 && !strcmp(fld[0], "COND") && !strcmp(fld[1], "trigger")) { trig = 1; if (strcmp(fld[2], trigger)) ok = 0; }
        else if (n >= 6 && !strcmp(fld[0], "COND") && !strcmp(fld[1], "kv")) {
            long a; kv_get(fld[2], fld[3], &a); long b = rhs_val(fld[5]); const char *op = fld[4]; int r;
            if (!strcmp(op, "ge")) r = a >= b; else if (!strcmp(op, "gt")) r = a > b; else if (!strcmp(op, "le")) r = a <= b;
            else if (!strcmp(op, "lt")) r = a < b; else if (!strcmp(op, "eq")) r = a == b; else if (!strcmp(op, "ne")) r = a != b; else r = 0;
            if (!r) ok = 0;
        } else if (n >= 2 && !strcmp(fld[0], "COND")) ok = 0; /* an unknown condition kind never matches (fail closed) */
    }
    fclose(f); return ok && trig;
}
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static int run_page(const char *pal) {
    char outp[P + 64]; snprintf(outp, sizeof outp, "%s/event_page_out.txt", state);
    pid_t pid = fork(); if (pid < 0) return 1;
    if (pid == 0) {
        setpgid(0, 0);
        if (chdir(state) != 0) _exit(126);
        int o = open(outp, O_WRONLY | O_CREAT | O_APPEND, 0644), dn = open("/dev/null", O_RDONLY);
        if (dn >= 0) dup2(dn, 0);
        if (o >= 0) { dup2(o, 1); dup2(o, 2); }
        execl(prisc, prisc, pal, (char *)NULL); _exit(127);
    }
    setpgid(pid, pid);
    for (int i = 0; i < 2000; i++) {
        int st; pid_t w = waitpid(pid, &st, WNOHANG);
        if (w == pid) return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : 1;
        if (w < 0) return 1;
        usleep(10000);
    }
    kill(-pid, SIGKILL); kill(pid, SIGKILL); waitpid(pid, NULL, 0); return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: event_page_op <pkg_dir> <house_root> [--trigger T] [--state DIR] [--prisc BIN]\n"); return 2; }
    char pkg_abs[P], house_abs[P], state_abs[P];
    const char *pkg = argv[1], *house = argv[2], *fstate = NULL, *fprisc = NULL;
    if (realpath(argv[1], pkg_abs)) pkg = pkg_abs;      /* pages run with cwd = the state dir, so every path must be absolute */
    if (realpath(argv[2], house_abs)) house = house_abs;
    const char *e;
    if ((e = getenv("EVENT_PAGE_TRIGGER")) && *e) snprintf(trigger, sizeof trigger, "%s", e);
    if ((e = getenv("EVENT_PAGE_STATE")) && *e) fstate = e;
    if ((e = getenv("EVENT_PAGE_PRISC")) && *e) fprisc = e;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--trigger")) snprintf(trigger, sizeof trigger, "%s", argv[i + 1]);
        else if (!strcmp(argv[i], "--state")) fstate = argv[i + 1];
        else if (!strcmp(argv[i], "--prisc")) fprisc = argv[i + 1];
        else { fprintf(stderr, "event_page_op: unknown flag %s\n", argv[i]); return 2; }
    }
    { const char *b = strrchr(pkg, '/'); snprintf(pkgname, sizeof pkgname, "%s", b ? b + 1 : pkg); if (!pkgname[0]) { size_t n = strlen(pkg); while (n > 1 && pkg[n-1] == '/') n--; snprintf(pkgname, sizeof pkgname, "%.*s", (int)n, pkg); } }
    /* state dir */
    snprintf(state, sizeof state, "%s", pkg);
    if (fstate) snprintf(state, sizeof state, "%s", fstate);
    else {
        char tp[P + 64]; snprintf(tp, sizeof tp, "%s/event_pkg/target.pdl", pkg); FILE *f = fopen(tp, "r"); char ln[1024];
        while (f && fgets(ln, sizeof ln, f)) { trim(ln); char *fld[4]; int n = split(ln, fld, 4);
            if (n >= 3 && !strcmp(fld[0], "TARGET") && !strcmp(fld[1], "state")) { if (fld[2][0] == '/') snprintf(state, sizeof state, "%s", fld[2]); else snprintf(state, sizeof state, "%.2000s/%.2000s", house, fld[2]); } }
        if (f) fclose(f);
    }
    if (realpath(state, state_abs)) snprintf(state, sizeof state, "%s", state_abs);
    struct stat sb; if (stat(state, &sb) != 0 || !S_ISDIR(sb.st_mode)) { fprintf(stderr, "event_page_op: state dir missing: %s\n", state); return 2; }
    /* prisc */
    if (fprisc) snprintf(prisc, sizeof prisc, "%s", fprisc);
    else { char pat[P + 64]; snprintf(pat, sizeof pat, "%s/101.mutaclsym*/system/prisc+x", house); glob_t g;
        if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc) snprintf(prisc, sizeof prisc, "%s", g.gl_pathv[0]); else prisc[0] = 0; globfree(&g); }
    if (!prisc[0] || access(prisc, X_OK) != 0) { fprintf(stderr, "event_page_op: no prisc (%s)\n", prisc); return 2; }
    /* mode */
    int sequential = 0;
    { char mp[P + 64]; snprintf(mp, sizeof mp, "%s/event_pkg/mode.pdl", pkg); FILE *f = fopen(mp, "r"); char ln[512];
      while (f && fgets(ln, sizeof ln, f)) { trim(ln); char *fld[4]; int n = split(ln, fld, 4); if (n >= 3 && !strcmp(fld[0], "MODE") && !strcmp(fld[1], "pages")) sequential = !strcmp(fld[2], "sequential"); }
      if (f) fclose(f); }
    /* pages, numeric order */
    char pdir[P + 64]; snprintf(pdir, sizeof pdir, "%s/event_pkg/pages", pkg);
    DIR *d = opendir(pdir); int nums[512], np = 0; struct dirent *de;
    while (d && (de = readdir(d)) && np < 512) { int n; char junk; if (sscanf(de->d_name, "page_%d%c", &n, &junk) == 1) nums[np++] = n; }
    if (d) closedir(d);
    qsort(nums, np, sizeof(int), cmp_int);
    int ran = 0, failed = 0, pick = -1;
    if (!sequential) { /* highest matching page only; decided once, before anything runs */
        for (int i = 0; i < np; i++) { char cp[P + 128]; snprintf(cp, sizeof cp, "%s/page_%d/condition.pdl", pdir, nums[i]); if (page_ok(cp)) pick = nums[i]; }
        if (pick < 0) np = 0;
    }
    for (int i = 0; i < np; i++) {
        if (!sequential && nums[i] != pick) continue;
        char cp[P + 128], pal[P + 128]; snprintf(cp, sizeof cp, "%s/page_%d/condition.pdl", pdir, nums[i]); snprintf(pal, sizeof pal, "%s/page_%d/event.pal", pdir, nums[i]);
        if (sequential && !page_ok(cp)) continue;   /* evaluated NOW, after the earlier pages ran */
        if (access(pal, R_OK)) { failed = 1; continue; }
        if (run_page(pal)) failed = 1;
        ran++;
        char lp[P + 64]; snprintf(lp, sizeof lp, "%s/event_page_ledger.txt", state); FILE *l = fopen(lp, "a");
        if (l) { fprintf(l, "EVENTPAGE|%s|%s|page_%d|ran|%ld\n", pkgname, trigger, nums[i], (long)time(NULL)); fclose(l); }
    }
    if (!ran) { fprintf(stderr, "event_page_op: no page of %s matches trigger '%s'\n", pkgname, trigger); return 1; }
    return failed ? 1 : 0;
}
