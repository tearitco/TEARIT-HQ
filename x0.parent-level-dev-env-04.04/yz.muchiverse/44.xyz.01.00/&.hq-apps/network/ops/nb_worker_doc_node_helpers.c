/* nb_worker_doc_node_helpers.c - document owner/DOM-exception helpers (mechanical extract, single-TU include) */
static void nb_set_owner_prop(JSContext *ctx, JSValue wrapper, JSValueConst owner_doc) {
    if (JS_IsObject(wrapper) && JS_IsObject(owner_doc))
        JS_SetPropertyStr(ctx, wrapper, "__nb_owner", JS_DupValue(ctx, owner_doc));
}
static JSValue nb_doc_throw_dom(JSContext *ctx, const char *name, const char *msg) {
    JSValue e = JS_NewError(ctx);
    if (!JS_IsException(e)) {
        JS_SetPropertyStr(ctx, e, "name", JS_NewString(ctx, name));
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, msg));
    }
    return JS_Throw(ctx, e);
}
