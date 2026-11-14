/* nb_worker_clone.c - cloneNode/nbi_clone helpers (mechanical extract, single-TU include) */
/* Deep/shallow copy. A shallow copy keeps tag/id/class/attrs/text but no
 * children, which is what cloneNode(false) means. The copy is orphan-listed
 * so teardown reclaims it if the page never inserts it. */
static NbNode *nbi_clone(const NbNode *n, int deep) {
    if (!n) return NULL;
    NbNode *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    if (n->tag)   c->tag   = strdup(n->tag);
    if (n->id)    c->id    = strdup(n->id);
    if (n->cls)   c->cls   = strdup(n->cls);
    if (n->attrs) c->attrs = strdup(n->attrs);
    if (n->text)  c->text  = strdup(n->text);
    orphan_add(c);
    if (deep) {
        for (const NbNode *k = n->first_child; k; k = k->next_sibling) {
            NbNode *kc = nbi_clone(k, 1);
            if (!kc) break;
            orphan_remove(kc);        /* it is inside the copy now, not loose */
            local_append(c, kc);
        }
    }
    return c;
}
static JSValue nb_el_cloneNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NULL;
    int deep = (argc > 0) ? JS_ToBool(ctx, argv[0]) : 0;
    NbNode *c = nbi_clone(n, deep);
    return c ? push_node(ctx, c) : JS_NULL;
}
