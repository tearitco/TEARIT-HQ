/* pet_player - the META HARNESS: plays the pet like a human, learns how to play it, and leaves an audit trail.
 *
 *   pet_player <steps> [--mode relay|direct] [--pid <khtpm window pid>] [--eps 20] [--wait_ms 1800] [--seed N]
 *
 * Outer loop (this program): observe state/ui.txt -> pick a behavior from player_bank.pdl whose `when` holds (weighted by (r+1)/(r+p+2), with eps% exploration)
 * -> act -> observe again -> utility change -> reward or punish -> rewrite the bank counts -> one ledger row. Inner loop (the pet): its own self-care, preferences and
 * word weights keep learning through pet_event.sh; the harness only presses what a human could press. That is the double recursion: a learner playing a learner.
 * mode relay (default): real input. STATUS: NOT PROVEN since the header tabs and the grouped menu moved the nav numbers (rows start at 11, next to the window chrome 11-13);
 * it needs a published nav map before it is safe (an earlier run minimized the window). mode direct is proven (learning run in the pet-house doc). It appends decimal key codes to #.desktop/entity_menu_history/<pid>.txt (the window's relay): the Menu row is nav 1, Inventory 2, the say field nav 3 (env PET_NAV_SAY),
 * menu row i is nav 4+i (env PET_NAV_BASE moves the base); a number key jumps, Enter fires; chat = jump to say, Enter, the letters, Enter.
 * mode direct: calls pet_event.sh itself (no window needed; for fast training runs in a scratch PET_DIR).
 * Files (state dir = env PET_DIR or <app>/state): player_ledger.txt (STEP rows), the bank is rewritten in place. Utility U = (100-hunger)+energy+clean+happy + 20*level + exp.
 * Build: gcc -std=c11 -O2 -Wall -Wextra -D_DEFAULT_SOURCE -o ops/+x/pet_player.+x ops/pet_player.c */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXB 64
typedef struct { char id[48], when[160], doit[96]; int r, p; } Beh;
static Beh bank[MAXB]; static int nb;
static char bank_path[PATH_MAX], pet[PATH_MAX], app[PATH_MAX], house[PATH_MAX];

static char *kv(const char *file, const char *key, char *out, size_t n) {
    out[0] = 0; FILE *f = fopen(file, "r"); if (!f) return out; char l[1024]; size_t kl = strlen(key);
    while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') { l[strcspn(l, "\r\n")] = 0; snprintf(out, n, "%s", l + kl + 1); break; }
    fclose(f); return out;
}
static double num(const char *key) { char b[64], p[PATH_MAX]; snprintf(p, sizeof p, "%s/ui.txt", pet); kv(p, key, b, sizeof b); return atof(b); }
static double util(void) { return (100 - num("hunger")) + num("energy") + num("clean") + num("happy") + 20 * num("level") + num("exp"); }
static int has_item(const char *it) { char p[PATH_MAX], b[256]; snprintf(p, sizeof p, "%s/ui.txt", pet); kv(p, "pantry", b, sizeof b); char pat[64]; snprintf(pat, sizeof pat, "%s ", it); char *s = strstr(b, pat); return s && atoi(s + strlen(pat)) > 0; }
static int holds(const char *when) {
    char w[160]; snprintf(w, sizeof w, "%s", when); char *save = NULL;
    for (char *t = strtok_r(w, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
        while (*t == ' ') t++;
        if (!strncmp(t, "has:", 4)) { if (!has_item(t + 4)) return 0; continue; }
        char var[32]; int k = 0; while (t[k] && t[k] != '<' && t[k] != '>' && t[k] != '=' && k < 31) { var[k] = t[k]; k++; } var[k] = 0;
        const char *op = t + k; double v = num(var), rhs;
        if (!strncmp(op, ">=", 2)) { rhs = atof(op + 2); if (!(v >= rhs)) return 0; }
        else if (!strncmp(op, "<=", 2)) { rhs = atof(op + 2); if (!(v <= rhs)) return 0; }
        else if (op[0] == '>') { rhs = atof(op + 1); if (!(v > rhs)) return 0; }
        else if (op[0] == '<') { rhs = atof(op + 1); if (!(v < rhs)) return 0; }
    }
    return 1;
}
static void load_bank(void) {
    FILE *f = fopen(bank_path, "r"); if (!f) return; char l[512];
    while (fgets(l, sizeof l, f) && nb < MAXB) {
        if (strncmp(l, "BEHAVIOR", 8)) continue; char *c[7] = { 0 }; int n = 0; char *s = l;
        while (n < 7) { c[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
        if (n < 6) continue;
        for (int i = 0; i < n; i++) { while (*c[i] == ' ') c[i]++; char *e = c[i] + strlen(c[i]); while (e > c[i] && (e[-1] == ' ' || e[-1] == '\n')) *--e = 0; }
        Beh *b = &bank[nb++]; snprintf(b->id, sizeof b->id, "%s", c[1]); snprintf(b->when, sizeof b->when, "%s", c[2]); snprintf(b->doit, sizeof b->doit, "%s", c[3]); b->r = atoi(c[4]); b->p = atoi(c[5]);
    }
    fclose(f);
}
static void save_bank(void) {
    char tmp[PATH_MAX]; snprintf(tmp, sizeof tmp, "%s.tmp", bank_path);
    FILE *in = fopen(bank_path, "r"), *out = fopen(tmp, "w"); if (!in || !out) { if (in) fclose(in); if (out) fclose(out); return; }
    char l[512];
    while (fgets(l, sizeof l, in)) {
        if (!strncmp(l, "BEHAVIOR", 8)) {
            char id[48] = ""; const char *a = strchr(l, '|'); if (a) { a++; while (*a == ' ') a++; int k = 0; while (a[k] && a[k] != ' ' && a[k] != '|' && k < 47) { id[k] = a[k]; k++; } id[k] = 0; }
            int done = 0;
            for (int i = 0; i < nb; i++) if (!strcmp(bank[i].id, id)) { fprintf(out, "BEHAVIOR | %s | %s | %s | %d | %d\n", bank[i].id, bank[i].when, bank[i].doit, bank[i].r, bank[i].p); done = 1; break; }
            if (done) continue;
        }
        fputs(l, out);
    }
    fclose(in); fclose(out); rename(tmp, bank_path);
}
static void key(FILE *rf, int code, int ms) { fprintf(rf, "KEY_PRESSED: %d\n", code); fflush(rf); usleep((useconds_t)ms * 1000); }
static void press_nav(FILE *rf, int nav) { char b[16]; snprintf(b, sizeof b, "%d", nav); for (char *c = b; *c; c++) key(rf, *c, 450); key(rf, 13, 600); }
static int menu_index(const char *verb, const char *arg) {
    char p[PATH_MAX], k[48], v[96]; snprintf(p, sizeof p, "%s/ui.txt", pet); int n = (int)num("n_menu");
    for (int i = 0; i < n; i++) {
        snprintf(k, sizeof k, "menu_%d_verb", i); kv(p, k, v, sizeof v); if (strcmp(v, verb)) continue;
        snprintf(k, sizeof k, "menu_%d_arg", i); kv(p, k, v, sizeof v); if (!strcmp(v, arg)) return i;
    }
    return -1;
}
static int act(const char *doit, int relay, FILE *rf, int nav_base, int wait_ms) {
    char d[96]; snprintf(d, sizeof d, "%s", doit); char cmd[2048];
    if (!strncmp(d, "chat:", 5)) {
        if (!relay) { snprintf(cmd, sizeof cmd, "PET_DIR='%s' sh '%s/ops/pet_event.sh' chat '%s' >/dev/null 2>&1", pet, app, d + 5); return system(cmd); }
        press_nav(rf, getenv("PET_NAV_SAY") ? atoi(getenv("PET_NAV_SAY")) : 3); for (const char *c = d + 5; *c; c++) key(rf, (int)(unsigned char)*c, 350); key(rf, 13, 600); usleep((useconds_t)wait_ms * 1000); return 0;
    }
    if (strncmp(d, "menu:", 5)) return -1;
    char *verb = d + 5, *arg = strchr(verb, ':'); if (!arg) return -1; *arg++ = 0;
    if (!relay) { snprintf(cmd, sizeof cmd, "PET_DIR='%s' sh '%s/ops/pet_event.sh' '%s' '%s' >/dev/null 2>&1", pet, app, verb, arg); return system(cmd); }
    /* the menu is two-level (groups, then a group's rows + Back) and every list is short, so every nav number is ONE digit; a computed nav >= 10 is refused (nav 11-13
     * are the window chrome: minimize / close, learned the hard way). Each stage re-reads ui.txt before pressing the next. */
    int gap = wait_ms > 1500 ? wait_ms : 1500;
    if ((int)num("inv_visible") == 1) { press_nav(rf, 2); usleep((useconds_t)gap * 1000); }          /* inventory rows would shift the menu rows */
    if ((int)num("menu_visible") != 1) { press_nav(rf, 1); usleep((useconds_t)gap * 1000); if ((int)num("menu_visible") != 1) return -1; }
    char tg[32] = "", mp[PATH_MAX]; snprintf(mp, sizeof mp, "%s/menu.pdl", app);
    { FILE *mf = fopen(mp, "r"); if (mf) { char l[512]; while (fgets(l, sizeof l, mf)) { if (strncmp(l, "MENU", 4)) continue; char *c[8] = { 0 }; int n = 0; char *q = l; while (n < 8) { c[n++] = q; char *bb = strchr(q, '|'); if (!bb) break; *bb = 0; q = bb + 1; }
            if (n < 6) continue; for (int k = 0; k < n; k++) { while (*c[k] == ' ') c[k]++; char *e = c[k] + strlen(c[k]); while (e > c[k] && (e[-1] == ' ' || e[-1] == '\n')) *--e = 0; }
            if (!strcmp(c[4], verb) && !strcmp(c[5], strcmp(arg, "") ? arg : "-")) { snprintf(tg, sizeof tg, "%s", c[2]); break; } } fclose(mf); } }
    if (!tg[0]) return -1;
    char pg[PATH_MAX], cur[64]; snprintf(pg, sizeof pg, "%s/ui.txt", pet); kv(pg, "menu_group", cur, sizeof cur);
    if (strcmp(cur, tg)) {
        if (cur[0]) { int bi = menu_index("menu_group", ""); if (bi < 0 || nav_base + bi >= 10) return -1; press_nav(rf, nav_base + bi); usleep((useconds_t)gap * 1000); }
        int gi = menu_index("menu_group", tg); if (gi < 0 || nav_base + gi >= 10) return -1; press_nav(rf, nav_base + gi); usleep((useconds_t)gap * 1000);
    }
    int i = menu_index(verb, arg); if (i < 0 || nav_base + i >= 10) return -1;
    press_nav(rf, nav_base + i); usleep((useconds_t)wait_ms * 1000); return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: pet_player <steps> [--mode relay|direct] [--pid N] [--eps 20] [--wait_ms 1800] [--seed N]\n"); return 2; }
    int steps = atoi(argv[1]), relay = 1, eps = 20, wait_ms = 1800, pid = 0; unsigned seed = (unsigned)time(NULL);
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--mode")) relay = strcmp(argv[i + 1], "direct") != 0; else if (!strcmp(argv[i], "--pid")) pid = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--eps")) eps = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--wait_ms")) wait_ms = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--seed")) seed = (unsigned)atoi(argv[i + 1]);
    }
    srand(seed);
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 1;
    snprintf(app, sizeof app, "%s", self); for (int i = 0; i < 3; i++) { char *s = strrchr(app, '/'); if (s) *s = 0; }
    snprintf(house, sizeof house, "%s", app); for (int i = 0; i < 2; i++) { char *s = strrchr(house, '/'); if (s) *s = 0; }
    const char *pd = getenv("PET_DIR"); if (pd && pd[0]) snprintf(pet, sizeof pet, "%s", pd); else snprintf(pet, sizeof pet, "%s/state", app);
    const char *bp = getenv("PET_BANK"); if (bp && bp[0]) snprintf(bank_path, sizeof bank_path, "%s", bp); else snprintf(bank_path, sizeof bank_path, "%s/player_bank.pdl", app);
    int nav_base = getenv("PET_NAV_BASE") ? atoi(getenv("PET_NAV_BASE")) : 4;   /* Menu=1, Inventory=2, say=3, menu rows from 4 */
    FILE *rf = NULL;
    if (relay) {
        if (!pid) { FILE *p = popen("ps -eo pid,args | awk '/khtpm_core_render.\\+x .*pet-house.xhtpm/ && !/awk/{print $1; exit}'", "r"); if (p) { char b[32]; if (fgets(b, sizeof b, p)) pid = atoi(b); pclose(p); } }
        if (!pid) { fprintf(stderr, "pet_player: no pet-house window (give --pid, or use --mode direct)\n"); return 3; }
        char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/#.desktop/entity_menu_history/%d.txt", house, pid); rf = fopen(rp, "a"); if (!rf) { fprintf(stderr, "pet_player: cannot open %s\n", rp); return 3; }
    }
    char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/player_ledger.txt", pet); FILE *lf = fopen(lp, "a");
    if (!relay) { char sc[2048]; snprintf(sc, sizeof sc, "PET_DIR='%s' sh '%s/ops/pet_event.sh' start >/dev/null 2>&1", pet, app); if (system(sc)) {} }   /* events are inert while the pet is stopped */
    load_bank(); if (!nb) { fprintf(stderr, "pet_player: empty bank %s\n", bank_path); return 2; }
    double first = 0, second = 0; int nfirst = 0, nsecond = 0;
    for (int st = 1; st <= steps; st++) {
        double u0 = util(); int cand[MAXB], nc = 0;
        for (int i = 0; i < nb; i++) if (holds(bank[i].when)) cand[nc++] = i;
        if (!nc) { if (lf) fprintf(lf, "STEP | %d | none valid | 0 | skip\n", st); usleep((useconds_t)wait_ms * 1000); continue; }
        int pick = cand[rand() % nc]; int explore = (rand() % 100) < eps;
        if (!explore) { double best = -1; for (int k = 0; k < nc; k++) { Beh *b = &bank[cand[k]]; double w = (b->r + 1.0) / (b->r + b->p + 2.0) + (rand() % 100) / 10000.0; if (w > best) { best = w; pick = cand[k]; } } }
        Beh *b = &bank[pick];
        int rc = act(b->doit, relay, rf, nav_base, wait_ms);
        if (!relay) usleep(120000);
        double u1 = util(), d = u1 - u0; const char *verdict;
        if (rc != 0) { b->p++; verdict = "failed"; } else if (d >= 1) { b->r++; verdict = "reward"; } else { b->p++; verdict = "punish"; }
        if (st <= steps / 2) { first += d; nfirst++; } else { second += d; nsecond++; }
        if (lf) { fprintf(lf, "STEP | %d | %s%s | dU=%+.0f | %s | w=%.2f\n", st, b->id, explore ? " (explore)" : "", d, verdict, (b->r + 1.0) / (b->r + b->p + 2.0)); fflush(lf); }
        printf("step %2d %-20s%s dU=%+4.0f %s\n", st, b->id, explore ? " *" : "  ", d, verdict);
    }
    save_bank();
    printf("mean dU first half %.2f, second half %.2f (%d / %d steps)\n", nfirst ? first / nfirst : 0, nsecond ? second / nsecond : 0, nfirst, nsecond);
    if (lf) fclose(lf); if (rf) fclose(rf);
    return 0;
}
