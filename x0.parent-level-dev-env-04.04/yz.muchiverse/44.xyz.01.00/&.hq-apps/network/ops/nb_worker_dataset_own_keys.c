/* nb_worker_dataset_own_keys.c - dataset ownKeys trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_own_keys(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = nb_ds_node(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (!n || !n->attrs) return arr;
    /* reuse nb_el_attributes parse for names, convert data-* keys */
    const char *p = n->attrs;
    int idx = 0;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        if (kl > 5 && !strncasecmp(ks, "data-", 5)) {
            /* convert data-x-y -> xY */
            char camel[128]; size_t ci = 0; int first = 1;
            for (size_t i = 5; i < kl && ci + 1 < sizeof(camel); i++) {
                if (ks[i] == '-' && i + 1 < kl) { camel[ci++] = (char)toupper((unsigned char)ks[i + 1]); i++; continue; }
                if (first) { camel[ci++] = ks[i]; first = 0; continue; }
                camel[ci++] = ks[i];
            }
            camel[ci] = 0;
            if (ci) JS_SetPropertyUint32(ctx, arr, (uint32_t)idx++, JS_NewString(ctx, camel));
        }
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, idx));
    return arr;
}
