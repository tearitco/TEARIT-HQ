/* frame_stats - measure a picture so a person (or a gate) can tell a REAL frame from a blank, flat or dark one (owner 2026-10-28: "we need to have confident testing tools"; the case that made it: a fresh board's 3D canvas came up as ONE flat brown
 * colour because the camera started inside the floor, and the only check was a person looking). Reads a PNG (RGB, RGBA, gray, palette; 8 or 16 bit), optionally only a rectangle (so the board's constant black chrome does not count).
 * Usage:  frame_stats <in.png> [--rect x,y,w,h] [--inset PCT] [--flat F] [--blank F] [--dark L]
 *   --inset PCT  after --rect (or the whole picture), drop PCT percent of the width and of the height from EACH side (0..45): look at the middle only, where the engine's debug text and minimap in the corners do not count
 * Prints (key=value): size=<W>x<H> pixels=<n> dominant_pct=<%> dominant_rgb=<r,g,b> colors=<n> mean_lum=<0..255> lum_stddev=<..> edge=<mean abs neighbour luminance step, 0..255> verdict=<ok|sparse|flat|blank|dark>
 *   Colours are counted after dropping each channel to 4 bits (4096 bins; `colors` = bins holding at least 0.1 % of the pixels); luminance = 0.2126 R + 0.7152 G + 0.0722 B (RGBA: alpha is ignored); edge = the mean of |lum(x+1,y)-lum(x,y)| and |lum(x,y+1)-lum(x,y)| over the rectangle.
 *   verdict:  blank = one colour bin holds >= --blank (default 99.5 %); flat = >= --flat (default 97 %); dark = mean_lum < --dark (default 8); sparse = one bin holds >= 80 % (a mostly empty scene, e.g. space with a few stars: fine, but look); ok otherwise.
 * Exit: 0 ok or sparse | 1 flat, blank or dark | 2 usage, unreadable picture or a rectangle outside the picture.   Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/frame_stats.+x frame_stats.c -lpng -lm */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <png.h>

static long rx = 0, ry = 0, rw = -1, rh = -1, inset = 0; static double flat = 97.0, blank = 99.5, dark = 8.0;      /* statics: libpng's error jump must not clobber them */
int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--rect") && i + 1 < argc) { if (sscanf(argv[++i], "%ld,%ld,%ld,%ld", &rx, &ry, &rw, &rh) != 4) goto usage; }
        else if (!strcmp(argv[i], "--inset") && i + 1 < argc) { inset = atol(argv[++i]); if (inset < 0 || inset > 45) goto usage; }
        else if (!strcmp(argv[i], "--flat") && i + 1 < argc) flat = atof(argv[++i]); else if (!strcmp(argv[i], "--blank") && i + 1 < argc) blank = atof(argv[++i]); else if (!strcmp(argv[i], "--dark") && i + 1 < argc) dark = atof(argv[++i]); else goto usage; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "frame_stats: cannot read %s\n", argv[1]); return 2; }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { if (png) png_destroy_read_struct(&png, &info, NULL); fclose(f); fprintf(stderr, "frame_stats: %s is not a readable PNG\n", argv[1]); return 2; }
    png_init_io(png, f); png_read_info(png, info); png_set_expand(png); png_set_strip_16(png); png_set_gray_to_rgb(png); png_set_palette_to_rgb(png);
    if (!(png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png, info); long W = (long)png_get_image_width(png, info), H = (long)png_get_image_height(png, info);
    if (W < 1 || H < 1 || W > 16384 || H > 16384) { png_destroy_read_struct(&png, &info, NULL); fclose(f); fprintf(stderr, "frame_stats: bad picture size\n"); return 2; }
    unsigned char *px = malloc((size_t)W * (size_t)H * 4); png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)H); if (!px || !rows) return 2;
    for (long y = 0; y < H; y++) rows[y] = px + (size_t)y * (size_t)W * 4;
    png_read_image(png, rows); png_destroy_read_struct(&png, &info, NULL); fclose(f); free(rows);
    if (rw < 0) { rw = W; rh = H; }
    if (rx < 0 || ry < 0 || rw < 1 || rh < 1 || rx + rw > W || ry + rh > H) { fprintf(stderr, "frame_stats: the rectangle %ld,%ld,%ld,%ld is not inside the %ldx%ld picture\n", rx, ry, rw, rh, W, H); return 2; }
    if (inset) { long dx = rw * inset / 100, dy = rh * inset / 100; rx += dx; ry += dy; rw -= 2 * dx; rh -= 2 * dy; if (rw < 1 || rh < 1) { fprintf(stderr, "frame_stats: --inset leaves nothing\n"); return 2; } }
    static long bins[4096]; double sum = 0, sum2 = 0, edge = 0; long ne = 0, n = rw * rh;
    for (long y = ry; y < ry + rh; y++) for (long x = rx; x < rx + rw; x++) {
        const unsigned char *p = px + ((size_t)y * (size_t)W + (size_t)x) * 4; bins[(p[0] >> 4) << 8 | (p[1] >> 4) << 4 | (p[2] >> 4)]++;
        double l = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]; sum += l; sum2 += l * l;
        if (x + 1 < rx + rw) { const unsigned char *q = p + 4; edge += fabs(0.2126 * q[0] + 0.7152 * q[1] + 0.0722 * q[2] - l); ne++; }
        if (y + 1 < ry + rh) { const unsigned char *q = p + (size_t)W * 4; edge += fabs(0.2126 * q[0] + 0.7152 * q[1] + 0.0722 * q[2] - l); ne++; } }
    int best = 0, colors = 0; for (int b = 0; b < 4096; b++) { if (bins[b] > bins[best]) best = b; if (bins[b] * 1000 >= n) colors++; }
    double dom = 100.0 * (double)bins[best] / (double)n, mean = sum / (double)n, var = sum2 / (double)n - mean * mean; if (var < 0) var = 0;
    const char *v = dom >= blank ? "blank" : dom >= flat ? "flat" : mean < dark ? "dark" : dom >= 80.0 ? "sparse" : "ok";
    printf("size=%ldx%ld\npixels=%ld\ndominant_pct=%.2f\ndominant_rgb=%d,%d,%d\ncolors=%d\nmean_lum=%.2f\nlum_stddev=%.2f\nedge=%.3f\nverdict=%s\n", W, H, n, dom, ((best >> 8) & 15) * 16 + 8, ((best >> 4) & 15) * 16 + 8, (best & 15) * 16 + 8, colors, mean, sqrt(var), ne ? edge / (double)ne : 0.0, v);
    free(px); return (!strcmp(v, "ok") || !strcmp(v, "sparse")) ? 0 : 1;
usage:
    fprintf(stderr, "Usage: frame_stats <in.png> [--rect x,y,w,h] [--inset PCT] [--flat F] [--blank F] [--dark L]\n"); return 2;
}
