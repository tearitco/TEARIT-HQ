/* nb_worker_serialize_get.c - XMLSerializer/outerHTML get serialization helpers (mechanical extract, single-TU include) */
/* ---- XMLSerializer + outerHTML (2026-11-09) ----
 * serializeToString was an inert plain stub: the method did not exist, so
 * every call threw "not a function". This returns the house outer-HTML
 * serializer (node_outer_html, the same one innerHTML reads) for elements
 * and text nodes; a #document serializes its element children (the
 * documentElement subtree). outerHTML is the same serializer as a writable
 * Element accessor. XMLSerializer passes xml=1: XML has no &nbsp; entity and
 * no void-element elision. */
static void nb_ser_fill(const NbNode *n, SB *b, int xml) {
    if (!n) return;
    if (n->tag && !strcmp(n->tag, "#document")) {
        for (const NbNode *c = n->first_child; c; c = c->next_sibling)
            if (c->tag && c->tag[0]) node_outer_html_m(c, b, xml);
    } else {
        node_outer_html_m(n, b, xml);
    }
}
static JSValue nb_xmlserializer_serialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: XMLSerializer.serializeToString requires a node"));
        return JS_Throw(ctx, e);
    }
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: XMLSerializer.serializeToString requires a node"));
        return JS_Throw(ctx, e);
    }
    SB b = {0, 0, 0};
    /* node_outer_html takes a string buffer; sb_* helpers are in this TU. */
    nb_ser_fill(n, &b, 1);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_outerHTML_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    SB b = {0, 0, 0};
    nb_ser_fill(n, &b, 0);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
