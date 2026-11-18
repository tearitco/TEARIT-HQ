/* nb_worker_el_offset_dim_decls.c - element offset/dimension forward declarations and notes (mechanical extract, single-TU include) */

/* offsetWidth/offsetHeight/clientWidth/clientHeight — magic 0/1 for the
 * offset pair, 2/3 for the client pair (client dims special-case the root
 * scrolling element, whose client box is the viewport, not a CSS width).
 * Hidden elements measure 0 (browser display:none semantics); visible
 * elements report the modeled CSS px size, else 0 (no layout engine). */
static double nb_client_dim(NbNode *n, int which);
static JSValue nb_el_scroll_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_dim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
