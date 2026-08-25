#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Convolution function
void convolve(float *input, float *output, int width, int height, int channels, float kernel[3][3]) {
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            for (int c = 0; c < channels; c++) {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int idx = ((y + ky) * width + (x + kx)) * channels + c;
                        sum += input[idx] * kernel[ky + 1][kx + 1];
                    }
                }
                output[(y * width + x) * channels + c] = sum;
            }
        }
    }
}

// Max pooling (2x2)
void max_pool(float *input, float *output, int width, int height, int channels) {
    int out_width = width / 2;
    int out_height = height / 2;
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                float max_val = -INFINITY;
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int idx = ((y * 2 + dy) * width + (x * 2 + dx)) * channels + c;
                        if (input[idx] > max_val) max_val = input[idx];
                    }
                }
                output[(y * out_width + x) * channels + c] = max_val;
            }
        }
    }
}

// Upsampling (nearest neighbor, 2x)
void upsample(float *input, float *output, int width, int height, int channels) {
    int out_width = width * 2;
    int out_height = height * 2;
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                int in_y = y / 2;
                int in_x = x / 2;
                output[(y * out_width + x) * channels + c] = input[(in_y * width + in_x) * channels + c];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_image> <output_file>\n", argv[0]);
        return 1;
    }

    // Load image
    int width, height, channels;
    unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Error: Failed to load %s\n", argv[1]);
        return 1;
    }

    // Convert to float
    int size = width * height * channels;
    float *input = (float *)malloc(size * sizeof(float));
    if (!input) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        stbi_image_free(img);
        return 1;
    }
    for (int i = 0; i < size; i++) input[i] = (float)img[i] / 255.0f;

    // Simplified U-Net: Downsample -> Bottleneck -> Upsample
    float kernel[3][3] = {{0.1f, 0.2f, 0.1f}, {0.2f, 0.4f, 0.2f}, {0.1f, 0.2f, 0.1f}}; // Example kernel
    int feature_channels = 8; // Simulate multiple feature maps

    // Downsampling
    float *conv1 = (float *)calloc(size, sizeof(float));
    float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
    if (!conv1 || !pool1) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1);
        return 1;
    }
    convolve(input, conv1, width, height, channels, kernel);
    max_pool(conv1, pool1, width, height, channels);

    // Bottleneck (another convolution)
    float *bottleneck = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
    if (!bottleneck) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(bottleneck);
        return 1;
    }
    convolve(pool1, bottleneck, width / 2, height / 2, channels, kernel);

    // Upsampling
    float *upsample1 = (float *)calloc(size, sizeof(float));
    float *output = (float *)calloc(size, sizeof(float));
    if (!upsample1 || !output) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(bottleneck); free(upsample1); free(output);
        return 1;
    }
    upsample(bottleneck, upsample1, width / 2, height / 2, channels);
    convolve(upsample1, output, width, height, channels, kernel);

    // Simulate noise prediction (scale output to [-1, 1])
    for (int i = 0; i < size; i++) {
        output[i] = output[i] * 2.0f - 1.0f;
    }

    // Save output
    FILE *fp = fopen(argv[2], "wb");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s for writing\n", argv[2]);
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(bottleneck); free(upsample1); free(output);
        return 1;
    }
    fwrite(output, sizeof(float), size, fp);
    fclose(fp);

    // Cleanup
    free(input);
    stbi_image_free(img);
    free(conv1);
    free(pool1);
    free(bottleneck);
    free(upsample1);
    free(output);
    return 0;
}
