/* gen_voxels8 - DEBUG/ASSET TOOL, not wired into pal/main_loop.pal or
 * default_op.txt (same "run manually when needed" convention as
 * ops/dump_rgb_png.c - see that file's own header comment).
 *
 * Backfills pieces/registry/emoji_assets/<asset>/voxels_8.csv for every
 * asset that has a voxels_16.csv but no voxels_8.csv yet - real fix for
 * "3D players/items/furniture render as flat fallback-color boxes,
 * never a real emoji texture": ops/compose_rgb_frame.c's 3D ray marcher
 * (raymarch_walls_3d(), see that function's own header comment) only
 * ever reads voxels_8.csv (via sample_voxel8_pixel()/get_voxel8_cached()),
 * but only a handful of block-shaped assets (t_wall, t_tree, f_crate,
 * f_sink, f_table, f_counter, t_window, t_water_dp) were ever given
 * one - every entity/item asset (hero, xlector, every item_id, every
 * monster_type) only has the voxels_16.csv the 2D top-down blit uses.
 * Rather than re-run whatever external emoji-rasterization pipeline
 * originally produced voxels_16.csv (not present in this project - see
 * extrude-emoji.md's own investigation), this downsamples the EXISTING,
 * already-correct 16x16 pixel data into a real 8x8 texture via a
 * straightforward 2x2 box filter (alpha-weighted color average, plain
 * alpha average) - same real pixel data, not a placeholder/solid color.
 *
 * Usage: ops/+x/gen_voxels8.+x  (no args - scans every asset dir once) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Reads a voxels_16.csv (header comments + "r,g,b,a" + 256 data rows,
 * see any existing asset's own file for the exact shape) into a flat
 * 16x16x4 buffer. Returns 1 on success. */
static int load_voxels16(const char *path, unsigned char px[16][16][4]) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < 256) {
        int r, g, b, a;
        if (line[0] == '#') continue;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue; /* skips the "r,g,b,a" header row */
        px[count / 16][count % 16][0] = (unsigned char)r;
        px[count / 16][count % 16][1] = (unsigned char)g;
        px[count / 16][count % 16][2] = (unsigned char)b;
        px[count / 16][count % 16][3] = (unsigned char)a;
        count++;
    }
    fclose(f);
    return count == 256;
}

/* 2x2 box filter: color is alpha-weighted (fully-transparent source
 * texels don't muddy a partially-opaque block's color), alpha is a
 * plain average - standard downsample compositing, nothing project-
 * specific. */
static void downsample_block(unsigned char px[16][16][4], int by, int bx, unsigned char out[4]) {
    long wsum = 0, rsum = 0, gsum = 0, bsum = 0, asum = 0;
    for (int dy = 0; dy < 2; dy++) {
        for (int dx = 0; dx < 2; dx++) {
            unsigned char *p = px[by * 2 + dy][bx * 2 + dx];
            int a = p[3];
            rsum += (long)p[0] * a; gsum += (long)p[1] * a; bsum += (long)p[2] * a;
            wsum += a;
            asum += a;
        }
    }
    if (wsum > 0) {
        out[0] = (unsigned char)(rsum / wsum);
        out[1] = (unsigned char)(gsum / wsum);
        out[2] = (unsigned char)(bsum / wsum);
    } else {
        out[0] = out[1] = out[2] = 0;
    }
    out[3] = (unsigned char)(asum / 4);
}

static int gen_one(const char *dir_path) {
    char in_path[PATH_BUF + 320 + 32], out_path[PATH_BUF + 320 + 32];
    snprintf(in_path, sizeof(in_path), "%s/voxels_16.csv", dir_path);
    snprintf(out_path, sizeof(out_path), "%s/voxels_8.csv", dir_path);
    if (!file_exists(in_path) || file_exists(out_path)) return 0;

    static unsigned char px16[16][16][4];
    if (!load_voxels16(in_path, px16)) {
        fprintf(stderr, "gen_voxels8: skipping %s (couldn't parse voxels_16.csv)\n", in_path);
        return 0;
    }

    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "gen_voxels8: couldn't write %s\n", out_path);
        return 0;
    }
    fprintf(out, "# resolution=8\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n");
    for (int by = 0; by < 8; by++) {
        for (int bx = 0; bx < 8; bx++) {
            unsigned char rgba[4];
            downsample_block(px16, by, bx, rgba);
            fprintf(out, "%d,%d,%d,%d\n", rgba[0], rgba[1], rgba[2], rgba[3]);
        }
    }
    fclose(out);
    return 1;
}

int main(void) {
    resolve_root();
    char assets_dir[PATH_BUF];
    snprintf(assets_dir, sizeof(assets_dir), "%s/pieces/registry/emoji_assets", project_root);
    DIR *d = opendir(assets_dir);
    if (!d) {
        fprintf(stderr, "gen_voxels8: couldn't open %s\n", assets_dir);
        return 1;
    }
    int generated = 0, scanned = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char sub_path[PATH_BUF + 320];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", assets_dir, entry->d_name);
        scanned++;
        if (gen_one(sub_path)) {
            printf("gen_voxels8: wrote %s/voxels_8.csv\n", entry->d_name);
            generated++;
        }
    }
    closedir(d);
    printf("gen_voxels8: done - %d/%d asset dirs got a new voxels_8.csv\n", generated, scanned);
    return 0;
}
