/* nb_worker_node_text_content.c - node textContent helper (mechanical extract, single-TU include) */
static void node_text_content(const NbNode *n, SB *b) {
    if (!n) return;
    if (n->tag && !strcmp(n->tag, "#comment")) {
        sb_put(b, n->text);
        return;
    }
    sb_put(b, n->text);
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        if (c->tag && !strcmp(c->tag, "#comment")) continue;
        node_text_content(c, b);
    }
}
