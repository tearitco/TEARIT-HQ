/* nb_worker_filereader_install.c - FileReader read and FormData/Blob/FileReader install (mechanical extract, single-TU include) */
/* FileReader: synchronous readAs* — the async model is not modelled yet,
 * but `result` and onload fire so feature-detect paths keep working. */
static JSValue nb_fr_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue b = (argc > 0) ? JS_GetPropertyStr(ctx, argv[0], "__nb_bytes") : JS_UNDEFINED;
    if (JS_IsUndefined(b)) { JS_FreeValue(ctx, b); b = JS_NewString(ctx, ""); }
    JS_SetPropertyStr(ctx, this_val, "result", JS_DupValue(ctx, b));
    JS_SetPropertyStr(ctx, this_val, "readyState", JS_NewInt32(ctx, 2));
    JSValue cb = JS_GetPropertyStr(ctx, this_val, "onload");
    if (JS_IsFunction(ctx, cb)) {
        JSValue ev = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ev, "target", JS_DupValue(ctx, this_val));
        JSValue r = JS_Call(ctx, cb, this_val, 1, &ev);
        JS_FreeValue(ctx, r); JS_FreeValue(ctx, ev);
    }
    JS_FreeValue(ctx, cb);
    JS_FreeValue(ctx, b);
    return JS_UNDEFINED;
}
static void nb_install_formdata_blob(JSContext *ctx, JSValueConst g) {
    JSValue f = add_class(ctx, "FormData", JS_UNDEFINED);
    JSValue p = JS_GetPropertyStr(ctx, f, "prototype");
    JS_SetPropertyStr(ctx, p, "append",  JS_NewCFunction(ctx, nb_fd_append,  "append", 2));
    JS_SetPropertyStr(ctx, p, "set",     JS_NewCFunction(ctx, nb_fd_append,  "set", 2));
    JS_SetPropertyStr(ctx, p, "get",     JS_NewCFunction(ctx, nb_fd_get,     "get", 1));
    JS_SetPropertyStr(ctx, p, "getAll",  JS_NewCFunction(ctx, nb_fd_getAll,  "getAll", 1));
    JS_SetPropertyStr(ctx, p, "has",     JS_NewCFunction(ctx, nb_fd_has,     "has", 1));
    JS_SetPropertyStr(ctx, p, "delete",  JS_NewCFunction(ctx, nb_fd_delete,  "delete", 1));
    JS_FreeValue(ctx, p);
    JS_SetPropertyStr(ctx, g, "FormData", f);

    /* Blob needs a custom constructor, so build its prototype explicitly and
     * attach it — mirroring add_class. Reading the auto-created .prototype off
     * a JS_CFUNC_constructor right after creation did not stick: slice ended
     * up undefined on every instance (measured live, 2026-10-21). */
    f = JS_NewCFunction2(ctx, nb_blob_from_parts, "Blob", 2, JS_CFUNC_constructor, 0);
    p = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, p, "slice", JS_NewCFunction(ctx, nb_blob_slice, "slice", 2));
    JS_SetPropertyStr(ctx, f, "prototype", p);
    JS_FreeValue(ctx, p);
    JS_SetPropertyStr(ctx, g, "Blob", f);

    f = add_class(ctx, "FileReader", JS_UNDEFINED);
    p = JS_GetPropertyStr(ctx, f, "prototype");
    JS_SetPropertyStr(ctx, p, "readAsText",    JS_NewCFunction(ctx, nb_fr_read, "readAsText", 1));
    JS_SetPropertyStr(ctx, p, "readAsDataURL", JS_NewCFunction(ctx, nb_fr_read, "readAsDataURL", 1));
    JS_SetPropertyStr(ctx, p, "readAsArrayBuffer", JS_NewCFunction(ctx, nb_fr_read, "readAsArrayBuffer", 1));
    JS_SetPropertyStr(ctx, p, "abort",         JS_NewCFunction(ctx, class_noop, "abort", 0));
    JS_FreeValue(ctx, p);
    JS_SetPropertyStr(ctx, g, "FileReader", f);
}
