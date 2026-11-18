/* nb_worker_element_native_forward_decls.c - element native forward declarations (mechanical extract, single-TU include) */
/* ---- element natives (this = element object) ---- */
static int attrs_has(const NbNode *n, const char *name);
static JSValue nb_el_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_normalize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_remove_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_before_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_after_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_replaceWith_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static const NbNode *nb_top_root(const NbNode *n);
static JSValue nb_global_document(JSContext *ctx);
