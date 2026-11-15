/* nb_worker_promise_empty.c - promise-resolved and empty-array helpers (mechanical extract, single-TU include) */
/* whenDefined must return a thenable, so hand back a resolved Promise. */
static JSValue nb_promise_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue funcs[2];
    JSValue p = JS_NewPromiseCapability(ctx, funcs);
    if (JS_IsException(p)) return p;
    JS_Call(ctx, funcs[0], JS_UNDEFINED, 1, argv ? argv : &this_val);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    return p;
}
static JSValue nb_empty_array(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewArray(ctx);
}
