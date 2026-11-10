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

#define P 4096
#define CORE_VERSION 1
#define MAXPL 8
#define DOCMAX 64
#define MAXJ 64
#define ROWMAX 1000

enum { K_LEDGER = 1, K_REALTIME = 2, K_EDITOR = 3 };
enum { PH_NONE = 0, PH_LOBBY = 1, PH_PLAY = 2, PH_OVER = 3 };
/* realtime MODE (board.pdl `MODE | track|snake|walk`): track = the original stub (x += dir on a track), snake = ONE snake on a grid (BOARD cols/rows, RULE walls/start_len/grow/score_food, seeded food), walk = a hero walking along the START row of a LEVEL file (no physics yet) */
enum { M_TRACK = 0, M_SNAKE = 1, M_WALK = 2 };
/* ledger finish rule (rules.pdl `RULE | finish | reach|exact_stay|bounce`): reach = a roll that reaches or passes the end wins (the original rule); exact_stay = the end must be hit exactly, an overshoot leaves you where you are; bounce = the surplus walks back */
enum { F_REACH = 0, F_STAY = 1, F_BOUNCE = 2 };
#define SNMAX 1100
#define LVMAXW 64
#define LVMAXH 32

static FILE *OUT, *ERRF;                     /* stdout/stderr for the CLI; memory streams when the driver calls a verb in-process */
static char APP[P], SD[P], LEDGER[P], TITLE[96] = "Game", RS[40] = "default", MSG[400];
static int KIND = K_LEDGER, MODE = M_TRACK, FINISH = F_REACH, WRAP = 0, LEVELDOC = 0;
static long R_maxp = 4, R_minp = 2, R_maxrows = 5000, R_ckpt = 10, R_seed = 7, R_maxticks = 4000, R_docmax = 40, R_startlen = 3, R_grow = 1, R_scorefood = 10, R_needclear = 0;
static int LENGTH = 30, NJ; static int JF[MAXJ], JT[MAXJ];
static int COLS = 16, ROWS = 12;
static char T_UP[48] = "jumped", T_DOWN[48] = "jumped", LEVELREL[300], LEVEL_SOLID[16] = "#";
static char REQ[8][32]; static int NREQ;
static uint64_t RULES_HASH;
/* a level (mode walk, and the editor's level document): LVROW[y] is a string of LW tiles; START and GOAL are cells */
static struct { int w, h, sx, sy, gx, gy; char rows[LVMAXH][LVMAXW + 1]; char name[64]; } LV;

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
    return 0;
}
static int level_render(char *o, size_t cap) {
    int n = snprintf(o, cap, "# level.pdl - a Teario level, the ONE format Make writes and Play reads (BOOK 16.game TEARIO plan 1.2 and 5.1). Top row is 0; . empty, # solid ground.\nMETA  | name | %s\nMETA  | size | %d x %d\nSTART | %d | %d\nGOAL  | %d | %d\n", LV.name, LV.w, LV.h, LV.sx, LV.sy, LV.gx, LV.gy);
    for (int y = 0; y < LV.h && n > 0 && (size_t)n < cap; y++) n += snprintf(o + n, cap - (size_t)n, "ROW   | %d | %s\n", y, LV.rows[y]);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}
static uint64_t level_hash(void) { static char b[8192]; int n = level_render(b, sizeof b); return n < 0 ? 0 : fnv(FNV0, b, (size_t)n); }
static int level_load_file(const char *path, uint64_t *fold) {
    FILE *f = fopen(path, "r"); if (!f) return bad("cannot read the level file %s", path); char ln[400], *fl[6]; int ns = 0, ng = 0, maxy = -1; memset(&LV, 0, sizeof LV); LV.sx = LV.sy = LV.gx = LV.gy = -1; snprintf(LV.name, sizeof LV.name, "level");
    while (fgets(ln, sizeof ln, f)) { if (fold) *fold = fnv(*fold, ln, strlen(ln)); if (ln[0] == '#') continue; int n = split_row(ln, fl, 6); if (n < 2) continue;
        if (!strcmp(fl[0], "META") && n >= 3 && !strcmp(fl[1], "name")) snprintf(LV.name, sizeof LV.name, "%s", fl[2]);
        else if (!strcmp(fl[0], "START") && n >= 3) { LV.sx = atoi(fl[1]); LV.sy = atoi(fl[2]); ns++; }
        else if (!strcmp(fl[0], "GOAL") && n >= 3) { LV.gx = atoi(fl[1]); LV.gy = atoi(fl[2]); ng++; }
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
    RULES_HASH = FNV0; NJ = 0; NREQ = 0; MODE = M_TRACK; FINISH = F_REACH; WRAP = 0; LEVELDOC = 0; LEVELREL[0] = 0; snprintf(T_UP, sizeof T_UP, "jumped"); snprintf(T_DOWN, sizeof T_DOWN, "jumped");
    const char *files[2] = { "board.pdl", "rules.pdl" };
    for (int k = 0; k < 2; k++) { snprintf(p, sizeof p, "%s/rulesets/%s/%s", APP, RS, files[k]); f = fopen(p, "r"); if (!f) return bad("missing %s", p);
        RULES_HASH = fnv(RULES_HASH, files[k], strlen(files[k]));
        while (fgets(ln, sizeof ln, f)) { RULES_HASH = fnv(RULES_HASH, ln, strlen(ln)); if (ln[0] == '#') continue; char cp[1024]; snprintf(cp, sizeof cp, "%s", ln); n = split_row(cp, fl, 10); if (n < 2) continue;
            if (!strcmp(fl[0], "TRACK") && n >= 3 && !strcmp(fl[1], "length")) { LENGTH = atoi(fl[2]); if (LENGTH < 3 || LENGTH > 1000) { fclose(f); return bad("board.pdl: TRACK length is 3 to 1000"); } }
            else if (!strcmp(fl[0], "JUMP") && n >= 3) { if (NJ >= MAXJ) { fclose(f); return bad("board.pdl: at most %d JUMP rows", MAXJ); } JF[NJ] = atoi(fl[1]); JT[NJ] = atoi(fl[2]); NJ++; }
            else if (!strcmp(fl[0], "REQUIRE") && NREQ < 8) snprintf(REQ[NREQ++], 32, "%s", fl[1]);
            else if (!strcmp(fl[0], "MODE") && n >= 2) { MODE = !strcmp(fl[1], "track") ? M_TRACK : !strcmp(fl[1], "snake") ? M_SNAKE : !strcmp(fl[1], "walk") ? M_WALK : -1; if (MODE < 0) { fclose(f); return bad("board.pdl: MODE is track, snake or walk"); } }
            else if (!strcmp(fl[0], "BOARD") && n >= 3 && !strcmp(fl[1], "cols")) COLS = atoi(fl[2]);
            else if (!strcmp(fl[0], "BOARD") && n >= 3 && !strcmp(fl[1], "rows")) ROWS = atoi(fl[2]);
            else if (!strcmp(fl[0], "TALK") && n >= 3 && !strcmp(fl[1], "up")) snprintf(T_UP, sizeof T_UP, "%s", fl[2]);
            else if (!strcmp(fl[0], "TALK") && n >= 3 && !strcmp(fl[1], "down")) snprintf(T_DOWN, sizeof T_DOWN, "%s", fl[2]);
            else if (!strcmp(fl[0], "LEVEL") && n >= 3 && !strcmp(fl[1], "file")) snprintf(LEVELREL, sizeof LEVELREL, "%s", fl[2]);
            else if (!strcmp(fl[0], "LEVEL") && n >= 3 && !strcmp(fl[1], "solid")) snprintf(LEVEL_SOLID, sizeof LEVEL_SOLID, "%s", fl[2]);
            else if (!strcmp(fl[0], "RULE") && n >= 3) { long v = atol(fl[2]);
                if (!strcmp(fl[1], "max_players")) R_maxp = v; else if (!strcmp(fl[1], "max_rows")) R_maxrows = v; else if (!strcmp(fl[1], "ckpt_every")) R_ckpt = v;
                else if (!strcmp(fl[1], "default_seed")) R_seed = v; else if (!strcmp(fl[1], "max_ticks")) R_maxticks = v; else if (!strcmp(fl[1], "max_doc_rows")) R_docmax = v;
                else if (!strcmp(fl[1], "min_players")) R_minp = v; else if (!strcmp(fl[1], "start_len")) R_startlen = v; else if (!strcmp(fl[1], "grow")) R_grow = v; else if (!strcmp(fl[1], "score_food")) R_scorefood = v;
                else if (!strcmp(fl[1], "publish_needs_clear")) R_needclear = v;
                else if (!strcmp(fl[1], "finish")) { FINISH = !strcmp(fl[2], "reach") ? F_REACH : !strcmp(fl[2], "exact_stay") ? F_STAY : !strcmp(fl[2], "bounce") ? F_BOUNCE : -1; if (FINISH < 0) { fclose(f); return bad("rules.pdl: finish is reach, exact_stay or bounce"); } }
                else if (!strcmp(fl[1], "walls")) { if (!strcmp(fl[2], "wrap")) WRAP = 1; else if (!strcmp(fl[2], "solid")) WRAP = 0; else { fclose(f); return bad("rules.pdl: walls is solid or wrap"); } }
                else if (!strcmp(fl[1], "doc_format")) { if (!strcmp(fl[2], "level")) LEVELDOC = 1; else { fclose(f); return bad("rules.pdl: doc_format is level"); } } } }
        fclose(f); }
    if (R_minp < 1 || R_minp > R_maxp) return bad("rules.pdl: min_players is 1 to max_players");
    if (R_maxp < 1 || R_maxp > MAXPL || (R_maxp < 2 && MODE != M_SNAKE && MODE != M_WALK)) return bad("rules.pdl: max_players is 2 to %d", MAXPL);
    if (MODE == M_SNAKE) { if (KIND != K_REALTIME) return bad("board.pdl: MODE snake needs a realtime game"); if (R_maxp != 1 || R_minp != 1) return bad("rules.pdl: a snake game has exactly one snake (min_players and max_players 1)");
        if (COLS < 6 || COLS > 32 || ROWS < 6 || ROWS > 32 || COLS * ROWS > SNMAX) return bad("board.pdl: BOARD cols and rows are 6 to 32 (at most %d cells)", SNMAX); if (R_startlen < 1 || R_startlen > COLS / 2 + 1) return bad("rules.pdl: start_len is 1 to cols/2+1"); if (R_grow < 0 || R_grow > 20) return bad("rules.pdl: grow is 0 to 20"); }
    if (MODE == M_WALK) { if (KIND != K_REALTIME) return bad("board.pdl: MODE walk needs a realtime game"); if (!LEVELREL[0]) return bad("board.pdl: MODE walk needs a LEVEL | file row"); }
    if (R_ckpt < 1 || R_ckpt > 1000) return bad("rules.pdl: ckpt_every is 1 to 1000");
    if (R_maxrows < 10 || R_maxrows > 100000) return bad("rules.pdl: max_rows is 10 to 100000");
    if (R_docmax < 1 || R_docmax > DOCMAX) return bad("rules.pdl: max_doc_rows is 1 to %d", DOCMAX);
    if (MODE == M_WALK) { snprintf(p, sizeof p, "%s/%s", APP, LEVELREL); RULES_HASH = fnv(RULES_HASH, "level", 5); if (level_load_file(p, &RULES_HASH)) return 1; LENGTH = LV.w - 1; }   /* the level file is part of the rules hash: editing it after a game began refuses that ledger */
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
static int spawn_food(void) {
    int nfree = 0; for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!on_body(x, y, S.slen)) nfree++;
    if (!nfree) return 0; int pick = (int)(mix((uint64_t)S.seed * 1000003ULL + (uint64_t)S.sfoods * 16 + 7) % (uint64_t)nfree), k = 0;
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!on_body(x, y, S.slen)) { if (k++ == pick) { S.sfood_x = x; S.sfood_y = y; S.sfoods++; return 1; } }
    return 0;
}
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
static void step_tick(void) {
    if (MODE == M_SNAKE) { snake_step(); return; }
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
        if (MODE != M_TRACK) { size_t L = strlen(out->res); snprintf(out->res + L, sizeof out->res - L, ",mode=%s", MODE == M_SNAKE ? "snake" : "walk"); }
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
        if (MODE == M_SNAKE) snake_init(); if (MODE == M_WALK) for (int i = 0; i < S.np; i++) S.pos[i] = LV.sx;
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
    if (KIND == K_REALTIME && MODE == M_WALK) n += snprintf(o + n, cap - (size_t)n, "level=%s\nlevel_w=%d\nlevel_h=%d\ngoal=%d,%d\nstart=%d,%d\n", LV.name, LV.w, LV.h, LV.gx, LV.gy, LV.sx, LV.sy);
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
        else if (!strcmp(nm, "jumps")) snprintf(v, sizeof v, "%d", NJ); else if (!strcmp(nm, "rolls")) snprintf(v, sizeof v, "%ld", S.nrolls); else if (!strcmp(nm, "mode")) snprintf(v, sizeof v, "%s", MODE == M_SNAKE ? "snake" : MODE == M_WALK ? "walk" : "track");
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
    for (int i = 0; i < S.np; i++) fprintf(OUT, "PLAYER p%d name=%s driver=%s pos=%d\n", i + 1, S.name[i], S.human[i] ? "human" : "pet", S.pos[i]); return 0;
}
static int cmd_check(void) {   /* replay already ran (load); the invariants */
    if (KIND == K_REALTIME && MODE == M_SNAKE) {
        if (S.phase >= PH_PLAY) { if (S.slen < 1 || S.slen + S.sgrow > COLS * ROWS) { fprintf(ERRF, "CHECK FAIL the snake has %d segments on a %dx%d board\n", S.slen, COLS, ROWS); return 1; }
            for (int i = 0; i < S.slen; i++) { if (S.bx[i] < 0 || S.bx[i] >= COLS || S.by[i] < 0 || S.by[i] >= ROWS) { fprintf(ERRF, "CHECK FAIL segment %d is off the board\n", i); return 1; } for (int j = i + 1; j < S.slen; j++) if (S.bx[i] == S.bx[j] && S.by[i] == S.by[j]) { if (S.phase == PH_OVER) continue; fprintf(ERRF, "CHECK FAIL segments %d and %d share a cell\n", i, j); return 1; } }
            if (S.phase == PH_PLAY && on_body(S.sfood_x, S.sfood_y, S.slen)) { fprintf(ERRF, "CHECK FAIL the food is inside the snake\n"); return 1; } }
        fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0; }
    if (KIND == K_REALTIME && MODE == M_WALK) { for (int i = 0; i < S.np; i++) if (S.pos[i] < 0 || S.pos[i] >= LV.w || is_solid(LV.rows[LV.sy][S.pos[i]])) { fprintf(ERRF, "CHECK FAIL p%d stands on column %d which is outside the level or solid\n", i + 1, S.pos[i]); return 1; }
        if (S.phase == PH_OVER && (S.winner < 0 || S.pos[S.winner] != LV.gx)) { fprintf(ERRF, "CHECK FAIL the winner is not on the goal column\n"); return 1; } fprintf(OUT, "CHECK ok events=%d\n", S.nrows); return 0; }
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
    TALK_PRINT = !strcmp(verb, "talk") && KIND == K_LEDGER; ROWHOOK = (!strcmp(verb, "export") && ac == 1) ? export_hook : NULL;   /* these two verbs observe the replay itself */
    if (load()) { fprintf(ERRF, "%s\n", MSG); return 1; }

    if (!strcmp(verb, "new")) { if (ac > 1) { fprintf(ERRF, "usage: new [seed]\n"); return 2; } if (ac == 1) { char *e; long v = strtol(av[0], &e, 10); if (*e || v < 0 || v > 999999999) { fprintf(ERRF, "new: the seed is a number 0 to 999999999\n"); return 2; } }
        if (live(-1, "", "new", ac ? av[0] : "-", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "new game seed=%ld kind=%s rows=%d\n", S.seed, KIND == K_LEDGER ? "ledger" : KIND == K_REALTIME ? "realtime" : "editor", S.nrows); return 0; }
    if (!strcmp(verb, "join")) { if (need_args(ac, 1, "join <name> [human|pet]")) return 2; if (live(-1, "", "join", av[0], ac > 1 ? av[1] : "pet", NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "p%d joined as %s\n", S.np, av[0]); return 0; }
    if (!strcmp(verb, "start")) { if (live(-1, "", "start", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "game started: %d players, seed %ld\n", S.np, S.seed); return 0; }
    if (!strcmp(verb, "seat")) { if (need_args(ac, 2, "seat <pN> human|pet")) return 2; if (live(-1, "", "seat", av[0], av[1], NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s is now %s\n", av[0], av[1]); return 0; }
    if (!strcmp(verb, "roll") && KIND == K_LEDGER) { Row r; if (live(-1, as, "roll", "", NULL, &r)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s rolled %s: %s\n", r.actor, r.target, r.res); return 0; }
    if (!strcmp(verb, "talk") && KIND == K_LEDGER) { if (ac) { fprintf(ERRF, "usage: talk\n"); return 2; } fprintf(OUT, "TALK sentences=%ld\n", S.ntk); return 0; }
    if (!strcmp(verb, "keys") && KIND == K_REALTIME && MODE == M_SNAKE) {   /* keys <relay_file> <byte_offset>: what the driver does with state/interact_relay.txt, as a verb the harness can run: steer p1 by the arrow codes written after the cursor */
        if (need_args(ac, 2, "keys <relay_file> <byte_offset>")) return 2; char *e; long off = strtol(av[1], &e, 10); if (*e || off < 0) { fprintf(ERRF, "keys: the offset is a number of bytes\n"); return 2; }
        if (S.phase != PH_PLAY) { fprintf(ERRF, "keys: the game is %s\n", S.phase == PH_OVER ? "over" : "not started"); return 2; }
        int codes[64], refused, ignored; int n = keys_poll(av[0], &off, codes, 64); int applied = keys_apply(codes, n, &refused, &ignored); fprintf(OUT, "KEYS read=%d applied=%d refused=%d ignored=%d offset=%ld tick=%ld\n", n, applied, refused, ignored, off, S.tick); return 0; }
    if (!strcmp(verb, "turn") && KIND == K_REALTIME && MODE == M_SNAKE) { if (need_args(ac, 2, "turn <pN> L|R|U|D [tick]")) return 2; long t = -1; if (ac > 2) { char *e; t = strtol(av[2], &e, 10); if (*e || t < 0) { fprintf(ERRF, "turn: tick is a number\n"); return 2; } }
        if (live(t, av[0], "turn", av[1], NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "%s turns %s at tick %ld\n", av[0], av[1], t >= 0 ? t : S.tick); return 0; }
    if (!strcmp(verb, "verify_clear") && KIND == K_REALTIME && MODE == M_WALK) {   /* read-only: the run ledger replayed (load did it) and ends at the goal; prints the proof Make records with its `clear` verb */
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
    if (KIND == K_EDITOR && !strcmp(verb, "publish")) { if (live(-1, as, "publish", "", NULL, NULL)) { fprintf(ERRF, "%s\n", MSG); return 2; } fprintf(OUT, "published doc=%s rows=%d\n", S.pubhash, S.ndoc); return 0; }
    if (!strcmp(verb, "export") && ac == 1) return cmd_export(av[0]);
    if (!strcmp(verb, "export")) { if (KIND != K_EDITOR) { fprintf(ERRF, "export: only an editor game has a document (give an out folder to write the data tables: export <out_dir>)\n"); return 2; } if (write_doc()) { fprintf(ERRF, "export: cannot write doc.txt\n"); return 2; } fprintf(OUT, "wrote doc.txt (%d rows)\n", S.ndoc); return 0; }
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
static int sync_state(void);
static int reload(void) { if (load()) { snprintf(dmsg, sizeof dmsg, "ledger refused: %.180s", MSG); return 1; } return 0; }
static int sync_state(void) { struct stat ls; if (S.phase != PH_NONE && !stat(LEDGER, &ls) && ls.st_size == KNOWN_SIZE) return 0; return reload(); }   /* reload only when the file is not what this process last wrote: a button press must not rewind the ticks since the last ckpt */
static void begin_game(void) {
    char out[300], sd[16]; archive_game(); mkdir_p(SD); snprintf(sd, sizeof sd, "%ld", seed_pref); char *n1[] = { (char *)"new", sd };
    if (verb_here(2, n1, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "new refused: %.180s", out); return; }
    for (int i = 1; i <= nplayers; i++) { char nm[4] = { (char)('A' + i - 1), 0 }; char *j[] = { (char *)"join", nm, (char *)(i == human_seat ? "human" : "pet") }; if (verb_here(3, j, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "join refused: %.180s", out); return; } }
    char *st[] = { (char *)"start" }; if (verb_here(1, st, out, sizeof out)) { snprintf(dmsg, sizeof dmsg, "start refused: %.180s", out); return; }
    mode_play = 1; reload(); write_ui(); snprintf(dmsg, sizeof dmsg, "game started: %d players, seed %ld%s", nplayers, seed_pref, human_seat ? ", you are a seat at the table" : ", all pets");
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
    else if (!strcmp(c, "new_game")) { if (mode_play && KIND != K_EDITOR) { archive_game(); mode_play = 0; memset(&S, 0, sizeof S); S.winner = -1; snprintf(dmsg, sizeof dmsg, "the last game is in archive/; choose the players, then START"); } }
    else if (KIND == K_LEDGER && !strcmp(c, "roll")) { if (!sync_state()) { char who[8] = ""; if (S.phase == PH_PLAY && S.human[S.cur]) snprintf(who, sizeof who, "p%d", S.cur + 1); human_live("roll", "", NULL, who); } }
    else if (KIND == K_REALTIME && MODE == M_SNAKE && (!strcmp(c, "left") || !strcmp(c, "right") || !strcmp(c, "up") || !strcmp(c, "down"))) { if (!S.human[0]) snprintf(dmsg, sizeof dmsg, "no human seat"); else if (!sync_state()) { char dc[2] = { (char)(c[0] - 'a' + 'A'), 0 }; human_live("turn", dc, NULL, "p1"); } }
    else if (KIND == K_REALTIME && (!strcmp(c, "left") || !strcmp(c, "right") || !strcmp(c, "stay"))) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) { hp = i + 1; break; } if (!hp) snprintf(dmsg, sizeof dmsg, "no human seat"); else { char a[8]; snprintf(a, sizeof a, "p%d", hp); if (!sync_state()) human_live("input", c, NULL, a); } }
    else if (KIND == K_EDITOR && !strncmp(c, "add_", 4)) { char id[32]; snprintf(id, sizeof id, "%s", c + 4); if (!sync_state()) { int ix = 0; for (int i = 0; i < S.ndoc; i++) if (!strcmp(S.did[i], id)) ix = 1; if (!strcmp(id, "item")) { snprintf(id, sizeof id, "item%d", S.ndoc + 1); ix = 0; } char tx[80]; snprintf(tx, sizeof tx, "%s (added from the bar)", id); human_live(ix ? "set" : "add", id, tx, "author"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "remove_last")) { if (!sync_state()) { if (S.ndoc) { char id[32]; snprintf(id, sizeof id, "%s", S.did[S.ndoc - 1]); human_live("del", id, NULL, "author"); } else snprintf(dmsg, sizeof dmsg, "the document is empty"); } }
    else if (KIND == K_EDITOR && !strcmp(c, "publish")) { if (!sync_state()) human_live("publish", "", NULL, "author"); }
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
        for (int i = 0; i < S.np; i++) { if (MODE == M_SNAKE) snprintf(b, sizeof b, "p%d %s (%s): length %d, score %d", i + 1, S.name[i], S.human[i] ? "you" : "pet", S.slen + S.sgrow, S.sscore); else snprintf(b, sizeof b, "p%d %s (%s): %d/%d", i + 1, S.name[i], S.human[i] ? "you" : "pet", S.pos[i], MODE == M_WALK ? LV.gx : LENGTH); add_txt(o, C, &nt, b); }
        if (KIND == K_LEDGER && S.l_valid) add_txt(o, C, &nt, S.tk[(S.ntk - 1) % 5]);
        if (over && S.winner >= 0) { snprintf(b, sizeof b, "winner: %s", S.name[S.winner]); add_txt(o, C, &nt, b); }
        if (!over && KIND == K_LEDGER && S.human[S.cur]) add_btn(o, C, &nb, "Roll", "roll");
        if (!over && KIND == K_REALTIME && MODE == M_SNAKE && S.human[0]) { add_btn(o, C, &nb, "Left", "left"); add_btn(o, C, &nb, "Up", "up"); add_btn(o, C, &nb, "Down", "down"); add_btn(o, C, &nb, "Right", "right"); }
        else if (!over && KIND == K_REALTIME) { int hp = 0; for (int i = 0; i < S.np; i++) if (S.human[i]) hp = 1; if (hp) { add_btn(o, C, &nb, "Left", "left"); add_btn(o, C, &nb, "Stay", "stay"); add_btn(o, C, &nb, "Right", "right"); } }
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
    OUT = stdout; ERRF = stderr; if (load_config()) { fprintf(stderr, "game_core driver: %s\n", MSG); return 1; } seed_pref = R_seed; nplayers = (int)R_maxp;   /* the setup screen starts with the table full: --players N at generation time */
    { char d[P]; snprintf(d, sizeof d, "%s/state/games", EDIT); mkdir(d, 0755); mkdir(GDIR, 0755); }
    { const char *ow = getenv("GAME_OWNER_PID"); OWNER = ow ? atol(ow) : 0; }
    char lk[P]; snprintf(lk, sizeof lk, "%s/driver.lock", GDIR); int lf = open(lk, O_RDWR | O_CREAT, 0644);
    if (lf < 0) { fprintf(stderr, "game_core driver: cannot open the lock file %s (does the editor's state folder exist?)\n", lk); return 1; }
    if (flock(lf, LOCK_EX | LOCK_NB)) { fprintf(stderr, "game_core driver: already running\n"); return 0; }
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    FILE *devnull = fopen("/dev/null", "w"); OUT = devnull ? devnull : stdout; ERRF = OUT;
    if (!reload() && S.phase >= PH_PLAY) { mode_play = 1; snprintf(dmsg, sizeof dmsg, "a game was in progress; it continues while the Player menu says playing"); }
    if (KIND == K_EDITOR) { mode_play = 1; if (S.phase == PH_NONE) { char o2[200]; char *n1[] = { (char *)"new" }; if (verb_here(1, n1, o2, sizeof o2)) snprintf(dmsg, sizeof dmsg, "new refused: %.180s", o2); else { reload(); write_doc(); snprintf(dmsg, sizeof dmsg, "an empty document: add start and goal, then publish"); } } }
    struct stat s; long cursor = stat(ACT, &s) ? 0 : (long)s.st_size; long next = now_ms(); char relay[P]; snprintf(relay, sizeof relay, "%s/state/interact_relay.txt", EDIT); long kcur = stat(relay, &s) ? 0 : (long)s.st_size;   /* the driver tails the editor's key file with its OWN cursor, starting at the end: keys pressed before it started are never replayed */
    publish();
    while (!stop_) {
        if (OWNER > 0 ? (kill((pid_t)OWNER, 0) != 0 && errno == ESRCH) : getppid() == 1) break;
        if (!stat(ACT, &s) && s.st_size > cursor) { FILE *f = fopen(ACT, "r"); if (f) { fseek(f, cursor, SEEK_SET); char ln[128]; while (fgets(ln, sizeof ln, f)) { char *e = strpbrk(ln, "\r\n"); if (e) *e = 0; if (ln[0]) handle(ln); } cursor = ftell(f); fclose(f); } if (cursor > 65536 && truncate(ACT, 0) == 0) cursor = 0; }
        else if (!stat(ACT, &s) && s.st_size < cursor) cursor = 0;
        static int was_run = -1; int run = running(); int run0 = run; if (run != was_run) { was_run = run; dirty = 1; }
        if (KIND == K_REALTIME && MODE == M_SNAKE) { int kc[32]; int kn = keys_poll(relay, &kcur, kc, 32);   /* arrows 1000-1003 steer a human snake while the game runs; while stopped the cursor still advances, so keys pressed during a pause are dropped, not queued */
            if (kn > 0 && mode_play && run0 && S.phase == PH_PLAY && S.human[0] && !sync_state()) { int rf, ig; int ap = keys_apply(kc, kn, &rf, &ig); if (ap) { snprintf(dmsg, sizeof dmsg, "arrow keys: %d turn(s)", ap); dirty = 1; } } }
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
