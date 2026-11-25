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
 *   eat <actor> <item>                   EAT row (removes one food item from the bag, lowers hunger by RULE food_<item>) + TURN; refused: not food/none in bag (16), hunger 0 (23)
 *   craft <actor> <recipe>               CRAFT row: RECIPE rows of rules.pdl (name out outn in1 n1 in2 n2 station): inputs leave the bag, the output enters it, a station block (craft_table) must be within RULE craft_reach; refused 10 unknown actor, 16 no such recipe or inputs missing, 20 no station near; needs RULE turns_craft and craft_reach
 *   walk <actor> <n|e|s|w>               MOVE one column on, resolving the target height from the terrain (highest solid + 1): refused 11 off the map or over no ground, 18 for a step up above RULE step_up, a fall above fall_max, water deeper than water_max_depth (D137)
 *   face <actor> <n|e|s|w>               FACE row (a turn on the spot; no cell change, no turn counted)      jump <actor>   two MOVE rows (up RULE jump_height, back down) in ONE write; 15 no ground under the feet, 18 no room
 *   seat <actor> <human|harness>         SEAT row: who drives the actor (a row, never a file); `seat <actor>` prints seat=human|harness (no row = harness)
 *   reset <actor>                        RESET row: back to the spawn point (the first MOVE, or the last SPAWNPOINT)      ground <x> <z>  top=<highest solid y> kind=<kind> water=<highest water y or -1>      where <actor>
 *   (data_dir may also hold terrain.heightmap: a second terrain source, see load_heightmap; limits DMAX 216 for the desk projection, HMAX 256 columns for the heightmap)
 *   status | cell <x y z> | inv <actor> | hunger <actor>  read only (hunger: rises hunger_per_turn on every TURN row for a spawned actor, cap hunger_max; at starve_at mine/place refuse 22)
 *   verify                               re-run every intent row into memory and compare bytes (exit 14 = hand-edited or not reproducible)
 *   replay <dst_ledger>                  re-run every intent row of <ledger> into a NEW file (identical bytes expected)
 * Rows: FACE|actor|dir  RESET|actor|spawn  SEAT|actor|who  GIVE|actor|item|n|why  MOVE|actor|x|y|z  DAMAGE|actor|tool|x|y|z|power|progress  MINE|actor|x|y|z|kind  DROP|actor|item|n
 *       PLACE|actor|item|x|y|z  EAT|actor|item|value  TURN|n|verb  DAY|d  SLEEP|actor|bx|by|bz|skipped_turns=N  SPAWNPOINT|actor|x|y|z
 * Every mutating verb runs verify first, so a hand-edited ledger refuses (14) before anything is added.
 * Exit: 0 ok, 2 usage, 9 missing rule/file, 10 unknown actor, 11 out of bounds, 12 out of reach, 13 target invalid, 14 ledger not reproducible,
 *       15 no support, 16 item missing or not placeable, 17 tool power below min_power, 18 move blocked, 19 sleep: not night, 20 sleep: no bed near, 21 bag full,
 *       22 starved (hunger >= starve_at: mine and place refuse; eat, wait, sleep, move still work), 23 eat: not hungry. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAXA 8
#define MAXI 16
#define MAXC 4096
#define MAXB 32
#define MAXR 64
#define MAXT 16
#define MAXD 32
#define MAXX 32
#define NM 32
typedef struct { char name[NM]; int x, y, z, has, hunger, ni; char item[MAXI][NM]; int cnt[MAXI]; int sx, sy, sz; char dir; int seat; } Actor;   /* sx sy sz = the spawn point (first MOVE, then the last SPAWNPOINT), dir = the last FACE letter (n e s w) or 0 */
typedef struct { int x, y, z; char kind[NM]; } Cell;
typedef struct { int x, y, z, prog; } Dmg;
typedef struct { char kind[NM], tool[NM], drops[NM]; int hard, minp, count, solid; } Blk;
typedef struct { char k[NM]; int v; } Rule;
typedef struct { int x1, y1, z1, x2, y2, z2; char kind[NM]; } Box;
typedef struct { Actor a[MAXA]; int na; Cell c[MAXC]; int nc; Dmg d[MAXC]; int nd; int turn; } W;

static FILE *OUT; /* stdout, or /dev/null while verify and replay re-run verbs */
static Blk BL[MAXB]; static int nb; static Rule RU[MAXR]; static int nr; static Rule TL[MAXT]; static int nt;
typedef struct { char k[NM], out[NM], i1[NM], i2[NM], st[NM]; int on, n1, n2; } Rcp; static Rcp RC[16]; static int nrc;   /* RECIPE rows of rules.pdl: name out outn in1 n1 in2 n2 station (- = none) */
static Rule DL[MAXD]; static int ndl; static Box BX[MAXX]; static int nx;
typedef struct { char kind[NM]; int slot, r, g, b; } Pic; static Pic PC[MAXB]; static int npc; static int PL[3] = { 255, 255, 0 };
static int SH_PIT[3], SH_RAISE[3], has_pit, has_raise;   /* SHADE rows of blocks.pdl (2D depth cue of map.png only); absent row = no cue */

static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
/* HEIGHTMAP terrain source (D137): <data>/terrain.heightmap, rows (runs of equal columns along one board row; vox z = the desk row, vox x = the desk column, vox y = height):
 *   META | width | W      META | height | H      META | shift | S      (S is only for the readers that map desk levels to vox y: ground_y = level - 1 + S)
 *   RUN | z | x0 | n | ground_y | kind_top | kind_below | water_level      columns x0..x0+n-1 of row z: y == ground_y is kind_top, 0 <= y < ground_y is kind_below,
 *                                                                          ground_y < y <= water_level is water (water_level -1 = none); every other y is air
 * A column with no RUN is sky (air all the way). Consulted by kind_at AFTER the changed cells and BEFORE the TERRAIN boxes. The arrays are static and sized by the engine's own board limit
 * (256 x 256 columns, 6 bytes each = 384 KiB of bss): the first island is 216 x 156. A row outside META width/height or past 256, or an unknown kind name more than HK kinds, refuses (exit 9). */
#define HMAX 256
#define HK 32
typedef struct { short top, wl; unsigned char kt, kb; } HCol;
static HCol HM[HMAX][HMAX]; static int hm_on, hm_w, hm_h; static char HKN[HK][NM]; static int nhk;
static int hk(const char *k) { for (int i = 0; i < nhk; i++) if (!strcmp(HKN[i], k)) return i; if (nhk >= HK) return -1; snprintf(HKN[nhk], NM, "%s", k); return nhk++; }
static int load_heightmap(FILE *fp) {
    char l[512], *f[10]; for (int z = 0; z < HMAX; z++) for (int x = 0; x < HMAX; x++) HM[z][x].top = -1, HM[z][x].wl = -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 10);
        if (n >= 3 && !strcmp(f[0], "META") && !strcmp(f[1], "width")) hm_w = atoi(f[2]);
        else if (n >= 3 && !strcmp(f[0], "META") && !strcmp(f[1], "height")) hm_h = atoi(f[2]);
        else if (n >= 8 && !strcmp(f[0], "RUN")) { int z = atoi(f[1]), x0 = atoi(f[2]), c = atoi(f[3]), gy = atoi(f[4]), wl = atoi(f[7]); int kt = hk(f[5]), kb = hk(f[6]);
            if (kt < 0 || kb < 0 || z < 0 || z >= hm_h || z >= HMAX || x0 < 0 || c < 1 || x0 + c > hm_w || x0 + c > HMAX || gy < 0) return -1;
            for (int x = x0; x < x0 + c; x++) { HM[z][x].top = (short)gy; HM[z][x].wl = (short)wl; HM[z][x].kt = (unsigned char)kt; HM[z][x].kb = (unsigned char)kb; } } }
    if (hm_w < 1 || hm_h < 1 || hm_w > HMAX || hm_h > HMAX) return -1;
    hm_on = 1; return 0;
}
static const char *hm_kind(int x, int y, int z) {   /* NULL = this source has no opinion (not loaded, off the map or sky column) */
    if (!hm_on || x < 0 || z < 0 || x >= hm_w || z >= hm_h || y < 0) return NULL;
    HCol *c = &HM[z][x]; if (c->top < 0) return NULL;
    if (y == c->top) return HKN[c->kt];
    if (y < c->top) return HKN[c->kb];
    if (c->wl >= 0 && y <= c->wl) return "water";
    return "air";
}
static int load_data(const char *dir) {
    char p[1024], l[512], *f[16]; FILE *fp;
    snprintf(p, sizeof p, "%s/rules.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 3 && !strcmp(f[0], "RULE") && nr < MAXR) { snprintf(RU[nr].k, NM, "%s", f[1]); RU[nr++].v = atoi(f[2]); }
        else if (n >= 3 && !strcmp(f[0], "TOOL") && nt < MAXT) { snprintf(TL[nt].k, NM, "%s", f[1]); TL[nt++].v = atoi(f[2]); }
        else if (n >= 9 && !strcmp(f[0], "RECIPE") && nrc < 16) { Rcp *c = &RC[nrc++]; snprintf(c->k, NM, "%s", f[1]); snprintf(c->out, NM, "%s", f[2]); c->on = atoi(f[3]); snprintf(c->i1, NM, "%s", f[4]); c->n1 = atoi(f[5]); snprintf(c->i2, NM, "%s", f[6]); c->n2 = atoi(f[7]); snprintf(c->st, NM, "%s", f[8]); }
        else if (n >= 3 && !strcmp(f[0], "DAYLIGHT") && ndl < MAXD) { snprintf(DL[ndl].k, NM, "%s", f[1]); DL[ndl++].v = atoi(f[2]); } }
    fclose(fp);
    snprintf(p, sizeof p, "%s/blocks.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 9 && !strcmp(f[0], "BLOCK") && nb < MAXB) { Blk *b = &BL[nb++]; snprintf(b->kind, NM, "%s", f[1]); b->hard = atoi(f[2]);
            snprintf(b->tool, NM, "%s", f[3]); b->minp = atoi(f[4]); snprintf(b->drops, NM, "%s", f[5]); b->count = atoi(f[6]); b->solid = atoi(f[7]); }
        else if (n >= 6 && !strcmp(f[0], "PICTURE") && npc < MAXB) { Pic *c = &PC[npc++]; snprintf(c->kind, NM, "%s", f[1]); c->slot = atoi(f[2]); c->r = atoi(f[3]); c->g = atoi(f[4]); c->b = atoi(f[5]); }
        else if (n >= 5 && !strcmp(f[0], "SHADE") && !strcmp(f[1], "pit")) { for (int k = 0; k < 3; k++) SH_PIT[k] = atoi(f[2 + k]); has_pit = 1; }
        else if (n >= 5 && !strcmp(f[0], "SHADE") && !strcmp(f[1], "raise")) { for (int k = 0; k < 3; k++) SH_RAISE[k] = atoi(f[2 + k]); has_raise = 1; }
        else if (n >= 4 && !strcmp(f[0], "PLAYER")) { PL[0] = atoi(f[1]); PL[1] = atoi(f[2]); PL[2] = atoi(f[3]); } }
    fclose(fp);
    snprintf(p, sizeof p, "%s/terrain.pdl", dir);
    if (!(fp = fopen(p, "r"))) return -1;
    while (fgets(l, sizeof l, fp)) { int n = split(l, f, 16);
        if (n >= 8 && !strcmp(f[0], "TERRAIN") && nx < MAXX) { Box *b = &BX[nx++]; b->x1 = atoi(f[1]); b->y1 = atoi(f[2]); b->z1 = atoi(f[3]);
            b->x2 = atoi(f[4]); b->y2 = atoi(f[5]); b->z2 = atoi(f[6]); snprintf(b->kind, NM, "%s", f[7]); } }
    fclose(fp);
    snprintf(p, sizeof p, "%s/terrain.heightmap", dir);   /* OPTIONAL second terrain source (2026-11-25, D137): a generated CONFIG, columns of a whole island */
    if ((fp = fopen(p, "r"))) { int rc = load_heightmap(fp); fclose(fp); if (rc) return -2; }
    return 0;
}
static int R(const char *k) { for (int i = 0; i < nr; i++) if (!strcmp(RU[i].k, k)) return RU[i].v; fprintf(stderr, "missing rule %s\n", k); exit(9); }
static Blk *blk(const char *kind) { for (int i = 0; i < nb; i++) if (!strcmp(BL[i].kind, kind)) return &BL[i]; return NULL; }
static Rcp *rcp(const char *k) { for (int i = 0; i < nrc; i++) if (!strcmp(RC[i].k, k)) return &RC[i]; return NULL; }
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
    { const char *h = hm_kind(x, y, z); if (h) return h; }
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
        else if (n >= 5 && !strcmp(f[0], "MOVE")) { Actor *a = actor(w, f[1], 1); if (a) { if (!a->has) { a->sx = atoi(f[2]); a->sy = atoi(f[3]); a->sz = atoi(f[4]); } a->x = atoi(f[2]); a->y = atoi(f[3]); a->z = atoi(f[4]); a->has = 1; } }
        else if (n >= 5 && !strcmp(f[0], "SPAWNPOINT")) { Actor *a = actor(w, f[1], 1); if (a) { a->sx = atoi(f[2]); a->sy = atoi(f[3]); a->sz = atoi(f[4]); } }
        else if (n >= 3 && !strcmp(f[0], "RESET")) { Actor *a = actor(w, f[1], 1); if (a && a->has) { a->x = a->sx; a->y = a->sy; a->z = a->sz; } }
        else if (n >= 3 && !strcmp(f[0], "FACE")) { Actor *a = actor(w, f[1], 1); if (a) a->dir = f[2][0]; }
        else if (n >= 3 && !strcmp(f[0], "SEAT")) { Actor *a = actor(w, f[1], 1); if (a) a->seat = !strcmp(f[2], "human") ? 1 : 2; }
        else if (n >= 8 && !strcmp(f[0], "DAMAGE")) { int *p = dmg_at(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), 1); if (p) *p = atoi(f[7]); }
        else if (n >= 5 && !strcmp(f[0], "MINE")) { int x = atoi(f[2]), y = atoi(f[3]), z = atoi(f[4]); set_cell(w, x, y, z, "air"); int *p = dmg_at(w, x, y, z, 0); if (p) *p = 0; }
        else if (n >= 4 && !strcmp(f[0], "DROP")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 1) : NULL; if (c) *c += atoi(f[3]); }
        else if (n >= 6 && !strcmp(f[0], "PLACE")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 0) : NULL; if (c) *c -= 1;
            set_cell(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), f[2]); int *p = dmg_at(w, atoi(f[3]), atoi(f[4]), atoi(f[5]), 0); if (p) *p = 0; }
        else if (n >= 5 && !strcmp(f[0], "CRAFT")) { Actor *a = actor(w, f[1], 1); Rcp *r = rcp(f[2]); if (a && r) { int *c = bag(a, r->i1, 0); if (c) *c -= r->n1; if (strcmp(r->i2, "-")) { c = bag(a, r->i2, 0); if (c) *c -= r->n2; } c = bag(a, r->out, 1); if (c) *c += r->on; } }
        else if (n >= 4 && !strcmp(f[0], "EAT")) { Actor *a = actor(w, f[1], 1); int *c = a ? bag(a, f[2], 0) : NULL; if (c) *c -= 1;
            if (a) { a->hunger -= atoi(f[3]); if (a->hunger < 0) a->hunger = 0; } }
        else if (n >= 3 && !strcmp(f[0], "TURN")) { int t = atoi(f[1]), hpt = R("hunger_per_turn"), hmax = R("hunger_max");
            for (int i = 0; i < w->na; i++) if (w->a[i].has) { w->a[i].hunger += (t - w->turn) * hpt; if (w->a[i].hunger > hmax) w->a[i].hunger = hmax; }
            w->turn = t; } }
}
/* hunger rises only on TURN rows (manual turns; no wall clock): every actor that has spawned gains hunger_per_turn per turn, capped at hunger_max; EAT lowers it */
static int food_value(const char *item) {   /* RULE food_<item>; -1 = not food */
    char k[NM + 8]; snprintf(k, sizeof k, "food_%s", item);
    for (int i = 0; i < nr; i++) if (!strcmp(RU[i].k, k)) return RU[i].v;
    return -1;
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
    if (a->hunger >= R("starve_at")) { fprintf(stderr, "refused: starved (hunger %d)\n", a->hunger); return 22; }
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
    if (a->hunger >= R("starve_at")) { fprintf(stderr, "refused: starved (hunger %d)\n", a->hunger); return 22; }
    if (!inb(x, y, z)) { fprintf(stderr, "refused: out of bounds\n"); return 11; }
    if (!in_reach(a, x, y, z, reach)) { fprintf(stderr, "refused: out of reach\n"); return 12; }
    int *c = bag(a, item, 0); if (!c || *c < 1 || !blk(item)) { fprintf(stderr, "refused: item missing or not placeable\n"); return 16; }
    if (solid_at(w, x, y, z) || (a->x == x && a->y == y && a->z == z)) { fprintf(stderr, "refused: target occupied\n"); return 13; }
    if (need) { static const int dd[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}; int sup = 0;
        for (int i = 0; i < 6; i++) if (inb(x + dd[i][0], y + dd[i][1], z + dd[i][2]) && solid_at(w, x + dd[i][0], y + dd[i][1], z + dd[i][2])) sup = 1;
        if (!sup) { fprintf(stderr, "refused: no support\n"); return 15; } }
    char o[512]; snprintf(o, sizeof o, "PLACE | %s | %s | %d | %d | %d\n", an, item, x, y, z); adv(o, w->turn, tm, "place"); commit(L, o); fprintf(OUT, "placed\n"); return 0;
}
static int v_eat(W *w, FILE *L, const char *an, const char *item) {
    int tm = R("turns_eat"); if (!word_ok(an) || !word_ok(item)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    int fv = food_value(item); if (fv < 1) { fprintf(stderr, "refused: not food\n"); return 16; }
    int *c = bag(a, item, 0); if (!c || *c < 1) { fprintf(stderr, "refused: no food in bag\n"); return 16; }
    if (a->hunger < 1) { fprintf(stderr, "refused: not hungry\n"); return 23; }
    char o[512]; snprintf(o, sizeof o, "EAT | %s | %s | %d\n", an, item, fv); adv(o, w->turn, tm, "eat"); commit(L, o);
    int h = a->hunger - fv; if (h < 0) h = 0; h += tm * R("hunger_per_turn"); if (h > R("hunger_max")) h = R("hunger_max"); fprintf(OUT, "ate=%s hunger=%d\n", item, h); return 0;
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

/* ---- the walking verbs (D137, owner 2026-11-25: Asa walks, turns and jumps on the island). Every number is a RULE row: step_up, fall_max, water_max_depth, jump_height (and move_max must be at least 1 + the larger of step_up and fall_max,
 * because a walk onto another level is ONE MOVE row whose manhattan length is 1 + |dy|). ---- */
static int dir_delta(const char *d, int *dx, int *dz) { if (!d[0] || d[1]) return 0; *dx = *dz = 0;
    switch (d[0]) { case 'n': *dz = -1; break; case 's': *dz = 1; break; case 'e': *dx = 1; break; case 'w': *dx = -1; break; default: return 0; } return 1; }
static int top_solid(W *w, int x, int z) { for (int y = R("world_y_max"); y >= 0; y--) if (solid_at(w, x, y, z)) return y; return -1; }   /* highest solid y of a column, -1 = none */
static int water_top(W *w, int x, int z, int from) { for (int y = R("world_y_max"); y >= from; y--) if (!strncmp(kind_at(w, x, y, z), "water", 5)) return y; return -1; }
static int v_face(W *w, FILE *L, const char *an, const char *d) {
    int dx, dz; if (!word_ok(an) || !dir_delta(d, &dx, &dz)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    char o[160]; snprintf(o, sizeof o, "FACE | %s | %s\n", an, d); commit(L, o); fprintf(OUT, "face=%s\n", d); return 0;
}
static int v_walk(W *w, FILE *L, const char *an, const char *d) {
    int dx, dz; if (!word_ok(an) || !dir_delta(d, &dx, &dz)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    int tx = a->x + dx, tz = a->z + dz; if (tx < 0 || tz < 0 || tx > R("world_x_max") || tz > R("world_z_max")) { fprintf(stderr, "refused: out of bounds\n"); return 11; }
    int top = top_solid(w, tx, tz); if (top < 0) { fprintf(stderr, "refused: no ground there\n"); return 11; }
    int ny = top + 1, wl = water_top(w, tx, tz, ny), depth = wl >= 0 ? wl - top : 0;
    if (depth > R("water_max_depth")) { fprintf(stderr, "refused: water %d deep\n", depth); return 18; }
    int up = R("step_up"); if (!strncmp(kind_at(w, a->x, a->y, a->z), "water", 5)) up = R("step_up_wade");   /* climbing out of water may step higher (RULE step_up_wade) */
    int dy = ny - a->y; if (dy > up) { fprintf(stderr, "refused: step up %d\n", dy); return 18; }
    if (-dy > R("fall_max")) { fprintf(stderr, "refused: fall %d\n", -dy); return 18; }
    return v_move(w, L, an, tx, ny, tz);
}
static int v_jump(W *w, FILE *L, const char *an) {
    int jh = R("jump_height"), tm = R("turns_move"); if (!word_ok(an) || jh < 1) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    if (!solid_at(w, a->x, a->y - 1, a->z)) { fprintf(stderr, "refused: no ground under the feet\n"); return 15; }
    for (int k = 1; k <= jh; k++) if (!inb(a->x, a->y + k, a->z) || solid_at(w, a->x, a->y + k, a->z)) { fprintf(stderr, "refused: no room to jump\n"); return 18; }
    if (jh > R("move_max")) { fprintf(stderr, "refused: jump_height above move_max\n"); return 18; }
    char o[512]; snprintf(o, sizeof o, "MOVE | %s | %d | %d | %d\n", an, a->x, a->y + jh, a->z); adv(o, w->turn, tm, "move");
    char t[160]; snprintf(t, sizeof t, "MOVE | %s | %d | %d | %d\n", an, a->x, a->y, a->z); strcat(o, t); adv(o, w->turn + tm, tm, "move");
    commit(L, o); fprintf(OUT, "jumped=%d,%d,%d\n", a->x, a->y, a->z); return 0;   /* BOTH rows or none: one write */
}
static int v_seat(W *w, FILE *L, const char *an, const char *who) {   /* who drives the actor: a ROW (D137, the Teropoly rule: a driver change is a row). The controllers (rmmc_beat_op, rmmc_driver) refuse when it is not them (exit 24); this op only records it */
    if (!word_ok(an) || (strcmp(who, "human") && strcmp(who, "harness"))) return 2; (void)w;
    char o[160]; snprintf(o, sizeof o, "SEAT | %s | %s\n", an, who); commit(L, o); fprintf(OUT, "seat=%s\n", who); return 0;
}
static int v_reset(W *w, FILE *L, const char *an) {
    int tm = R("turns_move"); if (!word_ok(an)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    char o[512]; snprintf(o, sizeof o, "RESET | %s | spawn\n", an); adv(o, w->turn, tm, "move"); commit(L, o); fprintf(OUT, "at=%d,%d,%d\n", a->sx, a->sy, a->sz); return 0;
}
/* intent dispatch shared by replay and verify: derived rows (MINE DROP DAY SPAWNPOINT, TURN other than wait) are regenerated, never replayed */
/* craft: one RECIPE of rules.pdl (inputs from the bag, output into the bag, a station block such as craft_table within RULE craft_reach); the row is CRAFT|actor|recipe|out|n, the fold looks the inputs up again */
static int v_craft(W *w, FILE *L, const char *an, const char *rc) {
    int tm = R("turns_craft"), reach = R("craft_reach"); if (!word_ok(an) || !word_ok(rc)) return 2;
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    Rcp *r = rcp(rc); if (!r) { fprintf(stderr, "refused: no such recipe\n"); return 16; }
    if (strcmp(r->st, "-")) { int found = 0;
        for (int dx = -reach; dx <= reach && !found; dx++) for (int dy = -reach; dy <= reach && !found; dy++) for (int dz = -reach; dz <= reach && !found; dz++) {
            int x = a->x + dx, y = a->y + dy, z = a->z + dz; if (inb(x, y, z) && !strcmp(kind_at(w, x, y, z), r->st) && in_reach(a, x, y, z, reach)) found = 1; }
        if (!found) { fprintf(stderr, "refused: no %s near\n", r->st); return 20; } }
    int *c1 = bag(a, r->i1, 0); if (!c1 || *c1 < r->n1) { fprintf(stderr, "refused: need %d %s\n", r->n1, r->i1); return 16; }
    if (strcmp(r->i2, "-")) { int *c2 = bag(a, r->i2, 0); if (!c2 || *c2 < r->n2) { fprintf(stderr, "refused: need %d %s\n", r->n2, r->i2); return 16; } }
    char o[512]; snprintf(o, sizeof o, "CRAFT | %s | %s | %s | %d\n", an, rc, r->out, r->on); adv(o, w->turn, tm, "craft"); commit(L, o); fprintf(OUT, "crafted=%s n=%d\n", r->out, r->on); return 0;
}
static int intent(FILE *dst, const char *line) {
    char l[512], *f[10]; snprintf(l, sizeof l, "%s", line); int n = split(l, f, 10); W w; fold(&w, dst); int r = 0;
    if (n >= 5 && !strcmp(f[0], "GIVE")) r = v_give(&w, dst, f[1], f[2], atoi(f[3]), f[4]);
    else if (n >= 5 && !strcmp(f[0], "MOVE")) r = v_move(&w, dst, f[1], atoi(f[2]), atoi(f[3]), atoi(f[4]));
    else if (n >= 3 && !strcmp(f[0], "FACE")) r = v_face(&w, dst, f[1], f[2]);
    else if (n >= 3 && !strcmp(f[0], "RESET")) r = v_reset(&w, dst, f[1]);
    else if (n >= 3 && !strcmp(f[0], "SEAT")) r = v_seat(&w, dst, f[1], f[2]);
    else if (n >= 8 && !strcmp(f[0], "DAMAGE")) r = v_mine(&w, dst, f[1], f[2], atoi(f[3]), atoi(f[4]), atoi(f[5]));
    else if (n >= 6 && !strcmp(f[0], "PLACE")) r = v_place(&w, dst, f[1], f[2], atoi(f[3]), atoi(f[4]), atoi(f[5]));
    else if (n >= 3 && !strcmp(f[0], "TURN") && !strcmp(f[2], "wait")) { int k = atoi(f[1]) - w.turn; r = v_wait(&w, dst, k); }
    else if (n >= 6 && !strcmp(f[0], "SLEEP")) r = v_sleep(&w, dst, f[1]);
    else if (n >= 4 && !strcmp(f[0], "EAT")) r = v_eat(&w, dst, f[1], f[2]);
    else if (n >= 5 && !strcmp(f[0], "CRAFT")) r = v_craft(&w, dst, f[1], f[2]);
    return r;
}
static int is_intent(const char *s) {
    char l[512], *f[10]; snprintf(l, sizeof l, "%s", s); int n = split(l, f, 10);
    return n >= 3 && (!strcmp(f[0], "GIVE") || !strcmp(f[0], "MOVE") || !strcmp(f[0], "FACE") || !strcmp(f[0], "RESET") || !strcmp(f[0], "SEAT") || !strcmp(f[0], "DAMAGE") || !strcmp(f[0], "PLACE") || !strcmp(f[0], "SLEEP") || !strcmp(f[0], "EAT") || !strcmp(f[0], "CRAFT") || (!strcmp(f[0], "TURN") && !strcmp(f[2], "wait")));
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

/* ---- projection (D112): the ledger world drawn as a pc-hq DESK, the same files Teario's board reads (cells.txt, cells.marker, cells.rgba, map.txt, extrusion.pdl, skin.txt, events.txt, map.png).
 * A PROJECTION: derived from the fold of the ledger over the seed terrain, safe to delete, rewritten byte for byte (no clock, no random). It decides nothing and writes no ledger row.
 * Axes: desk column = x, desk row = z, voxel y = HEIGHT. `desk` (top-down): each column shows its highest block (tile = its PICTURE slot) and the engine extrudes the column to its real height
 * (map.txt glyph + extrusion.pdl EXTRUDE row, the engine's own height path; a height-1 column needs none), so the 3D picture has real height and a mined column is a visible pit.
 * `layer <y>`: ONE y level as a flat floor (the block at that y, or blank), for the 2D slice. cells.txt rows are `F,Wl` pairs (floor slot, wall slot); the optional depth token is NOT written, so a column is exactly its height tall. ---- */
#define TPX 24
#define ACOLS 16
#define ATILES 43
#define ATW (ACOLS * TPX)
#define ATH (((ATILES + ACOLS - 1) / ACOLS) * TPX)
#define DMAX 216   /* was 64: the first island is 216 columns wide (the engine board limit is 256) */
static unsigned char AT[ATW * ATH * 4];
static int pic_slot(const char *kind) { for (int i = 0; i < npc; i++) if (!strcmp(PC[i].kind, kind) && PC[i].slot > 0 && PC[i].slot < ATILES) return PC[i].slot; return 0; }
static void build_atlas(void) {   /* one flat tile per PICTURE row (a darker 1 px edge and a faint 4 px check so a tile reads as a block); slot 0 stays fully transparent */
    memset(AT, 0, sizeof AT);
    for (int i = 0; i < npc; i++) { int s = PC[i].slot; if (s < 1 || s >= ATILES) continue;
        for (int py = 0; py < TPX; py++) for (int px = 0; px < TPX; px++) {
            int edge = !px || !py || px == TPX - 1 || py == TPX - 1, chk = ((px / 4) + (py / 4)) & 1; int c[3] = { PC[i].r, PC[i].g, PC[i].b };
            unsigned char *o = &AT[(((s / ACOLS) * TPX + py) * ATW + (s % ACOLS) * TPX + px) * 4];
            for (int k = 0; k < 3; k++) { int v = edge ? c[k] * 3 / 4 : c[k] + (chk ? 8 : 0); o[k] = (unsigned char)(v > 255 ? 255 : v); } o[3] = 255; } }
}
static int write_atomic(const char *path, const void *buf, size_t n) {
    char t[1100]; snprintf(t, sizeof t, "%s.tmp", path); FILE *f = fopen(t, "wb"); if (!f) return -1;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return -1; } fclose(f); return rename(t, path);
}
static void marker_bump(const char *dir) {   /* the house marker rule: the engine reloads the desk when this file GROWS (never on mtime); reset only past 4096 bytes, as game_core does */
    char mk[1100]; snprintf(mk, sizeof mk, "%s/cells.marker", dir); struct stat ms; FILE *f = fopen(mk, (!stat(mk, &ms) && ms.st_size >= 4096) ? "w" : "a"); if (f) { fputc('1', f); fclose(f); }
}
static char glyph_of(int h) { static const char g[] = "23456789ABCDEFGH"; return h <= 0 ? '.' : h == 1 ? '1' : g[h - 2]; }
static void top_of(W *w, int x, int z, int ymax, int *top, int *bot) {   /* highest and lowest non-air y of a column, -1 = empty */
    *top = *bot = -1; for (int y = 0; y <= ymax; y++) if (strcmp(kind_at(w, x, y, z), "air")) { if (*bot < 0) *bot = y; *top = y; }
}
static int run_png(const char *png_op, const char *raw, const char *png, int w, int h) {
    char sz[40]; snprintf(sz, sizeof sz, "%dx%d", w, h); fflush(NULL); pid_t p = fork(); if (p < 0) return -1;
    if (!p) { if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) _exit(126); execl(png_op, png_op, raw, png, "--size", sz, (char *)NULL); _exit(127); }
    int st = 0; waitpid(p, &st, 0); return WIFEXITED(st) && !WEXITSTATUS(st) ? 0 : -1;
}
/* layer < 0: top-down heightmap; otherwise the single y level `layer` */
static int project(W *w, const char *dir, int layer, const char *png_op) {
    int WX = R("world_x_max") + 1, WZ = R("world_z_max") + 1, YM = R("world_y_max"); if (WX > DMAX || WZ > DMAX || layer > YM) return 2;
    static int Fs[DMAX][DMAX], Ws[DMAX][DMAX], Hs[DMAX][DMAX]; int solid = 0, hi = 0; build_atlas();
    for (int z = 0; z < WZ; z++) for (int x = 0; x < WX; x++) { int top, bot, f = 0, wl = 0, h = 0;
        if (layer < 0) { top_of(w, x, z, YM, &top, &bot); if (top >= 0) { h = top + 1; if (h == 1) f = pic_slot(kind_at(w, x, top, z)); else { f = pic_slot(kind_at(w, x, top, z)); wl = pic_slot(kind_at(w, x, top - 1, z)); if (!wl) wl = f; } } }
        else { const char *k = kind_at(w, x, layer, z); if (strcmp(k, "air")) { h = 1; f = pic_slot(k); } }
        Fs[z][x] = f; Ws[z][x] = wl; Hs[z][x] = h; if (h) solid++; if (h > hi) hi = h; }
    size_t cap = (size_t)WX * WZ * 16 + 400; char *cb = malloc(cap), *mb = malloc((size_t)(WX + 1) * WZ + 1), ex[1200]; if (!cb || !mb) return 2; size_t n = 0, m = 0;
    n += snprintf(cb + n, cap - n, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=%d\natlas_tiles=%d\nevent_lift=1\ncells\n", WX, WZ, TPX, ACOLS, ATILES);
    for (int z = 0; z < WZ; z++) { for (int x = 0; x < WX; x++) { n += snprintf(cb + n, cap - n, "%s%d,%d", x ? " " : "", Fs[z][x], Ws[z][x]); mb[m++] = glyph_of(Hs[z][x]); } cb[n++] = '\n'; mb[m++] = '\n'; }
    int en = snprintf(ex, sizeof ex, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | note               | vox desk projection (vox_op), derived from the ledger; do not edit\n");
    for (int h = 2; h <= YM + 1 && h <= 16; h++) en += snprintf(ex + en, sizeof ex - en, "EXTRUDE      | %c                  | %d\n", glyph_of(h), h);
    char p[1100]; int bad = 0; mkdir(dir, 0775);
    snprintf(p, sizeof p, "%s/cells.rgba", dir); bad |= write_atomic(p, AT, sizeof AT);
    snprintf(p, sizeof p, "%s/skin.txt", dir); bad |= write_atomic(p, "rmmv\n", 5);
    snprintf(p, sizeof p, "%s/extrusion.pdl", dir); bad |= write_atomic(p, ex, (size_t)en);
    snprintf(p, sizeof p, "%s/map.txt", dir); bad |= write_atomic(p, mb, m);
    snprintf(p, sizeof p, "%s/cells.txt", dir); bad |= write_atomic(p, cb, n);
    { char ev[MAXA * 80 + 8]; size_t en2 = 0; ev[0] = 0; for (int i = 0; i < w->na; i++) if (w->a[i].has) en2 += snprintf(ev + en2, sizeof ev - en2, "%d %d %d %d %d People1 0 2 1\n", w->a[i].x, w->a[i].z, PL[0], PL[1], PL[2]);
      snprintf(p, sizeof p, "%s/events.txt", dir); bad |= write_atomic(p, ev, en2); }
    if (png_op && !bad) {   /* the 2D picture map.png: each cell's visible tile blitted 1:1 (a blank tile over a dark ground), through the house op raw_to_png like game_core desk_picture */
        size_t ow = (size_t)WX * TPX, oh = (size_t)WZ * TPX; unsigned char *o = malloc(ow * oh * 4); if (!o) return 2;
        for (int z = 0; z < WZ; z++) for (int x = 0; x < WX; x++) { int s = Ws[z][x] ? Ws[z][x] : Fs[z][x]; int sh[3] = { 0, 0, 0 };
            if (layer < 0 && Hs[z][x] > 0) {   /* depth cue: pit = lower than EVERY in-bounds neighbour, raise = higher than every one; levels x SHADE delta, added per channel */
                int lo = 1 << 20, hi2 = 1 << 20, hh = Hs[z][x]; static const int DX[4] = { -1, 1, 0, 0 }, DZ[4] = { 0, 0, -1, 1 };
                for (int k = 0; k < 4; k++) { int nx2 = x + DX[k], nz = z + DZ[k]; if (nx2 < 0 || nz < 0 || nx2 >= WX || nz >= WZ) continue; int nh = Hs[nz][nx2]; if (nh - hh < lo) lo = nh - hh; if (hh - nh < hi2) hi2 = hh - nh; }
                if (has_pit && lo > 0 && lo < (1 << 20)) for (int k = 0; k < 3; k++) sh[k] += lo * SH_PIT[k];
                if (has_raise && hi2 > 0 && hi2 < (1 << 20)) for (int k = 0; k < 3; k++) sh[k] += hi2 * SH_RAISE[k]; }
            for (int py = 0; py < TPX; py++) for (int px = 0; px < TPX; px++) { unsigned char *d = &o[(((size_t)z * TPX + py) * ow + (size_t)x * TPX + px) * 4];
                const unsigned char *a = &AT[(((s / ACOLS) * TPX + py) * ATW + (s % ACOLS) * TPX + px) * 4];
                if (s && a[3]) { for (int k = 0; k < 3; k++) { int v = a[k] + sh[k]; d[k] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); } } else { d[0] = 24; d[1] = 24; d[2] = 32; } d[3] = 255; } }
        char raw[1100], pn[1100]; snprintf(raw, sizeof raw, "%s/map.raw", dir); snprintf(pn, sizeof pn, "%s/map.png", dir);
        if (write_atomic(raw, o, ow * oh * 4)) bad = 1; else { if (run_png(png_op, raw, pn, (int)ow, (int)oh)) bad = 1; unlink(raw); } free(o); }
    if (!bad) marker_bump(dir);   /* LAST: the engine must see the whole desk when it reloads */
    free(cb); free(mb); if (bad) { fprintf(stderr, "refused: cannot write the desk %s\n", dir); return 2; }
    fprintf(OUT, "DESK mode=%s %dx%d solid_columns=%d highest=%d\n", layer < 0 ? "top" : "layer", WX, WZ, solid, hi); return 0;
}
/* the game bar feed (state/game_ui.txt shape of game_core: gm_on, gm_title, gmt_i_text, gmb_i_label / _act, counts): a PROJECTION, one text row per fact, one button per verb. A button runs `vox_op act` which appends the verb's rows,
 * then rewrites the desk and this feed. */
/* the popup shows six buttons without scrolling (found by the first live window: Up was off screen, so Up, Place, Up, Place could not be pressed): the building verbs first, then the rest */
static const char *ACTS[][2] = { { "mine", "Mine" }, { "place", "Place" }, { "up", "Up" }, { "down", "Down" }, { "eat", "Eat" }, { "wait", "Wait" }, { "sleep", "Sleep" }, { "x-", "Left" }, { "x+", "Right" }, { "z-", "Back" }, { "z+", "Forward" } };
#define NACT ((int)(sizeof ACTS / sizeof *ACTS))
typedef struct { const char *self, *data, *ledger, *desk, *feed, *png; } Opt;
static int write_feed(W *w, const char *an, const Opt *o) {
    char b[8192]; size_t n = 0; Actor *a = actor(w, an, 0); int mm = minute_of(w->turn);
    n += snprintf(b + n, sizeof b - n, "gm_on=1\ngm_title=Voxel\n");
    n += snprintf(b + n, sizeof b - n, "gmt_0_text=turn %d  day %ld  light %d\n", w->turn, total_min(w->turn) / R("day_minutes"), light_of(mm));
    if (a && a->has) { char bg[600] = ""; for (int i = 0; i < a->ni; i++) if (a->cnt[i]) { char t[80]; snprintf(t, sizeof t, "%s%s %d", bg[0] ? ", " : "", a->item[i], a->cnt[i]); strncat(bg, t, sizeof bg - strlen(bg) - 1); }
        n += snprintf(b + n, sizeof b - n, "gmt_1_text=%s at %d,%d,%d  hunger %d/%d\ngmt_2_text=bag: %s\n", an, a->x, a->y, a->z, a->hunger, R("hunger_max"), bg[0] ? bg : "empty"); }
    else n += snprintf(b + n, sizeof b - n, "gmt_1_text=no player %s yet (give and move first)\ngmt_2_text=bag: empty\n", an);
    for (int i = 0; i < NACT; i++) n += snprintf(b + n, sizeof b - n, "gmb_%d_label=%s\ngmb_%d_act='%s' act '%s' '%s' '%s' '%s' 'desk=%s' 'feed=%s' 'png=%s'\n", i, ACTS[i][1], i, o->self, o->data, o->ledger, an, ACTS[i][0], o->desk, o->feed, o->png ? o->png : "");
    int nt = 3; { char kp[2100], kl[400]; snprintf(kp, sizeof kp, "%s/keys.pdl", o->data); FILE *kf = fopen(kp, "r"); if (kf) { while (fgets(kl, sizeof kl, kf)) if (!strncmp(kl, "LEGEND | ", 9)) { kl[strcspn(kl, "\r\n")] = 0; n += snprintf(b + n, sizeof b - n, "gmt_%d_text=%s\n", nt++, kl + 9); } fclose(kf); } }   /* D115: the key legend is DATA (keys.pdl LEGEND rows); no keys.pdl, no extra line */
    n += snprintf(b + n, sizeof b - n, "gm_nt=%d\ngm_nb=%d\ngm_ne=0\ngm_nl=0\n", nt, NACT);
    return write_atomic(o->feed, b, n);
}
static int act(W *w, FILE *L, const char *an, const char *what) {
    Actor *a = actor(w, an, 0); if (!a || !a->has) { fprintf(stderr, "refused: unknown actor\n"); return 10; }
    static const struct { const char *k; int d[3]; } mv[] = { { "x+", { 1, 0, 0 } }, { "x-", { -1, 0, 0 } }, { "z+", { 0, 0, 1 } }, { "z-", { 0, 0, -1 } }, { "up", { 0, 1, 0 } }, { "down", { 0, -1, 0 } } };
    for (int i = 0; i < 6; i++) if (!strcmp(what, mv[i].k)) return v_move(w, L, an, a->x + mv[i].d[0], a->y + mv[i].d[1], a->z + mv[i].d[2]);
    if (!strcmp(what, "wait")) return v_wait(w, L, 1);
    if (!strcmp(what, "sleep")) return v_sleep(w, L, an);
    if (!strcmp(what, "eat")) { for (int i = 0; i < a->ni; i++) if (a->cnt[i] > 0 && food_value(a->item[i]) > 0) return v_eat(w, L, an, a->item[i]); fprintf(stderr, "refused: no food in bag\n"); return 16; }
    int ty = -1; for (int y = a->y - 1; y >= 0; y--) if (solid_at(w, a->x, y, a->z)) { ty = y; break; }   /* the highest solid block under the player's feet */
    if (!strcmp(what, "mine")) { if (ty < 0) { fprintf(stderr, "refused: nothing to mine\n"); return 13; } Blk *b = blk(kind_at(w, a->x, ty, a->z)); int *c = b ? bag(a, b->tool, 0) : NULL;
        return v_mine(w, L, an, (c && *c > 0) ? b->tool : "hand", a->x, ty, a->z); }
    if (!strcmp(what, "place")) { for (int i = 0; i < a->ni; i++) if (a->cnt[i] > 0 && blk(a->item[i]) && blk(a->item[i])->solid) return v_place(w, L, an, a->item[i], a->x, ty + 1, a->z); fprintf(stderr, "refused: item missing or not placeable\n"); return 16; }
    fprintf(stderr, "refused: unknown action\n"); return 2;
}
/* the window's 3D engine reads cells.txt again when it is sent key 6 (game_core engine_reload does the same for Teario): after a button changed the desk, send it to the session named in <data>/board/state/ui.txt; a data dir without that file (every harness scratch) does nothing */
static void reload3d(const char *data) {
    char p[2048], sess[2048] = "", ln[2100], op[2200]; snprintf(p, sizeof p, "%s/board/state/ui.txt", data); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "bv_session=", 11)) { snprintf(sess, sizeof sess, "%s", ln + 11); sess[strcspn(sess, "\r\n")] = 0; } fclose(f);
    snprintf(op, sizeof op, "%s/../../&.widgits/board-viewer/tools/+x/bv_keys.+x", data); if (!sess[0] || access(sess, R_OK) || access(op, X_OK)) return;
    pid_t c = fork(); if (c == 0) { freopen("/dev/null", "w", stdout); freopen("/dev/null", "w", stderr); execl(op, op, sess, "6", (char *)NULL); _exit(1); } if (c > 0) { int st; waitpid(c, &st, 0); }
}
/* the board hook (D114): a window job writes <ledger dir>/vox_board.pdl (rows `BOARD | desk|feed|png|self|actor | value`); with that file beside the ledger, every ACCEPTED verb called by an event page (or a CLI) rewrites the desk and the feed and nudges the engine, exactly as a window button does through act. No file, no picture, so every harness stays as it was; a refused verb never reaches here. */
static void board_hook(W *w, FILE *L, const char *ledger, const char *data, const char *me) {
    char p[2100], ln[2300], desk[2048] = "", feed[2048] = "", png[2048] = "", self[2048] = "", actor[64] = "player"; snprintf(p, sizeof p, "%s", ledger); char *sl = strrchr(p, '/'); if (!sl) return; snprintf(sl, sizeof p - (size_t)(sl - p), "/vox_board.pdl");
    FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char k[32], v[2048]; if (sscanf(ln, "BOARD | %31[^|] | %2047[^\n]", k, v) != 2) continue; for (char *e = k + strlen(k); e > k && e[-1] == ' '; ) *--e = 0;
        if (!strcmp(k, "desk")) snprintf(desk, sizeof desk, "%s", v); else if (!strcmp(k, "feed")) snprintf(feed, sizeof feed, "%s", v); else if (!strcmp(k, "png")) snprintf(png, sizeof png, "%s", v); else if (!strcmp(k, "self")) snprintf(self, sizeof self, "%s", v); else if (!strcmp(k, "actor")) snprintf(actor, sizeof actor, "%s", v); }
    fclose(f); if (!desk[0]) return;
    Opt o = { self[0] ? self : me, data, ledger, desk, feed, png[0] ? png : NULL };
    fold(w, L); project(w, desk, -1, o.png); if (feed[0]) write_feed(w, actor, &o); reload3d(data);
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
    int r = 2; W w; OUT = stdout; Opt op = { argv[0], argv[2], argv[3], "", "", NULL };
    /* a window button runs the feed's act string and the renderer appends 3 args (instance dir, house root, field value; found by the first live window 2026-11-22): for act, take the key=value options from anywhere and only the first two plain words (actor, action) */
    if (!strcmp(v, "act")) { int k = 0; for (int i = 0; i < ac; i++) { char *o = av[i];
            if (!strncmp(o, "desk=", 5)) op.desk = o + 5; else if (!strncmp(o, "feed=", 5)) op.feed = o + 5; else if (!strncmp(o, "png=", 4)) op.png = o[4] ? o + 4 : NULL; else if (!strncmp(o, "self=", 5)) op.self = o + 5; else av[k++] = o; }
        ac = k > 2 ? 2 : k; }
    else while (ac > 0 && strchr(av[ac - 1], (int)'=')) { char *o = av[ac - 1]; if (!strncmp(o, "desk=", 5)) op.desk = o + 5; else if (!strncmp(o, "feed=", 5)) op.feed = o + 5; else if (!strncmp(o, "png=", 4)) op.png = o[4] ? o + 4 : NULL; else if (!strncmp(o, "self=", 5)) op.self = o + 5; else break; ac--; }
    int mut = !strcmp(v, "give") || !strcmp(v, "move") || !strcmp(v, "mine") || !strcmp(v, "place") || !strcmp(v, "wait") || !strcmp(v, "sleep") || !strcmp(v, "craft") || !strcmp(v, "eat") || !strcmp(v, "act") || !strcmp(v, "walk") || !strcmp(v, "face") || !strcmp(v, "jump") || !strcmp(v, "reset") || (!strcmp(v, "seat") && argc == 6);
    if (mut && (r = verify(L))) { fclose(L); return r; }
    char cx[3][32]; char *av5[5];   /* a page passes the cell as one word x,y,z (the registry allows 4 params) */
    if ((!strcmp(v, "mine") || !strcmp(v, "place")) && ac == 3 && sscanf(av[2], "%31[-0-9],%31[-0-9],%31[-0-9]", cx[0], cx[1], cx[2]) == 3) {
        av5[0] = av[0]; av5[1] = av[1]; av5[2] = cx[0]; av5[3] = cx[1]; av5[4] = cx[2]; av = av5; ac = 5; }
    char *avm[4]; if (!strcmp(v, "move") && ac == 2 && sscanf(av[1], "%31[-0-9],%31[-0-9],%31[-0-9]", cx[0], cx[1], cx[2]) == 3) { avm[0] = av[0]; avm[1] = cx[0]; avm[2] = cx[1]; avm[3] = cx[2]; av = avm; ac = 4; }   /* a page passes the target as one word x,y,z */
    fold(&w, L); r = 2;
    if (!strcmp(v, "give") && ac == 4) r = v_give(&w, L, av[0], av[1], atoi(av[2]), av[3]);
    else if (!strcmp(v, "move") && ac == 4) r = v_move(&w, L, av[0], atoi(av[1]), atoi(av[2]), atoi(av[3]));
    else if (!strcmp(v, "mine") && ac == 5) r = v_mine(&w, L, av[0], av[1], atoi(av[2]), atoi(av[3]), atoi(av[4]));
    else if (!strcmp(v, "place") && ac == 5) r = v_place(&w, L, av[0], av[1], atoi(av[2]), atoi(av[3]), atoi(av[4]));
    else if (!strcmp(v, "wait") && ac == 1) r = v_wait(&w, L, atoi(av[0]));
    else if (!strcmp(v, "sleep") && ac == 1) r = v_sleep(&w, L, av[0]);
    else if (!strcmp(v, "eat") && ac == 2) r = v_eat(&w, L, av[0], av[1]);
    else if (!strcmp(v, "craft") && ac == 2) r = v_craft(&w, L, av[0], av[1]);
    else if (!strcmp(v, "walk") && ac == 2) r = v_walk(&w, L, av[0], av[1]);
    else if (!strcmp(v, "face") && ac == 2) r = v_face(&w, L, av[0], av[1]);
    else if (!strcmp(v, "jump") && ac == 1) r = v_jump(&w, L, av[0]);
    else if (!strcmp(v, "reset") && ac == 1) r = v_reset(&w, L, av[0]);
    else if (!strcmp(v, "seat") && ac == 2) r = v_seat(&w, L, av[0], av[1]);
    else if (!strcmp(v, "seat") && ac == 1) { Actor *a = actor(&w, av[0], 0); fprintf(OUT, "seat=%s\n", a && a->seat == 1 ? "human" : "harness"); r = 0; }   /* no SEAT row = harness */
    else if (!strcmp(v, "ground") && ac == 2) { int x = atoi(av[0]), z = atoi(av[1]); if (!inb(x, 0, z)) { fprintf(stderr, "out of bounds\n"); r = 11; } else { int t = top_solid(&w, x, z), wl = t >= 0 ? water_top(&w, x, z, t + 1) : -1;
        fprintf(OUT, "top=%d kind=%s water=%d\n", t, t >= 0 ? kind_at(&w, x, t, z) : "none", wl); r = 0; } }
    else if (!strcmp(v, "where") && ac == 1) { Actor *a = actor(&w, av[0], 0); if (!a || !a->has) { fprintf(stderr, "unknown actor\n"); r = 10; } else { fprintf(OUT, "at=%d,%d,%d spawn=%d,%d,%d face=%c\n", a->x, a->y, a->z, a->sx, a->sy, a->sz, a->dir ? a->dir : '-'); r = 0; } }
    else if (!strcmp(v, "hunger") && ac == 1) { Actor *a = actor(&w, av[0], 0);
        if (!a) { fprintf(stderr, "unknown actor\n"); r = 10; } else { fprintf(OUT, "hunger=%d max=%d starved=%d\n", a->hunger, R("hunger_max"), a->hunger >= R("starve_at")); r = 0; } }
    else if (!strcmp(v, "status") && ac == 0) {
        int mm = minute_of(w.turn); fprintf(OUT, "turn=%d minute=%d day=%ld light=%d\n", w.turn, mm, total_min(w.turn) / R("day_minutes"), light_of(mm)); r = 0; }
    else if (!strcmp(v, "cell") && ac == 3) { fprintf(OUT, "%s\n", kind_at(&w, atoi(av[0]), atoi(av[1]), atoi(av[2]))); r = 0; }
    else if (!strcmp(v, "inv") && ac == 1) { Actor *a = actor(&w, av[0], 0);
        if (!a) { fprintf(stderr, "unknown actor\n"); r = 10; } else { for (int i = 0; i < a->ni; i++) if (a->cnt[i]) fprintf(OUT, "%s=%d\n", a->item[i], a->cnt[i]); r = 0; } }
    else if (!strcmp(v, "desk") && ac == 1) r = project(&w, av[0], -1, op.png);
    else if (!strcmp(v, "layer") && ac == 2) r = project(&w, av[0], atoi(av[1]), op.png);
    else if (!strcmp(v, "feed") && ac == 1 && op.feed[0]) { r = write_feed(&w, av[0], &op) ? 2 : 0; if (!r) fprintf(OUT, "FEED written\n"); }
    else if (!strcmp(v, "act") && ac == 2) { r = act(&w, L, av[0], av[1]);
        if (!r) { fold(&w, L); if (op.desk[0]) r = project(&w, op.desk, -1, op.png); if (!r && op.feed[0]) r = write_feed(&w, av[0], &op) ? 2 : 0; if (!r && op.desk[0]) reload3d(op.data); } }
    else if (!strcmp(v, "verify") && ac == 0) { r = verify(L); if (!r) fprintf(OUT, "verified\n"); }
    else if (!strcmp(v, "replay") && ac == 1) {
        FILE *d = fopen(av[0], "w+x"); if (!d) { fprintf(stderr, "refused: destination exists or cannot be created\n"); r = 2; } else { FILE *keep = OUT; OUT = fopen("/dev/null", "w"); r = replay_into(L, d); fclose(OUT); OUT = keep; if (!r && !same(L, d)) r = 14; fclose(d); if (!r) fprintf(OUT, "replayed identical\n"); } }
    if (!r && (!strcmp(v, "mine") || !strcmp(v, "place") || !strcmp(v, "craft") || !strcmp(v, "eat") || !strcmp(v, "wait") || !strcmp(v, "sleep") || !strcmp(v, "move") || !strcmp(v, "give") || !strcmp(v, "walk") || !strcmp(v, "jump") || !strcmp(v, "reset") || !strcmp(v, "face"))) board_hook(&w, L, argv[3], argv[2], argv[0]);
    fclose(L); return r;
}
