/* nb_worker_c2d_pattern_linedash.c - canvas 2D pattern/line-dash helpers (mechanical extract, single-TU include) */
static JSValue nb_c2d_createPattern(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    /* Pattern holds its source reference; resolved to pixels at fill time
     * so canvas-source patterns track later draws (live pattern). */
    JSValue p = JS_NewObject(ctx);
    if (argc >= 1 && JS_IsObject(argv[0]))
        JS_SetPropertyStr(ctx, p, "src", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, p, "rep",
                      (argc >= 2 && JS_IsString(argv[1])) ? JS_DupValue(ctx, argv[1]) : JS_NewString(ctx, "repeat"));
    return p;
}
static JSValue nb_c2d_lineDash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewArray(ctx);
}
