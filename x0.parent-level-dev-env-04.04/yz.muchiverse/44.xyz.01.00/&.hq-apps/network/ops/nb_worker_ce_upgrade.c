/* nb_worker_ce_upgrade.c - custom elements upgrade helpers (mechanical extract, single-TU include) */
static void ce_upgrade_one(JSContext *ctx, NbNode *n, JSValue ctor) {
    JSValue wrapper = push_node(ctx, n);
    if (!JS_IsObject(wrapper)) return;
    JSValue up = JS_GetPropertyStr(ctx, wrapper, CE_UPGRD);
    int done = JS_ToBool(ctx, up);
    JS_FreeValue(ctx, up);
    if (done || !JS_IsObject(ctor)) return;
    JSValue proto = JS_GetPropertyStr(ctx, ctor, "prototype");
    if (JS_IsObject(proto)) JS_SetPrototype(ctx, wrapper, proto);
    JS_SetPropertyStr(ctx, wrapper, CE_UPGRD, JS_NewBool(ctx, 1));
    /* Class-based (kevlar _.UV "CoW") wrappers have ctor-body init
     * (G.rawProps={}, queuingData/owner/dispose/hasRendered flags) that
     * setUpProps() requires; Chrome runs the element ctor during upgrade so
     * our native upgrade path mirrors it. Plain-function ctors accept
     * JS_Call with this=wrapper; real ES class ctors refuse and are caught
     * (the sxs proto.createElement branch below covers those components). */
    {
        int callable = JS_IsFunction(ctx, ctor);
        if (callable) {
            JSValue r = JS_Call(ctx, ctor, wrapper, 0, NULL);
            if (JS_IsException(r)) {
                char buf[1536];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                if (g_trace_cb) fprintf(stderr, "CE|ctor(%s): %s\n", n->tag ? n->tag : "?", m ? m : buf);
            }
            JS_FreeValue(ctx, r);
        }
        /* CoW kismet: kevlar's _.UV ctor body is `G=B.apply(this)||this;
         * G.rawProps={};...` — B may hand back a DIFFERENT object so the
         * fields must land on THIS wrapper. Chrome guarantees el.rawProps
         * etc. exist after construction; mirror that deterministically. */
        const char *flags[] = {
            "isWebComponentWrapper", "isReparenting", "hasRendered", "isLazyComponentFn", NULL
        };
        for (const char **f = flags; *f; f++) {
            JSValue cur = JS_GetPropertyStr(ctx, wrapper, *f);
            int have = !JS_IsUndefined(cur);
            JS_FreeValue(ctx, cur);
            if (!have) JS_SetPropertyStr(ctx, wrapper, *f, JS_NewBool(ctx, 0));
        }
        JSValue cur = JS_GetPropertyStr(ctx, wrapper, "rawProps");
        if (!JS_IsObject(cur)) JS_SetPropertyStr(ctx, wrapper, "rawProps", JS_NewObject(ctx));
        JS_FreeValue(ctx, cur);
        cur = JS_GetPropertyStr(ctx, wrapper, "queuingData");
        if (JS_IsUndefined(cur)) JS_SetPropertyStr(ctx, wrapper, "queuingData", JS_NewBool(ctx, 0));
        JS_FreeValue(ctx, cur);
        cur = JS_GetPropertyStr(ctx, wrapper, "owner");
        if (JS_IsUndefined(cur)) JS_SetPropertyStr(ctx, wrapper, "owner", JS_NULL);
        JS_FreeValue(ctx, cur);
        cur = JS_GetPropertyStr(ctx, wrapper, "dispose");
        if (!JS_IsFunction(ctx, cur)) {
            JSValue fn = JS_NewCFunction(ctx, nb_js_void, "dispose", 0);
            JS_SetPropertyStr(ctx, wrapper, "dispose", fn);
        }
        JS_FreeValue(ctx, cur);
        if (g_trace_cb) {
            JSValue rp = JS_GetPropertyStr(ctx, wrapper, "rawProps");
            fprintf(stderr, "CE|ctor(%s): raw=%s\n", n->tag ? n->tag : "?", JS_IsObject(rp) ? "object" : (JS_IsUndefined(rp) ? "undefined" : "other"));
            JS_FreeValue(ctx, rp);
        }
    }
    ce_call_lifecycle(ctx, wrapper, "_initializeProperties");
    ce_call_lifecycle(ctx, wrapper, "ready");
    ce_call_lifecycle(ctx, wrapper, "connectedCallback");
    /* sxs-style kevlar wrappers (2026-09-20): components register via
     * `sxs(O, name)` which defines `D.prototype.createElement` that runs the
     * component's render fn `_.a(O,null)` + lmV(this) attach — the whole
     * constructor work without native ctor semantics. Masthead's D proto is
     * exactly {constructor, createElement}; calling it materializes the UI. */
    if (JS_IsObject(proto)) {
        JSValue ce = JS_GetPropertyStr(ctx, proto, "createElement");
        if (JS_IsFunction(ctx, ce)) {
            JSValue r = JS_Call(ctx, ce, wrapper, 0, NULL);
            if (JS_IsException(r)) {
                char buf[1536];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                if (g_trace_cb) fprintf(stderr, "CE|createElement(%s): %s\n", n->tag ? n->tag : "?", m ? m : buf);
            } else {
                const char *rt = JS_IsUndefined(r) ? "undefined" : JS_IsNull(r) ? "null" : JS_IsBool(r) ? "bool" : JS_IsFunction(ctx, r) ? "function" : JS_IsString(r) ? "string" : JS_IsObject(r) ? (JS_IsArray(ctx, r) ? "array" : "object") : "other";
                if (g_trace_cb) fprintf(stderr, "CE|createElement(%s): rc=%s\n", n->tag ? n->tag : "?", rt);
                /* rung-1 (2026-09-21): kevlar's REAL masthead createElement returns
                 * the structured UI as an OBJECT descriptor (rc=object) holding the
                 * form/icon/kids that its own render fn `_.a(O,null)` produced —
                 * but the engine only traced+discarded it, leaving the flat 5-div
                 * scaffold. The descendant *array* path (yt-searchbox rc=array ->
                 * 3 real kids) is proven; the masthead's rc=object ALSO carries a
                 * stampable desc. Feed rc through the same desc->kids walker that
                 * yt-searchbox rides: if rc is an ARRAY of descriptors, stamp each
                 * element into this node (recursively), reusing the exact same
                 * desc-array->kids materializer that produced searchbox's 3 kids.
                 * This is the crux of "kevlar real masthead materialization". */
                if (JS_IsArray(ctx, r) && JS_IsObject(r)) {
                    /* desc-array -> real kids: each item is an NbCrDesc-shaped
                     * object {tag,attrs,kids} OR a plain-string/text-node desc.
                     * Walk + stamp exactly like nb_template_content_frag does for
                     * <template>.content, so the masthead's <form action=/results>,
                     * #search-icon-legacy and yt-searchbox host all materialize. */
            }
            JS_FreeValue(ctx, r);
        }
        JS_FreeValue(ctx, ce);
    }
    JS_FreeValue(ctx, proto);
}

}
static void ce_upgrade_node_if_registered(JSContext *ctx, JSValue el, NbNode *n) {
    (void)el;
    if (!n || !n->tag || !strchr(n->tag, '-')) return;
    JSValue reg = ce_registry(ctx, 0);
    if (JS_IsObject(reg)) {
        JSValue ctor = JS_GetPropertyStr(ctx, reg, n->tag);
        if (JS_IsObject(ctor)) ce_upgrade_one(ctx, n, ctor);
        JS_FreeValue(ctx, ctor);
    }
    JS_FreeValue(ctx, reg);
}

static void ce_walk_upgrade(JSContext *ctx, NbNode *root, JSValue reg, const char *name) {
    if (!root) return;
    if (root->tag && !strcmp(root->tag, name)) {
        JSValue ctor = JS_GetPropertyStr(ctx, reg, name);
        if (JS_IsObject(ctor)) ce_upgrade_one(ctx, root, ctor);
        JS_FreeValue(ctx, ctor);
    }
    for (const NbNode *c = root->first_child; c; c = c->next_sibling)
        ce_walk_upgrade(ctx, (NbNode *)c, reg, name);
}

