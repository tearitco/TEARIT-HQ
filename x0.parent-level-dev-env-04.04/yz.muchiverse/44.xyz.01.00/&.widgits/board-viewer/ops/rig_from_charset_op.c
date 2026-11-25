/* rig_from_charset_op <charset.png> <char_index 0..7> <out_rig.pdl> [convert.pdl]
 *
 * Turns the FRONT standing frame of an RPG Maker MV character sprite into a box-limb rig colour file (BOOK 16.game/BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md,
 * header board-viewer/ops/bv_rig.h). The output is the RIG rows `RIG | head|hair|torso|sleeve|arm|leg|shoe|face | r g b`; no texture, no pose.
 *
 * Sheet layout: a normal sheet is 12 columns x 8 rows of frames (4 characters across, 2 down, each 3 x 4 frames); frame = sheet_w/12 x sheet_h/8.
 *   A sheet whose file BASENAME starts with `$` is ONE character (3 x 4 frames, frame = sheet_w/3 x sheet_h/4) and char_index is ignored.
 *   A sheet whose shape is 3:4 (width*4 == height*3, e.g. the generator-hq cast sheets) is also one character.
 *   The frame read is the down-facing standing one: middle column, first row of the character's block.
 * Settings (fractions of the opaque figure height, thresholds) are DATA in board-viewer/data/rig_convert.pdl (`CONVERT | key | value`), found beside this op's folder
 *   (ops/+x/../../data) unless a 4th argument names another file (the harness mutant does).
 * Regions: head 0..head_end, torso head_end..torso_end, legs the rest. Colours are the MODE of a 5-bit-per-channel histogram (mean of the winning bucket) over pixels with
 *   alpha >= alpha_min, ignoring near-black outlines (every channel <= outline_max).
 *   skin = head, rows skin_band_lo..hi of the head; hair = head top hair_top_frac and the outer hair_side_frac columns, minus skin-like pixels (no hair left = no hair row, a bald figure);
 *   torso = middle half of the torso rows; side = the outer sleeve_side_frac columns of the torso rows: if side is skin-like the arm is skin and the sleeve is the torso colour,
 *   else arm = sleeve = side; leg = legs minus the bottom shoe_frac; shoe = the bottom shoe_frac of the legs; face = the commonest non-skin non-hair pixel of the eye band (omitted if none).
 * Exit: 0 written | 2 usage / bad arguments | 3 no opaque pixel in the chosen block | 4 PNG unreadable | 5 settings file missing or unusable | 6 output not written.
 * The output is written to <out>.tmp and renamed, so a failed run leaves no half file.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rig_from_charset_op.+x ops/rig_from_charset_op.c -lpng -lz -lm
 */
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <libgen.h>

typedef struct { double head_end, torso_end, hair_top, hair_side, skin_lo, skin_hi, eye_lo, eye_hi, sleeve_side, shoe, same_dist; int alpha_min, outline_max, shift; int got; } Cfg;
#define CFG_KEYS 14

static unsigned char *load_png(const char *path, int *w, int *h) {
    FILE *fp = fopen(path, "rb"); if (!fp) return NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return NULL; }
    png_infop info = png_create_info_struct(png);
    unsigned char *img = NULL; png_bytep *rows = NULL;
    if (!info || setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png, info ? &info : NULL, NULL); fclose(fp); free(img); free(rows); return NULL; }
    png_init_io(png, fp); png_read_info(png, info);
    *w = (int)png_get_image_width(png, info); *h = (int)png_get_image_height(png, info);
    png_byte ct = png_get_color_type(png, info);
    if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (png_get_bit_depth(png, info) == 16) png_set_scale_16(png);
    if ((ct & PNG_COLOR_MASK_ALPHA) == 0) png_set_filler(png, 255, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    int rowb = (int)png_get_rowbytes(png, info);
    img = malloc((size_t)*h * (size_t)rowb); rows = malloc((size_t)*h * sizeof(png_bytep));
    if (!img || !rows) longjmp(png_jmpbuf(png), 1);
    for (int y = 0; y < *h; y++) rows[y] = img + (size_t)y * (size_t)rowb;
    png_read_image(png, rows);
    free(rows); png_destroy_read_struct(&png, &info, NULL); fclose(fp);
    return img;                                              /* RGBA, 4 bytes a pixel */
}

static int load_cfg(const char *path, Cfg *c) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    char line[256]; memset(c, 0, sizeof *c);
    static const char *k[CFG_KEYS] = { "head_end", "torso_end", "hair_top_frac", "hair_side_frac", "skin_band_lo", "skin_band_hi", "eye_band_lo", "eye_band_hi", "sleeve_side_frac", "shoe_frac", "same_colour_dist", "alpha_min", "outline_max", "quant_shift" };
    while (fgets(line, sizeof line, f)) {
        char a[16], key[64]; double v;
        if (sscanf(line, " %15[^| ] | %63[^| ] | %lf", a, key, &v) != 3 || strcmp(a, "CONVERT")) continue;
        double *d[11] = { &c->head_end, &c->torso_end, &c->hair_top, &c->hair_side, &c->skin_lo, &c->skin_hi, &c->eye_lo, &c->eye_hi, &c->sleeve_side, &c->shoe, &c->same_dist };
        for (int i = 0; i < CFG_KEYS; i++) if (!strcmp(key, k[i])) {
            if (i < 11) *d[i] = v; else if (i == 11) c->alpha_min = (int)v; else if (i == 12) c->outline_max = (int)v; else c->shift = (int)v;
            c->got |= 1 << i;
        }
    }
    fclose(f);
    if (c->got != (1 << CFG_KEYS) - 1 || c->shift < 3 || c->shift > 6 || c->head_end <= 0 || c->torso_end <= c->head_end || c->torso_end >= 1) return -1;
    return 0;
}

/* histogram accumulator: mode bucket, mean of the bucket */
static int g_shift;
static unsigned g_cnt[32768]; static unsigned long g_sr[32768], g_sg[32768], g_sb[32768];
static unsigned g_total;
static void acc_reset(void) { memset(g_cnt, 0, sizeof g_cnt); memset(g_sr, 0, sizeof g_sr); memset(g_sg, 0, sizeof g_sg); memset(g_sb, 0, sizeof g_sb); g_total = 0; }
static void acc_add(int r, int g, int b) {
    int bits = 8 - g_shift; unsigned i = (((unsigned)r >> g_shift) << (2 * bits)) | (((unsigned)g >> g_shift) << bits) | ((unsigned)b >> g_shift);
    g_cnt[i]++; g_sr[i] += (unsigned)r; g_sg[i] += (unsigned)g; g_sb[i] += (unsigned)b; g_total++;
}
static int acc_get(int out[3]) {                              /* 0 when empty */
    if (!g_total) return 0;
    unsigned best = 0, bi = 0, n = 1u << (3 * (8 - g_shift));
    for (unsigned i = 0; i < n; i++) if (g_cnt[i] > best) { best = g_cnt[i]; bi = i; }
    out[0] = (int)(g_sr[bi] / best); out[1] = (int)(g_sg[bi] / best); out[2] = (int)(g_sb[bi] / best);
    return 1;
}
static double dist(const int *a, const int *b) { return sqrt((double)((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]))); }

static const unsigned char *g_px; static int g_stride, g_fx, g_fy;
static const unsigned char *P(int x, int y) { return g_px + (size_t)(g_fy + y) * (size_t)g_stride + (size_t)(g_fx + x) * 4; }
static Cfg C;
static int opaque(const unsigned char *p) { return p[3] >= C.alpha_min; }
static int outline(const unsigned char *p) { return p[0] <= C.outline_max && p[1] <= C.outline_max && p[2] <= C.outline_max; }

/* mode over rows [ya,yb) and columns [xa,xb) that are opaque and not outline; if skip != NULL pixels within same_dist of skip[0] / skip[1] (when non-NULL) are left out */
static int region(int ya, int yb, int xa, int xb, const int *skip0, const int *skip1, int out[3]) {
    for (int pass = 0; pass < 2; pass++) {              /* pass 1 lets outline-dark pixels count: a black suit is a colour when nothing else is there */
    acc_reset();
    for (int y = ya; y < yb; y++) for (int x = xa; x < xb; x++) {
        const unsigned char *p = P(x, y); if (!opaque(p) || (!pass && outline(p))) continue;
        int c[3] = { p[0], p[1], p[2] };
        if (skip0 && dist(c, skip0) < C.same_dist) continue;
        if (skip1 && dist(c, skip1) < C.same_dist) continue;
        acc_add(c[0], c[1], c[2]);
    }
    if (acc_get(out)) return 1;
    }
    return 0;
}

static void extent(int ya, int yb, int fw, int *x0, int *x1) {   /* opaque columns of rows [ya,yb) */
    *x0 = fw; *x1 = -1;
    for (int y = ya; y < yb; y++) for (int x = 0; x < fw; x++) if (opaque(P(x, y))) { if (x < *x0) *x0 = x; if (x > *x1) *x1 = x; }
    if (*x1 < 0) { *x0 = 0; *x1 = fw - 1; }
}

int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) { fprintf(stderr, "usage: rig_from_charset_op <charset.png> <char_index 0..7> <out_rig.pdl> [convert.pdl]\n"); return 2; }
    char *end; long idx = strtol(argv[2], &end, 10);
    if (*end || idx < 0 || idx > 7) { fprintf(stderr, "char_index must be 0..7\n"); return 2; }
    char cfgp[4096];
    if (argc == 5) snprintf(cfgp, sizeof cfgp, "%s", argv[4]);
    else { char self[4096]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return 5; self[n] = 0; snprintf(cfgp, sizeof cfgp, "%s/../../data/rig_convert.pdl", dirname(self)); }
    if (load_cfg(cfgp, &C)) { fprintf(stderr, "settings missing or incomplete: %s\n", cfgp); return 5; }
    g_shift = C.shift;
    int w = 0, h = 0; unsigned char *img = load_png(argv[1], &w, &h);
    if (!img) { fprintf(stderr, "cannot read PNG %s\n", argv[1]); return 4; }
    char bn[4096]; snprintf(bn, sizeof bn, "%s", argv[1]); const char *base = basename(bn);
    int single = base[0] == '$' || w * 4 == h * 3;
    int fw = single ? w / 3 : w / 12, fh = single ? h / 4 : h / 8;
    if (fw < 4 || fh < 8) { fprintf(stderr, "sheet too small\n"); free(img); return 4; }
    int bx = single ? 0 : (int)(idx % 4) * 3 * fw, by = single ? 0 : (int)(idx / 4) * 4 * fh;
    g_px = img; g_stride = w * 4; g_fx = bx + fw; g_fy = by;   /* down-facing standing frame: middle column, first row of the block */
    int miny = fh, maxy = -1;
    for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) if (opaque(P(x, y))) { if (y < miny) miny = y; if (y > maxy) maxy = y; }
    if (maxy < 0) { fprintf(stderr, "no opaque pixel in the chosen block\n"); free(img); return 3; }
    int H = maxy - miny + 1;
    int hy1 = miny + (int)floor(C.head_end * H + 0.5), ty1 = miny + (int)floor(C.torso_end * H + 0.5);
    if (hy1 <= miny) hy1 = miny + 1;
    if (ty1 <= hy1) ty1 = hy1 + 1;
    if (ty1 > maxy) ty1 = maxy;
    int hh = hy1 - miny;
    int all[3] = { 128, 128, 128 }; region(miny, maxy + 1, 0, fw, NULL, NULL, all);

    int skin[3], hair[3], torso[3], side[3], sleeve[3], arm[3], leg[3], shoe[3], eye[3];
    int hx0, hx1; extent(miny, hy1, fw, &hx0, &hx1); int hw = hx1 - hx0 + 1;
    int sa = miny + (int)floor(C.skin_lo * hh), sb = miny + (int)ceil(C.skin_hi * hh); if (sb <= sa) sb = sa + 1;
    if (!region(sa, sb, hx0, hx1 + 1, NULL, NULL, skin) && !region(miny, hy1, hx0, hx1 + 1, NULL, NULL, skin)) memcpy(skin, all, sizeof skin);
    /* hair: head top rows and the outer side columns, not skin-like */
    acc_reset();
    int hair_rows = (int)ceil(C.hair_top * hh), side_cols = (int)ceil(C.hair_side * hw);
    for (int y = miny; y < hy1; y++) for (int x = hx0; x <= hx1; x++) {
        if (!((y - miny) < hair_rows || x - hx0 < side_cols || hx1 - x < side_cols)) continue;
        const unsigned char *p = P(x, y); if (!opaque(p) || outline(p)) continue;
        int c[3] = { p[0], p[1], p[2] }; if (dist(c, skin) < C.same_dist) continue; acc_add(c[0], c[1], c[2]);
    }
    int has_hair = acc_get(hair);
    int tx0, tx1; extent(hy1, ty1, fw, &tx0, &tx1); int tw = tx1 - tx0 + 1;
    if (!region(hy1, ty1, tx0 + tw / 4, tx1 - tw / 4 + 1, NULL, NULL, torso)) memcpy(torso, all, sizeof torso);
    int sc = (int)ceil(C.sleeve_side * tw); if (sc < 1) sc = 1;
    acc_reset();
    for (int y = hy1; y < ty1; y++) for (int x = tx0; x <= tx1; x++) {
        if (!(x - tx0 < sc || tx1 - x < sc)) continue;
        const unsigned char *p = P(x, y); if (!opaque(p) || outline(p)) continue; acc_add(p[0], p[1], p[2]);
    }
    if (!acc_get(side)) memcpy(side, torso, sizeof side);
    if (dist(side, skin) < C.same_dist) { memcpy(arm, skin, sizeof arm); memcpy(sleeve, torso, sizeof sleeve); }   /* bare arms: skin arm, short sleeve in the shirt colour */
    else { memcpy(arm, side, sizeof arm); memcpy(sleeve, side, sizeof sleeve); }
    int lh = maxy + 1 - ty1, sh = (int)ceil(C.shoe * lh); if (sh < 1) sh = 1; if (sh >= lh) sh = lh > 1 ? lh - 1 : 0;
    int lx0, lx1; extent(ty1, maxy + 1, fw, &lx0, &lx1);
    if (!region(ty1, maxy + 1 - sh, lx0, lx1 + 1, NULL, NULL, leg)) { if (!region(ty1, maxy + 1, lx0, lx1 + 1, NULL, NULL, leg)) memcpy(leg, all, sizeof leg); }
    if (!sh || !region(maxy + 1 - sh, maxy + 1, lx0, lx1 + 1, NULL, NULL, shoe)) memcpy(shoe, leg, sizeof shoe);
    int has_eye = region(miny + (int)floor(C.eye_lo * hh), miny + (int)ceil(C.eye_hi * hh), hx0, hx1 + 1, skin, has_hair ? hair : NULL, eye);

    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", argv[3]);
    FILE *o = fopen(tmp, "w"); if (!o) { free(img); return 6; }
    fprintf(o, "# rig colours from %s (char %ld, frame %dx%d, figure rows %d..%d) by rig_from_charset_op; no texture, boxes only\n", base, idx, fw, fh, miny, maxy);
    fprintf(o, "RIG | head | %d %d %d\n", skin[0], skin[1], skin[2]);
    if (has_hair) fprintf(o, "RIG | hair | %d %d %d\n", hair[0], hair[1], hair[2]);
    fprintf(o, "RIG | torso | %d %d %d\nRIG | sleeve | %d %d %d\nRIG | arm | %d %d %d\nRIG | leg | %d %d %d\nRIG | shoe | %d %d %d\n",
            torso[0], torso[1], torso[2], sleeve[0], sleeve[1], sleeve[2], arm[0], arm[1], arm[2], leg[0], leg[1], leg[2], shoe[0], shoe[1], shoe[2]);
    if (has_eye) fprintf(o, "RIG | face | %d %d %d\n", eye[0], eye[1], eye[2]);
    if (fclose(o) || rename(tmp, argv[3])) { remove(tmp); free(img); return 6; }
    free(img);
    printf("wrote %s\n", argv[3]);
    return 0;
}
