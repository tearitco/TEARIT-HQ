/* nb_worker_nb_content_h.c - content height helper (mechanical extract, single-TU include) */
static double nb_content_h(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    if (st.has_height && st.height > 0) return st.height;
    /* 6.3 (2026-10-19) - a flex ROW's height is the MAX of its children's,
     * not their sum. Summing is correct for a stacked column (that is what
     * this did), but a row's children all occupy the SAME vertical band, so
     * summing would make the container three times too tall and push
     * everything after it down. This mirrors the renderer's flex path,
     * where the cross-axis size is the largest child, not the total. */
    int flex_row = st.has_display_flex && st.display_flex &&
                   !(st.has_flex_direction && !st.flex_row);  /* default is ROW */
    if (flex_row) {
        double mx = 0;
        for (NbNode *c = n->first_child; c; c = c->next_sibling) {
            double ch = nb_outer_h(c);
            if (ch > mx) mx = ch;
        }
        return mx;
    }
    double sum = 0;
    for (NbNode *c = n->first_child; c; c = c->next_sibling) sum += nb_content_h(c);
    if (sum > 0) return sum;
    if (!n->text || !n->text[0]) return 0;
    int len = (int)strlen(n->text);
    /* wrap width follows the USABLE column (pane minus the scrollbar gutter
     * when it scrolls), or the text wraps at a column the pane does not have
     * and every line count downstream is wrong. */
    double col = g_col_px(n);
    if (col <= 0) col = (double)NB_CONTENT_PX;
    int per_line = (int)(col / NB_CHAR_PX);
    if (per_line < 1) per_line = 1;
    int lines = (len + per_line - 1) / per_line;
    if (lines < 1) lines = 1;
    /* Each wrapped line projects as its OWN row, so height is rows x row
     * height - not lines x line height. */
    return (double)lines * NB_ROW_PX;
}
