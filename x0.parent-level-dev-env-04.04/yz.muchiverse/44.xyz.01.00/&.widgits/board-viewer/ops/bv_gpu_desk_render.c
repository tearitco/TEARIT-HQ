/* bv_gpu_desk_render - OFFLINE 3D picture of a desk folder (cells.txt + map.txt + extrusion.pdl + cells.rgba), no window and no live engine (D112, the voxel app's picture proof).
 * It rebuilds the SAME scene the board engine builds for a maker desk (bv_render_3d.c, the maker_view branch): a column is as tall as its map.txt glyph's EXTRUDE row (default 1), the top face shows the cell floor slot and the side faces its wall slot (observed in the picture, 2026-11-22), atlas from cells.rgba,
 * then draws it with the same GPU raymarch (bv_gpu_raymarch). Each events.txt row (x y r g b ...) is drawn as a small box on top of that column. Build:
 *   gcc -O2 -I. -o bv_gpu_desk_render bv_gpu_desk_render.c bv_gpu_raymarch.c -lm -lEGL -lGLESv2     (run with BV_SHADER_FILE=<ops>/bv_raymarch.frag)
 * Usage: bv_gpu_desk_render <desk_dir> <out.raw> [yaw_deg [pitch_deg [dist]]]      yaw 0 = the camera stands on the row-0 side looking along +row (row 0 is the bottom of the picture); defaults 35 / -40 / 15.
 * Prints `RENDER <w>x<h> cols=<w>x<h> tallest=<levels> actors=<n> sky_px=<n> solid_px=<n>`. The raw file is 640x400 RGBA (raw_to_png <raw> <png> --size 640x400). Exit 0 ok, 2 no GL or a bad desk. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bv_gpu_raymarch.h"
#define OW 640
#define OH 400
#define MAXC 64
static unsigned char out[OW * OH * 4];
static unsigned short cells[MAXC * MAXC * 2];
static unsigned char glyph[MAXC * MAXC], zh[128], grid[MAXC * MAXC * 20];
static BvGpuScene sc;
static void cross(const double *a, const double *b, double *c) { c[0] = a[1] * b[2] - a[2] * b[1]; c[1] = a[2] * b[0] - a[0] * b[2]; c[2] = a[0] * b[1] - a[1] * b[0]; }
static void norm(double *a) { double l = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (l > 0) { a[0] /= l; a[1] /= l; a[2] /= l; } }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: bv_gpu_desk_render <desk_dir> <out.raw> [yaw [pitch [dist]]]\n"); return 2; }
    double yaw = argc > 3 ? atof(argv[3]) : 35, pitch = argc > 4 ? atof(argv[4]) : -40, dist = argc > 5 ? atof(argv[5]) : 15;
    char p[2048], ln[8192]; int W = 0, H = 0, px = 24, cols = 16, row = -1; FILE *f;
    snprintf(p, sizeof p, "%s/cells.txt", argv[1]); if (!(f = fopen(p, "r"))) { fprintf(stderr, "no cells.txt\n"); return 2; }
    while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) H = atoi(ln + 7); else if (!strncmp(ln, "tile_px=", 8)) px = atoi(ln + 8); else if (!strncmp(ln, "atlas_cols=", 11)) cols = atoi(ln + 11);
        else if (!strncmp(ln, "cells", 5)) row = 0; else if (row >= 0 && row < H && W <= MAXC && H <= MAXC) { char *q = ln; for (int x = 0; x < W; x++) { char *e; long fl = strtol(q, &e, 10); if (e == q || *e != ',') break; q = e + 1; long wl = strtol(q, &e, 10); q = e; cells[(row * W + x) * 2] = (unsigned short)fl; cells[(row * W + x) * 2 + 1] = (unsigned short)wl; } row++; } }
    fclose(f); if (W < 1 || H < 1 || W > MAXC || H > MAXC || row != H) { fprintf(stderr, "bad cells.txt\n"); return 2; }
    snprintf(p, sizeof p, "%s/map.txt", argv[1]); if ((f = fopen(p, "r"))) { for (int r = 0; r < H && fgets(ln, sizeof ln, f); r++) for (int x = 0; x < W && ln[x] && ln[x] != '\n'; x++) glyph[r * W + x] = (unsigned char)ln[x]; fclose(f); }
    snprintf(p, sizeof p, "%s/extrusion.pdl", argv[1]); if ((f = fopen(p, "r"))) { while (fgets(ln, sizeof ln, f)) { if (strstr(ln, "DEPTH") || !strstr(ln, "EXTRUDE")) continue; char *b = strchr(ln, '|'); if (!b) continue; b++; while (*b == ' ') b++; char g = *b, *b2 = strchr(b, '|'); if (b2 && g > 32 && g < 127) { int hv = atoi(b2 + 1); if (hv > 0 && hv <= 64) zh[(unsigned char)g] = (unsigned char)hv; } } fclose(f); }
    static unsigned char atlas[2000000]; snprintf(p, sizeof p, "%s/cells.rgba", argv[1]); if (!(f = fopen(p, "rb"))) { fprintf(stderr, "no cells.rgba\n"); return 2; } size_t asz = fread(atlas, 1, sizeof atlas, f); fclose(f); int aw = cols * px; if (!asz || asz % ((size_t)aw * 4)) { fprintf(stderr, "bad atlas\n"); return 2; }
    int tall = 1; for (int i = 0; i < W * H; i++) { int h = (glyph[i] < 128 && zh[glyph[i]] > 1) ? zh[glyph[i]] : 1; if (h == 1 && cells[i * 2 + 1]) h = 3; if (h > tall) tall = h; }
    int ZC = tall + 2; if ((size_t)W * H * ZC > sizeof grid) { fprintf(stderr, "too tall\n"); return 2; }
    for (int i = 0; i < W * H; i++) { int h = (glyph[i] < 128 && zh[glyph[i]] > 1) ? zh[glyph[i]] : 1; if (h == 1 && cells[i * 2 + 1]) h = 3; for (int l = 0; l < h; l++) grid[i + l * W * H] = 1; }
    bv_gpu_set_persistent(1); memset(&sc, 0, sizeof sc); sc.w = OW; sc.h = OH; sc.focal = (OH / 2.0f) / 0.8391f; sc.board_w = W; sc.board_h = H; sc.z_count = ZC; sc.grid = grid;
    sc.legend_n = 1; sc.legend_glyph[0] = 1; sc.legend_rgb[0][0] = sc.legend_rgb[0][1] = sc.legend_rgb[0][2] = 0.5f;
    sc.maker = 1; sc.maker_id = 1; sc.zoff = 0; sc.rock = 0; sc.atlas = atlas; sc.atlas_w = aw; sc.atlas_h = (int)(asz / ((size_t)aw * 4)); sc.atlas_cols = cols; sc.tile_px = px; sc.cells = cells; sc.cells_w = W; sc.cells_h = H;
    sc.light_level = 1.0f; sc.sky[0] = 26 / 255.0f; sc.sky[1] = 26 / 255.0f; sc.sky[2] = 51 / 255.0f;
    int na = 0; snprintf(p, sizeof p, "%s/events.txt", argv[1]);
    if ((f = fopen(p, "r"))) { while (fgets(ln, sizeof ln, f) && sc.box_n < BV_GPU_MAX_BOX) { int x, z, r, g, b; if (sscanf(ln, "%d %d %d %d %d", &x, &z, &r, &g, &b) == 5 && x >= 0 && z >= 0 && x < W && z < H) { int i = z * W + x, h = (glyph[i] < 128 && zh[glyph[i]] > 1) ? zh[glyph[i]] : 1;
        BvGpuBox *bx = &sc.box[sc.box_n++]; bx->min_x = x + 0.25f; bx->max_x = x + 0.75f; bx->min_z = z + 0.25f; bx->max_z = z + 0.75f; bx->min_y = (float)h; bx->max_y = h + 1.7f; bx->r = r / 255.0f; bx->g = g / 255.0f; bx->b = b / 255.0f; bx->model = -1; na++; } } fclose(f); }
    double cy = tall / 2.0, T[3] = { W / 2.0, cy, H / 2.0 }, ya = yaw * M_PI / 180, pi = pitch * M_PI / 180, fwd[3] = { sin(ya) * cos(pi), sin(pi), cos(ya) * cos(pi) }, up0[3] = { 0, 1, 0 }, rt[3], up[3];
    norm(fwd); cross(up0, fwd, rt); norm(rt); cross(fwd, rt, up);
    for (int k = 0; k < 3; k++) { sc.eye[k] = (float)(T[k] - fwd[k] * dist); sc.fwd[k] = (float)fwd[k]; sc.right[k] = (float)rt[k]; sc.up[k] = (float)up[k]; }
    if (bv_gpu_raymarch(&sc, out)) { fprintf(stderr, "no GL\n"); return 2; }
    int sky = 0, sol = 0; for (int i = 0; i < OW * OH; i++) { const unsigned char *q = out + i * 4; if (abs(q[0] - 26) < 4 && abs(q[1] - 26) < 4 && abs(q[2] - 51) < 5) sky++; else sol++; }
    f = fopen(argv[2], "wb"); if (!f) return 2; fwrite(out, 1, sizeof out, f); fclose(f);
    printf("RENDER %dx%d cols=%dx%d tallest=%d actors=%d sky_px=%d solid_px=%d\n", OW, OH, W, H, tall, na, sky, sol); return 0;
}
