/* game_core - the ONE shared engine, driver and button op of every game made by `game_new_op` (2026-11-09; BOOK 16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md).
 *
 *   game_core <app_dir> <state_dir> <verb> [args...]        the engine: ONE process per verb, no resident process. <app_dir> = @.apps/<game> (game.pdl, rulesets/, state.pdl, verbs.pdl),
 *                                                           <state_dir> = the folder of the ledger (the editor uses <editor>/state/games/<game>/game).
 *   game_core driver <house_root> <book_app_dir> <editor_app_dir>   what the editor's Play starts (game.pdl `GAME | driver`): one instance (flock), ticks only while the editor's switch running=1,
 *                                                           publishes the feed <editor>/state/game_ui.txt, takes button presses from <games_dir>/action.txt, ends when the editor is gone.
 *   game_core ctl <command> <games_dir> [ignored...]       what a game-bar button runs: appends ONE line to <games_dir>/action.txt (extra arguments are ignored on purpose: the renderer appends two).
 *
 * WHY ONE SHARED BINARY (decision D61): the per-game C is zero. A game made by the generator is DATA (game.pdl, rulesets/default/board.pdl + rules.pdl, state.pdl, verbs.pdl) and this binary. A fix here reaches every
 * game; the cases `game_new` (and every generated `<game>` case) are the regression gate. A game that needs real rules of its own copies the idea of this file into its own app (as deskopoly did), it does not edit this one.
 *
 * THREE KINDS (game.pdl `GAME | kind | ledger|realtime|editor`, recorded in the first ledger row and re-checked by every replay):
 *   ledger    turn based. Verbs: new join start roll tick seat autoplay ckpt; stub rule: a track of TRACK length, seeded dice, JUMP rows (chutes and ladders are exactly these), first to the end wins.
 *   realtime  inputs only: a row per INPUT (verb input, round = the logical TICK) and a `ckpt` row (state hash) every `ckpt_every` ticks. Ticks themselves are never rows; replay simulates between rows with the
 *             SAME step function, and a ckpt whose hash differs refuses the ledger. The step function is a STUB (x += dir per tick, first to TRACK length wins): the tick interpreter of a real game is NOT here.
 *             Pause = the driver stops asking for ticks; it writes no row.
 *   editor    a data document: ledger rows add / set / del (the EDIT rows) and publish (a rule stub: rules.pdl REQUIRE rows name ids the document must have). `export` writes the projection doc.txt.
 *
 * LEDGER ROW (the real Teropoly shape, spec DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC 5.2, so the data exporters and the master ledger read it): EVENT|round|actor|verb|target|at|result
 *   Every verb computes the row by the SAME function (apply) that replay uses. Replay recomputes round, actor, target, at and result of every row and REFUSES (exit 1) a ledger that differs: a hand-edited row,
 *   a deleted or inserted row, a changed seed, a rules file edited after the game began (rules hash in the first row), a ckpt row with the wrong state hash. A refused action appends nothing.
 *   RNG: die(seed, idx, k) = mix(seed*1000003 + idx*16 + k) % 6 + 1 with the splitmix64 mix() of deskopoly.c: no stored random state, the index is derived from the rows.
 * EXIT CODES: 0 ok, 1 the ledger is not reproduced by replay (or an audit/check failed), 2 usage or a refused action.
 * FILES (state.pdl declares them; `audit` fails on any other file in the state folder): ledger.txt LEDGER (cap `RULE | max_rows`, default 5000 rows = under 400 KB), ui.txt PROJECTION (verb ui), doc.txt PROJECTION (editor, verb export).
 * COST: the engine is a plain process per verb; the driver replays the ledger once and then keeps the state in memory (a stat() per loop notices an outside writer). Idle driver: a 250 ms nap; live: 100 ms. It starts NO process.
 * MUTANTS for the harness (compiled with -D, never installed): GC_MUT_NO_VERIFY (replay trusts the recorded row), GC_MUT_NO_GHOST (audit ignores undeclared files). The case game_new proves each is caught.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/game_core.+x game_core.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/file.h>

#define P 4096
#define CORE_VERSION 1
#define MAXPL 8
#define DOCMAX 64
#define MAXJ 64
#define ROWMAX 1000

enum { K_LEDGER = 1, K_REALTIME = 2, K_EDITOR = 3 };
enum { PH_NONE = 0, PH_LOBBY = 1, PH_PLAY = 2, PH_OVER = 3 };

static FILE *OUT, *ERRF;                     /* stdout/stderr for the CLI; memory streams when the driver calls a verb in-process */
static char APP[P], SD[P], LEDGER[P], TITLE[96] = "Game", RS[40] = "default", MSG[400];
static int KIND = K_LEDGER;
static long R_maxp = 4, R_maxrows = 5000, R_ckpt = 10, R_seed = 7, R_maxticks = 4000, R_docmax = 40;
static int LENGTH = 30, NJ; static int JF[MAXJ], JT[MAXJ];
static char REQ[8][32]; static int NREQ;
static uint64_t RULES_HASH;

typedef struct { long round; char actor[32], verb[16], target[200], at[48], res[300]; } Row;
static struct {
    int nrows, phase, np, cur, winner; long seed, round, nrolls, tick;
    char name[MAXPL][24]; int human[MAXPL], pos[MAXPL], dir[MAXPL];
    int ndoc, published; char did[DOCMAX][32], dtx[DOCMAX][200]; char pubhash[20]; long nedits;
} S;
static long KNOWN_SIZE;                      /* bytes of the ledger as this process last read or wrote it */

static int bad(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsnprintf(MSG, sizeof MSG, fmt, ap); va_end(ap); return 1; }
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const unsigned char *s = p; for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; } return h; }
#define FNV0 14695981039346656037ULL
static void hx(uint64_t h, char *o) { snprintf(o, 20, "%016llx", (unsigned long long)h); }
static uint64_t mix(uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
static int die(long sd, long idx, int k) { return (int)(mix((uint64_t)sd * 1000003ULL + (uint64_t)idx * 16 + (uint64_t)k) % 6) + 1; }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int rd(const char *p, char *b, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return -1; } size_t n = fread(b, 1, cap - 1, f); b[n] = 0; fclose(f); return (int)n; }
static void mkdir_p(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *c = t + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(t, 0755); *c = '/'; } mkdir(t, 0755); }
static int good_word(const char *s, size_t max, int allow_dash) { size_t n = strlen(s); if (n < 1 || n > max) return 0; for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (allow_dash && (c == '_' || c == '-')))) return 0; } return 1; }

/* ---------- data: game.pdl, rules ---------- */
static int split_row(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int load_config(void) {
    char p[P], ln[1024]; FILE *f; char *fl[10]; int n;
    snprintf(p, sizeof p, "%s/game.pdl", APP); f = fopen(p, "r"); if (!f) return bad("no game.pdl in %s", APP);
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; n = split_row(ln, fl, 10); if (n < 3 || strcmp(fl[0], "GAME")) continue;
        if (!strcmp(fl[1], "kind")) KIND = !strcmp(fl[2], "ledger") ? K_LEDGER : !strcmp(fl[2], "realtime") ? K_REALTIME : !strcmp(fl[2], "editor") ? K_EDITOR : 0;
        else if (!strcmp(fl[1], "title")) snprintf(TITLE, sizeof TITLE, "%s", fl[2]);
        else if (!strcmp(fl[1], "ruleset")) snprintf(RS, sizeof RS, "%s", fl[2]); }
    fclose(f); if (!KIND) return bad("game.pdl: GAME | kind must be ledger, realtime or editor");
    if (!good_word(RS, 24, 1)) return bad("game.pdl: bad ruleset name");
    RULES_HASH = FNV0; NJ = 0; NREQ = 0;
    const char *files[2] = { "board.pdl", "rules.pdl" };
    for (int k = 0; k < 2; k++) { snprintf(p, sizeof p, "%s/rulesets/%s/%s", APP, RS, files[k]); f = fopen(p, "r"); if (!f) return bad("missing %s", p);
        RULES_HASH = fnv(RULES_HASH, files[k], strlen(files[k]));
        while (fgets(ln, sizeof ln, f)) { RULES_HASH = fnv(RULES_HASH, ln, strlen(ln)); if (ln[0] == '#') continue; char cp[1024]; snprintf(cp, sizeof cp, "%s", ln); n = split_row(cp, fl, 10); if (n < 2) continue;
            if (!strcmp(fl[0], "TRACK") && n >= 3 && !strcmp(fl[1], "length")) { LENGTH = atoi(fl[2]); if (LENGTH < 3 || LENGTH > 1000) { fclose(f); return bad("board.pdl: TRACK length is 3 to 1000"); } }
            else if (!strcmp(fl[0], "JUMP") && n >= 3) { if (NJ >= MAXJ) { fclose(f); return bad("board.pdl: at most %d JUMP rows", MAXJ); } JF[NJ] = atoi(fl[1]); JT[NJ] = atoi(fl[2]); NJ++; }
            else if (!strcmp(fl[0], "REQUIRE") && NREQ < 8) snprintf(REQ[NREQ++], 32, "%s", fl[1]);
            else if (!strcmp(fl[0], "RULE") && n >= 3) { long v = atol(fl[2]);
                if (!strcmp(fl[1], "max_players")) R_maxp = v; else if (!strcmp(fl[1], "max_rows")) R_maxrows = v; else if (!strcmp(fl[1], "ckpt_every")) R_ckpt = v;
                else if (!strcmp(fl[1], "default_seed")) R_seed = v; else if (!strcmp(fl[1], "max_ticks")) R_maxticks = v; else if (!strcmp(fl[1], "max_doc_rows")) R_docmax = v; } }
        fclose(f); }
    if (R_maxp < 2 || R_maxp > MAXPL) return bad("rules.pdl: max_players is 2 to %d", MAXPL);
    if (R_ckpt < 1 || R_ckpt > 1000) return bad("rules.pdl: ckpt_every is 1 to 1000");
    if (R_maxrows < 10 || R_maxrows > 100000) return bad("rules.pdl: max_rows is 10 to 100000");
    if (R_docmax < 1 || R_docmax > DOCMAX) return bad("rules.pdl: max_doc_rows is 1 to %d", DOCMAX);
    for (int i = 0; i < NJ; i++) if (JF[i] < 1 || JF[i] >= LENGTH || JT[i] < 0 || JT[i] > LENGTH || JF[i] == JT[i]) return bad("board.pdl: JUMP %d -> %d is outside the track", JF[i], JT[i]);
    return 0;
}
static int jump_at(int sq) { for (int i = 0; i < NJ; i++) if (JF[i] == sq) return JT[i]; return -1; }

/* ---------- state hash (what a ckpt row pins) ---------- */
static uint64_t state_hash(void) {
    char b[4096]; int n = 0; n += snprintf(b + n, sizeof b - (size_t)n, "k=%d;seed=%ld;phase=%d;cur=%d;round=%ld;rolls=%ld;tick=%ld;win=%d;", KIND, S.seed, S.phase, S.cur, S.round, S.nrolls, S.tick, S.winner);
    for (int i = 0; i < S.np; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%s,%d,%d,%d;", S.name[i], S.human[i], S.pos[i], S.dir[i]);
    for (int i = 0; i < S.ndoc && n < 3500; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%s=%s;", S.did[i], S.dtx[i]);
    n += snprintf(b + n, sizeof b - (size_t)n, "pub=%d,%s", S.published, S.pubhash); return fnv(FNV0, b, strlen(b));
}
static uint64_t doc_hash(void) { uint64_t h = FNV0; for (int i = 0; i < S.ndoc; i++) { h = fnv(h, S.did[i], strlen(S.did[i])); h = fnv(h, "=", 1); h = fnv(h, S.dtx[i], strlen(S.dtx[i])); h = fnv(h, "\n", 1); } return h; }

/* ---------- the stub step function of kind realtime ---------- */
static void step_tick(void) {
    for (int i = 0; i < S.np; i++) { S.pos[i] += S.dir[i]; if (S.pos[i] < 0) S.pos[i] = 0; if (S.pos[i] > LENGTH) S.pos[i] = LENGTH; }
    S.tick++; int best = -1; for (int i = 0; i < S.np; i++) if (S.pos[i] >= LENGTH && (best < 0 || S.pos[i] > S.pos[best])) best = i;
    if (best >= 0) { S.winner = best; S.phase = PH_OVER; }
}
static void sim_to(long t) { while (S.phase == PH_PLAY && S.tick < t) step_tick(); }

/* ---------- apply: the ONE function that makes (live) and checks (replay) a row ---------- */
static int pidx(const char *a) { if (a[0] != 'p' || !a[1]) return -1; int i = atoi(a + 1) - 1; return (i >= 0 && i < S.np && !strchr(a + 1, '.')) ? i : -1; }
static int next_seat(void) { S.cur++; if (S.cur >= S.np) { S.cur = 0; S.round++; } return 0; }
/* in: round (-1 = now), actor ("" = whoever the rule says), verb, target; arg2 = a second argument (a name's driver, an editor text, a direction). out: the full row. */
static int apply(const Row *in, const char *arg2, Row *out) {
    memset(out, 0, sizeof *out); snprintf(out->verb, sizeof out->verb, "%s", in->verb); const char *v = in->verb; char hs[20];
    if (!strcmp(v, "new")) {
        if (S.phase != PH_NONE || S.nrows) return bad("a game already exists in this folder");
        long sd = atol(in->target[0] && strcmp(in->target, "-") ? in->target : "0"); if (!in->target[0] || !strcmp(in->target, "-")) sd = R_seed; hx(RULES_HASH, hs);
        S.seed = sd; S.phase = (KIND == K_EDITOR) ? PH_PLAY : PH_LOBBY; S.round = 1; S.cur = 0; S.winner = -1;
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "%ld", sd); snprintf(out->at, sizeof out->at, "0");
        snprintf(out->res, sizeof out->res, "ruleset=%s,schema=1,core=%d,kind=%s,rules=%s,max=%ld", RS, CORE_VERSION, KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", hs, R_maxp);
        out->round = 0; return 0; }
    if (S.phase == PH_NONE) return bad("no game yet: run `new` first");
    if (!strcmp(v, "ckpt")) {
        if (KIND == K_REALTIME) { long t = in->round >= 0 ? in->round : S.tick; sim_to(t); if (S.tick != t) return bad("ckpt tick %ld is not the tick the game reached (%ld)", t, S.tick); out->round = t; }
        else out->round = KIND == K_EDITOR ? S.nedits : S.round;
        hx(state_hash(), hs); snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "%ld", KIND == K_REALTIME ? S.tick : (long)S.nrows);
        snprintf(out->res, sizeof out->res, "hash=%s%s", hs, S.phase == PH_OVER ? ";over" : ""); return 0; }
    if (KIND == K_EDITOR) {
        char tx[200]; snprintf(tx, sizeof tx, "%s", arg2 ? arg2 : ""); const char *who = in->actor[0] ? in->actor : "author";
        if (!strcmp(v, "add") || !strcmp(v, "set") || !strcmp(v, "del")) {
            int ix = -1; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], in->target)) ix = i;
            if (!good_word(in->target, 24, 1)) return bad("an id is 1 to 24 letters, digits, - or _");
            if (!strcmp(v, "add") && ix >= 0) return bad("id %s already exists (use set)", in->target);
            if (strcmp(v, "add") && ix < 0) return bad("no id %s in the document", in->target);
            if (strcmp(v, "del")) { if (!tx[0] || strlen(tx) > 160 || strpbrk(tx, "|\n\r")) return bad("the text is 1 to 160 characters with no bar and no line break"); }
            if (!strcmp(v, "add") && S.ndoc >= R_docmax) return bad("the document is full (max_doc_rows %ld)", R_docmax);
            if (!good_word(who, 24, 1)) return bad("bad author name");
            if (!strcmp(v, "add")) { ix = S.ndoc++; snprintf(S.did[ix], 32, "%s", in->target); snprintf(S.dtx[ix], 200, "%s", tx); }
            else if (!strcmp(v, "set")) snprintf(S.dtx[ix], 200, "%s", tx);
            else { for (int i = ix; i < S.ndoc - 1; i++) { memcpy(S.did[i], S.did[i + 1], 32); memcpy(S.dtx[i], S.dtx[i + 1], 200); } S.ndoc--; }
            S.nedits++; out->round = S.nedits; snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "%d", S.ndoc);
            snprintf(out->res, sizeof out->res, "%s", strcmp(v, "del") ? tx : "removed"); return 0; }
        if (!strcmp(v, "publish")) {
            for (int r = 0; r < NREQ; r++) { int ok = 0; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], REQ[r])) ok = 1; if (!ok) return bad("publish refused: the document has no row %s (rules.pdl REQUIRE)", REQ[r]); }
            hx(doc_hash(), hs); S.published = 1; snprintf(S.pubhash, sizeof S.pubhash, "%s", hs); S.nedits++;
            out->round = S.nedits; snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "%d", S.ndoc); snprintf(out->res, sizeof out->res, "doc=%s", hs); return 0; }
        return bad("unknown verb %s for an editor game", v); }
    /* ledger + realtime from here */
    if (!strcmp(v, "join")) {
        if (S.phase != PH_LOBBY) return bad("join is only legal in the lobby"); if (S.np >= R_maxp) return bad("the table is full (%ld players)", R_maxp);
        if (!good_word(in->target, 16, 0)) return bad("a name is 1 to 16 letters or digits"); const char *dr = arg2 && *arg2 ? arg2 : "pet"; if (strcmp(dr, "pet") && strcmp(dr, "human")) return bad("a driver is human or pet");
        for (int i = 0; i < S.np; i++) if (!strcmp(S.name[i], in->target)) return bad("the name %s is taken", in->target);
        int i = S.np++; snprintf(S.name[i], 24, "%s", in->target); S.human[i] = !strcmp(dr, "human"); S.pos[i] = 0; S.dir[i] = 0;
        snprintf(out->actor, sizeof out->actor, "p%d", i + 1); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "driver=%s", dr); out->round = 0; return 0; }
    if (!strcmp(v, "start")) {
        if (S.phase != PH_LOBBY) return bad("start is only legal in the lobby"); if (S.np < 2) return bad("a game needs 2 to %ld players", R_maxp);
        S.phase = PH_PLAY; S.cur = 0; S.round = 1; S.tick = 0; hx(RULES_HASH, hs);
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "seed=%ld,players=%d,rules=%s", S.seed, S.np, hs); out->round = 0; return 0; }
    if (!strcmp(v, "seat")) {
        int i = pidx(in->target); if (i < 0) return bad("seat: no player %s", in->target); const char *dr = arg2 && *arg2 ? arg2 : ""; if (strcmp(dr, "pet") && strcmp(dr, "human")) return bad("seat: human or pet");
        if (S.phase == PH_OVER) return bad("the game is over"); S.human[i] = !strcmp(dr, "human");
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "driver=%s", dr); out->round = KIND == K_REALTIME ? S.tick : S.round; return 0; }
    if (KIND == K_LEDGER && !strcmp(v, "roll")) {
        if (S.phase == PH_OVER) return bad("the game is over"); if (S.phase != PH_PLAY) return bad("the game has not started");
        char who[8]; snprintf(who, sizeof who, "p%d", S.cur + 1); if (in->actor[0] && strcmp(in->actor, who)) return bad("it is %s's turn, not %s", who, in->actor);
        int d = die(S.seed, S.nrolls, 0); S.nrolls++; long rd0 = S.round; int np = S.pos[S.cur] + d; char res[120]; int n = snprintf(res, sizeof res, "to=%d", np > LENGTH ? LENGTH : np);
        if (np < LENGTH) { int j = jump_at(np); if (j >= 0) { n += snprintf(res + n, sizeof res - (size_t)n, ";jump=%d-%d", np, j); np = j; } }
        if (np >= LENGTH) { np = LENGTH; snprintf(res + n, sizeof res - (size_t)n, ";win"); S.winner = S.cur; S.phase = PH_OVER; }
        S.pos[S.cur] = np; if (S.phase == PH_PLAY) next_seat();
        snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "%d", d); snprintf(out->at, sizeof out->at, "%d", np); snprintf(out->res, sizeof out->res, "%s", res); out->round = rd0; return 0; }
    if (KIND == K_REALTIME && !strcmp(v, "input")) {
        if (S.phase == PH_OVER) return bad("the game is over"); if (S.phase != PH_PLAY) return bad("the game has not started");
        int i = pidx(in->actor); if (i < 0) return bad("input: no player %s", in->actor);
        int dir = !strcmp(in->target, "left") ? -1 : !strcmp(in->target, "right") ? 1 : !strcmp(in->target, "stay") ? 0 : 9; if (dir == 9) return bad("input: left, right or stay");
        long t = in->round >= 0 ? in->round : S.tick; if (t < S.tick) return bad("input at tick %ld is in the past (the game is at tick %ld)", t, S.tick); if (t > S.tick + 100000) return bad("input: tick too far ahead");
        sim_to(t); if (S.phase != PH_PLAY) return bad("the game ended at tick %ld, before this input", S.tick); if (S.tick != t) return bad("input: could not reach tick %ld", t);
        S.dir[i] = dir; snprintf(out->actor, sizeof out->actor, "%s", in->actor); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "%d", S.pos[i]); snprintf(out->res, sizeof out->res, "ok"); out->round = t; return 0; }
    return bad("unknown verb %s for a %s game", v, KIND == K_LEDGER ? "ledger" : "realtime");
}

/* ---------- ledger file: load (replay) and append ---------- */
static int field_ok(const char *s) { return !strpbrk(s, "|\r\n"); }
static int load(void) {
    memset(&S, 0, sizeof S); S.winner = -1; KNOWN_SIZE = 0; FILE *f = fopen(LEDGER, "r"); if (!f) { if (errno == ENOENT) return 0; return bad("cannot read %s", LEDGER); }
    char ln[2048]; int lineno = 0;
    while (fgets(ln, sizeof ln, f)) { lineno++; size_t L = strlen(ln); KNOWN_SIZE += (long)L;
        if (L == 0 || ln[L - 1] != '\n') { fclose(f); return bad("ledger line %d: not a whole row (no end of line, or longer than %d)", lineno, (int)sizeof ln - 2); }
        ln[L - 1] = 0; if (L > ROWMAX) { fclose(f); return bad("ledger line %d: longer than %d bytes", lineno, ROWMAX); }
        char cp[2048]; snprintf(cp, sizeof cp, "%s", ln); char *fl[8]; int n = split_row(cp, fl, 8);
        if (n != 7 || strcmp(fl[0], "EVENT")) { fclose(f); return bad("ledger line %d: not an EVENT row of 7 fields", lineno); }
        char *e; long rnd = strtol(fl[1], &e, 10); if (*e || !fl[1][0]) { fclose(f); return bad("ledger line %d: round is not a number", lineno); }
        if ((lineno == 1) != !strcmp(fl[3], "new")) { fclose(f); return bad("ledger line %d: `new` must be the first row and only the first", lineno); }
        Row in; memset(&in, 0, sizeof in); in.round = rnd; snprintf(in.actor, sizeof in.actor, "%s", fl[2]); snprintf(in.verb, sizeof in.verb, "%s", fl[3]); snprintf(in.target, sizeof in.target, "%s", fl[4]);
        const char *arg2 = NULL; char a2[220] = "";
        if (!strcmp(fl[3], "join") || !strcmp(fl[3], "seat")) { const char *d = strstr(fl[6], "driver="); if (d) { snprintf(a2, sizeof a2, "%s", d + 7); arg2 = a2; } }
        else if (KIND == K_EDITOR && (!strcmp(fl[3], "add") || !strcmp(fl[3], "set"))) { snprintf(a2, sizeof a2, "%s", fl[6]); arg2 = a2; }
        if (lineno == 1) {   /* the first row carries the seed (target) and must name this game's kind and rules */
            char want[16]; snprintf(want, sizeof want, "kind=%s", KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor"); if (!strstr(fl[6], want)) { fclose(f); return bad("ledger line 1: this ledger is not a %s game", want + 5); }
            const char *c = strstr(fl[6], "core="); if (c && atoi(c + 5) > CORE_VERSION) { fclose(f); return bad("ledger line 1: written by a newer core (%d)", atoi(c + 5)); } }
        Row out; if (apply(&in, arg2, &out)) { char m2[400]; snprintf(m2, sizeof m2, "%s", MSG); fclose(f); return bad("ledger line %d: %s", lineno, m2); }
#ifndef GC_MUT_NO_VERIFY
        if (out.round != in.round) { fclose(f); return bad("ledger line %d: round is %ld, the rules say %ld", lineno, in.round, out.round); }
        if (strcmp(out.actor, in.actor)) { fclose(f); return bad("ledger line %d: actor is %s, the rules say %s", lineno, in.actor, out.actor); }
        if (strcmp(out.target, in.target)) { fclose(f); return bad("ledger line %d: target is %s, the rules say %s", lineno, in.target, out.target); }
        if (strcmp(out.at, fl[5])) { fclose(f); return bad("ledger line %d: at is %s, the rules say %s", lineno, fl[5], out.at); }
        if (strcmp(out.res, fl[6])) { fclose(f); return bad("ledger line %d: result is %s, the rules say %s", lineno, fl[6], out.res); }
#endif
        S.nrows++; }
    fclose(f); return 0;
}
static int row_text(const Row *r, char *o, size_t cap) { if (!field_ok(r->actor) || !field_ok(r->verb) || !field_ok(r->target) || !field_ok(r->at) || !field_ok(r->res)) return bad("a field holds a bar or a line break");
    int n = snprintf(o, cap, "EVENT|%ld|%s|%s|%s|%s|%s\n", r->round, r->actor, r->verb, r->target, r->at, r->res); if (n <= 0 || (size_t)n >= cap || n > ROWMAX) return bad("row too long"); return 0; }
/* a live verb: apply on the loaded state, then append the row with ONE write(). A refusal appends nothing (but the in-memory state is then dirty: callers reload before the next verb). */
static int live(long round, const char *actor, const char *verb, const char *target, const char *arg2, Row *outp) {
    if (S.nrows >= R_maxrows) return bad("the ledger is full (max_rows %ld)", R_maxrows);
    Row in, out; memset(&in, 0, sizeof in); in.round = round; snprintf(in.actor, sizeof in.actor, "%s", actor ? actor : ""); snprintf(in.verb, sizeof in.verb, "%s", verb); snprintf(in.target, sizeof in.target, "%s", target ? target : "");
    if (apply(&in, arg2, &out)) return 1; char line[2048]; if (row_text(&out, line, sizeof line)) return 1;
    mkdir_p(SD); int fd = open(LEDGER, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return bad("cannot append to %s", LEDGER);
    ssize_t w = write(fd, line, strlen(line)); close(fd); if (w != (ssize_t)strlen(line)) return bad("short write"); S.nrows++; KNOWN_SIZE += (long)w; if (outp) *outp = out; return 0;
}
static uint64_t ledger_hash(const char *skip) { FILE *f = fopen(LEDGER, "r"); uint64_t h = FNV0; if (!f) return h; char ln[2048]; while (fgets(ln, sizeof ln, f)) { if (skip && *skip) { char *a = strchr(ln, '|'); if (a) a = strchr(a + 1, '|'); if (a) a = strchr(a + 1, '|'); if (a && !strncmp(a + 1, skip, strlen(skip)) && a[1 + strlen(skip)] == '|') continue; } h = fnv(h, ln, strlen(ln)); } fclose(f); return h; }

/* ---------- ui projection + status ---------- */
static const char *phase_name(void) { return S.phase == PH_NONE ? "none" : S.phase == PH_LOBBY ? "lobby" : S.phase == PH_PLAY ? "play" : "over"; }
static int build_ui(char *o, size_t cap) {
    int n = 0; const char *kn = KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor";
    n += snprintf(o + n, cap - (size_t)n, "kind=%s\ntitle=%s\nphase=%s\nrows=%d\n", kn, TITLE, phase_name(), S.nrows);
    if (KIND == K_EDITOR) { n += snprintf(o + n, cap - (size_t)n, "doc_rows=%d\npublished=%s\nedits=%ld\n", S.ndoc, S.published ? S.pubhash : "", S.nedits); for (int i = 0; i < S.ndoc; i++) n += snprintf(o + n, cap - (size_t)n, "d%d_id=%s\nd%d_text=%s\n", i + 1, S.did[i], i + 1, S.dtx[i]); return n; }
    n += snprintf(o + n, cap - (size_t)n, "length=%d\nn_players=%d\n", LENGTH, S.np);
    if (KIND == K_LEDGER) n += snprintf(o + n, cap - (size_t)n, "round=%ld\nrolls=%ld\n", S.round, S.nrolls); else n += snprintf(o + n, cap - (size_t)n, "tick=%ld\n", S.tick);
    int over = S.phase == PH_OVER; char wid[8] = ""; if (over && S.winner >= 0) snprintf(wid, sizeof wid, "p%d", S.winner + 1);
    n += snprintf(o + n, cap - (size_t)n, "over=%s\nwinner=%s\nwinner_name=%s\n", over ? "1" : "", wid, over && S.winner >= 0 ? S.name[S.winner] : "");
    if (KIND == K_LEDGER && S.phase == PH_PLAY) n += snprintf(o + n, cap - (size_t)n, "current=p%d\ncurrent_name=%s\ncurrent_driver=%s\nhuman_turn=%s\n", S.cur + 1, S.name[S.cur], S.human[S.cur] ? "human" : "pet", S.human[S.cur] ? "1" : "");
    for (int i = 0; i < S.np; i++) n += snprintf(o + n, cap - (size_t)n, "p%d_name=%s\np%d_driver=%s\np%d_pos=%d\n", i + 1, S.name[i], i + 1, S.human[i] ? "human" : "pet", i + 1, S.pos[i]);
    return n;
}
static int write_atomic(const char *path, const char *data) { char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return -1; fputs(data, f); if (fclose(f)) return -1; return rename(tmp, path); }
static int write_ui(void) { static char b[16000]; build_ui(b, sizeof b); char p[P + 16]; snprintf(p, sizeof p, "%s/ui.txt", SD); mkdir_p(SD); return write_atomic(p, b); }
static int write_doc(void) { static char b[16000]; int n = snprintf(b, sizeof b, "# doc.txt - a projection of ledger.txt (verb export); do not edit\n"); for (int i = 0; i < S.ndoc; i++) n += snprintf(b + n, sizeof b - (size_t)n, "ROW|%s|%s\n", S.did[i], S.dtx[i]); snprintf(b + n, sizeof b - (size_t)n, "PUBLISHED|%s\n", S.published ? S.pubhash : "-"); char p[P + 16]; snprintf(p, sizeof p, "%s/doc.txt", SD); mkdir_p(SD); return write_atomic(p, b); }

/* ---------- audit: no ghost data ---------- */
static int audit(void) {
    char p[P], ln[1024], decl[32][64], kinds[32][16], regen[32][32]; int nd = 0; char *fl[8]; snprintf(p, sizeof p, "%s/state.pdl", APP); FILE *f = fopen(p, "r"); if (!f) { fprintf(ERRF, "AUDIT FAIL no state.pdl\n"); return 1; }
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split_row(ln, fl, 8); if (n < 5 || strcmp(fl[0], "FILE") || nd >= 32) continue; snprintf(decl[nd], 64, "%s", fl[1]); snprintf(kinds[nd], 16, "%s", fl[2]); snprintf(regen[nd], 32, "%s", fl[4]); nd++; }
    fclose(f); int fails = 0, files = 0;
    char vp[P], vb[16000]; snprintf(vp, sizeof vp, "%s/verbs.pdl", APP); rd(vp, vb, sizeof vb);
    for (int i = 0; i < nd; i++) { int ok = !strcmp(kinds[i], "LEDGER") || !strcmp(kinds[i], "CONFIG") || !strcmp(kinds[i], "PREFERENCE") || !strcmp(kinds[i], "PROJECTION"); if (!ok) { fprintf(ERRF, "AUDIT FAIL %s has the unknown kind %s\n", decl[i], kinds[i]); fails++; }
        if (!strcmp(kinds[i], "PROJECTION")) { int has = 0; for (char *q = vb; (q = strstr(q, "VERB")); q += 4) { char *e = strchr(q, '\n'); char *b = strchr(q, '|'); if (b && (!e || b < e)) { char nm[40]; snprintf(nm, sizeof nm, "%s", b); char *b2 = strchr(nm + 1, '|'); if (b2) *b2 = 0; if (!strcmp(trim(nm + 1), regen[i])) has = 1; } }
            if (!has) { fprintf(ERRF, "AUDIT FAIL projection %s names the verb %s which verbs.pdl does not have\n", decl[i], regen[i]); fails++; } } }
    DIR *d = opendir(SD); if (d) { struct dirent *e; while ((e = readdir(d))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; files++; int ok = 0; for (int i = 0; i < nd; i++) if (!strcmp(decl[i], e->d_name)) ok = 1;
#ifdef GC_MUT_NO_GHOST
            ok = 1;
#endif
            if (!ok) { fprintf(ERRF, "AUDIT FAIL ghost file %s (not declared in state.pdl)\n", e->d_name); fails++; } } closedir(d); }
    if (fails) return 1; fprintf(OUT, "AUDIT ok files=%d declared=%d\n", files, nd); return 0;
}

/* ---------- the verbs ---------- */
static const char *pet_dir(int i) { int d = die(S.seed, S.tick * 16 + i, 5); return d <= 3 || d == 6 ? "right" : d == 4 ? "stay" : "left"; }
static int do_tick(int *did) {   /* ONE step. *did = 1 when the game moved. A human seat in a ledger game makes tick wait (exit 0, nothing written). */
    *did = 0;
    if (S.phase != PH_PLAY) return bad("the game is %s", S.phase == PH_OVER ? "over" : "not started");
    if (KIND == K_LEDGER) { if (S.human[S.cur]) return 0; if (live(-1, "", "roll", "", NULL, NULL)) return 1; *did = 1; return 0; }
    if (KIND == K_REALTIME) { if (S.nrows >= R_maxrows - 2) return bad("the ledger is full (max_rows %ld)", R_maxrows);
        if (S.tick % 4 == 0) for (int i = 0; i < S.np; i++) if (!S.human[i]) { char a[8]; snprintf(a, sizeof a, "p%d", i + 1); if (strcmp(pet_dir(i), S.dir[i] == 1 ? "right" : S.dir[i] == -1 ? "left" : "stay")) if (live(-1, a, "input", pet_dir(i), NULL, NULL)) return 1; }
        step_tick(); *did = 1; if (S.phase == PH_OVER || S.tick % R_ckpt == 0) { if (live(-1, "", "ckpt", "", NULL, NULL)) return 1; } return 0; }
    return bad("tick is not a verb of an editor game");
}
static int cmd_status(void) {
    fprintf(OUT, "STATE kind=%s phase=%s rows=%d", KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", phase_name(), S.nrows);
    if (KIND == K_EDITOR) { fprintf(OUT, " doc_rows=%d edits=%ld published=%s\n", S.ndoc, S.nedits, S.published ? S.pubhash : "no"); for (int i = 0; i < S.ndoc; i++) fprintf(OUT, "ROW %s %s\n", S.did[i], S.dtx[i]); return 0; }
    if (KIND == K_LEDGER) fprintf(OUT, " round=%ld current=p%d rolls=%ld", S.round, S.cur + 1, S.nrolls); else fprintf(OUT, " tick=%ld", S.tick);
    fprintf(OUT, " over=%d winner=%s\n", S.phase == PH_OVER, S.phase == PH_OVER && S.winner >= 0 ? S.name[S.winner] : "-");
    for (int i = 0; i < S.np; i++) fprintf(OUT, "PLAYER p%d name=%s driver=%s pos=%d\n", i + 1, S.name[i], S.human[i] ? "human" : "pet", S.pos[i]); return 0;
}
static int cmd_check(void) {   /* replay already ran (load); the invariants */
    for (int i = 0; i < S.np; i++) if (S.pos[i] < 0 || S.pos[i] > LENGTH) { fprintf(ERRF, "CHECK FAIL p%d is off the track (%d)\n", i + 1, S.pos[i]); return 1; }
    if (S.phase == PH_OVER && KIND != K_EDITOR) { if (S.winner < 0 || S.pos[S.winner] != LENGTH) { fprintf(ERRF, "CHECK FAIL the winner is not at the end of the track\n"); return 1; } int at_end = 0; for (int i = 0; i < S.np; i++) if (S.pos[i] >= LENGTH) at_end++; if (KIND == K_LEDGER && at_end != 1) { fprintf(ERRF, "CHECK FAIL %d players at the end of a ledger game\n", at_end); return 1; } }
    if (KIND == K_EDITOR) { if (S.published) { /* the last publish row recorded the doc hash then; later edits may differ, that is allowed */ } for (int i = 0; i < S.ndoc; i++) for (int j = i + 1; j < S.ndoc; j++) if (!strcmp(S.did[i], S.did[j])) { fprintf(ERRF, "CHECK FAIL duplicate id %s\n", S.did[i]); return 1; } }
    fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0;
}
static int need_args(int argc, int n, const char *usage) { if (argc < n) { fprintf(ERRF, "usage: %s\n", usage); return 1; } return 0; }
/* args: the words after the verb. `as=pN` anywhere sets the actor. Returns the exit code. */
static int engine_verb(int argc, char **argv) {
    const char *verb = argv[0]; char as[32] = ""; char *av[16]; int ac = 0;
    for (int i = 1; i < argc && ac < 15; i++) { if (!strncmp(argv[i], "as=", 3)) snprintf(as, sizeof as, "%s", argv[i] + 3); else av[ac++] = argv[i]; }
    if (!strcmp(verb, "help")) { char vp[P], ln[1024], *fl[8]; snprintf(vp, sizeof vp, "%s/verbs.pdl", APP); FILE *f = fopen(vp, "r"); if (!f) { fprintf(ERRF, "no verbs.pdl\n"); return 2; } while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split_row(ln, fl, 8); if (n >= 6 && !strcmp(fl[0], "VERB")) fprintf(OUT, "%-10s %s  [%s]\n", fl[1], fl[5], fl[4]); } fclose(f); return 0; }
    if (load()) { fprintf(ERRF, "%s\n", MSG); return 1; }

    if (!strcmp(verb, "new")) { if (ac > 1) { fprintf(ERRF, "usage: new [seed]\n"); return 2; } if (ac == 1) { char *e; long v = strtol(av[0], &e, 10); if (*e || v < 0 || v > 999999999) { fprintf(ERRF, "new: the seed is a number 0 to 999999999\n"); return 2; } }
        if (live(-1, "", "new", ac ? av[0] : "-", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "new game seed=%ld kind=%s rows=%d\n", S.seed, KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", S.nrows); return 0; }
    if (!strcmp(verb, "join")) { if (need_args(ac, 1, "join <name> [human|pet]")) return 2; if (live(-1, "", "join", av[0], ac > 1 ? av[1] : "pet", NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "p%d joined as %s\n", S.np, av[0]); return 0; }
    if (!strcmp(verb, "start")) { if (live(-1, "", "start", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "game started: %d players, seed %ld\n", S.np, S.seed); return 0; }
    if (!strcmp(verb, "seat")) { if (need_args(ac, 2, "seat <pN> human|pet")) return 2; if (live(-1, "", "seat", av[0], av[1], NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s is now %s\n", av[0], av[1]); return 0; }
    if (!strcmp(verb, "roll") && KIND == K_LEDGER) { Row r; if (live(-1, as, "roll", "", NULL, &r)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s rolled %s: %s\n", r.actor, r.target, r.res); return 0; }
    if (!strcmp(verb, "input") && KIND == K_REALTIME) { if (need_args(ac, 2, "input <pN> left|right|stay [tick]")) return 2; long t = -1; if (ac > 2) { char *e; t = strtol(av[2], &e, 10); if (*e || t < 0) { fprintf(ERRF, "input: tick is a number\n"); return 2; } } if (live(t, av[0], "input", av[1], NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s %s at tick %ld\n", av[0], av[1], t >= 0 ? t : S.tick); return 0; }
    if (!strcmp(verb, "advance") && KIND == K_REALTIME) { long n = 1; if (ac > 0) { char *e; n = strtol(av[0], &e, 10); if (*e || n < 1 || n > 100000) { fprintf(ERRF, "advance: 1 to 100000 ticks\n"); return 2; } } if (S.phase != PH_PLAY) { fprintf(ERRF, "the game is %s\n", S.phase == PH_OVER ? "over" : "not started"); return 2; }
        for (long i = 0; i < n && S.phase == PH_PLAY; i++) { int did; if (do_tick(&did)) { fprintf(ERRF, "%s\n", MSG); return 2; } } if (S.phase == PH_PLAY && S.tick % R_ckpt) { if (live(-1, "", "ckpt", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } } fprintf(OUT, "tick %ld rows=%d\n", S.tick, S.nrows); return 0; }
    if (!strcmp(verb, "tick")) { int did; if (do_tick(&did)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, did ? "ticked: tick=%ld rows=%d\n" : "waiting for a human seat (tick=%ld rows=%d)\n", KIND == K_LEDGER ? S.nrolls : S.tick, S.nrows); return 0; }
    if (!strcmp(verb, "autoplay") && KIND != K_EDITOR) { if (S.phase != PH_PLAY) { fprintf(ERRF, "autoplay: the game is %s\n", S.phase == PH_OVER ? "over" : "not started"); return 2; }
        for (int h = 0; h < S.np; h++) if (S.human[h]) { fprintf(ERRF, "autoplay: p%d is a human seat (use `seat p%d pet` first)\n", h + 1, h + 1); return 2; }
        long cap = KIND == K_LEDGER ? R_maxrows : R_maxticks, k = 0; while (S.phase == PH_PLAY && k < cap) { int did; if (do_tick(&did)) { fprintf(ERRF, "%s\n", MSG); return 2; } k++; }
        fprintf(OUT, "AUTOPLAY events=%d %s=%ld over=%d winner=%s\n", S.nrows, KIND == K_LEDGER ? "rounds" : "ticks", KIND == K_LEDGER ? S.round : S.tick, S.phase == PH_OVER, S.phase == PH_OVER && S.winner >= 0 ? S.name[S.winner] : "-"); return S.phase == PH_OVER ? 0 : 2; }
    if (!strcmp(verb, "ckpt")) { if (live(-1, "", "ckpt", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "pinned: state hash recorded in row %d\n", S.nrows); return 0; }
    if (KIND == K_EDITOR && (!strcmp(verb, "add") || !strcmp(verb, "set") || !strcmp(verb, "del"))) {
        if (need_args(ac, !strcmp(verb, "del") ? 1 : 2, "add|set <id> <text...>   del <id>   [as=author]")) return 2; char tx[200] = ""; for (int i = 1; i < ac; i++) { if (i > 1) strncat(tx, " ", sizeof tx - strlen(tx) - 1); strncat(tx, av[i], sizeof tx - strlen(tx) - 1); }
        if (live(-1, as, verb, av[0], tx, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s %s: %d rows in the document\n", verb, av[0], S.ndoc); return 0; }
    if (KIND == K_EDITOR && !strcmp(verb, "publish")) { if (live(-1, as, "publish", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "published doc=%s rows=%d\n", S.pubhash, S.ndoc); return 0; }
    if (!strcmp(verb, "export")) { if (KIND != K_EDITOR) { fprintf(ERRF, "export: only an editor game has a document\n"); return 2; } if (write_doc()) { fprintf(ERRF, "export: cannot write doc.txt\n"); return 2; } fprintf(OUT, "wrote doc.txt (%d rows)\n", S.ndoc); return 0; }
    if (!strcmp(verb, "status")) return cmd_status();
    if (!strcmp(verb, "check")) return cmd_check();
    if (!strcmp(verb, "replay")) { char hs[20]; hx(state_hash(), hs); fprintf(OUT, "REPLAY events=%d hash=%s\n", S.nrows, hs); return 0; }
    if (!strcmp(verb, "ledger_hash")) { char hs[20]; hx(ledger_hash(ac > 0 ? av[0] : NULL), hs); fprintf(OUT, "LEDGER events=%d hash=%s\n", S.nrows, hs); return 0; }
    if (!strcmp(verb, "audit")) return audit();
    if (!strcmp(verb, "ui")) { if (write_ui()) { fprintf(ERRF, "ui: cannot write ui.txt\n"); return 2; } fprintf(OUT, "wrote ui.txt\n"); return 0; }
    fprintf(ERRF, "unknown verb %s for a %s game (try help)\n", verb, KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor"); return 2;
}

/* ---------- ctl: what a bar button runs ---------- */
static int ctl_main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: game_core ctl <command> <games_dir>\n"); return 2; }
    for (const char *c = argv[2]; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '+' || *c == '-')) { fprintf(stderr, "game_core ctl: a command is lower-case letters, digits, _ + -\n"); return 2; }
    if (!argv[2][0] || strlen(argv[2]) > 40) { fprintf(stderr, "game_core ctl: a command is 1 to 40 characters\n"); return 2; }
    char p[P + 16]; snprintf(p, sizeof p, "%s/action.txt", argv[3]); int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd < 0) { perror("game_core ctl"); return 1; }
    char ln[80]; int l = snprintf(ln, sizeof ln, "%s\n", argv[2]); int ok = write(fd, ln, (size_t)l) == l; close(fd); return ok ? 0 : 1;
}

/* ---------- the driver ---------- */
static char HOUSE[P], BOOK[P], EDIT[P], GDIR[P], FEED[P], ACT[P], ARCH[P], SW[P], NAME[64], SELF[P];
static long OWNER; static int mode_play, nplayers = 3, human_seat, speed = 1, dirty = 1; static long seed_pref = 7; static char dmsg[240] = "Choose the players, then START";
static const int INTERVAL_MS[3] = { 120, 550, 1400 }, RT_MS[3] = { 40, 100, 250 }; static const char *SPEED_NAME[3] = { "fast", "normal", "slow" };
static volatile sig_atomic_t stop_;
static void on_term(int s) { (void)s; stop_ = 1; }
static int running(void) { FILE *f = fopen(SW, "r"); if (!f) return 0; char ln[80]; int r = 0; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "running=", 8)) r = atoi(ln + 8); fclose(f); return r; }
static void rm_tree(const char *d) { DIR *dr = opendir(d); if (!dr) return; struct dirent *e; while ((e = readdir(dr))) { if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue; char p[P]; snprintf(p, sizeof p, "%s/%s", d, e->d_name); struct stat s; if (!lstat(p, &s) && S_ISDIR(s.st_mode)) rm_tree(p); else unlink(p); } closedir(dr); rmdir(d); }
static int cmp_str(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void archive_game(void) {   /* keep the last 3 games in archive/ (the cap), older ones are deleted */
    struct stat s; if (stat(LEDGER, &s)) { rm_tree(SD); return; } mkdir(ARCH, 0755); char to[P]; snprintf(to, sizeof to, "%s/game-%ld", ARCH, (long)time(NULL)); int k = 0; while (!access(to, F_OK) && k < 50) snprintf(to, sizeof to, "%s/game-%ld-%d", ARCH, (long)time(NULL), ++k); rename(SD, to);
    static char names[64][64]; int n = 0; DIR *d = opendir(ARCH); if (d) { struct dirent *e; while ((e = readdir(d)) && n < 64) if (!strncmp(e->d_name, "game-", 5)) snprintf(names[n++], 64, "%s", e->d_name); closedir(d); }
    qsort(names, (size_t)n, 64, cmp_str); for (int i = 0; i < n - 3; i++) { char p[P]; snprintf(p, sizeof p, "%s/%s", ARCH, names[i]); rm_tree(p); }
}
/* run an engine verb IN THIS PROCESS (no child): output into a memory stream. */
static int verb_here(int argc, char **argv, char *out, size_t cap) {
    char *ob = NULL, *eb = NULL; size_t ol = 0, el = 0; FILE *o = open_memstream(&ob, &ol), *e = open_memstream(&eb, &el); FILE *so = OUT, *se = ERRF; OUT = o; ERRF = e;
    int rc = engine_verb(argc, argv); fflush(o); fflush(e); OUT = so; ERRF = se; fclose(o); fclose(e);
    snprintf(out, cap, "%s", (ob && ob[0]) ? ob : (eb ? eb : "")); char *nl = strpbrk(out, "\r\n"); if (nl) *nl = 0; free(ob); free(eb); return rc;
}
static int reload(void) { if (load()) { snprintf(dmsg, sizeof dmsg, "ledger refused: %.180s", MSG); return 1; } return 0; }
static void begin_game(void) {
    char out[300], sd[16]; archive_game(); mkdir_p(SD); snprintf(sd, sizeof sd, "%ld", seed_pref); char *n1[] = { (char *)"new", sd };
    if (verb_here(2, n1, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "new refused: %.180s", out); return; }
    for (int i = 1; i <= nplayers; i++) { char nm[4] = { (char)('A' + i - 1), 0 }; char *j[] = { (char *)"join", nm, (char *)(i == human_seat ? "human" : "pet") }; if (verb_here(3, j, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "join refused: %.180s", out); return; } }
    char *st[] = { (char *)"start" }; if (verb_here(1, st, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "start refused: %.180s", out); return; }
    mode_play = 1; reload(); write_ui(); snprintf(dmsg, sizeof dmsg, "game started: %d players, seed %ld%s", nplayers, seed_pref, human_seat ? ", you are a seat at the table" : ", all pets");
}
static void human_live(const char *verb, const char *target, const char *arg2, const char *actor) {
    if (reload()) return; if (live(-1, actor, verb, target, arg2, NULL)) snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); else snprintf(dmsg, sizeof dmsg, "you: %s %s", verb, target ? target : ""); write_ui(); if (KIND == K_EDITOR) write_doc();
}
static void handle(const char *c) {
    char out[300];
    if (!strcmp(c, "players_up")) { if (!mode_play && nplayers < R_maxp) nplayers++; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "players_down")) { if (!mode_play && nplayers > 2) nplayers--; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "human_next")) { if (!mode_play) human_seat = (human_seat + 1) % (nplayers + 1); }
    else if (!strcmp(c, "seed_next")) { if (!mode_play) seed_pref = seed_pref * 31 % 9973 + 7; }
    else if (!strcmp(c, "speed_next")) speed = (speed + 1) % 3;
    else if (!strcmp(c, "setup_reset")) { if (!mode_play) { nplayers = 3; human_seat = 0; seed_pref = R_seed; speed = 1; snprintf(dmsg, sizeof dmsg, "Choose the players, then START"); } }
    else if (!strcmp(c, "start")) { if (!mode_play && KIND != K_EDITOR) begin_game(); }
    else if (!strcmp(c, "step")) { if (mode_play && !running() && KIND != K_EDITOR) { if (!reload()) { int did; if (do_tick(&did)) snprintf(dmsg, sizeof dmsg, "stuck: %.180s", MSG); else snprintf(dmsg, sizeof dmsg, "stepped"); write_ui(); } } }
    else if (!strcmp(c, "new_game")) { if (mode_play && KIND != K_EDITOR) { archive_game(); mode_play = 0; memset(&S, 0, sizeof S); S.winner = -1; snprintf(dmsg, sizeof dmsg, "the last game is in archive/; choose the players, then START"); } }
    else if (KIND == K_LEDGER && !strcmp(c, "roll")) { if (!reload()) { char who[8] = ""; if (S.phase == PH_PLAY && S.human[S.cur]) snprintf(who, sizeof who, "p%d", S.cur + 1); human_live("roll", "", NULL, who); } }
    else if (KIND == K_REALTIME && (!strcmp(c, "left") || !strcmp(c, "right") || !strcmp(c, "stay"))) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) { hp = i + 1; break; } if (!hp) snprintf(dmsg, sizeof dmsg, "no human seat"); else { char a[8]; snprintf(a, sizeof a, "p%d", hp); if (!reload()) human_live("input", c, NULL, a); } }
    else if (KIND == K_EDITOR && !strncmp(c, "add_", 4)) { char id[32]; snprintf(id, sizeof id, "%s", c + 4); if (!reload()) { int ix = 0; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], id)) ix = 1; if (!strcmp(id, "item")) { snprintf(id, sizeof id, "item%d", S.ndoc + 1); ix = 0; } char tx[80]; snprintf(tx, sizeof tx, "%s (added from the bar)", id); human_live(ix ? "set" : "add", id, tx, "author"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "remove_last")) { if (!reload()) { if (S.ndoc) { char id[32]; snprintf(id, sizeof id, "%s", S.did[S.ndoc - 1]); human_live("del", id, NULL, "author"); } else snprintf(dmsg, sizeof dmsg, "the document is empty"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "publish")) { if (!reload()) human_live("publish", "", NULL, "author"); }
    else if (!strcmp(c, "refresh")) { reload(); }
    else snprintf(dmsg, sizeof dmsg, "unknown command %.40s", c);
    (void)out; dirty = 1;
}
static void add_btn(char *o, size_t cap, int *nb, const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "gmb_%d_label=%s\ngmb_%d_act='%s' ctl %s '%s'\n", *nb, label, *nb, SELF, cmd, GDIR); (*nb)++; strncat(o, b, cap - strlen(o) - 1); }
static void add_txt(char *o, size_t cap, int *nt, const char *t) { char b[400]; snprintf(b, sizeof b, "gmt_%d_text=%s\n", (*nt)++, t[0] ? t : " "); strncat(o, b, cap - strlen(o) - 1); }
static void publish(void) {
    static char o[12000]; o[0] = 0; size_t C = sizeof o; int nt = 0, nb = 0; char b[300]; int run = running();
    snprintf(b, sizeof b, "gm_on=1\ngm_pid=%ld\ngm_title=%s\n", (long)getpid(), TITLE); strncat(o, b, C - strlen(o) - 1);
    snprintf(b, sizeof b, "%.200s", dmsg); add_txt(o, C, &nt, b);
    if (KIND == K_EDITOR) {
        snprintf(b, sizeof b, "%d rows, %s, %ld edits", S.ndoc, S.published ? "published" : "not published", S.nedits); add_txt(o, C, &nt, b);
        add_btn(o, C, &nb, "add start", "add_start"); add_btn(o, C, &nb, "add goal", "add_goal"); add_btn(o, C, &nb, "add item", "add_item"); add_btn(o, C, &nb, "remove last", "remove_last"); add_btn(o, C, &nb, "publish", "publish");
    } else if (!mode_play) {
        snprintf(b, sizeof b, "%d players, seed %ld", nplayers, seed_pref); add_txt(o, C, &nt, b); add_btn(o, C, &nb, "players -", "players_down"); add_btn(o, C, &nb, "players +", "players_up");
        { char hp[8] = "none"; if (human_seat) snprintf(hp, sizeof hp, "p%d", human_seat); snprintf(b, sizeof b, "you play: %s", hp); } add_btn(o, C, &nb, b, "human_next");
        snprintf(b, sizeof b, "seed %ld (next)", seed_pref); add_btn(o, C, &nb, b, "seed_next"); snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(o, C, &nb, b, "speed_next"); add_btn(o, C, &nb, "START GAME", "start");
    } else {
        int over = S.phase == PH_OVER; if (KIND == K_LEDGER) snprintf(b, sizeof b, "r%ld  p%d %s  %s", S.round, S.cur + 1, S.name[S.cur], over ? "OVER" : (run ? "RUNNING" : "STOPPED")); else snprintf(b, sizeof b, "tick %ld  %s", S.tick, over ? "OVER" : (run ? "RUNNING" : "STOPPED")); add_txt(o, C, &nt, b);
        for (int i = 0; i < S.np; i++) { snprintf(b, sizeof b, "p%d %s (%s): %d/%d", i + 1, S.name[i], S.human[i] ? "you" : "pet", S.pos[i], LENGTH); add_txt(o, C, &nt, b); }
        if (over && S.winner >= 0) { snprintf(b, sizeof b, "winner: %s", S.name[S.winner]); add_txt(o, C, &nt, b); }
        if (!over && KIND == K_LEDGER && S.human[S.cur]) add_btn(o, C, &nb, "Roll", "roll");
        if (!over && KIND == K_REALTIME) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) hp = 1; if (hp) { add_btn(o, C, &nb, "Left", "left"); add_btn(o, C, &nb, "Stay", "stay"); add_btn(o, C, &nb, "Right", "right"); } }
        if (!over && !run) add_btn(o, C, &nb, "Step", "step"); snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(o, C, &nb, b, "speed_next"); if (over) add_btn(o, C, &nb, "New game", "new_game");
    }
    snprintf(b, sizeof b, "gm_nt=%d\ngm_nb=%d\ngm_ne=0\ngm_nl=0\n", nt, nb); strncat(o, b, C - strlen(o) - 1); write_atomic(FEED, o); dirty = 0;
}
static int driver_main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: game_core driver <house_root> <book_app_dir> <editor_app_dir>\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); snprintf(BOOK, sizeof BOOK, "%s", argv[3]); snprintf(EDIT, sizeof EDIT, "%s", argv[4]);
    { char bb[P]; snprintf(bb, sizeof bb, "%s", BOOK); size_t L = strlen(bb); while (L > 1 && bb[L - 1] == '/') bb[--L] = 0; char *bn = strrchr(bb, '/'); snprintf(NAME, sizeof NAME, "%s", bn ? bn + 1 : bb); }
    if (!good_word(NAME, 40, 1)) { fprintf(stderr, "game_core driver: bad book folder name\n"); return 2; }
    snprintf(APP, sizeof APP, "%s", BOOK); { ssize_t k = readlink("/proc/self/exe", SELF, sizeof SELF - 1); if (k > 0) SELF[k] = 0; else snprintf(SELF, sizeof SELF, "%s", argv[0]); }
    snprintf(GDIR, sizeof GDIR, "%s/state/games/%s", EDIT, NAME); snprintf(SD, sizeof SD, "%s/game", GDIR); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", SD); snprintf(ARCH, sizeof ARCH, "%s/archive", GDIR);
    snprintf(FEED, sizeof FEED, "%s/state/game_ui.txt", EDIT); snprintf(ACT, sizeof ACT, "%s/action.txt", GDIR); snprintf(SW, sizeof SW, "%s/state/world/switches.txt", EDIT);
    OUT = stdout; ERRF = stderr; if (load_config()) { fprintf(stderr, "game_core driver: %s\n", MSG); return 1; } seed_pref = R_seed; if (nplayers > R_maxp) nplayers = (int)R_maxp;
    { char d[P]; snprintf(d, sizeof d, "%s/state/games", EDIT); mkdir(d, 0755); mkdir(GDIR, 0755); }
    { const char *ow = getenv("GAME_OWNER_PID"); OWNER = ow ? atol(ow) : 0; }
    char lk[P]; snprintf(lk, sizeof lk, "%s/driver.lock", GDIR); int lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "game_core driver: cannot open the lock file %s (does the editor's state folder exist?)\n", lk); return 1; }
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "game_core driver: already running\n"); return 0; }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    FILE *devnull = fopen("/dev/null", "w"); OUT = devnull ? devnull : stdout; ERRF = OUT;
    if (!reload() && S.phase >= PH_PLAY) { mode_play = 1; snprintf(dmsg, sizeof dmsg, "a game was in progress; it continues while the Player menu says playing"); }
    if (KIND == K_EDITOR) { mode_play = 1; if (S.phase == PH_NONE) { char o2[200]; char *n1[] = { (char *)"new" }; if (verb_here(1, n1, o2, sizeof o2)) snprintf(dmsg, sizeof dmsg, "new refused: %.180s", o2); else { reload(); write_doc(); snprintf(dmsg, sizeof dmsg, "an empty document: add start and goal, then publish"); } } }
    struct stat s; long cursor = stat(ACT, &s) ? 0 : (long)s.st_size; long next = now_ms(); publish();
    while (!stop_) {
        if (OWNER > 0 ? (kill((pid_t)OWNER, 0) != 0 && errno == ESRCH) : getppid() == 1) break;
        if (!stat(ACT, &s) && s.st_size > cursor) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[128]; while (fgets(ln, sizeof ln, f)) { char *e = strpbrk(ln, "\r\n"); if (e) *e = 0; if (ln[0]) handle(ln); } cursor = ftell(f); fclose(f); } if (cursor > 65536 && truncate(ACT, 0) == 0) cursor = 0; }
        else if (!stat(ACT, &s) && s.st_size < cursor) cursor = 0;
        static int was_run = -1; int run = running(); if (run != was_run) { was_run = run; dirty = 1; }
        struct stat ls; if (mode_play && !stat(LEDGER, &ls) && ls.st_size != KNOWN_SIZE) { reload(); dirty = 1; }   /* somebody else (the CLI) wrote the ledger */
        if (mode_play && run && KIND != K_EDITOR && S.phase == PH_PLAY) { long t = now_ms(); if (t >= next) { int did = 0;
                if (KIND == K_LEDGER && S.human[S.cur]) { char yt[120]; snprintf(yt, sizeof yt, "your turn, %s: press Roll", S.name[S.cur]); if (strcmp(dmsg, yt)) { snprintf(dmsg, sizeof dmsg, "%s", yt); dirty = 1; } }
                else { if (do_tick(&did)) { snprintf(dmsg, sizeof dmsg, "stuck: %.180s", MSG); reload(); } else if (did && (KIND == K_LEDGER || S.tick % R_ckpt == 0)) { write_ui(); } dirty = 1; }
                if (S.phase == PH_OVER) { write_ui(); snprintf(dmsg, sizeof dmsg, "the game is over, winner %s", S.winner >= 0 ? S.name[S.winner] : "-"); dirty = 1; }
                next = t + (KIND == K_REALTIME ? RT_MS[speed] : INTERVAL_MS[speed]); } }
        { static long last_pub = 0; long tp = now_ms(); if (dirty && tp - last_pub >= 200) { publish(); last_pub = tp; } }   /* the feed is rewritten at most 5 times a second, whatever the tick pace */
        usleep((mode_play && run && S.phase == PH_PLAY && KIND != K_EDITOR && !(KIND == K_LEDGER && S.human[S.cur])) ? (KIND == K_REALTIME ? 40000 : 80000) : 250000);   /* live: tick pace; setup, stopped, over, a human thinking, an editor: 250 ms */
    }
    { char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", FEED); FILE *f = fopen(tmp, "w"); if (f) { fputs("gm_on=\n", f); fclose(f); rename(tmp, FEED); } }   /* the bar disappears when the game ends */
    return 0;
}

int main(int argc, char **argv) {
    OUT = stdout; ERRF = stderr;
    if (argc >= 2 && !strcmp(argv[1], "ctl")) return ctl_main(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "driver")) return driver_main(argc, argv);
    if (argc < 4) { fprintf(stderr, "usage: game_core <app_dir> <state_dir> <verb> [args]   |   game_core driver <house> <book> <editor>   |   game_core ctl <command> <games_dir>\n"); return 2; }
    snprintf(APP, sizeof APP, "%s", argv[1]); snprintf(SD, sizeof SD, "%s", argv[2]); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", SD);
    if (load_config()) { fprintf(stderr, "%s\n", MSG); return 2; }
    return engine_verb(argc - 3, argv + 3);
}
