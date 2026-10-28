/* bv_gpu_tile_test - tile-faced model pieces (BvGpuScene.ptex, box model id + 256): every cell face shows the tile picture, not one flat colour. Build against the shader under test:
 *   gcc -O2 -I. -o bv_gpu_tile_test bv_gpu_tile_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2        (run from ops/ so bv_raymarch.frag is found, or set BV_SHADER_FILE)
 * Scene: a grey floor and a 2x2x2-cell piece. Tile 0 (the top faces) is a red/white checkerboard, tile 1 (the side faces) green/blue vertical stripes. Checks: both checker colours show on the
 * top, both stripe colours on the sides, the tile is cut at the cell boundaries (the top shows more than 2 colour changes per row), and the old voxel-colour model of the same box shows none of it.
 * Exit 0 = every check held, 1 = a check failed, 2 = no GL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bv_gpu_raymarch.h"
#define W 240
#define H 180
static unsigned char out[W * H * 4];
static void scene(BvGpuScene *s, int tiled) {
    memset(s, 0, sizeof *s); s->w = W; s->h = H; s->focal = (H / 2.0f) / 0.8391f; s->board_w = 8; s->board_h = 8; s->z_count = 4;
    static unsigned char grid[8 * 8 * 4]; memset(grid, 0, sizeof grid); for (int i = 0; i < 64; i++) grid[i] = 's';
    s->grid = grid; s->legend_n = 1; s->legend_glyph[0] = 's'; s->legend_rgb[0][0] = s->legend_rgb[0][1] = s->legend_rgb[0][2] = 0.5f;
    s->light_level = 1.0f; s->sky[0] = 0.1f; s->sky[1] = 0.1f; s->sky[2] = 0.2f; s->eye[0] = 4; s->eye[1] = 6; s->eye[2] = -4;
    float fl = 1.0f / 1.118f; s->fwd[0] = 0; s->fwd[1] = -0.5f * fl; s->fwd[2] = 1.0f * fl; s->right[0] = 1;
    s->up[0] = s->right[1] * s->fwd[2] - s->right[2] * s->fwd[1]; s->up[1] = s->right[2] * s->fwd[0] - s->right[0] * s->fwd[2]; s->up[2] = s->right[0] * s->fwd[1] - s->right[1] * s->fwd[0];
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
        unsigned char *a = &s->ptex[0][(y * 24 + x) * 4], *b = &s->ptex[1][(y * 24 + x) * 4]; int chk = ((x / 6) + (y / 6)) & 1, str = (x / 4) & 1;
        a[0] = chk ? 255 : 230; a[1] = chk ? 255 : 20; a[2] = chk ? 255 : 20; a[3] = 255;      /* tile 0: white / red squares */
        b[0] = 0; b[1] = str ? 200 : 0; b[2] = str ? 0 : 200; b[3] = 255; }                    /* tile 1: green / blue stripes */
    s->ptex_n = 2; s->model_n = 1; s->model_dim[0][0] = 2; s->model_dim[0][1] = 2; s->model_dim[0][2] = 2;
    for (int z = 0; z < 2; z++) for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) { unsigned char *p = &s->model_vox[0][((z * BV_GPU_MDL_DIM + y) * BV_GPU_MDL_DIM + x) * 4];
        if (tiled) { p[0] = 0; p[1] = 1; p[2] = 0; } else { p[0] = 200; p[1] = 120; p[2] = 40; } p[3] = 255; }
    BvGpuBox *b = &s->box[s->box_n++]; b->min_x = 2; b->max_x = 4; b->min_y = 1; b->max_y = 3; b->min_z = 2; b->max_z = 4; b->r = 0.8f; b->g = 0.5f; b->b = 0.2f; b->self_lit = 0; b->model = tiled ? 256 : 0;
}
static int bad = 0;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static void classes(int *red, int *white, int *green, int *blue) { *red = *white = *green = *blue = 0; for (int i = 0; i < W * H; i++) { const unsigned char *p = out + i * 4;
    if (p[0] > 170 && p[1] < 90 && p[2] < 90) (*red)++; else if (p[0] > 200 && p[1] > 200 && p[2] > 200) (*white)++; else if (p[1] > 100 && p[0] < 60 && p[2] < 60) (*green)++; else if (p[2] > 100 && p[0] < 60 && p[1] < 60) (*blue)++; } }
int main(void) {
    BvGpuScene *s = malloc(sizeof *s); bv_gpu_set_persistent(1); int r, w, g, b;
    scene(s, 1); if (bv_gpu_raymarch(s, out)) { fprintf(stderr, "no GL\n"); return 2; }
    classes(&r, &w, &g, &b); printf("tile-faced piece: red %d white %d green %d blue %d\n", r, w, g, b);
    check(r > 40 && w > 40, "the top shows both checker colours of tile 0 (a picture, not one colour)");
    check(g > 15 && b > 15, "the side shows both stripe colours of tile 1");
    int changes = 0; for (int y = 0; y < H; y++) { int last = -1; for (int x = 0; x < W; x++) { const unsigned char *p = out + (y * W + x) * 4; int c = (p[0] > 170 && p[1] < 90) ? 1 : (p[0] > 200 && p[1] > 200) ? 2 : 0; if (c && last && c != last) changes++; if (c) last = c; } }
    check(changes > 30, "the checker is cut at the tile squares: many colour changes across the rows");
    scene(s, 0); bv_gpu_raymarch(s, out); classes(&r, &w, &g, &b); printf("same box as a plain voxel model: red %d white %d green %d blue %d\n", r, w, g, b);
    check(r + w + g + b < 30, "the plain voxel model of the same box shows none of the tile pictures");
    return bad;
}
