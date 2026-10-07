/* ring_audit_op - the independent REFEREE for the ring board game harness (ring_board_core). Called once after every bot turn; it re-derives what the rules
 * must have done from the ledgers and the previous snapshot and compares it with the variables, so a wrong page cannot grade itself.
 *
 * Usage: ring_audit_op <state_dir>
 * Reads (state_dir): variables.txt, switches.txt, board.pdl, rules.pdl, cash_ledger.txt, dice_ledger.txt; keeps audit_prev.txt (the last snapshot + ledger line counts).
 * Checks every call (a FAIL row names the first broken rule, exit 1):
 *   state      players 2..4; every pos_k in 0..board_size-1; no negative cash anywhere (cash only reaches 0 at a bankruptcy exit); dormant seats dead and cash 0;
 *              the current player is alive while the game runs; alive_count and winner/game_over_reason agree (reason 1 = one left, reason 2 = turn cap, winner 0)
 *   money      CASH CONSERVATION: replay cash_ledger.txt from nothing (only `mint` creates money, only reason `setup`, only turn 0): the replayed balances of the bank and
 *              every seat equal the variables, and their sum equals the minted total; row sanity (ok moved = requested, refused moved 0, bankrupt moved < requested)
 *   owners     one owner per tile (a variable), only on property tiles, owner in 1..players and ALIVE (bankrupt players own nothing)
 *   turn step  (against the previous snapshot; the mover = the previous `current`): turn += 1; dice_counter advanced by 0 (jailed skip) or 1 (roll) and the roll equals the
 *              last dice_ledger row; new pos = (old + roll) mod board_size, except Go To Jail -> jail_pos with jail = jail_turns; passing or landing on GO (old + roll >= size)
 *              paid exactly one `go` row of go_pay and otherwise none; landing on an owned property of another pays exactly one `rent` row to the OWNER of the tile's fixed rent;
 *              unowned property: either one `buy` row of the price (owner becomes the mover) or no row; tax tile: one `tax` row of the tax amount; any other tile: no rent/tax/buy row;
 *              a jailed player either pays `jail` (jail_fee, then rolls) or skips (no roll, jail - 1); a `bankrupt` result puts the payer out with cash 0 and returns every tile it owned
 *   game end   once game_over is set the state stops changing (nothing is audited after it, a repeated call is a no-op)
 * Appends to <state_dir>/audit_ledger.txt (append-only, no timestamps):  AUDIT|<turn>|<mover>|ok|<n checks>   or   AUDIT|<turn>|FAIL|<reason>
 * Exit 0 ok / no-op | 1 a rule broke | 2 usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/ring_audit_op.+x ring_audit_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#define P 4096
#define MAXKV 600
typedef struct { char k[48]; long v; } KV;
typedef struct { KV e[MAXKV]; int n; } Map;
static char D[P]; static int NCHK;
static Map V, PREV, SW, RULES;
static void load(Map *m, const char *file) {
    char p[P + 64], ln[512]; m->n = 0; snprintf(p, sizeof p, "%s/%s", D, file); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f) && m->n < MAXKV) { char *eq = strchr(ln, '='); if (ln[0] == '#' || !eq) continue; *eq = 0; snprintf(m->e[m->n].k, sizeof m->e[m->n].k, "%s", ln); m->e[m->n].v = atol(eq + 1); m->n++; }
    fclose(f);
}
static long get(Map *m, const char *k) { for (int i = 0; i < m->n; i++) if (!strcmp(m->e[i].k, k)) return m->e[i].v; return 0; }
static long getk(Map *m, const char *pre, long n) { char k[64]; snprintf(k, sizeof k, "%s%ld", pre, n); return get(m, k); }
static void audit_row(const char *fmt, long turn, const char *who, const char *msg) {
    char p[P + 64]; snprintf(p, sizeof p, "%s/audit_ledger.txt", D); FILE *f = fopen(p, "a"); if (!f) return; fprintf(f, fmt, turn, who, msg); fclose(f);
}
static int fail(long turn, const char *fmt, long a, long b, long c) { char m[300]; snprintf(m, sizeof m, fmt, a, b, c); audit_row("AUDIT|%ld|FAIL|%s%s\n", turn, m, ""); fprintf(stderr, "ring_audit_op FAIL turn %ld: %s\n", turn, m); return 1; }
#define CHECK(cond, ...) do { NCHK++; if (!(cond)) return fail(get(&V, "turn"), __VA_ARGS__); } while (0)
/* board */
typedef struct { char kind[16], group[32]; long price, rent; } Tile;
static Tile B[256]; static int NB;
static void load_board(void) {
    char p[P + 64], ln[512]; snprintf(p, sizeof p, "%s/board.pdl", D); FILE *f = fopen(p, "r"); NB = 0; if (!f) return;
    while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "TILE", 4)) continue; char *fl[8]; int c = 0; char *q = ln; while (c < 8) { fl[c++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; }
        if (c < 7) continue; for (int i = 0; i < c; i++) { while (*fl[i] == ' ') fl[i]++; char *e = fl[i] + strlen(fl[i]); while (e > fl[i] && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; }
        int n = atoi(fl[1]); if (n < 0 || n >= 256) continue; snprintf(B[n].kind, sizeof B[n].kind, "%s", fl[2]); snprintf(B[n].group, sizeof B[n].group, "%s", fl[6]); B[n].price = atol(fl[4]); B[n].rent = atol(fl[5]); if (n + 1 > NB) NB = n + 1; }
    fclose(f);
}
/* ledger rows */
typedef struct { long turn, req, moved; char from[16], to[16], res[16], reason[24]; } Row;
static Row R[20000]; static int NR;
static void load_cash(void) {
    char p[P + 64], ln[512]; snprintf(p, sizeof p, "%s/cash_ledger.txt", D); FILE *f = fopen(p, "r"); NR = 0; if (!f) return;
    while (fgets(ln, sizeof ln, f) && NR < 20000) { char *fl[9]; int c = 0; char *q = ln; while (c < 9) { fl[c++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; }
        if (c < 8 || strcmp(fl[0], "XFER")) continue; char *e = fl[7] + strlen(fl[7]); while (e > fl[7] && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        Row *r = &R[NR++]; r->turn = atol(fl[1]); snprintf(r->from, 16, "%s", fl[2]); snprintf(r->to, 16, "%s", fl[3]); r->req = atol(fl[4]); r->moved = atol(fl[5]); snprintf(r->res, 16, "%s", fl[6]); snprintf(r->reason, 24, "%s", fl[7]); }
    fclose(f);
}
static int count_lines(const char *file) { char p[P + 64], ln[512]; int n = 0; snprintf(p, sizeof p, "%s/%s", D, file); FILE *f = fopen(p, "r"); if (!f) return 0; while (fgets(ln, sizeof ln, f)) n++; fclose(f); return n; }
static int account_of(const char *s) { return !strcmp(s, "bank") ? 0 : !strcmp(s, "mint") ? -1 : atoi(s); }
/* rows in the window [from, NR) with a reason; returns count, sets first index */
static int win(int from, const char *reason, int *first) { int n = 0; for (int i = from; i < NR; i++) if (!strcmp(R[i].reason, reason)) { if (!n) *first = i; n++; } return n; }
static void save_prev(void) {
    char p[P + 64], ln[512], q[P + 64]; snprintf(p, sizeof p, "%s/variables.txt", D); snprintf(q, sizeof q, "%s/audit_prev.txt", D);
    FILE *f = fopen(p, "r"), *o = fopen(q, "w"); if (!o) return; while (f && fgets(ln, sizeof ln, f)) fputs(ln, o); if (f) fclose(f);
    fprintf(o, "__cash_lines=%d\n__dice_lines=%d\n__has=1\n", NR, count_lines("dice_ledger.txt")); fclose(o);
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: ring_audit_op <state_dir>\n"); return 2; }
    snprintf(D, sizeof D, "%s", argv[1]); struct stat sb; if (stat(D, &sb) || !S_ISDIR(sb.st_mode)) return 2;
    load(&V, "variables.txt"); load(&SW, "switches.txt"); load(&RULES, "rules.pdl"); load_board(); load_cash();
    char pp[P + 64]; snprintf(pp, sizeof pp, "%s/audit_prev.txt", D); int has_prev = access(pp, R_OK) == 0; if (has_prev) load(&PREV, "audit_prev.txt");
    long turn = get(&V, "turn"), players = get(&V, "players"), size = get(&RULES, "board_size"), over = get(&SW, "game_over");
    if (has_prev && get(&SW, "game_over") && get(&PREV, "__has") && turn == get(&PREV, "turn") && get(&V, "dice_counter") == get(&PREV, "dice_counter")) return 0;   /* idle after game over */
    CHECK(players >= 2 && players <= 4, "players %ld out of 2..4", players, 0, 0);
    CHECK(size > 0 && NB >= size, "board has %d tiles, rules say %ld", NB, size, 0);
    /* state */
    long alive_n = 0, sum = 0;
    for (long k = 1; k <= 4; k++) {
        long pos = getk(&V, "pos_", k), cash = getk(&V, "cash_", k), al = getk(&V, "alive_", k);
        CHECK(pos >= 0 && pos < size, "seat %ld position %ld outside the board", k, pos, 0);
        CHECK(cash >= 0, "seat %ld has negative cash %ld", k, cash, 0);
        if (k > players) CHECK(al == 0 && cash == 0, "dormant seat %ld alive=%ld cash=%ld", k, al, cash);
        if (al) alive_n++; else CHECK(cash == 0, "dead seat %ld holds cash %ld", k, cash, 0);
        sum += cash;
    }
    CHECK(get(&V, "cash_bank") >= 0, "bank cash negative %ld", get(&V, "cash_bank"), 0, 0);
    sum += get(&V, "cash_bank");
    /* money: replay the ledger */
    long bal[5] = {0, 0, 0, 0, 0}, minted = 0;
    for (int i = 0; i < NR; i++) {
        Row *r = &R[i]; int f = account_of(r->from), t = account_of(r->to);
        CHECK(t >= 0 && t <= 4 && f >= -1 && f <= 4, "ledger row %d has a bad account", i, 0, 0);
        if (!strcmp(r->res, "ok")) CHECK(r->moved == r->req, "row %d ok but moved %ld != requested %ld", i, r->moved, r->req);
        else if (!strcmp(r->res, "refused")) CHECK(r->moved == 0, "row %d refused but moved %ld", i, r->moved, 0);
        else CHECK(!strcmp(r->res, "bankrupt") && r->moved < r->req && f >= 1, "row %d bad bankrupt row", i, 0, 0);
        if (f == -1) { CHECK(!strcmp(r->reason, "setup") && r->turn == 0, "row %d mint outside setup/turn 0", i, 0, 0); minted += r->moved; }
        else bal[f] -= r->moved;
        bal[t] += r->moved;
    }
    CHECK(bal[0] == get(&V, "cash_bank"), "bank replay %ld != variable %ld", bal[0], get(&V, "cash_bank"), 0);
    for (long k = 1; k <= 4; k++) CHECK(bal[k] == getk(&V, "cash_", k), "seat %ld replay %ld != variable %ld", k, bal[k], getk(&V, "cash_", k));
    CHECK(sum == minted, "total cash %ld != minted %ld (money created or lost)", sum, minted, 0);
    /* owners */
    for (int i = 0; i < V.n; i++) if (!strncmp(V.e[i].k, "owner_", 6)) {
        long n = atol(V.e[i].k + 6), o = V.e[i].v;
        CHECK(n >= 0 && n < size && !strcmp(B[n].kind, "prop"), "owner_%ld set on a non-property tile", n, 0, 0);
        CHECK(o == 0 || (o >= 1 && o <= players && getk(&V, "alive_", o)), "tile %ld owned by %ld who is out of the game", n, o, 0);
    }
    CHECK(get(&V, "alive_count") == alive_n || turn == 0, "alive_count %ld != %ld", get(&V, "alive_count"), alive_n, 0);
    if (!over) { long cur = get(&V, "current"); CHECK(cur >= 1 && cur <= players && getk(&V, "alive_", cur), "current player %ld is not an alive seat", cur, 0, 0); }
    else {
        long why = get(&V, "game_over_reason"), w = get(&V, "winner");
        CHECK((why == 1 && alive_n == 1 && w >= 1 && getk(&V, "alive_", w)) || (why == 2 && w == 0 && turn >= get(&RULES, "turn_cap")), "game over reason %ld winner %ld inconsistent (alive %ld)", why, w, alive_n);
    }
    /* turn step */
    char who[16] = "-";
    if (has_prev && get(&PREV, "__has")) {
        long p = get(&PREV, "current"); snprintf(who, sizeof who, "%ld", p);
        int cl = (int)get(&PREV, "__cash_lines"), first = 0; long dctr = get(&PREV, "dice_counter"), dnow = get(&V, "dice_counter");
        long ppos = getk(&PREV, "pos_", p), pjail = getk(&PREV, "jail_", p);
        CHECK(turn == get(&PREV, "turn") + 1, "turn %ld did not advance by one from %ld", turn, get(&PREV, "turn"), 0);
        CHECK(dnow == dctr || dnow == dctr + 1, "dice counter %ld jumped from %ld", dnow, dctr, 0);
        int nrent = win(cl, "rent", &first), nbuy, nbuyf = 0, ntax, ngo, njail, i1;
        int rent_i = first; nbuy = win(cl, "buy", &nbuyf); int buy_i = nbuyf; ntax = win(cl, "tax", &i1); int tax_i = i1; ngo = win(cl, "go", &i1); int go_i = i1; njail = win(cl, "jail", &i1);
        for (int i = cl; i < NR; i++) CHECK(R[i].turn == get(&PREV, "turn"), "ledger row %d belongs to turn %ld, expected %ld", i, R[i].turn, get(&PREV, "turn"));
        if (dnow == dctr) {          /* no roll: a jailed player who did not pay */
            CHECK(pjail > 0 && njail == 0, "no roll but player %ld was not jailed-and-skipping (jail %ld)", p, pjail, 0);
            CHECK(getk(&V, "pos_", p) == ppos && getk(&V, "jail_", p) == pjail - 1, "jailed seat %ld: pos/jail changed wrongly (jail %ld)", p, getk(&V, "jail_", p), 0);
            CHECK(nrent + nbuy + ntax + ngo == 0, "a skipped turn moved money", 0, 0, 0);
        } else {
            long d1 = 0, d2 = 0, dc = -1; { char fp[P + 64], ln[256]; snprintf(fp, sizeof fp, "%s/dice_ledger.txt", D); FILE *f = fopen(fp, "r"); while (f && fgets(ln, sizeof ln, f)) { long s, c, a, b; if (sscanf(ln, "DICE|%ld|%ld|%ld|%ld", &s, &c, &a, &b) == 4) { dc = c; d1 = a; d2 = b; } } if (f) fclose(f); }
            CHECK(dc == dctr, "last dice row counter %ld != %ld", dc, dctr, 0);
            CHECK(d1 >= 1 && d1 <= 6 && d2 >= 1 && d2 <= 6 && get(&V, "roll") == d1 + d2, "dice %ld+%ld do not match roll %ld", d1, d2, get(&V, "roll"));
            CHECK(pjail == 0 ? njail == 0 : (njail == 1 && R[0].turn >= 0), "jail fee rows %ld for jail state %ld", njail, pjail, 0);
            if (pjail > 0) { for (int i = cl; i < NR; i++) if (!strcmp(R[i].reason, "jail")) CHECK(R[i].req == get(&RULES, "jail_fee") && !strcmp(R[i].res, "ok") && account_of(R[i].from) == p && account_of(R[i].to) == 0, "bad jail fee row %d", i, 0, 0); }
            long raw = ppos + d1 + d2, np = raw % size; int wrapped = raw >= size;
            CHECK(ngo == (wrapped ? 1 : 0), "GO rows %ld but wrapped=%ld", ngo, wrapped, 0);
            if (wrapped) CHECK(R[go_i].req == get(&RULES, "go_pay") && account_of(R[go_i].from) == 0 && account_of(R[go_i].to) == p && !strcmp(R[go_i].res, "ok"), "GO payout row wrong (%ld)", R[go_i].req, 0, 0);
            const char *kind = B[np].kind; int owner_prev = (int)getk(&PREV, "owner_", np), bankrupt = 0;
            if (!strcmp(kind, "gotojail")) {
                CHECK(getk(&V, "pos_", p) == get(&RULES, "jail_pos") && getk(&V, "jail_", p) == get(&RULES, "jail_turns"), "go to jail: pos %ld jail %ld wrong", getk(&V, "pos_", p), getk(&V, "jail_", p), 0);
                CHECK(nrent + nbuy + ntax == 0, "go to jail moved money", 0, 0, 0);
            } else {
                CHECK(getk(&V, "pos_", p) == np, "seat %ld pos %ld != (old+roll) mod size %ld", p, getk(&V, "pos_", p), np);
                CHECK(getk(&V, "jail_", p) == 0, "seat %ld jailed after an ordinary move", p, 0, 0);
                if (!strcmp(kind, "prop")) {
                    CHECK(ntax == 0, "tax on a property", 0, 0, 0);
                    if (owner_prev == 0) {
                        CHECK(nrent == 0 && nbuy <= 1, "unowned property %ld: rent rows %ld, buy rows %ld", np, nrent, nbuy);
                        if (nbuy) { CHECK(R[buy_i].req == B[np].price && account_of(R[buy_i].from) == p && account_of(R[buy_i].to) == 0 && !strcmp(R[buy_i].res, "ok"), "buy row wrong for tile %ld", np, 0, 0); CHECK(getk(&V, "owner_", np) == p, "bought tile %ld owner %ld", np, getk(&V, "owner_", np), 0); }
                        else CHECK(getk(&V, "owner_", np) == 0, "tile %ld changed owner without a buy row", np, 0, 0);
                    } else if (owner_prev == p) CHECK(nrent + nbuy == 0, "own property charged", 0, 0, 0);
                    else {
                        CHECK(nrent == 1 && nbuy == 0, "owned by %ld: rent rows %ld buy rows %ld", owner_prev, nrent, nbuy);
                        CHECK(R[rent_i].req == B[np].rent && account_of(R[rent_i].from) == p && account_of(R[rent_i].to) == owner_prev, "rent row wrong (requested %ld, to %ld)", R[rent_i].req, account_of(R[rent_i].to), 0);
                        bankrupt = !strcmp(R[rent_i].res, "bankrupt");
                    }
                } else if (!strcmp(kind, "tax")) {
                    char key[64]; snprintf(key, sizeof key, "tax_%s", B[np].group);
                    CHECK(ntax == 1 && nrent + nbuy == 0, "tax tile %ld: tax rows %ld", np, ntax, 0);
                    CHECK(R[tax_i].req == get(&RULES, key) && account_of(R[tax_i].to) == 0 && account_of(R[tax_i].from) == p, "tax row wrong (%ld)", R[tax_i].req, 0, 0);
                    bankrupt = !strcmp(R[tax_i].res, "bankrupt");
                } else CHECK(nrent + nbuy + ntax == 0, "tile kind %ld moved money", np, 0, 0);
            }
            if (bankrupt) {
                CHECK(getk(&V, "alive_", p) == 0 && getk(&V, "cash_", p) == 0, "bankrupt seat %ld still alive/holding cash", p, 0, 0);
                for (int i = 0; i < V.n; i++) if (!strncmp(V.e[i].k, "owner_", 6)) CHECK(V.e[i].v != p, "bankrupt seat %ld still owns tile ", p, 0, 0);
            } else CHECK(getk(&V, "alive_", p) == 1, "seat %ld died without a bankrupt payment", p, 0, 0);
        }
    }
    save_prev();
    { char m[32]; snprintf(m, sizeof m, "%d", NCHK); audit_row("AUDIT|%ld|%s|ok|%s\n", turn, who, m); }
    return 0;
}
