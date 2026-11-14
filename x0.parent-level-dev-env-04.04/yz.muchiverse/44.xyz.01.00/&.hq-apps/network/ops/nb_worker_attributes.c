/* nb_worker_attributes.c - element.attributes getter helper (mechanical extract, single-TU include) */
/* element.attributes (2026-09-21): kevlar's web-component wrapper render
 * starts with `_.C2(this.attributes)` in both setUpProps and render. Without
 * this property el.attributes is undefined, _.l(undefined) fails as
 * "cannot read property of undefined @ <script>:5224:69" and every
 * sxs-materialized component (yt-searchbox etc.) bricks its createElement.
 * Browsers expose a live NamedNodeMap; a fresh array of {name,value}
 * records per access is array-like and iterable, which is what kevlar
 * consumes (setUpProps walks name/value pairs). */
static JSValue nb_el_attributes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (!n || !n->attrs) return arr;
    const char *p = n->attrs;
    int idx = 0;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        int had_eq = (*p == '=');
        if (*p == '=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p == '"' || *p == '\'') { qc = *p; p++; }
            while (*p && !(qc ? (*p == qc) : (isspace((unsigned char)*p) || *p == '>'))) p++;
            if (qc && *p) p++;
        }
        /* the parsed raw blob leads with the tag name token (no '='); drop it */
        if (kl > 0 && (had_eq || idx > 0)) {
            char kbuf[64]; size_t kc = kl < 63 ? kl : 63; memcpy(kbuf, ks, kc); kbuf[kc] = 0;
            for (size_t i = 0; i < kc; i++) kbuf[i] = (char)tolower((unsigned char)kbuf[i]);
            const char *v = nb_attr_get(n, kbuf);
            JSValue o = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, o, "name", JS_NewString(ctx, kbuf));
            JS_SetPropertyStr(ctx, o, "nodeName", JS_NewString(ctx, kbuf));
            JS_SetPropertyStr(ctx, o, "localName", JS_NewString(ctx, kbuf));
            JS_SetPropertyStr(ctx, o, "value", JS_NewString(ctx, v ? v : ""));
            JS_SetPropertyStr(ctx, o, "specified", JS_NewBool(ctx, 1));
            JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, o);
            idx++;
        }
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    nb_list_proto(ctx, arr, "NamedNodeMap");
    return arr;
}
