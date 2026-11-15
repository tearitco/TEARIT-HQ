/* nb_worker_outerhtml_set.c - outerHTML setter (mechanical extract, single-TU include) */
/* outerHTML = value: parse in the parent's context and REPLACE this node
 * with the parsed nodes, in order. Spec throws NoModificationAllowedError
 * when there is no parent or the parent is a Document (an element cannot
 * replace the document element from inside the document). One childList
 * record per moved node, then one for the removed node - the same
 * per-node record model the tree mutators use. */
static JSValue nb_el_outerHTML_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    NbNode *parent = n->parent;
    if (!parent || (parent->tag && !strcmp(parent->tag, "#document"))) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NoModificationAllowedError: outerHTML needs a parent element"));
        return JS_Throw(ctx, e);
    }
    char *vl = NULL;
    const char *v = "";
    if (argc > 0 && !JS_IsNull(argv[0])) {
        v = vl = JS_ToCString(ctx, argv[0]);
        if (!v) return JS_EXCEPTION;
    }
    NbNode *ref = n->next_sibling;
    NbNode *prev = NULL;
    for (NbNode *c = parent->first_child; c && c != n; c = c->next_sibling) prev = c;
    NbNode *frag = nb_parse_html_nodes(v, strlen(v));
    NbNode *c = frag ? frag->first_child : NULL;
    if (frag) { frag->first_child = frag->last_child = NULL; }
    while (c) {
        NbNode *nx = c->next_sibling;
        c->next_sibling = NULL;
        c->parent = NULL;
        local_insert_before(parent, c, ref);
        nb_mutation_notify_childlist(ctx, parent, c, NULL, NULL, ref);
        c = nx;
    }
    node_detach(n);
    orphan_add(n);
    nb_mutation_notify_childlist(ctx, parent, NULL, n, prev, ref);
    if (frag) nb_node_free(frag);
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
