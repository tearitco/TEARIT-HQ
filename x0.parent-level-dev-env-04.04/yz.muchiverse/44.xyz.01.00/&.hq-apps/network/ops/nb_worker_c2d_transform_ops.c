/* nb_worker_c2d_transform_ops.c - canvas 2D transform operations (mechanical extract, single-TU include) */
static JSValue nb_c2d_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (cv) nb_ctx_xf_mul(cv, 1, 0, 0, 1, nb_c2d_argd(ctx, argv, argc, 0, 0), nb_c2d_argd(ctx, argv, argc, 1, 0));
    return JS_UNDEFINED;
}
static JSValue nb_c2d_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (cv) nb_ctx_xf_mul(cv, nb_c2d_argd(ctx, argv, argc, 0, 1), 0, 0, nb_c2d_argd(ctx, argv, argc, 1, 1), 0, 0);
    return JS_UNDEFINED;
}
static JSValue nb_c2d_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (cv) {
        double a = nb_c2d_argd(ctx, argv, argc, 0, 0);
        nb_ctx_xf_mul(cv, cos(a), sin(a), -sin(a), cos(a), 0, 0);
    }
    return JS_UNDEFINED;
}
static JSValue nb_c2d_setTransform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv) return JS_UNDEFINED;
    if (argc == 0) { memset(cv->m, 0, sizeof(cv->m)); cv->m[0] = cv->m[3] = 1.0; return JS_UNDEFINED; }
    cv->m[0] = nb_c2d_argd(ctx, argv, argc, 0, 1); cv->m[1] = nb_c2d_argd(ctx, argv, argc, 1, 0);
    cv->m[2] = nb_c2d_argd(ctx, argv, argc, 2, 0); cv->m[3] = nb_c2d_argd(ctx, argv, argc, 3, 1);
    cv->m[4] = nb_c2d_argd(ctx, argv, argc, 4, 0); cv->m[5] = nb_c2d_argd(ctx, argv, argc, 5, 0);
    return JS_UNDEFINED;
}
static JSValue nb_c2d_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (cv) nb_ctx_xf_mul(cv, nb_c2d_argd(ctx, argv, argc, 0, 1), nb_c2d_argd(ctx, argv, argc, 1, 0),
                          nb_c2d_argd(ctx, argv, argc, 2, 0), nb_c2d_argd(ctx, argv, argc, 3, 1),
                          nb_c2d_argd(ctx, argv, argc, 4, 0), nb_c2d_argd(ctx, argv, argc, 5, 0));
    return JS_UNDEFINED;
}
static JSValue nb_c2d_resetTransform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (cv) { memset(cv->m, 0, sizeof(cv->m)); cv->m[0] = cv->m[3] = 1.0; }
    return JS_UNDEFINED;
}
