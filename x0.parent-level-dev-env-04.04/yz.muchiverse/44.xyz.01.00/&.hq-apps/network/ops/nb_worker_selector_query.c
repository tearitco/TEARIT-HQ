/* nb_worker_selector_query.c - selector chain and query helpers (mechanical extract, single-TU include) */
static int match_chain(NbNode *n, char **parts, int idx) {
    if (!n) return 0;
    if (!match_compound(n, parts[idx])) return 0;
    if (idx == 0) return 1;
    return match_chain(n->parent, parts, idx - 1);
}
static int match_any_selector(NbNode *n, const char *sel) {
    if (!n || !sel) return 0;
    char copy[512]; size_t sl = strlen(sel); if (sl > 511) sl = 511;
    memcpy(copy, sel, sl); copy[sl] = 0;
    char *parts[16]; int np = 0;
    char *tok = strtok(copy, " \t");
    while (tok && np < 16) { parts[np++] = tok; tok = strtok(NULL, " \t"); }
    if (np == 0) return 0;
    return match_chain(n, parts, np - 1);
}
static NbNode *query_first(NbNode *n, const char *sel) {
    if (!n) return NULL;
    if (match_any_selector(n, sel)) return n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) return r;
    }
    return NULL;
}
