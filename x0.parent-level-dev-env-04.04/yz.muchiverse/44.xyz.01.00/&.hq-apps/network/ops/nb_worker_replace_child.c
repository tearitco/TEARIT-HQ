/* nb_worker_replace_child.c - Node.replaceChild (mechanical extract, single-TU include) */
static JSValue nb_el_replaceChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *nn = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    NbNode *on = (argc > 1 && JS_IsObject(argv[1])) ? get_node(ctx, argv[1]) : NULL;
    if (!n || !nn || !on || nn == on || nn == n) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "HierarchyRequestError: replaceChild needs two distinct real nodes"));
        return JS_Throw(ctx, e);
    }
    if (!is_child_of(n, on)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the old child is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    if (nb_is_doc_fragment_node(nn)) {
        if (nb_node_is_ancestor(nn, n))
            return nb_throw_hierarchy(ctx, "HierarchyRequestError: the fragment contains this node");
        NbNode *on_prev = NULL, *on_next = on->next_sibling;
        for (NbNode *c = n->first_child; c && c != on; c = c->next_sibling)
            on_prev = c;
        orphan_remove(nn);
        nb_insert_fragment_children(ctx, n, nn, on);
        orphan_add(nn);
        node_detach(on);
        orphan_add(on);
        nb_mutation_notify_childlist(ctx, n, NULL, on, on_prev, on_next);
        return push_node(ctx, on);
    }
    NbNode *old_prev = NULL, *old_next = on->next_sibling;
    for (NbNode *c = n->first_child; c; c = c->next_sibling) {
        if (c == on) break;
        old_prev = c;
    }
    node_detach(nn);                    /* newChild may live in this same list */
    orphan_remove(nn);
    NbNode *after = on->next_sibling;   /* correct after nn's detach relinks */
    node_detach(on);
    orphan_add(on);
    local_insert_before(n, nn, after);
    nb_mutation_notify_childlist(ctx, n, nn, on, old_prev, old_next);
    return push_node(ctx, on);          /* DOM returns the replaced child */
}
