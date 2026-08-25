/* emoji_xtract.c -- standalone TPM Op.
 * Reads a PNG (as produced by emoji_gen_atlas.+x), crops one emoji_size
 * cell out of it starting at emoji_index, area-averages it down to NxN,
 * and writes a plain-text RGBA CSV. No FreeType dependency -- pure
 * stb_image, matching the proven reference at
 * x0.parent-level-dev-env-02.01/#.emoji-studio-501.02.05t/
 * &.emoji-studio-solo.02.01/emoji-xtract.c (ported near-verbatim, per
 * wraimoji-j12-root-cause-and-fix-plan.txt Part 3 Step 1 -- this half
 * of the pipeline was never the buggy part, so it's not being changed).
 *
 * CSV format (must match wraith_rgb_daemon.c's reader exactly):
 *   # resolution=N
 *   # scale=1.0
 *   # transform=0,0,0
 *   r,g,b,a
 *   <r>,<g>,<b>,<a>   (repeated N*N times, row-major)
 *
 * Usage: emoji_xtract.+x <atlas_png_path> <emoji_index> <resolution> <output_csv_path>
 */
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

static void downscale_to_NxN(unsigned char *src_pixels, int src_width, int src_height, int channels,
                              int N, RGBA_Pixel *dst_pixels) {
    float x_ratio = (float)src_width / (float)N;
    float y_ratio = (float)src_height / (float)N;

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            int start_x = (int)(x * x_ratio);
            int start_y = (int)(y * y_ratio);
            int end_x = (int)((x + 1) * x_ratio);
            int end_y = (int)((y + 1) * y_ratio);

            if (end_x > src_width) end_x = src_width;
            if (end_y > src_height) end_y = src_height;

            long sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
            int count = 0;

            for (int sy = start_y; sy < end_y; sy++) {
                for (int sx = start_x; sx < end_x; sx++) {
                    int src_idx = (sy * src_width + sx) * channels;
                    sum_r += src_pixels[src_idx];
                    sum_g += src_pixels[src_idx + 1];
                    sum_b += src_pixels[src_idx + 2];
                    if (channels == 4) sum_a += src_pixels[src_idx + 3];
                    else sum_a += 255;
                    count++;
                }
            }

            if (count > 0) {
                dst_pixels[y * N + x].r = (unsigned char)(sum_r / count);
                dst_pixels[y * N + x].g = (unsigned char)(sum_g / count);
                dst_pixels[y * N + x].b = (unsigned char)(sum_b / count);
                dst_pixels[y * N + x].a = (unsigned char)(sum_a / count);
            }
        }
    }
}

static int write_csv(const char *filename, int N, RGBA_Pixel *pixels) {
    FILE *file = fopen(filename, "w");
    if (!file) return 0;

    fprintf(file, "# resolution=%d\n", N);
    fprintf(file, "# scale=1.0\n");
    fprintf(file, "# transform=0,0,0\n");
    fprintf(file, "r,g,b,a\n");

    for (int i = 0; i < N * N; i++) {
        fprintf(file, "%d,%d,%d,%d\n", pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].a);
    }

    fclose(file);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <atlas_path> <emoji_index> <resolution> <output_path>\n", argv[0]);
        return 1;
    }

    const char *atlas_path = argv[1];
    int emoji_index = atoi(argv[2]);
    int N = atoi(argv[3]);
    const char *output_path = argv[4];
    int emoji_size = 64; /* must match emoji_gen_atlas.c's EMOJI_SIZE */

    int atlas_w, atlas_h, channels;
    unsigned char *atlas_data = stbi_load(atlas_path, &atlas_w, &atlas_h, &channels, 0);
    if (!atlas_data) {
        fprintf(stderr, "Error: Could not load atlas %s\n", atlas_path);
        return 1;
    }

    int atlas_x = emoji_index * emoji_size;
    int atlas_y = 0;

    if (atlas_x + emoji_size > atlas_w) {
        fprintf(stderr, "Error: Index %d out of bounds (x=%d, w=%d)\n", emoji_index, atlas_x, atlas_w);
        stbi_image_free(atlas_data);
        return 1;
    }

    unsigned char *emoji_pixels = malloc(emoji_size * emoji_size * channels);
    for (int y = 0; y < emoji_size; y++) {
        memcpy(emoji_pixels + (y * emoji_size * channels),
               atlas_data + (((atlas_y + y) * atlas_w + atlas_x) * channels),
               emoji_size * channels);
    }

    RGBA_Pixel *downsampled = malloc(N * N * sizeof(RGBA_Pixel));
    downscale_to_NxN(emoji_pixels, emoji_size, emoji_size, channels, N, downsampled);

    if (!write_csv(output_path, N, downsampled)) {
        fprintf(stderr, "Error: Could not write CSV %s\n", output_path);
        free(emoji_pixels);
        free(downsampled);
        stbi_image_free(atlas_data);
        return 1;
    }

    free(emoji_pixels);
    free(downsampled);
    stbi_image_free(atlas_data);
    return 0;
}
