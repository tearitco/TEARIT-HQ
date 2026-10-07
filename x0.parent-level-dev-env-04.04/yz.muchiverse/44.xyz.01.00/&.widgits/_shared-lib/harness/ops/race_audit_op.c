/* race_audit_op - the independent REFEREE for the Footrace Fu race harness (footrace_fu_race), and its fairness statistics. Called by a pal after every bot action;
 * it re-derives what the rules must have done from the state, the ledger and the previous snapshot, so a wrong page cannot grade itself.
 *
 * Usage: race_audit_op audit    <state_dir>
 *        race_audit_op fairness <state_dir> <tie_tol_permille> <diff_tol_permille>
 *        race_audit_op exact
 *
 * audit - reads (state_dir): variables.txt, switches.txt, rules.pdl, ninjas.pdl, abilities.pdl, ledger.txt, dice_ledger.txt, rand_ledger.txt; keeps audit_prev.txt.
 *   state      players in range; every slot of a seat that plays and is within ninjas_per_player is alive (HP 1..class HP, position 0..track_len) or dead (HP 0, position
 *              rules.pdl off_track_pos); slots outside the army are empty; army HP at setup <= max_army_hp; alive_cnt / seat_up / seats_up agree with the slots
 *   tokens     0 <= tokens <= max_action_tokens
 *   abilities  per-ninja ability uses <= the ability limit, teleport uses <= teleport_limit_per_ninja, re-roll flag 0/1
 *   finish     no position above track_len; at most one ninja on Finish; the winner / reason / seats agree (1 = Finish, 2 = last army, 3 = both armies gone, 4 = turn cap,
 *              9 = setup refused); a running game has nobody on Finish and 2+ armies
 *   ledgers    dice_counter = number of DICE rows, rand_counter = number of RAND rows (nothing rolled off the books)
 *   new rows   (since the previous call) every ACT/CAST actor was alive at the previous call (dead ninjas never act); every `move` ACT row re-derived: distance = die + class
 *              Move - move_baseline (>= move_min), target = from + distance, and the finish rule (exact / bounce / overshoot wins) applied; every COMBAT row re-derived: the
 *              higher total wins, a tie follows tie_rule, losses are exactly combat_loss (or the HP that was left); both fighters were alive; total HP lost between
 *              the two calls equals the HP lost in the new COMBAT rows (nothing hurts without a combat row; HP never rises)
 *   dead stays dead (alive 1 -> 0 only), and a game that is over no longer changes.
 * Appends to <state_dir>/audit_ledger.txt (append-only, no timestamps):  AUDIT|<turn>|ok|<n checks>   or   AUDIT|<turn>|FAIL|<reason>.  Exit 0 ok | 1 a rule broke | 2 usage.
 *
 * fairness - reads <state_dir>/dice_ledger.txt (rows DICE|seed|counter|d1|d2) and rand_ledger.txt (RAND|seed|counter|sides|value). Treats every DICE row as one
 *   combat roll-off (d1 = attacker die, d2 = defender die, no bonuses) and prints `n=<N> tie=<pm> attacker=<pm> defender=<pm> coin_heads=<pm> d1_min=<pm> d1_max=<pm>`
 *   (per mille). Exit 0 when |tie - 1000/6| <= tie_tol, |attacker - defender| <= diff_tol, the coin (sides 2) share of heads is within 500 +- diff_tol, and every face of die 1
 *   lies within 1000/6 +- diff_tol; else 1.
 * exact - enumerates all 36 pairs of two d6: prints `tie=6 attacker=15 defender=15 of=36` (the probabilities the draft promises: tie 1/6, the two wins equal).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/race_audit_op.+x race_audit_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#define P 4096
#define MAXKV 4000
typedef struct { char k[64]; long v; } KV;
typedef struct { KV e[MAXKV]; int n; } Map;
static char D[P]; static int NCHK;
static Map V, PREV, SW, RULES, NIN, ABI;
static void load(Map *m, const char *file) {
    char p[P + 64], ln[512]; m->n = 0; snprintf(p, sizeof p, "%s/%s", D, file); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f) && m->n < MAXKV) { char *eq = strchr(ln, '='); if (ln[0] == '#' || !eq || strchr(ln, '|')) continue; *eq = 0; snprintf(m->e[m->n].k, sizeof m->e[m->n].k, "%s", ln); m->e[m->n].v = atol(eq + 1); m->n++; }
    fclose(f);
}
static long get(Map *m, const char *k) { for (int i = 0; i < m->n; i++) if (!strcmp(m->e[i].k, k)) return m->e[i].v; return 0; }
static long getf(Map *m, const char *fmt, long a, long b, long c) { char k[96]; snprintf(k, sizeof k, fmt, a, b, c); return get(m, k); }
static void audit_row(const char *kind, long turn, const char *msg) {
    char p[P + 64]; snprintf(p, sizeof p, "%s/audit_ledger.txt", D); FILE *f = fopen(p, "a"); if (!f) return;
    if (!strcmp(kind, "ok")) fprintf(f, "AUDIT|%ld|ok|%s\n", turn, msg); else fprintf(f, "AUDIT|%ld|FAIL|%s\n", turn, msg);
    fclose(f);
}
static int fail(long turn, const char *fmt, long a, long b, long c) { char m[300]; snprintf(m, sizeof m, fmt, a, b, c); audit_row("FAIL", turn, m); fprintf(stderr, "race_audit_op FAIL turn %ld: %s\n", turn, m); return 1; }
#define CHECK(cond, ...) do { NCHK++; if (!(cond)) return fail(get(&V, "turn"), __VA_ARGS__); } while (0)
/* ledger rows: split on '|' into up to 16 fields */
typedef struct { char f[16][24]; int n; } Row;
static Row *L; static int NL;
static void load_ledger(const char *file, Row **out, int *n) {
    char p[P + 64], ln[512]; snprintf(p, sizeof p, "%s/%s", D, file); FILE *f = fopen(p, "r"); *n = 0; *out = calloc(60000, sizeof(Row)); if (!f) return;
    while (fgets(ln, sizeof ln, f) && *n < 60000) { Row *r = &(*out)[(*n)++]; char *q = ln; r->n = 0; ln[strcspn(ln, "\r\n")] = 0;
        while (r->n < 16) { char *b = strchr(q, '|'); if (b) *b = 0; snprintf(r->f[r->n++], 24, "%s", q); if (!b) break; q = b + 1; } }
    fclose(f);
}
static int count_lines(const char *file) { char p[P + 64], ln[512]; int n = 0; snprintf(p, sizeof p, "%s/%s", D, file); FILE *f = fopen(p, "r"); if (!f) return 0; while (fgets(ln, sizeof ln, f)) n++; fclose(f); return n; }
static void save_prev(void) {
    char p[P + 64], ln[512], q[P + 64]; snprintf(p, sizeof p, "%s/variables.txt", D); snprintf(q, sizeof q, "%s/audit_prev.txt", D);
    FILE *f = fopen(p, "r"), *o = fopen(q, "w"); if (!o) return; while (f && fgets(ln, sizeof ln, f)) fputs(ln, o); if (f) fclose(f);
    fprintf(o, "__ledger_lines=%d\n__has=1\n", NL); fclose(o);
}
static int do_audit(void) {
    load(&V, "variables.txt"); load(&SW, "switches.txt"); load(&RULES, "rules.pdl"); load(&NIN, "ninjas.pdl"); load(&ABI, "abilities.pdl");
    load_ledger("ledger.txt", &L, &NL);
    char pp[P + 64]; snprintf(pp, sizeof pp, "%s/audit_prev.txt", D); int has_prev = access(pp, R_OK) == 0; if (has_prev) load(&PREV, "audit_prev.txt");
    long turn = get(&V, "turn"), players = get(&V, "players"), over = get(&SW, "game_over"), reason = get(&V, "game_over_reason");
    long len = get(&RULES, "track_len"), off = get(&RULES, "off_track_pos"), per = get(&RULES, "ninjas_per_player"), maxtok = get(&RULES, "max_action_tokens");
    long prev_lines = has_prev && get(&PREV, "__has") ? get(&PREV, "__ledger_lines") : 0;
    if (has_prev && get(&PREV, "__has") && over && NL == prev_lines && turn == get(&PREV, "turn") && reason == get(&PREV, "game_over_reason") && get(&V, "winner") == get(&PREV, "winner")) return 0;   /* idle after game over: nothing to re-check, no row */
    CHECK(players >= get(&RULES, "players_min") && players <= get(&RULES, "players_max"), "players %ld out of range", players, 0, 0);
    if (over && reason == 9) {                                    /* setup refused: the table must not have started */
        for (int i = 0; i < NL; i++) CHECK(strcmp(L[i].f[0], "ACT") && strcmp(L[i].f[0], "CAST"), "row %ld: an action after a refused setup", (long)i, 0, 0);
        audit_row("ok", turn, "refused"); return 0;
    }
    /* slots */
    long seats_up = 0, atleast_one_on_finish = 0, fin_seat = 0, fin_n = 0;
    for (long p = 1; p <= 4; p++) {
        long cnt = 0, hp0 = 0;
        for (long n = 1; n <= 5; n++) {
            long al = getf(&V, "alive_%ld_%ld", p, n, 0), hp = getf(&V, "hp_%ld_%ld", p, n, 0), pos = getf(&V, "pos_%ld_%ld", p, n, 0), cls = getf(&V, "cls_%ld_%ld", p, n, 0);
            int in = p <= players && n <= per;
            if (!in) { CHECK(al == 0 && hp == 0 && pos == off, "seat %ld slot %ld is outside the army but alive=%ld", p, n, al); continue; }
            CHECK(cls >= 1 && cls <= 5, "seat %ld slot %ld has class %ld", p, n, cls);
            long maxhp = getf(&NIN, "class_%ld_hp", cls, 0, 0); hp0 += maxhp;
            if (al) {
                CHECK(hp >= 1 && hp <= maxhp, "seat %ld ninja %ld HP %ld outside 1..class HP", p, n, hp);
                CHECK(pos >= 0 && pos <= len, "seat %ld ninja %ld position %ld outside the track", p, n, pos);
                cnt++; if (pos == len) { atleast_one_on_finish++; fin_seat = p; fin_n = n; }
            } else CHECK(hp == 0 && pos == off, "dead seat %ld ninja %ld keeps HP %ld or a position", p, n, hp);
            CHECK(getf(&V, "rr_%ld_%ld", p, n, 0) >= 0 && getf(&V, "rr_%ld_%ld", p, n, 0) <= 1, "seat %ld ninja %ld re-roll flag out of 0..1", p, n, 0);
            CHECK(getf(&V, "tp_%ld_%ld", p, n, 0) <= get(&ABI, "teleport_limit_per_ninja"), "seat %ld ninja %ld teleported %ld times, above the limit", p, n, getf(&V, "tp_%ld_%ld", p, n, 0));
            for (long a = 1; a <= 5; a++) {
                long lim = getf(&ABI, "ab_%ld_limit", a, 0, 0), use = getf(&V, "use_%ld_%ld_%ld", a, p, n);
                CHECK(use >= 0 && (lim == 0 || use <= lim), "ability %ld of seat %ld ninja %ld used above its limit", a, p, n);
            }
        }
        if (p <= players) {
            CHECK(hp0 <= get(&RULES, "max_army_hp"), "seat %ld army HP %ld above max_army_hp", p, hp0, 0);
            CHECK(getf(&V, "alive_cnt_%ld", p, 0, 0) == cnt, "seat %ld alive_cnt %ld != live slots", p, getf(&V, "alive_cnt_%ld", p, 0, 0), cnt);
            CHECK(getf(&V, "seat_up_%ld", p, 0, 0) == (cnt > 0), "seat %ld seat_up %ld disagrees with its army", p, getf(&V, "seat_up_%ld", p, 0, 0), 0);
        } else CHECK(getf(&V, "seat_up_%ld", p, 0, 0) == 0, "dormant seat %ld is up", p, 0, 0);
        seats_up += getf(&V, "seat_up_%ld", p, 0, 0);
    }
    CHECK(get(&V, "seats_up") == seats_up, "seats_up %ld != %ld", get(&V, "seats_up"), seats_up, 0);
    CHECK(get(&V, "tokens") >= 0 && get(&V, "tokens") <= maxtok, "tokens %ld outside 0..max_action_tokens", get(&V, "tokens"), 0, 0);
    CHECK(atleast_one_on_finish <= 1, "%ld ninjas on Finish", atleast_one_on_finish, 0, 0);
    if (!over) {
        CHECK(atleast_one_on_finish == 0 && seats_up >= 2, "running game with %ld on Finish and %ld armies", atleast_one_on_finish, seats_up, 0);
        long cur = get(&V, "current"); CHECK(cur >= 1 && cur <= players && getf(&V, "seat_up_%ld", cur, 0, 0), "current seat %ld is not an army still up", cur, 0, 0);
        CHECK(get(&V, "winner") == 0, "winner %ld set on a running game", get(&V, "winner"), 0, 0);
    } else if (reason == 1) {
        CHECK(atleast_one_on_finish == 1 && get(&V, "winner") == fin_seat && get(&V, "winner_n") == fin_n, "Finish reason 1 but winner %ld/%ld != ninja on Finish", get(&V, "winner"), get(&V, "winner_n"), 0);
    } else if (reason == 2) { CHECK(seats_up == 1 && getf(&V, "seat_up_%ld", get(&V, "winner"), 0, 0) == 1, "last-army reason 2 but seats_up=%ld winner=%ld", seats_up, get(&V, "winner"), 0); }
    else if (reason == 3) { CHECK(seats_up == 0 && get(&V, "winner") == 0, "draw reason 3 but seats_up=%ld winner=%ld", seats_up, get(&V, "winner"), 0); }
    else if (reason == 4) { CHECK(get(&V, "winner") == 0 && turn >= get(&RULES, "turn_cap"), "turn-cap reason 4 at turn %ld winner %ld", turn, get(&V, "winner"), 0); }
    else CHECK(0, "game over with unknown reason %ld", reason, 0, 0);
    CHECK(count_lines("dice_ledger.txt") == get(&V, "dice_counter"), "dice_counter %ld != DICE rows %ld", get(&V, "dice_counter"), count_lines("dice_ledger.txt"), 0);
    CHECK(count_lines("rand_ledger.txt") == get(&V, "rand_counter"), "rand_counter %ld != RAND rows %ld", get(&V, "rand_counter"), count_lines("rand_ledger.txt"), 0);
    /* against the previous call */
    long lost_total = 0;
    if (has_prev && get(&PREV, "__has")) {
        for (long p = 1; p <= 4; p++) for (long n = 1; n <= 5; n++) {
            long pa = getf(&PREV, "alive_%ld_%ld", p, n, 0), ph = getf(&PREV, "hp_%ld_%ld", p, n, 0), ha = getf(&V, "hp_%ld_%ld", p, n, 0), al = getf(&V, "alive_%ld_%ld", p, n, 0);
            if (!pa) CHECK(!al, "seat %ld ninja %ld came back from the dead", p, n, 0);
            CHECK(ha <= ph, "seat %ld ninja %ld HP rose to %ld", p, n, ha);
            lost_total += ph - ha;
        }
        CHECK(turn == get(&PREV, "turn") || turn == get(&PREV, "turn") + 1, "turn jumped %ld -> %ld", get(&PREV, "turn"), turn, 0);
        CHECK(get(&V, "dice_counter") >= get(&PREV, "dice_counter"), "dice counter went backwards", 0, 0, 0);
    }
    long row_lost = 0;
    for (int i = (int)prev_lines; i < NL; i++) {
        Row *r = &L[i];
        if (!strcmp(r->f[0], "ACT") || !strcmp(r->f[0], "CAST")) {
            long p = atol(r->f[2]), n = atol(r->f[3]);
            if (has_prev) CHECK(getf(&PREV, "alive_%ld_%ld", p, n, 0) == 1, "row %ld: seat %ld ninja %ld acted but was dead at the previous call", i, p, n);
            if (!strcmp(r->f[0], "ACT") && !strcmp(r->f[4], "move")) {
                long die = atol(r->f[5]), dist = atol(r->f[6]), from = atol(r->f[7]), tgt = atol(r->f[8]), cancel = atol(r->f[9]);
                long cls = getf(&V, "cls_%ld_%ld", p, n, 0), want = die + getf(&NIN, "class_%ld_move", cls, 0, 0) - get(&RULES, "move_baseline");
                if (want < get(&RULES, "move_min")) want = get(&RULES, "move_min");
                CHECK(die >= 1 && die <= 6 && dist == want, "row %ld: move distance %ld != die + class bonus (%ld)", i, dist, want);
                long raw = from + dist, fr = get(&RULES, "finish_rule"), exp = raw;
                if (raw > len) exp = fr == 1 ? from : fr == 2 ? 2 * len - raw : len;
                CHECK(tgt == exp, "row %ld: finish rule gives target %ld not %ld", i, exp, tgt);
                CHECK(cancel == (raw > len && fr == 1), "row %ld: exact-landing cancel flag wrong", (long)i, 0, 0);
            }
        } else if (!strcmp(r->f[0], "COMBAT")) {
            long ap = atol(r->f[2]), an = atol(r->f[3]), dp = atol(r->f[4]), dn = atol(r->f[5]), ar = atol(r->f[7]), dr = atol(r->f[8]), al = atol(r->f[9]), dl = atol(r->f[10]);
            long loss = get(&RULES, "combat_loss"), rule = get(&RULES, "tie_rule");
            if (has_prev) CHECK(getf(&PREV, "alive_%ld_%ld", ap, an, 0) == 1 && getf(&PREV, "alive_%ld_%ld", dp, dn, 0) == 1, "row %ld: a dead ninja fought", (long)i, 0, 0);
            CHECK(ap != dp, "row %ld: a ninja fought its own army", (long)i, 0, 0);
            CHECK(al >= 0 && al <= loss && dl >= 0 && dl <= loss, "row %ld: loss above combat_loss", (long)i, 0, 0);
            if (ar > dr) CHECK(al == 0 && dl >= 1, "row %ld: attacker won but losses are %ld/%ld", i, al, dl);
            else if (ar < dr) CHECK(dl == 0 && al >= 1, "row %ld: defender won but losses are %ld/%ld", i, al, dl);
            else if (rule == 1) CHECK(al >= 1 && dl >= 1, "row %ld: tie with both_lose but losses %ld/%ld", i, al, dl);
            else CHECK((al >= 1) != (dl >= 1), "row %ld: tie with coin must hurt exactly one (%ld/%ld)", i, al, dl);
            row_lost += al + dl;
        }
    }
    if (has_prev && get(&PREV, "__has")) CHECK(lost_total == row_lost, "HP lost %ld but COMBAT rows account for %ld", lost_total, row_lost, 0);
    char m[64]; snprintf(m, sizeof m, "%d", NCHK); audit_row("ok", turn, m);
    save_prev(); return 0;
}
static int do_fairness(long tie_tol, long diff_tol) {
    Row *dr, *rr; int nd, nr; load_ledger("dice_ledger.txt", &dr, &nd); load_ledger("rand_ledger.txt", &rr, &nr);
    long tie = 0, aw = 0, dw = 0, face[7] = {0}, heads = 0, coins = 0;
    for (int i = 0; i < nd; i++) { long a = atol(dr[i].f[3]), b = atol(dr[i].f[4]); if (a < 1 || a > 6 || b < 1 || b > 6) { printf("bad dice row %d\n", i); return 1; } face[a]++; if (a == b) tie++; else if (a > b) aw++; else dw++; }
    for (int i = 0; i < nr; i++) if (atol(rr[i].f[3]) == 2) { coins++; if (atol(rr[i].f[4]) == 1) heads++; }
    if (nd == 0) { printf("no dice rows\n"); return 1; }
    long tiepm = tie * 1000 / nd, apm = aw * 1000 / nd, dpm = dw * 1000 / nd, hpm = coins ? heads * 1000 / coins : 500, fmin = 1000, fmax = 0;
    for (int f = 1; f <= 6; f++) { long x = face[f] * 1000 / nd; if (x < fmin) fmin = x; if (x > fmax) fmax = x; }
    printf("n=%d tie=%ld attacker=%ld defender=%ld coin_heads=%ld d1_min=%ld d1_max=%ld\n", nd, tiepm, apm, dpm, hpm, fmin, fmax);
    long dtie = tiepm - 167; if (dtie < 0) dtie = -dtie; long dd = apm - dpm; if (dd < 0) dd = -dd; long dh = hpm - 500; if (dh < 0) dh = -dh;
    int ok = dtie <= tie_tol && dd <= diff_tol && dh <= diff_tol && fmin >= 167 - diff_tol && fmax <= 167 + diff_tol;
    return ok ? 0 : 1;
}
int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "exact")) {
        int t = 0, a = 0, d = 0; for (int x = 1; x <= 6; x++) for (int y = 1; y <= 6; y++) { if (x == y) t++; else if (x > y) a++; else d++; }
        printf("tie=%d attacker=%d defender=%d of=36\n", t, a, d); return 0;
    }
    if (argc < 3) { fprintf(stderr, "usage: race_audit_op audit <state_dir> | fairness <state_dir> <tie_tol_permille> <diff_tol_permille> | exact\n"); return 2; }
    snprintf(D, sizeof D, "%s", argv[2]); struct stat sb; if (stat(D, &sb) || !S_ISDIR(sb.st_mode)) return 2;
    if (!strcmp(argv[1], "audit") && argc == 3) return do_audit();
    if (!strcmp(argv[1], "fairness") && argc == 5) return do_fairness(atol(argv[3]), atol(argv[4]));
    fprintf(stderr, "usage: race_audit_op audit <state_dir> | fairness <state_dir> <tie_tol_permille> <diff_tol_permille> | exact\n"); return 2;
}
