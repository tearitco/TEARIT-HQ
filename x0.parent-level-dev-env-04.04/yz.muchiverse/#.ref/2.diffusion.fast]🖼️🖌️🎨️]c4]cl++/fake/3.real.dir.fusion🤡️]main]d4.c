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

// Maximum number of images to process
#define MAX_IMAGES 1000
// Maximum path length for file names
#define MAX_PATH 256
// Maximum length for user input
#define MAX_INPUT 256
// Directory for executable modules
#define MODULE_DIR "./+x/"

// Cleanup function to free resources
void cleanup(unsigned char *image_data[], char *filenames[], int image_count, char temp_files[][MAX_PATH], char neural_output_files[][MAX_PATH], int selected_count, char *noise_input_file) {
    for (int i = 0; i < image_count; i++) {
        if (image_data[i]) stbi_image_free(image_data[i]);
        if (filenames[i]) free(filenames[i]);
    }
    for (int i = 0; i < selected_count; i++) {
        if (temp_files[i][0]) remove(temp_files[i]);
        if (neural_output_files[i][0]) remove(neural_output_files[i]);
    }
    if (noise_input_file[0]) remove(noise_input_file);
}

int main(int argc, char *argv[]) {
    // Check if directory path is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Ensure module directory exists
    if (mkdir(MODULE_DIR, 0700) == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: Cannot create directory %s: %s\n", MODULE_DIR, strerror(errno));
        return 1;
    }

    // Directory handling
    DIR *dir = opendir(argv[1]);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    // Arrays to store image data and filenames
    unsigned char *image_data[MAX_IMAGES] = {0};
    int image_widths[MAX_IMAGES] = {0};
    int image_heights[MAX_IMAGES] = {0};
    int image_channels[MAX_IMAGES] = {0};
    char *filenames[MAX_IMAGES] = {0};
    int image_count = 0;

    // Read directory and load JPEG images
    struct dirent *entry;
    char filepath[MAX_PATH];
    while ((entry = readdir(dir)) && image_count < MAX_IMAGES) {
        if (strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg")) {
            snprintf(filepath, MAX_PATH, "%s/%s", argv[1], entry->d_name);

            int width, height, channels;
            unsigned char *img = stbi_load(filepath, &width, &height, &channels, 3);
            if (!img) {
                fprintf(stderr, "Warning: Failed to load %s\n", filepath);
                continue;
            }

            image_data[image_count] = img;
            image_widths[image_count] = width;
            image_heights[image_count] = height;
            image_channels[image_count] = channels;
            filenames[image_count] = strdup(entry->d_name);
            if (!filenames[image_count]) {
                fprintf(stderr, "Error: Memory allocation failed for filename\n");
                cleanup(image_data, filenames, image_count, NULL, NULL, 0, NULL);
                closedir(dir);
                return 1;
            }
            image_count++;
        }
    }
    closedir(dir);

    if (image_count == 0) {
        fprintf(stderr, "Error: No valid JPEG images found in directory\n");
        return 1;
    }

    // Display image list with indices
    printf("Available images:\n");
    for (int i = 0; i < image_count; i++) {
        printf("%d: %s\n", i, filenames[i]);
    }

    // Prompt user for indices
    printf("Enter indices of images to blend (space-separated, e.g., '1 4 6'): ");
    char input[MAX_INPUT] = {0};
    if (!fgets(input, MAX_INPUT, stdin)) {
        fprintf(stderr, "Error: Failed to read input\n");
        cleanup(image_data, filenames, image_count, NULL, NULL, 0, NULL);
        return 1;
    }

    // Parse input indices
    int selected_indices[MAX_IMAGES] = {0};
    int selected_count = 0;
    char *token = strtok(input, " \n");
    while (token && selected_count < MAX_IMAGES) {
        int idx = atoi(token);
        if (idx >= 0 && idx < image_count) {
            selected_indices[selected_count++] = idx;
        } else {
            fprintf(stderr, "Warning: Invalid index %d, ignoring\n", idx);
        }
        token = strtok(NULL, " \n");
    }

    if (selected_count == 0) {
        fprintf(stderr, "Error: No valid indices selected\n");
        cleanup(image_data, filenames, image_count, NULL, NULL, 0, NULL);
        return 1;
    }

    // Find minimum dimensions among selected images
    int min_width = image_widths[selected_indices[0]];
    int min_height = image_heights[selected_indices[0]];
    for (int i = 1; i < selected_count; i++) {
        int idx = selected_indices[i];
        if (image_widths[idx] < min_width) min_width = image_widths[idx];
        if (image_heights[idx] < min_height) min_height = image_heights[idx];
    }

    // Save selected images to temporary files for neural_net processing
    char temp_files[MAX_IMAGES][MAX_PATH] = {{0}};
    char neural_output_files[MAX_IMAGES][MAX_PATH] = {{0}};
    for (int i = 0; i < selected_count; i++) {
        int idx = selected_indices[i];
        snprintf(temp_files[i], MAX_PATH, "temp_input_%d.jpg", i);
        if (!stbi_write_jpg(temp_files[i], image_widths[idx], image_heights[idx], 3, image_data[idx], 90)) {
            fprintf(stderr, "Error: Failed to write temporary file %s\n", temp_files[i]);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, NULL);
            return 1;
        }
    }

    // Call neural_net.+x for each image
    for (int i = 0; i < selected_count; i++) {
        snprintf(neural_output_files[i], MAX_PATH, "neural_output_%d.dat", i);
        char command[MAX_PATH * 2];
        snprintf(command, MAX_PATH * 2, "%sneural_net.+x %s %s", MODULE_DIR, temp_files[i], neural_output_files[i]);
        if (system(command) != 0) {
            fprintf(stderr, "Error: Failed to execute neural_net.+x for %s\n", temp_files[i]);
            cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, NULL);
            return 1;
        }
    }

    // Call noise_schedule.+x to process neural outputs
    char noise_input_file[MAX_PATH] = "neural_outputs_list.txt";
    FILE *fp = fopen(noise_input_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Failed to create neural outputs list: %s\n", strerror(errno));
        cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, noise_input_file);
        return 1;
    }
    for (int i = 0; i < selected_count; i++) {
        fprintf(fp, "%s\n", neural_output_files[i]);
    }
    fclose(fp);

    char noise_output_file[MAX_PATH] = "diffusion_output.jpg";
    char command[MAX_PATH * 2];
    snprintf(command, MAX_PATH * 2, "%snoise_schedule.+x %s %s %d %d", MODULE_DIR, noise_input_file, noise_output_file, min_width, min_height);
    if (system(command) != 0) {
        fprintf(stderr, "Error: Failed to execute noise_schedule.+x\n");
        cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, noise_input_file);
        return 1;
    }

    // Load and verify final output
    int out_width, out_height, out_channels;
    unsigned char *final_image = stbi_load(noise_output_file, &out_width, &out_height, &out_channels, 3);
    if (!final_image) {
        fprintf(stderr, "Error: Failed to load final output %s\n", noise_output_file);
        cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, noise_input_file);
        return 1;
    } else {
        printf("Output saved as %s\n", noise_output_file);
        stbi_image_free(final_image);
    }

    // Cleanup
    cleanup(image_data, filenames, image_count, temp_files, neural_output_files, selected_count, noise_input_file);
    return 0;
}
