/* nb_worker_el_query.c - element query/collection methods (mechanical extract, single-TU include) */
static JSValue nb_el_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    NbNode *hit = NULL;
    /* descendants only: query_first() tests the node it is given, so start at
     * the children rather than at n, or an element would match itself. */
    if (n && sel)
        for (NbNode *c = n->first_child; c && !hit; c = c->next_sibling)
            hit = query_first(c, sel);
    JS_FreeCString(ctx, owned);
    return hit ? push_node(ctx, hit) : JS_NULL;
}
static JSValue nb_el_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n && sel) {
        int i = 0;
        for (NbNode *c = n->first_child; c; c = c->next_sibling)
            qsa_into(ctx, c, sel, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "NodeList");
    return arr;
}
static JSValue nb_el_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (NbNode *c = n->first_child; c; c = c->next_sibling)
            collect_tag_into(ctx, c, tag, arr, &i);   /* descendants only, as above */
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
static JSValue nb_el_getElementsByClassName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n && tok && *tok) {
        int i = 0;
        for (NbNode *c = n->first_child; c; c = c->next_sibling)
            collect_cls_into(ctx, c, tok, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
