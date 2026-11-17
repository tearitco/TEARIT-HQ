/* nb_worker_curl_cfg_helpers.c - curl config line helpers (mechanical extract, single-TU include) */
static void cfg_put(FILE *f, const char *val) {
    for (const char *p = val; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
}
static void cfg_line(FILE *f, const char *key, const char *val) {
    fputs(key, f); fputs(" = \"", f); cfg_put(f, val); fputs("\"\n", f);
}
static void cfg_data(FILE *f, const char *val) {
    fputs("data = \"", f);
    for (const char *p = val; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 && c != '\t') { fputc(' ', f); continue; }  /* curl config is single-line */
        if (c == '"') fputs("\\\"", f);
        else if (c == '\\') fputs("\\\\", f);
        else fputc(c, f);
    }
    fputs("\"\n", f);
}
