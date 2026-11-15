/* nb_worker_range_props_boundary.c - Range property/get and boundary helpers (mechanical extract, single-TU include) */
static JSValue nb_range_prop_common(JSContext *ctx, JSValueConst this_val) {
    JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
    NbNode *s = get_node(ctx, sc);
    NbNode *e = get_node(ctx, ec);
    JS_FreeValue(ctx, sc); JS_FreeValue(ctx, ec);
    if (!s || !e) return JS_NULL;
    NbNode *anc = g_dom_root;
    for (NbNode *p = s; p; p = p->parent) {
        for (NbNode *q = e; q; q = q->parent) if (p == q) { anc = p; break; }
    }
    return anc ? push_node(ctx, anc) : JS_NULL;
}
static JSValue nb_range_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)argc; (void)argv;
    switch (magic) {
    case 0: return JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    case 1: return JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
    case 2: return JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
    case 3: return JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
    case 4: {
        JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
        JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
        JSValue so = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
        JSValue eo = JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
        int same = JS_IsObject(sc) && JS_IsObject(ec) && (JS_StrictEq(ctx, sc, ec) || get_node(ctx, sc) == get_node(ctx, ec));
        int32_t a = 0, b = 0; JS_ToInt32(ctx, &a, so); JS_ToInt32(ctx, &b, eo);
        same = same && (a == b);
        JS_FreeValue(ctx, sc); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, so); JS_FreeValue(ctx, eo);
        return JS_NewBool(ctx, same);
    }
    case 5: return nb_range_prop_common(ctx, this_val);
    default: {
        JSValue cache = JS_GetPropertyStr(ctx, this_val, "__nb_r_text");
        if (!JS_IsString(cache)) {
            JS_FreeValue(ctx, cache);
            nb_range_fill(ctx, this_val);
            cache = JS_GetPropertyStr(ctx, this_val, "__nb_r_text");
        }
        if (!JS_IsString(cache)) { JS_FreeValue(ctx, cache); return JS_NewString(ctx, ""); }
        JSValue out = cache;
        return out;
    }
    }
}
static JSValue nb_range_set_m(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_range_set(ctx, this_val, argc, argv, magic ? 0 : 1);
}
static int nb_node_index(NbNode *n) {
    if (!n || !n->parent) return -1;
    int i = 0;
    for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) i++;
    return i;
}
static JSValue nb_range_before_after(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_start, int after) {
    (void)argc;
    if (!JS_IsObject(argv[0])) return JS_UNDEFINED;
    NbNode *n = get_node(ctx, argv[0]);
    if (!n || !n->parent || !n->parent->tag) {
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "InvalidNodeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "InvalidNodeError: node has no parent"));
        return JS_Throw(ctx, er);
    }
    int idx = nb_node_index(n);
    int off = after ? idx + 1 : idx;
    JSValue p = push_node(ctx, n->parent);
    JSValue a[2] = { p, JS_NewInt32(ctx, off) };
    JSValue r = nb_range_set(ctx, this_val, 2, a, is_start);
    JS_FreeValue(ctx, p);
    return r;
}
static JSValue nb_range_ba_m(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    int is_start = (magic == 0 || magic == 1);
    int after = (magic == 1 || magic == 3);
    return nb_range_before_after(ctx, this_val, argc, argv, is_start, after);
}
