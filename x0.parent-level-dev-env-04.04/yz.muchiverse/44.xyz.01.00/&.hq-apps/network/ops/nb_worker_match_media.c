/* nb_worker_match_media.c - matchMedia helpers (mechanical extract, single-TU include) */
static void nb_mq_viewport(double *w, double *h) {
    *w = g_viewport_px > 0.0 ? g_viewport_px : (double)NB_CONTENT_PX;
    *h = g_viewport_h > 0.0 ? g_viewport_h : 600.0;
}
static char *nb_mq_trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}
static int nb_mq_word_at(const char *s, const char *word) {
    while (*word) {
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*word)) return 0;
        s++; word++;
    }
    return !*s || isspace((unsigned char)*s) || *s == '(';
}
static int nb_mq_length_px(const char *s, double *out) {
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    double vw, vh;
    nb_mq_viewport(&vw, &vh);
    if (!*end || !strcasecmp(end, "px")) *out = v;
    else if (!strcasecmp(end, "em") || !strcasecmp(end, "rem")) *out = v * 16.0;
    else if (!strcasecmp(end, "vw")) *out = v * vw / 100.0;
    else if (!strcasecmp(end, "vh")) *out = v * vh / 100.0;
    else if (!strcasecmp(end, "vmin")) *out = v * (vw < vh ? vw : vh) / 100.0;
    else if (!strcasecmp(end, "vmax")) *out = v * (vw > vh ? vw : vh) / 100.0;
    else return 0;
    return 1;
}
static int nb_mq_resolution_dppx(const char *s, double *out) {
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (!*end || !strcasecmp(end, "dppx") || !strcasecmp(end, "x")) *out = v;
    else if (!strcasecmp(end, "dpi")) *out = v / 96.0;
    else if (!strcasecmp(end, "dpcm")) *out = v / 37.7952755906;
    else return 0;
    return 1;
}
static int nb_mq_ratio(const char *s, double *out) {
    char *end = NULL;
    double a = strtod(s, &end);
    if (end == s) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end == '/') {
        char *e2 = NULL;
        double b = strtod(end + 1, &e2);
        if (e2 == end + 1 || b == 0.0) return 0;
        *out = a / b;
    } else {
        *out = a;
    }
    return 1;
}
static int nb_mq_cmp(double actual, double want, const char *op) {
    if (!strcmp(op, "<")) return actual < want;
    if (!strcmp(op, "<=")) return actual <= want;
    if (!strcmp(op, ">")) return actual > want;
    if (!strcmp(op, ">=")) return actual >= want;
    return actual == want;
}
static int nb_mq_number(const char *s, double *out) {
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    *out = v;
    return 1;
}
static int nb_mq_len_match(const char *name, int has_val, const char *val, double actual) {
    if (!has_val) return actual > 0.0;
    double want;
    if (!nb_mq_length_px(val, &want)) return 0;
    if (strstr(name, "min-")) return actual >= want;
    if (strstr(name, "max-")) return actual <= want;
    return actual == want;
}
static int nb_mq_feature(const char *raw, double w, double h) {
    char buf[192];
    size_t rl = strlen(raw);
    if (rl >= sizeof(buf)) return 0;
    memcpy(buf, raw, rl + 1);
    char *s = nb_mq_trim(buf);
    if (!*s) return 0;

    /* Media Queries level 4 range form: (width >= 600px). Only the
     * feature-op-value direction is parsed; the reversed form stays
     * unknown rather than being guessed. */
    if (!strchr(s, ':')) {
        char *ge = strstr(s, ">="), *le = strstr(s, "<=");
        char *gt = ge ? NULL : strchr(s, '>');
        char *lt = le ? NULL : strchr(s, '<');
        char *op = ge ? ge : (le ? le : (gt ? gt : lt));
        if (op) {
            const char *ops = (op == ge) ? ">=" : (op == le) ? "<=" : (op == gt) ? ">" : "<";
            size_t oplen = strlen(ops);
            *op = '\0';
            char *nm = nb_mq_trim(s);
            char *rhs = nb_mq_trim(op + oplen);
            for (char *p = nm; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (!*rhs) return 0;
            if (!strcmp(nm, "width") || !strcmp(nm, "height")) {
                double actual = !strcmp(nm, "height") ? h : w;
                double want;
                if (!nb_mq_length_px(rhs, &want)) return 0;
                return nb_mq_cmp(actual, want, ops);
            }
            return 0;
        }
    }

    char name[64], val[128];
    name[0] = val[0] = '\0';
    char *colon = strchr(s, ':');
    int has_val = colon != NULL;
    if (colon) {
        *colon = '\0';
        snprintf(name, sizeof(name), "%s", nb_mq_trim(s));
        snprintf(val, sizeof(val), "%s", nb_mq_trim(colon + 1));
    } else {
        snprintf(name, sizeof(name), "%s", s);
    }
    for (char *p = name; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (char *p = val; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (!strcmp(name, "width") || !strcmp(name, "min-width") || !strcmp(name, "max-width") ||
        !strcmp(name, "device-width") || !strcmp(name, "min-device-width") || !strcmp(name, "max-device-width"))
        return nb_mq_len_match(name, has_val, val, w);
    if (!strcmp(name, "height") || !strcmp(name, "min-height") || !strcmp(name, "max-height") ||
        !strcmp(name, "device-height") || !strcmp(name, "min-device-height") || !strcmp(name, "max-device-height"))
        return nb_mq_len_match(name, has_val, val, h);
    if (!strcmp(name, "aspect-ratio") || !strcmp(name, "min-aspect-ratio") || !strcmp(name, "max-aspect-ratio") ||
        !strcmp(name, "device-aspect-ratio") || !strcmp(name, "min-device-aspect-ratio") ||
        !strcmp(name, "max-device-aspect-ratio")) {
        double actual = (h > 0.0) ? w / h : 0.0;
        if (!has_val) return actual > 0.0;
        double want;
        if (!nb_mq_ratio(val, &want)) return 0;
        if (strstr(name, "min-")) return actual >= want;
        if (strstr(name, "max-")) return actual <= want;
        return actual == want;
    }
    if (!strcmp(name, "resolution") || !strcmp(name, "min-resolution") || !strcmp(name, "max-resolution") ||
        strstr(name, "device-pixel-ratio")) {
        double actual = 1.0;   /* CSS reference pixel */
        if (!has_val) return actual > 0.0;
        double want;
        if (!nb_mq_resolution_dppx(val, &want)) return 0;
        if (strstr(name, "min-")) return actual >= want;
        if (strstr(name, "max-")) return actual <= want;
        return actual == want;
    }
    if (!strcmp(name, "orientation")) {
        if (!has_val) return 0;
        return !strcmp(val, w >= h ? "landscape" : "portrait");
    }
    if (!strcmp(name, "prefers-color-scheme")) {
        if (!has_val) return 0;
        return !strcmp(val, "light");
    }
    if (!strcmp(name, "prefers-reduced-motion")) {
        if (!has_val) return 1;
        return !strcmp(val, "no-preference");
    }
    if (!strcmp(name, "prefers-contrast")) {
        if (!has_val) return 0;
        return !strcmp(val, "no-preference");
    }
    if (!strcmp(name, "forced-colors")) {
        if (!has_val) return 0;
        return !strcmp(val, "none");
    }
    if (!strcmp(name, "hover") || !strcmp(name, "any-hover")) {
        if (!has_val) return 1;
        return !strcmp(val, "hover");
    }
    if (!strcmp(name, "pointer") || !strcmp(name, "any-pointer")) {
        if (!has_val) return 1;
        return !strcmp(val, "fine");
    }
    if (!strcmp(name, "display-mode")) {
        if (!has_val) return 0;
        return !strcmp(val, "browser");
    }
    if (!strcmp(name, "scripting")) {
        if (!has_val) return 1;
        return !strcmp(val, "enabled");
    }
    if (!strcmp(name, "update")) {
        if (!has_val) return 1;
        return !strcmp(val, "fast");
    }
    if (!strcmp(name, "color-gamut")) {
        if (!has_val) return 0;
        return !strcmp(val, "srgb");
    }
    if (!strcmp(name, "color") || !strcmp(name, "min-color") || !strcmp(name, "max-color") ||
        !strcmp(name, "color-index") || !strcmp(name, "min-color-index") || !strcmp(name, "max-color-index") ||
        !strcmp(name, "monochrome") || !strcmp(name, "min-monochrome") || !strcmp(name, "max-monochrome")) {
        int is_index = strstr(name, "color-index") != NULL;
        int is_mono = strstr(name, "monochrome") != NULL;
        double actual = (is_index || is_mono) ? 0.0 : 8.0;   /* 8-bit sRGB; no indexed/mono grid */
        if (!has_val) return actual > 0.0;
        double want;
        if (!nb_mq_number(val, &want)) return 0;
        if (strstr(name, "min-")) return actual >= want;
        if (strstr(name, "max-")) return actual <= want;
        return actual == want;
    }
    if (!strcmp(name, "grid")) {
        if (!has_val) return 0;
        return !strcmp(val, "0");
    }
    if (!strcmp(name, "scan"))
        return has_val && !strcmp(val, "progressive");
    if (!strcmp(name, "overflow-block") || !strcmp(name, "overflow-inline"))
        return has_val && !strcmp(val, "scroll");
    return 0;   /* an unclaimed feature is never a match */
}
static char *nb_mq_find_and(char *s) {
    for (char *p = s; *p; p++) {
        if ((p == s || isspace((unsigned char)p[-1])) && nb_mq_word_at(p, "and")) {
            char *rest = nb_mq_trim(p + 3);
            if (!*rest) return NULL;
            char *end = p;
            while (end > s && isspace((unsigned char)end[-1])) end--;
            *end = '\0';
            return rest;
        }
    }
    return NULL;
}
static int nb_mq_part(char *s, double w, double h) {
    s = nb_mq_trim(s);
    if (!*s) return 1;
    if (*s == '(') {
        char *e = s + strlen(s) - 1;
        while (e > s && isspace((unsigned char)*e)) e--;
        if (*e != ')') return 0;
        *e = '\0';
        return nb_mq_feature(s + 1, w, h);
    }
    if (!strcasecmp(s, "all") || !strcasecmp(s, "screen")) return 1;
    return 0;   /* print / speech / unknown media type */
}
static int nb_mq_eval_one(char *s, double w, double h) {
    s = nb_mq_trim(s);
    if (!*s) return 1;
    int negate = 0;
    if (nb_mq_word_at(s, "not")) {
        negate = 1;
        s = nb_mq_trim(s + 3);
    } else if (nb_mq_word_at(s, "only")) {
        s = nb_mq_trim(s + 4);
    }
    int result = 1;
    while (s && *s) {
        char *next = nb_mq_find_and(s);
        if (!nb_mq_part(s, w, h)) result = 0;
        s = next;
    }
    return negate ? !result : result;
}
static int nb_mq_eval(const char *query, double w, double h) {
    char buf[512];
    size_t ql = strlen(query);
    if (ql >= sizeof(buf)) return 0;
    memcpy(buf, query, ql + 1);
    int matched = 0;
    char *p = buf;
    for (;;) {
        char *comma = strchr(p, ',');
        if (comma) *comma = '\0';
        if (nb_mq_eval_one(p, w, h)) matched = 1;
        if (!comma) break;
        p = comma + 1;
    }
    return matched;
}
static JSValue nb_mq_get_query(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return JS_GetPropertyStr(ctx, this_val, "__nb_mq_query");
}
static JSValue nb_mq_get_matches(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue qv = JS_GetPropertyStr(ctx, this_val, "__nb_mq_query");
    const char *q = JS_ToCString(ctx, qv);
    double w, h;
    nb_mq_viewport(&w, &h);
    int matched = q ? nb_mq_eval(q, w, h) : 0;
    JS_FreeCString(ctx, q);
    JS_FreeValue(ctx, qv);
    return JS_NewBool(ctx, matched);
}
static JSValue nb_mq_listeners(JSContext *ctx, JSValueConst this_val) {
    JSValue a = JS_GetPropertyStr(ctx, this_val, "__nb_mq_listeners");
    if (!JS_IsArray(ctx, a)) {
        JS_FreeValue(ctx, a);
        a = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, this_val, "__nb_mq_listeners", JS_DupValue(ctx, a));
    }
    return a;
}
static void nb_mq_listen_add(JSContext *ctx, JSValueConst this_val, JSValueConst fn) {
    if (!JS_IsFunction(ctx, fn)) return;
    JSValue a = nb_mq_listeners(ctx, this_val);
    JSValue lv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    for (uint32_t i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, a, i);
        int same = JS_StrictEq(ctx, e, fn);
        JS_FreeValue(ctx, e);
        if (same) { JS_FreeValue(ctx, a); return; }
    }
    JS_SetPropertyUint32(ctx, a, n, JS_DupValue(ctx, fn));
    JS_FreeValue(ctx, a);
}
static void nb_mq_listen_remove(JSContext *ctx, JSValueConst this_val, JSValueConst fn) {
    JSValue a = nb_mq_listeners(ctx, this_val);
    JSValue lv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    JSValue kept = JS_NewArray(ctx);
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, a, i);
        int same = JS_StrictEq(ctx, e, fn);
        if (!same) JS_SetPropertyUint32(ctx, kept, k++, e);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, a);
    JS_SetPropertyStr(ctx, this_val, "__nb_mq_listeners", kept);
}
static JSValue nb_mq_dispatch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_NewBool(ctx, 0);
    JSValue tv = JS_GetPropertyStr(ctx, argv[0], "type");
    const char *type = JS_ToCString(ctx, tv);
    int is_change = type && !strcmp(type, "change");
    JS_FreeCString(ctx, type);
    JS_FreeValue(ctx, tv);
    if (!is_change) return JS_NewBool(ctx, 1);
    JSValue a = nb_mq_listeners(ctx, this_val);
    JSValue lv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lv);
    JS_FreeValue(ctx, lv);
    for (uint32_t i = 0; i < n; i++) {
        JSValue fn = JS_GetPropertyUint32(ctx, a, i);
        if (JS_IsFunction(ctx, fn)) {
            JSValue ev = JS_DupValue(ctx, argv[0]);
            JSValue av[1] = { ev };
            JSValue r = JS_Call(ctx, fn, this_val, 1, av);
            if (JS_IsException(r)) { JSValue e = JS_GetException(ctx); JS_FreeValue(ctx, e); }
            JS_FreeValue(ctx, r);
            JS_FreeValue(ctx, ev);
        }
        JS_FreeValue(ctx, fn);
    }
    JS_FreeValue(ctx, a);
    JSValue on = JS_GetPropertyStr(ctx, this_val, "onchange");
    if (JS_IsFunction(ctx, on)) {
        JSValue ev = JS_DupValue(ctx, argv[0]);
        JSValue av[1] = { ev };
        JSValue r = JS_Call(ctx, on, this_val, 1, av);
        if (JS_IsException(r)) { JSValue e = JS_GetException(ctx); JS_FreeValue(ctx, e); }
        JS_FreeValue(ctx, r);
        JS_FreeValue(ctx, ev);
    }
    JS_FreeValue(ctx, on);
    return JS_NewBool(ctx, 1);
}
static JSValue nb_mq_add_listener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc > 0) nb_mq_listen_add(ctx, this_val, argv[0]);
    return JS_UNDEFINED;
}
static JSValue nb_mq_remove_listener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc > 0) nb_mq_listen_remove(ctx, this_val, argv[0]);
    return JS_UNDEFINED;
}
static JSValue nb_mq_add_event_listener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0])) return JS_UNDEFINED;
    const char *type = JS_ToCString(ctx, argv[0]);
    if (type && !strcmp(type, "change")) nb_mq_listen_add(ctx, this_val, argv[1]);
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}
static JSValue nb_mq_remove_event_listener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || !JS_IsString(argv[0])) return JS_UNDEFINED;
    const char *type = JS_ToCString(ctx, argv[0]);
    if (type && !strcmp(type, "change")) nb_mq_listen_remove(ctx, this_val, argv[1]);
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}
static JSValue nb_match_media(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    char *q = (argc > 0 && JS_IsString(argv[0])) ? JS_ToCString(ctx, argv[0]) : NULL;
    JSValue mql = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, mql, "__nb_mq_query", JS_NewString(ctx, q ? q : ""));
    JS_SetPropertyStr(ctx, mql, "__nb_mq_listeners", JS_NewArray(ctx));
    JS_SetPropertyStr(ctx, mql, "addListener", JS_NewCFunction(ctx, nb_mq_add_listener, "addListener", 1));
    JS_SetPropertyStr(ctx, mql, "removeListener", JS_NewCFunction(ctx, nb_mq_remove_listener, "removeListener", 1));
    JS_SetPropertyStr(ctx, mql, "addEventListener", JS_NewCFunction(ctx, nb_mq_add_event_listener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, mql, "removeEventListener", JS_NewCFunction(ctx, nb_mq_remove_event_listener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, mql, "dispatchEvent", JS_NewCFunction(ctx, nb_mq_dispatch, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, mql, "onchange", JS_NULL);
    {
        JSAtom nm = JS_NewAtom(ctx, "media");
        JS_DefinePropertyGetSet(ctx, mql, nm, JS_NewCFunction(ctx, nb_mq_get_query, "get media", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "matches");
        JS_DefinePropertyGetSet(ctx, mql, nm, JS_NewCFunction(ctx, nb_mq_get_matches, "get matches", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_FreeCString(ctx, q);
    return mql;
}

