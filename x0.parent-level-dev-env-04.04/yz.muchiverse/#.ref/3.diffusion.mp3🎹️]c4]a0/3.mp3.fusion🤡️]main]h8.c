#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_AUDIOS 1000
#define MAX_PATH 256
#define MAX_INPUT 256
#define MODULE_DIR "./+x/"
#define MAX_AUDIO_LENGTH 44100 // 1 second at 44.1kHz
#define CHANNELS 1 // Mono audio

void cleanup(float *audio_data[], char *filenames[], int audio_count, char temp_files[][MAX_PATH], char neural_output_files[][MAX_PATH], char kernel_files[][MAX_PATH], int selected_count, char *noise_input_file) {
    for (int i = 0; i < audio_count; i++) {
        if (audio_data[i]) free(audio_data[i]);
        if (filenames[i]) free(filenames[i]);
    }
    for (int i = 0; i < selected_count; i++) {
        if (temp_files[i][0]) remove(temp_files[i]);
        if (neural_output_files[i][0]) remove(neural_output_files[i]);
        if (kernel_files[i][0]) remove(kernel_files[i]);
    }
    if (noise_input_file && noise_input_file[0]) remove(noise_input_file);
}

float *resize_audio(float *audio, int in_length, int channels, int out_length, FILE *log_fp) {
    float *resized = (float *)malloc(out_length * channels * sizeof(float));
    if (!resized) {
        fprintf(log_fp, "Error: Memory allocation failed for resized audio\n");
        return NULL;
    }
    if (in_length >= out_length) {
        // Truncate
        memcpy(resized, audio, out_length * channels * sizeof(float));
    } else {
        // Pad with zeros
        memcpy(resized, audio, in_length * channels * sizeof(float));
        memset(resized + in_length * channels, 0, (out_length - in_length) * channels * sizeof(float));
    }
    return resized;
}

int load_audio(const char *filepath, float **audio_data, int *length, int channels, FILE *log_fp) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -i \"%s\" -f f32le -ar 44100 -ac %d -loglevel quiet temp_audio.raw", filepath, channels);
    if (system(cmd) != 0) {
        fprintf(log_fp, "Warning: FFmpeg failed to process %s\n", filepath);
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
    size_t total_samples = file_size / sizeof(float);
    *length = total_samples / channels;

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

    *audio_data = (float *)malloc(*length * channels * sizeof(float));
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
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    FILE *log_fp = fopen("main_log.txt", "a");
    if (!log_fp) {
        fprintf(stderr, "Error: Failed to open main_log.txt\n");
        return 1;
    }

    if (mkdir(MODULE_DIR, 0700) == -1 && errno != EEXIST) {
        fprintf(log_fp, "Error: Cannot create directory %s: %s\n", MODULE_DIR, strerror(errno));
        fclose(log_fp);
        return 1;
    }

    DIR *dir = opendir(argv[1]);
    if (!dir) {
        fprintf(log_fp, "Error: Cannot open directory %s: %s\n", argv[1], strerror(errno));
        fclose(log_fp);
        return 1;
    }

    float *audio_data[MAX_AUDIOS] = {0};
    int audio_lengths[MAX_AUDIOS] = {0};
    int audio_channels[MAX_AUDIOS] = {0};
    char *filenames[MAX_AUDIOS] = {0};
    int audio_count = 0;

    struct dirent *entry;
    char filepath[MAX_PATH];
    while ((entry = readdir(dir)) && audio_count < MAX_AUDIOS) {
        if (strstr(entry->d_name, ".mp3")) {
            snprintf(filepath, MAX_PATH, "%s/%s", argv[1], entry->d_name);
            int length, channels = CHANNELS;
            float *audio;
            if (!load_audio(filepath, &audio, &length, channels, log_fp)) {
                continue;
            }
            audio_data[audio_count] = audio;
            audio_lengths[audio_count] = length;
            audio_channels[audio_count] = channels;
            filenames[audio_count] = strdup(entry->d_name);
            if (!filenames[audio_count]) {
                fprintf(log_fp, "Error: Memory allocation failed for filename\n");
                cleanup(audio_data, filenames, audio_count, NULL, NULL, NULL, 0, NULL);
                closedir(dir);
                fclose(log_fp);
                return 1;
            }
            audio_count++;
        }
    }
    closedir(dir);

    if (audio_count == 0) {
        fprintf(log_fp, "Error: No valid MP3 files found in directory\n");
        fclose(log_fp);
        return 1;
    }

    printf("Available audio files:\n");
    for (int i = 0; i < audio_count; i++) {
        printf("%d: %s (%d samples)\n", i, filenames[i], audio_lengths[i]);
    }

    printf("Enter indices of audio files to blend (space-separated, e.g., '1 4 6'): ");
    char input[MAX_INPUT] = {0};
    if (!fgets(input, MAX_INPUT, stdin)) {
        fprintf(log_fp, "Error: Failed to read input\n");
        cleanup(audio_data, filenames, audio_count, NULL, NULL, NULL, 0, NULL);
        fclose(log_fp);
        return 1;
    }

    int selected_indices[MAX_AUDIOS] = {0};
    int selected_count = 0;
    char *token = strtok(input, " \n");
    while (token && selected_count < MAX_AUDIOS) {
        int idx = atoi(token);
        if (idx >= 0 && idx < audio_count) {
            selected_indices[selected_count++] = idx;
        } else {
            fprintf(log_fp, "Warning: Invalid index %d, ignoring\n", idx);
        }
        token = strtok(NULL, " \n");
    }

    if (selected_count == 0) {
        fprintf(log_fp, "Error: No valid indices selected\n");
        cleanup(audio_data, filenames, audio_count, NULL, NULL, NULL, 0, NULL);
        fclose(log_fp);
        return 1;
    }

    fprintf(log_fp, "Selected %d audio files for blending:\n", selected_count);
    for (int i = 0; i < selected_count; i++) {
        fprintf(log_fp, "  %d: %s (%d samples, %d channels)\n", selected_indices[i], filenames[selected_indices[i]],
                audio_lengths[selected_indices[i]], audio_channels[selected_indices[i]]);
    }

    int min_length = audio_lengths[selected_indices[0]];
    for (int i = 1; i < selected_count; i++) {
        int idx = selected_indices[i];
        if (audio_lengths[idx] < min_length) min_length = audio_lengths[idx];
    }
    if (min_length > MAX_AUDIO_LENGTH) min_length = MAX_AUDIO_LENGTH;

    char temp_files[MAX_AUDIOS][MAX_PATH] = {{0}};
    char neural_output_files[MAX_AUDIOS][MAX_PATH] = {{0}};
    char kernel_files[MAX_AUDIOS][MAX_PATH] = {{0}};
    for (int i = 0; i < selected_count; i++) {
        int idx = selected_indices[i];
        snprintf(temp_files[i], MAX_PATH, "temp_input_%d.raw", i);
        snprintf(neural_output_files[i], MAX_PATH, "neural_output_%d.dat", i);
        snprintf(kernel_files[i], MAX_PATH, "kernels_%d.dat", i);
        float *resized = resize_audio(audio_data[idx], audio_lengths[idx], CHANNELS, min_length, log_fp);
        if (!resized) {
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        FILE *temp_fp = fopen(temp_files[i], "wb");
        if (!temp_fp) {
            fprintf(log_fp, "Error: Failed to write temporary file %s\n", temp_files[i]);
            free(resized);
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        fwrite(resized, sizeof(float), min_length * CHANNELS, temp_fp);
        fclose(temp_fp);
        free(resized);
    }

    // Train neural_net.+x for each audio
    for (int i = 0; i < selected_count; i++) {
        char command[MAX_PATH * 2];
        snprintf(command, MAX_PATH * 2, "%sneural_net.+x %s %s train %s %d", MODULE_DIR, temp_files[i], neural_output_files[i], kernel_files[i], min_length);
        fprintf(log_fp, "Executing: %s\n", command);
        printf("Training on %s...\n", temp_files[i]);
        if (system(command) != 0) {
            fprintf(log_fp, "Error: Failed to execute neural_net.+x train for %s\n", temp_files[i]);
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
    }

    // Run inference
    for (int i = 0; i < selected_count; i++) {
        char command[MAX_PATH * 2];
        snprintf(command, MAX_PATH * 2, "%sneural_net.+x %s %s infer %s %d", MODULE_DIR, temp_files[i], neural_output_files[i], kernel_files[i], min_length);
        fprintf(log_fp, "Executing: %s\n", command);
        printf("Inferring on %s...\n", temp_files[i]);
        if (system(command) != 0) {
            fprintf(log_fp, "Error: Failed to execute neural_net.+x infer for %s\n", temp_files[i]);
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        char output_raw[MAX_PATH];
        snprintf(output_raw, MAX_PATH, "output_%d.raw", i);
        if (access("output.raw", F_OK) == 0) {
            rename("output.raw", output_raw);
            fprintf(log_fp, "Generated %s for inspection\n", output_raw);
        } else {
            fprintf(log_fp, "Warning: output.raw not found for %s\n", temp_files[i]);
        }
    }

    char noise_input_file[MAX_PATH] = "neural_outputs_list.txt";
    FILE *fp = fopen(noise_input_file, "w");
    if (!fp) {
        fprintf(log_fp, "Error: Failed to create neural outputs list: %s\n", strerror(errno));
        cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
        fclose(log_fp);
        return 1;
    }
    for (int i = 0; i < selected_count; i++) {
        fprintf(fp, "%s\n", neural_output_files[i]);
    }
    fclose(fp);

    char noise_output_file[MAX_PATH] = "diffusion_output.raw";
    char command[MAX_PATH * 2];
    snprintf(command, MAX_PATH * 2, "%snoise_schedule.+x %s %s %d %s", MODULE_DIR, noise_input_file, noise_output_file, min_length, temp_files[0]);
    fprintf(log_fp, "Executing: %s\n", command);
    if (system(command) != 0) {
        fprintf(log_fp, "Warning: Failed to execute noise_schedule.+x, falling back to averaging\n");
        float *blend_output = (float *)calloc(min_length * CHANNELS, sizeof(float));
        if (!blend_output) {
            fprintf(log_fp, "Error: Memory allocation failed for blend_output\n");
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
            fclose(log_fp);
            return 1;
        }
        for (int i = 0; i < selected_count; i++) {
            FILE *out_fp = fopen(neural_output_files[i], "rb");
            if (!out_fp) {
                fprintf(log_fp, "Error: Failed to read %s\n", neural_output_files[i]);
                free(blend_output);
                cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
                fclose(log_fp);
                return 1;
            }
            float *temp = (float *)malloc(min_length * CHANNELS * sizeof(float));
            if (!temp) {
                fprintf(log_fp, "Error: Memory allocation failed for temp\n");
                fclose(out_fp);
                free(blend_output);
                cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
                fclose(log_fp);
                return 1;
            }
            fread(temp, sizeof(float), min_length * CHANNELS, out_fp);
            fclose(out_fp);
            for (int j = 0; j < min_length * CHANNELS; j++) {
                blend_output[j] += temp[j] / selected_count;
            }
            free(temp);
        }
        FILE *out_fp = fopen(noise_output_file, "wb");
        if (!out_fp) {
            fprintf(log_fp, "Error: Failed to write %s\n", noise_output_file);
            free(blend_output);
            cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
            fclose(log_fp);
            return 1;
        }
        fwrite(blend_output, sizeof(float), min_length * CHANNELS, out_fp);
        fclose(out_fp);
        free(blend_output);
    }

    FILE *final_fp = fopen(noise_output_file, "rb");
    if (!final_fp) {
        fprintf(log_fp, "Error: Failed to load final output %s\n", noise_output_file);
        cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
        fclose(log_fp);
        return 1;
    }
    fseek(final_fp, 0, SEEK_END);
    size_t file_size = ftell(final_fp);
    fseek(final_fp, 0, SEEK_SET);
    int out_length = file_size / (sizeof(float) * CHANNELS);
    fclose(final_fp);
    printf("Output saved as %s (%d samples, %d channels)\n", noise_output_file, out_length, CHANNELS);
    fprintf(log_fp, "Output saved as %s (%d samples, %d channels)\n", noise_output_file, out_length, CHANNELS);

    cleanup(audio_data, filenames, audio_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
    fclose(log_fp);
    return 0;
}
