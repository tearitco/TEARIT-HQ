/* generator_tile_op.c — in-house tiles for the palettes Generate row.
 * Replaces one RPG Maker sheet at a time. Writes sprite.csv (what the
 * board arms) and a PNG beside it. Same GEN rows and seed => same bytes.
 *
 *   generator_tile_op.+x run <gen.pdl> <outdir>
 *   generator_tile_op.+x catalog <outdir>
 *   generator_tile_op.+x audit <outdir>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <zlib.h>

#define MAXR 12
#define MAXN 48

typedef struct { int r, g, b; char name[24]; } Role;

static unsigned mix(unsigned h) {
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}
static unsigned pixhash(unsigned seed, int role, int x, int y) {
    return mix(seed ^ (unsigned)role * 0x9e3779b9u ^ (unsigned)x * 0x85ebca6bu ^ (unsigned)y * 0xc2b2ae35u);
}
static int hex2(const char *s) {
    int v = 0, i;
    for (i = 0; i < 2; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v += c - '0';
        else if (c >= 'a' && c <= 'f') v += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v += c - 'A' + 10;
        else return -1;
    }
    return v;
}
static int parse_palette(const char *s, Role *roles, int n) {
    int i = 0;
    const char *p = s;
    while (i < n && *p) {
        if (*p == '#') p++;
        if (hex2(p) < 0) return -1;
        roles[i].r = hex2(p); roles[i].g = hex2(p + 2); roles[i].b = hex2(p + 4);
        p += 6;
        i++;
        if (*p == ',') p++;
        else break;
    }
    return i == n ? 0 : -1;
}
static int split_roles(char *s, Role *roles) {
    int n = 0;
    char *p = s;
    while (n < MAXR && p && *p) {
        char *c = strchr(p, ',');
        if (c) *c = 0;
        if (strlen(p) == 0 || strlen(p) > 22) return -1;
        snprintf(roles[n].name, sizeof roles[n].name, "%s", p);
        n++;
        p = c ? c + 1 : NULL;
    }
    return n;
}
static void mkp(const char *path) {
    char buf[512];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
    mkdir(buf, 0755);
}
static void shade(int *r, int *g, int *b, int d) {
    *r += d; *g += d; *b += d;
    if (*r < 0) *r = 0; if (*g < 0) *g = 0; if (*b < 0) *b = 0;
    if (*r > 255) *r = 255; if (*g > 255) *g = 255; if (*b > 255) *b = 255;
}
static void paint(unsigned char *px, int n, int x, int y, int r, int g, int b) {
    if (x < 0 || y < 0 || x >= n || y >= n) return;
    unsigned char *p = px + (y * n + x) * 4;
    p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b; p[3] = 255;
}
static void draw_role(unsigned char *px, int n, unsigned seed, int ri, const Role *role, const char *style, int outline) {
    int y, x, step = (strcmp(style, "coarse") == 0) ? (n / 4 > 1 ? n / 4 : 2) : 1;
    const char *nm = role->name;
    for (y = 0; y < n; y++) for (x = 0; x < n; x++) {
        int sx = (x / step) * step, sy = (y / step) * step;
        unsigned h = pixhash(seed, ri, sx, sy);
        int r = role->r, g = role->g, b = role->b;
        int noise = (int)(h % 7) - 3;
        shade(&r, &g, &b, noise);
        if (strcmp(nm, "water") == 0 && ((y / (step)) % 2) == 0) shade(&r, &g, &b, 18);
        if (strcmp(nm, "water_edge") == 0) {
            if (y < n / 3) { r = role->r; g = role->g + 30; b = role->b - 40; }
            else if ((y / step) % 2 == 0) shade(&r, &g, &b, 16);
        }
        if (strcmp(nm, "wall") == 0 && y < n / 5) shade(&r, &g, &b, -36);
        if (strcmp(nm, "door") == 0) {
            if (y < n / 5) shade(&r, &g, &b, -28);
            if (x > n / 5 && x < n - n / 5 && y > n / 4 && y < n - 1) { r = role->r + 20; g = role->g - 10; b = role->b - 10; }
        }
        if (strcmp(nm, "tree") == 0) {
            int cx = x - n / 2, cy = y - n / 3, rad = n / 3;
            if (cx * cx + cy * cy < rad * rad) { r = role->r; g = role->g; b = role->b; shade(&r, &g, &b, noise); }
            else if (x > n / 2 - n / 8 && x < n / 2 + n / 8 && y > n / 2) { r = 90; g = 58; b = 32; }
            else { r = g = b = 0; px[(y * n + x) * 4 + 3] = 0; paint(px, n, x, y, 0, 0, 0); px[(y * n + x) * 4 + 3] = 0; continue; }
        }
        if (strcmp(nm, "roof") == 0 && y < n / 6) shade(&r, &g, &b, -24);
        if (strcmp(nm, "window") == 0) {
            shade(&r, &g, &b, -10);
            if (x > n / 4 && x < n - n / 4 && y > n / 4 && y < n - n / 4) { r = 150; g = 190; b = 210; }
        }
        if (strcmp(nm, "stairs") == 0 && ((x + y) / (step > 1 ? step : 2)) % 2 == 0) shade(&r, &g, &b, 28);
        if (strcmp(nm, "mountain") == 0) {
            int peak = n / 5 + (n / 2 - x > 0 ? n / 2 - x : x - n / 2);
            if (y < peak) { r = g = b = 0; paint(px, n, x, y, 0, 0, 0); px[(y * n + x) * 4 + 3] = 0; continue; }
        }
        if (outline && (x == 0 || y == 0 || x == n - 1 || y == n - 1) && strcmp(nm, "tree") != 0 && strcmp(nm, "mountain") != 0)
            shade(&r, &g, &b, -48);
        paint(px, n, x, y, r, g, b);
    }
}
static int write_csv(const char *path, const unsigned char *px, int n) {
    FILE *f = fopen(path, "w");
    int i, count = n * n;
    if (!f) return -1;
    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", n);
    for (i = 0; i < count; i++)
        fprintf(f, "%u,%u,%u,%u\n", px[i * 4], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
    fclose(f);
    return 0;
}
static int write_png(const char *path, const unsigned char *px, int n) {
    FILE *f;
    unsigned char *raw, *comp;
    uLongf clen;
    int y, x;
    unsigned long crc;
    size_t raw_n = (size_t)n * (n * 4 + 1);
    raw = malloc(raw_n);
    if (!raw) return -1;
    for (y = 0; y < n; y++) {
        raw[y * (n * 4 + 1)] = 0;
        for (x = 0; x < n; x++) memcpy(raw + y * (n * 4 + 1) + 1 + x * 4, px + (y * n + x) * 4, 4);
    }
    clen = compressBound(raw_n);
    comp = malloc(clen);
    if (!comp || compress(comp, &clen, raw, raw_n) != Z_OK) { free(raw); free(comp); return -1; }
    f = fopen(path, "wb");
    if (!f) { free(raw); free(comp); return -1; }
    /* PNG signature, IHDR, IDAT, IEND. CRC via zlib. */
    {
        unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        unsigned char ihdr[17];
        unsigned char lenb[4], crcb[4];
        uLong c;
        fwrite(sig, 1, 8, f);
        ihdr[0] = (n >> 24) & 255; ihdr[1] = (n >> 16) & 255; ihdr[2] = (n >> 8) & 255; ihdr[3] = n & 255;
        ihdr[4] = 0; ihdr[5] = 0; ihdr[6] = 0; ihdr[7] = n & 255; /* height, n <= 48 */
        ihdr[4] = (n >> 24) & 255; ihdr[5] = (n >> 16) & 255; ihdr[6] = (n >> 8) & 255; ihdr[7] = n & 255;
        ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
        lenb[0] = 0; lenb[1] = 0; lenb[2] = 0; lenb[3] = 13;
        fwrite(lenb, 1, 4, f); fwrite("IHDR", 1, 4, f); fwrite(ihdr, 1, 13, f);
        c = crc32(crc32(0, Z_NULL, 0), (unsigned char *)"IHDR", 4);
        c = crc32(c, ihdr, 13);
        crcb[0] = (c >> 24) & 255; crcb[1] = (c >> 16) & 255; crcb[2] = (c >> 8) & 255; crcb[3] = c & 255;
        fwrite(crcb, 1, 4, f);
        lenb[0] = (clen >> 24) & 255; lenb[1] = (clen >> 16) & 255; lenb[2] = (clen >> 8) & 255; lenb[3] = clen & 255;
        fwrite(lenb, 1, 4, f); fwrite("IDAT", 1, 4, f); fwrite(comp, 1, clen, f);
        c = crc32(crc32(0, Z_NULL, 0), (unsigned char *)"IDAT", 4);
        c = crc32(c, comp, clen);
        crcb[0] = (c >> 24) & 255; crcb[1] = (c >> 16) & 255; crcb[2] = (c >> 8) & 255; crcb[3] = c & 255;
        fwrite(crcb, 1, 4, f);
        lenb[0] = lenb[1] = lenb[2] = lenb[3] = 0;
        fwrite(lenb, 1, 4, f); fwrite("IEND", 1, 4, f);
        c = crc32(crc32(0, Z_NULL, 0), (unsigned char *)"IEND", 4);
        crcb[0] = (c >> 24) & 255; crcb[1] = (c >> 16) & 255; crcb[2] = (c >> 8) & 255; crcb[3] = c & 255;
        fwrite(crcb, 1, 4, f);
        (void)crc;
    }
    fclose(f); free(raw); free(comp);
    return 0;
}
static int emit(const char *outdir, const char *sheet, unsigned seed, int cell, const char *style, int outline, Role *roles, int n) {
    int i;
    char dir[512], path[512];
    unsigned char *px = malloc((size_t)cell * cell * 4);
    if (!px || cell < 8 || cell > MAXN) { free(px); return 2; }
    for (i = 0; i < n; i++) {
        snprintf(dir, sizeof dir, "%s/%s/%u/%s", outdir, sheet, seed, roles[i].name);
        mkp(dir);
        memset(px, 0, (size_t)cell * cell * 4);
        draw_role(px, cell, seed, i, &roles[i], style, outline);
        snprintf(path, sizeof path, "%s/sprite.csv", dir);
        if (write_csv(path, px, cell) != 0) { free(px); return 2; }
        snprintf(path, sizeof path, "%s/tile.png", dir);
        if (write_png(path, px, cell) != 0) { free(px); return 2; }
    }
    free(px);
    printf("WROTE %s roles=%d cell=%d seed=%u\n", sheet, n, cell, seed);
    return 0;
}
static int run_file(const char *gen, const char *outdir) {
    FILE *f = fopen(gen, "r");
    char line[512], roles_s[256] = "", pal[256] = "", sheet[64] = "", style[16] = "flat";
    Role roles[MAXR];
    unsigned seed = 0; int cell = 16, outline = 1, n, i;
    if (!f) { fprintf(stderr, "no gen file\n"); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *k, *v, *p;
        if (strncmp(line, "GEN |", 5) != 0) continue;
        p = line + 5; while (*p == ' ') p++;
        k = p; p = strchr(p, '|'); if (!p) continue; *p = 0; v = p + 1; while (*v == ' ') v++;
        p = strchr(v, '\n'); if (p) *p = 0;
        while (k[0] && k[strlen(k) - 1] == ' ') k[strlen(k) - 1] = 0;
        if (strcmp(k, "sheet") == 0) snprintf(sheet, sizeof sheet, "%s", v);
        else if (strcmp(k, "seed") == 0) seed = (unsigned)strtoul(v, NULL, 10);
        else if (strcmp(k, "cell") == 0) cell = atoi(v);
        else if (strcmp(k, "roles") == 0) snprintf(roles_s, sizeof roles_s, "%s", v);
        else if (strcmp(k, "palette") == 0) snprintf(pal, sizeof pal, "%s", v);
        else if (strcmp(k, "style") == 0) snprintf(style, sizeof style, "%s", v);
        else if (strcmp(k, "outline") == 0) outline = atoi(v);
    }
    fclose(f);
    if (!sheet[0] || !roles_s[0] || seed == 0) { fprintf(stderr, "gen missing sheet, roles, or seed\n"); return 2; }
    n = split_roles(roles_s, roles);
    if (n < 1) return 2;
    if (parse_palette(pal, roles, n) != 0) { fprintf(stderr, "palette count does not match roles\n"); return 3; }
    for (i = 0; i < n; i++) if (!roles[i].name[0]) return 3;
    return emit(outdir, sheet, seed, cell, style, outline, roles, n);
}

typedef struct { const char *sheet, *roles, *pal, *style; int cell, outline; unsigned seed; } Cat;
static const Cat CATS[] = {
    { "Outside", "grass,dirt,path,water,water_edge,wall,door,tree,roof",
      "#3a6b32,#6b4a2a,#c2b48a,#2a5a8a,#245070,#4a4a4a,#8a3a2a,#1e3a16,#6a5a4a", "flat", 16, 1, 20261105u },
    { "SF_Outside", "grass,dirt,path,water,water_edge,wall,door,tree,roof",
      "#2a5a48,#5a4038,#b0a898,#1a4068,#183848,#3a4048,#7a3030,#143018,#5a5048", "flat", 16, 1, 20261105u },
    { "Inside", "floor,wall,window,door",
      "#8a7a68,#c8c0b0,#88b0c8,#6a4030", "flat", 16, 1, 20261105u },
    { "Dungeon", "floor,wall,door,water,stairs",
      "#3a3a3a,#5a5a62,#4a3030,#1a3048,#7a7a82", "flat", 16, 1, 20261105u },
    { "World", "grass,water,mountain,sand,forest",
      "#2f6a28,#1a4878,#6a6a6a,#c2b070,#143018", "coarse", 16, 1, 20261105u },
};
static int catalog(const char *outdir) {
    int i, rc = 0;
    for (i = 0; i < 5; i++) {
        char buf[640];
        Role roles[MAXR];
        int n;
        snprintf(buf, sizeof buf, "%s", CATS[i].roles);
        n = split_roles(buf, roles);
        if (parse_palette(CATS[i].pal, roles, n) != 0) return 3;
        rc = emit(outdir, CATS[i].sheet, CATS[i].seed, CATS[i].cell, CATS[i].style, CATS[i].outline, roles, n);
        if (rc) return rc;
    }
    return 0;
}
static int mean_chan(const char *path, int chan, int *out, int *ncell, int *top_darker) {
    FILE *f = fopen(path, "r");
    char line[64];
    long sum = 0, n = 0, top = 0, mid = 0, nt = 0, nm = 0;
    int res = 0, pix = 0;
    if (!f) return -1;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# resolution=%d", &res) != 1) { fclose(f); return -1; }
    fgets(line, sizeof line, f); fgets(line, sizeof line, f); fgets(line, sizeof line, f);
    while (fgets(line, sizeof line, f)) {
        int r, g, b, a, v;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) { fclose(f); return -1; }
        v = chan == 0 ? r : chan == 1 ? g : b;
        if (a) {
            int y = pix / res;
            sum += v; n++;
            if (y < res / 5) { top += v; nt++; }
            if (y > res / 3 && y < 2 * res / 3) { mid += v; nm++; }
        }
        pix++;
    }
    fclose(f);
    if (n < 1) return -1;
    *out = (int)(sum / n);
    *ncell = res;
    *top_darker = (nt && nm && top / nt + 8 < mid / nm) ? 1 : 0;
    return 0;
}
static int audit(const char *outdir) {
    int ci, ri, fails = 0;
    for (ci = 0; ci < 5; ci++) {
        char buf[640], path[512];
        Role roles[MAXR];
        int n, prev_g = -1, i;
        int means[MAXR][3];
        snprintf(buf, sizeof buf, "%s", CATS[ci].roles);
        n = split_roles(buf, roles);
        for (ri = 0; ri < n; ri++) {
            int m0, m1, m2, res, dark, a;
            FILE *pf;
            snprintf(path, sizeof path, "%s/%s/%u/%s/sprite.csv", outdir, CATS[ci].sheet, CATS[ci].seed, roles[ri].name);
            if (mean_chan(path, 0, &m0, &res, &dark) || mean_chan(path, 1, &m1, &res, &dark) || mean_chan(path, 2, &m2, &res, &dark)) {
                printf("FAIL %s %s unreadable\n", CATS[ci].sheet, roles[ri].name); fails++; continue;
            }
            means[ri][0] = m0; means[ri][1] = m1; means[ri][2] = m2;
            if (res != CATS[ci].cell) { printf("FAIL %s %s cell\n", CATS[ci].sheet, roles[ri].name); fails++; }
            snprintf(path, sizeof path, "%s/%s/%u/%s/tile.png", outdir, CATS[ci].sheet, CATS[ci].seed, roles[ri].name);
            pf = fopen(path, "rb");
            if (!pf) { printf("FAIL %s %s no-png\n", CATS[ci].sheet, roles[ri].name); fails++; }
            else { unsigned char sig[4]; a = fread(sig, 1, 4, pf); fclose(pf); if (a != 4 || sig[1] != 'P') { printf("FAIL %s %s bad-png\n", CATS[ci].sheet, roles[ri].name); fails++; } }
            if (strcmp(roles[ri].name, "grass") == 0 && !(m1 > m0)) { printf("FAIL %s grass not-green\n", CATS[ci].sheet); fails++; }
            if (strcmp(roles[ri].name, "water") == 0 && !(m2 > m0)) { printf("FAIL %s water not-blue\n", CATS[ci].sheet); fails++; }
            if (strcmp(roles[ri].name, "wall") == 0 && !dark) { printf("FAIL %s wall no-top-edge\n", CATS[ci].sheet); fails++; }
            (void)prev_g;
        }
        for (i = 0; i < n; i++) for (ri = i + 1; ri < n; ri++) {
            int d = abs(means[i][0] - means[ri][0]) + abs(means[i][1] - means[ri][1]) + abs(means[i][2] - means[ri][2]);
            if (d < 24) { printf("FAIL %s %s-vs-%s too-alike d=%d\n", CATS[ci].sheet, roles[i].name, roles[ri].name, d); fails++; }
        }
        if (!fails) printf("AUDIT %s roles=%d cell=%d ok\n", CATS[ci].sheet, n, CATS[ci].cell);
    }
    return fails ? 4 : 0;
}
int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "run") == 0) return run_file(argv[2], argv[3]);
    if (argc == 3 && strcmp(argv[1], "catalog") == 0) return catalog(argv[2]);
    if (argc == 3 && strcmp(argv[1], "audit") == 0) return audit(argv[2]);
    fprintf(stderr, "usage: run <gen.pdl> <outdir> | catalog <outdir> | audit <outdir>\n");
    return 2;
}
