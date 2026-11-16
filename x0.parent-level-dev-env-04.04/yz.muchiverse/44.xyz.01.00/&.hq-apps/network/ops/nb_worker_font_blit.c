/* nb_worker_font_blit.c - minimal TTF glyph raster/blit (mechanical extract, single-TU include) */
/* fill glyph polygon (even-odd) into canvas at pixel size */
static void nb_font_blit(NbCanvas *cv, unsigned gid, double pen_x, double baseline,
                         double scale, const unsigned char s[4], double galpha) {
    static double xs[NB_FONT_PTS_MAX], ys[NB_FONT_PTS_MAX];
    static int ends[32];
    int ncont = 0;
    int npts = nb_font_points(gid, xs, ys, ends, &ncont);
    if (npts <= 0) return;
    /* whole-glyph device bbox */
    double gx0 = 1e18, gx1 = -1e18, gy0 = 1e18, gy1 = -1e18;
    for (int i = 0; i < npts; i++) {
        double px = pen_x + xs[i] * scale, py = baseline - ys[i] * scale;
        if (px < gx0) gx0 = px; if (px > gx1) gx1 = px;
        if (py < gy0) gy0 = py; if (py > gy1) gy1 = py;
    }
    int ix0 = (int)floor(gx0), iy0 = (int)floor(gy0);
    int ix1 = (int)ceil(gx1), iy1 = (int)ceil(gy1);
    if (ix0 < 0) ix0 = 0; if (iy0 < 0) iy0 = 0;
    if (ix1 > cv->w) ix1 = cv->w; if (iy1 > cv->h) iy1 = cv->h;
    int start = 0;
    for (int yy = iy0; yy < iy1; yy++) {
        for (int xx = ix0; xx < ix1; xx++) {
            double px = xx + 0.5, py = yy + 0.5;
            int inside = 0;
            int si = 0;
            for (int ci = 0; ci < ncont; ci++) {
                int en = ends[ci];
                for (int i = si; i <= en; i++) {
                    int j = (i == en) ? si : i + 1;
                    double ax = pen_x + xs[i] * scale, ay = baseline - ys[i] * scale;
                    double bx = pen_x + xs[j] * scale, by = baseline - ys[j] * scale;
                    if ((ay > py) != (by > py)) {
                        double ix = ax + (py - ay) * (bx - ax) / (by - ay);
                        if (ix > px) inside = !inside;
                    }
                }
                si = en + 1;
            }
            if (inside)
                nb_px_over(cv->px + ((size_t)yy * cv->w + xx) * 4, s, galpha);
        }
    }
}
