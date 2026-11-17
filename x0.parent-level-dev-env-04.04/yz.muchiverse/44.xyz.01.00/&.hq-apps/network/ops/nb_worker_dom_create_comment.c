/* nb_worker_dom_create_comment.c - document.createComment binding (mechanical extract, single-TU include) */
static JSValue nb_dom_createComment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *v = "";
    if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]))
        v = (owned = JS_ToCString(ctx, argv[0]));
    if (argc > 0 && !owned && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]))
        return JS_EXCEPTION;
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_ThrowOutOfMemory(ctx); }
    n->tag = strdup("#comment");
    n->text = strdup(v ? v : "");
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    JSValue w = push_node(ctx, n);
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}
