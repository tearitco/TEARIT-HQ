/* vox_op - the voxel foundation: place, mine, drops, manual turns, daylight and sleep, as ONE append-only ledger (D105).
 * The ledger is the only state; the world is the FOLD of its rows over the seed terrain (terrain.pdl, a CONFIG). Every number is a row in
 * <data>/rules.pdl (RULE, TOOL, DAYLIGHT) or <data>/blocks.pdl (BLOCK): this file has none (a missing row refuses, exit 9).
 * A verb either appends ALL of its rows in one write or refuses with a nonzero exit and writes NOTHING.
 * Usage: vox_op <verb> <data_dir> <ledger> args...        (data_dir holds rules.pdl blocks.pdl terrain.pdl)
 *   give <actor> <item> <n> <why>        GIVE row (scratch seeding with a stated reason; no turn)
 *   move <actor> <x> <y> <z>             MOVE row + TURN (first move = spawn; later at most RULE move_max cells; never into a solid cell)
 *   mine <actor> <tool> <x> <y> <z>      DAMAGE row; when progress >= hardness also MINE + DROP rows (drop goes to the bag); + TURN
 *   place <actor> <item> <x> <y> <z>     PLACE row (removes one item from the bag) + TURN
 *   wait <n>                             TURN row (a manual turn, n turns)         sleep <actor>   SLEEP + SPAWNPOINT + TURN skipping to the wake minute
 *   status | cell <x y z> | inv <actor>  read only
 *   verify                               re-run every intent row into memory and compare bytes (exit 14 = hand-edited or not reproducible)
 *   replay <dst_ledger>                  re-run every intent row of <ledger> into a NEW file (identical bytes expected)
 * Rows: GIVE|actor|item|n|why  MOVE|actor|x|y|z  DAMAGE|actor|tool|x|y|z|power|progress  MINE|actor|x|y|z|kind  DROP|actor|item|n
 *       PLACE|actor|item|x|y|z  TURN|n|verb  DAY|d  SLEEP|actor|bx|by|bz|skipped_turns=N  SPAWNPOINT|actor|x|y|z
 * Every mutating verb runs verify first, so a hand-edited ledger refuses (14) before anything is added.
 * Exit: 0 ok, 2 usage, 9 missing rule/file, 10 unknown actor, 11 out of bounds, 12 out of reach, 13 target invalid, 14 ledger not reproducible,
 *       15 no support, 16 item missing or not placeable, 17 tool power below min_power, 18 move blocked, 19 sleep: not night, 20 sleep: no bed near, 21 bag full. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAXA 8
#define MAXI 16
#define MAXC 4096
#define MAXB 32
#define MAXR 64
#define MAXT 16
#define MAXD 32
#define MAXX 32
#define NM 32
typedef struct { char name[NM]; int x, y, z, has, ni; char item[MAXI][NM]; int cnt[MAXI]; } Actor;
typedef struct { int x, y, z; char kind[NM]; } Cell;
typedef struct { int x, y, z, prog; } Dmg;
typedef struct { char kind[NM], tool[NM], drops[NM]; int hard, minp, count, solid; } Blk;
typedef struct { char k[NM]; int v; } Rule;
typedef struct { int x1, y1, z1, x2, y2, z2; char kind[NM]; } Box;
typedef struct { Actor a[MAXA]; int na; Cell c[MAXC]; int nc; Dmg d[MAXC]; int nd; int turn; } W;

static FILE *OUT; /* stdout, or /dev/null while verify and replay re-run verbs */
static Blk BL[MAXB]; static int nb; static Rule RU[MAXR]; static int nr; static Rule TL[MAXT]; static int nt;
static Rule DL[MAXD]; static int ndl; static Box BX[MAXX]; static int nx;

static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static int load_data(const char *dir) {
    char p[1024], l[512], *f[16]; FILE *fp;
    snprintf(p, sizeof p, "%s/rules.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 3 && !strcmp(f[0], "RULE") && nr < MAXR) { snprintf(RU[nr].k, NM, "%s", f[1]); RU[nr++].v = atoi(f[2]); }
        else if (n >= 3 && !strcmp(f[0], "TOOL") && nt < MAXT) { snprintf(TL[nt].k, NM, "%s", f[1]); TL[nt++].v = atoi(f[2]); }
        else if (n >= 3 && !strcmp(f[0], "DAYLIGHT") && ndl < MAXD) { snprintf(DL[ndl].k, NM, "%s", f[1]); DL[ndl++].v = atoi(f[2]); } }
    fclose(fp);
    snprintf(p, sizeof p, "%s/blocks.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 9 && !strcmp(f[0], "BLOCK") && nb < MAXB) { Blk *b = &BL[nb++]; snprintf(b->kind, NM, "%s", f[1]); b->hard = atoi(f[2]);
            snprintf(b->tool, NM, "%s", f[3]); b->minp = atoi(f[4]); snprintf(b->drops, NM, "%s", f[5]); b->count = atoi(f[6]); b->solid = atoi(f[7]); } }
    fclose(fp);
    snprintf(p, sizeof p, "%s/terrain.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 8 && !strcmp(f[0], "TERRAIN") && nx < MAXX) { Box *b = &BX[nx++]; b->x1 = atoi(f[1]); b->y1 = atoi(f[2]); b->z1 = atoi(f[3]);
            b->x2 = atoi(f[4]); b->y2 = atoi(f[5]); b->z2 = atoi(f[6]); snprintf(b->kind, NM, "%s", f[7]); } }
    fclose(fp); return 0;
}
static int R(const char *k) { for (int i = 0; i < nr; i++) if (!strcmp(RU[i].k, k)) return RU[i].v; fprintf(stderr, "missing rule %s\n", k); exit(9); }
static Blk *blk(const char *kind) { for (int i = 0; i < nb; i++) if (!strcmp(BL[i].kind, kind)) return &BL[i]; return NULL; }
static int tool_power(const char *t) { for (int i = 0; i < nt; i++) if (!strcmp(TL[i].k, t)) return TL[i].v; fprintf(stderr, "missing tool %s\n", t); exit(9); }

static Actor *actor(W *w, const char *n, int make) {
    for (int i = 0; i < w->na; i++) if (!strcmp(w->a[i].name, n)) return &w->a[i];
    if (!make || w->na >= MAXA) return NULL;
    Actor *a = &w->a[w->na++]; memset(a, 0, sizeof *a); snprintf(a->name, NM, "%s", n); return a;
}
static int *bag(Actor *a, const char *item, int make) {
    for (int i = 0; i < a->ni; i++) if (!strcmp(a->item[i], item)) return &a->cnt[i];
    if (!make || a->ni >= MAXI) return NULL;
    snprintf(a->item[a->ni], NM, "%s", item); a->cnt[a->ni] = 0; return &a->cnt[a->ni++];
}
static const char *kind_at(W *w, int x, int y, int z) {
    for (int i = w->nc - 1; i >= 0; i--) if (w->c[i].x == x && w->c[i].y == y && w->c[i].z == z) return w->c[i].kind;
    for (int i = nx - 1; i >= 0; i--) { Box *b = &BX[i];
        if (x >= b->x1 && x <= b->x2 && y >= b->y1 && y <= b->y2 && z >= b->z1 && z <= b->z2) return b->kind; }
    return "air";
}
static int solid_at(W *w, int x, int y, int z) { Blk *b = blk(kind_at(w, x, y, z)); return b && b->solid; }
static void set_cell(W *w, int x, int y, int z, const char *kind) {
    for (int i = 0; i < w->nc; i++) if (w->c[i].x == x && w->c[i].y == y && w->c[i].z == z) { snprintf(w->c[i].kind, NM, "%s", kind); return; }
    if (w->nc < MAXC) { Cell *c = &w->c[w->nc++]; c->x = x; c->y = y; c->z = z; snprintf(c->kind, NM, "%s", kind); }
}
static int *dmg_at(W *w, int x, int y, int z, int make) {
    for (int i = 0; i < w->nd; i++) if (w->d[i].x == x && w->d[i].y == y && w->d[i].z == z) return &w->d[i].prog;
    if (!make || w->nd >= MAXC) return NULL;
    Dmg *d = &w->d[w->nd++]; d->x = x; d->y = y; d->z = z; d->prog = 0; return &d->prog;
}

/* the fold: ledger rows -> world */
static void fold(W *w, FILE *L) {
    char l[512], *f[10]; memset(w, 0, sizeof *w); rewind(L);
    while (fgets(l, sizeof l, L)) { int n = split(l, f, 10);
        if (n >= 5 && !strcmp(f[0], "GIVE")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 1) : NULL; if (c) *c += atoi(f[3]); }
        else if (n >= 5 && !strcmp(f[0], "MOVE")) { Actor *a = actor(w, f[1], 1); if (a) { a->x = atoi(f[2]); a->y = atoi(f[3]); a->z = atoi(f[4]); a->has = 1; } }
        else if (n >= 8 && !strcmp(f[0], "DAMAGE")) { int *p = dmg_at(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), 1); if (p) *p = atoi(f[7]); }
        else if (n >= 5 && !strcmp(f[0], "MINE")) { int x = atoi(f[2]), y = atoi(f[3]), z = atoi(f[4]); set_cell(w, x, y, z, "air"); int *p = dmg_at(w, x, y, z, 0); if (p) *p = 0; }
        else if (n >= 4 && !strcmp(f[0], "DROP")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 1) : NULL; if (c) *c += atoi(f[3]); }
        else if (n >= 6 && !strcmp(f[0], "PLACE")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 0) : NULL; if (c) *c -= 1;
            set_cell(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), f[2]); int *p = dmg_at(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), 0); if (p) *p = 0; }
        else if (n >= 3 && !strcmp(f[0], "TURN")) w->turn = atoi(f[1]); }
}
static long total_min(int turn) { return (long)R("start_minute") + (long)turn * R("minutes_per_turn"); }
static int minute_of(int turn) { return (int)(total_min(turn) % R("day_minutes")); }
static int light_of(int minute) {
    for (int i = 0; i + 1 < ndl; i++) { int m0 = atoi(DL[i].k), m1 = atoi(DL[i + 1].k);
        if (minute >= m0 && minute <= m1) return m1 == m0 ? DL[i].v : DL[i].v + (DL[i + 1].v - DL[i].v) * (minute - m0) / (m1 - m0); }
    fprintf(stderr, "missing DAYLIGHT row for minute %d\n", minute); exit(9);
}
/* one buffered write: a refusal never reaches here */
static int commit(FILE *L, const char *buf) { fseek(L, 0, SEEK_END); fputs(buf, L); fflush(L); return 0; }
static void adv(char *o, int turn, int n, const char *verb) {   /* TURN row + one DAY row per midnight crossed */
    int dm = R("day_minutes"); long d0 = total_min(turn) / dm, d1 = total_min(turn + n) / dm; char t[96];
    snprintf(t, sizeof t, "TURN | %d | %s\n", turn + n, verb); strcat(o, t);
    for (long d = d0 + 1; d <= d1; d++) { snprintf(t, sizeof t, "DAY | %ld\n", d); strcat(o, t); }
}
static int inb(int x, int y, int z) { return x >= 0 && y >= 0 && z >= 0 && x <= R("world_x_max") && y <= R("world_y_max") && z <= R("world_z_max"); }
static int in_reach(Actor *a, int x, int y, int z, int r) { int dx = a->x - x, dy = a->y - y, dz = a->z - z; return dx * dx + dy * dy + dz * dz <= r * r; }
static int word_ok(const char *s) { if (!*s || strlen(s) >= NM) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-' && *s != '.') return 0; return 1; }

static int v_give(W *w, FILE *L, const char *an, const char *item, int n, const char *why) {
    int smax = R("stack_max"); if (!word_ok(an) || !word_ok(item) || !word_ok(why) || n < 1 || n > smax) return 2;
    Actor *a = actor(w, an, 1); if (!a) return 2; int *c = bag(a, item, 0);
    if ((c ? *c : 0) + n > smax) { fprintf(stderr, "refused: bag full\n"); return 21; }
    char o[256]; snprintf(o, sizeof o, "GIVE | %s | %s | %d | %s\n", an, item, n, why); commit(L, o); fprintf(OUT, "ok\n"); return 0;
}
static int v_move(W *w, FILE *L, const char *an, int x, int y, int z) {
    int mm = R("move_max"), tm = R("turns_move"); if (!word_ok(an)) return 2;
    if (!inb(x, y, z)) { fprintf(stderr, "refused: out of bounds\n"); return 11; }
    if (solid_at(w, x, y, z)) { fprintf(stderr, "refused: cell is solid\n"); return 18; }
    Actor *a = actor(w, an, 0);
    if (a && a->has) { int d = abs(a->x - x) + abs(a->y - y) + abs(a->z - z); if (d > mm || d == 0) { fprintf(stderr, "refused: step\n"); return 18; } }
    else if (!actor(w, an, 1)) return 2;
    char o[512]; snprintf(o, sizeof o, "MOVE | %s | %d | %d | %d\n", an, x, y, z); adv(o, w->turn, tm, "move"); commit(L, o); fprintf(OUT, "at=%d,%d,%d\n", x, y, z); return 0;
}
static int v_mine(W *w, FILE *L, const char *an, const char *tool, int x, int y, int z) {
    int reach = R("reach"), tm = R("turns_mine"), smax = R("stack_max"); if (!word_ok(an) || !word_ok(tool)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    if (!inb(x, y, z)) { fprintf(stderr, "refused: out of bounds\n"); return 11; }
    if (!in_reach(a, x, y, z, reach)) { fprintf(stderr, "refused: out of reach\n"); return 12; }
    Blk *b = blk(kind_at(w, x, y, z)); if (!b || !b->solid) { fprintf(stderr, "refused: nothing to mine\n"); return 13; }
    if (strcmp(tool, "hand")) { int *c = bag(a, tool, 0); if (!c || *c < 1) { fprintf(stderr, "refused: tool not in bag\n"); return 16; } }
    int power = tool_power(!strcmp(tool, b->tool) ? tool : "hand");
    if (power < b->minp) { fprintf(stderr, "refused: power %d below %d\n", power, b->minp); return 17; }
    int *pp = dmg_at(w, x, y, z, 0); int prog = (pp ? *pp : 0) + power; int done = prog >= b->hard;
    if (done && b->count > 0) { int *c = bag(a, b->drops, 0); if ((c ? *c : 0) + b->count > smax) { fprintf(stderr, "refused: bag full\n"); return 21; } }
    char o[1024], t[160]; snprintf(o, sizeof o, "DAMAGE | %s | %s | %d | %d | %d | %d | %d\n", an, tool, x, y, z, power, prog);
    if (done) { snprintf(t, sizeof t, "MINE | %s | %d | %d | %d | %s\n", an, x, y, z, b->kind); strcat(o, t);
        if (b->count > 0) { snprintf(t, sizeof t, "DROP | %s | %s | %d\n", an, b->drops, b->count); strcat(o, t); } }
    adv(o, w->turn, tm, "mine"); commit(L, o);
    fprintf(OUT, "%s stage=%d progress=%d/%d\n", done ? "broken" : "damaged", done ? 3 : 3 * prog / b->hard, prog, b->hard); return 0;
}
static int v_place(W *w, FILE *L, const char *an, const char *item, int x, int y, int z) {
    int reach = R("reach"), tm = R("turns_place"), need = R("place_needs_support"); if (!word_ok(an) || !word_ok(item)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    if (!inb(x, y, z)) { fprintf(stderr, "refused: out of bounds\n"); return 11; }
    if (!in_reach(a, x, y, z, reach)) { fprintf(stderr, "refused: out of reach\n"); return 12; }
    int *c = bag(a, item, 0); if (!c || *c < 1 || !blk(item)) { fprintf(stderr, "refused: item missing or not placeable\n"); return 16; }
    if (solid_at(w, x, y, z) || (a->x == x && a->y == y && a->z == z)) { fprintf(stderr, "refused: target occupied\n"); return 13; }
    if (need) { static const int dd[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}; int sup = 0;
        for (int i = 0; i < 6; i++) if (inb(x + dd[i][0], y + dd[i][1], z + dd[i][2]) && solid_at(w, x + dd[i][0], y + dd[i][1], z + dd[i][2])) sup = 1;
        if (!sup) { fprintf(stderr, "refused: no support\n"); return 15; } }
    char o[512]; snprintf(o, sizeof o, "PLACE | %s | %s | %d | %d | %d\n", an, item, x, y, z); adv(o, w->turn, tm, "place"); commit(L, o); fprintf(OUT, "placed\n"); return 0;
}
static int v_wait(W *w, FILE *L, int n) {
    if (n < 1 || n > R("wait_max_turns")) { fprintf(stderr, "refused: wait n\n"); return 2; }
    char o[16384]; o[0] = 0; adv(o, w->turn, n, "wait"); commit(L, o); fprintf(OUT, "turn=%d\n", w->turn + n); return 0;
}
static int v_sleep(W *w, FILE *L, const char *an) {
    int mm = minute_of(w->turn), light = light_of(mm), dm = R("day_minutes"), mpt = R("minutes_per_turn"), wake = R("sleep_wake_minute");
    int maxl = R("sleep_max_light"), breach = R("sleep_bed_reach"); if (!word_ok(an)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    if (light > maxl) { fprintf(stderr, "refused: not night (light %d)\n", light); return 19; }
    int bx = -1, by = 0, bz = 0, found = 0;
    for (int dx = -breach; dx <= breach && !found; dx++) for (int dy = -breach; dy <= breach && !found; dy++) for (int dz = -breach; dz <= breach && !found; dz++) {
        int x = a->x + dx, y = a->y + dy, z = a->z + dz; if (inb(x, y, z) && !strcmp(kind_at(w, x, y, z), "bed") && in_reach(a, x, y, z, breach)) { bx = x; by = y; bz = z; found = 1; } }
    if (!found) { fprintf(stderr, "refused: no bed near\n"); return 20; }
    int dt = ((wake - mm) % dm + dm) % dm; int turns = (dt + mpt - 1) / mpt; if (turns < 1) turns = 1;
    char o[1024], t[160]; snprintf(o, sizeof o, "SLEEP | %s | %d | %d | %d | skipped_turns=%d\n", an, bx, by, bz, turns);
    snprintf(t, sizeof t, "SPAWNPOINT | %s | %d | %d | %d\n", an, bx, by, bz); strcat(o, t);
    adv(o, w->turn, turns, "sleep"); commit(L, o); fprintf(OUT, "skipped_turns=%d\n", turns); return 0;
}

/* intent dispatch shared by replay and verify: derived rows (MINE DROP DAY SPAWNPOINT, TURN other than wait) are regenerated, never replayed */
static int intent(FILE *dst, const char *line) {
    char l[512], *f[10]; snprintf(l, sizeof l, "%s", line); int n = split(l, f, 10); W w; fold(&w, dst); int r = 0;
    if (n >= 5 && !strcmp(f[0], "GIVE")) r = v_give(&w, dst, f[1], f[2], atoi(f[3]), f[4]);
    else if (n >= 5 && !strcmp(f[0], "MOVE")) r = v_move(&w, dst, f[1], atoi(f[2]), atoi(f[3]), atoi(f[4]));
    else if (n >= 8 && !strcmp(f[0], "DAMAGE")) r = v_mine(&w, dst, f[1], f[2], atoi(f[3]), atoi(f[4]), atoi(f[5]));
    else if (n >= 6 && !strcmp(f[0], "PLACE")) r = v_place(&w, dst, f[1], f[2], atoi(f[3]), atoi(f[4]), atoi(f[5]));
    else if (n >= 3 && !strcmp(f[0], "TURN") && !strcmp(f[2], "wait")) { int k = atoi(f[1]) - w.turn; r = v_wait(&w, dst, k); }
    else if (n >= 6 && !strcmp(f[0], "SLEEP")) r = v_sleep(&w, dst, f[1]);
    return r;
}
static int is_intent(const char *s) {
    char l[512], *f[10]; snprintf(l, sizeof l, "%s", s); int n = split(l, f, 10);
    return n >= 3 && (!strcmp(f[0], "GIVE") || !strcmp(f[0], "MOVE") || !strcmp(f[0], "DAMAGE") || !strcmp(f[0], "PLACE") || !strcmp(f[0], "SLEEP") || (!strcmp(f[0], "TURN") && !strcmp(f[2], "wait")));
}
static int is_derived(const char *s) {
    char l[512], *f[10]; snprintf(l, sizeof l, "%s", s); int n = split(l, f, 10);
    return n >= 2 && (!strcmp(f[0], "MINE") || !strcmp(f[0], "DROP") || !strcmp(f[0], "DAY") || !strcmp(f[0], "SPAWNPOINT") || (!strcmp(f[0], "TURN") && n >= 3));
}
static int replay_into(FILE *src, FILE *dst) {
    char l[512]; rewind(src);
    while (fgets(l, sizeof l, src)) {
        if (is_intent(l)) { if (intent(dst, l)) return 14; }
        else if (!is_derived(l)) return 14;          /* a row of no known kind */
    }
    return 0;
}
static int same(FILE *a, FILE *b) {
    rewind(a); rewind(b); int x, y;
    do { x = fgetc(a); y = fgetc(b); if (x != y) return 0; } while (x != EOF);
    return 1;
}
static int verify(FILE *L) {
    FILE *keep = OUT; OUT = fopen("/dev/null", "w");
    FILE *t = tmpfile(); if (!t) return 2;
    int r = replay_into(L, t); if (!r && !same(L, t)) r = 14;
    fclose(OUT); OUT = keep; fclose(t); if (r) fprintf(stderr, "ledger is not reproducible (hand-edited?)\n"); return r;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--describe")) {
        puts("rule vox inputs data_dir(rules,blocks,terrain),ledger outputs GIVE MOVE DAMAGE MINE DROP PLACE TURN DAY SLEEP SPAWNPOINT rows numbers from RULE TOOL DAYLIGHT BLOCK rows refusal writes nothing");
        return 0;
    }
    if (argc < 4) { fprintf(stderr, "usage: vox_op <verb> <data_dir> <ledger> args (or --describe)\n"); return 2; }
    const char *v = argv[1]; int ac = argc - 4; char **av = argv + 4;
    if (load_data(argv[2])) { fprintf(stderr, "missing data file in %s\n", argv[2]); return 9; }
    FILE *L = fopen(argv[3], "a+"); if (!L) return 2;
    int r = 2; W w; OUT = stdout;
    int mut = !strcmp(v, "give") || !strcmp(v, "move") || !strcmp(v, "mine") || !strcmp(v, "place") || !strcmp(v, "wait") || !strcmp(v, "sleep");
    if (mut && (r = verify(L))) { fclose(L); return r; }
    char cx[3][32]; char *av5[5];   /* a page passes the cell as one word x,y,z (the registry allows 4 params) */
    if ((!strcmp(v, "mine") || !strcmp(v, "place")) && ac == 3 && sscanf(av[2], "%31[-0-9],%31[-0-9],%31[-0-9]", cx[0], cx[1], cx[2]) == 3) {
        av5[0] = av[0]; av5[1] = av[1]; av5[2] = cx[0]; av5[3] = cx[1]; av5[4] = cx[2]; av = av5; ac = 5; }
    fold(&w, L); r = 2;
    if (!strcmp(v, "give") && ac == 4) r = v_give(&w, L, av[0], av[1], atoi(av[2]), av[3]);
    else if (!strcmp(v, "move") && ac == 4) r = v_move(&w, L, av[0], atoi(av[1]), atoi(av[2]), atoi(av[3]));
    else if (!strcmp(v, "mine") && ac == 5) r = v_mine(&w, L, av[0], av[1], atoi(av[2]), atoi(av[3]), atoi(av[4]));
    else if (!strcmp(v, "place") && ac == 5) r = v_place(&w, L, av[0], av[1], atoi(av[2]), atoi(av[3]), atoi(av[4]));
    else if (!strcmp(v, "wait") && ac == 1) r = v_wait(&w, L, atoi(av[0]));
    else if (!strcmp(v, "sleep") && ac == 1) r = v_sleep(&w, L, av[0]);
    else if (!strcmp(v, "status") && ac == 0) {
        int mm = minute_of(w.turn); fprintf(OUT, "turn=%d minute=%d day=%ld light=%d\n", w.turn, mm, total_min(w.turn) / R("day_minutes"), light_of(mm)); r = 0; }
    else if (!strcmp(v, "cell") && ac == 3) { fprintf(OUT, "%s\n", kind_at(&w, atoi(av[0]), atoi(av[1]), atoi(av[2]))); r = 0; }
    else if (!strcmp(v, "inv") && ac == 1) { Actor *a = actor(&w, av[0], 0);
        if (!a) { fprintf(stderr, "unknown actor\n"); r = 10; } else { for (int i = 0; i < a->ni; i++) if (a->cnt[i]) fprintf(OUT, "%s=%d\n", a->item[i], a->cnt[i]); r = 0; } }
    else if (!strcmp(v, "verify") && ac == 0) { r = verify(L); if (!r) fprintf(OUT, "verified\n"); }
    else if (!strcmp(v, "replay") && ac == 1) {
        FILE *d = fopen(av[0], "w+x"); if (!d) { fprintf(stderr, "refused: destination exists or cannot be created\n"); r = 2; } else { FILE *keep = OUT; OUT = fopen("/dev/null", "w"); r = replay_into(L, d); fclose(OUT); OUT = keep; if (!r && !same(L, d)) r = 14; fclose(d); if (!r) fprintf(OUT, "replayed identical\n"); } }
    fclose(L); return r;
}
