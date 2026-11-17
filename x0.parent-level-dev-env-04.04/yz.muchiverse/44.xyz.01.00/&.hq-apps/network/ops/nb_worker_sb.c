/* nb_worker_sb.c - small string builder helpers (mechanical extract, single-TU include) */
/* ---- small string builder ---- */
typedef struct { char *s; size_t len, cap; } SB;
static void sb_grow(SB *b, size_t need) {
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 64;
    while (nc < b->len + need + 1) nc *= 2;
    char *nb = realloc(b->s, nc);
    if (!nb) abort();
    b->s = nb; b->cap = nc;
}
static void sb_put(SB *b, const char *x) {
    if (!x) return;
    size_t n = strlen(x);
    sb_grow(b, n);
    memcpy(b->s + b->len, x, n);
    b->len += n;
    b->s[b->len] = 0;
}
