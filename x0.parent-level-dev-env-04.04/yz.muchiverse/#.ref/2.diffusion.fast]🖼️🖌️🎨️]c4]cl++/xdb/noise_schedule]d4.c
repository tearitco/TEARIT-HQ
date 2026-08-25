#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FILES 1000
#define MAX_PATH 256
#define NUM_TIMESTEPS 50

// DDIM noise schedule (simplified)
void get_beta_schedule(float *betas, int num_timesteps) {
    float beta_start = 0.0001f;
    float beta_end = 0.02f;
    for (int t = 0; t < num_timesteps; t++) {
        betas[t] = beta_start + (beta_end - beta_start) * t / (num_timesteps - 1);
        if (betas[t] < 0.0001f) betas[t] = 0.0001f;
        if (betas[t] > 0.9999f) betas[t] = 0.9999f;
    }
}

// Compute edge map for conditioning
void compute_edge_map(float *input, float *edge_map, int width, int height, int channels) {
    float kernel[3][3] = {{-1.0f, -1.0f, -1.0f}, {-1.0f, 8.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}};
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            float edge_val = 0.0f;
            for (int c = 0; c < channels; c++) {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int idx = ((y + ky) * width + (x + kx)) * channels + c;
                        sum += input[idx] * kernel[ky + 1][kx + 1];
                    }
                }
                edge_val += fabsf(sum);
            }
            edge_map[y * width + x] = edge_val / (3.0f * 8.0f); // Normalize
        }
    }
}

// Contrast adjustment
void adjust_contrast(float *input, int size, float factor) {
    float mean = 0.0f;
    for (int i = 0; i < size; i++) mean += input[i];
    mean /= size;
    for (int i = 0; i < size; i++) {
        input[i] = mean + (input[i] - mean) * factor;
        if (input[i] < 0.0f) input[i] = 0.0f;
        if (input[i] > 1.0f) input[i] = 1.0f;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s <input_file_list> <output_image> <width> <height> <original_image>\n", argv[0]);
        return 1;
    }

    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    int channels = 3;
    int size = width * height * channels;

    // Load original image for blending and edge map
    float *original = (float *)malloc(size * sizeof(float));
    if (!original) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    unsigned char *orig_img = stbi_load(argv[5], &width, &height, &channels, 3);
    if (!orig_img) {
        fprintf(stderr, "Error: Failed to load %s\n", argv[5]);
        free(original);
        return 1;
    }
    for (int i = 0; i < size; i++) original[i] = (float)orig_img[i] / 255.0f;

    // Compute edge map
    float *edge_map = (float *)calloc(width * height, sizeof(float));
    if (!edge_map) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(original); stbi_image_free(orig_img);
        return 1;
    }
    compute_edge_map(original, edge_map, width, height, channels);
    stbi_image_free(orig_img);

    // Read list of input files
    char *input_files[MAX_FILES];
    int file_count = 0;
    char line[MAX_PATH];
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s\n", argv[1]);
        free(original); free(edge_map);
        return 1;
    }
    while (fgets(line, MAX_PATH, fp) && file_count < MAX_FILES) {
        line[strcspn(line, "\n")] = 0;
        input_files[file_count] = strdup(line);
        if (!input_files[file_count]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            fclose(fp); free(original); free(edge_map);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            return 1;
        }
        file_count++;
    }
    fclose(fp);

    if (file_count == 0) {
        fprintf(stderr, "Error: No input files found\n");
        free(original); free(edge_map);
        return 1;
    }

    // Load noise predictions
    float *noise_preds[MAX_FILES];
    for (int i = 0; i < file_count; i++) {
        noise_preds[i] = (float *)malloc(size * sizeof(float));
        if (!noise_preds[i]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(original); free(edge_map);
            for (int j = 0; j < i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            return 1;
        }
        FILE *in_fp = fopen(input_files[i], "rb");
        if (!in_fp) {
            fprintf(stderr, "Error: Failed to open %s\n", input_files[i]);
            free(original); free(edge_map);
            for (int j = 0; j <= i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            return 1;
        }
        fread(noise_preds[i], sizeof(float), size, in_fp);
        fclose(in_fp);
    }

    // Initialize noisy image (average of inputs)
    float *xt = (float *)calloc(size, sizeof(float));
    if (!xt) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(original); free(edge_map);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }
    for (int i = 0; i < file_count; i++) {
        for (int j = 0; j < size; j++) {
            xt[j] += noise_preds[i][j] / file_count;
        }
    }

    // DDIM noise schedule
    float betas[NUM_TIMESTEPS];
    get_beta_schedule(betas, NUM_TIMESTEPS);
    float alphas[NUM_TIMESTEPS];
    float alpha_prods[NUM_TIMESTEPS];
    float alpha_prod = 1.0f;
    for (int t = 0; t < NUM_TIMESTEPS; t++) {
        alphas[t] = 1.0f - betas[t];
        alpha_prod *= alphas[t];
        alpha_prods[t] = alpha_prod;
    }

    // DDIM denoising process
    for (int t = NUM_TIMESTEPS - 1; t >= 0; t--) {
        float *xt_minus_1 = (float *)calloc(size, sizeof(float));
        if (!xt_minus_1) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(xt); free(original); free(edge_map);
            for (int i = 0; i < file_count; i++) free(noise_preds[i]);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            return 1;
        }

        // DDIM update: x_{t-1} = sqrt(alpha_{t-1}) * f_theta + sqrt(1-alpha_{t-1}) * eps_theta
        float sqrt_alpha_t = sqrtf(alphas[t]);
        float sqrt_alpha_t_minus_1 = t > 0 ? sqrtf(alphas[t - 1]) : 1.0f;
        float sqrt_one_minus_alpha_t = sqrtf(1.0f - alphas[t]);
        float sqrt_one_minus_alpha_t_minus_1 = t > 0 ? sqrtf(1.0f - alphas[t - 1]) : 0.0f;
        for (int i = 0; i < file_count; i++) {
            for (int j = 0; j < size; j++) {
                float eps_theta = noise_preds[i][j] / file_count;
                float x0_pred = (xt[j] - sqrt_one_minus_alpha_t * eps_theta) / sqrt_alpha_t;
                float weight = edge_map[(j / channels) % (width * height)] + 0.1f; // Edge-based weighting
                xt_minus_1[j] += weight * (sqrt_alpha_t_minus_1 * x0_pred + sqrt_one_minus_alpha_t_minus_1 * eps_theta);
            }
        }
        for (int j = 0; j < size; j++) {
            xt_minus_1[j] /= file_count; // Average predictions
        }

        free(xt);
        xt = xt_minus_1;
    }

    // Blend with original image
    for (int i = 0; i < size; i++) {
        xt[i] = xt[i] * 0.8f + original[i] * 0.2f;
    }

    // Normalize xt to [0, 1]
    float min_val = xt[0], max_val = xt[0];
    for (int i = 1; i < size; i++) {
        if (xt[i] < min_val) min_val = xt[i];
        if (xt[i] > max_val) max_val = xt[i];
    }
    if (max_val > min_val) {
        for (int i = 0; i < size; i++) {
            xt[i] = (xt[i] - min_val) / (max_val - min_val);
        }
    }

    // Adjust contrast
    adjust_contrast(xt, size, 1.2f);

    // Convert to unsigned char
    unsigned char *final_pixels = (unsigned char *)malloc(size);
    if (!final_pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(xt); free(original); free(edge_map);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }
    for (int i = 0; i < size; i++) {
        float val = xt[i] * 255.0f;
        final_pixels[i] = (unsigned char)(val + 0.5f);
    }

    // Save output image
    if (!stbi_write_jpg(argv[2], width, height, channels, final_pixels, 90)) {
        fprintf(stderr, "Error: Failed to write %s\n", argv[2]);
        free(xt); free(original); free(edge_map); free(final_pixels);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    } else {
        printf("Diffusion output saved as %s\n", argv[2]);
    }

    // Cleanup
    free(xt);
    free(original);
    free(edge_map);
    free(final_pixels);
    for (int i = 0; i < file_count; i++) free(noise_preds[i]);
    for (int i = 0; i < file_count; i++) free(input_files[i]);
    return 0;
}
