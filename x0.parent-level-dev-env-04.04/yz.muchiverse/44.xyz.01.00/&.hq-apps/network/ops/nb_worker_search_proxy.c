/* nb_worker_search_proxy.c - search-box Enter navigation proxy (mechanical extract, single-TU include) */
/* ---- search-box Enter -> /results (2026-09-21): kevlar's sport masthead
 * is a string-template component, so its closure `on:keydown`/`on:input`
 * attach never runs against the parsed <input name=search_query> and the
 * live input could never submit. Engine-side proxy (document-level keydown):
 * Enter on any input[name=search_query] navigates /results?search_query=
 * (the same URL kevlar's own searchEndpoint builds ~<script>:268939).
 * Document-level (EVT_DOC, node NULL) keeps it free of node bookkeeping —
 * no dangling refs across navigations, and g_evl is cleared per page. */
static JSValue nb_search_submit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    static const char hex[] = "0123456789ABCDEF";
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_UNDEFINED;
    JSValue ev = argv[0];
    JSValue kv = JS_GetPropertyStr(ctx, ev, "key");
    int is_enter = 0;
    if (JS_IsString(kv)) {
        const char *k = JS_ToCString(ctx, kv);
        is_enter = k && !strcmp(k, "Enter");
        JS_FreeCString(ctx, k);
    }
    JS_FreeValue(ctx, kv);
    if (!is_enter) return JS_UNDEFINED;
    JSValue tv = JS_GetPropertyStr(ctx, ev, "target");
    NbNode *n = JS_IsObject(tv) ? get_node(ctx, tv) : NULL;
    if (!n || !n->tag || strcmp(n->tag, "input")) { JS_FreeValue(ctx, tv); return JS_UNDEFINED; }
    const char *attr = nb_attr_get(n, "name");
    if (!attr || strcmp(attr, "search_query")) { JS_FreeValue(ctx, tv); return JS_UNDEFINED; }
    JSValue v = JS_GetPropertyStr(ctx, tv, "value");
    const char *val = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    char q[2048]; size_t qi = 0;
    if (val) {
        for (const unsigned char *p = (const unsigned char *)val; *p && qi < sizeof(q) - 16; p++) {
            unsigned char c = *p;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                q[qi++] = (char)c;
            } else if (c == ' ') {
                q[qi++] = '+';
            } else {
                q[qi++] = '%'; q[qi++] = hex[c >> 4]; q[qi++] = hex[c & 15];
            }
        }
        JS_FreeCString(ctx, val);
    }
    q[qi] = 0;
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, tv);
    char url[4096];
    snprintf(url, sizeof(url), "/results?search_query=%s", q);
    JSValue loc = get_global_attr(ctx, "location");
    if (JS_IsObject(loc)) JS_SetPropertyStr(ctx, loc, "href", JS_NewString(ctx, url));
    JS_FreeValue(ctx, loc);
    return JS_UNDEFINED;
}

static void nb_wire_search_proxy(JSContext *ctx) {
    if (g_search_wired) return;
    JSValue type = JS_NewString(ctx, "keydown");
    JSValue cb = JS_NewCFunction(ctx, nb_search_submit, "searchEnter", 1);
    evl_add(ctx, EVT_DOC, NULL, type, cb);
    JS_FreeValue(ctx, type);
    JS_FreeValue(ctx, cb);
    g_search_wired = 1;
}
