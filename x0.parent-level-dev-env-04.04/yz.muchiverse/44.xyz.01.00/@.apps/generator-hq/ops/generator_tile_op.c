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
#define MAXN 64

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
static int special_style(const char *style) {
    return strcmp(style, "sprite") == 0 || strcmp(style, "face") == 0 || strcmp(style, "sv") == 0
        || strcmp(style, "parallax") == 0 || strcmp(style, "battle") == 0 || strcmp(style, "title") == 0
        || strcmp(style, "system") == 0 || strcmp(style, "anim") == 0;
}
static void draw_special(unsigned char *px, int w, int h, unsigned seed, int ri, const Role *role, const char *style, int outline) {
    int x, y;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        int r = role->r, g = role->g, b = role->b, a = 255;
        unsigned hv = pixhash(seed, ri, x, y);
        if (strcmp(style, "parallax") == 0 || strcmp(style, "battle") == 0 || strcmp(style, "title") == 0) {
            int sky = (h - y) * 40 / h;
            shade(&r, &g, &b, sky - 20);
            if (strcmp(style, "parallax") == 0 && y > h * 2 / 3) { r = 70; g = 90; b = 50; }
            if (strcmp(style, "battle") == 0 && (x - w / 3) * (x - w / 3) + (y - h / 4) * (y - h / 4) < (h / 8) * (h / 8)) { r = 230; g = 230; b = 200; }
            if (strcmp(style, "title") == 0 && y > h / 3 && y < h / 2) shade(&r, &g, &b, 50);
        } else if (strcmp(style, "anim") == 0) {
            int cx = x - w / 2, cy = y - h / 2, rad = 2 + ri * 3;
            if (cx * cx + cy * cy > rad * rad) a = 0;
        } else if (strcmp(style, "system") == 0 && strcmp(role->name, "border") == 0) {
            if (x > 1 && y > 1 && x < w - 2 && y < h - 2) a = 0;
        } else if (strcmp(style, "system") == 0 && strcmp(role->name, "cursor") == 0) {
            if (x < y || x > w / 2) a = 0;
        } else {
            int cx = w / 2 + (strcmp(style, "sprite") == 0 && ri == 1 ? -w / 8 : ri == 2 ? w / 8 : 0);
            int cy = h / 3 + (strcmp(style, "sprite") == 0 && ri == 3 ? -h / 8 : ri == 0 && strcmp(style, "sprite") == 0 ? h / 10 : 0);
            int dx = x - cx, dy = y - cy;
            int head = (strcmp(style, "face") == 0) ? (w / 3) : (w / 6);
            int body = (strcmp(style, "sv") == 0) ? w / 3 : w / 5;
            if (dx * dx + dy * dy < head * head) shade(&r, &g, &b, 24);
            else if (strcmp(style, "face") != 0 && y > h / 2 && x > cx - body && x < cx + body) shade(&r, &g, &b, -8);
            else a = 0;
            if (strcmp(style, "face") == 0 && y == cy && (x == cx - head / 2 || x == cx + head / 2)) { r = g = b = 20; a = 255; }
            (void)hv;
        }
        if (a && outline && (x == 0 || y == 0 || x == w - 1 || y == h - 1) && strcmp(style, "sprite") != 0 && strcmp(style, "face") != 0 && strcmp(style, "sv") != 0 && strcmp(style, "anim") != 0)
            shade(&r, &g, &b, -40);
        if (!a) { paint(px, w, x, y, 0, 0, 0); px[(y * w + x) * 4 + 3] = 0; }
        else paint(px, w, x, y, r, g, b);
    }
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
static int write_csv(const char *path, const unsigned char *px, int w, int h) {
    FILE *f = fopen(path, "w");
    int i, count = w * h;
    if (!f) return -1;
    if (w == h) fprintf(f, "# resolution=%d\n", w);
    else fprintf(f, "# resolution=%dx%d\n", w, h);
    fprintf(f, "# scale=1.0\n# transform=0,0,0\nr,g,b,a\n");
    for (i = 0; i < count; i++)
        fprintf(f, "%u,%u,%u,%u\n", px[i * 4], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
    fclose(f);
    return 0;
}
static int write_png(const char *path, const unsigned char *px, int w, int h) {
    FILE *f;
    unsigned char *raw, *comp;
    uLongf clen;
    int y, x;
    unsigned long crc;
    size_t raw_n = (size_t)h * (w * 4 + 1);
    raw = malloc(raw_n);
    if (!raw) return -1;
    for (y = 0; y < h; y++) {
        raw[y * (w * 4 + 1)] = 0;
        for (x = 0; x < w; x++) memcpy(raw + y * (w * 4 + 1) + 1 + x * 4, px + (y * w + x) * 4, 4);
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
        ihdr[0] = (w >> 24) & 255; ihdr[1] = (w >> 16) & 255; ihdr[2] = (w >> 8) & 255; ihdr[3] = w & 255;
        ihdr[4] = (h >> 24) & 255; ihdr[5] = (h >> 16) & 255; ihdr[6] = (h >> 8) & 255; ihdr[7] = h & 255;
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
static int emit(const char *outdir, const char *sheet, unsigned seed, int w, int h, const char *style, int outline, Role *roles, int n) {
    int i;
    char dir[640], path[700];
    unsigned char *px;
    if (w < 8 || h < 8 || w > MAXN || h > MAXN) return 2;
    px = calloc((size_t)w * h, 4);
    if (!px) return 2;
    for (i = 0; i < n; i++) {
        snprintf(dir, sizeof dir, "%s/%s/%u/%s", outdir, sheet, seed, roles[i].name);
        mkp(dir);
        memset(px, 0, (size_t)w * h * 4);
        if (special_style(style)) draw_special(px, w, h, seed, i, &roles[i], style, outline);
        else if (w == h) draw_role(px, w, seed, i, &roles[i], style, outline);
        else { free(px); return 2; }
        snprintf(path, sizeof path, "%s/sprite.csv", dir);
        if (write_csv(path, px, w, h) != 0) { free(px); return 2; }
        snprintf(path, sizeof path, "%s/tile.png", dir);
        if (write_png(path, px, w, h) != 0) { free(px); return 2; }
    }
    free(px);
    printf("WROTE %s roles=%d cell=%dx%d seed=%u\n", sheet, n, w, h, seed);
    return 0;
}
static int run_file(const char *gen, const char *outdir) {
    FILE *f = fopen(gen, "r");
    char line[640], roles_s[320] = "", pal[320] = "", sheet[64] = "", style[16] = "flat";
    Role roles[MAXR];
    unsigned seed = 0; int w = 16, h = 16, outline = 1, n, i;
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
        else if (strcmp(k, "cell") == 0) { w = atoi(v); if (h == 16) h = w; }
        else if (strcmp(k, "wide") == 0) w = atoi(v);
        else if (strcmp(k, "tall") == 0) h = atoi(v);
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
    return emit(outdir, sheet, seed, w, h, style, outline, roles, n);
}

typedef struct { const char *sheet, *roles, *pal, *style; int w, h, outline; unsigned seed; } Cat;
static const Cat CATS[] = {
    { "Outside", "grass,dirt,path,water,water_edge,wall,door,tree,roof",
      "#3a6b32,#6b4a2a,#c2b48a,#2a5a8a,#245070,#4a4a4a,#8a3a2a,#1e3a16,#6a5a4a", "flat", 16, 16, 1, 20261105u },
    { "SF_Outside", "grass,dirt,path,water,water_edge,wall,door,tree,roof",
      "#2a5a48,#5a4038,#b0a898,#1a4068,#183848,#3a4048,#7a3030,#143018,#5a5048", "flat", 16, 16, 1, 20261105u },
    { "Inside", "floor,wall,window,door",
      "#8a7a68,#c8c0b0,#88b0c8,#6a4030", "flat", 16, 16, 1, 20261105u },
    { "Dungeon", "floor,wall,door,water,stairs",
      "#3a3a3a,#5a5a62,#4a3030,#1a3048,#7a7a82", "flat", 16, 16, 1, 20261105u },
    { "World", "grass,water,mountain,sand,forest",
      "#2f6a28,#1a4878,#6a6a6a,#c2b070,#143018", "coarse", 16, 16, 1, 20261105u },
    { "Characters", "down,left,right,up",
      "#c4a574,#3a5a8a,#c4a574,#3a5a8a", "sprite", 32, 32, 0, 20261105u },
    { "Faces", "hero",
      "#c4a574", "face", 32, 32, 0, 20261105u },
    { "SvActors", "stand",
      "#6a8a4a", "sv", 32, 32, 0, 20261105u },
    { "SvEnemies", "slime",
      "#3a8a4a", "sv", 32, 32, 0, 20261105u },
    { "Parallax", "sky",
      "#6aa0d0", "parallax", 64, 32, 1, 20261105u },
    { "Battlebacks", "field",
      "#203040", "battle", 48, 32, 1, 20261105u },
    { "Titles", "banner",
      "#1a2030", "title", 64, 24, 1, 20261105u },
    { "System", "cursor,border,icon",
      "#f0f0f0,#d0d0e0,#c0a040", "system", 16, 16, 0, 20261105u },
    { "Animations", "f0,f1,f2,f3",
      "#f0e060,#f0e060,#f0e060,#f0e060", "anim", 32, 32, 0, 20261105u },
};
static int catalog(const char *outdir) {
    int i, rc = 0, ncat = (int)(sizeof CATS / sizeof CATS[0]);
    for (i = 0; i < ncat; i++) {
        char buf[640];
        Role roles[MAXR];
        int n;
        snprintf(buf, sizeof buf, "%s", CATS[i].roles);
        n = split_roles(buf, roles);
        if (parse_palette(CATS[i].pal, roles, n) != 0) return 3;
        rc = emit(outdir, CATS[i].sheet, CATS[i].seed, CATS[i].w, CATS[i].h, CATS[i].style, CATS[i].outline, roles, n);
        if (rc) return rc;
    }
    return 0;
}
static int mean_chan(const char *path, int chan, int *out, int *ncell, int *nheight, int *top_darker) {
    FILE *f = fopen(path, "r");
    char line[80];
    long sum = 0, n = 0, top = 0, mid = 0, nt = 0, nm = 0;
    int res = 0, resh = 0, pix = 0;
    if (!f) return -1;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# resolution=%d", &res) != 1) { fclose(f); return -1; }
    resh = res;
    { char *x = strchr(line, 'x'); if (x) resh = atoi(x + 1); }
    fgets(line, sizeof line, f); fgets(line, sizeof line, f); fgets(line, sizeof line, f);
    while (fgets(line, sizeof line, f)) {
        int r, g, b, a, v;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) { fclose(f); return -1; }
        v = chan == 0 ? r : chan == 1 ? g : b;
        if (a) {
            int y = pix / res;
            sum += v; n++;
            if (y < resh / 5) { top += v; nt++; }
            if (y > resh / 3 && y < 2 * resh / 3) { mid += v; nm++; }
        }
        pix++;
    }
    fclose(f);
    if (n < 1) return -1;
    *out = (int)(sum / n);
    *ncell = res;
    *nheight = resh;
    *top_darker = (nt && nm && top / nt + 8 < mid / nm) ? 1 : 0;
    return 0;
}
static int audit(const char *outdir) {
    int ci, ri, fails = 0, ncat = (int)(sizeof CATS / sizeof CATS[0]);
    for (ci = 0; ci < ncat; ci++) {
        char buf[640], path[512];
        Role roles[MAXR];
        int n, prev_g = -1, i;
        int means[MAXR][3];
        snprintf(buf, sizeof buf, "%s", CATS[ci].roles);
        n = split_roles(buf, roles);
        for (ri = 0; ri < n; ri++) {
            int m0, m1, m2, res, reh, dark, a;
            FILE *pf;
            snprintf(path, sizeof path, "%s/%s/%u/%s/sprite.csv", outdir, CATS[ci].sheet, CATS[ci].seed, roles[ri].name);
            if (mean_chan(path, 0, &m0, &res, &reh, &dark) || mean_chan(path, 1, &m1, &res, &reh, &dark) || mean_chan(path, 2, &m2, &res, &reh, &dark)) {
                printf("FAIL %s %s unreadable\n", CATS[ci].sheet, roles[ri].name); fails++; continue;
            }
            means[ri][0] = m0; means[ri][1] = m1; means[ri][2] = m2;
            if (res != CATS[ci].w || reh != CATS[ci].h) { printf("FAIL %s %s cell\n", CATS[ci].sheet, roles[ri].name); fails++; }
            snprintf(path, sizeof path, "%s/%s/%u/%s/tile.png", outdir, CATS[ci].sheet, CATS[ci].seed, roles[ri].name);
            pf = fopen(path, "rb");
            if (!pf) { printf("FAIL %s %s no-png\n", CATS[ci].sheet, roles[ri].name); fails++; }
            else { unsigned char sig[4]; a = fread(sig, 1, 4, pf); fclose(pf); if (a != 4 || sig[1] != 'P') { printf("FAIL %s %s bad-png\n", CATS[ci].sheet, roles[ri].name); fails++; } }
            if (strcmp(roles[ri].name, "grass") == 0 && !(m1 > m0)) { printf("FAIL %s grass not-green\n", CATS[ci].sheet); fails++; }
            if (strcmp(roles[ri].name, "water") == 0 && !(m2 > m0)) { printf("FAIL %s water not-blue\n", CATS[ci].sheet); fails++; }
            if (strcmp(roles[ri].name, "wall") == 0 && !dark) { printf("FAIL %s wall no-top-edge\n", CATS[ci].sheet); fails++; }
            (void)prev_g;
        }
        if (strcmp(CATS[ci].style, "anim") != 0 && strcmp(CATS[ci].style, "sprite") != 0) {
            for (i = 0; i < n; i++) for (ri = i + 1; ri < n; ri++) {
                int d = abs(means[i][0] - means[ri][0]) + abs(means[i][1] - means[ri][1]) + abs(means[i][2] - means[ri][2]);
                if (d < 24) { printf("FAIL %s %s-vs-%s too-alike d=%d\n", CATS[ci].sheet, roles[i].name, roles[ri].name, d); fails++; }
            }
        }
        if (!fails) printf("AUDIT %s roles=%d cell=%dx%d ok\n", CATS[ci].sheet, n, CATS[ci].w, CATS[ci].h);
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
