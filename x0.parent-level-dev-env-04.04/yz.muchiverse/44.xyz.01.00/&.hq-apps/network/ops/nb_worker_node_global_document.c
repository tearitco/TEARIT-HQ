/* nb_worker_node_global_document.c - Node global-document helper (mechanical extract, single-TU include) */
static JSValue nb_global_document(JSContext *ctx) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, g, "document");
    JS_FreeValue(ctx, g);
    return doc; /* caller frees */
}
