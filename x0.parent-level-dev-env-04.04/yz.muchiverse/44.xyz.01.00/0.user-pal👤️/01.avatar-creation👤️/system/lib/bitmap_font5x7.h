/* bitmap_font5x7.h - tiny hand-authored 5x7 pixel font, vendored the same
 * way stb_image.h/stb_image_write.h are (single header, no external
 * dependency, drop-in for a plain RGBA pixel buffer). Covers just the
 * character set a trading card needs: A-Z, 0-9, space, ':', '-', '/',
 * '.' - not a full font, extend the glyph table below if a new
 * character is ever needed on a card.
 *
 * Each glyph is 7 rows x 5 columns, stored as literal ASCII art ('#' =
 * lit, ' ' = unlit) rather than precomputed bitmask bytes, so the data
 * doubles as its own legibility check - what you see in the source is
 * what gets drawn.
 *
 * Usage: bmfont_draw_text(pixels, canvas_w, canvas_h, x, y, "HP:20/20",
 *                          r, g, b, a, scale)
 * draws left-to-right starting at (x,y) (top-left of the text), each
 * font pixel expanded to a scale x scale block, one pixel of spacing
 * between glyphs (scaled). pixels is a tightly-packed W*H*4 RGBA buffer,
 * top-left origin, row-major - matches stb_image_write's own convention. */
#ifndef BITMAP_FONT5X7_H
#define BITMAP_FONT5X7_H

#include <string.h>

typedef struct {
    char ch;
    const char *rows[7]; /* each row is exactly 5 chars: '#' or ' ' */
} BMFontGlyph;

static const BMFontGlyph BMFONT_GLYPHS[] = {
    {'0', {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "}},
    {'1', {"  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "}},
    {'2', {" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"}},
    {'3', {" ### ", "#   #", "    #", "  ## ", "    #", "#   #", " ### "}},
    {'4', {"   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "}},
    {'5', {"#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "}},
    {'6', {" ### ", "#    ", "#### ", "#   #", "#   #", "#   #", " ### "}},
    {'7', {"#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "}},
    {'8', {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "}},
    {'9', {" ### ", "#   #", "#   #", " ####", "    #", "    #", " ### "}},
    {'A', {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
    {'B', {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "}},
    {'C', {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "}},
    {'D', {"#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "}},
    {'E', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"}},
    {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
    {'G', {" ### ", "#   #", "#    ", "#  ##", "#   #", "#   #", " ### "}},
    {'H', {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
    {'I', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"}},
    {'J', {"    #", "    #", "    #", "    #", "#   #", "#   #", " ### "}},
    {'K', {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"}},
    {'L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"}},
    {'M', {"#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"}},
    {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"}},
    {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
    {'Q', {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"}},
    {'R', {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"}},
    {'S', {" ### ", "#   #", "#    ", " ### ", "    #", "#   #", " ### "}},
    {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "}},
    {'U', {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'V', {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "}},
    {'W', {"#   #", "#   #", "#   #", "#   #", "# # #", "## ##", "#   #"}},
    {'X', {"#   #", " # # ", "  #  ", "  #  ", "  #  ", " # # ", "#   #"}},
    {'Y', {"#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "}},
    {'Z', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"}},
    {' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "}},
    {':', {"     ", "  #  ", "     ", "     ", "     ", "  #  ", "     "}},
    {'-', {"     ", "     ", "     ", " ### ", "     ", "     ", "     "}},
    {'/', {"    #", "   # ", "   # ", "  #  ", " #   ", " #   ", "#    "}},
    {'.', {"     ", "     ", "     ", "     ", "     ", "  #  ", "     "}},
};
#define BMFONT_GLYPH_COUNT (int)(sizeof(BMFONT_GLYPHS) / sizeof(BMFONT_GLYPHS[0]))

static const BMFontGlyph *bmfont_find(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A'); /* fold lowercase to uppercase glyphs */
    for (int i = 0; i < BMFONT_GLYPH_COUNT; i++) {
        if (BMFONT_GLYPHS[i].ch == c) return &BMFONT_GLYPHS[i];
    }
    return NULL; /* unknown chars just draw nothing, not a placeholder box */
}

static void bmfont_set_pixel(unsigned char *pixels, int canvas_w, int canvas_h, int x, int y,
                              unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (x < 0 || y < 0 || x >= canvas_w || y >= canvas_h) return;
    unsigned char *p = &pixels[(y * canvas_w + x) * 4];
    p[0] = r; p[1] = g; p[2] = b; p[3] = a;
}

static int bmfont_draw_char(unsigned char *pixels, int canvas_w, int canvas_h, int x, int y, char c,
                             unsigned char r, unsigned char g, unsigned char b, unsigned char a, int scale) {
    const BMFontGlyph *glyph = bmfont_find(c);
    if (!glyph) return (5 * scale) + scale; /* still advance by one glyph width + spacing */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (glyph->rows[row][col] != '#') continue;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    bmfont_set_pixel(pixels, canvas_w, canvas_h, x + col * scale + sx, y + row * scale + sy, r, g, b, a);
                }
            }
        }
    }
    return (5 * scale) + scale; /* glyph width + one scaled pixel of spacing */
}

/* Draws left-to-right starting at (x,y), wrapping never - caller is
 * responsible for laying out multiple lines. */
static void bmfont_draw_text(unsigned char *pixels, int canvas_w, int canvas_h, int x, int y, const char *text,
                              unsigned char r, unsigned char g, unsigned char b, unsigned char a, int scale) {
    int cx = x;
    for (const char *c = text; *c; c++) {
        cx += bmfont_draw_char(pixels, canvas_w, canvas_h, cx, y, *c, r, g, b, a, scale);
    }
}

#endif /* BITMAP_FONT5X7_H */
