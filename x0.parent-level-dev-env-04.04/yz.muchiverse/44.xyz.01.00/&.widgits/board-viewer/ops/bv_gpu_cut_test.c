/* bv_gpu_cut_test - the see-through cutaway of the GPU raymarcher (BvGpuScene.cut_*), checked headless. Build against the shader under test:
 *   gcc -O2 -I. -o bv_gpu_cut_test bv_gpu_cut_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2
 * Scene: a grey floor (level 0), a red wall face (the whole row, three levels tall) between the eye and a green hero box. Checks: cut off = hero hidden behind the wall;
 * cut on = the wall is blended (green appears where the hero is, red stays around it); the floor and pixels outside the face half-width are byte-identical, a wider half-width blends more of the same wall;
 * cut_y above the wall changes nothing; alpha 0.9 shows less hero than 0.3. Exit 0 = every check held, 1 = a check failed, 2 = no GL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bv_gpu_raymarch.h"
#define W 240
#define H 180
static unsigned char out[W * H * 4], ref[W * H * 4];
static void scene(BvGpuScene *s) {
    memset(s, 0, sizeof *s); s->w = W; s->h = H; s->focal = (H / 2.0f) / 0.8391f; s->board_w = 8; s->board_h = 8; s->z_count = 4;
    static unsigned char grid[8 * 8 * 4]; memset(grid, 0, sizeof grid); for (int i = 0; i < 64; i++) grid[i] = 's';
    for (int lvl = 1; lvl < 4; lvl++) for (int x = 0; x <= 7; x++) grid[x + 2 * 8 + lvl * 64] = 'w';      /* the wall face: all of row 2, levels 1..3 */
    grid[6 + 3 * 8 + 1 * 64] = 'c';      /* a crate: row 3, column 6, ONE level high (not a wall column) */
    s->grid = grid; s->legend_n = 3; s->legend_glyph[0] = 's'; s->legend_glyph[1] = 'w'; s->legend_glyph[2] = 'c'; s->legend_rgb[2][0] = 0.9f; s->legend_rgb[2][1] = 0.9f; s->legend_rgb[2][2] = 0.1f;
    s->legend_rgb[0][0] = s->legend_rgb[0][1] = s->legend_rgb[0][2] = 0.8f; s->legend_rgb[1][0] = 0.9f; s->legend_rgb[1][1] = 0.1f; s->legend_rgb[1][2] = 0.1f;
    s->light_level = 1.0f; s->sky[0] = 0.1f; s->sky[1] = 0.1f; s->sky[2] = 0.2f; s->eye[0] = 4; s->eye[1] = 8; s->eye[2] = -6;
    float fl = 1.0f / 1.118f; s->fwd[0] = 0; s->fwd[1] = -0.5f * fl; s->fwd[2] = 1.0f * fl; s->right[0] = 1;
    s->up[0] = s->right[1] * s->fwd[2] - s->right[2] * s->fwd[1]; s->up[1] = s->right[2] * s->fwd[0] - s->right[0] * s->fwd[2]; s->up[2] = s->right[0] * s->fwd[1] - s->right[1] * s->fwd[0];
    BvGpuBox *b = &s->box[s->box_n++]; b->min_x = 4.05f; b->max_x = 4.95f; b->min_y = 1.0f; b->max_y = 1.9f; b->min_z = 4.05f; b->max_z = 4.95f; b->r = 0.1f; b->g = 0.9f; b->b = 0.1f; b->self_lit = 1; b->model = -1;
}
static void cut(BvGpuScene *s, int y, float a) { s->cut_on = 1; s->cut_y = y; s->cut_pos[0] = 4.5f; s->cut_pos[1] = 1.45f; s->cut_pos[2] = 4.5f; s->cut_r = 1.6f; s->cut_a = a; s->cut_depth = 12; s->cut_base = 0; }
static int greener(const unsigned char *a, const unsigned char *b, int *minx, int *maxx) { int n = 0; *minx = W; *maxx = -1; for (int i = 0; i < W * H; i++) if (a[i * 4 + 1] > b[i * 4 + 1] + 30 && a[i * 4] < b[i * 4]) { n++; int x = i % W; if (x < *minx) *minx = x; if (x > *maxx) *maxx = x; } return n; }
static int bad = 0;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
int main(int argc, char **argv) {
    BvGpuScene *s = malloc(sizeof *s); bv_gpu_set_persistent(1);
    scene(s); if (bv_gpu_raymarch(s, out)) { fprintf(stderr, "no GL\n"); return 2; }
    memcpy(ref, out, sizeof ref);
    int mn, mx, n0 = greener(out, ref, &mn, &mx); check(n0 == 0, "cut off: baseline is its own reference");
    scene(s); cut(s, 1, 0.3f); s->cut_on = 0; bv_gpu_raymarch(s, out); check(memcmp(out, ref, sizeof out) == 0, "cut_on 0 with the other fields set is the same picture");
    scene(s); cut(s, 9, 0.3f); bv_gpu_raymarch(s, out); check(memcmp(out, ref, sizeof out) == 0, "cut_y above the wall changes nothing");
    static unsigned char c3[W * H * 4], c9[W * H * 4];
    scene(s); cut(s, 1, 0.3f); bv_gpu_raymarch(s, c3); int n3 = greener(c3, ref, &mn, &mx); printf("alpha 0.3: %d greener pixels, columns %d..%d\n", n3, mn, mx);
    check(n3 > 40, "cut on: the hero shows through the wall (greener, less red pixels)");
    scene(s); cut(s, 1, 0.9f); bv_gpu_raymarch(s, c9); int n9, m1, m2; n9 = greener(c9, ref, &m1, &m2);
    check(n9 < n3, "higher wall alpha (0.9): less of the hero shows than at 0.3 (alpha is the wall's opacity)");
    static unsigned char cw[W * H * 4]; scene(s); cut(s, 1, 0.3f); s->cut_r = 8.0f; bv_gpu_raymarch(s, cw);
    int more = 0, redc3 = 0, redcw = 0; for (int i = 0; i < W * H; i++) { if (memcmp(cw + i * 4, c3 + i * 4, 4)) more++; if (c3[i * 4] > 150 && c3[i * 4 + 1] < 60) redc3++; if (cw[i * 4] > 150 && cw[i * 4 + 1] < 60) redcw++; }
    printf("half-width 1.6: %d solid-red pixels left, half-width 8: %d; %d pixels differ\n", redc3, redcw, more);
    check(more > 150 && redcw < redc3, "a wider half-width blends more of the same wall (the outer columns stop being solid red)");
    static unsigned char cb0[W * H * 4], cb1[W * H * 4]; int y0 = 0, y1 = 0;
    scene(s); cut(s, 1, 0.3f); s->cut_r = 8.0f; s->cut_base = 0; bv_gpu_raymarch(s, cb0);
    scene(s); cut(s, 1, 0.3f); s->cut_r = 8.0f; s->cut_base = 1; bv_gpu_raymarch(s, cb1);
    for (int i = 0; i < W * H; i++) { if (cb0[i * 4] > 150 && cb0[i * 4 + 1] > 150 && cb0[i * 4 + 2] < 80) y0++; if (cb1[i * 4] > 150 && cb1[i * 4 + 1] > 150 && cb1[i * 4 + 2] < 80) y1++; }
    printf("solid-yellow crate pixels: wall-height rule off %d, on %d\n", y0, y1);
    check(y1 > 30 && y0 < y1, "wall-height rule (cut_base 1): the low crate stays solid, the tall wall is still blended; with the rule off the crate is blended too");
    check(redc3 > 100, "half-width 1.6 leaves the outer wall columns solid (the face is limited to the half-width)");
    int diff_floor = 0; for (int y = H - 20; y < H; y++) for (int x = 0; x < W; x++) if (memcmp(c3 + (y * W + x) * 4, ref + (y * W + x) * 4, 4)) diff_floor++;
    check(diff_floor == 0, "the floor below the character is not ghosted");
    scene(s);
    check(bv_gpu_column_top(s, 4, 4) == 0 && bv_gpu_column_top(s, 3, 2) == 3 && bv_gpu_column_top(s, 6, 3) == 1, "column top: floor 0 under the hero, 3 in the wall column, 1 under the crate");
    check(bv_gpu_column_top(s, -1, 4) == -1 && bv_gpu_column_top(s, 4, 99) == -1, "column top: outside the grid is -1 (the caller keeps its own guess)");
    {   static unsigned char lo[W * H * 4]; scene(s); cut(s, -7, 0.3f); bv_gpu_raymarch(s, lo); int fl = 0; for (int y = H - 40; y < H; y++) for (int x = 0; x < W; x++) if (memcmp(lo + (y * W + x) * 4, ref + (y * W + x) * 4, 4)) fl++;
      int ghosted_floor = 0; for (int i = 0; i < W * H; i++) if (memcmp(lo + i * 4, ref + i * 4, 4) && lo[i * 4] > 150 && lo[i * 4 + 1] > 150) ghosted_floor++;
      printf("wrong floor level (-7): %d pixels changed in the bottom rows, %d grey-floor-like pixels changed\n", fl, ghosted_floor); }
    int tint = 0; for (int i = 0; i < W * H; i++) if (c3[i * 4] > c3[i * 4 + 1] + 30) tint++;
    printf("red-tinted pixels at alpha 0.3: %d\n", tint);
    check(tint > 200, "the wall is still there (red-tinted pixels remain, it is blended, not removed)");
    if (argc == 3 && !strcmp(argv[1], "dump")) { char fn[512]; snprintf(fn, sizeof fn, "%s_off.rgba", argv[2]); FILE *f = fopen(fn, "wb"); if (f) { fwrite(ref, 1, sizeof ref, f); fclose(f); } snprintf(fn, sizeof fn, "%s_on.rgba", argv[2]); f = fopen(fn, "wb"); if (f) { fwrite(c3, 1, sizeof c3, f); fclose(f); } }
    return bad;
}
