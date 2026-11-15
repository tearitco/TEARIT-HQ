/* nb_worker_insert_adjacent_place.c - insertAdjacent placement/attach helpers (mechanical extract, single-TU include) */
/* Resolve the insertion parent/reference for a position. Returns 0 for the
 * no-op case (beforebegin/afterend with no parent). */
static int nb_ia_place(NbNode *n, int pos, NbNode **parent, NbNode **ref) {
    *parent = NULL;
    *ref = NULL;
    if (!n) return 0;
    if (pos == NB_IA_BEFORE_BEGIN || pos == NB_IA_AFTER_END) {
        if (!n->parent) return 0;
        *parent = n->parent;
        *ref = (pos == NB_IA_BEFORE_BEGIN) ? n : n->next_sibling;
        return 1;
    }
    *parent = n;
    *ref = (pos == NB_IA_AFTER_BEGIN) ? n->first_child : NULL;
    return 1;
}
static void nb_ia_attach(JSContext *ctx, NbNode *parent, NbNode *ref, NbNode *node) {
    if (!parent || !node || node == parent || node == ref) return;   /* avoid self-cycle */
    node_detach(node);
    orphan_remove(node);
    local_insert_before(parent, node, ref);
    nb_mutation_notify_childlist(ctx, parent, node, NULL, NULL, ref);
}
