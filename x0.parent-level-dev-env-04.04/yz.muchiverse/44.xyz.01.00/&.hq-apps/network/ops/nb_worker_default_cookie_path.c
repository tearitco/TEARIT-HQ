/* nb_worker_default_cookie_path.c - default cookie path helper (mechanical extract, single-TU include) */
static void default_cookie_path(const char *rp, char *out, size_t olen) {
    if (!rp || rp[0] != '/') { snprintf(out, olen, "/"); return; }
    const char *r = strrchr(rp, '/');
    if (!r || r == rp) { snprintf(out, olen, "/"); return; }
    size_t n = (size_t)(r - rp);
    if (n >= olen) n = olen - 1;
    memcpy(out, rp, n); out[n] = 0;
    if (!out[0]) snprintf(out, olen, "/");
}
