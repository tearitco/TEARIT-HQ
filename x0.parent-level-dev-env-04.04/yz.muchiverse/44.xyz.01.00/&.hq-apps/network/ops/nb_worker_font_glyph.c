/* nb_worker_font_glyph.c - minimal TTF glyph index and advance helpers (mechanical extract, single-TU include) */
/* glyph index for ASCII codepoint via cmap4; 0 = missing */
static unsigned nb_font_gid(unsigned cp) {
    if (!g_font_cmap4 || cp > 0xFFFF) return 0;
    const unsigned char *t = g_font_cmap4;
    unsigned segX2 = u16r(t + 6);
    unsigned nseg = segX2 / 2;
    const unsigned char *end = t + 14;
    const unsigned char *start = end + nseg * 2 + 2; /* endCode[] + reservedPad */
    const unsigned char *delta = start + nseg * 2;
    const unsigned char *range = delta + nseg * 2;
    for (unsigned i = 0; i < nseg; i++) {
        unsigned e = u16r(end + i * 2), s = u16r(start + i * 2);
        if (cp < s || cp > e) continue;
        int dl = i16r(delta + i * 2);
        unsigned ro = u16r(range + i * 2);
        if (!ro) return (cp + dl) & 0xFFFF;
        const unsigned char *gp = range + i * 2 + ro + 2 * (cp - s);
        return (u16r(gp) + dl) & 0xFFFF;
    }
    return 0;
}
static int nb_font_advance(unsigned gid) {
    if (!g_font_hmtx || !gid || gid >= g_font_nglyph) return 0;
    unsigned idx = gid < (unsigned)g_font_nhmtx ? gid : (unsigned)(g_font_nhmtx - 1);
    return u16r(g_font_hmtx + idx * 4);
}
