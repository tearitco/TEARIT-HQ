/* nb_worker_ser_escape_helpers.c - HTML serializer tag/escape helpers (mechanical extract, single-TU include) */
static int nb_ser_void_tag(const char *t) {
    static const char *const V[] = {
        "area","base","br","col","embed","hr","img","input","link",
        "meta","param","source","track","wbr", NULL
    };
    for (int i = 0; t && V[i]; i++)
        if (!strcasecmp(t, V[i])) return 1;
    return 0;
}
static int nb_ser_raw_tag(const char *t) {
    static const char *const R[] = {
        "script","style","xmp","iframe","noembed","noframes","plaintext",
        "noscript", NULL
    };
    for (int i = 0; t && R[i]; i++)
        if (!strcasecmp(t, R[i])) return 1;
    return 0;
}
static void sb_put_escaped(SB *b, const char *s, int xml) {
    if (!s) return;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (!xml && p[0] == 0xC2 && p[1] == 0xA0) { sb_put(b, "&nbsp;"); p++; continue; }
        switch (*p) {
        case '&': sb_put(b, "&amp;"); break;
        case '<': sb_put(b, "&lt;"); break;
        case '>': sb_put(b, "&gt;"); break;
        default: { char one[2] = { (char)*p, 0 }; sb_put(b, one); }
        }
    }
}
