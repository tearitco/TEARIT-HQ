/* mr_monster_extract - real, GENERALIZED RPG Maker monster-sheet
 * extraction, 2026-08-05.
 *
 * Direct instruction: extract real monsters from $BigMonster1.png/
 * $BigMonster2.png. Unlike this house's own tp_rmmv_character_extract.c
 * (hardcoded FRAME_PX=48, real standard 4-col x 2-row x 3x4-frame RMMV
 * character-sheet layout), these two real sheets use a DIFFERENT,
 * NON-STANDARD layout (confirmed via direct pixel inspection, not
 * guessed): ONE monster per ROW (no 4-direction sub-grid), 3 walk-cycle
 * frames per COLUMN, and a real, DIFFERENT cell size per sheet
 * ($BigMonster1.png = 96x96, $BigMonster2.png = 120x120 - neither the
 * standard 48px, and not even the same as each other). Real precedent
 * reused, not reinvented: same box-filter downscale_to_NxN() + CSV
 * writer as tp_rmmv_character_extract.c/tp_asset_to_sprite.c (ported
 * verbatim), just with the CROP math adapted for this real, different
 * sheet shape.
 *
 * Usage: mr_monster_extract.+x <sheet.png> <row> <cell_size> <output_csv> [resolution] [frame_col]
 *   row: 0-based monster row index (confirmed 4 real monsters per
 *        sheet, rows 0-3, for both real sheets this session found).
 *   cell_size: real per-sheet frame size in px (96 for BigMonster1,
 *        120 for BigMonster2 - NOT a shared constant, pass explicitly).
 *   resolution: output NxN CSV resolution (default 64, matching this
 *        house's own real sprite.csv convention already used by pets).
 *   frame_col: which of the 3 walk-frame columns to use (default 1,
 *        the middle/standing frame - same real convention
 *        tp_rmmv_character_extract.c's own comment already documents).
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_COLS 3

typedef struct { unsigned char r, g, b, a; } RGBA_Pixel;

/* Verbatim port of tp_rmmv_character_extract.c's/tp_asset_to_sprite.c's
 * own real box-filter downscale - same proven algorithm, not
 * reinvented. */
static void downscale_to_NxN(unsigned char *src, int sw, int sh, int channels, int N, RGBA_Pixel *dst) {
    float xr = (float)sw / (float)N, yr = (float)sh / (float)N;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int sx0 = (int)(x * xr), sy0 = (int)(y * yr);
            int sx1 = (int)((x + 1) * xr), sy1 = (int)((y + 1) * yr);
            /* REAL FIX (same class already found+fixed this session in
             * tp_rmmv_character_extract.c's own copy of this function):
             * when N > source dimension (upscaling), truncation can
             * make sx1<=sx0, leaving an empty box-filter window and an
             * uninitialized destination pixel. Clamp to at least 1
             * source pixel sampled. */
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            if (sx1 > sw) sx1 = sw;
            if (sy1 > sh) sy1 = sh;
            if (sx0 >= sx1) sx0 = sx1 - 1;
            if (sy0 >= sy1) sy0 = sy1 - 1;
            long sr = 0, sg = 0, sb = 0, sa = 0;
            int count = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                for (int sx = sx0; sx < sx1; sx++) {
                    int idx = (sy * sw + sx) * channels;
                    sr += src[idx]; sg += src[idx + 1]; sb += src[idx + 2];
                    sa += (channels == 4) ? src[idx + 3] : 255;
                    count++;
                }
            }
            if (count > 0) {
                dst[y * N + x].r = (unsigned char)(sr / count);
                dst[y * N + x].g = (unsigned char)(sg / count);
                dst[y * N + x].b = (unsigned char)(sb / count);
                dst[y * N + x].a = (unsigned char)(sa / count);
            }
        }
    }
}

static int write_csv(const char *path, int N, RGBA_Pixel *px) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", N);
    for (int i = 0; i < N * N; i++)
        fprintf(f, "%d,%d,%d,%d\n", px[i].r, px[i].g, px[i].b, px[i].a);
    fclose(f);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <sheet.png> <row> <cell_size> <output_csv> [resolution] [frame_col]\n", argv[0]);
        return 1;
    }
    const char *sheet_path = argv[1];
    int row = atoi(argv[2]);
    int cell_size = atoi(argv[3]);
    const char *out_path = argv[4];
    int N = (argc >= 6) ? atoi(argv[5]) : 64;
    if (N <= 0) N = 64;
    int frame_col = (argc >= 7) ? atoi(argv[6]) : 1;
    if (frame_col < 0 || frame_col >= FRAME_COLS) frame_col = 1;
    if (row < 0) { fprintf(stderr, "mr_monster_extract: row must be >= 0\n"); return 1; }
    if (cell_size <= 0) { fprintf(stderr, "mr_monster_extract: cell_size must be > 0\n"); return 1; }

    int w, h, channels;
    unsigned char *img = stbi_load(sheet_path, &w, &h, &channels, 0);
    if (!img) {
        fprintf(stderr, "mr_monster_extract: could not load %s\n", sheet_path);
        return 1;
    }

    int frame_x0 = frame_col * cell_size;
    int frame_y0 = row * cell_size;
    if (frame_x0 + cell_size > w || frame_y0 + cell_size > h) {
        fprintf(stderr, "mr_monster_extract: row %d out of bounds for %dx%d sheet (cell_size=%d)\n",
                row, w, h, cell_size);
        stbi_image_free(img);
        return 1;
    }

    unsigned char *frame = malloc((size_t)cell_size * cell_size * channels);
    if (!frame) { stbi_image_free(img); return 1; }
    for (int y = 0; y < cell_size; y++) {
        memcpy(frame + (size_t)y * cell_size * channels,
               img + (size_t)((frame_y0 + y) * w + frame_x0) * channels,
               (size_t)cell_size * channels);
    }
    stbi_image_free(img);

    RGBA_Pixel *downsampled = malloc((size_t)N * N * sizeof(RGBA_Pixel));
    if (!downsampled) { free(frame); return 1; }
    downscale_to_NxN(frame, cell_size, cell_size, channels, N, downsampled);
    free(frame);

    int ok = write_csv(out_path, N, downsampled);
    free(downsampled);
    if (!ok) { fprintf(stderr, "mr_monster_extract: could not write %s\n", out_path); return 1; }
    printf("SPRITE %s (row %d, cell_size %d, frame_col %d -> %dx%d)\n",
           out_path, row, cell_size, frame_col, N, N);
    return 0;
}
