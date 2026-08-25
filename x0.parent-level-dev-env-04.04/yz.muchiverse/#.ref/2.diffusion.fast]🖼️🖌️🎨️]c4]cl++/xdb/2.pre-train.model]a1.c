#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>

#define BATCH_SIZE 4
#define EPOCHS 10  // Adjustable; reduce to 1 or 5 if needed
#define LEARNING_RATE 0.001f
#define WEIGHT_DECAY 0.001f
#define NUM_EPOCHS 50
#define MAX_IMAGES 1000
#define MAX_IMAGE_SIZE 1024
#define MAX_BUFFER_SIZE (MAX_IMAGE_SIZE * MAX_IMAGE_SIZE * 3)

// Convolution function with bounds checking
void convolve(float *input, float *output, int width, int height, int channels, float kernels[3][3][3], int use_identity, FILE *log_fp) {
    if (!input || !output || width < 3 || height < 3 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid convolve input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
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
                            if (idx >= 0 && idx < width * height * channels) {
                                sum += input[idx] * kernels[c][ky + 1][kx + 1];
                            }
                        }
                    }
                }
                output[(y * width + x) * channels + c] = fmaxf(0.0f, fminf(sum, 1.0f));
            }
        }
    }
}

// Backpropagation for convolution
void convolve_backward(float *input, float *grad_output, float *grad_kernels, int width, int height, int channels, FILE *log_fp) {
    if (!input || !grad_output || !grad_kernels || width < 3 || height < 3 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
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
                        if (idx >= 0 && idx < width * height * channels) {
                            float grad = grad_output[(y * width + x) * channels + c] * input[idx];
                            grad_kernels[kidx] += grad;
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < 3 * 3 * 3; i++) {
        grad_kernels[i] = fmaxf(-10.0f, fminf(grad_kernels[i], 10.0f));
    }
}

// Group normalization
void group_norm(float *input, int width, int height, int channels, FILE *log_fp) {
    if (!input || width < 1 || height < 1 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid group_norm input or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
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

// Max pooling with safe bounds
void max_pool(float *input, float *output, int width, int height, int channels, FILE *log_fp) {
    if (!input || !output || width < 2 || height < 2 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid max_pool input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int out_width = (width + 1) / 2; // Ceiling division for safety
    int out_height = (height + 1) / 2;
    if (out_width < 1 || out_height < 1) {
        fprintf(log_fp, "Error: Output dimensions too small (%dx%d)\n", out_width, out_height);
        return;
    }
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                float max_val = -INFINITY;
                for (int dy = 0; dy < 2 && (y * 2 + dy) < height; dy++) {
                    for (int dx = 0; dx < 2 && (x * 2 + dx) < width; dx++) {
                        int idx = ((y * 2 + dy) * width + (x * 2 + dx)) * channels + c;
                        if (idx < width * height * channels && input[idx] > max_val) {
                            max_val = input[idx];
                        }
                    }
                }
                output[(y * out_width + x) * channels + c] = max_val;
            }
        }
    }
}

// Upsampling
void upsample(float *input, float *output, int width, int height, int channels, FILE *log_fp) {
    if (!input || !output || width < 1 || height < 1 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid upsample input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int out_width = width * 2;
    int out_height = height * 2;
    if (out_width > MAX_IMAGE_SIZE || out_height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Upsampled dimensions too large (%dx%d)\n", out_width, out_height);
        return;
    }
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                int in_y = y / 2;
                int in_x = x / 2;
                if (in_y < height && in_x < width) {
                    output[(y * out_width + x) * channels + c] = input[(in_y * width + in_x) * channels + c];
                }
            }
        }
    }
}

// Add noise for training
void add_noise(float *input, float *output, float *noise, int size, float t, FILE *log_fp) {
    if (!input || !output || !noise || size < 1 || size > MAX_BUFFER_SIZE) {
        fprintf(log_fp, "Error: Invalid add_noise input/output or size (%d)\n", size);
        return;
    }
    float sqrt_t = sqrtf(t);
    for (int i = 0; i < size; i++) {
        noise[i] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
        output[i] = input[i] * (1.0f - t) + noise[i] * sqrt_t;
    }
}

// Compute MSE loss with clipping
float compute_loss(float *pred, float *target, int size, FILE *log_fp) {
    if (!pred || !target || size < 1 || size > MAX_BUFFER_SIZE) {
        fprintf(log_fp, "Error: Invalid compute_loss input or size (%d)\n", size);
        return 0.0f;
    }
    float loss = 0.0f;
    for (int i = 0; i < size; i++) {
        float pred_clipped = fmaxf(-0.05f, fminf(pred[i], 0.05f));
        float diff = pred_clipped - target[i];
        loss += diff * diff;
    }
    return loss / size;
}

// Compute stats for noise predictions
void log_noise_stats(float *data, int size, FILE *fp, const char *label, const char *type) {
    if (!data || !fp || size < 1 || size > MAX_BUFFER_SIZE) {
        fprintf(fp, "Error: Invalid log_noise_stats input or size (%d)\n", size);
        return;
    }
    float mean = 0.0f, variance = 0.0f;
    for (int i = 0; i < size; i++) {
        mean += data[i];
    }
    mean /= size;
    for (int i = 0; i < size; i++) {
        variance += (data[i] - mean) * (data[i] - mean);
    }
    variance /= size;
    fprintf(fp, "%s (%s): Mean = %f, Variance = %f\n", label, type, mean, variance);
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

// Load dataset with padding for odd dimensions
int load_dataset(const char *dataset_path, float ***images, int **widths, int **heights, int *num_images, int channels, FILE *log_fp) {
    *images = NULL;
    *widths = NULL;
    *heights = NULL;
    *num_images = 0;
    DIR *dir = opendir(dataset_path);
    if (!dir) {
        fprintf(log_fp, "Error: Cannot open directory %s\n", dataset_path);
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *num_images < MAX_IMAGES) {
        if (entry->d_type != DT_REG) continue;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dataset_path, entry->d_name);
        int w, h, c;
        unsigned char *img = stbi_load(filepath, &w, &h, &c, channels);
        if (!img) {
            fprintf(log_fp, "Warning: Failed to load %s\n", filepath);
            continue;
        }
        if (w < 3 || h < 3) {
            fprintf(log_fp, "Warning: Image %s too small (%dx%d), skipping\n", filepath, w, h);
            stbi_image_free(img);
            continue;
        }
        if (w > MAX_IMAGE_SIZE || h > MAX_IMAGE_SIZE) {
            fprintf(log_fp, "Warning: Image %s too large (%dx%d), skipping\n", filepath, w, h);
            stbi_image_free(img);
            continue;
        }

        int padded_width = (w % 2 == 0) ? w : w + 1;
        int padded_height = (h % 2 == 0) ? h : h + 1;

        *images = realloc(*images, (*num_images + 1) * sizeof(float *));
        *widths = realloc(*widths, (*num_images + 1) * sizeof(int));
        *heights = realloc(*heights, (*num_images + 1) * sizeof(int));
        if (!*images || !*widths || !*heights) {
            fprintf(log_fp, "Error: Memory allocation failed for dataset\n");
            free(img); free(*images); free(*widths); free(*heights);
            *num_images = 0;
            return 0;
        }
        (*images)[*num_images] = malloc(padded_width * padded_height * channels * sizeof(float));
        if (!(*images)[*num_images]) {
            fprintf(log_fp, "Error: Memory allocation failed for image data\n");
            free(img); free(*images); free(*widths); free(*heights);
            *num_images = 0;
            return 0;
        }
        memset((*images)[*num_images], 0, padded_width * padded_height * channels * sizeof(float));
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int c = 0; c < channels; c++) {
                    (*images)[*num_images][(y * padded_width + x) * channels + c] = (float)img[(y * w + x) * channels + c] / 255.0f;
                }
            }
        }
        (*widths)[*num_images] = padded_width;
        (*heights)[*num_images] = padded_height;
        (*num_images)++;
        stbi_image_free(img);
        fprintf(log_fp, "Loaded image %s (%dx%d padded to %dx%d)\n", filepath, w, h, padded_width, padded_height);
    }
    closedir(dir);
    fprintf(log_fp, "Loaded %d images from %s\n", *num_images, dataset_path);
    return *num_images > 0;
}

// Free dataset
void free_dataset(float ***images, int **widths, int **heights, int num_images) {
    if (!images || !*images || !widths || !*widths || !heights || !*heights) return;
    for (int i = 0; i < num_images; i++) {
        free((*images)[i]);
    }
    free(*images); *images = NULL;
    free(*widths); *widths = NULL;
    free(*heights); *heights = NULL;
}

// Forward pass for a single image
void forward_pass(float *input, float *output, int width, int height, int channels, float kernels[3][3][3], float kernels2[3][3][3], FILE *log_fp) {
    if (!input || !output || width < 3 || height < 3 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid forward_pass input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    static float conv1[MAX_BUFFER_SIZE];
    static float pool1[MAX_BUFFER_SIZE / 4];
    static float conv2[MAX_BUFFER_SIZE / 4];
    static float upsampled_conv2[MAX_BUFFER_SIZE];
    memset(conv1, 0, MAX_BUFFER_SIZE * sizeof(float));
    memset(pool1, 0, (MAX_BUFFER_SIZE / 4) * sizeof(float));
    memset(conv2, 0, (MAX_BUFFER_SIZE / 4) * sizeof(float));
    memset(upsampled_conv2, 0, MAX_BUFFER_SIZE * sizeof(float));

    convolve(input, conv1, width, height, channels, kernels, 1, log_fp);
    group_norm(conv1, width, height, channels, log_fp);
    max_pool(conv1, pool1, width, height, channels, log_fp);
    convolve(pool1, conv2, width / 2, height / 2, channels, kernels2, 0, log_fp);
    group_norm(conv2, width / 2, height / 2, channels, log_fp);
    upsample(conv2, upsampled_conv2, width / 2, height / 2, channels, log_fp);
    memcpy(output, upsampled_conv2, width * height * channels * sizeof(float));
}

// Train one epoch
void train_epoch(float **images, int *widths, int *heights, int num_images, int channels, float kernels[3][3][3], float kernels2[3][3][3], FILE *loss_fp) {
    static float noisy_input[MAX_BUFFER_SIZE];
    static float noise[MAX_BUFFER_SIZE];
    static float pred_noise[MAX_BUFFER_SIZE];
    static float grad_output[MAX_BUFFER_SIZE];
    static float temp_pool1[MAX_BUFFER_SIZE / 4];
    static float grad_kernels[3 * 3 * 3];
    static float grad_kernels2[3 * 3 * 3];

    for (int i = 0; i < num_images; i++) {
        int width = widths[i];
        int height = heights[i];
        int size = width * height * channels;
        fprintf(loss_fp, "Starting image %d (%dx%d, %d channels)\n", i, width, height, channels);
        fflush(loss_fp);
        if (width < 3 || height < 3 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE || size > MAX_BUFFER_SIZE) {
            fprintf(loss_fp, "Error: Image %d invalid dimensions (%dx%d, size %d), skipping\n", i, width, height, size);
            continue;
        }

        memset(noisy_input, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(noise, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(pred_noise, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(grad_output, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(temp_pool1, 0, (MAX_BUFFER_SIZE / 4) * sizeof(float));
        memset(grad_kernels, 0, 3 * 3 * 3 * sizeof(float));
        memset(grad_kernels2, 0, 3 * 3 * 3 * sizeof(float));

        float timestep = 0.001f + (0.5f - 0.001f) * (rand() % NUM_EPOCHS) / (NUM_EPOCHS - 1);
        add_noise(images[i], noisy_input, noise, size, timestep, loss_fp);
        fprintf(loss_fp, "Added noise to image %d\n", i);
        fflush(loss_fp);

        forward_pass(noisy_input, pred_noise, width, height, channels, kernels, kernels2, loss_fp);
        fprintf(loss_fp, "Forward pass completed for image %d\n", i);
        fflush(loss_fp);

        float loss = compute_loss(pred_noise, noise, size, loss_fp);
        if (loss == 0.0f) {
            fprintf(loss_fp, "Error: Loss computation failed for image %d\n", i);
            continue;
        }
        fprintf(loss_fp, "Computed loss for image %d: %f\n", i, loss);
        fflush(loss_fp);

        for (int j = 0; j < size; j++) {
            float pred_clipped = fmaxf(-0.05f, fminf(pred_noise[j], 0.05f));
            grad_output[j] = 2.0f * (pred_clipped - noise[j]) / size;
        }
        fprintf(loss_fp, "Computed grad_output for image %d\n", i);
        fflush(loss_fp);

        convolve(noisy_input, temp_pool1, width, height, channels, kernels, 1, loss_fp);
        group_norm(temp_pool1, width, height, channels, loss_fp);
        max_pool(temp_pool1, temp_pool1, width, height, channels, loss_fp);
        convolve_backward(temp_pool1, grad_output, grad_kernels2, width / 2, height / 2, channels, loss_fp);
        convolve_backward(noisy_input, grad_output, grad_kernels, width, height, channels, loss_fp);

        for (int c = 0; c < channels; c++) {
            for (int ky = 0; ky < 3; ky++) {
                for (int kx = 0; kx < 3; kx++) {
                    int idx = c * 9 + ky * 3 + kx;
                    kernels[c][ky][kx] -= LEARNING_RATE * (grad_kernels[idx] + WEIGHT_DECAY * kernels[c][ky][kx]);
                    kernels2[c][ky][kx] -= LEARNING_RATE * (grad_kernels2[idx] + WEIGHT_DECAY * kernels2[c][ky][kx]);
                }
            }
        }
        fprintf(loss_fp, "Completed image %d\n", i);
        fflush(loss_fp);
    }
}

// Main training function
void train_model(const char *dataset_path, const char *output_file) {
    srand(time(NULL));
    FILE *loss_fp = fopen("loss_debug.txt", "a");
    if (!loss_fp) {
        fprintf(stderr, "Error: Failed to open loss_debug.txt\n");
        return;
    }

    float **images = NULL;
    int *widths = NULL, *heights = NULL;
    int num_images = 0;
    int channels = 3;

    if (!load_dataset(dataset_path, &images, &widths, &heights, &num_images, channels, loss_fp)) {
        fprintf(stderr, "Error: No images loaded from %s\n", dataset_path);
        fclose(loss_fp);
        return;
    }

    float kernels[3][3][3];
    float kernels2[3][3][3];
    for (int c = 0; c < 3; c++) {
        for (int ky = 0; ky < 3; ky++) {
            for (int kx = 0; kx < 3; kx++) {
                kernels[c][ky][kx] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
                kernels2[c][ky][kx] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
            }
        }
    }

    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        printf("Epoch %d/%d\n", epoch + 1, EPOCHS);
        fprintf(loss_fp, "Epoch %d\n", epoch + 1);
        train_epoch(images, widths, heights, num_images, channels, kernels, kernels2, loss_fp);
    }

    FILE *kernel_fp = fopen(output_file, "wb");
    if (!kernel_fp) {
        fprintf(stderr, "Error: Failed to open %s\n", output_file);
        free_dataset(&images, &widths, &heights, num_images);
        fclose(loss_fp);
        return;
    }
    fwrite(kernels, sizeof(float), 3 * 3 * 3, kernel_fp);
    fwrite(kernels2, sizeof(float), 3 * 3 * 3, kernel_fp);
    fclose(kernel_fp);

    free_dataset(&images, &widths, &heights, num_images);
    fclose(loss_fp);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <dataset_path> <output_file>\n", argv[0]);
        return 1;
    }
    train_model(argv[1], argv[2]);
    return 0;
}
