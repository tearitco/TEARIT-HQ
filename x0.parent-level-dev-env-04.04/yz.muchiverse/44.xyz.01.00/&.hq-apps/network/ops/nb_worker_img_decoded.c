/* nb_worker_img_decoded.c - decoded image cache helpers (mechanical extract, single-TU include) */
#define MAX_IMG_DECODED 256
static struct { NbNode *n; int w, h; unsigned char *data; } g_img_decoded[256];
static int g_img_decoded_count = 0;
static void img_set_decoded(NbNode *n, int w, int h, unsigned char *data) {
    for (int i = 0; i < g_img_decoded_count; i++) if (g_img_decoded[i].n == n) {
        free(g_img_decoded[i].data); g_img_decoded[i].w = w; g_img_decoded[i].h = h; g_img_decoded[i].data = data; return;
    }
    if (g_img_decoded_count < MAX_IMG_DECODED) {
        g_img_decoded[g_img_decoded_count].n = n; g_img_decoded[g_img_decoded_count].w = w;
        g_img_decoded[g_img_decoded_count].h = h; g_img_decoded[g_img_decoded_count].data = data; g_img_decoded_count++;
    } else free(data);
}
static int img_get_decoded(const NbNode *n, int *w, int *h) {
    for (int i = 0; i < g_img_decoded_count; i++) if (g_img_decoded[i].n == n) {
        if (w) *w = g_img_decoded[i].w; if (h) *h = g_img_decoded[i].h; return g_img_decoded[i].data != NULL;
    }
    return 0;
}
