/* nb_worker_dom_cookie_get.c - document.cookie getter binding (mechanical extract, single-TU include) */
static JSValue nb_dom_cookie_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return JS_NewString(ctx, "");
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) return JS_NewString(ctx, "");
    for (char *c = host; *c; c++) *c = (char)tolower((unsigned char)*c);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;   /* expired */
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;          /* other host */
        if (!cookie_path_match(ents[i].path, rp)) continue;         /* other path */
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    JSValue r = JS_NewStringLen(ctx, b.s ? b.s : "", b.len);
    free(b.s);
    return r;
}
