/* nb_worker_img_src.c - image src storage helpers (mechanical extract, single-TU include) */
#define MAX_IMG_SRC 256
static struct { NbNode *n; char src[1024]; } g_img_src[256];
static int g_img_src_count = 0;
static void img_set_src(NbNode *n, const char *s) {
    for (int i = 0; i < g_img_src_count; i++) if (g_img_src[i].n == n) { snprintf(g_img_src[i].src, sizeof(g_img_src[i].src), "%s", s); return; }
    if (g_img_src_count < MAX_IMG_SRC) { g_img_src[g_img_src_count].n = n; snprintf(g_img_src[g_img_src_count].src, sizeof(g_img_src[g_img_src_count].src), "%s", s); g_img_src_count++; }
}
static const char *img_get_src(const NbNode *n) {
    for (int i = 0; i < g_img_src_count; i++) if (g_img_src[i].n == n) return g_img_src[i].src;
    return nb_attr_get(n, "src");
}
