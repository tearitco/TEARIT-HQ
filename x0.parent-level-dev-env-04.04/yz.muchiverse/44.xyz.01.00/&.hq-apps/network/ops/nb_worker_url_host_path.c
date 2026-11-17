/* nb_worker_url_host_path.c - URL host/path helper (mechanical extract, single-TU include) */
static int url_host_path(const char *url, char *hostb, size_t hl,
                         char *pathb, size_t pl) {
    hostb[0] = 0; pathb[0] = 0;
    const char *a = strstr(url, "://");
    if (!a) return 0;
    const char *s = a + 3;
    const char *q = s;
    while (*q && *q != '/' && *q != '?') q++;
    const char *col = NULL;
    for (const char *p = s; p < q; p++) if (*p == ':') { col = p; break; }
    size_t hn = col ? (size_t)(col - s) : (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *pp = (*q == '/') ? q : "/";
    snprintf(pathb, pl, "%s", pp);
    return 1;
}
