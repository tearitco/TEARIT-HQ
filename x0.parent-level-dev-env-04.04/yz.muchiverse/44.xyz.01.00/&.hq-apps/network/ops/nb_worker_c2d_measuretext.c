/* nb_worker_c2d_measuretext.c - canvas 2D measureText implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_measureText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue m = JS_NewObject(ctx);
    nb_font_load();
    double px = nb_font_size_px(ctx, this_val);
    char font_family[64];
    nb_font_family_from_ctx(ctx, this_val, font_family, sizeof(font_family));
    double family_scale = nb_font_family_scale(font_family);
    double scale = g_font_upm ? px / g_font_upm : 0;
    scale *= family_scale;
    double w = 0;
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) {
            for (const char *p = s; *p; p++) {
                unsigned char c = (unsigned char)*p;
                if (c < 32 || c > 126) continue;
                w += nb_font_advance(nb_font_gid(c)) * scale;
            }
            JS_FreeCString(ctx, s);
        }
    }
    JS_SetPropertyStr(ctx, m, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, m, "actualBoundingBoxAscent", JS_NewFloat64(ctx, g_font_asc * scale));
    JS_SetPropertyStr(ctx, m, "actualBoundingBoxDescent", JS_NewFloat64(ctx, -g_font_desc * scale));
    return m;
}
/* fillText through the canvas transform is out of scope for slice 1
 * (glyphs are axis-aligned); the translation part of the transform is
 * honored so translated text lands correctly. */
