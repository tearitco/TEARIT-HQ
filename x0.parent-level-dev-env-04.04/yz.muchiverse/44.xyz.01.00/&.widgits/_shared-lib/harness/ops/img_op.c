#define _GNU_SOURCE
/* img_op - picture checks for harness cases (no Python): numbers out of a PNG, so a case can EXPECT_GE / EXPECT_LE them. Added 2026-11-04 for the live window cases.
 *   img_op mean <png> <x0> <y0> <x1> <y1>               mean luma (0..255) of the box, one decimal
 *   img_op vrun <png> <x0> <x1> <y0> <y1> [thresh]      the longest vertical run of pixels brighter than thresh (default 110) in any column x0..x1-1 within y0..y1-1: a scrollbar thumb is a long run, text never is
 *   img_op diff <a.png> <b.png> <x0> <y0> <x1> <y1> [thr]   how many pixels of the box differ by more than thr (default 40) in luma: "did this region change?"
 *   img_op size <png>                                    "<w> <h>"
 * Exit 0 and one number on stdout; 2 on a usage error or an unreadable file (a case then FAILs on EXPECT_EXIT). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../stb_image.h"
static unsigned char *load(const char *p, int *w, int *h) { int c; unsigned char *d = stbi_load(p, w, h, &c, 4); if (!d) fprintf(stderr, "img_op: cannot read %s\n", p); return d; }
static int luma(const unsigned char *px) { return (px[0] * 299 + px[1] * 587 + px[2] * 114) / 1000; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: img_op mean|vrun|diff|size ...\n"); return 2; }
    int w, h; unsigned char *a = load(argv[2], &w, &h); if (!a) return 2;
    if (!strcmp(argv[1], "size")) { printf("%d %d\n", w, h); return 0; }
    if (!strcmp(argv[1], "mean") && argc >= 7) { int x0 = clampi(atoi(argv[3]), 0, w), y0 = clampi(atoi(argv[4]), 0, h), x1 = clampi(atoi(argv[5]), 0, w), y1 = clampi(atoi(argv[6]), 0, h); double s = 0; long n = 0;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { s += luma(a + ((size_t)y * w + x) * 4); n++; } if (!n) return 2; printf("%.1f\n", s / n); return 0; }
    if (!strcmp(argv[1], "vrun") && argc >= 7) { int x0 = clampi(atoi(argv[3]), 0, w), x1 = clampi(atoi(argv[4]), 0, w), y0 = clampi(atoi(argv[5]), 0, h), y1 = clampi(atoi(argv[6]), 0, h), th = argc >= 8 ? atoi(argv[7]) : 110, best = 0;
        for (int x = x0; x < x1; x++) { int r = 0; for (int y = y0; y < y1; y++) { if (luma(a + ((size_t)y * w + x) * 4) > th) { r++; if (r > best) best = r; } else r = 0; } } printf("%d\n", best); return 0; }
    if (!strcmp(argv[1], "diff") && argc >= 8) { int w2, h2; unsigned char *b = load(argv[3], &w2, &h2); if (!b) return 2; int x0 = clampi(atoi(argv[4]), 0, w < w2 ? w : w2), y0 = clampi(atoi(argv[5]), 0, h < h2 ? h : h2), x1 = clampi(atoi(argv[6]), 0, w < w2 ? w : w2), y1 = clampi(atoi(argv[7]), 0, h < h2 ? h : h2), thr = argc >= 9 ? atoi(argv[8]) : 40; long n = 0;
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { int d = luma(a + ((size_t)y * w + x) * 4) - luma(b + ((size_t)y * w2 + x) * 4); if (d < 0) d = -d; if (d > thr) n++; } printf("%ld\n", n); return 0; }
    fprintf(stderr, "img_op: bad arguments\n"); return 2;
}
