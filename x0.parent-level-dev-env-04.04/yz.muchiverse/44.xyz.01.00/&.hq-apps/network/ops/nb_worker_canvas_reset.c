/* nb_worker_canvas_reset.c - canvas state reset helper (mechanical extract, single-TU include) */
static void nb_canvas_reset(void) {
    for (int i = 0; i < NB_CANVAS_MAX; i++) {
        free(g_canvases[i].px);
        g_canvases[i].px = NULL;
        g_canvases[i].used = 0;
    }
}
