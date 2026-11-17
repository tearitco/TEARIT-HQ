/* nb_worker_dom_query_helpers.c - document query helpers (mechanical extract, single-TU include) */
static JSValue nb_dom_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *id = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!id || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static void collect_tag_into(JSContext *ctx, NbNode *n, const char *tag, JSValue arr, int *i) {
    if (!n) return;
    if (!tag || !*tag || !strcmp(tag, "*") || (n->tag && !strcasecmp(n->tag, tag))) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_tag_into(ctx, (NbNode *)c, tag, arr, i);
}
static JSValue nb_dom_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            collect_tag_into(ctx, (NbNode *)c, tag, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
static void qsa_into(JSContext *ctx, NbNode *n, const char *sel, JSValue arr, int *i) {
    if (!n) return;
    if (match_any_selector(n, sel)) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        qsa_into(ctx, (NbNode *)c, sel, arr, i);
}
static JSValue nb_dom_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!sel || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static JSValue nb_dom_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root && sel) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            qsa_into(ctx, (NbNode *)c, sel, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "NodeList");
    return arr;
}
