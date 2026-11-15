/* nb_worker_range_helpers.c - Range/Selection text and endpoint helpers (mechanical extract, single-TU include) */
/* ---- Selection and Range (2026-11-09) ----
 * The previous Range/Selection were inert plain stubs: new Range() created
 * an empty object with no endpoints, and window.getSelection did not exist
 * at all. This is a scoped first slice: endpoints, boundary setters,
 * selectNode/selectNodeContents, collapse/cloneRange, cloneContents,
 * deleteContents, extractContents, toString over the flattened text stream,
 * and a Selection managing ranges (addRange, removeAllRanges, collapse,
 * getRangeAt, toString, anchor/focus). Geometric rects are deliberately
 * not claimed yet. Text order follows node_text_content (an element's own
 * text precedes its children). */
static long nb_txt_total(const NbNode *n) {
    if (!n) return 0;
    long len = n->text ? (long)strlen(n->text) : 0;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        len += nb_txt_total(c);
    return len;
}
static long nb_linpos(const NbNode *n) {
    if (!n || !n->parent) return 0;
    long pos = nb_linpos(n->parent);
    if (n->parent->text) pos += (long)strlen(n->parent->text);
    for (const NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling)
        pos += nb_txt_total(s);
    return pos;
}
static const NbNode *nb_top_root(const NbNode *n) {
    if (!n) return NULL;
    while (n->parent) n = n->parent;
    return n;
}
static int nb_child_count(const NbNode *n) {
    int c = 0;
    if (!n) return 0;
    for (const NbNode *x = n->first_child; x; x = x->next_sibling) c++;
    return c;
}
static long nb_endpoint_lin(const NbNode *container, long offset) {
    if (!container) return 0;
    if (!container->tag || !container->tag[0]) {
        long base = nb_linpos(container);
        long max = container->text ? (long)strlen(container->text) : 0;
        if (offset < 0) offset = 0;
        if (offset > max) offset = max;
        return base + offset;
    }
    long base = nb_linpos(container);
    if (offset <= 0) return base;
    long pos = base + (container->text ? (long)strlen(container->text) : 0);
    const NbNode *c = container->first_child;
    for (long k = 0; k < offset && c; k++) { pos += nb_txt_total(c); c = c->next_sibling; }
    return pos;
}
static void nb_flatten(const NbNode *n, SB *b) {
    if (!n) return;
    sb_put(b, n->text);
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        nb_flatten(c, b);
}
static JSValue nb_range_compute_text(JSContext *ctx, JSValue r) {
    JSValue sc = JS_GetPropertyStr(ctx, r, "__nb_r_sc");
    JSValue so_v = JS_GetPropertyStr(ctx, r, "__nb_r_so");
    JSValue ec = JS_GetPropertyStr(ctx, r, "__nb_r_ec");
    JSValue eo_v = JS_GetPropertyStr(ctx, r, "__nb_r_eo");
    NbNode *s = get_node(ctx, sc);
    NbNode *e = get_node(ctx, ec);
    int32_t so = 0, eo = 0;
    JS_ToInt32(ctx, &so, so_v);
    JS_ToInt32(ctx, &eo, eo_v);
    JSValue out = JS_NewString(ctx, "");
    if (s && e) {
        SB b = {0, 0, 0};
        nb_flatten(nb_top_root(s), &b);
        long ls = nb_endpoint_lin(s, so);
        long le = nb_endpoint_lin(e, eo);
        char *str = b.s ? b.s : "";
        long total = (long)strlen(str);
        if (ls < 0) ls = 0;
        if (le < ls) le = ls;
        if (ls > total) ls = total;
        if (le > total) le = total;
        JS_FreeValue(ctx, out);
        out = JS_NewStringLen(ctx, str + ls, (size_t)(le - ls));
        free(b.s);
    }
    JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so_v);
    JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo_v);
    return out;
}
static void nb_range_fill(JSContext *ctx, JSValue r) {
    JSValue t = nb_range_compute_text(ctx, r);
    JS_SetPropertyStr(ctx, r, "__nb_r_text", JS_DupValue(ctx, t));
    JS_FreeValue(ctx, t);
}
static int nb_validate_endpoint(JSContext *ctx, JSValue v, long *out) {
    if (!JS_IsObject(v)) return 0;
    NbNode *n = get_node(ctx, v);
    if (!n) return 0;
    long max = (!n->tag || !n->tag[0]) ? (n->text ? (long)strlen(n->text) : 0) : nb_child_count(n);
    *out = max;
    return 1;
}
