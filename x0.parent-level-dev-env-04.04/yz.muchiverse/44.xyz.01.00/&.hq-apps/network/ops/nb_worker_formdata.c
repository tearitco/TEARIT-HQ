/* nb_worker_formdata.c - FormData helpers and methods (mechanical extract, single-TU include) */
/* ---- FormData / Blob / FileReader: real instance state (2026-10-20) ----
 * Stub constructors alone were not enough: `new FormData()` worked but
 * `new FormData().append('a','b')` threw TypeError, and form-posting SPAs
 * hit exactly that. These carry real data, not just a name.
 * FormData entries live in a hidden `__nb_fd` array as [k,v,k,v,...]. */
static JSValue add_class(JSContext *ctx, const char *name, JSValueConst parent);
static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_fd_entries(JSContext *ctx, JSValueConst this_val) {
    JSValue a = JS_GetPropertyStr(ctx, this_val, "__nb_fd");
    if (!JS_IsArray(ctx, a)) {
        JS_FreeValue(ctx, a);
        a = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, this_val, "__nb_fd", JS_DupValue(ctx, a));
    }
    return a;
}
static JSValue nb_fd_append(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_UNDEFINED;
    JSValue a = nb_fd_entries(ctx, this_val);
    uint32_t n = 0;
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, a, n,     JS_DupValue(ctx, argv[0]));
    /* form fields are stringified; a File/Blob value keeps its object */
    JSValue v = argv[1];
    if (JS_IsObject(v) && !JS_IsFunction(ctx, v)) {
        JS_SetPropertyUint32(ctx, a, n + 1, JS_DupValue(ctx, v));
    } else {
        JSValue s = JS_ToString(ctx, v);
        JS_SetPropertyUint32(ctx, a, n + 1, s);
    }
    JS_FreeValue(ctx, a);
    return JS_UNDEFINED;
}
static JSValue nb_fd_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NULL;
    JSValue key = JS_ToString(ctx, argv[0]);
    JSValue a = nb_fd_entries(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    JSValue out = JS_NULL;
    for (uint32_t i = 0; i + 1 < n; i += 2) {
        JSValue k = JS_GetPropertyUint32(ctx, a, i);
        int eq = JS_StrictEq(ctx, k, key);
        JS_FreeValue(ctx, k);
        if (eq) { out = JS_GetPropertyUint32(ctx, a, i + 1); break; }
    }
    JS_FreeValue(ctx, key); JS_FreeValue(ctx, a);
    return out;
}
static JSValue nb_fd_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue v = nb_fd_get(ctx, this_val, argc, argv);
    int r = !JS_IsNull(v);
    JS_FreeValue(ctx, v);
    return JS_NewBool(ctx, r);
}
static JSValue nb_fd_getAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue arr = JS_NewArray(ctx);
    if (argc < 1) return arr;
    JSValue key = JS_ToString(ctx, argv[0]);
    JSValue a = nb_fd_entries(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    uint32_t o = 0;
    for (uint32_t i = 0; i + 1 < n; i += 2) {
        JSValue k = JS_GetPropertyUint32(ctx, a, i);
        int eq = JS_StrictEq(ctx, k, key);
        JS_FreeValue(ctx, k);
        if (eq) {
            JSValue v = JS_GetPropertyUint32(ctx, a, i + 1);
            JS_SetPropertyUint32(ctx, arr, o++, v);
        }
    }
    JS_FreeValue(ctx, key); JS_FreeValue(ctx, a);
    return arr;
}
static JSValue nb_fd_delete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    JSValue key = JS_ToString(ctx, argv[0]);
    JSValue a = nb_fd_entries(ctx, this_val);
    JSValue out = JS_NewArray(ctx);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    uint32_t o = 0;
    for (uint32_t i = 0; i + 1 < n; i += 2) {
        JSValue k = JS_GetPropertyUint32(ctx, a, i);
        JSValue v = JS_GetPropertyUint32(ctx, a, i + 1);
        int eq = JS_StrictEq(ctx, k, key);
        if (eq) { JS_FreeValue(ctx, k); JS_FreeValue(ctx, v); }
        else {
            JS_SetPropertyUint32(ctx, out, o++, k);
            JS_SetPropertyUint32(ctx, out, o++, v);
        }
    }
    JS_SetPropertyStr(ctx, this_val, "__nb_fd", out);
    JS_FreeValue(ctx, key); JS_FreeValue(ctx, a);
    return JS_UNDEFINED;
}
