#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FILES 1000
#define MAX_PATH 256
#define NUM_TIMESTEPS 10

// Linear noise schedule
void get_beta_schedule(float *betas, int num_timesteps) {
    float start = 0.0001f;
    float end = 0.02f;
    for (int t = 0; t < num_timesteps; t++) {
        betas[t] = start + (end - start) * t / (num_timesteps - 1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file_list> <output_image> <width> <height>\n", argv[0]);
        return 1;
    }

    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    int channels = 3;
    int size = width * height * channels;

    // Read list of input files
    char *input_files[MAX_FILES];
    int file_count = 0;
    char line[MAX_PATH];
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s\n", argv[1]);
        return 1;
    }
    while (fgets(line, MAX_PATH, fp) && file_count < MAX_FILES) {
        line[strcspn(line, "\n")] = 0;
        input_files[file_count] = strdup(line);
        if (!input_files[file_count]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            fclose(fp);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            return 1;
        }
        file_count++;
    }
    fclose(fp);

    if (file_count == 0) {
        fprintf(stderr, "Error: No input files found\n");
        return 1;
    }

    // Load noise predictions
    float *noise_preds[MAX_FILES];
    for (int i = 0; i < file_count; i++) {
        noise_preds[i] = (float *)malloc(size * sizeof(float));
        if (!noise_preds[i]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            for (int j = 0; j < i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            return 1;
        }
        FILE *in_fp = fopen(input_files[i], "rb");
        if (!in_fp) {
            fprintf(stderr, "Error: Failed to open %s\n", input_files[i]);
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
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }
    for (int i = 0; i < file_count; i++) {
        for (int j = 0; j < size; j++) {
            xt[j] += noise_preds[i][j] / file_count;
        }
    }

    // Noise schedule
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

    // Denoising process
    for (int t = NUM_TIMESTEPS - 1; t >= 0; t--) {
        float *xt_minus_1 = (float *)calloc(size, sizeof(float));
        if (!xt_minus_1) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(xt);
            for (int i = 0; i < file_count; i++) free(noise_preds[i]);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            return 1;
        }

        // Simplified DDPM denoising: x_{t-1} = (x_t - sqrt(1-alpha_t)*eps_t)/sqrt(alpha_t)
        float sqrt_alpha = sqrtf(alphas[t]);
        float one_minus_alpha = 1.0f - alphas[t];
        for (int i = 0; i < file_count; i++) {
            for (int j = 0; j < size; j++) {
                xt_minus_1[j] += (xt[j] - sqrtf(one_minus_alpha) * noise_preds[i][j] / file_count) / sqrt_alpha;
            }
        }

        free(xt);
        xt = xt_minus_1;
    }

    // Convert to unsigned char
    unsigned char *final_pixels = (unsigned char *)malloc(size);
    if (!final_pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(xt);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }
    for (int i = 0; i < size; i++) {
        float val = xt[i] * 255.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;
        final_pixels[i] = (unsigned char)(val + 0.5f);
    }

    // Save output image
    if (!stbi_write_jpg(argv[2], width, height, channels, final_pixels, 90)) {
        fprintf(stderr, "Error: Failed to write %s\n", argv[2]);
        free(xt); free(final_pixels);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    } else {
        printf("Diffusion output saved as %s\n", argv[2]);
    }

    // Cleanup
    free(xt);
    free(final_pixels);
    for (int i = 0; i < file_count; i++) free(noise_preds[i]);
    for (int i = 0; i < file_count; i++) free(input_files[i]);
    return 0;
}
