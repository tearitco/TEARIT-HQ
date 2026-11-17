/* nb_worker_render_row_helpers.c - render row/selection helpers (mechanical extract, single-TU include) */
#define RWS_TEXT 88
#define RENDER_MAX 60000        /* keep the RENDER frame inside every buffer */
static void rw_row(SB *b, const char *key, const char *val) {
    if (b->len >= RENDER_MAX) return;
    size_t o = 0;
    char buf[4096];
    for (size_t i = 0; val && val[i] && o + 1 < sizeof(buf); i++) {
        unsigned char c = (unsigned char)val[i];
        if (c == '\r') continue;
        if (c == '\n' || c == '|') buf[o++] = ' ';
        else buf[o++] = (char)c;
    }
    buf[o] = 0;
    if (o) { sb_put(b, key); sb_put(b, "|"); sb_put(b, buf); sb_put(b, "\n"); }
}
static void rw_sel(SB *b, const NbNode *n);

/* Long text is split into several TEXT rows; each one gets its own SEL row so
 * every rendered row is independently clickable back to its element. */
static void rw_wrap(SB *b, char *s, const NbNode *n) {
    while (s && *s) {
        size_t L = strlen(s);
        if (L <= RWS_TEXT) { rw_sel(b, n); rw_row(b, "TEXT", s); break; }
        size_t cut = RWS_TEXT;
        while (cut > RWS_TEXT / 2 && s[cut] && s[cut] != ' ') cut--;
        if (s[cut] == ' ') { char save = s[cut]; s[cut] = 0; rw_sel(b, n); rw_row(b, "TEXT", s); s[cut] = save; s += cut + 1; }
        else { char save = s[RWS_TEXT]; s[RWS_TEXT] = 0; rw_sel(b, n); rw_row(b, "TEXT", s); s[RWS_TEXT] = save; s += RWS_TEXT; }
    }
}
static char *attrs_set(const NbNode *n, const char *name, const char *val);

/* Every row the walk emits is preceded by a SEL row naming a selector that
 * document.querySelector() can resolve back to the same element, so the
 * manager can turn a khtpm click on a rendered row into a real DOM EVENT.
 * Elements that already carry an id use it; the rest get a stable synthetic
 * id written into the raw attribute blob (nb_attr_get reads that blob, not
 * n->id, so setting n->id alone would not be findable). */
static int g_auto_id_seq;
static void rw_sel(SB *b, const NbNode *n) {
    if (!n) return;
    const char *id = nb_attr_get(n, "id");
    char auto_id[48];
    if (!id || !id[0]) {
        snprintf(auto_id, sizeof(auto_id), "nb-auto-%d", ++g_auto_id_seq);
        NbNode *w = (NbNode *)n;
        char *na = attrs_set(w, "id", auto_id);
        free(w->attrs);
        w->attrs = na;
        if (w->id) { free(w->id); }
        w->id = strdup(auto_id);
        id = auto_id;
    }
    char sel[96];
    snprintf(sel, sizeof(sel), "#%s", id);
    rw_row(b, "SEL", sel);
}
