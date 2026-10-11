/* dsr_scenario_gen - DSR step 1: turn a scenario.pdl (SETUP/TUNE rows) into a SCRATCH game root shaped like the dsr-test page.
 *
 * Usage:  dsr_scenario_gen <scenario.pdl> <game_root> [--seed N]
 *   Writes (never overwrites: refuses with exit 2 if <game_root>/pals or dsr_world already exists):
 *     <game_root>/pals/dsrtest_<kind>_<civ><n>/{pal.pdl, meta.pdl, desktop_pos.txt, glyph.txt, instance_id.txt, livedesk_index.txt, variables.txt}
 *         same shape as the real dsrtest_* pals (see /home/no/data-backups/dsr-test-copy-report.txt): pal.pdl has NO `PAL | hash` and there is no
 *         entity_uid / phone / runtime file (a copy mints those on first spawn), instance_id is `DT<n>` in creation order. variables.txt is the
 *         RPG Maker "variables" file (`key=int` lines, the house registry convention, ints only, money in whole cents).
 *     <game_root>/sessions/s1/desks/dsr-test.pdl   one DESK row per entity (same columns as the live desk file; the path column is relative to <game_root>)
 *     <game_root>/dsr_world/{variables.txt,tunables.pdl,scenario.pdl,world_ledger.txt}   world state (day=0, seed, sink=0), TUNE rows, a copy of the input,
 *         append-only ledger whose first rows are the SETUP rows then `START|0|total|sink|loans_out|loans_owed`
 *   Kinds built in step 1: castle, bank, store (population is one pool per civ). hotel/school are refused (exit 2) until their step is built.
 *   Layout = the live dsr page: per civ a block 800 px wide (A at x=800, B at x=1600, C/D start a second row at y+400); castle, banks, stores fill
 *   4 columns x 80 px, rows 160 px apart from the block origin (800,80); the population of civ A sits at (+320,+160) like the live page, the others take the next free grid slot (B default = the live (1840,240)). No two entities overlap.
 *   Starting loans: store i borrows loan_principal_c from bank ((i-1) mod banks)+1 if that bank's reserves >= 2x the amount (WSR bank_loan_op.c approval
 *   rule); the cash moves bank -> store at generation time, so money is conserved from day 0.
 *   Exit: 0 ok, 2 usage/refused/bad scenario.
 * No timestamps, no absolute paths are written, so the same scenario + seed gives a byte-identical tree. Self-contained (no shared headers). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

#define P 4096
#define MAXROW 64
static char SK[MAXROW][64], SV[MAXROW][256]; static int NS;       /* SETUP rows in file order */
static char TK[64][48]; static long TV[64]; static int NT;       /* TUNE rows */
static char TLINE[64][160];                                       /* TUNE rows verbatim (comment stripped) for tunables.pdl */

static char *trim(char *s) { char *e; while (isspace((unsigned char)*s)) s++; e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }
static const char *setup_get(const char *k) { for (int i = NS - 1; i >= 0; i--) if (!strcmp(SK[i], k)) return SV[i]; return NULL; }   /* last row wins */
static long tune(const char *k) { for (int i = NT - 1; i >= 0; i--) if (!strcmp(TK[i], k)) return TV[i];   /* last row wins */ fprintf(stderr, "dsr_scenario_gen: missing TUNE %s\n", k); exit(2); }
static void die(const char *m, const char *a) { fprintf(stderr, "dsr_scenario_gen: %s%s%s\n", m, a ? ": " : "", a ? a : ""); exit(2); }

static int mkdirs(const char *path) {
    char t[P]; snprintf(t, sizeof t, "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; }
    return mkdir(t, 0755) == 0 || errno == EEXIST ? 0 : -1;
}
static void put(const char *dir, const char *name, const char *text) {
    char path[P]; snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w"); if (!f) die("cannot write", path);
    fputs(text, f); fclose(f);
}
static void putf(const char *dir, const char *name, const char *fmt, ...) {
    char buf[8192]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap); put(dir, name, buf);
}

/* ledger append helper */
static char LEDGER[P];
static void ledger(const char *fmt, ...) {
    FILE *f = fopen(LEDGER, "a"); if (!f) die("cannot append", LEDGER);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fclose(f);
}

static const char META_HEAD[] =
    "SECTION      | KEY                | VALUE\n----------------------------------------\n";
/* the two METHOD rows every dsrtest_* pal carries, verbatim from the live entities */
static const char META_METHODS[] =
    "METHOD       | Events (hq)          | sh -c 'exec \"$1/&.widgits/events-hq/button.sh\" \"$0\" \"$1\"'\n"
    "METHOD       | Dir                  | sh -c 'exec xdg-open \"$0\"'\n"
    "METHOD       | Close                | CLOSE\n"
    "METHOD       | Cancel               | void\n";

static int nextdt = 1, nextidx = 144;
static char GAME[P];
static char DESK[65536]; static size_t DESKN;
static long total_cash, loans_out, loans_owed, sink;

static void emit(const char *name, const char *glyph, long x, long y, const char *vars) {
    char dir[P]; snprintf(dir, sizeof dir, "%s/pals/%s", GAME, name);
    if (mkdirs(dir)) die("mkdir", dir);
    putf(dir, "pal.pdl", "PAL | name | %s\nPAL | glyph | %s\n", name, glyph);
    putf(dir, "meta.pdl", "%sMETA         | piece_id           | %s\n%s", META_HEAD, name, META_METHODS);
    putf(dir, "desktop_pos.txt", "x=%ld\ny=%ld\nz=0\n", x, y);
    putf(dir, "glyph.txt", "%s", glyph);
    putf(dir, "instance_id.txt", "DT%d\n", nextdt++);
    putf(dir, "livedesk_index.txt", "%d\n", nextidx);
    put(dir, "variables.txt", vars);
    DESKN += (size_t)snprintf(DESK + DESKN, sizeof DESK - DESKN, "DESK | %s | pals/%s | %ld | %ld | %ld | %ld | %s | %d\n", name, name, x, y, x / 80, y / 80, glyph, nextidx);
    nextidx++;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: dsr_scenario_gen <scenario.pdl> <game_root> [--seed N]\n"); return 2; }
    const char *scen = argv[1]; snprintf(GAME, sizeof GAME, "%s", argv[2]);
    long seed_override = -1;
    for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed_override = atol(argv[++i]); else die("bad argument", argv[i]); }

    FILE *f = fopen(scen, "r"); if (!f) die("cannot read scenario", scen);
    char ln[1024];
    while (fgets(ln, sizeof ln, f)) {
        char *h = strchr(ln, '#'); if (h) *h = 0;
        char *s = trim(ln); if (!*s) continue;
        char *a = strtok(s, "|"), *b = strtok(NULL, "|"), *c = strtok(NULL, "");
        if (!a || !b || !c) continue;
        a = trim(a); b = trim(b); c = trim(c);
        if (!strcmp(a, "SETUP") && NS < MAXROW) { snprintf(SK[NS], 64, "%s", b); snprintf(SV[NS], 256, "%s", c); NS++; }
        else if (!strcmp(a, "TUNE") && NT < 64) { snprintf(TK[NT], 48, "%s", b); TV[NT] = atol(c); snprintf(TLINE[NT], 160, "TUNE | %s | %ld", b, TV[NT]); NT++; }
    }
    fclose(f);

    int civs = setup_get("civs") ? atoi(setup_get("civs")) : 2;
    if (civs < 1 || civs > 4) die("civs must be 1..4", NULL);
    long seed = seed_override >= 0 ? seed_override : (setup_get("seed") ? atol(setup_get("seed")) : 1);

    /* validate every civ's setup BEFORE writing anything, so a refusal leaves no half-written game */
    for (int c = 0; c < civs; c++) {
        char key[64], fr[256]; snprintf(key, sizeof key, "civ.%c.free", 'a' + c);
        const char *fs = setup_get(key); snprintf(fr, sizeof fr, "%s", fs ? fs : "castle=1,bank=2,store=4");
        for (char *tok = strtok(fr, ","); tok; tok = strtok(NULL, ",")) {
            char *eq = strchr(tok, '='); if (!eq) die("bad free list", fs);
            *eq = 0; int n = atoi(eq + 1); char *k = trim(tok);
            if (n < 0 || n > 9) die("free count out of range 0..9", k);
            if (strcmp(k, "castle") && strcmp(k, "bank") && strcmp(k, "store")) die("building kind not built in step 1 (castle, bank, store only)", k);
            if (!strcmp(k, "castle") && n > 1) die("at most one castle per civ", NULL);
        }
        snprintf(key, sizeof key, "civ.%c.pop", 'a' + c); if (setup_get(key) && atol(setup_get(key)) < 1) die("civ pop must be >= 1", key);
    }
    char chk[P]; struct stat st;
    snprintf(chk, sizeof chk, "%s/pals", GAME); if (!stat(chk, &st)) die("refusing: game root already has pals", GAME);
    snprintf(chk, sizeof chk, "%s/dsr_world", GAME); if (!stat(chk, &st)) die("refusing: game root already has dsr_world", GAME);
    if (mkdirs(GAME)) die("mkdir", GAME);
    char world[P], desks[P]; snprintf(world, sizeof world, "%s/dsr_world", GAME); snprintf(desks, sizeof desks, "%s/sessions/s1/desks", GAME);
    mkdirs(world); mkdirs(desks);
    char pals[P]; snprintf(pals, sizeof pals, "%s/pals", GAME); mkdirs(pals);
    snprintf(LEDGER, sizeof LEDGER, "%s/world_ledger.txt", world);

    long cost = tune("produce_cost_c"), margin = tune("base_margin_pct");
    long base_ask = (cost * (100 + margin) + 50) / 100;

    for (int c = 0; c < civs; c++) {
        char L = (char)('a' + c); char key[64];
        snprintf(key, sizeof key, "civ.%c.free", L); const char *free_s = setup_get(key); if (!free_s) free_s = "castle=1,bank=2,store=4";
        snprintf(key, sizeof key, "civ.%c.pop", L); long pop = setup_get(key) ? atol(setup_get(key)) : 1000; if (pop < 1) die("civ pop must be >= 1", key);
        int ncastle = 0, nbank = 0, nstore = 0;
        char fr[256]; snprintf(fr, sizeof fr, "%s", free_s);
        for (char *tok = strtok(fr, ","); tok; tok = strtok(NULL, ",")) {
            char *eq = strchr(tok, '='); if (!eq) die("bad free list", free_s);
            *eq = 0; int n = atoi(eq + 1); char *k = trim(tok);
            if (n < 0 || n > 9) die("free count out of range 0..9", k);
            if (!strcmp(k, "castle")) ncastle = n; else if (!strcmp(k, "bank")) nbank = n; else if (!strcmp(k, "store")) nstore = n;
            else die("building kind not built in step 1 (castle, bank, store only)", k);
        }
        if (ncastle > 1) die("at most one castle per civ", NULL);
        long ox = 800 + 800 * (c % 2), oy = 80 + 400 * (c / 2);
        int slot = 0; long bank_res[10] = {0};
        for (int i = 0; i < nbank; i++) bank_res[i] = tune("bank_reserves_c");
        /* castle, banks, stores in grid order */
        char nm[64], vars[2048];
        if (ncastle) {
            snprintf(nm, sizeof nm, "dsrtest_castle_%c", L);
            snprintf(vars, sizeof vars, "treasury=%ld\ntax_rate=%ld\ntax_in=0\n", tune("castle_treasury_c"), tune("tax_rate_bp"));
            emit(nm, "🏰", ox + 80 * (slot % 4), oy + 160 * (slot / 4), vars); slot++;
            total_cash += tune("castle_treasury_c");
        }
        long bank_loans[10] = {0};
        /* bank files are written after the loans are known: remember the slots */
        long bank_pos[10][2];
        for (int i = 0; i < nbank; i++) { bank_pos[i][0] = ox + 80 * (slot % 4); bank_pos[i][1] = oy + 160 * (slot / 4); slot++; }
        /* stores (loans decided here so banks can be written once) */
        struct { long cash, loan; int bank, good; long x, y; } stv[10];
        for (int i = 0; i < nstore; i++) {
            long cash = tune("store_cash_c"), loan = 0; int bank = 0;
            if (nbank > 0) {
                int bi = i % nbank; long p = tune("loan_principal_c");
                if (p > 0 && bank_res[bi] >= 2 * p) { bank_res[bi] -= p; bank_loans[bi] += p; cash += p; loan = p; bank = bi + 1; }
            }
            stv[i].cash = cash; stv[i].loan = loan; stv[i].bank = bank; stv[i].good = (i % 4) + 1;
            stv[i].x = ox + 80 * (slot % 4); stv[i].y = oy + 160 * (slot / 4); slot++;
        }
        for (int i = 0; i < nbank; i++) {
            snprintf(nm, sizeof nm, "dsrtest_bank_%c%d", L, i + 1);
            snprintf(vars, sizeof vars, "reserves=%ld\nloans_out=%ld\nrate=%ld\ninterest_in=0\n", bank_res[i], bank_loans[i], tune("bank_rate_ppm"));
            emit(nm, "🏦", bank_pos[i][0], bank_pos[i][1], vars);
            total_cash += bank_res[i]; loans_out += bank_loans[i];
        }
        for (int i = 0; i < nstore; i++) {
            snprintf(nm, sizeof nm, "dsrtest_store_%c%d", L, i + 1);
            snprintf(vars, sizeof vars,
                "cash=%ld\nprice=%ld\nshare_price=%ld\nshares=%ld\nstock=%ld\nwage_bill=0\nloan_balance=%ld\nloan_bank=%d\ngood_id=%d\n"
                "last_sold=0\nlast_offered=0\noffered_today=0\nsold_today=0\nrev_today=0\nproduced_today=0\ninterest_today=0\ntax_today=0\n",
                stv[i].cash, base_ask, tune("store_share_price_c"), tune("store_shares"), tune("store_stock"), stv[i].loan, stv[i].bank, stv[i].good);
            emit(nm, i == 3 ? "🏫" : "🏪", stv[i].x, stv[i].y, vars);
            total_cash += stv[i].cash; loans_owed += stv[i].loan;
        }
        snprintf(nm, sizeof nm, "dsrtest_population_%c", L);
        long pcash = pop * tune("pop_cash_per_capita_c");
        snprintf(vars, sizeof vars, "count=%ld\ncash=%ld\navg_wage=0\nunemployment=1000\nfood_supply=0\nwages_today=0\n", pop, pcash);
        if (c == 0) emit(nm, "🏨", ox + 320, oy + 160, vars);             /* civ A: the live dsr placement (a 5th column, never used by the grid) */
        else emit(nm, "🏨", ox + 80 * (slot % 4), oy + 160 * (slot / 4), vars);   /* others: the next free grid slot (B default = the live (1840,240)) */
        total_cash += pcash;
    }

    /* world */
    putf(world, "variables.txt", "day=0\nseed=%ld\ncivs=%d\nsink=0\n", seed, civs);
    {   char *buf = malloc(NT * 170 + 64); size_t o = 0; buf[0] = 0;
        for (int i = 0; i < NT; i++) o += (size_t)sprintf(buf + o, "%s\n", TLINE[i]);
        put(world, "tunables.pdl", buf); free(buf); }
    {   FILE *in = fopen(scen, "r"), *out; char path[P]; snprintf(path, sizeof path, "%s/scenario.pdl", world); out = fopen(path, "w");
        if (!in || !out) die("cannot copy scenario", path);
        int ch; while ((ch = fgetc(in)) != EOF) fputc(ch, out); fclose(in); fclose(out); }
    put(world, "world_ledger.txt", "");
    for (int i = 0; i < NS; i++) ledger("SETUP|%s|%s\n", SK[i], SV[i]);
    ledger("SETUP|effective_seed|%ld\n", seed);
    ledger("START|0|%ld|%ld|%ld|%ld\n", total_cash, sink, loans_out, loans_owed);
    put(desks, "dsr-test.pdl", DESK);
    printf("dsr_scenario_gen: %d civ(s), seed %ld, total cash %ld cents\n", civs, seed, total_cash);
    return 0;
}
