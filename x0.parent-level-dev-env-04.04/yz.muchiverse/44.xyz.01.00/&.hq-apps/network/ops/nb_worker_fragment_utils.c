/* nb_worker_fragment_utils.c - DocumentFragment insertion/validation helpers (mechanical extract, single-TU include) */
/* ---- DocumentFragment insertion (2026-11-11) ----
 * A fragment is not a node a tree can hold: inserting one MOVES its children
 * into the target, in order, and leaves the fragment empty (DOM insertion
 * steps). `ctx` NULL keeps the call record-free for the ParentNode helpers
 * (append/prepend/before/after/replaceChildren), which have never emitted
 * childList records; appendChild/insertBefore/replaceChild pass ctx and get
 * one record per moved child. The emptied fragment stays on the orphan list
 * so a wrapper still pointing at it remains valid and teardown frees it. */
static int nb_is_doc_fragment_node(const NbNode *n) {
    return n && n->tag && !strcmp(n->tag, "#document-fragment");
}
static int nb_node_is_ancestor(const NbNode *anc, const NbNode *n) {
    for (const NbNode *p = n; p; p = p->parent)
        if (p == anc) return 1;
    return 0;
}
static void nb_insert_fragment_children(JSContext *ctx, NbNode *parent, NbNode *frag, NbNode *ref) {
    NbNode *c = frag->first_child;
    frag->first_child = NULL;
    frag->last_child = NULL;
    while (c) {
        NbNode *nx = c->next_sibling;
        c->parent = NULL;
        c->next_sibling = NULL;
        local_insert_before(parent, c, ref);
        if (ctx) nb_mutation_notify_childlist(ctx, parent, c, NULL, NULL, ref);
        c = nx;
    }
}
static JSValue nb_throw_hierarchy(JSContext *ctx, const char *msg) {
    JSValue e = JS_NewError(ctx);
    if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, msg));
    return JS_Throw(ctx, e);
}
