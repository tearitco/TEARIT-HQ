#ifndef BV_CJK_GLYPH_H
#define BV_CJK_GLYPH_H

/* bv_cjk_glyph - rasterise one Unicode codepoint (built for CJK "tile
 * substitute" glyphs, but works for any scalar) to an 8-bit coverage
 * bitmap, <px> x <px>, centred in the box. Returns a pointer to a
 * small internal cache buffer (valid until the next call that lands on
 * a different cache slot); NULL if FreeType / the font is unavailable
 * or the glyph is missing. No colour - the caller tints the coverage.
 *
 * This is the ASCII/CJK terminal view for PCHQ-2D-TILE-VIEW.md (the
 * `` ` `` toggle, view_2d_style=ascii). Font code lives here, not in
 * bv_render_2d.c. */
const unsigned char *bv_cjk_coverage(unsigned int cp, int px);

/* First UTF-8 scalar value of s (0 if empty / invalid). */
unsigned int bv_cjk_utf8_first(const char *s);

#endif /* BV_CJK_GLYPH_H */
