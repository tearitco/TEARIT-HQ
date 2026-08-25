#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 1000
#define MAX_PATH 256

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file_list> <output_image> <width> <height>\n", argv[0]);
        return 1;
    }

    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    int size = width * height * 3;

    // Read list of input files
    char *input_files[MAX_FILES];
    int file_count = 0;
    char line[MAX_PATH];
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        printf("Error: Failed to open %s\n", argv[1]);
        return 1;
    }
    while (fgets(line, MAX_PATH, fp) && file_count < MAX_FILES) {
        line[strcspn(line, "\n")] = 0; // Remove newline
        input_files[file_count] = strdup(line);
        file_count++;
    }
    fclose(fp);

    if (file_count == 0) {
        printf("Error: No input files found\n");
        return 1;
    }

    // Allocate output array
    float *output = (float *)calloc(size, sizeof(float));
    if (!output) {
        printf("Error: Memory allocation failed\n");
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }

    // Read and average inputs (simulating denoising)
    for (int i = 0; i < file_count; i++) {
        float *data = (float *)malloc(size * sizeof(float));
        if (!data) {
            printf("Error: Memory allocation failed\n");
            free(output);
            for (int j = 0; j < file_count; j++) free(input_files[j]);
            return 1;
        }
        FILE *in_fp = fopen(input_files[i], "rb");
        if (!in_fp) {
            printf("Error: Failed to open %s\n", input_files[i]);
            free(data);
            continue;
        }
        fread(data, sizeof(float), size, in_fp);
        fclose(in_fp);

        for (int j = 0; j < size; j++) {
            output[j] += data[j] / file_count; // Average for simplicity
        }
        free(data);
    }

    // Convert to unsigned char
    unsigned char *final_pixels = (unsigned char *)malloc(size);
    if (!final_pixels) {
        printf("Error: Memory allocation failed\n");
        free(output);
        for (int i = 0; i < file_count; i++) free(input_files[i]);
        return 1;
    }
    for (int i = 0; i < size; i++) {
        final_pixels[i] = (unsigned char)(output[i] + 0.5f);
    }

    // Save output image
    if (!stbi_write_jpg(argv[2], width, height, 3, final_pixels, 90)) {
        printf("Error: Failed to write %s\n", argv[2]);
    } else {
        printf("Diffusion output saved as %s\n", argv[2]);
    }

    // Cleanup
    free(output);
    free(final_pixels);
    for (int i = 0; i < file_count; i++) free(input_files[i]);
    return 0;
}
