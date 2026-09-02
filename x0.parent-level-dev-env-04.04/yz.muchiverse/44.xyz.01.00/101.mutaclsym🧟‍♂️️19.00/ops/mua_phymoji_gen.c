/* pc_phymoji_gen - real PyMoji-standard voxel generator, per phymoji.md
 * (this project's own real adoption plan) and the real reference spec
 * it builds on: #.ref/plugy3d-ngn-2026_v19.PHYM/#.PYMOJI.md.
 *
 * Real pipeline, per that spec's own §8.1, each stage mapped to real
 * code (see phymoji.md §2 for the full writeup of what's reused vs
 * new):
 *   1. Font rasterize - SHELLS OUT to the house's own real, already-
 *      working emoji_gen_atlas.+x (FreeType + NotoColorEmoji.ttf),
 *      copied locally same as every other project that uses it
 *      (board-viewer's own build.sh copies it from wsr-pal - same
 *      real pattern, not reinvented).
 *   2. Downscale to 8x8 - real box-filter, ported from emoji_xtract.c's
 *      own downscale_to_NxN() (duplicated, not shared, per this
 *      house's own no-shared-headers convention).
 *   3. Real bounding-box crop ("dead space" removal, direct
 *      instruction) - alpha-aware, only the real occupied region
 *      survives.
 *   4. Real depth extrusion - D=8 voxels per opaque pixel, PyMoji §4
 *      Rules A/B/C (front crisp, sides/top attenuated by depth, back
 *      additionally darkened) - colors are BAKED at generation time
 *      (matches the standard's own §6.1 example - stored voxel color
 *      already reflects its own depth attenuation, not computed at
 *      render time).
 *   5. Real 0..7 coordinate normalization (PyMoji §5.4), origin
 *      flipped to bottom-left per that section's own recommendation.
 *   6. Real 3D voxel CSV export (PyMoji §6.2: x,y,z,r,g,b) to
 *      pieces/registry/phymoji_assets/<entity_id>/voxels.csv - a real
 *      TEMPLATE, shared/read-only (phymoji.md §3's own resolved
 *      decision - per-world-instance destructible state is a SEPARATE
 *      file, never written here, see pc_phymoji_mine.c).
 *
 * Self-contained except for the one real header-only library needed to
 * read the PNG (ops/phymoji_lib/stb_image.h, same file the house's own
 * emoji_gen_atlas.c/emoji_xtract.c already depend on, copied locally
 * per this house's own established per-project-copy convention, not a
 * cross-project shared include).
 *
 * Usage: pc_phymoji_gen.+x <emoji_utf8> <entity_id> [depth=8] */
#define _GNU_SOURCE
#define STB_IMAGE_IMPLEMENTATION
#include "phymoji_lib/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
/* REAL FIX 2026-08-04, direct user report ("hero looks nothing like a
 * human, solid cube - didn't remove unused colors"): the real root
 * cause was resolution, not a filtering bug - the emoji_gen_atlas.+x
 * rasterizer already produces a real 64x64 RGBA glyph with real
 * transparent gaps (between arms/legs etc), but downscaling that all
 * the way to a canonical 8x8 tile (TILE_N) collapses a real 2-pixel-
 * wide crop down to nothing recognizable - too few source samples
 * left to show any silhouette detail, so the whole crop reads as one
 * solid opaque block. Raised to 32 (only a 2x downscale from the real
 * 64px raster, not 8x) so real limb/gap detail actually survives into
 * the voxel model. TILE_N is a real, direct knob - PyMoji's own
 * pipeline is resolution-agnostic (every downstream stage already
 * reads TILE_N, not a hardcoded 8), so raising it needed no other
 * real code changes here.
 *
 * REVISED 2026-08-04, direct user question ("why are there 3400
 * voxels per tree? shouldnt there only be 16x16x16?"): a real, fair
 * catch - TILE_N only ever controlled the X/Y footprint, DEFAULT_DEPTH
 * (below) is a genuinely SEPARATE axis, so 32 produced a real, lopsided
 * 32x32x8 max, not a symmetric cube - and the larger footprint was
 * also the direct real cause of the same-day "slows when trees come
 * into view" report (more real occupied columns = more real per-pixel
 * work). Settled on 16 - a real 4x reduction in max footprint from 32
 * (real perf win) while still being a real 2x improvement over the
 * original 8 that caused the "solid cube, no human detail" bug in the
 * first place. */
#define TILE_N 16
#define DEFAULT_DEPTH 8
#define RASTER_SIZE 64

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

static char project_root[MAX_PATH] = ".";
static char real_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);

    snprintf(real_root, sizeof(real_root), "%s", project_root);
    char real_root_path[MAX_PATH + 64];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", project_root);
    FILE *rf = fopen(real_root_path, "r");
    if (rf) {
        char buf[MAX_PATH];
        if (fgets(buf, sizeof(buf), rf)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            if (buf[0]) snprintf(real_root, sizeof(real_root), "%s", buf);
        }
        fclose(rf);
    }
}

/* Real box-filter downscale, ported from the house's own real
 * emoji_xtract.c (ops/emoji_xtract.c:18-58, cited directly - same
 * average-of-source-block algorithm, duplicated not shared). */
static void downscale_to_NxN(unsigned char *src, int sw, int sh, int channels,
                              int N, RGBA_Pixel *dst) {
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
                    int idx = (sy * sw + sx) * channels;
                    sr += src[idx]; sg += src[idx + 1]; sb += src[idx + 2];
                    sa += (channels == 4) ? src[idx + 3] : 255;
                    n++;
                }
            }
            if (n > 0) {
                dst[y * N + x].r = (unsigned char)(sr / n);
                dst[y * N + x].g = (unsigned char)(sg / n);
                dst[y * N + x].b = (unsigned char)(sb / n);
                dst[y * N + x].a = (unsigned char)(sa / n);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pc_phymoji_gen.+x <emoji_utf8> <entity_id> [depth]\n");
        return 1;
    }
    resolve_root();

    const char *emoji_utf8 = argv[1];
    const char *entity_id = argv[2];
    int depth = (argc >= 4) ? atoi(argv[3]) : DEFAULT_DEPTH;
    if (depth < 1) depth = DEFAULT_DEPTH;

    /* Stage 1: real font rasterize, shell out to the house's own real
     * emoji_gen_atlas.+x (must already be copied into THIS project's
     * own ops/+x/ by build.sh, same real pattern board-viewer uses). */
    char tmp_dir[PATH_BUF];
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/pieces/system/phymoji_tmp", real_root);
    char mkdir_cmd[PATH_BUF + 16];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", tmp_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }

    char png_path[PATH_BUF];
    snprintf(png_path, sizeof(png_path), "%s/%s.png", tmp_dir, entity_id);
    char raster_cmd[PATH_BUF * 2];
    snprintf(raster_cmd, sizeof(raster_cmd),
             "'%s/ops/+x/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1",
             project_root, emoji_utf8, png_path);
    if (system(raster_cmd) != 0) {
        fprintf(stderr, "pc_phymoji_gen: emoji_gen_atlas.+x failed for '%s'\n", emoji_utf8);
        return 1;
    }

    int w, h, channels;
    unsigned char *raw = stbi_load(png_path, &w, &h, &channels, 4);
    if (!raw) {
        fprintf(stderr, "pc_phymoji_gen: could not load rasterized PNG %s\n", png_path);
        return 1;
    }
    channels = 4;

    /* Stage 2: real box-filter downscale to the canonical 8x8 tile
     * (PyMoji §2.1). */
    RGBA_Pixel tile[TILE_N * TILE_N];
    downscale_to_NxN(raw, w, h, channels, TILE_N, tile);
    stbi_image_free(raw);
    remove(png_path);

    /* Stage 3: real alpha-aware bounding-box crop - "removed of their
     * dead space" per direct instruction. Binary threshold (a >= 128)
     * per PyMoji §3.2's own simpler option, matching what this file's
     * own extrusion pass (stage 4) also uses for voxel existence. */
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
        fprintf(stderr, "pc_phymoji_gen: '%s' rasterized fully transparent, nothing to extrude\n", emoji_utf8);
        return 1;
    }
    int crop_w = max_x - min_x + 1, crop_h = max_y - min_y + 1;

    /* Stage 4/5/6: real depth extrusion with PyMoji §4 Rules A/B/C,
     * baked colors (matches the standard's own §6.1 example), real
     * 0..7 normalization (§5.4) over the CROPPED extent (so a small
     * cropped shape still fills the canonical 0..7 space, not a tiny
     * corner of it), bottom-left Y origin (source PNG rows are top-
     * down, standard recommends bottom-left for voxel alignment - flip
     * here). */
    char out_dir[PATH_BUF];
    snprintf(out_dir, sizeof(out_dir), "%s/pieces/registry/phymoji_assets/%s", real_root, entity_id);
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", out_dir);
    { int _rc = system(mkdir_cmd); (void)_rc; }

    char csv_path[PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/voxels.csv", out_dir);
    FILE *csv = fopen(csv_path, "w");
    if (!csv) {
        fprintf(stderr, "pc_phymoji_gen: could not open %s for writing\n", csv_path);
        return 1;
    }
    fprintf(csv, "x,y,z,r,g,b\n");

    /* REAL, NEW 2026-08-04: real per-asset sidecar recording the
     * SOURCE emoji this asset was generated from - the 3D voxel model
     * itself has no memory of what emoji produced it, but a real 2D
     * entity-overlay renderer (bv_compose_frame.c, this same session)
     * needs the ACTUAL emoji character to draw, not the voxel data. A
     * plain one-line UTF-8 text file is the simplest real, honest
     * store - no new binary format, human-readable/editable. */
    char emoji_sidecar_path[PATH_BUF];
    snprintf(emoji_sidecar_path, sizeof(emoji_sidecar_path), "%s/emoji.txt", out_dir);
    FILE *ef = fopen(emoji_sidecar_path, "w");
    if (ef) { fputs(emoji_utf8, ef); fclose(ef); }

    int voxel_count = 0;
    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            RGBA_Pixel p = tile[py * TILE_N + px];
            if (p.a < 128) continue; /* transparent source pixel - no column at all */

            /* REAL FIX 2026-08-04, direct user report ("phymoji is only
             * showing 2 parallel sides, empty in the middle"): the
             * FIRST version here stretched a crop's own pixels across
             * the FULL 0..7 canonical range (7.0*(px-min_x)/(crop_w-1))
             * - correct in general for reconciling DIFFERENT SOURCE
             * TILE RESOLUTIONS onto the same canonical space (PyMoji
             * §5.4's own real purpose), but WRONG here: this tile is
             * always a fixed 8x8 (TILE_N), and cropping only trims
             * away EMPTY border pixels - stretching a narrow 2-pixel-
             * wide crop across a full 8-unit span puts a real, empty
             * 7-unit GAP between what should be two ADJACENT columns,
             * exactly the reported hollow/2-parallel-sides bug. Real
             * fix: since crop_w/crop_h can never exceed TILE_N (8) to
             * begin with, no stretching is needed at all - use the
             * pixel's own RAW offset from the crop's own origin
             * directly (adjacent source pixels stay adjacent, 1 real
             * unit apart), matching the standard's own basic §4.2
             * mapping ("Source pixel (x,y) -> Voxel column at (x,y,
             * z=0..7)", no stretch formula there at all - §5.4's
             * stretch is for the OPTIONAL cross-resolution transform
             * case, not this project's own fixed-8x8-tile pipeline). */
            int norm_x = px - min_x;
            int norm_y_raw = py - min_y;
            int norm_y = (crop_h - 1) - norm_y_raw; /* bottom-left flip, PyMoji §5.4 */

            for (int z = 0; z < depth; z++) {
                /* REMOVED 2026-08-04, direct user correction ("why are
                 * the phymojis all dark on the backside? shouldn't
                 * they just mirror the front face, which is bright and
                 * complete"): the ORIGINAL PyMoji §4 Rules A/B/C real
                 * depth-attenuation formula (attenuation = 1 - z/depth,
                 * further darkened 0.7x on the very last layer) faded
                 * every column down to near-black by the back of the
                 * model - correct per that spec's own real intent
                 * (a genuine "depth cue" look), but not what's wanted
                 * here: the source emoji art only has ONE real
                 * viewpoint (the front), and the back face should read
                 * as the SAME bright, complete art, not a dim shadow of
                 * it. Every z-layer now gets the real, full source
                 * pixel color, unattenuated - a real mirror of the
                 * front, all the way through. */
                unsigned char r = p.r;
                unsigned char g = p.g;
                unsigned char b = p.b;

                int norm_z = (int)lround((depth > 1) ? (7.0 * z / (depth - 1)) : 0.0);
                fprintf(csv, "%d,%d,%d,%d,%d,%d\n", norm_x, norm_y, norm_z, r, g, b);
                voxel_count++;
            }
        }
    }
    fclose(csv);

    printf("pc_phymoji_gen: %s -> %s (%d voxels, crop %dx%d, depth %d)\n",
           emoji_utf8, csv_path, voxel_count, crop_w, crop_h, depth);
    return 0;
}
