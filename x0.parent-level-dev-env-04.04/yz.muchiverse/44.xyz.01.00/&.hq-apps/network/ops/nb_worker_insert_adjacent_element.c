/* nb_worker_insert_adjacent_element.c - Element.insertAdjacentElement (mechanical extract, single-TU include) */
static JSValue nb_el_insertAdjacentElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int pos = (argc > 0) ? nb_ia_parse_pos(ctx, argv[0]) : -1;
    if (pos == -2) return JS_EXCEPTION;
    if (pos < 0) return nb_ia_syntax_error(ctx);
    NbNode *node = (argc > 1 && JS_IsObject(argv[1])) ? get_node(ctx, argv[1]) : NULL;
    if (!n || !node) return JS_NULL;
    /* An element cannot be inserted into its own descendant. */
    for (NbNode *a = n; a; a = a->parent)
        if (a == node) {
            JSValue e = JS_NewError(ctx);
            if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
                JS_NewString(ctx, "HierarchyRequestError: the new element is an ancestor of the target"));
            return JS_Throw(ctx, e);
        }
    NbNode *parent = NULL, *ref = NULL;
    if (!nb_ia_place(n, pos, &parent, &ref)) return JS_NULL;
    if (node == n) return JS_NULL;
    nb_ia_attach(ctx, parent, ref, node);
    return push_node(ctx, node);
}
