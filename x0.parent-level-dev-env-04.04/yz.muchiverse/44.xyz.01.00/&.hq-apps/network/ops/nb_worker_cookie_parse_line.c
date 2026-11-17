/* nb_worker_cookie_parse_line.c - cookie line parse helper (mechanical extract, single-TU include) */
static int cookie_parse_line(char *line, CookieEnt *e) {
    memset(e, 0, sizeof(*e));
    char *f[6];
    int nf = 0;
    char *q = line;
    while (nf < 6 && *q) {
        f[nf] = q;
        char *t = strchr(q, '\t');
        if (t) { *t = 0; q = t + 1; }
        else { q += strlen(q); }
        nf++;
    }
    if (nf < 5) return 0;
    snprintf(e->host, sizeof(e->host), "%s", f[0]);
    snprintf(e->path, sizeof(e->path), "%s", f[1]);
    snprintf(e->name, sizeof(e->name), "%s", f[2]);
    snprintf(e->value, sizeof(e->value), "%s", f[3]);
    e->expires = (time_t)atol(f[4]);
    if (nf >= 6) e->secure = atoi(f[5]) ? 1 : 0;
    return 1;
}
