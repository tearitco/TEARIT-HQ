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
#define EPOCHS 10
#define LEARNING_RATE 0.001f
#define WEIGHT_DECAY 0.001f
#define NUM_EPOCHS 50
#define MAX_IMAGES 1000
#define MAX_IMAGE_SIZE 1024
#define MAX_BUFFER_SIZE (MAX_IMAGE_SIZE * MAX_IMAGE_SIZE * 3)

// Convolution function
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

// Max pooling
void max_pool(float *input, float *output, int width, int height, int channels, FILE *log_fp) {
    if (!input || !output || width < 2 || height < 2 || channels < 1 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(log_fp, "Error: Invalid max_pool input/output or dimensions (%dx%d, %d channels)\n", width, height, channels);
        return;
    }
    int out_width = width / 2;
    int out_height = height / 2;
    if (out_width < 1 || out_height < 1) {
        fprintf(log_fp, "Error: Output dimensions too small (%dx%d)\n", out_width, out_height);
        return;
    }
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            for (int c = 0; c < channels; c++) {
                float max_val = -INFINITY;
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int idx = ((y * 2 + dy) * width + (x * 2 + dx)) * channels + c;
                        if (idx >= 0 && idx < width * height * channels) {
                            if (input[idx] > max_val) max_val = input[idx];
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

// Load dataset from directory
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

        *images = realloc(*images, (*num_images + 1) * sizeof(float *));
        *widths = realloc(*widths, (*num_images + 1) * sizeof(int));
        *heights = realloc(*heights, (*num_images + 1) * sizeof(int));
        if (!*images || !*widths || !*heights) {
            fprintf(log_fp, "Error: Memory allocation failed for dataset\n");
            free(img); free(*images); free(*widths); free(*heights);
            *num_images = 0;
            return 0;
        }
        (*images)[*num_images] = malloc(w * h * channels * sizeof(float));
        if (!(*images)[*num_images]) {
            fprintf(log_fp, "Error: Memory allocation failed for image data\n");
            free(img); free(*images); free(*widths); free(*heights);
            *num_images = 0;
            return 0;
        }
        (*widths)[*num_images] = w;
        (*heights)[*num_images] = h;
        for (int i = 0; i < w * h * channels; i++) {
            (*images)[*num_images][i] = (float)img[i] / 255.0f;
        }
        (*num_images)++;
        stbi_image_free(img);
        fprintf(log_fp, "Loaded image %s (%dx%d)\n", filepath, w, h);
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
void train_epoch(int epoch, float **images, int *widths, int *heights, int num_images, int channels, float kernels[3][3][3], float kernels2[3][3][3], FILE *debug_fp, FILE *loss_fp) {
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
        fprintf(debug_fp, "Processing image %d (%dx%d, %d channels)\n", i, width, height, channels);
        if (width < 3 || height < 3 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE || size > MAX_BUFFER_SIZE) {
            fprintf(debug_fp, "Error: Image %d invalid dimensions (%dx%d, size %d), skipping\n", i, width, height, size);
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
        add_noise(images[i], noisy_input, noise, size, timestep, debug_fp);
        forward_pass(noisy_input, pred_noise, width, height, channels, kernels, kernels2, debug_fp);

        float loss = compute_loss(pred_noise, noise, size, debug_fp);
        if (loss == 0.0f) {
            fprintf(debug_fp, "Error: Loss computation failed for image %d\n", i);
            continue;
        }
        fprintf(loss_fp, "Epoch %d, Image %d: Loss = %f\n", epoch + 1, i, loss);

        for (int j = 0; j < size; j++) {
            float pred_clipped = fmaxf(-0.05f, fminf(pred_noise[j], 0.05f));
            grad_output[j] = 2.0f * (pred_clipped - noise[j]) / size;
        }

        convolve(noisy_input, temp_pool1, width, height, channels, kernels, 1, debug_fp);
        group_norm(temp_pool1, width, height, channels, debug_fp);
        max_pool(temp_pool1, temp_pool1, width, height, channels, debug_fp);
        convolve_backward(temp_pool1, grad_output, grad_kernels2, width / 2, height / 2, channels, debug_fp);
        convolve_backward(noisy_input, grad_output, grad_kernels, width, height, channels, debug_fp);

        for (int c = 0; c < channels; c++) {
            for (int ky = 0; ky < 3; ky++) {
                for (int kx = 0; kx < 3; kx++) {
                    int idx = c * 9 + ky * 3 + kx;
                    kernels[c][ky][kx] -= LEARNING_RATE * (grad_kernels[idx] + WEIGHT_DECAY * kernels[c][ky][kx]);
                    kernels2[c][ky][kx] -= LEARNING_RATE * (grad_kernels2[idx] + WEIGHT_DECAY * kernels2[c][ky][kx]);
                }
            }
        }

        float grad_mag = compute_grad_magnitude(grad_kernels, 3 * 3 * 3, debug_fp);
        float grad_mag2 = compute_grad_magnitude(grad_kernels2, 3 * 3 * 3, debug_fp);
        fprintf(debug_fp, "Image %d: Loss = %f, GradMag1 = %f, GradMag2 = %f\n", i, loss, grad_mag, grad_mag2);
        log_noise_stats(noise, size, debug_fp, "dataset_image", "target_noise");
        log_noise_stats(pred_noise, size, debug_fp, "dataset_image", "pred_noise");
        fflush(debug_fp);
    }
}

// Train model for a single image and output kernel file
void train_model_for_image(const char *image_path, const char *output_file, FILE *debug_fp, FILE *loss_fp) {
    srand(time(NULL));

    int width, height, channels = 3;
    unsigned char *img = stbi_load(image_path, &width, &height, NULL, channels);
    if (!img) {
        fprintf(debug_fp, "Error: Failed to load %s\n", image_path);
        return;
    }
    if (width < 3 || height < 3 || width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fprintf(debug_fp, "Error: Invalid image dimensions (%dx%d) for %s\n", width, height, image_path);
        stbi_image_free(img);
        return;
    }

    float **images = (float **)malloc(sizeof(float *));
    int *widths = (int *)malloc(sizeof(int));
    int *heights = (int *)malloc(sizeof(int));
    int num_images = 1;
    images[0] = (float *)malloc(width * height * channels * sizeof(float));
    if (!images || !widths || !heights || !images[0]) {
        fprintf(debug_fp, "Error: Memory allocation failed for %s\n", image_path);
        free(images); free(widths); free(heights); if (images && images[0]) free(images[0]);
        stbi_image_free(img);
        return;
    }
    widths[0] = width;
    heights[0] = height;
    for (int i = 0; i < width * height * channels; i++) {
        images[0][i] = (float)img[i] / 255.0f;
    }
    stbi_image_free(img);

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
        printf("Epoch %d/%d for %s\n", epoch + 1, EPOCHS, image_path);
        fprintf(debug_fp, "Epoch %d for %s\n", epoch + 1, image_path);
        train_epoch(epoch, images, widths, heights, num_images, channels, kernels, kernels2, debug_fp, loss_fp);
    }

    FILE *kernel_fp = fopen(output_file, "wb");
    if (!kernel_fp) {
        fprintf(stderr, "Error: Failed to open %s\n", output_file);
        free_dataset(&images, &widths, &heights, num_images);
        return;
    }
    fwrite(kernels, sizeof(float), 3 * 3 * 3, kernel_fp);
    fwrite(kernels2, sizeof(float), 3 * 3 * 3, kernel_fp);
    fclose(kernel_fp);

    free_dataset(&images, &widths, &heights, num_images);
}

// Main training function for multiple images
void train_model(const char *dataset_path, char **output_files, int num_outputs) {
    FILE *debug_fp = fopen("loss_debug.txt", "a");
    FILE *loss_fp = fopen("pretrain_loss.txt", "w");
    if (!debug_fp || !loss_fp) {
        if (!debug_fp) fprintf(stderr, "Error: Failed to open loss_debug.txt\n");
        if (!loss_fp) fprintf(stderr, "Error: Failed to open pretrain_loss.txt\n");
        if (debug_fp) fclose(debug_fp);
        if (loss_fp) fclose(loss_fp);
        return;
    }

    DIR *dir = opendir(dataset_path);
    if (!dir) {
        fprintf(debug_fp, "Error: Cannot open directory %s\n", dataset_path);
        fclose(debug_fp);
        fclose(loss_fp);
        return;
    }

    struct dirent *entry;
    int img_index = 0;
    while ((entry = readdir(dir)) != NULL && img_index < num_outputs) {
        if (entry->d_type != DT_REG) continue;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dataset_path, entry->d_name);
        if (strstr(filepath, ".jpg") || strstr(filepath, ".png")) {
            fprintf(debug_fp, "Training kernel %d for %s\n", img_index, filepath);
            train_model_for_image(filepath, output_files[img_index], debug_fp, loss_fp);
            img_index++;
        }
    }
    closedir(dir);

    if (img_index < num_outputs) {
        fprintf(debug_fp, "Warning: Only %d images found, but %d kernel files requested\n", img_index, num_outputs);
    }

    fclose(debug_fp);
    fclose(loss_fp);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <dataset_path> <output_file1> [<output_file2> ...]\n", argv[0]);
        return 1;
    }

    int num_outputs = argc - 2;
    char **output_files = (char **)malloc(num_outputs * sizeof(char *));
    for (int i = 0; i < num_outputs; i++) {
        output_files[i] = argv[i + 2];
    }

    train_model(argv[1], output_files, num_outputs);

    free(output_files);
    return 0;
}
