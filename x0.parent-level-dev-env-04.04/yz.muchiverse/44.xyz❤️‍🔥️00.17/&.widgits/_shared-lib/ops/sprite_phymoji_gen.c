/* sprite_phymoji_gen - real PyMoji-standard voxel generator, driven
 * by an EXISTING sprite.csv (a desktop entity's own real per-pixel
 * RGBA texture, tp_desktop_window_rgb.c's own real sprite.csv format
 * - "# resolution=N" header, then r,g,b,a rows) instead of re-
 * rasterizing a Unicode emoji glyph from a font.
 *
 * Direct instruction, 2026-08-30 ("u should make a script to do
 * phymoji of all entities. save it locally in shared. and all new
 * entities will use it as well"), and the real bug that made this
 * necessary: piececraft-hq's own real ops/pc_phymoji_gen.c (the
 * house's existing phymoji generator) only ever shells out to
 * emoji_gen_atlas.+x, which rasterizes the RAW Unicode codepoint via
 * NotoColorEmoji.ttf - confirmed live this can be a COMPLETELY
 * DIFFERENT image than a desktop entity's own actual sprite.csv art
 * (cursword's own 🗡️ sprite is a custom, straight-vertical sword;
 * the raw Noto glyph for the same codepoint is a diagonal, sparkling
 * dagger) - generating a phymoji asset from the wrong source silently
 * produces a real, working-but-WRONG-shaped 3D model. This tool reads
 * the REAL, ALREADY-CORRECT source (the entity's own sprite.csv)
 * directly, so the voxel model always genuinely matches what the
 * entity actually looks like in 2D.
 *
 * Real pipeline (deliberately the SAME real algorithm as
 * pc_phymoji_gen.c's own Stage 2-6, ported not reinvented - see that
 * file's own header comment for the full real PyMoji-spec mapping):
 *   1. Real box-filter downscale to a fixed TILE_N x TILE_N tile
 *      (same real algorithm as emoji_xtract.c's own downscale_to_NxN,
 *      duplicated per this house's own no-shared-headers convention -
 *      pc_phymoji_gen.c's own real current constant, TILE_N=16, not
 *      the older/canonical "8x8x8" this house's own phymoji.md still
 *      describes - matches the REAL, already-shipped chicken asset's
 *      own real 14x13x8 shape, not a stricter spec that's no longer
 *      what the actual generator produces).
 *   2. Real alpha-aware bounding-box crop (a >= 128), "dead space"
 *      removed.
 *   3. Real depth extrusion, D=8 by default, full source color at
 *      every z-layer (a real mirror of the front, matching
 *      pc_phymoji_gen.c's own 2026-08-04 "why are the phymojis all
 *      dark on the backside" fix - the single-viewpoint sprite art
 *      has no real back-face data of its own, so every layer just
 *      repeats the real front color, same real reasoning).
 *   4. Real 0..7-space local coords, RAW pixel offset from the crop's
 *      own origin (no stretch - matches pc_phymoji_gen.c's own
 *      2026-08-04 fix for the "2 parallel sides, empty in the middle"
 *      bug), bottom-left Y origin (source rows are top-down, flipped
 *      here to match the standard's own real bottom-left convention).
 *   5. Real 3D voxel CSV export (x,y,z,r,g,b) to
 *      <out_dir>/voxels.csv - same real format/location convention
 *      pc_phymoji_gen.c already established, so tp_desktop_window_
 *      rgb.c's own real phymoji loader (load_entity_phymoji()) reads
 *      either source's own output identically, no format difference.
 *
 * Self-contained, no shared headers, no external libraries needed
 * (unlike pc_phymoji_gen.c, this never touches a PNG at all - the
 * source is already real RGBA text data).
 *
 * Usage: sprite_phymoji_gen.+x <sprite_csv_path> <out_dir> [depth=8] */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SPRITE_RES 256
#define TILE_N 16
#define DEFAULT_DEPTH 8

typedef struct { unsigned char r, g, b, a; } RGBA_Pixel;

/* Real load of tp_desktop_window_rgb.c's own real sprite.csv format -
 * "# resolution=N" header row (defaults to sqrt(row_count) if
 * missing, matching that file's own load_sprite_csv() real fallback),
 * then "r,g,b,a" per pixel, row-major. */
static int load_sprite(const char *path, RGBA_Pixel *out, int *out_res) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "sprite_phymoji_gen: cannot open %s\n", path); return 0; }
    char line[256];
    int res = 0, n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') {
            int r;
            if (sscanf(line, "# resolution=%d", &r) == 1) res = r;
            continue;
        }
        if (strncmp(line, "r,g,b,a", 7) == 0) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            if (n < MAX_SPRITE_RES * MAX_SPRITE_RES) {
                out[n].r = (unsigned char)r; out[n].g = (unsigned char)g;
                out[n].b = (unsigned char)b; out[n].a = (unsigned char)a;
            }
            n++;
        }
    }
    fclose(f);
    if (res <= 0) { for (res = 1; res * res < n; res++) {} }
    *out_res = res;
    return (res > 0 && n > 0);
}

/* Real box-filter downscale, same real algorithm as pc_phymoji_gen.c's
 * own downscale_to_NxN() (itself ported from emoji_xtract.c) - ported
 * again here rather than shared, per this house's own convention. */
static void downscale_to_NxN(RGBA_Pixel *src, int sw, int sh, int N, RGBA_Pixel *dst) {
    float xr = (float)sw / (float)N, yr = (float)sh / (float)N;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int x0 = (int)(x * xr), y0 = (int)(y * yr);
            int x1 = (int)((x + 1) * xr), y1 = (int)((y + 1) * yr);
            if (x1 > sw) x1 = sw;
            if (y1 > sh) y1 = sh;
            long sr = 0, sg = 0, sb = 0, sa = 0;
            int n = 0;
            for (int sy = y0; sy < y1; sy++) {
                for (int sx = x0; sx < x1; sx++) {
                    RGBA_Pixel p = src[sy * sw + sx];
                    sr += p.r; sg += p.g; sb += p.b; sa += p.a;
                    n++;
                }
            }
            if (n > 0) {
                dst[y * N + x].r = (unsigned char)(sr / n);
                dst[y * N + x].g = (unsigned char)(sg / n);
                dst[y * N + x].b = (unsigned char)(sb / n);
                dst[y * N + x].a = (unsigned char)(sa / n);
            } else {
                dst[y * N + x].r = dst[y * N + x].g = dst[y * N + x].b = dst[y * N + x].a = 0;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: sprite_phymoji_gen.+x <sprite_csv_path> <out_dir> [depth]\n");
        return 1;
    }
    const char *sprite_path = argv[1];
    const char *out_dir = argv[2];
    int depth = (argc >= 4) ? atoi(argv[3]) : DEFAULT_DEPTH;
    if (depth < 1) depth = DEFAULT_DEPTH;

    static RGBA_Pixel src[MAX_SPRITE_RES * MAX_SPRITE_RES];
    int res = 0;
    if (!load_sprite(sprite_path, src, &res)) return 1;

    RGBA_Pixel tile[TILE_N * TILE_N];
    downscale_to_NxN(src, res, res, TILE_N, tile);

    int min_x = TILE_N, min_y = TILE_N, max_x = -1, max_y = -1;
    for (int y = 0; y < TILE_N; y++) {
        for (int x = 0; x < TILE_N; x++) {
            if (tile[y * TILE_N + x].a >= 128) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    if (max_x < min_x) {
        fprintf(stderr, "sprite_phymoji_gen: '%s' fully transparent, nothing to extrude\n", sprite_path);
        return 1;
    }
    int crop_w = max_x - min_x + 1, crop_h = max_y - min_y + 1;

    char mkdir_cmd[PATH_BUF + 16];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", out_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }

    char csv_path[PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/voxels.csv", out_dir);
    FILE *csv = fopen(csv_path, "w");
    if (!csv) {
        fprintf(stderr, "sprite_phymoji_gen: could not open %s for writing\n", csv_path);
        return 1;
    }
    fprintf(csv, "x,y,z,r,g,b\n");

    int voxel_count = 0;
    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            RGBA_Pixel p = tile[py * TILE_N + px];
            if (p.a < 128) continue;
            int norm_x = px - min_x;
            int norm_y = (crop_h - 1) - (py - min_y); /* bottom-left flip */
            for (int z = 0; z < depth; z++) {
                int norm_z = (int)lround((depth > 1) ? (7.0 * z / (depth - 1)) : 0.0);
                fprintf(csv, "%d,%d,%d,%d,%d,%d\n", norm_x, norm_y, norm_z, p.r, p.g, p.b);
                voxel_count++;
            }
        }
    }
    fclose(csv);

    printf("sprite_phymoji_gen: %s -> %s (%d voxels, crop %dx%d, depth %d)\n",
           sprite_path, csv_path, voxel_count, crop_w, crop_h, depth);
    return 0;
}
