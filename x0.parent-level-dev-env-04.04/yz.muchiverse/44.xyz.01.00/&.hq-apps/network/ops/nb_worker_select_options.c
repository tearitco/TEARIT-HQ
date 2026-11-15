/* nb_worker_select_options.c - select options/length/index accessors (mechanical extract, single-TU include) */
/* select.options (2026-10-05): a live array of the <option> children, plus
 * select.length and option.index to round out the collection. Pages iterate
 * options directly ("for (var i=0;i<sel.options.length;i++)"), so without
 * this a dropdown cannot be read at all. Returns fresh wrapper objects each
 * call; push_node()'s ID map makes them identity-stable per node. */
static JSValue nb_sel_options(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (!n) return arr;
    uint32_t k = 0;
    for (NbNode *c = n->first_child; c; c = c->next_sibling) {
        if (!node_is_tag(c, "option")) continue;
        JS_SetPropertyUint32(ctx, arr, k++, push_node(ctx, c));
    }
    return arr;
}
static int sel_option_count(const NbNode *sel) {
    int i = 0;
    for (NbNode *c = sel->first_child; c; c = c->next_sibling)
        if (node_is_tag(c, "option")) i++;
    return i;
}
static JSValue nb_sel_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewInt32(ctx, n ? sel_option_count(n) : 0);
}
static JSValue nb_opt_index(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, -1);
    NbNode *sel = n->parent;
    if (!node_is_tag(sel, "select")) return JS_NewInt32(ctx, -1);
    return JS_NewInt32(ctx, sel_option_index(sel, n));
}
