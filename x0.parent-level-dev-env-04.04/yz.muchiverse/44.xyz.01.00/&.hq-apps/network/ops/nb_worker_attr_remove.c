/* nb_worker_attr_remove.c - attrs_has/attrs_del/removeAttribute helpers (mechanical extract, single-TU include) */
static int attrs_has(const NbNode *n, const char *name) {
    if (!n || !n->attrs || !name) return 0;
    const char *p = n->attrs;
    size_t nl = strlen(name);
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        if (kl == nl && !strncasecmp(ks, name, nl)) return 1;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return 0;
}
static char *attrs_del(const NbNode *n, const char *name) {
    SB b = {0, 0, 0};
    const char *p = n->attrs ? n->attrs : "";
    size_t nl = strlen(name);
    int first = 1;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        char kbuf[64]; size_t kc = kl < 63 ? kl : 63; memcpy(kbuf, ks, kc); kbuf[kc] = 0;
        int is_target = kl == nl && !strncasecmp(ks, name, nl);
        char vtmp[1200]; int hasv = 0;
        const char *savep = p;
        if (*p == '=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p == '"' || *p == '\'') { qc = *p; p++; }
            const char *vs = p;
            while (*p && !(qc ? (*p == qc) : (isspace((unsigned char)*p) || *p == '>'))) p++;
            size_t vl = (size_t)(p - vs);
            if (qc && *p) p++;
            size_t vc = vl < 1199 ? vl : 1199; memcpy(vtmp, vs, vc); vtmp[vc] = 0;
            hasv = 1;
        }
        if (!is_target) {   /* keep the attribute (attrs doesn't reorder) */
            const char *after = hasv ? p : savep;
            size_t ll = (size_t)(after - ks);
            if (!first) sb_put(&b, " ");
            char tmp[8196]; size_t lc = ll < 8191 ? ll : 8191;
            memcpy(tmp, ks, lc); tmp[lc] = 0;
            sb_put(&b, tmp);
        }
        while (*p && !isspace((unsigned char)*p)) p++;
        first = 0;
    }
    return b.s ? b.s : strdup("");
}
static JSValue nb_el_removeAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name || !attrs_has(n, name)) { JS_FreeCString(ctx, nm); return JS_UNDEFINED; }
    const char *old0 = nb_attr_get(n, name);
    char *old_value = old0 ? strdup(old0) : NULL;
    if (!strcasecmp(name, "id")) { free(n->id); n->id = NULL; }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = NULL; }
    char *na = attrs_del(n, name);
    free(n->attrs);
    n->attrs = na;
    nb_mutation_notify_attribute(ctx, n, name, old_value);
    free(old_value);
    JS_FreeCString(ctx, nm);
    return JS_UNDEFINED;
}
