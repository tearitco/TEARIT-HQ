#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_FILES 1000
#define MAX_PATH 256
#define NUM_TIMESTEPS 50
#define MAX_AUDIO_LENGTH 44100
#define CHANNELS 1

// DDIM noise schedule
void get_beta_schedule(float *betas, int num_timesteps) {
    float beta_start = 0.0001f;
    float beta_end = 0.02f;
    for (int t = 0; t < num_timesteps; t++) {
        betas[t] = beta_start + (beta_end - beta_start) * t / (num_timesteps - 1);
        if (betas[t] < 0.0001f) betas[t] = 0.0001f;
        if (betas[t] > 0.9999f) betas[t] = 0.9999f;
    }
}

// Compute spectral envelope for conditioning (simple energy-based)
void compute_spectral_envelope(float *input, float *envelope, int length, int channels, FILE *log_fp) {
    if (!input || !envelope || length < 1 || channels != CHANNELS) {
        fprintf(log_fp, "Error: Invalid spectral_envelope input or length (%d, %d channels)\n", length, channels);
        return;
    }
    int window_size = 512; // Small window for coarse energy
    for (int i = 0; i < length; i += window_size) {
        float energy = 0.0f;
        int count = window_size;
        if (i + window_size > length) count = length - i;
        for (int j = 0; j < count; j++) {
            energy += input[(i + j) * channels] * input[(i + j) * channels];
        }
        energy = sqrtf(energy / count);
        for (int j = 0; j < count; j++) {
            if (i + j < length) envelope[i + j] = energy;
        }
    }
    fprintf(log_fp, "Spectral envelope computed\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file_list> <output_audio> <length> <original_audio>\n", argv[0]);
        return 1;
    }

    FILE *log_fp = fopen("noise_schedule_log.txt", "a");
    if (!log_fp) {
        fprintf(stderr, "Error: Cannot open noise_schedule_log.txt\n");
        return 1;
    }

    int length = atoi(argv[3]);
    int channels = CHANNELS;
    int size = length * channels;
    if (length < 9 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid audio length %d\n", length);
        fclose(log_fp);
        return 1;
    }

    // Load original audio
    float *original = (float *)malloc(size * sizeof(float));
    if (!original) {
        fprintf(log_fp, "Error: Memory allocation failed for original\n");
        fclose(log_fp);
        return 1;
    }
    FILE *orig_fp = fopen(argv[4], "rb");
    if (!orig_fp) {
        fprintf(log_fp, "Error: Failed to open original audio %s\n", argv[4]);
        free(original);
        fclose(log_fp);
        return 1;
    }
    if (fread(original, sizeof(float), size, orig_fp) != size) {
        fprintf(log_fp, "Error: Failed to read %s\n", argv[4]);
        free(original);
        fclose(orig_fp);
        fclose(log_fp);
        return 1;
    }
    fclose(orig_fp);

    // Compute spectral envelope
    float *envelope = (float *)calloc(length, sizeof(float));
    if (!envelope) {
        fprintf(log_fp, "Error: Memory allocation failed for envelope\n");
        free(original);
        fclose(log_fp);
        return 1;
    }
    compute_spectral_envelope(original, envelope, length, channels, log_fp);

    // Read input files
    char *input_files[MAX_FILES];
    int file_count = 0;
    char line[MAX_PATH];
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(log_fp, "Error: Failed to open %s\n", argv[1]);
        free(original);
        free(envelope);
        fclose(log_fp);
        return 1;
    }
    while (fgets(line, MAX_PATH, fp) && file_count < MAX_FILES) {
        line[strcspn(line, "\n")] = 0;
        input_files[file_count] = strdup(line);
        if (!input_files[file_count]) {
            fprintf(log_fp, "Error: Memory allocation failed for input file\n");
            fclose(fp);
            free(original);
            free(envelope);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            fclose(log_fp);
            return 1;
        }
        file_count++;
    }
    fclose(fp);

    if (file_count == 0) {
        fprintf(log_fp, "Error: No input files found\n");
        free(original);
        free(envelope);
        fclose(log_fp);
        return 1;
    }

    // Load noise predictions
    float *noise_preds[MAX_FILES];
    for (int i = 0; i < file_count; i++) {
        noise_preds[i] = (float *)malloc(size * sizeof(float));
        if (!noise_preds[i]) {
            fprintf(log_fp, "Error: Memory allocation failed for noise_pred\n");
            free(original);
            free(envelope);
            for (int j = 0; j < i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            fclose(log_fp);
            return 1;
        }
        FILE *in_fp = fopen(input_files[i], "rb");
        if (!in_fp) {
            fprintf(log_fp, "Error: Failed to open %s\n", input_files[i]);
            free(original);
            free(envelope);
            for (int j = 0; j <= i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            fclose(log_fp);
            return 1;
        }
        if (fread(noise_preds[i], sizeof(float), size, in_fp) != size) {
            fprintf(log_fp, "Error: Failed to read %s\n", input_files[i]);
            free(original);
            free(envelope);
            for (int j = 0; j <= i; j++) free(noise_preds[j]);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            fclose(in_fp);
            fclose(log_fp);
            return 1;
        }
        fclose(in_fp);
    }

    // Initialize noisy audio
    float *xt = (float *)calloc(size, sizeof(float));
    if (!xt) {
        fprintf(log_fp, "Error: Memory allocation failed for xt\n");
        free(original);
        free(envelope);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        fclose(log_fp);
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

    // DDIM denoising
    for (int t = NUM_TIMESTEPS - 1; t >= 0; t--) {
        float *xt_minus_1 = (float *)calloc(size, sizeof(float));
        if (!xt_minus_1) {
            fprintf(log_fp, "Error: Memory allocation failed for xt_minus_1\n");
            free(xt);
            free(original);
            free(envelope);
            for (int i = 0; i < file_count; i++) free(noise_preds[i]);
            for (int i = 0; i < file_count; i++) free(input_files[i]);
            fclose(log_fp);
            return 1;
        }

        float sqrt_alpha_t = sqrtf(alphas[t]);
        float sqrt_alpha_t_minus_1 = t > 0 ? sqrtf(alphas[t - 1]) : 1.0f;
        float sqrt_one_minus_alpha_t = sqrtf(1.0f - alphas[t]);
        float sqrt_one_minus_alpha_t_minus_1 = t > 0 ? sqrtf(1.0f - alphas[t - 1]) : 0.0f;
        for (int i = 0; i < file_count; i++) {
            for (int j = 0; j < size; j++) {
                float eps_theta = noise_preds[i][j];
                float x0_pred = (xt[j] - sqrt_one_minus_alpha_t * eps_theta) / sqrt_alpha_t;
                float weight = 0.5f + envelope[j / channels] * 0.1f; // Light envelope influence
                xt_minus_1[j] += (sqrt_alpha_t_minus_1 * x0_pred + sqrt_one_minus_alpha_t_minus_1 * eps_theta) / file_count;
            }
        }

        free(xt);
        xt = xt_minus_1;
    }

    // Blend with original
    for (int i = 0; i < size; i++) {
        xt[i] = xt[i] * 0.9f + original[i] * 0.1f;
    }

    // Normalize
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

    // Save output
    FILE *out_fp = fopen(argv[2], "wb");
    if (!out_fp) {
        fprintf(log_fp, "Error: Failed to open %s\n", argv[2]);
        free(xt);
        free(original);
        free(envelope);
        for (int i = 0; i < file_count; i++) free(noise_preds[i]);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        fclose(log_fp);
        return 1;
    }
    fwrite(xt, sizeof(float), size, out_fp);
    fclose(out_fp);
    printf("Diffusion output saved as %s\n", argv[2]);
    fprintf(log_fp, "Diffusion output saved as %s\n", argv[2]);

    // Cleanup
    free(xt);
    free(original);
    free(envelope);
    for (int i = 0; i < file_count; i++) free(noise_preds[i]);
    for (int i = 0; i < file_count; i++) free(input_files[i]);
    fclose(log_fp);
    return 0;
}
