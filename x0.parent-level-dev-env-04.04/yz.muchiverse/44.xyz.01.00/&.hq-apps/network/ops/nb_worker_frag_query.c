/* nb_worker_frag_query.c - fragment query helpers (mechanical extract, single-TU include) */
static JSValue nb_frag_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !sel) { JS_FreeCString(ctx, owned); return JS_NULL; }
    NbNode *r = query_first(n, sel);
    JS_FreeCString(ctx, owned);
    if (r) return push_node(ctx, r);
    return JS_NULL;
}
static JSValue nb_frag_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n && sel) {
        int i = 0;
        qsa_into(ctx, n, sel, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "NodeList");
    return arr;
}
static JSValue nb_frag_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *id = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !id) { JS_FreeCString(ctx, owned); return JS_NULL; }
    NbNode *r = find_by_id(n, id);
    JS_FreeCString(ctx, owned);
    if (r) return push_node(ctx, r);
    return JS_NULL;
}
static JSValue nb_frag_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        collect_tag_into(ctx, n, tag, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    return arr;
}
static JSValue nb_frag_getElementsByClassName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (n && tok && *tok) {
        int i = 0;
        collect_cls_into(ctx, n, tok, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    return arr;
}

