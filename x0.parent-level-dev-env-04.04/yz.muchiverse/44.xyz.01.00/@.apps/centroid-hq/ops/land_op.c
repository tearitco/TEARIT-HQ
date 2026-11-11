#define _GNU_SOURCE
/* land_op — one land generator (2026-11-11). Starts from a blank page.
 *   land_op <blueprint.pdl> <seed.pdl> <cells.txt>
 * seed.pdl holds `SEED | land | <n>` and `SCALE | land | <n>` (plan cells per world cell).
 * Writes cells.txt, map.txt beside it, and land.seed.pdl beside it (the seed that made the page).
 * Biomes, height from the seed, crude roads on the waypoints, a flat frame around each BUILD.
 * The same seed writes the same bytes. A page wider than 512 is refused.
 * Terrain defaults are the first atlas row: 1 grass, 2 dirt path, 3 water, 4 sand, 5 pale sand.
 * Those numbers are read from kits/ next to the blueprint (biome, road, snow, tree, level). A missing kit keeps the default.
 * Slot 33 is a sword-on-wood shop tile. It is not grass. A kit that names 33 is refused.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define GW 72
#define GH 52
#define MAXB 32
#define MAXI 16
#define MAXZ 24
#define MAXW 20

typedef struct { double cx, cy, rx, ry; int top; } Isl;
typedef struct { int ell; double a, b, c, d; int kind; } Zone;
typedef struct { int x0, y0, w, d; } Bld;
typedef struct { double x, y; } Wp;
enum { K_GRASS, K_DIRT, K_STONE, K_PALE, K_FIELD, K_CLIFF };

static Isl isls[MAXI]; static int ni;
static Zone zones[MAXZ]; static int nz;
static Bld blds[MAXB]; static int nb;
static Wp wps[MAXW]; static int nw;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0; return s; }
static int kind_of(const char *s) {
    if (strstr(s, "wheat")) return K_FIELD;
    if (strstr(s, "plaza") || strstr(s, "village")) return K_STONE;
    if (strstr(s, "grove") || strstr(s, "lawn")) return K_PALE;
    if (strstr(s, "cliff")) return K_CLIFF;
    if (strstr(s, "pond")) return K_DIRT;
    return K_GRASS;
}
static int inside_ell(double x, double y, double cx, double cy, double rx, double ry) {
    if (rx < 0.1 || ry < 0.1) return 0;
    double dx = (x - cx) / rx, dy = (y - cy) / ry; return dx * dx + dy * dy <= 1.0;
}
/* Kits live next to the blueprint, in kits/. Missing files keep these defaults, which match the shipped kits. Slot 33 is refused. */
static int slot_grass = 1, slot_path = 2, slot_water = 3, slot_sand = 4, slot_pale = 5, slot_snow = 6;
static int tree_slot = 20, tree_every = 8, road_slot = 2;
static double road_r2 = 1.6 * 1.6;
static int lvl_a = 1, lvl_b = 4, lvl_c = 8;
static int kit_bad = 0;
static int near_seg(double x, double y, double x0, double y0, double x1, double y1) {
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, t = 0;
    if (l2 > 1e-6) { t = ((x - x0) * dx + (y - y0) * dy) / l2; if (t < 0) t = 0; if (t > 1) t = 1; }
    double px = x0 + t * dx - x, py = y0 + t * dy - y;
    return px * px + py * py <= road_r2;
}
static unsigned lcg(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s; }

static int slot_ok(int s) { if (s == 33) { kit_bad = 1; return 0; } return s > 0 && s < 256; }

static void load_one_kit(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return;
    char ln[512];
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '#') continue;
        char *fs[8]; int nf = 0; char *p = ln; fs[nf++] = p;
        for (; *p && nf < 8; p++) if (*p == '|') { *p = 0; fs[nf++] = p + 1; }
        if (nf < 3) continue;
        char *tag = trim(fs[0]);
        if (!strcmp(tag, "BIOME") && nf >= 3) {
            int s = atoi(trim(fs[2])); if (!slot_ok(s)) continue;
            char *id = trim(fs[1]);
            if (!strcmp(id, "grass")) slot_grass = s;
            else if (!strcmp(id, "path")) slot_path = s;
            else if (!strcmp(id, "water")) slot_water = s;
            else if (!strcmp(id, "sand")) slot_sand = s;
            else if (!strcmp(id, "pale")) slot_pale = s;
        } else if (!strcmp(tag, "ROAD") && nf >= 4) {
            int s = atoi(trim(fs[2])); int w = atoi(trim(fs[3]));
            if (slot_ok(s)) road_slot = s;
            if (w < 1) w = 1; if (w > 16) w = 16;
            double r = w * 1.6 / 3.0; road_r2 = r * r;
        } else if (!strcmp(tag, "SNOW") && nf >= 3) {
            int s = atoi(trim(fs[2])); if (slot_ok(s)) slot_snow = s;
        } else if (!strcmp(tag, "TREE") && nf >= 4) {
            int s = atoi(trim(fs[2])); int n = atoi(trim(fs[3]));
            if (slot_ok(s)) tree_slot = s;
            if (n >= 1 && n <= 64) tree_every = n;
        } else if (!strcmp(tag, "LEVEL") && nf >= 3) {
            int h = atoi(trim(fs[2])); if (h < 1) h = 1; if (h > 16) h = 16;
            char *g = trim(fs[1]);
            if (!strcmp(g, "a")) lvl_a = h; else if (!strcmp(g, "b")) lvl_b = h; else if (!strcmp(g, "c")) lvl_c = h;
        }
    }
    fclose(f);
}

static int load_kits(const char *bp) {
    char path[1024];
    snprintf(path, sizeof path, "%s", bp);
    char *sl = strrchr(path, '/'); if (!sl) return 0;
    const char *names[] = { "biome.pdl", "road.pdl", "snow.pdl", "tree.pdl", "level.pdl" };
    for (int i = 0; i < 5; i++) {
        snprintf(sl + 1, sizeof path - (sl + 1 - path), "kits/%s", names[i]);
        load_one_kit(path);
    }
    return kit_bad ? 0 : 1;
}

static int load_bp(const char *path) {
    FILE *bp = fopen(path, "r"); if (!bp) return 0;
    char ln[1024];
    while (fgets(ln, sizeof ln, bp)) {
        if (ln[0] == '#') continue;
        char *f[12]; int nf = 0; char *p = ln; f[nf++] = p;
        for (; *p && nf < 12; p++) if (*p == '|') { *p = 0; f[nf++] = p + 1; }
        if (nf < 2) continue;
        char *tag = trim(f[0]);
        if (!strcmp(tag, "ISLAND") && nf >= 9 && ni < MAXI) {
            Isl *e = &isls[ni++]; e->cx = atof(trim(f[3])); e->cy = atof(trim(f[4])); e->rx = atof(trim(f[5])); e->ry = atof(trim(f[6])); e->top = atoi(trim(f[7]));
        } else if (!strcmp(tag, "ZONE") && nf >= 9 && nz < MAXZ) {
            Zone *z = &zones[nz++]; z->ell = !strcmp(trim(f[4]), "ell"); z->a = atof(trim(f[5])); z->b = atof(trim(f[6])); z->c = atof(trim(f[7])); z->d = atof(trim(f[8])); z->kind = kind_of(trim(f[3]));
        } else if (!strcmp(tag, "BUILD") && nf >= 8 && nb < MAXB) {
            Bld *b = &blds[nb++]; b->x0 = atoi(trim(f[3])); b->y0 = atoi(trim(f[4])); b->w = atoi(trim(f[5])); b->d = atoi(trim(f[6]));
        } else if (!strcmp(tag, "WAYPOINT") && nf >= 6 && nw < MAXW) {
            wps[nw].x = atof(trim(f[3])); wps[nw].y = atof(trim(f[4])); nw++;
        }
    }
    fclose(bp); return ni > 0 && nb > 0;
}
static int load_seed(const char *path, int *scale, unsigned *seed) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char ln[256]; int got = 0;
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '#') continue;
        char *a = ln, *b = NULL, *c = NULL, *p;
        for (p = ln; *p; p++) if (*p == '|') { *p = 0; if (!b) b = p + 1; else if (!c) c = p + 1; }
        if (!b || !c) continue;
        if (!strcmp(trim(a), "SEED")) { *seed = (unsigned)atoi(trim(c)); got |= 1; }
        if (!strcmp(trim(a), "SCALE")) { *scale = atoi(trim(c)); got |= 2; }
    }
    fclose(f); return got == 3;
}

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: land_op <blueprint.pdl> <seed.pdl> <cells.txt>\n"); return 2; }
    int scale = 0; unsigned seed = 0;
    if (!load_seed(argv[2], &scale, &seed)) { fprintf(stderr, "land_op: seed.pdl needs SEED and SCALE\n"); return 2; }
    if (scale < 1 || GW * scale > 512 || GH * scale > 512) { fprintf(stderr, "land_op: scale makes a page past 512\n"); return 2; }
    if (!load_bp(argv[1])) { fprintf(stderr, "land_op: blueprint had no islands or buildings\n"); return 1; }
    if (!load_kits(argv[1])) { fprintf(stderr, "land_op: a kit names slot 33\n"); return 2; }
    int W = GW * scale, H = GH * scale;
    char map[1024], saved[1024];
    snprintf(map, sizeof map, "%s", argv[3]);
    char *sl = strrchr(map, '/'); if (!sl) sl = map; else sl++;
    snprintf(sl, sizeof map - (sl - map), "map.txt");
    snprintf(saved, sizeof saved, "%s", argv[3]);
    sl = strrchr(saved, '/'); if (!sl) sl = saved; else sl++;
    snprintf(sl, sizeof saved - (sl - saved), "land.seed.pdl");
    FILE *cf = fopen(argv[3], "w"); FILE *mf = fopen(map, "w"); FILE *sf = fopen(saved, "w");
    if (!cf || !mf || !sf) { fprintf(stderr, "land_op: cannot write\n"); return 1; }
    fprintf(sf, "SEED | land | %u\nSCALE | land | %d\n", seed, scale);
    char extr[1024]; snprintf(extr, sizeof extr, "%s", argv[3]);
    sl = strrchr(extr, '/'); if (!sl) sl = extr; else sl++;
    snprintf(sl, sizeof extr - (sl - extr), "extrusion.pdl");
    FILE *ef = fopen(extr, "w");
    if (!ef) { fprintf(stderr, "land_op: no extrusion\n"); return 1; }
    /* Depth hangs under a flat top, so it does not make a hill. EXTRUDE on the glyph raises the top. a low, b mid, c high. */
    fprintf(ef, "EXTRUDE | a | %d\nEXTRUDE | b | %d\nEXTRUDE | c | %d\n", lvl_a, lvl_b, lvl_c);
    fclose(ef);
    fprintf(cf, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=43\nrock_slot=36\ncells\n", W, H);
    int land = 0, frames = 0, roads = 0;
    for (int y = 0; y < H; y++) {
        char mline[640]; int ml = 0;
        for (int x = 0; x < W; x++) {
            double px = (x + 0.5) / scale, py = (y + 0.5) / scale;
            int on = -1;
            for (int i = 0; i < ni; i++) if (inside_ell(px, py, isls[i].cx, isls[i].cy, isls[i].rx, isls[i].ry)) on = i;
            int fl = 0, wl = 0, dp = 0; char g = ' ';
            if (on >= 0) {
                int k = K_GRASS;
                for (int z = nz - 1; z >= 0; z--) {
                    int hit = zones[z].ell ? inside_ell(px, py, zones[z].a, zones[z].b, zones[z].c, zones[z].d)
                                           : (px >= zones[z].a && px < zones[z].c && py >= zones[z].b && py < zones[z].d);
                    if (hit) { k = zones[z].kind; break; }
                }
                fl = (k == K_FIELD) ? slot_sand : (k == K_PALE) ? slot_pale : (k == K_STONE) ? slot_path : (k == K_CLIFF) ? slot_path : (k == K_DIRT) ? slot_water : slot_grass;
                int base = 6; if (base > 12) base = 12;
                dp = base;
                int high = (py < isls[on].cy) + (isls[on].top >= 8);
                int road = 0, pad = 0;
                for (int i = 0; i + 1 < nw; i++) if (near_seg(px, py, wps[i].x, wps[i].y, wps[i + 1].x, wps[i + 1].y)) { fl = road_slot; road = 1; roads++; break; }
                for (int i = 0; i < nb; i++) {
                    double x1 = blds[i].x0 + blds[i].w, y1 = blds[i].y0 + blds[i].d;
                    if (px < blds[i].x0 || px >= x1 || py < blds[i].y0 || py >= y1) continue;
                    int edge = (px < blds[i].x0 + 1 || px >= x1 - 1 || py >= y1 - 1 || py < blds[i].y0 + 1);
                    fl = road_slot; wl = edge ? 36 : 0; pad = 1; frames++; break;
                }
                if (!road && !pad && high >= 2) fl = slot_snow;
                if (!road && !pad && fl == slot_grass && ((x / scale + y / scale + (int)seed) % tree_every) == 0 && (x % scale) == 0) wl = tree_slot;
                g = pad || road || high == 0 ? 'a' : (high == 1 ? 'b' : 'c');
                land++;
            }
            fprintf(cf, "%s%d,%d,%d", x ? " " : "", fl, wl, dp);
            if (ml < (int)sizeof mline - 1) mline[ml++] = g;
        }
        fprintf(cf, "\n"); mline[ml] = 0; fprintf(mf, "%s\n", mline);
    }
    fclose(cf); fclose(mf); fclose(sf);
    printf("LAND seed=%u scale=%d %dx%d land=%d frame=%d road=%d\n", seed, scale, W, H, land, frames, roads);
    return (land > 0 && frames > 0 && roads > 0) ? 0 : 1;
}
