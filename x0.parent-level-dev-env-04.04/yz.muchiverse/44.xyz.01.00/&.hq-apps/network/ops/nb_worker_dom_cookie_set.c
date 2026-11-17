/* nb_worker_dom_cookie_set.c - document.cookie setter binding (mechanical extract, single-TU include) */
static JSValue nb_dom_cookie_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *spec_owned = NULL;
    const char *spec = argc > 0 ? (spec_owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!spec) return JS_UNDEFINED;   /* JS_ToCString already threw on coercion failure */
    if (!g_cookie_path_set) cookie_jar_init();
    if (!spec || !*spec || !g_cookie_path[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", spec);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    tok = trim_c(tok);
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    while ((tok = strtok(NULL, ";")) != NULL) {
        tok = trim_c(tok);
        if (!strncasecmp(tok, "path=", 5))            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "domain=", 7))     snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "max-age=", 8))    { maxage = atol(tok + 8); }
        else if (!strncasecmp(tok, "expires=", 8))    snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure"))          secure = 1;
        /* HttpOnly / SameSite / unknown attrs are accepted and ignored. */
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;              /* strip leading dots */
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) snprintf(scope_host, sizeof(scope_host), "%s", host);
    if (!scope_path[0]) default_cookie_path(rp, scope_path, sizeof(scope_path));

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;               /* max-age=0 -> remove */
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;            /* unparseable -> session cookie */
        else if (exp <= nowt) delete = 1;          /* expired date -> remove */
    }

    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcasecmp(ents[i].host, scope_host) != 0) continue;
        if (ents[i].path[0] && strcmp(ents[i].path, scope_path) != 0) continue;
        if (strcmp(ents[i].name, name) != 0) continue;
        found = i;
        break;
    }
    if (delete) {
        if (found >= 0) {
            for (int i = found; i + 1 < n; i++) ents[i] = ents[i + 1];
            n--;
        }
    } else {
        if (found >= 0) {
            snprintf(ents[found].host, sizeof(ents[found].host), "%s", scope_host);
            snprintf(ents[found].path, sizeof(ents[found].path), "%s", scope_path);
            snprintf(ents[found].value, sizeof(ents[found].value), "%s", value);
            ents[found].expires = exp;
            ents[found].secure = secure;
        } else if (n < COOKIE_MAX_ENT) {
            CookieEnt *e = &ents[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->host, sizeof(e->host), "%s", scope_host);
            snprintf(e->path, sizeof(e->path), "%s", scope_path);
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->value, sizeof(e->value), "%s", value);
            e->expires = exp;
            e->secure = secure;
        }
    }
    cookie_save_file(ents, n);
    JS_FreeCString(ctx, spec_owned);
    return JS_UNDEFINED;
}
