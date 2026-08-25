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
#define MAX_TOTAL_EMOJIS 4096  // Maximum emojis in parsed_emojis.txt
#define MAX_NEEDED_EMOJIS 64   // Maximum emojis we'll actually need
#define MAX_NAME_LEN 256       // Max length of emoji name

// We'll work in RGBA for PNG
typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

// Structures for available emojis
char *all_emoji_strings[MAX_TOTAL_EMOJIS];
char *all_emoji_names[MAX_TOTAL_EMOJIS];  // Original names like "pig face"
char *all_emoji_lookup_keys[MAX_TOTAL_EMOJIS];  // Sanitized names for lookup
int ALL_EMOJIS_COUNT = 0;

// Structures for needed emojis
char *needed_emoji_strings[MAX_NEEDED_EMOJIS];
char *needed_emoji_names[MAX_NEEDED_EMOJIS];
int needed_emoji_indices[MAX_NEEDED_EMOJIS];  // Index in the original list
int NEEDED_EMOJIS_COUNT = 0;

// Function to sanitize emoji name for lookup
char* sanitize_name(const char* input) {
    char* result = malloc(MAX_NAME_LEN);
    int j = 0;
    for (int i = 0; input[i] && j < MAX_NAME_LEN-1; i++) {
        if ((input[i] >= 'a' && input[i] <= 'z') ||
            (input[i] >= 'A' && input[i] <= 'Z') ||
            (input[i] >= '0' && input[i] <= '9') ||
            input[i] == '_' || input[i] == '-') {
            result[j++] = input[i];
        } else {
            result[j++] = '_';
        }
    }
    result[j] = '\0';
    return result;
}

// Function to read ALL emojis from parsed_emojis.txt
int read_all_emojis_from_file() {
    FILE *file = fopen("parsed_emojis.txt", "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open parsed_emojis.txt\n");
        return 0;
    }

    char line[1024];
    int count = 0;
    
    while (fgets(line, sizeof(line), file) && count < MAX_TOTAL_EMOJIS) {
        if (line[0] == '#' || strlen(line) < 5) continue;  // Skip comment lines and empty lines
        
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        
        // Parse the line: codepoint_hex_string|emoji_string|emoji_name
        char *token = strtok(line, "|");
        if (!token) continue;
        
        // Get emoji string (second part)
        token = strtok(NULL, "|");
        if (!token) continue;
        all_emoji_strings[count] = malloc(strlen(token) + 1);
        strcpy(all_emoji_strings[count], token);
        
        // Get emoji name (third part)
        token = strtok(NULL, "|");
        if (!token) {
            free(all_emoji_strings[count]);
            continue;
        }
        all_emoji_names[count] = malloc(strlen(token) + 1);
        strcpy(all_emoji_names[count], token);
        
        // Create sanitized version for lookup
        all_emoji_lookup_keys[count] = sanitize_name(token);
        
        count++;
    }
    
    fclose(file);
    ALL_EMOJIS_COUNT = count;
    printf("✅ Loaded %d total emojis from parsed_emojis.txt\n", ALL_EMOJIS_COUNT);
    return 1;
}

// Function to find emoji by name (with fuzzy matching)
int find_emoji_by_name(const char* requested_name) {
    char* sanitized_requested = sanitize_name(requested_name);
    
    // First, try exact match
    for (int i = 0; i < ALL_EMOJIS_COUNT; i++) {
        if (strcmp(all_emoji_lookup_keys[i], sanitized_requested) == 0) {
            printf("Found exact match: '%s' -> '%s'\n", requested_name, all_emoji_names[i]);
            free(sanitized_requested);
            return i;
        }
    }
    
    // Next, try substring match (if requested name is contained in emoji name)
    for (int i = 0; i < ALL_EMOJIS_COUNT; i++) {
        char* emoji_sanitized_lower = malloc(strlen(all_emoji_lookup_keys[i]) + 1);
        strcpy(emoji_sanitized_lower, all_emoji_lookup_keys[i]);
        
        // Convert to lowercase for comparison
        for (int j = 0; emoji_sanitized_lower[j]; j++) {
            if (emoji_sanitized_lower[j] >= 'A' && emoji_sanitized_lower[j] <= 'Z')
                emoji_sanitized_lower[j] = emoji_sanitized_lower[j] + 32;
        }
        
        char* requested_lower = malloc(strlen(sanitized_requested) + 1);
        strcpy(requested_lower, sanitized_requested);
        for (int j = 0; requested_lower[j]; j++) {
            if (requested_lower[j] >= 'A' && requested_lower[j] <= 'Z')
                requested_lower[j] = requested_lower[j] + 32;
        }
        
        if (strstr(emoji_sanitized_lower, requested_lower) != NULL) {
            printf("Found partial match: '%s' -> '%s'\n", requested_name, all_emoji_names[i]);
            free(sanitized_requested);
            free(emoji_sanitized_lower);
            free(requested_lower);
            return i;
        }
        
        free(emoji_sanitized_lower);
        free(requested_lower);
    }
    
    free(sanitized_requested);
    return -1; // Not found
}

// Function to add needed emojis based on command-line arguments
int setup_needed_emojis(int argc, char *argv[]) {
    if (argc <= 1) {
        fprintf(stderr, "Usage: %s <emoji1> <emoji2> ...\n", argv[0]);
        return 0;
    }
    
    printf("🔍 Looking for %d requested emojis...\n", argc - 1);
    
    for (int i = 1; i < argc; i++) {
        printf("Searching for: %s\n", argv[i]);
        int idx = find_emoji_by_name(argv[i]);
        if (idx == -1) {
            fprintf(stderr, "❌ Could not find emoji: %s\n", argv[i]);
            continue; // Skip this emoji
        }
        
        // Add to needed list
        needed_emoji_strings[NEEDED_EMOJIS_COUNT] = malloc(strlen(all_emoji_strings[idx]) + 1);
        strcpy(needed_emoji_strings[NEEDED_EMOJIS_COUNT], all_emoji_strings[idx]);
        
        needed_emoji_names[NEEDED_EMOJIS_COUNT] = malloc(strlen(all_emoji_names[idx]) + 1);
        strcpy(needed_emoji_names[NEEDED_EMOJIS_COUNT], all_emoji_names[idx]);
        
        needed_emoji_indices[NEEDED_EMOJIS_COUNT] = idx;  // Store original index
        
        NEEDED_EMOJIS_COUNT++;
        printf("✓ Added emoji: %s at position %d\n", all_emoji_names[idx], NEEDED_EMOJIS_COUNT - 1);
    }
    
    if (NEEDED_EMOJIS_COUNT == 0) {
        fprintf(stderr, "❌ No emojis could be found\n");
        return 0;
    }
    
    printf("✅ Preparing to render %d needed emojis\n", NEEDED_EMOJIS_COUNT);
    return 1;
}

// Function to free allocated memory
void free_all_emoji_data() {
    for (int i = 0; i < ALL_EMOJIS_COUNT; i++) {
        if (all_emoji_strings[i]) free(all_emoji_strings[i]);
        if (all_emoji_names[i]) free(all_emoji_names[i]);
        if (all_emoji_lookup_keys[i]) free(all_emoji_lookup_keys[i]);
    }
}

void free_needed_emoji_data() {
    for (int i = 0; i < NEEDED_EMOJIS_COUNT; i++) {
        if (needed_emoji_strings[i]) free(needed_emoji_strings[i]);
        if (needed_emoji_names[i]) free(needed_emoji_names[i]);
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
    printf("✅ Small atlas saved to %s\n", filename);
    return 1;
}

// --- Main ---
int main(int argc, char *argv[]) {
    // Load ALL emojis from parsed_emojis.txt first
    if (!read_all_emojis_from_file()) {
        fprintf(stderr, "❌ Failed to load emojis from file\n");
        return 1;
    }
    
    // Setup needed emojis based on command line args
    if (!setup_needed_emojis(argc, argv)) {
        fprintf(stderr, "❌ Failed to setup needed emojis\n");
        free_all_emoji_data();
        return 1;
    }
    
    // Calculate atlas dimensions for only needed emojis
    int atlas_width = EMOJI_RENDER_SIZE * NEEDED_EMOJIS_COUNT;
    int atlas_height = EMOJI_RENDER_SIZE;
    
    FT_Library ft;
    FT_Face face;
    RGBA_Pixel* atlas = calloc(atlas_width * atlas_height, sizeof(RGBA_Pixel));
    if (!atlas) {
        fprintf(stderr, "Error: Failed to allocate atlas memory\n");
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Error: Could not init FreeType\n");
        free(atlas);
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    if (FT_New_Face(ft, emoji_font_path, 0, &face)) {
        fprintf(stderr, "Error: Could not load font %s\n", emoji_font_path);
        FT_Done_FreeType(ft);
        free(atlas);
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    if (face->num_fixed_sizes == 0) {
        fprintf(stderr, "Error: Font has no fixed sizes\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_all_emoji_data();
        free_needed_emoji_data();
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
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    int loaded_size = face->available_sizes[best_match].height;
    printf("📌 Selected font strike: %dx%d → rendering at %dx%d\n",
           loaded_size, loaded_size, EMOJI_RENDER_SIZE, EMOJI_RENDER_SIZE);

    // Create mapping file to tell the extraction script where each emoji is
    FILE *mapping_file = fopen("emoji_positions_map.txt", "w");
    if (!mapping_file) {
        fprintf(stderr, "Error: Could not create emoji_positions_map.txt\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    fprintf(mapping_file, "# Emoji Position Mapping\n");
    fprintf(mapping_file, "# Format: emoji_name|position_index|sanitized_name\n");

    for (int i = 0; i < NEEDED_EMOJIS_COUNT; i++) {
        const unsigned char* str = (const unsigned char*)needed_emoji_strings[i];
        unsigned int codepoint;
        decode_utf8(str, &codepoint);

        if (codepoint == 0xFE0F) continue;  // Skip variation selector

        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
            fprintf(stderr, "⚠️  Could not load U+%04X (%s)\n", codepoint, needed_emoji_strings[i]);
            // Fill with placeholder pink
            for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                    int ax = i * EMOJI_RENDER_SIZE + x;
                    int ay = y;
                    atlas[ay * atlas_width + ax] = (RGBA_Pixel){255, 0, 255, 255};
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
                        atlas[ay * atlas_width + ax] = (RGBA_Pixel){0, 255, 255, 255};
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
                    atlas[ay * atlas_width + ax] = (RGBA_Pixel){0, 0, 0, 0};
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

                    if (dst_x >= atlas_width || dst_y >= atlas_height || dst_y < 0) continue;

                    // Convert BGRA → RGBA
                    atlas[dst_y * atlas_width + dst_x] = (RGBA_Pixel){
                        .r = src_pixel[2],
                        .g = src_pixel[1],
                        .b = src_pixel[0],
                        .a = src_pixel[3]
                    };
                }
            }
        }
        
        // Write position mapping
        char* sanitized_name = sanitize_name(needed_emoji_names[i]);
        fprintf(mapping_file, "%s|%d|%s\n", needed_emoji_names[i], i, sanitized_name);
        free(sanitized_name);
    }

    fclose(mapping_file);
    printf("✅ Emoji position mapping saved to emoji_positions_map.txt\n");

    // Create a meaningful filename
    char output_filename[1024];
    snprintf(output_filename, sizeof(output_filename), "small_emoji_atlas_");
    
    for (int i = 0; i < NEEDED_EMOJIS_COUNT; i++) {
        char* sanitized = sanitize_name(needed_emoji_names[i]);
        strcat(output_filename, sanitized);
        if (i < NEEDED_EMOJIS_COUNT - 1) strcat(output_filename, "_");
        free(sanitized);
    }
    strcat(output_filename, ".png");

    if (!write_png(output_filename, atlas, atlas_width, atlas_height)) {
        free(atlas);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free_all_emoji_data();
        free_needed_emoji_data();
        return 1;
    }

    free(atlas);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    free_all_emoji_data();
    free_needed_emoji_data();

    printf("🎉 SMALL EMOJI ATLAS GENERATED — %d/%d emojis requested, size: %dx%d\n", 
           NEEDED_EMOJIS_COUNT, argc-1, atlas_width, atlas_height);
    printf("Atlas file: %s\n", output_filename);
    return 0;
}