/* deskopoly - a small property-board rules engine whose WHOLE state is an append-only ledger (pass 1, 2026-11-07; BOOK 08-roadmap/design-docs/HANDOFF-NOV-7 section 10).
 *
 * Usage:  deskopoly <app_dir> <state_dir> <verb> [args]
 *   <app_dir>   the folder with board.pdl, rules.pdl, verbs.pdl (data: tiles, rules, players, the manifest). This op hard-codes no tile and no price.
 *   <state_dir> the game's runtime folder: ledger.txt (the hard state), decisions.txt (DEC rows written by autoplay), mechanics.txt (projection).
 *   verbs: new [seed] [players] | roll | buy | skip | end_turn | autoplay | status | replay | check | mechanics     (the list lives in verbs.pdl)
 *
 * HARD STATE: state is never kept in memory between calls. Every verb REPLAYS ledger.txt (rows `EVENT|round|actor|verb|target|at|result`, the S1 grammar of 16.game/S1-EVENT-LEDGER.md),
 * applies one action, and appends. Dice are a pure function of (seed, roll index), so a roll row stores them in `target` ("d1,d2") and replay needs no random state. Replay re-computes the
 * result of every roll / buy / skip / end_turn and REFUSES a ledger whose recorded result differs (a hand-edited ledger cannot reproduce itself).
 * Money is conserved: sum(player cash) + bank == start_cash * players at all times (the bank is an account that goes negative when it pays Start bonuses); `check` proves it.
 *
 * Exit: 0 ok | 1 replay/invariant failure | 2 usage, bad data, or a refused action (nothing is appended on a refusal).
 * Files written: ledger.txt (<= rounds * players * 4 rows, about 60 KB at the defaults; verbs refuse once the game is over, so it cannot grow past one game), decisions.txt (<= 1 row per
 * decision, same bound), mechanics.txt (overwritten, a few KB). `new` refuses to start over an existing ledger: a new game needs an empty state folder.
 * Build:  gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/deskopoly.+x ops/deskopoly.c      (run through cpu_gate; no scripts)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>

#define MAXT 64
#define MAXP 8
#define MAXP_MINI 4   /* the mini ruleset keeps its pass-1 limit; classic allows 2..8 */
#define LINE 1024     /* one ledger row, at most 1023 bytes (spec 5.2); was 512 */

typedef struct { char name[32], kind[12]; int price, rent, group; int r[6], house; } Tile;   /* r[] and house: classic streets only (rent by buildings, house cost) */
typedef struct { char id[8], name[20], style[20]; int param; int token; } Pl;

static char g_app[1024], g_state[1024];
static char g_rs[32] = "mini";           /* the ruleset: "mini" = schema 1 (a `new` first row); anything else = schema 2 (a `lobby` first row names it) */
static int g_schema = 1;
static uint64_t g_rules_hash;             /* FNV-1a of the ruleset files (board, rules, cards): recorded in `lobby` and `start`, re-checked by every replay (spec 5.2, F7) */
static Tile T[MAXT]; static int nt;
static Pl PL[MAXP]; static int npl;
static int R_start = 1500, R_go = 200, R_rounds = 40, R_maxp = MAXP_MINI; static long R_seed = 7;

/* derived state (replay only) */
static int cash[MAXP], pos[MAXP], out[MAXP], owner[MAXT], human[MAXP];
static char lastlog[5][LINE]; static int nlast;
static int bankv, cur, rolled, pending, round_, over, nrolls, nev, np_game;
static long seed;

static void say(const char *m) { fprintf(stderr, "deskopoly: %s\n", m); }
static void bad(const char *m) { say(m); exit(2); }

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *s, char **f, int max) {
    int n = 0; char *p = s; f[n++] = p;
    for (; *p; p++) if (*p == '|' && n < max) { *p = 0; f[n++] = p + 1; }
    for (int i = 0; i < n; i++) f[i] = trim(f[i]);
    return n;
}

/* ruleset files (board.pdl, rules.pdl): a scratch/legacy app keeps them at its root and wins; otherwise they come from rulesets/mini/ (every schema-1 ledger, which starts with a `new` row, replays under mini; classic arrives in M2). */
static void rs_path(char *p, size_t n, const char *file) {
    struct stat sb;
    if (!strcmp(g_rs, "mini")) { snprintf(p, n, "%s/%s", g_app, file); if (!stat(p, &sb)) return; }
    snprintf(p, n, "%s/rulesets/%s/%s", g_app, g_rs, file);
}
/* a ruleset name is a folder name under rulesets/: lower-case letters, digits, '_' and '-' only (no slash, no "..") */
static int rs_name_ok(const char *s) { size_t n = strlen(s); if (n < 1 || n > 24) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }
static uint64_t fnv_feed(uint64_t h, const char *s, size_t n) { for (size_t i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 1099511628211ULL; } return h; }
/* the rules hash: the bytes of board.pdl, rules.pdl and cards.pdl (when present), each preceded by its file name, so editing any number or name of the ruleset changes it */
static uint64_t rules_hash(void) {
    static const char *names[3] = { "board.pdl", "rules.pdl", "cards.pdl" }; uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < 3; i++) {
        char p[1100], b[8192]; rs_path(p, sizeof p, names[i]); FILE *fp = fopen(p, "rb"); if (!fp) { if (i == 2) continue; bad("cannot read a ruleset file for the rules hash"); }
        h = fnv_feed(h, names[i], strlen(names[i])); size_t k; while ((k = fread(b, 1, sizeof b, fp)) > 0) h = fnv_feed(h, b, k); fclose(fp);
    }
    return h;
}

/* classic board.pdl: TILE | idx | id | kind | group | price | r0 | r1 | r2 | r3 | r4 | r5 | house_cost   (spec 4.1). Refuses everything the spec says the loader must refuse. */
static void load_classic_board(FILE *fp) {
    char ln[LINE]; char *f[16]; int hc[10]; for (int i = 0; i < 10; i++) hc[i] = -1;
    static const char *kinds[] = { "go", "street", "chance", "chest", "tax", "rail", "util", "jail", "free", "gotojail" };
    nt = 0;
    while (fgets(ln, sizeof ln, fp)) {
        if (ln[0] == '#') continue; int n = split(ln, f, 16); if (n < 1 || strcmp(f[0], "TILE")) continue;
        if (n != 13) bad("board.pdl: a classic TILE row has 13 fields (TILE | idx | id | kind | group | price | r0..r5 | house_cost)");
        if (nt >= MAXT || atoi(f[1]) != nt) bad("board.pdl: TILE indexes must run 0..n-1 in order");
        if (strlen(f[2]) < 1 || strlen(f[2]) > 16) bad("board.pdl: a tile id is 1 to 16 characters");
        int ok = 0; for (size_t k = 0; k < sizeof kinds / sizeof *kinds; k++) if (!strcmp(f[3], kinds[k])) ok = 1; if (!ok) bad("board.pdl: unknown tile kind");
        Tile *t = &T[nt]; memset(t, 0, sizeof *t); snprintf(t->name, sizeof t->name, "%s", f[2]); snprintf(t->kind, sizeof t->kind, "%s", f[3]);
        t->group = atoi(f[4]); t->price = atoi(f[5]); for (int i = 0; i < 6; i++) t->r[i] = atoi(f[6 + i]); t->house = atoi(f[12]); t->rent = t->r[0];
        for (int i = 0; i < nt; i++) if (!strcmp(T[i].name, t->name)) bad("board.pdl: a tile id is used twice");
        if (!strcmp(t->kind, "street")) {
            if (t->price <= 0 || t->group < 0 || t->group > 7 || t->house <= 0) bad("board.pdl: a street needs a price, a group 0..7 and a house cost");
            for (int i = 0; i < 6; i++) if (t->r[i] <= 0 || (i && t->r[i] <= t->r[i - 1])) bad("board.pdl: a street needs six rents, each above the one before");
            if (hc[t->group] >= 0 && hc[t->group] != t->house) bad("board.pdl: one colour group has two house costs");
            hc[t->group] = t->house;
        } else if (!strcmp(t->kind, "rail") || !strcmp(t->kind, "util")) {
            int want = !strcmp(t->kind, "rail") ? 8 : 9; if (t->price <= 0 || t->group != want) bad("board.pdl: a rail needs group 8 and a util group 9, each with a price");
        } else if (!strcmp(t->kind, "tax")) { if (t->price <= 0) bad("board.pdl: a tax tile needs an amount"); }
        nt++;
    }
    if (nt < 4 || nt > 64) bad("board.pdl: a board has 4 to 64 tiles");
    if (strcmp(T[0].kind, "go")) bad("board.pdl: tile 0 must be kind go");
    int jails = 0; for (int i = 0; i < nt; i++) if (!strcmp(T[i].kind, "jail")) jails++;
    if (jails != 1) bad("board.pdl: a board needs exactly one jail tile");
}

static void load_app(void) {
    char p[1100], ln[LINE]; char *f[10]; FILE *fp;
    if (strcmp(g_rs, "mini")) {   /* schema 2: a classic-format ruleset; the roster is NOT config (it comes from `join` rows, finding F6) */
        rs_path(p, sizeof p, "board.pdl"); fp = fopen(p, "r"); if (!fp) bad("cannot read board.pdl for this ruleset"); load_classic_board(fp); fclose(fp);
        rs_path(p, sizeof p, "rules.pdl"); fp = fopen(p, "r"); if (!fp) bad("cannot read rules.pdl for this ruleset");
        R_rounds = 400; R_maxp = 8;
        while (fgets(ln, sizeof ln, fp)) {
            if (ln[0] == '#') continue; int n = split(ln, f, 10); if (n < 3) continue;
            if (!strcmp(f[0], "PLAYER")) bad("rules.pdl: a classic ruleset has no PLAYER rows (players join)");
            if (strcmp(f[0], "RULE")) continue;
            if (!strcmp(f[1], "start_cash")) R_start = atoi(f[2]); else if (!strcmp(f[1], "pass_go")) R_go = atoi(f[2]);
            else if (!strcmp(f[1], "safety_rounds")) R_rounds = atoi(f[2]); else if (!strcmp(f[1], "seed")) R_seed = atol(f[2]);
            else if (!strcmp(f[1], "max_players")) R_maxp = atoi(f[2]);
        }
        fclose(fp);
        if (R_start <= 0 || R_go < 0 || R_rounds <= 0) bad("rules.pdl: start_cash and safety_rounds must be positive");
        if (R_maxp < 2 || R_maxp > MAXP) bad("rules.pdl: max_players is 2..8");
        npl = 0; g_rules_hash = rules_hash(); return;
    }
    rs_path(p, sizeof p, "board.pdl"); fp = fopen(p, "r"); if (!fp) bad("cannot read board.pdl");
    nt = 0;
    while (fgets(ln, sizeof ln, fp)) {
        if (ln[0] == '#') continue; int n = split(ln, f, 10); if (n < 7 || strcmp(f[0], "TILE")) continue;
        if (nt >= MAXT || atoi(f[1]) != nt) bad("board.pdl: TILE indexes must run 0..n-1 in order");
        Tile *t = &T[nt++]; snprintf(t->name, sizeof t->name, "%s", f[2]); snprintf(t->kind, sizeof t->kind, "%s", f[3]);
        t->price = atoi(f[4]); t->rent = atoi(f[5]); t->group = atoi(f[6]);
        if (strcmp(t->kind, "go") && strcmp(t->kind, "prop") && strcmp(t->kind, "tax") && strcmp(t->kind, "rest")) bad("board.pdl: unknown tile kind");
        if (!strcmp(t->kind, "prop") && (t->price <= 0 || t->rent < 0 || t->group < 0)) bad("board.pdl: a prop needs a price, a rent and a group");
    }
    fclose(fp);
    if (nt < 4 || strcmp(T[0].kind, "go")) bad("board.pdl: need at least 4 tiles and tile 0 must be kind go");
    rs_path(p, sizeof p, "rules.pdl"); fp = fopen(p, "r"); if (!fp) bad("cannot read rules.pdl");
    npl = 0;
    while (fgets(ln, sizeof ln, fp)) {
        if (ln[0] == '#') continue; int n = split(ln, f, 10);
        if (n >= 3 && !strcmp(f[0], "RULE")) {
            if (!strcmp(f[1], "start_cash")) R_start = atoi(f[2]); else if (!strcmp(f[1], "pass_go")) R_go = atoi(f[2]);
            else if (!strcmp(f[1], "rounds")) R_rounds = atoi(f[2]); else if (!strcmp(f[1], "seed")) R_seed = atol(f[2]);
        } else if (n >= 5 && !strcmp(f[0], "PLAYER")) {
            if (npl >= MAXP_MINI) bad("rules.pdl: at most 4 players in the mini ruleset");
            Pl *q = &PL[npl++]; snprintf(q->id, sizeof q->id, "%s", f[1]); snprintf(q->name, sizeof q->name, "%s", f[2]); snprintf(q->style, sizeof q->style, "%s", f[3]); q->param = atoi(f[4]);
            if (strcmp(q->style, "bold") && strcmp(q->style, "cautious") && strcmp(q->style, "coin")) bad("rules.pdl: unknown player style");
        }
    }
    fclose(fp);
    if (npl < 2) bad("rules.pdl: need at least 2 players");
    if (R_start <= 0 || R_rounds <= 0) bad("rules.pdl: start_cash and rounds must be positive");
}

static uint64_t mix(uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
static int die(long sd, int idx, int k) { return (int)(mix((uint64_t)sd * 1000003ULL + (uint64_t)idx * 16 + (uint64_t)k) % 6) + 1; }

static int pidx(const char *id) { for (int i = 0; i < npl; i++) if (!strcmp(PL[i].id, id)) return i; return -1; }

static void reset_state(void) {
    for (int i = 0; i < MAXP; i++) { cash[i] = R_start; pos[i] = 0; out[i] = 0; human[i] = 0; } nlast = 0;
    for (int i = 0; i < MAXT; i++) owner[i] = -1;
    bankv = 0; cur = 0; rolled = 0; pending = -1; round_ = 0; over = 0; nrolls = 0; nev = 0; np_game = npl;
}
static int active_count(void) { int c = 0; for (int i = 0; i < np_game; i++) if (!out[i]) c++; return c; }
static int holds_group(int pl, int g) { int any = 0; for (int i = 0; i < nt; i++) if (!strcmp(T[i].kind, "prop") && T[i].group == g) { any = 1; if (owner[i] != pl) return 0; } return any; }
static void release(int pl) { for (int i = 0; i < nt; i++) if (owner[i] == pl) owner[i] = -1; }

/* pay `amount` from pl to creditor (-1 = bank). A short payer hands over what it has and is out of the game; its tiles go back to the bank. Returns the amount actually paid. */
static int pay(int pl, int amount, int creditor, int *bankrupt) {
    int paid = amount; *bankrupt = 0;
    if (cash[pl] < amount) { paid = cash[pl]; *bankrupt = 1; }
    cash[pl] -= paid; if (creditor < 0) bankv += paid; else cash[creditor] += paid;
    if (*bankrupt) { out[pl] = 1; release(pl); }
    return paid;
}

static void ap(char *res, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void ap(char *res, size_t cap, const char *fmt, ...) {   /* LOUD on overflow (finding F11): a silently cut status could hide the very row that shows a problem */
    size_t l = strlen(res); va_list a; va_start(a, fmt); int w = vsnprintf(res + l, cap - l, fmt, a); va_end(a);
    if (w < 0 || (size_t)w >= cap - l) { fprintf(stderr, "deskopoly: output buffer overflow (%zu bytes): refusing to print a cut result\n", cap); exit(1); }
}

static void do_roll(int pl, int d1, int d2, char *res, size_t cap) {
    int old = pos[pl], step = d1 + d2, np = (old + step) % nt; res[0] = 0; pos[pl] = np; pending = -1;
    if (old + step >= nt) { cash[pl] += R_go; bankv -= R_go; ap(res, cap, "go+%d ", R_go); }
    Tile *t = &T[np]; int bk;
    if (!strcmp(t->kind, "tax")) { int p = pay(pl, t->price, -1, &bk); ap(res, cap, "tax-%d%s", p, bk ? " bankrupt" : ""); }
    else if (!strcmp(t->kind, "rest")) ap(res, cap, "rest");
    else if (!strcmp(t->kind, "go")) ap(res, cap, "start");
    else if (owner[np] < 0) { pending = np; ap(res, cap, "for-sale"); }
    else if (owner[np] == pl) ap(res, cap, "own");
    else { int rent = t->rent * (holds_group(owner[np], t->group) ? 2 : 1); int ow = owner[np]; int p = pay(pl, rent, ow, &bk); ap(res, cap, "rent-%d->%s%s", p, PL[ow].id, bk ? " bankrupt" : ""); }
    size_t l = strlen(res); if (l && res[l - 1] == ' ') res[l - 1] = 0;
    rolled = 1; nrolls++;
}
static const char *do_buy(int pl, char *res, size_t cap) {
    if (pending < 0) return "nothing is for sale"; Tile *t = &T[pending];
    if (cash[pl] < t->price) return "not enough cash";
    cash[pl] -= t->price; bankv += t->price; owner[pending] = pl; snprintf(res, cap, "bought-%d", t->price); pending = -1; return NULL;
}
static const char *do_skip(char *res, size_t cap) { if (pending < 0) return "nothing is for sale"; snprintf(res, cap, "declined"); pending = -1; return NULL; }
static const char *do_end(char *res, size_t cap) {
    if (!rolled) return "roll first"; if (pending >= 0) return "buy or skip the tile first";
    int old = cur, n = cur; do { n = (n + 1) % np_game; } while (out[n] && n != old); if (n <= old) round_++;
    cur = n; rolled = 0;
    if (active_count() <= 1 || round_ >= R_rounds) over = 1;
    snprintf(res, cap, "next=%s round=%d%s", PL[cur].id, round_, over ? " over" : ""); return NULL;
}

static void ledger_path(char *p, size_t cap) { snprintf(p, cap, "%s/ledger.txt", g_state); }

/* replay the ledger into the derived state; returns 0 ok, 1 if a recorded result does not reproduce */
static int replay1(int *started) {
    char p[1100], ln[LINE]; char *f[10]; FILE *fp; *started = 0; reset_state(); seed = R_seed; ledger_path(p, sizeof p);
    fp = fopen(p, "r"); if (!fp) return 0;
    int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; char raw[LINE]; snprintf(raw, sizeof raw, "%s", ln); { char *e = raw + strlen(raw); while (e > raw && (e[-1] == 10 || e[-1] == 13)) *--e = 0; }
        int n = split(ln, f, 10); if (n < 7 || strcmp(f[0], "EVENT")) continue;
        if (nlast < 5) snprintf(lastlog[nlast++], LINE, "%s", raw); else { for (int q = 1; q < 5; q++) memcpy(lastlog[q - 1], lastlog[q], LINE); snprintf(lastlog[4], LINE, "%s", raw); }
        const char *actor = f[2], *verb = f[3], *target = f[4], *rec = f[6]; char res[LINE]; res[0] = 0; const char *err = NULL;
        if (!strcmp(verb, "new")) { long sd; int pn; if (sscanf(rec, "seed=%ld,players=%d", &sd, &pn) != 2 || pn < 2 || pn > npl) { fprintf(stderr, "ledger line %d: bad new row\n", lineno); fclose(fp); return 1; } seed = sd; reset_state(); np_game = pn; *started = 1; nev++; continue; }
        if (!*started) { fprintf(stderr, "ledger line %d: row before new\n", lineno); fclose(fp); return 1; }
        if (!strcmp(verb, "seat")) { int sp = pidx(target); if (sp < 0 || sp >= np_game || over || (strcmp(rec, "human") && strcmp(rec, "pet")) || human[sp] == !strcmp(rec, "human")) { fprintf(stderr, "ledger line %d: a bad seat row\n", lineno); fclose(fp); return 1; } human[sp] = !strcmp(rec, "human"); nev++; continue; }
        int pl = pidx(actor); if (pl < 0 || pl != cur || over) { fprintf(stderr, "ledger line %d: %s by %s is out of turn or after the end\n", lineno, verb, actor); fclose(fp); return 1; }
        if (!strcmp(verb, "roll")) {
            int d1, d2; if (rolled || sscanf(target, "%d,%d", &d1, &d2) != 2 || d1 != die(seed, nrolls, 0) || d2 != die(seed, nrolls, 1)) { fprintf(stderr, "ledger line %d: roll does not match the seeded dice\n", lineno); fclose(fp); return 1; }
            do_roll(pl, d1, d2, res, sizeof res);
        } else if (!strcmp(verb, "buy")) err = do_buy(pl, res, sizeof res);
        else if (!strcmp(verb, "skip")) err = do_skip(res, sizeof res);
        else if (!strcmp(verb, "end_turn")) err = do_end(res, sizeof res);
        else { fprintf(stderr, "ledger line %d: unknown verb %s\n", lineno, verb); fclose(fp); return 1; }
        if (err || strcmp(res, rec)) { fprintf(stderr, "ledger line %d: recorded result \"%s\" but replay gives \"%s\"\n", lineno, rec, err ? err : res); fclose(fp); return 1; }
        nev++;
    }
    fclose(fp); return 0;
}

/* ===== schema 2 (classic): lobby, join, leave, start, order, first (spec sections 5.2, 10.1). Rows after `first` (roll, buy, ...) arrive in M3+; replay refuses them as unknown verbs until then. ===== */
enum { PH_LOBBY = 0, PH_ORDER = 1, PH_PRE = 2 };
static int phase2, thrown[MAXP], tot[MAXP], elig[MAXP], first_w = -1;
static char hex16[20];
static const char *hx(uint64_t h) { snprintf(hex16, sizeof hex16, "%016llx", (unsigned long long)h); return hex16; }

static int name_ok(const char *s) { size_t n = strlen(s); if (n < 1 || n > 16) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == ' ' || *s == '_' || *s == '-')) return 0; return 1; }
static int pers_ok(const char *s) {
    size_t n = strlen(s); if (n < 1 || n > 16) return 0; for (const char *c = s; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) return 0;
    if (!strcmp(s, "-")) return 1;
    char p[1100], ln[LINE]; char *f[10]; snprintf(p, sizeof p, "%s/personalities.pdl", g_app); FILE *fp = fopen(p, "r"); if (!fp) return 1;   /* no personalities file yet (M10): any well-formed id is accepted */
    int ok = 0; while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int k = split(ln, f, 10); if (k >= 2 && !strcmp(f[0], "PERSONALITY") && !strcmp(f[1], s)) ok = 1; } fclose(fp); return ok;
}
static void next_id(char *id) { for (int k = 1; k <= 8; k++) { char c[8]; snprintf(c, sizeof c, "p%d", k); if (pidx(c) < 0) { snprintf(id, 8, "%s", c); return; } } id[0] = 0; }
static int token_taken(int t) { for (int i = 0; i < npl; i++) if (PL[i].token == t) return 1; return 0; }

/* join: adds the next free id; res is the recorded result. Returns an error sentence or NULL. */
static const char *do_join(const char *name, const char *drv, const char *pers, int token, char *res, size_t cap) {
    if (phase2 != PH_LOBBY) return "joining closes when the game starts";
    if (npl >= R_maxp) return "the table is full";
    if (!name_ok(name)) return "a name is 1 to 16 letters, digits, space, _ or -";
    for (int i = 0; i < npl; i++) if (!strcmp(PL[i].name, name)) return "that name is already at the table";
    if (strcmp(drv, "human") && strcmp(drv, "pet")) return "a driver is human or pet";
    if (!pers_ok(pers)) return "unknown personality";
    if (token < 0) { token = 0; while (token < 8 && token_taken(token)) token++; }
    if (token > 7 || token_taken(token)) return "that token is taken (tokens are 0..7)";
    Pl *q = &PL[npl]; memset(q, 0, sizeof *q); next_id(q->id); snprintf(q->name, sizeof q->name, "%s", name); snprintf(q->style, sizeof q->style, "%s", pers); q->token = token;
    human[npl] = !strcmp(drv, "human"); npl++; np_game = npl;
    snprintf(res, cap, "driver=%s,token=%d,personality=%s", drv, token, pers); return NULL;
}
static const char *do_leave(int pl, char *res, size_t cap) {
    if (phase2 != PH_LOBBY) return "leaving closes when the game starts";
    for (int i = pl; i + 1 < npl; i++) { PL[i] = PL[i + 1]; human[i] = human[i + 1]; } npl--; np_game = npl; snprintf(res, cap, "left"); return NULL;
}
static const char *do_start(long sd, char *res, size_t cap) {
    if (phase2 != PH_LOBBY) return "the game already started";
    if (npl < 2) return "a game needs at least 2 players";
    seed = sd; phase2 = PH_ORDER; np_game = npl; for (int i = 0; i < MAXP; i++) { thrown[i] = 0; tot[i] = 0; elig[i] = i < npl; }
    snprintf(res, cap, "seed=%ld,players=%d,rules=%s", sd, npl, hx(g_rules_hash)); return NULL;
}
static int order_next(void) { for (int i = 0; i < npl; i++) if (elig[i] && !thrown[i]) return i; return -1; }
/* one opening throw. When every eligible player has thrown: a unique highest total sets first_w; a tie keeps only the tied players eligible and clears their throws. */
static const char *do_order(int pl, int d1, int d2, char *res, size_t cap) {
    if (phase2 != PH_ORDER || first_w >= 0) return "no opening throw is due";
    if (pl != order_next()) return "it is not that player's opening throw";
    thrown[pl] = 1; tot[pl] = d1 + d2; nrolls++; snprintf(res, cap, "total=%d", d1 + d2);
    if (order_next() < 0) {
        int mx = 0, c = 0; for (int i = 0; i < npl; i++) if (elig[i] && tot[i] > mx) mx = tot[i];
        for (int i = 0; i < npl; i++) if (elig[i] && tot[i] == mx) { c++; first_w = i; }
        if (c > 1) { first_w = -1; for (int i = 0; i < npl; i++) { elig[i] = elig[i] && tot[i] == mx; thrown[i] = 0; } }
    }
    return NULL;
}
static void apply_first(void) { cur = first_w; first_w = -1; phase2 = PH_PRE; }

/* ===== M3: roll, land, Start money, tax, rent (street / rail / util), buy, skip, end_turn, simple bankruptcy, over. Doubles and jail are M4, cards M5, buildings/mortgages M6, auctions M7, the debt queue M8:
 * until then a tile of those kinds resolves as `todo-<kind>` (nothing happens) and `skip` just declines. ===== */
enum { PH_BUY = 3, PH_DEBT = 4, PH_POST = 5, PH_OVER = 6 };
static int net(int pl);
static int debt_amt, debt_cred = -1, over_due, dice_last;
static char over_res[40];
static void tk(char *res, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void tk(char *res, size_t cap, const char *fmt, ...) {
    char t[160]; va_list a; va_start(a, fmt); vsnprintf(t, sizeof t, fmt, a); va_end(a);
    if (res[0]) ap(res, cap, ";"); ap(res, cap, "%s", t);
}
static int count_owned_kind(int p, const char *kind) { int c = 0; for (int k = 0; k < nt; k++) if (owner[k] == p && !strcmp(T[k].kind, kind)) c++; return c; }
/* the rent function (spec 4.3), pure: what `ow` is owed for tile t after a roll of `dice` */
static int rent_due(int t, int ow, int dice) {
    Tile *x = &T[t];
    if (!strcmp(x->kind, "street")) { int full = 1; for (int k = 0; k < nt; k++) if (!strcmp(T[k].kind, "street") && T[k].group == x->group && owner[k] != ow) full = 0; return full ? 2 * x->r[0] : x->r[0]; }
    if (!strcmp(x->kind, "rail")) { int n = count_owned_kind(ow, "rail"); return 25 << (n - 1); }
    if (!strcmp(x->kind, "util")) { int n = count_owned_kind(ow, "util"); return (n >= 2 ? 10 : 4) * dice; }
    return 0;
}
/* pay `amount` from pl to cred (-1 bank) if the cash is there; otherwise open a debt (phase DEBT) and move nothing. Returns 1 when paid. */
static int charge2(int pl, int amount, int cred, char *res, size_t cap, const char *paidtok) {
    if (cash[pl] >= amount) { cash[pl] -= amount; if (cred < 0) bankv += amount; else cash[cred] += amount; tk(res, cap, "%s-%d%s%s", paidtok, amount, cred >= 0 ? "->" : "", cred >= 0 ? PL[cred].id : ""); return 1; }
    debt_amt = amount; debt_cred = cred; phase2 = PH_DEBT; tk(res, cap, "debt=%d->%s", amount, cred >= 0 ? PL[cred].id : "bank"); return 0;
}
static void land2(int pl, int dice, char *res, size_t cap) {
    Tile *t = &T[pos[pl]]; const char *k = t->kind; pending = -1;
    if (!strcmp(k, "go")) tk(res, cap, "start");
    else if (!strcmp(k, "street") || !strcmp(k, "rail") || !strcmp(k, "util")) {
        if (owner[pos[pl]] < 0) { pending = pos[pl]; phase2 = PH_BUY; tk(res, cap, "for-sale"); }
        else if (owner[pos[pl]] == pl) tk(res, cap, "own");
        else charge2(pl, rent_due(pos[pl], owner[pos[pl]], dice), owner[pos[pl]], res, cap, "rent");
    } else if (!strcmp(k, "tax")) charge2(pl, t->price, -1, res, cap, "tax");
    else if (!strcmp(k, "free")) tk(res, cap, "free");
    else if (!strcmp(k, "jail")) tk(res, cap, "visit");
    else tk(res, cap, "todo-%s", k);
}
static int best_net(void) { int best = -1; for (int i = 0; i < np_game; i++) if (!out[i] && (best < 0 || net(i) > net(best))) best = i; return best; }
/* after a turn passes: is the game over? (one player left = winner; the safety cap = stalemate won by net worth). The caller's next row must be `over`. */
static void check_over_due(void) {
    if (active_count() <= 1) { snprintf(over_res, sizeof over_res, "winner=%s", PL[best_net()].id); over_due = 1; }
    else if (round_ >= R_rounds) { snprintf(over_res, sizeof over_res, "stalemate=%s", PL[best_net()].id); over_due = 1; }
}
static void advance_turn(void) { int old = cur, n = cur; do { n = (n + 1) % np_game; } while (out[n] && n != old); if (n <= old) round_++; cur = n; phase2 = PH_PRE; dice_last = 0; check_over_due(); }
/* one action by the current player. `target` is "d1,d2" for roll (already checked against the seeded dice by the caller), the creditor id or "bank" for bankrupt, else "-". */
static const char *apply2(int pl, const char *verb, const char *target, char *res, size_t cap) {
    res[0] = 0;
    if (!strcmp(verb, "roll")) {
        if (phase2 != PH_PRE) return "you cannot roll now"; int d1, d2; if (sscanf(target, "%d,%d", &d1, &d2) != 2) return "bad dice"; nrolls++;
        int old = pos[pl], step = d1 + d2; pos[pl] = (old + step) % nt; dice_last = step; tk(res, cap, "to=%d", pos[pl]);
        if (old + step >= nt) { cash[pl] += R_go; bankv -= R_go; tk(res, cap, "go+%d", R_go); }
        phase2 = PH_POST; land2(pl, step, res, cap); return NULL;
    }
    if (!strcmp(verb, "buy")) {
        if (phase2 != PH_BUY) return "nothing is for sale"; Tile *t = &T[pending]; if (cash[pl] < t->price) return "not enough cash";
        cash[pl] -= t->price; bankv += t->price; owner[pending] = pl; snprintf(res, cap, "bought-%d", t->price); pending = -1; phase2 = PH_POST; return NULL;
    }
    if (!strcmp(verb, "skip")) { if (phase2 != PH_BUY) return "nothing is for sale"; snprintf(res, cap, "declined"); pending = -1; phase2 = PH_POST; return NULL; }
    if (!strcmp(verb, "end_turn")) {
        if (phase2 != PH_POST) return phase2 == PH_PRE ? "roll first" : phase2 == PH_BUY ? "buy or skip the tile first" : "settle the debt first";
        advance_turn(); snprintf(res, cap, "next=%s;round=%d", PL[cur].id, round_); return NULL;
    }
    if (!strcmp(verb, "bankrupt")) {
        if (phase2 != PH_DEBT) return "you owe nothing you cannot pay";
        const char *want = debt_cred >= 0 ? PL[debt_cred].id : "bank"; if (strcmp(target, want)) return "the creditor does not match the debt";
        int props = 0; for (int k = 0; k < nt; k++) if (owner[k] == pl) { props++; owner[k] = debt_cred; }   /* to a player: all assets go to the creditor (finding F3); to the bank: back to the bank (the auction is M7/M8) */
        int c = cash[pl]; if (debt_cred >= 0) cash[debt_cred] += c; else bankv += c; cash[pl] = 0; out[pl] = 1; debt_amt = 0; debt_cred = -1;
        snprintf(res, cap, "out;cash=%d;props=%d;cards=0", c, props); advance_turn(); return NULL;
    }
    return "unknown verb";
}
static const char *apply_over(char *res, size_t cap) { if (!over_due) return "the game is not over"; snprintf(res, cap, "%s", over_res); over_due = 0; over = 1; phase2 = PH_OVER; return NULL; }
static const char *legal_now(void) {
    static const char *t[7] = { "join leave start seat (lobby)", "order seat (opening throw)", "roll seat", "buy skip seat", "bankrupt seat", "end_turn seat", "read-only verbs only (the game is over)" };
    return t[phase2];
}
static int replay2(int *started) {
    char p[1100], ln[LINE]; char *f[10]; FILE *fp; *started = 0; reset_state(); seed = R_seed; npl = 0; np_game = 0; phase2 = PH_LOBBY; first_w = -1; over_due = 0; debt_amt = 0; debt_cred = -1; pending = -1; dice_last = 0;
    for (int i = 0; i < MAXP; i++) { thrown[i] = 0; tot[i] = 0; elig[i] = 0; }
    ledger_path(p, sizeof p); fp = fopen(p, "r"); if (!fp) return 0;
    int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) {
        lineno++; char raw[LINE]; snprintf(raw, sizeof raw, "%s", ln); { char *e = raw + strlen(raw); while (e > raw && (e[-1] == 10 || e[-1] == 13)) *--e = 0; }
        if (strlen(raw) >= LINE - 1) { fprintf(stderr, "ledger line %d: a row is longer than %d bytes\n", lineno, LINE - 2); fclose(fp); return 1; }
        int n = split(ln, f, 10); if (n < 7 || strcmp(f[0], "EVENT")) { fprintf(stderr, "ledger line %d: not an EVENT row of 7 fields\n", lineno); fclose(fp); return 1; }
        if (nlast < 5) snprintf(lastlog[nlast++], LINE, "%s", raw); else { for (int q = 1; q < 5; q++) memcpy(lastlog[q - 1], lastlog[q], LINE); snprintf(lastlog[4], LINE, "%s", raw); }
        const char *actor = f[2], *verb = f[3], *target = f[4], *rec = f[6]; char res[LINE]; res[0] = 0; const char *err = NULL;
        if (first_w >= 0 && strcmp(verb, "first")) { fprintf(stderr, "ledger line %d: the opening throws ended, `first` must follow\n", lineno); fclose(fp); return 1; }
        if (over_due && strcmp(verb, "over")) { fprintf(stderr, "ledger line %d: the game ended, `over` must follow\n", lineno); fclose(fp); return 1; }
        if (!*started) {
            char want[160]; snprintf(want, sizeof want, "ruleset=%s,schema=2,rules=%s,max=%d", g_rs, hx(g_rules_hash), R_maxp);
            if (strcmp(verb, "lobby") || strcmp(actor, "bank")) { fprintf(stderr, "ledger line %d: a schema 2 ledger starts with `lobby`\n", lineno); fclose(fp); return 1; }
            if (strcmp(rec, want)) { fprintf(stderr, "ledger line %d: the lobby row says \"%s\" but this ruleset gives \"%s\" (the rules changed since the game began)\n", lineno, rec, want); fclose(fp); return 1; }
            *started = 1; nev++; continue;
        }
        if (!strcmp(verb, "join")) {
            char drv[8] = "", pers[24] = ""; int tk = -1; if (sscanf(rec, "driver=%7[a-z],token=%d,personality=%23s", drv, &tk, pers) != 3) { fprintf(stderr, "ledger line %d: a bad join row\n", lineno); fclose(fp); return 1; }
            err = do_join(target, drv, pers, tk, res, sizeof res); if (!err && strcmp(PL[npl - 1].id, actor)) err = "the joining id is not the next free id";
        } else if (!strcmp(verb, "leave")) { int pl = pidx(actor); err = pl < 0 ? "no such player" : do_leave(pl, res, sizeof res); }
        else if (!strcmp(verb, "start")) {
            long sd; int pn; char hh[24]; if (strcmp(actor, "bank") || sscanf(rec, "seed=%ld,players=%d,rules=%23s", &sd, &pn, hh) != 3) { fprintf(stderr, "ledger line %d: a bad start row\n", lineno); fclose(fp); return 1; }
            err = do_start(sd, res, sizeof res);
        } else if (!strcmp(verb, "order")) {
            int d1, d2, pl = pidx(actor); if (pl < 0 || sscanf(target, "%d,%d", &d1, &d2) != 2 || d1 != die(seed, nrolls, 0) || d2 != die(seed, nrolls, 1)) { fprintf(stderr, "ledger line %d: order does not match the seeded dice\n", lineno); fclose(fp); return 1; }
            err = do_order(pl, d1, d2, res, sizeof res);
        } else if (!strcmp(verb, "first")) {
            if (first_w < 0 || strcmp(actor, "bank")) { fprintf(stderr, "ledger line %d: `first` without a decided opening\n", lineno); fclose(fp); return 1; }
            snprintf(res, sizeof res, "%s", PL[first_w].id); apply_first();
        } else if (!strcmp(verb, "seat")) {
            int sp = pidx(target); if (sp < 0 || (strcmp(rec, "human") && strcmp(rec, "pet")) || human[sp] == !strcmp(rec, "human")) { fprintf(stderr, "ledger line %d: a bad seat row\n", lineno); fclose(fp); return 1; }
            human[sp] = !strcmp(rec, "human"); snprintf(res, sizeof res, "%s", rec);
        } else if (!strcmp(verb, "roll") || !strcmp(verb, "buy") || !strcmp(verb, "skip") || !strcmp(verb, "end_turn") || !strcmp(verb, "bankrupt")) {
            int pl = pidx(actor); if (pl < 0 || pl != cur || phase2 < PH_PRE || phase2 == PH_OVER) { fprintf(stderr, "ledger line %d: %s by %s is out of turn or outside play\n", lineno, verb, actor); fclose(fp); return 1; }
            if (!strcmp(verb, "roll")) { int d1, d2; if (sscanf(target, "%d,%d", &d1, &d2) != 2 || d1 != die(seed, nrolls, 0) || d2 != die(seed, nrolls, 1)) { fprintf(stderr, "ledger line %d: roll does not match the seeded dice\n", lineno); fclose(fp); return 1; } }
            err = apply2(pl, verb, target, res, sizeof res);
        } else if (!strcmp(verb, "over")) { if (strcmp(actor, "bank")) { fprintf(stderr, "ledger line %d: only the bank ends the game\n", lineno); fclose(fp); return 1; } err = apply_over(res, sizeof res);
        } else { fprintf(stderr, "ledger line %d: unknown verb %s (this verb is not part of the ruleset yet)\n", lineno, verb); fclose(fp); return 1; }
        if (err || strcmp(res, rec)) { fprintf(stderr, "ledger line %d: recorded result \"%s\" but replay gives \"%s\"\n", lineno, rec, err ? err : res); fclose(fp); return 1; }
        nev++;
    }
    fclose(fp); return 0;
}
static int replay(int *started) { return g_schema == 2 ? replay2(started) : replay1(started); }

static void append_file(const char *name, const char *row) {
    char p[1100]; snprintf(p, sizeof p, "%s/%s", g_state, name);
    int fd = open(p, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) bad("cannot append to the state folder");
    char b[LINE + 2]; int l = snprintf(b, sizeof b, "%s\n", row); if (write(fd, b, (size_t)l) != l) { close(fd); bad("short write"); } close(fd);
}
static void event(const char *actor, const char *verb, const char *target, int at, const char *res) {
    char row[LINE]; snprintf(row, sizeof row, "EVENT|%d|%s|%s|%s|%d|%s", round_, actor, verb, target, at, res); append_file("ledger.txt", row); nev++;
}

static int net(int pl) { int v = cash[pl]; for (int i = 0; i < nt; i++) if (owner[i] == pl) v += T[i].price; return v; }
static void build_status(char *b, size_t cap) {
    b[0] = 0; ap(b, cap, "GAME deskopoly seed=%ld players=%d events=%d\n", seed, np_game, nev);
    ap(b, cap, "STATE round=%d current=%s rolled=%d pending=%d over=%d bank=%d\n", round_, PL[cur].id, rolled, pending, over, bankv);
    int sum = bankv;
    for (int i = 0; i < np_game; i++) { int props = 0; for (int k = 0; k < nt; k++) if (owner[k] == i) props++; sum += cash[i];
        ap(b, cap, "PLAYER %s %s style=%s cash=%d pos=%d out=%d props=%d net=%d%s\n", PL[i].id, PL[i].name, PL[i].style, cash[i], pos[i], out[i], props, net(i), human[i] ? " driver=human" : ""); }
    for (int k = 0; k < nt; k++) if (owner[k] >= 0) ap(b, cap, "TILE %d %s owner=%s\n", k, T[k].name, PL[owner[k]].id);
    ap(b, cap, "TOTAL sum=%d expect=%d\n", sum, R_start * np_game);
}
static uint64_t fnv(const char *s) { uint64_t h = 1469598103934665603ULL; for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; } return h; }

static int need_game(int *started) { if (replay(started)) return 1; if (!*started) { say("no game: run `new` first"); return 2; } return 0; }

/* --- decisions (autoplay): the pet stand-in. Same DEC grammar the decision bank (dec_op) already digests. --- */
static int wants_buy(int pl) {
    Tile *t = &T[pending]; Pl *q = &PL[pl];
    if (!strcmp(q->style, "bold")) return 1;
    if (!strcmp(q->style, "cautious")) return cash[pl] - t->price >= q->param;
    return (int)(mix((uint64_t)seed * 7919ULL + (uint64_t)nrolls * 31ULL + 99) % 100) < q->param;
}
static void dec(int pl, const char *act, int tile, const char *outcome) {
    char row[LINE]; snprintf(row, sizeof row, "DEC|%d|R%d|%s|forsale|%s|%d|%s", 1000000000 + nev, round_, PL[pl].name, act, tile, outcome); append_file("decisions.txt", row);
}

static int v_roll(void) {
    int st, rc = need_game(&st); if (rc) return rc; if (over) bad("the game is over"); if (rolled) bad("already rolled this turn");
    char t[32], res[LINE]; int d1 = die(seed, nrolls, 0), d2 = die(seed, nrolls, 1); snprintf(t, sizeof t, "%d,%d", d1, d2);
    int pl = cur; do_roll(pl, d1, d2, res, sizeof res); event(PL[pl].id, "roll", t, pos[pl], res); printf("%s rolled %s -> tile %d: %s\n", PL[pl].id, t, pos[pl], res); return 0;
}
static int act_simple(const char *verb) {
    int st, rc = need_game(&st); if (rc) return rc; if (over) bad("the game is over");
    char res[LINE]; res[0] = 0; const char *err; int pl = cur, tile = pending;
    if (!strcmp(verb, "buy")) err = do_buy(pl, res, sizeof res); else if (!strcmp(verb, "skip")) err = do_skip(res, sizeof res); else err = do_end(res, sizeof res);
    if (err) { if (human[pl] && !strcmp(verb, "buy") && tile >= 0 && cash[pl] < T[tile].price) dec(pl, "buy", tile, "blocked"); fprintf(stderr, "deskopoly: refused: %s\n", err); return 2; }
    event(PL[pl].id, verb, "-", pos[pl], res); if (human[pl] && (!strcmp(verb, "buy") || !strcmp(verb, "skip"))) dec(pl, verb, tile, "ok");
    printf("%s %s: %s\n", PL[pl].id, verb, res); return 0;
}
static int v_new(int argc, char **argv) {
    char p[1100]; ledger_path(p, sizeof p); struct stat sb; if (stat(p, &sb) == 0) bad("a ledger already exists here: a new game needs an empty state folder");
    long sd = argc > 0 ? atol(argv[0]) : R_seed; int pn = argc > 1 ? atoi(argv[1]) : npl; if (pn < 2 || pn > npl) bad("players must be 2..the PLAYER rows in rules.pdl");
    char res[64]; snprintf(res, sizeof res, "seed=%ld,players=%d", sd, pn); round_ = 0; event("bank", "new", "-", 0, res); printf("new game %s\n", res); return 0;
}
/* --- schema 2 verbs. Each replays first, checks legality, appends, and a refusal appends nothing (exit 2). --- */
static int need2(const char *verb) {
    if (g_schema != 2) { fprintf(stderr, "deskopoly: refused: %s belongs to a schema 2 (classic) game; this state folder is empty or holds a schema 1 game (use `lobby <ruleset>` for a classic one)\n", verb); return 2; }
    return 0;
}
static int v_lobby(int argc, char **argv) {
    (void)argc; (void)argv; char p[1100]; ledger_path(p, sizeof p); struct stat sb; if (stat(p, &sb) == 0) bad("a ledger already exists here: a new game needs an empty state folder");
    char res[160]; snprintf(res, sizeof res, "ruleset=%s,schema=2,rules=%s,max=%d", g_rs, hx(g_rules_hash), R_maxp); round_ = 0; event("bank", "lobby", "-", 0, res); printf("lobby %s\n", res); return 0;
}
static int v_join(int argc, char **argv) {
    if (need2("join")) return 2; int st, rc = need_game(&st); if (rc) return rc;
    if (argc < 2) bad("usage: join <name> <human|pet> [personality] [token]");
    const char *pers = argc > 2 ? argv[2] : "-"; int tk = argc > 3 ? atoi(argv[3]) : -1; if (argc > 3 && (strlen(argv[3]) != 1 || argv[3][0] < '0' || argv[3][0] > '7')) bad("a token is 0..7");
    char res[160]; const char *err = do_join(argv[0], argv[1], pers, tk, res, sizeof res); if (err) { fprintf(stderr, "deskopoly: refused: %s\n", err); return 2; }
    event(PL[npl - 1].id, "join", argv[0], 0, res); printf("%s\n", PL[npl - 1].id); return 0;
}
static int v_leave(int argc, char **argv) {
    if (need2("leave")) return 2; int st, rc = need_game(&st); if (rc) return rc; if (argc < 1) bad("usage: leave <player id>");
    int pl = pidx(argv[0]); if (pl < 0) bad("no such player"); char res[32]; const char *err = do_leave(pl, res, sizeof res); if (err) { fprintf(stderr, "deskopoly: refused: %s\n", err); return 2; }
    event(argv[0], "leave", "-", 0, res); printf("%s left\n", argv[0]); return 0;
}
static int v_start(int argc, char **argv) {
    if (need2("start")) return 2; int st, rc = need_game(&st); if (rc) return rc;
    long sd = argc > 0 ? atol(argv[0]) : R_seed; char res[160]; const char *err = do_start(sd, res, sizeof res); if (err) { fprintf(stderr, "deskopoly: refused: %s\n", err); return 2; }
    event("bank", "start", "-", 0, res); printf("start %s\n", res); return 0;
}
static int v_order(void) {
    if (need2("order")) return 2; int st, rc = need_game(&st); if (rc) return rc;
    int pl = order_next(); if (phase2 != PH_ORDER || first_w >= 0 || pl < 0) { fprintf(stderr, "deskopoly: refused: no opening throw is due\n"); return 2; }
    int d1 = die(seed, nrolls, 0), d2 = die(seed, nrolls, 1); char t[16], res[64]; snprintf(t, sizeof t, "%d,%d", d1, d2); char id[8]; snprintf(id, sizeof id, "%s", PL[pl].id);
    const char *err = do_order(pl, d1, d2, res, sizeof res); if (err) { fprintf(stderr, "deskopoly: refused: %s\n", err); return 2; }
    event(id, "order", t, 0, res); printf("%s threw %s %s\n", id, t, res);
    if (first_w >= 0) { char fid[8]; snprintf(fid, sizeof fid, "%s", PL[first_w].id); apply_first(); event("bank", "first", "-", 0, fid); printf("first %s\n", fid); }
    return 0;
}
static void build_status2(char *b, size_t cap) {
    static const char *ph[7] = { "lobby", "order", "pre", "buy", "debt", "post", "over" };
    b[0] = 0; ap(b, cap, "GAME deskopoly schema=2 ruleset=%s seed=%ld players=%d events=%d\n", g_rs, seed, np_game, nev);
    ap(b, cap, "STATE phase=%s current=%s round=%d pending=%d debt=%d rules=%s bank=%d over=%d\n", ph[phase2], phase2 >= PH_PRE ? PL[cur].id : "-", round_, pending, debt_amt, hx(g_rules_hash), bankv, over);
    int sum = bankv;
    for (int i = 0; i < np_game; i++) { sum += cash[i]; ap(b, cap, "PLAYER %s %s driver=%s token=%d personality=%s cash=%d pos=%d out=%d\n", PL[i].id, PL[i].name, human[i] ? "human" : "pet", PL[i].token, PL[i].style, cash[i], pos[i], out[i]); }
    for (int k = 0; k < nt; k++) if (owner[k] >= 0) ap(b, cap, "TILE %d %s owner=%s\n", k, T[k].name, PL[owner[k]].id);
    ap(b, cap, "TOTAL sum=%d expect=%d\n", sum, R_start * np_game);
}

/* --- M3 verbs for a classic game: roll buy skip end_turn bankrupt. `as=pN` (optional last argument) names who the caller thinks is acting; it is refused unless that player is the current one (finding F24). --- */
static int act2_do(const char *verb, const char *as, char *msg, size_t cap) {
    int st, rc = need_game(&st); if (rc) return rc;
    if (phase2 < PH_PRE || phase2 == PH_OVER) { snprintf(msg, cap, "refused: %s is not legal now (%s)", verb, legal_now()); return 2; }
    if (first_w >= 0 || over_due) { snprintf(msg, cap, "refused: a decided opening or ending is missing its row (run check)"); return 2; }
    int pl = cur; if (as && strcmp(as, PL[pl].id)) { snprintf(msg, cap, "refused: it is %s's turn, not %s's", PL[pl].id, as); return 2; }
    char t[40] = "-", res[LINE]; if (!strcmp(verb, "roll")) { int d1 = die(seed, nrolls, 0), d2 = die(seed, nrolls, 1); snprintf(t, sizeof t, "%d,%d", d1, d2); }
    else if (!strcmp(verb, "bankrupt") && phase2 == PH_DEBT) snprintf(t, sizeof t, "%s", debt_cred >= 0 ? PL[debt_cred].id : "bank");
    char id[8]; snprintf(id, sizeof id, "%s", PL[pl].id);
    const char *err = apply2(pl, verb, t, res, sizeof res); if (err) { snprintf(msg, cap, "refused: %s", err); return 2; }
    event(id, verb, t, pos[pl], res); snprintf(msg, cap, "%s %s: %s", id, verb, res);
    if (over_due) { char r2[LINE]; apply_over(r2, sizeof r2); event("bank", "over", "-", 0, r2); char m2[LINE]; snprintf(m2, sizeof m2, "\nover: %s", r2); size_t l = strlen(msg); snprintf(msg + l, cap - l, "%s", m2); }
    return 0;
}
static int v_act2(const char *verb, int argc, char **argv) {
    const char *as = NULL; for (int i = 0; i < argc; i++) if (!strncmp(argv[i], "as=", 3)) as = argv[i] + 3;
    char msg[LINE * 2]; int rc = act2_do(verb, as, msg, sizeof msg); if (rc == 2) { fprintf(stderr, "deskopoly: %s\n", msg); return 2; } if (rc) return rc; puts(msg); return 0;
}
/* STOPGAP pet policy (removal condition: M10 replaces it with personalities.pdl + BRAIN rows): buy when affordable, otherwise skip; a debt is declared bankrupt; everything else is the next step. */
static int tick2(char *msg, size_t cap) {
    if (phase2 == PH_OVER) { snprintf(msg, cap, "over"); return 2; }
    if (phase2 == PH_ORDER) { int o = order_next(); if (o >= 0 && human[o]) { snprintf(msg, cap, "waiting %s %s human (opening throw)", PL[o].id, PL[o].name); return 1; } if (o < 0 || first_w >= 0) { snprintf(msg, cap, "stuck in the opening"); return -1; } int rc = v_order(); snprintf(msg, cap, "opening throw"); return rc == 0 ? 0 : -1; }
    if (phase2 < PH_PRE) { snprintf(msg, cap, "refused: the opening is not finished (%s)", legal_now()); return -1; }
    if (human[cur]) { snprintf(msg, cap, "waiting %s %s human", PL[cur].id, PL[cur].name); return 1; }
    const char *v = phase2 == PH_PRE ? "roll" : phase2 == PH_BUY ? (cash[cur] >= T[pending].price ? "buy" : "skip") : phase2 == PH_DEBT ? "bankrupt" : "end_turn";
    int rc = act2_do(v, NULL, msg, cap); return rc == 0 ? 0 : -1;
}
static int v_tick2(void) { int st, rc = need_game(&st); if (rc) return rc; char m[LINE * 2]; int r = tick2(m, sizeof m); if (r < 0) { fprintf(stderr, "deskopoly: %s\n", m); return 1; } printf("TICK %s\n", m); return 0; }
static int v_autoplay2(void) {
    int st, rc = need_game(&st); if (rc) return rc; char m[LINE * 2]; int r = 0;
    for (long g = 0; g < 400000; g++) { r = tick2(m, sizeof m); if (r != 0) break; }
    if (r < 0) { fprintf(stderr, "deskopoly: autoplay %s\n", m); return 1; }
    if (r == 1) { printf("AUTOPLAY waiting events=%d %s\n", nev, m); return 0; }
    if (r != 2) { fprintf(stderr, "deskopoly: autoplay guard hit before the game ended\n"); return 1; }
    int ok = 0; char p[1100], ln[LINE]; char *f[10]; ledger_path(p, sizeof p); FILE *fp = fopen(p, "r"); char last[80] = ""; if (fp) { while (fgets(ln, sizeof ln, fp)) { int n = split(ln, f, 10); if (n >= 7 && !strcmp(f[3], "over")) { snprintf(last, sizeof last, "%s", f[6]); ok = 1; } } fclose(fp); }
    printf("AUTOPLAY events=%d rounds=%d over=1 %s\n", nev, round_, ok ? last : "none"); return 0;
}
/* help [verb]: the verbs from verbs.pdl (data) and, for a classic game, what is legal right now. */
static int v_help(int argc, char **argv) {
    char p[1100], ln[LINE]; char *f[8]; snprintf(p, sizeof p, "%s/verbs.pdl", g_app); FILE *fp = fopen(p, "r"); if (!fp) bad("cannot read verbs.pdl");
    int shown = 0; if (argc == 0) printf("deskopoly <app_dir> <state_dir> <verb> [args]   (ruleset %s, schema %d)\n", g_rs, g_schema);
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n < 6 || strcmp(f[0], "VERB")) continue;
        if (argc > 0 && strcmp(f[1], argv[0])) continue; printf("  %-12s %-34s %s\n", f[1], strcmp(f[4], "-") ? f[4] : "", f[5]); shown++; }
    fclose(fp); if (argc > 0 && !shown) { fprintf(stderr, "deskopoly: no such verb %s (run help)\n", argv[0]); return 2; }
    if (g_schema == 2 && argc == 0) { int st; if (replay(&st) == 0 && st) printf("\nphase now: %s%s\nlegal now: %s\n", (const char *[]){ "lobby", "order", "pre", "buy", "debt", "post", "over" }[phase2], phase2 >= PH_PRE && phase2 != PH_OVER ? "" : "", legal_now()); }
    printf("\nHow to play and drive it: BOOK 16.game/DESKOPOLY-CLASSIC-HOW-TO-PLAY-AND-DRIVE.md; every action may end with as=pN (refused unless pN is the player whose turn it is).\n"); return 0;
}

static int v_status(void) { int st, rc = need_game(&st); if (rc) return rc; static char b[32768]; if (g_schema == 2) build_status2(b, sizeof b); else build_status(b, sizeof b); fputs(b, stdout); return 0; }
static int v_replay(void) { int st, rc = need_game(&st); if (rc) return rc; static char b[32768]; if (g_schema == 2) build_status2(b, sizeof b); else build_status(b, sizeof b); printf("REPLAY events=%d hash=%016llx\n", nev, (unsigned long long)fnv(b)); return 0; }
/* check for a schema 2 game (spec 5.6: the invariants that exist at this milestone: I1 money, I2 cash, I3 position, I4/I16 ownership of nothing yet, I13 one current player, I18 dice index) */
static int check2(void) {
    int fails = 0, sum = bankv, orders = 0;
    for (int i = 0; i < np_game; i++) { sum += cash[i]; if (cash[i] < 0) { printf("FAIL I2 negative-cash %s\n", PL[i].id); fails++; } if (pos[i] < 0 || pos[i] >= nt) { printf("FAIL I3 position-range %s\n", PL[i].id); fails++; } }
    if (sum != R_start * np_game) { printf("FAIL I1 money-not-conserved sum=%d expect=%d\n", sum, R_start * np_game); fails++; }
    for (int k = 0; k < nt; k++) if (owner[k] >= 0 && (strcmp(T[k].kind, "street") && strcmp(T[k].kind, "rail") && strcmp(T[k].kind, "util") || out[owner[k]])) { printf("FAIL I4 bad-owner tile=%d\n", k); fails++; }
    for (int i = 0; i < np_game; i++) if (out[i] && cash[i] != 0) { printf("FAIL I16 out-player-has-cash %s\n", PL[i].id); fails++; }
    if (phase2 == PH_DEBT && (debt_amt <= 0 || cash[cur] >= debt_amt)) { printf("FAIL I14 a debt that could be paid\n"); fails++; }
    if (phase2 >= PH_PRE && phase2 != PH_OVER && (cur < 0 || cur >= np_game || out[cur])) { printf("FAIL I13 no-current-player\n"); fails++; }
    if (first_w >= 0) { printf("FAIL I17 the opening is decided but the `first` row is missing\n"); fails++; }
    if (over_due) { printf("FAIL I20 the game is over but the `over` row is missing\n"); fails++; }
    { char p[1100], ln[LINE]; char *f[10]; ledger_path(p, sizeof p); FILE *fp = fopen(p, "r"); if (fp) { while (fgets(ln, sizeof ln, fp)) { int n = split(ln, f, 10); if (n >= 7 && (!strcmp(f[3], "order") || !strcmp(f[3], "roll"))) orders++; } fclose(fp); } }
    if (orders != nrolls) { printf("FAIL I18 dice-index=%d but %d throws\n", nrolls, orders); fails++; }
    if (fails) return 1; printf("CHECK ok schema=2 events=%d sum=%d\n", nev, sum); return 0;
}
static int v_check(void) {
    int st, rc = need_game(&st); if (rc) return rc; if (g_schema == 2) return check2();
    int sum = bankv; int fails = 0;
    for (int i = 0; i < np_game; i++) { sum += cash[i]; if (cash[i] < 0) { printf("FAIL negative-cash %s\n", PL[i].id); fails++; } if (pos[i] < 0 || pos[i] >= nt) { printf("FAIL position-range %s\n", PL[i].id); fails++; } }
    if (sum != R_start * np_game) { printf("FAIL money-not-conserved sum=%d expect=%d\n", sum, R_start * np_game); fails++; }
    for (int k = 0; k < nt; k++) if (owner[k] >= 0 && (strcmp(T[k].kind, "prop") || out[owner[k]])) { printf("FAIL bad-owner tile=%d\n", k); fails++; }
    if (fails) return 1; printf("CHECK ok events=%d sum=%d\n", nev, sum); return 0;
}

/* mechanics: a pure PROJECTION of the data files (verbs.pdl, rules.pdl, board.pdl) plus the replayed state: the known mechanics and the hard state, as plain rows an editor can read.
 * It adds no fact of its own. GAP rows name a verb with no FLOW row (a mechanic nobody described). */
static int v_mechanics(void) {
    char p[1100], ln[LINE], out_[24576]; char *f[10]; FILE *fp; out_[0] = 0; char vn[40][24]; int nv = 0, hasflow[40] = {0};
    snprintf(p, sizeof p, "%s/verbs.pdl", g_app); fp = fopen(p, "r"); if (!fp) bad("cannot read verbs.pdl");
    ap(out_, sizeof out_, "MECHANICS|deskopoly|tiles=%d|players=%d|rounds=%d|start_cash=%d|pass_go=%d\n", nt, npl, R_rounds, R_start, R_go);
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; char raw[LINE]; snprintf(raw, sizeof raw, "%s", ln); char *e = raw + strlen(raw); while (e > raw && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        int n = split(ln, f, 10); if (n < 2) continue;
        if (!strcmp(f[0], "VERB") && nv < 40) { snprintf(vn[nv++], 24, "%s", f[1]); ap(out_, sizeof out_, "%s\n", raw); }
        else if (!strcmp(f[0], "FLOW") || !strcmp(f[0], "BRAIN")) ap(out_, sizeof out_, "%s\n", raw); }
    fclose(fp);
    snprintf(p, sizeof p, "%s/verbs.pdl", g_app); fp = fopen(p, "r");
    while (fp && fgets(ln, sizeof ln, fp)) { int n = split(ln, f, 10); if (n >= 2 && !strcmp(f[0], "FLOW")) for (int i = 0; i < nv; i++) if (!strcmp(vn[i], f[1])) hasflow[i] = 1; }
    if (fp) fclose(fp);
    for (int i = 0; i < nv; i++) if (!hasflow[i]) ap(out_, sizeof out_, "GAP|%s|no FLOW row\n", vn[i]);
    for (int i = 0; i < npl; i++) ap(out_, sizeof out_, "PLAYER|%s|%s|%s|%d\n", PL[i].id, PL[i].name, PL[i].style, PL[i].param);
    for (int k = 0; k < nt; k++) ap(out_, sizeof out_, "TILE|%d|%s|%s|%d|%d|%d\n", k, T[k].name, T[k].kind, T[k].price, T[k].rent, T[k].group);
    int st = 0, rc = replay(&st); if (rc) { ap(out_, sizeof out_, "STATE|ledger|does-not-replay\n"); }
    else if (!st) ap(out_, sizeof out_, "STATE|game|not-started\n");
    else { ap(out_, sizeof out_, "STATE|seed|%ld\nSTATE|round|%d\nSTATE|current|%s\nSTATE|rolled|%d\nSTATE|pending|%d\nSTATE|over|%d\nSTATE|bank|%d\nSTATE|events|%d\n", seed, round_, PL[cur].id, rolled, pending, over, bankv, nev);
        for (int i = 0; i < np_game; i++) ap(out_, sizeof out_, "STATE|%s.cash|%d\nSTATE|%s.pos|%d\nSTATE|%s.out|%d\nSTATE|%s.driver|%s\n", PL[i].id, cash[i], PL[i].id, pos[i], PL[i].id, out[i], PL[i].id, human[i] ? "human" : "pet");
        for (int k = 0; k < nt; k++) if (owner[k] >= 0) ap(out_, sizeof out_, "STATE|tile%d.owner|%s\n", k, PL[owner[k]].id); }
    fputs(out_, stdout);
    char tp[1100], fp2[1100]; snprintf(tp, sizeof tp, "%s/mechanics.txt.tmp", g_state); snprintf(fp2, sizeof fp2, "%s/mechanics.txt", g_state);
    FILE *o = fopen(tp, "w"); if (!o) bad("cannot write mechanics.txt"); fputs(out_, o); fclose(o); if (rename(tp, fp2)) bad("cannot rename mechanics.txt");
    return rc ? 1 : 0;
}

/* --- page: the board DRAWN as an editor desk (cells.txt) from board.pdl + page.pdl + the replayed ledger. A projection: it decides no rule. --- */
static int HD_px = 24, HD_cols = 16, HD_tiles = 43, HD_rock = 36, PG_block = 3, PG_floor = 1, PG_go = 40, PG_tax = 36, PG_rest = 7;
static int PG_group[8] = {32, 2, 37, 39, 32, 2, 37, 39}, PG_owner[MAXP] = {35, 38, 41, 33};
static void load_page(void) {
    char p[1100], ln[LINE]; char *f[8]; snprintf(p, sizeof p, "%s/page.pdl", g_app); FILE *fp = fopen(p, "r"); if (!fp) bad("cannot read page.pdl");
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n < 3) continue; int v = atoi(f[2]);
        if (!strcmp(f[0], "HEADER")) { if (!strcmp(f[1], "tile_px")) HD_px = v; else if (!strcmp(f[1], "atlas_cols")) HD_cols = v; else if (!strcmp(f[1], "atlas_tiles")) HD_tiles = v; else if (!strcmp(f[1], "rock_slot")) HD_rock = v; }
        else if (!strcmp(f[0], "PAGE") && !strcmp(f[1], "block")) PG_block = v;
        else if (!strcmp(f[0], "TILEMAP")) { int g;
            if (!strcmp(f[1], "floor")) PG_floor = v; else if (!strcmp(f[1], "go")) PG_go = v; else if (!strcmp(f[1], "tax")) PG_tax = v; else if (!strcmp(f[1], "rest")) PG_rest = v;
            else if (sscanf(f[1], "group%d", &g) == 1 && g >= 0 && g < 8) PG_group[g] = v; else if (sscanf(f[1], "owner_p%d", &g) == 1 && g >= 1 && g <= MAXP) PG_owner[g - 1] = v; } }
    fclose(fp);
    if (HD_px < 1 || HD_cols < 1 || HD_tiles < 1 || PG_block < 1 || PG_block > 8) bad("page.pdl: bad header or block size");
    int all[] = {PG_floor, PG_go, PG_tax, PG_rest}; for (int i = 0; i < 4; i++) if (all[i] < 0 || all[i] >= HD_tiles) bad("page.pdl: a tile index is outside the atlas");
    for (int i = 0; i < 8; i++) if (PG_group[i] < 0 || PG_group[i] >= HD_tiles) bad("page.pdl: a group tile is outside the atlas");
    for (int i = 0; i < MAXP; i++) if (PG_owner[i] < 0 || PG_owner[i] >= HD_tiles) bad("page.pdl: an owner tile is outside the atlas");
}
static void write_atomic(const char *dir, const char *name, const char *text) {
    char tp[1200], fp2[1200]; snprintf(tp, sizeof tp, "%s/%s.tmp", dir, name); snprintf(fp2, sizeof fp2, "%s/%s", dir, name);
    FILE *o = fopen(tp, "w"); if (!o) bad("cannot write into the desk folder"); fputs(text, o); fclose(o); if (rename(tp, fp2)) bad("cannot rename into the desk folder");
}
static void ring_xy(int idx, int n, int *bx, int *by) {
    int s = n - 1; if (idx <= s) { *bx = idx; *by = 0; } else if (idx <= 2 * s) { *bx = s; *by = idx - s; } else if (idx <= 3 * s) { *bx = s - (idx - 2 * s); *by = s; } else { *bx = 0; *by = s - (idx - 3 * s); }
}
static int v_page(int argc, char **argv) {
    if (argc < 1) bad("usage: page <desk_dir> [atlas.rgba]"); const char *dd = argv[0]; struct stat sb; if (stat(dd, &sb) || !S_ISDIR(sb.st_mode)) bad("the desk folder does not exist (make it first)");
    load_page(); if (nt % 4 || nt / 4 + 1 < 3) bad("the ring needs tiles = 4 x (side - 1) with side >= 3");
    int st, rc = replay(&st); if (rc) return 1; int n = nt / 4 + 1, W = n * PG_block, H = W; if (W > 64) bad("page too large");
    static int ct[64][64], cd[64][64]; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { ct[y][x] = PG_floor; cd[y][x] = 1; }
    int owned = 0;
    for (int i = 0; i < nt; i++) { int bx, by; ring_xy(i, n, &bx, &by); int tf = !strcmp(T[i].kind, "go") ? PG_go : !strcmp(T[i].kind, "tax") ? PG_tax : !strcmp(T[i].kind, "rest") ? PG_rest : PG_group[T[i].group & 7];
        for (int y = 0; y < PG_block; y++) for (int x = 0; x < PG_block; x++) { ct[by * PG_block + y][bx * PG_block + x] = tf; cd[by * PG_block + y][bx * PG_block + x] = 2; }
        if (st && owner[i] >= 0) { ct[by * PG_block + PG_block / 2][bx * PG_block + PG_block / 2] = PG_owner[owner[i]]; owned++; } }
    static char buf[64 * 64 * 12 + 512]; buf[0] = 0;
    ap(buf, sizeof buf, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=%d\natlas_tiles=%d\nrock_slot=%d\ncells\n", W, H, HD_px, HD_cols, HD_tiles, HD_rock);
    for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) ap(buf, sizeof buf, "%s%d,0,%d", x ? " " : "", ct[y][x], cd[y][x]); ap(buf, sizeof buf, "\n"); }
    write_atomic(dd, "cells.txt", buf);
    buf[0] = 0; for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) ap(buf, sizeof buf, "."); ap(buf, sizeof buf, "\n"); } write_atomic(dd, "map.txt", buf);
    char q[1200]; snprintf(q, sizeof q, "%s/events.txt", dd); if (stat(q, &sb)) write_atomic(dd, "events.txt", "");
    snprintf(q, sizeof q, "%s/skin.txt", dd); if (stat(q, &sb)) write_atomic(dd, "skin.txt", "rmmv\n");
    snprintf(q, sizeof q, "%s/extrusion.pdl", dd); if (stat(q, &sb)) write_atomic(dd, "extrusion.pdl", "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | deskopoly\nMETA         | desk_id            | board\nMETA         | note               | deskopoly board page, drawn from board.pdl + the ledger by `deskopoly page`\n");
    snprintf(q, sizeof q, "%s/cells.rgba", dd);
    if (argc > 1 && stat(q, &sb)) { FILE *a = fopen(argv[1], "rb"); if (!a) bad("cannot read the atlas"); FILE *o = fopen(q, "wb"); if (!o) bad("cannot write the atlas"); char b[8192]; size_t k; while ((k = fread(b, 1, sizeof b, a)) > 0) fwrite(b, 1, k, o); fclose(a); fclose(o); }
    printf("PAGE %s %dx%d tiles=%d owned=%d\n", dd, W, H, nt, owned); return 0;
}
/* preview: a FLAT top-down picture of a desk (cells.txt + cells.rgba), tile by tile, no depth, no engine. Raw RGBA, for raw_to_png. It is not the engine's render. */
static int v_preview(int argc, char **argv) {
    if (argc < 2) bad("usage: preview <desk_dir> <out.raw>"); char p[1200], ln[16384]; snprintf(p, sizeof p, "%s/cells.txt", argv[0]); FILE *fp = fopen(p, "r"); if (!fp) bad("no cells.txt (run page first)");
    int W = 0, H = 0, px = 24, cols = 16; static int ct[64][64]; int y = -1;
    while (fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "tile_px=", 8)) px = atoi(ln + 8); else if (!strncmp(ln, "atlas_cols=", 11)) cols = atoi(ln + 11);
        else if (!strncmp(ln, "cells", 5)) y = 0; else if (y >= 0 && y < H && y < 64) { int x = 0; for (char *t = strtok(ln, " \n"); t && x < W && x < 64; t = strtok(NULL, " \n"), x++) ct[y][x] = atoi(t); y++; } }
    fclose(fp); if (W < 1 || H < 1 || W > 64 || H > 64 || y != H) bad("cells.txt is not a complete page");
    snprintf(p, sizeof p, "%s/cells.rgba", argv[0]); fp = fopen(p, "rb"); if (!fp) bad("no cells.rgba atlas"); fseek(fp, 0, SEEK_END); long asz = ftell(fp); rewind(fp);
    unsigned char *at = malloc((size_t)asz); if (!at || fread(at, 1, (size_t)asz, fp) != (size_t)asz) bad("cannot read the atlas"); fclose(fp);
    int aw = cols * px; if (asz % (aw * 4)) bad("atlas size does not match atlas_cols x tile_px"); int ah = (int)(asz / (aw * 4));
    size_t ow = (size_t)W * px, oh = (size_t)H * px; unsigned char *o = calloc(ow * oh, 4); if (!o) bad("out of memory");
    for (int cy = 0; cy < H; cy++) for (int cx = 0; cx < W; cx++) { int t = ct[cy][cx], sx = (t % cols) * px, sy = (t / cols) * px; if (sy + px > ah) bad("a tile index is outside the atlas");
        for (int r = 0; r < px; r++) memcpy(o + (((size_t)cy * px + r) * ow + (size_t)cx * px) * 4, at + ((size_t)(sy + r) * aw + sx) * 4, (size_t)px * 4); }
    FILE *w = fopen(argv[1], "wb"); if (!w) bad("cannot write the raw file"); fwrite(o, 4, ow * oh, w); fclose(w); free(at); free(o);
    printf("PREVIEW %zux%zu\n", ow, oh); return 0;
}

/* --- seats, tick, ui (A1/A2 of PROPOSAL-PLAY-MODE-SEATS-AND-GAME-SIDEBARS-2026-11-07): who drives a seat is HARD STATE (a `seat` row in the ledger), so a replay knows who was in charge of each move. --- */
static int v_seat(int argc, char **argv) {
    if (argc < 2) bad("usage: seat <player id> <human|pet>");
    int st, rc = need_game(&st); if (rc) return rc; if (over) bad("the game is over");
    int pl = pidx(argv[0]); if (pl < 0 || pl >= np_game) bad("no such player in this game");
    if (strcmp(argv[1], "human") && strcmp(argv[1], "pet")) bad("a driver is human or pet");
    if (human[pl] == !strcmp(argv[1], "human")) { fprintf(stderr, "deskopoly: refused: %s is already %s\n", argv[0], argv[1]); return 2; }
    human[pl] = !strcmp(argv[1], "human"); event("bank", "seat", PL[pl].id, 0, argv[1]); printf("seat %s %s\n", PL[pl].id, argv[1]); return 0;
}
/* one step for the current seat. 0 = one event appended, 1 = waiting for a human, 2 = game over, -1 = stuck. A human seat never moves by itself: its verbs (roll, buy, skip, end_turn) are the same ones a pet's step performs. */
static int tick_once(char *msg, size_t cap) {
    if (over) { snprintf(msg, cap, "over"); return 2; }
    int pl = cur; char res[LINE]; res[0] = 0;
    if (human[pl]) { snprintf(msg, cap, "waiting %s %s human", PL[pl].id, PL[pl].name); return 1; }
    if (!rolled) { char t[32]; int d1 = die(seed, nrolls, 0), d2 = die(seed, nrolls, 1); snprintf(t, sizeof t, "%d,%d", d1, d2); do_roll(pl, d1, d2, res, sizeof res); event(PL[pl].id, "roll", t, pos[pl], res); snprintf(msg, cap, "%s roll %s: %s", PL[pl].id, t, res); return 0; }
    if (pending >= 0) {
        int tile = pending, want = wants_buy(pl); const char *vb = "skip";
        if (want) { const char *err = do_buy(pl, res, sizeof res); if (err) { dec(pl, "buy", tile, "blocked"); res[0] = 0; do_skip(res, sizeof res); event(PL[pl].id, "skip", "-", pos[pl], res); dec(pl, "skip", tile, "ok"); }
                    else { event(PL[pl].id, "buy", "-", pos[pl], res); dec(pl, "buy", tile, "ok"); vb = "buy"; } }
        else { do_skip(res, sizeof res); event(PL[pl].id, "skip", "-", pos[pl], res); dec(pl, "skip", tile, "ok"); }
        snprintf(msg, cap, "%s %s: %s", PL[pl].id, vb, res); return 0;
    }
    const char *err = do_end(res, sizeof res); if (err) { snprintf(msg, cap, "stuck: %s", err); return -1; }
    event(PL[pl].id, "end_turn", "-", pos[pl], res); snprintf(msg, cap, "%s end_turn: %s", PL[pl].id, res); return 0;
}
static int v_tick(void) {
    int st, rc = need_game(&st); if (rc) return rc; char m[LINE]; int r = tick_once(m, sizeof m); printf("TICK %s\n", m); return r < 0 ? 1 : 0;
}
static int v_autoplay(void) {
    int st, rc = need_game(&st); if (rc) return rc; char m[LINE]; int r = 0;
    for (long guard = 0; guard < 200000; guard++) { r = tick_once(m, sizeof m); if (r != 0) break; }
    if (r < 0) { fprintf(stderr, "deskopoly: autoplay %s\n", m); return 1; }
    if (r == 1) { printf("AUTOPLAY waiting events=%d %s\n", nev, m); return 0; }
    if (!over) { fprintf(stderr, "deskopoly: autoplay guard hit before the game ended\n"); return 1; }
    int best = -1; for (int i = 0; i < np_game; i++) if (!out[i] && (best < 0 || net(i) > net(best))) best = i;
    printf("AUTOPLAY events=%d rounds=%d over=1 winner=%s\n", nev, round_, best >= 0 ? PL[best].name : "none"); return 0;
}
/* ui: the window's feed (key=value, atomic), a PROJECTION of the replayed ledger. A show_* key is "1" when the control may be offered and EMPTY when not (the house convention: a "0" string would count as visible). */
static int v_ui(void) {
    int st, rc = need_game(&st); if (rc) return rc; static char b[8192]; b[0] = 0; int pl = cur, ht = !over && human[pl] && !out[pl];
    ap(b, sizeof b, "game=deskopoly\nn_players=%d\nround=%d\nover=%d\ncurrent=%s\ncurrent_name=%s\ncurrent_driver=%s\n", np_game, round_, over, PL[pl].id, PL[pl].name, human[pl] ? "human" : "pet");
    ap(b, sizeof b, "human_turn=%s\nshow_roll=%s\nshow_buy=%s\nshow_skip=%s\nshow_end=%s\n", ht ? "1" : "", ht && !rolled ? "1" : "", ht && pending >= 0 && cash[pl] >= T[pending].price ? "1" : "", ht && pending >= 0 ? "1" : "", ht && rolled && pending < 0 ? "1" : "");
    ap(b, sizeof b, "pending_tile=%s\npending_price=%d\n", pending >= 0 ? T[pending].name : "", pending >= 0 ? T[pending].price : 0);
    for (int i = 0; i < np_game; i++) { int props = 0; for (int k = 0; k < nt; k++) if (owner[k] == i) props++;
        ap(b, sizeof b, "%s_name=%s\n%s_driver=%s\n%s_cash=%d\n%s_pos=%d\n%s_out=%d\n%s_props=%d\n%s_turn=%s\n", PL[i].id, PL[i].name, PL[i].id, human[i] ? "human" : "pet", PL[i].id, cash[i], PL[i].id, pos[i], PL[i].id, out[i], PL[i].id, props, PL[i].id, i == pl && !over ? "1" : ""); }
    for (int i = 0; i < 5; i++) ap(b, sizeof b, "log_%d=%s\n", i, i < nlast ? lastlog[nlast - 1 - i] : "");
    write_atomic(g_state, "ui.txt", b); printf("UI %s/ui.txt current=%s human_turn=%s\n", g_state, PL[pl].id, ht ? "1" : "0"); return 0;
}

/* ledger_hash [skip_verb]: a read-only digest of the ledger rows (FNV-1a over each row and its newline), optionally leaving out every row whose verb is skip_verb (for example seat: a control change, not a game move).
 * It lets a case say "these two games made the same moves" exactly, without capturing a whole ledger. */
static int v_ledger_hash(int argc, char **argv) {
    char p[1100], ln[LINE]; ledger_path(p, sizeof p); FILE *fp = fopen(p, "r"); if (!fp) bad("no ledger here"); const char *skip = argc > 0 ? argv[0] : NULL;
    uint64_t h = 1469598103934665603ULL; long rows = 0;
    while (fgets(ln, sizeof ln, fp)) { char raw[LINE]; snprintf(raw, sizeof raw, "%s", ln); char *f[10]; int n = split(ln, f, 10); if (n < 7 || strcmp(f[0], "EVENT")) continue; if (skip && !strcmp(f[3], skip)) continue;
        for (const char *c = raw; *c; c++) { h ^= (unsigned char)*c; h *= 1099511628211ULL; } rows++; }
    fclose(fp); printf("LEDGER rows=%ld hash=%016llx%s%s\n", rows, (unsigned long long)h, skip ? " without=" : "", skip ? skip : ""); return 0;
}

/* audit: the no-ghost-data check. state.pdl declares every file the state folder may hold; anything else is a GHOST (a file that could carry a fact the ledger does not have). Read-only. */
static int v_audit(void) {
    char p[1100], ln[LINE]; char *f[8]; FILE *fp; char dn[16][48], dk[16][12]; int nd = 0; char vn[48][24]; int nv = 0;
    snprintf(p, sizeof p, "%s/verbs.pdl", g_app); fp = fopen(p, "r"); if (!fp) bad("cannot read verbs.pdl");
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n >= 2 && !strcmp(f[0], "VERB") && nv < 48) snprintf(vn[nv++], 24, "%s", f[1]); } fclose(fp);
    snprintf(p, sizeof p, "%s/state.pdl", g_app); fp = fopen(p, "r"); if (!fp) bad("cannot read state.pdl");
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n < 5 || strcmp(f[0], "FILE")) continue; if (nd >= 16) bad("state.pdl: too many FILE rows");
        if (strcmp(f[2], "LEDGER") && strcmp(f[2], "PROJECTION") && strcmp(f[2], "CONFIG") && strcmp(f[2], "PREFERENCE")) bad("state.pdl: a kind is LEDGER, PROJECTION, CONFIG or PREFERENCE");
        if (!strcmp(f[2], "PROJECTION")) { int ok = 0; for (int i = 0; i < nv; i++) if (!strcmp(vn[i], f[4])) ok = 1; if (!ok) { fprintf(stderr, "deskopoly: state.pdl: projection %s names a regenerating verb (%s) that is not a VERB in verbs.pdl\n", f[1], f[4]); return 2; } }
        for (int i = 0; i < nd; i++) if (!strcmp(dn[i], f[1])) bad("state.pdl: a file is declared twice");
        snprintf(dn[nd], 48, "%s", f[1]); snprintf(dk[nd], 12, "%s", f[2]); nd++; }
    fclose(fp);
    DIR *d = opendir(g_state); if (!d) bad("the state folder does not exist"); struct dirent *e; int ghosts = 0, present = 0;
    while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; int ok = 0; for (int i = 0; i < nd; i++) if (!strcmp(dn[i], e->d_name)) ok = 1;
        if (ok) present++; else { printf("GHOST %s\n", e->d_name); ghosts++; } }
    closedir(d);
    if (ghosts) { printf("AUDIT ghosts=%d declared=%d\n", ghosts, nd); return 1; }
    printf("AUDIT ok files=%d declared=%d\n", present, nd); return 0;
}

/* which ruleset does this call run under? `lobby <ruleset>` names it; otherwise the first ledger row decides (a `lobby` row names it, anything else is schema 1 = mini). */
static void pick_ruleset(const char *verb, int argc, char **argv) {
    if (!strcmp(verb, "lobby")) {
        const char *nm = argc > 0 ? argv[0] : "classic"; if (!rs_name_ok(nm)) bad("a ruleset name is lower-case letters, digits, _ and - (1..24)");
        if (!strcmp(nm, "mini")) bad("mini is a schema 1 ruleset: start it with `new`");
        snprintf(g_rs, sizeof g_rs, "%s", nm); g_schema = 2; return;
    }
    char p[1100], ln[LINE]; char *f[10]; snprintf(p, sizeof p, "%s/ledger.txt", g_state); FILE *fp = fopen(p, "r"); if (!fp) return;
    if (fgets(ln, sizeof ln, fp)) { int n = split(ln, f, 10); if (n >= 7 && !strcmp(f[0], "EVENT") && !strcmp(f[3], "lobby")) {
        char nm[40] = ""; if (sscanf(f[6], "ruleset=%39[^,]", nm) != 1 || !rs_name_ok(nm) || !strcmp(nm, "mini")) { fclose(fp); bad("the lobby row names no usable ruleset"); }
        snprintf(g_rs, sizeof g_rs, "%s", nm); g_schema = 2; } }
    fclose(fp);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: deskopoly <app_dir> <state_dir> <verb> [args]\n"); return 2; }
    snprintf(g_app, sizeof g_app, "%s", argv[1]); snprintf(g_state, sizeof g_state, "%s", argv[2]); const char *v = argv[3];
    pick_ruleset(v, argc - 4, argv + 4);
    load_app(); reset_state(); seed = R_seed;
    struct stat sb; if (stat(g_state, &sb) || !S_ISDIR(sb.st_mode)) bad("the state folder does not exist (make it first)");
    if (!strcmp(v, "new")) { if (g_schema == 2) bad("this folder already holds a classic game"); return v_new(argc - 4, argv + 4); }
    if (!strcmp(v, "lobby")) return v_lobby(argc - 4, argv + 4);
    if (!strcmp(v, "join")) return v_join(argc - 4, argv + 4);
    if (!strcmp(v, "leave")) return v_leave(argc - 4, argv + 4);
    if (!strcmp(v, "start")) return v_start(argc - 4, argv + 4);
    if (!strcmp(v, "order")) return v_order();
    if (!strcmp(v, "help")) return v_help(argc - 4, argv + 4);
    if (g_schema == 2 && (!strcmp(v, "roll") || !strcmp(v, "buy") || !strcmp(v, "skip") || !strcmp(v, "end_turn") || !strcmp(v, "bankrupt"))) return v_act2(v, argc - 4, argv + 4);
    if (g_schema == 2 && !strcmp(v, "autoplay")) return v_autoplay2();
    if (g_schema == 2 && !strcmp(v, "tick")) return v_tick2();
    if (g_schema == 2 && (!strcmp(v, "ui") || !strcmp(v, "mechanics") || !strcmp(v, "page") || !strcmp(v, "preview"))) { fprintf(stderr, "deskopoly: refused: %s is not built for classic games yet (milestones M10 and M11)\n", v); return 2; }
    if (!strcmp(v, "roll")) return v_roll();
    if (!strcmp(v, "buy") || !strcmp(v, "skip") || !strcmp(v, "end_turn")) return act_simple(v);
    if (!strcmp(v, "autoplay")) return v_autoplay();
    if (!strcmp(v, "tick")) return v_tick();
    if (!strcmp(v, "seat")) return v_seat(argc - 4, argv + 4);
    if (!strcmp(v, "ui")) return v_ui();
    if (!strcmp(v, "audit")) return v_audit();
    if (!strcmp(v, "ledger_hash")) return v_ledger_hash(argc - 4, argv + 4);
    if (!strcmp(v, "status")) return v_status();
    if (!strcmp(v, "replay")) return v_replay();
    if (!strcmp(v, "check")) return v_check();
    if (!strcmp(v, "mechanics")) return v_mechanics();
    if (!strcmp(v, "page")) return v_page(argc - 4, argv + 4);
    if (!strcmp(v, "preview")) return v_preview(argc - 4, argv + 4);
    fprintf(stderr, "deskopoly: unknown verb %s\n", v); return 2;
}
