/* nb_worker_selector_compound.c - basic selector compound matching helpers (mechanical extract, single-TU include) */
static int has_class(const NbNode *n, const char *tok) {
    if (!n->cls || !tok) return 0;
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) { if (!strcmp(c, tok)) return 1; c = strtok(NULL, " "); }
    return 0;
}
static int match_compound(const NbNode *n, const char *cmp) {
    char tag[64] = "", id[64] = "", clbuf[512] = "";
    const char *p = cmp;
    const char *ts = p;
    while (*ts && (isalnum((unsigned char)*ts) || *ts == '-' || *ts == '_' || *ts == ':')) ts++;
    size_t tg = (size_t)(ts - p);
    if (tg && tg < 64) { memcpy(tag, p, tg); tag[tg] = 0; p = ts; }
    while (*p == '#' || *p == '.') {
        if (*p == '#') {
            p++; const char *es = p;
            while (*es && (isalnum((unsigned char)*es) || *es == '-' || *es == '_' || *es == ':')) es++;
            size_t d = (size_t)(es - p);
            if (d < 64) { memcpy(id, p, d); id[d] = 0; }
            p = es;
        } else {
            p++; const char *es = p;
            while (*es && (isalnum((unsigned char)*es) || *es == '-' || *es == '_' || *es == ':')) es++;
            size_t d = (size_t)(es - p);
            if (strlen(clbuf) + d < 510) {
                if (clbuf[0]) strcat(clbuf, " ");
                memcpy(clbuf + strlen(clbuf), p, d);
                clbuf[strlen(clbuf) + d] = 0;
            }
            p = es;
        }
    }
    if (tag[0] && (!n->tag || strcasecmp(n->tag, tag))) return 0;
    if (id[0] && (!n->id || strcmp(n->id, id))) return 0;
    char *tok = strtok(clbuf, " ");
    while (tok) { if (!has_class(n, tok)) return 0; tok = strtok(NULL, " "); }
    /* attribute selectors: [attr], [attr=v], [attr*=v], [attr^=v],
     * [attr$=v], [attr~=v]. Real bundles depend on exact matching
     * (webcomponents-lite.js: script[src*="webcomponents-lite.js"]), and
     * unsupported trailing syntax must FAIL rather than match by tag —
     * otherwise querySelector returns a wrong (truthy) element and the
     * caller dereferences it. */
    while (*p == '[') {
        p++;
        const char *as = p;
        while (*as && *as != '=' && *as != ']' && *as != '~' &&
               *as != '^' && *as != '$' && *as != '*') as++;
        char attr[64]; size_t ac = (size_t)(as - p); if (ac > 63) ac = 63;
        memcpy(attr, p, ac); attr[ac] = 0;
        for (char *t = attr; *t; t++) *t = (char)tolower((unsigned char)*t);
        p = as;
        int op = 0;
        if (*p == '~' || *p == '^' || *p == '$' || *p == '*') {
            op = (*p == '~') ? 5 : (*p == '^') ? 3 : (*p == '$') ? 4 : 2;
            p++; if (*p == '=') p++;
        } else if (*p == '=') { op = 1; p++; }
        const char *av = nb_attr_get(n, attr);
        if (op == 0) {
            if (!av || !av[0]) return 0;
        } else {
            while (*p == ' ' || *p == '\t') p++;
            char q = 0;
            if (*p == '"' || *p == '\'') { q = *p; p++; }
            const char *vs = p;
            if (q) { while (*p && *p != q) p++; }
            else { while (*p && *p != ']') p++; }
            size_t vn = (size_t)(p - vs); char val[512];
            size_t vc = vn < 511 ? vn : 511; memcpy(val, vs, vc); val[vc] = 0;
            if (q && *p == q) p++;
            int ok = 0;
            switch (op) {
                case 1: ok = (strcmp(av, val) == 0); break;
                case 2: ok = (strstr(av, val) != NULL); break;
                case 3: ok = (strncmp(av, val, strlen(val)) == 0); break;
                case 4: { size_t al = strlen(av), vl = strlen(val);
                          ok = (al >= vl && strcmp(av + al - vl, val) == 0); } break;
                case 5: { const char *s = av; size_t vl = strlen(val);
                          while (*s) { while (*s == ' ' || *s == '\t') s++;
                              const char *ws = s;
                              while (*s && *s != ' ' && *s != '\t') s++;
                              if ((size_t)(s - ws) == vl && strncmp(ws, val, vl) == 0) { ok = 1; break; } } } break;
            }
            if (!ok) return 0;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ']') p++; else return 0;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (*p) return 0;   /* unsupported selector syntax -> no false match */
    return 1;
}
