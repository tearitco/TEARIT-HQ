/* nb_worker_sb_put_attrs_canonical.c - canonical attribute serialization helper (mechanical extract, single-TU include) */
static void sb_put_attrs_canonical(SB *b, const NbNode *n) {
    if (!n || !n->attrs || !n->attrs[0]) return;
    const char *a = n->attrs;
    size_t tl = n->tag ? strlen(n->tag) : 0;
    const char *p = NULL;
    int parser_form = 0;
    if (n->tag && !strcmp(a, n->tag)) {
        return;
    } else if (n->tag && !strncmp(a, n->tag, tl) && (a[tl] == ' ' || a[tl] == '\t')) {
        p = a + tl;
        parser_form = 1;
    } else {
        /* Normalized blob (setAttribute/attrs_set): already escaped markup. */
        sb_put(b, " ");
        sb_put(b, a);
        return;
    }
    /* Parser/load raw blob: parse and re-emit with escaped values so raw
     * '&', quotes in values, and source entity forms are serialized safely
     * without changing attribute semantics. */
    while (p && *p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        if (kl == 0) { p++; continue; }
        char kbuf[128]; size_t kc = kl < 127 ? kl : 127;
        memcpy(kbuf, ks, kc); kbuf[kc] = 0;
        /* skip tag token if it appears again unexpectedly */
        if (parser_form && n->tag && kl == tl && !strncasecmp(ks, n->tag, tl)) {
            while (*p && !isspace((unsigned char)*p)) p++;
            continue;
        }
        int had_eq = 0;
        char vraw[4096]; size_t vr = 0;
        if (*p == '=') {
            had_eq = 1;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p == '"' || *p == '\'') { qc = *p; p++; }
            const char *vs = p;
            if (qc) { while (*p && *p != qc) p++; }
            else    { while (*p && !isspace((unsigned char)*p) && *p != '>') p++; }
            vr = (size_t)(p - vs);
            if (vr >= sizeof(vraw)) vr = sizeof(vraw) - 1;
            memcpy(vraw, vs, vr); vraw[vr] = 0;
            if (qc && *p) p++;
        }
        sb_put(b, " ");
        sb_put(b, kbuf);
        if (had_eq) {
            sb_put(b, "=\"");
            /* Decode source entities to logical value, then escape for
             * serialization (avoids double-escaping already-escaped entities
             * while making raw '&' safe). */
            char dec[4096];
            size_t dl = nb_decode_entities(vraw, vr, dec);
            (void)dl;
            sb_put_attr_escaped(b, dec);
            sb_put(b, "\"");
        }
        while (*p && !isspace((unsigned char)*p)) p++;
    }
}
