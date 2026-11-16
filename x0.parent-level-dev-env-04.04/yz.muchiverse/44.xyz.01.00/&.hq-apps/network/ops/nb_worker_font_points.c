/* nb_worker_font_points.c - minimal TTF glyph outline decoding (mechanical extract, single-TU include) */
/* decode simple-glyph outline points; returns 0 on composite/empty */
#define NB_FONT_PTS_MAX 512
static int nb_font_points(unsigned gid, double *xs, double *ys, int *contour_end, int *ncont) {
    if (!g_font_loca || !g_font_glyf || gid >= g_font_nglyph) return 0;
    size_t lo = g_font_loca_short ? (size_t)u16r(g_font_loca + gid * 2) * 2
                                  : u32r(g_font_loca + gid * 4);
    size_t hi = g_font_loca_short ? (size_t)u16r(g_font_loca + gid * 2 + 2) * 2
                                  : u32r(g_font_loca + gid * 4 + 4);
    if (hi <= lo) return 0;
    const unsigned char *g = g_font_glyf + lo;
    size_t glen = hi - lo;
    if (glen < 10) return 0;
    int ncontours = i16r(g);
    if (ncontours <= 0) return 0;   /* composite or empty */
    if (ncontours > 32) return 0;
    const unsigned char *ep = g + 10;
    int npts = 0;
    for (int i = 0; i < ncontours; i++) {
        if (ep + 2 > g + glen) return 0;
        int last = u16r(ep); ep += 2;
        if (last >= NB_FONT_PTS_MAX) return 0;
        contour_end[i] = last;
        npts = last + 1;
    }
    unsigned instr_len = u16r(ep);
    ep += 2;
    if ((size_t)(ep - g) + instr_len > glen) return 0;
    /* flags run starts right after instructions */
    const unsigned char *fp = ep + instr_len;
    int fi = 0, x = 0, y = 0;
    static unsigned char flags[NB_FONT_PTS_MAX];
    while (fi < npts) {
        if (fp >= g + glen) return 0;
        unsigned char fl = *fp++;
        flags[fi++] = fl;
        if (fl & 8) {
            if (fp >= g + glen) return 0;
            int rep = *fp++;
            while (rep-- > 0 && fi < npts) flags[fi++] = fl;
        }
    }
    const unsigned char *xp = fp;
    for (int i = 0; i < npts; i++) {
        int dx = 0;
        if (flags[i] & 2) {
            if (xp >= g + glen) return 0;
            dx = *xp++;
            if (!(flags[i] & 16)) dx = -dx;
        } else if (!(flags[i] & 16)) {
            if (xp + 1 >= g + glen) return 0;
            dx = (short)((xp[0] << 8) | xp[1]); xp += 2;
        }
        x += dx;
        xs[i] = (double)x;
    }
    const unsigned char *yp = xp;
    for (int i = 0; i < npts; i++) {
        int dy = 0;
        if (flags[i] & 4) {
            if (yp >= g + glen) return 0;
            dy = *yp++;
            if (!(flags[i] & 32)) dy = -dy;
        } else if (!(flags[i] & 32)) {
            if (yp + 1 >= g + glen) return 0;
            dy = (short)((yp[0] << 8) | yp[1]); yp += 2;
        }
        y += dy;
        ys[i] = (double)y;
    }
    /* on-curve flags live in bit 0; off-curve quadratic midpoints implied.
     * Slice 1 simplification: treat every point as on-curve polygon vertex
     * (drops curve fidelity, keeps letterforms readable at small sizes). */
    *ncont = ncontours;
    return npts;
}
