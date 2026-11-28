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
#define _DEFAULT_SOURCE   /* realpath, strdup on glibc 2.36 (debil) need it as well */
#include <limits.h>
#include <stdint.h>
#include <ctype.h>
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
#define MAXCIV 5
#define MAXR 256
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
typedef struct { int turn, w, h, nu, nc, nunits_made, ncities_made, nciv, size, diff, mapk; int seat_agent[MAXCIV], civid[MAXCIV];
                 unsigned seed; int ter[MAXT]; Unit u[MAXU]; City c[MAXC]; unsigned char seen[MAXCIV][MAXT]; } World;

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

/* ---------- setup names and presets (every number is a RULE row; D157 setup) ---------- */
static const char *sizename[] = { "small", "medium", "large", "huge" };
#define NSIZE 4
static const char *diffname[] = { "easy", "normal", "hard" };
static const char *mapname[] = { "random", "earth" };
static int name_idx(const char *const *tab, int n, const char *s) { for (int i = 0; i < n; i++) if (!strcmp(tab[i], s)) return i; return -1; }
static int rule_size(int size, const char *what) { char n[80]; snprintf(n, sizeof n, "size_%s_%s", sizename[size], what); return rule(n); }
static int rule_diff(int diff, const char *what) { char n[80]; snprintf(n, sizeof n, "diff_%s_%s", diffname[diff], what); return rule(n); }
static void text_for(const char *key, char *out, size_t n);
/* ---------- the civs are DATA: numbers in RULE rows civ_<k>_{r,g,b,lat,lon,explore_pct} (k = 1..civ_count), words (id, name, leader, colour) in text.pdl ---------- */
static int rule_civ(int k, const char *what) { char n[64]; snprintf(n, sizeof n, "civ_%d_%s", k + 1, what); return rule(n); }
static void civ_word(int k, const char *what, char *o, size_t n) { char key[64]; snprintf(key, sizeof key, "civ_%d_%s", k + 1, what); text_for(key, o, n); }
static int civ_index(const char *id) { char w[48]; for (int k = 0; k < rule("civ_count"); k++) { civ_word(k, "id", w, sizeof w); if (!strcmp(w, id)) return k; } return -1; }

/* ---------- seeded hash and the world ---------- */
static uint32_t mix(uint32_t s, int x, int y, int st) {
    uint32_t h = s * 0x9E3779B9u; h ^= (uint32_t)x * 0x85EBCA6Bu; h ^= (uint32_t)y * 0xC2B2AE35u; h ^= (uint32_t)st * 0x27D4EB2Fu;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; return h; }
static int classify(World *W, int x, int y, int v, int sea, int hill, int mount) {
    int fpct = rule("forest_pct");
    if (v < sea) return WATER;
    if (v >= mount) return MOUNT;
    if (v >= hill) return HILLS;
    return (int)(mix(W->seed, x, y, 2) % 100) >= fpct ? FOREST : GRASS; }
static int gen_terrain(World *W) {
    int w = W->w, h = W->h, S = rule_size(W->size, "noise"), fine = rule("noise_fine"), em = rule_size(W->size, "edge"), ep = rule("edge_pen");
    int sea = rule("sea"), hill = rule("hill"), mount = rule("mount"), land = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int i = x / S, j = y / S, fx = x % S, fy = y % S;
        int a = (int)(mix(W->seed, i, j, 7) % 100), b = (int)(mix(W->seed, i + 1, j, 7) % 100);
        int c = (int)(mix(W->seed, i, j + 1, 7) % 100), d = (int)(mix(W->seed, i + 1, j + 1, 7) % 100);
        int v = ((a * (S - fx) + b * fx) * (S - fy) + (c * (S - fx) + d * fx) * fy) / (S * S);
        v += (int)(mix(W->seed, x, y, 1) % (uint32_t)fine) - fine / 2;
        int dm = x; if (y < dm) dm = y; if (w - 1 - x < dm) dm = w - 1 - x; if (h - 1 - y < dm) dm = h - 1 - y;
        if (dm < em) v -= (em - dm) * ep;
        int k = classify(W, x, y, v, sea, hill, mount);
        W->ter[y * w + x] = k; if (k != WATER) land++; }
    return land; }

/* ---------- the real Earth: ETOPO5 (the SAME file @.apps/earth-moth reads; one authority, never copied) ---------- */
#define ETW 4320
#define ETH 2160
static int16_t *etopo; static char etopo_path[PATH_MAX + 64];
static int find_etopo(void) {
    const char *e = getenv("CIV_ETOPO"); if (e && *e) { snprintf(etopo_path, sizeof etopo_path, "%s", e); return access(etopo_path, R_OK); }
    char r[PATH_MAX]; if (!realpath(rules_path, r)) return 1;
    for (;;) { char *s = strrchr(r, '/'); if (!s) return 1; *s = 0;
        snprintf(etopo_path, sizeof etopo_path, "%s/#.NNEST_ASSETS/rta-sources/earth-moth/ETOPO5.DAT", r);
        if (access(etopo_path, R_OK) == 0) return 0;
        if (!r[0]) return 1; } }
static int load_etopo(void) {            /* big-endian int16, ETW x ETH, row 0 = 90N, col 0 = 0E (earth_moth_op.c load_etopo) */
    if (etopo) return 0;
    if (find_etopo()) return 1;
    FILE *f = fopen(etopo_path, "rb"); if (!f) return 1; size_t n = (size_t)ETW * ETH; etopo = malloc(n * 2); if (!etopo) { fclose(f); return 1; }
    if (fread(etopo, 2, n, f) != n) { free(etopo); etopo = NULL; fclose(f); return 1; } fclose(f);
    for (size_t i = 0; i < n; i++) { unsigned u = (unsigned)(uint16_t)etopo[i]; etopo[i] = (int16_t)((u >> 8) | ((u & 0xff) << 8)); }
    return 0; }
/* elevation at a coordinate in whole degrees x10 (lat10 -899..899, lon10 -1800..3599): the same cell rule as earth_moth_op.c elev_at (12 cells a degree) */
static int elev10(int lat10, int lon10) {
    int east = lon10 < 0 ? lon10 + 3600 : lon10; if (east >= 3600) east = 3599; if (lat10 > 899) lat10 = 899; if (lat10 < -899) lat10 = -899;
    int col = east * 12 / 10, row = (900 - lat10) * 12 / 10; if (col >= ETW) col = ETW - 1; if (row >= ETH) row = ETH - 1; if (row < 0) row = 0;
    return etopo[(size_t)row * ETW + col]; }
/* a tile's elevation: the mean of earth_sub x earth_sub samples over the tile's span (integer) */
static int tile_elev(int w, int h, int x, int y) {
    int top = rule("earth_lat_north"), bot = rule("earth_lat_south"), sub = rule("earth_sub"); long sum = 0;
    for (int sy = 0; sy < sub; sy++) for (int sx = 0; sx < sub; sx++) {
        int lon10 = -1800 + (int)(((long)x * sub + sx) * 3600 / ((long)w * sub)) + (int)(3600 / ((long)w * sub * 2));
        int lat10 = top * 10 - (int)(((long)y * sub + sy) * (long)(top - bot) * 10 / ((long)h * sub)) - (int)((long)(top - bot) * 10 / ((long)h * sub * 2));
        sum += elev10(lat10, lon10); }
    return (int)(sum / (sub * sub)); }
static int gen_earth(World *W) {
    if (load_etopo()) return -1;
    int sea = rule("earth_sea"), hill = rule("earth_hill"), mount = rule("earth_mount"), land = 0;
    for (int y = 0; y < W->h; y++) for (int x = 0; x < W->w; x++) {
        int k = classify(W, x, y, tile_elev(W->w, W->h, x, y), sea, hill, mount); W->ter[y * W->w + x] = k; if (k != WATER) land++; }
    return land; }
static uint64_t grid_hash(const World *W) { uint64_t h = 1469598103934665603ULL; for (int i = 0; i < W->w * W->h; i++) { h ^= (uint64_t)(W->ter[i] + 1); h *= 1099511628211ULL; } return h; }
static int in_map(const World *W, int x, int y) { return x >= 0 && y >= 0 && x < W->w && y < W->h; }
static int tcost(int k) { return k == GRASS ? rule("cost_grass") : k == FOREST ? rule("cost_forest") : k == HILLS ? rule("cost_hills") : rule("cost_mountain"); }

/* ---------- the fold helpers ---------- */
static Unit *unit_at(World *W, int x, int y) { for (int i = 0; i < W->nu; i++) if (W->u[i].alive && W->u[i].x == x && W->u[i].y == y) return &W->u[i]; return NULL; }
static City *city_at(World *W, int x, int y) { for (int i = 0; i < W->nc; i++) if (W->c[i].x == x && W->c[i].y == y) return &W->c[i]; return NULL; }
static Unit *unit_id(World *W, const char *id) { for (int i = 0; i < W->nu; i++) if (W->u[i].alive && !strcmp(W->u[i].id, id)) return &W->u[i]; return NULL; }
static City *city_id(World *W, const char *id) { for (int i = 0; i < W->nc; i++) if (!strcmp(W->c[i].id, id)) return &W->c[i]; return NULL; }
static int civ_of(const char *s) { return !strncmp(s, "civ", 3) && s[3] >= '1' && s[3] <= '4' && !s[4] ? s[3] - '0' : 0; }
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
typedef struct { unsigned seed; int size, diff, rivals, mapk, pick; } Setup;
static int place_near(World *W, int civ, int kind, int bx, int by, Rows *R) {
    for (int d = 0; d < 8; d++) { int k = (2 + d) & 7;     /* east first, then clockwise */
        if (land_free(W, bx + DX[k], by + DY[k])) {
            Unit *u = add_unit(W, civ, kind, bx + DX[k], by + DY[k]); rowf(R, "UNIT|%s|civ%d|%s|%d|%d", u->id, civ, kindname[kind], u->x, u->y); return 1; } }
    return 0; }
/* where a civ wants to start: random maps = four fixed spots of the map, earth = a latitude and longitude of the RULE rows */
static void start_target(World *W, int civ, int *tx, int *ty) {
    if (W->mapk == 1) { int lat = rule_civ(W->civid[civ], "lat"), lon = rule_civ(W->civid[civ], "lon"), top = rule("earth_lat_north"), bot = rule("earth_lat_south");
        *tx = (lon + 180) * W->w / 360; *ty = (top - lat) * W->h / (top - bot); return; }
    *tx = civ == 1 ? W->w / 4 : civ == 2 ? (3 * W->w) / 4 : W->w / 2; *ty = civ <= 2 ? W->h / 2 : civ == 3 ? W->h / 4 : (3 * W->h) / 4; }
static int do_new(World *W, const Setup *S, Rows *R) {
    unsigned seed = S->seed;
    if (S->size < 0 || S->size >= NSIZE) return refuse("size is small medium large or huge");
    if (S->diff < 0 || S->diff > 2) return refuse("difficulty is easy normal or hard");
    if (S->rivals < 1 || S->rivals > 3) return refuse("rivals are 1 to 3");
    if (S->mapk < 0 || S->mapk > 1) return refuse("map is random or earth");
    if (S->pick < 0 || S->pick >= rule("civ_count")) return refuse("no such civ to play");
    memset(W, 0, sizeof *W); W->seed = seed; W->size = S->size; W->diff = S->diff; W->mapk = S->mapk; W->nciv = 1 + S->rivals;
    W->civid[1] = S->pick; { int k = 0; for (int slot = 2; slot <= W->nciv; slot++) { while (k == S->pick) k++; if (k >= rule("civ_count")) return refuse("not enough civs for that many rivals"); W->civid[slot] = k++; } }
    W->w = rule_size(S->size, "w"); W->h = rule_size(S->size, "h");
    if (W->w * W->h > MAXT || W->w < 8 || W->h < 6) return refuse("map size out of range");
    int land = W->mapk == 1 ? gen_earth(W) : gen_terrain(W);
    if (land < 0) return refuse("the ETOPO5 earth data is missing");
    if (land < rule_size(S->size, "land")) return refuse("not enough land for this seed");
    { char cid[48]; civ_word(S->pick, "id", cid, sizeof cid); rowf(R, "NEW|%u|%d|%d|%016llx|%s|%s|%d|%s|%016llx|%s", seed, W->w, W->h, (unsigned long long)rules_hash, sizename[S->size], diffname[S->diff], S->rivals, mapname[S->mapk], (unsigned long long)grid_hash(W), cid); }
    for (int civ = 1; civ <= W->nciv; civ++) {
        int tx, ty, best = 1 << 30, bx = -1, by = -1; start_target(W, civ, &tx, &ty);
        for (int y = 0; y < W->h; y++) for (int x = 0; x < W->w; x++) if (W->ter[y * W->w + x] == GRASS && land_free(W, x, y)) {
            int d = abs(x - tx) + abs(y - ty); if (d < best) { best = d; bx = x; by = y; } }
        if (bx < 0) return refuse("no grass start for a civ");
        Unit *s = add_unit(W, civ, 0, bx, by); rowf(R, "UNIT|%s|civ%d|settler|%d|%d", s->id, civ, bx, by);
        if (!place_near(W, civ, 1, bx, by, R)) return refuse("no room for the warrior");
        if (civ == 1) for (int i = 0; i < rule_diff(S->diff, "human_warriors"); i++) { if (!place_near(W, civ, 1, bx, by, R)) return refuse("no room for the extra warrior"); }
        else for (int i = 0; i < rule_diff(S->diff, "rival_settlers"); i++) { if (!place_near(W, civ, 0, bx, by, R)) return refuse("no room for the extra settler"); } }
    for (int civ = 1; civ <= W->nciv; civ++) { W->seat_agent[civ] = civ > 1; rowf(R, "SEAT|civ%d|%s", civ, civ > 1 ? "agent:basic" : "human"); }
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
            int sc = (rule("aiw_explore") * rule_civ(W->civid[civ], "explore_pct") / 100) * unseen_at(W, civ, u->x + DX[d], u->y + DY[d]) + rule("aiw_walk");
            if (u->kind == 0 && ncity_of(W, civ) == 0) sc += rule("aiw_walk");
            out[n++] = (Cand){ sc, 2, u, NULL, d, 0 }; } }
    for (int i = 0; i < W->nc && n < max; i++) { City *c = &W->c[i]; if (c->civ != civ) continue;
        int want = ncity_of(W, civ) < rule("ai_cities") ? 0 : 1;
        if (c->build != want) out[n++] = (Cand){ rule("aiw_build"), 1, NULL, c, 0, want }; }
    return n; }
static int agent_turn(World *W, int civ, Rows *R) {
    int acted = 0, lim = rule_diff(W->diff, "agent_actions");
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
        int sh = rule("city_shields") + rough_nb(W, c) / rule("rough_div") + (W->seat_agent[c->civ] ? rule_diff(W->diff, "rival_shield_add") : 0); c->stock += sh;
        rowf(R, "PROD|%s|%d|%d", c->id, sh, c->stock);
        int cost = c->build ? rule("cost_warrior") : rule("cost_settler");
        if (c->stock >= cost) {
            int bx = -1, by = -1;
            if (!unit_at(W, c->x, c->y)) { bx = c->x; by = c->y; }
            for (int d = 0; d < 8 && bx < 0; d++) if (land_free(W, c->x + DX[d], c->y + DY[d])) { bx = c->x + DX[d]; by = c->y + DY[d]; }
            if (bx >= 0) { Unit *u = add_unit(W, c->civ, c->build, bx, by); if (u) { c->stock -= cost; rowf(R, "BORN|%s|civ%d|%s|%d|%d", u->id, c->civ, kindname[c->build], bx, by); } } } }
    refresh_mp(W);
    for (int civ = 1; civ <= W->nciv; civ++) if (W->seat_agent[civ]) agent_turn(W, civ, R);
    return 0; }

/* ---------- ledger: load, replay (verify), append ---------- */
#define MAXLEDGER 400000   /* rows a ledger may have (about 5000 turns of a 4-civ game): a longer one is REFUSED, never silently cut (it was a fixed 8192 before 2026-11-27) */
static char **g_lines; static int g_n, g_cap;
static void free_lines(void) { for (int i = 0; i < g_n; i++) free(g_lines[i]); free(g_lines); g_lines = NULL; g_n = g_cap = 0; }
static int read_ledger(const char *path) {
    FILE *f = fopen(path, "r"); char ln[LINE * 2]; free_lines(); if (!f) return 5;
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue;
        if (g_n >= MAXLEDGER) { fclose(f); fprintf(stderr, "civ_op: the ledger has more than %d rows: refused, not cut\n", MAXLEDGER); return 14; }
        if (g_n >= g_cap) { g_cap = g_cap ? g_cap * 2 : 4096; g_lines = realloc(g_lines, (size_t)g_cap * sizeof *g_lines); if (!g_lines) return 5; }
        g_lines[g_n++] = strdup(ln); }
    fclose(f); return 0; }
static int bad(int at, const char *why) { fprintf(stderr, "ledger line %d: %s\n", at + 1, why); return 14; }
/* compare the block just made to the next lines of the ledger (skip the first when it is the row being applied) */
static int match_block(const Rows *R, int from, int at) {
    for (int i = from; i < R->n; i++) { if (at + i - from >= g_n) return bad(g_n, "a derived row is missing"); if (strcmp(g_lines[at + i - from], R->r[i])) { fprintf(stderr, "ledger line %d: expected %s\n", at + i - from + 1, R->r[i]); return bad(at + i - from, "a derived row differs from the rules"); } }
    return 0; }
static int replay(World *W) {
    static Rows R; if (g_n < 1) return bad(0, "empty ledger"); int at = 0; unsigned seed; int w, h, riv; unsigned long long hh, gh; char sz[16], df[16], mp[16], pk[48];
    if (sscanf(g_lines[0], "NEW|%u|%d|%d|%llx|%15[^|]|%15[^|]|%d|%15[^|]|%llx|%47s", &seed, &w, &h, &hh, sz, df, &riv, mp, &gh, pk) != 10) return bad(0, "first row is not NEW (seed, size, difficulty, rivals, map, civ)");
    if (hh != (unsigned long long)rules_hash) return bad(0, "rules.pdl changed after the game began (hash)");
    Setup S = { seed, name_idx(sizename, NSIZE, sz), name_idx(diffname, 3, df), riv, name_idx(mapname, 2, mp), civ_index(pk) };
    if (S.size < 0 || S.diff < 0 || S.mapk < 0 || S.pick < 0) return bad(0, "the setup in the NEW row is not a known size, difficulty, map or civ");
    R.n = 0; if (do_new(W, &S, &R)) return bad(0, msg);
    if (w != W->w || h != W->h) return bad(0, "map size differs from the rules");
    if (gh != (unsigned long long)grid_hash(W)) return bad(0, "the map grid differs from the one the game began with (source data changed)");
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
            int civ = civ_of(a); if (!civ || civ > W->nciv || (strcmp(b, "human") && strcmp(b, "agent:basic"))) return bad(at, "bad SEAT row"); W->seat_agent[civ] = strcmp(b, "human") != 0; at++; }
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
            ch = c ? "ABCD"[c->civ - 1] : u ? (u->kind ? "wxyv"[u->civ - 1] : "szqr"[u->civ - 1]) : tglyph[W->ter[y * W->w + x]]; }
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
    text_for(key, t, sizeof t); printf("next=%s\n", t);
    { char nm[48], ld[48], co[24]; civ_word(W->civid[civ], "name", nm, sizeof nm); civ_word(W->civid[civ], "leader", ld, sizeof ld); civ_word(W->civid[civ], "colour", co, sizeof co);
      printf("civ=%s\nleader=%s\ncolour=%s\nrgb=%d,%d,%d\n", nm, ld, co, rule_civ(W->civid[civ], "r"), rule_civ(W->civid[civ], "g"), rule_civ(W->civid[civ], "b"));
      for (int s = 1; s <= W->nciv; s++) { civ_word(W->civid[s], "name", nm, sizeof nm); printf("party_%d_label=%s%s\nparty_%d_id=civ_%d\nparty_%d_cls=%s\n", s - 1, s == civ ? "> " : "", nm, s - 1, s, s - 1, s == civ ? "active" : "idle"); }
      printf("n_party=%d\n", W->nciv); } }
static void cmd_status(World *W) {
    { int nu1 = 0, nu2 = 0; for (int i = 0; i < W->nu; i++) if (W->u[i].alive) { if (W->u[i].civ == 1) nu1++; else nu2++; } printf("turn=%d cities=%d/%d units=%d/%d seats=%s/%s civs=%d size=%s diff=%s map=%s\n", W->turn, ncity_of(W, 1), ncity_of(W, 2), nu1, nu2, W->seat_agent[1] ? "agent:basic" : "human", W->seat_agent[2] ? "agent:basic" : "human", W->nciv, sizename[W->size], diffname[W->diff], mapname[W->mapk]); } }

/* ---------- lesson: the tutorial grade, DERIVED from the ledger rows (never stored) ---------- */
static int step_done(World *W, const char *check, const char *arg, int civ, int *found_at) {
    if (!strcmp(check, "started")) return g_n >= 1;
    int n = 0;
    for (int i = 0; i < g_n; i++) { char a[20], b[20]; const char *l = g_lines[i];
        if (!strcmp(check, "moved") && sscanf(l, "MOVE|%19[^|]|", a) == 1) { Unit *u = unit_id(W, a); if (!u) for (int k = 0; k < W->nu; k++) if (!strcmp(W->u[k].id, a)) u = &W->u[k]; if (u && u->civ == civ) n++; }
        else if (!strcmp(check, "found") && sscanf(l, "FOUND|%19[^|]|", a) == 1) { for (int k = 0; k < W->nu; k++) if (!strcmp(W->u[k].id, a) && W->u[k].civ == civ) n++; }
        else if (!strcmp(check, "build") && sscanf(l, "BUILD|%19[^|]|%19s", a, b) == 2) { City *c = city_id(W, a); if (c && c->civ == civ && !strcmp(b, arg)) n++; }
        else if (!strcmp(check, "turn")) { if (!strncmp(l, "FOUND|", 6) && *found_at < 0 && sscanf(l, "FOUND|%19[^|]|", a) == 1) { for (int k = 0; k < W->nu; k++) if (!strcmp(W->u[k].id, a) && W->u[k].civ == civ) *found_at = i; } if (!strncmp(l, "TURN|", 5) && *found_at >= 0 && i > *found_at) n++; } }
    return n >= (isdigit((unsigned char)arg[0]) ? atoi(arg) : 1); }
static int cmd_lesson(World *W, const char *file, int civ) {
    FILE *f = fopen(file, "r"); char ln[LINE * 2]; if (!f) return 5;
    struct { char id[24], label[64], sentence[160], check[16], arg[24]; } st[16]; int ns = 0, lmax = 20, smax = 60; char title[80] = "";
    while (fgets(ln, sizeof ln, f) && ns < 16) { ln[strcspn(ln, "\r\n")] = 0; char *x[6]; int nf = 0; char *s = ln;
        while (nf < 6) { while (*s == ' ') s++; x[nf++] = s; char *b = strstr(s, " | "); if (!b) break; *b = 0; s = b + 3; }
        for (int i = 0; i < nf; i++) { char *e = x[i] + strlen(x[i]); while (e > x[i] && e[-1] == ' ') *--e = 0; }
        if (nf >= 3 && !strcmp(x[0], "RULE")) { if (!strcmp(x[1], "label_max")) lmax = atoi(x[2]); if (!strcmp(x[1], "sentence_max")) smax = atoi(x[2]); }
        else if (nf >= 3 && !strcmp(x[0], "LESSON")) snprintf(title, sizeof title, "%s", x[2]);
        else if (nf >= 6 && !strcmp(x[0], "STEP")) { snprintf(st[ns].id, 24, "%s", x[1]); snprintf(st[ns].label, 64, "%s", x[2]); snprintf(st[ns].sentence, 160, "%s", x[3]); snprintf(st[ns].check, 16, "%s", x[4]); snprintf(st[ns].arg, 24, "%s", x[5]); ns++; } }
    fclose(f); if (!ns) return 5;
    for (int i = 0; i < ns; i++) if ((int)strlen(st[i].label) > lmax || (int)strlen(st[i].sentence) > smax) { printf("too long: step %s\n", st[i].id); return 4; }
    int found_at = -1, cur = 0; while (cur < ns && step_done(W, st[cur].check, st[cur].arg, civ, &found_at)) { cur++; found_at = -1; }
    printf("title=%s\n", title);
    if (cur >= ns) { printf("step=%d/%d\nlabel=lesson done!\ngoal=Lesson complete. Well done.\ndone=1\n", ns, ns); return 0; }
    printf("step=%d/%d\nlabel=%s\ngoal=%s\ndone=0\n", cur + 1, ns, st[cur].label, st[cur].sentence); return 0; }

/* ---------- the projection: ONE desk read by the 2D and the 3D board ---------- */
/* ---------- the art: every tile is DRAWN by this op from rules and hash-free patterns (a pure function of the civ colours), 16 x 16 px, SNES-Civ-like: textured terrain, and the pieces drawn ON the terrain tile (settler, warrior, city castle in the owner's colour).
 * The flat 2D view blits cells.txt + cells.rgba, the 3D view textures the floor with the same atlas: one picture for both. Atlas: 0 fog, 1..5 terrain (water grass forest hills mountain), 6 + ((civ-1)*3 + kind) * 5 + terrain = piece on terrain (kind 0 settler 1 warrior 2 city). */
#define NTILES 66
static const char *SPR[3][16] = {
  { "................", ".......oo.......", "......owwo......", "......owwo......", ".......oo.......", ".....ooXXoo.....", "....oXXXXXXo....", "....oXXXXXXoXo..", "....oXXXXXXoXXo.", ".....oXXXXo.oo..", ".....oXooXo.....", ".....oXo.oXo....", ".....oXo.oXo....", ".....ooo.ooo....", "................", "................" },
  { ".............o..", ".............o..", ".......oo....o..", "......ooooo..o..", "......owwwo..o..", ".......oo....o..", "....ooXXXXoo.o..", "...oXXXXXXXXoo..", "...oXXoXXXXoXo..", "...oXXoXXXXoXo..", "...ooXoXXXXoo...", ".....oXooXo.....", ".....oXo.oXo....", ".....oXo.oXo....", ".....ooo.ooo....", "................" },
  { "................", ".o.o..oooo..o.o.", ".oXoooXXXXoooXo.", ".oXXXXXXXXXXXXo.", ".oXwXXXXXXXXwXo.", ".oXXXXXXXXXXXXo.", ".oXXXXXXXXXXXXo.", ".oXXXXXooXXXXXo.", ".oXXXXoooowXXXo.", ".oXXXXooooXXXXo.", ".oXXXXooooXXXXo.", ".oooooooooooooo.", "................", "................", "................", "................" } };
static void terr_px(int k, int x, int y, int *r, int *g, int *b) {
    static const int BASE[5][3] = { {40,90,200}, {96,176,72}, {40,140,60}, {160,136,84}, {172,172,178} };
    int R = BASE[k][0], G = BASE[k][1], B = BASE[k][2], h = (x * 7 + y * 13 + x * y) % 9;
    if (k == 0) { if ((x * 3 + y * 5) % 11 == 0 || ((x + 8) * 3 + (y + 3) * 5) % 13 == 0) { R += 40; G += 50; B += 30; } }
    else if (k == 1) { if (h == 0) { R -= 26; G -= 22; B -= 20; } else if (h == 4 && (x + y) % 3 == 0) { R += 18; G += 14; B += 6; } }
    else if (k == 2) { int tx = x % 8, ty = y % 8; if (ty < 6 && abs(2 * tx - 7) <= ty + 1 && ty >= 1) { R = 18; G = 96; B = 44; if (ty == 1 || (tx == 3 && ty % 2)) { R += 22; G += 40; B += 16; } } else if (ty >= 6 && (tx == 3 || tx == 4)) { R = 96; G = 66; B = 36; } }
    else if (k == 3) { int tx = (x + 4) % 8, ty = y % 8; double dx = (tx - 3.5) / 3.5, dy = (ty - 5.5) / 3.0; if (dx * dx + dy * dy < 1.0 && ty <= 5) { if (tx < 4 && ty < 4) { R += 36; G += 30; B += 20; } } else if ((x + 2 * y) % 5 == 0) { R -= 24; G -= 20; B -= 14; } }
    else { int in = 0, cap = 0; for (int p = 0; p < 2; p++) { int cx = p ? 11 : 4, top = p ? 6 : 2; if (y >= top && abs(x - cx) * 10 <= (y - top) * 7 + 3) { in = 1; if (y < top + 3) cap = 1; } } if (in) { if (cap) { R = 240; G = 240; B = 246; } else { R = 112; G = 112; B = 124; } } else if (h == 2) { R -= 18; G -= 18; B -= 18; } }
    *r = R < 0 ? 0 : R > 255 ? 255 : R; *g = G < 0 ? 0 : G > 255 ? 255 : G; *b = B < 0 ? 0 : B > 255 ? 255 : B; }
static void paint_tile(unsigned char *o, int k, int kind, const unsigned char col[3]) {      /* kind -1 = bare terrain */
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { int r, g, b; terr_px(k, x, y, &r, &g, &b);
        if (x == 0 || y == 0) { r = r * 88 / 100; g = g * 88 / 100; b = b * 88 / 100; }      /* a faint grid line, like the reference's tile edge */
        if (kind >= 0) { char ch = SPR[kind][y][x]; int dk = kind == 1 ? 78 : 100;
            if (ch == 'X') { r = col[0] * dk / 100; g = col[1] * dk / 100; b = col[2] * dk / 100; } else if (ch == 'o') { r = 18; g = 18; b = 30; } else if (ch == 'w') { r = 250; g = 232; b = 200; } }
        size_t i = ((size_t)y * 16 + (size_t)x) * 4; o[i] = (unsigned char)r; o[i + 1] = (unsigned char)g; o[i + 2] = (unsigned char)b; o[i + 3] = 255; } }
static int piece_tile(int slot, int kind, int k) { return 6 + ((slot - 1) * 3 + kind) * 5 + k; }
/* atlas tiles: 0 fog, 1 water, 2 grass, 3 forest, 4 hills, 5 mountain, 6 city civ1, 7 city civ2 (flat colours, a stopgap: placeholder art) */
static const unsigned char TILE[10][3] = { {0,0,0}, {40,90,200}, {90,170,70}, {40,150,60}, {150,130,80}, {170,170,175}, {80,140,255}, {255,90,80}, {250,220,60}, {200,90,230} };
/* unit colours: civ 1..4, [kind 0 settler, 1 warrior] */
static int write_file(const char *path, const char *data, size_t n) {
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, "w"); if (!f) return 5;
    int ok = fwrite(data, 1, n, f) == n; fclose(f); if (!ok || rename(tmp, path)) return 5; return 0; }
static int cmd_project(World *W, int civ, const char *dir, const char *sel) {
    static char cells[MAXT * 8 + 256], mapt[MAXT + 128], ev[MAXT * 80], atl[16 * 16 * NTILES * 4]; size_t nc = 0, nm = 0, ne = 0; char p[PATH_MAX + 32];
    mkdir(dir, 0755);
    nc += (size_t)snprintf(cells + nc, sizeof cells - nc, "width=%d\nheight=%d\ntile_px=16\natlas_cols=1\natlas_tiles=66\ncells\n", W->w, W->h);
    for (int y = 0; y < W->h; y++) { for (int x = 0; x < W->w; x++) { int t, g; City *c = city_at(W, x, y);
            if (!vis(W, civ, x, y)) { t = 0; g = '#'; } else if (c) { t = piece_tile(c->civ, 2, W->ter[y * W->w + x]); g = "ABCD"[c->civ - 1]; } else { int k = W->ter[y * W->w + x]; Unit *pu = unit_at(W, x, y); t = pu ? piece_tile(pu->civ, pu->kind, k) : k + 1; g = tglyph[k]; }
            nc += (size_t)snprintf(cells + nc, sizeof cells - nc, "%s%d,0", x ? " " : "", t); mapt[nm++] = (char)g; }
        cells[nc++] = '\n'; mapt[nm++] = '\n'; }
    for (int i = 0; i < W->nu; i++) { Unit *u = &W->u[i]; if (!u->alive || !vis(W, civ, u->x, u->y)) continue;
        int cr = rule_civ(W->civid[u->civ], "r"), cg = rule_civ(W->civid[u->civ], "g"), cb = rule_civ(W->civid[u->civ], "b"), lt = u->kind ? 0 : 90;      /* the piece wears its civ colour: the warrior as is, the settler lighter */
        int r = cr + (255 - cr) * lt / 255, g = cg + (255 - cg) * lt / 255, b = cb + (255 - cb) * lt / 255;
        if (sel && !strcmp(u->id, sel)) ne += (size_t)snprintf(ev + ne, sizeof ev - ne, "%d %d %d %d %d rig:civ 0 2 0\n", u->x, u->y, r, g, b);      /* the selected unit is the TOKEN the player possesses (the engine forwards the game keys only while a rig: token is possessed) */
        else if (u->kind == 0) ne += (size_t)snprintf(ev + ne, sizeof ev - ne, "%d %d %d %d %d item:settler 0 0 0\n", u->x, u->y, r, g, b);      /* a settler hovers as a small cube */
        else ne += (size_t)snprintf(ev + ne, sizeof ev - ne, "%d %d %d %d %d block 0 2 1\n", u->x, u->y, r, g, b); }      /* a warrior stands as a pillar */
    unsigned char tile[10][3]; memcpy(tile, TILE, sizeof tile);
    for (int s = 1; s <= 4 && s <= W->nciv; s++) { tile[5 + s][0] = (unsigned char)rule_civ(W->civid[s], "r"); tile[5 + s][1] = (unsigned char)rule_civ(W->civid[s], "g"); tile[5 + s][2] = (unsigned char)rule_civ(W->civid[s], "b"); }      /* a city tile is its owner civ colour */
    memset(atl, 0, sizeof atl); for (int i = 0; i < 256; i++) atl[i * 4 + 3] = (char)255;      /* tile 0: unseen = black */
    for (int k = 0; k < 5; k++) paint_tile((unsigned char *)atl + (size_t)(k + 1) * 1024, k, -1, tile[0]);
    for (int s = 1; s <= 4; s++) { unsigned char col[3] = { tile[5 + s][0], tile[5 + s][1], tile[5 + s][2] }; for (int kind = 0; kind < 3; kind++) for (int k = 0; k < 5; k++) paint_tile((unsigned char *)atl + (size_t)piece_tile(s, kind, k) * 1024, k, kind, col); }
    for (int i = 0; i < W->nc; i++) { City *c = &W->c[i]; if (!vis(W, civ, c->x, c->y)) continue;      /* a city is a tower in 3D: a brown trunk with a flag block of the owner's colour */
        int cr = rule_civ(W->civid[c->civ], "r"), cg = rule_civ(W->civid[c->civ], "g"), cb = rule_civ(W->civid[c->civ], "b");
        ne += (size_t)snprintf(ev + ne, sizeof ev - ne, "%d %d %d %d %d tree:2:0:1 0 0 0\n", c->x, c->y, cr, cg, cb); }
    static const char *ext = "EXTRUDE | ~ | 1\nEXTRUDE | . | 1\nEXTRUDE | T | 1\nEXTRUDE | ^ | 3\nEXTRUDE | M | 5\nEXTRUDE | A | 2\nEXTRUDE | B | 2\nEXTRUDE | C | 2\nEXTRUDE | D | 2\nEXTRUDE | # | 1\n";
    int rc = 0;
    snprintf(p, sizeof p, "%s/cells.txt", dir); rc |= write_file(p, cells, nc);
    snprintf(p, sizeof p, "%s/map.txt", dir); rc |= write_file(p, mapt, nm);
    snprintf(p, sizeof p, "%s/events.txt", dir); rc |= write_file(p, ev, ne);
    snprintf(p, sizeof p, "%s/extrusion.pdl", dir); rc |= write_file(p, ext, strlen(ext));
    snprintf(p, sizeof p, "%s/cells.rgba", dir); rc |= write_file(p, atl, sizeof atl);
    {   /* the FLAT view (bv_render_2d) draws this desk's map.txt in these colours when ascii_legend.txt exists: glyph|height|r|g|b|asset|name|char, the SAME colours as the atlas tiles above (one palette, one derivation) */
        char lg[2400]; size_t nl = 0; static const char *gname[] = { "water", "grass", "forest", "hills", "mountain" };
        for (int k = 0; k < 5; k++) nl += (size_t)snprintf(lg + nl, sizeof lg - nl, "%c|%d|%d|%d|%d|-|%s|%c\n", tglyph[k], k == 3 ? 3 : k == 4 ? 5 : 1, tile[k + 1][0], tile[k + 1][1], tile[k + 1][2], gname[k], tglyph[k]);
        for (int s = 1; s <= 4; s++) nl += (size_t)snprintf(lg + nl, sizeof lg - nl, "%c|2|%d|%d|%d|-|city|%c\n", "ABCD"[s - 1], tile[5 + s][0], tile[5 + s][1], tile[5 + s][2], "ABCD"[s - 1]);
        nl += (size_t)snprintf(lg + nl, sizeof lg - nl, "#|1|0|0|0|-|unseen|#\n");
        snprintf(p, sizeof p, "%s/ascii_legend.txt", dir); rc |= write_file(p, lg, nl); }
    /* the engine reloads when cells.marker GROWS (house marker rule): append one byte, restart at 4096 */
    snprintf(p, sizeof p, "%s/cells.marker", dir); struct stat st; int restart = stat(p, &st) == 0 && st.st_size >= 4096;
    FILE *m = fopen(p, restart ? "w" : "a"); if (m) { fputc('1', m); fclose(m); } else rc |= 5;
    return rc ? 5 : 0; }

/* ---------- setup: the choices before the NEW row. The draft file is a PREFERENCE (a cursor, changes no outcome); `start` commits it as ONE ledger row (NEW carries size, difficulty, rivals, map, seed, grid hash) ---------- */
static const char *optname[] = { "Size", "Map", "Difficulty", "Rivals", "Civ", "Seed" };
#define NOPT 6
typedef struct { Setup s; int opt; } Draft;
static void draft_default(Draft *d) { d->s.seed = (unsigned)rule("default_seed"); d->s.size = rule("default_size"); d->s.diff = rule("default_diff"); d->s.rivals = rule("default_rivals"); d->s.mapk = rule("default_map"); d->s.pick = rule("default_civ"); d->opt = 0; }
static void draft_load(const char *path, Draft *d) {
    char ln[LINE]; unsigned long v; draft_default(d); FILE *f = fopen(path, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) { char k[24]; if (sscanf(ln, "%23[^=]=%lu", k, &v) == 2) {
        if (!strcmp(k, "size") && v < NSIZE) d->s.size = (int)v; else if (!strcmp(k, "diff") && v <= 2) d->s.diff = (int)v;
        else if (!strcmp(k, "rivals") && v >= 1 && v <= 3) d->s.rivals = (int)v; else if (!strcmp(k, "map") && v <= 1) d->s.mapk = (int)v;
        else if (!strcmp(k, "seed") && v <= (unsigned long)rule("seed_max")) d->s.seed = (unsigned)v; else if (!strcmp(k, "pick") && v < (unsigned long)rule("civ_count")) d->s.pick = (int)v; else if (!strcmp(k, "opt") && v < NOPT) d->opt = (int)v; } }
    fclose(f); }
static int draft_save(const char *path, const Draft *d) {
    char b[160]; int n = snprintf(b, sizeof b, "size=%d\ndiff=%d\nrivals=%d\nmap=%d\nseed=%u\npick=%d\nopt=%d\n", d->s.size, d->s.diff, d->s.rivals, d->s.mapk, d->s.seed, d->s.pick, d->opt);
    return write_file(path, b, (size_t)n); }
static void draft_value(const Draft *d, int opt, char *o, size_t n) {
    switch (opt) {
    case 0: snprintf(o, n, "%s %dx%d", sizename[d->s.size], rule_size(d->s.size, "w"), rule_size(d->s.size, "h")); break;
    case 1: snprintf(o, n, "%s", mapname[d->s.mapk]); break;
    case 2: snprintf(o, n, "%s", diffname[d->s.diff]); break;
    case 3: snprintf(o, n, "%d", d->s.rivals); break;
    case 4: civ_word(d->s.pick, "name", o, n); break;
    default: snprintf(o, n, "%u", d->s.seed); } }
static void draft_show(const Draft *d) {
    char v[64], t[160]; draft_value(d, d->opt, v, sizeof v);
    { char cn[48]; civ_word(d->s.pick, "name", cn, sizeof cn); printf("nick_line=Setup: you are %s\nstat_line=%s: %s\n", cn, optname[d->opt], v); }
    char a[48], b[48]; draft_value(d, 0, a, sizeof a); draft_value(d, 1, b, sizeof b); printf("stat_line2=%s, %s\n", a, b);
    text_for("setup_next", t, sizeof t); printf("next=%s\n", t);
    for (int i = 0; i < NOPT; i++) { draft_value(d, i, v, sizeof v); printf("option%d=%s%s: %s\n", i + 1, i == d->opt ? "> " : "  ", optname[i], v); }
    for (int k = 0; k < rule("civ_count"); k++) { char nm[48], ld[48], co[24]; civ_word(k, "name", nm, sizeof nm); civ_word(k, "leader", ld, sizeof ld); civ_word(k, "colour", co, sizeof co);
        printf("party_%d_label=%s%s\nparty_%d_id=civ_%d\nparty_%d_cls=%s\ncivline%d=%s%s: %s, %s\n", k, k == d->s.pick ? "> " : "", nm, k, k + 1, k, k == d->s.pick ? "active" : "idle", k + 1, k == d->s.pick ? "> " : "  ", nm, ld, co); }
    printf("n_party=%d\n", rule("civ_count"));
    printf("opt=%d\n", d->opt); }
static int draft_act(Draft *d, const char *act, const char *arg) {
    int smax = rule("seed_max");
    if (!strcmp(act, "show")) return 0;
    if (!strcmp(act, "reset")) { draft_default(d); return 0; }
    if (!strcmp(act, "next")) { d->opt = (d->opt + 1) % NOPT; return 0; }
    if (!strcmp(act, "prev")) { d->opt = (d->opt + NOPT - 1) % NOPT; return 0; }
    if (!strcmp(act, "inc") || !strcmp(act, "dec")) { int up = act[0] == 'i';
        switch (d->opt) {
        case 0: d->s.size = (d->s.size + (up ? 1 : NSIZE - 1)) % NSIZE; break;
        case 1: d->s.mapk = 1 - d->s.mapk; break;
        case 2: d->s.diff = (d->s.diff + (up ? 1 : 2)) % 3; break;
        case 4: d->s.pick = (d->s.pick + (up ? 1 : rule("civ_count") - 1)) % rule("civ_count"); break;
        case 3: d->s.rivals = d->s.rivals + (up ? 1 : -1); if (d->s.rivals > 3) d->s.rivals = 1; if (d->s.rivals < 1) d->s.rivals = 3; break;
        default: if (up) d->s.seed = d->s.seed >= (unsigned)smax ? 0 : d->s.seed + 1; else d->s.seed = d->s.seed ? d->s.seed - 1 : (unsigned)smax; }
        return 0; }
    if (!strcmp(act, "digit")) { if (d->opt != 5 || !arg || arg[0] < '0' || arg[0] > '9' || arg[1]) return refuse("digits type the seed: pick the Seed option first");
        unsigned long v = (unsigned long)d->s.seed * 10 + (unsigned long)(arg[0] - '0'); if (v > (unsigned long)smax) return refuse("that seed is too long"); d->s.seed = (unsigned)v; return 0; }
    if (!strcmp(act, "back")) { d->s.seed /= 10; return 0; }
    if (!strcmp(act, "random")) { if (!arg || !*arg) return refuse("random needs a number"); d->s.seed = (unsigned)(strtoul(arg, NULL, 10) % ((unsigned long)smax + 1)); return 0; }
    return refuse("unknown setup action"); }

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
    if (!strcmp(verb, "world")) { if (argc < 4) { return 2; }       /* world <seed> [size] [map]: the terrain a setup would give, no ledger */
        Setup S = { (unsigned)strtoul(argv[3], NULL, 10), argc > 4 ? name_idx(sizename, NSIZE, argv[4]) : rule("default_size"), 1, 1, argc > 5 ? name_idx(mapname, 2, argv[5]) : 0, 0 };
        if (S.size < 0 || S.mapk < 0) return 2;
        memset(&W, 0, sizeof W); W.seed = S.seed; W.size = S.size; W.mapk = S.mapk; W.w = rule_size(S.size, "w"); W.h = rule_size(S.size, "h"); int land = S.mapk ? gen_earth(&W) : gen_terrain(&W);
        if (land < 0) { printf("refused: the ETOPO5 earth data is missing\n"); return 5; }
        for (int y = 0; y < W.h; y++) { for (int x = 0; x < W.w; x++) { putchar(tglyph[W.ter[y * W.w + x]]); } putchar(10); }
        printf("land=%d of %d\n", land, W.w * W.h); return 0; }
    if (!strcmp(verb, "earthprobe")) { if (argc < 5) return 2;      /* earthprobe <lat x10> <lon x10>: the raw ETOPO5 elevation of a point and its class */
        if (load_etopo()) { printf("refused: the ETOPO5 earth data is missing\n"); return 5; }
        int e = elev10(atoi(argv[3]), atoi(argv[4])); int k = e < rule("earth_sea") ? WATER : e >= rule("earth_mount") ? MOUNT : e >= rule("earth_hill") ? HILLS : GRASS;
        printf("elev=%d class=%s\n", e, k == WATER ? "water" : k == MOUNT ? "mountain" : k == HILLS ? "hills" : "land"); return 0; }
    if (!strcmp(verb, "earthtile")) { if (argc < 6) return 2;      /* earthtile <size> <lat> <lon>: the tile of the earth world of that preset that holds a coordinate, its class and the grid hash */
        int sz = name_idx(sizename, NSIZE, argv[3]); if (sz < 0) return 2; memset(&W, 0, sizeof W); W.size = sz; W.mapk = 1; W.w = rule_size(sz, "w"); W.h = rule_size(sz, "h");
        if (gen_earth(&W) < 0) { printf("refused: the ETOPO5 earth data is missing\n"); return 5; }
        int lat = atoi(argv[4]), lon = atoi(argv[5]), top = rule("earth_lat_north"), bot = rule("earth_lat_south"), x = (lon + 180) * W.w / 360, y = (top - lat) * W.h / (top - bot);
        printf("tile=%d,%d water=%d class=%s hash=%016llx\n", x, y, W.ter[y * W.w + x] == WATER, tname[W.ter[y * W.w + x]], (unsigned long long)grid_hash(&W)); return 0; }
    if (!strcmp(verb, "setup")) { if (argc < 5) return 2; Draft d; draft_load(argv[3], &d); int rc0 = draft_act(&d, argv[4], argc > 5 ? argv[5] : NULL);
        if (rc0) { printf("refused: %s\n", msg); return rc0; }
        if (strcmp(argv[4], "show") && draft_save(argv[3], &d)) return 5;
        draft_show(&d); return 0; }
    if (!strcmp(verb, "preview")) { if (argc < 5) return 2; Draft d; draft_load(argv[3], &d); int rc0 = do_new(&W, &d.s, &R);
        if (rc0) { printf("refused: %s\n", msg); return rc0; } return cmd_project(&W, 0, argv[4], "u1"); }
    if (!strcmp(verb, "new") || !strcmp(verb, "start")) {
        Setup S; if (!strcmp(verb, "start")) { if (argc < 4) return 2; Draft d; draft_load(argv[3], &d); S = d.s; }
        else { if (argc < 4) return 2; S.seed = (unsigned)strtoul(argv[3], NULL, 10); S.size = argc > 4 ? name_idx(sizename, NSIZE, argv[4]) : rule("default_size"); S.diff = argc > 5 ? name_idx(diffname, 3, argv[5]) : rule("default_diff");
            S.rivals = argc > 6 ? atoi(argv[6]) : rule("default_rivals"); S.mapk = argc > 7 ? name_idx(mapname, 2, argv[7]) : rule("default_map"); S.pick = argc > 8 ? civ_index(argv[8]) : rule("default_civ"); }
        struct stat st; if (stat(ledger, &st) == 0 && st.st_size > 0) { printf("refused: a ledger already exists\n"); return 4; }
        int rc = do_new(&W, &S, &R); if (rc) { printf("refused: %s\n", msg); return rc; }
        if (append_rows(ledger, &R)) { return 5; }
        printf("ok: new game seed %u, %s, %s, %d rival(s), %s map, %d rows\n", S.seed, sizename[S.size], diffname[S.diff], S.rivals, mapname[S.mapk], R.n); return 0; }
    if (!strcmp(verb, "lesson")) { struct stat st; if (argc >= 4 && (stat(ledger, &st) != 0 || st.st_size == 0)) { memset(&W, 0, sizeof W); g_n = 0; return cmd_lesson(&W, argv[3], argc > 4 ? civ_of(argv[4]) : 1); } }
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
    if (!strcmp(verb, "lesson")) { if (argc < 4) { return 2; } return cmd_lesson(&W, argv[3], argc > 4 ? civ_of(argv[4]) : 1); }
    if (!strcmp(verb, "project")) { if (argc < 5) return 2; return cmd_project(&W, civ_of(argv[3]), argv[4], argc > 5 ? argv[5] : ""); }
    if (!strcmp(verb, "move")) { if (argc < 5) return 2; Unit *u = unit_id(&W, argv[3]); int d = dir_of(argv[4]);
        if (!u) rc = refuse("no such unit"); else if (d < 0) rc = refuse("direction is n ne e se s sw w nw"); else rc = do_move(&W, u, d, &R); }
    else if (!strcmp(verb, "found")) { if (argc < 4) return 2; Unit *u = unit_id(&W, argv[3]); rc = u ? do_found(&W, u, &R) : refuse("no such unit"); }
    else if (!strcmp(verb, "build")) { if (argc < 5) return 2; City *c = city_id(&W, argv[3]); rc = c ? do_build(&W, c, kind_of(argv[4]), &R) : refuse("no such city"); }
    else if (!strcmp(verb, "endturn")) rc = do_endturn(&W, &R);
    else if (!strcmp(verb, "seat")) { if (argc < 5) return 2; int civ = civ_of(argv[3]);
        if (!civ || civ > W.nciv) rc = refuse("no such civ in this game"); else if (strcmp(argv[4], "human") && strcmp(argv[4], "agent:basic")) rc = refuse("who is human or agent:basic"); else rowf(&R, "SEAT|%s|%s", argv[3], argv[4]); }
    else { fprintf(stderr, "civ_op: unknown verb %s\n", verb); return 2; }
    if (rc) { printf("refused: %s\n", msg); return rc; }
    if (append_rows(ledger, &R)) return 5;
    printf("ok:"); for (int i = 0; i < R.n; i++) printf(" %s%s", R.r[i], i + 1 < R.n ? " ;" : ""); printf("\n"); return 0; }
