/* nb_worker_el_match_closest.c - element localName/matches/closest (mechanical extract, single-TU include) */
/* el.matches(sel) / el.closest(sel). matches() tests the element ITSELF -
 * that is the whole difference from querySelector, which is scoped BELOW its
 * receiver. closest() walks self-then-ancestors, which is what makes it
 * usable on an event target. */
static JSValue nb_el_localName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, (n && n->tag) ? n->tag : "");
}
static JSValue nb_el_matches(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    int r = (n && sel) ? match_any_selector(n, sel) : 0;
    JS_FreeCString(ctx, owned);
    return JS_NewBool(ctx, r);
}
static JSValue nb_el_closest(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    NbNode *hit = NULL;
    if (n && sel)
        for (NbNode *p = n; p; p = p->parent)
            if (match_any_selector(p, sel)) { hit = p; break; }
    JS_FreeCString(ctx, owned);
    return hit ? push_node(ctx, hit) : JS_NULL;
}
