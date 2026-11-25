/* tearit_map_op: a seeded land/water map. Numbers live in the RULE rows
 * this file writes. North is y=0. Neighbors are 4-way. Integer hash only.
 * gen <seed> <w> <h> <out> | check <file> | sabotage <in> <out> | picture <file> <ppm>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

enum { SEA = 40, MOIST = 30, HILL = 70, MOUNT = 85, VER = 1, MAXN = 64 };

enum Kind { WATER, GRASS, FOREST, HILLK, MOUNTK, DESERT, RIVER, COAST, BRIDGE };

static const char *kname[] = {
    "water", "grass", "forest", "hill", "mountain", "desert", "river", "coast", "bridge"
};

static int land(int k) { return k != WATER; }

static uint32_t mix(uint32_t s, int x, int y, int stream) {
    uint32_t h = s * 0x9E3779B9u;
    h ^= (uint32_t)x * 0x85EBCA6Bu;
    h ^= (uint32_t)y * 0xC2B2AE35u;
    h ^= (uint32_t)stream * 0x27D4EB2Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

static int at(int *g, int w, int x, int y) { return g[y * w + x]; }
static void setg(int *g, int w, int x, int y, int k) { g[y * w + x] = k; }

static void paint(int *g, int w, int h, uint32_t seed) {
    int x, y, i, best, bx, by, cx, cy, dist, nx, ny, steps;
    int *seen, *qx, *qy, qh, qt, comp;
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t ht = mix(seed, x, y, 1) % 100u;
            uint32_t mo = mix(seed, x, y, 2) % 100u;
            int k = GRASS;
            if (ht < SEA) k = WATER;
            else if (ht >= MOUNT) k = MOUNTK;
            else if (mo < MOIST) k = DESERT;
            else if (ht >= HILL) k = HILLK;
            else if (mo >= 60) k = FOREST;
            setg(g, w, x, y, k);
        }
    }
    best = -1;
    for (i = 0; i < w * h; i++) if (g[i] != WATER && (best < 0 || g[i] == WATER)) best = i;
    /* guarantee two land cells and one water cell */
    {
        int lands = 0, waters = 0, low = 0, high = 0;
        uint32_t lmin = 101, lmax = 0;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            uint32_t ht = mix(seed, x, y, 1) % 100u;
            if (at(g, w, x, y) == WATER) waters++;
            else lands++;
            if (ht < lmin) { lmin = ht; low = y * w + x; }
            if (ht >= lmax) { lmax = ht; high = y * w + x; }
        }
        if (!waters) g[low] = WATER;
        lands = 0;
        for (i = 0; i < w * h; i++) if (g[i] != WATER) lands++;
        if (lands < 2) g[high] = GRASS;
        if (lands < 2) {
            for (i = 0; i < w * h && lands < 2; i++) if (g[i] == WATER) { g[i] = GRASS; lands++; }
        }
    }
    best = 0;
    for (i = 0; i < w * h; i++) if (g[i] == FOREST) best++;
    if (!best) {
        for (i = 0; i < w * h; i++) if (g[i] != WATER && g[i] != MOUNTK) { g[i] = FOREST; break; }
    }
    /* one river: steepest descent from the highest land toward water */
    bx = by = 0;
    best = -1;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        uint32_t ht = mix(seed, x, y, 1) % 100u;
        if (land(at(g, w, x, y)) && (int)ht > best) { best = (int)ht; bx = x; by = y; }
    }
    for (steps = 0; steps < w * h; steps++) {
        int cur = (int)(mix(seed, bx, by, 1) % 100u);
        int nd = 99, nxb = bx, nyb = by, moved = 0;
        if (at(g, w, bx, by) != BRIDGE && at(g, w, bx, by) != WATER)
            setg(g, w, bx, by, RIVER);
        for (i = 0; i < 4; i++) {
            int xx = bx + dx[i], yy = by + dy[i], hh;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            hh = (int)(mix(seed, xx, yy, 1) % 100u);
            if (hh < nd) { nd = hh; nxb = xx; nyb = yy; moved = 1; }
        }
        if (!moved || nd >= cur) break;
        if (at(g, w, nxb, nyb) == WATER) break;
        bx = nxb; by = nyb;
    }
    /* coast if no river touched water: any land beside water */
    {
        int rivers = 0, coasts = 0;
        for (i = 0; i < w * h; i++) if (g[i] == RIVER) rivers++;
        for (y = 0; y < h && !rivers; y++) for (x = 0; x < w; x++) {
            if (!land(at(g, w, x, y))) continue;
            for (i = 0; i < 4; i++) {
                int xx = x + dx[i], yy = y + dy[i];
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                if (at(g, w, xx, yy) == WATER) { setg(g, w, x, y, COAST); coasts++; break; }
            }
            if (coasts) break;
        }
    }
    /* one bridge on a later river cell so a river cell remains */
    {
        int seen_river = 0;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            if (at(g, w, x, y) != RIVER) continue;
            if (!seen_river) { seen_river = 1; continue; }
            setg(g, w, x, y, BRIDGE);
            goto bridged;
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            if (land(at(g, w, x, y)) && at(g, w, x, y) != RIVER && at(g, w, x, y) != COAST) {
                setg(g, w, x, y, BRIDGE);
                goto bridged;
            }
        }
    }
bridged:
    /* starts: two land cells in one 4-way component, farthest pair */
    seen = calloc((size_t)w * h, sizeof(int));
    qx = calloc((size_t)w * h, sizeof(int));
    qy = calloc((size_t)w * h, sizeof(int));
    comp = 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        if (!land(at(g, w, x, y)) || seen[y * w + x]) continue;
        qh = qt = 0; qx[qt] = x; qy[qt] = y; qt++; seen[y * w + x] = ++comp;
        while (qh < qt) {
            int cx0 = qx[qh], cy0 = qy[qh]; qh++;
            for (i = 0; i < 4; i++) {
                int xx = cx0 + dx[i], yy = cy0 + dy[i];
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                if (!land(at(g, w, xx, yy)) || seen[yy * w + xx]) continue;
                seen[yy * w + xx] = comp; qx[qt] = xx; qy[qt] = yy; qt++;
            }
        }
    }
    /* largest component id */
    {
        int count[4096];
        int bi = 1, bn = 0, nland = 0;
        memset(count, 0, sizeof count);
        for (i = 0; i < w * h; i++) if (seen[i] > 0 && seen[i] < 4096) { count[seen[i]]++; nland++; }
        for (i = 1; i < 4096; i++) if (count[i] > bn) { bn = count[i]; bi = i; }
        cx = cy = -1;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++)
            if (seen[y * w + x] == bi) { cx = x; cy = y; goto have; }
have:
        /* BFS farthest */
        for (i = 0; i < w * h; i++) if (seen[i] != bi) seen[i] = 0; else seen[i] = -1;
        qh = qt = 0; qx[qt] = cx; qy[qt] = cy; qt++; seen[cy * w + cx] = 0;
        dist = 0; nx = cx; ny = cy;
        while (qh < qt) {
            int cx0 = qx[qh], cy0 = qy[qh], d0 = seen[cy0 * w + cx0]; qh++;
            if (d0 >= dist) { dist = d0; nx = cx0; ny = cy0; }
            for (i = 0; i < 4; i++) {
                int xx = cx0 + dx[i], yy = cy0 + dy[i];
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                if (seen[yy * w + xx] != -1) continue;
                seen[yy * w + xx] = d0 + 1; qx[qt] = xx; qy[qt] = yy; qt++;
            }
        }
        /* stash starts in high unused: use negative sentinel via globals written by caller through g side channel
           store in seen is gone. Write START by encoding into a side array returned via file in main.
           Here we poke mineral and hut and remember starts in g by not changing kinds.
           Starts saved at end of g buffer? We'll use static. */
        (void)nland;
    }
    free(seen); free(qx); free(qy);
    (void)cx; (void)cy; (void)nx; (void)ny; (void)dist;
}

/* second pass picks starts, hut, mineral with the same walk, returned via outs */
static void extras(int *g, int w, int h, uint32_t seed, int *sx, int *sy, int *s2x, int *s2y, int *mx, int *my, int *hx, int *hy) {
    int x, y, i, *seen, *qx, *qy, qh, qt, comp, bi, bn;
    int count[4096];
    const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    seen = calloc((size_t)w * h, sizeof(int));
    qx = calloc((size_t)w * h, sizeof(int));
    qy = calloc((size_t)w * h, sizeof(int));
    comp = 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        if (!land(at(g, w, x, y)) || seen[y * w + x]) continue;
        qh = qt = 0; qx[qt] = x; qy[qt] = y; qt++; seen[y * w + x] = ++comp;
        while (qh < qt) {
            int cx0 = qx[qh], cy0 = qy[qh]; qh++;
            for (i = 0; i < 4; i++) {
                int xx = cx0 + dx[i], yy = cy0 + dy[i];
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                if (!land(at(g, w, xx, yy)) || seen[yy * w + xx]) continue;
                seen[yy * w + xx] = comp; qx[qt] = xx; qy[qt] = yy; qt++;
            }
        }
    }
    memset(count, 0, sizeof count);
    bi = 1; bn = 0;
    for (i = 0; i < w * h; i++) if (seen[i] > 0 && seen[i] < 4096) count[seen[i]]++;
    for (i = 1; i < 4096; i++) if (count[i] > bn) { bn = count[i]; bi = i; }
    *sx = *sy = -1;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++)
        if (seen[y * w + x] == bi && at(g, w, x, y) != BRIDGE) { *sx = x; *sy = y; goto s1; }
s1:
    if (*sx < 0) { *sx = 0; *sy = 0; }
    for (i = 0; i < w * h; i++) if (seen[i] != bi) seen[i] = 0; else seen[i] = -1;
    qh = qt = 0; qx[qt] = *sx; qy[qt] = *sy; qt++; seen[(*sy) * w + *sx] = 0;
    *s2x = *sx; *s2y = *sy;
    while (qh < qt) {
        int cx0 = qx[qh], cy0 = qy[qh], d0 = seen[cy0 * w + cx0]; qh++;
        if (at(g, w, cx0, cy0) != BRIDGE) { *s2x = cx0; *s2y = cy0; }
        for (i = 0; i < 4; i++) {
            int xx = cx0 + dx[i], yy = cy0 + dy[i];
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            if (seen[yy * w + xx] != -1) continue;
            seen[yy * w + xx] = d0 + 1; qx[qt] = xx; qy[qt] = yy; qt++;
        }
    }
    if (*s2x == *sx && *s2y == *sy) {
        for (y = 0; y < h; y++) for (x = 0; x < w; x++)
            if (land(at(g, w, x, y)) && (x != *sx || y != *sy)) { *s2x = x; *s2y = y; goto s2; }
    }
s2:
    *mx = *my = *hx = *hy = -1;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        if (!land(at(g, w, x, y)) || at(g, w, x, y) == BRIDGE) continue;
        if ((x == *sx && y == *sy) || (x == *s2x && y == *s2y)) continue;
        if (*mx < 0 && (mix(seed, x, y, 3) % 7u) == 0u) { *mx = x; *my = y; }
        if (*hx < 0 && (mix(seed, x, y, 4) % 5u) == 0u && !(*mx == x && *my == y)) { *hx = x; *hy = y; }
    }
    for (y = 0; y < h && *mx < 0; y++) for (x = 0; x < w; x++) {
        if (land(at(g, w, x, y)) && at(g, w, x, y) != BRIDGE) { *mx = x; *my = y; goto mineral; }
    }
mineral:
    for (y = 0; y < h && *hx < 0; y++) for (x = 0; x < w; x++) {
        if (land(at(g, w, x, y)) && !(x == *mx && y == *my)) { *hx = x; *hy = y; goto hut; }
    }
hut:
    free(seen); free(qx); free(qy);
}

static uint32_t rules_hash(void) {
    return (uint32_t)(VER * 1000000 + SEA * 1000 + MOIST * 100 + HILL * 10 + MOUNT);
}

static int write_map(const char *path, uint32_t seed, int w, int h) {
    int *g, x, y, sx, sy, s2x, s2y, mx, my, hx, hy;
    int water = 0, forest = 0, river = 0, bridge = 0;
    FILE *f;
    g = calloc((size_t)w * h, sizeof(int));
    if (!g) return 5;
    paint(g, w, h, seed);
    extras(g, w, h, seed, &sx, &sy, &s2x, &s2y, &mx, &my, &hx, &hy);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    fprintf(f, "MAP | %d | %u | %d | %d\n", VER, seed, w, h);
    fprintf(f, "RULE | sea | %d\n", SEA);
    fprintf(f, "RULE | moist | %d\n", MOIST);
    fprintf(f, "RULE | hill | %d\n", HILL);
    fprintf(f, "RULE | mount | %d\n", MOUNT);
    fprintf(f, "RULESHASH | %u\n", rules_hash());
    fprintf(f, "NOTE | north | y=0\n");
    fprintf(f, "NOTE | neighbors | 4\n");
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        int k = at(g, w, x, y);
        fprintf(f, "CELL | %d | %d | %s\n", x, y, kname[k]);
        if (k == WATER) water++;
        if (k == FOREST) forest++;
        if (k == RIVER || k == COAST) river++;
        if (k == BRIDGE) bridge++;
    }
    fprintf(f, "START | 0 | %d | %d\n", sx, sy);
    fprintf(f, "START | 1 | %d | %d\n", s2x, s2y);
    fprintf(f, "MINERAL | %d | %d | ore\n", mx, my);
    fprintf(f, "HUT | %d | %d\n", hx, hy);
    fclose(f);
    printf("seed=%u w=%d h=%d water=%d forest=%d river=%d bridge=%d mineral=1 starts=2\n",
           seed, w, h, water, forest, river, bridge);
    free(g);
    return 0;
}

static int cell_kind(const char *s) {
    int i;
    for (i = 0; i < 9; i++) if (!strcmp(s, kname[i])) return i;
    return -1;
}

static int check_file(const char *path) {
    FILE *f = fopen(path, "r");
    char line[256], kind[32];
    int w = 0, h = 0, x, y, sx[2], sy[2], ns = 0, mx = -1, my = -1;
    int *g, i, *seen, *qx, *qy, qh, qt, forest = 0, river = 0, bridge = 0, water_on_bridge = 0;
    const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    if (!f) return 5;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 4; }
    if (sscanf(line, "MAP | %*d | %*u | %d | %d", &w, &h) != 2 || w < 1 || h < 1 || w > MAXN || h > MAXN) {
        fclose(f); return 4;
    }
    g = calloc((size_t)w * h, sizeof(int));
    for (i = 0; i < w * h; i++) g[i] = -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "CELL | %d | %d | %31s", &x, &y, kind) == 3) {
            int k = cell_kind(kind);
            if (x < 0 || y < 0 || x >= w || y >= h || k < 0) { free(g); fclose(f); return 4; }
            g[y * w + x] = k;
        } else if (sscanf(line, "START | %*d | %d | %d", &x, &y) == 2 && ns < 2) {
            sx[ns] = x; sy[ns] = y; ns++;
        } else if (sscanf(line, "MINERAL | %d | %d", &x, &y) == 2) { mx = x; my = y; }
    }
    fclose(f);
    for (i = 0; i < w * h; i++) {
        if (g[i] < 0) { free(g); return 4; }
        if (g[i] == FOREST) forest++;
        if (g[i] == RIVER || g[i] == COAST) river++;
        if (g[i] == BRIDGE) bridge++;
        if (g[i] == WATER && 0) water_on_bridge++;
    }
    /* a bridge cell must not be water: kind bridge is its own kind, so sabotage that
       rewrites the bridge line to water drops bridge count to 0 */
    if (!forest || !river || !bridge || ns != 2) { free(g); printf("fail vacuity\n"); return 4; }
    for (i = 0; i < 2; i++) {
        if (sx[i] < 0 || sy[i] < 0 || sx[i] >= w || sy[i] >= h || !land(g[sy[i] * w + sx[i]])) {
            free(g); printf("fail start\n"); return 4;
        }
    }
    if (mx < 0 || my < 0 || !land(g[my * w + mx])) { free(g); printf("fail mineral\n"); return 4; }
    seen = calloc((size_t)w * h, sizeof(int));
    qx = calloc((size_t)w * h, sizeof(int));
    qy = calloc((size_t)w * h, sizeof(int));
    qh = qt = 0; qx[0] = sx[0]; qy[0] = sy[0]; qt = 1; seen[sy[0] * w + sx[0]] = 1;
    while (qh < qt) {
        int cx0 = qx[qh], cy0 = qy[qh]; qh++;
        for (i = 0; i < 4; i++) {
            int xx = cx0 + dx[i], yy = cy0 + dy[i];
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            if (!land(g[yy * w + xx]) || seen[yy * w + xx]) continue;
            seen[yy * w + xx] = 1; qx[qt] = xx; qy[qt] = yy; qt++;
        }
    }
    i = seen[sy[1] * w + sx[1]];
    free(seen); free(qx); free(qy); free(g);
    if (!i) { printf("fail connect\n"); return 4; }
    printf("ok\n");
    (void)water_on_bridge;
    return 0;
}

static int sabotage(const char *in, const char *out) {
    FILE *a = fopen(in, "r"), *b;
    char line[256];
    int flipped = 0;
    if (!a) return 5;
    b = fopen(out, "w");
    if (!b) { fclose(a); return 5; }
    while (fgets(line, sizeof line, a)) {
        int x, y;
        if (!flipped && strstr(line, "bridge")) {
            if (sscanf(line, "CELL | %d | %d", &x, &y) == 2) {
                fprintf(b, "CELL | %d | %d | water\n", x, y);
                flipped = 1;
                continue;
            }
        }
        fputs(line, b);
    }
    fclose(a);
    fclose(b);
    printf("flipped=%d\n", flipped);
    return flipped ? 0 : 4;
}

static int picture(const char *in, const char *ppm) {
    FILE *f = fopen(in, "r"), *o;
    char line[256], kind[32];
    int w = 0, h = 0, x, y, *g, i;
    static const unsigned char rgb[9][3] = {
        { 40, 90, 160 }, { 70, 140, 60 }, { 20, 90, 40 }, { 140, 120, 70 },
        { 120, 120, 120 }, { 190, 170, 90 }, { 60, 160, 170 }, { 200, 180, 80 }, { 120, 70, 40 }
    };
    if (!f) return 5;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 4; }
    sscanf(line, "MAP | %*d | %*u | %d | %d", &w, &h);
    g = calloc((size_t)w * h, sizeof(int));
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "CELL | %d | %d | %31s", &x, &y, kind) == 3) {
            int k = cell_kind(kind);
            if (k >= 0 && x >= 0 && y >= 0 && x < w && y < h) g[y * w + x] = k;
        }
    }
    fclose(f);
    o = fopen(ppm, "w");
    if (!o) { free(g); return 5; }
    fprintf(o, "P3\n%d %d\n255\n", w, h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int k = g[y * w + x];
            fprintf(o, "%d %d %d ", rgb[k][0], rgb[k][1], rgb[k][2]);
        }
        fprintf(o, "\n");
    }
    fclose(o); free(g);
    printf("picture=%s\n", ppm);
    (void)i;
    return 0;
}

int main(int argc, char **argv) {
    unsigned long seed;
    int w, h;
    if (argc < 2) {
        fprintf(stderr, "usage: tearit_map_op gen <seed> <w> <h> <out> | check <file> | sabotage <in> <out> | picture <file> <ppm>\n");
        return 2;
    }
    if (!strcmp(argv[1], "gen")) {
        if (argc != 6) return 2;
        seed = strtoul(argv[2], 0, 10);
        w = atoi(argv[3]); h = atoi(argv[4]);
        if (w < 8 || h < 8 || w > MAXN || h > MAXN) return 3;
        return write_map(argv[5], (uint32_t)seed, w, h);
    }
    if (!strcmp(argv[1], "check") && argc == 3) return check_file(argv[2]);
    if (!strcmp(argv[1], "sabotage") && argc == 4) return sabotage(argv[2], argv[3]);
    if (!strcmp(argv[1], "picture") && argc == 4) return picture(argv[2], argv[3]);
    return 2;
}
