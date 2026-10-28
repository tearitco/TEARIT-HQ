/* views_to_voxel - distil three orthographic pictures of a thing into the engine's voxel model (model.txt): the visual hull, plus a consistency check that tells whether the pictures agree.
 *
 * WHY (owner 2026-10-27: entities ask an API / Claude for ".txt / .png renders from different angles" and distil them into game assets): a generated picture set is only usable if it is
 * GEOMETRICALLY CONSISTENT. The visual hull is the one piece of mathematics that decides it: a voxel (x,y,z) is kept iff it lies inside all three silhouettes. If the pictures disagree (the side
 * view has a part the front view cannot see, a wing in one and not the other) the hull's projection no longer reproduces the silhouettes, the consistency number drops below 1, and the set is rejected
 * (exit 4) before it reaches the game: a LAW (deterministic, unit-tested, never weighted), not a choice. Design: #.#.calendar-dox/!.HQ-IQ-BOOK/16.game/TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md.
 *
 * Usage:  views_to_voxel <out_model.txt> <front> <side> <top> [--solid] [--max-vox N] [--min-consistency F] [--bg R,G,B]
 *   Each view is a .png or a .txt grid (decided by the extension). AXES = the model's own (the engine's `dim X Y Z`, Y is UP, Z is the grid y):
 *     front : columns = X (left to right), rows top to bottom = Y high to low   (width = dim X, height = dim Y)
 *     side  : columns = Z (left to right, z = 0 at the left), rows = Y high to low   (width = dim Z, height = dim Y)   (the thing seen from its right, +X side)
 *     top   : columns = X left to right, rows top to bottom = Z increasing (z = 0 in the top row)   (width = dim X, height = dim Z)
 *   .txt view: one row per line; '.' or ' ' = empty, any other character (# included) = filled. Lines starting "# legend <char> <r> <g> <b>" give a filled character its colour (default grey 160 160 160).
 *   .png view: a pixel is filled when alpha >= 128 and, if the picture has no transparency at all, when it differs from the background (--bg R,G,B, default the top-left pixel).
 *   Output: `dim X Y Z` then one `x y z r g b` line per voxel (the format rpg-space asteroids/<name>/model.txt and pieces/veh_N/model.txt use). Default = SURFACE voxels only (a voxel with an
 *   empty or outside 6-neighbour); --solid keeps every hull voxel. A voxel's colour: the front picture where it is the front-most hull voxel of its column (smallest z), else the top picture where it
 *   is the top-most (largest y), else the side picture where it is the right-most (largest x), else the front picture.
 *   Report on stdout (key=value): dim_x dim_y dim_z hull_voxels written_voxels consistency_front consistency_side consistency_top consistency fits_engine (1 when X<=32, Y<=32, Z<=16:
 *   the GPU model slot, see bv_gpu_raymarch.h) .  consistency_* = share of a picture's filled pixels the hull's projection reproduces (the projection can never EXCEED a silhouette, so 1.0 = the
 *   three pictures describe one solid thing); consistency = the smallest of the three.
 * Exit: 0 ok | 1 usage | 2 unreadable picture or sizes that do not agree (front width = top width, front height = side height, side width = top height) | 3 over --max-vox
 *       | 4 consistency below --min-consistency (default 1.0) or an empty hull.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/views_to_voxel.+x views_to_voxel.c -lpng -lm */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <png.h>

#define MAXDIM 96
typedef struct { int w, h; unsigned char *fill; unsigned char *rgb; } View;      /* row-major, y = 0 is the TOP row of the picture */

static int ends_with(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); if (a < b) return 0; for (size_t i = 0; i < b; i++) if (tolower((unsigned char)s[a - b + i]) != suf[i]) return 0; return 1; }

static int load_txt(const char *path, View *v) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    static char line[4096]; static char rows[MAXDIM][512]; int nrows = 0, w = 0; unsigned char leg[256][3]; unsigned char has[256]; memset(has, 0, sizeof has);
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "# legend ", 9)) { char c; int r, g, b; if (sscanf(line, "# legend %c %d %d %d", &c, &r, &g, &b) == 4) { unsigned char u = (unsigned char)c; leg[u][0] = (unsigned char)r; leg[u][1] = (unsigned char)g; leg[u][2] = (unsigned char)b; has[u] = 1; } continue; }      /* only "# legend" lines are metadata: a row may start with # (a filled cell) */
        if (!line[0] && nrows == 0) continue;
        if (nrows >= MAXDIM) { fclose(f); return 0; }
        size_t n = strlen(line); if (n >= sizeof rows[0]) n = sizeof rows[0] - 1; memcpy(rows[nrows], line, n); rows[nrows][n] = 0; if ((int)n > w) w = (int)n; nrows++;
    }
    fclose(f);
    while (nrows > 0 && !rows[nrows - 1][0]) nrows--;                  /* trailing blank lines are not part of the picture */
    if (nrows < 1 || w < 1 || w > MAXDIM) return 0;
    v->w = w; v->h = nrows; v->fill = calloc((size_t)w * nrows, 1); v->rgb = calloc((size_t)w * nrows, 3); if (!v->fill || !v->rgb) return 0;
    for (int y = 0; y < nrows; y++) for (int x = 0; x < w; x++) {
        unsigned char c = x < (int)strlen(rows[y]) ? (unsigned char)rows[y][x] : '.';
        if (c == '.' || c == ' ') continue;
        v->fill[y * w + x] = 1; unsigned char *p = &v->rgb[(y * w + x) * 3];
        if (has[c]) { p[0] = leg[c][0]; p[1] = leg[c][1]; p[2] = leg[c][2]; } else { p[0] = p[1] = p[2] = 160; }
    }
    return 1;
}

static int load_png(const char *path, View *v, int have_bg, const int bg[3]) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { if (png) png_destroy_read_struct(&png, &info, NULL); fclose(f); return 0; }
    png_init_io(png, f); png_read_info(png, info);
    png_set_expand(png); png_set_strip_16(png); png_set_gray_to_rgb(png); png_set_palette_to_rgb(png);
    if (!(png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA)) png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    int w = (int)png_get_image_width(png, info), h = (int)png_get_image_height(png, info);
    if (w < 1 || h < 1 || w > MAXDIM || h > MAXDIM || png_get_rowbytes(png, info) < (size_t)w * 4) { png_destroy_read_struct(&png, &info, NULL); fclose(f); return 0; }
    unsigned char *px = malloc((size_t)w * h * 4); png_bytep *rp = malloc(sizeof(png_bytep) * (size_t)h);
    if (!px || !rp) { png_destroy_read_struct(&png, &info, NULL); fclose(f); free(px); free(rp); return 0; }
    for (int y = 0; y < h; y++) rp[y] = px + (size_t)y * w * 4;
    png_read_image(png, rp); png_destroy_read_struct(&png, &info, NULL); fclose(f); free(rp);
    int anytrans = 0; for (int i = 0; i < w * h; i++) if (px[i * 4 + 3] < 255) { anytrans = 1; break; }
    int b[3] = { px[0], px[1], px[2] }; if (have_bg) { b[0] = bg[0]; b[1] = bg[1]; b[2] = bg[2]; }
    v->w = w; v->h = h; v->fill = calloc((size_t)w * h, 1); v->rgb = calloc((size_t)w * h, 3);
    if (!v->fill || !v->rgb) { free(px); return 0; }
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4]; int on = p[3] >= 128;
        if (on && !anytrans && p[0] == b[0] && p[1] == b[1] && p[2] == b[2]) on = 0;
        if (on) { v->fill[i] = 1; v->rgb[i * 3] = p[0]; v->rgb[i * 3 + 1] = p[1]; v->rgb[i * 3 + 2] = p[2]; }
    }
    free(px); return 1;
}

static int load_view(const char *path, View *v, int have_bg, const int bg[3]) {
    memset(v, 0, sizeof *v);
    if (ends_with(path, ".png")) return load_png(path, v, have_bg, bg);
    if (ends_with(path, ".txt")) return load_txt(path, v);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 5) goto usage;
    const char *out = argv[1]; int solid = 0, have_bg = 0, bg[3] = { 0, 0, 0 }; long max_vox = 0; double min_cons = 1.0;
    for (int i = 5; i < argc; i++) {
        if (!strcmp(argv[i], "--solid")) solid = 1;
        else if (!strcmp(argv[i], "--max-vox") && i + 1 < argc) max_vox = atol(argv[++i]);
        else if (!strcmp(argv[i], "--min-consistency") && i + 1 < argc) min_cons = atof(argv[++i]);
        else if (!strcmp(argv[i], "--bg") && i + 1 < argc) { if (sscanf(argv[++i], "%d,%d,%d", &bg[0], &bg[1], &bg[2]) != 3) goto usage; have_bg = 1; }
        else goto usage;
    }
    View F, S, T;
    if (!load_view(argv[2], &F, have_bg, bg)) { fprintf(stderr, "views_to_voxel: cannot read the front picture %s (.png or .txt, at most %d x %d)\n", argv[2], MAXDIM, MAXDIM); return 2; }
    if (!load_view(argv[3], &S, have_bg, bg)) { fprintf(stderr, "views_to_voxel: cannot read the side picture %s\n", argv[3]); return 2; }
    if (!load_view(argv[4], &T, have_bg, bg)) { fprintf(stderr, "views_to_voxel: cannot read the top picture %s\n", argv[4]); return 2; }
    int DX = F.w, DY = F.h, DZ = S.w;
    if (T.w != DX || T.h != DZ || S.h != DY) { fprintf(stderr, "views_to_voxel: the pictures disagree on size: front %dx%d (X x Y), side %dx%d (Z x Y), top %dx%d (X x Z); need top = %dx%d and side height = %d\n", F.w, F.h, S.w, S.h, T.w, T.h, DX, DZ, DY); return 2; }
    size_t N = (size_t)DX * DY * DZ; unsigned char *H = calloc(N, 1); if (!H) return 2;
#define ID(x, y, z) (((size_t)(z) * DY + (y)) * DX + (x))                /* y = 0 is the BOTTOM of the model */
#define FP(x, y)  F.fill[(DY - 1 - (y)) * DX + (x)]
#define SP(z, y)  S.fill[(DY - 1 - (y)) * DZ + (z)]
#define TP(x, z)  T.fill[(z) * DX + (x)]
    long hull = 0;
    for (int z = 0; z < DZ; z++) for (int y = 0; y < DY; y++) for (int x = 0; x < DX; x++) if (FP(x, y) && SP(z, y) && TP(x, z)) { H[ID(x, y, z)] = 1; hull++; }
    /* consistency: how much of each silhouette the hull's own projection reproduces */
    long fc = 0, ff = 0, sc = 0, sf = 0, tc = 0, tf = 0;
    for (int y = 0; y < DY; y++) for (int x = 0; x < DX; x++) if (FP(x, y)) { ff++; for (int z = 0; z < DZ; z++) if (H[ID(x, y, z)]) { fc++; break; } }
    for (int y = 0; y < DY; y++) for (int z = 0; z < DZ; z++) if (SP(z, y)) { sf++; for (int x = 0; x < DX; x++) if (H[ID(x, y, z)]) { sc++; break; } }
    for (int z = 0; z < DZ; z++) for (int x = 0; x < DX; x++) if (TP(x, z)) { tf++; for (int y = 0; y < DY; y++) if (H[ID(x, y, z)]) { tc++; break; } }
    double cF = ff ? (double)fc / ff : 0.0, cS = sf ? (double)sc / sf : 0.0, cT = tf ? (double)tc / tf : 0.0, cons = cF < cS ? cF : cS; if (cT < cons) cons = cT;
    /* write */
    long written = 0; FILE *o = fopen(out, "w"); if (!o) { fprintf(stderr, "views_to_voxel: cannot write %s\n", out); return 2; }
    for (int pass = 0; pass < 2; pass++) {                              /* pass 0 counts, pass 1 writes: --max-vox is checked before a single byte is written */
        written = 0;
        for (int z = 0; z < DZ; z++) for (int y = 0; y < DY; y++) for (int x = 0; x < DX; x++) {
            if (!H[ID(x, y, z)]) continue;
            if (!solid) { int inner = x > 0 && x < DX - 1 && y > 0 && y < DY - 1 && z > 0 && z < DZ - 1 && H[ID(x - 1, y, z)] && H[ID(x + 1, y, z)] && H[ID(x, y - 1, z)] && H[ID(x, y + 1, z)] && H[ID(x, y, z - 1)] && H[ID(x, y, z + 1)]; if (inner) continue; }
            written++;
            if (pass == 0) continue;
            const unsigned char *c; int zz, yy, xx;
            int frontmost = 1, topmost = 1, rightmost = 1; (void)zz; (void)yy; (void)xx;
            for (zz = 0; zz < z; zz++) if (H[ID(x, y, zz)]) { frontmost = 0; break; }
            for (yy = DY - 1; yy > y; yy--) if (H[ID(x, yy, z)]) { topmost = 0; break; }
            for (xx = DX - 1; xx > x; xx--) if (H[ID(xx, y, z)]) { rightmost = 0; break; }
            if (frontmost) c = &F.rgb[((DY - 1 - y) * DX + x) * 3]; else if (topmost) c = &T.rgb[(z * DX + x) * 3]; else if (rightmost) c = &S.rgb[((DY - 1 - y) * DZ + z) * 3]; else c = &F.rgb[((DY - 1 - y) * DX + x) * 3];
            fprintf(o, "%d %d %d %d %d %d\n", x, y, z, c[0], c[1], c[2]);
        }
        if (pass == 0) { if (max_vox > 0 && written > max_vox) { fclose(o); remove(out); fprintf(stderr, "views_to_voxel: %ld voxels exceed --max-vox %ld\n", written, max_vox); printf("hull_voxels=%ld written_voxels=%ld\n", hull, written); return 3; }
            fprintf(o, "dim %d %d %d\n", DX, DY, DZ); }
    }
    fclose(o);
    printf("dim_x=%d dim_y=%d dim_z=%d\nhull_voxels=%ld\nwritten_voxels=%ld\nconsistency_front=%.4f\nconsistency_side=%.4f\nconsistency_top=%.4f\nconsistency=%.4f\nfits_engine=%d\n", DX, DY, DZ, hull, written, cF, cS, cT, cons, DX <= 32 && DY <= 32 && DZ <= 16);
    if (hull == 0) { remove(out); fprintf(stderr, "views_to_voxel: the three pictures share no volume (empty hull)\n"); return 4; }
    if (cons + 1e-9 < min_cons) { remove(out); fprintf(stderr, "views_to_voxel: the pictures disagree (consistency %.4f < %.4f): not one solid thing\n", cons, min_cons); return 4; }
    return 0;
usage:
    fprintf(stderr, "Usage: views_to_voxel <out_model.txt> <front.png|txt> <side.png|txt> <top.png|txt> [--solid] [--max-vox N] [--min-consistency F] [--bg R,G,B]\n"); return 1;
}
