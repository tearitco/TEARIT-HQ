/* nb_worker_node_set_owner.c - Node owner setter helper (mechanical extract, single-TU include) */
static void nb_set_owner_if_doc(JSContext *ctx, JSValue wrapper, JSValueConst creator) {
    if (!JS_IsObject(wrapper) || !JS_IsObject(creator)) return;
    JSValue gdoc = nb_global_document(ctx);
    int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(creator);
    JS_FreeValue(ctx, gdoc);
    if (is_live) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
        return;
    }
    NbNode *cn = get_this(ctx, creator);
    if (cn && cn->tag && !strcmp(cn->tag, "#document")) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
        return;
    }
    JSValue ct = JS_GetPropertyStr(ctx, creator, "contentType");
    int looks_doc = JS_IsString(ct);
    JS_FreeValue(ctx, ct);
    if (looks_doc) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
    }
}
