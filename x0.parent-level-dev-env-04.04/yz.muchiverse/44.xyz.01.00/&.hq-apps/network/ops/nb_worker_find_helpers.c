/* nb_worker_find_helpers.c - basic DOM find helpers (mechanical extract, single-TU include) */
static NbNode *find_by_id(const NbNode *n, const char *id) {
    if (!n || !id) return NULL;
    if (n->id && !strcmp(n->id, id)) return (NbNode *)n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) return r;
    }
    return NULL;
}
static NbNode *find_tag_first(const NbNode *n, const char *tag) {
    if (!n || !tag) return NULL;
    if (n->tag && !strcasecmp(n->tag, tag)) return (NbNode *)n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = find_tag_first(c, tag);
        if (r) return r;
    }
    return NULL;
}
