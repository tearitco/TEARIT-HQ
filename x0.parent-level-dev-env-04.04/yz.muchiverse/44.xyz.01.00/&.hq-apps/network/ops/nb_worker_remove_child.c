/* nb_worker_remove_child.c - Node.removeChild (mechanical extract, single-TU include) */
/* rung-2 remainder: the tree mutators. A removed node is orphaned, not
 * freed, so a JS wrapper still referencing it stays valid (teardown frees
 * the orphan list). Mirrors DOM errors for the wrong parent/child cases. */
static JSValue nb_el_removeChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *ch = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!n || !ch) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: removeChild needs an element child"));
        return JS_Throw(ctx, e);
    }
    if (!is_child_of(n, ch)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the node is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    NbNode *prev = NULL, *next = ch->next_sibling;
    for (NbNode *c = n->first_child; c; c = c->next_sibling) {
        if (c == ch) break;
        prev = c;
    }
    node_detach(ch);
    orphan_add(ch);
    nb_mutation_notify_childlist(ctx, n, NULL, ch, prev, next);
    return push_node(ctx, ch);
}
