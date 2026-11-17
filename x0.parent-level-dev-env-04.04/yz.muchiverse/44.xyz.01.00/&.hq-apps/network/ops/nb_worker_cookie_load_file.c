/* nb_worker_cookie_load_file.c - cookie file load helper (mechanical extract, single-TU include) */
static int cookie_load_file(CookieEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(g_cookie_path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) { *nl = 0; }
        if (cookie_parse_line(p, &ents[n])) n++;
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}
