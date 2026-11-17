/* nb_worker_js_node_binding.c - JS/C node binding helpers (mechanical extract, single-TU include) */
static NbNode *get_node(JSContext *ctx, JSValueConst v) {
    if (!JS_IsObject(v)) return NULL;
    JSValue k = JS_GetPropertyStr(ctx, v, NODEKEY);
    int i = -1;
    if (JS_IsNumber(k)) {
        int32_t iv;
        if (JS_ToInt32(ctx, &iv, k) == 0) i = (int)iv;
    }
    JS_FreeValue(ctx, k);
    if (i < 0 || i >= g_nodecount) return NULL;
    return g_nodeindex[i];
}
/* Node from the `this` binding of an element native (QuickJS passes the
 * this-binding in the native's JSValueConst this_val argument). */
static NbNode *get_this(JSContext *ctx, JSValueConst this_val) {
    return get_node(ctx, this_val);
}
/* String arg i as own C string (only for string-typed args — the call sites
 * previously used duk_get_string + a "" fallback), or NULL when absent. */
static const char *js_arg_str(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc || !JS_IsString(argv[i])) return NULL;
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned;
}
/* String arg i coercing any value via toString (duk_safe_to_string), "" when
 * absent. */
static const char *js_arg_str_any(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc) return "";
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned ? *owned : "";
}
/* Numeric arg i (numbers only), or -1 when absent/non-number. */
static int js_arg_index(JSContext *ctx, JSValueConst *argv, int argc, int i) {
    if (i >= argc || !JS_IsNumber(argv[i])) return -1;
    int32_t iv;
    if (JS_ToInt32(ctx, &iv, argv[i]) != 0) return -1;
    return (int)iv;
}
