#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Image structure: RGB image as float array [0, 1]
typedef struct {
    int width, height, channels;
    float *data; // 1D array: [r,g,b, r,g,b, ...]
} Image;

// Allocate image memory
Image *create_image(int width, int height, int channels) {
    Image *img = (Image *)malloc(sizeof(Image));
    img->width = width;
    img->height = height;
    img->channels = channels;
    img->data = (float *)calloc(width * height * channels, sizeof(float));
    return img;
}

// Free image memory
void free_image(Image *img) {
    free(img->data);
    free(img);
}

// Load image from file
Image *load_image(const char *filename) {
    int width, height, channels;
    unsigned char *img_data = stbi_load(filename, &width, &height, &channels, 3); // Force 3 channels (RGB)
    if (!img_data) {
        fprintf(stderr, "Error: Failed to load image %s\n", filename);
        return NULL;
    }
    
    Image *img = create_image(width, height, 3);
    // Convert uint8 [0, 255] to float [0, 1]
    for (int i = 0; i < width * height * 3; i++) {
        img->data[i] = img_data[i] / 255.0f;
    }
    
    stbi_image_free(img_data);
    return img;
}

// Save image to file
int save_image(const Image *img, const char *filename) {
    unsigned char *img_data = (unsigned char *)malloc(img->width * img->height * 3);
    // Convert float [0, 1] to uint8 [0, 255]
    for (int i = 0; i < img->width * img->height * 3; i++) {
        img_data[i] = (unsigned char)(fmaxf(0.0f, fminf(1.0f, img->data[i])) * 255.0f);
    }
    
    int result = stbi_write_jpg(filename, img->width, img->height, 3, img_data, 95); // 95% quality
    free(img_data);
    return result;
}

// Unsharp masking for sharpness enhancement
void unsharp_mask(Image *img, float sigma, float strength) {
    float gaussian[3][3] = {{1.0f/16, 2.0f/16, 1.0f/16},
                           {2.0f/16, 4.0f/16, 2.0f/16},
                           {1.0f/16, 2.0f/16, 1.0f/16}};
    
    Image *blurred = create_image(img->width, img->height, img->channels);
    
    // Apply Gaussian blur
    for (int c = 0; c < img->channels; c++) {
        for (int y = 1; y < img->height - 1; y++) {
            for (int x = 1; x < img->width - 1; x++) {
                float sum = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int idx = (y + ky) * img->width * img->channels + 
                                 (x + kx) * img->channels + c;
                        sum += gaussian[ky + 1][kx + 1] * img->data[idx];
                    }
                }
                int out_idx = y * img->width * img->channels + x * img->channels + c;
                blurred->data[out_idx] = sum;
            }
        }
    }
    
    // Apply unsharp mask: output = original + strength * (original - blurred)
    for (int i = 0; i < img->width * img->height * img->channels; i++) {
        float detail = img->data[i] - blurred->data[i];
        img->data[i] = img->data[i] + strength * detail;
        img->data[i] = fmaxf(0.0f, fminf(1.0f, img->data[i])); // Clamp to [0, 1]
    }
    
    free_image(blurred);
}

// Scale image variance to target
void scale_variance(Image *img, float target_variance) {
    float mean = 0.0f, variance = 0.0f;
    int total_pixels = img->width * img->height * img->channels;
    
    for (int i = 0; i < total_pixels; i++) {
        mean += img->data[i];
    }
    mean /= total_pixels;
    
    for (int i = 0; i < total_pixels; i++) {
        float diff = img->data[i] - mean;
        variance += diff * diff;
    }
    variance /= total_pixels;
    
    if (variance > 0.0f) {
        float scale = sqrtf(target_variance / variance);
        for (int i = 0; i < total_pixels; i++) {
            img->data[i] = mean + (img->data[i] - mean) * scale;
        }
    }
}

// Histogram stretching for contrast enhancement
void histogram_stretch(Image *img) {
    float min_val[3] = {1.0f, 1.0f, 1.0f};
    float max_val[3] = {0.0f, 0.0f, 0.0f};
    int total_pixels = img->width * img->height;
    
    // Find min/max per channel
    for (int i = 0; i < total_pixels; i++) {
        for (int c = 0; c < 3; c++) {
            float val = img->data[i * 3 + c];
            min_val[c] = fminf(min_val[c], val);
            max_val[c] = fmaxf(max_val[c], val);
        }
    }
    
    // Stretch each channel to [0, 1]
    for (int i = 0; i < total_pixels; i++) {
        for (int c = 0; c < 3; c++) {
            float range = max_val[c] - min_val[c];
            if (range > 0.0f) {
                img->data[i * 3 + c] = (img->data[i * 3 + c] - min_val[c]) / range;
            }
        }
    }
}

// Brightness boost
void boost_brightness(Image *img, float factor) {
    int total_pixels = img->width * img->height * img->channels;
    for (int i = 0; i < total_pixels; i++) {
        img->data[i] = fminf(1.0f, img->data[i] * factor);
    }
}

// Saturation boost (RGB to HSV, boost S, back to RGB)
void boost_saturation(Image *img, float factor) {
    int total_pixels = img->width * img->height;
    for (int i = 0; i < total_pixels; i++) {
        float r = img->data[i * 3 + 0];
        float g = img->data[i * 3 + 1];
        float b = img->data[i * 3 + 2];
        
        // RGB to HSV
        float max = fmaxf(r, fmaxf(g, b));
        float min = fminf(r, fminf(g, b));
        float h, s, v = max;
        float delta = max - min;
        
        if (delta < 1e-5f || max < 1e-5f) {
            s = 0.0f;
            h = 0.0f;
        } else {
            s = delta / max;
            if (r >= max) h = (g - b) / delta; // Between yellow & magenta
            else if (g >= max) h = 2.0f + (b - r) / delta; // Between cyan & yellow
            else h = 4.0f + (r - g) / delta; // Between magenta & cyan
            h *= 60.0f;
            if (h < 0.0f) h += 360.0f;
        }
        
        // Boost saturation
        s = fminf(1.0f, s * factor);
        
        // HSV to RGB
        if (s == 0.0f) {
            r = g = b = v;
        } else {
            h /= 60.0f;
            int i = (int)h;
            float f = h - i;
            float p = v * (1.0f - s);
            float q = v * (1.0f - s * f);
            float t = v * (1.0f - s * (1.0f - f));
            
            switch (i) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
            }
        }
        
        img->data[i * 3 + 0] = fmaxf(0.0f, fminf(1.0f, r));
        img->data[i * 3 + 1] = fmaxf(0.0f, fminf(1.0f, g));
        img->data[i * 3 + 2] = fmaxf(0.0f, fminf(1.0f, b));
    }
}

// Enhance image sharpness and vibrancy
void enhance_image(Image *img, float target_variance) {
    // Sharpness: scale variance and apply unsharp masking
    scale_variance(img, target_variance);
    unsharp_mask(img, 1.0f, 1.5f); // sigma=1.0, strength=1.5
    
    // Vibrancy: boost contrast, brightness, saturation
    histogram_stretch(img);
    boost_brightness(img, 1.2f); // 20% brightness boost
    boost_saturation(img, 1.3f); // 30% saturation boost
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_image.jpg>\n", argv[0]);
        return 1;
    }
    
    // Load image from arg1
    Image *img = load_image(argv[1]);
    if (!img) {
        return 1;
    }
    
    // Enhance sharpness and vibrancy
    float target_variance = 0.00083f; // From noise_stats.txt
    enhance_image(img, target_variance);
    
    // Save enhanced image
    if (!save_image(img, "output_enhanced.jpg")) {
        fprintf(stderr, "Error: Failed to save output_enhanced.jpg\n");
        free_image(img);
        return 1;
    }
    
    printf("Enhanced image saved as output_enhanced.jpg\n");
    free_image(img);
    return 0;
}
