/* bv_gpu_light_test - the lights of the GPU raymarcher (BvGpuScene.light_*), checked headless. Build against the shader under test:
 *   gcc -O2 -I. -o bv_gpu_light_test bv_gpu_light_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2        (new)
 *   copy this file next to an OLD bv_gpu_raymarch.c/.h (from before lights) and build there with -DLIGHT_TEST_OLD: only the unlit render is made (the header must be the old one too: the quotes include picks the file next to the test)
 * Prints one line per case; `dump <file>` writes the unlit render's raw RGBA so two builds can be compared byte for byte. Exit 0 = every check held, 1 = a check failed, 2 = no GL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bv_gpu_raymarch.h"
#define W 240
#define H 180
static unsigned char out[W * H * 4];
static void scene(BvGpuScene *s, float light) {
    memset(s, 0, sizeof *s); s->w = W; s->h = H; s->focal = (H / 2.0f) / 0.8391f; s->board_w = 8; s->board_h = 8; s->z_count = 4;
    static unsigned char grid[8 * 8 * 4]; memset(grid, 0, sizeof grid); for (int i = 0; i < 64; i++) grid[i] = 's';      /* a flat floor, one voxel thick */
    s->grid = grid; s->legend_n = 1; s->legend_glyph[0] = 's'; s->legend_rgb[0][0] = s->legend_rgb[0][1] = s->legend_rgb[0][2] = 0.8f;
    s->light_level = light; s->sky[0] = 0.1f; s->sky[1] = 0.1f; s->sky[2] = 0.2f; s->eye[0] = 4; s->eye[1] = 8; s->eye[2] = -6;
    float fl = 1.0f / 1.118f; s->fwd[0] = 0; s->fwd[1] = -0.5f * fl; s->fwd[2] = 1.0f * fl; s->right[0] = 1;
    s->up[0] = s->right[1] * s->fwd[2] - s->right[2] * s->fwd[1]; s->up[1] = s->right[2] * s->fwd[0] - s->right[0] * s->fwd[2]; s->up[2] = s->right[0] * s->fwd[1] - s->right[1] * s->fwd[0];
}
static double lum(int x0, int y0, int x1, int y1) { double t = 0; int n = 0; for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { const unsigned char *p = out + ((size_t)y * W + x) * 4; t += 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]; n++; } return t / n; }
static int bad = 0;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
int main(int argc, char **argv) {
    BvGpuScene *s = malloc(sizeof *s); bv_gpu_set_persistent(1);
    scene(s, 0.15f); if (bv_gpu_raymarch(s, out)) { fprintf(stderr, "no GL\n"); return 2; }
    if (argc == 3 && !strcmp(argv[1], "dump")) { FILE *f = fopen(argv[2], "wb"); fwrite(out, 1, sizeof out, f); fclose(f); return 0; }
#ifndef LIGHT_TEST_OLD
    double base = lum(100, 48, 160, 66); printf("night, no lights: floor luminance %.1f\n", base);
    scene(s, 0.15f); s->light_amb_on = 1; s->light_amb = 0.6f; bv_gpu_raymarch(s, out); double amb = lum(100, 48, 160, 66);
    check(amb > base * 3.0, "ambient 0.6 replaces the night sun (about 4x brighter than 0.15)");
    scene(s, 1.0f); bv_gpu_raymarch(s, out); double day = lum(100, 48, 160, 66);
    scene(s, 1.0f); s->light_amb_on = 1; s->light_amb = 0.15f; bv_gpu_raymarch(s, out); double dim = lum(100, 48, 160, 66);
    check(dim < day * 0.25, "ambient also darkens a desk at noon (the room ignores the clock)");
    /* a lamp: compare 10 x 6 px blocks of the floor patch with the unlit picture (no assumption where the lit patch lands on screen) */
    static unsigned char ref[W * H * 4]; scene(s, 0.15f); bv_gpu_raymarch(s, out); memcpy(ref, out, sizeof ref);
    for (int pass = 0; pass < 2; pass++) {
        scene(s, 0.15f); s->light_n = 1; s->light_pos[0][0] = 1.5f; s->light_pos[0][1] = 2.5f; s->light_pos[0][2] = 4; s->light_pos[0][3] = 4;
        s->light_col[0][0] = 1; s->light_col[0][1] = pass ? 0.0f : 1.0f; s->light_col[0][2] = pass ? 0.0f : 1.0f; s->light_col[0][3] = 1;
        bv_gpu_raymarch(s, out); int lit = 0, same = 0, red = 0, total = 0;
        for (int by = 44; by < 80; by += 6) for (int bx = 70; bx < 190; bx += 10) { double a = 0, b2 = 0; for (int y = by; y < by + 6; y++) for (int x = bx; x < bx + 10; x++) { const unsigned char *p = out + ((size_t)y * W + x) * 4, *q = ref + ((size_t)y * W + x) * 4; a += p[0] + p[1] + p[2]; b2 += q[0] + q[1] + q[2]; red += pass && p[0] > q[0] + 30 && p[1] <= q[1] + 3; }
            total++; if (a > b2 * 1.8) lit++; else if (a >= b2 * 0.99 && a <= b2 * 1.01) same++; }
        if (!pass) { printf("lamp: %d of %d blocks lit, %d unchanged\n", lit, total, same); check(lit >= 3, "a lamp lights the floor near it (>= 3 blocks at least 1.8x brighter)"); check(same >= 6, "far from the lamp the floor is exactly as before"); }
        else check(red > 200, "a red lamp adds red only (pixels gain red, not green)");
    }
    scene(s, 0.15f); s->light_n = 0; bv_gpu_raymarch(s, out); check(lum(100, 48, 160, 66) == base, "light_n 0 is the same picture as before");
#endif
    return bad;
}
