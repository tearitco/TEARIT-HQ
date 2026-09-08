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
 * worker  -> manager: RENDER\n<len>\n<page.state rows>   (post-JS DOM, step 4)
 * worker  -> manager: STATUS ok|err:<message>
 * manager -> worker: QUIT            (shut down cleanly)
 */
#include "nb_host.h"
#include "../nb_dom.h"

#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

extern char **environ;

#define MAX_MSG (1024 * 1024)

/* ---- wire framing ---- */
static char g_rbuf[MAX_MSG];      /* command payload buffer */
static size_t g_rlen = 0;

static char g_page_js[4096];
static char g_fetch_dom[4096];
static char g_href[4096];
static char g_title[512];

static int g_cli = 0;            /* argv mode: plain text out, no RPC framing */
static int g_cli_status_ok = 0;  /* CLI exit-status latch set by send_status */

static void send_payload(const char *payload, size_t n) {
    if (g_cli) {
        /* plain text: drop the "RENDER\n" frame header, then the rows */
        size_t off = (strncmp(payload, "RENDER\n", 7) == 0 && n >= 7) ? 7 : 0;
        (void)!write(STDOUT_FILENO, payload + off, n - off);
        if (n - off == 0 || payload[n - 1] != '\n')
            (void)!write(STDOUT_FILENO, "\n", 1);
        return;
    }
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(STDOUT_FILENO, lenbuf, (size_t)ln);
    if (n) (void)!write(STDOUT_FILENO, payload, n);
    (void)!write(STDOUT_FILENO, "\n", 1);
}

static void send_status(const char *status) {
    if (g_cli) {
        g_cli_status_ok = (strncmp(status, "STATUS ok", 9) == 0);
        if (strncmp(status, "STATUS ", 7) == 0) status += 7;
        (void)!write(STDOUT_FILENO, status, strlen(status));
        (void)!write(STDOUT_FILENO, "\n", 1);
        return;
    }
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
#define NODE_HANDLE_CAP 250000   /* plan step 5: bound JS-created wrappers */
static int node_index(NbNode *n) {
    for (int i = 0; i < g_nodecount; i++) if (g_nodeindex[i] == n) return i;
    if (g_nodecount >= NODE_HANDLE_CAP) return -1;   /* cap: fail the wrapper */
    if (g_nodecount >= g_nodecap) {
        int nc = g_nodecap ? g_nodecap * 2 : 64;
        if (nc > NODE_HANDLE_CAP) nc = NODE_HANDLE_CAP;
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

/* ---- step 4: RENDER — serialize the (post-JS) DOM into page.state.txt
 * rows exactly as the manager's projector consumes them (TITLE/TEXT/LINK/IMG).
 * Only the worker's own tree is authoritative here, so JS mutations
 * (document.title, el.textContent, innerHTML=, appendChild) become visible. */
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
static void rw_wrap(SB *b, char *s) {
    while (s && *s) {
        size_t L = strlen(s);
        if (L <= RWS_TEXT) { rw_row(b, "TEXT", s); break; }
        size_t cut = RWS_TEXT;
        while (cut > RWS_TEXT / 2 && s[cut] && s[cut] != ' ') cut--;
        if (s[cut] == ' ') { char save = s[cut]; s[cut] = 0; rw_row(b, "TEXT", s); s[cut] = save; s += cut + 1; }
        else { char save = s[RWS_TEXT]; s[RWS_TEXT] = 0; rw_row(b, "TEXT", s); s[RWS_TEXT] = save; s += RWS_TEXT; }
    }
}
static void dom_walk_render(const NbNode *n, int *titled, SB *b) {
    if (!n) return;
    const char *tg = n->tag;
    int caption_used = 0;   /* element's own text already surfaced as TITLE/LINK/IMG */
    if (tg && !strcasecmp(tg, "title") && !*titled) {
        SB t = {0, 0, 0};
        node_text_content(n, &t);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
        *titled = 1;
        caption_used = 1;
    } else if (tg && !strcasecmp(tg, "a")) {
        const char *href = nb_attr_get(n, "href");
        if (href && *href) {
            SB t = {0, 0, 0};
            node_text_content(n, &t);
            char linkbuf[2048];
            snprintf(linkbuf, sizeof(linkbuf), "%s|%s", href, t.s && t.s[0] ? t.s : href);
            rw_row(b, "LINK", linkbuf);
            free(t.s);
            caption_used = 1;
        }
    } else if (tg && !strcasecmp(tg, "img")) {
        char srcbuf[1024] = "", altbuf[1024] = "";
        snprintf(srcbuf, sizeof(srcbuf), "%s", nb_attr_get(n, "src"));
        snprintf(altbuf, sizeof(altbuf), "%s", nb_attr_get(n, "alt"));
        if (srcbuf[0]) {
            char imgbuf[2048];
            snprintf(imgbuf, sizeof(imgbuf), "%s|%s", srcbuf, altbuf);
            rw_row(b, "IMG", imgbuf);
            caption_used = 1;
        }
    }
    /* the fetch.dom wire form carries an element's direct text inline on
     * the element node, so surface n->text itself (skip captions above and
     * whitespace-only runs, matching the static extractor) */
    if (!caption_used && n->text && n->text[0] && strspn(n->text, " \t\r\n") < strlen(n->text)) {
        size_t L = strlen(n->text);
        char *copy = malloc(L + 1);
        if (copy) { memcpy(copy, n->text, L + 1); rw_wrap(b, copy); free(copy); }
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        dom_walk_render(c, titled, b);
}
static void dom_render_rows(SB *b) {
    /* TITLE first, like the static extractor: from the <title> element if the
     * parser kept one, else document.title / the LOAD title (g_title). */
    NbNode *te = g_dom_root ? find_tag_first(g_dom_root, "title") : NULL;
    if (te) {
        SB t = {0, 0, 0};
        node_text_content(te, &t);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
    } else if (g_title[0]) {
        rw_row(b, "TITLE", g_title);
    }
    int titled = 1;                 /* <title> already handled above */
    dom_walk_render(g_dom_root, &titled, b);
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
/* el.on<name> accessor names (also parsed from property arg0 of the natives) */
static const char *const ONPROPS[] = {
    "click", "dblclick", "change", "input", "submit", "keydown", "keyup",
    "keypress", "mouseover", "mouseout", "mouseenter", "mouseleave",
    "mousedown", "mouseup", "mousemove", "focus", "blur", "load", "error",
    "resize", "scroll", "contextmenu", NULL
};

static void push_node(duk_context *ctx, NbNode *n);
static duk_ret_t nb_el_addEventListener(duk_context *ctx);
static duk_ret_t nb_el_removeEventListener(duk_context *ctx);
static duk_ret_t nb_el_dispatchEvent(duk_context *ctx);
static duk_ret_t nb_el_click(duk_context *ctx);
static duk_ret_t nb_el_onprop_get(duk_context *ctx);
static duk_ret_t nb_el_onprop_set(duk_context *ctx);

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

#define STASH_NODE_MAP 40000   /* single object: node index -> JS wrapper (identity) */

/* Build a JS element object wrapping a C NbNode. */
static void push_node(duk_context *ctx, NbNode *n) {
    int nidx = node_index(n);
    /* wrapper identity: one JS object per C node (heap is fresh per page).
     * stack: [stash][map]; the wrapper is built on top of both. */
    duk_push_global_stash(ctx);
    duk_get_prop_index(ctx, -1, STASH_NODE_MAP);
    if (!duk_is_object(ctx, -1)) {
        duk_pop(ctx);
        duk_push_object(ctx);
        duk_dup(ctx, -1);
        duk_put_prop_index(ctx, -3, STASH_NODE_MAP);
    }
    if (duk_has_prop_index(ctx, -1, nidx)) {
        duk_get_prop_index(ctx, -1, nidx);   /* existing wrapper */
        duk_remove(ctx, -3);                 /* drop stash, then map */
        duk_remove(ctx, -2);
        return;
    }
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
    duk_push_c_function(ctx, nb_el_addEventListener, 2);    duk_put_prop_string(ctx, -2, "addEventListener");
    duk_push_c_function(ctx, nb_el_removeEventListener, 2); duk_put_prop_string(ctx, -2, "removeEventListener");
    duk_push_c_function(ctx, nb_el_dispatchEvent, 1);       duk_put_prop_string(ctx, -2, "dispatchEvent");
    duk_push_c_function(ctx, nb_el_click, 0);               duk_put_prop_string(ctx, -2, "click");

    /* el.on<type> = cb accessors; native magic carries the ONPROPS index
     * (Duktape accessors pass no property name — setters receive the value,
     * getters receive nothing). */
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        duk_push_string(ctx, onname);
        duk_push_c_function(ctx, nb_el_onprop_get, 0); duk_set_magic(ctx, -1, i);
        duk_push_c_function(ctx, nb_el_onprop_set, 1); duk_set_magic(ctx, -1, i);
        duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    }

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

    /* cache wrapper in the identity map, then drop stash+map, leaving [wrapper] */
    duk_dup(ctx, -1);
    duk_put_prop_index(ctx, -2, nidx);
    duk_remove(ctx, -2);
    duk_remove(ctx, -2);
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

#define EVAL_BUDGET_SEC 2   /* plan step 5: watchdog for runaway page.js */
#define MAX_DRAIN_MS 800    /* commit 7: bounded wait so short timers fire pre-RENDER */
static void sigalrm(int sig);   /* used by run_event_loop below */

/* ===================== Phase 2 (commit 7): timers + microtasks + events ================ */

#define MAX_TIMERS 2048
#define MAX_MICRO  2048
#define MAX_EVENTS 4096
#define MAX_ONPROPS 1024
#define MAX_TIMER_INVOCATIONS 5000   /* plan §2 CPU safety */
#define MAX_RAF_FRAMES 120           /* plan §2: ~2s of rAF */
#define RAF_MS 16

typedef struct { int id, active; long interval; uint64_t due; int slot; } Timer;
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; } EvL;
/* el.on<type> handlers CANNOT live as data props on the wrapper objects:
 * push_node() creates a fresh JS object per wrap, so the C side must own the
 * callback (stashed, keyed by node+kind+type). */
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; } OnProp;

#define STASH_TIMER 0     /* stash index base per table (fixed, non-overlapping) */
#define STASH_MICRO 10000
#define STASH_EVT   20000
#define STASH_ONPROP 30000
#define EVT_NODE 1
#define EVT_WIN  2
#define EVT_DOC  3

static Timer g_timers[MAX_TIMERS];
static int g_timer_count = 0;
static int g_micro_n = 0, g_micro_head = 0;   /* microtask FIFO lives in the global stash */
static EvL g_evl[MAX_EVENTS];
static int g_evl_count = 0;
static OnProp g_onprop[MAX_ONPROPS];
static int g_onprop_count = 0;
static int g_next_id = 1;
static int g_invocations = 0;
static int g_raf_fires = 0;
static int g_pending_err = 0;   /* set when an event-loop callback throws */
static char g_pending_errmsg[512];

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}
static void stash_set(duk_context *ctx, int base, int slot, duk_idx_t fn) {
    duk_push_global_stash(ctx);
    duk_dup(ctx, fn);
    duk_put_prop_index(ctx, -2, (duk_uarridx_t)(base + slot));
    duk_pop(ctx);
}
static void stash_del(duk_context *ctx, int base, int slot) {
    duk_push_global_stash(ctx);
    duk_del_prop_index(ctx, -1, (duk_uarridx_t)(base + slot));
    duk_pop(ctx);
}
static void stash_push(duk_context *ctx, int base, int slot) {
    duk_push_global_stash(ctx);
    duk_get_prop_index(ctx, -1, (duk_uarridx_t)(base + slot));
    duk_remove(ctx, -2);
}

/* Run the callback currently on the stack (below the top) with this=globalThis,
 * 0 args. Duktape pcall_method layout is [args][func][this], this ON TOP: func
 * at top-2, this at top-1. Caller pushes the cb, we push global on top.
 * Returns 0 on success, 1 on thrown error (message captured, stack popped). */
static int invoke_cb0(duk_context *ctx) {
    duk_push_global_object(ctx);   /* [cb][global], global on top = the this */
    if (duk_pcall_method(ctx, 0) != 0) {
        if (!g_pending_err) {
            g_pending_err = 1;
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s",
                     duk_safe_to_string(ctx, -1));
        }
        duk_pop(ctx);
        return 1;
    }
    duk_pop(ctx);
    return 0;
}

/* ---- timers ---- */
static void timer_schedule(duk_context *ctx, long interval, int repeat) {
    if (!duk_is_callable(ctx, 0) || g_timer_count >= MAX_TIMERS) { duk_push_int(ctx, 0); return; }
    int slot = g_timer_count++;
    g_timers[slot].id = g_next_id++;
    g_timers[slot].active = 1;
    g_timers[slot].interval = repeat ? (interval > 0 ? interval : 1) : 0;
    g_timers[slot].due = now_ms() + (uint64_t)(interval > 0 ? interval : 1);
    g_timers[slot].slot = slot;
    stash_set(ctx, STASH_TIMER, slot, 0);
    duk_push_int(ctx, g_timers[slot].id);
}
static duk_ret_t nb_timer_setTimeout(duk_context *ctx) {
    double ms = duk_is_number(ctx, 1) ? duk_get_number(ctx, 1) : 0;
    if (ms < 0) ms = 0;
    timer_schedule(ctx, (long)ms, 0);
    return 1;
}
static duk_ret_t nb_timer_setInterval(duk_context *ctx) {
    double ms = duk_is_number(ctx, 1) ? duk_get_number(ctx, 1) : 0;
    if (ms < 1) ms = 1;
    timer_schedule(ctx, (long)ms, 1);
    return 1;
}
static void timer_clear(duk_context *ctx, int want_oneshot) {
    int id = (int)duk_get_int(ctx, 0);
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active && g_timers[i].id == id &&
            (want_oneshot ? g_timers[i].interval == 0 : g_timers[i].interval > 0)) {
            g_timers[i].active = 0;
            stash_del(ctx, STASH_TIMER, i);
            break;
        }
}
static duk_ret_t nb_timer_clearTimeout(duk_context *ctx) { timer_clear(ctx, 1); return 0; }
static duk_ret_t nb_timer_clearInterval(duk_context *ctx) { timer_clear(ctx, 0); return 0; }

/* ---- microtasks + rAF ---- */
static duk_ret_t nb_queueMicrotask(duk_context *ctx) {
    if (duk_is_callable(ctx, 0) && g_micro_n < MAX_MICRO)
        stash_set(ctx, STASH_MICRO, g_micro_n++, 0);
    return 0;
}
static duk_ret_t nb_raf(duk_context *ctx) {
    if (duk_is_callable(ctx, 0) && g_raf_fires < MAX_RAF_FRAMES) {
        g_raf_fires++;
        timer_schedule(ctx, RAF_MS, 0);
    }
    return 0;
}
/* ---- rung 4: fetch / XHR transport — blocking curl child, Promise-shaped ---- */
/* The JS prelude (nb_host.h) wraps this in a Promise polyfill + fetch() +
 * XMLHttpRequest. Blocking is fine: the drain loop afterwards exhausts the
 * microtask queue, so .then() chains render before RENDER. The manager stays
 * parse-time network owner; the worker is JS-time network owner. */
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

static duk_ret_t nb_fetch_sync(duk_context *ctx) {
    const char *method = duk_require_string(ctx, 0);
    const char *url = duk_require_string(ctx, 1);
    const char *headers = duk_get_string(ctx, 2); if (!headers) headers = "";
    const char *body = duk_get_string(ctx, 3); if (!body) body = "";

    char *rb = NULL; size_t rn = 0; int status = 0; char errbuf[256] = "";

    alarm(0);   /* a blocking curl must never trip the EVAL_BUDGET_SEC watchdog */

    if (strncmp(url, "file:", 5) == 0) {
        /* normalize file://host/path, file:///path, file:/path -> /path */
        const char *p = url + 5;
        while (*p == '/') p++;
        if (strncmp(p, "localhost", 9) == 0 && p[9] == '/') p += 10;
        char abspath[2048];
        snprintf(abspath, sizeof(abspath), "/%s", p);
        if (read_file(abspath, &rb, &rn)) status = 200;
        else snprintf(errbuf, sizeof(errbuf), "cannot read %s", abspath);
    } else if (strncmp(url, "http:", 5) == 0 || strncmp(url, "https:", 6) == 0) {
        char cfgpath[1024] = "", bodypath[1024] = "";
        char t1[] = "/tmp/nbfetch.XXXXXX", t2[] = "/tmp/nbfetchbody.XXXXXX";
        int fd1 = mkstemp(t1), fd2 = mkstemp(t2);
        if (fd1 < 0 || fd2 < 0) { snprintf(errbuf, sizeof(errbuf), "mkstemp failed"); }
        else {
            close(fd1); close(fd2);
            snprintf(cfgpath, sizeof(cfgpath), "%s", t1);
            snprintf(bodypath, sizeof(bodypath), "%s", t2);
            FILE *cf = fopen(cfgpath, "w");
            if (!cf) snprintf(errbuf, sizeof(errbuf), "cannot write curl config");
            else {
                cfg_line(cf, "url", url);
                cfg_line(cf, "user-agent", "Mozilla/5.0 (NNEST network-browser-hq nb-js-worker rung4)");
                cfg_line(cf, "max-time", "8");
                fputs("silent\nlocation\nfail\n", cf);
                /* one header = line per raw "Name: value" line (no strtok_r) */
                for (const char *p = headers; *p; ) {
                    const char *nl = strchr(p, '\n');
                    size_t n = nl ? (size_t)(nl - p) : strlen(p);
                    while (n && (p[n-1] == '\r' || p[n-1] == ' ')) n--;
                    if (n && memchr(p, ':', n)) {
                        char hb[512];
                        if (n >= sizeof(hb)) n = sizeof(hb) - 1;
                        memcpy(hb, p, n); hb[n] = 0;
                        cfg_line(cf, "header", hb);
                    }
                    p += n + (nl ? 1 : 0);
                }
                if (body[0]) cfg_data(cf, body);
                cfg_line(cf, "request", method);
                fputs("output = \"", cf); cfg_put(cf, bodypath); fputs("\"\n", cf);
                fputs("write-out = \"%{http_code}\"\n", cf);
                fclose(cf);
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "curl -sS -K '%s' 2>/dev/null", cfgpath);
                FILE *po = popen(cmd, "r");
                if (po) {
                    char code[16] = ""; size_t got = 0;
                    while (got + 1 < sizeof(code)) {
                        int c = fgetc(po);
                        if (c == EOF) break;
                        code[got++] = (char)c;
                    }
                    code[got] = 0;
                    int rc = pclose(po);
                    status = (int)strtol(code, NULL, 10);
                    if (status <= 0) status = 0;
                    if (status > 0 && read_file(bodypath, &rb, &rn)) {
                        /* treats zero-byte bodies as a successful empty read */
                    }
                    if (!rb) snprintf(errbuf, sizeof(errbuf), "curl rc=%d status=%d", rc, status);
                } else snprintf(errbuf, sizeof(errbuf), "popen curl failed");
                unlink(cfgpath);
                unlink(bodypath);
            }
        }
    } else {
        /* data: URLs can carry small inline blobs; everything else is refused */
        if (strncmp(url, "data:", 5) == 0) {
            const char *c = strchr(url, ',');
            rb = strdup(c ? c + 1 : "");
            rn = rb ? strlen(rb) : 0;
            status = 200;
        } else snprintf(errbuf, sizeof(errbuf), "unsupported scheme in %s", url);
    }

    alarm(EVAL_BUDGET_SEC);   /* re-arm the budget for the rest of the drain */

    duk_push_object(ctx);
    duk_push_boolean(ctx, status >= 200 && status < 300 && rb != NULL);
    duk_put_prop_string(ctx, -2, "ok");
    duk_push_int(ctx, status);
    duk_put_prop_string(ctx, -2, "status");
    duk_push_string(ctx, rb ? rb : "");
    duk_put_prop_string(ctx, -2, "body");
    duk_push_string(ctx, errbuf[0] ? errbuf : "");
    duk_put_prop_string(ctx, -2, "error");
    free(rb);
    return 1;
}

static int drain_microtasks(duk_context *ctx) {
    int ran = 0;
    while (g_micro_head < g_micro_n) {
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        int slot = g_micro_head++;
        stash_push(ctx, STASH_MICRO, slot);
        if (duk_is_callable(ctx, -1)) {
            if (invoke_cb0(ctx)) { g_invocations++; break; }
            g_invocations++; ran = 1;
        } else duk_pop(ctx);
    }
    return ran;
}
static uint64_t timer_min_due(void) {
    uint64_t m = 0; int have = 0;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active) { if (!have || g_timers[i].due < m) { m = g_timers[i].due; have = 1; } }
    return have ? m : 0;
}
static int run_due_timers(duk_context *ctx, uint64_t now) {
    int ran = 0;
    for (int i = 0; i < g_timer_count; i++) {
        if (!g_timers[i].active || g_timers[i].due > now) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        long iv = g_timers[i].interval;
        stash_push(ctx, STASH_TIMER, i);          /* callback on stack */
        if (duk_is_callable(ctx, -1)) {
            if (invoke_cb0(ctx)) { g_invocations++; break; }
            g_invocations++; ran = 1;
        } else duk_pop(ctx);
        if (iv > 0) g_timers[i].due = now + (uint64_t)iv;   /* repeating — re-arm */
        else { g_timers[i].active = 0; stash_del(ctx, STASH_TIMER, i); } /* oneshot */
    }
    return ran;
}

/* ---- events (EventTarget add/removeEventListener; dispatch is commit 8) ---- */
static void evl_add(duk_context *ctx, int kind, NbNode *n) {
    const char *type = duk_get_string(ctx, 0);
    if (!type || !type[0] || !duk_is_callable(ctx, 1) || g_evl_count >= MAX_EVENTS) return;
    int slot = g_evl_count++;
    g_evl[slot].kind = kind; g_evl[slot].node = n; g_evl[slot].slot = slot; g_evl[slot].active = 1;
    snprintf(g_evl[slot].type, sizeof(g_evl[slot].type), "%s", type);
    stash_set(ctx, STASH_EVT, slot, 1);
}
static void evl_del(duk_context *ctx, int kind, NbNode *n) {
    const char *type = duk_get_string(ctx, 0);
    for (int i = 0; i < g_evl_count; i++)
        if (g_evl[i].active && g_evl[i].kind == kind && g_evl[i].node == n &&
            (!type || !type[0] || !strcmp(g_evl[i].type, type))) {
            g_evl[i].active = 0;
            stash_del(ctx, STASH_EVT, i);
            break;
        }
}
static duk_ret_t nb_el_addEventListener(duk_context *ctx)  { evl_add(ctx, EVT_NODE, get_this(ctx)); return 0; }
static duk_ret_t nb_el_removeEventListener(duk_context *ctx) { evl_del(ctx, EVT_NODE, get_this(ctx)); return 0; }
static duk_ret_t nb_doc_addEventListener(duk_context *ctx)  { evl_add(ctx, EVT_DOC, NULL); return 0; }
static duk_ret_t nb_doc_removeEventListener(duk_context *ctx) { evl_del(ctx, EVT_DOC, NULL); return 0; }
static duk_ret_t nb_win_addEventListener(duk_context *ctx)  { evl_add(ctx, EVT_WIN, NULL); return 0; }
static duk_ret_t nb_win_removeEventListener(duk_context *ctx) { evl_del(ctx, EVT_WIN, NULL); return 0; }
static duk_ret_t nb_event_preventDefault(duk_context *ctx) {
    duk_push_this(ctx);
    if (duk_is_object(ctx, -1)) {
        duk_get_prop_string(ctx, -1, "cancelable");
        int can = duk_to_boolean(ctx, -1); duk_pop(ctx);
        if (can) { duk_push_boolean(ctx, 1); duk_put_prop_string(ctx, -2, "defaultPrevented"); }
    }
    duk_pop(ctx);
    return 0;
}
static duk_ret_t nb_event_stopPropagation(duk_context *ctx) {
    duk_push_this(ctx);
    if (duk_is_object(ctx, -1)) { duk_push_boolean(ctx, 1); duk_put_prop_string(ctx, -2, "propagationStopped"); }
    duk_pop(ctx);
    return 0;
}
/* ---- on-* handlers: el.onclick = fn (C-side registry, per kind+node+type).
 * Duktape passes the property key as arg0 to getter/setter natives. */
static int onprop_find(int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_onprop_count; i++) {
        if (!g_onprop[i].active) continue;
        if (g_onprop[i].kind != kind || g_onprop[i].node != n) continue;
        if (strcmp(g_onprop[i].type, type)) continue;
        return i;
    }
    return -1;
}
static const char *onprop_type_from_magic(duk_context *ctx) {
    int idx = duk_get_current_magic(ctx);
    if (idx < 0 || !ONPROPS[idx]) return NULL;
    return ONPROPS[idx];
}
static int onprop_set_core(duk_context *ctx, int kind, NbNode *n) {
    const char *type = onprop_type_from_magic(ctx);
    if (!type || !duk_is_callable(ctx, 0)) return 0;
    int i = onprop_find(kind, n, type);
    if (i < 0) {
        if (g_onprop_count >= MAX_ONPROPS) return 0;
        i = g_onprop_count++;
        g_onprop[i].kind = kind; g_onprop[i].node = n;
        snprintf(g_onprop[i].type, sizeof(g_onprop[i].type), "%s", type);
    }
    g_onprop[i].active = 1;
    g_onprop[i].slot = i;
    stash_set(ctx, STASH_ONPROP, i, 0);
    return 0;
}
static duk_ret_t nb_el_onprop_set(duk_context *ctx)   { return onprop_set_core(ctx, EVT_NODE, get_this(ctx)); }
static duk_ret_t nb_doc_onprop_set(duk_context *ctx)   { return onprop_set_core(ctx, EVT_DOC, NULL); }
static duk_ret_t nb_win_onprop_set(duk_context *ctx)   { return onprop_set_core(ctx, EVT_WIN, NULL); }
static duk_ret_t nb_onprop_get_core(duk_context *ctx, int kind, NbNode *n) {
    const char *type = onprop_type_from_magic(ctx);
    if (!type) { duk_push_undefined(ctx); return 1; }
    int i = onprop_find(kind, n, type);
    if (i < 0 || !g_onprop[i].active) { duk_push_undefined(ctx); return 1; }
    stash_push(ctx, STASH_ONPROP, g_onprop[i].slot);
    return 1;
}
static duk_ret_t nb_el_onprop_get(duk_context *ctx)   { return nb_onprop_get_core(ctx, EVT_NODE, get_this(ctx)); }
static duk_ret_t nb_doc_onprop_get(duk_context *ctx)   { return nb_onprop_get_core(ctx, EVT_DOC, NULL); }
static duk_ret_t nb_win_onprop_get(duk_context *ctx)   { return nb_onprop_get_core(ctx, EVT_WIN, NULL); }
/* one dispatch level for a single node/window/document: exact evl registrations,
 * then the on-* callback. Each target scans the registers from scratch (an
 * entry's kind+node binds it to exactly one target), so bubbling reaches the
 * document/window levels without a shared cursor skipping their listeners. */
static void dispatch_level(duk_context *ctx, int kind, NbNode *node, duk_idx_t ev,
                           const char *type, int *stopped) {
    ev = duk_normalize_index(ctx, ev);
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != node) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        /* Duktape 2.x pcall_method layout: [func][this][arg1..argN], arg ON TOP */
        stash_push(ctx, STASH_EVT, i);                /* cb */
        if (kind == EVT_NODE && node) push_node(ctx, node);
        else duk_get_global_string(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        duk_dup(ctx, ev);                             /* event arg on top */
        if (duk_pcall_method(ctx, 1) != 0) {
            if (!g_pending_err) {
                g_pending_err = 1;
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s",
                         duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx); g_invocations++; return;
        }
        duk_pop(ctx); g_invocations++;
        duk_get_prop_string(ctx, ev, "propagationStopped");
        *stopped = duk_to_boolean(ctx, -1); duk_pop(ctx);
        if (*stopped) return;
    }
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != node) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        stash_push(ctx, STASH_ONPROP, o->slot);       /* cb */
        if (kind == EVT_NODE && node) push_node(ctx, node);
        else duk_get_global_string(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        duk_dup(ctx, ev);                             /* event arg on top */
        if (duk_pcall_method(ctx, 1) != 0) {
            if (!g_pending_err) {
                g_pending_err = 1;
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s",
                         duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx); g_invocations++; return;
        }
        duk_pop(ctx); g_invocations++;
        duk_get_prop_string(ctx, ev, "propagationStopped");
        *stopped = duk_to_boolean(ctx, -1); duk_pop(ctx);
        if (*stopped) return;
    }
}
static int dispatch_event(duk_context *ctx, int kind, NbNode *node, duk_idx_t ev, int bubbles) {
    ev = duk_normalize_index(ctx, ev);
    duk_get_prop_string(ctx, ev, "type");
    const char *type = duk_get_string(ctx, -1);
    duk_pop(ctx);
    if (!type) return 1;
    int stopped = 0;

    if (kind == EVT_NODE && node) {
        push_node(ctx, node);        duk_put_prop_string(ctx, ev, "target");
        push_node(ctx, node);        duk_put_prop_string(ctx, ev, "currentTarget");
        dispatch_level(ctx, EVT_NODE, node, ev, type, &stopped);
        if (!stopped && bubbles) {               /* bubble node->...->root->document->window */
            NbNode *a = node->parent;
            while (a) {
                if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
                push_node(ctx, a);   duk_put_prop_string(ctx, ev, "currentTarget");
                dispatch_level(ctx, EVT_NODE, a, ev, type, &stopped);
                if (stopped) break;
                a = a->parent;
            }
            if (!stopped) {
                duk_get_global_string(ctx, "document"); duk_put_prop_string(ctx, ev, "currentTarget");
                dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
            }
            if (!stopped) {
                duk_get_global_string(ctx, "window");   duk_put_prop_string(ctx, ev, "currentTarget");
                dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
            }
        }
    } else if (kind == EVT_DOC) {
        duk_get_global_string(ctx, "document"); duk_put_prop_string(ctx, ev, "target");
        duk_get_global_string(ctx, "document"); duk_put_prop_string(ctx, ev, "currentTarget");
        dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
        if (!stopped) {
            duk_get_global_string(ctx, "window"); duk_put_prop_string(ctx, ev, "currentTarget");
            dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
        }
    } else {                                       /* EVT_WIN */
        duk_get_global_string(ctx, "window");   duk_put_prop_string(ctx, ev, "target");
        duk_get_global_string(ctx, "window");   duk_put_prop_string(ctx, ev, "currentTarget");
        dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
    }
    duk_get_prop_string(ctx, ev, "defaultPrevented");
    int dp = duk_to_boolean(ctx, -1); duk_pop(ctx);
    return !dp;
}
static duk_ret_t nb_el_dispatchEvent(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (!n) { duk_push_boolean(ctx, 0); return 1; }
    duk_get_prop_string(ctx, 0, "bubbles");
    int bubbles = duk_to_boolean(ctx, -1); duk_pop(ctx);
    duk_push_boolean(ctx, dispatch_event(ctx, EVT_NODE, n, 0, bubbles));
    return 1;
}
static duk_ret_t nb_doc_dispatchEvent(duk_context *ctx) {
    duk_get_prop_string(ctx, 0, "bubbles");
    int bubbles = duk_to_boolean(ctx, -1); duk_pop(ctx);
    duk_push_boolean(ctx, dispatch_event(ctx, EVT_DOC, NULL, 0, bubbles));
    return 1;
}
static duk_ret_t nb_win_dispatchEvent(duk_context *ctx) {
    duk_get_prop_string(ctx, 0, "bubbles");
    int bubbles = duk_to_boolean(ctx, -1); duk_pop(ctx);
    duk_push_boolean(ctx, dispatch_event(ctx, EVT_WIN, NULL, 0, bubbles));
    return 1;
}
static duk_ret_t nb_el_click(duk_context *ctx) {
    NbNode *n = get_this(ctx);
    if (!n) { duk_push_undefined(ctx); return 1; }
    duk_get_global_string(ctx, "Event");
    if (!duk_is_callable(ctx, -1)) { duk_pop(ctx); duk_push_undefined(ctx); return 1; }
    duk_push_string(ctx, "click");
    duk_push_object(ctx);
    duk_push_boolean(ctx, 1); duk_put_prop_string(ctx, -2, "bubbles");
    duk_push_boolean(ctx, 1); duk_put_prop_string(ctx, -2, "cancelable");
    if (duk_pnew(ctx, 2) == 0) {
        duk_push_boolean(ctx, dispatch_event(ctx, EVT_NODE, n, -1, 1));
        return 1;
    }
    duk_pop(ctx); duk_push_undefined(ctx);
    return 1;
}
static void fire_event(duk_context *ctx, int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != n) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        /* Duktape 2.x pcall_method layout is [func][this][arg1..argN], arg ON TOP.
         * Push the cb first (bottom), then `this` (element/document/window), then
         * the Event argument on top -> [cb][this][Event]. */
        stash_push(ctx, STASH_EVT, i);              /* cb (func) at bottom */
        if (kind == EVT_NODE && n) push_node(ctx, n);
        else duk_get_global_string(ctx, kind == EVT_WIN ? "window" : "document");
        duk_push_object(ctx);                       /* minimal Event (argument) on top */
        duk_push_string(ctx, type);      duk_put_prop_string(ctx, -2, "type");
        duk_push_boolean(ctx, 0);        duk_put_prop_string(ctx, -2, "defaultPrevented");
        duk_push_boolean(ctx, 0);        duk_put_prop_string(ctx, -2, "cancelable");
        if (kind == EVT_NODE && n) { push_node(ctx, n); duk_put_prop_string(ctx, -2, "target"); }
        else { duk_get_global_string(ctx, kind == EVT_WIN ? "window" : "document");
               duk_put_prop_string(ctx, -2, "target"); }
        duk_push_c_function(ctx, nb_event_preventDefault, 0); duk_put_prop_string(ctx, -2, "preventDefault");
        duk_push_c_function(ctx, nb_event_stopPropagation, 0); duk_put_prop_string(ctx, -2, "stopPropagation");
        if (duk_pcall_method(ctx, 1) != 0) {
            if (!g_pending_err) {
                g_pending_err = 1;
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s",
                         duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx);
            g_invocations++;
            break;
        }
        duk_pop(ctx);
        g_invocations++;
    }
    /* lifecycle on-* props: window.onload, document.onDOMContentLoaded (commit 8) */
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != n) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        stash_push(ctx, STASH_ONPROP, o->slot);               /* cb at bottom */
        if (kind == EVT_NODE && n) push_node(ctx, n);
        else duk_get_global_string(ctx, kind == EVT_WIN ? "window" : "document"); /* this */
        duk_push_object(ctx);                       /* minimal Event (arg) on top */
        duk_push_string(ctx, type);      duk_put_prop_string(ctx, -2, "type");
        duk_push_boolean(ctx, 0);        duk_put_prop_string(ctx, -2, "defaultPrevented");
        duk_push_boolean(ctx, 0);        duk_put_prop_string(ctx, -2, "cancelable");
        duk_push_c_function(ctx, nb_event_preventDefault, 0); duk_put_prop_string(ctx, -2, "preventDefault");
        duk_push_c_function(ctx, nb_event_stopPropagation, 0); duk_put_prop_string(ctx, -2, "stopPropagation");
        if (duk_pcall_method(ctx, 1) != 0) {
            if (!g_pending_err) {
                g_pending_err = 1;
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s",
                         duk_safe_to_string(ctx, -1));
            }
            duk_pop(ctx);
            g_invocations++;
            break;
        }
        duk_pop(ctx);
        g_invocations++;
    }
}

/* ---- the loop: lifecycle -> microtask/timer drain until quiescent or budget ---- */
/* returns nonzero if an event-loop callback threw (caller -> STATUS err) */
static int run_event_loop(duk_context *ctx) {
    signal(SIGALRM, sigalrm);
    alarm(EVAL_BUDGET_SEC);   /* phase-1 backstop also covers timer/microtask callbacks */
    drain_microtasks(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_DOC, NULL, "DOMContentLoaded");
    drain_microtasks(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_WIN, NULL, "load");
    drain_microtasks(ctx);
    uint64_t start = now_ms();
    for (int guard = 0; guard < 100000 && !g_pending_err; guard++) {
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        if (now_ms() - start > MAX_DRAIN_MS) break;   /* bound page_load wait */
        uint64_t now = now_ms();
        int ran = run_due_timers(ctx, now);
        if (drain_microtasks(ctx)) ran = 1;
        if (!ran) {
            uint64_t m = timer_min_due();
            if (!m) break;                    /* nothing scheduled — quiescent */
            if (m <= now) continue;           /* due but callback skipped? re-drain */
            /* next timer in the future — sleep up to it (bounded by MAX_DRAIN_MS) */
            uint64_t d = m - now;
            if (d > 5) d = 5;
            struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
            nanosleep(&ts, NULL);
            continue;
        }
    }
    alarm(0);
    return g_pending_err;
}

static void install_events_timers(duk_context *ctx) {
    duk_get_global_string(ctx, "document");
    duk_push_c_function(ctx, nb_doc_addEventListener, 2);    duk_put_prop_string(ctx, -2, "addEventListener");
    duk_push_c_function(ctx, nb_doc_removeEventListener, 2); duk_put_prop_string(ctx, -2, "removeEventListener");
    duk_push_c_function(ctx, nb_doc_dispatchEvent, 1);       duk_put_prop_string(ctx, -2, "dispatchEvent");
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        duk_push_string(ctx, onname);
        duk_push_c_function(ctx, nb_doc_onprop_get, 0); duk_set_magic(ctx, -1, i);
        duk_push_c_function(ctx, nb_doc_onprop_set, 1); duk_set_magic(ctx, -1, i);
        duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    }
    duk_pop(ctx);
    duk_push_global_object(ctx);
    duk_push_c_function(ctx, nb_timer_setTimeout, 2);    duk_put_prop_string(ctx, -2, "setTimeout");
    duk_push_c_function(ctx, nb_timer_setInterval, 2);   duk_put_prop_string(ctx, -2, "setInterval");
    duk_push_c_function(ctx, nb_timer_clearTimeout, 1);  duk_put_prop_string(ctx, -2, "clearTimeout");
    duk_push_c_function(ctx, nb_timer_clearInterval, 1); duk_put_prop_string(ctx, -2, "clearInterval");
    duk_push_c_function(ctx, nb_queueMicrotask, 1);      duk_put_prop_string(ctx, -2, "queueMicrotask");
    duk_push_c_function(ctx, nb_raf, 1);                 duk_put_prop_string(ctx, -2, "requestAnimationFrame");
    duk_push_c_function(ctx, nb_fetch_sync, 4);          duk_put_prop_string(ctx, -2, "nbFetchSync");
    duk_push_c_function(ctx, nb_win_addEventListener, 2);    duk_put_prop_string(ctx, -2, "addEventListener");
    duk_push_c_function(ctx, nb_win_removeEventListener, 2); duk_put_prop_string(ctx, -2, "removeEventListener");
    duk_push_c_function(ctx, nb_win_dispatchEvent, 1);       duk_put_prop_string(ctx, -2, "dispatchEvent");
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        duk_push_string(ctx, onname);
        duk_push_c_function(ctx, nb_win_onprop_get, 0); duk_set_magic(ctx, -1, i);
        duk_push_c_function(ctx, nb_win_onprop_set, 1); duk_set_magic(ctx, -1, i);
        duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    }
    duk_pop(ctx);
}

/* plan step 5: CPU budget for script eval. If page.js burns through
 * EVAL_BUDGET_SEC of CPU (while(true) {} and friends) SIGALRM fires while
 * Duktape is running; the deadly default _exit kills the worker mid-eval, the
 * manager sees the socket close and respawns on the next LOAD. */
static void sigalrm(int sig) { _exit(128 + sig); }
static int peval_budget(duk_context *ctx, const char *src) {
    signal(SIGALRM, sigalrm);
    alarm(EVAL_BUDGET_SEC);
    int rc = src ? duk_peval_string(ctx, src) : duk_peval(ctx);
    alarm(0);
    return rc;
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
    /* phase-2 (commit 7): per-page event/timer/microtask state */
    g_timer_count = 0; g_micro_n = 0; g_micro_head = 0; g_evl_count = 0;
    g_onprop_count = 0;
    g_next_id = 1; g_invocations = 0; g_raf_fires = 0;
    g_pending_err = 0; g_pending_errmsg[0] = 0;

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
    } else if (g_cli) {
        /* standalone nbjs: give the page an empty document/body */
        static const char empty_html[] = "<html><body></body></html>";
        g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    }

    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) { dom_teardown(); send_status("STATUS err:heap"); return; }
    install_host(ctx);

    /* rung-6 prelude: swallow its own failure, page continues */
    if (peval_budget(ctx, g_js_prelude) != 0) duk_pop(ctx);
    duk_pop(ctx);

    install_dom(ctx);
    install_events_timers(ctx);

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
    int rc = peval_budget(ctx, NULL);
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
    /* phase-2 (commit 7): lifecycle + event loop — timers/microtasks now fire */
    int ev_err = run_event_loop(ctx);
    if (ev_err) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s",
                 g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
        duk_destroy_heap(ctx);
        dom_teardown();
        send_status(msg);
        return;
    }
    duk_destroy_heap(ctx);

    /* step 4: emit RENDER rows from the post-JS DOM, then STATUS. The
     * manager merges these into page.state.txt so JS mutations show. */
    SB r = {0, 0, 0};
    dom_render_rows(&r);
    if (r.s && r.s[0]) {
        size_t rn = strlen(r.s);
        char *pay = NULL;
        size_t pn = 0;
        if (rn < RENDER_MAX) {
            pn = 7 + rn;                       /* "RENDER\n" + rows */
            pay = malloc(pn + 1);
            if (pay) {
                memcpy(pay, "RENDER\n", 7);
                memcpy(pay + 7, r.s, rn);
                pay[pn] = 0;
                send_payload(pay, pn);
            }
        }
        free(pay);
    }
    free(r.s);

    dom_teardown();
    send_status("STATUS ok");
}

/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon. */
/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon, but
 * `duk -i` forces the REPL even when stdin is piped. */
/* CLI-2/CLI-3 native hook used by both the REPL and node mode. */
static duk_ret_t nb_cjs_read_file(duk_context *ctx);
static void      nb_install_fs(duk_context *ctx);
static const char g_cjs_prelude[];
static int repl_main(void) {
    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) return 1;
    g_cli = 1; g_cli_log = 1;
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);
    install_host(ctx);
    if (peval_budget(ctx, g_js_prelude) != 0) duk_pop(ctx);
    duk_pop(ctx);
    /* CLI-2/CLI-3 in the REPL too: require() + fs work line-by-line,
     * sharing the browser prelude above (window/document still present). */
    duk_push_c_function(ctx, nb_cjs_read_file, 1);
    duk_put_global_string(ctx, "__nb_read_file");
    nb_install_fs(ctx);
    if (peval_budget(ctx, g_cjs_prelude) != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "err:%s\n", m ? m : "loader error");
        duk_pop(ctx); duk_destroy_heap(ctx); dom_teardown(); return 1;
    }
    duk_pop(ctx);
    duk_get_global_string(ctx, "__nb_install_cjs");
    char cwd_buf[PATH_MAX];
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) snprintf(cwd_buf, sizeof(cwd_buf), ".");
    duk_push_string(ctx, cwd_buf);
    duk_push_string(ctx, "[repl]");
    int cinc = 0;
    signal(SIGALRM, sigalrm);
    alarm(EVAL_BUDGET_SEC);
    cinc = duk_pcall(ctx, 2);
    alarm(0);
    if (cinc != 0) {
        fprintf(stderr, "err:%s\n", duk_safe_to_string(ctx, -1));
        duk_pop(ctx); duk_destroy_heap(ctx); dom_teardown(); return 1;
    }
    duk_pop(ctx);
    static const char empty_html[] = "<html><body></body></html>";
    g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    install_dom(ctx);
    install_events_timers(ctx);

    int tty_out = isatty(STDOUT_FILENO);
    char line[8192];
    if (tty_out) { printf("nbjs (duk) — type JS; Ctrl-D or 'exit' to quit\n"); fflush(stdout); }
    for (;;) {
        if (tty_out) { fputs("> ", stdout); fflush(stdout); }
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t ln = strlen(line);
        while (ln && (line[ln - 1] == '\n' || line[ln - 1] == '\r')) line[--ln] = 0;
        if (!ln) continue;
        if (!strcmp(line, "exit") || !strcmp(line, "quit") || !strcmp(line, ".exit")) break;

        /* don't leak listeners/timers across lines */
        g_timer_count = 0; g_micro_n = 0; g_micro_head = 0; g_evl_count = 0;
        g_onprop_count = 0; g_invocations = 0; g_raf_fires = 0;
        g_pending_err = 0; g_pending_errmsg[0] = 0;

        int rc = peval_budget(ctx, line);
        if (rc != 0) {
            const char *m = duk_safe_to_string(ctx, -1);
            printf("err:%s\n", m ? m : "script error");
        } else if (duk_get_type(ctx, -1) != DUK_TYPE_UNDEFINED) {
            const char *s = duk_safe_to_string(ctx, -1);
            printf("%s\n", s ? s : "");
        }
        duk_pop(ctx);
        fflush(stdout);

        /* drain microtasks + timers (no lifecycle events), CPU-bounded */
        signal(SIGALRM, sigalrm);
        alarm(EVAL_BUDGET_SEC);
        uint64_t start = now_ms();
        for (int guard = 0; guard < 10000 && !g_pending_err; guard++) {
            if (now_ms() - start > 200) break;
            uint64_t now = now_ms();
            int ran = run_due_timers(ctx, now);
            if (drain_microtasks(ctx)) ran = 1;
            if (!ran) {
                uint64_t m = timer_min_due();
                if (!m) break;
                if (m <= now) continue;
                uint64_t d = m - now; if (d > 5) d = 5;
                struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
                nanosleep(&ts, NULL);
            }
        }
        alarm(0);
    }
    duk_destroy_heap(ctx);
    dom_teardown();
    return 0;
}

/* ---- CLI-1: node-like runner (nbjs file.js [args...]) ---- */
static duk_ret_t nb_cli_stdout(duk_context *ctx) {
    const char *s = duk_safe_to_string(ctx, 0);
    if (g_out) { fputs(s ? s : "", g_out); fflush(g_out); }
    return 0;
}
static duk_ret_t nb_cli_stderr(duk_context *ctx) {
    const char *s = duk_safe_to_string(ctx, 0);
    if (s) { fputs(s, stderr); fflush(stderr); }
    return 0;
}
/* console.error -> stderr (node parity; log/info/warn stay on stdout) */
static duk_ret_t nb_cli_error(duk_context *ctx) {
    duk_idx_t n = duk_get_top(ctx);
    for (duk_idx_t i = 0; i < n; i++) {
        const char *s = duk_safe_to_string(ctx, i);
        if (s) fputs(s, stderr);
        if (i + 1 < n) fputs(" ", stderr);
    }
    fputs("\n", stderr);
    fflush(stderr);
    return 0;
}
/* CLI-2: read a module file for require(). Returns the source string, or
 * undefined if the path is missing/unreadable (loader turns that into a
 * "Cannot find module" error). Same 512 kB cap as the page loader. */
static duk_ret_t nb_cjs_read_file(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    char *s = NULL; size_t n = 0;
    if (p && p[0] && read_file(p, &s, &n)) {
        duk_push_lstring(ctx, s, n);
        free(s);
        return 1;
    }
    duk_push_undefined(ctx);
    return 1;
}
static duk_ret_t nb_cli_exit(duk_context *ctx) {
    int code = 0;
    if (duk_get_top(ctx) > 0 && duk_is_number(ctx, 0)) code = (int)duk_get_int(ctx, 0);
    if (g_out) fflush(g_out);
    duk_destroy_heap(ctx);
    exit(code);
    return 0;
}
static duk_ret_t nb_cli_cwd(duk_context *ctx) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) duk_push_string(ctx, buf);
    else duk_push_string(ctx, "/");
    return 1;
}
/* ---- CLI-3: fs-lite natives (sync-only, over the same read_file cap).
 * String-only payloads (no Buffer type in this Duktape); an optional
 * encoding arg is accepted so node-style call sites keep working. */
static duk_ret_t nb_fs_readfile(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    char *s = NULL; size_t n = 0;
    if (!p || !p[0] || !read_file(p, &s, &n))
        return duk_error(ctx, DUK_ERR_ERROR, "ENOENT: cannot read '%s'", p ? p : "(empty)");
    duk_push_lstring(ctx, s, n);
    free(s);
    return 1;
}
static duk_ret_t nb_fs_writefile(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    const char *d = duk_safe_to_string(ctx, 1);
    FILE *f = fopen(p, "wb");
    if (!f) return duk_error(ctx, DUK_ERR_ERROR, "EIO: cannot write '%s'", p ? p : "(empty)");
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    return 0;
}
static duk_ret_t nb_fs_appendfile(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    const char *d = duk_safe_to_string(ctx, 1);
    FILE *f = fopen(p, "ab");
    if (!f) return duk_error(ctx, DUK_ERR_ERROR, "EIO: cannot append '%s'", p ? p : "(empty)");
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    return 0;
}
static duk_ret_t nb_fs_exists(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    duk_push_boolean(ctx, p && p[0] && access(p, F_OK) == 0);
    return 1;
}
static duk_ret_t nb_fs_mkdir(duk_context *ctx) {
    const char *p = duk_safe_to_string(ctx, 0);
    if (!p || !p[0]) return duk_error(ctx, DUK_ERR_ERROR, "EINVAL: empty mkdir path");
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", p);
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
    return 0;
}
static void nb_install_fs(duk_context *ctx) {
    duk_push_object(ctx);                     /* fs (abs index 0) */
    duk_push_c_function(ctx, nb_fs_readfile, 1);
    duk_put_prop_string(ctx, 0, "readFileSync");
    duk_push_c_function(ctx, nb_fs_writefile, 2);
    duk_put_prop_string(ctx, 0, "writeFileSync");
    duk_push_c_function(ctx, nb_fs_appendfile, 2);
    duk_put_prop_string(ctx, 0, "appendFileSync");
    duk_push_c_function(ctx, nb_fs_exists, 1);
    duk_put_prop_string(ctx, 0, "existsSync");
    duk_push_c_function(ctx, nb_fs_mkdir, 1);
    duk_put_prop_string(ctx, 0, "mkdirSync");
    duk_push_global_object(ctx);              /* [fs, G] */
    duk_dup(ctx, -2);                         /* [fs, G, fs] */
    duk_put_prop_string(ctx, -2, "__nb_fs");  /* G.__nb_fs = fs; -> [fs, G] */
    duk_pop_2(ctx);
}
/* build a JS object from the process environment (not the full sys env —
 * see getenv below). Env exposure is opt-in via require('os')-free helper;
 * CLI-1 keeps it simple: expose a snapshot under process.env. */
static void nb_cli_install(duk_context *ctx, int argc, char **argv) {
    duk_push_object(ctx);                    /* process (abs index 0) */
    /* argv — node convention: [interpreter, script, args...] */
    duk_idx_t argv_arr = duk_push_array(ctx);
    for (int i = 0; i < argc; i++) {
        duk_push_string(ctx, argv[i]);
        duk_put_prop_index(ctx, argv_arr, (duk_uarridx_t)i);
    }
    duk_put_prop_string(ctx, 0, "argv");     /* process.argv = [strings] */

    /* env (snapshot of environ, ENAME="value" pairs) */
    duk_idx_t env_obj = duk_push_object(ctx);   /* abs index 1 */
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq) continue;
        char *k = strndup(*e, (size_t)(eq - *e));
        if (k) { duk_push_string(ctx, k); duk_push_string(ctx, eq + 1); duk_put_prop(ctx, env_obj); free(k); }
    }
    duk_put_prop_string(ctx, 0, "env");      /* process.env = {...} */
    duk_push_c_function(ctx, nb_cli_cwd, 0);
    duk_put_prop_string(ctx, 0, "cwd");      /* process.cwd = fn */
    /* stdout / stderr — each a small object with write() */
    duk_push_object(ctx);                    /* abs index 1 */
    duk_push_c_function(ctx, nb_cli_stdout, DUK_VARARGS);
    duk_put_prop_string(ctx, 1, "write");
    duk_put_prop_string(ctx, 0, "stdout");
    duk_push_object(ctx);                    /* abs index 1 */
    duk_push_c_function(ctx, nb_cli_stderr, DUK_VARARGS);
    duk_put_prop_string(ctx, 1, "write");
    duk_put_prop_string(ctx, 0, "stderr");
    duk_push_c_function(ctx, nb_cli_exit, DUK_VARARGS);
    duk_put_prop_string(ctx, 0, "exit");     /* process.exit = fn */

    /* expose as global `process` */
    duk_push_global_object(ctx);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "process");
    duk_pop_2(ctx);
}

/* The released browser page runner: duk --browser page.js [fetch.dom]
 * runs the full DOM engine (tree, events+timer loop, fetch/XHR+Promise,
 * render-back) with rendered rows -> stdout. exit 0 ok / 1 js err / 2 usage.
 * Same code path the old `duk page.js` default took before --node/--browser. */
static int browser_cli_main(int argc, char **argv) {
    g_cli = 1;
    g_cli_log = 1;   /* bare console lines, no LOG| prefix */
    if (!g_out) { g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0); }
    const char *pg = argv[1];
    if (argc < 2 || strcmp(pg, "-h") == 0 || strcmp(pg, "--help") == 0) {
        fprintf(stderr, "usage: duk --browser <page.js> [fetch.dom]\n");
        return 2;
    }
    const char *base = strrchr(pg, '/');
    snprintf(g_title, sizeof(g_title), "%s", base ? base + 1 : pg);
    snprintf(g_href, sizeof(g_href), "file://%s", pg);
    snprintf(g_page_js, sizeof(g_page_js), "%s", pg);
    if (argc > 2) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", argv[2]);
    if (access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }
    run_page();
    return g_cli_status_ok ? 0 : 1;
}

/* CLI-2: CommonJS module loader, installed in node mode before the entry
 * script runs. Pure ES5.1 JS so it works on Duktape 2.7.0 (no arrows/let):
 * relative/absolute resolution against the caller's directory, per-file
 * module/exports wrapper (module-local var scoping), JSON require, and the
 * standard circular-require rule (partial exports served on re-entry via
 * the pre-populated cache). The only host hook is `__nb_read_file` (see
 * nb_cjs_read_file). `this` is the global object by design. */
static const char g_cjs_prelude[] =
"/* NB-JS CJS loader (CLI-2): require/module/exports, JSON, cycles. */\n"
"(function(){\n"
"var cache = {};\n"
"function dirname(p){\n"
"  var i = p.lastIndexOf('/');\n"
"  if (i <= 0) return '/';\n"
"  return p.slice(0, i);\n"
"}\n"
"function resolve(req, dir){\n"
"  if (req.charAt(0) === '/') return req;\n"
"  var two = req.slice(0, 2);\n"
"  if (two !== './' && two !== '..' && req.slice(0, 3) !== '../') return null;\n"
"  var base = (dir === '/' ? '' : dir).replace(/\\/+$/, '');\n"
"  var parts = (base + '/' + req).split('/');\n"
"  var out = [];\n"
"  for (var i = 0; i < parts.length; i++){\n"
"    var p = parts[i];\n"
"    if (p === '' || p === '.') continue;\n"
"    if (p === '..'){ if (out.length) out.pop(); else return null; continue; }\n"
"    out.push(p);\n"
"  }\n"
"  return '/' + out.join('/');\n"
"}\n"
"function makeRequire(dir){\n"
"  return function(request){\n"
"    if (request === 'fs'){ if (typeof __nb_fs !== 'undefined') return __nb_fs; }\n"
"    var abs = resolve(request, dir);\n"
"    if (abs === null)\n"
"      throw new Error(\"Cannot find module '\" + request + \"' (nbjs has no packages/builtins)\");\n"
"    if (abs in cache) return cache[abs].exports;\n"
"    var src = __nb_read_file(abs);\n"
"    if (src === undefined || src === null)\n"
"      throw new Error(\"Cannot find module '\" + request + \"' (resolved to \" + abs + \")\");\n"
"    var mod = { id: abs, filename: abs, exports: {}, loaded: false };\n"
"    cache[abs] = mod;\n"
"    if (abs.slice(-5) === '.json'){\n"
"      mod.exports = JSON.parse(src);\n"
"      mod.loaded = true;\n"
"      return mod.exports;\n"
"    }\n"
"    if (src.charAt(0) === '#'){\n"
"      var nl = src.indexOf('\\n');\n"
"      if (nl >= 0) src = src.slice(nl + 1); else src = '';\n"
"    }\n"
"    var rd = dirname(abs);\n"
"    var fn = new Function('exports', 'require', 'module', '__filename', '__dirname', src);\n"
"    fn.call(mod.exports, mod.exports, makeRequire(rd), mod, abs, rd);\n"
"    mod.loaded = true;\n"
"    return mod.exports;\n"
"  };\n"
"}\n"
"this.__nb_install_cjs = function(entryDir, entryFile){\n"
"  this.require = makeRequire(entryDir);\n"
"  this.__dirname = entryDir;\n"
"  this.__filename = entryFile;\n"
"};\n"
"})();\n";

static int cli_main(int argc, char **argv) {
    g_cli = 1; g_cli_log = 1;         /* bare console lines -> stdout */
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);

    /* parse: duk [--node] file.js|-- [args...] */
    int script_i = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require() + fs)\n");
            fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
            fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
            return 2;
        }
        if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) continue;
        script_i = i; break;
    }
    if (script_i < 0) {
        fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require() + fs)\n");
        fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
        fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
        return 2;
    }
    const char *pg = argv[script_i];
    if (strcmp(pg, "-") != 0 && access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }

    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) return 1;

    /* node-like host: console/print only, plus process. No DOM, no events,
     * no timers, no browser prelude — window/document/location are absent. */
    duk_push_global_object(ctx);
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "print");
    duk_push_object(ctx);
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "log");
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "info");
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "warn");
    duk_push_c_function(ctx, nb_cli_error, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "error");
    duk_put_prop_string(ctx, -2, "console");
    duk_pop(ctx);

    /* process.argv = [interp, script, args...] (node convention) */
    {
        int nargv = 1 + (argc - script_i);
        char **a = calloc((size_t)nargv, sizeof(char *));
        if (!a) { duk_destroy_heap(ctx); return 1; }
        a[0] = argv[0];
        for (int i = 0; i + script_i < argc; i++) a[i + 1] = argv[script_i + i];
        nb_cli_install(ctx, nargv, a);
        free(a);
    }

    /* CLI-2 CommonJS + CLI-3 fs: the only host hook the JS loader needs is
     * a module file reader; it defines `require`/`__dirname`/`__filename`
     * itself. `require('fs')` resolves to the fs-lite natives below. */
    duk_push_c_function(ctx, nb_cjs_read_file, 1);
    duk_put_global_string(ctx, "__nb_read_file");
    nb_install_fs(ctx);

    /* Entry directory/file, absolute, for the entry script's require base. */
    static char entry_dir[PATH_MAX], entry_file[PATH_MAX];
    {
        if (strcmp(pg, "-") == 0) {
            if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
            snprintf(entry_file, sizeof(entry_file), "[stdin]");
        } else {
            static char rp[PATH_MAX];
            if (!realpath(pg, rp)) snprintf(rp, sizeof(rp), "%s", pg);
            const char *abs = (rp[0] == '/' ? rp : pg);
            snprintf(entry_file, sizeof(entry_file), "%s", abs);
            const char *sl = strrchr(abs, '/');
            if (sl == abs) snprintf(entry_dir, sizeof(entry_dir), "/");
            else if (sl) { size_t d = (size_t)(sl - abs); memcpy(entry_dir, abs, d); entry_dir[d] = 0; }
            else if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
        }
    }
    if (peval_budget(ctx, g_cjs_prelude) != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "%s\n", m ? m : "loader error");
        duk_destroy_heap(ctx);
        return 1;
    }
    duk_pop(ctx);
    duk_get_global_string(ctx, "__nb_install_cjs");
    duk_push_string(ctx, entry_dir);
    duk_push_string(ctx, entry_file);
    int lrc;
    signal(SIGALRM, sigalrm);
    alarm(EVAL_BUDGET_SEC);
    lrc = duk_pcall(ctx, 2);
    alarm(0);
    if (lrc != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "%s\n", m ? m : "loader error");
        duk_destroy_heap(ctx);
        return 1;
    }
    duk_pop(ctx);

    char *src = NULL; size_t n = 0;
    if (strcmp(pg, "-") == 0) {
        /* read the whole script from stdin */
        {
            size_t cap = 1 << 16, len = 0;
            char *b = malloc(cap);
            if (!b) { duk_destroy_heap(ctx); return 1; }
            for (;;) {
                size_t got = fread(b + len, 1, cap - len, stdin);
                len += got;
                if (len == cap) { cap *= 2; b = realloc(b, cap); if (!b) { duk_destroy_heap(ctx); return 1; } }
                if (feof(stdin) || got == 0) break;
            }
            b[len] = 0; src = b; n = len;
        }
    } else if (!read_file(pg, &src, &n)) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        duk_destroy_heap(ctx);
        return 2;
    }

    /* Compile with shebang support, then call the module body. */
    duk_push_string(ctx, pg);   /* filename arg (stack shape: [filename]) */
    int rc = duk_pcompile_lstring_filename(ctx, DUK_COMPILE_SHEBANG, src, n);
    free(src);
    if (rc != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "%s\n", m ? m : "script error");
        duk_destroy_heap(ctx);
        return 1;
    }
    /* CPU guard: a while(true){} script must be killed, same guarantee as
     * the page runner. sigalrm() hard-exits the worker; in CLI mode that's
     * a clean-enough "runaway script" stop (exit via signal). */
    int rc2;
    signal(SIGALRM, sigalrm);
    alarm(EVAL_BUDGET_SEC);
    rc2 = duk_pcall(ctx, 0);
    alarm(0);
    if (rc2 != 0) {
        const char *m = duk_safe_to_string(ctx, -1);
        fprintf(stderr, "%s\n", m ? m : "script error");
        duk_destroy_heap(ctx);
        return 1;
    }
    duk_pop(ctx);
    if (g_out) fflush(g_out);
    duk_destroy_heap(ctx);
    return 0;
}

int main(int argc, char **argv) {
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */
    {
        const char *nbw_out = getenv("NBW_CONSOLE");
        if (nbw_out && nbw_out[0]) {
            g_out = fopen(nbw_out, "w");
            if (g_out) setvbuf(g_out, NULL, _IOLBF, 0);   /* console capture (debug/tests) */
        }
    }

    /* duk -i / --interactive: force the REPL even when stdin is piped
     * (bare `duk` also reaches it on a tty via the isatty check below). */
    if (argc > 1 && (strcmp(argv[1], "-i") == 0 || strcmp(argv[1], "--interactive") == 0))
        return repl_main();

    if (argc > 1) {
        /* duk --browser page.js [fetch.dom] — the released DOM page runner
         * (full DOM engine + render-back). duk file.js — node mode, no
         * browser globals, process/console + require()/fs host. Either way:
         * exit 0 clean / 1 thrown error / 2 usage. No RPC framing, no khtpm
         * dependency. */
        if (strcmp(argv[1], "--browser") == 0 || strcmp(argv[1], "-b") == 0)
            return browser_cli_main(argc - 1, argv + 1);
        return cli_main(argc, argv);
    }

    /* bare `duk` with a terminal as stdin: interactive REPL instead of the
     * framed daemon (the manager spawns over a socketpair -> not a tty). */
    if (isatty(STDIN_FILENO)) return repl_main();

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
