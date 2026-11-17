/* nb_worker_nb_box_w.c - box width helper (mechanical extract, single-TU include) */
static double nb_box_w(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    if (st.has_width && st.width > 0) return st.width;

    /* 6.3 (2026-10-19) - a FLEX ROW's child takes a SHARE of the row, not
     * the whole content column. Without this every child of the test's
     * `#fx` would report w=389 (the full column), so three "columns" would
     * total 1167px inside a 389px span and clause 4 of nb_flex_test.sh
     * would fail - the widths, not the positions, are what make it a row.
     *
     * Equal share when no child declares flex-grow; otherwise weighted by
     * flex-grow, which is what the renderer's css_layout_pass() already
     * does (grow_total/fixed_total). The test's three children carry no
     * CSS beyond display:flex, so they take the equal-share path.
     *
     * Only block-level children are sized here; an inline child keeps its
     * shrink-to-fit behaviour below (a <span> in a flex row still hugs its
     * text), matching how the renderer treats inline content. */
    if (n->parent && n->parent->tag) {
        NbCssStyle pst;
        nb_css_resolve(g_css, n->parent, nb_attr_get(n->parent, "style"), &pst);
        if (pst.has_display_flex && pst.display_flex == 2) {
            /* simple grid: distribute across grid_cols */
            int cols = pst.has_grid_template_columns && pst.grid_cols > 0 ? pst.grid_cols : 1;
            if (cols < 1) cols = 1;
            double row_w = nb_box_w(n->parent);
            if (row_w > 0) {
                /* count visible children to place */
                int idx = 0;
                for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
                    if (layout_hidden_anc(s)) continue;
                    idx++;
                }
                int col = idx % cols;
                /* compute column width */
                double col_w = row_w / (double)cols;
                if (pst.has_grid_template_columns) {
                    /* rough equal split */
                    col_w = row_w / (double)cols;
                }
                return col_w;
            }
        }
        if (pst.has_display_flex && pst.display_flex == 1 &&
            !(pst.has_flex_direction && !pst.flex_row) &&  /* default is ROW */
            layout_is_block_tag(n->tag)) {
            double row_w = nb_box_w(n->parent);
            if (row_w > 0) {
                int nvis = 0;
                double grow_total = 0.0, grow_me = 0.0;
                for (NbNode *s = n->parent->first_child; s; s = s->next_sibling) {
                    if (layout_hidden_anc(s)) continue;
                    nvis++;
                    NbCssStyle sst;
                    nb_css_resolve(g_css, s, nb_attr_get(s, "style"), &sst);
                    if (sst.has_flex_grow && sst.flex_grow > 0) grow_total += sst.flex_grow;
                    if (s == n && sst.has_flex_grow && sst.flex_grow > 0) grow_me = sst.flex_grow;
                }
                if (nvis < 1) nvis = 1;
                if (grow_total > 0.0 && grow_me > 0.0)
                    return row_w * (grow_me / grow_total);
                if (grow_total <= 0.0)
                    return row_w / (double)nvis;   /* equal share */
                /* mixed: a grow child is handled above; a fixed child with
                 * no grow falls through to shrink-to-text/column below. */
            }
        }
    }

    if (!layout_is_block_tag(n->tag)) {
        /* shrink to text, clamped to the column so a long span still has
         * SOME width rather than none */
        double text_w = (n->text && n->text[0])
                        ? (double)strlen(n->text) * (double)NB_CHAR_PX : 0.0;
        double avail = nb_box_w(n->parent);
        if (avail <= 0) avail = g_col_px(n);
        return text_w < avail ? text_w : avail;
    }
    if (n->parent) {
        double avail = nb_box_w(n->parent);
        if (avail > 0) return avail;
    }
    return g_col_px(n);            /* the document's usable content column */
}
