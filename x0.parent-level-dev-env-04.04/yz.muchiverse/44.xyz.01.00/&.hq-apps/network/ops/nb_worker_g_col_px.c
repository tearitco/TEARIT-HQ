/* nb_worker_g_col_px.c - content column width helper (mechanical extract, single-TU include) */
static double g_col_px(NbNode *from) {
    if (g_measuring) return g_viewport_px;
    if (g_gutter_state < 0) {
        if (g_viewport_h <= 0.0 || !from) {
            g_gutter_state = 0;              /* no measurement -> no guess */
        } else {
            g_measuring = 1;
            double doc_h = nb_doc_root_h(from);
            g_measuring = 0;
            g_gutter_state = (doc_h > g_viewport_h) ? 1 : 0;
        }
    }
    return g_gutter_state ? (g_viewport_px - NB_SCROLLBAR_W) : g_viewport_px;
}
