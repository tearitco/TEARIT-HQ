/* nb_worker_insert_adjacent_html_helper.c - insertAdjacent HTML insertion helper (mechanical extract, single-TU include) */
static void nb_ia_insert_html(JSContext *ctx, NbNode *parent, NbNode *ref, const char *html) {
    NbNode *frag = nb_parse_html_nodes(html, strlen(html));
    if (!frag) return;
    NbNode *cur = frag->first_child;
    frag->first_child = frag->last_child = NULL;
    while (cur) {
        NbNode *nx = cur->next_sibling;
        cur->next_sibling = NULL;
        cur->parent = NULL;
        nb_ia_attach(ctx, parent, ref, cur);
        cur = nx;
    }
    nb_node_free(frag);
}
