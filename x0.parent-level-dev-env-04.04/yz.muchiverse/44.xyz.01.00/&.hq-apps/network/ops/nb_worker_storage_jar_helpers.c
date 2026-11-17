/* nb_worker_storage_jar_helpers.c - localStorage jar/path/encoding helpers (mechanical extract, single-TU include) */
static void ls_jar_init(void) {
    g_ls_path_set = 1;
    const char *env = getenv("NB_LOCALSTORAGE_FILE");
    if (env && env[0]) { snprintf(g_ls_path, sizeof(g_ls_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_ls_path, sizeof(g_ls_path), "%s/.config/nbjs/nb_localstorage.txt", home);
    else
        g_ls_path[0] = 0;   /* no writable location: reads null, writes no-op */
}

static int pct_is_safe(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
static void pct_encode(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 3 < olen; c++) {
        if (pct_is_safe(*c)) out[o++] = (char)*c;
        else { unsigned char uc = *c;
            static const char H[] = "0123456789ABCDEF";
            out[o++] = '%'; out[o++] = H[uc >> 4]; out[o++] = H[uc & 15];
        }
    }
    out[o] = 0;
}
static void pct_decode(char *s) {
    char *w = s;
    for (const char *r = s; *r;) {
        if (r[0] == '%' && r[1] && r[2]) {
            int hi, lo;
            char h1 = r[1], h2 = r[2];
            hi = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            lo = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) { *w++ = (char)((hi << 4) | lo); r += 3; continue; }
        }
        *w++ = *r++;
    }
    *w = 0;
}

static int st_load_file(const char *path, StEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *tab = strchr(p, '\t');
        if (tab) {
            *tab = 0;
            snprintf(ents[n].key, sizeof(ents[n].key), "%s", p);
            pct_decode(ents[n].key);
            snprintf(ents[n].value, sizeof(ents[n].value), "%s", tab + 1);
            pct_decode(ents[n].value);
            n++;
        }
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}
