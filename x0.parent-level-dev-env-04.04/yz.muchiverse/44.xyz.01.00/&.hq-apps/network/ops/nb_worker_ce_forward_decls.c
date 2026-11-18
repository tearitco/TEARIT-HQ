/* nb_worker_ce_forward_decls.c - customElements/ShadowRoot/template forward declarations (mechanical extract, single-TU include) */

/* Forward decls for the customElements / ShadowRoot / template-content
 * section (2026-09-20). Defined AFTER js_error_to_cstr so lifecycle hooks
 * reuse the g_trace_cb dump channel. */
static JSValue ce_define(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_when_def(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_upgrade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_attachShadow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static NbNode *nb_template_content_frag(JSContext *ctx, JSValue el, NbNode *n);
static JSValue make_fragment_wrapper(JSContext *ctx, NbNode *frag, const char *nodeName);
