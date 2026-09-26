/* tp_asset_to_sprite - convert an ARBITRARY user image (any size, PNG or
 * JPG) into the same NxN RGBA sprite.csv format tp_desktop_window.c
 * already loads.
 *
 * Usage: tp_asset_to_sprite.+x <image_path> <resolution> <output_csv>
 *
 * REAL, NEW 2026-08-04, direct instruction ("allow user editing of
 * asset... path of asset (jpg/png)... it will use that instead of
 * default emoji"). wsr-pal's own emoji_xtract.+x looks similar but is
 * NOT a general image converter - it assumes its input is an "atlas" of
 * fixed 64px cells side by side and crops ONE cell at a given index
 * (`atlas_x = emoji_index * 64`, confirmed via direct read of
 * 014.wsr-pal💸️📌️+2/ops/emoji_xtract.c) - feeding it an arbitrary
 * user-sized image (a 537x466 photo, say) would silently crop a
 * meaningless 64x64 corner instead of scaling the whole image. This op
 * reuses emoji_xtract.c's own real `downscale_to_NxN` box-filter
 * algorithm and `write_csv` format verbatim, but downscales the WHOLE
 * loaded image, not a cropped atlas cell - the correct operation for a
 * real, arbitrary user-supplied asset.
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

/* Verbatim port of emoji_xtract.c's own downscale_to_NxN - same real,
 * proven box-filter algorithm, not reinvented. */
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
                    int idx = (sy * src_width + sx) * channels;
                    sum_r += src_pixels[idx];
                    sum_g += src_pixels[idx + 1];
                    sum_b += src_pixels[idx + 2];
                    if (channels == 4) sum_a += src_pixels[idx + 3];
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
    for (int i = 0; i < N * N; i++)
        fprintf(file, "%d,%d,%d,%d\n", pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].a);
    fclose(file);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <image_path> <resolution> <output_csv>\n", argv[0]);
        return 1;
    }
    const char *image_path = argv[1];
    int N = atoi(argv[2]);
    const char *output_path = argv[3];
    if (N <= 0) N = 64;

    int w, h, channels;
    unsigned char *pixels = stbi_load(image_path, &w, &h, &channels, 0);
    if (!pixels) {
        fprintf(stderr, "tp_asset_to_sprite: could not load image %s\n", image_path);
        return 1;
    }

    RGBA_Pixel *downsampled = malloc((size_t)N * N * sizeof(RGBA_Pixel));
    if (!downsampled) { stbi_image_free(pixels); return 1; }
    downscale_to_NxN(pixels, w, h, channels, N, downsampled);
    stbi_image_free(pixels);

    int ok = write_csv(output_path, N, downsampled);
    free(downsampled);
    if (!ok) {
        fprintf(stderr, "tp_asset_to_sprite: could not write %s\n", output_path);
        return 1;
    }
    printf("SPRITE %s (%dx%d -> %dx%d)\n", output_path, w, h, N, N);
    return 0;
}
