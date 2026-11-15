/* nb_worker_insert_before.c - Node.insertBefore (mechanical extract, single-TU include) */
static JSValue nb_el_insertBefore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *nn = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    NbNode *rn = (argc > 1 && JS_IsObject(argv[1])) ? get_node(ctx, argv[1]) : NULL;
    if (!n || !nn || nn == n) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "HierarchyRequestError: insertBefore needs a real new node"));
        return JS_Throw(ctx, e);
    }
    if (rn && !is_child_of(n, rn)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the reference node is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    if (nb_is_doc_fragment_node(nn)) {
        if (nb_node_is_ancestor(nn, n))
            return nb_throw_hierarchy(ctx, "HierarchyRequestError: the fragment contains this node");
        orphan_remove(nn);
        nb_insert_fragment_children(ctx, n, nn, rn);
        orphan_add(nn);
        return JS_DupValue(ctx, argv[0]);   /* spec returns the fragment */
    }
    node_detach(nn);
    orphan_remove(nn);
    local_insert_before(n, nn, rn);
    nb_mutation_notify_childlist(ctx, n, nn, NULL, NULL, rn);
    return push_node(ctx, nn);
}
