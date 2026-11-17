/* nb_worker_layout_hidden_anc.c - layout hidden ancestor helper (mechanical extract, single-TU include) */
static int layout_hidden_anc(NbNode *n) {
    for (NbNode *a = n; a; a = a->parent) if (css_hidden(a)) return 1;
    return 0;
}
