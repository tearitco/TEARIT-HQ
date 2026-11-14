/* nb_worker_node_compare.c - Node.compareDocumentPosition helpers (mechanical extract, single-TU include) */
static int nb_is_ancestor_or_self(const NbNode *anc, const NbNode *n) {
    for (const NbNode *p = n; p; p = p->parent)
        if (p == anc) return 1;
    return 0;
}
static int nb_node_precedes(const NbNode *a, const NbNode *b) {
    /* a and b must be different nodes in the same tree, neither ancestor of other. */
    /* collect ancestors of a including a */
    const NbNode *a_chain[256]; int an = 0;
    for (const NbNode *p = a; p && an < 255; p = p->parent) a_chain[an++] = p;
    /* walk up b to find common ancestor */
    const NbNode *b_chain_node = NULL;
    const NbNode *a_branch = NULL;
    for (const NbNode *p = b; p; p = p->parent) {
        for (int i = 0; i < an; i++) {
            if (a_chain[i] == p) {
                b_chain_node = p;
                a_branch = (i == 0) ? a : a_chain[i - 1];
                /* b branch root under common ancestor */
                const NbNode *b_branch = b;
                for (const NbNode *q = b; q && q->parent != p; q = q->parent) b_branch = q->parent ? q : b_branch;
                /* recompute properly */
                b_branch = b;
                for (const NbNode *q = b; q->parent && q->parent != p; q = q->parent) b_branch = q->parent;
                if (b->parent == p) b_branch = b;
                else {
                    b_branch = b;
                    for (const NbNode *q = b; q->parent != p; q = q->parent) b_branch = q->parent;
                }
                /* scan children of common ancestor */
                for (const NbNode *c = p->first_child; c; c = c->next_sibling) {
                    if (c == a_branch) return 1;
                    if (c == b_branch) return 0;
                }
                return 0;
            }
        }
    }
    (void)b_chain_node; (void)a_branch;
    return 0;
}
static JSValue nb_node_compareDocumentPosition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    enum {
        DOC_POS_DISCONNECTED = 1,
        DOC_POS_PRECEDING = 2,
        DOC_POS_FOLLOWING = 4,
        DOC_POS_CONTAINS = 8,
        DOC_POS_CONTAINED_BY = 16,
        DOC_POS_IMPLEMENTATION_SPECIFIC = 32
    };
    NbNode *node = get_this(ctx, this_val);
    if (!node) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        JS_FreeValue(ctx, gdoc);
        if (is_live) node = g_dom_root;
        else return JS_NewInt32(ctx, 0);
    }
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_NewInt32(ctx, DOC_POS_DISCONNECTED | DOC_POS_IMPLEMENTATION_SPECIFIC | DOC_POS_FOLLOWING);
    NbNode *other = get_node(ctx, argv[0]);
    if (!other) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live_other = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(argv[0]);
        JS_FreeValue(ctx, gdoc);
        if (is_live_other) other = g_dom_root;
        else return JS_NewInt32(ctx, DOC_POS_DISCONNECTED | DOC_POS_IMPLEMENTATION_SPECIFIC | DOC_POS_FOLLOWING);
    }
    if (node == other) return JS_NewInt32(ctx, 0);
    NbNode *r1 = (NbNode *)nb_top_root(node);
    NbNode *r2 = (NbNode *)nb_top_root(other);
    if (r1 != r2) {
        /* deterministic but implementation-specific order for disconnected nodes */
        unsigned mask = DOC_POS_DISCONNECTED | DOC_POS_IMPLEMENTATION_SPECIFIC;
        mask |= (r1 < r2) ? DOC_POS_FOLLOWING : DOC_POS_PRECEDING;
        return JS_NewInt32(ctx, (int)mask);
    }
    if (nb_is_ancestor_or_self(other, node)) return JS_NewInt32(ctx, DOC_POS_CONTAINS | DOC_POS_PRECEDING);
    if (nb_is_ancestor_or_self(node, other)) return JS_NewInt32(ctx, DOC_POS_CONTAINED_BY | DOC_POS_FOLLOWING);
    if (nb_node_precedes(other, node)) return JS_NewInt32(ctx, DOC_POS_PRECEDING);
    return JS_NewInt32(ctx, DOC_POS_FOLLOWING);
}
