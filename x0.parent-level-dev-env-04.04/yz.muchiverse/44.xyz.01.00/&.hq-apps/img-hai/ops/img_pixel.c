/* img_pixel - turn a large picture into small pixel art: img_pixel <in> <out.png> [size=32] [colors=16] [mode=mode|avg]
 * 1. pick a palette of <colors> colours for the whole picture by median cut (split the colour box with the widest channel at its median until <colors> boxes remain; each box's mean is a palette entry);
 * 2. map every source pixel to its nearest palette colour;
 * 3. cut the picture into size x size blocks and give each output pixel ONE palette colour:
 *      mode  = the colour that occurs most often in the block (crisp, never invents a blended colour; ties go to the lower palette index)
 *      avg   = the block's mean colour, then the nearest palette colour.
 * Pixels with alpha < 128 count as transparent; a block that is mostly transparent stays transparent.
 * This replaces the first-version reduction in img_draft.py (nearest-neighbour sample of one source pixel per block, then PIL's 16-colour quantize), which threw away most of each block and kept stray pixels.
 * Reads png jpg bmp gif tga psd hdr pnm; writes an RGBA png. Pure: no network, deterministic (same input and options -> same bytes).
 * Exit 0 ok, 1 cannot read the input or bad options, 2 usage. Docs: &.hq-apps/img-hai/docs/CODE.md section 6 and EXTENDING.md recipe L. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
typedef struct { int lo, hi; unsigned char min[3], max[3]; } Box;   /* [lo,hi) range of the sorted index array */
static unsigned char *g_px; static int *g_idx; static int g_ch;
static int cmp_ch(const void *a, const void *b) { int x = g_px[(size_t)*(const int *)a * 4 + g_ch], y = g_px[(size_t)*(const int *)b * 4 + g_ch]; return x - y; }
static void box_range(Box *b) { for (int c = 0; c < 3; c++) { b->min[c] = 255; b->max[c] = 0; } for (int i = b->lo; i < b->hi; i++) for (int c = 0; c < 3; c++) { unsigned char v = g_px[(size_t)g_idx[i] * 4 + c]; if (v < b->min[c]) b->min[c] = v; if (v > b->max[c]) b->max[c] = v; } }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: img_pixel in out.png [size=32] [colors=16] [mode=mode|avg]\n"); return 2; }
    int size = argc > 3 ? atoi(argv[3]) : 32, K = argc > 4 ? atoi(argv[4]) : 16; const char *mode = argc > 5 ? argv[5] : "mode";
    if (size < 4 || size > 256 || K < 2 || K > 64 || (strcmp(mode, "mode") && strcmp(mode, "avg"))) { fprintf(stderr, "bad options (size 4..256, colors 2..64, mode mode|avg)\n"); return 1; }
    int w, h, c; unsigned char *src = stbi_load(argv[1], &w, &h, &c, 4); if (!src) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    if (w < size || h < size) { fprintf(stderr, "picture %dx%d is smaller than %d\n", w, h, size); stbi_image_free(src); return 1; }
    size_t n = (size_t)w * h; g_px = src; g_idx = malloc(n * sizeof(int)); int nopaque = 0; for (size_t i = 0; i < n; i++) if (src[i * 4 + 3] >= 128) g_idx[nopaque++] = (int)i;
    if (!nopaque) { fprintf(stderr, "picture is fully transparent\n"); return 1; }
    Box box[64]; int nb = 1; box[0].lo = 0; box[0].hi = nopaque; box_range(&box[0]);
    while (nb < K) {      /* split the box with the widest channel range (and more than one pixel) */
        int best = -1, bestr = 0, bch = 0; for (int i = 0; i < nb; i++) { if (box[i].hi - box[i].lo < 2) continue; for (int ch = 0; ch < 3; ch++) { int r = box[i].max[ch] - box[i].min[ch]; if (r > bestr) { bestr = r; best = i; bch = ch; } } }
        if (best < 0 || bestr == 0) break;
        g_ch = bch; qsort(g_idx + box[best].lo, (size_t)(box[best].hi - box[best].lo), sizeof(int), cmp_ch);
        int mid = (box[best].lo + box[best].hi) / 2; Box nbx = box[best]; nbx.lo = mid; box[best].hi = mid; box_range(&box[best]); box_range(&nbx); box[nb++] = nbx;
    }
    unsigned char pal[64][3]; for (int i = 0; i < nb; i++) { long s[3] = { 0, 0, 0 }; int cnt = box[i].hi - box[i].lo; for (int j = box[i].lo; j < box[i].hi; j++) for (int ch = 0; ch < 3; ch++) s[ch] += src[(size_t)g_idx[j] * 4 + ch]; for (int ch = 0; ch < 3; ch++) pal[i][ch] = (unsigned char)(s[ch] / (cnt ? cnt : 1)); }
    free(g_idx);
    #define NEAREST(r, g, b, out) { int bi = 0; long bd = 1L << 40; for (int p = 0; p < nb; p++) { long dr = (r) - pal[p][0], dg = (g) - pal[p][1], db = (b) - pal[p][2], d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; bi = p; } } (out) = bi; }
    unsigned char *idxmap = malloc(n); for (size_t i = 0; i < n; i++) { if (src[i * 4 + 3] < 128) { idxmap[i] = 255; continue; } int k; NEAREST(src[i * 4], src[i * 4 + 1], src[i * 4 + 2], k); idxmap[i] = (unsigned char)k; }
    unsigned char *out = calloc((size_t)size * size, 4);
    for (int oy = 0; oy < size; oy++) for (int ox = 0; ox < size; ox++) {
        int x0 = ox * w / size, x1 = (ox + 1) * w / size, y0 = oy * h / size, y1 = (oy + 1) * h / size; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1; int count[65]; memset(count, 0, sizeof count); long sum[3] = { 0, 0, 0 }; int total = 0, opaque = 0;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { size_t i = (size_t)y * w + x; total++; if (idxmap[i] == 255) continue; opaque++; count[idxmap[i]]++; for (int ch = 0; ch < 3; ch++) sum[ch] += src[i * 4 + ch]; }
        unsigned char *o = out + ((size_t)oy * size + ox) * 4; if (opaque * 2 < total) { o[3] = 0; continue; }
        int pick = 0; if (!strcmp(mode, "mode")) { for (int p = 1; p < nb; p++) if (count[p] > count[pick]) pick = p; } else { NEAREST((int)(sum[0] / opaque), (int)(sum[1] / opaque), (int)(sum[2] / opaque), pick); }
        o[0] = pal[pick][0]; o[1] = pal[pick][1]; o[2] = pal[pick][2]; o[3] = 255; }
    int ok = stbi_write_png(argv[2], size, size, 4, out, size * 4); fprintf(stderr, "wrote %s (%dx%d, %d colours, %s)\n", argv[2], size, size, nb, mode); free(out); free(idxmap); stbi_image_free(src); return ok ? 0 : 1;
}
