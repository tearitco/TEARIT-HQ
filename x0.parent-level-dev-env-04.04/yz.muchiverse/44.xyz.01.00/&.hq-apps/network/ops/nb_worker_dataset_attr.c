/* nb_worker_dataset_attr.c - dataset key-to-attribute helper (mechanical extract, single-TU include) */
static int nb_ds_to_attr(const char *key, char *out, size_t olen) {
    if (!key || !*key || olen < 8) return 0;
    snprintf(out, olen, "data-");
    size_t p = 5, q = 0;
    for (; key[0] && p + 1 < olen; key++) {
        if (key[0] == '-') { if (key[1] && p + 1 < olen) { out[p++] = '-'; out[p++] = 'a'; } continue; }
        if (isupper((unsigned char)key[0])) {
            if (p + 2 >= olen) return 0;
            out[p++] = '-';
            out[p++] = (char)tolower((unsigned char)key[0]);
        } else out[p++] = key[0];
        q++;
    }
    if (!q) return 0;
    out[p] = 0;
    return 1;
}
