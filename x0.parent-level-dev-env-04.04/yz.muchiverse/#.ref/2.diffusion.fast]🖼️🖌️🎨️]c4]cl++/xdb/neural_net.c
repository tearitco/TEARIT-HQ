#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define NUM_EPOCHS 100
#define LEARNING_RATE 0.01f
#define WEIGHT_DECAY 0.001f
#define KERNEL_REGULARIZATION 0.001f
#define PATCH_SIZE 64

float original_kernels[3][3][3];
float original_kernels2[3][3][3];

// Convolution function
void convolve(float *input, float *output, int width, int height, int channels, float kernels[3][3][3], FILE *log_fp) {
    if (!input || !output || width < 3 || height < 3 || channels != 3) {
        fprintf(log_fp, "Error: Invalid convolve input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
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
                output[(y * width + x) * channels + c] = fmaxf(0.0f, fminf(sum, 1.0f));
            }
        }
    }
}

// Backpropagation for convolution
void convolve_backward(float *input, float *grad_output, float *grad_kernels, int width, int height, int channels, FILE *log_fp) {
    if (!input || !grad_output || !grad_kernels || width < 3 || height < 3 || channels != 3) {
        fprintf(log_fp, "Error: Invalid convolve_backward input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    for (int c = 0; c < channels; c++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int kidx = c * 9 + (ky + 1) * 3 + (kx + 1);
                        int idx = ((y + ky) * width + (x + kx)) * channels + c;
                        float grad = grad_output[(y * width + x) * channels + c] * input[idx];
                        grad_kernels[kidx] += grad;
                    }
                }
            }
        }
    }
    for (int i = 0; i < 27; i++) {
        grad_kernels[i] = fmaxf(-10.0f, fminf(grad_kernels[i], 10.0f));
    }
}

// Group normalization with channel balance
void group_norm(float *input, int width, int height, int channels, FILE *log_fp) {
    if (!input || width < 1 || height < 1 || channels != 3) {
        fprintf(log_fp, "Error: Invalid group_norm input or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int count = width * height;
    float means[3] = {0}, variances[3] = {0};
    for (int c = 0; c < channels; c++) {
        for (int i = 0; i < count; i++) {
            means[c] += input[i * channels + c];
        }
        means[c] /= count;
        for (int i = 0; i < count; i++) {
            variances[c] += (input[i * channels + c] - means[c]) * (input[i * channels + c] - means[c]);
        }
        variances[c] = sqrtf(variances[c] / count + 1e-6f);
    }
    for (int c = 0; c < channels; c++) {
        for (int i = 0; i < count; i++) {
            input[i * channels + c] = variances[c] > 1e-3f ? (input[i * channels + c] - means[c]) / variances[c] : input[i * channels + c];
        }
    }
    fprintf(log_fp, "GroupNorm Stats: R(Mean=%f, Var=%f), G(Mean=%f, Var=%f), B(Mean=%f, Var=%f)\n",
            means[0], variances[0], means[1], variances[1], means[2], variances[2]);
}

// Patch-based self-attention
void attention(float *input, float *output, int width, int height, int channels, float *work_buffer, size_t work_size, FILE *log_fp) {
    if (!input || !output || !work_buffer || width < PATCH_SIZE || height < PATCH_SIZE || channels != 3) {
        fprintf(log_fp, "Error: Invalid attention input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int patch_pixels = PATCH_SIZE * PATCH_SIZE;
    if (work_size < patch_pixels * channels * 3 + patch_pixels * patch_pixels) {
        fprintf(log_fp, "Error: Work buffer too small (%zu < %zu)\n", work_size, patch_pixels * channels * 3 + patch_pixels * patch_pixels);
        return;
    }
    float *query = work_buffer;
    float *key = query + patch_pixels * channels;
    float *value = key + patch_pixels * channels;
    float *attn_weights = value + patch_pixels * channels;
    float scale = 1.0f / sqrtf(channels);
    float mean = 0.0f, variance = 0.0f;
    int total_pixels = 0;

    for (int py = 0; py < height; py += PATCH_SIZE) {
        for (int px = 0; px < width; px += PATCH_SIZE) {
            int pw = fmin(PATCH_SIZE, width - px);
            int ph = fmin(PATCH_SIZE, height - py);
            int patch_size = pw * ph;
            if (patch_size < 1) continue;

            for (int y = 0; y < ph; y++) {
                for (int x = 0; x < pw; x++) {
                    int idx = ((py + y) * width + (px + x)) * channels;
                    int pidx = (y * pw + x) * channels;
                    for (int c = 0; c < channels; c++) {
                        query[pidx + c] = input[idx + c];
                        key[pidx + c] = input[idx + c];
                        value[pidx + c] = input[idx + c];
                    }
                }
            }

            for (int i = 0; i < patch_size; i++) {
                for (int j = 0; j < patch_size; j++) {
                    float score = 0.0f;
                    for (int c = 0; c < channels; c++) {
                        score += query[i * channels + c] * key[j * channels + c];
                    }
                    attn_weights[i * patch_size + j] = score * scale;
                }
            }

            for (int i = 0; i < patch_size; i++) {
                float max_val = -INFINITY;
                for (int j = 0; j < patch_size; j++) {
                    if (attn_weights[i * patch_size + j] > max_val) max_val = attn_weights[i * patch_size + j];
                }
                float sum = 0.0f;
                for (int j = 0; j < patch_size; j++) {
                    attn_weights[i * patch_size + j] = expf(attn_weights[i * patch_size + j] - max_val);
                    sum += attn_weights[i * patch_size + j];
                }
                for (int j = 0; j < patch_size; j++) {
                    attn_weights[i * patch_size + j] /= sum + 1e-6f;
                    mean += attn_weights[i * patch_size + j];
                    total_pixels++;
                }
            }

            for (int i = 0; i < patch_size; i++) {
                for (int c = 0; c < channels; c++) {
                    float sum = 0.0f;
                    for (int j = 0; j < patch_size; j++) {
                        sum += attn_weights[i * patch_size + j] * value[j * channels + c];
                    }
                    int y = i / pw, x = i % pw;
                    int idx = ((py + y) * width + (px + x)) * channels + c;
                    output[idx] = fmaxf(0.0f, fminf(sum, 1.0f));
                }
            }
        }
    }

    if (total_pixels > 0) {
        mean /= total_pixels;
        for (int i = 0; i < total_pixels; i++) {
            variance += (attn_weights[i % (patch_pixels * patch_pixels)] - mean) * (attn_weights[i % (patch_pixels * patch_pixels)] - mean);
        }
        variance /= total_pixels;
        fprintf(log_fp, "Attention Stats: Mean = %f, Variance = %f\n", mean, variance);
    }
}

// Max pooling
void max_pool(float *input, float *output, int width, int height, int channels, FILE *log_fp) {
    if (!input || !output || width < 2 || height < 2 || channels != 3) {
        fprintf(log_fp, "Error: Invalid max_pool input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int out_width = width / 2, out_height = height / 2;
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
void upsample(float *input, float *output, int width, int height, int channels, FILE *log_fp) {
    if (!input || !output || width < 1 || height < 1 || channels != 3) {
        fprintf(log_fp, "Error: Invalid upsample input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int out_width = width * 2, out_height = height * 2;
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                output[(y * out_width + x) * channels + c] = input[(y / 2 * width + x / 2) * channels + c];
            }
        }
    }
}

// Combine skip connection
void combine_skip(float *main, float *skip, float *output, int width, int height, int channels, float skip_weight, float kernels[3][3][3], FILE *log_fp) {
    if (!main || !skip || !output || width < 3 || height < 3 || channels != 3) {
        fprintf(log_fp, "Error: Invalid combine_skip input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    float *skip_conv = (float *)calloc(width * height * channels, sizeof(float));
    if (!skip_conv) {
        fprintf(log_fp, "Error: Memory allocation failed for skip_conv\n");
        return;
    }
    convolve(skip, skip_conv, width, height, channels, kernels, log_fp);
    for (int i = 0; i < width * height * channels; i++) {
        output[i] = main[i] * (1.0f - skip_weight) + skip_conv[i] * skip_weight;
    }
    free(skip_conv);
}

// Residual connection
void add_residual(float *output, float *input, int size, float residual_weight) {
    for (int i = 0; i < size; i++) {
        output[i] = output[i] * (1.0f - residual_weight) + input[i] * residual_weight;
    }
}

// Add noise for training
void add_noise(float *input, float *output, float *noise, int size, float t, FILE *log_fp) {
    if (!input || !output || !noise || size < 1) {
        fprintf(log_fp, "Error: Invalid add_noise input/output or size (%d)\n", size);
        return;
    }
    float sqrt_t = sqrtf(t);
    for (int i = 0; i < size; i++) {
        noise[i] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
        output[i] = input[i] * (1.0f - t) + noise[i] * sqrt_t;
    }
}

// Compute MSE loss
float compute_loss(float *pred, float *target, int size, float kernels[3][3][3], float kernels2[3][3][3], FILE *log_fp) {
    if (!pred || !target || size < 1) {
        fprintf(log_fp, "Error: Invalid compute_loss input or size (%d)\n", size);
        return 0.0f;
    }
    float loss = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    loss /= size;
    float reg_loss = 0.0f;
    for (int c = 0; c < 3; c++) {
        for (int ky = 0; ky < 3; ky++) {
            for (int kx = 0; kx < 3; kx++) {
                float diff1 = kernels[c][ky][kx] - original_kernels[c][ky][kx];
                float diff2 = kernels2[c][ky][kx] - original_kernels2[c][ky][kx];
                reg_loss += diff1 * diff1 + diff2 * diff2;
            }
        }
    }
    loss += KERNEL_REGULARIZATION * reg_loss;
    return loss;
}

// Log channel statistics
void log_channel_stats(float *data, int width, int height, int channels, FILE *fp, const char *label) {
    if (!data || !fp || width < 1 || height < 1 || channels != 3) {
        fprintf(fp, "Error: Invalid log_channel_stats input or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int count = width * height;
    float means[3] = {0}, variances[3] = {0};
    for (int c = 0; c < channels; c++) {
        for (int i = 0; i < count; i++) {
            means[c] += data[i * channels + c];
        }
        means[c] /= count;
        for (int i = 0; i < count; i++) {
            variances[c] += (data[i * channels + c] - means[c]) * (data[i * channels + c] - means[c]);
        }
        variances[c] /= count;
    }
    fprintf(fp, "%s: R(Mean=%f, Var=%f), G(Mean=%f, Var=%f), B(Mean=%f, Var=%f)\n",
            label, means[0], variances[0], means[1], variances[1], means[2], variances[2]);
    fflush(fp);
}

// Compute gradient magnitude
float compute_grad_magnitude(float *grad_kernels, int size, FILE *log_fp) {
    if (!grad_kernels || size < 1) {
        fprintf(log_fp, "Error: Invalid compute_grad_magnitude input or size (%d)\n", size);
        return 0.0f;
    }
    float mag = 0.0f;
    for (int i = 0; i < size; i++) {
        mag += grad_kernels[i] * grad_kernels[i];
    }
    return sqrtf(mag);
}

// Log kernel statistics
void log_kernel_stats(float kernels[3][3][3], const char *label, FILE *fp) {
    float means[3] = {0}, variances[3] = {0};
    for (int c = 0; c < 3; c++) {
        for (int ky = 0; ky < 3; ky++) {
            for (int kx = 0; kx < 3; kx++) {
                means[c] += kernels[c][ky][kx];
            }
        }
        means[c] /= 9;
        for (int ky = 0; ky < 3; ky++) {
            for (int kx = 0; kx < 3; kx++) {
                variances[c] += (kernels[c][ky][kx] - means[c]) * (kernels[c][ky][kx] - means[c]);
            }
        }
        variances[c] /= 9;
    }
    fprintf(fp, "Kernel Stats (%s): R(Mean=%f, Var=%f), G(Mean=%f, Var=%f), B(Mean=%f, Var=%f)\n",
            label, means[0], variances[0], means[1], variances[1], means[2], variances[2]);
    fflush(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input_image> <output_file> <mode:train/infer> <kernel_file>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    int train_mode = strcmp(argv[3], "train") == 0;

    float kernels[3][3][3];
    float kernels2[3][3][3];
    float sharpen_kernel[3][3][3] = {{{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}}, {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}}, {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}}};

    FILE *kernel_fp = fopen(argv[4], "rb");
    if (!kernel_fp) {
        fprintf(stderr, "Error: Failed to open %s\n", argv[4]);
        return 1;
    }
    size_t read = fread(kernels, sizeof(float), 27, kernel_fp);
    size_t read2 = fread(kernels2, sizeof(float), 27, kernel_fp);
    fclose(kernel_fp);
    if (read != 27 || read2 != 27) {
        fprintf(stderr, "Error: Failed to read %s\n", argv[4]);
        return 1;
    }
    memcpy(original_kernels, kernels, 27 * sizeof(float));
    memcpy(original_kernels2, kernels2, 27 * sizeof(float));

    int width, height, channels;
    unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 3);
    if (!img || channels != 3 || width < PATCH_SIZE || height < PATCH_SIZE) {
        fprintf(stderr, "Error: Failed to load %s or invalid dimensions (%dx%d, %d channels)\n", argv[1], width, height, channels);
        if (img) stbi_image_free(img);
        return 1;
    }

    int size = width * height * channels;
    float *input = (float *)malloc(size * sizeof(float));
    if (!input) {
        fprintf(stderr, "Error: Memory allocation failed for input\n");
        stbi_image_free(img);
        return 1;
    }
    for (int i = 0; i < size; i++) input[i] = img[i] / 255.0f;

    size_t work_size = size * 2 + PATCH_SIZE * PATCH_SIZE * (channels * 3 + PATCH_SIZE);
    float *work_buffer = (float *)calloc(work_size, sizeof(float));
    if (!work_buffer) {
        fprintf(stderr, "Error: Memory allocation failed for work_buffer (%zu bytes)\n", work_size * sizeof(float));
        free(input); stbi_image_free(img);
        return 1;
    }

    FILE *log_fp = fopen("neural_log.txt", "a");
    if (!log_fp) {
        fprintf(stderr, "Error: Failed to open neural_log.txt\n");
        free(input); stbi_image_free(img); free(work_buffer);
        return 1;
    }
    fprintf(log_fp, "Processing %s (%dx%d, %d channels, %zu bytes)\n", argv[1], width, height, channels, size * sizeof(float));
    log_channel_stats(input, width, height, channels, log_fp, "Input");

    if (train_mode) {
        float *noisy_input = work_buffer;
        float *noise = work_buffer + size;
        float *conv1 = (float *)calloc(size, sizeof(float));
        float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
        float *conv2 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
        float *upsampled = (float *)calloc(size, sizeof(float));
        float *pred_noise = (float *)calloc(size, sizeof(float));
        float *grad_output = (float *)calloc(size, sizeof(float));
        float grad_kernels[27] = {0}, grad_kernels2[27] = {0};
        if (!conv1 || !pool1 || !conv2 || !upsampled || !pred_noise || !grad_output) {
            fprintf(log_fp, "Error: Memory allocation failed for training buffers\n");
            free(input); stbi_image_free(img); free(work_buffer); free(conv1); free(pool1); free(conv2); free(upsampled); free(pred_noise); free(grad_output); fclose(log_fp);
            return 1;
        }

        float initial_kernels[3][3][3];
        float initial_kernels2[3][3][3];
        memcpy(initial_kernels, kernels, 27 * sizeof(float));
        memcpy(initial_kernels2, kernels2, 27 * sizeof(float));
        log_kernel_stats(kernels, "Initial Kernels", log_fp);
        log_kernel_stats(kernels2, "Initial Kernels2", log_fp);

        for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
            float t = 0.001f + (0.5f - 0.001f) * epoch / (NUM_EPOCHS - 1);
            add_noise(input, noisy_input, noise, size, t, log_fp);

            convolve(noisy_input, conv1, width, height, channels, kernels, log_fp);
            group_norm(conv1, width, height, channels, log_fp);
            log_channel_stats(conv1, width, height, channels, log_fp, "Conv1");
            max_pool(conv1, pool1, width, height, channels, log_fp);
            convolve(pool1, conv2, width / 2, height / 2, channels, kernels2, log_fp);
            group_norm(conv2, width / 2, height / 2, channels, log_fp);
            log_channel_stats(conv2, width / 2, height / 2, channels, log_fp, "Conv2");
            attention(conv2, conv2, width / 2, height / 2, channels, work_buffer + size * 2, work_size - size * 2, log_fp);
            log_channel_stats(conv2, width / 2, height / 2, channels, log_fp, "Attention");
            upsample(conv2, upsampled, width / 2, height / 2, channels, log_fp);
            convolve(upsampled, pred_noise, width, height, channels, kernels, log_fp);
            log_channel_stats(pred_noise, width, height, channels, log_fp, "PredNoise");

            float loss = compute_loss(pred_noise, noise, size, kernels, kernels2, log_fp);
            for (int i = 0; i < size; i++) {
                grad_output[i] = 2.0f * (pred_noise[i] - noise[i]) / size;
            }
            memset(grad_kernels2, 0, 27 * sizeof(float));
            convolve_backward(pool1, grad_output, grad_kernels2, width / 2, height / 2, channels, log_fp);
            memset(grad_kernels, 0, 27 * sizeof(float));
            convolve_backward(noisy_input, grad_output, grad_kernels, width, height, channels, log_fp);

            for (int i = 0; i < 27; i++) {
                kernels[i / 9][(i % 9) / 3][i % 3] -= LEARNING_RATE * (grad_kernels[i] + WEIGHT_DECAY * kernels[i / 9][(i % 9) / 3][i % 3]);
                kernels2[i / 9][(i % 9) / 3][i % 3] -= LEARNING_RATE * (grad_kernels2[i] + WEIGHT_DECAY * kernels2[i / 9][(i % 9) / 3][i % 3]);
            }

            float grad_mag = compute_grad_magnitude(grad_kernels, 27, log_fp);
            float grad_mag2 = compute_grad_magnitude(grad_kernels2, 27, log_fp);
            fprintf(log_fp, "Epoch %d: Loss = %f, GradMag1 = %f, GradMag2 = %f\n", epoch + 1, loss, grad_mag, grad_mag2);
            log_channel_stats(noise, width, height, channels, log_fp, "TargetNoise");
            log_kernel_stats(kernels, "Updated Kernels", log_fp);
            log_kernel_stats(kernels2, "Updated Kernels2", log_fp);
            fflush(log_fp);
        }

        float kernel_diff = 0.0f, kernel2_diff = 0.0f;
        for (int i = 0; i < 27; i++) {
            kernel_diff += fabsf(kernels[i / 9][(i % 9) / 3][i % 3] - initial_kernels[i / 9][(i % 9) / 3][i % 3]);
            kernel2_diff += fabsf(kernels2[i / 9][(i % 9) / 3][i % 3] - initial_kernels2[i / 9][(i % 9) / 3][i % 3]);
        }
        fprintf(log_fp, "Kernel Diff: %f, Kernel2 Diff: %f\n", kernel_diff, kernel2_diff);

        kernel_fp = fopen(argv[4], "wb");
        if (!kernel_fp) {
            fprintf(log_fp, "Error: Failed to open %s for writing\n", argv[4]);
            free(input); stbi_image_free(img); free(work_buffer); free(conv1); free(pool1); free(conv2); free(upsampled); free(pred_noise); free(grad_output); fclose(log_fp);
            return 1;
        }
        fwrite(kernels, sizeof(float), 27, kernel_fp);
        fwrite(kernels2, sizeof(float), 27, kernel_fp);
        fclose(kernel_fp);
        free(conv1); free(pool1); free(conv2); free(upsampled); free(pred_noise); free(grad_output);
    } else {
        float *conv1 = (float *)calloc(size, sizeof(float));
        float *pool1 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
        float *conv2 = (float *)calloc((width / 2) * (height / 2) * channels, sizeof(float));
        float *upsampled = (float *)calloc(size, sizeof(float));
        float *output = (float *)calloc(size, sizeof(float));
        if (!conv1 || !pool1 || !conv2 || !upsampled || !output) {
            fprintf(log_fp, "Error: Memory allocation failed for inference buffers\n");
            free(input); stbi_image_free(img); free(work_buffer); free(conv1); free(pool1); free(conv2); free(upsampled); free(output); fclose(log_fp);
            return 1;
        }

        convolve(input, conv1, width, height, channels, kernels, log_fp);
        group_norm(conv1, width, height, channels, log_fp);
        log_channel_stats(conv1, width, height, channels, log_fp, "Conv1");
        max_pool(conv1, pool1, width, height, channels, log_fp);
        convolve(pool1, conv2, width / 2, height / 2, channels, kernels2, log_fp);
        group_norm(conv2, width / 2, height / 2, channels, log_fp);
        log_channel_stats(conv2, width / 2, height / 2, channels, log_fp, "Conv2");
        attention(conv2, conv2, width / 2, height / 2, channels, work_buffer + size * 2, work_size - size * 2, log_fp);
        log_channel_stats(conv2, width / 2, height / 2, channels, log_fp, "Attention");
        upsample(conv2, upsampled, width / 2, height / 2, channels, log_fp);
        convolve(upsampled, output, width, height, channels, kernels, log_fp);
        log_channel_stats(output, width, height, channels, log_fp, "OutputPreResidual");
        add_residual(output, input, size, 0.2f);
        log_channel_stats(output, width, height, channels, log_fp, "OutputPostResidual");
        convolve(output, output, width, height, channels, sharpen_kernel, log_fp);
        log_channel_stats(output, width, height, channels, log_fp, "OutputFinal");

        float mins[3] = {INFINITY, INFINITY, INFINITY}, maxs[3] = {-INFINITY, -INFINITY, -INFINITY};
        for (int i = 0; i < width * height; i++) {
            for (int c = 0; c < channels; c++) {
                float val = output[i * channels + c];
                if (val < mins[c]) mins[c] = val;
                if (val > maxs[c]) maxs[c] = val;
            }
        }
        for (int i = 0; i < size; i++) {
            int c = i % channels;
            output[i] = (maxs[c] > mins[c]) ? (output[i] - mins[c]) / (maxs[c] - mins[c]) : output[i];
            output[i] = fmaxf(0.0f, fminf(output[i], 1.0f));
        }
        log_channel_stats(output, width, height, channels, log_fp, "OutputScaled");

        unsigned char *img_out = (unsigned char *)malloc(size);
        if (!img_out) {
            fprintf(log_fp, "Error: Memory allocation failed for img_out\n");
            free(input); stbi_image_free(img); free(work_buffer); free(conv1); free(pool1); free(conv2); free(upsampled); free(output); fclose(log_fp);
            return 1;
        }
        for (int i = 0; i < size; i++) {
            img_out[i] = (unsigned char)(output[i] * 255.0f);
        }
        stbi_write_jpg("output.jpg", width, height, channels, img_out, 100);
        free(img_out);

        FILE *stats_fp = fopen("noise_stats.txt", "a");
        if (stats_fp) {
            log_channel_stats(output, width, height, channels, stats_fp, "Output");
            log_kernel_stats(kernels, "Inference Kernels", stats_fp);
            log_kernel_stats(kernels2, "Inference Kernels2", stats_fp);
            fclose(stats_fp);
        }

        FILE *fp = fopen(argv[2], "wb");
        if (!fp) {
            fprintf(log_fp, "Error: Failed to open %s for writing\n", argv[2]);
            free(input); stbi_image_free(img); free(work_buffer); free(conv1); free(pool1); free(conv2); free(upsampled); free(output); fclose(log_fp);
            return 1;
        }
        fwrite(output, sizeof(float), size, fp);
        fclose(fp);
        free(conv1); free(pool1); free(conv2); free(upsampled); free(output);
    }

    free(input);
    stbi_image_free(img);
    free(work_buffer);
    fclose(log_fp);
    return 0;
}
