#define _POSIX_C_SOURCE 200809L
/* nb_js_worker.c — resident JavaScript worker for network-browser-hq.
 * Owned by network_browser_manager (its direct child, spawned lazily on
 * first <script> presence), talking line-RPC over a socketpair dup2'd to
 * stdin/stdout. NB-JS worker plan §1/§2/§4.
 *
 * This is the step-2 SKELETON: it creates one Duktape heap, reuses the
 * shared rung-1/6 host (nb_host.h), reads LOAD-delivered files, runs the
 * page's JS, and reports STATUS ok|err. It writes nothing to the page yet
 * (the DOM-tree accessors + RENDER merge land in steps 3-4). It stays
 * resident across LOADs and never bare-spins (blocks on stdin read).
 *
 * RPC framing (plan §4): length-prefixed lines.
 *   send:  "%.6d\n" + payload + "\n"
 *   recv:  read a %06d length line, then that many payload bytes.
 *
 * manager -> worker: LOAD\n<path page.js>\n<path fetch.dom>\n<href>\n<title>
 *                    (this step uses only page.js; fetch.dom is read in
 *                     step 3 once the DOM tree lands in the worker)
 * worker  -> manager: STATUS ok
 * worker  -> manager: STATUS err:<message>
 * manager -> worker: QUIT            (shut down cleanly)
 */
#include "nb_host.h"
#include "../nb_dom.h"

#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>

#define MAX_MSG (1024 * 1024)

/* ---- wire framing ---- */
static char g_rbuf[MAX_MSG];      /* command payload buffer */
static size_t g_rlen = 0;

static char g_page_js[4096];
static char g_fetch_dom[4096];
static char g_href[4096];
static char g_title[512];

static void send_payload(const char *payload, size_t n) {
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(STDOUT_FILENO, lenbuf, (size_t)ln);
    if (n) (void)!write(STDOUT_FILENO, payload, n);
    (void)!write(STDOUT_FILENO, "\n", 1);
}

static void send_status(const char *status) {
    send_payload(status, strlen(status));
}

/* Read one length-prefixed payload from stdin into g_rbuf.
 * Returns 1 on success, 0 on EOF/shutdown. */
static int recv_frame(void) {
    char lenbuf[16];
    size_t i = 0;
    for (;;) {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r == 0) return 0;                 /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (c == '\n') break;
        if (i < sizeof(lenbuf) - 1) lenbuf[i++] = c;
    }
    lenbuf[i] = 0;
    long n = strtol(lenbuf, NULL, 10);
    if (n < 0 || n > MAX_MSG) return 0;
    size_t got = 0;
    while (got < (size_t)n) {
        ssize_t r = read(STDIN_FILENO, g_rbuf + got, (size_t)n - got);
        if (r == 0) return 0;
        if (r < 0) { if (errno == EINTR) continue; return 0; }
        got += (size_t)r;
    }
    g_rbuf[got] = 0;
    g_rlen = got;
    /* consume the trailing '\n' after payload (if any) */
    {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r > 0 && c != '\n') {
            /* not a trailing newline: buffer it back conceptually by not
             * over-reading; we simply ignore stray bytes here */
            (void)c;
        }
    }
    return 1;
}

/* Split g_rbuf into the LOAD fields, copying each into a target buffer.
 * LOAD payload layout (newline-delimited, plan §4 + extra href/title):
 *   line 0 = "LOAD"
 *   line 1 = page.js path
 *   line 2 = fetch.dom path
 *   line 3 = href
 *   line 4 = title
 */
static void split_lines(char *fields[8]) {
    int fi = 0;
    char *p = g_rbuf;
    fields[0] = p;
    for (size_t i = 0; i < g_rlen; i++) {
        if (g_rbuf[i] == '\n') {
            g_rbuf[i] = 0;
            if (i + 1 < g_rlen && fi < 7) fields[++fi] = &g_rbuf[i + 1];
        }
    }
    for (int k = fi + 1; k < 8; k++) fields[k] = NULL;
}

/* ==================== rung 2 DOM (worker side) ====================
 * The manager serializes the DOM to fetch.dom (nb_dom.h/.c); the worker
 * rebuilds the NbNode tree here and exposes it to JS via native Duktape
 * accessors (plan §7 step 3, roadmap §2 minimum API). The JS side only
 * holds opaque pointer handles; the tree is C-side. No shared memory. */

static NbNode *g_dom_root = NULL;  /* current page's DOM tree (#document) */
static NbNode *g_orphans = NULL;   /* detached createElement() nodes still to free */
#define NODEKEY "_nbnode"

/* JS handles are a plain numeric index into g_nodeindex (index -> NbNode*),
 * which is more robust than round-tripping a Duktape pointer object and
 * survives across multiple lazily-created wrappers for the same node. */
static NbNode **g_nodeindex = NULL;
static int g_nodecount = 0, g_nodecap = 0;
static int node_index(NbNode *n) {
    for (int i = 0; i < g_nodecount; i++) if (g_nodeindex[i] == n) return i;
    if (g_nodecount >= g_nodecap) {
        int nc = g_nodecap ? g_nodecap * 2 : 64;
        NbNode **na = realloc(g_nodeindex, (size_t)nc * sizeof(*na));
        if (!na) return -1;
        g_nodeindex = na; g_nodecap = nc;
    }
    g_nodeindex[g_nodecount++] = n;
    return g_nodecount - 1;
}
static void node_index_reset(void) {
    free(g_nodeindex);
    g_nodeindex = NULL;
    g_nodecount = 0;
    g_nodecap = 0;
}

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

/* ---- node tree helpers (NbNode fields are public in nb_dom.h) ---- */
static void local_append(NbNode *parent, NbNode *child) {
    child->parent = parent;
    child->next_sibling = NULL;
    if (parent->last_child) parent->last_child->next_sibling = child;
    else parent->first_child = child;
    parent->last_child = child;
}
static void node_detach(NbNode *n) {
    if (!n || !n->parent) return;
    NbNode *p = n->parent, *prev = NULL;
    for (NbNode *c = p->first_child; c; c = c->next_sibling) {
        if (c == n) break;
        prev = c;
    }
    if (prev) prev->next_sibling = n->next_sibling;
    else p->first_child = n->next_sibling;
    if (p->last_child == n) p->last_child = prev;
    n->parent = NULL;
    n->next_sibling = NULL;
}
static void clear_children(NbNode *n) {
    while (n->first_child) {
        NbNode *c = n->first_child;
        n->first_child = c->next_sibling;
        if (n->last_child == c) n->last_child = NULL;
        nb_node_free(c);
    }
    n->last_child = NULL;
}
static void orphan_add(NbNode *n) {
    n->next_sibling = (NbNode *)g_orphans;
    g_orphans = n;
}
static void orphan_remove(NbNode *n) {
    NbNode *prev = NULL;
    for (NbNode *o = g_orphans; o; o = o->next_sibling) {
        if (o == n) {
            if (prev) prev->next_sibling = o->next_sibling;
            else g_orphans = o->next_sibling;
            o->next_sibling = NULL;
            return;
        }
        prev = o;
    }
}

/* ---- text / html serialization ---- */
static void node_text_content(const NbNode *n, SB *b) {
    if (!n) return;
    sb_put(b, n->text);
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        node_text_content(c, b);
}
static void node_outer_html(const NbNode *n, SB *b) {
    if (!n) return;
    sb_put(b, "<");
    sb_put(b, n->attrs ? n->attrs : n->tag);
    sb_put(b, ">");
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        node_outer_html(c, b);
    sb_put(b, "</");
    sb_put(b, n->tag);
    sb_put(b, ">");
}

/* ---- selector engine (subset: tag, #id, .class, tag.class, descendants) ---- */
static int has_class(const NbNode *n, const char *tok) {
    if (!n->cls || !tok) return 0;
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) { if (!strcmp(c, tok)) return 1; c = strtok(NULL, " "); }
    return 0;
}
static int match_compound(const NbNode *n, const char *cmp) {
    char tag[64] = "", id[64] = "", clbuf[512] = "";
    const char *p = cmp;
    const char *ts = p;
    while (*ts && (isalnum((unsigned char)*ts) || *ts == '-' || *ts == '_' || *ts == ':')) ts++;
    size_t tg = (size_t)(ts - p);
    if (tg && tg < 64) { memcpy(tag, p, tg); tag[tg] = 0; p = ts; }
    while (*p == '#' || *p == '.') {
        if (*p == '#') {
            p++; const char *es = p;
            while (*es && (isalnum((unsigned char)*es) || *es == '-' || *es == '_' || *es == ':')) es++;
            size_t d = (size_t)(es - p);
            if (d < 64) { memcpy(id, p, d); id[d] = 0; }
            p = es;
        } else {
            p++; const char *es = p;
            while (*es && (isalnum((unsigned char)*es) || *es == '-' || *es == '_' || *es == ':')) es++;
            size_t d = (size_t)(es - p);
            if (strlen(clbuf) + d < 510) {
                if (clbuf[0]) strcat(clbuf, " ");
                memcpy(clbuf + strlen(clbuf), p, d);
                clbuf[strlen(clbuf) + d] = 0;
            }
            p = es;
        }
    }
    if (tag[0] && (!n->tag || strcasecmp(n->tag, tag))) return 0;
    if (id[0] && (!n->id || strcmp(n->id, id))) return 0;
    char *tok = strtok(clbuf, " ");
    while (tok) { if (!has_class(n, tok)) return 0; tok = strtok(NULL, " "); }
    return 1;
}
static int match_chain(NbNode *n, char **parts, int idx) {
    if (!n) return 0;
    if (!match_compound(n, parts[idx])) return 0;
    if (idx == 0) return 1;
    return match_chain(n->parent, parts, idx - 1);
}
static int match_any_selector(NbNode *n, const char *sel) {
    if (!n || !sel) return 0;
    char copy[512]; size_t sl = strlen(sel); if (sl > 511) sl = 511;
    memcpy(copy, sel, sl); copy[sl] = 0;
    char *parts[16]; int np = 0;
    char *tok = strtok(copy, " \t");
    while (tok && np < 16) { parts[np++] = tok; tok = strtok(NULL, " \t"); }
    if (np == 0) return 0;
    return match_chain(n, parts, np - 1);
}
static NbNode *query_first(NbNode *n, const char *sel) {
    if (!n) return NULL;
    if (match_any_selector(n, sel)) return n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) return r;
    }
    return NULL;
}
static NbNode *find_by_id(const NbNode *n, const char *id) {
    if (!n || !id) return NULL;
    if (n->id && !strcmp(n->id, id)) return (NbNode *)n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) return r;
    }
    return NULL;
}
static NbNode *find_tag_first(const NbNode *n, const char *tag) {
    if (!n || !tag) return NULL;
    if (n->tag && !strcasecmp(n->tag, tag)) return (NbNode *)n;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        NbNode *r = find_tag_first(c, tag);
        if (r) return r;
    }
    return NULL;
}

/* ---- JS <-> C node binding ---- */
static NbNode *get_node(duk_context *ctx, duk_idx_t idx) {
    if (!duk_is_object(ctx, idx)) return NULL;
    duk_get_prop_string(ctx, idx, NODEKEY);
    int i = duk_is_number(ctx, -1) ? (int)duk_get_int(ctx, -1) : -1;
    duk_pop(ctx);
    if (i < 0 || i >= g_nodecount) return NULL;
    return g_nodeindex[i];
}
/* Node from the `this` binding of an element native (Duktape places the
 * this-binding above the args; the API is duk_push_this()). */
static NbNode *get_this(duk_context *ctx) {
    duk_push_this(ctx);
    if (!duk_is_object(ctx, -1)) { duk_pop(ctx); return NULL; }
    duk_get_prop_string(ctx, -1, NODEKEY);
    int i = duk_is_number(ctx, -1) ? (int)duk_get_int(ctx, -1) : -1;
    duk_pop(ctx);
    duk_pop(ctx);
    if (i < 0 || i >= g_nodecount) return NULL;
    return g_nodeindex[i];
}
static void push_node(duk_context *ctx, NbNode *n);

/* ---- document natives ---- */
static duk_ret_t nb_dom_getElementById(duk_context *ctx) {
    const char *id = duk_get_string(ctx, 0);
    if (!id || !g_dom_root) { duk_push_null(ctx); return 1; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) { push_node(ctx, r); return 1; }
    }
    duk_push_null(ctx);
    return 1;
}
static void collect_tag_into(duk_context *ctx, NbNode *n, const char *tag, duk_idx_t arr, int *i) {
    if (!n) return;
    if (!tag || !*tag || !strcmp(tag, "*") || (n->tag && !strcasecmp(n->tag, tag))) {
        push_node(ctx, n);
        duk_put_prop_index(ctx, arr, (*i)++);
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_tag_into(ctx, (NbNode *)c, tag, arr, i);
}
static duk_ret_t nb_dom_getElementsByTagName(duk_context *ctx) {
    const char *tag = duk_get_string(ctx, 0);
    duk_idx_t arr = duk_push_array(ctx);
    if (!g_dom_root) return 1;
    int i = 0;
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
        collect_tag_into(ctx, (NbNode *)c, tag, arr, &i);
    return 1;
}
static void qsa_into(duk_context *ctx, NbNode *n, const char *sel, duk_idx_t arr, int *i) {
    if (!n) return;
    if (match_any_selector(n, sel)) { push_node(ctx, n); duk_put_prop_index(ctx, arr, (*i)++); }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        qsa_into(ctx, (NbNode *)c, sel, arr, i);
}
static duk_ret_t nb_dom_querySelector(duk_context *ctx) {
    const char *sel = duk_get_string(ctx, 0);
    if (!sel || !g_dom_root) { duk_push_null(ctx); return 1; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) { push_node(ctx, r); return 1; }
    }
    duk_push_null(ctx);
    return 1;
}
static duk_ret_t nb_dom_querySelectorAll(duk_context *ctx) {
    const char *sel = duk_get_string(ctx, 0);
    duk_idx_t arr = duk_push_array(ctx);
    if (!g_dom_root || !sel) return 1;
    int i = 0;
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
        qsa_into(ctx, (NbNode *)c, sel, arr, &i);
    return 1;
}
static duk_ret_t nb_dom_createElement(duk_context *ctx) {
    const char *tag = duk_get_string(ctx, 0) ? duk_get_string(ctx, 0) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { duk_push_null(ctx); return 1; }
    n->tag = strdup(tag);
    for (char *t = n->tag; *t; t++) *t = (char)((*t >= 'A' && *t <= 'Z') ? *t + 32 : *t);
    orphan_add(n);
    push_node(ctx, n);
    return 1;
}
static duk_ret_t nb_dom_documentElement(duk_context *ctx) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "html") : NULL;
    if (el) push_node(ctx, el); else duk_push_null(ctx);
    return 1;
}
static duk_ret_t nb_dom_body(duk_context *ctx) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "body") : NULL;
    if (el) push_node(ctx, el); else duk_push_null(ctx);
    return 1;
}

/* ---- element natives (this = element object) ---- */
static duk_ret_t nb_el_getAttribute(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *name = duk_get_string(ctx, 0);
    if (!n || !name) { duk_push_null(ctx); return 1; }
    const char *v = nb_attr_get(n, name);
    if (v && v[0]) { duk_push_string(ctx, v); return 1; }
    duk_push_null(ctx);
    return 1;
}
static char *attrs_set(const NbNode *n, const char *name, const char *val) {
    SB b = {0, 0, 0};
    const char *p = n->attrs ? n->attrs : "";
    size_t nl = strlen(name);
    int found = 0, first = 1;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        char kbuf[64]; size_t kc = kl < 63 ? kl : 63; memcpy(kbuf, ks, kc); kbuf[kc] = 0;
        int is_target = kl == nl && !strncasecmp(ks, name, nl);
        char vtmp[1200]; int hasv = 0;
        const char *savep = p;
        if (*p == '=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p == '"' || *p == '\'') { qc = *p; p++; }
            const char *vs = p;
            while (*p && !(qc ? (*p == qc) : (isspace((unsigned char)*p) || *p == '>'))) p++;
            size_t vl = (size_t)(p - vs);
            if (qc && *p) p++;
            size_t vc = vl < 1199 ? vl : 1199; memcpy(vtmp, vs, vc); vtmp[vc] = 0;
            hasv = 1;
        }
        if (is_target) {
            found = 1;
            if (!first) sb_put(&b, " ");
            sb_put(&b, name); sb_put(&b, "=\"");
            sb_put(&b, val ? val : "");
            sb_put(&b, "\"");
        } else {
            const char *after = hasv ? p : savep;
            size_t ll = (size_t)(after - ks);
            if (!first) sb_put(&b, " ");
            char tmp[8196]; size_t lc = ll < 8191 ? ll : 8191;
            memcpy(tmp, ks, lc); tmp[lc] = 0;
            sb_put(&b, tmp);
        }
        while (*p && !isspace((unsigned char)*p)) p++;
        first = 0;
    }
    if (!found) {
        if (!first) sb_put(&b, " ");
        sb_put(&b, name); sb_put(&b, "=\"");
        sb_put(&b, val ? val : "");
        sb_put(&b, "\"");
    }
    return b.s ? b.s : strdup("");
}
static duk_ret_t nb_el_setAttribute(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *name = duk_get_string(ctx, 0);
    const char *val = duk_get_string(ctx, 1);
    if (!n || !name) return 0;
    if (!val) val = "";
    if (!strcasecmp(name, "id")) { free(n->id); n->id = strdup(val); }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = strdup(val); }
    char *na = attrs_set(n, name, val);
    free(n->attrs);
    n->attrs = na;
    return 0;
}
static duk_ret_t nb_el_id_get(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    duk_push_string(ctx, n && n->id ? n->id : "");
    return 1;
}
static duk_ret_t nb_el_id_set(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *v = duk_get_string(ctx, 0) ? duk_get_string(ctx, 0) : "";
    if (n) { free(n->id); n->id = strdup(v); }
    return 0;
}
static duk_ret_t nb_el_className_get(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    duk_push_string(ctx, n && n->cls ? n->cls : "");
    return 1;
}
static duk_ret_t nb_el_className_set(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *v = duk_get_string(ctx, 0) ? duk_get_string(ctx, 0) : "";
    if (n) { free(n->cls); n->cls = strdup(v); }
    return 0;
}
static duk_ret_t nb_el_textContent_get(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    SB b = {0, 0, 0};
    node_text_content(n, &b);
    duk_push_string(ctx, b.s ? b.s : "");
    free(b.s);
    return 1;
}
static duk_ret_t nb_el_textContent_set(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (!n) return 0;
    const char *v = duk_get_string(ctx, 0) ? duk_get_string(ctx, 0) : "";
    clear_children(n);
    free(n->text);
    n->text = strdup(v);
    return 0;
}
static duk_ret_t nb_el_innerHTML_get(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    SB b = {0, 0, 0};
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling) node_outer_html(c, &b);
    duk_push_string(ctx, b.s ? b.s : "");
    free(b.s);
    return 1;
}
static duk_ret_t nb_el_innerHTML_set(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (!n) return 0;
    const char *v = duk_get_string(ctx, 0) ? duk_get_string(ctx, 0) : "";
    NbNode *frag = nb_parse_html(v, strlen(v));
    NbNode *child = frag ? frag->first_child : NULL;
    clear_children(n);
    if (child) {
        frag->first_child = frag->last_child = NULL;
        NbNode *cur = child;
        while (cur) {
            NbNode *nx = cur->next_sibling;
            cur->next_sibling = NULL;
            cur->parent = NULL;       /* let local_append set it */
            local_append(n, cur);
            cur = nx;
        }
    }
    nb_node_free(frag);
    return 0;
}
static duk_ret_t nb_el_children(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    duk_idx_t arr = duk_push_array(ctx);
    if (!n) return 1;
    int i = 0;
    for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
        if (!c->tag) continue;   /* children is element-only (childNodes keeps text) */
        push_node(ctx, (NbNode *)c);
        duk_put_prop_index(ctx, arr, i++);
    }
    return 1;
}
static duk_ret_t nb_el_childNodes(duk_context *ctx) { return nb_el_children(ctx); }
static duk_ret_t nb_el_parentNode(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    NbNode *p = n ? n->parent : NULL;
    if (p) push_node(ctx, p); else duk_push_null(ctx);
    return 1;
}
static duk_ret_t nb_el_firstChild(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (n && n->first_child) push_node(ctx, n->first_child); else duk_push_null(ctx);
    return 1;
}
static duk_ret_t nb_el_nextSibling(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (n && n->next_sibling) push_node(ctx, n->next_sibling); else duk_push_null(ctx);
    return 1;
}
static duk_ret_t nb_el_appendChild(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    NbNode *ch = duk_is_object(ctx, 0) ? get_node(ctx, 0) : NULL;
    if (!n || !ch || ch == n) { duk_push_null(ctx); return 1; }
    node_detach(ch);
    orphan_remove(ch);
    local_append(n, ch);
    push_node(ctx, ch);
    return 1;
}
/* ---- classList natives (this = the classList object, shares \xffnode) ---- */
static duk_ret_t nb_cl_add(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *tok = duk_get_string(ctx, 0);
    if (!n || !tok || !*tok) return 0;
    if (!has_class(n, tok)) {
        SB b = {0, 0, 0};
        if (n->cls && *n->cls) { sb_put(&b, n->cls); sb_put(&b, " "); }
        sb_put(&b, tok);
        free(n->cls); n->cls = b.s;
    }
    return 0;
}
static duk_ret_t nb_cl_remove(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *tok = duk_get_string(ctx, 0);
    if (!n || !tok) return 0;
    if (!has_class(n, tok)) return 0;
    SB b = {0, 0, 0};
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) {
        if (strcmp(c, tok)) { if (b.len) sb_put(&b, " "); sb_put(&b, c); }
        c = strtok(NULL, " ");
    }
    free(n->cls); n->cls = b.s ? b.s : strdup("");
    return 0;
}
static duk_ret_t nb_cl_toggle(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *tok = duk_get_string(ctx, 0);
    if (!n || !tok || !*tok) { duk_push_boolean(ctx, 0); return 1; }
    if (has_class(n, tok)) { nb_cl_remove(ctx); duk_push_boolean(ctx, 0); return 1; }
    nb_cl_add(ctx);
    duk_push_boolean(ctx, 1);
    return 1;
}
static duk_ret_t nb_cl_contains(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    const char *tok = duk_get_string(ctx, 0);
    duk_push_boolean(ctx, n && tok ? has_class(n, tok) : 0);
    return 1;
}

/* Build a JS element object wrapping a C NbNode. */
static void push_node(duk_context *ctx, NbNode *n) {
    int nidx = node_index(n);
    duk_push_object(ctx);                            /* el */
    duk_push_int(ctx, nidx);
    duk_put_prop_string(ctx, -2, NODEKEY);
    duk_push_string(ctx, n->tag ? n->tag : "");
    duk_put_prop_string(ctx, -2, "nodeName");
    duk_push_string(ctx, n->tag ? n->tag : "");
    duk_put_prop_string(ctx, -2, "tagName");

    duk_push_c_function(ctx, nb_el_getAttribute, 1);  duk_put_prop_string(ctx, -2, "getAttribute");
    duk_push_c_function(ctx, nb_el_setAttribute, 2);  duk_put_prop_string(ctx, -2, "setAttribute");
    duk_push_c_function(ctx, nb_el_appendChild, 1);   duk_put_prop_string(ctx, -2, "appendChild");

    /* read-only accessor properties: children, childNodes, parentNode, firstChild, nextSibling */
    duk_push_string(ctx, "children");
    duk_push_c_function(ctx, nb_el_children, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "childNodes");
    duk_push_c_function(ctx, nb_el_childNodes, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "parentNode");
    duk_push_c_function(ctx, nb_el_parentNode, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "firstChild");
    duk_push_c_function(ctx, nb_el_firstChild, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "nextSibling");
    duk_push_c_function(ctx, nb_el_nextSibling, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);

    /* accessors: id, className, textContent, innerHTML */
    duk_push_string(ctx, "id");
    duk_push_c_function(ctx, nb_el_id_get, 0);
    duk_push_c_function(ctx, nb_el_id_set, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "className");
    duk_push_c_function(ctx, nb_el_className_get, 0);
    duk_push_c_function(ctx, nb_el_className_set, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "textContent");
    duk_push_c_function(ctx, nb_el_textContent_get, 0);
    duk_push_c_function(ctx, nb_el_textContent_set, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "innerHTML");
    duk_push_c_function(ctx, nb_el_innerHTML_get, 0);
    duk_push_c_function(ctx, nb_el_innerHTML_set, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);

    /* classList */
    duk_push_object(ctx);                            /* classList */
    duk_push_int(ctx, nidx);
    duk_put_prop_string(ctx, -2, NODEKEY);
    duk_push_c_function(ctx, nb_cl_add, 1);          duk_put_prop_string(ctx, -2, "add");
    duk_push_c_function(ctx, nb_cl_remove, 1);       duk_put_prop_string(ctx, -2, "remove");
    duk_push_c_function(ctx, nb_cl_toggle, 1);       duk_put_prop_string(ctx, -2, "toggle");
    duk_push_c_function(ctx, nb_cl_contains, 1);     duk_put_prop_string(ctx, -2, "contains");
    duk_put_prop_string(ctx, -2, "classList");
}

/* Attach the DOM natives to the global `document` object. */
static void install_dom(duk_context *ctx) {
    duk_get_global_string(ctx, "document");
    duk_push_c_function(ctx, nb_dom_getElementById, 1);        duk_put_prop_string(ctx, -2, "getElementById");
    duk_push_c_function(ctx, nb_dom_getElementsByTagName, 1);  duk_put_prop_string(ctx, -2, "getElementsByTagName");
    duk_push_c_function(ctx, nb_dom_querySelector, 1);         duk_put_prop_string(ctx, -2, "querySelector");
    duk_push_c_function(ctx, nb_dom_querySelectorAll, 1);      duk_put_prop_string(ctx, -2, "querySelectorAll");
    duk_push_c_function(ctx, nb_dom_createElement, 1);         duk_put_prop_string(ctx, -2, "createElement");
    duk_push_string(ctx, "documentElement");
    duk_push_c_function(ctx, nb_dom_documentElement, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_string(ctx, "body");
    duk_push_c_function(ctx, nb_dom_body, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_pop(ctx);
}

/* Run the page script; sends STATUS ok|err across the wire. */
static void dom_teardown(void) {
    /* free detached createElement() nodes, then the main tree */
    while (g_orphans) {
        NbNode *o = g_orphans;
        g_orphans = o->next_sibling;
        o->next_sibling = NULL;
        nb_node_free(o);
    }
    nb_node_free(g_dom_root);
    g_dom_root = NULL;
}
static void run_page(void) {
    g_dom_root = NULL;
    g_orphans = NULL;
    node_index_reset();

    /* build the DOM tree from the manager's fetch.dom (if present) */
    if (g_fetch_dom[0]) {
        FILE *df = fopen(g_fetch_dom, "rb");
        if (df) {
            g_dom_root = nb_dom_load(df);
            fclose(df);
        }
    }

    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) { dom_teardown(); send_status("STATUS err:heap"); return; }
    install_host(ctx);

    /* rung-6 prelude: swallow its own failure, page continues */
    if (duk_peval_string(ctx, g_js_prelude) != 0) duk_pop(ctx);
    duk_pop(ctx);

    install_dom(ctx);

    char *src = NULL;
    size_t src_n = 0;
    if (!read_file(g_page_js, &src, &src_n)) {
        duk_destroy_heap(ctx);
        free(src);
        dom_teardown();
        send_status("STATUS err:cannot read page.js");
        return;
    }
    if (src_n == 0) { free(src); duk_destroy_heap(ctx); dom_teardown(); send_status("STATUS ok"); return; }

    duk_push_lstring(ctx, src, src_n);
    free(src);
    int rc = duk_peval(ctx);
    if (rc != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        char msg[1024];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "script error");
        duk_destroy_heap(ctx);
        dom_teardown();
        send_status(msg);
        return;
    }
    duk_pop(ctx);
    duk_destroy_heap(ctx);
    dom_teardown();
    send_status("STATUS ok");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */

    for (;;) {
        if (!recv_frame()) break;

        char *f[8];           split_lines(f);
        const char *cmd = f[0] ? f[0] : "";
        if (strcmp(cmd, "QUIT") == 0) {
            break;
        } else if (strcmp(cmd, "LOAD") == 0) {
            g_title[0] = 0; g_href[0] = 0;
            if (f[1]) snprintf(g_page_js, sizeof(g_page_js), "%s", f[1]);
            if (f[2]) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", f[2]);
            if (f[3]) snprintf(g_href, sizeof(g_href), "%s", f[3]);
            if (f[4]) snprintf(g_title, sizeof(g_title), "%s", f[4]);
            run_page();
        } else {
            send_status("STATUS err:unknown command");
        }
    }
    return 0;
}
