/* nb_worker_image_ctor.c - Image constructor helper (mechanical extract, single-TU include) */
static __attribute__((used)) JSValue nb_image_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, global, "document");
    JSValue ce = JS_GetPropertyStr(ctx, doc, "createElement");
    JSValue arg = JS_NewString(ctx, "img");
    JSValue el = JS_Call(ctx, ce, doc, 1, &arg);
    JS_FreeValue(ctx, arg); JS_FreeValue(ctx, ce); JS_FreeValue(ctx, doc); JS_FreeValue(ctx, global);
    return el;
}
