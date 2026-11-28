/* civ_op: the rules of civ-snes (BOOK 16.game/CIV-SNES-2D-AND-3D-PLAN-2026-11-27.md, decision D157).
 * ONE op, one VERB per rule (D105 shape): new move found build endturn seat, plus the read-only
 * queries status units cities map fog legal ui nextunit verify project audit.
 * The ledger is the ONLY authority. The world is the FOLD of the NEW row's seed over the RULE rows of
 * rules.pdl (their hash is in the NEW row: edit a number after a game began and verify refuses it).
 * Every mutating verb replays the ledger first (verify), applies ONE action through the same function the
 * replay uses, and appends the rows it made in a single write; a refused action writes nothing.
 * Integer only: no float, no clock, no rand(); the die is a seeded hash.
 *
 * Ledger rows (no spaces around the bars):
 *   NEW|seed|w|h|rules_hash16          then the derived UNIT rows and the SEAT rows
 *   UNIT|id|civ|kind|x|y               a start unit (derived by NEW, recomputed on replay)
 *   SEAT|civ|human or agent:basic      who plays a civ (a driver change is a row; the last row wins)
 *   MOVE|unit|x0|y0|x1|y1|cost         one step (dir: n ne e se s sw w nw)
 *   FOUND|unit|city|x|y                the settler becomes a city
 *   BUILD|city|kind                    what the city makes
 *   TURN|n                             the only thing that advances time; then, derived and recomputed on replay:
 *   PROD|city|shields|stock   BORN|unit|civ|kind|x|y   AGENT|civ|verb|arg  (+ the MOVE/FOUND/BUILD the agent made)
 * usage: civ_op <ledger> <verb> [args]      env CIV_RULES / CIV_TEXT override the data paths (a harness copy)
 * exit: 0 ok, 2 usage, 4 refused (nothing written, "refused: <why>" on stdout), 5 io / missing rule, 14 ledger refused.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/civ_op.+x civ_op.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdint.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXT 4096
#define MAXU 256
#define MAXC 64
#define MAXR 96
#define MAXROWS 4096
#define LINE 256

enum { WATER, GRASS, FOREST, HILLS, MOUNT };
static const char *tname[] = { "water", "grass", "forest", "hills", "mountain" };
static const char tglyph[] = { '~', '.', 'T', '^', 'M' };
static const int DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const char *DN[8] = { "n", "ne", "e", "se", "s", "sw", "w", "nw" };

typedef struct { char name[40]; int v; } Rule;
typedef struct { char id[20]; int civ, kind, x, y, mp, alive; } Unit;     /* kind 0 settler 1 warrior */
typedef struct { char id[20]; int civ, x, y, size, build, stock; } City;
typedef struct { int turn, w, h, nu, nc, nunits_made, ncities_made, nseat[3]; int seat_agent[3];
                 unsigned seed; int ter[MAXT]; Unit u[MAXU]; City c[MAXC]; unsigned char seen[3][MAXT]; } World;

static char rules_path[PATH_MAX + 32];
static Rule rules[MAXR]; static int nrules; static uint64_t rules_hash;
static const char *kindname[] = { "settler", "warrior" };
static char msg[160];                      /* the last refusal reason */

/* ---------- data: rules.pdl ---------- */
static int rule(const char *n) {
    for (int i = 0; i < nrules; i++) if (!strcmp(rules[i].name, n)) return rules[i].v;
    fprintf(stderr, "civ_op: missing RULE %s\n", n); exit(5);
}
static int load_rules(const char *self) {
    char p[PATH_MAX + 32], ln[LINE], nm[40]; int v; const char *e = getenv("CIV_RULES");
    if (e && *e) snprintf(p, sizeof p, "%s", e);
    else { char r[PATH_MAX]; if (!realpath(self, r)) return 1;
        for (int i = 0; i < 3; i++) { char *s = strrchr(r, '/'); if (!s) return 1; *s = 0; }
        snprintf(p, sizeof p, "%s/rules.pdl", r); }
    snprintf(rules_path, sizeof rules_path, "%s", p); FILE *f = fopen(p, "r"); if (!f) return 1;
    uint64_t h = 1469598103934665603ULL;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "RULE | %39s | %d", nm, &v) == 2 && nrules < MAXR) {
        snprintf(rules[nrules].name, sizeof rules[0].name, "%s", nm); rules[nrules++].v = v;
        char t[80]; int n = snprintf(t, sizeof t, "%s=%d;", nm, v);
        for (int i = 0; i < n; i++) { h ^= (unsigned char)t[i]; h *= 1099511628211ULL; } }
    fclose(f); rules_hash = h; return nrules ? 0 : 1; }

/* ---------- seeded hash and the world ---------- */
static uint32_t mix(uint32_t s, int x, int y, int st) {
    uint32_t h = s * 0x9E3779B9u; h ^= (uint32_t)x * 0x85EBCA6Bu; h ^= (uint32_t)y * 0xC2B2AE35u; h ^= (uint32_t)st * 0x27D4EB2Fu;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; return h; }
static int gen_terrain(World *W) {
    int w = W->w, h = W->h, S = rule("noise_cell"), fine = rule("noise_fine"), em = rule("edge_margin"), ep = rule("edge_pen");
    int sea = rule("sea"), hill = rule("hill"), mount = rule("mount"), fpct = rule("forest_pct"), land = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int i = x / S, j = y / S, fx = x % S, fy = y % S;
        int a = (int)(mix(W->seed, i, j, 7) % 100), b = (int)(mix(W->seed, i + 1, j, 7) % 100);
        int c = (int)(mix(W->seed, i, j + 1, 7) % 100), d = (int)(mix(W->seed, i + 1, j + 1, 7) % 100);
        int v = ((a * (S - fx) + b * fx) * (S - fy) + (c * (S - fx) + d * fx) * fy) / (S * S);
        v += (int)(mix(W->seed, x, y, 1) % (uint32_t)fine) - fine / 2;
        int dm = x; if (y < dm) dm = y; if (w - 1 - x < dm) dm = w - 1 - x; if (h - 1 - y < dm) dm = h - 1 - y;
        if (dm < em) v -= (em - dm) * ep;
        int k = GRASS;
        if (v < sea) k = WATER; else if (v >= mount) k = MOUNT; else if (v >= hill) k = HILLS;
        else if ((int)(mix(W->seed, x, y, 2) % 100) >= fpct) k = FOREST;
        W->ter[y * w + x] = k; if (k != WATER) land++; }
    return land; }
static int in_map(const World *W, int x, int y) { return x >= 0 && y >= 0 && x < W->w && y < W->h; }
static int tcost(int k) { return k == GRASS ? rule("cost_grass") : k == FOREST ? rule("cost_forest") : k == HILLS ? rule("cost_hills") : rule("cost_mountain"); }

/* ---------- the fold helpers ---------- */
static Unit *unit_at(World *W, int x, int y) { for (int i = 0; i < W->nu; i++) if (W->u[i].alive && W->u[i].x == x && W->u[i].y == y) return &W->u[i]; return NULL; }
static City *city_at(World *W, int x, int y) { for (int i = 0; i < W->nc; i++) if (W->c[i].x == x && W->c[i].y == y) return &W->c[i]; return NULL; }
static Unit *unit_id(World *W, const char *id) { for (int i = 0; i < W->nu; i++) if (W->u[i].alive && !strcmp(W->u[i].id, id)) return &W->u[i]; return NULL; }
static City *city_id(World *W, const char *id) { for (int i = 0; i < W->nc; i++) if (!strcmp(W->c[i].id, id)) return &W->c[i]; return NULL; }
static int civ_of(const char *s) { return !strcmp(s, "civ1") ? 1 : !strcmp(s, "civ2") ? 2 : 0; }
static void reveal(World *W, int civ, int x, int y) {
    int r = rule("sight");
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (in_map(W, x + dx, y + dy)) W->seen[civ][(y + dy) * W->w + x + dx] = 1; }
static Unit *add_unit(World *W, int civ, int kind, int x, int y) {
    if (W->nu >= MAXU) return NULL;
    Unit *u = &W->u[W->nu++]; snprintf(u->id, sizeof u->id, "u%d", ++W->nunits_made);
    u->civ = civ; u->kind = kind; u->x = x; u->y = y; u->alive = 1; u->mp = kind ? rule("mp_warrior") : rule("mp_settler");
    reveal(W, civ, x, y); return u; }
static void refresh_mp(World *W) { for (int i = 0; i < W->nu; i++) if (W->u[i].alive) W->u[i].mp = W->u[i].kind ? rule("mp_warrior") : rule("mp_settler"); }
static int refuse(const char *m) { snprintf(msg, sizeof msg, "%s", m); return 4; }

/* the rows a verb made are collected here and written (or compared on replay) as one block */
typedef struct { char r[MAXROWS][LINE]; int n; } Rows;
static void rowf(Rows *R, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void rowf(Rows *R, const char *fmt, ...) { if (R->n >= MAXROWS) return; va_list ap; va_start(ap, fmt); vsnprintf(R->r[R->n++], LINE, fmt, ap); va_end(ap); }

/* ---------- RULE: new (start units) ---------- */
static int land_free(World *W, int x, int y) { return in_map(W, x, y) && W->ter[y * W->w + x] != WATER && !unit_at(W, x, y) && !city_at(W, x, y); }
static int do_new(World *W, unsigned seed, Rows *R) {
    memset(W, 0, sizeof *W); W->seed = seed; W->w = rule("map_w"); W->h = rule("map_h");
    if (W->w * W->h > MAXT || W->w < 8 || W->h < 6) return refuse("map size out of range");
    int land = gen_terrain(W);
    if (land < rule("min_land")) return refuse("not enough land for this seed");
    rowf(R, "NEW|%u|%d|%d|%016llx", seed, W->w, W->h, (unsigned long long)rules_hash);
    for (int civ = 1; civ <= 2; civ++) {
        int tx = civ == 1 ? W->w / 4 : (3 * W->w) / 4, ty = W->h / 2, best = 1 << 30, bx = -1, by = -1;
        for (int y = 0; y < W->h; y++) for (int x = 0; x < W->w; x++) if (W->ter[y * W->w + x] == GRASS && land_free(W, x, y)) {
            int d = abs(x - tx) + abs(y - ty); if (d < best) { best = d; bx = x; by = y; } }
        if (bx < 0) return refuse("no grass start for a civ");
        Unit *s = add_unit(W, civ, 0, bx, by); rowf(R, "UNIT|%s|civ%d|settler|%d|%d", s->id, civ, bx, by);
        int placed = 0;
        for (int d = 0; d < 8 && !placed; d++) { int k = (2 + d) & 7;     /* east first, then clockwise */
            if (land_free(W, bx + DX[k], by + DY[k])) {
                Unit *wr = add_unit(W, civ, 1, bx + DX[k], by + DY[k]); rowf(R, "UNIT|%s|civ%d|warrior|%d|%d", wr->id, civ, wr->x, wr->y); placed = 1; } }
        if (!placed) return refuse("no room for the warrior"); }
    W->seat_agent[1] = 0; W->seat_agent[2] = 1;
    rowf(R, "SEAT|civ1|human"); rowf(R, "SEAT|civ2|agent:basic");
    return 0; }

/* ---------- RULE: move ---------- */
static int dir_of(const char *s) { for (int i = 0; i < 8; i++) if (!strcmp(s, DN[i])) return i; return -1; }
static int do_move(World *W, Unit *u, int d, Rows *R) {
    int nx = u->x + DX[d], ny = u->y + DY[d];
    if (u->mp <= 0) return refuse("no moves left this turn");
    if (!in_map(W, nx, ny)) return refuse("off the map");
    int k = W->ter[ny * W->w + nx];
    if (k == WATER) return refuse("water: land units cannot enter");
    if (unit_at(W, nx, ny)) return refuse("a unit is already there");
    City *c = city_at(W, nx, ny); if (c && c->civ != u->civ) return refuse("an enemy city");
    int cost = tcost(k), ox = u->x, oy = u->y;
    u->mp -= cost; if (u->mp < 0) u->mp = 0;       /* the last step may overspend, as in the reference game */
    u->x = nx; u->y = ny; reveal(W, u->civ, nx, ny);
    rowf(R, "MOVE|%s|%d|%d|%d|%d|%d", u->id, ox, oy, nx, ny, cost); return 0; }

/* ---------- RULE: found ---------- */
static int do_found(World *W, Unit *u, Rows *R) {
    if (u->kind != 0) return refuse("only a settler can found a city");
    if (W->ter[u->y * W->w + u->x] == WATER) return refuse("not on water");
    for (int i = 0; i < W->nc; i++) if (abs(W->c[i].x - u->x) + abs(W->c[i].y - u->y) < rule("citygap")) return refuse("too close to another city");
    if (W->nc >= MAXC) return refuse("too many cities");
    City *c = &W->c[W->nc++]; snprintf(c->id, sizeof c->id, "c%d", ++W->ncities_made);
    c->civ = u->civ; c->x = u->x; c->y = u->y; c->size = 1; c->build = rule("default_build"); c->stock = 0;
    u->alive = 0; reveal(W, c->civ, c->x, c->y);
    rowf(R, "FOUND|%s|%s|%d|%d", u->id, c->id, c->x, c->y); return 0; }

/* ---------- RULE: build ---------- */
static int kind_of(const char *s) { return !strcmp(s, "settler") ? 0 : !strcmp(s, "warrior") ? 1 : -1; }
static int do_build(World *W, City *c, int kind, Rows *R) {
    (void)W; if (kind < 0) return refuse("unknown unit kind");
    if (c->build == kind) return refuse("already building that");
    c->build = kind; rowf(R, "BUILD|%s|%s", c->id, kindname[kind]); return 0; }

/* ---------- the rival agent (basic): legal moves, weights from rules, a validator that writes the row ---------- */
typedef struct { int score, verb; Unit *u; City *c; int d, kind; } Cand;  /* verb 0 found 1 build 2 move */
static int unseen_at(World *W, int civ, int x, int y) {
    int r = rule("sight"), n = 0;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (in_map(W, x + dx, y + dy) && !W->seen[civ][(y + dy) * W->w + x + dx]) n++;
    return n; }
static int mp_ok_move(World *W, Unit *u, int d) {
    int nx = u->x + DX[d], ny = u->y + DY[d];
    if (u->mp <= 0 || !in_map(W, nx, ny) || W->ter[ny * W->w + nx] == WATER || unit_at(W, nx, ny)) return 0;
    City *c = city_at(W, nx, ny); return !c || c->civ == u->civ; }
static int found_ok(World *W, Unit *u) {
    if (u->kind != 0) return 0;
    for (int i = 0; i < W->nc; i++) if (abs(W->c[i].x - u->x) + abs(W->c[i].y - u->y) < rule("citygap")) return 0;
    return W->nc < MAXC; }
static int ncity_of(World *W, int civ) { int n = 0; for (int i = 0; i < W->nc; i++) if (W->c[i].civ == civ) n++; return n; }
/* the candidate list in a FIXED order: units by id (found, then n..nw), then cities by id (build). The first highest score wins. */
static int candidates(World *W, int civ, Cand *out, int max) {
    int n = 0;
    for (int i = 0; i < W->nu && n < max; i++) { Unit *u = &W->u[i]; if (!u->alive || u->civ != civ || u->mp <= 0) continue;
        if (found_ok(W, u)) { out[n++] = (Cand){ rule("aiw_found"), 0, u, NULL, 0, 0 }; }
        for (int d = 0; d < 8 && n < max; d++) if (mp_ok_move(W, u, d)) {
            int sc = rule("aiw_explore") * unseen_at(W, civ, u->x + DX[d], u->y + DY[d]) + rule("aiw_walk");
            if (u->kind == 0 && ncity_of(W, civ) == 0) sc += rule("aiw_walk");
            out[n++] = (Cand){ sc, 2, u, NULL, d, 0 }; } }
    for (int i = 0; i < W->nc && n < max; i++) { City *c = &W->c[i]; if (c->civ != civ) continue;
        int want = ncity_of(W, civ) < rule("ai_cities") ? 0 : 1;
        if (c->build != want) out[n++] = (Cand){ rule("aiw_build"), 1, NULL, c, 0, want }; }
    return n; }
static int agent_turn(World *W, int civ, Rows *R) {
    int acted = 0, lim = rule("agent_actions");
    for (int a = 0; a < lim; a++) {
        Cand cs[MAXU * 9 + MAXC]; int n = candidates(W, civ, cs, (int)(sizeof cs / sizeof cs[0])), best = -1;
        for (int i = 0; i < n; i++) if (cs[i].score > 0 && (best < 0 || cs[i].score > cs[best].score)) best = i;
        if (best < 0) break;
        Cand *c = &cs[best]; int rc;
        if (c->verb == 0) { rowf(R, "AGENT|civ%d|found|%s", civ, c->u->id); rc = do_found(W, c->u, R); }
        else if (c->verb == 1) { rowf(R, "AGENT|civ%d|build|%s", civ, c->c->id); rc = do_build(W, c->c, c->kind, R); }
        else { rowf(R, "AGENT|civ%d|move|%s", civ, c->u->id); rc = do_move(W, c->u, c->d, R); }
        if (rc) { fprintf(stderr, "civ_op: agent chose an illegal action (%s): a bug\n", msg); exit(5); }
        acted++; }
    return acted; }

/* ---------- RULE: endturn (production, births, mp, then the agents) ---------- */
static int rough_nb(World *W, City *c) { int n = 0; for (int d = 0; d < 8; d++) { int x = c->x + DX[d], y = c->y + DY[d]; if (in_map(W, x, y)) { int k = W->ter[y * W->w + x]; if (k == FOREST || k == HILLS || k == MOUNT) n++; } } return n; }
static int do_endturn(World *W, Rows *R) {
    W->turn++; rowf(R, "TURN|%d", W->turn);
    for (int i = 0; i < W->nc; i++) { City *c = &W->c[i];
        int sh = rule("city_shields") + rough_nb(W, c) / rule("rough_div"); c->stock += sh;
        rowf(R, "PROD|%s|%d|%d", c->id, sh, c->stock);
        int cost = c->build ? rule("cost_warrior") : rule("cost_settler");
        if (c->stock >= cost) {
            int bx = -1, by = -1;
            if (!unit_at(W, c->x, c->y)) { bx = c->x; by = c->y; }
            for (int d = 0; d < 8 && bx < 0; d++) if (land_free(W, c->x + DX[d], c->y + DY[d])) { bx = c->x + DX[d]; by = c->y + DY[d]; }
            if (bx >= 0) { Unit *u = add_unit(W, c->civ, c->build, bx, by); if (u) { c->stock -= cost; rowf(R, "BORN|%s|civ%d|%s|%d|%d", u->id, c->civ, kindname[c->build], bx, by); } } } }
    refresh_mp(W);
    for (int civ = 1; civ <= 2; civ++) if (W->seat_agent[civ]) agent_turn(W, civ, R);
    return 0; }

/* ---------- ledger: load, replay (verify), append ---------- */
static char *g_lines[MAXROWS * 2]; static int g_n;
static void free_lines(void) { for (int i = 0; i < g_n; i++) free(g_lines[i]); g_n = 0; }
static int read_ledger(const char *path) {
    FILE *f = fopen(path, "r"); char ln[LINE * 2]; free_lines(); if (!f) return 5;
    while (fgets(ln, sizeof ln, f) && g_n < MAXROWS * 2) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) g_lines[g_n++] = strdup(ln); }
    fclose(f); return 0; }
static int bad(int at, const char *why) { fprintf(stderr, "ledger line %d: %s\n", at + 1, why); return 14; }
/* compare the block just made to the next lines of the ledger (skip the first when it is the row being applied) */
static int match_block(const Rows *R, int from, int at) {
    for (int i = from; i < R->n; i++) { if (at + i - from >= g_n) return bad(g_n, "a derived row is missing"); if (strcmp(g_lines[at + i - from], R->r[i])) { fprintf(stderr, "ledger line %d: expected %s\n", at + i - from + 1, R->r[i]); return bad(at + i - from, "a derived row differs from the rules"); } }
    return 0; }
static int replay(World *W) {
    static Rows R; if (g_n < 1) return bad(0, "empty ledger"); int at = 0; unsigned seed; int w, h; unsigned long long hh;
    if (sscanf(g_lines[0], "NEW|%u|%d|%d|%llx", &seed, &w, &h, &hh) != 4) return bad(0, "first row is not NEW");
    if (hh != (unsigned long long)rules_hash) return bad(0, "rules.pdl changed after the game began (hash)");
    R.n = 0; if (do_new(W, seed, &R)) return bad(0, msg);
    if (w != W->w || h != W->h) return bad(0, "map size differs from the rules");
    int rc = match_block(&R, 0, 0); if (rc) return rc; at = R.n;
    while (at < g_n) { char *l = g_lines[at]; R.n = 0; char a[16], b[16]; int x0, y0, x1, y1, cost, n;
        if (sscanf(l, "MOVE|%15[^|]|%d|%d|%d|%d|%d", a, &x0, &y0, &x1, &y1, &cost) == 6) {
            Unit *u = unit_id(W, a); int d = -1; if (!u) return bad(at, "MOVE of a unit that does not exist");
            for (int i = 0; i < 8; i++) if (DX[i] == x1 - x0 && DY[i] == y1 - y0) d = i;
            if (d < 0 || u->x != x0 || u->y != y0) return bad(at, "MOVE does not start where the unit is");
            if (do_move(W, u, d, &R)) return bad(at, msg);
            if (strcmp(R.r[0], l)) { return bad(at, "MOVE cost differs from the rules"); }
            at++; }
        else if (sscanf(l, "FOUND|%15[^|]|%15[^|]|%d|%d", a, b, &x0, &y0) == 4) {
            Unit *u = unit_id(W, a); if (!u) return bad(at, "FOUND by a unit that does not exist");
            if (do_found(W, u, &R)) { return bad(at, msg); }
            if (strcmp(R.r[0], l)) { return bad(at, "FOUND differs from the rules"); }
            at++; }
        else if (sscanf(l, "BUILD|%15[^|]|%15s", a, b) == 2) {
            City *c = city_id(W, a); if (!c) return bad(at, "BUILD of a city that does not exist");
            if (do_build(W, c, kind_of(b), &R)) { return bad(at, msg); }
            at++; }
        else if (sscanf(l, "SEAT|%15[^|]|%15s", a, b) == 2) {
            int civ = civ_of(a); if (!civ || (strcmp(b, "human") && strcmp(b, "agent:basic"))) return bad(at, "bad SEAT row"); W->seat_agent[civ] = strcmp(b, "human") != 0; at++; }
        else if (sscanf(l, "TURN|%d", &n) == 1) {
            if (n != W->turn + 1) return bad(at, "TURN out of order");
            do_endturn(W, &R); if (strcmp(R.r[0], l)) return bad(at, "TURN differs"); rc = match_block(&R, 1, at + 1); if (rc) return rc; at += R.n; }
        else return bad(at, "unknown or forged row"); }
    return 0; }
static int append_rows(const char *path, const Rows *R) {
    FILE *f = fopen(path, "a"); if (!f) return 5; static char buf[MAXROWS / 4 * LINE]; size_t n = 0;
    for (int i = 0; i < R->n; i++) { size_t l = strlen(R->r[i]); if (n + l + 2 > sizeof buf) { fclose(f); return 5; } memcpy(buf + n, R->r[i], l); n += l; buf[n++] = '\n'; }
    int rc = fwrite(buf, 1, n, f) == n ? 0 : 5; fclose(f); return rc; }

/* ---------- queries ---------- */
static int vis(World *W, int civ, int x, int y) { return civ == 0 || W->seen[civ][y * W->w + x]; }
static void cmd_map(World *W, int civ) {
    for (int y = 0; y < W->h; y++) { for (int x = 0; x < W->w; x++) { char ch;
        if (!vis(W, civ, x, y)) ch = ' '; else { Unit *u = unit_at(W, x, y); City *c = city_at(W, x, y);
            ch = c ? (c->civ == 1 ? 'A' : 'B') : u ? (u->kind ? (u->civ == 1 ? 'w' : 'x') : (u->civ == 1 ? 's' : 'z')) : tglyph[W->ter[y * W->w + x]]; }
        putchar(ch); } putchar('\n'); } }
static void year_text(int turn, char *o, size_t n) { int y = rule("year0") + rule("year_step") * turn; if (y < 0) snprintf(o, n, "%d BC", -y); else snprintf(o, n, "%d AD", y); }
static void text_for(const char *key, char *out, size_t n) {
    char p[PATH_MAX + 32], ln[LINE], k[40], t[160]; const char *e = getenv("CIV_TEXT"); snprintf(out, n, "%s", key);
    if (e && *e) snprintf(p, sizeof p, "%s", e); else { snprintf(p, sizeof p, "%s", rules_path); char *s = strrchr(p, '/'); if (!s) return; snprintf(s, 16, "/text.pdl"); }
    FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "TEXT | %39s | %159[^\n]", k, t) == 2 && !strcmp(k, key)) { snprintf(out, n, "%s", t); break; }
    fclose(f); }
/* ui: the left bar of the editor (the rmmc shape: nick_line, stat_line, stat_line2) and the one NEXT line */
static void cmd_ui(World *W, int civ, const char *sel) {
    char yr[24], t[160]; year_text(W->turn, yr, sizeof yr); Unit *u = sel && *sel ? unit_id(W, sel) : NULL; City *sc = sel && *sel ? city_id(W, sel) : NULL;
    int nus = 0; for (int i = 0; i < W->nu; i++) if (W->u[i].alive && W->u[i].civ == civ) nus++;
    printf("turn=%d\nyear=%s\nnick_line=Turn %d  %s\n", W->turn, yr, W->turn, yr);
    if (u) printf("stat_line=%s %s  moves %d\nstat_line2=%s at %d,%d\n", u->id, kindname[u->kind], u->mp, tname[W->ter[u->y * W->w + u->x]], u->x, u->y);
    else if (sc) printf("stat_line=%s size %d  makes %s\nstat_line2=shields %d\n", sc->id, sc->size, kindname[sc->build], sc->stock);
    else printf("stat_line=units %d  cities %d\nstat_line2=press ] for a unit\n", nus, ncity_of(W, civ));
    int has_settler = 0, moves_left = 0, ncity = ncity_of(W, civ);
    for (int i = 0; i < W->nu; i++) if (W->u[i].alive && W->u[i].civ == civ) { if (W->u[i].kind == 0) has_settler = 1; if (W->u[i].mp > 0) moves_left = 1; }
    const char *key = ncity == 0 && has_settler ? (u && u->kind == 0 ? (found_ok(W, u) ? "next_found" : "next_walk") : "next_pick") : ncity > 0 && W->turn == 0 ? "next_endturn" : moves_left ? "next_move" : "next_end";
    text_for(key, t, sizeof t); printf("next=%s\n", t); }
static void cmd_status(World *W) {
    { int nu1 = 0, nu2 = 0; for (int i = 0; i < W->nu; i++) if (W->u[i].alive) { if (W->u[i].civ == 1) nu1++; else nu2++; } printf("turn=%d cities=%d/%d units=%d/%d seats=%s/%s\n", W->turn, ncity_of(W, 1), ncity_of(W, 2), nu1, nu2, W->seat_agent[1] ? "agent:basic" : "human", W->seat_agent[2] ? "agent:basic" : "human"); } }

/* ---------- the projection: ONE desk read by the 2D and the 3D board ---------- */
/* atlas tiles: 0 fog, 1 water, 2 grass, 3 forest, 4 hills, 5 mountain, 6 city civ1, 7 city civ2 (flat colours, a stopgap: placeholder art) */
static const unsigned char TILE[8][3] = { {0,0,0}, {40,90,200}, {90,170,70}, {30,110,50}, {150,130,80}, {170,170,175}, {80,140,255}, {255,90,80} };
static int write_file(const char *path, const char *data, size_t n) {
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return 5;
    int ok = fwrite(data, 1, n, f) == n; fclose(f); if (!ok || rename(tmp, path)) return 5; return 0; }
static int cmd_project(World *W, int civ, const char *dir) {
    static char cells[MAXT * 8 + 256], mapt[MAXT + 128], ev[MAXT * 40], atl[16 * 16 * 8 * 4]; size_t nc = 0, nm = 0, ne = 0; char p[PATH_MAX + 32];
    mkdir(dir, 0755);
    nc += (size_t)snprintf(cells + nc, sizeof cells - nc, "width=%d\nheight=%d\ntile_px=16\natlas_cols=1\natlas_tiles=8\ncells\n", W->w, W->h);
    for (int y = 0; y < W->h; y++) { for (int x = 0; x < W->w; x++) { int t, g; City *c = city_at(W, x, y);
            if (!vis(W, civ, x, y)) { t = 0; g = '#'; } else if (c) { t = c->civ == 1 ? 6 : 7; g = c->civ == 1 ? 'A' : 'B'; } else { int k = W->ter[y * W->w + x]; t = k + 1; g = tglyph[k]; }
            nc += (size_t)snprintf(cells + nc, sizeof cells - nc, "%s%d,0", x ? " " : "", t); mapt[nm++] = (char)g; }
        cells[nc++] = '\n'; mapt[nm++] = '\n'; }
    for (int i = 0; i < W->nu; i++) { Unit *u = &W->u[i]; if (!u->alive || !vis(W, civ, u->x, u->y)) continue;
        int r = u->civ == 1 ? (u->kind ? 40 : 120) : (u->kind ? 200 : 255), g = u->civ == 1 ? (u->kind ? 80 : 200) : (u->kind ? 30 : 160), b = u->civ == 1 ? (u->kind ? 230 : 255) : (u->kind ? 40 : 60);
        ne += (size_t)snprintf(ev + ne, sizeof ev - ne, "%d %d %d %d %d block 0 2 1\n", u->x, u->y, r, g, b); }
    for (int t = 0; t < 8; t++) for (int i = 0; i < 256; i++) { size_t o = ((size_t)t * 256 + (size_t)i) * 4; int edge = (i % 16 == 0 || i / 16 == 0) && t > 0, sh = edge ? 12 : 0;
        atl[o] = (char)(TILE[t][0] > sh ? TILE[t][0] - sh : 0); atl[o + 1] = (char)(TILE[t][1] > sh ? TILE[t][1] - sh : 0); atl[o + 2] = (char)(TILE[t][2] > sh ? TILE[t][2] - sh : 0); atl[o + 3] = (char)255; }
    static const char *ext = "EXTRUDE | ~ | 1\nEXTRUDE | . | 1\nEXTRUDE | T | 2\nEXTRUDE | ^ | 3\nEXTRUDE | M | 5\nEXTRUDE | A | 2\nEXTRUDE | B | 2\nEXTRUDE | # | 1\n";
    int rc = 0;
    snprintf(p, sizeof p, "%s/cells.txt", dir); rc |= write_file(p, cells, nc);
    snprintf(p, sizeof p, "%s/map.txt", dir); rc |= write_file(p, mapt, nm);
    snprintf(p, sizeof p, "%s/events.txt", dir); rc |= write_file(p, ev, ne);
    snprintf(p, sizeof p, "%s/extrusion.pdl", dir); rc |= write_file(p, ext, strlen(ext));
    snprintf(p, sizeof p, "%s/cells.rgba", dir); rc |= write_file(p, atl, sizeof atl);
    /* the engine reloads when cells.marker GROWS (house marker rule): append one byte, restart at 4096 */
    snprintf(p, sizeof p, "%s/cells.marker", dir); struct stat st; int restart = stat(p, &st) == 0 && st.st_size >= 4096;
    FILE *m = fopen(p, restart ? "w" : "a"); if (m) { fputc('1', m); fclose(m); } else rc |= 5;
    return rc ? 5 : 0; }

/* ---------- audit: every file in the state folder must be declared in state.pdl (FILE | name | ...) ---------- */
static int cmd_audit(const char *dir, const char *statepdl) {
    char names[64][64], ln[LINE], k[64]; int nn = 0, bad_n = 0; FILE *f = fopen(statepdl, "r"); if (!f) return 5;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "FILE | %63s", k) == 1 && nn < 64) snprintf(names[nn++], 64, "%s", k);
    fclose(f);
    DIR *d = opendir(dir); struct dirent *e; if (!d) return 5;
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; int ok = 0; for (int i = 0; i < nn; i++) if (!strcmp(names[i], e->d_name)) ok = 1;
        if (!ok) { printf("undeclared: %s\n", e->d_name); bad_n++; } }
    closedir(d); printf("audit: %d undeclared\n", bad_n); return bad_n ? 4 : 0; }

/* ---------- main ---------- */
static int load_world(const char *ledger, World *W) { int rc = read_ledger(ledger); if (rc) return rc; return replay(W); }
int main(int argc, char **argv) {
    static World W; static Rows R; R.n = 0;
    if (argc < 3) { fprintf(stderr, "usage: civ_op <ledger> <verb> [args] (new move found build endturn seat | status units cities map fog legal ui nextunit verify project audit)\n"); return 2; }
    const char *ledger = argv[1], *verb = argv[2];
    if (load_rules(argv[0])) { fprintf(stderr, "civ_op: no rules.pdl\n"); return 5; }
    if (!strcmp(verb, "audit")) { if (argc < 5) return 2; return cmd_audit(argv[3], argv[4]); }
    if (!strcmp(verb, "world")) { if (argc < 4) { return 2; }
        memset(&W, 0, sizeof W); W.seed = (unsigned)strtoul(argv[3], NULL, 10); W.w = rule("map_w"); W.h = rule("map_h"); int land = gen_terrain(&W);
        for (int y = 0; y < W.h; y++) { for (int x = 0; x < W.w; x++) { putchar(tglyph[W.ter[y * W.w + x]]); } putchar(10); }
        printf("land=%d of %d\n", land, W.w * W.h); return 0; }
    if (!strcmp(verb, "new")) {
        if (argc < 4) { return 2; }
        struct stat st; if (stat(ledger, &st) == 0 && st.st_size > 0) { printf("refused: a ledger already exists\n"); return 4; }
        int rc = do_new(&W, (unsigned)strtoul(argv[3], NULL, 10), &R); if (rc) { printf("refused: %s\n", msg); return rc; }
        if (append_rows(ledger, &R)) { return 5; }
        printf("ok: new game seed %s, %d rows\n", argv[3], R.n); return 0; }
    int rc = load_world(ledger, &W); if (rc) { printf("ledger refused\n"); free_lines(); return rc; }
    if (!strcmp(verb, "verify")) { printf("ok: %d rows replay\n", g_n); return 0; }
    if (!strcmp(verb, "status")) { cmd_status(&W); return 0; }
    if (!strcmp(verb, "units")) { for (int i = 0; i < W.nu; i++) if (W.u[i].alive) printf("unit %s civ%d %s %d %d mp=%d\n", W.u[i].id, W.u[i].civ, kindname[W.u[i].kind], W.u[i].x, W.u[i].y, W.u[i].mp); return 0; }
    if (!strcmp(verb, "cities")) { for (int i = 0; i < W.nc; i++) printf("city %s civ%d %d %d size=%d build=%s stock=%d\n", W.c[i].id, W.c[i].civ, W.c[i].x, W.c[i].y, W.c[i].size, kindname[W.c[i].build], W.c[i].stock); return 0; }
    if (!strcmp(verb, "map")) { cmd_map(&W, argc > 3 ? civ_of(argv[3]) : 0); return 0; }
    if (!strcmp(verb, "fog")) { int civ = argc > 3 ? civ_of(argv[3]) : 1, n = 0; for (int i = 0; i < W.w * W.h; i++) n += W.seen[civ][i]; printf("seen=%d of %d\n", n, W.w * W.h); return 0; }
    if (!strcmp(verb, "legal")) { int civ = argc > 3 ? civ_of(argv[3]) : 2; static Cand cs[MAXU * 9 + MAXC]; int n = candidates(&W, civ, cs, MAXU * 9 + MAXC);
        for (int i = 0; i < n; i++) { printf("%s %s score=%d\n", cs[i].verb == 0 ? "found" : cs[i].verb == 1 ? "build" : "move", cs[i].u ? cs[i].u->id : cs[i].c->id, cs[i].score); }
        return 0; }
    if (!strcmp(verb, "ui")) { cmd_ui(&W, argc > 3 ? civ_of(argv[3]) : 1, argc > 4 ? argv[4] : ""); return 0; }
    if (!strcmp(verb, "nextunit")) { int civ = argc > 3 ? civ_of(argv[3]) : 1, after = -1; if (argc > 4) for (int i = 0; i < W.nu; i++) if (!strcmp(W.u[i].id, argv[4])) after = i;
        for (int k = 1; k <= W.nu; k++) { Unit *u = &W.u[(after + k + W.nu) % W.nu]; if (u->alive && u->civ == civ && u->mp > 0) { printf("%s\n", u->id); return 0; } } printf("none\n"); return 4; }
    if (!strcmp(verb, "project")) { if (argc < 5) return 2; return cmd_project(&W, civ_of(argv[3]), argv[4]); }
    if (!strcmp(verb, "move")) { if (argc < 5) return 2; Unit *u = unit_id(&W, argv[3]); int d = dir_of(argv[4]);
        if (!u) rc = refuse("no such unit"); else if (d < 0) rc = refuse("direction is n ne e se s sw w nw"); else rc = do_move(&W, u, d, &R); }
    else if (!strcmp(verb, "found")) { if (argc < 4) return 2; Unit *u = unit_id(&W, argv[3]); rc = u ? do_found(&W, u, &R) : refuse("no such unit"); }
    else if (!strcmp(verb, "build")) { if (argc < 5) return 2; City *c = city_id(&W, argv[3]); rc = c ? do_build(&W, c, kind_of(argv[4]), &R) : refuse("no such city"); }
    else if (!strcmp(verb, "endturn")) rc = do_endturn(&W, &R);
    else if (!strcmp(verb, "seat")) { if (argc < 5) return 2; int civ = civ_of(argv[3]);
        if (!civ) rc = refuse("civ is civ1 or civ2"); else if (strcmp(argv[4], "human") && strcmp(argv[4], "agent:basic")) rc = refuse("who is human or agent:basic"); else rowf(&R, "SEAT|%s|%s", argv[3], argv[4]); }
    else { fprintf(stderr, "civ_op: unknown verb %s\n", verb); return 2; }
    if (rc) { printf("refused: %s\n", msg); return rc; }
    if (append_rows(ledger, &R)) return 5;
    printf("ok:"); for (int i = 0; i < R.n; i++) printf(" %s%s", R.r[i], i + 1 < R.n ? " ;" : ""); printf("\n"); return 0; }
