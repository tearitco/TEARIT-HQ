#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "!.emoji.xtract.stb]c4/stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

// --- Config ---
#define EMOJI_RENDER_SIZE 64   // Matches your Candy Crush tile inner size
#define MAX_EMOJIS 64          // Maximum number of emojis to support (much smaller)
#define ATLAS_WIDTH (EMOJI_RENDER_SIZE * MAX_EMOJIS)
#define ATLAS_HEIGHT EMOJI_RENDER_SIZE

// We'll work in RGBA for PNG
typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

char *emoji_strings[MAX_EMOJIS];
char *emoji_names[MAX_EMOJIS];
char *emoji_requested_names[MAX_EMOJIS];  // Names of emojis we want to extract
int NUM_REQUESTED_EMOJIS = 0;             // Number of emojis we want to extract
int NUM_AVAILABLE_EMOJIS = 0;             // Number of available emojis from parsed_emojis.txt
int NUM_EMOJIS = 0;                       // Number of emojis actually loaded (declare here to fix compilation)

// Function to read specific emojis from parsed_emojis.txt based on requested names
int read_needed_emojis_from_file() {
    FILE *file = fopen("parsed_emojis.txt", "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open parsed_emojis.txt\n");
        return 0;
    }

    char line[1024];
    int available_count = 0;
    int matched_count = 0;
    
    // First pass: count total available emojis
    long file_pos = ftell(file);
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '#') { // Skip comment lines
            available_count++;
        }
    }
    NUM_AVAILABLE_EMOJIS = available_count;
    printf("🔍 Found %d total emojis in parsed_emojis.txt\n", available_count);
    
    // Reset file pointer
    fseek(file, file_pos, SEEK_SET);
    
    // Allocate memory for all available emojis
    char **all_emoji_names = malloc(available_count * sizeof(char*));
    char **all_emoji_strings = malloc(available_count * sizeof(char*));
    if (!all_emoji_names || !all_emoji_strings) {
        fprintf(stderr, "Error: Could not allocate memory for emoji arrays\n");
        if (all_emoji_names) free(all_emoji_names);
        if (all_emoji_strings) free(all_emoji_strings);
        fclose(file);
        return 0;
    }

    // Second pass: read all available emojis
    int all_emoji_index = 0;
    while (fgets(line, sizeof(line), file) && all_emoji_index < available_count) {
        if (line[0] == '#') continue;  // Skip comment lines
        
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        
        // Parse the line: codepoint_hex_string|emoji_string|emoji_name
        char *token = strtok(line, "|");
        if (!token) continue;
        
        // Get emoji string (second part)
        char *emoji_str = strtok(NULL, "|");
        if (!emoji_str) continue;
        
        // Get emoji description (third part)  
        char *emoji_desc = strtok(NULL, "|");
        if (!emoji_desc) continue;

        // Allocate memory and store emoji string
        all_emoji_strings[all_emoji_index] = malloc(strlen(emoji_str) + 1);
        strcpy(all_emoji_strings[all_emoji_index], emoji_str);
        
        // Sanitize the emoji description to create a lookup name
        char sanitized_name[256];
        int j = 0;
        for (int i = 0; emoji_desc[i] && j < 254; i++) {
            if ((emoji_desc[i] >= 'a' && emoji_desc[i] <= 'z') ||
                (emoji_desc[i] >= 'A' && emoji_desc[i] <= 'Z') ||
                (emoji_desc[i] >= '0' && emoji_desc[i] <= '9') ||
                emoji_desc[i] == '_' || emoji_desc[i] == '-') {
                sanitized_name[j++] = emoji_desc[i];
            } else {
                sanitized_name[j++] = '_';
            }
        }
        sanitized_name[j] = '\0';
        
        all_emoji_names[all_emoji_index] = malloc(strlen(sanitized_name) + 1);
        strcpy(all_emoji_names[all_emoji_index], sanitized_name);
        
        all_emoji_index++;
    }
    
    fclose(file);

    // Third pass: match requested emojis with available emojis
    for (int req_idx = 0; req_idx < NUM_REQUESTED_EMOJIS; req_idx++) {
        int found = 0;
        for (int avail_idx = 0; avail_idx < all_emoji_index; avail_idx++) {
            if (strcmp(all_emoji_names[avail_idx], emoji_requested_names[req_idx]) == 0) {
                // Match found - store this emoji
                emoji_strings[matched_count] = malloc(strlen(all_emoji_strings[avail_idx]) + 1);
                strcpy(emoji_strings[matched_count], all_emoji_strings[avail_idx]);
                
                emoji_names[matched_count] = malloc(strlen(all_emoji_names[avail_idx]) + 1); 
                strcpy(emoji_names[matched_count], all_emoji_names[avail_idx]);
                
                printf("✅ Requested '%s' matched available emoji\n", emoji_requested_names[req_idx]);
                matched_count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "❌ Requested emoji '%s' not found in available emojis\n", emoji_requested_names[req_idx]);
        }
    }
    
    // Clean up temporary arrays
    for (int i = 0; i < available_count; i++) {
        if (all_emoji_names[i]) free(all_emoji_names[i]);
        if (all_emoji_strings[i]) free(all_emoji_strings[i]);
    }
    free(all_emoji_names);
    free(all_emoji_strings);
    
    NUM_EMOJIS = matched_count;
    printf("✅ Loaded %d requested emojis that were found in parsed_emojis.txt\n", NUM_EMOJIS);
    return 1;
}

// Function to free allocated memory
void free_emoji_data() {
    for (int i = 0; i < NUM_EMOJIS; i++) {
        if (emoji_strings[i]) free(emoji_strings[i]);
        if (emoji_names[i]) free(emoji_names[i]);
    }
    for (int i = 0; i < NUM_REQUESTED_EMOJIS; i++) {
        if (emoji_requested_names[i]) free(emoji_requested_names[i]);
    }
}

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[0] & 0xC0) == 0x80 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

// --- Write PNG via stb_image_write ---
int write_png(const char* filename, RGBA_Pixel* pixels, int width, int height) {
    int result = stbi_write_png(filename, width, height, 4, pixels, width * 4);
    if (result == 0) {
        fprintf(stderr, "❌ Failed to write PNG: %s\n", filename);
        return 0;
    }
    printf("✅ Custom atlas saved to %s (%dx%d, %d emojis)\n", filename, width, height, NUM_EMOJIS);
    return 1;
}

// --- Main ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s emoji1 emoji2 emoji3... \n", argv[0]);
        fprintf(stderr, "Example: %s cow pig chicken\n", argv[0]);
        return 1;
    }
    
    // Parse command line arguments to get requested emoji names
    NUM_REQUESTED_EMOJIS = argc - 1;
    for (int i = 0; i < NUM_REQUESTED_EMOJIS; i++) {
        emoji_requested_names[i] = malloc(strlen(argv[i + 1]) + 1);
        strcpy(emoji_requested_names[i], argv[i + 1]);
    }
    
    // Load only the requested emojis from parsed_emojis.txt
    if (!read_needed_emojis_from_file()) {
        fprintf(stderr, "❌ Failed to read needed emojis from file\n");
        free_emoji_data();
        return 1;
    }
    
    if (NUM_EMOJIS == 0) {
        fprintf(stderr, "❌ No requested emojis were found, exiting\n");
        free_emoji_data();
        return 1;
    }
    
    // Calculate actual atlas dimensions based on loaded emojis
    int actual_atlas_width = EMOJI_RENDER_SIZE * NUM_EMOJIS;
    int actual_atlas_height = EMOJI_RENDER_SIZE;
    
    FT_Library ft;
    FT_Face face;
    RGBA_Pixel* atlas = calloc(actual_atlas_width * actual_atlas_height, sizeof(RGBA_Pixel));
    if (!atlas) {
        fprintf(stderr, "Error: Failed to allocate atlas memory\n");
        free_emoji_data();
        return 1;
    }

    const char *emoji_font_paths[] = {
        "./NotoColorEmoji.ttf",                    /* Current directory */
        "../NotoColorEmoji.ttf",                   /* Parent directory */
        "../../NotoColorEmoji.ttf",                /* Two levels up */
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", /* Standard Linux location */
        "/System/Library/Fonts/Apple Color Emoji.ttc",       /* macOS location */
        "C:/Windows/Fonts/segoeui.ttf"             /* Windows fallback */
    };
    
    const char *emoji_font_path = NULL;
    FILE *font_test;
    for (int i = 0; i < sizeof(emoji_font_paths)/sizeof(emoji_font_paths[0]); i++) {
        font_test = fopen(emoji_font_paths[i], "r");
        if (font_test) {
            emoji_font_path = emoji_font_paths[i];
            fclose(font_test);
            printf("✅ Using font: %s\n", emoji_font_path);
            break;
        }
    }
    
    if (!emoji_font_path) {
        fprintf(stderr, "Error: Could not find emoji font in any of the expected locations:\n");
        for (int i = 0; i < sizeof(emoji_font_paths)/sizeof(emoji_font_paths[0]); i++) {
            fprintf(stderr, "  - %s\n", emoji_font_paths[i]);
        }
        free(atlas);
        free_emoji_data();
        return 1;
    }

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Error: Could not init FreeType\n");
        free(atlas);
        free_emoji_data();
        return 1;
    }

    if (FT_New_Face(ft, emoji_font_path, 0, &face)) {
        fprintf(stderr, "Error: Could not load font %s\n", emoji_font_path);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    if (face->num_fixed_sizes == 0) {
        fprintf(stderr, "Error: Font has no fixed sizes\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    // Pick strike closest to EMOJI_RENDER_SIZE (64)
    int best_match = 0;
    int best_diff = abs(face->available_sizes[0].height - EMOJI_RENDER_SIZE);
    for (int i = 1; i < face->num_fixed_sizes; i++) {
        int diff = abs(face->available_sizes[i].height - EMOJI_RENDER_SIZE);
        if (diff < best_diff) {
            best_diff = diff;
            best_match = i;
        }
    }

    if (FT_Select_Size(face, best_match)) {
        fprintf(stderr, "Error: Could not select size index %d\n", best_match);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    int loaded_size = face->available_sizes[best_match].height;
    printf("📌 Selected font strike: %dx%d → rendering at %dx%d\n",
           loaded_size, loaded_size, EMOJI_RENDER_SIZE, EMOJI_RENDER_SIZE);

    for (int i = 0; i < NUM_EMOJIS; i++) {
        const unsigned char* str = (const unsigned char*)emoji_strings[i];
        unsigned int codepoint;
        decode_utf8(str, &codepoint);

        if (codepoint == 0xFE0F) continue;  // Skip variation selector

        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
            fprintf(stderr, "⚠️  Could not load U+%04X (%s)\n", codepoint, emoji_strings[i]);
            // Fill with placeholder pink
            for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                    int ax = i * EMOJI_RENDER_SIZE + x;
                    int ay = y;
                    atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){255, 0, 255, 255};
                }
            }
        } else {
            FT_GlyphSlot slot = face->glyph;
            if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
                fprintf(stderr, "⚠️  U+%04X not in BGRA mode\n", codepoint);
                // Fill with cyan placeholder
                for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                    for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                        int ax = i * EMOJI_RENDER_SIZE + x;
                        int ay = y;
                        atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){0, 255, 255, 255};
                    }
                }
                continue;
            }

            // Get source dimensions
            int src_w = slot->bitmap.width;
            int src_h = slot->bitmap.rows;

            // Calculate scaling factor to fit within EMOJI_RENDER_SIZE
            float scale = (float)EMOJI_RENDER_SIZE / fmaxf((float)src_w, (float)src_h);
            int target_w = (int)(src_w * scale);
            int target_h = (int)(src_h * scale);

            // Use bitmap_top to vertically align relative to baseline
            // bitmap_top = distance from top of bitmap to baseline
            // We want baseline at 80% from top of cell
            int baseline_y = (int)(EMOJI_RENDER_SIZE * 0.8f);
            int scaled_bitmap_top = (int)(slot->bitmap_top * scale);
            int dst_y_offset = baseline_y - scaled_bitmap_top;

            // Horizontal center
            int dst_x_offset = (EMOJI_RENDER_SIZE - target_w) / 2;

            // Safety clamps
            if (dst_y_offset < 0) {
                int shift = -dst_y_offset;
                dst_y_offset = 0;
            }
            if (dst_y_offset + target_h > EMOJI_RENDER_SIZE) {
                int overflow = (dst_y_offset + target_h) - EMOJI_RENDER_SIZE;
                dst_y_offset -= overflow;
                if (dst_y_offset < 0) dst_y_offset = 0;
            }

            // Clear the target cell
            for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                    int ax = i * EMOJI_RENDER_SIZE + x;
                    int ay = y;
                    atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){0, 0, 0, 0};
                }
            }

            // Scale and blit
            for (int y = 0; y < target_h; y++) {
                for (int x = 0; x < target_w; x++) {
                    // Source pixel (nearest neighbor)
                    int src_x = (int)((float)x / scale);
                    int src_y = (int)((float)y / scale);
                    if (src_x >= src_w || src_y >= src_h) continue;

                    int src_idx = src_y * slot->bitmap.pitch + src_x * 4;
                    unsigned char* src_pixel = &slot->bitmap.buffer[src_idx];

                    int dst_x = i * EMOJI_RENDER_SIZE + dst_x_offset + x;
                    int dst_y = dst_y_offset + y;

                    if (dst_x >= actual_atlas_width || dst_y >= actual_atlas_height || dst_y < 0) continue;

                    // Convert BGRA → RGBA
                    atlas[dst_y * actual_atlas_width + dst_x] = (RGBA_Pixel){
                        .r = src_pixel[2],
                        .g = src_pixel[1],
                        .b = src_pixel[0],
                        .a = src_pixel[3]
                    };
                }
            }
        }
    }

    // Generate a unique filename based on the emojis included
    char output_filename[1024];
    strcpy(output_filename, "custom_emoji_atlas_");
    for (int i = 0; i < NUM_EMOJIS; i++) {
        strcat(output_filename, emoji_names[i]);
        if (i < NUM_EMOJIS - 1) strcat(output_filename, "_");
    }
    strcat(output_filename, ".png");

    if (!write_png(output_filename, atlas, actual_atlas_width, actual_atlas_height)) {
        free(atlas);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free_emoji_data();
        return 1;
    }
    
    // Write emoji position mapping for the extraction script to use
    FILE *map_file = fopen("emoji_positions_map.txt", "w");
    if (map_file) {
        fprintf(map_file, "# Emoji Position Mapping\n");
        fprintf(map_file, "# Format: emoji_name|position_index|sanitized_name\n");
        for (int i = 0; i < NUM_EMOJIS; i++) {
            fprintf(map_file, "%s|%d|%s\n", emoji_names[i], i, emoji_names[i]);
        }
        fclose(map_file);
        printf("✅ Wrote emoji position map for %d emojis to emoji_positions_map.txt\n", NUM_EMOJIS);
    } else {
        fprintf(stderr, "⚠️ Warning: Could not write emoji_positions_map.txt\n");
    }

    free(atlas);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    free_emoji_data();

    printf("🎉 CUSTOM ATLAS GENERATED — %d/%d emojis from requested list, size: %dx%d\n", 
           NUM_EMOJIS, NUM_REQUESTED_EMOJIS, actual_atlas_width, actual_atlas_height);
    
    // Output the filename so caller can use it
    printf("Atlas file: %s\n", output_filename);
    return 0;
}