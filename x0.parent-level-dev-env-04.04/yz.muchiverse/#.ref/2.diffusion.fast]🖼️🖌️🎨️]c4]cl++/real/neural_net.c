#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define NUM_EPOCHS 10
#define LEARNING_RATE 0.001f
#define TIMESTEP 0.1f

// Convolution function
void convolve(float *input, float *output, int width, int height, int channels, float kernels[3][3][3], int use_identity) {
    for (int c = 0; c < channels; c++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                float sum = 0.0f;
                if (use_identity && c == 0) {
                    sum = input[(y * width + x) * channels + c];
                } else {
                    for (int ky = -1; ky <= 1; ky++) {
                        for (int kx = -1; kx <= 1; kx++) {
                            int idx = ((y + ky) * width + (x + kx)) * channels + c;
                            sum += input[idx] * kernels[c][ky + 1][kx + 1];
                        }
                    }
                }
                output[(y * width + x) * channels + c] = fmaxf(0.0f, fminf(sum, 1.0f));
            }
        }
    }
}

// Backpropagation for convolution
void convolve_backward(float *input, float *grad_output, float *grad_kernels, int width, int height, int channels) {
    for (int c = 0; c < channels; c++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int kidx = c * 9 + (ky + 1) * 3 + (kx + 1);
                        int idx = ((y + ky) * width + (x + kx)) * channels + c;
                        grad_kernels[kidx] += grad_output[(y * width + x) * channels + c] * input[idx];
                    }
                }
            }
        }
    }
}

// Simplified attention mechanism
void attention(float *input, float *output, int width, int height, int channels) {
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            for (int c = 0; c < channels; c++) {
                float sum = 0.0f, weight_sum = 0.0f;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny < 0 || ny >= height || nx < 0 || nx >= width) continue;
                        int idx = (ny * width + nx) * channels + c;
                        float val = input[idx];
                        float variance = 0.0f;
                        int count = 0;
                        for (int vy = -1; vy <= 1; vy++) {
                            for (int vx = -1; vx <= 1; vx++) {
                                int vny = ny + vy;
                                int vnx = nx + vx;
                                if (vny < 0 || vny >= height || vnx < 0 || vnx >= width) continue;
                                int vidx = (vny * width + vnx) * channels + c;
                                variance += (input[vidx] - val) * (input[vidx] - val);
                                count++;
                            }
                        }
                        variance = count > 0 ? variance / count : 0.0f;
                        float weight = 1.0f + variance;
                        sum += val * weight;
                        weight_sum += weight;
                    }
                }
                output[(y * width + x) * channels + c] = weight_sum > 0.0f ? sum / weight_sum : input[(y * width + x) * channels + c];
            }
        }
    }
}

// Group normalization
void group_norm(float *input, int width, int height, int channels) {
    for (int c = 0; c < channels; c++) {
        float mean = 0.0f, variance = 0.0f;
        int count = width * height;
        for (int i = 0; i < count; i++) {
            mean += input[i * channels + c];
        }
        mean /= count;
        for (int i = 0; i < count; i++) {
            variance += (input[i * channels + c] - mean) * (input[i * channels + c] - mean);
        }
        variance = sqrtf(variance / count + 1e-6f);
        for (int i = 0; i < count; i++) {
            input[i * channels + c] = (input[i * channels + c] - mean) / variance;
        }
    }
}

// Max pooling
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

// Upsampling
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
void combine_skip(float *main, float *skip, float *output, int width, int height, int channels, float skip_weight) {
    for (int i = 0; i < width * height * channels; i++) {
        output[i] = main[i] * (1.0f - skip_weight) + skip[i] * skip_weight;
    }
}

// Residual connection
void add_residual(float *output, float *input, int size, float residual_weight) {
    for (int i = 0; i < size; i++) {
        output[i] = output[i] * (1.0f - residual_weight) + input[i] * residual_weight;
    }
}

// Add noise for training
void add_noise(float *input, float *output, float *noise, int size, float t) {
    float sqrt_t = sqrtf(t);
    for (int i = 0; i < size; i++) {
        noise[i] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
        output[i] = input[i] * (1.0f - t) + noise[i] * sqrt_t;
    }
}

// Compute MSE loss
float compute_loss(float *pred, float *target, int size) {
    float loss = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    return loss / size;
}

// Compute stats for noise predictions
void log_noise_stats(float *data, int size, FILE *fp, const char *label) {
    float mean = 0.0f, variance = 0.0f;
    for (int i = 0; i < size; i++) {
        mean += data[i];
    }
    mean /= size;
    for (int i = 0; i < size; i++) {
        variance += (data[i] - mean) * (data[i] - mean);
    }
    variance /= size;
    fprintf(fp, "%s: Mean = %f, Variance = %f\n", label, mean, variance);
    fflush(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_image> <output_file> <mode:train/infer> <kernel_file>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    int train_mode = strcmp(argv[3], "train") == 0;

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

    // Define kernels
    float kernels[3][3][3] = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {{0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
        {{0.1f, 0.2f, 0.1f}, {0.2f, 0.4f, 0.2f}, {0.1f, 0.2f, 0.1f}}
    };
    float sharpen_kernel[3][3][3] = {
        {{0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
        {{0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
        {{0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}}
    };

    if (train_mode) {
        // Training mode
        float *noisy_input = (float *)calloc(size, sizeof(float));
        float *noise = (float *)calloc(size, sizeof(float));
        float *pred_noise = (float *)calloc(size, sizeof(float));
        float *grad_output = (float *)calloc(size, sizeof(float));
        float grad_kernels[3 * 3 * 3] = {0};
        if (!noisy_input || !noise || !pred_noise || !grad_output) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(input); stbi_image_free(img); free(noisy_input); free(noise); free(pred_noise); free(grad_output);
            return 1;
        }

        FILE *loss_fp = fopen("loss_debug.txt", "a");
        if (!loss_fp) {
            fprintf(stderr, "Error: Failed to open loss_debug.txt\n");
            free(input); stbi_image_free(img); free(noisy_input); free(noise); free(pred_noise); free(grad_output);
            return 1;
        }

        fprintf(loss_fp, "Training %s\n", argv[1]);
        for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
            // Add noise
            add_noise(input, noisy_input, noise, size, TIMESTEP);

            // Forward pass
            float *conv1 = (float *)calloc(size, sizeof(float));
            float *attn1 = (float *)calloc(size, sizeof(float));
            float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
            if (!conv1 || !attn1 || !pool1) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                free(input); stbi_image_free(img); free(noisy_input); free(noise); free(pred_noise); free(grad_output);
                free(conv1); free(attn1); free(pool1); fclose(loss_fp);
                return 1;
            }
            convolve(noisy_input, conv1, width, height, channels, kernels, 1);
            group_norm(conv1, width, height, channels);
            attention(conv1, attn1, width, height, channels);
            max_pool(attn1, pool1, width, height, channels);

            // Backward pass
            float loss = compute_loss(conv1, noise, size);
            for (int i = 0; i < size; i++) {
                grad_output[i] = 2.0f * (conv1[i] - noise[i]) / size;
            }
            memset(grad_kernels, 0, 3 * 3 * 3 * sizeof(float));
            convolve_backward(noisy_input, grad_output, grad_kernels, width, height, channels);
            for (int c = 0; c < channels; c++) {
                for (int ky = 0; ky < 3; ky++) {
                    for (int kx = 0; kx < 3; kx++) {
                        kernels[c][ky][kx] -= LEARNING_RATE * grad_kernels[c * 9 + ky * 3 + kx];
                    }
                }
            }

            // Log loss
            fprintf(loss_fp, "Epoch %d: Loss = %f\n", epoch + 1, loss);
            fflush(loss_fp);

            free(conv1); free(attn1); free(pool1);
        }

        // Save trained kernels
        FILE *kernel_fp = fopen(argv[4], "wb");
        if (!kernel_fp) {
            fprintf(stderr, "Error: Failed to open %s\n", argv[4]);
            free(input); stbi_image_free(img); free(noisy_input); free(noise); free(pred_noise); free(grad_output); fclose(loss_fp);
            return 1;
        }
        fwrite(kernels, sizeof(float), 3 * 3 * 3, kernel_fp);
        fclose(kernel_fp);
        fclose(loss_fp);
        free(noisy_input); free(noise); free(pred_noise); free(grad_output);
        free(input); stbi_image_free(img);
        return 0;
    }

    // Inference mode
    // Load trained kernels
    FILE *kernel_fp = fopen(argv[4], "rb");
    if (kernel_fp) {
        size_t read = fread(kernels, sizeof(float), 3 * 3 * 3, kernel_fp);
        fclose(kernel_fp);
        if (read != 3 * 3 * 3) {
            fprintf(stderr, "Warning: Failed to read %s, using defaults\n", argv[4]);
        }
    }

    // U-Net inference
    float *conv1 = (float *)calloc(size, sizeof(float));
    float *attn1 = (float *)calloc(size, sizeof(float));
    float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
    if (!conv1 || !attn1 || !pool1) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1);
        return 1;
    }
    convolve(input, conv1, width, height, channels, kernels, 1);
    group_norm(conv1, width, height, channels);
    attention(conv1, attn1, width, height, channels);
    max_pool(attn1, pool1, width, height, channels);

    FILE *skip1_fp = fopen("skip1.dat", "wb");
    if (!skip1_fp) {
        fprintf(stderr, "Error: Failed to write skip1.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1);
        return 1;
    }
    fwrite(conv1, sizeof(float), size, skip1_fp);
    fclose(skip1_fp);

    int width2 = width / 2;
    int height2 = height / 2;
    float *conv2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    float *attn2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    float *pool2 = (float *)calloc((width2 / 2) * (height2 / 2) * channels, sizeof(float));
    if (!conv2 || !attn2 || !pool2) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2);
        return 1;
    }
    convolve(pool1, conv2, width2, height2, channels, kernels, 0);
    group_norm(conv2, width2, height2, channels);
    attention(conv2, attn2, width2, height2, channels);
    max_pool(attn2, pool2, width2, height2, channels);

    FILE *skip2_fp = fopen("skip2.dat", "wb");
    if (!skip2_fp) {
        fprintf(stderr, "Error: Failed to write skip2.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2);
        return 1;
    }
    fwrite(conv2, sizeof(float), width2 * height2 * channels, skip2_fp);
    fclose(skip2_fp);

    int width3 = width2 / 2;
    int height3 = height2 / 2;
    float *conv3 = (float *)calloc(width3 * height3 * channels, sizeof(float));
    float *attn3 = (float *)calloc(width3 * height3 * channels, sizeof(float));
    float *pool3 = (float *)calloc((width3 / 2) * (height3 / 2) * channels, sizeof(float));
    if (!conv3 || !attn3 || !pool3) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3);
        return 1;
    }
    convolve(pool2, conv3, width3, height3, channels, kernels, 0);
    group_norm(conv3, width3, height3, channels);
    attention(conv3, attn3, width3, height3, channels);
    max_pool(attn3, pool3, width3, height3, channels);

    float *bottleneck = (float *)calloc((width3 / 2) * (height3 / 2) * channels, sizeof(float));
    if (!bottleneck) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck);
        return 1;
    }
    convolve(pool3, bottleneck, width3 / 2, height3 / 2, channels, kernels, 0);
    group_norm(bottleneck, width3 / 2, height3 / 2, channels);

    float *upsample3 = (float *)calloc(width3 * height3 * channels, sizeof(float));
    float *conv4 = (float *)calloc(width3 * height3 * channels, sizeof(float));
    if (!upsample3 || !conv4) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4);
        return 1;
    }
    upsample(bottleneck, upsample3, width3 / 2, height3 / 2, channels);
    convolve(upsample3, conv4, width3, height3, channels, kernels, 0);
    group_norm(conv4, width3, height3, channels);

    float *upsample2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    float *conv5 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    if (!upsample2 || !conv5) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5);
        return 1;
    }
    upsample(conv4, upsample2, width3, height3, channels);
    float *skip2 = (float *)calloc(width2 * height2 * channels, sizeof(float));
    if (!skip2) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2);
        return 1;
    }
    FILE *skip2_fp_read = fopen("skip2.dat", "rb");
    if (!skip2_fp_read) {
        fprintf(stderr, "Error: Failed to read skip2.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2);
        return 1;
    }
    size_t skip2_read = fread(skip2, sizeof(float), width2 * height2 * channels, skip2_fp_read);
    fclose(skip2_fp_read);
    remove("skip2.dat");
    if (skip2_read != width2 * height2 * channels) {
        fprintf(stderr, "Error: Failed to read skip2.dat correctly\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2);
        return 1;
    }
    combine_skip(upsample2, skip2, conv5, width2, height2, channels, 0.6f);
    convolve(conv5, conv5, width2, height2, channels, kernels, 0);
    group_norm(conv5, width2, height2, channels);

    float *upsample1 = (float *)calloc(size, sizeof(float));
    float *conv6 = (float *)calloc(size, sizeof(float));
    float *output = (float *)calloc(size, sizeof(float));
    if (!upsample1 || !conv6 || !output) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2); free(upsample1); free(conv6); free(output);
        return 1;
    }
    upsample(conv5, upsample1, width2, height2, channels);
    float *skip1 = (float *)calloc(size, sizeof(float));
    if (!skip1) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2); free(upsample1); free(conv6); free(output); free(skip1);
        return 1;
    }
    FILE *skip1_fp_read = fopen("skip1.dat", "rb");
    if (!skip1_fp_read) {
        fprintf(stderr, "Error: Failed to read skip1.dat\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2); free(upsample1); free(conv6); free(output); free(skip1);
        return 1;
    }
    size_t skip1_read = fread(skip1, sizeof(float), size, skip1_fp_read);
    fclose(skip1_fp_read);
    remove("skip1.dat");
    if (skip1_read != size) {
        fprintf(stderr, "Error: Failed to read skip1.dat correctly\n");
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2); free(upsample1); free(conv6); free(output); free(skip1);
        return 1;
    }
    combine_skip(upsample1, skip1, conv6, width, height, channels, 0.7f);
    convolve(conv6, output, width, height, channels, kernels, 1);
    group_norm(output, width, height, channels);

    convolve(output, output, width, height, channels, sharpen_kernel, 0);
    add_residual(output, input, size, 0.2f);

    float max_val = 0.0f;
    for (int i = 0; i < size; i++) {
        if (fabsf(output[i]) > max_val) max_val = fabsf(output[i]);
    }
    max_val = fmaxf(max_val, 0.1f);
    for (int i = 0; i < size; i++) {
        output[i] = (output[i] / max_val) * 1.0f;
    }

    // Log noise prediction stats
    FILE *stats_fp = fopen("noise_stats.txt", "a");
    if (stats_fp) {
        log_noise_stats(output, size, stats_fp, argv[1]);
        fclose(stats_fp);
    }

    FILE *fp = fopen(argv[2], "wb");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s for writing\n", argv[2]);
        free(input); stbi_image_free(img); free(conv1); free(attn1); free(pool1); free(conv2); free(attn2); free(pool2); free(conv3); free(attn3); free(pool3); free(bottleneck); free(upsample3); free(conv4); free(upsample2); free(conv5); free(skip2); free(upsample1); free(conv6); free(output); free(skip1);
        return 1;
    }
    fwrite(output, sizeof(float), size, fp);
    fclose(fp);

    free(input);
    stbi_image_free(img);
    free(conv1);
    free(attn1);
    free(pool1);
    free(conv2);
    free(attn2);
    free(pool2);
    free(conv3);
    free(attn3);
    free(pool3);
    free(bottleneck);
    free(upsample3);
    free(conv4);
    free(upsample2);
    free(conv5);
    free(skip2);
    free(upsample1);
    free(conv6);
    free(output);
    free(skip1);
    return 0;
}
