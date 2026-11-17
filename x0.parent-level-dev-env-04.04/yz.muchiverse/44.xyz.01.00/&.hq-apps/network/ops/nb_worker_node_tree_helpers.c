/* nb_worker_node_tree_helpers.c - basic node tree helpers (mechanical extract, single-TU include) */
/* ---- node tree helpers (NbNode fields are public in nb_dom.h) ---- */
static void local_append(NbNode *parent, NbNode *child) {
    child->parent = parent;
    child->next_sibling = NULL;
    if (parent->last_child) parent->last_child->next_sibling = child;
    else parent->first_child = child;
    parent->last_child = child;
}
/* rung-2 remainder: insert `newn` before `refn` (a child of `parent`, or
 * NULL to append), like DOM insertBefore. Local append when refn is NULL. */
static void local_insert_before(NbNode *parent, NbNode *newn, NbNode *refn) {
    newn->parent = parent;
    if (!refn) { local_append(parent, newn); return; }
    newn->next_sibling = refn;
    NbNode *prev = NULL;
    for (NbNode *c = parent->first_child; c; c = c->next_sibling) {
        if (c == refn) break;
        prev = c;
    }
    if (prev) prev->next_sibling = newn;
    else parent->first_child = newn;
}
static int is_child_of(NbNode *parent, NbNode *ch) {
    if (!parent || !ch) return 0;
    for (NbNode *c = parent->first_child; c; c = c->next_sibling)
        if (c == ch) return 1;
    return 0;
}
static void node_detach(NbNode *n) {
    if (!n || !n->parent) return;
    NbNode *p = n->parent, *prev = NULL;
    for (NbNode *c = p->first_child; c; c = c->next_sibling) {
        if (c == n) break;
        prev = c;
    }
    if (prev) prev->next_sibling = n->next_sibling;
    else p->first_child = n->next_sibling;
    if (p->last_child == n) p->last_child = prev;
    n->parent = NULL;
    n->next_sibling = NULL;
}
static void clear_children(NbNode *n) {
    while (n->first_child) {
        NbNode *c = n->first_child;
        n->first_child = c->next_sibling;
        if (n->last_child == c) n->last_child = NULL;
        nb_node_free(c);
    }
    n->last_child = NULL;
}
static void orphan_add(NbNode *n) {
    n->next_sibling = (NbNode *)g_orphans;
    g_orphans = n;
}
static void orphan_remove(NbNode *n) {
    NbNode *prev = NULL;
    for (NbNode *o = g_orphans; o; o = o->next_sibling) {
        if (o == n) {
            if (prev) prev->next_sibling = o->next_sibling;
            else g_orphans = o->next_sibling;
            o->next_sibling = NULL;
            return;
        }
        prev = o;
    }
}
