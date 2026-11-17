/* nb_worker_resolve_doc_url.c - document-relative URL resolver (mechanical extract, single-TU include) */
/* Resolve a fetch/XHR URL against the current document URL (g_href), the
 * same RFC 3986 §5-style merge the manager does for script srcs:
 *   already-schemed   -> as-is (file:, http:, https:, data:, ...)
 *   //host/[...]      -> scheme of g_href + the rest
 *   /abs/path         -> scheme://host of g_href + the rest
 *   rel/path          -> scheme://host + dirname(g_href's path) + the rest
 * g_href may itself be file:///... (file-based suites keep working). */
static void resolve_doc_url(const char *rel, char *out, size_t olen) {
    out[0] = 0;
    if (!rel || !rel[0]) return;
    const char *sc = strstr(g_href, "://");
    const char *rp = rel;
    while (*rp && *rp != ':' && *rp != '/' && *rp != '?' && *rp != '#') rp++;
    if (*rp == ':') { snprintf(out, olen, "%s", rel); return; }      /* has scheme */
    if (strncmp(rel, "//", 2) == 0 && sc) {
        snprintf(out, olen, "%.*s%s", (int)(sc - g_href + 3), g_href, rel);
        return;
    }
    const char *hp = sc ? sc + 3 : NULL;
    const char *hostslash = hp ? strchr(hp, '/') : NULL;
    if (rel[0] == '/') {
        if (sc && hostslash) snprintf(out, olen, "%.*s%s",
                                      (int)(hostslash - g_href), g_href, rel);
        else if (sc) snprintf(out, olen, "%s%s", g_href, rel);
        else snprintf(out, olen, "%s", rel);
        return;
    }
    if (sc && hp) {
        const char *dlast = strrchr(hp, '/');
        if (dlast && dlast > hp)
            snprintf(out, olen, "%.*s%s", (int)(dlast - g_href + 1), g_href, rel);
        else if (hostslash)
            snprintf(out, olen, "%.*s%s", (int)(hostslash - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s/%s", g_href, rel);
    } else {
        const char *lst = strrchr(g_href, '/');
        if (lst) snprintf(out, olen, "%.*s%s", (int)(lst - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s", rel);
    }
}
