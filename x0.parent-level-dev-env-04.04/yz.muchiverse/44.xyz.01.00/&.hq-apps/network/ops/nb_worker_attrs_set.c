/* nb_worker_attrs_set.c - attribute set helper (mechanical extract, single-TU include) */
static char *attrs_set(const NbNode *n, const char *name, const char *val) {
    SB b = {0, 0, 0};
    const char *p = n->attrs ? n->attrs : "";
    size_t nl = strlen(name);
    int found = 0, first = 1;
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
        if (is_target) {
            found = 1;
            if (!first) sb_put(&b, " ");
            sb_put(&b, name); sb_put(&b, "=\"");
            sb_put_attr_escaped(&b, val ? val : "");
            sb_put(&b, "\"");
        } else {
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
    if (!found) {
        if (!first) sb_put(&b, " ");
        sb_put(&b, name); sb_put(&b, "=\"");
        sb_put_attr_escaped(&b, val ? val : "");
        sb_put(&b, "\"");
    }
    return b.s ? b.s : strdup("");
}
