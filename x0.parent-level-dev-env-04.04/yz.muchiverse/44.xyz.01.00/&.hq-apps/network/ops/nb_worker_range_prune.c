/* nb_worker_range_prune.c - Range text trim/delete/prune helpers (mechanical extract, single-TU include) */
static void nb_trim_text(NbNode *n, long ls, long le) {
    if (!n || !n->text) return;
    long own_s = nb_linpos(n);
    long own_e = own_s + (long)strlen(n->text);
    long cs = ls > own_s ? ls : own_s;
    long ce = le < own_e ? le : own_e;
    if (ce <= cs) return;
    size_t del = (size_t)(ce - cs);
    size_t off = (size_t)(cs - own_s);
    char *t = n->text;
    size_t rest = strlen(t + off + del);
    memmove(t + off, t + off + del, rest + 1);
}
static void nb_delete_from(JSContext *ctx, NbNode *parent, NbNode *child) {
    NbNode *prev = NULL;
    for (NbNode *c = parent->first_child; c && c != child; c = c->next_sibling) prev = c;
    NbNode *next = child->next_sibling;
    node_detach(child);
    orphan_add(child);
    nb_mutation_notify_childlist(ctx, parent, NULL, child, prev, next);
}
static void nb_prune_node(JSContext *ctx, NbNode *parent, long ls, long le);
static void nb_prune_node(JSContext *ctx, NbNode *parent, long ls, long le) {
    if (!parent) return;
    /* snapshot children (spans computed live, reverse document order so
     * deletions never shift not-yet-visited spans). */
    int n = nb_child_count(parent);
    if (n == 0) { nb_trim_text(parent, ls, le); return; }
    NbNode **kids = malloc((size_t)n * sizeof(*kids));
    if (!kids) return;
    int ki = 0;
    for (NbNode *c = parent->first_child; c; c = c->next_sibling) kids[ki++] = c;
    for (int i = ki - 1; i >= 0; i--) {
        NbNode *c = kids[i];
        long low = nb_linpos(c);
        long high = low + nb_txt_total(c);
        if (high <= ls || low >= le) continue; /* disjoint */
        if (low >= ls && high <= le) {
            nb_delete_from(ctx, parent, c);
        } else if (!c->tag || !c->tag[0]) {
            nb_trim_text(c, ls, le);
        } else {
            nb_prune_node(ctx, c, ls, le);
        }
    }
    free(kids);
    /* own text trims last (it precedes children in text order). */
    nb_trim_text(parent, ls, le);
}
