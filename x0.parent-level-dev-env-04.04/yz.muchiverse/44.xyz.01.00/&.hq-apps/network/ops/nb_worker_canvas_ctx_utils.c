/* nb_worker_canvas_ctx_utils.c - canvas context utility helpers (mechanical extract, single-TU include) */
static double nb_ctx_num(JSContext *ctx, JSValueConst o, const char *k, double def) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    double d = def;
    if (JS_IsNumber(v)) JS_ToFloat64(ctx, &d, v);
    JS_FreeValue(ctx, v);
    return d;
}
static void nb_ctx_fill_rgb(JSContext *ctx, JSValueConst o, unsigned char out[4]) {
    JSValue v = JS_GetPropertyStr(ctx, o, "fillStyle");
    const char *s = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    nb_color_parse(s, out);
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
}
/* multiply current transform by [a b c d e f] (canvas semantics) */
static void nb_ctx_xf_mul(NbCanvas *cv, double a, double b, double c, double d, double e, double f) {    double m0 = cv->m[0], m1 = cv->m[1], m2 = cv->m[2], m3 = cv->m[3], m4 = cv->m[4], m5 = cv->m[5];
    cv->m[0] = m0 * a + m2 * b;
    cv->m[1] = m1 * a + m3 * b;
    cv->m[2] = m0 * c + m2 * d;
    cv->m[3] = m1 * c + m3 * d;
    cv->m[4] = m0 * e + m2 * f + m4;
    cv->m[5] = m1 * e + m3 * f + m5;
}
static double nb_c2d_argd(JSContext *ctx, JSValueConst *argv, int argc, int i, double def) {
    double d = def;
    if (i < argc && JS_IsNumber(argv[i])) JS_ToFloat64(ctx, &d, argv[i]);
    return d;
}
