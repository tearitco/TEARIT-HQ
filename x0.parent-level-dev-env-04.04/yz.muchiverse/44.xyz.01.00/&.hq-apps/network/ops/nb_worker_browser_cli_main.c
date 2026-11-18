/* nb_worker_browser_cli_main.c - browser CLI runner (mechanical extract, single-TU include) */
/* The released browser page runner: duk --browser page.js [fetch.dom]
 * runs the full DOM engine (tree, events+timer loop, fetch/XHR+Promise,
 * render-back) with rendered rows -> stdout. exit 0 ok / 1 js err / 2 usage.
 * Same code path the old `duk page.js` default took before --node/--browser. */
static int browser_cli_main(int argc, char **argv) {
    g_cli = 1;
    g_cli_log = 1;   /* bare console lines, no LOG| prefix */
    if (!g_out) { g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0); }
    const char *pg = argv[1];
    if (argc < 2 || strcmp(pg, "-h") == 0 || strcmp(pg, "--help") == 0) {
        fprintf(stderr, "usage: duk --browser <page.js> [fetch.dom]\n");
        return 2;
    }
    const char *base = strrchr(pg, '/');
    snprintf(g_title, sizeof(g_title), "%s", base ? base + 1 : pg);
    snprintf(g_href, sizeof(g_href), "file://%s", pg);
    snprintf(g_page_js, sizeof(g_page_js), "%s", pg);
    if (argc > 2) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", argv[2]);
    if (access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }
    run_page();
    return g_cli_status_ok ? 0 : 1;
}
