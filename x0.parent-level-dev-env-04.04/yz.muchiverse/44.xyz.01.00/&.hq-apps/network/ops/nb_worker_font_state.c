/* nb_worker_font_state.c - minimal TTF font state and table lookup (mechanical extract, single-TU include) */
/* ---- minimal TTF (DejaVuSans, ASCII simple glyphs) for canvas fillText ----
 * REAL, NEW 2026-10-07: fillText drew nothing (noop). No TTF library is
 * vendored and none may be added (no new deps), so this reads the system
 * DejaVuSans.ttf directly: sfnt directory, head/maxp/hhea/hmtx, cmap
 * format 4, loca/glyf simple outlines, even-odd scanline raster. ASCII
 * 32..126 only, composites skipped, no kerning/hinting. Enough for real
 * canvas text pixels; full shaping is out of scope. */
static void nb_px_over(unsigned char *d, const unsigned char s[4], double galpha);
static unsigned char *g_font_data = NULL;
static size_t g_font_len = 0;
static unsigned g_font_upm = 2048;
static int g_font_asc = 1901, g_font_desc = -483;
static unsigned g_font_nglyph = 0;
static const unsigned char *g_font_cmap4 = NULL;
static const unsigned char *g_font_loca = NULL;
static const unsigned char *g_font_glyf = NULL;
static const unsigned char *g_font_hmtx = NULL;
static int g_font_loca_short = 1, g_font_nhmtx = 0;
static unsigned u16r(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }
static int i16r(const unsigned char *p) { int v = ((int)p[0] << 8) | p[1]; return v >= 32768 ? v - 65536 : v; }
static unsigned u32r(const unsigned char *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
static int nb_font_tab(const char *tag, const unsigned char **out, unsigned *len) {
    if (!g_font_data || g_font_len < 12) return 0;
    unsigned n = u16r(g_font_data + 4);
    for (unsigned i = 0; i < n; i++) {
        const unsigned char *e = g_font_data + 12 + i * 16;
        if (e + 16 > g_font_data + g_font_len) return 0;
        if (!memcmp(e, tag, 4)) {
            unsigned off = u32r(e + 8), ln = u32r(e + 12);
            if ((size_t)off + ln > g_font_len) return 0;
            *out = g_font_data + off; *len = ln;
            return 1;
        }
    }
    return 0;
}
