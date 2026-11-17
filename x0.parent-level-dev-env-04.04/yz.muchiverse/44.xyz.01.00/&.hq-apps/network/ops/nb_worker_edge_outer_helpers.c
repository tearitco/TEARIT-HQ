/* nb_worker_edge_outer_helpers.c - edge/outer box helpers (mechanical extract, single-TU include) */
static double nb_edge_pad(NbNode *n, int top) {
    if (!n) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    double p = top ? (st.has_pad_top ? st.pad_top : 0)
                   : (st.has_pad_bottom ? st.pad_bottom : 0);
    double m = top ? (st.has_mar_top ? st.mar_top : 0)
                   : (st.has_mar_bottom ? st.mar_bottom : 0);
    return p + m;
}

/* Horizontal counterpart of nb_edge_pad: left+right padding/margin, the
 * amount that displaces the NEXT sibling along a flex row's main axis.
 * 6.2/6.3 (2026-10-19). */
static double nb_edge_pad_h(NbNode *n) {
    if (!n) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    double p = (st.has_pad_top ? st.pad_top : 0); /* only a top value is modelled */
    double m = (st.has_mar_top ? st.mar_top : 0);
    return p + m;
}

/* Outer height of a node: its content plus its own vertical margin, which
 * is what actually displaces the NEXT sibling. Without this a paragraph
 * with margin-bottom 20px would be followed by the next one immediately,
 * which is the "stacked but cramped" look you get when margins are
 * dropped on the floor. */
static double nb_outer_h(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    return nb_content_h(n) + nb_edge_pad(n, 0) + nb_edge_pad(n, 1);
}
