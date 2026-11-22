/* mech_step_op - the generic MECHANIC RULE as an op (D92: every rule is an op; numbers are RULE rows). One verb of a generated mechanic (mech_new_op): it costs some resources, gives some, and may be capped.
 * The rows come from <app>/mechanics.pdl:  MECH | <verb> | <arg names, - for none>   COST | <verb> | <resource> | <rule key>   GAIN | <verb> | <resource> | <rule key>   LIMIT | <verb> | <resource> | <rule key>   RULE | <key> | <number>   (start stock = RULE start_<resource>)
 * The LEDGER <app>/state/mech_ledger.txt is the only authority for how much of a resource exists (NO GHOST DATA): rows `EVENT|<n>|player|<verb>|<args>|-|<resource><+|-><n>,...`; the stock is START plus the replay of those deltas, never a stored counter.
 * Usage: mech_step_op [--app <dir>] <verb> [args...]     one step: refused (exit 1, no row) when a cost cannot be paid or a LIMIT would be passed; else one ledger row, prints `<verb>: ok <resource>=<stock> ...`
 *        mech_step_op [--app <dir>] status                `<resource>=<stock>` for every start_ rule, then `EVENTS=<n>`
 *        mech_step_op --describe
 * --app defaults to the folder two levels above this binary (<app>/ops/+x/mech_step.+x, the way playthrough_op compiles a game). Exit 0 ok, 1 refused by the rule, 2 usage / missing RULE / unknown verb / wrong argument count. */
#define _GNU_SOURCE
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <limits.h>
#include "../tool_rules.c"
#define P 4096
#define MAXE 32
typedef struct { char verb[64], res[64], key[64]; } Eff;
static Eff cost[MAXE], gain[MAXE], lim[MAXE]; static int ncost, ngain, nlim;
static char names[MAXE][64]; static long stock[MAXE]; static int nres;
static int res_idx(const char *r) { for (int i = 0; i < nres; i++) if (!strcmp(names[i], r)) return i; if (nres >= MAXE) return -1; snprintf(names[nres], 64, "%s", r); stock[nres] = 0; return nres++; }
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) { puts("mech_step_op: IN [--app dir] verb args | status; app/mechanics.pdl rows MECH COST GAIN LIMIT RULE (RULE start_<resource> = start stock) | OUT `<verb>: ok <resource>=<n>...` + one EVENT row in app/state/mech_ledger.txt (the stock is the replay of the ledger), or `<verb>: refused: ...` exit 1 | RULE keys: every key the COST/GAIN/LIMIT rows name, start_<resource>"); return 0; }
    char app[P] = ""; int a = 1;
    if (argc > 2 && !strcmp(argv[1], "--app")) { snprintf(app, sizeof app, "%s", argv[2]); a = 3; }
    if (a >= argc) { fprintf(stderr, "usage: mech_step_op [--app dir] verb [args...] | status | --describe\n"); return 2; }
    if (!app[0]) { char ex[P]; ssize_t n = readlink("/proc/self/exe", ex, sizeof ex - 1); if (n <= 0) return 2; ex[n] = 0; char d1[P]; snprintf(d1, sizeof d1, "%s", dirname(ex)); char d2[P]; snprintf(d2, sizeof d2, "%s", dirname(d1)); snprintf(app, sizeof app, "%s", dirname(d2)); }
    const char *verb = argv[a]; char mp[P], lp[P]; snprintf(mp, sizeof mp, "%s/mechanics.pdl", app); snprintf(lp, sizeof lp, "%s/state/mech_ledger.txt", app);
    if (tr_load(mp) != 0) { fprintf(stderr, "mech_step_op: refused: no mechanics.pdl in %s\n", app); return 2; }
    char margs[128] = ""; int known = 0; FILE *f = fopen(mp, "r"); char ln[1024];
    while (f && fgets(ln, sizeof ln, f)) { char *c[5]; int k = 0; char *s = ln; while (k < 5) { c[k++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; } for (int i = 0; i < k; i++) c[i] = tr_trim(c[i]); if (k < 3) continue;
        if (!strcmp(c[0], "MECH") && !strcmp(c[1], verb)) { known = 1; snprintf(margs, sizeof margs, "%s", c[2]); }
        else if (k >= 4 && !strcmp(c[1], verb)) { Eff *e = !strcmp(c[0], "COST") ? &cost[ncost] : !strcmp(c[0], "GAIN") ? &gain[ngain] : !strcmp(c[0], "LIMIT") ? &lim[nlim] : NULL;
            if (e && ncost < MAXE && ngain < MAXE && nlim < MAXE) { snprintf(e->verb, 64, "%s", c[1]); snprintf(e->res, 64, "%s", c[2]); snprintf(e->key, 64, "%s", c[3]); if (e == &cost[ncost]) ncost++; else if (e == &gain[ngain]) ngain++; else nlim++; } } }
    if (f) fclose(f);
    int is_status = !strcmp(verb, "status");
    if (!known && !is_status) { fprintf(stderr, "mech_step_op: unknown verb %s\n", verb); return 2; }
    /* stock = start rules + replay of the ledger */
    for (int i = 0; i < tr_n; i++) if (!strncmp(tr_k[i], "start_", 6)) { double v; if (tr_num("mech_step_op", tr_k[i], &v)) return 2; int r = res_idx(tr_k[i] + 6); if (r < 0) return 2; stock[r] = (long)v; }
    long rows = 0; f = fopen(lp, "r");
    while (f && fgets(ln, sizeof ln, f)) { if (strncmp(ln, "EVENT|", 6)) continue; rows++; char *last = strrchr(ln, '|'); if (!last) continue; last++; last[strcspn(last, "\r\n")] = 0;
        for (char *t = strtok(last, ","); t; t = strtok(NULL, ",")) { char *sg = strpbrk(t, "+-"); if (!sg) continue; char sgn = *sg; *sg = 0; int r = res_idx(t); if (r >= 0) stock[r] += (sgn == '+' ? 1 : -1) * atol(sg + 1); } }
    if (f) fclose(f);
    if (is_status) { for (int i = 0; i < tr_n; i++) if (!strncmp(tr_k[i], "start_", 6)) printf("%s=%ld\n", tr_k[i] + 6, stock[res_idx(tr_k[i] + 6)]); printf("EVENTS=%ld\n", rows); return 0; }
    int want = strcmp(margs, "-") ? 0 : 0; if (strcmp(margs, "-")) { for (char *p = margs; *p; p++) if ((p == margs || p[-1] == ' ') && *p != ' ') want++; }
    if (argc - a - 1 != want) { fprintf(stderr, "mech_step_op: %s takes %d argument(s): %s\n", verb, want, margs); return 2; }
    double cv[MAXE], gv[MAXE], lv[MAXE];
    for (int i = 0; i < ncost; i++) if (tr_num("mech_step_op", cost[i].key, &cv[i])) return 2;
    for (int i = 0; i < ngain; i++) if (tr_num("mech_step_op", gain[i].key, &gv[i])) return 2;
    for (int i = 0; i < nlim; i++) if (tr_num("mech_step_op", lim[i].key, &lv[i])) return 2;
    for (int i = 0; i < ncost; i++) { int r = res_idx(cost[i].res); if (r < 0 || stock[r] < (long)cv[i]) { printf("%s: refused: need %ld %s, have %ld\n", verb, (long)cv[i], cost[i].res, r < 0 ? 0L : stock[r]); return 1; } }
    for (int i = 0; i < nlim; i++) for (int g = 0; g < ngain; g++) if (!strcmp(lim[i].res, gain[g].res)) { int r = res_idx(gain[g].res); if (r >= 0 && stock[r] + (long)gv[g] > (long)lv[i]) { printf("%s: refused: %s is full (%ld)\n", verb, gain[g].res, (long)lv[i]); return 1; } }
    char delta[512] = "", shown[512] = ""; for (int i = 0; i < ncost; i++) { int r = res_idx(cost[i].res); stock[r] -= (long)cv[i]; size_t o = strlen(delta); snprintf(delta + o, sizeof delta - o, "%s%s-%ld", o ? "," : "", cost[i].res, (long)cv[i]); }
    for (int i = 0; i < ngain; i++) { int r = res_idx(gain[i].res); stock[r] += (long)gv[i]; size_t o = strlen(delta); snprintf(delta + o, sizeof delta - o, "%s%s+%ld", o ? "," : "", gain[i].res, (long)gv[i]); }
    for (int i = 0; i < ncost; i++) { size_t o = strlen(shown); snprintf(shown + o, sizeof shown - o, " %s=%ld", cost[i].res, stock[res_idx(cost[i].res)]); }
    for (int i = 0; i < ngain; i++) { size_t o = strlen(shown); snprintf(shown + o, sizeof shown - o, " %s=%ld", gain[i].res, stock[res_idx(gain[i].res)]); }
    char sd[P]; snprintf(sd, sizeof sd, "%s/state", app); mkdir(sd, 0755);
    f = fopen(lp, "a"); if (!f) { fprintf(stderr, "mech_step_op: cannot append the ledger\n"); return 2; }
    char as[512] = ""; for (int i = a + 1; i < argc; i++) { size_t o = strlen(as); snprintf(as + o, sizeof as - o, "%s%s", o ? " " : "", argv[i]); }
    fprintf(f, "EVENT|%ld|player|%s|%s|-|%s\n", rows + 1, verb, as[0] ? as : "-", delta); fclose(f);
    printf("%s: ok%s\n", verb, shown); return 0;
}
