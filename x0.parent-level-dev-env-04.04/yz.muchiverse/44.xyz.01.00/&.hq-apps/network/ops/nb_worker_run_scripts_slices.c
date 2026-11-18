/* nb_worker_run_scripts_slices.c - document-order script slice executor (mechanical extract, single-TU include) */
static void run_scripts_slices(JSContext *ctx, char *src, size_t src_n) {
    const char *p = src, *end = src + src_n;
    const char *after = NULL;
    char errbuf[512];
    char *probe = NULL; size_t probe_n = 0;
    if (nb_bundle_probe(src, src_n, &probe, &probe_n) && probe) {
        src = probe; src_n = probe_n;
        p = probe; end = probe + probe_n;
    }
    if (!find_script_boundary(p, end, &after)) {
        /* legacy single-program page.js */
        if (peval_budget(ctx, src, src_n, errbuf, sizeof(errbuf)) != 0)
            fprintf(stderr, "WERR| script 0: %s\n", errbuf[0] ? errbuf : "eval error");
        free(probe);
        return;
    }
    p = after;
    int idx = 0;
    for (;;) {
        const char *next = NULL;
        size_t slice = (size_t)(end - p);
        const char *bn = find_script_boundary(p, end, &next);
        if (bn) slice = (size_t)(bn - p);
        if (slice > 0) {
            /* QuickJS's lexer peeks input[input_len] for EOI, so each slice
             * must be NUL-terminated at its eval length. Slices own one
             * contiguous buffer, so terminate in place; the boundary's first
             * byte is never read again (next = the position after it). */
            ((char *)p)[slice] = 0;
            /* Check for module marker at slice start (skip leading whitespace). */
            int is_module = 0;
            const char *p2 = p;
            size_t slice2 = slice;
            while (slice2 > 0 && (*p2 == '\n' || *p2 == '\r' || *p2 == ' ' || *p2 == '\t')) {
                p2++; slice2--;
            }
            if (slice2 >= 15 && memcmp(p2, "/*nbjs-module*/", 15) == 0) {
                is_module = 1;
                /* Advance p past the marker and optional whitespace. */
                const char *marker_end = p2 + 15;
                while (marker_end < p + slice && (*marker_end == '\n' || *marker_end == '\r' || *marker_end == ' ' || *marker_end == '\t'))
                    marker_end++;
                size_t skip = (size_t)(marker_end - p);
                p = marker_end;
                slice = (slice > skip) ? (slice - skip) : 0;
            }
            if (slice > 0) {
                fprintf(stderr, "DEBUG: executing script slice %d, size: %zu, first 100 chars: %.100s\n", idx, slice, p);
                if (is_module) {
                    if (peval_budget_module(ctx, p, slice, errbuf, sizeof(errbuf)) != 0)
                        fprintf(stderr, "WERR| module %d: %s\n", idx, errbuf[0] ? errbuf : "eval error");
                } else {
                    if (peval_budget(ctx, p, slice, errbuf, sizeof(errbuf)) != 0)
                        fprintf(stderr, "WERR| script %d: %s\n", idx, errbuf[0] ? errbuf : "eval error");
                }
            }
        }
        idx++;
        if (!bn) break;
        p = next;
    }
    free(probe);
}
