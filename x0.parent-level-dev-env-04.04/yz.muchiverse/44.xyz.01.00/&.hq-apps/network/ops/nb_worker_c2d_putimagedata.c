/* nb_worker_c2d_putimagedata.c - canvas 2D putImageData implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_putImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv || argc < 3) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    int sw = 0, sh = 0, owned = 0;
    unsigned char *spx = NULL;
    if (!nb_c2d_source(ctx, argv[0], &sw, &sh, &spx, &owned)) return JS_UNDEFINED;
    int dx = (int)nb_c2d_argd(ctx, argv, argc, 1, 0), dy = (int)nb_c2d_argd(ctx, argv, argc, 2, 0);
    int sx = 0, sy = 0, dw = sw, dh = sh;
    if (argc >= 7) {
        sx = (int)nb_c2d_argd(ctx, argv, argc, 3, 0); sy = (int)nb_c2d_argd(ctx, argv, argc, 4, 0);
        dw = (int)nb_c2d_argd(ctx, argv, argc, 5, sw); dh = (int)nb_c2d_argd(ctx, argv, argc, 6, sh);
    }
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            int qx = sx + x, qy = sy + y, tx = dx + x, ty = dy + y;
            if (qx < 0 || qy < 0 || qx >= sw || qy >= sh) continue;
            if (tx < 0 || ty < 0 || tx >= cv->w || ty >= cv->h) continue;
            memcpy(cv->px + ((size_t)ty * cv->w + tx) * 4, spx + ((size_t)qy * sw + qx) * 4, 4);
        }
    if (owned) free(spx);
    return JS_UNDEFINED;
}
static JSValue nb_c2d_drawImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv || argc < 3) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    int sw = 0, sh = 0, owned = 0;
    unsigned char *spx = NULL;
    if (!nb_c2d_source(ctx, argv[0], &sw, &sh, &spx, &owned)) return JS_UNDEFINED;
    double sx = 0, sy = 0, sdw = sw, sdh = sh, dx, dy, dw, dh;
    if (argc >= 9) {
        sx = nb_c2d_argd(ctx, argv, argc, 1, 0); sy = nb_c2d_argd(ctx, argv, argc, 2, 0);
        sdw = nb_c2d_argd(ctx, argv, argc, 3, sw); sdh = nb_c2d_argd(ctx, argv, argc, 4, sh);
        dx = nb_c2d_argd(ctx, argv, argc, 5, 0); dy = nb_c2d_argd(ctx, argv, argc, 6, 0);
        dw = nb_c2d_argd(ctx, argv, argc, 7, sdw); dh = nb_c2d_argd(ctx, argv, argc, 8, sdh);
    } else if (argc >= 5) {
        dx = nb_c2d_argd(ctx, argv, argc, 1, 0); dy = nb_c2d_argd(ctx, argv, argc, 2, 0);
        dw = nb_c2d_argd(ctx, argv, argc, 3, sdw); dh = nb_c2d_argd(ctx, argv, argc, 4, sdh);
    } else {
        dx = nb_c2d_argd(ctx, argv, argc, 1, 0); dy = nb_c2d_argd(ctx, argv, argc, 2, 0);
        dw = sdw; dh = sdh;
    }
    if (dw <= 0 || dh <= 0 || sdw <= 0 || sdh <= 0) { if (owned) free(spx); return JS_UNDEFINED; }
    double ga = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    double a = cv->m[0], b = cv->m[1], c = cv->m[2], d = cv->m[3], e = cv->m[4], f = cv->m[5];
    double det = a * d - b * c;
    double xs[4] = {dx, dx + dw, dx, dx + dw}, ys[4] = {dy, dy, dy + dh, dy + dh};
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
    if (det != 0.0) {
        for (int py = iy0; py < iy1; py++)
            for (int px = ix0; px < ix1; px++) {
                double u = ((px + 0.5 - e) * d - (py + 0.5 - f) * c) / det;
                double v = (a * (py + 0.5 - f) - b * (px + 0.5 - e)) / det;
                double fx = sx + (u - dx) * sdw / dw, fy = sy + (v - dy) * sdh / dh;
                int qx = (int)floor(fx), qy = (int)floor(fy);
                if (qx < 0 || qy < 0 || qx >= sw || qy >= sh) continue;
                if (fx < sx || fy < sy || fx >= sx + sdw || fy >= sy + sdh) continue;
                nb_px_over(cv->px + ((size_t)py * cv->w + px) * 4,
                           spx + ((size_t)qy * sw + qx) * 4, ga);
            }
    }
    if (owned) free(spx);
    return JS_UNDEFINED;
}
/* getImageData returns REAL canvas pixels (was zeros). Out-of-canvas
 * region reads transparent black; zero-size throws per spec. */
