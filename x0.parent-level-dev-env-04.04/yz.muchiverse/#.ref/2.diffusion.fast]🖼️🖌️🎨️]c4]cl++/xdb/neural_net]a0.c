#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_image> <output_file>\n", argv[0]);
        return 1;
    }

    int width, height, channels;
    unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 3);
    if (!img) {
        printf("Error: Failed to load %s\n", argv[1]);
        return 1;
    }

    // Simplified neural network: apply a basic transformation (e.g., enhance contrast)
    int size = width * height * 3;
    float *output = (float *)malloc(size * sizeof(float));
    if (!output) {
        printf("Error: Memory allocation failed\n");
        stbi_image_free(img);
        return 1;
    }

    for (int i = 0; i < size; i++) {
        output[i] = (float)img[i] * 1.5f; // Example: increase brightness/contrast
        if (output[i] > 255.0f) output[i] = 255.0f;
    }

    // Save output to file
    FILE *fp = fopen(argv[2], "wb");
    if (!fp) {
        printf("Error: Failed to open %s for writing\n", argv[2]);
        free(output);
        stbi_image_free(img);
        return 1;
    }
    fwrite(output, sizeof(float), size, fp);
    fclose(fp);

    free(output);
    stbi_image_free(img);
    return 0;
}
