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
* Also writes <out minus .pdl>.skin.csv: a Minecraft-style SKIN (part,face,u,v,r,g,b: head 8x8x8, torso 8x12x4, limbs 4x12x4, every face sampled from the four standing frames) for the voxel-model rig renderer.
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

/* Profile mode (2026-11-25, chibi sheets): OPTIONAL keys of the same data file. profile_mode 1 finds the face as the first band of skin-hued pixels and anchors the
 * head on the chin instead of a fixed fraction; a file without these keys keeps the old fixed-fraction behaviour (the harness mutants and the first 51 checks rely on it). */
typedef struct { int profile; double skin_r_min, skin_g_lo, skin_g_hi, skin_b_lo, skin_b_hi, body_torso, chin_min_px; } Prof;
static Prof PR;
static void load_prof(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return; char line[256]; memset(&PR, 0, sizeof PR);
    while (fgets(line, sizeof line, f)) {
        char a[16], key[64]; double v; if (sscanf(line, " %15[^| ] | %63[^| ] | %lf", a, key, &v) != 3 || strcmp(a, "CONVERT")) continue;
        if (!strcmp(key, "profile_mode")) PR.profile = (int)v; else if (!strcmp(key, "skin_r_min")) PR.skin_r_min = v; else if (!strcmp(key, "skin_g_lo")) PR.skin_g_lo = v;
        else if (!strcmp(key, "skin_g_hi")) PR.skin_g_hi = v; else if (!strcmp(key, "skin_b_lo")) PR.skin_b_lo = v; else if (!strcmp(key, "skin_b_hi")) PR.skin_b_hi = v;
        else if (!strcmp(key, "body_torso_frac")) PR.body_torso = v; else if (!strcmp(key, "chin_min_px")) PR.chin_min_px = v;
    }
    fclose(f);
    if (PR.profile && (PR.skin_r_min <= 0 || PR.skin_g_hi <= PR.skin_g_lo || PR.skin_b_hi <= PR.skin_b_lo || PR.body_torso <= 0 || PR.body_torso >= 1 || PR.chin_min_px < 1)) PR.profile = 0;
}

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
static int g_skipskin;      /* profile mode: skin-hued pixels never count as clothing (torso, sleeves, legs) */
static int skinlike(const unsigned char *p);
static int region(int ya, int yb, int xa, int xb, const int *skip0, const int *skip1, int out[3]) {
    for (int pass = 0; pass < 2; pass++) {              /* pass 1 lets outline-dark pixels count: a black suit is a colour when nothing else is there */
    acc_reset();
    for (int y = ya; y < yb; y++) for (int x = xa; x < xb; x++) {
        const unsigned char *p = P(x, y); if (!opaque(p) || (!pass && outline(p))) continue; if (g_skipskin && skinlike(p)) continue;
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

/* skin-hued: red dominant and the green / blue ratios to red inside the skin ranges of the data file (hair that is redder or yellower fails the ratios) */
static int skinlike(const unsigned char *p) {
    if (!opaque(p) || p[0] < PR.skin_r_min) return 0;
    double g = (double)p[1] / p[0], b = (double)p[2] / p[0];
    return g >= PR.skin_g_lo && g <= PR.skin_g_hi && b >= PR.skin_b_lo && b <= PR.skin_b_hi;
}
/* the chin: the last row of the band of rows (a gap of one row is allowed) that holds at least chin_min_px skin-hued pixels and has the MOST skin pixels in all, searched in the top 75%
 * of the figure. The first band is wrong on sprites whose hair highlights are skin-hued (Harold): the face is the biggest band. g_face_top = the first row of that band.
 * Returns -1 when there is no such band (a sheet without a visible face: the fixed fractions are used). */
static int g_face_top;
static int find_chin(int fw, int miny, int maxy) {
    int limit = miny + (int)((maxy - miny + 1) * 0.75), start = -1, last = -1, gap = 0, sum = 0, best_sum = 0, best_last = -1, best_start = -1;
    for (int y = miny; y <= limit + 1; y++) {
        int n = 0; if (y <= limit) for (int x = 0; x < fw; x++) if (skinlike(P(x, y))) n++;
        if (y <= limit && n >= (int)PR.chin_min_px) { if (start < 0) start = y; last = y; gap = 0; sum += n; continue; }
        if (start >= 0 && ++gap > 1) { if (sum > best_sum) { best_sum = sum; best_last = last; best_start = start; } start = -1; sum = 0; gap = 0; }
        else if (start >= 0 && y > limit) { if (sum > best_sum) { best_sum = sum; best_last = last; best_start = start; } start = -1; }
    }
    g_face_top = best_start; return best_last;
}

/* ---- SKIN (owner 2026-11-25: "wrapping skin ... more than one colour as a pattern of the sprite ... it's CSV"): a Minecraft-style skin, one colour per surface voxel of every limb, sampled from the four standing frames
 * (rows: 0 down = front, 1 left, 2 right, 3 up = back). Written as CSV beside the rig file (<name>.skin.csv): `part,face,u,v,r,g,b`. Parts and their voxel boxes (w x h x d, Minecraft's): head 8x8x8, torso 8x12x4,
 * rarm / larm 4x12x4, rleg / lleg 4x12x4. Faces: F front (w x h), B back (w x h), L left (d x h), R right (d x h), T top (w x d), D bottom (w x d). u runs left to right and v top to bottom AS SEEN FROM OUTSIDE the
 * face. rarm / rleg are the figure's right limbs (on the viewer's LEFT in the front frame, on the viewer's RIGHT in the back frame). A cell is the average of the opaque, non-outline pixels under it
 * (outline pixels only when nothing else is there); a cell with no pixel takes the part's base colour. The renderer turns this into small voxel models (one box slot per limb, the box size is the scale). */
static int g_frow, g_fh_;
static void set_frame(int row, int by, int fh) { g_fy = by + row * fh; g_frow = row; g_fh_ = fh; }
/* SKIN settings (OPTIONAL keys of rig_convert.pdl; a file without them keeps the plain average, which the first skin used). All tuning is DATA, never C:
 *   skin_mode 0 | 1      0 = mean of the cell's pixels (soft, blurs eyes and outlines); 1 = DOMINANT colour: the most common colour bucket of the cell's pixels, so flat areas stay flat
 *   skin_quant 3..6      bucket coarseness for the dominant colour (bits dropped per channel; higher = coarser, fewer distinct colours)
 *   skin_detail_frac     0 = off; else a minority colour that covers at least this fraction of the cell's pixels and differs from the dominant by more than skin_detail_dist WINS (keeps an eye or a mouth that is a third of a cell)
 *   skin_detail_dist     colour distance (sum of channel differences) a minority must exceed to count as detail
 *   skin_outline_frac    0 = outline (very dark) pixels only count when nothing else is in the cell; else a cell whose opaque pixels are at least this fraction outline becomes the outline colour (crisp edges, a dark seam between limbs)
 *   skin_hand_rows 0..4  the last N rows of each ARM (all four sides) and the bottom face are the SKIN tone: the hand. The sprite's hands are a few pixels under the torso band that the arm cells sample, so without this the arm ends in the sleeve colour (0 = off)
 *   skin_limb_rows 0 | 1 1 = arms and legs get ONE colour per row across their width (no pixel flecks: a limb is only about 3 sprite pixels wide, so a cell would be one pixel plus its outline); torso and head stay per cell
 *   skin_darken          0..1, multiplies every skin cell's brightness by (1 - value) on the SIDE faces L and R and the bottom D only, a cheap shading cue (0 = off) */
typedef struct { int mode, quant; double detail_frac, detail_dist, outline_frac, darken; int limb_rows, hand_rows; } Skn;
static Skn SK = { 0, 4, 0, 60, 0, 0, 0, 0 };
static int g_hand[3];   /* the skin tone, set by main before the faces are written */
static void load_skin(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return; char line[256];
    while (fgets(line, sizeof line, f)) {
        char a[16], key[64]; double v; if (sscanf(line, " %15[^| ] | %63[^| ] | %lf", a, key, &v) != 3 || strcmp(a, "CONVERT")) continue;
        if (!strcmp(key, "skin_mode")) SK.mode = (int)v; else if (!strcmp(key, "skin_quant")) SK.quant = (int)v; else if (!strcmp(key, "skin_detail_frac")) SK.detail_frac = v;
        else if (!strcmp(key, "skin_detail_dist")) SK.detail_dist = v; else if (!strcmp(key, "skin_outline_frac")) SK.outline_frac = v; else if (!strcmp(key, "skin_darken")) SK.darken = v; else if (!strcmp(key, "skin_limb_rows")) SK.limb_rows = (int)v; else if (!strcmp(key, "skin_hand_rows")) SK.hand_rows = (int)v; }
    fclose(f);
    if (SK.mode < 0 || SK.mode > 1) SK.mode = 0; if (SK.quant < 3 || SK.quant > 6) SK.quant = 4;
    if (SK.detail_frac < 0 || SK.detail_frac > 1) SK.detail_frac = 0; if (SK.outline_frac < 0 || SK.outline_frac > 1) SK.outline_frac = 0; if (SK.darken < 0 || SK.darken > 0.8) SK.darken = 0; }
static int qkey(const unsigned char *p) { int s = SK.quant; return ((p[0] >> s) << 16) | ((p[1] >> s) << 8) | (p[2] >> s); }
static void cell_dom(int x0, int y0, int x1, int y1, const int base[3], int out[3]) {
    enum { MAXP = 1024 }; int key[MAXP], rr[MAXP], gg[MAXP], bb[MAXP], n = 0, nout = 0, nopq = 0; long orr = 0, ogg = 0, obb = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { const unsigned char *p = P(x, y); if (!opaque(p)) continue; nopq++;
        if (outline(p)) { nout++; orr += p[0]; ogg += p[1]; obb += p[2]; continue; }
        if (n < MAXP) { key[n] = qkey(p); rr[n] = p[0]; gg[n] = p[1]; bb[n] = p[2]; n++; } }
    if (SK.outline_frac > 0 && nopq && (double)nout / nopq >= SK.outline_frac) { out[0] = (int)(orr / nout); out[1] = (int)(ogg / nout); out[2] = (int)(obb / nout); return; }
    if (!n) { if (nout) { out[0] = (int)(orr / nout); out[1] = (int)(ogg / nout); out[2] = (int)(obb / nout); } else memcpy(out, base, 3 * sizeof(int)); return; }
    int best = 0, bc = 0;                                              /* the most common bucket (ties: the first seen) */
    for (int i = 0; i < n; i++) { int c = 0; for (int j = 0; j < n; j++) if (key[j] == key[i]) c++; if (c > bc) { bc = c; best = key[i]; } }
    int pick = best, pc = bc;
    if (SK.detail_frac > 0) {                                          /* a distinct minority covering enough of the cell is detail, not noise */
        long mr = 0, mg = 0, mb = 0, mn = 0; for (int i = 0; i < n; i++) if (key[i] == best) { mr += rr[i]; mg += gg[i]; mb += bb[i]; mn++; }
        int dr = (int)(mr / mn), dg = (int)(mg / mn), db = (int)(mb / mn), bestc = 0;
        for (int i = 0; i < n; i++) { if (key[i] == best) continue; int c = 0; for (int j = 0; j < n; j++) if (key[j] == key[i]) c++;
            int dist = abs(rr[i] - dr) + abs(gg[i] - dg) + abs(bb[i] - db);
            if ((double)c / (n + nout) >= SK.detail_frac && dist > SK.detail_dist && c > bestc) { bestc = c; pick = key[i]; pc = c; } } }
    long r = 0, g = 0, b = 0, m = 0; for (int i = 0; i < n; i++) if (key[i] == pick) { r += rr[i]; g += gg[i]; b += bb[i]; m++; } (void)pc;
    out[0] = (int)(r / m); out[1] = (int)(g / m); out[2] = (int)(b / m); }
static void cell_avg(int x0, int y0, int x1, int y1, const int base[3], int out[3]) {
    if (SK.mode == 1) { cell_dom(x0, y0, x1, y1, base, out); return; }
    for (int pass = 0; pass < 2; pass++) { long r = 0, g = 0, b = 0, n = 0;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { const unsigned char *p = P(x, y); if (!opaque(p) || (!pass && outline(p))) continue; r += p[0]; g += p[1]; b += p[2]; n++; }
        if (n) { out[0] = (int)(r / n); out[1] = (int)(g / n); out[2] = (int)(b / n); return; } }
    memcpy(out, base, 3 * sizeof(int)); }
static void face_out(FILE *o, const char *part, char face, int w, int h, int x0, int y0, int x1, int y1, const int base[3]) {
    if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
    int rowmode = SK.limb_rows && (part[1] == 'a' || part[1] == 'l');   /* rarm larm rleg lleg */
    for (int v = 0; v < h; v++) for (int u = 0; u < w; u++) {
        int ax = x0 + u * (x1 - x0) / w, bx = x0 + (u + 1) * (x1 - x0) / w, ay = y0 + v * (y1 - y0) / h, by_ = y0 + (v + 1) * (y1 - y0) / h;  if (rowmode) { ax = x0; bx = x1; } if (bx <= ax) bx = ax + 1; if (by_ <= ay) by_ = ay + 1;
        int c[3]; cell_avg(ax, ay, bx, by_, base, c); if (SK.hand_rows > 0 && part[1] == 'a' && v >= h - SK.hand_rows) memcpy(c, g_hand, sizeof c); if (SK.darken > 0 && (face == 'L' || face == 'R' || face == 'D')) for (int q = 0; q < 3; q++) c[q] = (int)(c[q] * (1.0 - SK.darken)); fprintf(o, "%s,%c,%d,%d,%d,%d,%d\n", part, face, u, v, c[0], c[1], c[2]); } }
static void flat_out(FILE *o, const char *part, char face, int w, int h, const int c[3]) { for (int v = 0; v < h; v++) for (int u = 0; u < w; u++) { const int *cc = (SK.hand_rows > 0 && part[1] == 'a' && face != 'T' && v >= h - SK.hand_rows) ? g_hand : c; fprintf(o, "%s,%c,%d,%d,%d,%d,%d\n", part, face, u, v, cc[0], cc[1], cc[2]); } }
int main(int argc, char **argv) {
    if (argc < 4 || argc > 5) { fprintf(stderr, "usage: rig_from_charset_op <charset.png> <char_index 0..7> <out_rig.pdl> [convert.pdl]\n"); return 2; }
    char *end; long idx = strtol(argv[2], &end, 10);
    if (*end || idx < 0 || idx > 7) { fprintf(stderr, "char_index must be 0..7\n"); return 2; }
    char cfgp[4096];
    if (argc == 5) snprintf(cfgp, sizeof cfgp, "%s", argv[4]);
    else { char self[4096]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return 5; self[n] = 0; snprintf(cfgp, sizeof cfgp, "%s/../../data/rig_convert.pdl", dirname(self)); }
    if (load_cfg(cfgp, &C)) { fprintf(stderr, "settings missing or incomplete: %s\n", cfgp); return 5; }
    load_skin(cfgp);
    load_prof(cfgp);
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
    int chin = PR.profile ? find_chin(fw, miny, maxy) : -1;
    if (chin >= 0) {                                     /* profile mode: the head ends under the chin, the rest of the body is torso then legs by body_torso_frac */
        hy1 = chin + 1;
        ty1 = hy1 + (int)floor(PR.body_torso * (maxy + 1 - hy1) + 0.5);
    }
    if (hy1 <= miny) hy1 = miny + 1;
    if (ty1 <= hy1) ty1 = hy1 + 1;
    if (ty1 > maxy) ty1 = maxy;
    int hh = hy1 - miny;
    int all[3] = { 128, 128, 128 }; region(miny, maxy + 1, 0, fw, NULL, NULL, all);

    int skin[3], hair[3], torso[3], side[3], sleeve[3], arm[3], leg[3], shoe[3], eye[3];
    int hx0, hx1; extent(miny, hy1, fw, &hx0, &hx1); int hw = hx1 - hx0 + 1;
    int sa = miny + (int)floor(C.skin_lo * hh), sb = miny + (int)ceil(C.skin_hi * hh); if (sb <= sa) sb = sa + 1;
    int prof_skin = 0;
    if (chin >= 0) { acc_reset(); for (int y = g_face_top; y <= chin; y++) for (int x = hx0; x <= hx1; x++) { const unsigned char *p = P(x, y); if (skinlike(p)) acc_add(p[0], p[1], p[2]); } prof_skin = acc_get(skin); }
    if (!prof_skin && !region(sa, sb, hx0, hx1 + 1, NULL, NULL, skin) && !region(miny, hy1, hx0, hx1 + 1, NULL, NULL, skin)) memcpy(skin, all, sizeof skin);
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
    g_skipskin = prof_skin;
    if (!region(hy1, ty1, tx0 + tw / 4, tx1 - tw / 4 + 1, prof_skin ? skin : NULL, NULL, torso)) memcpy(torso, prof_skin ? skin : all, sizeof torso);   /* profile mode: a skin-coloured torso only for a bare chest */
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
    if (!region(ty1, maxy + 1 - sh, lx0, lx1 + 1, prof_skin ? skin : NULL, NULL, leg)) { if (!region(ty1, maxy + 1, lx0, lx1 + 1, prof_skin ? skin : NULL, NULL, leg)) memcpy(leg, all, sizeof leg); }
    if (!sh || !region(maxy + 1 - sh, maxy + 1, lx0, lx1 + 1, NULL, NULL, shoe)) memcpy(shoe, leg, sizeof shoe);
    int has_eye = region(miny + (int)floor(C.eye_lo * hh), miny + (int)ceil(C.eye_hi * hh), hx0, hx1 + 1, skin, has_hair ? hair : NULL, eye);
    int iris[3] = { 40, 30, 30 }, has_iris = 0;               /* profile mode: the iris = the bluest / greenest pixel of the face band (above the chin), else the darkest non-skin one */
    if (chin >= 0) { int best = 20, dk = 1000; for (int y = (chin - 9 > miny ? chin - 9 : miny); y < chin; y++) for (int x = hx0; x <= hx1; x++) {
            const unsigned char *p = P(x, y); if (!opaque(p) || skinlike(p) || outline(p)) continue;
            int sc = (p[2] - p[0] > p[1] - p[0]) ? p[2] - p[0] : p[1] - p[0], lum = p[0] + p[1] + p[2];
            if (sc > best) { best = sc; iris[0] = p[0]; iris[1] = p[1]; iris[2] = p[2]; has_iris = 1; } else if (!has_iris && lum < dk) { dk = lum; iris[0] = p[0]; iris[1] = p[1]; iris[2] = p[2]; } } }

    {   /* the SKIN CSV beside the rig file: rig_x.pdl -> rig_x.skin.csv */
        char sp[4300], st[4400]; snprintf(sp, sizeof sp, "%s", argv[3]); char *dot = strrchr(sp, '.'); if (dot && !strcmp(dot, ".pdl")) *dot = 0; strncat(sp, ".skin.csv", sizeof sp - strlen(sp) - 1); snprintf(st, sizeof st, "%s.tmp", sp);
        FILE *so = fopen(st, "w"); if (!so) { free(img); return 6; }
        fputs("part,face,u,v,r,g,b\n", so);
        const int *hc = has_hair ? hair : skin; memcpy(g_hand, skin, sizeof g_hand);
        for (int k = 0; k < 2; k++) {              /* front (down frame) and back (up frame): the viewer-left limb is the figure's right in the front frame, the figure's left in the back frame */
            char fc = k ? 'B' : 'F'; set_frame(k ? 3 : 0, by, fh);
            int ax0, ax1, bx0, bx1, cx0, cx1; extent(miny, hy1, fw, &ax0, &ax1); extent(hy1, ty1, fw, &bx0, &bx1); extent(ty1, maxy + 1, fw, &cx0, &cx1);
            int q = (bx1 - bx0 + 1) / 4; if (q < 1) q = 1; int mid = (cx0 + cx1 + 1) / 2;
            face_out(so, "head", fc, 8, 8, ax0, miny, ax1 + 1, hy1, skin);
            face_out(so, "torso", fc, 8, 12, bx0 + q, hy1, bx1 + 1 - q, ty1, torso);
            face_out(so, k ? "larm" : "rarm", fc, 4, 12, bx0, hy1, bx0 + q, ty1, arm);
            face_out(so, k ? "rarm" : "larm", fc, 4, 12, bx1 + 1 - q, hy1, bx1 + 1, ty1, arm);
            face_out(so, k ? "lleg" : "rleg", fc, 4, 12, cx0, ty1, mid, maxy + 1, leg);
            face_out(so, k ? "rleg" : "lleg", fc, 4, 12, mid, ty1, cx1 + 1, maxy + 1, leg); }
        for (int k = 0; k < 2; k++) {              /* left (row 1) and right (row 2) frames: head, torso and the limbs on the visible side; the hidden faces of a limb are its base colour */
            char fc = k ? 'R' : 'L', ofc = k ? 'L' : 'R'; set_frame(k ? 2 : 1, by, fh);
            int ax0, ax1, bx0, bx1, cx0, cx1; extent(miny, hy1, fw, &ax0, &ax1); extent(hy1, ty1, fw, &bx0, &bx1); extent(ty1, maxy + 1, fw, &cx0, &cx1);
            const char *arm_n = k ? "rarm" : "larm", *arm_o = k ? "larm" : "rarm", *leg_n = k ? "rleg" : "lleg", *leg_o = k ? "lleg" : "rleg";
            face_out(so, "head", fc, 8, 8, ax0, miny, ax1 + 1, hy1, skin); face_out(so, "torso", fc, 4, 12, bx0, hy1, bx1 + 1, ty1, torso);
            face_out(so, arm_n, fc, 4, 12, bx0, hy1, bx1 + 1, ty1, arm); face_out(so, leg_n, fc, 4, 12, cx0, ty1, cx1 + 1, maxy + 1, leg);
            if (!k) { flat_out(so, arm_n, ofc, 4, 12, arm); flat_out(so, arm_o, fc, 4, 12, arm); flat_out(so, leg_n, ofc, 4, 12, leg); flat_out(so, leg_o, fc, 4, 12, leg); } }   /* the hidden faces, once: larm R and rarm L, lleg R and rleg L */
        flat_out(so, "head", 'T', 8, 8, hc); flat_out(so, "head", 'D', 8, 8, skin); flat_out(so, "torso", 'T', 8, 4, torso); flat_out(so, "torso", 'D', 8, 4, torso);
        flat_out(so, "rarm", 'T', 4, 4, sleeve); flat_out(so, "larm", 'T', 4, 4, sleeve); flat_out(so, "rarm", 'D', 4, 4, SK.hand_rows > 0 ? skin : arm); flat_out(so, "larm", 'D', 4, 4, SK.hand_rows > 0 ? skin : arm);
        flat_out(so, "rleg", 'T', 4, 4, leg); flat_out(so, "lleg", 'T', 4, 4, leg); flat_out(so, "rleg", 'D', 4, 4, shoe); flat_out(so, "lleg", 'D', 4, 4, shoe);
        if (fclose(so) || rename(st, sp)) { remove(st); free(img); return 6; }
        set_frame(0, by, fh);
    }
    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", argv[3]);
    FILE *o = fopen(tmp, "w"); if (!o) { free(img); return 6; }
    fprintf(o, "# rig colours from %s (char %ld, frame %dx%d, figure rows %d..%d) by rig_from_charset_op; no texture, boxes only\n", base, idx, fw, fh, miny, maxy);
    fprintf(o, "RIG | head | %d %d %d\n", skin[0], skin[1], skin[2]);
    if (has_hair) fprintf(o, "RIG | hair | %d %d %d\n", hair[0], hair[1], hair[2]);
    fprintf(o, "RIG | torso | %d %d %d\nRIG | sleeve | %d %d %d\nRIG | arm | %d %d %d\nRIG | leg | %d %d %d\nRIG | shoe | %d %d %d\n",
            torso[0], torso[1], torso[2], sleeve[0], sleeve[1], sleeve[2], arm[0], arm[1], arm[2], leg[0], leg[1], leg[2], shoe[0], shoe[1], shoe[2]);
    if (has_eye) fprintf(o, "RIG | face | %d %d %d\n", eye[0], eye[1], eye[2]);
    if (chin >= 0) fprintf(o, "RIG | eyes | %d %d %d\n", iris[0], iris[1], iris[2]);
    if (fclose(o) || rename(tmp, argv[3])) { remove(tmp); free(img); return 6; }
    free(img);
    printf("wrote %s\n", argv[3]);
    return 0;
}
