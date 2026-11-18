/* nb_worker_teardown.c - DOM/runtime teardown helpers (mechanical extract, single-TU include) */
static void dom_teardown(void) {
    /* free detached createElement() nodes, then the main tree */
    while (g_orphans) {
        NbNode *o = g_orphans;
        g_orphans = o->next_sibling;
        o->next_sibling = NULL;
        nb_node_free(o);
    }
    nb_node_free(g_dom_root);
    g_dom_root = NULL;
    nb_css_free(g_css);
    g_css = NULL;
}

/* devtools console EVAL: tear down everything a previous LOAD left live
 * (resident QuickJS runtime + its DOM tree). run_page() keeps those alive on
 * the success path so an eval:<js> snippet can run against the page; this
 * helper is the disciplined cleanup for the next LOAD, errors and QUIT. */
static void live_teardown(void) {
    nb_canvas_reset();   /* page-local raster buffers die with the page */
#ifdef NB_HAVE_GLES
    nb_gl_end_all();     /* EGL surfaces cannot outlive the page */
#endif
    if (g_live_ctx) {
        free_held_callbacks(g_live_ctx);   /* release refs before the heap dies */
        JS_FreeContext(g_live_ctx);
        g_live_ctx = NULL;
    }
    if (g_live_rt) {
        JS_FreeRuntime(g_live_rt);
        g_live_rt = NULL;
    }
    dom_teardown();
}
