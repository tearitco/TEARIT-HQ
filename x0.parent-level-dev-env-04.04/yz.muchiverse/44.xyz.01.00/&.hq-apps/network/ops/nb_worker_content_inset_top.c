/* nb_worker_content_inset_top.c - content inset helper (mechanical extract, single-TU include) */
#define NB_SBAR_ARROW_H 14
static char g_page_title[256];

static double g_content_inset_top(void) {
    double inset = g_gutter_state ? (double)NB_SBAR_ARROW_H : 0.0;
    if (g_page_title[0]) inset += (double)NB_ROW_PX;
    return inset;
}
