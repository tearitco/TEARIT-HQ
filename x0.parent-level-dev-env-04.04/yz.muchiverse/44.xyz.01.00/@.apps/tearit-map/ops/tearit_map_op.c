/* tearit_map_op: a seeded land/water map. Numbers live in the RULE rows
 * this file writes. North is y=0. Neighbors are 4-way. Integer hash only.
 * gen <seed> <w> <h> <out> | check <file> | sabotage <in> <out> | picture <file> <ppm>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <math.h>
#include <png.h>
#include <setjmp.h>

enum { SEA = 40, MOIST = 30, HILL = 70, MOUNT = 85, VER = 1, MAXN = 64, RATION_CAP = 3, WATER_CAP = 3 };

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
    return (uint32_t)(VER * 1000000 + SEA * 1000 + MOIST * 100 + HILL * 10 + MOUNT + RATION_CAP * 10000000 + WATER_CAP * 100000);
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
    fprintf(f, "RULE | ration | %d\n", RATION_CAP);
    fprintf(f, "RULE | water | %d\n", WATER_CAP);
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

static int mineral_of(const char *path, int *mx, int *my) {
    FILE *f = fopen(path, "r");
    char line[256];
    int x, y;
    *mx = *my = -1;
    if (!f) return 5;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "MINERAL | %d | %d", &x, &y) == 2) { *mx = x; *my = y; }
    }
    fclose(f);
    return *mx < 0 ? 4 : 0;
}

static int ledger_has_dig(const char *ledger, int x, int y) {
    FILE *f;
    char line[256];
    int a, b;
    if (!ledger) return 0;
    f = fopen(ledger, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "DIG | %d | %d", &a, &b) == 2 && a == x && b == y) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static int ledger_has_place(const char *ledger, int x, int y) {
    FILE *f;
    char line[256];
    int a, b, placed = 0;
    if (!ledger) return 0;
    f = fopen(ledger, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "DIG | %d | %d", &a, &b) == 2 && a == x && b == y) placed = 0;
        else if (sscanf(line, "PLACE | %d | %d", &a, &b) == 2 && a == x && b == y) placed = 1;
    }
    fclose(f);
    return placed;
}

static int ledger_has_found(const char *ledger, int x, int y);
static int ration_left(const char *map, const char *ledger, int *cap_out);
static int where(const char *ledger, int sx, int sy, int *x, int *y);
static void agent_at(const char *ledger, int sx, int sy, int *x, int *y);
static int count_rows(const char *ledger, const char *prefix);
/* The player and the ring around them stay clear. Fog is only land a step has not reached. */
static void mark_seen(int *seen, int w, int h, int x, int y) {
    int dy, dx;
    for (dy = -2; dy <= 2; dy++) for (dx = -2; dx <= 2; dx++) {
        int xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        seen[yy * w + xx] = 1;
    }
}

static void pix(unsigned char *p, int x, int y, int r, int g, int b) {
    unsigned char *d;
    if (x < 0 || y < 0 || x > 15 || y > 15) return;
    d = p + (y * 16 + x) * 4;
    d[0] = (unsigned char)r; d[1] = (unsigned char)g; d[2] = (unsigned char)b; d[3] = 255;
}

static void fill_sprite(unsigned char *p, int which) {
    int x, y;
    memset(p, 0, 16 * 16 * 4);
    for (y = 2; y < 14; y++) for (x = 4; x < 12; x++) {
        if (which == 0) pix(p, x, y, 240, 200, 160);
        if (which == 1) pix(p, x, y, 170, 180, 190);
        if (which == 2) pix(p, x, y, 180, 140, 70);
        if (which == 3) pix(p, x, y, 80, 220, 210);
    }
    if (which == 0) { pix(p, 6, 6, 40, 30, 20); pix(p, 9, 6, 40, 30, 20); pix(p, 7, 9, 180, 80, 70); pix(p, 8, 9, 180, 80, 70); }
    if (which == 1) { pix(p, 6, 6, 40, 200, 255); pix(p, 9, 6, 40, 200, 255); pix(p, 6, 10, 40, 40, 50); pix(p, 9, 10, 40, 40, 50); }
    if (which == 2) { for (x = 3; x < 13; x++) pix(p, x, 6, 120, 80, 40); for (y = 3; y < 8; y++) { pix(p, 4, y, 120, 80, 40); pix(p, 11, y, 120, 80, 40); } }
    if (which == 3) { for (x = 6; x < 10; x++) pix(p, x, 4, 230, 255, 255); pix(p, 5, 8, 230, 255, 255); pix(p, 10, 8, 230, 255, 255); }
}

static int write_vox(const char *dir, const char *hex, int which) {
    char path[512];
    unsigned char px[16 * 16 * 4];
    FILE *f;
    int i;
    snprintf(path, sizeof path, "%s/emoji", dir);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/emoji/%s", dir, hex);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/emoji/%s/voxels_16.csv", dir, hex);
    f = fopen(path, "w");
    if (!f) return 5;
    fill_sprite(px, which);
    fputs("# resolution=16\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", f);
    for (i = 0; i < 256; i++)
        fprintf(f, "%d,%d,%d,%d\n", px[i * 4], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
    fclose(f);
    return 0;
}

static unsigned char *load_a5(int *w, int *h) {
    const char *cands[] = {
        "/home/no/Desktop/github/work/NNEST-12.00/#.NNEST_ASSETS/rmmv-www-img/tilesets/Outside_A5.png",
        "#.NNEST_ASSETS/rmmv-www-img/tilesets/Outside_A5.png",
        0
    };
    int i;
    for (i = 0; cands[i]; i++) {
        FILE *fp = fopen(cands[i], "rb");
        png_structp png;
        png_infop info;
        png_bytep *rows;
        unsigned char *img;
        int y, ww, hh;
        if (!fp) continue;
        png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if (!png) { fclose(fp); continue; }
        info = png_create_info_struct(png);
        if (!info || setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png, info ? &info : 0, 0); fclose(fp); continue; }
        png_init_io(png, fp);
        png_read_info(png, info);
        ww = (int)png_get_image_width(png, info);
        hh = (int)png_get_image_height(png, info);
        png_set_expand(png);
        png_set_strip_16(png);
        if (!(png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA)) png_set_filler(png, 0xff, PNG_FILLER_AFTER);
        png_read_update_info(png, info);
        if (png_get_channels(png, info) < 3 || ww < 48 || hh < 48) { png_destroy_read_struct(&png, &info, 0); fclose(fp); continue; }
        rows = calloc((size_t)hh, sizeof(png_bytep));
        img = calloc((size_t)ww * hh, 4);
        if (!rows || !img) { free(rows); free(img); png_destroy_read_struct(&png, &info, 0); fclose(fp); return 0; }
        for (y = 0; y < hh; y++) rows[y] = img + (size_t)y * ww * 4;
        png_read_image(png, rows);
        png_destroy_read_struct(&png, &info, 0);
        fclose(fp);
        free(rows);
        *w = ww; *h = hh;
        return img;
    }
    return 0;
}

static int count_rows(const char *ledger, const char *prefix) {
    FILE *f;
    char line[256];
    int n = 0, k;
    if (!ledger) return 0;
    f = fopen(ledger, "r");
    if (!f) return 0;
    k = (int)strlen(prefix);
    while (fgets(line, sizeof line, f)) if (!strncmp(line, prefix, (size_t)k)) n++;
    fclose(f);
    return n;
}

static int desk(const char *in, const char *dir, const char *ledger) {
    FILE *f = fopen(in, "r");
    char line[256], kind[32], path[512];
    int w = 0, h = 0, x, y, *g, i, sx = 0, sy = 0, s2x = -1, s2y = -1, mx = 0, my = 0;
    static const unsigned char rgb[9][3] = {
        { 40, 90, 160 }, { 70, 140, 60 }, { 20, 90, 40 }, { 140, 120, 70 },
        { 120, 120, 120 }, { 190, 170, 90 }, { 60, 160, 170 }, { 200, 180, 80 }, { 120, 70, 40 }
    };
    static const char glyph[] = "~.T^Ms=c#";
    if (!f) return 5;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 4; }
    if (sscanf(line, "MAP | %*d | %*u | %d | %d", &w, &h) != 2) { fclose(f); return 4; }
    g = calloc((size_t)w * h, sizeof(int));
    for (i = 0; i < w * h; i++) g[i] = -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "CELL | %d | %d | %31s", &x, &y, kind) == 3) {
            int k = cell_kind(kind);
            if (k >= 0 && x >= 0 && y >= 0 && x < w && y < h) g[y * w + x] = k;
        } else if (sscanf(line, "START | 0 | %d | %d", &x, &y) == 2) { sx = x; sy = y; }
        else if (sscanf(line, "START | 1 | %d | %d", &x, &y) == 2) { s2x = x; s2y = y; }
        else if (sscanf(line, "MINERAL | %d | %d", &x, &y) == 2) { mx = x; my = y; }
    }
    fclose(f);
    for (i = 0; i < w * h; i++) if (g[i] < 0) { free(g); return 4; }
    if (mx >= 0 && my >= 0 && mx < w && my < h) {
        g[my * w + mx] = ledger_has_place(ledger, mx, my) ? 13 : ledger_has_dig(ledger, mx, my) ? GRASS : 9;
    }
    if (ledger) {
        for (y = 0; y < h; y++) for (x = 0; x < w; x++)
            if (ledger_has_found(ledger, x, y)) g[y * w + x] = 10;
    }
    int px = sx, py = sy, cap = RATION_CAP, rat = RATION_CAP;
    int ne = 0, exs[8], eys[8], ewhich[8];
    char eid[8][16], enam[8][16], ehex[8][8];
    if (ledger) where(ledger, sx, sy, &px, &py);
    rat = ration_left(in, ledger, &cap);
    if (ledger && rat < cap && g[py * w + px] != 10) g[py * w + px] = 11;
    int ax = s2x, ay = s2y;
    if (ledger && s2x >= 0) agent_at(ledger, s2x, s2y, &ax, &ay);
    if (ledger && ax >= 0 && ay >= 0 && ax < w && ay < h && g[ay * w + ax] != 10 && g[ay * w + ax] != 11)
        g[ay * w + ax] = 12;
    {
        int *seen = calloc((size_t)w * h, sizeof(int));
        int fog = 0;
        FILE *mv;
        if (!seen) { free(g); return 5; }
        mark_seen(seen, w, h, sx, sy);
        if (ledger && (mv = fopen(ledger, "r"))) {
            while (fgets(line, sizeof line, mv)) {
                int a, b;
                if (sscanf(line, "MOVE | %d | %d", &a, &b) == 2) mark_seen(seen, w, h, a, b);
            }
            fclose(mv);
        }
        if (py >= 0 && py < h && px >= 0 && px < w && seen[py * w + px] && ne < 8) {
            snprintf(eid[ne], 16, "seat0"); snprintf(enam[ne], 16, "person"); snprintf(ehex[ne], 8, "1F9D1");
            exs[ne] = px; eys[ne] = py; ewhich[ne] = 0; ne++;
        }
        if (ax >= 0 && ay >= 0 && ay < h && ax < w && seen[ay * w + ax] && ne < 8) {
            snprintf(eid[ne], 16, "seat1"); snprintf(enam[ne], 16, "robot"); snprintf(ehex[ne], 8, "1F916");
            exs[ne] = ax; eys[ne] = ay; ewhich[ne] = 1; ne++;
        }
        if (mx >= 0 && my >= 0 && my < h && mx < w && seen[my * w + mx] && !ledger_has_dig(ledger, mx, my) && !ledger_has_place(ledger, mx, my) && ne < 8) {
            snprintf(eid[ne], 16, "mineral"); snprintf(enam[ne], 16, "gem"); snprintf(ehex[ne], 8, "1F48E");
            exs[ne] = mx; eys[ne] = my; ewhich[ne] = 3; ne++;
        }
        if (ledger) {
            FILE *cf = fopen(ledger, "r");
            char cl[256];
            int ncity = 0, a, b;
            if (cf) {
                while (fgets(cl, sizeof cl, cf)) {
                    if (sscanf(cl, "FOUND | %d | %d", &a, &b) == 2 && a >= 0 && b >= 0 && a < w && b < h && seen[b * w + a] && ne < 8) {
                        snprintf(eid[ne], 16, "city%d", ncity); snprintf(enam[ne], 16, "castle"); snprintf(ehex[ne], 8, "1F3F0");
                        exs[ne] = a; eys[ne] = b; ewhich[ne] = 2; ne++; ncity++;
                    }
                }
                fclose(cf);
            }
        }
        for (i = 0; i < w * h; i++) if (!seen[i]) { g[i] = 14; fog++; }
        free(seen);
        printf("fog=%d cities=%d entities=%d\n", fog, count_rows(ledger, "FOUND |"), ne);
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { free(g); return 5; }
    snprintf(path, sizeof path, "%s/cells.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    fprintf(f, "width=%d\nheight=%d\ntile_px=16\natlas_cols=1\natlas_tiles=22\ncells\n", w, h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int slot = g[y * w + x];
            /* Eight smoke tiles so a fog field is not one stamped square. */
            if (slot == 14) slot = 14 + ((x * 3 + y * 5) & 7);
            fprintf(f, "%s%d,0", x ? " " : "", slot);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/cells.rgba", dir);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    {
        /* Outside_A5.png is 48px tiles, 8 across. Ground kinds sample that tile. Slots 14..21 are smoke, shifted so the field is not one stamp. */
        static const int tile_at[9][2] = { {3,6},{0,2},{0,3},{4,3},{0,7},{1,2},{7,6},{5,8},{0,4} };
        int aw = 0, ah = 0, used = 0, ti;
        unsigned char *a5 = load_a5(&aw, &ah);
        for (ti = 0; ti < 22; ti++) {
            int py, px, tx = 0, ty = 0;
            unsigned char r = ti < 9 ? rgb[ti][0] : (ti == 9 ? 180 : ti == 10 ? 240 : ti == 11 ? 90 : ti == 12 ? 40 : ti == 13 ? 230 : 20);
            unsigned char gg = ti < 9 ? rgb[ti][1] : (ti == 9 ? 40 : ti == 10 ? 200 : ti == 11 ? 40 : ti == 12 ? 80 : ti == 13 ? 230 : 24);
            unsigned char b = ti < 9 ? rgb[ti][2] : (ti == 9 ? 200 : ti == 10 ? 40 : ti == 11 ? 30 : ti == 12 ? 220 : ti == 13 ? 210 : 32);
            if (a5 && ti < 9) { tx = tile_at[ti][0] * 48; ty = tile_at[ti][1] * 48; used = 1; }
            for (py = 0; py < 16; py++) for (px = 0; px < 16; px++) {
                if (a5 && ti < 9 && tx + 47 < aw && ty + 47 < ah) {
                    int sx = tx + px * 3, sy = ty + py * 3;
                    unsigned char *s = a5 + ((size_t)sy * aw + sx) * 4;
                    fputc(s[0], f); fputc(s[1], f); fputc(s[2], f); fputc(255, f);
                } else if (ti >= 14) {
                    int shift = ti - 14, u = (px + shift * 3) & 15, v = (py + shift * 5) & 15;
                    int n = (int)(22.0 * sin(u * 6.2832 / 16.0) + 16.0 * sin(v * 6.2832 / 8.0));
                    int d2 = (u - 7) * (u - 7) + (v - 4) * (v - 4);
                    int wisp = d2 < 10 ? 55 : d2 < 26 ? 28 : 0;
                    int d3 = (u - 12) * (u - 12) + (v - 11) * (v - 11);
                    wisp += d3 < 8 ? 36 : 0;
                    int base = 176 + n + wisp;
                    if (base < 150) base = 150;
                    if (base > 242) base = 242;
                    fputc(base, f); fputc(base + 2 > 255 ? 255 : base + 2, f); fputc(base + 12 > 255 ? 255 : base + 12, f); fputc(255, f);
                } else { fputc(r, f); fputc(gg, f); fputc(b, f); fputc(255, f); }
            }
        }
        free(a5);
        printf("tiles=%s\n", used ? "rtp" : "flat");
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/map.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int k = g[y * w + x];
            fputc(k == 9 ? '*' : k == 10 ? 'C' : k == 11 ? '@' : k == 12 ? 'A' : k == 13 ? '+' : k == 14 ? '?' : glyph[k], f);
        }
        fputc('\n', f);
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/extrusion.pdl", dir);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    fputs("EXTRUDE | ~ | 1\nEXTRUDE | . | 1\nEXTRUDE | T | 2\nEXTRUDE | ^ | 3\nEXTRUDE | M | 5\nEXTRUDE | s | 1\nEXTRUDE | = | 1\nEXTRUDE | c | 1\nEXTRUDE | # | 2\n", f);
    fclose(f);
    snprintf(path, sizeof path, "%s/events.txt", dir);
    f = fopen(path, "w");
    if (!f) { free(g); return 5; }
    {
        int rr, gg, bb;
        rr = 70 + (170 * rat) / (cap > 0 ? cap : 1);
        gg = 70 + (150 * rat) / (cap > 0 ? cap : 1);
        bb = 40 + (40 * rat) / (cap > 0 ? cap : 1);
        fprintf(f, "%d %d %d %d %d block 0 2 1\n", px, py, rr, gg, bb);
    }
    fprintf(f, "%d %d 180 80 200 block 0 2 1\n", mx, my);
    if (ax >= 0 && ay >= 0) fprintf(f, "%d %d 40 80 220 block 0 2 1\n", ax, ay);
    fclose(f);
    snprintf(path, sizeof path, "%s/map.ppm", dir);
    {
        char mapfile[512];
        snprintf(mapfile, sizeof mapfile, "%s", in);
        picture(mapfile, path);
    }
    snprintf(path, sizeof path, "%s/cells.marker", dir);
    {
        /* House marker (owner 2026-10-25): append one byte. The engine reloads when this file's size grows, never on mtime. At 4096 bytes start again so the next size is 1. */
        struct stat ms;
        int n = 0;
        int restart = (stat(path, &ms) == 0 && ms.st_size >= 4096);
        FILE *m = fopen(path, restart ? "w" : "a");
        if (!m) { free(g); return 5; }
        fputc('1', m);
        fclose(m);
        if (stat(path, &ms) == 0) n = (int)ms.st_size;
        printf("desk=%s marker=%d\n", dir, n);
        snprintf(path, sizeof path, "%s/entities.pdl", dir);
        m = fopen(path, "w");
        if (m) {
            int ei;
            for (ei = 0; ei < ne; ei++) {
                fprintf(m, "ENTITY | %s | %s | %s | %d | %d\n", eid[ei], enam[ei], ehex[ei], exs[ei], eys[ei]);
                write_vox(dir, ehex[ei], ewhich[ei]);
            }
            fclose(m);
        }
        snprintf(path, sizeof path, "%s/tiles.pdl", dir);
        m = fopen(path, "w");
        if (m) {
            fputs("TILE | water | Outside_A5.png | 3 | 6\nTILE | grass | Outside_A5.png | 0 | 2\nTILE | forest | Outside_A5.png | 0 | 3\nTILE | hill | Outside_A5.png | 4 | 3\nTILE | mountain | Outside_A5.png | 0 | 7\nTILE | desert | Outside_A5.png | 1 | 2\nTILE | river | Outside_A5.png | 7 | 6\nTILE | coast | Outside_A5.png | 5 | 8\nTILE | bridge | Outside_A5.png | 0 | 4\n", m);
            fclose(m);
        }
    }
    free(g);
    return 0;
}

static int load_map(const char *path, int **g, int *w, int *h, int *sx, int *sy) {
    FILE *f = fopen(path, "r");
    char line[256], kind[32];
    int x, y, i;
    if (!f) return 5;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 4; }
    if (sscanf(line, "MAP | %*d | %*u | %d | %d", w, h) != 2) { fclose(f); return 4; }
    *g = calloc((size_t)(*w) * (*h), sizeof(int));
    for (i = 0; i < (*w) * (*h); i++) (*g)[i] = -1;
    *sx = *sy = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "CELL | %d | %d | %31s", &x, &y, kind) == 3) {
            int k = cell_kind(kind);
            if (k >= 0 && x >= 0 && y >= 0 && x < *w && y < *h) (*g)[y * (*w) + x] = k;
        } else if (sscanf(line, "START | 0 | %d | %d", &x, &y) == 2) { *sx = x; *sy = y; }
    }
    fclose(f);
    for (i = 0; i < (*w) * (*h); i++) if ((*g)[i] < 0) return 4;
    return 0;
}

static int where(const char *ledger, int sx, int sy, int *x, int *y) {
    FILE *f = fopen(ledger, "r");
    char line[256];
    int a, b;
    *x = sx; *y = sy;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "MOVE | %d | %d", &a, &b) == 2) { *x = a; *y = b; }
    }
    fclose(f);
    return 0;
}

static int move_op(const char *map, const char *ledger, const char *which) {
    int *g, w, h, sx, sy, x, y, i, rc, nx, ny, found = 0;
    const int dx[4] = { 1, 0, -1, 0 };
    const int dy[4] = { 0, 1, 0, -1 };
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    for (i = 0; i < 4; i++) {
        int xx = x + dx[i], yy = y + dy[i];
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        if (!strcmp(which, "land") && land(g[yy * w + xx])) { nx = xx; ny = yy; found = 1; break; }
        if (!strcmp(which, "water") && g[yy * w + xx] == WATER) { nx = xx; ny = yy; found = 1; break; }
    }
    if (!found) { free(g); printf("fail no-neighbor\n"); return 6; }
    if (!land(g[ny * w + nx])) {
        free(g);
        printf("fail water\n");
        return 4;
    }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "MOVE | %d | %d | %s\n", nx, ny, kname[g[ny * w + nx]]);
    fclose(f);
    printf("move=%d,%d\n", nx, ny);
    free(g);
    return 0;
}

static int step_op(const char *map, const char *ledger, const char *dir) {
    int *g, w, h, sx, sy, x, y, rc, dx = 0, dy = 0, nx, ny;
    FILE *f;
    if (!strcmp(dir, "e")) dx = 1;
    else if (!strcmp(dir, "s")) dy = 1;
    else if (!strcmp(dir, "w")) dx = -1;
    else if (!strcmp(dir, "n")) dy = -1;
    else { printf("fail dir\n"); return 2; }
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    nx = x + dx; ny = y + dy;
    if (nx < 0 || ny < 0 || nx >= w || ny >= h) { free(g); printf("fail edge\n"); return 4; }
    if (!land(g[ny * w + nx])) { free(g); printf("fail water\n"); return 4; }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "MOVE | %d | %d | %s\n", nx, ny, dir);
    fclose(f);
    printf("step=%d,%d %s\n", nx, ny, dir);
    free(g);
    return 0;
}

static int approach_mineral(const char *map, const char *ledger) {
    int *g, w, h, sx, sy, x, y, mx, my, rc, i, n = 0;
    int *seen, *qx, *qy, *px, *py, qh, qt;
    const int dx[4] = { 1, 0, -1, 0 };
    const int dy[4] = { 0, 1, 0, -1 };
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    if (mineral_of(map, &mx, &my)) { free(g); return 4; }
    where(ledger, sx, sy, &x, &y);
    seen = calloc((size_t)w * h, sizeof(int));
    qx = calloc((size_t)w * h, sizeof(int));
    qy = calloc((size_t)w * h, sizeof(int));
    px = calloc((size_t)w * h, sizeof(int));
    py = calloc((size_t)w * h, sizeof(int));
    qh = qt = 0;
    qx[qt] = x; qy[qt] = y; qt++;
    seen[y * w + x] = 1;
    px[y * w + x] = x; py[y * w + x] = y;
    while (qh < qt) {
        int cx0 = qx[qh], cy0 = qy[qh];
        qh++;
        if (cx0 == mx && cy0 == my) break;
        for (i = 0; i < 4; i++) {
            int xx = cx0 + dx[i], yy = cy0 + dy[i];
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            if (!land(g[yy * w + xx]) || seen[yy * w + xx]) continue;
            seen[yy * w + xx] = 1;
            px[yy * w + xx] = cx0; py[yy * w + xx] = cy0;
            qx[qt] = xx; qy[qt] = yy; qt++;
        }
    }
    if (!seen[my * w + mx]) { free(seen); free(qx); free(qy); free(px); free(py); free(g); printf("fail no-path\n"); return 6; }
    f = fopen(ledger, "a");
    if (!f) { free(seen); free(qx); free(qy); free(px); free(py); free(g); return 5; }
    {
        int pathx[4096], pathy[4096], cx0 = mx, cy0 = my, nn = 0;
        while (!(cx0 == x && cy0 == y) && nn < 4096) {
            pathx[nn] = cx0; pathy[nn] = cy0; nn++;
            { int nx0 = px[cy0 * w + cx0], ny0 = py[cy0 * w + cx0]; cx0 = nx0; cy0 = ny0; }
        }
        for (i = nn - 1; i >= 0; i--) fprintf(f, "MOVE | %d | %d | step\n", pathx[i], pathy[i]);
        n = nn;
    }
    fclose(f);
    printf("approach=%d steps=%d\n", 1, n);
    free(seen); free(qx); free(qy); free(px); free(py); free(g);
    return 0;
}

static int ledger_has_found(const char *ledger, int x, int y) {
    FILE *f = fopen(ledger, "r");
    char line[256];
    int a, b;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "FOUND | %d | %d", &a, &b) == 2 && a == x && b == y) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static int found_op(const char *map, const char *ledger) {
    int *g, w, h, sx, sy, x, y, rc;
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    if (!land(g[y * w + x]) || ledger_has_found(ledger, x, y)) {
        free(g); printf("fail found\n"); return 4;
    }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "FOUND | %d | %d | %s\n", x, y, kname[g[y * w + x]]);
    fclose(f);
    printf("found=%d,%d\n", x, y);
    free(g);
    return 0;
}

static int ration_cap_of(const char *map) {
    FILE *f = fopen(map, "r");
    char line[256];
    int n = RATION_CAP, v;
    if (!f) return n;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "RULE | ration | %d", &v) == 1 && v > 0 && v < 100) n = v;
    }
    fclose(f);
    return n;
}

static int ration_left(const char *map, const char *ledger, int *cap_out) {
    FILE *f;
    char line[256];
    int cap = ration_cap_of(map), eat = 0, sleep = 0, n;
    if (cap_out) *cap_out = cap;
    if (!ledger) return cap;
    f = fopen(ledger, "r");
    if (!f) return cap;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "EAT |", 5)) eat++;
        else if (!strncmp(line, "SLEEP |", 7)) sleep++;
    }
    fclose(f);
    n = cap - eat + sleep;
    if (n < 0) n = 0;
    if (n > cap) n = cap;
    return n;
}

static int body_op(const char *map, const char *ledger, int eat) {
    int *g, w, h, sx, sy, x, y, rc, left, cap = 0;
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    if (!land(g[y * w + x])) { free(g); printf("fail water\n"); return 4; }
    left = ration_left(map, ledger, &cap);
    if (eat && left <= 0) { free(g); printf("fail ration\n"); return 4; }
    if (!eat && left >= cap) { free(g); printf("fail rested\n"); return 4; }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "%s | %d | %d\n", eat ? "EAT" : "SLEEP", x, y);
    fclose(f);
    left = ration_left(map, ledger, &cap);
    printf("%s=%d,%d ration=%d cap=%d\n", eat ? "ate" : "slept", x, y, left, cap);
    free(g);
    return 0;
}

static void agent_at(const char *ledger, int sx, int sy, int *x, int *y) {
    FILE *f;
    char line[256];
    int a, b;
    *x = sx;
    *y = sy;
    if (!ledger) return;
    f = fopen(ledger, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "AGENT | %d | %d", &a, &b) == 2) { *x = a; *y = b; }
    }
    fclose(f);
}

static int start_n(const char *map, int which, int *x, int *y) {
    FILE *f = fopen(map, "r");
    char line[256];
    int n, a, b;
    if (!f) return 5;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "START | %d | %d | %d", &n, &a, &b) == 3 && n == which) {
            *x = a; *y = b; fclose(f); return 0;
        }
    }
    fclose(f);
    return 4;
}

static int agent_op(const char *map, const char *ledger, const char *mood) {
    int *g, w, h, sx, sy, hx, hy, ax, ay, s2x, s2y, i, rc, nx = 0, ny = 0, found = 0;
    const int near_dx[4] = { 1, 0, -1, 0 };
    const int near_dy[4] = { 0, 1, 0, -1 };
    const int far_dx[4] = { 0, -1, 0, 1 };
    const int far_dy[4] = { -1, 0, 1, 0 };
    const int *dx, *dy;
    FILE *f;
    if (!strcmp(mood, "near")) { dx = near_dx; dy = near_dy; }
    else if (!strcmp(mood, "far")) { dx = far_dx; dy = far_dy; }
    else { printf("fail mood\n"); return 2; }
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    if (start_n(map, 1, &s2x, &s2y)) { free(g); printf("fail start\n"); return 4; }
    where(ledger, sx, sy, &hx, &hy);
    agent_at(ledger, s2x, s2y, &ax, &ay);
    for (i = 0; i < 4; i++) {
        int xx = ax + dx[i], yy = ay + dy[i];
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        if (!land(g[yy * w + xx])) continue;
        if (xx == hx && yy == hy) continue;
        nx = xx; ny = yy; found = 1; break;
    }
    if (!found) { free(g); printf("fail no-step\n"); return 4; }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "AGENT | %d | %d | %s\n", nx, ny, mood);
    fclose(f);
    printf("agent=%d,%d %s\n", nx, ny, mood);
    free(g);
    return 0;
}

static int dig_op(const char *map, const char *ledger) {
    int *g, w, h, sx, sy, x, y, mx, my, rc;
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    if (mineral_of(map, &mx, &my)) { free(g); return 4; }
    where(ledger, sx, sy, &x, &y);
    if (x != mx || y != my || ledger_has_dig(ledger, mx, my)) {
        free(g); printf("fail dig\n"); return 4;
    }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "DIG | %d | %d\n", mx, my);
    fclose(f);
    printf("dug=%d,%d\n", mx, my);
    free(g);
    return 0;
}

static uint32_t fold_line(uint32_t h, const char *s) {
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static uint32_t ledger_hash(const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    uint32_t h = 2166136261u;
    if (!f) return h;
    while (fgets(line, sizeof line, f)) h = fold_line(h, line);
    fclose(f);
    return h;
}

static int checkpoint_op(const char *ledger) {
    uint32_t h = ledger_hash(ledger);
    FILE *f = fopen(ledger, "a");
    if (!f) return 5;
    fprintf(f, "CHECK | %u\n", h);
    fclose(f);
    printf("check=%u\n", h);
    return 0;
}

static int verify_op(const char *ledger) {
    FILE *f = fopen(ledger, "r");
    char line[512];
    uint32_t h = 2166136261u;
    int checks = 0;
    unsigned got;
    if (!f) return 5;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "CHECK | %u", &got) == 1) {
            if (got != h) { fclose(f); printf("fail check\n"); return 4; }
            checks++;
        }
        h = fold_line(h, line);
    }
    fclose(f);
    if (!checks) { printf("fail none\n"); return 4; }
    printf("checks=%d\n", checks);
    return 0;
}

static int alter_op(const char *in, const char *out) {
    FILE *f = fopen(in, "r");
    FILE *o;
    char line[512];
    int flipped = 0;
    if (!f) return 5;
    o = fopen(out, "w");
    if (!o) { fclose(f); return 5; }
    while (fgets(line, sizeof line, f)) {
        int a, b;
        char rest[64];
        if (!flipped && sscanf(line, "MOVE | %d | %d | %63s", &a, &b, rest) == 3) {
            fprintf(o, "MOVE | %d | %d | %s\n", a + 1, b, rest);
            flipped = 1;
        } else fputs(line, o);
    }
    fclose(f);
    fclose(o);
    if (!flipped) { printf("fail alter\n"); return 4; }
    printf("altered=1\n");
    return 0;
}

static int water_cap_of(const char *map) {
    FILE *f = fopen(map, "r");
    char line[256];
    int n = WATER_CAP, v;
    if (!f) return n;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "RULE | water | %d", &v) == 1 && v > 0 && v < 100) n = v;
    }
    fclose(f);
    return n;
}

static int beside_water(const int *g, int w, int h, int x, int y) {
    const int dx[4] = { 1, 0, -1, 0 };
    const int dy[4] = { 0, 1, 0, -1 };
    int i;
    for (i = 0; i < 4; i++) {
        int xx = x + dx[i], yy = y + dy[i], k;
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        k = g[yy * w + xx];
        if (k == WATER || k == RIVER) return 1;
    }
    return 0;
}

static int draw_op(const char *map, const char *ledger) {
    int *g, w, h, sx, sy, x, y, rc, cap, stock;
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    cap = water_cap_of(map);
    stock = count_rows(ledger, "DRAW |");
    if (!land(g[y * w + x]) || !beside_water(g, w, h, x, y)) {
        free(g); printf("fail dry\n"); return 4;
    }
    if (stock >= cap) { free(g); printf("fail full\n"); return 4; }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "DRAW | %d | %d\n", x, y);
    fclose(f);
    printf("stock=%d cap=%d\n", stock + 1, cap);
    free(g);
    return 0;
}

static int place_op(const char *map, const char *ledger) {
    int *g, w, h, sx, sy, x, y, rc;
    FILE *f;
    rc = load_map(map, &g, &w, &h, &sx, &sy);
    if (rc) return rc;
    where(ledger, sx, sy, &x, &y);
    if (!land(g[y * w + x]) || !ledger_has_dig(ledger, x, y) || ledger_has_place(ledger, x, y)) {
        free(g); printf("fail place\n"); return 4;
    }
    f = fopen(ledger, "a");
    if (!f) { free(g); return 5; }
    fprintf(f, "PLACE | %d | %d | block\n", x, y);
    fclose(f);
    printf("placed=%d,%d\n", x, y);
    free(g);
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
    if (!strcmp(argv[1], "desk") && argc == 4) return desk(argv[2], argv[3], NULL);
    if (!strcmp(argv[1], "desk") && argc == 5) return desk(argv[2], argv[3], argv[4]);
    if (!strcmp(argv[1], "move") && argc == 5) return move_op(argv[2], argv[3], argv[4]);
    if (!strcmp(argv[1], "step") && argc == 5) return step_op(argv[2], argv[3], argv[4]);
    if (!strcmp(argv[1], "approach") && argc == 4) return approach_mineral(argv[2], argv[3]);
    if (!strcmp(argv[1], "dig") && argc == 4) return dig_op(argv[2], argv[3]);
    if (!strcmp(argv[1], "place") && argc == 4) return place_op(argv[2], argv[3]);
    if (!strcmp(argv[1], "draw") && argc == 4) return draw_op(argv[2], argv[3]);
    if (!strcmp(argv[1], "checkpoint") && argc == 3) return checkpoint_op(argv[2]);
    if (!strcmp(argv[1], "verify") && argc == 3) return verify_op(argv[2]);
    if (!strcmp(argv[1], "alter") && argc == 4) return alter_op(argv[2], argv[3]);
    if (!strcmp(argv[1], "found") && argc == 4) return found_op(argv[2], argv[3]);
    if (!strcmp(argv[1], "eat") && argc == 4) return body_op(argv[2], argv[3], 1);
    if (!strcmp(argv[1], "sleep") && argc == 4) return body_op(argv[2], argv[3], 0);
    if (!strcmp(argv[1], "agent") && argc == 4) return agent_op(argv[2], argv[3], "near");
    if (!strcmp(argv[1], "agent") && argc == 5) return agent_op(argv[2], argv[3], argv[4]);
    return 2;
}
