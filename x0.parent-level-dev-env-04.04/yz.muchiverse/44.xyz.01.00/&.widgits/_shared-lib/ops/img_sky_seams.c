/* img_sky_seams - count the ISOLATED pixels of one exact colour in a raw RGB24 picture (2026-11-08). Isolated = the colour at (x,y) while the pixel above AND the pixel below are a different colour: the signature of a sky-coloured
 * seam (one pixel thick) between two rows of floor. Milliseconds in C: the shell version (a per-pixel ffmpeg loop, then an awk array over 1.4 million cells) ran at 311% of a core, got the house ended by the CPU guard, and is exactly what NOT to do (CPU-OPTIMIZATION-PITFALLS 30).
 *   img_sky_seams <file.rgb24> <width> <height> [r g b]        default colour 135 180 220 (the sky)       prints the count; exit 0
 * Make the raw file with ONE ffmpeg call:  ffmpeg -i shot.png -vf crop=W:H:X:Y -f rawvideo -pix_fmt rgb24 out.rgb */
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: img_sky_seams <file.rgb24> <width> <height> [r g b]\n"); return 1; }
    int w = atoi(argv[2]), h = atoi(argv[3]); int r = argc > 6 ? atoi(argv[4]) : 135, g = argc > 6 ? atoi(argv[5]) : 180, b = argc > 6 ? atoi(argv[6]) : 220;
    if (w < 1 || h < 3 || w > 8192 || h > 8192) { fprintf(stderr, "bad size\n"); return 1; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    size_t n = (size_t)w * h * 3; unsigned char *p = malloc(n); if (!p || fread(p, 1, n, f) != n) { fprintf(stderr, "short read (need %zu bytes)\n", n); return 1; } fclose(f);
#define IS(x, y) (p[((size_t)(y) * w + (x)) * 3] == r && p[((size_t)(y) * w + (x)) * 3 + 1] == g && p[((size_t)(y) * w + (x)) * 3 + 2] == b)
    long cnt = 0, total = 0; for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (IS(x, y)) { total++; if (y > 0 && y < h - 1 && !IS(x, y - 1) && !IS(x, y + 1)) cnt++; }
    printf("%ld\n", cnt); fprintf(stderr, "img_sky_seams: %ld isolated, %ld of the colour in total\n", cnt, total); free(p); return 0;
}
