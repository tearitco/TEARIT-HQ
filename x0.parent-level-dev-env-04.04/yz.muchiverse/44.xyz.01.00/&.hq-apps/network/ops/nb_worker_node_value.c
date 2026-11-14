/* nb_worker_node_value.c - nodeValue get/set helpers (mechanical extract, single-TU include) */
static JSValue nb_node_value_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NULL;
    if (!n->tag || !n->tag[0] || (n->tag && !strcmp(n->tag, "#comment")))
        return JS_NewString(ctx, n->text ? n->text : "");
    return JS_NULL;
}
static JSValue nb_node_value_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (!n->tag || !n->tag[0] || (n->tag && !strcmp(n->tag, "#comment")))
        return nb_text_data_set(ctx, this_val, argc, argv);
    return JS_UNDEFINED;
}
