/* nb_worker_sanitize_cookie_value.c - cookie value sanitize helper (mechanical extract, single-TU include) */
static void sanitize_cookie_value(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 1 < olen; c++) {
        if (*c < 0x20 || *c == 0x7f) continue;   /* drop CR/LF/controls (line-injection) */
        out[o++] = (char)*c;
    }
    out[o] = 0;
}
