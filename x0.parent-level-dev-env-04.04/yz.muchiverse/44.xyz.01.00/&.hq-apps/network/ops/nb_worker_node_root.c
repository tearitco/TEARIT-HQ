/* nb_worker_node_root.c - Node root/ownership helpers (mechanical extract, single-TU include) */
#define NB_OWNER_PROP "__nb_owner"

static JSValue nb_global_document(JSContext *ctx) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, g, "document");
    JS_FreeValue(ctx, g);
    return doc; /* caller frees */
}
static JSValue nb_node_getRootNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        if (is_live) return gdoc; /* live document root is itself */
        JS_FreeValue(ctx, gdoc);
        return JS_UNDEFINED;
    }
    NbNode *root = (NbNode *)nb_top_root(n);
    if (!root) return JS_UNDEFINED;
    if (root->tag && !strcmp(root->tag, "#document")) {
        if (root == g_dom_root) return nb_global_document(ctx);
        return push_node(ctx, root);
    }
    return push_node(ctx, root);
}
/* Node.compareDocumentPosition helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_compare.c"
static JSValue nb_node_isConnected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        JS_FreeValue(ctx, gdoc);
        return JS_NewBool(ctx, is_live);
    }
    NbNode *root = (NbNode *)nb_top_root(n);
    return JS_NewBool(ctx, root && root->tag && !strcmp(root->tag, "#document"));
}
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
static void nb_set_owner_if_doc(JSContext *ctx, JSValue wrapper, JSValueConst creator) {
    if (!JS_IsObject(wrapper) || !JS_IsObject(creator)) return;
    JSValue gdoc = nb_global_document(ctx);
    int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(creator);
    JS_FreeValue(ctx, gdoc);
    if (is_live) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
        return;
    }
    NbNode *cn = get_this(ctx, creator);
    if (cn && cn->tag && !strcmp(cn->tag, "#document")) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
        return;
    }
    JSValue ct = JS_GetPropertyStr(ctx, creator, "contentType");
    int looks_doc = JS_IsString(ct);
    JS_FreeValue(ctx, ct);
    if (looks_doc) {
        JS_SetPropertyStr(ctx, wrapper, NB_OWNER_PROP, JS_DupValue(ctx, creator));
    }
}
