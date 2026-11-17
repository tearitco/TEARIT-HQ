/* nb_worker_script_src_helpers.c - script src storage and execution helpers (mechanical extract, single-TU include) */
/* ---- script src getter/setter for dynamic script loading (Cloudflare challenge) ---- */
#define MAX_SCRIPT_SRC 256
static struct { NbNode *n; char src[1024]; } g_script_src[256];
static int g_script_src_count = 0;

static void script_set_src(NbNode *n, const char *s) {
    for (int i = 0; i < g_script_src_count; i++) if (g_script_src[i].n == n) { snprintf(g_script_src[i].src, sizeof(g_script_src[i].src), "%s", s); return; }
    if (g_script_src_count < MAX_SCRIPT_SRC) { g_script_src[g_script_src_count].n = n; snprintf(g_script_src[g_script_src_count].src, sizeof(g_script_src[g_script_src_count].src), "%s", s); g_script_src_count++; }
}

static const char *script_get_src(const NbNode *n) {
    for (int i = 0; i < g_script_src_count; i++) if (g_script_src[i].n == n) return g_script_src[i].src;
    return nb_attr_get(n, "src");
}

/* Execute a script in the worker context. Called from script src setter. */
static void nb_execute_script_in_worker(JSContext *ctx, const char *script_url, const char *script_src) {
    if (!script_src || !script_src[0]) return;
    char errbuf[512];
    if (peval_budget(ctx, script_src, strlen(script_src), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "WERR| dynamic script %s: %s\n", script_url, errbuf[0] ? errbuf : "eval error");
    }
}
