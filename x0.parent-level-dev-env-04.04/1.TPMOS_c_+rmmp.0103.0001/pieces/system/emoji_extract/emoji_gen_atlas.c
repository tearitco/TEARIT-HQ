/* emoji_gen_atlas.c -- standalone TPM Op.
 * Renders ONE emoji symbol to a small RGBA PNG using FreeType's color
 * bitmap path. Ported from the proven reference at
 * x0.parent-level-dev-env-02.01/#.emoji-studio-501.02.05t/
 * &.emoji-studio-solo.02.01/emoji-gen-atlas.c (verified working there),
 * per wraimoji-j12-root-cause-and-fix-plan.txt Part 3 Step 1 -- the
 * FreeType calls here are copied, not reinvented, because getting them
 * wrong (missing FT_LOAD_COLOR, using FT_Set_Pixel_Sizes instead of
 * FT_Select_Size on a fixed-size embedded-bitmap font) is exactly the
 * bug this plan exists to fix. Only change from the reference: the
 * font path is read from pieces/config/wraith_debug.conf's
 * [emoji_rendering] font_path key (matching wraith_rgb_daemon.c's own
 * load_emoji_config() convention) instead of being hardcoded, so both
 * consumers honor the same config.
 *
 * Usage: emoji_gen_atlas.+x <emoji_symbol_utf8> <output_png_path>
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define EMOJI_SIZE 64
#define DEFAULT_FONT_PATH "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"

typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

static void load_font_path(char *out, size_t out_sz) {
    FILE *f;
    char line[1024];
    int in_section = 0;

    strncpy(out, DEFAULT_FONT_PATH, out_sz - 1);
    out[out_sz - 1] = '\0';

    f = fopen("pieces/config/wraith_debug.conf", "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "[emoji_rendering]")) { in_section = 1; continue; }
        if (line[0] == '[') { in_section = 0; continue; }
        if (!in_section) continue;
        if (strncmp(line, "font_path=", 10) == 0) {
            char *val = line + 10;
            val[strcspn(val, "\n\r")] = '\0';
            if (val[0]) {
                strncpy(out, val, out_sz - 1);
                out[out_sz - 1] = '\0';
            }
        }
    }
    fclose(f);
}

static int decode_utf8(const unsigned char *str, unsigned int *codepoint) {
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
        fprintf(stderr, "Usage: %s <emoji_symbol_utf8> <output_png>\n", argv[0]);
        return 1;
    }

    const char *emoji_str = argv[1];
    const char *output_path = argv[2];
    char font_path[1024];

    load_font_path(font_path, sizeof(font_path));

    FT_Library ft;
    FT_Face face;
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "FT_Init_FreeType failed\n");
        return 1;
    }
    if (FT_New_Face(ft, font_path, 0, &face)) {
        fprintf(stderr, "FT_New_Face failed for %s\n", font_path);
        return 1;
    }

    /* Noto Color Emoji is a fixed-size embedded-bitmap font (CBDT/CBLC).
       FT_Set_Pixel_Sizes() cannot synthesize an arbitrary size from it --
       must select one of its actual embedded strikes via FT_Select_Size().
       This exact distinction (and getting it wrong) is
       wraimoji-j12-root-cause-and-fix-plan.txt's root cause #1. */
    if (face->num_fixed_sizes > 0) {
        FT_Select_Size(face, 0);
    } else {
        FT_Set_Pixel_Sizes(face, 0, EMOJI_SIZE);
    }

    RGBA_Pixel *buffer = calloc(EMOJI_SIZE * EMOJI_SIZE, sizeof(RGBA_Pixel));
    if (!buffer) {
        fprintf(stderr, "calloc failed\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return 1;
    }

    unsigned int codepoint;
    decode_utf8((const unsigned char *)emoji_str, &codepoint);

    /* FT_LOAD_COLOR is required for FreeType to decode the embedded
       color bitmap layer at all -- root cause #2 without it. */
    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR) == 0) {
        FT_GlyphSlot slot = face->glyph;
        int src_w = slot->bitmap.width;
        int src_h = slot->bitmap.rows;

        if (src_w > 0 && src_h > 0) {
            float scale = (float)EMOJI_SIZE / fmaxf((float)src_w, (float)src_h);
            int target_w = (int)(src_w * scale);
            int target_h = (int)(src_h * scale);

            int baseline_y = (int)(EMOJI_SIZE * 0.8f);
            int scaled_bitmap_top = (int)(slot->bitmap_top * scale);
            int dst_y_offset = baseline_y - scaled_bitmap_top;
            int dst_x_offset = (EMOJI_SIZE - target_w) / 2;

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

                    /* Color glyphs are FT_PIXEL_MODE_BGRA (4 bytes/pixel);
                       treating this as 1-byte grayscale is root cause #3.
                       Also handle FT_PIXEL_MODE_GRAY as a white-glyph
                       alpha mask, for non-color fallback fonts. */
                    if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                        unsigned char *src_pixel = &slot->bitmap.buffer[(src_y * slot->bitmap.pitch) + (src_x * 4)];
                        buffer[dy * EMOJI_SIZE + dx] = (RGBA_Pixel){src_pixel[2], src_pixel[1], src_pixel[0], src_pixel[3]};
                    } else if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                        unsigned char val = slot->bitmap.buffer[src_y * slot->bitmap.pitch + src_x];
                        buffer[dy * EMOJI_SIZE + dx] = (RGBA_Pixel){255, 255, 255, val};
                    }
                }
            }
        }
    } else {
        fprintf(stderr, "FT_Load_Char failed for codepoint U+%X\n", codepoint);
    }

    if (!stbi_write_png(output_path, EMOJI_SIZE, EMOJI_SIZE, 4, buffer, EMOJI_SIZE * 4)) {
        fprintf(stderr, "stbi_write_png failed for %s\n", output_path);
        free(buffer);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return 1;
    }

    free(buffer);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
