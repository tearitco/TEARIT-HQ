/* nb_worker_parsed_doc_accessors.c - parsed-document accessors (mechanical extract, single-TU include) */
static JSValue nb_parsed_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *id = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    NbNode *doc = get_this(ctx, this_val);
    if (!id || !doc) { JS_FreeCString(ctx, owned); return JS_NULL; }
    NbNode *r = NULL;
    for (const NbNode *c = doc->first_child; c && !r; c = c->next_sibling)
        r = find_by_id(c, id);
    JS_FreeCString(ctx, owned);
    return r ? push_node(ctx, r) : JS_NULL;
}
static JSValue nb_parsed_documentElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *doc = get_this(ctx, this_val);
    if (!doc) return JS_NULL;
    for (const NbNode *c = doc->first_child; c; c = c->next_sibling)
        if (c->tag && c->tag[0]) return push_node(ctx, (NbNode *)c);
    return JS_NULL;
}
static JSValue nb_parsed_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *doc = get_this(ctx, this_val);
    NbNode *el = doc ? find_tag_first(doc, "body") : NULL;
    return el ? push_node(ctx, el) : JS_NULL;
}
static JSValue nb_parsed_head(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *doc = get_this(ctx, this_val);
    NbNode *el = doc ? find_tag_first(doc, "head") : NULL;
    return el ? push_node(ctx, el) : JS_NULL;
}
