/* nb_worker_canvas_px.c - canvas pixel blend/fill helpers (mechanical extract, single-TU include) */
/* source-over blend of one pixel */
static void nb_px_over(unsigned char *d, const unsigned char s[4], double galpha) {
    double sa = (s[3] / 255.0) * galpha;
    if (sa <= 0.0) return;
    double da = d[3] / 255.0;
    double oa = sa + da * (1.0 - sa);
    for (int i = 0; i < 3; i++)
        d[i] = (unsigned char)((s[i] * sa + d[i] * da * (1.0 - sa)) / (oa > 0 ? oa : 1.0) + 0.5);
    d[3] = (unsigned char)(oa * 255.0 + 0.5);
}
/* fill axis-mapped rect (x0,y0,x1,y1 already in device space, exclusive hi) */
static void nb_px_fill_rect(NbCanvas *cv, int x0, int y0, int x1, int y1,
                            const unsigned char s[4], double galpha) {
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            nb_px_over(cv->px + ((size_t)y * cv->w + x) * 4, s, galpha);
}
/* fill rect through the canvas transform via per-pixel inverse map */
static void nb_px_fill_rect_xf(NbCanvas *cv, double x, double y, double w, double h,
                               const unsigned char s[4], double galpha) {
    double a = cv->m[0], b = cv->m[1], c = cv->m[2], d = cv->m[3], e = cv->m[4], f = cv->m[5];
    double det = a * d - b * c;
    if (det == 0.0) return;
    double xs[4] = {x, x + w, x, x + w}, ys[4] = {y, y, y + h, y + h};
    double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
    for (int i = 0; i < 4; i++) {
        double px = a * xs[i] + c * ys[i] + e, py = b * xs[i] + d * ys[i] + f;
        if (px < x0) x0 = px; if (px > x1) x1 = px;
        if (py < y0) y0 = py; if (py > y1) y1 = py;
    }
    int ix0 = (int)floor(x0), iy0 = (int)floor(y0);
    int ix1 = (int)ceil(x1), iy1 = (int)ceil(y1);
    if (ix0 < 0) ix0 = 0; if (iy0 < 0) iy0 = 0;
    if (ix1 > cv->w) ix1 = cv->w; if (iy1 > cv->h) iy1 = cv->h;
    for (int py = iy0; py < iy1; py++)
        for (int px = ix0; px < ix1; px++) {
            double u = ((px + 0.5 - e) * d - (py + 0.5 - f) * c) / det;
            double v = (a * (py + 0.5 - f) - b * (px + 0.5 - e)) / det;
            if (u >= x && u < x + w && v >= y && v < y + h)
                nb_px_over(cv->px + ((size_t)py * cv->w + px) * 4, s, galpha);
        }
}
