/* nb_worker_layout_xy.c - layout position helper (mechanical extract, single-TU include) */
static void layout_xy(NbNode *n, double *out_x, double *out_y) {
    if (!n || layout_hidden_anc(n)) { *out_x = 0; *out_y = 0; return; }
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    /* position:absolute or position:fixed handling - positioned elements
     * are taken out of normal flow and positioned via top/left. */
    if (st.has_position && (st.position_absolute || st.position_fixed)) {
        double x = st.has_left ? st.left : 0;
        double y = st.has_top ? st.top : 0;
        if (st.position_fixed) {
            /* fixed: relative to viewport (window), not parent. Viewport
             * origin is (0,0) at window top-left. g_viewport_px is width. */
            *out_x = x;
            *out_y = y;
            return;
        }
        /* absolute: relative to nearest positioned ancestor (one with
         * has_position). relative/absolute/fixed all create positioning
         * contexts. If none, use viewport. */
        NbNode *anc = n->parent;
        while (anc && anc->tag) {
            NbCssStyle ast;
            nb_css_resolve(g_css, anc, nb_attr_get(anc, "style"), &ast);
            if (ast.has_position) {
                layout_xy(anc, out_x, out_y);
                *out_x += x;
                *out_y += y;
                return;
            }
            anc = anc->parent;
        }
        /* no positioned ancestor: fall back to viewport origin */
        *out_x = x;
        *out_y = y;
        return;
    }

    /* normal flow layout - original code continues below */
    double x = 0, y = 0;
    if (n->parent && n->parent->tag) {
        double px, py;
        layout_xy(n->parent, &px, &py);
        y += py;
        x += px;   /* 6.2: the parent ORIGIN, both axes */
        /* Inside the parent's content box: its padding-top, then this
         * node's own margin-top. G2.1's "strictly increasing y per depth"
         * depends on exactly this - without it a first child sits at the
         * same y as its parent and no amount of walking produces depth. */
        y += nb_edge_pad(n->parent, 1);
        y += nb_edge_pad(n, 1);
    }
    /* 6.2/6.3 (2026-10-19) - THE HORIZONTAL AXIS. Before this, the walk
     * below accumulated heights unconditionally and finished `*out_x = 0`,
     * so a flex row's children all reported x=0 and only the renderer (once
     * it consumed display:flex) would have disagreed. That is the G2.2
     * re-open nb_flex_test.sh warns about.
     *
     * When the parent is a flex ROW, the main axis is horizontal: siblings
     * accumulate along x and every child shares the parent's content-box
     * top (NOT a stacked y). When it is not, behaviour is exactly as before
     * - heights accumulate, x stays at the parent origin - so no existing
     * page moves by a pixel. `flex-direction` defaults to ROW in CSS, so a
     * bare `display:flex` IS a row; only an explicit
     * `flex-direction:column` stacks. (Getting this backwards is how the
     * first cut kept the test red - see the note on parent_is_row below.)
     *
     * The child's own horizontal share is nb_box_w()'s job (6.3), not this
     * function's - this walk only places origins. */
    NbCssStyle pst;
    int parent_is_row = 0;
    if (n->parent && n->parent->tag) {
        nb_css_resolve(g_css, n->parent, nb_attr_get(n->parent, "style"), &pst);
        if (pst.has_display_flex && pst.display_flex == 2) {
            /* grid layout */
            int cols = pst.has_grid_template_columns && pst.grid_cols > 0 ? pst.grid_cols : 1;
            if (cols < 1) cols = 1;
            int idx = 0;
            for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
                if (layout_hidden_anc(s)) continue;
                idx++;
            }
            int col = idx % cols;
            double row_w = nb_box_w(n->parent);
            if (row_w <= 0) row_w = g_col_px(n->parent);
            double gap = 0;
            double col_w = row_w / (double)cols;
            if (cols > 1) col_w = (row_w - gap * (cols - 1)) / (double)cols;
            if (col_w < 0) col_w = row_w / (double)cols;
            x += col * (col_w + gap);
            int row_idx = idx / cols;
            y += row_idx * 30;
            *out_x = x;
            *out_y = y;
            return;
        }
        /* flex-direction DEFAULTS TO ROW, per CSS. Only an explicit
         * `flex-direction:column` turns the main axis vertical. */
        int dir_col = pst.has_flex_direction && !pst.flex_row;
        parent_is_row = pst.has_display_flex && pst.display_flex == 1 && !dir_col;
    }
    if (parent_is_row) {
        for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
            if (layout_hidden_anc(s)) continue;
            x += nb_box_w(s) + nb_edge_pad_h(s);
        }
        *out_x = x;
        *out_y = y;
        return;
    }
    for (NbNode *s = n->parent ? n->parent->first_child : NULL; s && s != n; s = s->next_sibling) {
        if (layout_hidden_anc(s)) continue;
        y += nb_outer_h(s);
    }
    *out_x = x; *out_y = y;
}
