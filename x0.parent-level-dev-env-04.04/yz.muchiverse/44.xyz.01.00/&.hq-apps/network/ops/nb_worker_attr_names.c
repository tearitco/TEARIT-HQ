/* nb_worker_attr_names.c - hasAttributes/getAttributeNames helpers (mechanical extract, single-TU include) */
static JSValue nb_el_hasAttributes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->attrs) return JS_FALSE;
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
        if (kl > 0 && (had_eq || idx > 0)) return JS_TRUE;
        idx++;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return JS_FALSE;
}
static JSValue nb_el_getAttributeNames(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (!n || !n->attrs) return arr;
    const char *p = n->attrs;
    int idx = 0, out = 0;
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
        if (kl > 0 && (had_eq || idx > 0)) {
            char kbuf[128]; size_t kc = kl < 127 ? kl : 127; memcpy(kbuf, ks, kc); kbuf[kc] = 0;
            for (size_t i = 0; i < kc; i++) kbuf[i] = (char)tolower((unsigned char)kbuf[i]);
            JS_SetPropertyUint32(ctx, arr, (uint32_t)out++, JS_NewString(ctx, kbuf));
        }
        idx++;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return arr;
}
