/* rpg_pvp_hq.c - rpg-pvp-hq step P1: a seeded city (roads, blocks, buildings with doors, interiors, loot, spawns) plus a checker.
 * Design: BOOK 16.game/RPG-PVP-HQ-DESIGN.md (sections 4.1 and 6). Pure file I/O, no X11, no window yet (the board export is the next step).
 *
 *   rpg_pvp_hq gen <seed> [w h]     build a city, write state/city.txt (the map), state/city.pdl (rows), state/city.png (8 px per cell)
 *   rpg_pvp_hq check                re-read the files from disk and test the invariants (exit 1 and a reason on the first failure)
 *   rpg_pvp_hq status               one line about the files that exist
 *   rpg_pvp_hq weapons              the WEAPON rows of tuning.pdl as the op reads them
 *   rpg_pvp_hq view                 (re)write the window files from the city on disk: state/scene.raw + scene.receipt.txt (the canvas) and state/ui.txt (the window's variables)
 * gen / check / status / weapons also refresh the window files, with their one-line result as the window's message. `gen new` picks a seed from the clock.
 * The window is rpg-pvp-hq.xhtpm (shared khtpm_core_render.+x, no per-project renderer code); toys menu entry = toy.pdl + button.sh.
 * Env: RPGPVP_STATE=<dir> overrides the state folder (tests run in a scratch folder this way); default <app>/state.
 *
 * Map glyphs: '.' ground  '=' road  '#' wall  'W' window  'D' door  ',' floor (inside).  Loot and spawns are rows in city.pdl, not glyphs.
 * city.pdl rows:  CITY|seed|w|h|block   BUILDING|id|x|y|w|h|door_x|door_y   LOOT|n|x|y|item|qty   SPAWN|n|x|y
 * The generator uses the same seeded LCG as 007-goldeye (rng_u), so a seed always gives the same bytes.
 */
#define _GNU_SOURCE
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"

#define MAXW 160
#define MAXH 120
#define MAXB 256
#define MAXL 1024
#define MAXS 64
#define CELL_PX 8

static char g_map[MAXH][MAXW + 1];
static int g_w = 64, g_h = 48;
static unsigned g_seed = 1, g_rng = 1;
static char g_state[PATH_MAX], g_app[PATH_MAX];

typedef struct { int x, y, w, h, dx, dy; } Bld;
typedef struct { int x, y, qty; char item[16]; } Loot;
typedef struct { int x, y; } Spawn;
static Bld g_b[MAXB];  static int g_nb;
static Loot g_l[MAXL]; static int g_nl;
static Spawn g_s[MAXS]; static int g_ns;

/* tuning (tuning.pdl); the defaults are the 007-goldeye values so a missing file still works */
static int t_block = 16, t_minb = 5, t_lmin = 1, t_lmax = 3, t_fill = 85;
typedef struct { char id[16]; double dmg, cd, speed, spread, range; int pellets, ammo; } Weapon;
static Weapon g_wp[8]; static int g_nwp;

static unsigned rng_u(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng; }
static int rng_i(int n) { return n <= 0 ? 0 : (int)((rng_u() >> 16) % (unsigned)n); }   /* same LCG as 007-goldeye, high bits: its low bits repeat with a short period */

static void trim(char *s) { char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; }
static int split_bar(char *line, char **f, int max) {
    int n = 0; char *p = line;
    while (n < max) { f[n++] = p; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; }
    for (int i = 0; i < n; i++) { while (*f[i] == ' ' || *f[i] == '\t') f[i]++; trim(f[i]); }
    return n;
}

static void load_tuning(void) {
    char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/tuning.pdl", g_app);
    FILE *f = fopen(p, "r"); if (!f) { g_nwp = 0; return; }
    char line[512], *c[10]; g_nwp = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        int n = split_bar(line, c, 10);
        if (n >= 9 && !strcmp(c[0], "WEAPON") && g_nwp < 8) {
            Weapon *w = &g_wp[g_nwp++]; snprintf(w->id, sizeof w->id, "%s", c[1]);
            w->dmg = atof(c[2]); w->cd = atof(c[3]); w->speed = atof(c[4]); w->pellets = atoi(c[5]); w->spread = atof(c[6]); w->ammo = atoi(c[7]); w->range = atof(c[8]);
        } else if (n >= 6 && !strcmp(c[0], "CITY")) {
            t_block = atoi(c[1]); t_minb = atoi(c[2]); t_lmin = atoi(c[3]); t_lmax = atoi(c[4]); t_fill = atoi(c[5]);
        }
    }
    fclose(f);
    if (t_block < 12) t_block = 12;
    if (t_block > 40) t_block = 40;
    if (t_minb < 4) t_minb = 4;
    if (t_lmin < 0) t_lmin = 0;
    if (t_lmax < t_lmin) t_lmax = t_lmin;
    if (t_fill < 0) t_fill = 0;
    if (t_fill > 100) t_fill = 100;
}

static int ammo_for(const char *item) { for (int i = 0; i < g_nwp; i++) if (!strcmp(g_wp[i].id, item)) return g_wp[i].ammo; return !strcmp(item, "pistol") ? 28 : !strcmp(item, "auto") ? 48 : !strcmp(item, "shotgun") ? 16 : 12; }

static int in_map(int x, int y) { return x >= 0 && y >= 0 && x < g_w && y < g_h; }
static int walkable(char c) { return c == '.' || c == '=' || c == ',' || c == 'D'; }

static void build_one(int x, int y, int w, int h) {
    if (g_nb >= MAXB) return;
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) g_map[j][i] = (i == x || j == y || i == x + w - 1 || j == y + h - 1) ? '#' : ',';
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {          /* windows: some perimeter cells that are not corners */
        int edge = (i == x || j == y || i == x + w - 1 || j == y + h - 1), corner = (i == x || i == x + w - 1) && (j == y || j == y + h - 1);
        if (edge && !corner && rng_i(100) < 22) g_map[j][i] = 'W';
    }
    int side = rng_i(4), dx, dy;                                               /* door: one cell on a random side, never a corner */
    if (side == 0) { dx = x + 1 + rng_i(w - 2); dy = y; }
    else if (side == 1) { dx = x + 1 + rng_i(w - 2); dy = y + h - 1; }
    else if (side == 2) { dx = x; dy = y + 1 + rng_i(h - 2); }
    else { dx = x + w - 1; dy = y + 1 + rng_i(h - 2); }
    g_map[dy][dx] = 'D';
    if (w >= 9) {                                                              /* a partition wall with one gap when the building is wide enough */
        int px = x + w / 2, gap = y + 1 + rng_i(h - 2);
        for (int j = y + 1; j < y + h - 1; j++) g_map[j][px] = (j == gap) ? ',' : '#';
    }
    {                                                                          /* the cell just inside the door must be floor: the partition may not seal the door (the checker caught this) */
        int ix = dx + (dx == x ? 1 : dx == x + w - 1 ? -1 : 0), iy = dy + (dy == y ? 1 : dy == y + h - 1 ? -1 : 0);
        g_map[iy][ix] = ',';
    }
    g_b[g_nb++] = (Bld){ x, y, w, h, dx, dy };
    static const char *kind[] = { "pistol", "pistol", "pistol", "pistol", "auto", "auto", "auto", "shotgun", "shotgun", "ammo", "ammo" };
    int nl = t_lmin + rng_i(t_lmax - t_lmin + 1);
    for (int k = 0; k < nl && g_nl < MAXL; k++) {
        int lx = 0, ly = 0, ok = 0;
        for (int t = 0; t < 40 && !ok; t++) {
            lx = x + 1 + rng_i(w - 2); ly = y + 1 + rng_i(h - 2);
            ok = g_map[ly][lx] == ',';
            for (int q = 0; q < g_nl && ok; q++) if (g_l[q].x == lx && g_l[q].y == ly) ok = 0;
        }
        if (!ok) continue;
        Loot *L = &g_l[g_nl++]; L->x = lx; L->y = ly; snprintf(L->item, sizeof L->item, "%s", kind[rng_i(11)]); L->qty = ammo_for(L->item);
    }
}

static void gen(unsigned seed, int w, int h) {
    g_w = w; g_h = h; g_seed = seed; g_rng = seed ? seed : 1u; g_nb = g_nl = g_ns = 0;
    for (int y = 0; y < g_h; y++) { memset(g_map[y], '.', g_w); g_map[y][g_w] = 0; }
    int B = t_block;
    for (int y = 0; y < g_h; y++) for (int x = 0; x < g_w; x++) if (x % B < 2 || y % B < 2) g_map[y][x] = '=';
    for (int by = 0; by * B + B <= g_h; by++)
        for (int bx = 0; bx * B + B <= g_w; bx++) {
            int x0 = bx * B + 2, y0 = by * B + 2, bw = B - 2, bh = B - 2;       /* the block interior between two road lines */
            if (rng_i(100) >= t_fill) continue;
            int maxw = bw - 2, maxh = bh - 2;                                   /* one cell of ground left on every side so the door opens on walkable ground */
            if (maxw < t_minb || maxh < t_minb) continue;
            int w2 = t_minb + rng_i(maxw - t_minb + 1), h2 = t_minb + rng_i(maxh - t_minb + 1);
            build_one(x0 + 1 + rng_i(maxw - w2 + 1), y0 + 1 + rng_i(maxh - h2 + 1), w2, h2);
        }
    for (int by = 0; by * B < g_h && g_ns < MAXS; by++) for (int bx = 0; bx * B < g_w && g_ns < MAXS; bx++)
        if (rng_i(100) < 50) g_s[g_ns++] = (Spawn){ bx * B, by * B };       /* road crossings */
    if (g_ns == 0) g_s[g_ns++] = (Spawn){ 0, 0 };
}

static int write_atomic(const char *path, const void *data, size_t n) {
    char tmp[PATH_MAX + 24]; snprintf(tmp, sizeof tmp, "%s.tmp%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb"); if (!f) return 0;
    int ok = fwrite(data, 1, n, f) == n; if (fclose(f) != 0) ok = 0;
    if (ok && rename(tmp, path) == 0) return 1;
    unlink(tmp); return 0;
}

static void loot_rgb(const char *item, unsigned char *c) {
    if (!strcmp(item, "pistol")) { c[0] = 255; c[1] = 255; c[2] = 0; }
    else if (!strcmp(item, "auto")) { c[0] = 255; c[1] = 40; c[2] = 40; }
    else if (!strcmp(item, "shotgun")) { c[0] = 255; c[1] = 150; c[2] = 0; }
    else { c[0] = 0; c[1] = 230; c[2] = 230; }
}

static void cell_rgb(char g, unsigned char *c) {
    switch (g) {
        case '=': c[0] = 70; c[1] = 70; c[2] = 78; break;     case '#': c[0] = 150; c[1] = 140; c[2] = 130; break;
        case 'W': c[0] = 120; c[1] = 180; c[2] = 220; break;  case 'D': c[0] = 200; c[1] = 120; c[2] = 40; break;
        case ',': c[0] = 190; c[1] = 175; c[2] = 140; break;  default: c[0] = 60; c[1] = 120; c[2] = 60;
    }
}

static int write_all(void) {
    char p[PATH_MAX + 16]; static char buf[MAXH * (MAXW + 1) + 64];
    mkdir(g_state, 0755);
    size_t n = 0; for (int y = 0; y < g_h; y++) n += (size_t)snprintf(buf + n, sizeof buf - n, "%s\n", g_map[y]);
    snprintf(p, sizeof p, "%s/city.txt", g_state); if (!write_atomic(p, buf, n)) return 0;
    static char rows[MAXB * 80 + MAXL * 64 + MAXS * 32 + 256]; n = 0;
    n += (size_t)snprintf(rows + n, sizeof rows - n, "SECTION | KEY | VALUE\n----------------------------------------\nCITY|%u|%d|%d|%d\n", g_seed, g_w, g_h, t_block);
    for (int i = 0; i < g_nb; i++) n += (size_t)snprintf(rows + n, sizeof rows - n, "BUILDING|b%d|%d|%d|%d|%d|%d|%d\n", i + 1, g_b[i].x, g_b[i].y, g_b[i].w, g_b[i].h, g_b[i].dx, g_b[i].dy);
    for (int i = 0; i < g_nl; i++) n += (size_t)snprintf(rows + n, sizeof rows - n, "LOOT|%d|%d|%d|%s|%d\n", i + 1, g_l[i].x, g_l[i].y, g_l[i].item, g_l[i].qty);
    for (int i = 0; i < g_ns; i++) n += (size_t)snprintf(rows + n, sizeof rows - n, "SPAWN|%d|%d|%d\n", i + 1, g_s[i].x, g_s[i].y);
    snprintf(p, sizeof p, "%s/city.pdl", g_state); if (!write_atomic(p, rows, n)) return 0;
    int W = g_w * CELL_PX, H = g_h * CELL_PX; unsigned char *px = malloc((size_t)W * H * 3); if (!px) return 0;
    for (int y = 0; y < g_h; y++) for (int x = 0; x < g_w; x++) {
        unsigned char c[3]; cell_rgb(g_map[y][x], c);
        for (int j = 0; j < CELL_PX; j++) for (int i = 0; i < CELL_PX; i++) memcpy(px + ((size_t)(y * CELL_PX + j) * W + x * CELL_PX + i) * 3, c, 3);
    }
    for (int k = 0; k < g_nl; k++) { unsigned char c[3]; loot_rgb(g_l[k].item, c);
        for (int j = 2; j < 6; j++) for (int i = 2; i < 6; i++) memcpy(px + ((size_t)(g_l[k].y * CELL_PX + j) * W + g_l[k].x * CELL_PX + i) * 3, c, 3); }
    for (int k = 0; k < g_ns; k++) { static const unsigned char m[3] = { 255, 0, 255 };
        for (int j = 3; j < 5; j++) for (int i = 3; i < 5; i++) memcpy(px + ((size_t)(g_s[k].y * CELL_PX + j) * W + g_s[k].x * CELL_PX + i) * 3, m, 3); }
    snprintf(p, sizeof p, "%s/city.png", g_state);
    char tmp[PATH_MAX + 24]; snprintf(tmp, sizeof tmp, "%s.tmp%d", p, (int)getpid());
    int ok = stbi_write_png(tmp, W, H, 3, px, W * 3); free(px);
    if (ok && rename(tmp, p) == 0) return 1;
    unlink(tmp); return 0;
}

/* ---- the window's files: a fixed 512x384 RGBA canvas (cell size = the largest whole number that fits) + ui.txt ---- */
#define VIEW_W 512
#define VIEW_H 384
static void write_view(const char *msg) {
    static unsigned char fr[VIEW_W * VIEW_H * 4]; char p[PATH_MAX + 24], ui[PATH_MAX * 2 + 600];
    int cell = VIEW_W / g_w < VIEW_H / g_h ? VIEW_W / g_w : VIEW_H / g_h; if (cell < 1) cell = 1;
    int ox = (VIEW_W - g_w * cell) / 2, oy = (VIEW_H - g_h * cell) / 2;
    for (int i = 0; i < VIEW_W * VIEW_H; i++) { fr[i * 4] = 20; fr[i * 4 + 1] = 22; fr[i * 4 + 2] = 28; fr[i * 4 + 3] = 255; }
    for (int y = 0; y < g_h * cell && y + oy < VIEW_H; y++) for (int x = 0; x < g_w * cell && x + ox < VIEW_W; x++) {
        unsigned char c[3]; cell_rgb(g_map[y / cell][x / cell], c); unsigned char *d = fr + ((size_t)(y + oy) * VIEW_W + x + ox) * 4; d[0] = c[0]; d[1] = c[1]; d[2] = c[2];
    }
    for (int k = 0; k < g_nl; k++) { unsigned char c[3]; loot_rgb(g_l[k].item, c); int s0 = cell > 3 ? cell / 4 : 0, s1 = cell > 3 ? cell - cell / 4 : cell;
        for (int j = s0; j < s1; j++) for (int i = s0; i < s1; i++) { unsigned char *d = fr + ((size_t)(oy + g_l[k].y * cell + j) * VIEW_W + ox + g_l[k].x * cell + i) * 4; d[0] = c[0]; d[1] = c[1]; d[2] = c[2]; } }
    for (int k = 0; k < g_ns; k++) { int s0 = cell > 3 ? cell / 3 : 0, s1 = cell > 3 ? cell - cell / 3 : cell;
        for (int j = s0; j < s1; j++) for (int i = s0; i < s1; i++) { unsigned char *d = fr + ((size_t)(oy + g_s[k].y * cell + j) * VIEW_W + ox + g_s[k].x * cell + i) * 4; d[0] = 255; d[1] = 0; d[2] = 255; } }
    mkdir(g_state, 0755);
    snprintf(p, sizeof p, "%s/scene.raw", g_state); write_atomic(p, fr, sizeof fr);
    { char r[64]; int n = snprintf(r, sizeof r, "frame_w=%d\nframe_h=%d\n", VIEW_W, VIEW_H); snprintf(p, sizeof p, "%s/scene.receipt.txt", g_state); write_atomic(p, r, (size_t)n); }
    int n = snprintf(ui, sizeof ui, "title=rpg-pvp-hq  -  seed %u  %dx%d\ncanvas_raw=%s/scene.raw\nmsg=%s\ncity_line=%d buildings, %d loot, %d spawns\n", g_seed, g_w, g_h, g_state, msg && msg[0] ? msg : "ready", g_nb, g_nl, g_ns);
    snprintf(p, sizeof p, "%s/ui.txt", g_state); write_atomic(p, ui, (size_t)n);
}

/* ---- read back from disk (the checker never trusts the in-memory copy) ---- */
static int read_back(char *why, size_t cap) {
    char p[PATH_MAX + 16], line[512], *c[10];
    snprintf(p, sizeof p, "%s/city.pdl", g_state); FILE *f = fopen(p, "r");
    if (!f) { snprintf(why, cap, "no city.pdl in %s (run gen first)", g_state); return 0; }
    g_nb = g_nl = g_ns = 0; int have_city = 0;
    while (fgets(line, sizeof line, f)) {
        int n = split_bar(line, c, 10);
        if (n >= 5 && !strcmp(c[0], "CITY")) { g_seed = (unsigned)strtoul(c[1], 0, 10); g_w = atoi(c[2]); g_h = atoi(c[3]); t_block = atoi(c[4]); have_city = 1; }
        else if (n >= 8 && !strcmp(c[0], "BUILDING") && g_nb < MAXB) g_b[g_nb++] = (Bld){ atoi(c[2]), atoi(c[3]), atoi(c[4]), atoi(c[5]), atoi(c[6]), atoi(c[7]) };
        else if (n >= 6 && !strcmp(c[0], "LOOT") && g_nl < MAXL) { Loot *L = &g_l[g_nl++]; L->x = atoi(c[2]); L->y = atoi(c[3]); snprintf(L->item, sizeof L->item, "%s", c[4]); L->qty = atoi(c[5]); }
        else if (n >= 4 && !strcmp(c[0], "SPAWN") && g_ns < MAXS) g_s[g_ns++] = (Spawn){ atoi(c[2]), atoi(c[3]) };
    }
    fclose(f);
    if (!have_city || g_w < 8 || g_h < 8 || g_w > MAXW || g_h > MAXH) { snprintf(why, cap, "city.pdl has no usable CITY row"); return 0; }
    snprintf(p, sizeof p, "%s/city.txt", g_state); f = fopen(p, "r");
    if (!f) { snprintf(why, cap, "no city.txt"); return 0; }
    for (int y = 0; y < g_h; y++) {
        if (!fgets(line, sizeof line, f)) { snprintf(why, cap, "city.txt has %d rows, CITY says %d", y, g_h); fclose(f); return 0; }
        line[strcspn(line, "\r\n")] = 0;
        if ((int)strlen(line) != g_w) { snprintf(why, cap, "row %d is %d wide, CITY says %d", y, (int)strlen(line), g_w); fclose(f); return 0; }
        memcpy(g_map[y], line, (size_t)g_w + 1);
    }
    if (fgets(line, sizeof line, f) && line[0] != '\n') { snprintf(why, cap, "city.txt has more rows than CITY says"); fclose(f); return 0; }
    fclose(f); return 1;
}

static int check(char *why, size_t cap) {
    if (!read_back(why, cap)) return 0;
    for (int y = 0; y < g_h; y++) for (int x = 0; x < g_w; x++) if (!strchr(".=#WD,", g_map[y][x])) { snprintf(why, cap, "illegal glyph '%c' at %d,%d", g_map[y][x], x, y); return 0; }
    for (int i = 0; i < g_nb; i++) {
        Bld *b = &g_b[i]; int doors = 0;
        if (b->x < 0 || b->y < 0 || b->x + b->w > g_w || b->y + b->h > g_h) { snprintf(why, cap, "building b%d is outside the map", i + 1); return 0; }
        for (int j = 0; j < b->h; j++) for (int k = 0; k < b->w; k++) {
            char c = g_map[b->y + j][b->x + k]; int edge = (j == 0 || k == 0 || j == b->h - 1 || k == b->w - 1);
            if (edge && !strchr("#WD", c)) { snprintf(why, cap, "building b%d wall has '%c' at %d,%d", i + 1, c, b->x + k, b->y + j); return 0; }
            if (!edge && !strchr(",#", c)) { snprintf(why, cap, "building b%d interior has '%c' at %d,%d", i + 1, c, b->x + k, b->y + j); return 0; }
            if (c == 'D') { doors++; if (b->x + k != b->dx || b->y + j != b->dy) { snprintf(why, cap, "building b%d door is not where its row says", i + 1); return 0; } }
        }
        if (doors != 1) { snprintf(why, cap, "building b%d has %d doors (want 1)", i + 1, doors); return 0; }
        for (int q = i + 1; q < g_nb; q++) { Bld *o = &g_b[q];
            if (b->x < o->x + o->w && o->x < b->x + b->w && b->y < o->y + o->h && o->y < b->y + b->h) { snprintf(why, cap, "buildings b%d and b%d overlap", i + 1, q + 1); return 0; } }
    }
    for (int i = 0; i < g_nl; i++) {
        if (!in_map(g_l[i].x, g_l[i].y) || g_map[g_l[i].y][g_l[i].x] != ',') { snprintf(why, cap, "loot %d is not on an inside floor cell", i + 1); return 0; }
        int inside = 0; for (int q = 0; q < g_nb; q++) if (g_l[i].x > g_b[q].x && g_l[i].x < g_b[q].x + g_b[q].w - 1 && g_l[i].y > g_b[q].y && g_l[i].y < g_b[q].y + g_b[q].h - 1) inside = 1;
        if (!inside) { snprintf(why, cap, "loot %d is in no building", i + 1); return 0; }
        for (int q = 0; q < i; q++) if (g_l[q].x == g_l[i].x && g_l[q].y == g_l[i].y) { snprintf(why, cap, "loot %d and %d share a cell", q + 1, i + 1); return 0; }
    }
    if (g_ns < 1) { snprintf(why, cap, "no spawns"); return 0; }
    for (int i = 0; i < g_ns; i++) if (!in_map(g_s[i].x, g_s[i].y) || g_map[g_s[i].y][g_s[i].x] != '=') { snprintf(why, cap, "spawn %d is not on a road", i + 1); return 0; }
    /* reachability: walk from every spawn at once; every door and every loot cell must be reached (a sealed building fails here) */
    static int seen[MAXH][MAXW]; static int qx[MAXH * MAXW], qy[MAXH * MAXW]; int qh = 0, qt = 0;
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < g_ns; i++) { seen[g_s[i].y][g_s[i].x] = 1; qx[qt] = g_s[i].x; qy[qt++] = g_s[i].y; }
    while (qh < qt) { int x = qx[qh], y = qy[qh++]; static const int d[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
        for (int k = 0; k < 4; k++) { int nx = x + d[k][0], ny = y + d[k][1]; if (in_map(nx, ny) && !seen[ny][nx] && walkable(g_map[ny][nx])) { seen[ny][nx] = 1; qx[qt] = nx; qy[qt++] = ny; } } }
    for (int i = 0; i < g_nb; i++) if (!seen[g_b[i].dy][g_b[i].dx]) { snprintf(why, cap, "door of b%d cannot be reached from any spawn", i + 1); return 0; }
    for (int i = 0; i < g_nl; i++) if (!seen[g_l[i].y][g_l[i].x]) { snprintf(why, cap, "loot %d (%s at %d,%d) cannot be reached from any spawn", i + 1, g_l[i].item, g_l[i].x, g_l[i].y); return 0; }
    snprintf(why, cap, "seed %u %dx%d: %d buildings, %d loot, %d spawns; all doors and loot reachable", g_seed, g_w, g_h, g_nb, g_nl, g_ns);
    return 1;
}

int main(int argc, char **argv) {
    char exe[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) { fprintf(stderr, "cannot find myself\n"); return 2; }
    exe[n] = 0;
    for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (!s) return 2; *s = 0; }   /* .../ops/+x/<exe> -> app folder */
    snprintf(g_app, sizeof g_app, "%s", exe);
    const char *e = getenv("RPGPVP_STATE"); snprintf(g_state, sizeof g_state, "%s", e && e[0] ? e : "");
    if (!g_state[0]) snprintf(g_state, sizeof g_state, "%s/state", g_app);
    load_tuning();
    const char *v = argc > 1 ? argv[1] : "status"; char why[400];
    if (!strcmp(v, "gen") && argc >= 3) {
        unsigned seed = !strcmp(argv[2], "new") ? (unsigned)time(NULL) % 100000u : (unsigned)strtoul(argv[2], 0, 10); int w = argc >= 5 ? atoi(argv[3]) : 64, h = argc >= 5 ? atoi(argv[4]) : 48;
        if (w < 20 || h < 20 || w > MAXW || h > MAXH) { fprintf(stderr, "gen: size must be 20..%d x 20..%d\n", MAXW, MAXH); return 2; }
        gen(seed, w, h);
        if (!write_all()) { fprintf(stderr, "gen: could not write %s\n", g_state); return 1; }
        { char m[200]; snprintf(m, sizeof m, "new city: seed %u %dx%d, %d buildings, %d loot", seed, w, h, g_nb, g_nl); write_view(m); }
        printf("gen: seed %u %dx%d: %d buildings, %d loot, %d spawns -> %s\n", seed, w, h, g_nb, g_nl, g_ns, g_state); return 0;
    }
    if (!strcmp(v, "check")) { int ok = check(why, sizeof why); printf("check: %s%s\n", ok ? "ok: " : "FAIL: ", why);
        if (ok || g_nb || g_w >= 8) { char m[460]; snprintf(m, sizeof m, "check %s: %s", ok ? "ok" : "FAILED", why); write_view(m); }
        return ok ? 0 : 1; }
    if (!strcmp(v, "view")) {
        if (!read_back(why, sizeof why)) { gen(1, 64, 48); write_all(); }      /* first run: a default city so the window has something to show */
        write_view("ready"); printf("view: wrote scene.raw, scene.receipt.txt, ui.txt in %s\n", g_state); return 0; }
    if (!strcmp(v, "status")) {
        char p[PATH_MAX + 16]; struct stat st; snprintf(p, sizeof p, "%s/city.pdl", g_state);
        if (stat(p, &st) || !read_back(why, sizeof why)) { printf("status: no city yet (%s)\n", g_state); return 0; }
        printf("status: seed %u %dx%d, %d buildings, %d loot, %d spawns (%s)\n", g_seed, g_w, g_h, g_nb, g_nl, g_ns, g_state); write_view("ready"); return 0;
    }
    if (!strcmp(v, "weapons")) {
        if (!g_nwp) { printf("weapons: no WEAPON rows in %s/tuning.pdl\n", g_app); return 1; }
        if (read_back(why, sizeof why)) { char m[300]; int o = 0; for (int i = 0; i < g_nwp && o < 250; i++) o += snprintf(m + o, sizeof m - o, "%s%s %.0f dmg %.2fs", i ? "  |  " : "", g_wp[i].id, g_wp[i].dmg, g_wp[i].cd); write_view(m); }
        for (int i = 0; i < g_nwp; i++) printf("%-8s dmg %.0f  cd %.2fs  speed %.0f  pellets %d  spread %.2f  pickup_ammo %d  range %.1f\n", g_wp[i].id, g_wp[i].dmg, g_wp[i].cd, g_wp[i].speed, g_wp[i].pellets, g_wp[i].spread, g_wp[i].ammo, g_wp[i].range);
        return 0;
    }
    fprintf(stderr, "usage: rpg_pvp_hq gen <seed> [w h] | check | status | weapons\n"); return 2;
}
