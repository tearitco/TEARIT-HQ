/* nb_worker_select_selection.c - select selection helpers (mechanical extract, single-TU include) */
/* HTML default selection: the first option carrying `selected`, else the
 * first option at all. */
static NbNode *sel_selected(JSContext *ctx, NbNode *sel) {
    NbNode *first = NULL;
    for (NbNode *c = sel->first_child; c; c = c->next_sibling) {
        if (!node_is_tag(c, "option")) continue;
        if (!first) first = c;
        if (opt_selected(ctx, c)) return c;
    }
    return first;
}
static void sel_select_index(JSContext *ctx, NbNode *sel, int want) {
    int i = 0;
    for (NbNode *c = sel->first_child; c; c = c->next_sibling) {
        if (!node_is_tag(c, "option")) continue;
        opt_set_selected(ctx, c, i == want);
        i++;
    }
}
