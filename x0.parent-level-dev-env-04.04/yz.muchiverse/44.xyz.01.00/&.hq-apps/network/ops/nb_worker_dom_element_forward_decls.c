/* nb_worker_dom_element_forward_decls.c - DOM element/native forward declarations (mechanical extract, single-TU include) */

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_img_src_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_src_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_img_naturalWidth_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_naturalHeight_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_complete_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_image_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_fetch_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static int dispatch_event(JSContext *ctx, int kind, NbNode *node, JSValue ev, int bubbles);
static JSValue peval_budget_value(JSContext *ctx, const char *src, size_t src_n);
static const char *img_get_src(const NbNode *n);
static void img_set_src(NbNode *n, const char *s);
static int layout_hidden_anc(NbNode *n);
static void layout_xy(NbNode *n, double *out_x, double *out_y);
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* canvas 2D natives defined with the other DOM natives (getBoundingClientRect
 * already ships on every wrapper via the existing definition above) */
static JSValue nb_el_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_canvas_toDataURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

static JSValue get_global_attr(JSContext *ctx, const char *name);
