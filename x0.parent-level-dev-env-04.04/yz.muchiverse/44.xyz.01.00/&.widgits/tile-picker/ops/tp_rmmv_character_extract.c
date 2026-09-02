/* tp_rmmv_character_extract - extract ONE individual character icon from
 * a real RPG Maker MV/MZ character sheet, for use as a desktop-entity
 * asset (see asset.pal's own "rmmv_character=" key in
 * tp_desktop_window.c's apply_asset_override()).
 *
 * Usage: tp_rmmv_character_extract.+x <sheet.png> <slot 0-7> <output_csv> [resolution] [direction 0-3]
 * direction: 0=down (default) 1=left 2=right 3=up - real RMMV row order
 * within a slot's own 3x4 frame grid. Direct instruction 2026-08-04:
 * "we will have them face in direction they are moving, since the
 * tilesheets allow for this" - a real AI tick loop can re-run this op
 * with a different direction as a pet's movement direction changes,
 * regenerating sprite.csv to match, same "just rewrite the file, the
 * renderer re-reads it" convention every other live-updating file in
 * this house already uses.
 *
 * REAL RPG MAKER MV/MZ LAYOUT (confirmed 2026-08-04 by direct
 * inspection of a real character sheet, Actor1.png, 576x384px): a
 * "large" character sheet holds 8 character slots, arranged 4 columns x
 * 2 rows of slots. Each slot is itself a 3x4 grid of 48x48 walk-cycle
 * frames (3 animation frames per direction x 4 directions: down, left,
 * right, up, in that row order). This op extracts ONE slot's default
 * "facing down, standing" frame - the middle column (frame 1 of 0..2)
 * of the slot's first row (direction=down) - as a single real, static
 * icon, matching the "individual" (one entity = one specific
 * character) designation, as distinct from a whole tileset/tilemap
 * (see TILE_PICKER_DESIGN.md's own future §on RMMV tile addressing for
 * the "tile on a shared tilemap" designation this op does NOT attempt).
 *
 * Reuses the same real box-filter downscale + CSV writer as
 * tp_asset_to_sprite.c (kept as a separate, small op rather than a
 * shared library, matching this house's own "self-contained ops, no
 * shared headers" convention already established by every op in this
 * project).
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_COLS 4
#define SLOT_ROWS 2
#define FRAMES_PER_ROW 3
#define DIRECTIONS_PER_SLOT 4
#define FRAME_PX 48

typedef struct { unsigned char r, g, b, a; } RGBA_Pixel;

static void downscale_to_NxN(unsigned char *src, int sw, int sh, int channels, int N, RGBA_Pixel *dst) {
    /* REAL FIX 2026-08-04 ("grainy, translucent, overlaid with multi-
     * color static" on dog/cat/chicken desktop icons - direct user
     * report, root cause confirmed via byte-for-byte comparison
     * against a Python crop of the same real source frame, which was
     * clean). This op is called with N=64 against a 48px RMMV frame -
     * an UPSCALE (xr=yr=0.75<1), unlike this function's other real
     * caller (tp_asset_to_sprite.c downscaling a large user photo to
     * 16px, always a true downscale). When ratio<1, truncating
     * (int)(x*xr) and (int)((x+1)*xr) can land on the SAME integer for
     * many x, making the box-filter window empty - count stays 0, and
     * dst[y*N+x] is left as whatever malloc() handed back (uninitialized
     * heap memory), rendered as literal random-byte "static" specks
     * across the sprite. Clamping the window to at least 1 source
     * pixel makes this a real (blocky) upscale instead of silently
     * skipping the pixel. */
    float xr = (float)sw / (float)N, yr = (float)sh / (float)N;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int sx0 = (int)(x * xr), sy0 = (int)(y * yr);
            int sx1 = (int)((x + 1) * xr), sy1 = (int)((y + 1) * yr);
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
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <sheet.png> <slot 0-7> <output_csv> [resolution]\n", argv[0]);
        return 1;
    }
    const char *sheet_path = argv[1];
    int slot = atoi(argv[2]);
    const char *out_path = argv[3];
    int N = (argc >= 5) ? atoi(argv[4]) : 64;
    if (N <= 0) N = 64;
    int direction = (argc >= 6) ? atoi(argv[5]) : 0;
    if (direction < 0 || direction > 3) direction = 0;
    if (slot < 0 || slot >= SLOT_COLS * SLOT_ROWS) {
        fprintf(stderr, "tp_rmmv_character_extract: slot must be 0-%d\n", SLOT_COLS * SLOT_ROWS - 1);
        return 1;
    }

    int w, h, channels;
    unsigned char *img = stbi_load(sheet_path, &w, &h, &channels, 0);
    if (!img) {
        fprintf(stderr, "tp_rmmv_character_extract: could not load %s\n", sheet_path);
        return 1;
    }

    int slot_col = slot % SLOT_COLS;
    int slot_row = slot / SLOT_COLS;
    int slot_w = FRAMES_PER_ROW * FRAME_PX;
    int slot_h = DIRECTIONS_PER_SLOT * FRAME_PX;
    int slot_x0 = slot_col * slot_w;
    int slot_y0 = slot_row * slot_h;
    /* Middle column (index 1, the "standing still" pose) of the row for
     * the requested direction (real RMMV row order: 0=down 1=left
     * 2=right 3=up). */
    int frame_x0 = slot_x0 + 1 * FRAME_PX;
    int frame_y0 = slot_y0 + direction * FRAME_PX;

    if (frame_x0 + FRAME_PX > w || frame_y0 + FRAME_PX > h) {
        fprintf(stderr, "tp_rmmv_character_extract: slot %d out of bounds for %dx%d sheet\n", slot, w, h);
        stbi_image_free(img);
        return 1;
    }

    unsigned char *frame = malloc((size_t)FRAME_PX * FRAME_PX * channels);
    if (!frame) { stbi_image_free(img); return 1; }
    for (int y = 0; y < FRAME_PX; y++) {
        memcpy(frame + (size_t)y * FRAME_PX * channels,
               img + (size_t)((frame_y0 + y) * w + frame_x0) * channels,
               (size_t)FRAME_PX * channels);
    }
    stbi_image_free(img);

    RGBA_Pixel *downsampled = malloc((size_t)N * N * sizeof(RGBA_Pixel));
    if (!downsampled) { free(frame); return 1; }
    downscale_to_NxN(frame, FRAME_PX, FRAME_PX, channels, N, downsampled);
    free(frame);

    int ok = write_csv(out_path, N, downsampled);
    free(downsampled);
    if (!ok) { fprintf(stderr, "tp_rmmv_character_extract: could not write %s\n", out_path); return 1; }
    printf("SPRITE %s (slot %d, direction %d idle frame -> %dx%d)\n", out_path, slot, direction, N, N);
    return 0;
}
