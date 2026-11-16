/* nb_worker_c2d_filltext.c - canvas 2D fillText implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_fillText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv || argc < 1) return JS_UNDEFINED;
    nb_font_load();
    if (!g_font_data) return JS_UNDEFINED;
    const char *s = JS_IsString(argv[0]) ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!s) return JS_UNDEFINED;
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
        if (c < 32 || c > 126) continue;
        total += nb_font_advance(nb_font_gid(c)) * scale;
    }
    double pen = x + cv->m[4];
    if (!strcmp(align, "center")) pen -= total / 2;
    else if (!strcmp(align, "right") || !strcmp(align, "end")) pen -= total;
    double asc = g_font_asc * scale, desc = -g_font_desc * scale;
    double baseline = y + cv->m[5];
    if (!strcmp(base, "top") || !strcmp(base, "hanging")) baseline += asc;
    else if (!strcmp(base, "middle")) baseline += asc / 2;
    else if (!strcmp(base, "bottom") || !strcmp(base, "ideographic")) baseline -= desc;
    unsigned char col[4];
    nb_ctx_fill_rgb(ctx, this_val, col);
    double ga = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c > 126) { pen += 4 * scale; continue; }
        unsigned gid = nb_font_gid(c);
        if (gid) nb_font_blit(cv, gid, pen, baseline, scale, col, ga);
        pen += nb_font_advance(gid) * scale;
    }
    JS_FreeCString(ctx, s);
    if (cv) cv->dirty = 1;
    return JS_UNDEFINED;
}
/* strokeText: rasterize the run to a mask, then paint every unset pixel
 * within lineWidth/2 of a set pixel (true outline, not faux-bold).
 * Translation part of the transform honored via pen/baseline shift. */
