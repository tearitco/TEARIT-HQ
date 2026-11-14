/* nb_worker_collections.c - NodeList/HTMLCollection helpers (mechanical extract, single-TU include) */
static JSValue nb_list_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t idx = 0;
    if (argc > 0) JS_ToInt32(ctx, &idx, argv[0]);
    if (idx < 0) return JS_NULL;
    JSValue v = JS_GetPropertyUint32(ctx, this_val, (uint32_t)idx);
    if (JS_IsUndefined(v)) { JS_FreeValue(ctx, v); return JS_NULL; }
    return v;
}
static void nb_list_proto(JSContext *ctx, JSValue arr, const char *class_name) {
    JSValue cls = get_global_attr(ctx, class_name);
    JSValue proto = JS_GetPropertyStr(ctx, cls, "prototype");
    if (JS_IsObject(proto)) JS_SetPrototype(ctx, arr, proto);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, cls);
}
