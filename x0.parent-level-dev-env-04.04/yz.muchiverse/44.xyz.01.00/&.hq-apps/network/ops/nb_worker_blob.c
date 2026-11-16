/* nb_worker_blob.c - Blob implementation (mechanical extract, single-TU include) */
/* Blob: real bytes (hidden `__nb_bytes` string) + size/type/slice.
 * slice and size are attached as own properties of every instance rather
 * than on Blob.prototype: registering on the prototype of a
 * JS_NewCFunction2 constructor did not take in this QuickJS build (measured
 * live 2026-10-21: typeof new Blob(['x']).slice was undefined, while the
 * identical add_class pattern works for FormData). Per-instance attachment
 * is equivalent for an immutable value object and cannot silently miss. */
static JSValue nb_blob_bytes(JSContext *ctx, JSValueConst this_val) {
    JSValue b = JS_GetPropertyStr(ctx, this_val, "__nb_bytes");
    if (JS_IsUndefined(b)) { JS_FreeValue(ctx, b); b = JS_NewString(ctx, ""); }
    return b;
}
/* Resolve `val` into a real Promise via the global Promise, taking ownership
 * of val. Blob.text()/arrayBuffer() are promise-returning in the spec, and a
 * page that awaits a plain value would silently see undefined. Falls back to
 * handing back the raw value if Promise is somehow absent. */
static JSValue nb_promise_resolve(JSContext *ctx, JSValue val) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue P = JS_GetPropertyStr(ctx, g, "Promise");
    JS_FreeValue(ctx, g);
    if (!JS_IsFunction(ctx, P)) { JS_FreeValue(ctx, P); return val; }
    JSAtom a = JS_NewAtom(ctx, "resolve");
    JSValue r = JS_Invoke(ctx, P, a, 1, (JSValueConst *)&val);
    JS_FreeAtom(ctx, a);
    JS_FreeValue(ctx, P);
    JS_FreeValue(ctx, val);
    return r;
}
/* Blob.text() / Blob.arrayBuffer() (2026-11-02): the worker models a blob's
 * bytes as the string in __nb_bytes (see nb_blob_from_parts), so text() hands
 * that string over and arrayBuffer() is its UTF-8 bytes - not an empty or
 * fabricated buffer, which is what a page reading bytes would mis-handle.
 * Installed PER INSTANCE, like slice: the constructor returns a plain object,
 * so nothing on Blob.prototype is reachable from an instance. */
static JSValue nb_blob_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_promise_resolve(ctx, nb_blob_bytes(ctx, this_val));
}
static JSValue nb_blob_arraybuffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue b = nb_blob_bytes(ctx, this_val);
    size_t n = 0;
    const char *s = JS_ToCStringLen(ctx, &n, b);
    JSValue ab = JS_NewArrayBufferCopy(ctx, (const uint8_t *)(s ? s : ""), s ? n : 0);
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, b);
    return nb_promise_resolve(ctx, ab);
}
static JSValue nb_blob_slice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue b = nb_blob_bytes(ctx, this_val);
    size_t n = 0; const char *s = JS_ToCStringLen(ctx, &n, b);
    int32_t a = 0, z = (int32_t)n;
    if (argc > 0) JS_ToInt32(ctx, &a, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &z, argv[1]);
    if (a < 0) a += (int32_t)n;
    if (z < 0) z += (int32_t)n;
    if (a < 0) a = 0; if (z > (int32_t)n) z = (int32_t)n; if (z < a) z = a;
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "__nb_bytes", JS_NewStringLen(ctx, s + a, (size_t)(z - a)));
    JS_SetPropertyStr(ctx, out, "size", JS_NewInt64(ctx, (int64_t)(z - a)));
    JS_SetPropertyStr(ctx, out, "slice", JS_NewCFunction(ctx, nb_blob_slice, "slice", 2));
    /* A slice is itself a Blob, so it needs the same reader set. */
    JS_SetPropertyStr(ctx, out, "text", JS_NewCFunction(ctx, nb_blob_text, "text", 0));
    JS_SetPropertyStr(ctx, out, "arrayBuffer", JS_NewCFunction(ctx, nb_blob_arraybuffer, "arrayBuffer", 0));
    if (argc > 1) {
        JSValue t = JS_GetPropertyStr(ctx, this_val, "type");
        if (!JS_IsUndefined(t)) JS_SetPropertyStr(ctx, out, "type", t);
        else JS_FreeValue(ctx, t);
    }
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, b);
    return out;
}
static JSValue nb_blob_from_parts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue out = JS_NewObject(ctx);
    JSValue parts_out = JS_NewArray(ctx);
    uint32_t o = 0;
    if (argc > 0) {
        JSValue parts = JS_GetPropertyStr(ctx, argv[0], "length");
        uint32_t n = 0; JS_ToUint32(ctx, &n, parts); JS_FreeValue(ctx, parts);
        for (uint32_t i = 0; i < n; i++) {
            JSValue el = JS_GetPropertyUint32(ctx, argv[0], i);
            JSValue es = JS_ToString(ctx, el);
            JS_SetPropertyUint32(ctx, parts_out, o++, es);
            JS_FreeValue(ctx, el);
        }
    }
    /* join in the engine: no public JS_ConcatString in this QuickJS build */
    JSAtom join_atom = JS_NewAtom(ctx, "join");
    JSValue sep = JS_NewString(ctx, "");
    JSValue joined = JS_Invoke(ctx, parts_out, join_atom, 1, &sep);
    JS_FreeValue(ctx, sep);
    JS_FreeAtom(ctx, join_atom);
    JS_FreeValue(ctx, parts_out);
    JS_SetPropertyStr(ctx, out, "__nb_bytes", joined);
    /* size as a plain data property; slice attached per-instance */
    {
        size_t n = 0;
        const char *cs = JS_ToCStringLen(ctx, &n, joined);
        if (cs) JS_FreeCString(ctx, cs);
        JS_SetPropertyStr(ctx, out, "size", JS_NewInt64(ctx, (int64_t)n));
    }
    JS_SetPropertyStr(ctx, out, "slice", JS_NewCFunction(ctx, nb_blob_slice, "slice", 2));
    JS_SetPropertyStr(ctx, out, "text", JS_NewCFunction(ctx, nb_blob_text, "text", 0));
    JS_SetPropertyStr(ctx, out, "arrayBuffer", JS_NewCFunction(ctx, nb_blob_arraybuffer, "arrayBuffer", 0));
    if (argc > 1) {
        JSValue t = JS_GetPropertyStr(ctx, argv[1], "type");
        if (!JS_IsUndefined(t)) JS_SetPropertyStr(ctx, out, "type", t);
        else JS_FreeValue(ctx, t);
    }
    return out;
}
