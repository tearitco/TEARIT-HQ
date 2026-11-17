/* nb_worker_b64_helpers.c - base64 decode helpers (mechanical extract, single-TU include) */
static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static unsigned char *b64_decode(const char *in, size_t *out_len) {
    size_t len = strlen(in);
    size_t out_cap = (len * 3) / 4 + 4;
    unsigned char *out = (unsigned char *)malloc(out_cap);
    if (!out) return NULL;
    size_t o = 0; int v = 0, bits = -8;
    for (size_t i = 0; i < len; i++) {
        char c = in[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int d = b64_val(c);
        if (d < 0) continue;
        v = (v << 6) + d;
        bits += 6;
        if (bits >= 0) { out[o++] = (unsigned char)((v >> bits) & 0xFF); bits -= 8; }
    }
    *out_len = o;
    return out;
}
