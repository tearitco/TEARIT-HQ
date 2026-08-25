#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_UPSCALE_FACTOR 2

// Bilinear interpolation for a single pixel
void bilinear_interpolate(unsigned char *input, unsigned char *output, int in_width, int in_height, int out_width, int out_height, int channels) {
    float x_ratio = (float)in_width / out_width;
    float y_ratio = (float)in_height / out_height;

    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            float fx = x * x_ratio;
            float fy = y * y_ratio;
            int x1 = (int)fx;
            int y1 = (int)fy;
            int x2 = (x1 + 1 < in_width) ? x1 + 1 : x1;
            int y2 = (y1 + 1 < in_height) ? y1 + 1 : y1;
            float dx = fx - x1;
            float dy = fy - y1;

            for (int c = 0; c < channels; c++) {
                float p1 = input[(y1 * in_width + x1) * channels + c];
                float p2 = input[(y1 * in_width + x2) * channels + c];
                float p3 = input[(y2 * in_width + x1) * channels + c];
                float p4 = input[(y2 * in_width + x2) * channels + c];

                float interpolated = p1 * (1 - dx) * (1 - dy) +
                                    p2 * dx * (1 - dy) +
                                    p3 * (1 - dx) * dy +
                                    p4 * dx * dy;

                output[(y * out_width + x) * channels + c] = (unsigned char)interpolated;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int upscale_factor = DEFAULT_UPSCALE_FACTOR;
    const char *input_file = "diffusion_output.jpg";
    const char *output_file = "upscaled_output.jpg";

    // Parse command-line arguments
    if (argc > 1) {
        upscale_factor = atoi(argv[1]);
        if (upscale_factor <= 1) {
            fprintf(stderr, "Error: Upscale factor must be greater than 1\n");
            return 1;
        }
    }

    // Open log file
    FILE *log_fp = fopen("upscale_log.txt", "a");
    if (!log_fp) {
        fprintf(stderr, "Error: Failed to open upscale_log.txt\n");
        return 1;
    }

    // Load the input image
    int width, height, channels;
    unsigned char *img = stbi_load(input_file, &width, &height, &channels, 3);
    if (!img) {
        fprintf(log_fp, "Error: Failed to load %s\n", input_file);
        fclose(log_fp);
        return 1;
    }
    fprintf(log_fp, "Loaded %s (%dx%d, %d channels)\n", input_file, width, height, channels);

    // Calculate output dimensions
    int out_width = width * upscale_factor;
    int out_height = height * upscale_factor;

    // Allocate memory for the upscaled image
    unsigned char *upscaled_img = (unsigned char *)malloc(out_width * out_height * channels);
    if (!upscaled_img) {
        fprintf(log_fp, "Error: Memory allocation failed for upscaled image\n");
        stbi_image_free(img);
        fclose(log_fp);
        return 1;
    }

    // Perform bilinear interpolation
    bilinear_interpolate(img, upscaled_img, width, height, out_width, out_height, channels);
    fprintf(log_fp, "Upscaled image to %dx%d (factor: %d)\n", out_width, out_height, upscale_factor);

    // Save the upscaled image
    if (!stbi_write_jpg(output_file, out_width, out_height, channels, upscaled_img, 90)) {
        fprintf(log_fp, "Error: Failed to write %s\n", output_file);
        free(upscaled_img);
        stbi_image_free(img);
        fclose(log_fp);
        return 1;
    }
    printf("Upscaled image saved as %s (%dx%d, %d channels)\n", output_file, out_width, out_height, channels);
    fprintf(log_fp, "Saved upscaled image as %s (%dx%d, %d channels)\n", output_file, out_width, out_height, channels);

    // Cleanup
    free(upscaled_img);
    stbi_image_free(img);
    fclose(log_fp);
    return 0;
}
