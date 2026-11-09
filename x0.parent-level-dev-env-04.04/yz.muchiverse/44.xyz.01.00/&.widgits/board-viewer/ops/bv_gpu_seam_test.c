/* bv_gpu_seam_test - a big FLAT floor seen from a LOW camera must have no sky showing through between cells (2026-11-08, Teropoly: sky-coloured dotted lines along cell boundaries, only on the 33x33 board). Build against the shader under test:
 *   gcc -O2 -I. -o bv_gpu_seam_test bv_gpu_seam_test.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2        (run from ops/ so bv_raymarch.frag is found, or set BV_SHADER_FILE=<file>)
 * Scene: a one-voxel floor of N x N cells (N from argv[1], default 33), the sky colour (26,26,51) is unlike the floor (grey), the camera sits 0.6 above the floor at the middle and looks along +z a little below the horizon. A ray that enters a SOLID cell must hit it: any pixel BELOW the
 * horizon that shows the sky colour is a seam. Exit 0 = no seam pixels, 1 = seams (count printed), 2 = no GL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bv_gpu_raymarch.h"
#define W 480
#define H 360
static unsigned char out[W * H * 4];
int main(int argc, char **argv) {
    int N = argc > 1 ? atoi(argv[1]) : 33; double pitch = argc > 2 ? atof(argv[2]) : -0.04, ey = argc > 3 ? atof(argv[3]) : 1.6; int deep = argc > 4 ? atoi(argv[4]) : 0; int ZC = deep ? 3 : 2;
    BvGpuScene *s = calloc(1, sizeof *s); bv_gpu_set_persistent(1);
    static unsigned char grid[64 * 64 * 4]; memset(grid, 0, sizeof grid); if (!deep) { for (int i = 0; i < N * N; i++) grid[i] = 's'; } else { for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { int ring = (x < 3 || y < 3 || x >= N - 3 || y >= N - 3); for (int l = ring ? 0 : 1; l < 3; l++) grid[x + y * N + l * N * N] = 's'; } }
    s->w = W; s->h = H; s->focal = (H / 2.0f) / 0.8391f; s->board_w = N; s->board_h = N; s->z_count = ZC; s->grid = grid;
    s->legend_n = 1; s->legend_glyph[0] = 's'; s->legend_rgb[0][0] = s->legend_rgb[0][1] = s->legend_rgb[0][2] = 0.8f;
    s->light_level = 1.0f; s->sky[0] = 26 / 255.0f; s->sky[1] = 26 / 255.0f; s->sky[2] = 51 / 255.0f;
    s->eye[0] = N / 2.0f; s->eye[1] = (float)ey; s->eye[2] = N / 2.0f;
    double fx = 0, fy = sin(pitch), fz = cos(pitch); s->fwd[0] = 0; s->fwd[1] = (float)fy; s->fwd[2] = (float)fz; s->right[0] = 1;
    s->up[0] = 0; s->up[1] = (float)fz; s->up[2] = (float)-fy; (void)fx;   /* up = fwd x right: pointing UP in the picture */
    if (bv_gpu_raymarch(s, out)) { fprintf(stderr, "no GL\n"); return 2; }
    int seam = 0, floorpx = 0, hz = -1; for (int y = H - 1; y >= 0; y--) { const unsigned char *p = out + (y * W + W / 2) * 4; if (abs(p[0] - 26) < 6 && abs(p[2] - 51) < 8) { hz = y; break; } }   /* the horizon: the first sky pixel going UP the middle column from the bottom */
    if (getenv("SEAM_DUMP")) { FILE *f = fopen(getenv("SEAM_DUMP"), "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); } }
    for (int y = hz + 2; y < H && hz >= 0; y++) for (int x = 0; x < W; x++) { const unsigned char *p = out + (y * W + x) * 4; if (p[0] > 100) floorpx++; else if (abs(p[0] - 26) < 6 && abs(p[2] - 51) < 8) seam++; }
    printf("N=%d pitch=%.3f eye_y=%.2f: horizon row %d, floor px %d, SEAM (sky below horizon) px %d\n", N, pitch, ey, hz, floorpx, seam);
    return (hz < 0 || floorpx < 1000) ? 2 : (seam ? 1 : 0);
}
