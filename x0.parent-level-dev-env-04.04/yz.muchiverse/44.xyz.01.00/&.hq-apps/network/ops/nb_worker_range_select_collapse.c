/* nb_worker_range_select_collapse.c - Range selectNode/selectNodeContents/collapse (mechanical extract, single-TU include) */
static JSValue nb_range_select_node(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
    JSValue p = push_node(ctx, n->parent);
    JSValue a[2] = { p, JS_NewInt32(ctx, idx) };
    JSValue r = nb_range_set(ctx, this_val, 2, a, 1);
    JS_FreeValue(ctx, p);
    if (!JS_IsException(r)) {
        p = push_node(ctx, n->parent);
        a[0] = p; a[1] = JS_NewInt32(ctx, idx + 1);
        JSValue r2 = nb_range_set(ctx, this_val, 2, a, 0);
        JS_FreeValue(ctx, r2);
        JS_FreeValue(ctx, p);
    }
    return r;
}
static JSValue nb_range_select_contents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc;
    if (!JS_IsObject(argv[0])) return JS_UNDEFINED;
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) return JS_UNDEFINED;
    int cnt = nb_child_count(n);
    JSValue a[2] = { JS_DupValue(ctx, argv[0]), JS_NewInt32(ctx, 0) };
    JSValue r = nb_range_set(ctx, this_val, 2, a, 1);
    if (!JS_IsException(r)) {
        a[0] = JS_DupValue(ctx, argv[0]); a[1] = JS_NewInt32(ctx, cnt);
        JSValue r2 = nb_range_set(ctx, this_val, 2, a, 0);
        JS_FreeValue(ctx, r2);
    }
    return r;
}
static JSValue nb_range_collapse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int to_start = argc > 0 ? JS_ToBool(ctx, argv[0]) : 0;
    if (to_start) {
        JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
        JSValue so = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
        JS_SetPropertyStr(ctx, this_val, "__nb_r_ec", sc);
        JS_SetPropertyStr(ctx, this_val, "__nb_r_eo", so);
        JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so);
    } else {
        JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
        JSValue eo = JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
        JS_SetPropertyStr(ctx, this_val, "__nb_r_sc", ec);
        JS_SetPropertyStr(ctx, this_val, "__nb_r_so", eo);
        JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo);
    }
    nb_range_fill(ctx, this_val);
    return JS_UNDEFINED;
}
