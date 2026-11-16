/* nb_worker_c2d_fillrect.c - canvas 2D fillRect implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_fillRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    double x = nb_c2d_argd(ctx, argv, argc, 0, 0), y = nb_c2d_argd(ctx, argv, argc, 1, 0);
    double w = nb_c2d_argd(ctx, argv, argc, 2, 0), h = nb_c2d_argd(ctx, argv, argc, 3, 0);
    if (w < 0) { x += w; w = -w; } if (h < 0) { y += h; h = -h; }
    if (w <= 0 || h <= 0) return JS_UNDEFINED;
    double ga = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    /* gradient fillStyle: sample stops per pixel through the transform */
    JSValue fs = JS_GetPropertyStr(ctx, this_val, "fillStyle");
    if (JS_IsObject(fs)) {
        JSValue srcv = JS_GetPropertyStr(ctx, fs, "src");
        if (JS_IsObject(srcv)) {
            /* pattern fillStyle: tile source pixels across the rect.
             * repetition modes other than repeat/no-repeat collapse to
             * repeat (noted); transform applies to the rect, pattern
             * phase stays axis-aligned (noted). */
            char rep[16] = "repeat";
            JSValue repv = JS_GetPropertyStr(ctx, fs, "rep");
            const char *rs = JS_IsString(repv) ? JS_ToCString(ctx, repv) : NULL;
            if (rs) { snprintf(rep, sizeof(rep), "%s", rs); JS_FreeCString(ctx, rs); }
            JS_FreeValue(ctx, repv);
            int sw = 0, sh = 0, owned = 0;
            unsigned char *spx = NULL;
            if (nb_c2d_source(ctx, srcv, &sw, &sh, &spx, &owned) && sw > 0 && sh > 0) {
                double a = cv->m[0], b = cv->m[1], cc = cv->m[2], d = cv->m[3], e = cv->m[4], f = cv->m[5];
                double det = a * d - b * cc;
                if (det != 0.0) {
                    double xs[4] = {x, x + w, x, x + w}, ys[4] = {y, y, y + h, y + h};
                    double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
                    for (int i = 0; i < 4; i++) {
                        double px = a * xs[i] + cc * ys[i] + e, py = b * xs[i] + d * ys[i] + f;
                        if (px < x0) x0 = px; if (px > x1) x1 = px;
                        if (py < y0) y0 = py; if (py > y1) y1 = py;
                    }
                    int ix0 = (int)floor(x0), iy0 = (int)floor(y0);
                    int ix1 = (int)ceil(x1), iy1 = (int)ceil(y1);
                    if (ix0 < 0) ix0 = 0; if (iy0 < 0) iy0 = 0;
                    if (ix1 > cv->w) ix1 = cv->w; if (iy1 > cv->h) iy1 = cv->h;
                    int norep = !strcmp(rep, "no-repeat");
                    for (int py = iy0; py < iy1; py++)
                        for (int px = ix0; px < ix1; px++) {
                            double u = ((px + 0.5 - e) * d - (py + 0.5 - f) * cc) / det;
                            double v = (a * (py + 0.5 - f) - b * (px + 0.5 - e)) / det;
                            if (u < x || u >= x + w || v < y || v >= y + h) continue;
                            int qx = (int)floor(u - x), qy = (int)floor(v - y);
                            if (norep) {
                                if (qx < 0 || qy < 0 || qx >= sw || qy >= sh) continue;
                            } else {
                                qx %= sw; qy %= sh;
                                if (qx < 0) qx += sw; if (qy < 0) qy += sh;
                            }
                            nb_px_over(cv->px + ((size_t)py * cv->w + px) * 4,
                                       spx + ((size_t)qy * sw + qx) * 4, ga);
                        }
                    JS_FreeValue(ctx, srcv);
                    if (owned) free(spx);
                    JS_FreeValue(ctx, fs);
                    return JS_UNDEFINED;
                }
            }
            if (owned) free(spx);
            JS_FreeValue(ctx, srcv);
        } else {
            JS_FreeValue(ctx, srcv);
        }
        JSValue glv = JS_GetPropertyStr(ctx, fs, "glen");
        int32_t glen = 0;
        if (JS_IsNumber(glv)) JS_ToInt32(ctx, &glen, glv);
        JS_FreeValue(ctx, glv);
        if (glen == 4 || glen == 6) {
            double gp[6] = {0};
            for (int i = 0; i < glen; i++) {
                char k[8];
                snprintf(k, sizeof(k), "g%d", i);
                JSValue gv = JS_GetPropertyStr(ctx, fs, k);
                if (JS_IsNumber(gv)) JS_ToFloat64(ctx, &gp[i], gv);
                JS_FreeValue(ctx, gv);
            }
            double a = cv->m[0], b = cv->m[1], cc = cv->m[2], d = cv->m[3], e = cv->m[4], f = cv->m[5];
            double det = a * d - b * cc;
            if (det != 0.0) {
                double xs[4] = {x, x + w, x, x + w}, ys[4] = {y, y, y + h, y + h};
                double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
                for (int i = 0; i < 4; i++) {
                    double px = a * xs[i] + cc * ys[i] + e, py = b * xs[i] + d * ys[i] + f;
                    if (px < x0) x0 = px; if (px > x1) x1 = px;
                    if (py < y0) y0 = py; if (py > y1) y1 = py;
                }
                int ix0 = (int)floor(x0), iy0 = (int)floor(y0);
                int ix1 = (int)ceil(x1), iy1 = (int)ceil(y1);
                if (ix0 < 0) ix0 = 0; if (iy0 < 0) iy0 = 0;
                if (ix1 > cv->w) ix1 = cv->w; if (iy1 > cv->h) iy1 = cv->h;
                double dx = gp[2] - gp[0], dy = gp[3] - gp[1];
                double denom = dx * dx + dy * dy;
                for (int py = iy0; py < iy1; py++)
                    for (int px = ix0; px < ix1; px++) {
                        double u = ((px + 0.5 - e) * d - (py + 0.5 - f) * cc) / det;
                        double v = (a * (py + 0.5 - f) - b * (px + 0.5 - e)) / det;
                        if (u < x || u >= x + w || v < y || v >= y + h) continue;
                        double t;
                        if (glen == 4) {
                            t = denom > 0 ? ((u - gp[0]) * dx + (v - gp[1]) * dy) / denom : 0;
                        } else {
                            double d0 = sqrt((u - gp[0]) * (u - gp[0]) + (v - gp[1]) * (v - gp[1])) - gp[2];
                            double d1 = sqrt((u - gp[3]) * (u - gp[3]) + (v - gp[4]) * (v - gp[4])) - gp[5];
                            double dn = d1 - d0;
                            t = dn != 0 ? d0 / dn : (d0 >= 0 ? 1 : 0);
                        }
                        if (t < 0) t = 0; if (t > 1) t = 1;
                        unsigned char col[4];
                        nb_grad_sample(ctx, fs, t, col);
                        nb_px_over(cv->px + ((size_t)py * cv->w + px) * 4, col, ga);
                    }
                JS_FreeValue(ctx, fs);
                return JS_UNDEFINED;
            }
        }
    }
    JS_FreeValue(ctx, fs);
    unsigned char s[4];
    nb_ctx_fill_rgb(ctx, this_val, s);
    nb_px_fill_rect_xf(cv, x, y, w, h, s, nb_ctx_num(ctx, this_val, "globalAlpha", 1));
    return JS_UNDEFINED;
}
