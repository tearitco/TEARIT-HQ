/* rta_life_op - one living entity, a car, and the ledger rows between them.
 * The ledger is the only state. Rules (numbers) are RULE rows in a rules file:
 *   RULE | hunger_start | N   RULE | eat_amount | N   RULE | fuel_per_cell | N   RULE | drive_speed | 1..4
 * A missing rule refuses (exit 9); no number lives in this C file.
 * Ledger rows (accepted verbs only; a refusal writes nothing):
 *   PART  | car | slot | kind | state | n     slot engine (motor|gas|electric), tank (gas|electric), tires, steering
 *                                             state ok|missing|empty; n = fuel count on the tank row (conserved:
 *                                             only DRIVE lowers it, nothing here ever raises it except an explicit PART row)
 *   ENTER | entity | car      EXIT | entity | car      EAT | entity | hunger_after
 *   DRIVE | entity | car | x | y | fuel_after         (road step by rta_momentum_op, only if in car and parts allow)
 *   SLEEP | entity | n | hunger_after   a skip of n turns (1..sleep_max_turns), refused in the car; hunger rises hunger_per_turn
 *                                             per skipped turn, capped at hunger_max. Writes ONE TURN row with the new cumulative
 *                                             count (turn+n, verb sleep): the skip is in the ledger twice and the two agree
 *   TURN  | n | verb         one manual turn per accepted ENTER/EXIT/EAT/DRIVE (n = cumulative count); no world clock
 *   more RULE rows for sleep: sleep_max_turns, hunger_per_turn, hunger_max (a missing one refuses, exit 9)
 * Usage (rules and ledger are paths):
 *   rta_life_op part  <rules> <ledger> <car> <slot> <kind> <state> <n>
 *   rta_life_op enter|exit <rules> <ledger> <entity> <car>
 *   rta_life_op eat   <rules> <ledger> <entity>
 *   rta_life_op sleep <rules> <ledger> <entity> <turns>
 *   rta_life_op part  <rules> <ledger> <car> <slot> <kind> <state[:n]>   (4-parameter form for the event page, the registry allows 4:
 *                                             "ok:3" = state ok, n 3; "ok" = n 0)
 *   rta_life_op drive <rules> <ledger> <entity> <car> <momentum_bin> <grid> <mom_ledger> <events>
 *   rta_life_op status <rules> <ledger> <entity> <car>
 *   rta_life_op replay <rules> <src_ledger> <dst_ledger> <momentum_bin> <grid> <mom_ledger> <events>
 * Exit: 0 ok, 2 usage (bad slot or part state), 9 missing rule, 10 not in car, 11 part refuses, 12 already in/not in, 13 road stopped,
 *       14 replay diverged, 15 cannot sleep in the car, 16 sleep turns not a whole number in 1..sleep_max_turns.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define SLOTS 4
static const char *SL[SLOTS] = {"engine", "tank", "tires", "steering"};

typedef struct { int in_car, hunger, turn, fuel; char st[SLOTS][16]; } S;

static int rule(const char *path, const char *key, int *out) {
    FILE *f = fopen(path, "r"); char l[256], k[64]; int v;
    if (!f) return -1;
    while (fgets(l, sizeof l, f))
        if (sscanf(l, "RULE | %63[^ |] | %d", k, &v) == 2 && !strcmp(k, key)) { *out = v; fclose(f); return 0; }
    fclose(f); fprintf(stderr, "missing rule %s\n", key); return -1;
}

static void load(const char *rules, const char *led, const char *ent, S *s) {
    memset(s, 0, sizeof *s); s->hunger = -1;
    for (int i = 0; i < SLOTS; i++) strcpy(s->st[i], "missing");
    FILE *f = fopen(led, "r"); char l[512], a[64], b[64], c[64], d[64], e[64]; int n, x, y;
    if (!f) goto defaults;   /* no ledger yet: the start hunger still comes from the rule (was left at -1: the first sleep on a fresh ledger read 5) */
    while (fgets(l, sizeof l, f)) {
        if (sscanf(l, "PART | %63s | %63s | %63s | %63s | %d", a, b, c, d, &n) == 5) {
            for (int i = 0; i < SLOTS; i++) if (!strcmp(b, SL[i])) {
                snprintf(s->st[i], 16, "%.15s", d); if (i == 1) s->fuel = n; }
        }
        else if (sscanf(l, "ENTER | %63s | %63s", a, b) == 2 && !strcmp(a, ent)) s->in_car = 1;
        else if (sscanf(l, "EXIT | %63s | %63s", a, b) == 2 && !strcmp(a, ent)) s->in_car = 0;
        else if (sscanf(l, "EAT | %63s | %d", a, &n) == 2 && !strcmp(a, ent)) s->hunger = n;
        else if (sscanf(l, "DRIVE | %63s | %63s | %d | %d | %d", a, e, &x, &y, &n) == 5 && !strcmp(a, ent)) s->fuel = n;
        else if (sscanf(l, "SLEEP | %63s | %d | %d", a, &x, &n) == 3 && !strcmp(a, ent)) s->hunger = n;
        else if (sscanf(l, "TURN | %d", &n) == 1) s->turn = n;
    }
    fclose(f);
defaults:
    if (s->hunger < 0) { int h = 0; rule(rules, "hunger_start", &h); s->hunger = h; }
}

static int row(const char *led, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static int row(const char *led, const char *fmt, ...) {
    FILE *f = fopen(led, "a");
    if (!f) return -1;
    __builtin_va_list ap; __builtin_va_start(ap, fmt); vfprintf(f, fmt, ap); __builtin_va_end(ap);
    fclose(f); return 0;
}
static void turn(const char *led, int t, const char *verb) { row(led, "TURN | %d | %s\n", t + 1, verb); }

static int mom(char **a, const char *sub, int vx, int *x, int *y, int *ovx) {
    char vs[16]; snprintf(vs, sizeof vs, "%d", vx); int p[2]; if (pipe(p)) return -1;
    pid_t c = fork();
    if (!c) {
        dup2(p[1], 1); close(p[0]); close(p[1]);
        if (!strcmp(sub, "push")) execl(a[0], a[0], "push", a[1], a[2], a[3], vs, (char *)0);
        else execl(a[0], a[0], "step", a[1], a[2], a[3], (char *)0);
        _exit(127);
    }
    close(p[1]); char buf[128] = {0}; ssize_t n = read(p[0], buf, sizeof buf - 1); close(p[0]); (void)n;
    int st; waitpid(c, &st, 0);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0 && sscanf(buf, "x=%d y=%d vx=%d", x, y, ovx) == 3) ? 0 : -1;
}

static int has_mom(const char *p) {
    FILE *f = fopen(p, "r"); char l[256]; int r = 0;
    if (!f) return 0;
    while (fgets(l, sizeof l, f)) if (!strncmp(l, "MOM |", 5)) r = 1;
    fclose(f); return r;
}

static int do_part(const char *led, char **a) {
    int ok = 0;
    for (int i = 0; i < SLOTS; i++) if (!strcmp(a[1], SL[i])) ok = 1;
    if (!ok) return 2;
    if (strcmp(a[3], "ok") && strcmp(a[3], "missing") && strcmp(a[3], "empty")) return 2;   /* a part state is one of three words */
    return row(led, "PART | %s | %s | %s | %s | %d\n", a[0], a[1], a[2], a[3], atoi(a[4])) ? 2 : 0;
}

static int do_move(const char *rules, const char *led, const char *ent, const char *car, int enter) {
    S s; load(rules, led, ent, &s);
    if (enter == s.in_car) { fprintf(stderr, "refused: %s\n", enter ? "already in car" : "not in car"); return 12; }
    row(led, "%s | %s | %s\n", enter ? "ENTER" : "EXIT", ent, car);
    turn(led, s.turn, enter ? "enter" : "exit"); printf("ok\n"); return 0;
}

static int do_eat(const char *rules, const char *led, const char *ent) {
    int amt; if (rule(rules, "eat_amount", &amt)) return 9;
    S s; load(rules, led, ent, &s);
    int h = s.hunger - amt; if (h < 0) h = 0;
    row(led, "EAT | %s | %d\n", ent, h); turn(led, s.turn, "eat"); printf("hunger=%d\n", h); return 0;
}

/* sleep: skip n turns. All rules first (exit 9), then the car (15), then the count (16); a refusal writes nothing. */
static int do_sleep(const char *rules, const char *led, const char *ent, const char *nstr) {
    int mx, hpt, hmax;
    if (rule(rules, "sleep_max_turns", &mx) || rule(rules, "hunger_per_turn", &hpt) || rule(rules, "hunger_max", &hmax)) return 9;
    S s; load(rules, led, ent, &s);
    if (s.in_car) { fprintf(stderr, "refused: cannot sleep in the car\n"); return 15; }
    char *end = 0; long n = strtol(nstr, &end, 10);
    if (!*nstr || *end || n < 1 || n > mx) { fprintf(stderr, "refused: turns must be a whole number 1..%d\n", mx); return 16; }
    long h = s.hunger + n * hpt; if (h > hmax) h = hmax;
    row(led, "SLEEP | %s | %ld | %ld\n", ent, n, h);
    row(led, "TURN | %ld | sleep\n", s.turn + n); printf("slept=%ld hunger=%ld turn=%ld\n", n, h, s.turn + n); return 0;
}

static int do_drive(const char *rules, const char *led, const char *ent, const char *car, char **m) {
    int fpc, spd; if (rule(rules, "fuel_per_cell", &fpc) || rule(rules, "drive_speed", &spd)) return 9;
    S s; load(rules, led, ent, &s);
    if (!s.in_car) { fprintf(stderr, "refused: not in car\n"); return 10; }
    for (int i = 0; i < SLOTS; i++) if (strcmp(s.st[i], "ok")) { fprintf(stderr, "refused: %s %s\n", SL[i], s.st[i]); return 11; }
    if (s.fuel < fpc) { fprintf(stderr, "refused: tank empty\n"); return 11; }
    int x = 0, y = 0, vx = 0, x0 = 0;
    if (!has_mom(m[2]) && mom(m, "push", spd, &x, &y, &vx)) return 13;
    if (!has_mom(m[2])) return 13;
    { FILE *f = fopen(m[2], "r"); char l[256]; int a, b, c; /* x before the step = last MOM row */
      while (f && fgets(l, sizeof l, f)) if (sscanf(l, "MOM | %d | %d | %d", &a, &b, &c) == 3) x0 = a;
      if (f) fclose(f); }
    if (mom(m, "step", spd, &x, &y, &vx)) return 13;
    if (x == x0) { fprintf(stderr, "road stopped\n"); return 13; }
    int fuel = s.fuel - fpc;
    row(led, "DRIVE | %s | %s | %d | %d | %d\n", ent, car, x, y, fuel);
    turn(led, s.turn, "drive"); printf("x=%d y=%d fuel=%d\n", x, y, fuel); return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        printf("rule rta_life inputs rules,ledger outputs PART ENTER EXIT EAT SLEEP DRIVE TURN rows numbers from RULE rows\n"); return 0;
    }
    if (argc < 4) return 2;
    const char *v = argv[1], *rules = argv[2], *led = argv[3];
    if (!strcmp(v, "part") && argc == 9) { char *a[5] = {argv[4], argv[5], argv[6], argv[7], argv[8]}; int r = do_part(led, a); if (!r) printf("ok\n"); return r; }
    if (!strcmp(v, "part") && argc == 8) {   /* page form: state[:n] in one word */
        char st[32], *c; snprintf(st, sizeof st, "%.31s", argv[7]); int n = 0; if ((c = strchr(st, ':'))) { *c = 0; n = atoi(c + 1); }
        char ns[16]; snprintf(ns, sizeof ns, "%d", n); char *a[5] = {argv[4], argv[5], argv[6], st, ns};
        int r = do_part(led, a); if (!r) printf("ok\n"); return r; }
    if (!strcmp(v, "sleep") && argc == 6) return do_sleep(rules, led, argv[4], argv[5]);
    if ((!strcmp(v, "enter") || !strcmp(v, "exit")) && argc == 6) return do_move(rules, led, argv[4], argv[5], !strcmp(v, "enter"));
    if (!strcmp(v, "eat") && argc == 5) return do_eat(rules, led, argv[4]);
    if (!strcmp(v, "drive") && argc == 10) return do_drive(rules, led, argv[4], argv[5], argv + 6);
    if (!strcmp(v, "status") && argc == 6) {
        S s; load(rules, led, argv[4], &s);
        printf("in_car=%d hunger=%d fuel=%d turn=%d\n", s.in_car, s.hunger, s.fuel, s.turn); return 0;
    }
    if (!strcmp(v, "replay") && argc == 9) {
        FILE *f = fopen(led, "r"); char l[512], a[64], b[64], c[64], d[64], e[64]; int n;
        if (!f) return 2;
        while (fgets(l, sizeof l, f)) {
            int r = 0;
            if (sscanf(l, "PART | %63s | %63s | %63s | %63s | %d", a, b, c, d, &n) == 5) { char ns[16]; snprintf(ns, 16, "%d", n); char *p[5] = {a, b, c, d, ns}; r = do_part(argv[4], p); }
            else if (sscanf(l, "ENTER | %63s | %63s", a, b) == 2) r = do_move(rules, argv[4], a, b, 1);
            else if (sscanf(l, "EXIT | %63s | %63s", a, b) == 2) r = do_move(rules, argv[4], a, b, 0);
            else if (sscanf(l, "EAT | %63s | %d", a, &n) == 2) r = do_eat(rules, argv[4], a);
            else if (sscanf(l, "SLEEP | %63s | %d", a, &n) == 2) { char ns[16]; snprintf(ns, 16, "%d", n); r = do_sleep(rules, argv[4], a, ns); }
            else if (sscanf(l, "DRIVE | %63s | %63s | %d", a, e, &n) == 3) r = do_drive(rules, argv[4], a, e, argv + 5);
            if (r) { fprintf(stderr, "replay diverged at: %s", l); return 14; }
        }
        fclose(f); return 0;
    }
    return 2;
}
