/* nb_worker_frag_clone.c - fragment clone helpers (mechanical extract, single-TU include) */
static NbNode *node_deep_copy(const NbNode *n) {
    NbNode *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    if (n->tag) c->tag = strdup(n->tag);
    if (n->id) c->id = strdup(n->id);
    if (n->cls) c->cls = strdup(n->cls);
    if (n->attrs) c->attrs = strdup(n->attrs);
    if (n->text) c->text = strdup(n->text);
    for (const NbNode *ch = n->first_child; ch; ch = ch->next_sibling) {
        NbNode *cc = node_deep_copy(ch);
        if (cc) local_append(c, cc);
    }
    return c;
}
static JSValue nb_frag_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int deep = (argc > 0) ? JS_ToBool(ctx, argv[0]) : 0;
    NbNode *frag = calloc(1, sizeof(*frag));
    if (!frag) return JS_NULL;
    frag->tag = strdup("#document-fragment");
    if (n && deep) {
        for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
            NbNode *cc = nbi_clone(c, 1);
            if (!cc) break;
            orphan_remove(cc);
            local_append(frag, cc);
        }
    }
    orphan_add(frag);
    return make_fragment_wrapper(ctx, frag, "#document-fragment");
}

