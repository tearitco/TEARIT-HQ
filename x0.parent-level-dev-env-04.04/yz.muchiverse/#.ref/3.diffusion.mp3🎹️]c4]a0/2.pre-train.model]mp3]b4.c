#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <libgen.h>

#define BATCH_SIZE 4
#define EPOCHS 100
#define LEARNING_RATE 0.001f
#define WEIGHT_DECAY 0.001f
#define NUM_EPOCHS 50
#define MAX_AUDIOS 1000
#define MAX_AUDIO_LENGTH 44100 // 1 second at 44.1kHz
#define MAX_BUFFER_SIZE (MAX_AUDIO_LENGTH)

// Escape filepath for FFmpeg command
void escape_filepath(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    // Normalize path to remove double slashes
    char temp[PATH_MAX];
    strncpy(temp, input, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';
    for (size_t i = 0; temp[i]; i++) {
        if (temp[i] == '/' && temp[i + 1] == '/') {
            continue; // Skip extra slashes
        }
        if (j >= output_size - 1) break;
        if (temp[i] == ' ' || temp[i] == '"' || temp[i] == '\\' || temp[i] == '(' || temp[i] == ')') {
            if (j < output_size - 2) output[j++] = '\\';
        }
        output[j++] = temp[i];
    }
    output[j] = '\0';
}

// 1D Convolution function for audio
void convolve(float *input, float *output, int length, int channels, float kernels[3][9], int use_identity, FILE *log_fp) {
    if (!input || !output || length < 9 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid convolve input/output or length (%d, %d channels)\n", length, channels);
        return;
    }
    for (int c = 0; c < channels; c++) {
        for (int x = 4; x < length - 4; x++) {
            float sum = 0.0f;
            if (use_identity && c == 0) {
                sum = input[x * channels + c];
            } else {
                for (int kx = -4; kx <= 4; kx++) {
                    int idx = (x + kx) * channels + c;
                    if (idx >= 0 && idx < length * channels) {
                        sum += input[idx] * kernels[c][kx + 4];
                    }
                }
            }
            output[x * channels + c] = fmaxf(-1.0f, fminf(sum, 1.0f));
        }
    }
    fprintf(log_fp, "Convolution completed for %d samples, %d channels\n", length, channels);
}

// Backpropagation for 1D convolution
void convolve_backward(float *input, float *grad_output, float *grad_kernels, int length, int channels, FILE *log_fp) {
    if (!input || !grad_output || !grad_kernels || length < 9 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid convolve_backward input/output or length (%d, %d channels)\n", length, channels);
        return;
    }
    for (int c = 0; c < channels; c++) {
        for (int x = 4; x < length - 4; x++) {
            for (int kx = -4; kx <= 4; kx++) {
                int kidx = c * 9 + (kx + 4);
                int idx = (x + kx) * channels + c;
                if (idx >= 0 && idx < length * channels) {
                    grad_kernels[kidx] += grad_output[x * channels + c] * input[idx];
                }
            }
        }
    }
    for (int i = 0; i < 3 * 9; i++) {
        grad_kernels[i] = fmaxf(-10.0f, fminf(grad_kernels[i], 10.0f));
    }
    fprintf(log_fp, "Convolution backward completed\n");
}

// Group normalization for audio
void group_norm(float *input, int length, int channels, FILE *log_fp) {
    if (!input || length < 1 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid group_norm input or length (%d, %d channels)\n", length, channels);
        return;
    }
    for (int c = 0; c < channels; c++) {
        float mean = 0.0f, variance = 0.0f;
        int count = length;
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
    fprintf(log_fp, "Group normalization completed\n");
}

// Max pooling for audio (1D)
void max_pool(float *input, float *output, int length, int channels, FILE *log_fp) {
    if (!input || !output || length < 2 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid max_pool input/output or length (%d, %d channels)\n", length, channels);
        return;
    }
    int out_length = length / 2;
    if (out_length < 1) {
        fprintf(log_fp, "Error: Output length too small (%d)\n", out_length);
        return;
    }
    for (int x = 0; x < out_length; x++) {
        for (int c = 0; c < channels; c++) {
            float max_val = -INFINITY;
            for (int dx = 0; dx < 2; dx++) {
                int idx = (x * 2 + dx) * channels + c;
                if (idx >= 0 && idx < length * channels) {
                    if (input[idx] > max_val) max_val = input[idx];
                }
            }
            output[x * channels + c] = max_val;
        }
    }
    fprintf(log_fp, "Max pooling completed\n");
}

// Upsampling for audio (1D)
void upsample(float *input, float *output, int length, int channels, FILE *log_fp) {
    if (!input || !output || length < 1 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid upsample input/output or length (%d, %d channels)\n", length, channels);
        return;
    }
    int out_length = length * 2;
    if (out_length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Upsampled length too large (%d)\n", out_length);
        return;
    }
    for (int x = 0; x < out_length; x++) {
        for (int c = 0; c < channels; c++) {
            int in_x = x / 2;
            output[x * channels + c] = (in_x < length) ? input[in_x * channels + c] : 0.0f;
        }
    }
    fprintf(log_fp, "Upsampling completed\n");
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
    fprintf(log_fp, "Noise added with timestep %f\n", t);
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

// Load audio using FFmpeg
int load_audio(const char *filepath, float **audio_data, int *length, int channels, FILE *log_fp) {
    char escaped_filepath[PATH_MAX];
    escape_filepath(filepath, escaped_filepath, sizeof(escaped_filepath));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -i \"%s\" -f f32le -ar 44100 -ac %d -loglevel quiet temp_audio.raw 2> ffmpeg_err.txt", escaped_filepath, channels);
    fprintf(log_fp, "Executing FFmpeg: %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(log_fp, "Warning: FFmpeg failed to process %s (exit code %d)\n", filepath, ret);
        FILE *err_fp = fopen("ffmpeg_err.txt", "r");
        if (err_fp) {
            char err_buf[1024];
            while (fgets(err_buf, sizeof(err_buf), err_fp)) {
                fprintf(log_fp, "FFmpeg error: %s", err_buf);
            }
            fclose(err_fp);
        }
        remove("ffmpeg_err.txt");
        return 0;
    }

    FILE *raw_fp = fopen("temp_audio.raw", "rb");
    if (!raw_fp) {
        fprintf(log_fp, "Warning: Failed to open temp_audio.raw for %s\n", filepath);
        return 0;
    }

    fseek(raw_fp, 0, SEEK_END);
    size_t file_size = ftell(raw_fp);
    fseek(raw_fp, 0, SEEK_SET);
    if (file_size < sizeof(float) * 9 * channels) {
        fprintf(log_fp, "Warning: Audio %s too small (%zu bytes), skipping\n", filepath, file_size);
        fclose(raw_fp);
        remove("temp_audio.raw");
        return 0;
    }
    size_t total_samples = file_size / sizeof(float);
    *length = total_samples / channels;

    fprintf(log_fp, "Audio %s: total_samples=%zu, length=%d\n", filepath, total_samples, *length);
    if (*length < 9) {
        fprintf(log_fp, "Warning: Audio %s too short (%d samples), skipping\n", filepath, *length);
        fclose(raw_fp);
        remove("temp_audio.raw");
        return 0;
    }
    if (*length > MAX_AUDIO_LENGTH) {
        *length = MAX_AUDIO_LENGTH;
        fprintf(log_fp, "Warning: Audio %s truncated to %d samples\n", filepath, *length);
    }

    *audio_data = (float *)calloc(*length * channels, sizeof(float));
    if (!*audio_data) {
        fprintf(log_fp, "Error: Memory allocation failed for audio %s\n", filepath);
        fclose(raw_fp);
        remove("temp_audio.raw");
        return 0;
    }

    size_t read_samples = fread(*audio_data, sizeof(float), *length * channels, raw_fp);
    if (read_samples != *length * channels) {
        fprintf(log_fp, "Warning: Incomplete read for %s (%zu samples read, %d expected)\n", filepath, read_samples, *length * channels);
        free(*audio_data);
        fclose(raw_fp);
        remove("temp_audio.raw");
        return 0;
    }

    fclose(raw_fp);
    remove("temp_audio.raw");
    fprintf(log_fp, "Successfully loaded audio %s (%d samples, %d channels)\n", filepath, *length, channels);
    return 1;
}

// Load dataset from directory (MP3 files)
int load_dataset(const char *dataset_path, float ***audios, int **lengths, int *num_audios, int channels, FILE *log_fp) {
    *audios = NULL;
    *lengths = NULL;
    *num_audios = 0;
    DIR *dir = opendir(dataset_path);
    if (!dir) {
        fprintf(log_fp, "Error: Cannot open directory %s\n", dataset_path);
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *num_audios < MAX_AUDIOS) {
        if (entry->d_type != DT_REG) continue;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dataset_path, entry->d_name);
        if (!strstr(filepath, ".mp3")) continue;

        float *audio_data;
        int length;
        if (!load_audio(filepath, &audio_data, &length, channels, log_fp)) {
            continue;
        }

        *audios = realloc(*audios, (*num_audios + 1) * sizeof(float *));
        *lengths = realloc(*lengths, (*num_audios + 1) * sizeof(int));
        if (!*audios || !*lengths) {
            fprintf(log_fp, "Error: Memory allocation failed for dataset\n");
            free(audio_data);
            free(*audios);
            free(*lengths);
            *num_audios = 0;
            closedir(dir);
            return 0;
        }

        (*audios)[*num_audios] = audio_data;
        (*lengths)[*num_audios] = length;
        (*num_audios)++;
        fprintf(log_fp, "Loaded audio %s (%d samples)\n", filepath, length);
    }

    closedir(dir);
    fprintf(log_fp, "Loaded %d audios from %s\n", *num_audios, dataset_path);
    return *num_audios > 0;
}

// Free dataset
void free_dataset(float ***audios, int **lengths, int num_audios) {
    if (!audios || !*audios || !lengths || !*lengths) return;
    for (int i = 0; i < num_audios; i++) {
        free((*audios)[i]);
    }
    free(*audios);
    *audios = NULL;
    free(*lengths);
    *lengths = NULL;
}

// Forward pass for a single audio
void forward_pass(float *input, float *output, int length, int channels, float kernels[3][9], float kernels2[3][9], FILE *log_fp) {
    if (!input || !output || length < 9 || channels < 1 || length > MAX_AUDIO_LENGTH) {
        fprintf(log_fp, "Error: Invalid forward_pass input/output or length (%d, %d channels)\n", length, channels);
        return;
    }
    static float conv1[MAX_BUFFER_SIZE];
    static float pool1[MAX_BUFFER_SIZE / 2];
    static float conv2[MAX_BUFFER_SIZE / 2];
    static float upsampled_conv2[MAX_BUFFER_SIZE];
    memset(conv1, 0, MAX_BUFFER_SIZE * sizeof(float));
    memset(pool1, 0, (MAX_BUFFER_SIZE / 2) * sizeof(float));
    memset(conv2, 0, (MAX_BUFFER_SIZE / 2) * sizeof(float));
    memset(upsampled_conv2, 0, MAX_BUFFER_SIZE * sizeof(float));

    convolve(input, conv1, length, channels, kernels, 1, log_fp);
    group_norm(conv1, length, channels, log_fp);
    max_pool(conv1, pool1, length, channels, log_fp);
    convolve(pool1, conv2, length / 2, channels, kernels2, 0, log_fp);
    group_norm(conv2, length / 2, channels, log_fp);
    upsample(conv2, upsampled_conv2, length / 2, channels, log_fp);
    memcpy(output, upsampled_conv2, length * channels * sizeof(float));
}

// Train one epoch
void train_epoch(int epoch, float **audios, int *lengths, int num_audios, int channels, float kernels[3][9], float kernels2[3][9], FILE *debug_fp, FILE *loss_fp) {
    static float noisy_input[MAX_BUFFER_SIZE];
    static float noise[MAX_BUFFER_SIZE];
    static float pred_noise[MAX_BUFFER_SIZE];
    static float grad_output[MAX_BUFFER_SIZE];
    static float temp_pool1[MAX_BUFFER_SIZE / 2];
    static float grad_kernels[3 * 9];
    static float grad_kernels2[3 * 9];

    for (int i = 0; i < num_audios; i++) {
        int length = lengths[i];
        int size = length * channels;
        fprintf(debug_fp, "Processing audio %d (%d samples, %d channels)\n", i, length, channels);
        if (length < 9 || length > MAX_AUDIO_LENGTH || size > MAX_BUFFER_SIZE) {
            fprintf(debug_fp, "Error: Audio %d invalid length (%d, size %d), skipping\n", i, length, size);
            continue;
        }

        memset(noisy_input, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(noise, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(pred_noise, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(grad_output, 0, MAX_BUFFER_SIZE * sizeof(float));
        memset(temp_pool1, 0, (MAX_BUFFER_SIZE / 2) * sizeof(float));
        memset(grad_kernels, 0, 3 * 9 * sizeof(float));
        memset(grad_kernels2, 0, 3 * 9 * sizeof(float));

        float timestep = 0.001f + (0.5f - 0.001f) * (rand() % NUM_EPOCHS) / (NUM_EPOCHS - 1);
        add_noise(audios[i], noisy_input, noise, size, timestep, debug_fp);
        forward_pass(noisy_input, pred_noise, length, channels, kernels, kernels2, debug_fp);

        float loss = compute_loss(pred_noise, noise, size, debug_fp);
        if (loss == 0.0f) {
            fprintf(debug_fp, "Error: Loss computation failed for audio %d\n", i);
            continue;
        }
        fprintf(loss_fp, "Epoch %d, Audio %d: Loss = %f\n", epoch + 1, i, loss);

        for (int j = 0; j < size; j++) {
            float pred_clipped = fmaxf(-0.05f, fminf(pred_noise[j], 0.05f));
            grad_output[j] = 2.0f * (pred_clipped - noise[j]) / size;
        }

        convolve(noisy_input, temp_pool1, length, channels, kernels, 1, debug_fp);
        group_norm(temp_pool1, length, channels, debug_fp);
        max_pool(temp_pool1, temp_pool1, length, channels, debug_fp);
        convolve_backward(temp_pool1, grad_output, grad_kernels2, length / 2, channels, debug_fp);
        convolve_backward(noisy_input, grad_output, grad_kernels, length, channels, debug_fp);

        for (int c = 0; c < channels; c++) {
            for (int kx = 0; kx < 9; kx++) {
                int idx = c * 9 + kx;
                kernels[c][kx] -= LEARNING_RATE * (grad_kernels[idx] + WEIGHT_DECAY * kernels[c][kx]);
                kernels2[c][kx] -= LEARNING_RATE * (grad_kernels2[idx] + WEIGHT_DECAY * kernels2[c][kx]);
            }
        }

        float grad_mag = compute_grad_magnitude(grad_kernels, 3 * 9, debug_fp);
        float grad_mag2 = compute_grad_magnitude(grad_kernels2, 3 * 9, debug_fp);
        fprintf(debug_fp, "Audio %d: Loss = %f, GradMag1 = %f, GradMag2 = %f\n", i, loss, grad_mag, grad_mag2);
        log_noise_stats(noise, size, debug_fp, "dataset_audio", "target_noise");
        log_noise_stats(pred_noise, size, debug_fp, "dataset_audio", "pred_noise");
        fflush(debug_fp);
    }
}

// Train model for a single audio and output kernel file
void train_model_for_audio(const char *audio_path, const char *output_file, FILE *debug_fp, FILE *loss_fp) {
    srand(time(NULL));

    float *audio_data;
    int length;
    int channels = 1; // Mono audio
    if (!load_audio(audio_path, &audio_data, &length, channels, debug_fp)) {
        fprintf(debug_fp, "Error: Failed to load audio %s\n", audio_path);
        return;
    }

    float **audios = (float **)malloc(sizeof(float *));
    int *lengths = (int *)malloc(sizeof(int));
    int num_audios = 1;
    if (!audios || !lengths) {
        fprintf(debug_fp, "Error: Memory allocation failed for dataset\n");
        free(audio_data);
        free(audios);
        free(lengths);
        return;
    }

    audios[0] = audio_data;
    lengths[0] = length;

    float kernels[3][9];
    float kernels2[3][9];
    for (int c = 0; c < 3; c++) {
        for (int kx = 0; kx < 9; kx++) {
            kernels[c][kx] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
            kernels2[c][kx] = (rand() / (float)RAND_MAX - 0.5f) * 0.1f;
        }
    }

    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        printf("Epoch %d/%d for %s\n", epoch + 1, EPOCHS, audio_path);
        fprintf(debug_fp, "Epoch %d for %s\n", epoch + 1, audio_path);
        train_epoch(epoch, audios, lengths, num_audios, channels, kernels, kernels2, debug_fp, loss_fp);
    }

    FILE *kernel_fp = fopen(output_file, "wb");
    if (!kernel_fp) {
        fprintf(stderr, "Error: Failed to open %s\n", output_file);
        free_dataset(&audios, &lengths, num_audios);
        return;
    }
    fwrite(kernels, sizeof(float), 3 * 9, kernel_fp);
    fwrite(kernels2, sizeof(float), 3 * 9, kernel_fp);
    fclose(kernel_fp);

    free_dataset(&audios, &lengths, num_audios);
}

// Main training function for multiple audios
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
    int audio_index = 0;
    while ((entry = readdir(dir)) != NULL && audio_index < num_outputs) {
        if (entry->d_type != DT_REG) continue;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dataset_path, entry->d_name);
        if (strstr(filepath, ".mp3")) {
            fprintf(debug_fp, "Training kernel %d for %s\n", audio_index, filepath);
            train_model_for_audio(filepath, output_files[audio_index], debug_fp, loss_fp);
            audio_index++;
        }
    }
    closedir(dir);

    if (audio_index < num_outputs) {
        fprintf(debug_fp, "Warning: Only %d audios found, but %d kernel files requested\n", audio_index, num_outputs);
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
    if (!output_files) {
        fprintf(stderr, "Error: Memory allocation failed for output_files\n");
        return 1;
    }
    for (int i = 0; i < num_outputs; i++) {
        output_files[i] = argv[i + 2];
    }

    train_model(argv[1], output_files, num_outputs);

    free(output_files);
    return 0;
}
