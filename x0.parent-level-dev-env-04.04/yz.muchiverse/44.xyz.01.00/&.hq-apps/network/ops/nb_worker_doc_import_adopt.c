/* nb_worker_doc_import_adopt.c - document.importNode/adoptNode bindings (mechanical extract, single-TU include) */
static JSValue nb_doc_importNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return nb_doc_throw_dom(ctx, "TypeError", "importNode requires a node");
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) return nb_doc_throw_dom(ctx, "NotFoundError", "importNode: unknown node");
    if (n->tag && !strcmp(n->tag, "#document"))
        return nb_doc_throw_dom(ctx, "NotSupportedError", "importNode: cannot import a document");
    int deep = 0;
    if (argc > 1) deep = JS_ToBool(ctx, argv[1]);
    NbNode *c = nbi_clone(n, deep);
    if (!c) return JS_ThrowOutOfMemory(ctx);
    JSValue w;
    if (nb_is_doc_fragment_node(c))
        w = make_fragment_wrapper(ctx, c, "#document-fragment");
    else
        w = push_node(ctx, c);
    nb_set_owner_prop(ctx, w, this_val);
    return w;
}
static JSValue nb_doc_adoptNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return nb_doc_throw_dom(ctx, "TypeError", "adoptNode requires a node");
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) return nb_doc_throw_dom(ctx, "NotFoundError", "adoptNode: unknown node");
    if (n->tag && !strcmp(n->tag, "#document"))
        return nb_doc_throw_dom(ctx, "NotSupportedError", "adoptNode: cannot adopt a document");
    if (n->parent) node_detach(n);
    nb_set_owner_prop(ctx, argv[0], this_val);
    return JS_DupValue(ctx, argv[0]);
}
