/* bv_gpu_voxel_test - the faces of a voxel model piece are shaded by the VOXEL face the ray entered (top bright, sides dimmer), checked headless. Build against the shader under test:
 *   gcc -O2 -I. -o bv_gpu_voxel_test bv_gpu_voxel_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2        (run from ops/ so bv_raymarch.frag is found, or set BV_SHADER_FILE)
 * Scene: a grey floor and a 4x4x4 box carrying a model of four stair steps (heights 1..4 along the view direction), one flat orange. Before the change every face of the whole box took the box's own
 * entry face (all 0.75), so the stairs read as one flat orange silhouette; now the treads (top faces) and risers (side faces) differ. Exit 0 = every check held, 1 = a check failed, 2 = no GL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bv_gpu_raymarch.h"
#define W 240
#define H 180
static unsigned char out[W * H * 4];
static void scene(BvGpuScene *s, int flat) {
    memset(s, 0, sizeof *s); s->w = W; s->h = H; s->focal = (H / 2.0f) / 0.8391f; s->board_w = 8; s->board_h = 8; s->z_count = 4;
    static unsigned char grid[8 * 8 * 4]; memset(grid, 0, sizeof grid); for (int i = 0; i < 64; i++) grid[i] = 's';
    s->grid = grid; s->legend_n = 1; s->legend_glyph[0] = 's'; s->legend_rgb[0][0] = s->legend_rgb[0][1] = s->legend_rgb[0][2] = 0.8f;
    s->light_level = 1.0f; s->sky[0] = 0.1f; s->sky[1] = 0.1f; s->sky[2] = 0.2f; s->eye[0] = 4; s->eye[1] = 8; s->eye[2] = -6;
    float fl = 1.0f / 1.118f; s->fwd[0] = 0; s->fwd[1] = -0.5f * fl; s->fwd[2] = 1.0f * fl; s->right[0] = 1;
    s->up[0] = s->right[1] * s->fwd[2] - s->right[2] * s->fwd[1]; s->up[1] = s->right[2] * s->fwd[0] - s->right[0] * s->fwd[2]; s->up[2] = s->right[0] * s->fwd[1] - s->right[1] * s->fwd[0];
    s->model_n = 1; s->model_dim[0][0] = 4; s->model_dim[0][1] = 4; s->model_dim[0][2] = 4;
    for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (flat || y <= z) { unsigned char *p = &s->model_vox[0][((z * BV_GPU_MDL_DIM + y) * BV_GPU_MDL_DIM + x) * 4]; p[0] = 230; p[1] = 128; p[2] = 38; p[3] = 255; }
    BvGpuBox *b = &s->box[s->box_n++]; b->min_x = 2; b->max_x = 6; b->min_y = 1; b->max_y = 5; b->min_z = 2; b->max_z = 6; b->r = 0.9f; b->g = 0.5f; b->b = 0.15f; b->self_lit = 0; b->model = 0;
}
static int bad = 0;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad = 1; }
static void spread(double *mx, double *mn, int *n) { *mx = 0; *mn = 1e9; *n = 0; for (int i = 0; i < W * H; i++) { const unsigned char *p = out + i * 4; if (p[0] > 70 && p[0] > p[2] + 60 && p[0] > p[1] + 25) { double l = 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]; if (l > *mx) *mx = l; if (l < *mn) *mn = l; (*n)++; } } }
int main(void) {
    BvGpuScene *s = malloc(sizeof *s); bv_gpu_set_persistent(1);
    double mx, mn; int n;
    scene(s, 1); if (bv_gpu_raymarch(s, out)) { fprintf(stderr, "no GL\n"); return 2; }
    spread(&mx, &mn, &n); printf("solid orange cube: %d px, luminance %.0f..%.0f\n", n, mn, mx); check(n > 300 && mx / mn > 1.25, "a plain cube still shows a bright top and dimmer sides (the box faces, unchanged)");
    scene(s, 0); bv_gpu_raymarch(s, out); spread(&mx, &mn, &n); printf("stairs: %d px, luminance %.0f..%.0f\n", n, mn, mx);
    check(n > 300 && mx / mn > 1.45, "the stairs show treads (1.0), side risers (0.82 / 0.66) in different brightness: a lump of voxels reads as a solid, not a flat silhouette (the old shader gave 1.34, the box faces only)");
    return bad;
}
