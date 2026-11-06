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
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
/* tile_autotile.c is text-included, not linked (house sharing rule: inline, text-include or op+IPC, never header+link). It is a pure table; its main() is compiled out by TILE_AUTOTILE_LIB. */
#define TILE_AUTOTILE_LIB
#include "../../../_.monads/_.livedesk-taskbar/ops/tile_autotile.c"

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
    return stbi_write_png(path, w, h, 4, px, w * 4) ? 0 : -1;
}
static void blit_x4(unsigned char *dst, int dw, const unsigned char *src, int sw, int sh, int ox) {
    int y, x, sy, sx;
    for (y = 0; y < sh; y++) for (x = 0; x < sw; x++) {
        const unsigned char *s = src + (y * sw + x) * 4;
        for (sy = 0; sy < 4; sy++) for (sx = 0; sx < 4; sx++) {
            unsigned char *d = dst + (((y * 4 + sy) * dw + (ox + x * 4 + sx)) * 4);
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
    }
}
static int emit(const char *outdir, const char *sheet, unsigned seed, int w, int h, const char *style, int outline, Role *roles, int n) {
    int i;
    char dir[640], path[700];
    unsigned char *px, *strip;
    if (w < 8 || h < 8 || w > MAXN || h > MAXN) return 2;
    px = calloc((size_t)w * h, 4);
    strip = calloc((size_t)n * w * 4 * h * 4, 4);
    if (!px || !strip) { free(px); free(strip); return 2; }
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
        if (write_png(path, px, w, h) != 0) { free(px); free(strip); return 2; }
        blit_x4(strip, n * w * 4, px, w, h, i * w * 4);
    }
    snprintf(dir, sizeof dir, "%s/%s/%u", outdir, sheet, seed);
    snprintf(path, sizeof path, "%s/sheet.png", dir);
    if (write_png(path, strip, n * w * 4, h * 4) != 0) { free(px); free(strip); return 2; }
    free(strip);
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
static int from_csv(const char *csv, const char *png, int scale) {
    FILE *f = fopen(csv, "r");
    char line[80];
    int w = 0, h = 0, n = 0, cap = 0, i, y, x, sy, sx;
    unsigned char *px = NULL, *big;
    if (!f) return 2;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# resolution=%d", &w) != 1) { fclose(f); return 2; }
    h = w;
    { char *xp = strchr(line, 'x'); if (xp) h = atoi(xp + 1); }
    fgets(line, sizeof line, f); fgets(line, sizeof line, f); fgets(line, sizeof line, f);
    while (fgets(line, sizeof line, f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (n >= cap) { cap = cap ? cap * 2 : w * h; px = realloc(px, (size_t)cap * 4); if (!px) { fclose(f); return 2; } }
        px[n * 4] = (unsigned char)r; px[n * 4 + 1] = (unsigned char)g; px[n * 4 + 2] = (unsigned char)b; px[n * 4 + 3] = (unsigned char)a;
        n++;
    }
    fclose(f);
    if (n != w * h || scale < 1 || scale > 8) { free(px); return 2; }
    big = calloc((size_t)w * scale * h * scale, 4);
    if (!big) { free(px); return 2; }
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) for (sy = 0; sy < scale; sy++) for (sx = 0; sx < scale; sx++) {
        unsigned char *s = px + (y * w + x) * 4;
        unsigned char *d = big + (((y * scale + sy) * (w * scale) + (x * scale + sx)) * 4);
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
    }
    i = write_png(png, big, w * scale, h * scale);
    free(px); free(big);
    return i == 0 ? 0 : 2;
}

static void proof_block(unsigned char *src) {
    int y, x;
    for (y = 0; y < 144; y++) for (x = 0; x < 96; x++) {
        int qx = x / 24, qy = y / 24;
        unsigned char *p = src + (y * 96 + x) * 4;
        p[0] = (unsigned char)(qx * 40);
        p[1] = (unsigned char)(qy * 30);
        p[2] = 17;
        p[3] = 255;
    }
}
static int shapes_from(const char *outdir, unsigned char *src, const char *label) {
    unsigned char *tile = calloc(48 * 48, 4);
    unsigned char *sheet = calloc((size_t)8 * 48 * 6 * 48, 4);
    int shape, q, y, x, fails = 0;
    char bl[200] = "";
    if (label) snprintf(bl, sizeof bl, " block=%.150s", label);
    if (!tile || !sheet) { free(tile); free(sheet); return 2; }
    for (shape = 0; shape < 48; shape++) {
        int b[4][6];
        if (!tile_autotile_blits(0, shape, 48, b)) { fails++; continue; }
        memset(tile, 0, 48 * 48 * 4);
        for (q = 0; q < 4; q++) for (y = 0; y < b[q][5]; y++) for (x = 0; x < b[q][4]; x++) {
            int sx = b[q][0] + x, sy = b[q][1] + y, dx = b[q][2] + x, dy = b[q][3] + y;
            if (sx < 0 || sy < 0 || sx >= 96 || sy >= 144 || dx < 0 || dy < 0 || dx >= 48 || dy >= 48) { fails++; continue; }
            memcpy(tile + (dy * 48 + dx) * 4, src + (sy * 96 + sx) * 4, 4);
        }
        for (q = 0; q < 4; q++) for (y = 0; y < b[q][5]; y++) for (x = 0; x < b[q][4]; x++) {
            int sx = b[q][0] + x, sy = b[q][1] + y, dx = b[q][2] + x, dy = b[q][3] + y;
            unsigned char *s = src + (sy * 96 + sx) * 4;
            unsigned char *d = tile + (dy * 48 + dx) * 4;
            if (s[0] != d[0] || s[1] != d[1] || s[2] != d[2] || s[3] != d[3]) fails++;
        }
        if (outdir) {
            char dir[700], path[760];
            int col = shape % 8, row = shape / 8;
            snprintf(dir, sizeof dir, "%s/s%02d", outdir, shape);
            mkp(dir);
            snprintf(path, sizeof path, "%s/sprite.csv", dir);
            if (write_csv(path, tile, 48, 48) != 0) fails++;
            snprintf(path, sizeof path, "%s/tile.png", dir);
            if (write_png(path, tile, 48, 48) != 0) fails++;
            for (y = 0; y < 48; y++) for (x = 0; x < 48; x++)
                memcpy(sheet + (((row * 48 + y) * (8 * 48) + col * 48 + x) * 4), tile + (y * 48 + x) * 4, 4);
        }
    }
    if (outdir) {
        char path[760];
        snprintf(path, sizeof path, "%s/sheet.png", outdir);
        if (write_png(path, sheet, 8 * 48, 6 * 48) != 0) fails++;
        printf("SHAPES floor=48%s wrote=%s quad=%s\n", bl, outdir, fails ? "bad" : "ok");
    } else {
        printf("SHAPES floor=48%s quad=%s\n", bl, fails ? "bad" : "ok");
    }
    free(tile); free(sheet);
    return fails ? 4 : 0;
}
/* The proof block: flat colours, so a wrong quadrant shows as a wrong colour (the original check). */
static int shapes(const char *outdir) {
    int rc;
    unsigned char *src = calloc(96 * 144, 4);
    if (!src) return 2;
    proof_block(src);
    rc = shapes_from(outdir, src, NULL);
    free(src);
    return rc;
}
/* A real block: block N (96x144) of a whole RPG Maker autotile sheet, 8 blocks across (A2: 768x576 = 8 x 4 blocks).
 * The table is the same, so the 48 shapes now carry real pixels, edges included. */
static int shapes_sheet(const char *outdir, const char *sheet, int idx) {
    int w = 0, h = 0, n = 0, bx, by, y, rc;
    unsigned char *img = stbi_load(sheet, &w, &h, &n, 4), *src;
    char label[160], *base;
    if (!img) { fprintf(stderr, "cannot read PNG %s\n", sheet); return 2; }
    if (w % 96 != 0 || h % 144 != 0 || idx < 0 || idx >= (w / 96) * (h / 144)) {
        fprintf(stderr, "sheet %dx%d has no block %d (blocks are 96x144)\n", w, h, idx);
        stbi_image_free(img); return 2;
    }
    bx = (idx % (w / 96)) * 96; by = (idx / (w / 96)) * 144;
    src = malloc(96 * 144 * 4);
    if (!src) { stbi_image_free(img); return 2; }
    for (y = 0; y < 144; y++) memcpy(src + y * 96 * 4, img + ((by + y) * w + bx) * 4, 96 * 4);
    stbi_image_free(img);
    base = strrchr(sheet, '/'); base = base ? base + 1 : (char *)sheet;
    snprintf(label, sizeof label, "%.100s#%d", base, idx);
    rc = shapes_from(outdir, src, label);
    free(src);
    return rc;
}
/* ---- restyle: recolour sliced RPG Maker cells into a Tear-it look (decision D27, option A) ----
 *   generator_tile_op.+x restyle <style.pdl> <outdir> <cell sprite.csv>...
 * Same cell size and same pixel grid, so a restyled cell still fits the autotile table. ALPHA IS NEVER CHANGED.
 * Each opaque pixel is mapped to a step of the style's ramp by its luminance, scaled to that cell's own
 * min..max so the cell keeps its contrast. An opaque pixel next to a transparent one INSIDE the cell gets
 * the darkest ramp step (an outline; the cell border is never outlined, so tiles still meet). A few
 * deterministic accent pixels (hash of seed, x, y: no rand) land on the lightest step.
 * Style file rows (same shape as GEN rows):  GEN | ramp | #dark,#..,#light   GEN | accent | #rrggbb
 *   GEN | accent_per256 | 0..255   GEN | seed | n   GEN | outline | 0 or 1
 * Output: <outdir>/<sheet>_<cell>/sprite.csv + tile.png (x4 view). Candidates only; the owner moves a kept
 * file to out/good/ (TEAR-IT-TILES-100). This op does not read from or write to out/good/. */
#define RS_MAXRAMP 8
typedef struct { int n, ramp[RS_MAXRAMP][3], accent[3], accent_per256, outline; unsigned seed; } RStyle;
static unsigned char *read_cell(const char *csv, int *w, int *h) {
    FILE *f = fopen(csv, "r");
    char line[80];
    int n = 0, i;
    unsigned char *px;
    if (!f) return NULL;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# resolution=%d", w) != 1) { fclose(f); return NULL; }
    *h = *w;
    { char *xp = strchr(line, 'x'); if (xp) *h = atoi(xp + 1); }
    if (*w < 1 || *h < 1 || *w > 256 || *h > 256) { fclose(f); return NULL; }
    for (i = 0; i < 3; i++) if (!fgets(line, sizeof line, f)) { fclose(f); return NULL; }
    px = calloc((size_t)*w * *h, 4);
    if (!px) { fclose(f); return NULL; }
    while (fgets(line, sizeof line, f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (n >= *w * *h) { free(px); fclose(f); return NULL; }
        px[n * 4] = (unsigned char)r; px[n * 4 + 1] = (unsigned char)g; px[n * 4 + 2] = (unsigned char)b; px[n * 4 + 3] = (unsigned char)a;
        n++;
    }
    fclose(f);
    if (n != *w * *h) { free(px); return NULL; }
    return px;
}
static int parse_hex3(const char *s, int *rgb) {
    if (*s == '#') s++;
    if (hex2(s) < 0 || hex2(s + 2) < 0 || hex2(s + 4) < 0) return -1;
    rgb[0] = hex2(s); rgb[1] = hex2(s + 2); rgb[2] = hex2(s + 4);
    return 0;
}
static int read_style(const char *path, RStyle *st) {
    FILE *f = fopen(path, "r");
    char line[400];
    memset(st, 0, sizeof *st);
    st->outline = 1; st->seed = 1;
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *k, *v, *p;
        if (strncmp(line, "GEN |", 5) != 0) continue;
        p = line + 5; while (*p == ' ') p++;
        k = p; p = strchr(p, '|'); if (!p) continue; *p = 0; v = p + 1; while (*v == ' ') v++;
        p = strchr(v, '\n'); if (p) *p = 0;
        while (k[0] && k[strlen(k) - 1] == ' ') k[strlen(k) - 1] = 0;
        if (strcmp(k, "ramp") == 0) {
            char *q = v;
            st->n = 0;
            while (*q) {
                if (st->n >= RS_MAXRAMP || parse_hex3(q, st->ramp[st->n]) != 0) { fclose(f); return -1; }
                st->n++;
                q += (*q == '#') ? 7 : 6;
                if (*q == ',') q++; else break;
            }
        }
        else if (strcmp(k, "accent") == 0) { if (parse_hex3(v, st->accent) != 0) { fclose(f); return -1; } }
        else if (strcmp(k, "accent_per256") == 0) st->accent_per256 = atoi(v);
        else if (strcmp(k, "seed") == 0) st->seed = (unsigned)strtoul(v, NULL, 10);
        else if (strcmp(k, "outline") == 0) st->outline = atoi(v);
    }
    fclose(f);
    return (st->n >= 2 && st->accent_per256 >= 0 && st->accent_per256 <= 255) ? 0 : -1;
}
static int restyle_cell(const RStyle *st, const char *outdir, const char *csv) {
    int w, h, x, y, lo = 1 << 20, hi = -1, opaque = 0, changed = 0, outl = 0, acc = 0, i;
    unsigned char *src = read_cell(csv, &w, &h), *out;
    char name[208], dir[1000], path[1100], tmp[512], *a, *b;
    unsigned *ci, *co;
    int cin = 0, cout = 0;
    if (!src) { fprintf(stderr, "cannot read cell %s\n", csv); return 2; }
    out = malloc((size_t)w * h * 4);
    if (!out) { free(src); return 2; }
    memcpy(out, src, (size_t)w * h * 4);
    { int any = 0; for (i = 0; i < w * h; i++) if (src[i * 4 + 3]) { any = 1; break; } if (!any) { free(src); free(out); return 0; } }
    for (i = 0; i < w * h; i++) if (src[i * 4 + 3] > 0) {
        int l = (299 * src[i * 4] + 587 * src[i * 4 + 1] + 114 * src[i * 4 + 2]) / 1000;
        if (l < lo) lo = l;
        if (l > hi) hi = l;
        opaque++;
    }
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        unsigned char *s = src + (y * w + x) * 4, *d = out + (y * w + x) * 4;
        int l, lev, edge = 0;
        if (s[3] == 0) continue;
        l = (299 * s[0] + 587 * s[1] + 114 * s[2]) / 1000;
        lev = (hi == lo) ? st->n / 2 : (l - lo) * st->n / (hi - lo + 1);
        if (lev >= st->n) lev = st->n - 1;
        d[0] = (unsigned char)st->ramp[lev][0]; d[1] = (unsigned char)st->ramp[lev][1]; d[2] = (unsigned char)st->ramp[lev][2];
        if (st->outline) {
            if (x > 0 && src[(y * w + x - 1) * 4 + 3] == 0) edge = 1;
            if (x < w - 1 && src[(y * w + x + 1) * 4 + 3] == 0) edge = 1;
            if (y > 0 && src[((y - 1) * w + x) * 4 + 3] == 0) edge = 1;
            if (y < h - 1 && src[((y + 1) * w + x) * 4 + 3] == 0) edge = 1;
        }
        if (edge) { d[0] = (unsigned char)st->ramp[0][0]; d[1] = (unsigned char)st->ramp[0][1]; d[2] = (unsigned char)st->ramp[0][2]; outl++; }
        else if (lev == st->n - 1 && st->accent_per256 > 0 && (int)(pixhash(st->seed, 0, x, y) & 255) < st->accent_per256) {
            d[0] = (unsigned char)st->accent[0]; d[1] = (unsigned char)st->accent[1]; d[2] = (unsigned char)st->accent[2]; acc++;
        }
    }
    ci = calloc((size_t)w * h, sizeof *ci); co = calloc((size_t)w * h, sizeof *co);
    if (!ci || !co) { free(ci); free(co); free(src); free(out); return 2; }
    for (i = 0; i < w * h; i++) {
        unsigned char *s = src + i * 4, *d = out + i * 4;
        if (s[3] == 0) continue;
        if (memcmp(s, d, 3) != 0) changed++;
        { unsigned k = ((unsigned)s[0] << 16) | ((unsigned)s[1] << 8) | s[2], j; for (j = 0; j < (unsigned)cin && ci[j] != k; j++) ; if (j == (unsigned)cin) ci[cin++] = k; }
        { unsigned k = ((unsigned)d[0] << 16) | ((unsigned)d[1] << 8) | d[2], j; for (j = 0; j < (unsigned)cout && co[j] != k; j++) ; if (j == (unsigned)cout) co[cout++] = k; }
    }
    snprintf(tmp, sizeof tmp, "%s", csv);
    b = strrchr(tmp, '/'); if (b) *b = 0;
    b = strrchr(tmp, '/'); b = b ? b + 1 : tmp;
    a = b - 1;
    if (a > tmp) { *a = 0; a = strrchr(tmp, '/'); a = a ? a + 1 : tmp; } else a = (char *)"";
    snprintf(name, sizeof name, "%.100s_%.100s", a, b);
    snprintf(dir, sizeof dir, "%s/%s", outdir, name);
    mkp(dir);
    snprintf(path, sizeof path, "%s/sprite.csv", dir);
    if (write_csv(path, out, w, h) != 0) { free(src); free(out); return 2; }
    {
        unsigned char *big = calloc((size_t)w * 4 * h * 4, 4);
        if (!big) { free(src); free(out); return 2; }
        blit_x4(big, w * 4, out, w, h, 0);
        snprintf(path, sizeof path, "%s/tile.png", dir);
        i = write_png(path, big, w * 4, h * 4);
        free(big);
        if (i != 0) { free(src); free(out); return 2; }
    }
    free(ci); free(co);
    printf("RESTYLE %s %dx%d opaque=%d changed=%d outline=%d accent=%d colours_in=%d colours_out=%d\n", name, w, h, opaque, changed, outl, acc, cin, cout);
    free(src); free(out);
    return 0;
}
static int restyle(const char *stylef, const char *outdir, int ncell, char **cells) {
    RStyle st;
    int i, rc = 0;
    if (read_style(stylef, &st) != 0) { fprintf(stderr, "bad style file (needs GEN | ramp with 2..%d colours)\n", RS_MAXRAMP); return 3; }
    for (i = 0; i < ncell; i++) { int r = restyle_cell(&st, outdir, cells[i]); if (r) rc = r; }
    return rc;
}

/* contact <out.png> <cols> <cell sprite.csv>...: lay cells out in a grid, in the order given (the owner reviews one contact sheet at a time). */
static int contact(const char *outpng, int cols, int ncell, char **cells) {
    int rows, cw = 0, ch = 0, i, y, rc;
    unsigned char *img = NULL;
    if (cols < 1 || cols > 32 || ncell < 1) return 2;
    rows = (ncell + cols - 1) / cols;
    for (i = 0; i < ncell; i++) {
        int w, h;
        unsigned char *c = read_cell(cells[i], &w, &h);
        if (!c) { fprintf(stderr, "cannot read cell %s\n", cells[i]); free(img); return 2; }
        if (i == 0) {
            cw = w; ch = h;
            img = calloc((size_t)cols * cw * rows * ch, 4);
            if (!img) { free(c); return 2; }
        } else if (w != cw || h != ch) { fprintf(stderr, "cell size differs: %s\n", cells[i]); free(c); free(img); return 2; }
        for (y = 0; y < ch; y++) memcpy(img + (((i / cols) * ch + y) * (cols * cw) + (i % cols) * cw) * 4, c + y * cw * 4, (size_t)cw * 4);
        free(c);
    }
    rc = write_png(outpng, img, cols * cw, rows * ch);
    if (rc == 0) printf("CONTACT %s cells=%d cols=%d size=%dx%d\n", outpng, ncell, cols, cols * cw, rows * ch);
    free(img);
    return rc == 0 ? 0 : 2;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "shapes-check") == 0) return shapes(NULL);
    if (argc == 3 && strcmp(argv[1], "shapes") == 0) return shapes(argv[2]);
    if (argc == 5 && strcmp(argv[1], "shapes") == 0) return shapes_sheet(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && strcmp(argv[1], "shapes-check") == 0) return shapes_sheet(NULL, argv[2], atoi(argv[3]));
    if (argc >= 5 && strcmp(argv[1], "restyle") == 0) return restyle(argv[2], argv[3], argc - 4, argv + 4);
    if (argc >= 5 && strcmp(argv[1], "contact") == 0) return contact(argv[2], atoi(argv[3]), argc - 4, argv + 4);
    if (argc == 4 && strcmp(argv[1], "fromcsv") == 0) return from_csv(argv[2], argv[3], 4);
    if (argc == 5 && strcmp(argv[1], "fromcsv") == 0) return from_csv(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && strcmp(argv[1], "run") == 0) return run_file(argv[2], argv[3]);
    if (argc == 3 && strcmp(argv[1], "catalog") == 0) return catalog(argv[2]);
    if (argc == 3 && strcmp(argv[1], "audit") == 0) return audit(argv[2]);
    fprintf(stderr, "usage: run <gen.pdl> <outdir> | catalog <outdir> | audit <outdir> | fromcsv <sprite.csv> <png> [scale] | restyle <style.pdl> <outdir> <cell.csv>... | contact <out.png> <cols> <cell.csv>... | shapes-check [<sheet.png> <block>] | shapes <outdir> [<sheet.png> <block>] | <outdir>\n");
    return 2;
}
