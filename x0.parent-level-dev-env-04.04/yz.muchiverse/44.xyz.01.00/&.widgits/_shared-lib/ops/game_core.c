/* game_core - the ONE shared engine, driver and button op of every game made by `game_new_op` (2026-11-09; BOOK 16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md).
 *
 *   game_core <app_dir> <state_dir> <verb> [args...]        the engine: ONE process per verb, no resident process. <app_dir> = @.apps/<game> (game.pdl, rulesets/, state.pdl, verbs.pdl),
 *                                                           <state_dir> = the folder of the ledger (the editor uses <editor>/state/games/<game>/game).
 *   game_core driver <house_root> <book_app_dir> <editor_app_dir>   what the editor's Play starts (game.pdl `GAME | driver`): one instance (flock), ticks only while the editor's switch running=1,
 *                                                           publishes the feed <editor>/state/game_ui.txt, takes button presses from <games_dir>/action.txt, ends when the editor is gone.
 *   game_core ctl <command> <games_dir> [ignored...]       what a game-bar button runs: appends ONE line to <games_dir>/action.txt (extra arguments are ignored on purpose: the renderer appends two).
 *
 * WHY ONE SHARED BINARY (decision D62): the per-game C is zero. A game made by the generator is DATA (game.pdl, rulesets/default/board.pdl + rules.pdl, state.pdl, verbs.pdl) and this binary. A fix here reaches every
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
#include <sys/wait.h>
#include <poll.h>
#include <sys/inotify.h>

#define P 4096
#define CORE_VERSION 1
#define MAXPL 8
#define DOCMAX 64
#define MAXJ 64
#define ROWMAX 1000

enum { K_LEDGER = 1, K_REALTIME = 2, K_EDITOR = 3 };
enum { PH_NONE = 0, PH_LOBBY = 1, PH_PLAY = 2, PH_OVER = 3 };
/* realtime MODE (board.pdl `MODE | track|snake|walk`): track = the original stub (x += dir on a track), snake = ONE snake on a grid (BOARD cols/rows, RULE walls/start_len/grow/score_food, seeded food), walk = a hero walking along the START row of a LEVEL file (no physics yet) */
enum { M_TRACK = 0, M_SNAKE = 1, M_WALK = 2, M_PLATFORM = 3 };   /* platform = walk plus the shared gravity-field body (phys_field.c, D90): gravity, run, jump; the level file is the same */
/* ledger finish rule (rules.pdl `RULE | finish | reach|exact_stay|bounce`): reach = a roll that reaches or passes the end wins (the original rule); exact_stay = the end must be hit exactly, an overshoot leaves you where you are; bounce = the surplus walks back */
enum { F_REACH = 0, F_STAY = 1, F_BOUNCE = 2 };
#define SNMAX 1100
#define LVMAXW 64
#define LVMAXH 32
#include "../phys_field.c"
#include "../rule_patrol.c"   /* the PATROL rule (D92/D93): one pure module, also the CLI op rule_patrol_op */
#include "../rule_fireball.c"   /* the FIREBALL rule (D92/D93): one pure module, also the CLI op rule_fireball_op */
#include "../rule_stomp.c"   /* the STOMP / HIT rule (D92/D93): one pure module, also the CLI op rule_stomp_op */
#include "../rule_pickup.c"   /* the PICKUP rule (D92/D93): one pure module, also the CLI op rule_pickup_op */
#include "../rule_goal.c"   /* the GOAL rule (D92/D93): one pure module, also the CLI op rule_goal_op */
#include "../rule_win.c"   /* the WIN rule (D92/D93): one pure module over rule_goal.c, also the CLI op rule_win_op */
#include "../rule_food.c"   /* the FOOD rule (D92/D93): one pure module over seed_mix.c, also the CLI op rule_food_op */
#include "../rule_spawn.c"   /* the SPAWN rule (D92/D93): one pure module, also the CLI op rule_spawn_op */
#include "../rule_fall.c"   /* the FALL rule (D92/D93): one pure module, also the CLI op rule_fall_op */
#include "../rule_input.c"   /* the INPUT rule (D92/D93): one pure module, also the CLI op rule_input_op */
#define WALKISH (MODE == M_WALK || MODE == M_PLATFORM)
static PF_Tune PT; static PF_Field PFIELD;   /* the physics knobs (RULE rows gravity, jump_v, run_max, accel, friction: part of the rules hash) and the one plane field of a flat level */

static FILE *OUT, *ERRF;                     /* stdout/stderr for the CLI; memory streams when the driver calls a verb in-process */
static char APP[P], SD[P], LEDGER[P], TITLE[96] = "Game", RS[40] = "default", MSG[400];
static int KIND = K_LEDGER, MODE = M_TRACK, FINISH = F_REACH, WRAP = 0, LEVELDOC = 0;
static long R_maxp = 4, R_minp = 2, R_maxrows = 5000, R_ckpt = 10, R_seed = 7, R_maxticks = 4000, R_docmax = 40, R_startlen = 3, R_grow = 1, R_scorefood = 10, R_needclear = 0, R_patrol = 10;   /* R_patrol: RULE patrol_speed (Q8 per tick), default 10 = the speed before it was data */
static FB_Rule FBR = { 56, 36, 4, 100 };   /* RULE fireball_speed, fireball_life, fireball_max, fireball_score; the defaults are the values before they were data */
static ST_Rule STR = { 64, 133, 100 };   /* RULE stomp_margin, stomp_bounce, stomp_score; the defaults are the values before they were data (the bounce was jump_v * 2 / 3 = 133) */
static PK_Rule PKR = { 50 };   /* RULE pickup_score; the default is the value before it was data */
static SP_Rule SPR = { 200, 200 };   /* RULE body_w, body_h; the defaults are the sizes before they were data */
static long FAM = 2;   /* RULE fall_margin (whole cells below the world); the default is the value before it was data */
static int LENGTH = 30, NJ; static int JF[MAXJ], JT[MAXJ];
static int COLS = 16, ROWS = 12;
static char T_UP[48] = "jumped", T_DOWN[48] = "jumped", LEVELREL[300], LEVEL_SOLID[16] = "#";
static char REQ[8][32]; static int NREQ;
static uint64_t RULES_HASH;
/* a level (mode walk, and the editor's level document): LVROW[y] is a string of LW tiles; START and GOAL are cells */
static struct { int w, h, sx, sy, gx, gy; char rows[LVMAXH][LVMAXW + 1]; char name[64]; int ne, ex[8], ey[8], npw, px[4], py[4]; } LV;   /* ne enemies (ENEMY rows), npw projectile power-ups (POWER rows) */

typedef struct { long round; char actor[32], verb[16], target[200], at[48], res[300]; } Row;
typedef struct { long tick; char verb[8], target[16], at[32], res[200]; } Ev;
static struct {
    int nrows, phase, np, cur, winner; long seed, round, nrolls, tick;
    char name[MAXPL][24]; int human[MAXPL], pos[MAXPL], dir[MAXPL], ever[MAXPL];
    int ndoc, published; char did[DOCMAX][32], dtx[DOCMAX][200]; char pubhash[20]; long nedits;
    /* ledger: the last roll (derived by replay, not hashed) and the last five table-talk sentences */
    int l_valid, l_who, l_roll, l_from, l_land, l_to, l_kind, l_win, l_human; char tk[5][200]; long ntk;
    /* snake (mode snake): body[0] is the head; sdir 0 L 1 R 2 U 3 D; q = consequence rows the ticks made and no row has recorded yet */
    int slen, sgrow, sdir, sscore, sfood_x, sfood_y, sfoods, bx[SNMAX], by[SNMAX], lastturn_ok; long lastturn_tick; int nq; Ev q[4];
    /* editor with a level document: the last verified clear (its level hash and run hash) */
    char clear_level[20], clear_run[20];
    /* platform (mode platform): one gravity-field body per player (Q8 cells), the pending one-tick jump pulse, and how many times a player fell out of the level and was put back on START */
    PF_Body pb[MAXPL]; int jp[MAXPL]; long falls;
    /* platform extras (2026-11-18): enemies (walkers on the same physics), power-ups, fireballs; per player: armed, facing, fire pulse, score; counts of stomps, shots and hits */
    RP_Walker en[8]; int pw_taken[4]; FB_Shot fb[4]; int armed[MAXPL], facing[MAXPL], fire[MAXPL]; long score, stomps, shots_kills, hits;
} S;
static long KNOWN_SIZE;                      /* bytes of the ledger as this process last read or wrote it */

static int bad(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsnprintf(MSG, sizeof MSG, fmt, ap); va_end(ap); return 1; }
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const unsigned char *s = p; for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; } return h; }
#define FNV0 14695981039346656037ULL
static void hx(uint64_t h, char *o) { snprintf(o, 20, "%016llx", (unsigned long long)h); }
static uint64_t mix(uint64_t x) { return sm_mix(x); }   /* the house seeded mixer: seed_mix.c, one authority (rule_food.c uses it too) */
static int die(long sd, long idx, int k) { return (int)(mix((uint64_t)sd * 1000003ULL + (uint64_t)idx * 16 + (uint64_t)k) % 6) + 1; }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int rd(const char *p, char *b, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return -1; } size_t n = fread(b, 1, cap - 1, f); b[n] = 0; fclose(f); return (int)n; }
static void mkdir_p(const char *path) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *c = t + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(t, 0755); *c = '/'; } mkdir(t, 0755); }
static int good_word(const char *s, size_t max, int allow_dash) { size_t n = strlen(s); if (n < 1 || n > max) return 0; for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (allow_dash && (c == '_' || c == '-')))) return 0; } return 1; }

/* ---------- data: game.pdl, rules ---------- */
static int split_row(char *line, char **f, int max);
static int is_solid(char c) { return c && strchr(LEVEL_SOLID, c) != NULL; }
/* the level: ONE canonical text (level_render) is what both the Make document and a level file reduce to, and its FNV-1a is the level's identity inside the engine (the sha256 of the file is the human-facing identity: hash_op) */
static int level_check(void) {
    if (LV.w < 4 || LV.w > LVMAXW || LV.h < 3 || LV.h > LVMAXH) return bad("level: the size is 4..%d x 3..%d tiles", LVMAXW, LVMAXH);
    for (int y = 0; y < LV.h; y++) { if ((int)strlen(LV.rows[y]) != LV.w) return bad("level: row %d has %d tiles, the level is %d wide", y, (int)strlen(LV.rows[y]), LV.w);
        for (int x = 0; x < LV.w; x++) { char c = LV.rows[y][x]; if (!(c == '.' || (c > ' ' && c < 127 && c != '|'))) return bad("level: row %d has a bad tile character", y); } }
    if (LV.sx < 0 || LV.sx >= LV.w || LV.sy < 0 || LV.sy >= LV.h) return bad("level: START is outside the level");
    if (LV.gx < 0 || LV.gx >= LV.w || LV.gy < 0 || LV.gy >= LV.h) return bad("level: GOAL is outside the level");
    if (is_solid(LV.rows[LV.sy][LV.sx])) return bad("level: START is inside a solid tile");
    if (is_solid(LV.rows[LV.gy][LV.gx])) return bad("level: GOAL is inside a solid tile");
    for (int i = 0; i < LV.ne; i++) if (LV.ex[i] < 0 || LV.ex[i] >= LV.w || LV.ey[i] < 0 || LV.ey[i] >= LV.h || is_solid(LV.rows[LV.ey[i]][LV.ex[i]])) return bad("level: ENEMY %d is outside the level or inside a solid tile", i + 1);
    for (int i = 0; i < LV.npw; i++) if (LV.px[i] < 0 || LV.px[i] >= LV.w || LV.py[i] < 0 || LV.py[i] >= LV.h || is_solid(LV.rows[LV.py[i]][LV.px[i]])) return bad("level: POWER %d is outside the level or inside a solid tile", i + 1);
    return 0;
}
static int level_render(char *o, size_t cap) {
    int n = snprintf(o, cap, "# level.pdl - a Teario level, the ONE format Make writes and Play reads (BOOK 16.game TEARIO plan 1.2 and 5.1). Top row is 0; . empty, # solid ground.\nMETA  | name | %s\nMETA  | size | %d x %d\nSTART | %d | %d\nGOAL  | %d | %d\n", LV.name, LV.w, LV.h, LV.sx, LV.sy, LV.gx, LV.gy);
    for (int y = 0; y < LV.h && n > 0 && (size_t)n < cap; y++) n += snprintf(o + n, cap - (size_t)n, "ROW   | %d | %s\n", y, LV.rows[y]);
    for (int i = 0; i < LV.ne && n > 0 && (size_t)n < cap; i++) n += snprintf(o + n, cap - (size_t)n, "ENEMY | %d | %d\n", LV.ex[i], LV.ey[i]);
    for (int i = 0; i < LV.npw && n > 0 && (size_t)n < cap; i++) n += snprintf(o + n, cap - (size_t)n, "POWER | %d | %d\n", LV.px[i], LV.py[i]);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}
static uint64_t level_hash(void) { static char b[8192]; int n = level_render(b, sizeof b); return n < 0 ? 0 : fnv(FNV0, b, (size_t)n); }
static int level_load_file(const char *path, uint64_t *fold) {
    FILE *f = fopen(path, "r"); if (!f) return bad("cannot read the level file %s", path); char ln[400], *fl[6]; int ns = 0, ng = 0, maxy = -1; memset(&LV, 0, sizeof LV); LV.sx = LV.sy = LV.gx = LV.gy = -1; snprintf(LV.name, sizeof LV.name, "level");
    while (fgets(ln, sizeof ln, f)) { if (fold) *fold = fnv(*fold, ln, strlen(ln)); if (ln[0] == '#') continue; int n = split_row(ln, fl, 6); if (n < 2) continue;
        if (!strcmp(fl[0], "META") && n >= 3 && !strcmp(fl[1], "name")) snprintf(LV.name, sizeof LV.name, "%s", fl[2]);
        else if (!strcmp(fl[0], "START") && n >= 3) { LV.sx = atoi(fl[1]); LV.sy = atoi(fl[2]); ns++; }
        else if (!strcmp(fl[0], "GOAL") && n >= 3) { LV.gx = atoi(fl[1]); LV.gy = atoi(fl[2]); ng++; }
        else if (!strcmp(fl[0], "ENEMY") && n >= 3) { if (LV.ne >= 8) { fclose(f); return bad("level: at most 8 ENEMY rows"); } LV.ex[LV.ne] = atoi(fl[1]); LV.ey[LV.ne++] = atoi(fl[2]); }
        else if (!strcmp(fl[0], "POWER") && n >= 3) { if (LV.npw >= 4) { fclose(f); return bad("level: at most 4 POWER rows"); } LV.px[LV.npw] = atoi(fl[1]); LV.py[LV.npw++] = atoi(fl[2]); }
        else if (!strcmp(fl[0], "ROW") && n >= 3) { int y = atoi(fl[1]); if (y < 0 || y >= LVMAXH || strlen(fl[2]) > LVMAXW) { fclose(f); return bad("level: bad ROW %s", fl[1]); } if (LV.rows[y][0]) { fclose(f); return bad("level: ROW %d twice", y); } snprintf(LV.rows[y], sizeof LV.rows[y], "%s", fl[2]); if (y > maxy) maxy = y; } }
    fclose(f); if (ns != 1 || ng != 1) return bad("level: exactly one START and one GOAL row are needed"); LV.h = maxy + 1; LV.w = LV.h > 0 ? (int)strlen(LV.rows[0]) : 0;
    for (int y = 0; y < LV.h; y++) if (!LV.rows[y][0]) return bad("level: ROW %d is missing", y);
    return level_check();
}
/* the editor's document -> LV (ids: name, start "x y", goal "x y", r00..r31) */
static int level_from_doc(void) {
    memset(&LV, 0, sizeof LV); LV.sx = LV.sy = LV.gx = LV.gy = -1; snprintf(LV.name, sizeof LV.name, "level"); int maxy = -1, hs = 0, hg = 0;
    for (int i = 0; i < S.ndoc; i++) { const char *id = S.did[i], *tx = S.dtx[i];
        if (!strcmp(id, "name")) snprintf(LV.name, sizeof LV.name, "%s", tx);
        else if (!strcmp(id, "start")) { if (sscanf(tx, "%d %d", &LV.sx, &LV.sy) != 2) return bad("level: start is `x y`"); hs = 1; }
        else if (!strcmp(id, "goal")) { if (sscanf(tx, "%d %d", &LV.gx, &LV.gy) != 2) return bad("level: goal is `x y`"); hg = 1; }
        else if (strlen(id) == 2 && (id[0] == 'e' || id[0] == 'p') && id[1] >= '1' && id[1] <= '8') { int a, b; if (sscanf(tx, "%d %d", &a, &b) != 2) return bad("level: %s is `x y`", id); if (id[0] == 'e') { if (LV.ne >= 8) return bad("level: at most 8 enemies"); LV.ex[LV.ne] = a; LV.ey[LV.ne++] = b; } else { if (LV.npw >= 4) return bad("level: at most 4 power-ups"); LV.px[LV.npw] = a; LV.py[LV.npw++] = b; } }
        else if (id[0] == 'r' && strlen(id) == 3 && id[1] >= '0' && id[1] <= '9' && id[2] >= '0' && id[2] <= '9') { int y = atoi(id + 1); if (y >= LVMAXH || strlen(tx) > LVMAXW) return bad("level: bad row %s", id); snprintf(LV.rows[y], sizeof LV.rows[y], "%s", tx); if (y > maxy) maxy = y; } }
    if (!hs || !hg) return bad("level: the document needs start and goal rows"); LV.h = maxy + 1; LV.w = LV.h > 0 ? (int)strlen(LV.rows[0]) : 0;
    for (int y = 0; y < LV.h; y++) if (!LV.rows[y][0]) return bad("level: row r%02d is missing", y);
    return level_check();
}
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
    RULES_HASH = FNV0; PT = pf_default_tune(); R_patrol = 10; FBR = (FB_Rule){ 56, 36, 4, 100 }; STR = (ST_Rule){ 64, 133, 100 }; PKR = (PK_Rule){ 50 }; SPR = (SP_Rule){ 200, 200 }; FAM = 2; memset(&PFIELD, 0, sizeof PFIELD); PFIELD.dy = 1; NJ = 0; NREQ = 0; MODE = M_TRACK; FINISH = F_REACH; WRAP = 0; LEVELDOC = 0; LEVELREL[0] = 0; snprintf(T_UP, sizeof T_UP, "jumped"); snprintf(T_DOWN, sizeof T_DOWN, "jumped");
    const char *files[2] = { "board.pdl", "rules.pdl" };
    for (int k = 0; k < 2; k++) { snprintf(p, sizeof p, "%s/rulesets/%s/%s", APP, RS, files[k]); f = fopen(p, "r"); if (!f) return bad("missing %s", p);
        RULES_HASH = fnv(RULES_HASH, files[k], strlen(files[k]));
        while (fgets(ln, sizeof ln, f)) { RULES_HASH = fnv(RULES_HASH, ln, strlen(ln)); if (ln[0] == '#') continue; char cp[1024]; snprintf(cp, sizeof cp, "%s", ln); n = split_row(cp, fl, 10); if (n < 2) continue;
            if (!strcmp(fl[0], "TRACK") && n >= 3 && !strcmp(fl[1], "length")) { LENGTH = atoi(fl[2]); if (LENGTH < 3 || LENGTH > 1000) { fclose(f); return bad("board.pdl: TRACK length is 3 to 1000"); } }
            else if (!strcmp(fl[0], "JUMP") && n >= 3) { if (NJ >= MAXJ) { fclose(f); return bad("board.pdl: at most %d JUMP rows", MAXJ); } JF[NJ] = atoi(fl[1]); JT[NJ] = atoi(fl[2]); NJ++; }
            else if (!strcmp(fl[0], "REQUIRE") && NREQ < 8) snprintf(REQ[NREQ++], 32, "%s", fl[1]);
            else if (!strcmp(fl[0], "MODE") && n >= 2) { MODE = !strcmp(fl[1], "track") ? M_TRACK : !strcmp(fl[1], "snake") ? M_SNAKE : !strcmp(fl[1], "walk") ? M_WALK : !strcmp(fl[1], "platform") ? M_PLATFORM : -1; if (MODE < 0) { fclose(f); return bad("board.pdl: MODE is track, snake, walk or platform"); } }
            else if (!strcmp(fl[0], "BOARD") && n >= 3 && !strcmp(fl[1], "cols")) COLS = atoi(fl[2]);
            else if (!strcmp(fl[0], "BOARD") && n >= 3 && !strcmp(fl[1], "rows")) ROWS = atoi(fl[2]);
            else if (!strcmp(fl[0], "TALK") && n >= 3 && !strcmp(fl[1], "up")) snprintf(T_UP, sizeof T_UP, "%s", fl[2]);
            else if (!strcmp(fl[0], "TALK") && n >= 3 && !strcmp(fl[1], "down")) snprintf(T_DOWN, sizeof T_DOWN, "%s", fl[2]);
            else if (!strcmp(fl[0], "LEVEL") && n >= 3 && !strcmp(fl[1], "file")) snprintf(LEVELREL, sizeof LEVELREL, "%s", fl[2]);
            else if (!strcmp(fl[0], "LEVEL") && n >= 3 && !strcmp(fl[1], "solid")) snprintf(LEVEL_SOLID, sizeof LEVEL_SOLID, "%s", fl[2]);
            else if (!strcmp(fl[0], "RULE") && n >= 3) { long v = atol(fl[2]);
                if (!strcmp(fl[1], "max_players")) R_maxp = v; else if (!strcmp(fl[1], "max_rows")) R_maxrows = v; else if (!strcmp(fl[1], "ckpt_every")) R_ckpt = v;
                else if (!strcmp(fl[1], "default_seed")) R_seed = v; else if (!strcmp(fl[1], "max_ticks")) R_maxticks = v; else if (!strcmp(fl[1], "max_doc_rows")) R_docmax = v;
                else if (!strcmp(fl[1], "gravity")) PT.gravity = v; else if (!strcmp(fl[1], "jump_v")) PT.jump_v = v; else if (!strcmp(fl[1], "run_max")) PT.run_max = v; else if (!strcmp(fl[1], "accel")) PT.accel = v; else if (!strcmp(fl[1], "friction")) PT.friction = v;
                else if (!strcmp(fl[1], "min_players")) R_minp = v; else if (!strcmp(fl[1], "start_len")) R_startlen = v; else if (!strcmp(fl[1], "grow")) R_grow = v; else if (!strcmp(fl[1], "score_food")) R_scorefood = v;
                else if (!strcmp(fl[1], "publish_needs_clear")) R_needclear = v; else if (!strcmp(fl[1], "fall_margin")) { if (v < 0 || v > 64) { fclose(f); return bad("rules.pdl: fall_margin is 0 to 64"); } FAM = v; }
                else if (!strcmp(fl[1], "body_w")) { if (v < 1 || v > 256) { fclose(f); return bad("rules.pdl: body_w is 1 to 256"); } SPR.w = v; } else if (!strcmp(fl[1], "body_h")) { if (v < 1 || v > 256) { fclose(f); return bad("rules.pdl: body_h is 1 to 256"); } SPR.h = v; }
                else if (!strcmp(fl[1], "pickup_score")) { if (v < 0 || v > 100000) { fclose(f); return bad("rules.pdl: pickup_score is 0 to 100000"); } PKR.score = v; }
                else if (!strcmp(fl[1], "stomp_margin")) { if (v < 0 || v > 256) { fclose(f); return bad("rules.pdl: stomp_margin is 0 to 256"); } STR.margin = v; } else if (!strcmp(fl[1], "stomp_bounce")) { if (v < 0 || v > 1000) { fclose(f); return bad("rules.pdl: stomp_bounce is 0 to 1000"); } STR.bounce = v; } else if (!strcmp(fl[1], "stomp_score")) { if (v < 0 || v > 100000) { fclose(f); return bad("rules.pdl: stomp_score is 0 to 100000"); } STR.score = v; }
                else if (!strcmp(fl[1], "fireball_speed")) { if (v < 1 || v > 256) { fclose(f); return bad("rules.pdl: fireball_speed is 1 to 256"); } FBR.speed = v; } else if (!strcmp(fl[1], "fireball_life")) { if (v < 1 || v > 600) { fclose(f); return bad("rules.pdl: fireball_life is 1 to 600"); } FBR.life = (int)v; } else if (!strcmp(fl[1], "fireball_max")) { if (v < 1 || v > 4) { fclose(f); return bad("rules.pdl: fireball_max is 1 to 4"); } FBR.max = (int)v; } else if (!strcmp(fl[1], "fireball_score")) { if (v < 0 || v > 100000) { fclose(f); return bad("rules.pdl: fireball_score is 0 to 100000"); } FBR.score = v; }
                else if (!strcmp(fl[1], "patrol_speed")) { if (v < 1 || v > 256) { fclose(f); return bad("rules.pdl: patrol_speed is 1 to 256"); } R_patrol = v; }
                else if (!strcmp(fl[1], "finish")) { FINISH = !strcmp(fl[2], "reach") ? F_REACH : !strcmp(fl[2], "exact_stay") ? F_STAY : !strcmp(fl[2], "bounce") ? F_BOUNCE : -1; if (FINISH < 0) { fclose(f); return bad("rules.pdl: finish is reach, exact_stay or bounce"); } }
                else if (!strcmp(fl[1], "walls")) { if (!strcmp(fl[2], "wrap")) WRAP = 1; else if (!strcmp(fl[2], "solid")) WRAP = 0; else { fclose(f); return bad("rules.pdl: walls is solid or wrap"); } }
                else if (!strcmp(fl[1], "doc_format")) { if (!strcmp(fl[2], "level")) LEVELDOC = 1; else { fclose(f); return bad("rules.pdl: doc_format is level"); } } } }
        fclose(f); }
    if (R_minp < 1 || R_minp > R_maxp) return bad("rules.pdl: min_players is 1 to max_players");
    if (R_maxp < 1 || R_maxp > MAXPL || (R_maxp < 2 && MODE != M_SNAKE && !WALKISH)) return bad("rules.pdl: max_players is 2 to %d", MAXPL);
    if (MODE == M_SNAKE) { if (KIND != K_REALTIME) return bad("board.pdl: MODE snake needs a realtime game"); if (R_maxp != 1 || R_minp != 1) return bad("rules.pdl: a snake game has exactly one snake (min_players and max_players 1)");
        if (COLS < 6 || COLS > 32 || ROWS < 6 || ROWS > 32 || COLS * ROWS > SNMAX) return bad("board.pdl: BOARD cols and rows are 6 to 32 (at most %d cells)", SNMAX); if (R_startlen < 1 || R_startlen > COLS / 2 + 1) return bad("rules.pdl: start_len is 1 to cols/2+1"); if (R_grow < 0 || R_grow > 20) return bad("rules.pdl: grow is 0 to 20"); }
    if (WALKISH) { if (KIND != K_REALTIME) return bad("board.pdl: MODE walk or platform needs a realtime game"); if (!LEVELREL[0]) return bad("board.pdl: MODE walk needs a LEVEL | file row"); }
    if (R_ckpt < 1 || R_ckpt > 1000) return bad("rules.pdl: ckpt_every is 1 to 1000");
    if (R_maxrows < 10 || R_maxrows > 100000) return bad("rules.pdl: max_rows is 10 to 100000");
    if (R_docmax < 1 || R_docmax > DOCMAX) return bad("rules.pdl: max_doc_rows is 1 to %d", DOCMAX);
    if (WALKISH) { snprintf(p, sizeof p, "%s/%s", APP, LEVELREL); RULES_HASH = fnv(RULES_HASH, "level", 5); if (level_load_file(p, &RULES_HASH)) return 1; LENGTH = LV.w - 1; }   /* the level file is part of the rules hash: editing it after a game began refuses that ledger */
    for (int i = 0; i < NJ; i++) if (JF[i] < 1 || JF[i] >= LENGTH || JT[i] < 0 || JT[i] > LENGTH || JF[i] == JT[i]) return bad("board.pdl: JUMP %d -> %d is outside the track", JF[i], JT[i]);
    return 0;
}
static int jump_at(int sq) { for (int i = 0; i < NJ; i++) if (JF[i] == sq) return JT[i]; return -1; }

/* ---------- state hash (what a ckpt row pins) ---------- */
static uint64_t state_hash(void) {
    char b[4096]; int n = 0; n += snprintf(b + n, sizeof b - (size_t)n, "k=%d;seed=%ld;phase=%d;cur=%d;round=%ld;rolls=%ld;tick=%ld;win=%d;", KIND, S.seed, S.phase, S.cur, S.round, S.nrolls, S.tick, S.winner);
    for (int i = 0; i < S.np; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%s,%d,%d,%d;", S.name[i], S.human[i], S.pos[i], S.dir[i]);
    for (int i = 0; i < S.ndoc && n < 3500; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%s=%s;", S.did[i], S.dtx[i]);
    n += snprintf(b + n, sizeof b - (size_t)n, "pub=%d,%s", S.published, S.pubhash);
    if (S.clear_level[0]) n += snprintf(b + n, sizeof b - (size_t)n, ";clear=%s,%s", S.clear_level, S.clear_run);
    uint64_t h = fnv(FNV0, b, strlen(b));
    if (MODE == M_SNAKE) { char sb[8192]; int m = snprintf(sb, sizeof sb, "snake;len=%d;grow=%d;dir=%d;score=%d;food=%d,%d;foods=%d;", S.slen, S.sgrow, S.sdir, S.sscore, S.sfood_x, S.sfood_y, S.sfoods);
        for (int i = 0; i < S.slen && m < 8000; i++) m += snprintf(sb + m, sizeof sb - (size_t)m, "%d,%d;", S.bx[i], S.by[i]); h = fnv(h, sb, strlen(sb)); }
    if (MODE == M_PLATFORM) { char pb[2048]; int m = snprintf(pb, sizeof pb, "platform;falls=%ld;", S.falls); for (int i = 0; i < S.np && m < 1900; i++) { const PF_Body *b = &S.pb[i]; m += snprintf(pb + m, sizeof pb - (size_t)m, "%ld,%ld,%ld,%ld,%d,%d,%d,%d,%d;", b->x, b->y, b->vx, b->vy, b->field, b->grounded, b->coyote, b->jbuf, S.jp[i]); } h = fnv(h, pb, strlen(pb)); }
    if (MODE == M_PLATFORM) { char xb[2048]; int m = snprintf(xb, sizeof xb, "plat2;score=%ld,%ld,%ld,%ld;", S.score, S.stomps, S.shots_kills, S.hits); for (int i = 0; i < LV.ne && m < 1700; i++) m += snprintf(xb + m, sizeof xb - (size_t)m, "e%d,%ld,%ld,%ld,%d,%d;", i, S.en[i].b.x, S.en[i].b.y, S.en[i].b.vx, S.en[i].dir, S.en[i].alive);
        for (int k = 0; k < 4 && m < 1900; k++) m += snprintf(xb + m, sizeof xb - (size_t)m, "f%d,%ld,%ld,%d,%d;", k, S.fb[k].x, S.fb[k].y, S.fb[k].life, S.fb[k].alive); for (int p = 0; p < 4; p++) m += snprintf(xb + m, sizeof xb - (size_t)m, "p%d;", S.pw_taken[p]); for (int i = 0; i < S.np; i++) m += snprintf(xb + m, sizeof xb - (size_t)m, "a%d,%d,%d;", S.armed[i], S.facing[i], S.fire[i]); h = fnv(h, xb, strlen(xb)); }
    return h;
}
static uint64_t doc_hash(void) { uint64_t h = FNV0; for (int i = 0; i < S.ndoc; i++) { h = fnv(h, S.did[i], strlen(S.did[i])); h = fnv(h, "=", 1); h = fnv(h, S.dtx[i], strlen(S.dtx[i])); h = fnv(h, "\n", 1); } return h; }

/* ---------- the step functions of kind realtime (ONE per MODE; replay and the live driver call the same one) ---------- */
static const int DX[4] = { -1, 1, 0, 0 }, DY[4] = { 0, 0, -1, 1 }, OPP[4] = { 1, 0, 3, 2 }; static const char DCH[5] = "LRUD";
static void q_push(const char *verb, const char *target, const char *at, const char *res) {
    if (S.nq >= 4) return; Ev *e = &S.q[S.nq++]; e->tick = S.tick; snprintf(e->verb, sizeof e->verb, "%s", verb); snprintf(e->target, sizeof e->target, "%s", target); snprintf(e->at, sizeof e->at, "%s", at); snprintf(e->res, sizeof e->res, "%s", res);
}
static int on_body(int x, int y, int upto) { for (int i = 0; i < upto; i++) if (S.bx[i] == x && S.by[i] == y) return 1; return 0; }
/* the k-th food goes to free[ mix(seed*1000003 + k*16 + 7) % nfree ], free = the unoccupied cells in row-major order at the moment it spawns; returns 0 when the board is full */
static int spawn_food(void) { int fx = 0, fy = 0; if (!fd_spawn(S.bx, S.by, S.slen, COLS, ROWS, S.seed, S.sfoods, &fx, &fy)) return 0; S.sfood_x = fx; S.sfood_y = fy; S.sfoods++; return 1; }   /* the FOOD rule: rule_food.c, the same function the op rule_food_op runs (the caller counts the spawn) */
static void snake_init(void) {
    S.slen = (int)R_startlen; S.sgrow = 0; S.sdir = 1; S.sscore = 0; S.sfoods = 0; S.nq = 0; S.lastturn_tick = -1; int hx = COLS / 2, hy = ROWS / 2;
    for (int i = 0; i < S.slen; i++) { S.bx[i] = hx - i; S.by[i] = hy; } spawn_food();
}
static void snake_step(void) {
    int hx = S.bx[0], hy = S.by[0], nx = hx + DX[S.sdir], ny = hy + DY[S.sdir]; const char *cause = NULL; char at[32], res[200], hs[20];
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { if (WRAP) { nx = (nx + COLS) % COLS; ny = (ny + ROWS) % ROWS; } else cause = "wall"; }
    int growing = S.sgrow > 0; if (!cause && on_body(nx, ny, growing ? S.slen : S.slen - 1)) cause = "self";   /* a tail leaving its cell this tick does not block, unless the snake grows */
    S.tick++; snprintf(at, sizeof at, "%d,%d", hx, hy);
    if (cause) { S.phase = PH_OVER; S.winner = -1; snprintf(res, sizeof res, "cause=%s", cause); q_push("die", "p1", at, res);
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)state_hash()); snprintf(res, sizeof res, "winner=none;ticks=%ld;score=%d;hash=%s", S.tick, S.sscore, hs); q_push("over", "-", at, res); return; }
    int shift = growing ? S.slen : S.slen - 1; memmove(&S.bx[1], &S.bx[0], (size_t)shift * sizeof S.bx[0]); memmove(&S.by[1], &S.by[0], (size_t)shift * sizeof S.by[0]);
    if (growing) { S.slen++; S.sgrow--; } S.bx[0] = nx; S.by[0] = ny; snprintf(at, sizeof at, "%d,%d", nx, ny);
    if (nx == S.sfood_x && ny == S.sfood_y) { S.sscore += (int)R_scorefood; S.sgrow += (int)R_grow; int fx = S.sfood_x, fy = S.sfood_y, ok = spawn_food();
        snprintf(res, sizeof res, "len=%d;score=%d;food=%d,%d;next=%d,%d", S.slen + S.sgrow, S.sscore, fx, fy, ok ? S.sfood_x : -1, ok ? S.sfood_y : -1); q_push("eat", "p1", at, res);
        if (!ok) { S.phase = PH_OVER; S.winner = 0; snprintf(hs, sizeof hs, "%016llx", (unsigned long long)state_hash()); snprintf(res, sizeof res, "winner=p1;ticks=%ld;score=%d;hash=%s", S.tick, S.sscore, hs); q_push("over", "-", at, res); } }
}
/* ---------- mode platform: walk's level with the shared gravity-field body (phys_field.c, D90). ONE plane field pulling down; left/right are surface-relative, jump is a one-tick pulse. A body that falls below the level is put back on START and counted.
 * Enemies (level rows ENEMY x y) are walkers on the SAME physics: they patrol, turn at a wall or at a ledge, and die when a falling hero lands on their head (a stomp bounces the hero) or when a fireball hits them; touching one from the side puts the hero back on START (hits). A POWER row is a pick-up that arms the hero: the input `fire` then shoots a fireball (at most 4 live, 36 ticks, stopped by a wall). All of it is integer and replays from the inputs alone. ---------- */
static int plat_solid(void *ctx, long cx, long cy) { (void)ctx; if (cx < 0 || cx >= LV.w) return 1; if (cy < 0 || cy >= LV.h) return 0; return is_solid(LV.rows[cy][cx]); }
static void plat_spawn(int i) { sp_hero(&S.pb[i], LV.sx, LV.sy, &SPR, &S.jp[i], &S.pos[i], &S.armed[i], &S.facing[i], &S.fire[i]); }   /* the SPAWN rule: rule_spawn.c, the same function the op rule_spawn_op runs */
static void plat_world_init(void) {
    memset(S.en, 0, sizeof S.en); memset(S.pw_taken, 0, sizeof S.pw_taken); memset(S.fb, 0, sizeof S.fb); S.score = S.stomps = S.shots_kills = S.hits = 0;
    for (int i = 0; i < LV.ne; i++) sp_walker(&S.en[i], LV.ex[i], LV.ey[i], &SPR);   /* the SPAWN rule again, for the walkers */
    for (int i = 0; i < S.np; i++) plat_spawn(i);
}
static void plat_step(void) {
    for (int i = 0; i < S.np; i++) { PF_Body *b = &S.pb[i]; int shoot = 0, mask = in_controls(S.dir[i], S.jp[i], S.fire[i], S.armed[i], &S.facing[i], &shoot);   /* the INPUT rule: rule_input.c, the same function the op rule_input_op runs */
        if (shoot) fb_spawn(S.fb, 4, &FBR, b, S.facing[i]);   /* the FIREBALL rule: rule_fireball.c, the same function the op rule_fireball_op runs */
        pf_step(b, &PT, &PFIELD, 1, mask, plat_solid, NULL); in_consume(&S.jp[i], &S.fire[i]);
        if (fa_fell(b, LV.h, FAM)) { S.falls++; plat_spawn(i); } S.pos[i] = (int)((b->x + b->w / 2) / PF_Q); }   /* the FALL rule: rule_fall.c, the same function the op rule_fall_op runs (what a fall does stays the game's: count it and respawn) */
    for (int e = 0; e < LV.ne; e++) rp_step(&S.en[e], &PT, R_patrol, &PFIELD, 1, plat_solid, NULL, fa_limit(LV.h, FAM));   /* the PATROL rule: rule_patrol.c, the same function the op rule_patrol_op runs */
    { int kk = fb_step(S.fb, 4, S.en, LV.ne, plat_solid, NULL); S.score += (long)kk * FBR.score; S.shots_kills += kk; }   /* the FIREBALL rule (rule_fireball.c): moves the shots, kills, returns the kills */
    for (int i = 0; i < S.np; i++) { PF_Body *h = &S.pb[i];
        { ST_Result sr = st_contact(h, S.en, LV.ne, &STR); S.score += (long)sr.stomps * STR.score; S.stomps += sr.stomps; if (sr.hit) { S.hits++; plat_spawn(i); } }   /* the STOMP / HIT rule: rule_stomp.c, the same function the op rule_stomp_op runs (a hit respawns the hero: still the game's) */
        pk_collect(h, LV.px, LV.py, S.pw_taken, LV.npw, &PKR, &S.armed[i], &S.score); }   /* the PICKUP rule: rule_pickup.c, the same function the op rule_pickup_op runs */
    S.tick++; { int w = wn_winner(S.pb, S.np, LV.gx, LV.gy); if (w >= 0) { S.winner = w; S.phase = PH_OVER; } }   /* the WIN rule: rule_win.c (over rule_goal.c), the same function the op rule_win_op runs (the phase and the winner stay the game's state) */
}
static void step_tick(void) {
    if (MODE == M_SNAKE) { snake_step(); return; }
    if (MODE == M_PLATFORM) { plat_step(); return; }
    if (MODE == M_WALK) {   /* no physics: the hero steps along the START row; a solid tile stops it; the goal column wins */
        for (int i = 0; i < S.np; i++) { int nx = S.pos[i] + S.dir[i]; if (nx < 0 || nx >= LV.w || is_solid(LV.rows[LV.sy][nx])) nx = S.pos[i]; S.pos[i] = nx; }
        S.tick++; for (int i = 0; i < S.np; i++) if (S.pos[i] == LV.gx) { S.winner = i; S.phase = PH_OVER; break; } return; }
    for (int i = 0; i < S.np; i++) { S.pos[i] += S.dir[i]; if (S.pos[i] < 0) S.pos[i] = 0; if (S.pos[i] > LENGTH) S.pos[i] = LENGTH; }
    S.tick++; int best = -1; for (int i = 0; i < S.np; i++) if (S.pos[i] >= LENGTH && (best < 0 || S.pos[i] > S.pos[best])) best = i;
    if (best >= 0) { S.winner = best; S.phase = PH_OVER; }
}
static void sim_to(long t) { while (S.phase == PH_PLAY && S.tick < t) step_tick(); }

/* ---------- table talk (ledger kind): a PROJECTION of the replayed rows, never stored; the verb `talk` prints one sentence per roll, `ui` keeps the last five ---------- */
static int TALK_PRINT;
static void (*ROWHOOK)(int lineno, const Row *r);   /* set by `export` for one load(): called after every replayed row with the state AFTER it */
static void talk_make(char *o, size_t cap) {
    const char *nm = S.name[S.l_who]; int d = S.l_roll, k = S.l_kind; int n;
    if (k == 1 || k == 2) n = snprintf(o, cap, "%s rolled %d and %s from %d to %d", nm, d, k == 1 ? T_UP : T_DOWN, S.l_land, S.l_to);
    else if (k == 3) n = snprintf(o, cap, "%s rolled %d but needs exactly %d: stays on %d", nm, d, LENGTH - S.l_from, S.l_from);
    else if (k >= 4) { n = snprintf(o, cap, "%s rolled %d on %d: bounces back to %d", nm, d, S.l_from, S.l_land); if (k >= 5 && n > 0 && (size_t)n < cap) snprintf(o + n, cap - (size_t)n, " and %s from %d to %d", k == 5 ? T_UP : T_DOWN, S.l_land, S.l_to); }
    else n = snprintf(o, cap, "%s rolled %d: square %d to %d", nm, d, S.l_from, S.l_to);
    if (S.l_win && n > 0 && (size_t)n < cap) { n = (int)strlen(o); snprintf(o + n, cap - (size_t)n, " and wins after %ld rolls", S.nrolls); }
}
static void talk_push(void) {
    char *slot = S.tk[S.ntk % 5]; talk_make(slot, sizeof S.tk[0]); S.ntk++;
    if (TALK_PRINT && OUT) fprintf(OUT, "%ld: %s\n", S.nrolls, slot);
}

/* ---------- apply: the ONE function that makes (live) and checks (replay) a row ---------- */
/* the key space of the house (petslike_core.c header, line 34): arrows arrive as 1000 left / 1001 right / 1002 up / 1003 down in state/interact_relay.txt (the agent relay band 200-203 is also accepted); a turn takes these codes as well as L R U D */
static char key_dir(int code) { return (code == 1000 || code == 200) ? 'L' : (code == 1001 || code == 201) ? 'R' : (code == 1002 || code == 202) ? 'U' : (code == 1003 || code == 203) ? 'D' : 0; }
static char turn_dir(const char *t) { if (!strcmp(t, "L") || !strcmp(t, "R") || !strcmp(t, "U") || !strcmp(t, "D")) return t[0]; char *e; long c = strtol(t, &e, 10); return (*e || !*t) ? 0 : key_dir((int)c); }
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
        if (MODE != M_TRACK) { size_t L = strlen(out->res); snprintf(out->res + L, sizeof out->res - L, ",mode=%s", MODE == M_SNAKE ? "snake" : MODE == M_PLATFORM ? "platform" : "walk"); }
        out->round = 0; return 0; }
    if (S.phase == PH_NONE) return bad("no game yet: run `new` first");
    if (!strcmp(v, "ckpt")) {
        if (KIND == K_REALTIME) { long t = in->round >= 0 ? in->round : S.tick; sim_to(t); if (S.tick != t) return bad("ckpt tick %ld is not the tick the game reached (%ld)", t, S.tick); if (S.nq) return bad("ckpt at tick %ld: the ticks before it made a %s row that no row recorded", t, S.q[0].verb); out->round = t; }
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
        if (!strcmp(v, "clear") && LEVELDOC) {   /* the proof that this level can be won: arg2 = `level=<hex>;run=<hex>;ticks=<n>` made by the CLI from a replay of a Play run ledger (verify_clear); here it must name THIS level as it is now */
            if (level_from_doc()) return 1; char lh[20]; hx(level_hash(), lh); const char *lv = strstr(tx, "level="); if (!lv || strncmp(lv + 6, lh, 16)) return bad("clear refused: the proof is for another level (this level is %s)", lh);
            if (!strstr(tx, ";run=") || !strstr(tx, ";ticks=")) return bad("clear: the proof needs level, run and ticks");
            snprintf(S.clear_level, sizeof S.clear_level, "%s", lh); const char *rn = strstr(tx, ";run="); snprintf(S.clear_run, 17, "%.16s", rn + 5); S.clear_run[16] = 0; S.nedits++;
            out->round = S.nedits; snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "%d", S.ndoc); snprintf(out->res, sizeof out->res, "%s", tx); return 0; }
        if (!strcmp(v, "publish")) {
            for (int r = 0; r < NREQ; r++) { int ok = 0; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], REQ[r])) ok = 1; if (!ok) return bad("publish refused: the document has no row %s (rules.pdl REQUIRE)", REQ[r]); }
            char extra[80] = ""; if (LEVELDOC) { if (level_from_doc()) return bad("publish refused: %s", MSG); char lh[20]; hx(level_hash(), lh);
                if (R_needclear) { if (!S.clear_level[0]) return bad("publish refused: no verified clear (play the level to its goal, then `clear <play app> <play state>`)"); if (strcmp(S.clear_level, lh)) return bad("publish refused: the last verified clear is for another version of the level (it was %s, the level is %s now)", S.clear_level, lh); }
                snprintf(extra, sizeof extra, ";level=%s%s%s", lh, S.clear_level[0] ? ";clear=" : "", S.clear_level[0] ? S.clear_run : ""); }
            hx(doc_hash(), hs); S.published = 1; snprintf(S.pubhash, sizeof S.pubhash, "%s", hs); S.nedits++;
            out->round = S.nedits; snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "%d", S.ndoc); snprintf(out->res, sizeof out->res, "doc=%s%s", hs, extra); return 0; }
        return bad("unknown verb %s for an editor game", v); }
    /* ledger + realtime from here */
    if (!strcmp(v, "join")) {
        if (S.phase != PH_LOBBY) return bad("join is only legal in the lobby"); if (S.np >= R_maxp) return bad("the table is full (%ld players)", R_maxp);
        if (!good_word(in->target, 16, 0)) return bad("a name is 1 to 16 letters or digits"); const char *dr = arg2 && *arg2 ? arg2 : "pet"; if (strcmp(dr, "pet") && strcmp(dr, "human")) return bad("a driver is human or pet");
        for (int i = 0; i < S.np; i++) if (!strcmp(S.name[i], in->target)) return bad("the name %s is taken", in->target);
        int i = S.np++; snprintf(S.name[i], 24, "%s", in->target); S.human[i] = !strcmp(dr, "human"); S.ever[i] = S.human[i]; S.pos[i] = 0; S.dir[i] = 0;
        snprintf(out->actor, sizeof out->actor, "p%d", i + 1); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "driver=%s", dr); out->round = 0; return 0; }
    if (!strcmp(v, "start")) {
        if (S.phase != PH_LOBBY) return bad("start is only legal in the lobby"); if (S.np < R_minp) return bad("a game needs %ld to %ld players", R_minp, R_maxp);
        S.phase = PH_PLAY; S.cur = 0; S.round = 1; S.tick = 0; hx(RULES_HASH, hs); S.lastturn_tick = -1;
        if (MODE == M_SNAKE) snake_init(); if (WALKISH) for (int i = 0; i < S.np; i++) S.pos[i] = LV.sx;
        if (MODE == M_PLATFORM) plat_world_init();
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "-"); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "seed=%ld,players=%d,rules=%s", S.seed, S.np, hs); out->round = 0; return 0; }
    if (!strcmp(v, "seat")) {
        int i = pidx(in->target); if (i < 0) return bad("seat: no player %s", in->target); const char *dr = arg2 && *arg2 ? arg2 : ""; if (strcmp(dr, "pet") && strcmp(dr, "human")) return bad("seat: human or pet");
        if (S.phase == PH_OVER) return bad("the game is over"); S.human[i] = !strcmp(dr, "human"); if (S.human[i]) S.ever[i] = 1;
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "0"); snprintf(out->res, sizeof out->res, "driver=%s", dr); out->round = KIND == K_REALTIME ? S.tick : S.round; return 0; }
    if (KIND == K_LEDGER && !strcmp(v, "roll")) {
        if (S.phase == PH_OVER) return bad("the game is over"); if (S.phase != PH_PLAY) return bad("the game has not started");
        char who[8]; snprintf(who, sizeof who, "p%d", S.cur + 1); if (in->actor[0] && strcmp(in->actor, who)) return bad("it is %s's turn, not %s", who, in->actor);
        int d = die(S.seed, S.nrolls, 0); S.nrolls++; long rd0 = S.round; int from = S.pos[S.cur], np = from + d, who_i = S.cur; char res[120]; int n, bounced = 0, stayed = 0, jumped = 0, landing;
        if (np > LENGTH && FINISH == F_STAY) { np = from; stayed = 1; n = snprintf(res, sizeof res, "to=%d;stay", np); }   /* exact finish: an overshoot leaves you where you are */
        else { if (np > LENGTH && FINISH == F_BOUNCE) { np = LENGTH - (np - LENGTH); bounced = 1; }   /* the surplus walks back from the end */
            n = snprintf(res, sizeof res, "to=%d%s", np > LENGTH ? LENGTH : np, bounced ? ";bounce" : "");
            if (np < LENGTH) { int j = jump_at(np); if (j >= 0) { n += snprintf(res + n, sizeof res - (size_t)n, ";jump=%d-%d", np, j); jumped = j - np; np = j; } } }
        landing = stayed ? from : (jumped ? np - jumped : np);
        if (np >= LENGTH) { np = LENGTH; snprintf(res + n, sizeof res - (size_t)n, ";win"); S.winner = S.cur; S.phase = PH_OVER; }
        S.l_valid = 1; S.l_who = who_i; S.l_roll = d; S.l_from = from; S.l_land = landing; S.l_to = np; S.l_win = S.phase == PH_OVER; S.l_human = S.human[who_i];
        S.l_kind = stayed ? 3 : bounced ? 4 : jumped > 0 ? 1 : jumped < 0 ? 2 : 0; if (bounced && jumped) S.l_kind = jumped > 0 ? 5 : 6;   /* 0 plain, 1 ladder, 2 chute, 3 stay, 4 bounce, 5 bounce then ladder, 6 bounce then chute */
        S.pos[S.cur] = np; talk_push(); if (S.phase == PH_PLAY) next_seat();
        snprintf(out->actor, sizeof out->actor, "%s", who); snprintf(out->target, sizeof out->target, "%d", d); snprintf(out->at, sizeof out->at, "%d", np); snprintf(out->res, sizeof out->res, "%s", res); out->round = rd0; return 0; }
    if (KIND == K_REALTIME && MODE == M_SNAKE && (!strcmp(v, "eat") || !strcmp(v, "die") || !strcmp(v, "over"))) {   /* a CONSEQUENCE row: the ticks made it, replay must make the same one (round = the tick it happened at) */
        long t = in->round >= 0 ? in->round : S.tick; sim_to(t); if (S.tick != t) return bad("%s at tick %ld: the game is at tick %ld", v, t, S.tick);
        if (!S.nq) return bad("%s at tick %ld: the rules made no such consequence", v, t);
        Ev e = S.q[0]; if (strcmp(e.verb, v) || e.tick != t) return bad("%s at tick %ld: the rules say the next consequence is %s at tick %ld", v, t, e.verb, e.tick);
        memmove(&S.q[0], &S.q[1], (size_t)(S.nq - 1) * sizeof S.q[0]); S.nq--;
        snprintf(out->actor, sizeof out->actor, "bank"); snprintf(out->target, sizeof out->target, "%s", e.target); snprintf(out->at, sizeof out->at, "%s", e.at); snprintf(out->res, sizeof out->res, "%s", e.res); out->round = t; return 0; }
    if (KIND == K_REALTIME && MODE == M_SNAKE && !strcmp(v, "turn")) {
        if (S.phase == PH_OVER) return bad("the game is over"); if (S.phase != PH_PLAY) return bad("the game has not started");
        if (pidx(in->actor) != 0) return bad("turn: no player %s", in->actor);
        char tc = turn_dir(in->target); int dir = tc == 'L' ? 0 : tc == 'R' ? 1 : tc == 'U' ? 2 : tc == 'D' ? 3 : -1; if (dir < 0) return bad("turn: L, R, U, D or an arrow code 1000-1003");
        long t = in->round >= 0 ? in->round : S.tick; if (t < S.tick) return bad("turn at tick %ld is in the past (the game is at tick %ld)", t, S.tick); if (t > S.tick + 100000) return bad("turn: tick too far ahead");
        sim_to(t); if (S.phase != PH_PLAY) return bad("the game ended at tick %ld, before this turn", S.tick); if (S.tick != t) return bad("turn: could not reach tick %ld", t);
        if (S.nq) return bad("turn at tick %ld: the ticks before it made a %s row that no row recorded (use advance)", t, S.q[0].verb);
        if (S.lastturn_tick == t) return bad("turn: one turn per tick"); if (dir == S.sdir) return bad("turn: already heading %c", tc); if (S.slen > 1 && dir == OPP[S.sdir]) return bad("turn: a reversal into the neck is refused");
        S.sdir = dir; S.lastturn_tick = t; snprintf(out->actor, sizeof out->actor, "%s", in->actor); snprintf(out->target, sizeof out->target, "%c", tc); snprintf(out->at, sizeof out->at, "%d,%d", S.bx[0], S.by[0]); snprintf(out->res, sizeof out->res, "applies=%ld", t); out->round = t; return 0; }
    if (KIND == K_REALTIME && MODE != M_SNAKE && !strcmp(v, "input")) {
        if (S.phase == PH_OVER) return bad("the game is over"); if (S.phase != PH_PLAY) return bad("the game has not started");
        int i = pidx(in->actor); if (i < 0) return bad("input: no player %s", in->actor);
        int isjump = MODE == M_PLATFORM && !strcmp(in->target, "jump"), isfire = MODE == M_PLATFORM && !strcmp(in->target, "fire"); int dir = (isjump || isfire) ? 0 : !strcmp(in->target, "left") ? -1 : !strcmp(in->target, "right") ? 1 : !strcmp(in->target, "stay") ? 0 : 9; if (dir == 9) return bad(MODE == M_PLATFORM ? "input: left, right, stay, jump or fire" : "input: left, right or stay");
        long t = in->round >= 0 ? in->round : S.tick; if (t < S.tick) return bad("input at tick %ld is in the past (the game is at tick %ld)", t, S.tick); if (t > S.tick + 100000) return bad("input: tick too far ahead");
        sim_to(t); if (S.phase != PH_PLAY) return bad("the game ended at tick %ld, before this input", S.tick); if (S.tick != t) return bad("input: could not reach tick %ld", t);
        if (isjump) S.jp[i] = 1; else if (isfire) S.fire[i] = 1; else S.dir[i] = dir; snprintf(out->actor, sizeof out->actor, "%s", in->actor); snprintf(out->target, sizeof out->target, "%s", in->target); snprintf(out->at, sizeof out->at, "%d", S.pos[i]); snprintf(out->res, sizeof out->res, "ok"); out->round = t; return 0; }
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
        else if (KIND == K_EDITOR && (!strcmp(fl[3], "add") || !strcmp(fl[3], "set") || !strcmp(fl[3], "clear"))) { snprintf(a2, sizeof a2, "%s", fl[6]); arg2 = a2; }
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
        S.nrows++; if (ROWHOOK) ROWHOOK(lineno, &out); }
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
    if (KIND == K_LEDGER) {   /* the last roll and the last five table-talk sentences (newest first): projections of the replayed rows */
        static const char *KN[7] = { "none", "ladder", "chute", "stay", "bounce", "ladder_after_bounce", "chute_after_bounce" };
        n += snprintf(o + n, cap - (size_t)n, "finish=%s\nlast_valid=%s\n", FINISH == F_STAY ? "exact_stay" : FINISH == F_BOUNCE ? "bounce" : "reach", S.l_valid ? "1" : "");
        if (S.l_valid) n += snprintf(o + n, cap - (size_t)n, "last_who=p%d\nlast_roll=%d\nlast_from=%d\nlast_landed=%d\nlast_to=%d\nlast_kind=%s\n", S.l_who + 1, S.l_roll, S.l_from, S.l_land, S.l_to, KN[S.l_kind]);
        int nt = S.ntk < 5 ? (int)S.ntk : 5; n += snprintf(o + n, cap - (size_t)n, "talk_n=%d\n", nt); for (int i = 0; i < nt; i++) n += snprintf(o + n, cap - (size_t)n, "talk_%d=%s\n", i, S.tk[(S.ntk - 1 - i) % 5]);
    }
    if (KIND == K_REALTIME && MODE == M_SNAKE) {
        n += snprintf(o + n, cap - (size_t)n, "cols=%d\nrows_n=%d\nsnake_len=%d\nsnake_grow=%d\nsnake_dir=%c\nscore=%d\nfood=%d,%d\nfoods_spawned=%d\ncells=", COLS, ROWS, S.slen, S.sgrow, DCH[S.sdir], S.sscore, S.sfood_x, S.sfood_y, S.sfoods);
        for (int i = 0; i < S.slen && n < (int)cap - 40; i++) n += snprintf(o + n, cap - (size_t)n, "%s%d,%d", i ? ";" : "", S.bx[i], S.by[i]); n += snprintf(o + n, cap - (size_t)n, "\n");
    }
    if (KIND == K_REALTIME && WALKISH) n += snprintf(o + n, cap - (size_t)n, "level=%s\nlevel_w=%d\nlevel_h=%d\ngoal=%d,%d\nstart=%d,%d\n", LV.name, LV.w, LV.h, LV.gx, LV.gy, LV.sx, LV.sy);
    if (KIND == K_REALTIME && MODE == M_PLATFORM) { n += snprintf(o + n, cap - (size_t)n, "falls=%ld\n", S.falls); for (int i = 0; i < S.np; i++) n += snprintf(o + n, cap - (size_t)n, "body%d=%ld,%ld,%ld,%ld,%d\n", i + 1, S.pb[i].x, S.pb[i].y, S.pb[i].vx, S.pb[i].vy, S.pb[i].grounded); }
    return n;
}
static int write_atomic(const char *path, const char *data) { char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return -1; fputs(data, f); if (fclose(f)) return -1; return rename(tmp, path); }
static int write_ui(void) { static char b[16000]; build_ui(b, sizeof b); char p[P + 16]; snprintf(p, sizeof p, "%s/ui.txt", SD); mkdir_p(SD); return write_atomic(p, b); }
static int write_doc(void) { static char b[16000]; int n = snprintf(b, sizeof b, "# doc.txt - a projection of ledger.txt (verb export); do not edit\n"); for (int i = 0; i < S.ndoc; i++) n += snprintf(b + n, sizeof b - (size_t)n, "ROW|%s|%s\n", S.did[i], S.dtx[i]); snprintf(b + n, sizeof b - (size_t)n, "PUBLISHED|%s\n", S.published ? S.pubhash : "-"); char p[P + 16]; snprintf(p, sizeof p, "%s/doc.txt", SD); mkdir_p(SD);
    if (LEVELDOC) {   /* a level document also has the level file as a projection: the SAME text Play reads (a half-made document has no level yet, and then no level.pdl) */
        char lp[P + 16]; snprintf(lp, sizeof lp, "%s/level.pdl", SD); static char lb[8192]; MSG[0] = 0; if (!level_from_doc() && level_render(lb, sizeof lb) > 0) write_atomic(lp, lb); else unlink(lp); }
    return write_atomic(p, b); }

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

/* ---------- export: the data studio's tables (data-hq `GAME | export`; BOOK 16.game GAME-FOUNDATION-GENERATOR guide) ---------- */
/* `game_core <app> <state> export <out_dir>`: replays the ledger with the engine's own replay (a ledger that does not reproduce itself is refused, exit 1, nothing written) and writes PROJECTIONS into <out_dir>, never into
 * the state folder: game.csv (ONE row, a game_id column: what `data_op index` reads), events.csv (one row per ledger row), moves.csv (ledger kind: one row per roll), schema.pdl (TABLE and COL rows, roles id/category/measure/time/text).
 * Deterministic: the same ledger gives the same bytes. Read-only on the state folder. Refused: an out folder inside the state folder, one that holds a ledger.txt. */
typedef struct { char *p; size_t n, cap; } Buf;
static Buf XEV, XMV;
static void bcat(Buf *b, const char *s) { size_t l = strlen(s); if (b->n + l + 1 > b->cap) { b->cap = (b->n + l + 1) * 2 + 4096; b->p = realloc(b->p, b->cap); } memcpy(b->p + b->n, s, l + 1); b->n += l; }
static void bcell(Buf *b, const char *s, int last) { if (strpbrk(s, ",\"\r\n")) { bcat(b, "\""); for (; *s; s++) { char c[2] = { *s, 0 }; if (*s == '"') bcat(b, "\"\""); else bcat(b, c); } bcat(b, "\""); } else bcat(b, s); bcat(b, last ? "\n" : ","); }
static void export_hook(int lineno, const Row *r) {
    char nb[32]; snprintf(nb, sizeof nb, "%d", lineno); bcell(&XEV, nb, 0); snprintf(nb, sizeof nb, "%ld", r->round); bcell(&XEV, nb, 0); bcell(&XEV, r->actor, 0); bcell(&XEV, r->verb, 0); bcell(&XEV, r->target, 0); bcell(&XEV, r->at, 0); bcell(&XEV, r->res, 1);
    if (KIND == K_LEDGER && !strcmp(r->verb, "roll") && S.l_valid) { static const char *KN[7] = { "plain", "ladder", "chute", "stay", "bounce", "ladder_after_bounce", "chute_after_bounce" };
        snprintf(nb, sizeof nb, "%ld", S.nrolls); bcell(&XMV, nb, 0); snprintf(nb, sizeof nb, "%ld", r->round); bcell(&XMV, nb, 0); snprintf(nb, sizeof nb, "p%d", S.l_who + 1); bcell(&XMV, nb, 0); bcell(&XMV, S.name[S.l_who], 0); bcell(&XMV, S.l_human ? "human" : "pet", 0);
        snprintf(nb, sizeof nb, "%d", S.l_roll); bcell(&XMV, nb, 0); snprintf(nb, sizeof nb, "%d", S.l_from); bcell(&XMV, nb, 0); snprintf(nb, sizeof nb, "%d", S.l_land); bcell(&XMV, nb, 0); snprintf(nb, sizeof nb, "%d", S.l_to); bcell(&XMV, nb, 0);
        bcell(&XMV, KN[S.l_kind], 0); bcell(&XMV, S.l_win ? "1" : "0", 1); }
}
typedef struct { const char *file, *name, *role, *unit, *hint; int mask; } XCol;   /* mask: 1 ledger, 2 realtime, 4 editor */
static const XCol XC[] = {
    {"game.csv", "game_id", "id", "", "FNV-1a of the ledger rows (the ledger_hash verb), 16 hex", 7}, {"game.csv", "kind", "category", "", "ledger, realtime or editor", 7}, {"game.csv", "ruleset", "category", "", "ruleset folder named by game.pdl", 7},
    {"game.csv", "rules_hash", "id", "", "FNV-1a of board.pdl, rules.pdl (and the level file) recorded in the first row", 7}, {"game.csv", "seed", "id", "", "seed of the first row", 7},
    {"game.csv", "players", "measure", "players", "seats at the table", 7}, {"game.csv", "humans_now", "measure", "players", "seats driven by a human at the end", 7}, {"game.csv", "human_ever", "measure", "players", "seats that were human at any time; 0 = pets only", 7},
    {"game.csv", "ledger_rows", "measure", "rows", "ledger rows replayed", 7}, {"game.csv", "rounds", "measure", "rounds", "ledger: the round counter; realtime: the tick; editor: the edit counter", 7},
    {"game.csv", "status", "category", "", "over or open (editor: published or open)", 7}, {"game.csv", "outcome", "category", "", "winner, nobody (everybody lost), open; editor: published or open", 7},
    {"game.csv", "winner", "category", "", "player id of the winner, blank while open", 3}, {"game.csv", "winner_driver", "category", "", "human or pet (the final seat state)", 3},
    {"game.csv", "state_hash", "id", "", "FNV-1a of the replayed state (the replay verb hash)", 7},
    {"game.csv", "track_length", "measure", "squares", "squares on the track", 1}, {"game.csv", "finish", "category", "", "reach, exact_stay or bounce", 1}, {"game.csv", "jumps", "measure", "count", "JUMP rows in board.pdl", 1}, {"game.csv", "rolls", "measure", "count", "dice rolled", 1},
    {"game.csv", "mode", "category", "", "track, snake or walk", 2}, {"game.csv", "ticks", "measure", "ticks", "logical ticks reached", 2}, {"game.csv", "score", "measure", "points", "snake score (blank in other modes)", 2},
    {"game.csv", "doc_rows", "measure", "rows", "rows in the document", 4}, {"game.csv", "edits", "measure", "count", "edit rows", 4},
    {"events.csv", "seq", "id", "", "ledger line number", 7}, {"events.csv", "round", "time", "", "ledger: round; realtime: the tick; editor: the edit number", 7}, {"events.csv", "actor", "category", "", "p1.., bank or the author", 7},
    {"events.csv", "verb", "category", "", "the ledger verb", 7}, {"events.csv", "target", "text", "", "the row target", 7}, {"events.csv", "at", "text", "", "the row's place field", 7}, {"events.csv", "result", "text", "", "the row's result field", 7},
    {"moves.csv", "roll_no", "id", "", "which roll of the game (1..)", 1}, {"moves.csv", "round", "time", "round", "the round the roll was made in", 1}, {"moves.csv", "seat", "category", "", "player id", 1}, {"moves.csv", "name", "category", "", "player name", 1}, {"moves.csv", "driver", "category", "", "human or pet at that moment", 1},
    {"moves.csv", "roll", "measure", "pips", "the die", 1}, {"moves.csv", "from", "measure", "square", "square before the roll", 1}, {"moves.csv", "landed", "measure", "square", "square the roll ended on, before any jump", 1}, {"moves.csv", "to", "measure", "square", "square after the jump", 1},
    {"moves.csv", "kind", "category", "", "plain, ladder, chute, stay, bounce, ladder_after_bounce, chute_after_bounce", 1}, {"moves.csv", "win", "measure", "", "1 on the winning roll", 1},
};
static int cmd_export(const char *outdir) {
    char od[P]; if (outdir[0] != '/') { char cw[P]; if (!getcwd(cw, sizeof cw)) { fprintf(ERRF, "export: getcwd\n"); return 2; } snprintf(od, sizeof od, "%s/%s", cw, outdir); } else snprintf(od, sizeof od, "%s", outdir);
    char sdr[P]; if (!realpath(SD, sdr)) snprintf(sdr, sizeof sdr, "%s", SD); { char dr[P]; char *sl = strrchr(od, '/'); if (sl && sl != od) { *sl = 0; if (realpath(od, dr)) snprintf(od, sizeof od, "%s/%s", dr, sl + 1); else *sl = '/'; } }   /* compare real paths: a relative state folder or a symlink must not hide an out folder inside the game */
    size_t L = strlen(sdr); if (!strncmp(od, sdr, L) && (od[L] == 0 || od[L] == '/')) { fprintf(ERRF, "export refused: the out folder is inside the game's state folder (the studio never writes beside a game)\n"); return 2; }
    { char lp[P + 16]; snprintf(lp, sizeof lp, "%s/ledger.txt", od); if (!access(lp, F_OK)) { fprintf(ERRF, "export refused: %s holds a ledger.txt (an out folder is never a game folder)\n", od); return 2; } }
    if (S.nrows == 0) { fprintf(ERRF, "export refused: no game here (the ledger is empty)\n"); return 2; }
    int mask = KIND == K_LEDGER ? 1 : KIND == K_REALTIME ? 2 : 4; char v[64], hs[20]; Buf G = { 0 }, SC = { 0 };
    int hnow = 0, hever = 0; for (int i = 0; i < S.np; i++) { hnow += S.human[i]; hever += S.ever[i]; }
    for (unsigned i = 0; i < sizeof XC / sizeof *XC; i++) if (!strcmp(XC[i].file, "game.csv") && (XC[i].mask & mask)) bcell(&G, XC[i].name, 0); G.p[G.n - 1] = '\n';   /* header: the last comma becomes the end of line */
    for (unsigned i = 0; i < sizeof XC / sizeof *XC; i++) { if (strcmp(XC[i].file, "game.csv") || !(XC[i].mask & mask)) continue; const char *nm = XC[i].name; int last = 0; (void)last; v[0] = 0;
        if (!strcmp(nm, "game_id")) hx(ledger_hash(NULL), v); else if (!strcmp(nm, "kind")) snprintf(v, sizeof v, "%s", KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor"); else if (!strcmp(nm, "ruleset")) snprintf(v, sizeof v, "%s", RS);
        else if (!strcmp(nm, "rules_hash")) hx(RULES_HASH, v); else if (!strcmp(nm, "seed")) snprintf(v, sizeof v, "%ld", S.seed); else if (!strcmp(nm, "players")) snprintf(v, sizeof v, "%d", S.np); else if (!strcmp(nm, "humans_now")) snprintf(v, sizeof v, "%d", hnow);
        else if (!strcmp(nm, "human_ever")) snprintf(v, sizeof v, "%d", hever); else if (!strcmp(nm, "ledger_rows")) snprintf(v, sizeof v, "%d", S.nrows);
        else if (!strcmp(nm, "rounds")) snprintf(v, sizeof v, "%ld", KIND == K_LEDGER ? S.round : KIND == K_REALTIME ? S.tick : S.nedits);
        else if (!strcmp(nm, "status")) snprintf(v, sizeof v, "%s", KIND == K_EDITOR ? (S.published ? "published" : "open") : S.phase == PH_OVER ? "over" : "open");
        else if (!strcmp(nm, "outcome")) snprintf(v, sizeof v, "%s", KIND == K_EDITOR ? (S.published ? "published" : "open") : S.phase != PH_OVER ? "open" : S.winner >= 0 ? "winner" : "nobody");
        else if (!strcmp(nm, "winner")) { if (S.phase == PH_OVER && S.winner >= 0) snprintf(v, sizeof v, "p%d", S.winner + 1); } else if (!strcmp(nm, "winner_driver")) { if (S.phase == PH_OVER && S.winner >= 0) snprintf(v, sizeof v, "%s", S.human[S.winner] ? "human" : "pet"); }
        else if (!strcmp(nm, "state_hash")) hx(state_hash(), v); else if (!strcmp(nm, "track_length")) snprintf(v, sizeof v, "%d", LENGTH); else if (!strcmp(nm, "finish")) snprintf(v, sizeof v, "%s", FINISH == F_STAY ? "exact_stay" : FINISH == F_BOUNCE ? "bounce" : "reach");
        else if (!strcmp(nm, "jumps")) snprintf(v, sizeof v, "%d", NJ); else if (!strcmp(nm, "rolls")) snprintf(v, sizeof v, "%ld", S.nrolls); else if (!strcmp(nm, "mode")) snprintf(v, sizeof v, "%s", MODE == M_SNAKE ? "snake" : MODE == M_WALK ? "walk" : MODE == M_PLATFORM ? "platform" : "track");
        else if (!strcmp(nm, "ticks")) snprintf(v, sizeof v, "%ld", S.tick); else if (!strcmp(nm, "score")) { if (MODE == M_SNAKE) snprintf(v, sizeof v, "%d", S.sscore); } else if (!strcmp(nm, "doc_rows")) snprintf(v, sizeof v, "%d", S.ndoc); else if (!strcmp(nm, "edits")) snprintf(v, sizeof v, "%ld", S.nedits);
        bcell(&G, v, 0); }
    G.p[G.n - 1] = '\n';
    bcat(&SC, "# schema.pdl - made by game_core export; the columns of the tables beside it. Roles: id, category, measure, time, text (data-hq).\n");
    const char *files[3] = { "game.csv", "events.csv", "moves.csv" }; const char *what[3] = { "one row per game", "one row per ledger row", "one row per roll" };
    for (int f = 0; f < 3; f++) { if (f == 2 && !(mask & 1)) continue; char tl[300]; snprintf(tl, sizeof tl, "TABLE | %s | %s\n", files[f], what[f]); bcat(&SC, tl);
        for (unsigned i = 0; i < sizeof XC / sizeof *XC; i++) if (!strcmp(XC[i].file, files[f]) && (XC[i].mask & mask)) { char cl[400]; snprintf(cl, sizeof cl, "COL | %s | %s | %s | %s\n", XC[i].name, XC[i].role, XC[i].unit, XC[i].hint); bcat(&SC, cl); } }
    mkdir_p(od); char fp[P + 32], *hdr; Buf EH = { 0 }, MH = { 0 };
    for (unsigned i = 0; i < sizeof XC / sizeof *XC; i++) { if (!(XC[i].mask & mask)) continue; if (!strcmp(XC[i].file, "events.csv")) bcell(&EH, XC[i].name, 0); else if (!strcmp(XC[i].file, "moves.csv")) bcell(&MH, XC[i].name, 0); }
    EH.p[EH.n - 1] = '\n'; if (MH.p) MH.p[MH.n - 1] = '\n'; (void)hdr;
    int bad_w = 0; snprintf(fp, sizeof fp, "%s/game.csv", od); bad_w |= write_atomic(fp, G.p); snprintf(fp, sizeof fp, "%s/schema.pdl", od); bad_w |= write_atomic(fp, SC.p);
    { Buf E2 = { 0 }; bcat(&E2, EH.p); if (XEV.p) bcat(&E2, XEV.p); snprintf(fp, sizeof fp, "%s/events.csv", od); bad_w |= write_atomic(fp, E2.p); free(E2.p); }
    if (mask & 1) { Buf M2 = { 0 }; bcat(&M2, MH.p); if (XMV.p) bcat(&M2, XMV.p); snprintf(fp, sizeof fp, "%s/moves.csv", od); bad_w |= write_atomic(fp, M2.p); free(M2.p); }
    free(G.p); free(SC.p); free(EH.p); free(MH.p); free(XEV.p); free(XMV.p); (void)hs;
    if (bad_w) { fprintf(ERRF, "export: cannot write into %s\n", od); return 2; }
    fprintf(OUT, "EXPORT ok rows=%d out=%s files=%d\n", S.nrows, od, (mask & 1) ? 4 : 3); return 0;
}
/* run <argv> with stdout+stderr captured (fork/exec, no shell, 10 s watchdog); returns the exit code, -1 on a failure to run. Short-lived: no process-registry row needed. */
static int run_capture(char *const argv[], char *out, size_t cap) {
    int pf[2]; if (pipe(pf)) return -1; pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { setpgid(0, 0); dup2(pf[1], 1); dup2(pf[1], 2); close(pf[0]); close(pf[1]); execv(argv[0], argv); _exit(127); }
    close(pf[1]); size_t on = 0; long t0 = now_ms(); int status = 0, done = 0, killed = 0;
    while (!done) { struct pollfd p = { pf[0], POLLIN, 0 }; int pr = poll(&p, 1, 100); if (pr > 0) { ssize_t k = read(pf[0], out + on, cap - 1 - on); if (k > 0) on += (size_t)k; else { waitpid(pid, &status, 0); done = 1; continue; } if (on >= cap - 1) { kill(-pid, SIGKILL); waitpid(pid, &status, 0); killed = 1; break; } }
        if (now_ms() - t0 > 10000) { kill(-pid, SIGKILL); waitpid(pid, &status, 0); killed = 1; break; } }
    close(pf[0]); out[on] = 0; if (killed) return -1; return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* read the new key codes of a relay file from the byte cursor *off (the same reader as petslike_core.c poll_keys: "<decimal code> <ms>" lines and "KEY_PRESSED: n" lines; a file shorter than the cursor was truncated, start again). Nothing before the cursor is ever read. */
static int keys_poll(const char *p, long *off, int *codes, int max) {
    struct stat st; if (stat(p, &st)) return 0; if (st.st_size < *off) *off = 0; if (st.st_size == *off) return 0; FILE *f = fopen(p, "r"); if (!f) return 0; fseek(f, *off, SEEK_SET); char l[80]; int n = 0;
    while (n < max && fgets(l, sizeof l, f)) { if (!strncmp(l, "KEY_PRESSED:", 12)) { codes[n++] = atoi(l + 12); continue; } if (l[0] == '#' || l[0] < '0' || l[0] > '9') continue; codes[n++] = atoi(l); }
    *off = ftell(f); fclose(f); return n;
}
/* steer the (human) snake with arrow codes: one turn row per accepted key, stamped with the CURRENT tick; a refused one (the neck, the same direction, a second turn in the tick) is counted and appends nothing */
static int keys_apply(const int *codes, int n, int *refused, int *ignored) {
    int applied = 0; *refused = 0; *ignored = 0;
    for (int i = 0; i < n; i++) { char d = key_dir(codes[i]); if (!d) { (*ignored)++; continue; } char dc[2] = { d, 0 }; if (live(-1, "p1", "turn", dc, NULL, NULL)) (*refused)++; else applied++; }
    return applied;
}
/* mode platform keys (the press-impulse model: the key IO has no release line, so a direction key LATCHES until another one): arrow left / right = run that way, down = stay, up or space (32) = a jump pulse; one `input` row per accepted key, stamped with the current tick */
static int plat_keys_apply(const int *codes, int n) {
    int applied = 0;
    for (int i = 0; i < n; i++) { char d = key_dir(codes[i]); const char *t = d == 'L' ? "left" : d == 'R' ? "right" : d == 'D' ? "stay" : (d == 'U' || codes[i] == 32) ? "jump" : (codes[i] == 102 || codes[i] == 120) ? "fire" : NULL; if (t && !live(-1, "p1", "input", t, NULL, NULL)) applied++; }
    return applied;
}
/* ---------- pace: tuning.pdl `TUNE | tick_ms | N` (2026-11-09, BOOK 16.game/REALTIME-SPIKES-HELD-KEYS-TICK-RATE-PICTURE-RATE-2026-11-09.md) ----------
 * tick_ms = milliseconds per step (a roll for a ledger game, a logical tick for a realtime game) at the "normal" speed button. It is a CONFIG/PREFERENCE of the window: it is NOT in the rules hash and never in the ledger
 * (ticks are logical, so replay and the final hash are the same at 33 ms and at 2000 ms). The fast / slow buttons scale it by the kind's own ratios (120:550:1400 ledger, 40:100:250 realtime) and the result is never under 33 ms (30 ticks a second).
 * Absent row = the kind's default (550 ledger, 100 realtime). A row outside 33..2000, or not a whole number, is REFUSED (verb tuning exit 2, the driver does not start). */
#define TICK_MIN 33
#define TICK_MAX 2000
static long TICK_MS;   /* 0 = the kind's default */
static const int PACE_LEDGER[3] = { 120, 550, 1400 }, PACE_RT[3] = { 40, 100, 250 };
static int load_tuning(void) {
    char p[P], ln[512], *fl[8]; TICK_MS = 0; snprintf(p, sizeof p, "%s/tuning.pdl", APP); FILE *f = fopen(p, "r"); if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split_row(ln, fl, 8); if (n < 3 || strcmp(fl[0], "TUNE") || strcmp(fl[1], "tick_ms")) continue;
        char *e; long v = strtol(fl[2], &e, 10); if (!fl[2][0] || *e || v < TICK_MIN || v > TICK_MAX) { fclose(f); return bad("tuning.pdl: tick_ms must be a whole number of milliseconds from %d (30 steps a second) to %d, got '%.20s'", TICK_MIN, TICK_MAX, fl[2]); }
        TICK_MS = v; }
    fclose(f); return 0;
}
static long pace_ms(int sp) { const int *t = KIND == K_REALTIME ? PACE_RT : PACE_LEDGER; long base = TICK_MS ? TICK_MS : t[1]; long v = base * t[sp] / t[1]; return v < TICK_MIN ? TICK_MIN : v; }
static long now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000000L + t.tv_nsec / 1000L; }

/* ---------- the verbs ---------- */
static const char *pet_dir(int i) { int d = die(S.seed, S.tick * 16 + i, 5); return d <= 3 || d == 6 ? "right" : d == 4 ? "stay" : "left"; }
/* the pet's steering for a snake (a ledger fact as soon as it is written as a turn row): never into the neck, never into a wall or its own body if it can help it, and toward the food; ties by the seeded die */
static int snake_pet(void) {
    int best = S.sdir, bs = -100000; int growing = S.sgrow > 0;
    for (int d = 0; d < 4; d++) { if (S.slen > 1 && d == OPP[S.sdir]) continue; int nx = S.bx[0] + DX[d], ny = S.by[0] + DY[d]; int dead = 0;
        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { if (WRAP) { nx = (nx + COLS) % COLS; ny = (ny + ROWS) % ROWS; } else dead = 1; }
        if (!dead && on_body(nx, ny, growing ? S.slen : S.slen - 1)) dead = 1;
        int dist = abs(nx - S.sfood_x) + abs(ny - S.sfood_y), sc = (dead ? -1000 : 0) - dist * 10 + (d == S.sdir ? 3 : 0) + die(S.seed, S.tick * 16 + d, 6) % 3;
        if (sc > bs) { bs = sc; best = d; } }
    return best;
}
static int do_tick(int *did) {   /* ONE step. *did = 1 when the game moved. A human seat in a ledger game makes tick wait (exit 0, nothing written). */
    *did = 0;
    if (S.phase != PH_PLAY) return bad("the game is %s", S.phase == PH_OVER ? "over" : "not started");
    if (KIND == K_LEDGER) { if (S.human[S.cur]) return 0; if (live(-1, "", "roll", "", NULL, NULL)) return 1; *did = 1; return 0; }
    if (KIND == K_REALTIME && MODE == M_SNAKE) {   /* a pet steers by writing a turn row; the step makes its consequence rows (eat, die, over) and each is appended as a row; a ckpt row every ckpt_every ticks and at the end */
        if (S.nrows >= R_maxrows - 6) return bad("the ledger is full (max_rows %ld)", R_maxrows);
        if (S.tick >= R_maxticks) return bad("max_ticks %ld reached", R_maxticks);
        if (!S.human[0]) { int d = snake_pet(); if (d != S.sdir) { char dc[2] = { DCH[d], 0 }; if (live(S.tick, "p1", "turn", dc, NULL, NULL)) return 1; } }
        step_tick(); *did = 1; while (S.nq) { if (live(-1, "", S.q[0].verb, "", NULL, NULL)) return 1; }
        if (S.phase == PH_OVER || S.tick % R_ckpt == 0) { if (live(-1, "", "ckpt", "", NULL, NULL)) return 1; } return 0; }
    if (KIND == K_REALTIME) { if (S.nrows >= R_maxrows - 2) return bad("the ledger is full (max_rows %ld)", R_maxrows);
        if (S.tick % 4 == 0) for (int i = 0; i < S.np; i++) if (!S.human[i]) { char a[8]; snprintf(a, sizeof a, "p%d", i + 1); if (strcmp(pet_dir(i), S.dir[i] == 1 ? "right" : S.dir[i] == -1 ? "left" : "stay")) if (live(-1, a, "input", pet_dir(i), NULL, NULL)) return 1; }
        if (MODE == M_PLATFORM && S.tick % 10 == 3) for (int i = 0; i < S.np; i++) if (!S.human[i]) { char a[8]; snprintf(a, sizeof a, "p%d", i + 1); if (live(-1, a, "input", "jump", NULL, NULL)) return 1; }
        step_tick(); *did = 1; if (S.phase == PH_OVER || S.tick % R_ckpt == 0) { if (live(-1, "", "ckpt", "", NULL, NULL)) return 1; } return 0; }
    return bad("tick is not a verb of an editor game");
}
static int cmd_status(void) {
    fprintf(OUT, "STATE kind=%s phase=%s rows=%d", KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", phase_name(), S.nrows);
    if (KIND == K_EDITOR) { fprintf(OUT, " doc_rows=%d edits=%ld published=%s\n", S.ndoc, S.nedits, S.published ? S.pubhash : "no"); for (int i = 0; i < S.ndoc; i++) fprintf(OUT, "ROW %s %s\n", S.did[i], S.dtx[i]); return 0; }
    if (KIND == K_REALTIME && MODE == M_SNAKE) { fprintf(OUT, " tick=%ld over=%d winner=%s\n", S.tick, S.phase == PH_OVER, S.phase == PH_OVER && S.winner >= 0 ? S.name[S.winner] : "-"); for (int i = 0; i < S.np; i++) fprintf(OUT, "PLAYER p%d name=%s driver=%s\n", i + 1, S.name[i], S.human[i] ? "human" : "pet");
        if (S.phase >= PH_PLAY) fprintf(OUT, "SNAKE len=%d grow=%d dir=%c score=%d head=%d,%d food=%d,%d foods=%d\n", S.slen, S.sgrow, DCH[S.sdir], S.sscore, S.bx[0], S.by[0], S.sfood_x, S.sfood_y, S.sfoods); return 0; }
    if (KIND == K_LEDGER) fprintf(OUT, " round=%ld current=p%d rolls=%ld", S.round, S.cur + 1, S.nrolls); else fprintf(OUT, " tick=%ld", S.tick);
    fprintf(OUT, " over=%d winner=%s\n", S.phase == PH_OVER, S.phase == PH_OVER && S.winner >= 0 ? S.name[S.winner] : "-");
    for (int i = 0; i < S.np; i++) { fprintf(OUT, "PLAYER p%d name=%s driver=%s pos=%d", i + 1, S.name[i], S.human[i] ? "human" : "pet", S.pos[i]); if (MODE == M_PLATFORM) fprintf(OUT, " body=%ld,%ld,%ld,%ld grounded=%d falls=%ld score=%ld stomps=%ld kills=%ld hits=%ld armed=%d enemies=%d", S.pb[i].x, S.pb[i].y, S.pb[i].vx, S.pb[i].vy, S.pb[i].grounded, S.falls, S.score, S.stomps, S.shots_kills, S.hits, S.armed[i], ({ int c_ = 0; for (int e_ = 0; e_ < LV.ne; e_++) c_ += S.en[e_].alive; c_; })); fprintf(OUT, "\n"); } if (MODE == M_PLATFORM) for (int e = 0; e < LV.ne; e++) fprintf(OUT, "ENEMY e%d body=%ld,%ld dir=%d alive=%d\n", e + 1, S.en[e].b.x, S.en[e].b.y, S.en[e].dir, S.en[e].alive); if (MODE == M_PLATFORM) for (int k = 0; k < 4; k++) if (S.fb[k].alive) fprintf(OUT, "FIREBALL f%d at=%ld,%ld vx=%ld life=%d\n", k + 1, S.fb[k].x, S.fb[k].y, S.fb[k].vx, S.fb[k].life); return 0;
}
static int cmd_check(void) {   /* replay already ran (load); the invariants */
    if (KIND == K_REALTIME && MODE == M_SNAKE) {
        if (S.phase >= PH_PLAY) { if (S.slen < 1 || S.slen + S.sgrow > COLS * ROWS) { fprintf(ERRF, "CHECK FAIL the snake has %d segments on a %dx%d board\n", S.slen, COLS, ROWS); return 1; }
            for (int i = 0; i < S.slen; i++) { if (S.bx[i] < 0 || S.bx[i] >= COLS || S.by[i] < 0 || S.by[i] >= ROWS) { fprintf(ERRF, "CHECK FAIL segment %d is off the board\n", i); return 1; } for (int j = i + 1; j < S.slen; j++) if (S.bx[i] == S.bx[j] && S.by[i] == S.by[j]) { if (S.phase == PH_OVER) continue; fprintf(ERRF, "CHECK FAIL segments %d and %d share a cell\n", i, j); return 1; } }
            if (S.phase == PH_PLAY && on_body(S.sfood_x, S.sfood_y, S.slen)) { fprintf(ERRF, "CHECK FAIL the food is inside the snake\n"); return 1; } }
        fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0; }
    if (KIND == K_REALTIME && MODE == M_PLATFORM) { for (int i = 0; i < S.np; i++) { const PF_Body *b = &S.pb[i]; if (pf_hits(b, b->x, b->y, plat_solid, NULL)) { fprintf(ERRF, "CHECK FAIL p%d is inside a solid tile\n", i + 1); return 1; } }
        if (S.phase == PH_OVER && S.winner < 0) { fprintf(ERRF, "CHECK FAIL over without a winner\n"); return 1; } fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0; }
    if (KIND == K_REALTIME && MODE == M_WALK) { for (int i = 0; i < S.np; i++) if (S.pos[i] < 0 || S.pos[i] >= LV.w || is_solid(LV.rows[LV.sy][S.pos[i]])) { fprintf(ERRF, "CHECK FAIL p%d stands on column %d which is outside the level or solid\n", i + 1, S.pos[i]); return 1; }
        if (S.phase == PH_OVER && (S.winner < 0 || S.pos[S.winner] != LV.gx)) { fprintf(ERRF, "CHECK FAIL the winner is not on the goal column\n"); return 1; } fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0; }
    for (int i = 0; i < S.np; i++) if (S.pos[i] < 0 || S.pos[i] > LENGTH) { fprintf(ERRF, "CHECK FAIL p%d is off the track (%d)\n", i + 1, S.pos[i]); return 1; }
    if (S.phase == PH_OVER && KIND != K_EDITOR) { if (S.winner < 0 || S.pos[S.winner] != LENGTH) { fprintf(ERRF, "CHECK FAIL the winner is not at the end of the track\n"); return 1; } int at_end = 0; for (int i = 0; i < S.np; i++) if (S.pos[i] >= LENGTH) at_end++; if (KIND == K_LEDGER && at_end != 1) { fprintf(ERRF, "CHECK FAIL %d players at the end of a ledger game\n", at_end); return 1; } }
    if (KIND == K_EDITOR) { if (S.published) { /* the last publish row recorded the doc hash then; later edits may differ, that is allowed */ } for (int i = 0; i < S.ndoc; i++) for (int j = i + 1; j < S.ndoc; j++) if (!strcmp(S.did[i], S.did[j])) { fprintf(ERRF, "CHECK FAIL duplicate id %s\n", S.did[i]); return 1; } }
    fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0;
}
static int need_args(int argc, int n, const char *usage) { if (argc < n) { fprintf(ERRF, "usage: %s\n", usage); return 1; } return 0; }
/* ---------- the PAGE (2026-11-09, BOOK 16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE section 11b): the board of a LEDGER TRACK game DRAWN as an editor desk, from DATA. ----------
 * Verbs: `page <desk_dir> [atlas.rgba] [noevents]` writes the desk (cells.txt, cells.marker, map.txt, events.txt, skin.txt, extrusion.pdl, cells.rgba), `preview <desk_dir> <out.raw>` a flat top-down picture of it, `squarecell <n>` where a square is,
 * `labelmask <A> <text>` the label baker's mask. A projection: it decides no rule and the ledger never reads it. The page is made by the same pipeline as Teropoly's board (deskopoly.c `page` + `label_atlas`: the names are BAKED into the floor texture, so
 * the 3D view and the 2D view come from one description at zero per-frame cost); the font, the five font tiers and the wrap/mask code below are a COPY of deskopoly.c lines (decision D67: deskopoly.c is not edited, it is pinned by its own cases;
 * a text-include would have made two consumers share code that only one of them may change). THE DRIFT CHECK is the case `chutes` / cs_page_drift: the table lines are found as whole lines in deskopoly.c and the mask of fixed labels equals the mask
 * deskopoly.c's own label_mask makes (an op that #includes deskopoly.c). Geometry: board.pdl `BOARD | cols | rows | path (serpentine|rows)` (cols x rows must equal TRACK length); everything else is rulesets/<name>/page.pdl (grammar in that file).
 * Square 1 is bottom-left; a serpentine board runs row 1 left to right, row 2 right to left, ...; `PAGE | start | strip` adds one row of blocks under the board for square 0 (tokens wait there). */
#define PGMAX 64
#define NLB 128
static int HD_px = 24, HD_cols = 16, HD_tiles = 43, HD_rock = 36, PG_block = 3, PG_start = 0, PG_labels = 1, PG_serp = 1;
static unsigned PG_lab_text = 0xffffff, PG_lab_out = 0x000000;
static struct { char key[40], text[120]; } LB[NLB]; static int nlb;
static const unsigned char F57[95][5] = {   /* ASCII 32..126, 5 columns, bit 0 = top row (the classic 5x7 font) */
{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0,5,3,0,0},{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x2A,0x1C,0x7F,0x1C,0x2A},{8,8,0x3E,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},{2,4,8,0x10,0x20},{0,0x41,0x41,0x7F,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},
{0,1,2,4,0},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{8,0x7E,9,1,2},{0x0C,0x52,0x52,0x52,0x3E},{0x7F,8,4,4,0x78},{0,0x44,0x7D,0x40,0},{0x20,0x40,0x44,0x3D,0},{0x7F,0x10,0x28,0x44,0},{0,0x41,0x7F,0x40,0},{0x7C,4,0x18,4,0x78},{0x7C,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
{0x7C,0x14,0x14,0x14,8},{8,0x14,0x14,0x18,0x7C},{0x7C,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},{4,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0,8,0x36,0x41,0},{0,0,0x7F,0,0},{0,0x41,0x36,8,0},{8,8,0x2A,0x1C,8} };
static const struct { char c; unsigned char r[5]; } F35[] = {   /* the smallest tiers: 3x5, capitals and digits only (3 bits per row, bit 2 = left) */
{'A',{2,5,7,5,5}},{'B',{6,5,6,5,6}},{'C',{3,4,4,4,3}},{'D',{6,5,5,5,6}},{'E',{7,4,6,4,7}},{'F',{7,4,6,4,4}},{'G',{3,4,5,5,3}},{'H',{5,5,7,5,5}},{'I',{7,2,2,2,7}},{'J',{1,1,1,5,2}},{'K',{5,5,6,5,5}},{'L',{4,4,4,4,7}},{'M',{5,7,7,5,5}},
{'N',{6,5,5,5,5}},{'O',{2,5,5,5,2}},{'P',{6,5,6,4,4}},{'Q',{2,5,5,7,3}},{'R',{6,5,6,5,5}},{'S',{3,4,2,1,6}},{'T',{7,2,2,2,2}},{'U',{5,5,5,5,7}},{'V',{5,5,5,5,2}},{'W',{5,5,7,7,5}},{'X',{5,5,2,5,5}},{'Y',{5,5,2,2,2}},{'Z',{7,1,2,4,7}},
{'0',{7,5,5,5,7}},{'1',{2,6,2,2,7}},{'2',{6,1,2,4,7}},{'3',{6,1,2,1,6}},{'4',{5,5,7,1,1}},{'5',{7,4,6,1,6}},{'6',{3,4,7,5,7}},{'7',{7,1,2,2,2}},{'8',{7,5,7,5,7}},{'9',{7,5,7,1,6}},
{'&',{2,5,2,5,3}},{'-',{0,0,7,0,0}},{'.',{0,0,0,0,2}},{',',{0,0,0,2,4}},{'\'',{2,2,0,0,0}},{'!',{2,2,2,0,2}},{'?',{6,1,2,0,2}},{':',{0,2,0,2,0}},{'/',{1,1,2,4,4}},{'+',{0,2,7,2,0}},{'(',{1,2,2,2,1}},{')',{4,2,2,2,4}} };
typedef struct { int f35, sc, gap, lgap; } Tier;
static const Tier TIERS[] = { {0, 3, 2, 3}, {0, 2, 1, 2}, {1, 2, 2, 2}, {0, 1, 1, 1}, {1, 1, 1, 1} };   /* glyph heights 21, 14, 10, 7, 5 px: the largest that fits wins */
#define NTIERS 5
#define LAB_LINES 24
#define LAB_CHARS 96
static int tier_cw(const Tier *t) { return (t->f35 ? 3 : 5) * t->sc; }
static int tier_ch(const Tier *t) { return (t->f35 ? 5 : 7) * t->sc; }
/* word-wrap `text` for a tier into lines[]; returns the line count, or -1 when a word is wider than a line (unless `hard`, which breaks it) or the lines do not fit `avail` px high or `maxl` lines */
static int label_wrap(const char *text, const Tier *t, int avail, int maxl, int hard, char lines[][LAB_CHARS]) {
    int cw = tier_cw(t), maxc = (avail + t->gap) / (cw + t->gap), nl = 0, cur = 0; if (maxc < 1) return -1; lines[0][0] = 0;
    char w[LAB_CHARS * 2]; const char *p = text;
    while (*p) {
        while (*p == ' ') p++; if (!*p) break;
        if (p[0] == '\\' && p[1] == 'n') { p += 2; if (++nl >= LAB_LINES) return -1; lines[nl][0] = 0; cur = 0; continue; }
        int wl = 0; while (*p && *p != ' ' && !(p[0] == '\\' && p[1] == 'n') && wl < LAB_CHARS * 2 - 1) w[wl++] = *p++; w[wl] = 0;
        if (wl > maxc && !hard) return -1;
        for (int off = 0; off < wl; ) {
            int take = wl - off; if (take > maxc) take = maxc;
            if (cur && cur + 1 + take > maxc) { if (++nl >= LAB_LINES) return -1; lines[nl][0] = 0; cur = 0; }
            if (cur) { lines[nl][cur++] = ' '; } memcpy(lines[nl] + cur, w + off, (size_t)take); cur += take; lines[nl][cur] = 0; off += take;
            if (off < wl) { if (++nl >= LAB_LINES) return -1; lines[nl][0] = 0; cur = 0; } } }
    nl++; if (lines[nl - 1][0] == 0 && nl > 1) nl--;
    if (nl > maxl) return -1; if (nl * tier_ch(t) + (nl - 1) * t->lgap > avail) return -1; return nl;
}
static void label_glyph(unsigned char *m, int A, int x, int y, char c, const Tier *t) {
    int sc = t->sc;
    if (t->f35) { if (c >= 'a' && c <= 'z') c = (char)(c - 32); const unsigned char *r = NULL; for (size_t k = 0; k < sizeof F35 / sizeof *F35; k++) if (F35[k].c == c) r = F35[k].r;
        if (!r) { if (c == ' ') return; for (size_t k = 0; k < sizeof F35 / sizeof *F35; k++) if (F35[k].c == '?') r = F35[k].r; }
        for (int ry = 0; ry < 5; ry++) for (int cx = 0; cx < 3; cx++) if (r[ry] & (4 >> cx)) for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) { int px = x + cx * sc + dx, py = y + ry * sc + dy; if (px >= 0 && py >= 0 && px < A && py < A) m[py * A + px] = 2; } }
    else { if (c < 32 || c > 126) c = '?'; const unsigned char *col = F57[c - 32];
        for (int cx = 0; cx < 5; cx++) for (int ry = 0; ry < 7; ry++) if (col[cx] & (1 << ry)) for (int dy = 0; dy < sc; dy++) for (int dx = 0; dx < sc; dx++) { int px = x + cx * sc + dx, py = y + ry * sc + dy; if (px >= 0 && py >= 0 && px < A && py < A) m[py * A + px] = 2; } }
}
/* the label mask of one A x A block: 2 = glyph, 1 = outline (8-neighbours of a glyph pixel), 0 = nothing. Returns 1 if anything was drawn. */
static int g_lab_trunc;   /* set when a label needed more lines than the smallest font allows and the lines that fit were drawn (the only case where text is dropped) */
static int label_mask(const char *text, int A, unsigned char *m) {
    static char lines[LAB_LINES][LAB_CHARS]; memset(m, 0, (size_t)A * A); int avail = A - 2, nl = -1, ti = NTIERS - 1, hard = 0;
    for (int pass = 0; pass < 2 && nl < 0; pass++) for (int k = 0; k < NTIERS && nl < 0; k++) { nl = label_wrap(text, &TIERS[k], avail, pass ? LAB_LINES : 3, 0, lines); ti = k; }
    if (nl < 0) { hard = 1; ti = NTIERS - 1; int maxl = (avail + TIERS[ti].lgap) / (tier_ch(&TIERS[ti]) + TIERS[ti].lgap); nl = label_wrap(text, &TIERS[ti], avail, maxl < LAB_LINES ? maxl : LAB_LINES, hard, lines);
        if (nl < 0) { nl = maxl < LAB_LINES ? maxl : LAB_LINES; if (nl < 1) return 0; g_lab_trunc = 1; } }   /* last resort: too much text for any tier, the lines that fit are drawn (never past the rim) */
    const Tier *t = &TIERS[ti]; int ch = tier_ch(t), total = nl * ch + (nl - 1) * t->lgap, y0 = (A - total) / 2, any = 0;
    for (int l = 0; l < nl; l++) { int len = (int)strlen(lines[l]); if (!len) continue; int wd = len * (tier_cw(t) + t->gap) - t->gap, x0 = (A - wd) / 2;
        for (int c = 0; c < len; c++) { label_glyph(m, A, x0 + c * (tier_cw(t) + t->gap), y0 + l * (ch + t->lgap), lines[l][c], t); any = 1; } }
    if (!any) return 0;
    static unsigned char o[96 * 96]; if (A > 96) return 1; memcpy(o, m, (size_t)A * A);
    for (int y = 0; y < A; y++) for (int x = 0; x < A; x++) if (!m[y * A + x]) { for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int xx = x + dx, yy = y + dy; if (xx >= 0 && yy >= 0 && xx < A && yy < A && m[yy * A + xx] == 2) o[y * A + x] = 1; } }
    memcpy(m, o, (size_t)A * A); return 1;
}
enum { TM_A, TM_B, TM_START, TM_LF, TM_LT, TM_CF, TM_CT, TM_N };
static const char *TM_NAME[TM_N] = { "floor_a", "floor_b", "start", "ladder_from", "ladder_to", "chute_from", "chute_to" };
static int PG_tm[TM_N] = { 1, 2, 7, 35, 38, 36, 37 };   /* atlas indexes of the stand-in tiles (defaults; page.pdl TILEMAP rows replace them) */
static char PG_jt[4][120];                               /* JUMPTEXT ladder_from ladder_to chute_from chute_to: the label of a jump's two ends; {n} {to} {from} are replaced */
static unsigned PG_tok[MAXPL] = { 0xdc3c3c, 0x3c78e6, 0x3cbe5a, 0xe6c832, 0xb450c8, 0x3cc8c8, 0xf08c28, 0xc8c8c8 };   /* token colours (Teropoly's table); TOKEN | pN | rrggbb */

static int pg_num(const char *s, long lo, long hi, long *v) { char *e; if (!s[0]) return 0; long x = strtol(s, &e, 10); if (*e || x < lo || x > hi) return 0; *v = x; return 1; }
static int pg_hex(const char *s, unsigned *v) { char *e; if (strlen(s) != 6) return 0; unsigned long x = strtoul(s, &e, 16); if (*e) return 0; *v = (unsigned)x; return 1; }
/* the geometry: board.pdl BOARD rows (COLS and ROWS were read by load_config; the path is read here, the file is not hashed again) */
static int pg_geometry(void) {
    if (KIND != K_LEDGER || MODE != M_TRACK) return bad("page: only a ledger track game has a board page");
    char p[P], ln[1024], *fl[10]; PG_serp = 0; snprintf(p, sizeof p, "%s/rulesets/%s/board.pdl", APP, RS); FILE *f = fopen(p, "r"); if (!f) return bad("missing %s", p);
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split_row(ln, fl, 10); if (n >= 3 && !strcmp(fl[0], "BOARD") && !strcmp(fl[1], "path")) { if (!strcmp(fl[2], "serpentine")) PG_serp = 1; else if (!strcmp(fl[2], "rows")) PG_serp = 0; else { fclose(f); return bad("board.pdl: BOARD path is serpentine or rows"); } } }
    fclose(f); if (COLS < 1 || ROWS < 1 || COLS * ROWS != LENGTH) return bad("page: BOARD cols x rows (%d x %d) must equal the TRACK length %d", COLS, ROWS, LENGTH);
    return 0;
}
/* square n (1..LENGTH) -> its block (bx from the left, by from the TOP); row r counts from the bottom; a serpentine board reverses every odd row */
static void sq_cell(int n, int *bx, int *by) {
    int i = n - 1, r = i / COLS, c = i % COLS, flip = PG_serp && (r & 1);
#ifdef GC_MUT_PAGE_FLIP
    flip = !flip;   /* mutant for the harness: the serpentine direction swapped (never installed) */
#endif
    if (flip) c = COLS - 1 - c; *bx = c; *by = ROWS - 1 - r;
}
static int load_page(void) {
    HD_px = 24; HD_cols = 16; HD_tiles = 43; HD_rock = 36; PG_block = 3; PG_start = 0; PG_labels = 1; PG_lab_text = 0xffffff; PG_lab_out = 0; nlb = 0;
    static const int tm0[TM_N] = { 1, 2, 7, 35, 38, 36, 37 }; memcpy(PG_tm, tm0, sizeof PG_tm); for (int i = 0; i < 4; i++) snprintf(PG_jt[i], sizeof PG_jt[i], "{n}");
    if (pg_geometry()) return 1;
    char p[P], ln[1024], *f[6]; snprintf(p, sizeof p, "%s/rulesets/%s/page.pdl", APP, RS); FILE *fp = fopen(p, "r"); if (!fp) return bad("cannot read %s", p);
    int lineno = 0;
    while (fgets(ln, sizeof ln, fp)) { lineno++; if (ln[0] == '#' || !trim(ln)[0]) continue; int n = split_row(ln, f, 6); long v; unsigned c;
        if (!strcmp(f[0], "HEADER") && n == 3) { if (!pg_num(f[2], 1, 4096, &v)) { fclose(fp); return bad("page.pdl line %d: HEADER %s needs a number", lineno, f[1]); }
            if (!strcmp(f[1], "tile_px")) { if (v > 96) { fclose(fp); return bad("page.pdl line %d: tile_px is 1 to 96", lineno); } HD_px = (int)v; } else if (!strcmp(f[1], "atlas_cols")) HD_cols = (int)v; else if (!strcmp(f[1], "atlas_tiles")) HD_tiles = (int)v; else if (!strcmp(f[1], "rock_slot")) HD_rock = (int)v;
            else { fclose(fp); return bad("page.pdl line %d: unknown HEADER key %s", lineno, f[1]); } }
        else if (!strcmp(f[0], "PAGE") && n == 3 && !strcmp(f[1], "block")) { if (!pg_num(f[2], 1, 8, &v)) { fclose(fp); return bad("page.pdl line %d: PAGE block is 1 to 8", lineno); } PG_block = (int)v; }
        else if (!strcmp(f[0], "PAGE") && n == 3 && !strcmp(f[1], "start")) { if (!strcmp(f[2], "strip")) PG_start = 1; else if (!strcmp(f[2], "none")) PG_start = 0; else { fclose(fp); return bad("page.pdl line %d: PAGE start is strip or none", lineno); } }
        else if (!strcmp(f[0], "LABELS") && n == 2) { if (!strcmp(f[1], "on")) PG_labels = 1; else if (!strcmp(f[1], "off")) PG_labels = 0; else { fclose(fp); return bad("page.pdl line %d: LABELS is on or off", lineno); } }
        else if (!strcmp(f[0], "LABELCOLOR") && n == 3) { if (!pg_hex(f[2], &c)) { fclose(fp); return bad("page.pdl line %d: LABELCOLOR needs 6 hex digits", lineno); } if (!strcmp(f[1], "text")) PG_lab_text = c; else if (!strcmp(f[1], "outline")) PG_lab_out = c; else { fclose(fp); return bad("page.pdl line %d: LABELCOLOR is text or outline", lineno); } }
        else if (!strcmp(f[0], "TILEMAP") && n == 3) { int k = -1; for (int i = 0; i < TM_N; i++) if (!strcmp(f[1], TM_NAME[i])) k = i;
            if (k < 0) { fclose(fp); return bad("page.pdl line %d: unknown TILEMAP name %s", lineno, f[1]); } if (!pg_num(f[2], 0, 4095, &v)) { fclose(fp); return bad("page.pdl line %d: TILEMAP needs an atlas index", lineno); } PG_tm[k] = (int)v; }
        else if (!strcmp(f[0], "LABEL") && n == 3) { if (!pg_num(f[1], 0, LENGTH, &v)) { fclose(fp); return bad("page.pdl line %d: LABEL key must be a square number 0 to %d", lineno, LENGTH); }
            if (nlb >= NLB || strlen(f[2]) >= sizeof LB[0].text) { fclose(fp); return bad("page.pdl line %d: too many LABEL rows or a label is too long", lineno); } snprintf(LB[nlb].key, sizeof LB[nlb].key, "%s", f[1]); snprintf(LB[nlb].text, sizeof LB[nlb].text, "%s", f[2]); nlb++; }
        else if (!strcmp(f[0], "JUMPTEXT") && n == 3) { int k = -1; static const char *jn[4] = { "ladder_from", "ladder_to", "chute_from", "chute_to" }; for (int i = 0; i < 4; i++) if (!strcmp(f[1], jn[i])) k = i;
            if (k < 0 || strlen(f[2]) >= sizeof PG_jt[0]) { fclose(fp); return bad("page.pdl line %d: JUMPTEXT is ladder_from, ladder_to, chute_from or chute_to, and short", lineno); } snprintf(PG_jt[k], sizeof PG_jt[k], "%s", f[2]); }
        else if (!strcmp(f[0], "TOKEN") && n == 3) { long pl; if (f[1][0] != 'p' || !pg_num(f[1] + 1, 1, MAXPL, &pl) || !pg_hex(f[2], &c)) { fclose(fp); return bad("page.pdl line %d: TOKEN | pN | rrggbb", lineno); } PG_tok[pl - 1] = c; }
        else { fclose(fp); return bad("page.pdl line %d: unknown or malformed row %s", lineno, f[0]); } }
    fclose(fp);
    if (HD_cols < 1 || HD_tiles < 1) return bad("page.pdl: bad atlas header");
    for (int i = 0; i < TM_N; i++) if (PG_tm[i] >= HD_tiles) return bad("page.pdl: TILEMAP %s is outside the atlas (%d tiles)", TM_NAME[i], HD_tiles);
    if (COLS * PG_block > PGMAX || (ROWS + PG_start) * PG_block > PGMAX) return bad("page.pdl: the page would be %d x %d cells (at most %d x %d)", COLS * PG_block, (ROWS + PG_start) * PG_block, PGMAX, PGMAX);
    return 0;
}
/* the display text of square n: an explicit LABEL row (the LAST row of a key wins; empty text = no label), else the JUMPTEXT of the jump end it is (a square that is both a start and an end is a start), else the number. {n} {to} {from} are replaced. */
static void pg_text(int n, char *o, size_t cap) {
    for (int j = nlb - 1; j >= 0; j--) if (atoi(LB[j].key) == n) { snprintf(o, cap, "%s", LB[j].text); return; }
    if (n == 0) { snprintf(o, cap, "START"); return; }
    int cls = -1, from = 0, to = 0; for (int i = 0; i < NJ; i++) if (JF[i] == n) { cls = JT[i] > JF[i] ? 0 : 2; from = JF[i]; to = JT[i]; break; }
    if (cls < 0) for (int i = 0; i < NJ; i++) if (JT[i] == n) { cls = JT[i] > JF[i] ? 1 : 3; from = JF[i]; to = JT[i]; break; }
    const char *t = cls < 0 ? "{n}" : PG_jt[cls]; size_t w = 0; o[0] = 0;
    for (; *t && w + 12 < cap; ) { if (!strncmp(t, "{n}", 3)) { w += (size_t)snprintf(o + w, cap - w, "%d", n); t += 3; } else if (!strncmp(t, "{to}", 4)) { w += (size_t)snprintf(o + w, cap - w, "%d", to); t += 4; } else if (!strncmp(t, "{from}", 6)) { w += (size_t)snprintf(o + w, cap - w, "%d", from); t += 6; } else o[w++] = *t++; }
    o[w] = 0;
}
typedef struct { int bx, by; char text[160]; } PgLab;
/* LABELS: draw every item's label over the cells of its block (deskopoly.c label_atlas with the items as data instead of the ring). `ct` holds the PLAIN tile of every cell; each cell with ink gets a NEW atlas tile (that plain tile with the glyphs over it),
 * appended after the HD_tiles base slots, and `ct` points at it. Returns the number of new tiles; 0 when the base atlas is not a usable atlas of HD_cols x HD_tiles or no item has ink; -1 on a memory failure. */
static int pg_label_atlas(const char *base_path, int nit, const PgLab *it, int (*ct)[PGMAX], unsigned char **out, size_t *outsz) {
    FILE *a = fopen(base_path, "rb"); if (!a) return 0; fseek(a, 0, SEEK_END); long sz = ftell(a); rewind(a);
    int aw = HD_cols * HD_px; size_t rowb = (size_t)aw * 4; if (sz <= 0 || (size_t)sz % rowb || (long)((size_t)sz / rowb / HD_px) * HD_cols < HD_tiles || (size_t)sz / rowb % HD_px) { fclose(a); return 0; }
    unsigned char *base = malloc((size_t)sz); if (!base || fread(base, 1, (size_t)sz, a) != (size_t)sz) { fclose(a); free(base); return 0; } fclose(a);
    int B = PG_block, A = B * HD_px; if (A > 96) { free(base); return 0; }
    int cap = nit * B * B, nn = 0; unsigned char *tiles = calloc((size_t)cap * HD_px * HD_px, 4), *mask = malloc((size_t)A * A); int *dst = malloc((size_t)cap * 2 * sizeof(int));
    if (!tiles || !mask || !dst) { free(base); free(tiles); free(mask); free(dst); return -1; }
    for (int i = 0; i < nit; i++) { const char *tx = it[i].text; g_lab_trunc = 0; if (!tx[0] || !label_mask(tx, A, mask)) continue; if (g_lab_trunc) fprintf(ERRF, "game_core: the label of item %d is too long for any font: the lines that fit are drawn\n", i);
        for (int ky = 0; ky < B; ky++) for (int kx = 0; kx < B; kx++) { int ink = 0; for (int y = 0; y < HD_px && !ink; y++) for (int x = 0; x < HD_px; x++) if (mask[(ky * HD_px + y) * A + kx * HD_px + x]) { ink = 1; break; }
            if (!ink) continue; int cy = it[i].by * B + ky, cx = it[i].bx * B + kx, t = ct[cy][cx]; unsigned char *d = tiles + (size_t)nn * HD_px * HD_px * 4;
            for (int y = 0; y < HD_px; y++) for (int x = 0; x < HD_px; x++) { int mv = mask[(ky * HD_px + y) * A + kx * HD_px + x]; unsigned char *o = d + ((size_t)y * HD_px + x) * 4;
                if (mv) { unsigned c = mv == 2 ? PG_lab_text : PG_lab_out; o[0] = (unsigned char)(c >> 16); o[1] = (unsigned char)(c >> 8); o[2] = (unsigned char)c; o[3] = 255; }
                else memcpy(o, base + ((size_t)((t / HD_cols) * HD_px + y) * aw + (size_t)(t % HD_cols) * HD_px + x) * 4, 4); }
            dst[nn * 2] = cy; dst[nn * 2 + 1] = cx; nn++; } }
    if (!nn) { free(base); free(tiles); free(mask); free(dst); return 0; }
    int total = HD_tiles + nn, rows = (total + HD_cols - 1) / HD_cols; unsigned char *at = calloc((size_t)rows * HD_px * rowb, 1); if (!at) { free(base); free(tiles); free(mask); free(dst); return -1; }
    for (int s = 0; s < HD_tiles; s++) for (int y = 0; y < HD_px; y++) memcpy(at + ((size_t)((s / HD_cols) * HD_px + y) * aw + (size_t)(s % HD_cols) * HD_px) * 4, base + ((size_t)((s / HD_cols) * HD_px + y) * aw + (size_t)(s % HD_cols) * HD_px) * 4, (size_t)HD_px * 4);
    for (int k = 0; k < nn; k++) { int s = HD_tiles + k; for (int y = 0; y < HD_px; y++) memcpy(at + ((size_t)((s / HD_cols) * HD_px + y) * aw + (size_t)(s % HD_cols) * HD_px) * 4, tiles + ((size_t)k * HD_px * HD_px + (size_t)y * HD_px) * 4, (size_t)HD_px * 4); ct[dst[k * 2]][dst[k * 2 + 1]] = s; }
    free(base); free(tiles); free(mask); free(dst); *out = at; *outsz = (size_t)rows * HD_px * rowb; return nn;
}
/* write the atlas only when its bytes differ from the file that is there (labels are static: a rerun must not make the engine reload for nothing); tmp + rename, so a reader never sees half an atlas */
static int pg_write_atlas(const char *dd, const unsigned char *at, size_t sz) {
    char p[P], tp[P + 16]; snprintf(p, sizeof p, "%s/cells.rgba", dd); snprintf(tp, sizeof tp, "%s.tmp", p); struct stat s;
    if (!stat(p, &s) && (size_t)s.st_size == sz) { FILE *f = fopen(p, "rb"); if (f) { unsigned char *old = malloc(sz); int same = old && fread(old, 1, sz, f) == sz && !memcmp(old, at, sz); fclose(f); free(old); if (same) return 0; } }
    FILE *o = fopen(tp, "wb"); if (!o) return -1; if (fwrite(at, 1, sz, o) != sz) { fclose(o); return -1; } fclose(o); if (rename(tp, p)) return -1; return 1;
}
/* the token rows (events.txt: x y r g b charset index dir pattern glyph; the glyph MUST be a letter, a digit parses as a float and the 3D loader drops the row, deskopoly_driver.c): one per player,
 * on the top row of its square's block (up to 4 on a square), a player on square 0 stands in the start strip. Returns the number of rows. */
static int pg_events(char *buf, size_t cap) {
    static int cnt[1024]; memset(cnt, 0, sizeof cnt); buf[0] = 0; int ne = 0, B = PG_block; if (S.phase == PH_NONE) return 0;
    for (int pl = 0; pl < S.np && pl < MAXPL; pl++) { int sq = S.pos[pl], cx, cy; if (sq < 0 || sq > LENGTH) continue;
        if (sq == 0) { if (!PG_start) continue; cx = ((pl + 1) % COLS) * B + B / 2; cy = ROWS * B + B / 2; }   /* the start strip: block 0 holds the word START, the tokens stand in blocks 1.. */
        else { int bx, by, s = cnt[sq]++; if (s > 3) s = 3; sq_cell(sq, &bx, &by); cx = bx * B + (s == 0 ? 0 : s == 1 ? B - 1 : B / 2); cy = by * B + (s == 3 ? B / 2 : 0); }   /* the TOP row of the block: a figure is two cells tall (feet on the cell), so it stands over the empty margin above the number, not over its digits */
        char row[96]; snprintf(row, sizeof row, "%d %d %u %u %u People1 %d 2 1 %c\n", cx, cy, (PG_tok[pl] >> 16) & 255, (PG_tok[pl] >> 8) & 255, PG_tok[pl] & 255, pl, (char)(65 + pl)); if (strlen(buf) + strlen(row) + 1 < cap) { strcat(buf, row); ne++; } }
    return ne;
}
static int cmd_scene(int argc, char **argv);
static int doc_blank(long w, long h, const char *as); static int doc_paint(const char *tool, long x, long y, const char *as); static int cursor_cell(long *x, long *y); static int cmd_stamp(const char *verb, int ac, char **av, const char *as); static int stamps_load(void); static int stamp_picked(void); static int stamp_pick(const char *id);
static int cmd_page(int argc, char **argv) {
    if (argc < 1) { fprintf(ERRF, "usage: page <desk_dir> [atlas.rgba] [noevents]\n"); return 2; } const char *dd = argv[0]; struct stat sb; if (stat(dd, &sb) || !S_ISDIR(sb.st_mode)) { fprintf(ERRF, "page: the desk folder does not exist (make it first)\n"); return 2; }
    if (load_page()) { fprintf(ERRF, "%s\n", MSG); return 2; }
    int B = PG_block, W = COLS * B, H = (ROWS + PG_start) * B; static int ct[PGMAX][PGMAX]; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ct[y][x] = PG_tm[TM_A];
    static PgLab it[1100]; int nit = 0, nl = 0, nc = 0;
    for (int n = 1; n <= LENGTH; n++) { int bx, by, tf; sq_cell(n, &bx, &by); int r = (n - 1) / COLS; tf = ((bx + r) & 1) ? PG_tm[TM_B] : PG_tm[TM_A];
        for (int i = 0; i < NJ; i++) { if (JT[i] == n) tf = JT[i] > JF[i] ? PG_tm[TM_LT] : PG_tm[TM_CT]; }
        for (int i = 0; i < NJ; i++) { if (JF[i] == n) tf = JT[i] > JF[i] ? PG_tm[TM_LF] : PG_tm[TM_CF]; }
        for (int y = 0; y < B; y++) for (int x = 0; x < B; x++) ct[by * B + y][bx * B + x] = tf;
        it[nit].bx = bx; it[nit].by = by; pg_text(n, it[nit].text, sizeof it[nit].text); nit++; }
    for (int i = 0; i < NJ; i++) { if (JT[i] > JF[i]) nl++; else nc++; }
    if (PG_start) { for (int x = 0; x < COLS * B; x++) for (int y = 0; y < B; y++) ct[ROWS * B + y][x] = PG_tm[TM_START]; it[nit].bx = 0; it[nit].by = ROWS; pg_text(0, it[nit].text, sizeof it[nit].text); nit++; }
    int lab_n = 0, lab_wrote = 0; if (PG_labels && argc > 1) { unsigned char *lat = NULL; size_t lsz = 0; lab_n = pg_label_atlas(argv[1], nit, it, ct, &lat, &lsz); if (lab_n < 0) { fprintf(ERRF, "page: out of memory\n"); return 2; } if (lab_n) { lab_wrote = pg_write_atlas(dd, lat, lsz); free(lat); if (lab_wrote < 0) { fprintf(ERRF, "page: cannot write the atlas\n"); return 2; } } }   /* the atlas lands BEFORE cells.txt and the marker, so a reload never sees indexes past the file */
    static char buf[PGMAX * PGMAX * 12 + 512]; size_t w = 0; w += (size_t)snprintf(buf + w, sizeof buf - w, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=%d\natlas_tiles=%d\nrock_slot=%d\ncells\n", W, H, HD_px, HD_cols, HD_tiles + lab_n, HD_rock);
    for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) w += (size_t)snprintf(buf + w, sizeof buf - w, "%s%d,0", x ? " " : "", ct[y][x]); w += (size_t)snprintf(buf + w, sizeof buf - w, "\n"); }
    char q[P + 32]; snprintf(q, sizeof q, "%s/cells.txt", dd); if (write_atomic(q, buf)) { fprintf(ERRF, "page: cannot write cells.txt\n"); return 2; }
    { char mk[P + 32]; snprintf(mk, sizeof mk, "%s/cells.marker", dd); struct stat ms; FILE *mf = fopen(mk, (!stat(mk, &ms) && ms.st_size >= 4096) ? "w" : "a"); if (mf) { fputc('1', mf); fclose(mf); } }   /* the house marker rule: the engine reloads a desk when this file GROWS (never on mtime) */
    w = 0; for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) buf[w++] = '.'; buf[w++] = '\n'; } buf[w] = 0; snprintf(q, sizeof q, "%s/map.txt", dd); write_atomic(q, buf);
    int ne = 0; snprintf(q, sizeof q, "%s/events.txt", dd);
    if (!(argc > 2 && !strcmp(argv[2], "noevents"))) { ne = pg_events(buf, sizeof buf); write_atomic(q, buf); } else if (stat(q, &sb)) write_atomic(q, "");
    snprintf(q, sizeof q, "%s/skin.txt", dd); if (stat(q, &sb)) write_atomic(q, "rmmv\n");
    snprintf(q, sizeof q, "%s/extrusion.pdl", dd); if (stat(q, &sb)) write_atomic(q, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | game_core\nMETA         | desk_id            | play\nMETA         | note               | board page drawn from board.pdl + page.pdl by `game_core page`\n");
    snprintf(q, sizeof q, "%s/cells.rgba", dd);
    if (argc > 1 && stat(q, &sb)) { FILE *a = fopen(argv[1], "rb"); if (!a) { fprintf(ERRF, "page: cannot read the atlas\n"); return 2; } FILE *o = fopen(q, "wb"); if (!o) { fclose(a); fprintf(ERRF, "page: cannot write the atlas\n"); return 2; } char b[8192]; size_t k; while ((k = fread(b, 1, sizeof b, a)) > 0) fwrite(b, 1, k, o); fclose(a); fclose(o); }
    fprintf(OUT, "PAGE %s %dx%d squares=%d ladders=%d chutes=%d labels=%d events=%d\n", dd, W, H, LENGTH, nl, nc, nit, ne);
    if (lab_n) fprintf(OUT, "LABELS new_tiles=%d atlas_tiles=%d atlas_written=%d\n", lab_n, HD_tiles + lab_n, lab_wrote);
    return 0;
}
/* preview: a FLAT top-down picture of a desk (cells.txt + cells.rgba), tile by tile, no depth, no engine (deskopoly.c v_preview). Raw RGBA, for raw_to_png. It is not the engine's render. */
static int cmd_preview(int argc, char **argv) {
    if (argc < 2) { fprintf(ERRF, "usage: preview <desk_dir> <out.raw>\n"); return 2; } char p[P], ln[16384]; snprintf(p, sizeof p, "%s/cells.txt", argv[0]); FILE *fp = fopen(p, "r"); if (!fp) { fprintf(ERRF, "preview: no cells.txt (run page first)\n"); return 2; }
    int W = 0, H = 0, px = 24, cols = 16; static int ct[PGMAX][PGMAX]; int y = -1;
    while (fgets(ln, sizeof ln, fp)) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "tile_px=", 8)) px = atoi(ln + 8); else if (!strncmp(ln, "atlas_cols=", 11)) cols = atoi(ln + 11);
        else if (!strncmp(ln, "cells", 5)) y = 0; else if (y >= 0 && y < H && y < PGMAX) { int x = 0; for (char *t = strtok(ln, " \n"); t && x < W && x < PGMAX; t = strtok(NULL, " \n"), x++) ct[y][x] = atoi(t); y++; } }
    fclose(fp); if (W < 1 || H < 1 || W > PGMAX || H > PGMAX || y != H || px < 1 || cols < 1) { fprintf(ERRF, "preview: cells.txt is not a complete page\n"); return 2; }
    snprintf(p, sizeof p, "%s/cells.rgba", argv[0]); fp = fopen(p, "rb"); if (!fp) { fprintf(ERRF, "preview: no cells.rgba atlas\n"); return 2; } fseek(fp, 0, SEEK_END); long asz = ftell(fp); rewind(fp);
    unsigned char *at = malloc((size_t)asz); if (!at || fread(at, 1, (size_t)asz, fp) != (size_t)asz) { fclose(fp); free(at); fprintf(ERRF, "preview: cannot read the atlas\n"); return 2; } fclose(fp);
    int aw = cols * px; if (asz % (aw * 4)) { free(at); fprintf(ERRF, "preview: atlas size does not match atlas_cols x tile_px\n"); return 2; } int ah = (int)(asz / (aw * 4));
    size_t ow = (size_t)W * px, oh = (size_t)H * px; unsigned char *o = calloc(ow * oh, 4); if (!o) { free(at); fprintf(ERRF, "preview: out of memory\n"); return 2; }
    for (int cy = 0; cy < H; cy++) for (int cx = 0; cx < W; cx++) { int t = ct[cy][cx], sx = (t % cols) * px, sy = (t / cols) * px; if (sy + px > ah) { free(at); free(o); fprintf(ERRF, "preview: a tile index is outside the atlas\n"); return 2; }
        for (int r = 0; r < px; r++) memcpy(o + (((size_t)cy * px + r) * ow + (size_t)cx * px) * 4, at + ((size_t)(sy + r) * aw + sx) * 4, (size_t)px * 4); }
    FILE *w = fopen(argv[1], "wb"); if (!w) { free(at); free(o); fprintf(ERRF, "preview: cannot write the raw file\n"); return 2; } fwrite(o, 4, ow * oh, w); fclose(w); free(at); free(o);
    fprintf(OUT, "PREVIEW %zux%zu\n", ow, oh); return 0;
}
/* args: the words after the verb. `as=pN` anywhere sets the actor. Returns the exit code. */
static int engine_verb(int argc, char **argv) {
    const char *verb = argv[0]; char as[32] = ""; char *av[16]; int ac = 0;
    for (int i = 1; i < argc && ac < 15; i++) { if (!strncmp(argv[i], "as=", 3)) snprintf(as, sizeof as, "%s", argv[i] + 3); else av[ac++] = argv[i]; }
    if (!strcmp(verb, "help")) { char vp[P], ln[1024], *fl[8]; snprintf(vp, sizeof vp, "%s/verbs.pdl", APP); FILE *f = fopen(vp, "r"); if (!f) { fprintf(ERRF, "no verbs.pdl\n"); return 2; } while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split_row(ln, fl, 8); if (n >= 6 && !strcmp(fl[0], "VERB")) fprintf(OUT, "%-10s %s  [%s]\n", fl[1], fl[5], fl[4]); } fclose(f); return 0; }
    if (!strcmp(verb, "tuning")) {   /* read-only: the pace the driver would use (tuning.pdl tick_ms, 33 to 2000); a bad value is refused with the reason, exit 2. Needs no ledger. */
        if (ac) { fprintf(ERRF, "usage: tuning\n"); return 2; } if (load_tuning()) { fprintf(ERRF, "%s\n", MSG); return 2; }
        fprintf(OUT, "TUNING tick_ms=%ld source=%s fast=%ld normal=%ld slow=%ld steps_per_second=%.1f\n", pace_ms(1), TICK_MS ? "tuning.pdl" : "default", pace_ms(0), pace_ms(1), pace_ms(2), 1000.0 / (double)pace_ms(1)); return 0; }
    if (!strcmp(verb, "labelmask")) {   /* labelmask <A> <text...> [--art]: the label baker's mask of one A x A block (read-only, needs no ledger; the drift check compares it with deskopoly.c's own label_mask) */
        int art = 0; char tx[200] = ""; long A = 0; int have = 0; for (int i = 0; i < ac; i++) { if (!strcmp(av[i], "--art")) { art = 1; continue; } if (!have) { char *e; A = strtol(av[i], &e, 10); if (*e || A < 8 || A > 96) { fprintf(ERRF, "usage: labelmask <A 8..96> <text...> [--art]\n"); return 2; } have = 1; continue; } if (tx[0]) strncat(tx, " ", sizeof tx - strlen(tx) - 1); strncat(tx, av[i], sizeof tx - strlen(tx) - 1); }
        if (!have) { fprintf(ERRF, "usage: labelmask <A 8..96> <text...> [--art]\n"); return 2; }
        static unsigned char m[96 * 96]; g_lab_trunc = 0; int ink = label_mask(tx, (int)A, m); int ng = 0, no = 0; uint64_t h = FNV0; for (int i = 0; i < A * A; i++) { if (m[i] == 2) ng++; else if (m[i] == 1) no++; h = fnv(h, &m[i], 1); } char hs[20]; hx(h, hs);
        fprintf(OUT, "LABELMASK A=%ld ink=%d glyph=%d outline=%d fnv=%s\n", A, ink, ng, no, hs); if (art) for (int y = 0; y < A; y++) { for (int x = 0; x < A; x++) fputc(m[y * A + x] == 2 ? '#' : m[y * A + x] == 1 ? '+' : '.', OUT); fputc('\n', OUT); } return 0; }
    if (!strcmp(verb, "fontsum")) {   /* fontsum: the checksums of the baker's tables; the drift check compares them with the same line from an op that #includes deskopoly.c */
        uint64_t a = FNV0, b = FNV0, c = FNV0; a = fnv(a, F57, sizeof F57); for (size_t k = 0; k < sizeof F35 / sizeof *F35; k++) { b = fnv(b, &F35[k].c, 1); b = fnv(b, F35[k].r, 5); } for (int k = 0; k < NTIERS; k++) { c = fnv(c, &TIERS[k].f35, sizeof(int)); c = fnv(c, &TIERS[k].sc, sizeof(int)); c = fnv(c, &TIERS[k].gap, sizeof(int)); c = fnv(c, &TIERS[k].lgap, sizeof(int)); }
        fprintf(OUT, "FONTSUM f57=%016llx f35=%016llx tiers=%016llx\n", (unsigned long long)a, (unsigned long long)b, (unsigned long long)c); return 0; }
    if (!strcmp(verb, "squarecell")) {   /* squarecell <n>: where square n is on the page (block from the left, from the top); read-only, needs no ledger */
        long n; if (ac != 1 || !pg_num(av[0], 1, 100000, &n)) { fprintf(ERRF, "usage: squarecell <n>\n"); return 2; } if (pg_geometry()) { fprintf(ERRF, "%s\n", MSG); return 2; } if (n > LENGTH) { fprintf(ERRF, "squarecell: the track has %d squares\n", LENGTH); return 2; }
        int bx, by; sq_cell((int)n, &bx, &by); fprintf(OUT, "SQUARE %ld block=%d,%d\n", n, bx, by); return 0; }
    TALK_PRINT = !strcmp(verb, "talk") && KIND == K_LEDGER; ROWHOOK = (!strcmp(verb, "export") && ac == 1) ? export_hook : NULL;   /* these two verbs observe the replay itself */
    if (load()) { fprintf(ERRF, "%s\n", MSG); return 1; }

    if (!strcmp(verb, "new")) { if (ac > 1) { fprintf(ERRF, "usage: new [seed]\n"); return 2; } if (ac == 1) { char *e; long v = strtol(av[0], &e, 10); if (*e || v < 0 || v > 999999999) { fprintf(ERRF, "new: the seed is a number 0 to 999999999\n"); return 2; } }
        if (live(-1, "", "new", ac ? av[0] : "-", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "new game seed=%ld kind=%s rows=%d\n", S.seed, KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", S.nrows); return 0; }
    if (!strcmp(verb, "join")) { if (need_args(ac, 1, "join <name> [human|pet]")) return 2; if (live(-1, "", "join", av[0], ac > 1 ? av[1] : "pet", NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "p%d joined as %s\n", S.np, av[0]); return 0; }
    if (!strcmp(verb, "start")) { if (live(-1, "", "start", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "game started: %d players, seed %ld\n", S.np, S.seed); return 0; }
    if (!strcmp(verb, "seat")) { if (need_args(ac, 2, "seat <pN> human|pet")) return 2; if (live(-1, "", "seat", av[0], av[1], NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s is now %s\n", av[0], av[1]); return 0; }
    if (!strcmp(verb, "roll") && KIND == K_LEDGER) { Row r; if (live(-1, as, "roll", "", NULL, &r)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s rolled %s: %s\n", r.actor, r.target, r.res); return 0; }
    if (!strcmp(verb, "talk") && KIND == K_LEDGER) { if (ac) { fprintf(ERRF, "usage: talk\n"); return 2; } fprintf(OUT, "TALK sentences=%ld\n", S.ntk); return 0; }
    if (!strcmp(verb, "keys") && KIND == K_REALTIME && (MODE == M_SNAKE || MODE == M_PLATFORM)) {   /* keys <relay_file> <byte_offset>: what the driver does with state/interact_relay.txt, as a verb the harness can run: steer p1 by the arrow codes written after the cursor */
        if (need_args(ac, 2, "keys <relay_file> <byte_offset>")) return 2; char *e; long off = strtol(av[1], &e, 10); if (*e || off < 0) { fprintf(ERRF, "keys: the offset is a number of bytes\n"); return 2; }
        if (S.phase != PH_PLAY) { fprintf(ERRF, "keys: the game is %s\n", S.phase == PH_OVER ? "over" : "not started"); return 2; }
        int codes[64], refused = 0, ignored = 0; int n = keys_poll(av[0], &off, codes, 64); int applied = MODE == M_PLATFORM ? plat_keys_apply(codes, n) : keys_apply(codes, n, &refused, &ignored); if (MODE == M_PLATFORM) ignored = n - applied; fprintf(OUT, "KEYS read=%d applied=%d refused=%d ignored=%d offset=%ld tick=%ld\n", n, applied, refused, ignored, off, S.tick); return 0; }
    if (!strcmp(verb, "turn") && KIND == K_REALTIME && MODE == M_SNAKE) { if (need_args(ac, 2, "turn <pN> L|R|U|D [tick]")) return 2; long t = -1; if (ac > 2) { char *e; t = strtol(av[2], &e, 10); if (*e || t < 0) { fprintf(ERRF, "turn: tick is a number\n"); return 2; } }
        if (live(t, av[0], "turn", av[1], NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s turns %s at tick %ld\n", av[0], av[1], t >= 0 ? t : S.tick); return 0; }
    if (!strcmp(verb, "verify_clear") && KIND == K_REALTIME && WALKISH) {   /* read-only: the run ledger replayed (load did it) and ends at the goal; prints the proof Make records with its `clear` verb */
        if (S.phase != PH_OVER || S.winner < 0) { fprintf(ERRF, "NOT CLEAR: the run is %s\n", S.phase == PH_OVER ? "over without a winner" : "not at the goal yet"); return 1; }
        char lh[20], rh[20]; hx(level_hash(), lh); hx(ledger_hash(NULL), rh); fprintf(OUT, "CLEAR level=%s;run=%s;ticks=%ld\n", lh, rh, S.tick); return 0; }
    if (!strcmp(verb, "clear") && KIND == K_EDITOR && LEVELDOC) {   /* clear <play_app> <play_state>: run the Play app's verify_clear (a second, short-lived process) and record its proof */
        if (need_args(ac, 2, "clear <play_app_dir> <play_state_dir>   [as=author]")) return 2; char self[P]; ssize_t k = readlink("/proc/self/exe", self, sizeof self - 1); if (k <= 0) { fprintf(ERRF, "clear: cannot find myself\n"); return 2; } self[k] = 0;
        char *cv[] = { self, av[0], av[1], (char *)"verify_clear", NULL }; static char co[600]; int rc = run_capture(cv, co, sizeof co); char *nl = strpbrk(co, "\r\n"); if (nl) *nl = 0;
        if (rc != 0 || strncmp(co, "CLEAR ", 6)) { fprintf(ERRF, "clear refused: the Play run is not a clear (%s)\n", co[0] ? co : "no answer"); return 2; }
        if (live(-1, as, "clear", "", co + 6, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "clear recorded: %s\n", co + 6); return 0; }
    if (!strcmp(verb, "input") && KIND == K_REALTIME && MODE != M_SNAKE) { if (need_args(ac, 2, "input <pN> left|right|stay [tick]")) return 2; long t = -1; if (ac > 2) { char *e; t = strtol(av[2], &e, 10); if (*e || t < 0) { fprintf(ERRF, "input: tick is a number\n"); return 2; } } if (live(t, av[0], "input", av[1], NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s %s at tick %ld\n", av[0], av[1], t >= 0 ? t : S.tick); return 0; }
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
    if (KIND == K_EDITOR && LEVELDOC && !strcmp(verb, "blank")) { long w = 20, h = 12; char *e; if (ac > 0) { w = strtol(av[0], &e, 10); if (*e) { fprintf(ERRF, "blank: width is a number\n"); return 2; } } if (ac > 1) { h = strtol(av[1], &e, 10); if (*e) { fprintf(ERRF, "blank: height is a number\n"); return 2; } } if (doc_blank(w, h, as)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "blank %ldx%ld: %d rows in the document\n", w, h, S.ndoc); return 0; }
    if (KIND == K_EDITOR && LEVELDOC && !strcmp(verb, "paint")) { if (need_args(ac, 3, "paint ground|erase|start|goal <x> <y>   [as=author]")) return 2; char *e1, *e2; long x = strtol(av[1], &e1, 10), y = strtol(av[2], &e2, 10); if (*e1 || *e2) { fprintf(ERRF, "paint: x and y are numbers\n"); return 2; } if (doc_paint(av[0], x, y, as)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "paint %s %ld,%ld: %d rows in the document\n", av[0], x, y, S.ndoc); return 0; }
    if (KIND == K_EDITOR && LEVELDOC && (!strcmp(verb, "stamps") || !strcmp(verb, "pick") || !strcmp(verb, "place"))) return cmd_stamp(verb, ac, av, as);
    if (KIND == K_EDITOR && !strcmp(verb, "publish")) { if (live(-1, as, "publish", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "published doc=%s rows=%d\n", S.pubhash, S.ndoc); return 0; }
    if (!strcmp(verb, "export") && ac == 1) return cmd_export(av[0]);
    if (!strcmp(verb, "export")) { if (KIND != K_EDITOR) { fprintf(ERRF, "export: only an editor game has a document (give an out folder to write the data tables: export <out_dir>)\n"); return 2; } if (write_doc()) { fprintf(ERRF, "export: cannot write doc.txt\n"); return 2; } fprintf(OUT, "wrote doc.txt (%d rows)\n", S.ndoc); return 0; }
    if (!strcmp(verb, "gridpng")) {   /* snake picture: a projection of the replayed grid. Not a rule. */
        if (ac != 1) { fprintf(ERRF, "usage: gridpng <out.ppm>\n"); return 2; }
        if (MODE != M_SNAKE) { fprintf(ERRF, "gridpng: only MODE snake has a grid picture\n"); return 2; }
        if (S.phase == PH_LOBBY || S.np < 1) { fprintf(ERRF, "gridpng: start the game first\n"); return 2; }
        const int sc = 8; int w = COLS * sc, h = ROWS * sc; FILE *fp = fopen(av[0], "wb"); if (!fp) { fprintf(ERRF, "gridpng: cannot write\n"); return 2; }
        fprintf(fp, "P6\n%d %d\n255\n", w, h);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int cx = x / sc, cy = y / sc; unsigned char rgb[3] = { 18, 24, 28 };
            if (cx == S.sfood_x && cy == S.sfood_y) { rgb[0] = 220; rgb[1] = 48; rgb[2] = 48; }
            else if (on_body(cx, cy, S.slen)) { int head = (cx == S.bx[0] && cy == S.by[0]); rgb[0] = head ? 80 : 30; rgb[1] = head ? 220 : 140; rgb[2] = 60; }
            if (fwrite(rgb, 1, 3, fp) != 3) { fclose(fp); fprintf(ERRF, "gridpng: short write\n"); return 2; }
        }
        fclose(fp); fprintf(OUT, "GRID cols=%d rows=%d len=%d food=%d,%d file=%s\n", COLS, ROWS, S.slen, S.sfood_x, S.sfood_y, av[0]); return 0; }
    if (!strcmp(verb, "page")) return cmd_page(ac, av);
    if (!strcmp(verb, "scene")) { if (cmd_scene(ac, av)) { fprintf(ERRF, "%s\n", MSG); return 2; } return 0; }
    if (!strcmp(verb, "preview")) return cmd_preview(ac, av);
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
static const char *SPEED_NAME[3] = { "fast", "normal", "slow" };
static volatile sig_atomic_t stop_;
static void on_term(int s) { (void)s; stop_ = 1; }
#define CATCH_RT 4      /* a realtime driver that fell behind runs at most this many steps per wake, then drops the rest of the lateness (the game slows; the ledger is unaffected, ticks are counted, not timed) */
#define HK_MS 250       /* while live: housekeeping (owner, buttons, ledger size, feed) at least this often even if a step is further away */
#define IDLE_MS 500     /* not live: one wake per this long (2 a second) unless a button press arrives sooner */
/* step statistics, kept only when GAME_TICK_STATS=<file> is set (measuring): late = actual start minus the scheduled deadline, interval = time since the previous step of the same live stretch */
static struct { long loops, dropped, ticks, catch_ticks, expected, live_us, n, cap, publishes; long *late, *intv; } ST;
static void stats_note(long late_us, long intv_us, int step_in_pass) {
    ST.ticks++; if (step_in_pass > 0) ST.catch_ticks++; if (!ST.late) { ST.cap = 1L << 17; ST.late = calloc((size_t)ST.cap, sizeof(long)); ST.intv = calloc((size_t)ST.cap, sizeof(long)); if (!ST.late || !ST.intv) { ST.cap = 0; return; } }
    if (ST.n < ST.cap) { ST.late[ST.n] = late_us; ST.intv[ST.n] = intv_us; ST.n++; }
}
static int cmp_long(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return x < y ? -1 : x > y; }
static long pct(const long *v, long n, int p) { return n ? v[(n - 1) * p / 100] : 0; }
static void stats_seg_end(long live_us, long pace_us, const char *path) {   /* a live stretch ended (pause, stop, over, exit): count the steps it should have had and, when measuring, rewrite the summary file */
    ST.expected += pace_us > 0 ? live_us / pace_us : 0; ST.live_us += live_us; if (!path || !ST.n) return;
    long *a = malloc((size_t)ST.n * sizeof(long)), *b = malloc((size_t)ST.n * sizeof(long)); if (!a || !b) { free(a); free(b); return; } long nb = 0; for (long i = 0; i < ST.n; i++) { a[i] = ST.late[i]; if (ST.intv[i] > 0) b[nb++] = ST.intv[i]; }
    qsort(a, (size_t)ST.n, sizeof(long), cmp_long); qsort(b, (size_t)nb, sizeof(long), cmp_long);
    char o[1000]; int n = snprintf(o, sizeof o, "STATS ticks=%ld expected=%ld live_ms=%ld pace_ms=%ld rate_hz=%.3f loops=%ld dropped=%ld catchup_ticks=%ld recorded=%ld feed_writes=%ld\nLATE_US p50=%ld p90=%ld p99=%ld max=%ld\nINTERVAL_US p50=%ld p90=%ld p99=%ld min=%ld max=%ld\n",
        ST.ticks, ST.expected, ST.live_us / 1000, pace_us / 1000, ST.live_us > 0 ? (double)ST.ticks * 1e6 / (double)ST.live_us : 0.0, ST.loops, ST.dropped, ST.catch_ticks, ST.n, ST.publishes, pct(a, ST.n, 50), pct(a, ST.n, 90), pct(a, ST.n, 99), a[ST.n - 1],
        pct(b, nb, 50), pct(b, nb, 90), pct(b, nb, 99), nb ? b[0] : 0, nb ? b[nb - 1] : 0); (void)n; write_atomic(path, o); free(a); free(b);
}
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
static int sync_state(void);
static int reload(void) { if (load()) { snprintf(dmsg, sizeof dmsg, "ledger refused: %.180s", MSG); return 1; } return 0; }
static int sync_state(void) { struct stat ls; if (S.phase != PH_NONE && !stat(LEDGER, &ls) && ls.st_size == KNOWN_SIZE) return 0; return reload(); }   /* reload only when the file is not what this process last wrote: a button press must not rewind the ticks since the last ckpt */
/* THE BOARD PAGE OF THE LIVE GAME (2026-11-09; a ledger track game with a rulesets/<name>/page.pdl). The live game is drawn as its own generated desk "play" of the book (never into the authored "board" desk): `page` + `preview` run in-process
 * (no process), the 2D picture map.png is made by the house op raw_to_png (ONE short-lived child per game start, 10 s watchdog), and the editor world (pieces/world_01/state.txt desk_id) points at "play" while a game is on and back at "board" when the game is
 * reset or the driver ends. The page is STATIC (numbers, ladders, chutes): it is made once per game start; the tokens are events.txt rows rewritten only when a position changes (a few hundred bytes, no fork). */
static char DESKDIR[P], ATLAS[P], WORLD[P]; static int page_on; static uint64_t page_sig;
static void set_desk(const char *want) {
    FILE *f = fopen(WORLD, "r"); if (!f) return; static char buf[4096]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); static char out[4400]; out[0] = 0; int saw = 0, same = 0;
    for (char *p = buf; *p; ) { char *e = strchr(p, '\n'); size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll >= 8 && !strncmp(p, "desk_id=", 8)) { snprintf(out + strlen(out), sizeof out - strlen(out), "desk_id=%s\n", want); saw = 1; if (ll == 8 + strlen(want) && !strncmp(p + 8, want, strlen(want))) same = 1; }
        else if (ll < 600) snprintf(out + strlen(out), sizeof out - strlen(out), "%.*s\n", (int)ll, p);
        if (!e) break; p = e + 1; }
    if (!saw || same) return; char tmp[P + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", WORLD); FILE *w = fopen(tmp, "w"); if (!w) return; fputs(out, w); fclose(w); rename(tmp, WORLD);
}
/* ---------- scene: the LEVEL of a walk / platform game drawn as an editor desk (2026-11-18, owner: "no land under the player"). A PROJECTION of the level file, safe to delete, rewritten byte for byte: cells.txt (the land) + map.txt + the atlas and the look files copied from the book's authored board desk. Slots of the Teropoly atlas: 0 empty (sky), 1 grass (the surface), 32 dirt (below the surface). Heroes and the goal flag are events.txt rows (scene_events), refreshed as the game runs. ---------- */
static int scene_on;
static int cmd_scene(int argc, char **argv) {
    if (LEVELDOC) { if (level_from_doc()) return 1; } else if (!WALKISH) return bad("scene: only a walk, platform or level-document game has a level to draw");
    if (argc < 1) return bad("usage: scene <desk_dir> [atlas.rgba]");
    struct stat sb; if (stat(argv[0], &sb) || !S_ISDIR(sb.st_mode)) return bad("scene: the desk folder does not exist (make it first)");
    static char cb[24000], mb[4200]; int n = snprintf(cb, sizeof cb, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=36\ncells\n", LV.w, LV.h), m = 0;
    for (int y = 0; y < LV.h; y++) { for (int x = 0; x < LV.w; x++) { int sol = is_solid(LV.rows[y][x]), top = sol && (y == 0 || !is_solid(LV.rows[y - 1][x])); n += snprintf(cb + n, sizeof cb - (size_t)n, "%s%d,0,%d", x ? " " : "", sol ? (top ? 1 : 32) : 0, sol ? 2 : 1); mb[m++] = sol ? '#' : '.'; } cb[n++] = '\n'; mb[m++] = '\n'; }
    mb[m] = 0; char p[P + 16]; snprintf(p, sizeof p, "%s/cells.txt", argv[0]); if (write_atomic(p, cb)) return bad("scene: cannot write cells.txt");
    snprintf(p, sizeof p, "%s/map.txt", argv[0]); if (write_atomic(p, mb)) return bad("scene: cannot write map.txt");
    if (argc > 1 && strstr(argv[1], ".rgba")) { char *sl = strrchr(argv[1], '/'); size_t dl = sl ? (size_t)(sl - argv[1]) : 0; static const char *cp[] = { "cells.rgba", "skin.txt", "extrusion.pdl" };
        for (int k = 0; k < 3 && dl; k++) { char src[P + 16], dst[P + 16], *buf; snprintf(src, sizeof src, "%.*s/%s", (int)dl, argv[1], cp[k]); snprintf(dst, sizeof dst, "%s/%s", argv[0], cp[k]); FILE *f = fopen(src, "rb"); if (!f) continue;
            fseek(f, 0, SEEK_END); long z = ftell(f); rewind(f); buf = malloc((size_t)z + 1); if (buf && fread(buf, 1, (size_t)z, f) == (size_t)z) { struct stat ds; if (stat(dst, &ds) || ds.st_size != z) { FILE *w = fopen(dst, "wb"); if (w) { fwrite(buf, 1, (size_t)z, w); fclose(w); } } } free(buf); fclose(f); } }
    { char mk[P + 16]; snprintf(mk, sizeof mk, "%s/cells.marker", argv[0]); struct stat ms; FILE *mf = fopen(mk, (!stat(mk, &ms) && ms.st_size >= 4096) ? "w" : "a"); if (mf) { fputc('1', mf); fclose(mf); } }   /* the engine reloads the atlas by this marker's size, not by mtime */
    fprintf(OUT, "SCENE %dx%d\n", LV.w, LV.h); return 0;
}
static void scene_cell(int i, int *cx, int *cy) { if (MODE == M_PLATFORM) { const PF_Body *b = &S.pb[i]; *cx = (int)((b->x + b->w / 2) / PF_Q); *cy = (int)((b->y + b->h / 2) / PF_Q); } else { *cx = S.pos[i]; *cy = LV.sy; } }
/* GAME | ui_mode | play  (2026-11-18, owner: Teario should not show palettes while it plays): the editor's own bars are declared in its layouts.pdl (palettes = edit only, pets = play); this driver puts the window in that mode while the game runs (ui_mode_op, bars only: --no-settings leaves the chat flag alone) and back to edit when the driver ends. A book without the row is untouched. */
static int ui_mode_wanted(void) { char p[P + 16], ln[512]; snprintf(p, sizeof p, "%s/game.pdl", BOOK); FILE *f = fopen(p, "r"); if (!f) return 0; int want = 0;
    while (fgets(ln, sizeof ln, f)) { char *fl[6]; if (ln[0] == '#') continue; int n = split_row(ln, fl, 6); if (n >= 3 && !strcmp(fl[0], "GAME") && !strcmp(fl[1], "ui_mode") && !strcmp(fl[2], "play")) want = 1; } fclose(f); return want; }
static void ui_mode_set(const char *mode) { char op[P + 64], sides[P + 64], lay[P + 64], out[300]; snprintf(op, sizeof op, "%s/&.widgits/_shared-lib/ops/+x/ui_mode_op.+x", HOUSE); snprintf(sides, sizeof sides, "%s/board/state/sides.txt", EDIT); snprintf(lay, sizeof lay, "%s/layouts.pdl", EDIT);
    if (access(op, X_OK) || access(lay, R_OK)) return; char *av[] = { op, sides, (char *)"--bars", lay, (char *)"--no-settings", (char *)mode, NULL }; run_capture(av, out, sizeof out); }
/* ---------- Make's level tools (2026-11-18): the level is the ledger DOCUMENT (rows name, start, goal, r00..rNN); every tool is an ordinary EDIT row, so a replay rebuilds the level and Play's own level file is never touched (Make publishes, Play reads the published level). ---------- */
static int doc_find(const char *id) { for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], id)) return i; return -1; }
static int doc_rows(void) { int n = 0; char id[8]; for (;; n++) { snprintf(id, sizeof id, "r%02d", n); if (doc_find(id) < 0) break; } return n; }
/* blank <w> <h>: a starter level in an empty document: ground along the bottom, start at the left, goal at the right */
static int doc_blank(long w, long h, const char *as) {
    if (!LEVELDOC) return bad("blank: only a level document has tiles"); if (S.ndoc) return bad("blank: the document is not empty (it already has %d rows)", S.ndoc);
    if (w < 8 || w > LVMAXW || h < 4 || h > 30) return bad("blank: width is 8 to %d, height 4 to 30", LVMAXW);
    char id[8], tx[LVMAXW + 2]; if (live(-1, as, "add", "name", "untitled", NULL)) return 1;
    snprintf(tx, sizeof tx, "2 %ld", h - 2); if (live(-1, as, "add", "start", tx, NULL)) return 1; snprintf(tx, sizeof tx, "%ld %ld", w - 3, h - 2); if (live(-1, as, "add", "goal", tx, NULL)) return 1;
    for (long y = 0; y < h; y++) { memset(tx, y == h - 1 ? '#' : '.', (size_t)w); tx[w] = 0; snprintf(id, sizeof id, "r%02ld", y); if (live(-1, as, "add", id, tx, NULL)) return 1; }
    return 0;
}
/* paint <tool> <x> <y>: ground | erase | start | goal at a cell of the level */
static int doc_paint(const char *tool, long x, long y, const char *as) {
    if (!LEVELDOC) return bad("paint: only a level document has tiles"); int h = doc_rows(); if (!h) return bad("paint: the document has no level yet (use blank)");
    int w = (int)strlen(S.dtx[doc_find("r00")]); if (x < 0 || x >= w || y < 0 || y >= h) return bad("paint: the cell %ld,%ld is outside the level (%d x %d)", x, y, w, h);
    if (!strcmp(tool, "start") || !strcmp(tool, "goal")) { char tx[40]; snprintf(tx, sizeof tx, "%ld %ld", x, y); int ix = doc_find(tool); if (ix >= 0 && !strcmp(S.dtx[ix], tx)) return bad("paint: the %s is already there", tool); return live(-1, as, ix >= 0 ? "set" : "add", tool, tx, NULL); }
    if (!strcmp(tool, "enemy") || !strcmp(tool, "power")) {   /* enemy: ids e1..e8, power: p1..p4, text `x y` */
        char pre = tool[0] == 'e' ? 'e' : 'p'; int mx = pre == 'e' ? 8 : 4; char tx[40], id[4]; snprintf(tx, sizeof tx, "%ld %ld", x, y); if (is_solid(S.dtx[doc_find(((void)(snprintf(id, sizeof id, "r%02ld", y)), id))][x])) return bad("paint: the cell is solid ground, an %s needs an empty cell", tool);
        for (int k = 1; k <= mx; k++) { snprintf(id, sizeof id, "%c%d", pre, k); int ix = doc_find(id); if (ix >= 0 && !strcmp(S.dtx[ix], tx)) return bad("paint: there is already an %s at %ld,%ld", tool, x, y); }
        for (int k = 1; k <= mx; k++) { snprintf(id, sizeof id, "%c%d", pre, k); if (doc_find(id) < 0) return live(-1, as, "add", id, tx, NULL); } return bad("paint: at most %d %s rows", mx, pre == 'e' ? "enemy" : "power-up"); }
    if (!strcmp(tool, "erase")) { char tx[40], id[4]; snprintf(tx, sizeof tx, "%ld %ld", x, y);   /* erase takes an enemy or a power-up off the cell first, else the ground */
        for (int k = 0; k < 12; k++) { snprintf(id, sizeof id, "%c%d", k < 8 ? 'e' : 'p', k < 8 ? k + 1 : k - 7); int ix = doc_find(id); if (ix >= 0 && !strcmp(S.dtx[ix], tx)) return live(-1, as, "del", id, NULL, NULL); } }
    char ch = !strcmp(tool, "ground") ? '#' : !strcmp(tool, "erase") ? '.' : 0; if (!ch) return bad("paint: the tool is ground, erase, start, goal, enemy or power");
    char id[8], tx[LVMAXW + 2]; snprintf(id, sizeof id, "r%02ld", y); snprintf(tx, sizeof tx, "%s", S.dtx[doc_find(id)]); if (tx[x] == ch) return bad("paint: that cell is already %s", ch == '#' ? "ground" : "empty");
    tx[x] = ch; return live(-1, as, "set", id, tx, NULL);
}
/* the editor's cursor cell (the xelector of the editor's own world): where the shelf buttons paint */
/* the STAMP BANK (Make's sidebar, 2026-11-18): rulesets/<rs>/stamps.pdl rows `STAMP | id | label | class`; the id IS the doc_paint tool. Which stamp is picked is a PREFERENCE file <state>/stamp_pick.txt
 * (a tool choice, never read by the rules); an absent or unknown pick means the first stamp. */
typedef struct { char id[24], label[32], cls[12]; } Stamp; static Stamp STAMPS[16]; static int nstamps;
static int stamps_load(void) {
    char p[P], ln[512], *f[6]; nstamps = 0; snprintf(p, sizeof p, "%s/rulesets/%s/stamps.pdl", APP, RS); FILE *fp = fopen(p, "r"); if (!fp) return bad("missing %s", p);
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split_row(ln, f, 6); if (n < 4 || strcmp(f[0], "STAMP")) continue;
        if (nstamps >= 16) { fclose(fp); return bad("stamps.pdl: at most 16 stamps"); }
        for (int i = 0; i < nstamps; i++) if (!strcmp(STAMPS[i].id, f[1])) { fclose(fp); return bad("stamps.pdl: the stamp %s is listed twice", f[1]); }
        if (strcmp(f[3], "tile") && strcmp(f[3], "event")) { fclose(fp); return bad("stamps.pdl: the class of %s is tile or event", f[1]); }
        snprintf(STAMPS[nstamps].id, sizeof STAMPS[0].id, "%s", f[1]); snprintf(STAMPS[nstamps].label, sizeof STAMPS[0].label, "%s", f[2]); snprintf(STAMPS[nstamps].cls, sizeof STAMPS[0].cls, "%s", f[3]); nstamps++; }
    fclose(fp); if (!nstamps) return bad("stamps.pdl has no STAMP rows"); return 0;
}
static int stamp_picked(void) { char p[P], b[64] = ""; snprintf(p, sizeof p, "%s/stamp_pick.txt", SD); FILE *f = fopen(p, "r"); if (f) { if (fgets(b, sizeof b, f)) b[strcspn(b, "\r\n")] = 0; fclose(f); }
    for (int i = 0; i < nstamps; i++) if (!strcmp(STAMPS[i].id, b)) return i; return 0; }
static int stamp_pick(const char *id) { if (stamps_load()) return 1; int k = -1; for (int i = 0; i < nstamps; i++) if (!strcmp(STAMPS[i].id, id)) k = i;
    if (k < 0) return bad("pick: there is no stamp %s in the bank", id); char p[P]; snprintf(p, sizeof p, "%s/stamp_pick.txt", SD); char d[64]; snprintf(d, sizeof d, "%s\n", id); return write_atomic(p, d) ? bad("pick: cannot write stamp_pick.txt") : 0; }
static int cmd_stamp(const char *verb, int ac, char **av, const char *as) {
    if (stamps_load()) { fprintf(ERRF, "%s\n", MSG); return 2; }
    if (!strcmp(verb, "stamps")) { int pk = stamp_picked(); for (int i = 0; i < nstamps; i++) fprintf(OUT, "STAMP %s %s %s picked=%d\n", STAMPS[i].id, STAMPS[i].label, STAMPS[i].cls, i == pk); return 0; }
    if (!strcmp(verb, "pick")) { if (ac != 1) { fprintf(ERRF, "usage: pick <stamp id>   (see: stamps)\n"); return 2; } if (stamp_pick(av[0])) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "picked %s\n", av[0]); return 0; }
    if (ac != 2) { fprintf(ERRF, "usage: place <x> <y>   [as=author]   (paints the picked stamp)\n"); return 2; }
    char *e1, *e2; long x = strtol(av[0], &e1, 10), y = strtol(av[1], &e2, 10); if (*e1 || *e2) { fprintf(ERRF, "place: x and y are numbers\n"); return 2; }
    const char *id = STAMPS[stamp_picked()].id; if (doc_paint(id, x, y, as)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "place %s %ld,%ld: %d rows in the document\n", id, x, y, S.ndoc); return 0;
}
static int cursor_cell(long *x, long *y) { char eb[P], p[P + 96], b[600]; snprintf(eb, sizeof eb, "%s", EDIT); size_t L = strlen(eb); while (L > 1 && eb[L - 1] == '/') eb[--L] = 0; char *bn = strrchr(eb, '/'); bn = bn ? bn + 1 : eb;
    snprintf(p, sizeof p, "%s/%s-pchq/pieces/xelector_01/state.txt", EDIT, bn); FILE *f = fopen(p, "r"); if (!f) return 0; size_t n = fread(b, 1, sizeof b - 1, f); b[n] = 0; fclose(f); int hx_ = 0, hy_ = 0;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "pos_x=", 6)) { *x = atol(l + 6); hx_ = 1; } else if (!strncmp(l, "pos_y=", 6)) { *y = atol(l + 6); hy_ = 1; } } return hx_ && hy_; }
static void scene_events(void) {
    static char b[4096]; int n = snprintf(b, sizeof b, "%d %d 255 210 60 !Other1 0 2 1 \xe6\x97\x97\n", LV.gx, LV.gy);   /* the goal: a flag (the Chinese character 旗) */
    if (LEVELDOC) n += snprintf(b + n, sizeof b - (size_t)n, "%d %d 220 60 60 People1 0 2 1\n", LV.sx, LV.sy);   /* Make: the start cell shows the hero */
    for (int i = 0; i < LV.ne && n < 3600; i++) if (LEVELDOC || (MODE == M_PLATFORM && S.en[i].alive)) { int cx = LEVELDOC ? LV.ex[i] : (int)((S.en[i].b.x + S.en[i].b.w / 2) / PF_Q), cy = LEVELDOC ? LV.ey[i] : (int)((S.en[i].b.y + S.en[i].b.h / 2) / PF_Q); n += snprintf(b + n, sizeof b - (size_t)n, "%d %d 190 70 210 Monster %d 2 1\n", cx, cy, i % 8); }   /* enemies (怪) */
    for (int i = 0; i < LV.npw && n < 3600; i++) if (LEVELDOC || !S.pw_taken[i]) n += snprintf(b + n, sizeof b - (size_t)n, "%d %d 90 200 255 !Crystal 0 2 1\n", LV.px[i], LV.py[i]);   /* power-ups (晶) */
    if (MODE == M_PLATFORM && !LEVELDOC) for (int k = 0; k < 4 && n < 3700; k++) if (S.fb[k].alive) n += snprintf(b + n, sizeof b - (size_t)n, "%ld %ld 255 120 30 !Flame 0 2 1\n", (long)pf_floordiv(S.fb[k].x, PF_Q), (long)pf_floordiv(S.fb[k].y, PF_Q));   /* fireballs (火) */
    for (int i = 0; i < S.np && n < 3800; i++) { int cx, cy; scene_cell(i, &cx, &cy); unsigned c = PG_tok[i % MAXPL]; n += snprintf(b + n, sizeof b - (size_t)n, "%d %d %u %u %u People1 %d 2 1\n", cx, cy, (c >> 16) & 255, (c >> 8) & 255, c & 255, i); }
    char p[P + 16]; snprintf(p, sizeof p, "%s/events.txt", DESKDIR); write_atomic(p, b);
}
static void page_events(void) { if (scene_on) { scene_events(); return; } static char b[4096]; pg_events(b, sizeof b); char p[P + 16]; snprintf(p, sizeof p, "%s/events.txt", DESKDIR); write_atomic(p, b); }
static uint64_t pos_sig(void) { uint64_t h = fnv(FNV0, &S.np, sizeof S.np); h = fnv(h, &S.phase, sizeof S.phase); if (scene_on) for (int i = 0; i < S.np; i++) { int cx, cy; scene_cell(i, &cx, &cy); h = fnv(h, &cx, sizeof cx); h = fnv(h, &cy, sizeof cy); } if (scene_on && MODE == M_PLATFORM && !LEVELDOC) { for (int e = 0; e < LV.ne; e++) { long ex_ = (S.en[e].b.x + 100) / PF_Q, ey_ = (S.en[e].b.y + 100) / PF_Q; h = fnv(h, &ex_, sizeof ex_); h = fnv(h, &ey_, sizeof ey_); h = fnv(h, &S.en[e].alive, sizeof S.en[e].alive); } for (int k = 0; k < 4; k++) { long fx_ = S.fb[k].alive ? S.fb[k].x / PF_Q : -1, fy_ = S.fb[k].y / PF_Q; h = fnv(h, &fx_, sizeof fx_); h = fnv(h, &fy_, sizeof fy_); } h = fnv(h, S.pw_taken, sizeof S.pw_taken); } return fnv(h, S.pos, sizeof S.pos); }
/* the board engine sleeps until it is woken: a changed events.txt is read when the next frame is drawn, and nothing asks for one while nobody touches the window (seen 2026-11-18: the text popup said "tick 241, over" while the picture still showed the start). The same wake-up the key tool sends (the code 126, bound to nothing) is appended to the session's relay, at most every 80 ms. */
static void engine_nudge(void) { static long last_ms; static char sess[P + 64]; static long sess_ms; long now = now_ms(); if (now - last_ms < 80) return; last_ms = now;
    if (!sess[0] || now - sess_ms > 2000) { sess_ms = now; sess[0] = 0; char p[P + 64], ln[600]; snprintf(p, sizeof p, "%s/board/state/ui.txt", EDIT); FILE *f = fopen(p, "r"); if (!f) return;
        while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "bv_session=", 11)) { snprintf(sess, sizeof sess, "%s", ln + 11); sess[strcspn(sess, "\r\n")] = 0; } fclose(f); }
    if (!sess[0]) return; char rp[P + 128]; snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess); FILE *r = fopen(rp, "a"); if (!r) return; fputs("126\n", r); fclose(r); }
/* the engine loads a desk's picture (map.png) when the desk LOADS, so Make asks the board engine for a desk reload (the key `6`, through its own key file as the toolbar's Desk > reload does) after it redraws the level */
static void engine_reload(void) { char p[P + 64], ln[600], sess[P + 64] = "", op[P + 64], out[200]; snprintf(p, sizeof p, "%s/board/state/ui.txt", EDIT); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "bv_session=", 11)) { snprintf(sess, sizeof sess, "%s", ln + 11); sess[strcspn(sess, "\r\n")] = 0; } fclose(f);
    snprintf(op, sizeof op, "%s/&.widgits/board-viewer/tools/+x/bv_keys.+x", HOUSE); if (!sess[0] || access(sess, R_OK) || access(op, X_OK)) return; char *av[] = { op, sess, (char *)"6", NULL }; run_capture(av, out, sizeof out); }
static void page_make(void) {
    if (!page_on) return; mkdir_p(DESKDIR); char p[P + 16], out[300]; snprintf(p, sizeof p, "%s/events.txt", DESKDIR); FILE *f = fopen(p, "w"); if (f) fclose(f);   /* the last game's figures are emptied before the new page is drawn */
    { char ex[P], sheet[P], fr[P]; snprintf(ex, sizeof ex, "%s/@.apps/deskopoly/ops/+x/deskopoly_frames.+x", HOUSE); snprintf(sheet, sizeof sheet, "%s/../../../#.NNEST_ASSETS/tsots-characters/People1.png", HOUSE); snprintf(fr, sizeof fr, "%s/../../../#.NNEST_ASSETS/tsots-characters/frames", HOUSE);
      if (!access(ex, X_OK) && !access(sheet, R_OK) && !access(fr, W_OK)) { char *av[] = { ex, sheet, (char *)"People1", fr, NULL }; run_capture(av, out, sizeof out); } }   /* the 3D walk frames of People1: only the MISSING ones are made, never overwriting (deskopoly_frames, as the Teropoly driver does) */
    char *a[] = { (char *)(scene_on ? "scene" : "page"), DESKDIR, ATLAS, (char *)"noevents" }; if (verb_here(4, a, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "page refused: %.180s", out); return; }
    char raw[P + 16], png[P + 16], r2p[P + 96], sz[40]; snprintf(raw, sizeof raw, "%s/map.raw", DESKDIR); snprintf(png, sizeof png, "%s/map.png", DESKDIR); snprintf(r2p, sizeof r2p, "%s/&.widgits/_shared-lib/ops/+x/raw_to_png.+x", HOUSE);
    char *b[] = { (char *)"preview", DESKDIR, raw }; if (!verb_here(3, b, out, sizeof out)) { if (scene_on) snprintf(sz, sizeof sz, "%dx%d", LV.w * 24, LV.h * 24); else snprintf(sz, sizeof sz, "%dx%d", COLS * PG_block * HD_px, (ROWS + PG_start) * PG_block * HD_px); char *c[] = { r2p, raw, png, (char *)"--size", sz, NULL }; run_capture(c, out, sizeof out); unlink(raw); }   /* 2D: the picture beside map.txt, which the 2D renderer blits 1:1 and draws the events.txt figures over */
    set_desk("play"); page_events(); page_sig = pos_sig(); if (scene_on && LEVELDOC) engine_reload();
}
static void begin_game(void) {
    char out[300], sd[16]; archive_game(); mkdir_p(SD); snprintf(sd, sizeof sd, "%ld", seed_pref); char *n1[] = { (char *)"new", sd };
    if (verb_here(2, n1, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "new refused: %.180s", out); return; }
    for (int i = 1; i <= nplayers; i++) { char nm[4] = { (char)('A' + i - 1), 0 }; char *j[] = { (char *)"join", nm, (char *)(i == human_seat ? "human" : "pet") }; if (verb_here(3, j, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "join refused: %.180s", out); return; } }
    char *st[] = { (char *)"start" }; if (verb_here(1, st, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "start refused: %.180s", out); return; }
    mode_play = 1; reload(); write_ui(); page_make(); snprintf(dmsg, sizeof dmsg, "game started: %d players, seed %ld%s", nplayers, seed_pref, human_seat ? ", you are a seat at the table" : ", all pets");
}
static void human_live(const char *verb, const char *target, const char *arg2, const char *actor) {
    if (sync_state()) return; if (live(-1, actor, verb, target, arg2, NULL)) { snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); if (!(MODE == M_SNAKE && !strcmp(verb, "turn"))) reload(); }   /* a refused turn changed nothing: reloading would rewind the ticks since the last row */ else snprintf(dmsg, sizeof dmsg, "you: %s %s", verb, target ? target : ""); write_ui(); if (KIND == K_EDITOR) write_doc();
}
static void handle(const char *c) {
    char out[300];
    if (!strcmp(c, "players_up")) { if (!mode_play && nplayers < R_maxp) nplayers++; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "players_down")) { if (!mode_play && nplayers > 2) nplayers--; if (human_seat > nplayers) human_seat = 0; }
    else if (!strcmp(c, "human_next")) { if (!mode_play) human_seat = (human_seat + 1) % (nplayers + 1); }
    else if (!strcmp(c, "seed_next")) { if (!mode_play) seed_pref = seed_pref * 31 % 9973 + 7; }
    else if (!strcmp(c, "speed_next")) speed = (speed + 1) % 3;
    else if (!strcmp(c, "setup_reset")) { if (!mode_play) { nplayers = (int)R_maxp; human_seat = 0; seed_pref = R_seed; speed = 1; snprintf(dmsg, sizeof dmsg, "Choose the players, then START"); } }
    else if (!strcmp(c, "start")) { if (!mode_play && KIND != K_EDITOR) begin_game(); }
    else if (!strcmp(c, "step")) { if (mode_play && !running() && KIND != K_EDITOR) { if (!sync_state()) { int did; if (do_tick(&did)) snprintf(dmsg, sizeof dmsg, "stuck: %.180s", MSG); else snprintf(dmsg, sizeof dmsg, "stepped"); write_ui(); } } }
    else if (!strcmp(c, "new_game")) { if (mode_play && KIND != K_EDITOR) { archive_game(); mode_play = 0; memset(&S, 0, sizeof S); S.winner = -1; if (page_on) set_desk("board"); snprintf(dmsg, sizeof dmsg, "the last game is in archive/; choose the players, then START"); } }
    else if (KIND == K_LEDGER && !strcmp(c, "roll")) { if (!sync_state()) { char who[8] = ""; if (S.phase == PH_PLAY && S.human[S.cur]) snprintf(who, sizeof who, "p%d", S.cur + 1); human_live("roll", "", NULL, who); } }
    else if (KIND == K_REALTIME && MODE == M_SNAKE && (!strcmp(c, "left") || !strcmp(c, "right") || !strcmp(c, "up") || !strcmp(c, "down"))) { if (!S.human[0]) snprintf(dmsg, sizeof dmsg, "no human seat"); else if (!sync_state()) { char dc[2] = { (char)(c[0] - 'a' + 'A'), 0 }; human_live("turn", dc, NULL, "p1"); } }
    else if (KIND == K_REALTIME && (!strcmp(c, "left") || !strcmp(c, "right") || !strcmp(c, "stay") || (MODE == M_PLATFORM && (!strcmp(c, "jump") || !strcmp(c, "fire"))))) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) { hp = i + 1; break; } if (!hp) snprintf(dmsg, sizeof dmsg, "no human seat"); else { char a[8]; snprintf(a, sizeof a, "p%d", hp); if (!sync_state()) human_live("input", c, NULL, a); } }
    else if (KIND == K_EDITOR && LEVELDOC && !strcmp(c, "blank_level")) { if (!sync_state()) { if (doc_blank(20, 12, "author")) { snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); reload(); } else snprintf(dmsg, sizeof dmsg, "a starter level: 20 x 12, ground along the bottom"); } }
    else if (KIND == K_EDITOR && LEVELDOC && !strncmp(c, "pick_", 5)) { if (stamp_pick(c + 5)) snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); else snprintf(dmsg, sizeof dmsg, "picked %s", c + 5); }
    else if (KIND == K_EDITOR && LEVELDOC && !strcmp(c, "place")) { long x = 0, y = 0; if (stamps_load()) snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); else if (!cursor_cell(&x, &y)) snprintf(dmsg, sizeof dmsg, "no cursor cell: the editor cursor was not found"); else if (!sync_state()) { const char *id = STAMPS[stamp_picked()].id; if (doc_paint(id, x, y, "author")) { snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); reload(); } else snprintf(dmsg, sizeof dmsg, "%s at %ld,%ld", id, x, y); } }
    else if (KIND == K_EDITOR && LEVELDOC && !strncmp(c, "paint_", 6)) { long x = 0, y = 0; if (!cursor_cell(&x, &y)) snprintf(dmsg, sizeof dmsg, "no cursor cell: the editor cursor was not found"); else if (!sync_state()) { if (doc_paint(c + 6, x, y, "author")) { snprintf(dmsg, sizeof dmsg, "refused: %.200s", MSG); reload(); } else snprintf(dmsg, sizeof dmsg, "%s at %ld,%ld", c + 6, x, y); } }
    else if (KIND == K_EDITOR && !strncmp(c, "add_", 4)) { char id[32]; snprintf(id, sizeof id, "%s", c + 4); if (!sync_state()) { int ix = 0; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], id)) ix = 1; if (!strcmp(id, "item")) { snprintf(id, sizeof id, "item%d", S.ndoc + 1); ix = 0; } char tx[80]; snprintf(tx, sizeof tx, "%s (added from the bar)", id); if (LEVELDOC && !strcmp(id, "start")) snprintf(tx, sizeof tx, "2 10"); if (LEVELDOC && !strcmp(id, "goal")) snprintf(tx, sizeof tx, "17 10"); human_live(ix ? "set" : "add", id, tx, "author"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "remove_last")) { if (!sync_state()) { if (S.ndoc) { char id[32]; snprintf(id, sizeof id, "%s", S.did[S.ndoc - 1]); human_live("del", id, NULL, "author"); } else snprintf(dmsg, sizeof dmsg, "the document is empty"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "publish")) { if (!sync_state()) human_live("publish", "", NULL, "author"); }
    else if (!strcmp(c, "refresh")) { reload(); }
    else snprintf(dmsg, sizeof dmsg, "unknown command %.40s", c);
    (void)out; dirty = 1;
}
static void add_btn(char *o, size_t cap, int *nb, const char *label, const char *cmd) { char b[P + 300]; snprintf(b, sizeof b, "gmb_%d_label=%s\ngmb_%d_act='%s' ctl %s '%s'\n", *nb, label, *nb, SELF, cmd, GDIR); (*nb)++; strncat(o, b, cap - strlen(o) - 1); }
static void add_txt(char *o, size_t cap, int *nt, const char *t) { char b[400]; snprintf(b, sizeof b, "gmt_%d_text=%s\n", (*nt)++, t[0] ? t : " "); strncat(o, b, cap - strlen(o) - 1); }
/* the sidebar's list for the editor window (vars file <editor>/state/stamps_ui.txt): stamp_on, n_stamp, stamp_<i>_id / _label / _cls (the class carries `stamp-picked` for the picked one). Off (stamp_on empty) for a game that has no level document. */
static void stamps_publish(void) {
    char p[P], o[6000] = "", b[900]; snprintf(p, sizeof p, "%s/state/stamps_ui.txt", EDIT);
    if (KIND == K_EDITOR && LEVELDOC && S.ndoc && !stamps_load()) { int pk = stamp_picked(); snprintf(b, sizeof b, "stamp_on=1\nn_stamp=%d\nstamp_picked=%s\n", nstamps, STAMPS[pk].id); strncat(o, b, sizeof o - strlen(o) - 1);
        for (int i = 0; i < nstamps; i++) { snprintf(b, sizeof b, "stamp_%d_id=%s\nstamp_%d_label=%s%s\nstamp_%d_cls=%s\nstamp_%d_act='%s' ctl pick_%s '%s'\n", i, STAMPS[i].id, i, i == pk ? "> " : "", STAMPS[i].label, i, i == pk ? "stamp-picked" : STAMPS[i].cls, i, SELF, STAMPS[i].id, GDIR); strncat(o, b, sizeof o - strlen(o) - 1); } }
    else strcpy(o, "stamp_on=\nstamp_off=1\nn_stamp=0\n");
    write_atomic(p, o);
}
static void publish(void) {
    static char o[12000]; o[0] = 0; size_t C = sizeof o; int nt = 0, nb = 0; char b[300]; int run = running();
    snprintf(b, sizeof b, "gm_on=1\ngm_pid=%ld\ngm_title=%s\n", (long)getpid(), TITLE); strncat(o, b, C - strlen(o) - 1);
    snprintf(b, sizeof b, "%.200s", dmsg); add_txt(o, C, &nt, b);
    if (KIND == K_EDITOR) {
        snprintf(b, sizeof b, "%d rows, %s, %ld edits", S.ndoc, S.published ? "published" : "not published", S.nedits); add_txt(o, C, &nt, b);
        if (LEVELDOC) { if (!S.ndoc) add_btn(o, C, &nb, "new level", "blank_level"); else { char pl[80] = "place stamp"; if (!stamps_load()) snprintf(pl, sizeof pl, "place: %s", STAMPS[stamp_picked()].label); add_btn(o, C, &nb, pl, "place"); } add_btn(o, C, &nb, "publish", "publish"); }
        else { add_btn(o, C, &nb, "add start", "add_start"); add_btn(o, C, &nb, "add goal", "add_goal"); add_btn(o, C, &nb, "add item", "add_item"); add_btn(o, C, &nb, "remove last", "remove_last"); add_btn(o, C, &nb, "publish", "publish"); }
    } else if (!mode_play) {
        snprintf(b, sizeof b, "%d players, seed %ld", nplayers, seed_pref); add_txt(o, C, &nt, b); add_btn(o, C, &nb, "players -", "players_down"); add_btn(o, C, &nb, "players +", "players_up");
        { char hp[8] = "none"; if (human_seat) snprintf(hp, sizeof hp, "p%d", human_seat); snprintf(b, sizeof b, "you play: %s", hp); } add_btn(o, C, &nb, b, "human_next");
        snprintf(b, sizeof b, "seed %ld (next)", seed_pref); add_btn(o, C, &nb, b, "seed_next"); snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(o, C, &nb, b, "speed_next"); add_btn(o, C, &nb, "START GAME", "start");
    } else {
        int over = S.phase == PH_OVER; if (KIND == K_LEDGER) snprintf(b, sizeof b, "r%ld  p%d %s  %s", S.round, S.cur + 1, S.name[S.cur], over ? "OVER" : (run ? "RUNNING" : "STOPPED")); else snprintf(b, sizeof b, "tick %ld  %s", S.tick, over ? "OVER" : (run ? "RUNNING" : "STOPPED")); add_txt(o, C, &nt, b);
        for (int i = 0; i < S.np; i++) { if (MODE == M_SNAKE) snprintf(b, sizeof b, "p%d %s (%s): length %d, score %d", i + 1, S.name[i], S.human[i] ? "you" : "pet", S.slen + S.sgrow, S.sscore); else snprintf(b, sizeof b, "p%d %s (%s): %d/%d", i + 1, S.name[i], S.human[i] ? "you" : "pet", S.pos[i], MODE == M_WALK ? LV.gx : LENGTH); add_txt(o, C, &nt, b); }
        if (KIND == K_LEDGER && S.l_valid) add_txt(o, C, &nt, S.tk[(S.ntk - 1) % 5]);
        if (over && S.winner >= 0) { snprintf(b, sizeof b, "winner: %s", S.name[S.winner]); add_txt(o, C, &nt, b); }
        if (!over && KIND == K_LEDGER && S.human[S.cur]) add_btn(o, C, &nb, "Roll", "roll");
        if (!over && KIND == K_REALTIME && MODE == M_SNAKE && S.human[0]) { add_btn(o, C, &nb, "Left", "left"); add_btn(o, C, &nb, "Up", "up"); add_btn(o, C, &nb, "Down", "down"); add_btn(o, C, &nb, "Right", "right"); }
        else if (!over && KIND == K_REALTIME) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) hp = 1; if (hp) { add_btn(o, C, &nb, "Left", "left"); add_btn(o, C, &nb, "Stay", "stay"); add_btn(o, C, &nb, "Right", "right"); if (MODE == M_PLATFORM) { add_btn(o, C, &nb, "Jump", "jump"); add_btn(o, C, &nb, "Fire", "fire"); } } }
        if (!over && !run) add_btn(o, C, &nb, "Step", "step"); snprintf(b, sizeof b, "speed: %s", SPEED_NAME[speed]); add_btn(o, C, &nb, b, "speed_next"); if (over) add_btn(o, C, &nb, "New game", "new_game");
    }
    snprintf(b, sizeof b, "gm_nt=%d\ngm_nb=%d\ngm_ne=0\ngm_nl=0\n", nt, nb); strncat(o, b, C - strlen(o) - 1); write_atomic(FEED, o); stamps_publish(); dirty = 0; ST.publishes++;
}
static int driver_main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: game_core driver <house_root> <book_app_dir> <editor_app_dir>\n"); return 2; }
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); snprintf(BOOK, sizeof BOOK, "%s", argv[3]); snprintf(EDIT, sizeof EDIT, "%s", argv[4]);
    { char bb[P]; snprintf(bb, sizeof bb, "%s", BOOK); size_t L = strlen(bb); while (L > 1 && bb[L - 1] == '/') bb[--L] = 0; char *bn = strrchr(bb, '/'); snprintf(NAME, sizeof NAME, "%s", bn ? bn + 1 : bb); }
    if (!good_word(NAME, 40, 1)) { fprintf(stderr, "game_core driver: bad book folder name\n"); return 2; }
    snprintf(APP, sizeof APP, "%s", BOOK); { ssize_t k = readlink("/proc/self/exe", SELF, sizeof SELF - 1); if (k > 0) SELF[k] = 0; else snprintf(SELF, sizeof SELF, "%s", argv[0]); }
    snprintf(GDIR, sizeof GDIR, "%s/state/games/%s", EDIT, NAME); snprintf(SD, sizeof SD, "%s/game", GDIR); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", SD); snprintf(ARCH, sizeof ARCH, "%s/archive", GDIR);
    snprintf(FEED, sizeof FEED, "%s/state/game_ui.txt", EDIT); snprintf(ACT, sizeof ACT, "%s/action.txt", GDIR); snprintf(SW, sizeof SW, "%s/state/world/switches.txt", EDIT);
    OUT = stdout; ERRF = stderr; if (load_config()) { fprintf(stderr, "game_core driver: %s\n", MSG); return 1; } seed_pref = R_seed; nplayers = (int)R_maxp;   /* the setup screen starts with the table full: --players N at generation time */
    { char d[P]; snprintf(d, sizeof d, "%s/state/games", EDIT); mkdir(d, 0755); mkdir(GDIR, 0755); }
    { const char *ow = getenv("GAME_OWNER_PID"); OWNER = ow ? atol(ow) : 0; }
    if (KIND == K_LEDGER && MODE == M_TRACK) { char pp[P]; snprintf(pp, sizeof pp, "%s/rulesets/%s/page.pdl", APP, RS);   /* a game with a page.pdl is drawn; one without keeps the authored empty board (nothing below runs) */
        if (!access(pp, R_OK)) { if (load_page()) snprintf(dmsg, sizeof dmsg, "page.pdl refused: %.180s", MSG); else {
            char eb[P]; snprintf(eb, sizeof eb, "%s", EDIT); size_t L = strlen(eb); while (L > 1 && eb[L - 1] == '/') eb[--L] = 0; char *bn = strrchr(eb, '/'); bn = bn ? bn + 1 : eb;
            snprintf(WORLD, sizeof WORLD, "%s/%s-pchq/pieces/world_01/state.txt", EDIT, bn); snprintf(DESKDIR, sizeof DESKDIR, "%s/%s-pchq/pieces/system/maps/%s/play", BOOK, NAME, NAME); snprintf(ATLAS, sizeof ATLAS, "%s/%s-pchq/pieces/system/maps/%s/board/cells.rgba", BOOK, NAME, NAME); page_on = 1; } } }
    if ((KIND == K_REALTIME && WALKISH) || (KIND == K_EDITOR && LEVELDOC)) {   /* the level is drawn as the book's "play" desk (scene verb) and the heroes follow the ledger, as the Teropoly page does */
        char eb[P]; snprintf(eb, sizeof eb, "%s", EDIT); size_t L = strlen(eb); while (L > 1 && eb[L - 1] == '/') eb[--L] = 0; char *bn = strrchr(eb, '/'); bn = bn ? bn + 1 : eb;
        snprintf(WORLD, sizeof WORLD, "%s/%s-pchq/pieces/world_01/state.txt", EDIT, bn); snprintf(DESKDIR, sizeof DESKDIR, "%s/%s-pchq/pieces/system/maps/%s/play", BOOK, NAME, NAME); snprintf(ATLAS, sizeof ATLAS, "%s/%s-pchq/pieces/system/maps/%s/board/cells.rgba", BOOK, NAME, NAME); page_on = 1; scene_on = 1; }
    static int ui_play; ui_play = ui_mode_wanted(); if (ui_play) ui_mode_set("play");
    char lk[P]; snprintf(lk, sizeof lk, "%s/driver.lock", GDIR); int lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "game_core driver: cannot open the lock file %s (does the editor's state folder exist?)\n", lk); return 1; }
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "game_core driver: already running\n"); return 0; }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    FILE *devnull = fopen("/dev/null", "w"); OUT = devnull ? devnull : stdout; ERRF = OUT;
    if (!reload() && S.phase >= PH_PLAY) { mode_play = 1; page_make(); snprintf(dmsg, sizeof dmsg, "a game was in progress; it continues while the Player menu says playing"); }
    if (KIND == K_EDITOR) { mode_play = 1; if (S.phase == PH_NONE) { char o2[200]; char *n1[] = { (char *)"new" }; if (verb_here(1, n1, o2, sizeof o2)) snprintf(dmsg, sizeof dmsg, "new refused: %.180s", o2); else { reload(); write_doc(); snprintf(dmsg, sizeof dmsg, "an empty document: add start and goal, then publish"); } } }
    struct stat s; long cursor = stat(ACT, &s) ? 0 : (long)s.st_size; char relay[P]; snprintf(relay, sizeof relay, "%s/state/interact_relay.txt", EDIT); long kcur = stat(relay, &s) ? 0 : (long)s.st_size;   /* the driver tails the editor's key file with its OWN cursor, starting at the end: keys pressed before it started are never replayed */
    if (load_tuning()) { snprintf(dmsg, sizeof dmsg, "%.150s (the default pace is used)", MSG); TICK_MS = 0; }   /* a refused tick_ms is shown in the feed, not hidden: the game still runs at the kind's default */
    publish();
    /* THE TICK LOOP (2026-11-09). Everything is in this process: no fork, no shell, one monotonic deadline per step (next_us += pace, so jitter never accumulates into drift), the sleep is a ppoll() that ends at the next deadline OR when the
     * game folder changes (a button press is seen at once). Live = a game in play, the editor's running=1, not waiting for a human roll: it wakes once per step (at most every HK_MS for housekeeping). Not live (setup, paused, stopped, over, a human thinking, an
     * editor game): it blocks IDLE_MS in ppoll, so a paused game costs 2 wakeups a second and no tick. A step that is late is run late; at most CATCH steps run per wake and the rest of the lateness is dropped (the game slows, it never bursts), counted in `dropped`. */
    int ino = inotify_init1(IN_NONBLOCK | IN_CLOEXEC); if (ino >= 0 && inotify_add_watch(ino, GDIR, IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO) < 0) { close(ino); ino = -1; }
    const char *stats_path = getenv("GAME_TICK_STATS");   /* measuring only: when set, the per-step lateness and interval are kept in memory and summarised in this file at pause and at exit; nothing is written otherwise */
    long next_us = 0, last_tick_us = 0, seg_us = 0, last_pub = 0; int was_live = 0, stuck = 0;
    while (!stop_) {
        if (OWNER > 0 ? (kill((pid_t)OWNER, 0) != 0 && errno == ESRCH) : getppid() == 1) break;
        ST.loops++;
        if (!stat(ACT, &s) && s.st_size > cursor) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[128]; while (fgets(ln, sizeof ln, f)) { char *e = strpbrk(ln, "\r\n"); if (e) *e = 0; if (ln[0]) { handle(ln); stuck = 0; } } cursor = ftell(f); fclose(f); } if (cursor > 65536 && truncate(ACT, 0) == 0) cursor = 0; }
        else if (!stat(ACT, &s) && s.st_size < cursor) cursor = 0;
        static int was_run = -1; int run = running(); int run0 = run; if (run != was_run) { was_run = run; dirty = 1; }
        if (KIND == K_REALTIME && MODE == M_SNAKE) { int kc[32]; int kn = keys_poll(relay, &kcur, kc, 32);   /* arrows 1000-1003 steer a human snake while the game runs; while stopped the cursor still advances, so keys pressed during a pause are dropped, not queued */
            if (kn > 0 && mode_play && run0 && S.phase == PH_PLAY && S.human[0] && !sync_state()) { int rf, ig; int ap = keys_apply(kc, kn, &rf, &ig); if (ap) { snprintf(dmsg, sizeof dmsg, "arrow keys: %d turn(s)", ap); dirty = 1; } } }
        if (KIND == K_REALTIME && MODE == M_PLATFORM) { int kc[32]; int kn = keys_poll(relay, &kcur, kc, 32);
            if (kn > 0 && mode_play && run0 && S.phase == PH_PLAY && !sync_state()) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) hp = 1; if (hp) { int ap = plat_keys_apply(kc, kn); if (ap) { snprintf(dmsg, sizeof dmsg, "keys: %d input(s)", ap); dirty = 1; } } } }
        struct stat ls; if (mode_play && !stat(LEDGER, &ls) && ls.st_size != KNOWN_SIZE) { reload(); dirty = 1; stuck = 0; }   /* somebody else (the CLI) wrote the ledger */
        int live = mode_play && run && KIND != K_EDITOR && S.phase == PH_PLAY && !stuck && !(KIND == K_LEDGER && S.human[S.cur]);
        long t = now_us(), pace_us = pace_ms(speed) * 1000L;
        if (live) {
            if (!was_live) { was_live = 1; seg_us = t; next_us = t + pace_us; last_tick_us = 0; }
            int steps = 0, cap = KIND == K_REALTIME ? CATCH_RT : 1;
            while (t >= next_us && steps < cap && S.phase == PH_PLAY && !stuck) { int did = 0;
                if (stats_path) stats_note(t - next_us, last_tick_us ? t - last_tick_us : 0, steps);
                last_tick_us = t;
                if (do_tick(&did)) { snprintf(dmsg, sizeof dmsg, "stuck: %.180s", MSG); reload(); stuck = 1; } else if (did && (KIND == K_LEDGER || S.tick % R_ckpt == 0)) { write_ui(); } dirty = 1;
                if (S.phase == PH_OVER) { write_ui(); snprintf(dmsg, sizeof dmsg, "the game is over, winner %s", S.winner >= 0 ? S.name[S.winner] : "-"); dirty = 1; }
                next_us += pace_us; steps++; t = now_us(); }
            if (t >= next_us) { if (S.phase == PH_PLAY && !stuck) ST.dropped++; next_us = t + pace_us; }
        } else {
            if (was_live) { was_live = 0; stats_seg_end(now_us() - seg_us, pace_us, stats_path); }
            if (mode_play && run && KIND == K_LEDGER && S.phase == PH_PLAY && S.human[S.cur]) { char yt[120]; snprintf(yt, sizeof yt, "your turn, %s: press Roll", S.name[S.cur]); if (strcmp(dmsg, yt)) { snprintf(dmsg, sizeof dmsg, "%s", yt); dirty = 1; } }
        }
        if (scene_on && LEVELDOC && mode_play) { static int dsig_set; static uint64_t dsig; uint64_t dg = doc_hash(); if (!dsig_set || dg != dsig) { dsig_set = 1; dsig = dg; if (S.ndoc) page_make(); } }   /* Make: the level is redrawn when the document changes */
        if (page_on && mode_play) { uint64_t sg = pos_sig(); if (sg != page_sig) { page_sig = sg; page_events(); if (scene_on) engine_nudge(); } static long dg = 0; long td = now_ms(); if (td - dg >= 2000) { dg = td; set_desk("play"); } }   /* the figures follow the ledger; the desk guard re-points the world after a board relaunch (as the Teropoly driver) */
        { long tp = now_us(); if (dirty && tp - last_pub >= 200000) { publish(); last_pub = tp; } }   /* the feed is rewritten at most 5 times a second, whatever the step pace */
        { long tn = now_us(), wake = tn + (live ? HK_MS : IDLE_MS) * 1000L; if (live && next_us < wake) wake = next_us; if (dirty && last_pub + 200000 < wake) wake = last_pub + 200000;
          if (wake > tn) { struct pollfd pf = { ino, POLLIN, 0 }; struct timespec to = { (wake - tn) / 1000000L, ((wake - tn) % 1000000L) * 1000L }; ppoll(&pf, ino >= 0 ? 1 : 0, &to, NULL);
              if (ino >= 0) { char ib[512]; while (read(ino, ib, sizeof ib) > 0) { } } } }
    }
    if (page_on) set_desk("board");   /* the world goes back to the authored board when the driver ends */
    if (ui_play) ui_mode_set("edit");   /* and the window goes back to its edit bars */
    if (was_live) stats_seg_end(now_us() - seg_us, pace_ms(speed) * 1000L, stats_path);
    { char sp[P]; snprintf(sp, sizeof sp, "%s/state/stamps_ui.txt", EDIT); write_atomic(sp, "stamp_on=\nn_stamp=0\n"); }
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
