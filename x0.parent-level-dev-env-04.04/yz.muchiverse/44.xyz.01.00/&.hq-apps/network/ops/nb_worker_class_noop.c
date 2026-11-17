/* nb_worker_class_noop.c - DOM class noop constructor helper (mechanical extract, single-TU include) */
static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* quickjs.c: C constructors receive new_target as this_val, so build the
     * instance off its .prototype (the fresh object already carries f.prototype)
     * — `new Element()` then yields an Element instance and instanceof works.
     * Generic method calls get a non-function receiver and return undefined. */
    if (JS_IsFunction(ctx, this_val)) {
        JSValue proto = JS_GetPropertyStr(ctx, this_val, "prototype");
        JSValue obj = JS_IsObject(proto) ? JS_NewObjectProto(ctx, proto) : JS_NewObject(ctx);
        JS_FreeValue(ctx, proto);
        return obj;
    }
    return JS_UNDEFINED;
}
