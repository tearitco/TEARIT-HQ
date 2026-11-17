/* nb_worker_cookie_set_from_wire.c - cookie Set-Cookie ingest helper (mechanical extract, single-TU include) */
static void cookie_set_from_wire(const char *header_val, const char *request_url) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0] || !header_val || !*header_val) return;

    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", header_val);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) return;
    while (*tok == ' ') tok++;
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) return;
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) return;

    while ((tok = strtok(NULL, ";")) != NULL) {
        while (*tok == ' ') tok++;
        if (!strncasecmp(tok, "domain=", 7))
            snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "path=", 5))
            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "max-age=", 8)) maxage = atol(tok + 8);
        else if (!strncasecmp(tok, "expires=", 8))
            snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure")) secure = 1;
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            snprintf(scope_host, sizeof(scope_host), "%s", dh);
    }
    if (!scope_path[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            default_cookie_path(dp, scope_path, sizeof(scope_path));
        else
            snprintf(scope_path, sizeof(scope_path), "/");
    }

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;
        else if (exp <= nowt) delete = 1;
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
}
