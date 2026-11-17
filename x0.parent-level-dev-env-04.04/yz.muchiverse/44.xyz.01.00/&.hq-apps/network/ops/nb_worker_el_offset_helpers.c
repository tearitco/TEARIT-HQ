/* nb_worker_el_offset_helpers.c - element offset/dimension helpers (mechanical extract, single-TU include) */
static JSValue nb_el_offdim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    NbNode *n = get_this(ctx, this_val);
    if (magic >= 2) return JS_NewFloat64(ctx, nb_client_dim(n, magic & 1));
    int which = magic;
    double v = 0;
    if (n && !css_hidden(n)) {
        NbCssStyle st;
        nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
        v = which ? st.height : st.width;
    }
    return JS_NewFloat64(ctx, v);
}

static JSValue nb_el_offset_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || css_hidden(n)) return JS_NULL;
    NbNode *p = n->parent;
    while (p && !p->tag) p = p->parent;   /* climb past #document */
    if (!p) return JS_NULL;
    return push_node(ctx, p);
}
