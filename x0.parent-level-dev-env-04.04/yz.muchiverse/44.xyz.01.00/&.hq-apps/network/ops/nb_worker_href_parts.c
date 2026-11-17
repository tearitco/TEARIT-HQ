/* nb_worker_href_parts.c - href host/path helper (mechanical extract, single-TU include) */
static int href_parts(char *hostb, size_t hl, char *pathb, size_t pl) {
    const char *p = g_href;
    const char *a = strstr(p, "://");
    const char *s = a ? a + 3 : p;
    const char *q = s;
    while (*q) {
        if (*q == '/' || *q == '?' || *q == '#') break;
        q++;
    }
    size_t hn = (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    /* RFC 6265 §1: cookie scope is host-only — cut a ":port" suffix if the
     * colon slice between scheme and path is entirely digits. */
    for (char *c = hostb; *c; c++) if (*c == ':') { *c = 0; break; }
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *ph = q;
    while (*ph && *ph != '?' && *ph != '#') ph++;
    size_t pn = (size_t)(ph - q);
    if (pn >= pl) pn = pl - 1;
    memcpy(pathb, q, pn); pathb[pn] = 0;
    if (!pathb[0]) snprintf(pathb, pl, "/");
    return 1;
}
