#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* 
 * font-gen-op.c - TPM Font Muscle
 * Extracts a character from a TTF font and converts it to a .txt grid.
 * Standards: Human-readable (#/.), Piece-based directory structure.
 */

#define TARGET_W 8
#define TARGET_H 16

void write_glyph_txt(const char *path, int w, int h, unsigned char *buffer, int pitch) {
    FILE *f = fopen(path, "w");
    if (!f) return;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Threshold at 128 for '#' vs '.'
            if (buffer[y * pitch + x] > 128) {
                fputc('#', f);
            } else {
                fputc('.', f);
            }
        }
        fputc('\n', f);
    }
    fclose(f);
}

void write_piece_pdl(const char *path, int w, int h, int baseline, int advance) {
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "SECTION | KEY | VALUE\n");
    fprintf(f, "--------|-----|------\n");
    fprintf(f, "META    | width | %d\n", w);
    fprintf(f, "META    | height | %d\n", h);
    fprintf(f, "META    | baseline | %d\n", baseline);
    fprintf(f, "META    | advance | %d\n", advance);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <font_path> <char_code> <output_dir>\n", argv[0]);
        return 1;
    }

    const char *font_path = argv[1];
    unsigned int char_code = (unsigned int)atoi(argv[2]);
    const char *out_dir = argv[3];

    FT_Library ft;
    FT_Face face;

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Error: Could not init FreeType\n");
        return 1;
    }

    if (FT_New_Face(ft, font_path, 0, &face)) {
        fprintf(stderr, "Error: Could not load font %s\n", font_path);
        FT_Done_FreeType(ft);
        return 1;
    }

    // Set size to fit our target cell
    FT_Set_Pixel_Sizes(face, 0, TARGET_H - 2); // Leave 2px for padding

    if (FT_Load_Char(face, char_code, FT_LOAD_RENDER)) {
        fprintf(stderr, "Error: Could not load char %u\n", char_code);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return 1;
    }

    mkdir(out_dir, 0777);

    char glyph_path[1024];
    char pdl_path[1024];
    snprintf(glyph_path, sizeof(glyph_path), "%s/glyph.txt", out_dir);
    snprintf(pdl_path, sizeof(pdl_path), "%s/piece.pdl", out_dir);

    FT_GlyphSlot slot = face->glyph;
    printf("DEBUG: Glyph metrics for char %u: w=%d, h=%d, top=%d, left=%d, pitch=%d\n", 
           char_code, slot->bitmap.width, slot->bitmap.rows, slot->bitmap_top, slot->bitmap_left, slot->bitmap.pitch);
    
    // Create a local buffer for the target cell
    unsigned char *cell = calloc(TARGET_W * TARGET_H, 1);
    
    // Centering/Baseline logic
    int baseline = 12; // TARGET_H * 0.75
    int start_y = baseline - slot->bitmap_top;
    int start_x = (TARGET_W - (int)slot->bitmap.width) / 2;

    printf("DEBUG: Mapping at start_x=%d, start_y=%d\n", start_x, start_y);

    for (int y = 0; y < (int)slot->bitmap.rows; y++) {
        for (int x = 0; x < (int)slot->bitmap.width; x++) {
            int dy = start_y + y;
            int dx = start_x + x;
            if (dx >= 0 && dx < TARGET_W && dy >= 0 && dy < TARGET_H) {
                unsigned char val = slot->bitmap.buffer[y * slot->bitmap.pitch + x];
                cell[dy * TARGET_W + dx] = val;
                if (val > 0) printf("DEBUG: Found pixel at %d,%d val=%u\n", dx, dy, val);
            }
        }
    }

    write_glyph_txt(glyph_path, TARGET_W, TARGET_H, cell, TARGET_W);
    write_piece_pdl(pdl_path, TARGET_W, TARGET_H, baseline, slot->advance.x >> 6);

    printf("SUCCESS: Generated piece for char %u in %s\n", char_code, out_dir);

    free(cell);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
