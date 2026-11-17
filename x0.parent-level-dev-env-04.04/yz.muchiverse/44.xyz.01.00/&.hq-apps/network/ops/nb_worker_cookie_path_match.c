/* nb_worker_cookie_path_match.c - cookie path match helper (mechanical extract, single-TU include) */
static int cookie_path_match(const char *cp, const char *rp) {
    if (!cp || !rp) return 0;
    if (!strcmp(cp, rp)) return 1;
    size_t n = strlen(cp);
    if (n == 0) return 1;
    if (strncmp(rp, cp, n) != 0) return 0;
    if (cp[n - 1] == '/') return 1;
    return rp[n] == '/';
}
