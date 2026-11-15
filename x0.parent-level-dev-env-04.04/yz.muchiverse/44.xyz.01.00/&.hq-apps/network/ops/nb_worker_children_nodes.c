/* nb_worker_children_nodes.c - children/childNodes getters (mechanical extract, single-TU include) */
static JSValue nb_el_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
            if (!c->tag || !c->tag[0]) continue;   /* children is element-only (childNodes keeps text) */
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i++, push_node(ctx, (NbNode *)c));
        }
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    return arr;
}
static JSValue nb_el_childNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i++, push_node(ctx, (NbNode *)c));  /* text nodes incl. */
        }
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    return arr;
}
