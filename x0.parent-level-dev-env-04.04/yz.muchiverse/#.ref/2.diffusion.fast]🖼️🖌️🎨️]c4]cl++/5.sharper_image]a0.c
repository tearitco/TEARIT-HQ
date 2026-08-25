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
    for (int i = 0; i < width * height * 3; i++) {
        img->data[i] = img_data[i] / 255.0f;
    }
    
    stbi_image_free(img_data);
    return img;
}

// Save image to file
int save_image(const Image *img, const char *filename) {
    unsigned char *img_data = (unsigned char *)malloc(img->width * img->height * 3);
    for (int i = 0; i < img->width * img->height * 3; i++) {
        img_data[i] = (unsigned char)(fmaxf(0.0f, fminf(1.0f, img->data[i])) * 255.0f);
    }
    
    int result = stbi_write_jpg(filename, img->width, img->height, 3, img_data, 100); // 100% quality
    free(img_data);
    return result;
}

// Compute Sobel gradient magnitude for adaptive sharpening
void sobel_gradient_magnitude(const Image *img, Image *grad_mag) {
    float sobel_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    float sobel_y[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    
    for (int c = 0; c < img->channels; c++) {
        for (int y = 1; y < img->height - 1; y++) {
            for (int x = 1; x < img->width - 1; x++) {
                float gx = 0.0f, gy = 0.0f;
                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        int idx = (y + ky) * img->width * img->channels + 
                                 (x + kx) * img->channels + c;
                        float pixel = img->data[idx];
                        gx += sobel_x[ky + 1][kx + 1] * pixel;
                        gy += sobel_y[ky + 1][kx + 1] * pixel;
                    }
                }
                int out_idx = y * img->width * img->channels + x * img->channels + c;
                grad_mag->data[out_idx] = sqrtf(gx * gx + gy * gy);
            }
        }
    }
}

// Adaptive sharpening based on gradient magnitude
void adaptive_sharpen(Image *img, float max_strength, float sigma) {
    float gaussian[3][3] = {{1.0f/16, 2.0f/16, 1.0f/16},
                           {2.0f/16, 4.0f/16, 2.0f/16},
                           {1.0f/16, 2.0f/16, 1.0f/16}};
    
    Image *blurred = create_image(img->width, img->height, img->channels);
    Image *grad_mag = create_image(img->width, img->height, img->channels);
    
    // Compute gradient magnitude
    sobel_gradient_magnitude(img, grad_mag);
    
    // Normalize gradient magnitude to [0, 1]
    float max_grad = 0.0f;
    int total_pixels = img->width * img->height * img->channels;
    for (int i = 0; i < total_pixels; i++) {
        max_grad = fmaxf(max_grad, grad_mag->data[i]);
    }
    if (max_grad > 0.0f) {
        for (int i = 0; i < total_pixels; i++) {
            grad_mag->data[i] /= max_grad;
        }
    }
    
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
    
    // Apply adaptive sharpening: strength scales with gradient magnitude
    for (int i = 0; i < total_pixels; i++) {
        float strength = max_strength * grad_mag->data[i]; // Stronger on edges
        float detail = img->data[i] - blurred->data[i];
        img->data[i] = img->data[i] + strength * detail;
        img->data[i] = fmaxf(0.0f, fminf(1.0f, img->data[i]));
    }
    
    free_image(blurred);
    free_image(grad_mag);
}

// Bilateral filter for texture preservation
void bilateral_filter(Image *img, float sigma_spatial, float sigma_color) {
    Image *filtered = create_image(img->width, img->height, img->channels);
    int radius = (int)(2.0f * sigma_spatial);
    
    for (int c = 0; c < img->channels; c++) {
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                float sum = 0.0f, weight_sum = 0.0f;
                float center_val = img->data[y * img->width * img->channels + x * img->channels + c];
                
                for (int ky = -radius; ky <= radius; ky++) {
                    for (int kx = -radius; kx <= radius; kx++) {
                        int ny = y + ky;
                        int nx = x + kx;
                        if (nx >= 0 && nx < img->width && ny >= 0 && ny < img->height) {
                            int idx = ny * img->width * img->channels + nx * img->channels + c;
                            float val = img->data[idx];
                            float spatial_dist = (kx * kx + ky * ky) / (2.0f * sigma_spatial * sigma_spatial);
                            float color_dist = (val - center_val) * (val - center_val) / (2.0f * sigma_color * sigma_color);
                            float weight = expf(-spatial_dist - color_dist);
                            sum += weight * val;
                            weight_sum += weight;
                        }
                    }
                }
                
                int out_idx = y * img->width * img->channels + x * img->channels + c;
                filtered->data[out_idx] = weight_sum > 0.0f ? sum / weight_sum : center_val;
            }
        }
    }
    
    // Copy filtered result back to input
    memcpy(img->data, filtered->data, img->width * img->height * img->channels * sizeof(float));
    free_image(filtered);
}

// Gamma correction for perceptual brightness
void gamma_correction(Image *img, float gamma) {
    int total_pixels = img->width * img->height * img->channels;
    for (int i = 0; i < total_pixels; i++) {
        img->data[i] = powf(fmaxf(0.0f, img->data[i]), gamma);
    }
}

// Histogram stretching for contrast
void histogram_stretch(Image *img) {
    float min_val[3] = {1.0f, 1.0f, 1.0f};
    float max_val[3] = {0.0f, 0.0f, 0.0f};
    int total_pixels = img->width * img->height;
    
    for (int i = 0; i < total_pixels; i++) {
        for (int c = 0; c < 3; c++) {
            float val = img->data[i * 3 + c];
            min_val[c] = fminf(min_val[c], val);
            max_val[c] = fmaxf(max_val[c], val);
        }
    }
    
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
        
        float max = fmaxf(r, fmaxf(g, b));
        float min = fminf(r, fminf(g, b));
        float h, s, v = max;
        float delta = max - min;
        
        if (delta < 1e-5f || max < 1e-5f) {
            s = 0.0f;
            h = 0.0f;
        } else {
            s = delta / max;
            if (r >= max) h = (g - b) / delta;
            else if (g >= max) h = 2.0f + (b - r) / delta;
            else h = 4.0f + (r - g) / delta;
            h *= 60.0f;
            if (h < 0.0f) h += 360.0f;
        }
        
        s = fminf(1.0f, s * factor);
        
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

// Match variance to target
void match_variance(Image *img, float target_variance) {
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

// Enhance image with all tricks
void enhance_image(Image *img, float target_variance) {
    // Texture preservation: bilateral filter
    bilateral_filter(img, 1.0f, 0.1f); // sigma_spatial=1.0, sigma_color=0.1
    
    // Sharpness: adaptive sharpening
    adaptive_sharpen(img, 1.5f, 1.0f); // max_strength=1.5, sigma=1.0
    
    // Vibrancy: contrast, brightness, saturation
    histogram_stretch(img);
    boost_brightness(img, 1.2f); // 20% boost
    boost_saturation(img, 1.3f); // 30% boost
    
    // Perceptual brightness: gamma correction
    gamma_correction(img, 0.8f); // gamma < 1 brightens mid-tones
    
    // Noise distribution: match variance
    match_variance(img, target_variance);
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
    
    // Enhance image
    float target_variance = 0.027f; // Adjusted to match input image variance
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
