/* nb_worker_node_root.c - Node root/ownership helpers (mechanical extract, single-TU include) */
#define NB_OWNER_PROP "__nb_owner"

/* Node global-document helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_global_document.c"
/* Node getRootNode helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_get_root_node.c"
/* Node.compareDocumentPosition helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_compare.c"
/* Node isConnected helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_is_connected.c"
static JSValue nb_node_ownerDocument(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    /* Document nodes have null ownerDocument. The live global document is
     * a host plain object with no NODEKEY, so get_this() returns NULL for
     * it; detect it explicitly. */
    if (!n) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        JS_FreeValue(ctx, gdoc);
        return is_live ? JS_NULL : JS_UNDEFINED;
    }
    if (n->tag && !strcmp(n->tag, "#document")) return JS_NULL;

    NbNode *root = nb_top_root(n);
    if (root && root->tag && !strcmp(root->tag, "#document")) {
        if (root == g_dom_root) return nb_global_document(ctx);
        return push_node(ctx, root); /* parsed document wrapper */
    }

    /* Detached subtree: honor owner recorded on this wrapper or on the
     * detached root wrapper (so imported/adopted clones propagate owner
     * through their subtree without per-node stamping). */
    JSValue owner = JS_GetPropertyStr(ctx, this_val, NB_OWNER_PROP);
    if (JS_IsObject(owner)) return owner;
    JS_FreeValue(ctx, owner);
    if (root && root != n) {
        JSValue rw = push_node(ctx, root);
        owner = JS_GetPropertyStr(ctx, rw, NB_OWNER_PROP);
        JS_FreeValue(ctx, rw);
        if (JS_IsObject(owner)) return owner;
        JS_FreeValue(ctx, owner);
    }

    /* Fallback: live global document is the safest default. */
    return nb_global_document(ctx);
}
/* Node owner setter helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_set_owner.c"
