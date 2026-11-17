/* nb_worker_cookie_header_for_url.c - cookie header builder (mechanical extract, single-TU include) */
static void cookie_header_for_url(const char *url, char *out, size_t olen) {
    out[0] = 0;
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return;
    char host[128], rp[512];
    if (!url_host_path(url, host, sizeof(host), rp, sizeof(rp))) return;
    int is_https = (strncmp(url, "https://", 8) == 0);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;
        if (ents[i].secure && !is_https) continue;
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;
        if (!cookie_path_match(ents[i].path, rp)) continue;
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    if (b.len && b.len + 10 < olen) {
        snprintf(out, olen, "Cookie: %.*s", (int)b.len, b.s);
    }
    free(b.s);
}
