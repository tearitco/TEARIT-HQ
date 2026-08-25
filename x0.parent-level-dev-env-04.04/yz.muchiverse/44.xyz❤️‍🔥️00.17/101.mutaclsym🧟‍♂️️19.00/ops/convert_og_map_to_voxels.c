/* convert_og_map_to_voxels - convert +18.0G's hand-crafted 40x16 map into
 * the voxel chunk format for mutaclysm 19.00. Tiles across 3 chunks
 * (chunk_0_0, chunk_1_0, chunk_2_0) with flat surface and 10-voxel-high walls.
 *
 * Usage: convert_og_map_to_voxels.+x <source_project_root> <dest_project_root>
 *   <source_project_root>: path to +18.0G (where map.txt/furniture.txt live)
 *   <dest_project_root>: path to 19.00 (where chunks/ will be written)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define CHUNK_DIM 16
#define Z_COUNT 32
#define FLAT_SURFACE_Z 16
#define WALL_HEIGHT_ABOVE_SURFACE 9

typedef struct {
    int x, y;
} Coord;

static void resolve_root(const char *arg, char *out) {
    if (arg && arg[0]) {
        snprintf(out, MAX_PATH, "%s", arg);
    } else {
        snprintf(out, MAX_PATH, ".");
    }
}

static int mkdir_p(const char *path) {
#ifdef _WIN32
    return mkdir(path);
#else
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "mkdir -p '%s'", path);
    return system(tmp);
#endif
}

/* Read a single glyph from a file at (x,y). Returns default_glyph if out of
 * bounds or file doesn't exist. */
static char glyph_at(const char *path, int x, int y, char default_glyph) {
    if (x < 0 || y < 0) return default_glyph;
    FILE *f = fopen(path, "r");
    if (!f) return default_glyph;
    char line[256];
    for (int row = 0; row <= y; row++) {
        if (!fgets(line, sizeof(line), f)) break;
        if (row == y) {
            char result = (x < (int)strlen(line)) ? line[x] : default_glyph;
            fclose(f);
            return result;
        }
    }
    fclose(f);
    return default_glyph;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_project_root> <dest_project_root>\n", argv[0]);
        return 1;
    }

    char src_root[MAX_PATH], dst_root[MAX_PATH];
    resolve_root(argv[1], src_root);
    resolve_root(argv[2], dst_root);

    /* Paths to source map files (from +18.0G) */
    char src_map_path[PATH_BUF], src_furniture_path[PATH_BUF];
    snprintf(src_map_path, sizeof(src_map_path), "%s/pieces/world_01/map_start/map.txt", src_root);
    snprintf(src_furniture_path, sizeof(src_furniture_path), "%s/pieces/world_01/map_start/furniture.txt", src_root);

    /* Read source map dimensions (should be 40x16) */
    FILE *f = fopen(src_map_path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open source map: %s\n", src_map_path);
        return 1;
    }
    int src_w = 0, src_h = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line) - 1;
        if (len > src_w) src_w = len;
        src_h++;
    }
    fclose(f);

    fprintf(stderr, "Source map: %d x %d\n", src_w, src_h);
    if (src_w != 40 || src_h != 16) {
        fprintf(stderr, "WARNING: Expected 40x16, got %dx%d\n", src_w, src_h);
    }

    /* For each chunk (chunk_0_0, chunk_1_0, chunk_2_0), generate voxel data */
    for (int chunk_x = 0; chunk_x < 3; chunk_x++) {
        int chunk_y = 0;
        int chunk_base_x = chunk_x * CHUNK_DIM;
        int chunk_base_y = chunk_y * CHUNK_DIM;

        /* Compute surface heights for this chunk */
        int surface[CHUNK_DIM][CHUNK_DIM];
        for (int row = 0; row < CHUNK_DIM; row++) {
            for (int col = 0; col < CHUNK_DIM; col++) {
                int global_x = chunk_base_x + col;
                int global_y = chunk_base_y + row;

                /* Read the map glyph at this position */
                char map_glyph = glyph_at(src_map_path, global_x, global_y, '#');

                /* Wall (#) gets raised 10 voxels; floor (.) stays at base */
                if (map_glyph == '#') {
                    surface[row][col] = FLAT_SURFACE_Z + WALL_HEIGHT_ABOVE_SURFACE;
                } else {
                    surface[row][col] = FLAT_SURFACE_Z;
                }
            }
        }

        /* Create chunk directory */
        char chunk_dir[PATH_BUF];
        snprintf(chunk_dir, sizeof(chunk_dir), "%s/pieces/system/chunks/chunk_%d_%d", dst_root, chunk_x, chunk_y);
        mkdir_p(chunk_dir);

        /* Generate Z-level files for this chunk */
        for (int z = 0; z < Z_COUNT; z++) {
            char z_path[PATH_BUF];
            snprintf(z_path, sizeof(z_path), "%s/chunk_%d_%d_z%d.txt", chunk_dir, chunk_x, chunk_y, z);
            FILE *zf = fopen(z_path, "w");
            if (!zf) {
                fprintf(stderr, "ERROR: Cannot create %s\n", z_path);
                continue;
            }

            for (int row = 0; row < CHUNK_DIM; row++) {
                for (int col = 0; col < CHUNK_DIM; col++) {
                    int sh = surface[row][col];
                    char glyph;
                    if (z > sh) glyph = '_';         /* air */
                    else if (z == sh) glyph = ',';   /* grass surface */
                    else if (z >= sh - 3) glyph = '.'; /* dirt subsurface */
                    else glyph = 's';                /* stone below */
                    fputc(glyph, zf);
                }
                fputc('\n', zf);
            }
            fclose(zf);
        }

        fprintf(stderr, "Generated chunk_%d_%d\n", chunk_x, chunk_y);
    }

    /* Write board_manifest.txt pointing to chunk_0_0 as default */
    char manifest_path[PATH_BUF];
    snprintf(manifest_path, sizeof(manifest_path), "%s/pieces/system/board_manifest.txt", dst_root);
    FILE *mf = fopen(manifest_path, "w");
    if (mf) {
        fprintf(mf, "z_base=pieces/system/chunks/chunk_0_0/chunk_0_0_z\n");
        fprintf(mf, "z_count=%d\n", Z_COUNT);
        fclose(mf);
        fprintf(stderr, "Wrote board_manifest.txt\n");
    }

    fprintf(stderr, "Conversion complete!\n");
    return 0;
}
