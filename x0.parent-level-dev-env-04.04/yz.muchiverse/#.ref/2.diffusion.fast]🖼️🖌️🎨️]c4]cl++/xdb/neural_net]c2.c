#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Convolution function with channel-specific kernels
void convolve(float *input, float *output, int width, int height, int channels, float kernels[3][3][3]) {
    for (int c = 0; c < channels; c++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int idx = ((y + ky) * width + (x + kx)) * channels + c;
                        sum += input[idx] * kernels[c][ky + 1][kx + 1];
                    }
                }
                output[(y * width + x) * channels + c] = fmaxf(0.0f, sum); // ReLU activation
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

// Combine skip connection
void combine_skip(float *main, float *skip, float *output, int width, int height, int channels) {
    for (int i = 0; i < width * height * channels; i++) {
        output[i] = (main[i] + skip[i]) * 0.5f; // Average main and skip
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

    // Define kernels (one per channel: edge detection for R, sharpening for G, smoothing for B)
    float kernels[3][3][3] = {
        {{-1.0f, -1.0f, -1.0f}, {-1.0f, 8.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}}, // Edge detection (R)
        {{0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},     // Sharpening (G)
        {{0.1f, 0.2f, 0.1f}, {0.2f, 0.4f, 0.2f}, {0.1f, 0.2f, 0.1f}}          // Smoothing (B)
    };

    // U-Net: Two downsample/upsample stages with skip connections
    // Stage 1: Downsample
    float *conv1 = (float *)calloc(size, sizeof(float));
    float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
    if (!conv1 || !pool1) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1);
        return 1;
    }
    convolve(input, conv1, width, height, channels, kernels);
    max_pool(conv1, pool1, width, height, channels);

    // Save conv1 for skip connection
    FILE *skip1_fp = fopen("skip1.dat", "wb");
    if (!skip1_fp) {
        fprintf(stderr, "Error: Failed to write skip1.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1);
        return 1;
    }
    fwrite(conv1, sizeof(float), size, skip1_fp);
    fclose(skip1_fp);

    // Stage 2: Downsample
    int width2 = width / 2;
    int height2 = height / 2;
    float *conv2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    float *pool2 = (float *)calloc((width2 / 2) * (height2 / 2) * channels, sizeof(float));
    if (!conv2 || !pool2) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2);
        return 1;
    }
    convolve(pool1, conv2, width2, height2, channels, kernels);
    max_pool(conv2, pool2, width2, height2, channels);

    // Bottleneck
    float *bottleneck = (float *)calloc((width2 / 2) * (height2 / 2) * channels, sizeof(float));
    if (!bottleneck) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck);
        return 1;
    }
    convolve(pool2, bottleneck, width2 / 2, height2 / 2, channels, kernels);

    // Stage 2: Upsample
    float *upsample2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    float *conv3 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    if (!upsample2 || !conv3) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck); free(upsample2); free(conv3);
        return 1;
    }
    upsample(bottleneck, upsample2, width2 / 2, height2 / 2, channels);
    convolve(upsample2, conv3, width2, height2, channels, kernels);

    // Stage 1: Upsample with skip connection
    float *upsample1 = (float *)calloc(size, sizeof(float));
    float *conv4 = (float *)calloc(size, sizeof(float));
    float *output = (float *)calloc(size, sizeof(float));
    if (!upsample1 || !conv4 || !output) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck); free(upsample2); free(conv3); free(upsample1); free(conv4); free(output);
        return 1;
    }
    upsample(conv3, upsample1, width2, height2, channels);
    // Load skip connection
    float *skip1 = (float *)calloc(size, sizeof(float));
    if (!skip1) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck); free(upsample2); free(conv3); free(upsample1); free(conv4); free(output); free(skip1);
        return 1;
    }
    FILE *skip1_fp_read = fopen("skip1.dat", "rb");
    if (!skip1_fp_read) {
        fprintf(stderr, "Error: Failed to read skip1.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck); free(upsample2); free(conv3); free(upsample1); free(conv4); free(output); free(skip1);
        return 1;
    }
    fread(skip1, sizeof(float), size, skip1_fp_read);
    fclose(skip1_fp_read);
    remove("skip1.dat");
    combine_skip(upsample1, skip1, conv4, width, height, channels);
    convolve(conv4, output, width, height, channels, kernels);

    // Normalize output to [-1, 1] for noise prediction
    float max_val = 0.0f;
    for (int i = 0; i < size; i++) {
        if (fabsf(output[i]) > max_val) max_val = fabsf(output[i]);
    }
    if (max_val > 0.0f) {
        for (int i = 0; i < size; i++) {
            output[i] = (output[i] / max_val) * 1.0f; // Scale to [-1, 1]
        }
    }

    // Save output
    FILE *fp = fopen(argv[2], "wb");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s for writing\n", argv[2]);
        free(input); stbi_image_free(img); free(conv1); free(pool1); free(conv2); free(pool2); free(bottleneck); free(upsample2); free(conv3); free(upsample1); free(conv4); free(output); free(skip1);
        return 1;
    }
    fwrite(output, sizeof(float), size, fp);
    fclose(fp);

    // Cleanup
    free(input);
    stbi_image_free(img);
    free(conv1);
    free(pool1);
    free(conv2);
    free(pool2);
    free(bottleneck);
    free(upsample2);
    free(conv3);
    free(upsample1);
    free(conv4);
    free(output);
    free(skip1);
    return 0;
}
