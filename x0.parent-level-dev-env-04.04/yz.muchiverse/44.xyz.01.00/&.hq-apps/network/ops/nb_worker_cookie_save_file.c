/* nb_worker_cookie_save_file.c - cookie file save helper (mechanical extract, single-TU include) */
static void cookie_save_file(const CookieEnt *ents, int n) {
    if (!g_cookie_path[0]) return;
    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", g_cookie_path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) { *slash = 0; if (slash != dirbuf) mkdir_p(dirbuf); }
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        sb_put(&b, ents[i].host); sb_put(&b, "\t");
        sb_put(&b, ents[i].path); sb_put(&b, "\t");
        sb_put(&b, ents[i].name); sb_put(&b, "\t");
        sb_put(&b, ents[i].value);
        char tail[64];
        snprintf(tail, sizeof(tail), "\t%ld\t%d\n",
                 (long)ents[i].expires, ents[i].secure ? 1 : 0);
        sb_put(&b, tail);
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_cookie_path);
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (b.s && b.len) fwrite(b.s, 1, b.len, f);
        fclose(f);
        rename(tmp, g_cookie_path);
    }
    free(b.s);
}
