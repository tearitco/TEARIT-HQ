/* nb_worker_range_delete.c - Range delete/extract core wrappers (mechanical extract, single-TU include) */
static JSValue nb_range_clone_contents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_range_delete(JSContext *ctx, JSValueConst this_val, int want_extract) {
    JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    JSValue so_v = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
    JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
    JSValue eo_v = JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
    NbNode *s = get_node(ctx, sc);
    NbNode *e = get_node(ctx, ec);
    int32_t so = 0, eo = 0;
    JS_ToInt32(ctx, &so, so_v);
    JS_ToInt32(ctx, &eo, eo_v);
    JSValue frag = JS_NewString(ctx, "");
    if (want_extract) frag = nb_range_clone_contents(ctx, this_val, 0, NULL);
    if (s && e) {
        long ls = nb_endpoint_lin(s, so);
        long le = nb_endpoint_lin(e, eo);
        if (le > ls) {
            NbNode *ca = NULL;
            for (NbNode *p = s; p && !ca; p = p->parent) {
                for (NbNode *q = e; q; q = q->parent) {
                    if (p == q) { ca = p; break; }
                }
            }
            if (!ca) ca = nb_top_root(s);
            if (!ca->tag || !ca->tag[0]) {
                nb_trim_text(ca, ls, le);
            } else {
                nb_prune_node(ctx, ca, ls, le);
            }
        }
    }
    /* collapse the range to its start (endpoints may have moved). */
    JSValue start_c = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    JSValue start_o = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
    JS_SetPropertyStr(ctx, this_val, "__nb_r_ec", JS_DupValue(ctx, start_c));
    JS_SetPropertyStr(ctx, this_val, "__nb_r_eo", JS_DupValue(ctx, start_o));
    JS_FreeValue(ctx, start_c);
    JS_FreeValue(ctx, start_o);
    nb_range_fill(ctx, this_val);
    JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so_v); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo_v);
    return want_extract ? frag : JS_UNDEFINED;
}
static JSValue nb_range_delete_contents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_range_delete(ctx, this_val, 0);
}
static JSValue nb_range_extract_contents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_range_delete(ctx, this_val, 1);
}
