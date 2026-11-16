/* nb_worker_c2d_gradient.c - canvas 2D gradient helpers (mechanical extract, single-TU include) */
/* Gradients hold their stops on the JS object; fillRect samples them.
 * addColorStop with offset outside [0,1] throws per spec (IndexSizeError). */
static JSValue nb_c2d_addColorStop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double off = 0;
    if (argc >= 1 && JS_IsNumber(argv[0])) JS_ToFloat64(ctx, &off, argv[0]);
    if (off < 0 || off > 1 || (argc >= 1 && !JS_IsNumber(argv[0]))) {
        JSValue ex = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, ex, "name", JS_NewString(ctx, "IndexSizeError"));
        JS_Throw(ctx, ex);
        return JS_EXCEPTION;
    }
    JSValue stops = JS_GetPropertyStr(ctx, this_val, "stops");
    if (!JS_IsObject(stops)) { JS_FreeValue(ctx, stops); stops = JS_NewArray(ctx); }
    JSValue lv = JS_GetPropertyStr(ctx, stops, "length");
    int32_t len = 0;
    if (JS_IsNumber(lv)) JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    JSValue e = JS_NewArray(ctx);
    JSValue eo = JS_NewFloat64(ctx, off);
    JS_SetPropertyUint32(ctx, e, 0, eo);
    JSValue ec = (argc >= 2 && JS_IsString(argv[1])) ? JS_DupValue(ctx, argv[1]) : JS_NewString(ctx, "#000000");
    JS_SetPropertyUint32(ctx, e, 1, ec);
    JS_SetPropertyUint32(ctx, stops, (uint32_t)len, e);
    JS_SetPropertyStr(ctx, this_val, "stops", JS_DupValue(ctx, stops));
    JS_FreeValue(ctx, stops);
    return JS_UNDEFINED;
}
static JSValue nb_c2d_gradient(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    JSValue g = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, g, "addColorStop", JS_NewCFunction(ctx, nb_c2d_addColorStop, "addColorStop", 2));
    JS_SetPropertyStr(ctx, g, "stops", JS_NewArray(ctx));
    for (int i = 0; i < argc && i < 6; i++) {
        JSValue v = JS_NewFloat64(ctx, nb_c2d_argd(ctx, argv, argc, i, 0));
        char k[8];
        snprintf(k, sizeof(k), "g%d", i);
        JS_SetPropertyStr(ctx, g, k, v);
    }
    JS_SetPropertyStr(ctx, g, "glen", JS_NewInt32(ctx, argc > 6 ? 6 : argc));
    return g;
}
/* sample gradient stops at t in [0,1] */
static void nb_grad_sample(JSContext *ctx, JSValue grad, double t, unsigned char out[4]) {
    out[0] = out[1] = out[2] = 0; out[3] = 255;
    JSValue stops = JS_GetPropertyStr(ctx, grad, "stops");
    JSValue lv = JS_IsObject(stops) ? JS_GetPropertyStr(ctx, stops, "length") : JS_UNDEFINED;
    int32_t n = 0;
    if (JS_IsNumber(lv)) JS_ToInt32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    if (n <= 0) { JS_FreeValue(ctx, stops); return; }
    double bo = 0, to = 1;
    unsigned char bc[4] = {0, 0, 0, 255}, tc[4] = {0, 0, 0, 255};
    int have_b = 0, have_t = 0;
    for (int32_t i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, stops, (uint32_t)i);
        JSValue eo = JS_IsObject(e) ? JS_GetPropertyUint32(ctx, e, 0) : JS_UNDEFINED;
        JSValue ec = JS_IsObject(e) ? JS_GetPropertyUint32(ctx, e, 1) : JS_UNDEFINED;
        double o = 0;
        if (JS_IsNumber(eo)) JS_ToFloat64(ctx, &o, eo);
        unsigned char cc[4] = {0, 0, 0, 255};
        if (JS_IsString(ec)) {
            const char *s = JS_ToCString(ctx, ec);
            nb_color_parse(s, cc);
            if (s) JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, eo); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, e);
        /* nearest stop at/below t and at/above t across all stops */
        if (o <= t && (!have_b || o >= bo)) { bo = o; memcpy(bc, cc, 4); have_b = 1; }
        if (o >= t && (!have_t || o <= to)) { to = o; memcpy(tc, cc, 4); have_t = 1; }
    }
    if (!have_b && have_t) { bo = to; memcpy(bc, tc, 4); }
    if (!have_t && have_b) { to = bo; memcpy(tc, bc, 4); }
    JS_FreeValue(ctx, stops);
    double f = (to > bo) ? (t - bo) / (to - bo) : 0;
    if (f < 0) f = 0; if (f > 1) f = 1;
    for (int i = 0; i < 4; i++)
        out[i] = (unsigned char)(bc[i] + (tc[i] - bc[i]) * f + 0.5);
}
