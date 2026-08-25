#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_IMAGES 1000
#define MAX_PATH 256
#define MAX_INPUT 256
#define MODULE_DIR "./+x/"

void cleanup(unsigned char *image_data[], char *filenames[], int image_count, char temp_files[][MAX_PATH], char neural_output_files[][MAX_PATH], char kernel_files[][MAX_PATH], int selected_count, char *noise_input_file) {
    for (int i = 0; i < image_count; i++) {
        if (image_data[i]) stbi_image_free(image_data[i]);
        if (filenames[i]) free(filenames[i]);
    }
    for (int i = 0; i < selected_count; i++) {
        if (temp_files[i][0]) remove(temp_files[i]);
        if (neural_output_files[i][0]) remove(neural_output_files[i]);
        if (kernel_files[i][0]) remove(kernel_files[i]);
    }
    if (noise_input_file && noise_input_file[0]) remove(noise_input_file);
}

unsigned char *resize_image(unsigned char *img, int in_width, int in_height, int channels, int out_width, int out_height, FILE *log_fp) {
    unsigned char *resized = (unsigned char *)malloc(out_width * out_height * channels);
    if (!resized) {
        fprintf(log_fp, "Error: Memory allocation failed for resized image\n");
        return NULL;
    }
    for (int y = 0; y < out_height; y++) {
        for (int x = 0; x < out_width; x++) {
            int src_x = (x * in_width) / out_width;
            int src_y = (y * in_height) / out_height;
            for (int c = 0; c < channels; c++) {
                resized[(y * out_width + x) * channels + c] = img[(src_y * in_width + src_x) * channels + c];
            }
        }
    }
    return resized;
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

    unsigned char *image_data[MAX_IMAGES] = {0};
    int image_widths[MAX_IMAGES] = {0};
    int image_heights[MAX_IMAGES] = {0};
    int image_channels[MAX_IMAGES] = {0};
    char *filenames[MAX_IMAGES] = {0};
    int image_count = 0;

    struct dirent *entry;
    char filepath[MAX_PATH];
    while ((entry = readdir(dir)) && image_count < MAX_IMAGES) {
        if (strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg")) {
            snprintf(filepath, MAX_PATH, "%s/%s", argv[1], entry->d_name);
            int width, height, channels;
            unsigned char *img = stbi_load(filepath, &width, &height, &channels, 3);
            if (!img) {
                fprintf(log_fp, "Warning: Failed to load %s\n", filepath);
                continue;
            }
            image_data[image_count] = img;
            image_widths[image_count] = width;
            image_heights[image_count] = height;
            image_channels[image_count] = channels;
            filenames[image_count] = strdup(entry->d_name);
            if (!filenames[image_count]) {
                fprintf(log_fp, "Error: Memory allocation failed for filename\n");
                cleanup(image_data, filenames, image_count, NULL, NULL, NULL, 0, NULL);
                closedir(dir);
                fclose(log_fp);
                return 1;
            }
            image_count++;
        }
    }
    closedir(dir);

    if (image_count == 0) {
        fprintf(log_fp, "Error: No valid JPEG images found in directory\n");
        fclose(log_fp);
        return 1;
    }

    printf("Available images:\n");
    for (int i = 0; i < image_count; i++) {
        printf("%d: %s\n", i, filenames[i]);
    }

    printf("Enter indices of images to blend (space-separated, e.g., '1 4 6'): ");
    char input[MAX_INPUT] = {0};
    if (!fgets(input, MAX_INPUT, stdin)) {
        fprintf(log_fp, "Error: Failed to read input\n");
        cleanup(image_data, filenames, image_count, NULL, NULL, NULL, 0, NULL);
        fclose(log_fp);
        return 1;
    }

    int selected_indices[MAX_IMAGES] = {0};
    int selected_count = 0;
    char *token = strtok(input, " \n");
    while (token && selected_count < MAX_IMAGES) {
        int idx = atoi(token);
        if (idx >= 0 && idx < image_count) {
            selected_indices[selected_count++] = idx;
        } else {
            fprintf(log_fp, "Warning: Invalid index %d, ignoring\n", idx);
        }
        token = strtok(NULL, " \n");
    }

    if (selected_count == 0) {
        fprintf(log_fp, "Error: No valid indices selected\n");
        cleanup(image_data, filenames, image_count, NULL, NULL, NULL, 0, NULL);
        fclose(log_fp);
        return 1;
    }

    fprintf(log_fp, "Selected %d images for blending:\n", selected_count);
    for (int i = 0; i < selected_count; i++) {
        fprintf(log_fp, "  %d: %s (%dx%d, %d channels)\n", selected_indices[i], filenames[selected_indices[i]],
                image_widths[selected_indices[i]], image_heights[selected_indices[i]], image_channels[selected_indices[i]]);
    }

    int min_width = image_widths[selected_indices[0]];
    int min_height = image_heights[selected_indices[0]];
    for (int i = 1; i < selected_count; i++) {
        int idx = selected_indices[i];
        if (image_widths[idx] < min_width) min_width = image_widths[idx];
        if (image_heights[idx] < min_height) min_height = image_heights[idx];
    }

    char temp_files[MAX_IMAGES][MAX_PATH] = {{0}};
    char neural_output_files[MAX_IMAGES][MAX_PATH] = {{0}};
    char kernel_files[MAX_IMAGES][MAX_PATH] = {{0}};
    for (int i = 0; i < selected_count; i++) {
        int idx = selected_indices[i];
        snprintf(temp_files[i], MAX_PATH, "temp_input_%d.jpg", i);
        snprintf(neural_output_files[i], MAX_PATH, "neural_output_%d.dat", i);
        snprintf(kernel_files[i], MAX_PATH, "kernels_%d.dat", i);
        unsigned char *resized = resize_image(image_data[idx], image_widths[idx], image_heights[idx], 3, min_width, min_height, log_fp);
        if (!resized) {
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        if (!stbi_write_jpg(temp_files[i], min_width, min_height, 3, resized, 90)) {
            fprintf(log_fp, "Error: Failed to write temporary file %s\n", temp_files[i]);
            free(resized);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        free(resized);
    }

    // Train neural_net.+x for each image
    for (int i = 0; i < selected_count; i++) {
        char command[MAX_PATH * 2];
        snprintf(command, MAX_PATH * 2, "%sneural_net.+x %s %s train %s", MODULE_DIR, temp_files[i], neural_output_files[i], kernel_files[i]);
        fprintf(log_fp, "Executing: %s\n", command);
        printf("Training on %s...\n", temp_files[i]);
        if (system(command) != 0) {
            fprintf(log_fp, "Error: Failed to execute neural_net.+x train for %s\n", temp_files[i]);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
    }

    // Run inference and validate output.jpg
    for (int i = 0; i < selected_count; i++) {
        char command[MAX_PATH * 2];
        snprintf(command, MAX_PATH * 2, "%sneural_net.+x %s %s infer %s", MODULE_DIR, temp_files[i], neural_output_files[i], kernel_files[i]);
        fprintf(log_fp, "Executing: %s\n", command);
        printf("Inferring on %s...\n", temp_files[i]);
        if (system(command) != 0) {
            fprintf(log_fp, "Error: Failed to execute neural_net.+x infer for %s\n", temp_files[i]);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, NULL);
            fclose(log_fp);
            return 1;
        }
        char output_jpg[MAX_PATH];
        snprintf(output_jpg, MAX_PATH, "output_%d.jpg", i);
        if (access("output.jpg", F_OK) == 0) {
            rename("output.jpg", output_jpg);
            fprintf(log_fp, "Generated %s for inspection\n", output_jpg);
        } else {
            fprintf(log_fp, "Warning: output.jpg not found for %s\n", temp_files[i]);
        }
    }

    char noise_input_file[MAX_PATH] = "neural_outputs_list.txt";
    FILE *fp = fopen(noise_input_file, "w");
    if (!fp) {
        fprintf(log_fp, "Error: Failed to create neural outputs list: %s\n", strerror(errno));
        cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
        fclose(log_fp);
        return 1;
    }
    for (int i = 0; i < selected_count; i++) {
        fprintf(fp, "%s\n", neural_output_files[i]);
    }
    fclose(fp);

    char noise_output_file[MAX_PATH] = "diffusion_output.jpg";
    char command[MAX_PATH * 2];
    snprintf(command, MAX_PATH * 2, "%snoise_schedule.+x %s %s %d %d", MODULE_DIR, noise_input_file, noise_output_file, min_width, min_height);
    fprintf(log_fp, "Executing: %s\n", command);
    if (system(command) != 0) {
        fprintf(log_fp, "Warning: Failed to execute noise_schedule.+x, falling back to averaging\n");
        float *blend_output = (float *)calloc(min_width * min_height * 3, sizeof(float));
        if (!blend_output) {
            fprintf(log_fp, "Error: Memory allocation failed for blend_output\n");
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
            fclose(log_fp);
            return 1;
        }
        for (int i = 0; i < selected_count; i++) {
            FILE *out_fp = fopen(neural_output_files[i], "rb");
            if (!out_fp) {
                fprintf(log_fp, "Error: Failed to read %s\n", neural_output_files[i]);
                free(blend_output);
                cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
                fclose(log_fp);
                return 1;
            }
            float *temp = (float *)malloc(min_width * min_height * 3 * sizeof(float));
            if (!temp) {
                fprintf(log_fp, "Error: Memory allocation failed for temp\n");
                fclose(out_fp);
                free(blend_output);
                cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
                fclose(log_fp);
                return 1;
            }
            fread(temp, sizeof(float), min_width * min_height * 3, out_fp);
            fclose(out_fp);
            for (int j = 0; j < min_width * min_height * 3; j++) {
                blend_output[j] += temp[j] / selected_count;
            }
            free(temp);
        }
        unsigned char *img_out = (unsigned char *)malloc(min_width * min_height * 3);
        if (!img_out) {
            fprintf(log_fp, "Error: Memory allocation failed for img_out\n");
            free(blend_output);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
            fclose(log_fp);
            return 1;
        }
        for (int i = 0; i < min_width * min_height * 3; i++) {
            img_out[i] = (unsigned char)(fminf(fmaxf(blend_output[i], 0.0f), 1.0f) * 255.0f);
        }
        if (!stbi_write_jpg(noise_output_file, min_width, min_height, 3, img_out, 90)) {
            fprintf(log_fp, "Error: Failed to write %s\n", noise_output_file);
            free(blend_output);
            free(img_out);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
            fclose(log_fp);
            return 1;
        }
        free(blend_output);
        free(img_out);
    }

    int out_width, out_height, out_channels;
    unsigned char *final_image = stbi_load(noise_output_file, &out_width, &out_height, &out_channels, 3);
    if (!final_image) {
        fprintf(log_fp, "Error: Failed to load final output %s\n", noise_output_file);
        cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
        fclose(log_fp);
        return 1;
    } else {
        printf("Output saved as %s (%dx%d, %d channels)\n", noise_output_file, out_width, out_height, out_channels);
        fprintf(log_fp, "Output saved as %s (%dx%d, %d channels)\n", noise_output_file, out_width, out_height, out_channels);
        stbi_image_free(final_image);
    }

    cleanup(image_data, filenames, image_count, temp_files, neural_output_files, kernel_files, selected_count, noise_input_file);
    fclose(log_fp);
    return 0;
}
