/* bv_cjk_glyph.c - see bv_cjk_glyph.h.
 *
 * Mirrors the house's existing FreeType usage (ops/emoji_gen_atlas.c),
 * just pointed at a CJK outline font and emitting raw coverage instead
 * of a PNG. Single open Face, tiny (cp,px) cache - bv_render_2d calls
 * this once per visible cell per frame and the distinct glyph set is
 * ~20 (one per terrain legend row + a few actors). */

#include "bv_cjk_glyph.h"

#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define BV_CJK_MAXPX  128
#define BV_CJK_CACHE   16

unsigned int bv_cjk_utf8_first(const char *s) {
    if (!s) return 0;
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] == 0) return 0;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0 && p[1]) return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2])
        return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
        return ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    return 0;
}

/* First that FT_New_Face accepts wins. Noto Sans CJK is the desktop's
 * own CJK fallback; the WenQuanYi entries are the common Debian/Ubuntu
 * alternates. */
static const char *k_font_paths[] = {
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    NULL
};

static FT_Library s_lib;
static FT_Face    s_face;
static int        s_state = 0;   /* 0 untried, 1 ready, -1 failed */

static void ensure_face(void) {
    if (s_state) return;
    s_state = -1;
    if (FT_Init_FreeType(&s_lib)) return;
    for (int i = 0; k_font_paths[i]; i++) {
        if (FT_New_Face(s_lib, k_font_paths[i], 0, &s_face) == 0) { s_state = 1; return; }
    }
}

const unsigned char *bv_cjk_coverage(unsigned int cp, int px) {
    if (!cp) return NULL;
    if (px < 4) px = 4;
    if (px > BV_CJK_MAXPX) px = BV_CJK_MAXPX;

    static struct {
        unsigned int cp;
        int px, ok, used;
        unsigned char buf[BV_CJK_MAXPX * BV_CJK_MAXPX];
    } cache[BV_CJK_CACHE];
    static int rr = 0;

    for (int i = 0; i < BV_CJK_CACHE; i++)
        if (cache[i].used && cache[i].cp == cp && cache[i].px == px)
            return cache[i].ok ? cache[i].buf : NULL;

    int slot = rr; rr = (rr + 1) % BV_CJK_CACHE;
    cache[slot].used = 1;
    cache[slot].cp = cp;
    cache[slot].px = px;
    cache[slot].ok = 0;
    memset(cache[slot].buf, 0, (size_t)px * px);

    ensure_face();
    if (s_state != 1) return NULL;

    int render_px = (px * 80) / 100;          /* margin inside the cell (wide CJK strokes can exceed the em box) */
    if (render_px < 4) render_px = 4;
    if (FT_Set_Pixel_Sizes(s_face, 0, (FT_UInt)render_px)) return NULL;
    if (FT_Load_Char(s_face, cp, FT_LOAD_RENDER)) return NULL;

    FT_GlyphSlot g = s_face->glyph;
    int gw = (int)g->bitmap.width, gh = (int)g->bitmap.rows;
    if (gw <= 0 || gh <= 0) { cache[slot].ok = 1; return cache[slot].buf; } /* blank (e.g. space) is legit */
    if (g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) return NULL;

    int ox = (px - gw) / 2;
    int oy = (px - gh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    int pitch = g->bitmap.pitch;             /* may be negative (bottom-up) */
    for (int y = 0; y < gh; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= px) continue;
        const unsigned char *srow = g->bitmap.buffer + (long)y * pitch;
        for (int x = 0; x < gw; x++) {
            int dx = ox + x;
            if (dx < 0 || dx >= px) continue;
            cache[slot].buf[(size_t)dy * px + dx] = srow[x];
        }
    }
    cache[slot].ok = 1;
    return cache[slot].buf;
}
