/* nb_worker_bundle_probe.c - debug bundle probe/patch helper (mechanical extract, single-TU include) */
/* Debug-only (NB_BUNDLE_TRACE=1): inject console probes into kevlar's sxs
 * customElement wrapper (D.prototype.createElement) so a real LOAD reveals
 * what `_.a(O,null)` returns, the live binding of `_.a`/`O`, and whether the
 * wrapper's try threw (the catch swallows the error and RETURNS UNDEFINED).
 * The page bundle is re-extracted every load, so injection happens here at
 * eval time against the materialized slice text. Returns a malloc'd patched
 * buffer via *out (caller frees), or 0 when the env is off / needles absent.
 * Needle counts must match exactly, else the patch is declined. */
static int nb_bundle_probe(const char *src, size_t src_n,
                           char **out, size_t *out_n) {
    static const char *env = NULL;
    if (!env) env = getenv("NB_BUNDLE_TRACE");
    if (!env || !env[0] || env[0] == '0') return 0;
    static const struct { const char *nd; const char *rp; int want; } pat[] = {
        { "if(!this.isInert)if(_.f(\"web_monomer_web_component_wrapper_handle_errors\")){WI=this;try{",
          "if(!this.isInert)if(_.f(\"web_monomer_web_component_wrapper_handle_errors\")){try{console.log(\"NBT|enter:\"+this.tagName+\" flag=1\");}catch(e0){}WI=this;try{", 1 },
        { "function(){return _.a(O,null)}",
          "function(){var r=_.a(O,null);try{console.log(\"NBT|euv:a=\"+(typeof _.a)+\"|\"+String(_.a).slice(0,40)+\"|O=\"+(typeof O)+\"|\"+String(O).slice(0,40)+\"|nS=\"+(_.nS===void 0?\"u\":_.nS?1:0)+\"|ret=\"+(typeof r)+\"|\"+String(r).slice(0,40));}catch(e1){}return r}", 2 },
        { "catch(L){h=function(B)",
          "catch(L){try{console.log(\"NBT|ERR:\"+String(L&&L.message||L));}catch(e2){}h=function(B)", 1 },
        { "if(O instanceof cSc)return L=O.render(),Qkq(L,S,D,g,h);",
          "try{console.log(\"NBT|qkq c=\"+(O&&O.constructor&&O.constructor.name)+\" instcSc=\"+(O instanceof cSc)+\" render=\"+(typeof (O&&O.render)));}catch(eQ){console.log(\"NBT|qkqT:\"+eQ)}if(O instanceof cSc)return L=O.render(),Qkq(L,S,D,g,h);", 1 },
        { "(Q[R]instanceof Function&&FyC(\"Function props must be configured",
          "((function(){try{console.log(\"SBP| R=\"+R+\" q=\"+(typeof Q)+\" qr=\"+(Q==null?\"na\":(typeof Q[R]))+\" L=\"+(typeof L));}catch(e9){console.log(\"SBP|x \"+e9)}})(),Q[R]instanceof Function&&FyC(\"Function props must be configured", 1 },
    };
    const int pat_n = (int)(sizeof(pat) / sizeof(pat[0]));
    int cnt[5] = { 0, 0, 0, 0, 0 };
    const char *q = src, *end = src + src_n;
    for (int i = 0; i < pat_n; i++) {
        size_t nl = strlen(pat[i].nd);
        for (const char *z = src; z + nl <= end; ) {
            const char *hit = memmem(z, (size_t)(end - z), pat[i].nd, nl);
            if (!hit) break;
            cnt[i]++;
            q = hit + 1;
            z = q;
        }
    }
    for (int i = 0; i < pat_n; i++)
        if (cnt[i] != pat[i].want) {
            fprintf(stderr, "NBT|declined: counts %d/%d/%d/%d/%d (want %d/%d/%d/%d/%d)\n",
                    cnt[0], cnt[1], cnt[2], cnt[3], cnt[4],
                    pat[0].want, pat[1].want, pat[2].want, pat[3].want, pat[4].want);
            return 0;
        }
    size_t out_cap = src_n + 4096, off = 0;
    char *b = malloc(out_cap);
    if (!b) return 0;
    const char *z = src;
    while (z < end) {
        int done = 0;
        for (int i = 0; i < pat_n && !done; i++) {
            size_t nl = strlen(pat[i].nd);
            if ((size_t)(end - z) >= nl && memcmp(z, pat[i].nd, nl) == 0) {
                size_t rl = strlen(pat[i].rp);
                if (off + rl >= out_cap) { free(b); return 0; }
                memcpy(b + off, pat[i].rp, rl); off += rl; z += nl; done = 1;
            }
        }
        if (!done) { b[off++] = *z++; }
    }
    b[off] = 0;
    fprintf(stderr, "NBT|patch applied (%zu bytes)\n", off);
    *out = b; *out_n = off;
    return 1;
}
