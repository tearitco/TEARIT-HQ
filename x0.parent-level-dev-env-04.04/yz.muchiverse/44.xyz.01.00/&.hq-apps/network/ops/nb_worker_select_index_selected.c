/* nb_worker_select_index_selected.c - select.selectedIndex and option.selected accessors (mechanical extract, single-TU include) */
/* select.selectedIndex */
static JSValue nb_sel_index_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, -1);
    return JS_NewInt32(ctx, sel_option_index(n, sel_selected(ctx, n)));
}
static JSValue nb_sel_index_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    int32_t i = 0;
    if (argc > 0) JS_ToInt32(ctx, &i, argv[0]);
    sel_select_index(ctx, n, i);
    fire_change(ctx, n);
    return JS_UNDEFINED;
}
/* option.selected — read/write, and writing it keeps the parent select in
 * agreement so select.value never contradicts the option it points at. */
static JSValue nb_opt_selected_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewBool(ctx, 0);
    return JS_NewBool(ctx, opt_selected(ctx, n));
}
static JSValue nb_opt_selected_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    int on = (argc > 0) ? JS_ToBool(ctx, argv[0]) : 1;
    opt_set_selected(ctx, n, on);
    NbNode *sel = n->parent;
    if (node_is_tag(sel, "select")) {
        if (on) {
            int i = 0;
            for (NbNode *c = sel->first_child; c; c = c->next_sibling) {
                if (!node_is_tag(c, "option")) continue;
                opt_set_selected(ctx, c, c == n);
                i++;
            }
        }
        fire_change(ctx, sel);
    }
    return JS_UNDEFINED;
}
