/* nb_worker_c2d_stroketext.c - canvas 2D strokeText implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_strokeText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    if (argc < 1 || !JS_IsString(argv[0])) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) return JS_UNDEFINED;
    nb_font_load();
    double x = nb_c2d_argd(ctx, argv, argc, 1, 0), y = nb_c2d_argd(ctx, argv, argc, 2, 0);
    double px = nb_font_size_px(ctx, this_val);
    char font_family[64];
    nb_font_family_from_ctx(ctx, this_val, font_family, sizeof(font_family));
    double family_scale = nb_font_family_scale(font_family);
    double scale = g_font_upm ? px / g_font_upm : 0;
    scale *= family_scale;
    char align[32], base[32];
    nb_ctx_str(ctx, this_val, "textAlign", align, sizeof(align), "start");
    nb_ctx_str(ctx, this_val, "textBaseline", base, sizeof(base), "alphabetic");
    double total = 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c > 126 || !g_font_data) continue;
        total += nb_font_advance(nb_font_gid(c)) * scale;
    }
    double pen = x + cv->m[4];
    if (!strcmp(align, "center")) pen -= total / 2;
    else if (!strcmp(align, "right") || !strcmp(align, "end")) pen -= total;
    double asc = g_font_data ? g_font_asc * scale : px;
    double desc = g_font_data ? -g_font_desc * scale : 0;
    double baseline = y + cv->m[5];
    if (!strcmp(base, "top") || !strcmp(base, "hanging")) baseline += asc;
    else if (!strcmp(base, "middle")) baseline += asc / 2;
    else if (!strcmp(base, "bottom") || !strcmp(base, "ideographic")) baseline -= desc;
    double lw = nb_ctx_num(ctx, this_val, "lineWidth", 1);
    int rad = (int)(lw / 2 + 0.5);
    if (rad < 1) rad = 1;
    if (rad > 8) rad = 8;
    size_t npix = (size_t)cv->w * cv->h;
    unsigned char *mask = calloc(npix ? npix : 1, 1);
    if (!mask) { JS_FreeCString(ctx, s); return JS_UNDEFINED; }
    double p2 = pen;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c > 126 || !g_font_data) { p2 += 4 * scale; continue; }
        unsigned gid = nb_font_gid(c);
        if (gid) {
            static double xs[512], ys[512];
            static int ends[32];
            int ncont = 0;
            int npts = nb_font_points(gid, xs, ys, ends, &ncont);
            if (npts > 0) {
                double gx0 = 1e18, gx1 = -1e18, gy0 = 1e18, gy1 = -1e18;
                for (int i = 0; i < npts; i++) {
                    double qx = p2 + xs[i] * scale, qy = baseline - ys[i] * scale;
                    if (qx < gx0) gx0 = qx; if (qx > gx1) gx1 = qx;
                    if (qy < gy0) gy0 = qy; if (qy > gy1) gy1 = qy;
                }
                int ix0 = (int)floor(gx0) - 1, iy0 = (int)floor(gy0) - 1;
                int ix1 = (int)ceil(gx1) + 1, iy1 = (int)ceil(gy1) + 1;
                if (ix0 < 0) ix0 = 0; if (iy0 < 0) iy0 = 0;
                if (ix1 > cv->w) ix1 = cv->w; if (iy1 > cv->h) iy1 = cv->h;
                int sj = 0;
                for (int yy = iy0; yy < iy1; yy++)
                    for (int xx = ix0; xx < ix1; xx++) {
                        double qx = xx + 0.5, qy = yy + 0.5;
                        int inside = 0;
                        int sk = 0;
                        for (int ci = 0; ci < ncont; ci++) {
                            int en = ends[ci];
                            for (int i = sk; i <= en; i++) {
                                int j = (i == en) ? sk : i + 1;
                                double ax = p2 + xs[i] * scale, ay = baseline - ys[i] * scale;
                                double bx = p2 + xs[j] * scale, by = baseline - ys[j] * scale;
                                if ((ay > qy) != (by > qy)) {
                                    double ix = ax + (qy - ay) * (bx - ax) / (by - ay);
                                    if (ix > qx) inside = !inside;
                                }
                            }
                            sk = en + 1;
                        }
                        if (inside) mask[(size_t)yy * cv->w + xx] = 1;
                    }
            }
        }
        p2 += nb_font_advance(gid) * scale;
    }
    unsigned char col[4];
    nb_ctx_fill_rgb(ctx, this_val, col);
    {
        JSValue sv = JS_GetPropertyStr(ctx, this_val, "strokeStyle");
        const char *ss = JS_IsString(sv) ? JS_ToCString(ctx, sv) : NULL;
        if (ss && ss[0]) nb_color_parse(ss, col);
        if (ss) JS_FreeCString(ctx, ss);
        JS_FreeValue(ctx, sv);
    }
    double ga = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    for (int yy = 0; yy < cv->h; yy++)
        for (int xx = 0; xx < cv->w; xx++) {
            if (mask[(size_t)yy * cv->w + xx]) continue;
            int hit = 0;
            for (int oy = -rad; oy <= rad && !hit; oy++)
                for (int ox = -rad; ox <= rad && !hit; ox++) {
                    int qx = xx + ox, qy = yy + oy;
                    if (qx < 0 || qy < 0 || qx >= cv->w || qy >= cv->h) continue;
                    if (mask[(size_t)qy * cv->w + qx]) hit = 1;
                }
            if (hit) nb_px_over(cv->px + ((size_t)yy * cv->w + xx) * 4, col, ga);
        }
    free(mask);
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
