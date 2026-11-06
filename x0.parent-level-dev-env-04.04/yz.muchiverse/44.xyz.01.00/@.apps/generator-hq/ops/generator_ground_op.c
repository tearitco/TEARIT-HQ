/* generator_ground_op: ground (RPG Maker A2) blocks drawn from a RECIPE and KNOBS, not repainted from the original. Decision D33 step 2.
 *   ground <ground.pdl> <original-sheet.png> <out.png> [bank=<concept-bank dir>] [knob=<warm|wet|wear|grain|edge>:<delta>]... [select=bc,br,bw,bh] [crop=bc,br,bw,bh]
 *   check  <ground.pdl>        parse only
 * What is kept from the original sheet: ONLY its transparency (the autotile geometry the RPG Maker runtime cuts the 48 shapes from; a block is 96 x 144 = 4 x 6 quarters of 24 px).
 * What is generated: every colour. Interior = a 24-pixel-periodic texture (so any two quarters meet without a seam), edge ring = darker ink steps by distance to the transparent area,
 * colours = a 5-step ramp from hue / saturation / lightness / contrast. Blocks with no BLOCK row stay transparent (nothing of the original is copied).
 * Rows ('|' separated, '#' comments):
 *   RECIPE | name | hue 0..359 | sat 0..100 | light 0..100 | contrast 0..100 | pattern noise|cobble|plank|blades | scale 1..3 | seed
 *   BLOCK  | <sheet name without .png> | <block 0.. row-major, 8 across> | <recipe name>
 * Knobs (weights): warm -1..1 (hue toward orange / blue), wet 0..1 (darker, more saturated, specular glints), wear 0..1 (dark speckle), grain 0..1 (fine noise), edge 0..1 (ring 2..6 px).
 * Weights come from the Concept Bank spoke ground_<recipe>.pdl (slots POINTS_TO=ground_warm|ground_wet|ground_wear|ground_grain|ground_edge, WEIGHT) when bank= is given; a missing
 * spoke means 0.0. knob= adds a sheet-wide delta (clamped); select= limits the knob= deltas to blocks inside the rectangle (block units); crop= writes only that rectangle. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
#define MAXR 64
#define MAXB 256
#define RING_MAX 6
typedef struct { char name[40], pattern[16]; double hue, sat, light, contrast; int scale; unsigned seed; } Recipe;
typedef struct { char sheet[60], recipe[40]; int idx; } Block;
static Recipe R[MAXR]; static int nr;
static Block B[MAXB]; static int nb;
static char g_bank[600];
static double g_knob[5]; static int g_knob_on, g_sel_on, g_sel[4], g_crop_on, g_crop[4];
static const char *KN[5] = {"warm", "wet", "wear", "grain", "edge"};
static double clampd(double v, double lo, double hi);
static unsigned hash32(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static char *fld(char **p) {
    char *s = *p, *e; while (*s == ' ') s++;
    e = strchr(s, '|'); if (e) { *e = 0; *p = e + 1; } else { e = s + strlen(s); *p = e; }
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
    return s;
}
static int read_rows(const char *path) {
    FILE *f = fopen(path, "r"); char line[400];
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *k;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "RECIPE")) {
            Recipe *r; if (nr >= MAXR) { fprintf(stderr, "too many recipes\n"); fclose(f); return 2; }
            r = &R[nr]; snprintf(r->name, sizeof r->name, "%s", fld(&p)); r->hue = atof(fld(&p)); r->sat = atof(fld(&p)); r->light = atof(fld(&p)); r->contrast = atof(fld(&p));
            snprintf(r->pattern, sizeof r->pattern, "%s", fld(&p)); r->scale = atoi(fld(&p)); r->seed = (unsigned)strtoul(fld(&p), NULL, 10);
            if (r->hue < 0 || r->hue > 359 || r->sat < 0 || r->sat > 100 || r->light < 0 || r->light > 100 || r->contrast < 0 || r->contrast > 100 || r->scale < 1 || r->scale > 3 ||
                (strcmp(r->pattern, "noise") && strcmp(r->pattern, "cobble") && strcmp(r->pattern, "plank") && strcmp(r->pattern, "blades"))) { fprintf(stderr, "bad RECIPE %s\n", r->name); fclose(f); return 2; }
            nr++;
        } else if (!strcmp(k, "BLOCK")) {
            Block *b; int i, ok = 0; if (nb >= MAXB) { fprintf(stderr, "too many blocks\n"); fclose(f); return 2; }
            b = &B[nb]; snprintf(b->sheet, sizeof b->sheet, "%s", fld(&p)); b->idx = atoi(fld(&p)); snprintf(b->recipe, sizeof b->recipe, "%s", fld(&p));
            for (i = 0; i < nr; i++) if (!strcmp(R[i].name, b->recipe)) ok = 1;
            if (!ok || b->idx < 0) { fprintf(stderr, "bad BLOCK (recipe %s must be defined first)\n", b->recipe); fclose(f); return 2; }
            nb++;
        }
    }
    fclose(f); return 0;
}
/* one weight from the bank spoke ground_<recipe>; returns 1 when found, *bad set when outside the master's range */
static int bank_weight(const char *recipe, const char *master, double lo, double *w, int *bad) {
    char path[900], line[300]; FILE *f; int found = 0;
    *w = 0.0; if (!g_bank[0]) return 0;
    snprintf(path, sizeof path, "%.600s/data/spokes/ground_%.40s.pdl", g_bank, recipe);
    f = fopen(path, "r"); if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *p = strstr(line, "POINTS_TO="), *q;
        if (strncmp(line, "SLOT", 4) != 0 || !p) continue;
        p += 10; q = p; while (*q && *q != ' ' && *q != '|' && *q != '\n') q++;
        if ((size_t)(q - p) == strlen(master) && !strncmp(p, master, (size_t)(q - p))) { char *wv = strstr(line, "WEIGHT="); if (!wv) continue; *w = atof(wv + 7); found = 1; if (*w < lo || *w > 1.0) *bad = 1; }
    }
    fclose(f); return found;
}
static void hsl(double h, double s, double l, unsigned char *o) {
    double c = (1 - fabs(2 * l - 1)) * s, hp = fmod(h, 360.0) / 60.0, x = c * (1 - fabs(fmod(hp, 2.0) - 1)), r = 0, g = 0, b = 0, m = l - c / 2;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; } else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    r = (r + m) * 255; g = (g + m) * 255; b = (b + m) * 255;
    o[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r + .5); o[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g + .5); o[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b + .5);
}
static double lat(int i, int j, int cx, int cy, unsigned seed) { return (hash32(seed ^ (unsigned)i * 374761393u ^ (unsigned)j * 668265263u ^ (unsigned)(cx * 31 + cy)) & 1023) / 1023.0; }
static double vn(int x, int y, int cx, int cy, unsigned seed) { /* value noise periodic over 24 px: cx, cy must divide 24 */
    double fx = x / (24.0 / cx), fy = y / (24.0 / cy), tx, ty, a, b, c, d; int i = (int)fx, j = (int)fy;
    tx = fx - i; ty = fy - j; tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
    a = lat(i % cx, j % cy, cx, cy, seed); b = lat((i + 1) % cx, j % cy, cx, cy, seed); c = lat(i % cx, (j + 1) % cy, cx, cy, seed); d = lat((i + 1) % cx, (j + 1) % cy, cx, cy, seed);
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
}
/* texture value 0..1 at (x,y) (taken mod 24), or -1 for a mortar / joint line */
static double texture(const Recipe *r, int x, int y) {
    x = ((x % 24) + 24) % 24; y = ((y % 24) + 24) % 24;
    if (!strcmp(r->pattern, "cobble")) {
        double best = 1e9, second = 1e9; int bi = 0, i, j, ox, oy;
        for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
            double px = i * 8 + 4 + ((int)(hash32(r->seed ^ (unsigned)(i * 7 + j * 13)) % 5) - 2), py = j * 8 + 4 + ((int)(hash32(r->seed ^ (unsigned)(i * 11 + j * 17 + 5)) % 5) - 2);
            for (ox = -24; ox <= 24; ox += 24) for (oy = -24; oy <= 24; oy += 24) {
                double dx = x - (px + ox), dy = y - (py + oy), dd = sqrt(dx * dx + dy * dy);
                if (dd < best) { second = best; best = dd; bi = i * 3 + j; } else if (dd < second) second = dd;
            }
        }
        if (second - best < 1.7) return -1.0;
        return 0.25 + 0.5 * ((hash32(r->seed ^ (unsigned)bi * 977u) & 255) / 255.0) + 0.2 * (vn(x, y, 8, 8, r->seed) - 0.5);
    }
    if (!strcmp(r->pattern, "plank")) {
        int row = y / 6, off = (int)(hash32(r->seed ^ (unsigned)row) % 24), xx = (x + off) % 24;
        if (y % 6 == 0 || xx == 0) return -1.0;
        return 0.3 + 0.4 * ((hash32(r->seed ^ (unsigned)row * 31u) & 255) / 255.0) + 0.25 * (vn(x * 3 % 24, y, 4, 12, r->seed) - 0.5);
    }
    if (!strcmp(r->pattern, "blades")) return 0.15 + 0.7 * (0.6 * vn(x, y, 12, 4, r->seed) + 0.4 * vn(x, y, 6, 12, r->seed + 7));
    return 0.65 * vn(x, y, r->scale == 1 ? 6 : r->scale == 2 ? 4 : 3, r->scale == 1 ? 6 : r->scale == 2 ? 4 : 3, r->seed) + 0.35 * vn(x, y, 12, 12, r->seed + 3);
}
static double angdiff(double to, double from) { double d = fmod(to - from + 540.0, 360.0) - 180.0; return d; } /* shortest signed angle from -> to */
static void ramp5(const Recipe *r, double warm, double wet, unsigned char ramp[5][3]) {
    int k; double h = r->hue + (warm >= 0 ? warm * 0.45 * angdiff(30.0, r->hue) : -warm * 0.45 * angdiff(220.0, r->hue)); /* warm: toward orange; cool: toward blue */
    for (k = 0; k < 5; k++) {
        double l = r->light / 100.0 + ((k - 2) / 4.0) * (r->contrast / 100.0) - wet * 0.10, s = r->sat / 100.0 * (1.0 - 0.08 * (k - 2)) + wet * 0.12;
        l = clampd(l, 0.04, 0.96); s = clampd(s, 0, 1);
        hsl(h, s, l, ramp[k]);
    }
}
static const Recipe *find_recipe(const char *name) { int i; for (i = 0; i < nr; i++) if (!strcmp(R[i].name, name)) return &R[i]; return NULL; }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
/* paint one 96 x 144 block: alpha from the original block, colours generated */
static int paint_block(const unsigned char *src, int sw, int bx, int by, unsigned char *dst, int dw, int ox, int oy, const Recipe *r, double warm, double wet, double wear, double grain, double edge) {
    int x, y, ring = 2 + (int)(edge * 4.0 + 0.5), dy, dx, painted = 0, open_below = 0; unsigned char ramp[5][3];
    for (y = 48; y < 144 && !open_below; y++) for (x = 0; x < 96; x++) if (src[((size_t)(by + y) * sw + bx + x) * 4 + 3] == 0) { open_below = 1; break; } /* a lower region with no transparency is a flat floor tile: no border to draw */
    ramp5(r, warm, wet, ramp);
    for (y = 0; y < 144; y++) for (x = 0; x < 96; x++) {
        const unsigned char *s = src + ((size_t)(by + y) * sw + bx + x) * 4; unsigned char *d = dst + ((size_t)(oy + y) * dw + ox + x) * 4;
        int bottom = y >= 48, dist = 99; double v; int step; unsigned char c[3];
        if (s[3] == 0) continue;
        for (dy = -RING_MAX; dy <= RING_MAX && dist > 1; dy++) for (dx = -RING_MAX; dx <= RING_MAX; dx++) {
            int yy = y + dy, xx = x + dx, dd, tr;
            if (!dx && !dy) continue;
            if (xx < 0 || xx >= 96 || yy < 0 || yy >= 144) tr = bottom && open_below; /* outside the block: transparent under a 96 x 96 rounded square, opaque in the top row or under a flat floor tile */
            else if (bottom && yy < 48) tr = open_below;
            else tr = src[((size_t)(by + yy) * sw + bx + xx) * 4 + 3] == 0;
            if (!tr) continue;
            dd = abs(dx) > abs(dy) ? abs(dx) : abs(dy); if (dd < dist) dist = dd;
        }
        v = texture(r, x, y);
        if (v < 0) { step = 0; v = 0.1; } else {
            double gr = ((hash32((r->seed + 91u) ^ (unsigned)(x % 24) * 7919u ^ (unsigned)(y % 24) * 104729u) & 255) / 255.0 - 0.5) * (0.12 + 0.5 * grain);
            v = clampd(v + gr, 0, 0.999); step = (int)(v * 5);
        }
        if (wet > 0 && v > 0.93 - 0.1 * wet && (hash32(r->seed ^ (unsigned)(x % 24) * 31u ^ (unsigned)(y % 24) * 17u) & 3) == 0) step = 4;
        if (wear > 0 && (hash32((r->seed + 5u) ^ (unsigned)(x % 24) * 13u ^ (unsigned)(y % 24) * 101u) % 1000) < (unsigned)(wear * 110.0)) step = step > 0 ? step - 2 : 0;
        if (step < 0) step = 0;
        memcpy(c, ramp[step], 3);
        if (dist <= ring) { /* edge ring: ink at the outermost step, then the dark ramp end */
            double t = dist == 1 ? 0.45 : dist == 2 ? 0.78 : 0.92; int k;
            for (k = 0; k < 3; k++) c[k] = (unsigned char)(ramp[0][k] * t + (dist == 1 ? 0 : 0));
            if (dist > 2) for (k = 0; k < 3; k++) c[k] = (unsigned char)((c[k] + ramp[step][k]) / 2);
        }
        d[0] = c[0]; d[1] = c[1]; d[2] = c[2]; d[3] = s[3]; painted++;
    }
    return painted;
}
static int ground(char **argv) {
    int w = 0, h = 0, n = 0, i, blocks = 0, rc = 0, bpr; unsigned char *img, *out; char sname[100], *base, *dot;
    if (read_rows(argv[2])) return 2;
    img = stbi_load(argv[3], &w, &h, &n, 4);
    if (!img) { fprintf(stderr, "cannot read PNG %s\n", argv[3]); return 2; }
    if (w % 96 || h % 144) { fprintf(stderr, "sheet is not a multiple of 96 x 144\n"); stbi_image_free(img); return 2; }
    out = calloc((size_t)w * h, 4); if (!out) { stbi_image_free(img); return 2; }
    base = strrchr(argv[3], '/'); base = base ? base + 1 : argv[3]; snprintf(sname, sizeof sname, "%.90s", base); dot = strrchr(sname, '.'); if (dot) *dot = 0;
    bpr = w / 96;
    for (i = 0; i < nb && !rc; i++) {
        double wt[5] = {0, 0, 0, 0, 0}; int bad = 0, k, bc, br, sel; const Recipe *r;
        if (strcmp(B[i].sheet, sname)) continue;
        if (B[i].idx >= bpr * (h / 144)) { fprintf(stderr, "no block %d\n", B[i].idx); rc = 2; break; }
        r = find_recipe(B[i].recipe);
        bank_weight(r->name, "ground_warm", -1.0, &wt[0], &bad); bank_weight(r->name, "ground_wet", 0.0, &wt[1], &bad); bank_weight(r->name, "ground_wear", 0.0, &wt[2], &bad);
        bank_weight(r->name, "ground_grain", 0.0, &wt[3], &bad); bank_weight(r->name, "ground_edge", 0.0, &wt[4], &bad);
        if (bad) { fprintf(stderr, "bank weight out of range for ground_%s\n", r->name); rc = 2; break; }
        bc = B[i].idx % bpr; br = B[i].idx / bpr;
        sel = !g_sel_on || (bc >= g_sel[0] && bc < g_sel[0] + g_sel[2] && br >= g_sel[1] && br < g_sel[1] + g_sel[3]);
        if (g_knob_on && sel) for (k = 0; k < 5; k++) wt[k] += g_knob[k];
        wt[0] = clampd(wt[0], -1, 1); for (k = 1; k < 5; k++) wt[k] = clampd(wt[k], 0, 1);
        paint_block(img, w, bc * 96, br * 144, out, w, bc * 96, br * 144, r, wt[0], wt[1], wt[2], wt[3], wt[4]);
        blocks++;
    }
    if (!rc && blocks == 0) { fprintf(stderr, "no BLOCK row for sheet %s\n", sname); rc = 2; }
    if (!rc && g_crop_on) {
        int cw = g_crop[2] * 96, ch = g_crop[3] * 144, y; unsigned char *cr;
        if (g_crop[0] < 0 || g_crop[1] < 0 || g_crop[2] < 1 || g_crop[3] < 1 || (g_crop[0] + g_crop[2]) * 96 > w || (g_crop[1] + g_crop[3]) * 144 > h) { fprintf(stderr, "crop outside the sheet\n"); rc = 2; }
        else { cr = malloc((size_t)cw * ch * 4); if (!cr) rc = 2; else { for (y = 0; y < ch; y++) memcpy(cr + (size_t)y * cw * 4, out + ((size_t)(g_crop[1] * 144 + y) * w + g_crop[0] * 96) * 4, (size_t)cw * 4);
            if (!stbi_write_png(argv[4], cw, ch, 4, cr, cw * 4)) rc = 2; else printf("GROUND crop=%d,%d,%d,%d size=%dx%d blocks=%d\n", g_crop[0], g_crop[1], g_crop[2], g_crop[3], cw, ch, blocks); free(cr); } }
    } else if (!rc) { if (!stbi_write_png(argv[4], w, h, 4, out, w * 4)) rc = 2; else printf("GROUND size=%dx%d blocks=%d\n", w, h, blocks); }
    stbi_image_free(img); free(out); return rc;
}
int main(int argc, char **argv) {
    while (argc > 3 && (!strncmp(argv[argc - 1], "bank=", 5) || !strncmp(argv[argc - 1], "knob=", 5) || !strncmp(argv[argc - 1], "select=", 7) || !strncmp(argv[argc - 1], "crop=", 5))) {
        char *a = argv[argc - 1];
        if (!strncmp(a, "bank=", 5)) snprintf(g_bank, sizeof g_bank, "%s", a + 5);
        else if (!strncmp(a, "knob=", 5)) { char *cl = strchr(a + 5, ':'); int k, hit = 0; if (cl) { *cl = 0; for (k = 0; k < 5; k++) if (!strcmp(a + 5, KN[k])) { g_knob[k] += atof(cl + 1); g_knob_on = hit = 1; } } if (!hit) { fprintf(stderr, "bad knob (use warm|wet|wear|grain|edge:<value>)\n"); return 2; } }
        else if (!strncmp(a, "select=", 7)) { if (sscanf(a + 7, "%d,%d,%d,%d", &g_sel[0], &g_sel[1], &g_sel[2], &g_sel[3]) != 4 || g_sel[2] < 1 || g_sel[3] < 1) { fprintf(stderr, "bad select (bc,br,bw,bh)\n"); return 2; } g_sel_on = 1; }
        else if (sscanf(a + 5, "%d,%d,%d,%d", &g_crop[0], &g_crop[1], &g_crop[2], &g_crop[3]) != 4) { fprintf(stderr, "bad crop (bc,br,bw,bh)\n"); return 2; } else g_crop_on = 1;
        argc--;
    }
    if (argc == 3 && !strcmp(argv[1], "check")) { int rc = read_rows(argv[2]); if (!rc) printf("GROUNDCHECK recipes=%d blocks=%d\n", nr, nb); return rc; }
    if (argc == 5 && !strcmp(argv[1], "ground")) return ground(argv);
    fprintf(stderr, "usage: ground <ground.pdl> <original-sheet.png> <out.png> [bank=<dir>] [knob=name:delta] [select=bc,br,bw,bh] [crop=bc,br,bw,bh] | check <ground.pdl>\n");
    return 2;
}
