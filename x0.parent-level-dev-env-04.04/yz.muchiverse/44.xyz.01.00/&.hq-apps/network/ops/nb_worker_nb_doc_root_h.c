/* nb_worker_nb_doc_root_h.c - document root height helper (mechanical extract, single-TU include) */
static double nb_doc_root_h(NbNode *from) {
    if (!from) return 0.0;
    NbNode *top = from;
    while (top->parent) top = top->parent;
    double sum = 0.0;
    for (NbNode *c = top->first_child; c; c = c->next_sibling) sum += nb_content_h(c);
    return sum;
}
