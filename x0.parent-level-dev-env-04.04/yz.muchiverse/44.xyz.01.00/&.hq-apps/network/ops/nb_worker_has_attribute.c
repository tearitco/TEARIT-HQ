/* nb_worker_has_attribute.c - hasAttribute helper (mechanical extract, single-TU include) */
static JSValue nb_el_hasAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, owned); return JS_FALSE; }
    int has = attrs_has(n, name);
    JS_FreeCString(ctx, owned);
    return JS_NewBool(ctx, has);
}
