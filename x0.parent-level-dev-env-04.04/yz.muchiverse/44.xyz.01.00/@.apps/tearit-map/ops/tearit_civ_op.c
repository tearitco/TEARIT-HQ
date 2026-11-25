/* tearit_civ_op: the Civ turn loop of the tearit-map book as ledger rows (BOOK 16.game/TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md sections 4 and 5, decision D136).
 * It never edits tearit_map_op: it reads the same ledger (CELL, START, MOVE, FOUND, AGENT, CHECK rows) and appends only its own rows:
 *   TURN | n                        the only thing that advances time (endturn)
 *   SEAT | civ1 | human             who plays a civ; the LAST row for a civ wins (a driver change is a row)
 *   SEAT | civ2 | agent:basic
 * and for the basic agent (civ2) the rows tearit_map_op already knows: AGENT | x | y | basic, FOUND | x | y | agent.
 * Food and city counts are FOLDS of the ledger (status), never stored. Numbers are RULE rows of civ.pdl (CONFIG), never C.
 * usage: tearit_civ_op status <ledger> | seat <ledger> <civ1|civ2> <human|agent:basic> | legal <ledger> | agentmove <ledger> <found|n|e|s|w>
 *        | aiturn <ledger> | endturn <ledger>
 * exit: 0 ok, 2 usage, 4 refused (nothing written), 5 io. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tearit_civ_op.+x tearit_civ_op.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXC 4096
static int W, H_, *land_;          /* the map, from CELL rows */
static int sx0, sy0, sx1, sy1;     /* START 0 (human) and START 1 (other seat) */
static int has1;
static int rule_city_food, rule_citygap, rule_found, rule_expand, rule_walk;

static int rule_file(char *out, size_t n) {
    const char *e = getenv("TEARIT_CIV_RULES");
    if (e && *e) { snprintf(out, n, "%s", e); return 0; }
    return 1;
}
static int load_rules(const char *self) {
    char p[PATH_MAX + 32], ln[256], nm[48]; int v, got = 0;
    if (rule_file(p, sizeof p)) {   /* <book>/ops/+x/<this> -> <book>/civ.pdl */
        char r[PATH_MAX]; if (!realpath(self, r)) return 1;
        for (int i = 0; i < 3; i++) { char *s = strrchr(r, '/'); if (!s) return 1; *s = 0; }
        snprintf(p, sizeof p, "%s/civ.pdl", r);
    }
    FILE *f = fopen(p, "r"); if (!f) return 1;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "RULE | %47s | %d", nm, &v) == 2) {
        if (!strcmp(nm, "city_food")) { rule_city_food = v; got |= 1; }
        else if (!strcmp(nm, "citygap")) { rule_citygap = v; got |= 2; }
        else if (!strcmp(nm, "aiw_found")) { rule_found = v; got |= 4; }
        else if (!strcmp(nm, "aiw_expand")) { rule_expand = v; got |= 8; }
        else if (!strcmp(nm, "aiw_walk")) { rule_walk = v; got |= 16; } }
    fclose(f); return got == 31 ? 0 : 1; }

static int load_map(const char *ledger) {
    FILE *f = fopen(ledger, "r"); char ln[512], kind[32]; int x, y, a, b;
    if (!f) return 5;
    W = H_ = 0; has1 = 0;
    while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "CELL | %d | %d | %31s", &x, &y, kind) == 3) { if (x + 1 > W) W = x + 1; if (y + 1 > H_) H_ = y + 1; }
    if (W < 1 || H_ < 1 || W > 256 || H_ > 256) { fclose(f); return 4; }
    land_ = calloc((size_t)W * H_, sizeof(int)); if (!land_) { fclose(f); return 5; }
    rewind(f);
    while (fgets(ln, sizeof ln, f)) {
        if (sscanf(ln, "CELL | %d | %d | %31s", &x, &y, kind) == 3) land_[y * W + x] = strcmp(kind, "water") != 0;
        else if (sscanf(ln, "START | 0 | %d | %d", &a, &b) == 2) { sx0 = a; sy0 = b; }
        else if (sscanf(ln, "START | 1 | %d | %d", &a, &b) == 2) { sx1 = a; sy1 = b; has1 = 1; } }
    fclose(f); return 0; }

/* the fold: turn, per-civ city counts and food, positions, seats. Returns 0, or 4 when a TURN row is out of order (a hand edit). */
typedef struct { int turn, hc, ac, hfood, afood, hx, hy, ax, ay; char seat1[24], seat2[24]; int fx[MAXC], fy[MAXC], nf; } Fold;
static int fold(const char *ledger, Fold *F) {
    FILE *f = fopen(ledger, "r"); char ln[512], who[24], seat[24]; int a, b, n;
    if (!f) return 5;
    memset(F, 0, sizeof *F);
    F->hx = sx0; F->hy = sy0; F->ax = sx1; F->ay = sy1;
    snprintf(F->seat1, sizeof F->seat1, "none"); snprintf(F->seat2, sizeof F->seat2, "none");
    while (fgets(ln, sizeof ln, f)) {
        if (sscanf(ln, "MOVE | %d | %d", &a, &b) == 2) { F->hx = a; F->hy = b; }
        else if (sscanf(ln, "AGENT | %d | %d", &a, &b) == 2) { F->ax = a; F->ay = b; }
        else if (sscanf(ln, "FOUND | %d | %d | %23s", &a, &b, who) == 3) {
            if (F->nf < MAXC) { F->fx[F->nf] = a; F->fy[F->nf] = b; F->nf++; }
            if (!strcmp(who, "agent")) F->ac++; else F->hc++; }
        else if (sscanf(ln, "TURN | %d", &n) == 1) {
            if (n != F->turn + 1) { fclose(f); return 4; }
            F->turn = n; F->hfood += F->hc * rule_city_food; F->afood += F->ac * rule_city_food; }
        else if (sscanf(ln, "SEAT | %23s | %23s", who, seat) == 2) {
            if (!strcmp(who, "civ1")) snprintf(F->seat1, sizeof F->seat1, "%.23s", seat);
            else if (!strcmp(who, "civ2")) snprintf(F->seat2, sizeof F->seat2, "%.23s", seat); } }
    fclose(f); return 0; }

/* a CHECK row is the hash of every row before it (the same fold tearit_map_op checkpoint writes); any hand edit above it is refused */
static int chain_ok(const char *ledger) {
    FILE *f = fopen(ledger, "r"); char ln[512]; uint32_t h = 2166136261u; unsigned got;
    if (!f) return 5;
    while (fgets(ln, sizeof ln, f)) {
        if (sscanf(ln, "CHECK | %u", &got) == 1 && got != h) { fclose(f); return 4; }
        for (const char *s = ln; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; } }
    fclose(f); return 0; }

static int dist_city(const Fold *F, int x, int y) {
    int best = 1 << 20; for (int i = 0; i < F->nf; i++) { int d = abs(F->fx[i] - x) + abs(F->fy[i] - y); if (d < best) best = d; } return best; }
static int is_land(int x, int y) { return x >= 0 && y >= 0 && x < W && y < H_ && land_[y * W + x]; }
static const char *DN = "nesw"; static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

typedef struct { char m[8]; int w; } Move;
static int legal(const Fold *F, Move *out) {
    int n = 0;
    if (is_land(F->ax, F->ay) && dist_city(F, F->ax, F->ay) >= rule_citygap) { snprintf(out[n].m, 8, "found"); out[n].w = rule_found; n++; }
    int here = dist_city(F, F->ax, F->ay);
    for (int i = 0; i < 4; i++) {
        int x = F->ax + DX[i], y = F->ay + DY[i];
        if (!is_land(x, y) || (x == F->hx && y == F->hy)) continue;
        out[n].m[0] = DN[i]; out[n].m[1] = 0; out[n].w = (F->nf == 0 || dist_city(F, x, y) > here) ? rule_expand : rule_walk; n++; }
    return n; }

static int append(const char *ledger, const char *fmt, int a, int b) {  /* fmt may use one or both ints */
    FILE *f = fopen(ledger, "a"); if (!f) return 5; fprintf(f, fmt, a, b); fclose(f); return 0; }  /* NOLINT */

static int do_move(const char *ledger, const Fold *F, const char *m) {
    if (!strcmp(m, "found")) { if (append(ledger, "FOUND | %d | %d | agent\n", F->ax, F->ay)) return 5;
        printf("agent=found %d,%d\n", F->ax, F->ay); return 0; }
    for (int i = 0; i < 4; i++) if (m[0] == DN[i] && !m[1]) {
        if (append(ledger, "AGENT | %d | %d | basic\n", F->ax + DX[i], F->ay + DY[i])) return 5;
        printf("agent=walk %d,%d\n", F->ax + DX[i], F->ay + DY[i]); return 0; }
    return 4; }

static int agent_seat(const Fold *F) { return !strcmp(F->seat2, "agent:basic") && has1; }

static int cmd_agentmove(const char *ledger, const char *m) {
    Fold *F = malloc(sizeof *F); Move L[8]; int rc = fold(ledger, F); if (rc) { printf("fail turn\n"); free(F); return rc; }
    if (!agent_seat(F)) { printf("fail seat\n"); free(F); return 4; }
    int n = legal(F, L), ok = 0; for (int i = 0; i < n; i++) if (!strcmp(L[i].m, m)) ok = 1;
    if (!ok) { printf("fail illegal\n"); free(F); return 4; }
    rc = do_move(ledger, F, m); free(F); return rc; }

static int cmd_aiturn(const char *ledger) {
    Fold *F = malloc(sizeof *F); Move L[8]; int rc = fold(ledger, F); if (rc) { printf("fail turn\n"); free(F); return rc; }
    if (!agent_seat(F)) { printf("aiturn=none\n"); free(F); return 0; }
    int n = legal(F, L); if (!n) { printf("aiturn=pass\n"); free(F); return 0; }
    int best = 0; for (int i = 1; i < n; i++) if (L[i].w > L[best].w) best = i;   /* strict >: ties keep the first */
    rc = do_move(ledger, F, L[best].m); free(F); return rc; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: tearit_civ_op status|legal|aiturn|endturn <ledger> | seat <ledger> <civ> <who> | agentmove <ledger> <move>\n"); return 2; }
    const char *v = argv[1], *ledger = argv[2];
    if (load_rules(argv[0])) { fprintf(stderr, "tearit_civ_op: civ.pdl rules missing\n"); return 2; }
    int rc = load_map(ledger); if (rc) { printf("fail map\n"); return rc; }
    if (!strcmp(v, "status") && argc == 3) {
        Fold *F = malloc(sizeof *F); rc = fold(ledger, F); if (rc) { printf("fail turn\n"); free(F); return rc; }
        printf("turn=%d cities=%d food=%d ai_cities=%d ai_food=%d civ1=%s civ2=%s\n", F->turn, F->hc, F->hfood, F->ac, F->afood, F->seat1, F->seat2); free(F); return 0; }
    if (!strcmp(v, "seat") && argc == 5) {
        if ((strcmp(argv[3], "civ1") && strcmp(argv[3], "civ2")) || (strcmp(argv[4], "human") && strcmp(argv[4], "agent:basic"))) { printf("fail seat\n"); return 2; }
        FILE *f = fopen(ledger, "a"); if (!f) return 5; fprintf(f, "SEAT | %s | %s\n", argv[3], argv[4]); fclose(f); printf("seat=%s %s\n", argv[3], argv[4]); return 0; }
    if (!strcmp(v, "legal") && argc == 3) {
        Fold *F = malloc(sizeof *F); Move L[8]; rc = fold(ledger, F); if (rc) { printf("fail turn\n"); free(F); return rc; }
        int n = legal(F, L); for (int i = 0; i < n; i++) printf("move %s weight=%d\n", L[i].m, L[i].w); free(F); return 0; }
    if (!strcmp(v, "agentmove") && argc == 4) return cmd_agentmove(ledger, argv[3]);
    if (!strcmp(v, "aiturn") && argc == 3) return cmd_aiturn(ledger);
    if (!strcmp(v, "endturn") && argc == 3) {
        Fold *F = malloc(sizeof *F);
        if (chain_ok(ledger)) { printf("fail check\n"); free(F); return 4; }
        rc = fold(ledger, F); if (rc) { printf("fail turn\n"); free(F); return rc; }
        int nt = F->turn + 1; free(F);
        if (append(ledger, "TURN | %d\n", nt, 0)) return 5;
        printf("turn=%d\n", nt);
        return cmd_aiturn(ledger); }
    return 2; }
