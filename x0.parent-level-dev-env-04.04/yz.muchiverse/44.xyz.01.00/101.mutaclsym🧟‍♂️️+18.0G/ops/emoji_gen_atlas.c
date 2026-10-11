#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define EMOJI_SIZE 64

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

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
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <emoji_symbol> <output_png>\n", argv[0]);
        return 1;
    }

    const char* emoji_str = argv[1];
    const char* output_path = argv[2];

    FT_Library ft;
    FT_Face face;
    if (FT_Init_FreeType(&ft)) return 1;
    /* 2026-10-07: was a single hard-coded Linux path and a silent return 1, so the Mac (Apple's own emoji font) never made a sprite. Search order:
     * $EMOJI_FONT, Noto (Linux), Apple Color Emoji (macOS); say which font was missing instead of failing silently. */
    {
        const char *envf = getenv("EMOJI_FONT");
        const char *fonts[] = { envf, "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", "/usr/share/fonts/noto/NotoColorEmoji.ttf",
                                "/System/Library/Fonts/Apple Color Emoji.ttc", "/Library/Fonts/Apple Color Emoji.ttc", NULL };
        int loaded = 0;
        for (int fi = 0; fi < 5 && !loaded; fi++)
            if (fonts[fi] && fonts[fi][0] && FT_New_Face(ft, fonts[fi], 0, &face) == 0) loaded = 1;
        if (!loaded) { fprintf(stderr, "emoji_gen_atlas: no color emoji font found (set EMOJI_FONT, or install Noto Color Emoji / use macOS Apple Color Emoji)\n"); return 3; }
    }

    if (face->num_fixed_sizes > 0) {
        /* bitmap-strike fonts: Noto has one strike (index 0, unchanged); Apple Color Emoji has several, so take the LARGEST (index 0 is tiny) */
        int best = 0;
        for (int si = 1; si < face->num_fixed_sizes; si++)
            if (face->available_sizes[si].height > face->available_sizes[best].height) best = si;
        FT_Select_Size(face, best);
    } else {
        FT_Set_Pixel_Sizes(face, 0, EMOJI_SIZE);
    }

    RGBA_Pixel* buffer = calloc(EMOJI_SIZE * EMOJI_SIZE, sizeof(RGBA_Pixel));

    unsigned int codepoint;
    decode_utf8((const unsigned char*)emoji_str, &codepoint);

    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR) == 0) {
        FT_GlyphSlot slot = face->glyph;
        int src_w = slot->bitmap.width;
        int src_h = slot->bitmap.rows;
        
        if (src_w > 0 && src_h > 0) {
            float scale = (float)EMOJI_SIZE / fmaxf((float)src_w, (float)src_h);
            int target_w = (int)(src_w * scale);
            int target_h = (int)(src_h * scale);

            // Centering logic from example
            int baseline_y = (int)(EMOJI_SIZE * 0.8f);
            int scaled_bitmap_top = (int)(slot->bitmap_top * scale);
            int dst_y_offset = baseline_y - scaled_bitmap_top;
            int dst_x_offset = (EMOJI_SIZE - target_w) / 2;

            // Clamps
            if (dst_y_offset < 0) dst_y_offset = 0;
            if (dst_y_offset + target_h > EMOJI_SIZE) dst_y_offset = EMOJI_SIZE - target_h;

            for (int y = 0; y < target_h; y++) {
                for (int x = 0; x < target_w; x++) {
                    int src_x = (int)(x / scale);
                    int src_y = (int)(y / scale);
                    if (src_x >= src_w || src_y >= src_h) continue;

                    int dy = dst_y_offset + y;
                    int dx = dst_x_offset + x;
                    if (dx >= EMOJI_SIZE || dy >= EMOJI_SIZE) continue;

                    if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                        unsigned char* src_pixel = &slot->bitmap.buffer[(src_y * slot->bitmap.pitch) + (src_x * 4)];
                        buffer[dy * EMOJI_SIZE + dx] = (RGBA_Pixel){src_pixel[2], src_pixel[1], src_pixel[0], src_pixel[3]};
                    } else if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                        unsigned char val = slot->bitmap.buffer[src_y * slot->bitmap.pitch + src_x];
                        buffer[dy * EMOJI_SIZE + dx] = (RGBA_Pixel){255, 255, 255, val};
                    }
                }
            }
        }
    }

    stbi_write_png(output_path, EMOJI_SIZE, EMOJI_SIZE, 4, buffer, EMOJI_SIZE * 4);

    free(buffer);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
