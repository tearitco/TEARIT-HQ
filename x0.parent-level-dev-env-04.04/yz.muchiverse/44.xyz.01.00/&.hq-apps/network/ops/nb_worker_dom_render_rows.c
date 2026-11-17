/* nb_worker_dom_render_rows.c - DOM render rows serializer (mechanical extract, single-TU include) */
static void dom_render_rows(SB *b) {
    /* TITLE first, like the static extractor: from the <title> element if the
     * parser kept one, else document.title / the LOAD title (g_title). */
    NbNode *te = g_dom_root ? find_tag_first(g_dom_root, "title") : NULL;
    if (te) {
        SB t = {0, 0, 0};
        node_text_content(te, &t);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
    } else if (g_title[0]) {
        rw_row(b, "TITLE", g_title);
    }
    int titled = 1;                 /* <title> already handled above */
    dom_walk_render(g_dom_root, &titled, b, 0);
    /* Canvas display publish (2026-10-07): dirty backing stores become
     * PNGs + 5-field IMG rows; the manager's own MEDIA->sprite pipeline
     * (IMG without nb_sprites_ → sprite) tiles them with zero new
     * machinery. Clears dirty so quiet canvases don't re-emit. */
    for (int i = 0; i < NB_CANVAS_MAX; i++) {
        NbCanvas *cv = &g_canvases[i];
        if (!cv->used || !cv->dirty || !cv->px) continue;
        char path[128];
        snprintf(path, sizeof(path), "/tmp/nb_canvas_%d.png", i);
        if (!stbi_write_png(path, cv->w, cv->h, 4, cv->px, cv->w * 4)) continue;
        char row[512];
        snprintf(row, sizeof(row), "file://%s|%d|%d|%s|canvas", path, cv->w, cv->h, path);
        /* NOTE: rw_row() maps '|' to space (wire escaping for TEXT/LINK
         * rows), which would collapse this 5-field row - emit raw. */
        if (b->len < RENDER_MAX) {
            sb_put(b, "IMG|");
            sb_put(b, row);
            sb_put(b, "\n");
        }
        cv->dirty = 0;
    }
}
