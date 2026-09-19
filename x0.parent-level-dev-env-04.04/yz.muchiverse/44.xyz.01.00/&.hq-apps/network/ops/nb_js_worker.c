#define _POSIX_C_SOURCE 200809L
/* nb_js_worker.c — resident JavaScript worker for network-browser-hq.
 * Owned by network_browser_manager (its direct child, spawned lazily on
 * first <script> presence), talking line-RPC over a socketpair dup2'd to
 * stdin/stdout. NB-JS worker plan §1/§2/§4.
 *
 * This is the step-2 SKELETON: it creates one QuickJS runtime/context, reuses the
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
#include "../nb_css.h"
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
static char g_style_css[4096];   /* rung 7: LOAD-delivered stylesheet file */
static NbCss *g_css = NULL;      /* parsed rule cache, per LOAD */
static char g_href[4096];
static char g_title[512];

static int g_cli = 0;            /* argv mode: plain text out, no RPC framing */
static int g_cli_status_ok = 0;  /* CLI exit-status latch set by send_status */
static int g_nav_emit = 0;       /* rung-6 slice 2: daemon only (!g_cli, set */
                                 /* per run_page) - NAV frames go to manager */

/* devtools console EVAL: the last LOAD's QuickJS context + DOM tree stay live
 * (g_live_ctx / g_dom_root) between LOAD and EVAL, so an eval:<js> snippet
 * runs against the real page context (document, window, globals, timer
 * bindings) and can mutate the DOM. Tear down only at the next LOAD or on
 * QUIT/error - see run_page() and live_teardown(). */
static JSContext *g_live_ctx = NULL;
static JSRuntime *g_live_rt  = NULL;

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
 * rebuilds the NbNode tree here and exposes it to JS via native QuickJS
 * accessors (plan §7 step 3, roadmap §2 minimum API). The JS side only
 * holds opaque pointer handles; the tree is C-side. No shared memory. */

static NbNode *g_dom_root = NULL;  /* current page's DOM tree (#document) */
static NbNode *g_orphans = NULL;   /* detached createElement() nodes still to free */
#define NODEKEY "_nbnode"

/* JS handles are a plain numeric index into g_nodeindex (index -> NbNode*),
 * which is more robust than round-tripping a JSValue pointer object and
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
/* rung-2 remainder: insert `newn` before `refn` (a child of `parent`, or
 * NULL to append), like DOM insertBefore. Local append when refn is NULL. */
static void local_insert_before(NbNode *parent, NbNode *newn, NbNode *refn) {
    newn->parent = parent;
    if (!refn) { local_append(parent, newn); return; }
    newn->next_sibling = refn;
    NbNode *prev = NULL;
    for (NbNode *c = parent->first_child; c; c = c->next_sibling) {
        if (c == refn) break;
        prev = c;
    }
    if (prev) prev->next_sibling = newn;
    else parent->first_child = newn;
}
static int is_child_of(NbNode *parent, NbNode *ch) {
    if (!parent || !ch) return 0;
    for (NbNode *c = parent->first_child; c; c = c->next_sibling)
        if (c == ch) return 1;
    return 0;
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
    if (!n->tag || !n->tag[0]) { sb_put(b, n->text); return; }   /* #text node */
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
static void resolve_doc_url(const char *rel, char *out, size_t olen);   /* defined below (fetch/rung 4) */
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
            /* D4 2026-09-11: emit MEDIA|I|resolved|alt so collect_page_media
             * in the manager runs the same fetch->nb_media_to_sprite->sprite
             * pass it runs for static pages. rw_row() strips '|' (its LINK/IMG
             * rows are space-joined single fields), so this row is built with
             * sb_put() directly to preserve the pipe-separated MEDIA wire
             * format that collect_page_media() parses. */
            char rs[2048];
            resolve_doc_url(srcbuf, rs, sizeof(rs));
            char altb[512];
            for (size_t i = 0; altbuf[i] && i + 1 < sizeof(altb); i++) {
                char c = altbuf[i];
                if (c == '\r') continue;
                altb[i] = (c == '\n' || c == '|') ? ' ' : c;
                altb[i + 1] = 0;
            }
            if (b->len < RENDER_MAX) {
                sb_put(b, "MEDIA|I|");
                sb_put(b, rs);
                sb_put(b, "|");
                if (altb[0]) sb_put(b, altb);
                sb_put(b, "\n");
            }
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
static NbNode *get_node(JSContext *ctx, JSValueConst v) {
    if (!JS_IsObject(v)) return NULL;
    JSValue k = JS_GetPropertyStr(ctx, v, NODEKEY);
    int i = -1;
    if (JS_IsNumber(k)) {
        int32_t iv;
        if (JS_ToInt32(ctx, &iv, k) == 0) i = (int)iv;
    }
    JS_FreeValue(ctx, k);
    if (i < 0 || i >= g_nodecount) return NULL;
    return g_nodeindex[i];
}
/* Node from the `this` binding of an element native (QuickJS passes the
 * this-binding in the native's JSValueConst this_val argument). */
static NbNode *get_this(JSContext *ctx, JSValueConst this_val) {
    return get_node(ctx, this_val);
}
/* String arg i as own C string (only for string-typed args — the call sites
 * previously used duk_get_string + a "" fallback), or NULL when absent. */
static const char *js_arg_str(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc || !JS_IsString(argv[i])) return NULL;
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned;
}
/* String arg i coercing any value via toString (duk_safe_to_string), "" when
 * absent. */
static const char *js_arg_str_any(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc) return "";
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned ? *owned : "";
}
/* Numeric arg i (numbers only), or -1 when absent/non-number. */
static int js_arg_index(JSContext *ctx, JSValueConst *argv, int argc, int i) {
    if (i >= argc || !JS_IsNumber(argv[i])) return -1;
    int32_t iv;
    if (JS_ToInt32(ctx, &iv, argv[i]) != 0) return -1;
    return (int)iv;
}
/* el.on<name> accessor names (also parsed from property arg0 of the natives) */
static const char *const ONPROPS[] = {
    "click", "dblclick", "change", "input", "submit", "keydown", "keyup",
    "keypress", "mouseover", "mouseout", "mouseenter", "mouseleave",
    "mousedown", "mouseup", "mousemove", "focus", "blur", "load", "error",
    "resize", "scroll", "contextmenu", NULL
};

/* ---- rung 7 slice 1: CSS/layout-intent ---------------------------------
 * The page's stylesheet text (inline <style> + linked CSS, manager-side)
 * reaches the worker via the LOAD style file; g_css holds the parsed rule
 * cache for this page. We model exactly: display: none, visibility:
 * hidden/collapse, opacity, and px width/height — enough for
 * getComputedStyle, display:none-driven hiding, and the offset/rect
 * family to behave on real widgets. There is no real layout engine: x/y
 * are always 0, sizes come from the CSS declaration or 0. */

#define STYK "_nbsty"   /* per-wrapper style-snapshot cache key */

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static int css_hidden(const NbNode *n);

static int css_hidden(const NbNode *n) {
    return g_css && nb_css_hidden(g_css, n);
}
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Return (as JSValue) an object with the modeled computed style of `n` plus a
 * getPropertyValue(). Missing display => "" (visible), opacity => "1". */
static JSValue push_computed_style(JSContext *ctx, NbNode *n) {
    JSValue o = JS_NewObject(ctx);
    NbCssStyle st;
    memset(&st, 0, sizeof(st));
    if (n) nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    JS_SetPropertyStr(ctx, o, "display", JS_NewString(ctx, st.display[0] ? st.display : ""));
    JS_SetPropertyStr(ctx, o, "visibility", JS_NewString(ctx, st.visibility));
    JS_SetPropertyStr(ctx, o, "opacity", JS_NewString(ctx, st.opacity[0] ? st.opacity : "1"));
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, st.width));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, st.height));
    /* font-size is read as a CSS string (kevlar: fontSize.replace('px','')) */
    JS_SetPropertyStr(ctx, o, "fontSize", JS_NewString(ctx, "16px"));
    JS_SetPropertyStr(ctx, o, "getPropertyValue",
                      JS_NewCFunction(ctx, nb_css_getprop, "getPropertyValue", 1));
    return o;
}

static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!name) return JS_NewString(ctx, "");
    JSValue v = JS_GetPropertyStr(ctx, this_val, name);
    JS_FreeCString(ctx, name);
    return JS_ToString(ctx, v);
}

/* window.getComputedStyle(el) — registered globally as `__nb_ges` by the
 * resident worker (overriding install_host's minimal default); the host
 * prelude's getComputedStyle delegates here. */
static JSValue nb_ges_rich(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_node(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    return push_computed_style(ctx, n);
}

/* el.style — identity-cached snapshot of the inline style attribute.
 * Reads mirror the declared inline props; JS writes persist on the
 * snapshot (browser-like authoring) but — no layout engine — do not feed
 * the metrics below. Non-enumerable cache prop keeps it per-wrapper. */
static JSValue nb_el_style_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue cached = JS_GetPropertyStr(ctx, this_val, STYK);
    if (JS_IsObject(cached)) return cached;         /* identity-cached snapshot */
    JS_FreeValue(ctx, cached);
    JSValue o = JS_NewObject(ctx);                  /* style snapshot */
    NbNode *n = get_this(ctx, this_val);
    if (n) {
        const char *attr = nb_attr_get(n, "style");
        if (attr && *attr) {
            const char *p = attr;
            while (*p) {
                while (*p && (*p == ';' || *p == ' ' || *p == '\t' || *p == '\n')) p++;
                if (!*p) break;
                const char *cs = p;
                while (*p && *p != ';') p++;
                const char *colon = memchr(cs, ':', (size_t)(p - cs));
                if (colon && p > cs) {
                    size_t kl = (size_t)(colon - cs);
                    while (kl && (cs[kl - 1] == ' ' || cs[kl - 1] == '\t')) kl--;
                    if (kl > 0 && kl < 48) {
                        char kbuf[48];
                        for (size_t i = 0; i < kl; i++)
                            kbuf[i] = (char)tolower((unsigned char)cs[i]);
                        kbuf[kl] = 0;
                        const char *vs = colon + 1;
                        size_t vn = (size_t)(p - vs);
                        while (vn && (vs[vn - 1] == ' ' || vs[vn - 1] == '\t' ||
                                      vs[vn - 1] == '\n')) vn--;
                        char vbuf[96];
                        size_t vv = vn < sizeof(vbuf) - 1 ? vn : sizeof(vbuf) - 1;
                        memcpy(vbuf, vs, vv);
                        vbuf[vv] = 0;
                        JS_SetPropertyStr(ctx, o, kbuf, JS_NewString(ctx, vbuf));
                    }
                }
            }
        }
    }
    JS_SetPropertyStr(ctx, this_val, STYK, JS_DupValue(ctx, o));
    return o;
}

/* offsetWidth/offsetHeight/clientWidth/clientHeight — magic 0=width,1=height.
 * Hidden elements measure 0 (browser display:none semantics); visible
 * elements report the modeled CSS px size, else 0 (no layout engine). */
static JSValue nb_el_offdim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    NbNode *n = get_this(ctx, this_val);
    int which = magic;
    double v = 0;
    if (n && !css_hidden(n)) {
        NbCssStyle st;
        nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
        v = which ? st.height : st.width;
    }
    return JS_NewFloat64(ctx, v);
}

static JSValue nb_el_offset_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || css_hidden(n)) return JS_NULL;
    NbNode *p = n->parent;
    while (p && !p->tag) p = p->parent;   /* climb past #document */
    if (!p) return JS_NULL;
    return push_node(ctx, p);
}

static JSValue nb_rect_tojson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_DupValue(ctx, this_val);    /* this is the rect */
}

static JSValue nb_el_getBoundingClientRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    double w = 0, h = 0;
    if (n && !css_hidden(n)) {
        NbCssStyle st;
        nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
        w = st.width;
        h = st.height;
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "x", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, o, "y", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, o, "top", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, o, "right", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "bottom", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, o, "left", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, o, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    return o;
}

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* canvas 2D natives defined with the other DOM natives (getBoundingClientRect
 * already ships on every wrapper via the existing definition above) */
static JSValue nb_el_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_canvas_toDataURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

/* ---- document natives ---- */
static JSValue nb_dom_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *id = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!id || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static void collect_tag_into(JSContext *ctx, NbNode *n, const char *tag, JSValue arr, int *i) {
    if (!n) return;
    if (!tag || !*tag || !strcmp(tag, "*") || (n->tag && !strcasecmp(n->tag, tag))) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_tag_into(ctx, (NbNode *)c, tag, arr, i);
}
static JSValue nb_dom_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            collect_tag_into(ctx, (NbNode *)c, tag, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    return arr;
}
static void qsa_into(JSContext *ctx, NbNode *n, const char *sel, JSValue arr, int *i) {
    if (!n) return;
    if (match_any_selector(n, sel)) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        qsa_into(ctx, (NbNode *)c, sel, arr, i);
}
static JSValue nb_dom_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!sel || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static JSValue nb_dom_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root && sel) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            qsa_into(ctx, (NbNode *)c, sel, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    return arr;
}
static JSValue nb_dom_createElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->tag = strdup(tag);
    for (char *t = n->tag; *t; t++) *t = (char)((*t >= 'A' && *t <= 'Z') ? *t + 32 : *t);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    return push_node(ctx, n);
}
static JSValue nb_dom_createElementNS(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* Our element tree is flat (no namespace tracking), so a namespace +
     * qualified name collapses to the plain tag — same node the browser
     * would build for e.g. an SVG <svg>. Matches createElement semantics;
     * real bundles (web-animations-lite calls createElementNS during load)
     * only need the returned element to exist and accept method calls. */
    char *owned = NULL;
    const char *tag = (argc > 1 && JS_IsString(argv[1])) ? (owned = JS_ToCString(ctx, argv[1])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->tag = strdup(tag);
    for (char *t = n->tag; *t; t++) *t = (char)((*t >= 'A' && *t <= 'Z') ? *t + 32 : *t);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    return push_node(ctx, n);
}
static JSValue nb_dom_documentElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "html") : NULL;
    if (el) return push_node(ctx, el);
    return JS_NULL;
}
static JSValue nb_dom_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "body") : NULL;
    if (el) return push_node(ctx, el);
    return JS_NULL;
}
/* rung-2 remainder: document.createTextNode / getElementsByClassName /
 * document.head. The parser skips <head> wholesale, so browsers' implicit
 * empty <head> is created on first access (stays out of the render path). */
static JSValue nb_dom_createTextNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->text = strdup(v);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    return push_node(ctx, n);
}
static void collect_cls_into(JSContext *ctx, NbNode *n, const char *tok, JSValue arr, int *i) {
    if (!n) return;
    if (n->tag && n->tag[0] && has_class(n, tok)) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_cls_into(ctx, (NbNode *)c, tok, arr, i);
}
static JSValue nb_dom_getElementsByClassName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root && tok && *tok) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            collect_cls_into(ctx, (NbNode *)c, tok, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    return arr;
}
static JSValue nb_dom_head(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_dom_root) return JS_NULL;
    NbNode *head = find_tag_first(g_dom_root, "head");
    if (!head) {
        head = calloc(1, sizeof(*head));
        if (!head) return JS_NULL;
        head->tag = strdup("head");
        NbNode *html = find_tag_first(g_dom_root, "html");
        if (html) local_insert_before(html, head, html->first_child);
        else local_insert_before(g_dom_root, head, NULL);
    }
    return push_node(ctx, head);
}

/* ---- element natives (this = element object) ---- */
static JSValue nb_el_getAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, owned); return JS_NULL; }
    const char *v = nb_attr_get(n, name);
    JS_FreeCString(ctx, owned);
    if (v && v[0]) return JS_NewString(ctx, v);
    return JS_NULL;
}
/* rung 8: hasAttribute — real bundles gate on documentElement.hasAttribute */
static JSValue nb_el_hasAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *owned = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, owned); return JS_FALSE; }
    const char *v = nb_attr_get(n, name);
    JS_FreeCString(ctx, owned);
    return JS_NewBool(ctx, v != NULL);
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
static JSValue nb_el_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL, *vl = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    const char *val = (argc > 1 && JS_IsString(argv[1])) ? (vl = JS_ToCString(ctx, argv[1])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl); return JS_UNDEFINED; }
    if (!val) val = "";
    if (!strcasecmp(name, "id")) { free(n->id); n->id = strdup(val); }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = strdup(val); }
    char *na = attrs_set(n, name, val);
    free(n->attrs);
    n->attrs = na;
    JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
/* rung-2 remainder: element.removeAttribute(name) — rebuild the raw attrs
 * blob without the named attribute (attrs_set's loop, skipping the match). */
static int attrs_has(const NbNode *n, const char *name) {
    if (!n || !n->attrs || !name) return 0;
    const char *p = n->attrs;
    size_t nl = strlen(name);
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        if (kl == nl && !strncasecmp(ks, name, nl)) return 1;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return 0;
}
static char *attrs_del(const NbNode *n, const char *name) {
    SB b = {0, 0, 0};
    const char *p = n->attrs ? n->attrs : "";
    size_t nl = strlen(name);
    int first = 1;
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
        if (!is_target) {   /* keep the attribute (attrs doesn't reorder) */
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
    return b.s ? b.s : strdup("");
}
static JSValue nb_el_removeAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name || !attrs_has(n, name)) { JS_FreeCString(ctx, nm); return JS_UNDEFINED; }
    if (!strcasecmp(name, "id")) { free(n->id); n->id = NULL; }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = NULL; }
    char *na = attrs_del(n, name);
    free(n->attrs);
    n->attrs = na;
    JS_FreeCString(ctx, nm);
    return JS_UNDEFINED;
}
static JSValue nb_el_id_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, n && n->id ? n->id : "");
}
static JSValue nb_el_id_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (n) { free(n->id); n->id = strdup(v); }
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
static JSValue nb_el_className_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, n && n->cls ? n->cls : "");
}
static JSValue nb_el_className_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (n) { free(n->cls); n->cls = strdup(v); }
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
static JSValue nb_el_textContent_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    SB b = {0, 0, 0};
    node_text_content(n, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_textContent_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    clear_children(n);
    free(n->text);
    n->text = strdup(v);
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
static JSValue nb_el_innerHTML_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    SB b = {0, 0, 0};
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling) node_outer_html(c, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_innerHTML_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
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
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
static JSValue nb_el_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
            if (!c->tag || !c->tag[0]) continue;   /* children is element-only (childNodes keeps text) */
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i++, push_node(ctx, (NbNode *)c));
        }
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    return arr;
}
static JSValue nb_el_childNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (const NbNode *c = n->first_child; c; c = c->next_sibling) {
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i++, push_node(ctx, (NbNode *)c));  /* text nodes incl. */
        }
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    return arr;
}
static JSValue nb_el_parentNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *p = n ? n->parent : NULL;
    if (p) return push_node(ctx, p);
    return JS_NULL;
}
static JSValue nb_el_firstChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n && n->first_child) return push_node(ctx, n->first_child);
    return JS_NULL;
}
static JSValue nb_el_nextSibling(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n && n->next_sibling) return push_node(ctx, n->next_sibling);
    return JS_NULL;
}
static JSValue nb_el_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *ch = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!n || !ch || ch == n) return JS_NULL;
    node_detach(ch);
    orphan_remove(ch);
    local_append(n, ch);
    return push_node(ctx, ch);
}
/* rung-2 remainder: the tree mutators. A removed node is orphaned, not
 * freed, so a JS wrapper still referencing it stays valid (teardown frees
 * the orphan list). Mirrors DOM errors for the wrong parent/child cases. */
static JSValue nb_el_removeChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *ch = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!n || !ch) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: removeChild needs an element child"));
        return JS_Throw(ctx, e);
    }
    if (!is_child_of(n, ch)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the node is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    node_detach(ch);
    orphan_add(ch);
    return push_node(ctx, ch);
}
static JSValue nb_el_insertBefore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *nn = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    NbNode *rn = (argc > 1 && JS_IsObject(argv[1])) ? get_node(ctx, argv[1]) : NULL;
    if (!n || !nn || nn == n) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "HierarchyRequestError: insertBefore needs a real new node"));
        return JS_Throw(ctx, e);
    }
    if (rn && !is_child_of(n, rn)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the reference node is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    node_detach(nn);
    orphan_remove(nn);
    local_insert_before(n, nn, rn);
    return push_node(ctx, nn);
}
static JSValue nb_el_replaceChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *nn = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    NbNode *on = (argc > 1 && JS_IsObject(argv[1])) ? get_node(ctx, argv[1]) : NULL;
    if (!n || !nn || !on || nn == on || nn == n) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "HierarchyRequestError: replaceChild needs two distinct real nodes"));
        return JS_Throw(ctx, e);
    }
    if (!is_child_of(n, on)) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "NotFoundError: the old child is not a child of this element"));
        return JS_Throw(ctx, e);
    }
    node_detach(nn);                    /* newChild may live in this same list */
    orphan_remove(nn);
    NbNode *after = on->next_sibling;   /* correct after nn's detach relinks */
    node_detach(on);
    orphan_add(on);
    local_insert_before(n, nn, after);
    return push_node(ctx, on);          /* DOM returns the replaced child */
}
/* rung-2 remainder: el.value for form fields — a get/set pair; the set
 * string is held on the wrapper (identity-cached per node) under a hidden
 * \xff key, and an unsets element falls back to its `value` attribute. */
static int is_form_field(const char *tag) {
    if (!tag) return 0;
    return !strcmp(tag, "input") || !strcmp(tag, "textarea")
        || !strcmp(tag, "select") || !strcmp(tag, "button")
        || !strcmp(tag, "option");
}
static JSValue nb_el_value_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    JSValue v0 = JS_GetPropertyStr(ctx, this_val, "\xffvalue");
    if (JS_IsString(v0)) return v0;
    JS_FreeValue(ctx, v0);
    const char *v = nb_attr_get(n, "value");
    if (v && v[0]) return JS_NewString(ctx, v);
    return JS_NewString(ctx, "");
}
static JSValue nb_el_value_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    JS_SetPropertyStr(ctx, this_val, "\xffvalue", JS_NewString(ctx, v));
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
/* ---- classList natives (this = the classList object, shares \xffnode) ---- */
static JSValue nb_cl_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok || !*tok) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    if (!has_class(n, tok)) {
        SB b = {0, 0, 0};
        if (n->cls && *n->cls) { sb_put(&b, n->cls); sb_put(&b, " "); }
        sb_put(&b, tok);
        free(n->cls); n->cls = b.s;
    }
    JS_FreeCString(ctx, tk);
    return JS_UNDEFINED;
}
static JSValue nb_cl_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    if (!has_class(n, tok)) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    SB b = {0, 0, 0};
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) {
        if (strcmp(c, tok)) { if (b.len) sb_put(&b, " "); sb_put(&b, c); }
        c = strtok(NULL, " ");
    }
    free(n->cls); n->cls = b.s ? b.s : strdup("");
    JS_FreeCString(ctx, tk);
    return JS_UNDEFINED;
}
static JSValue nb_cl_toggle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok || !*tok) { JS_FreeCString(ctx, tk); return JS_NewBool(ctx, 0); }
    if (has_class(n, tok)) { nb_cl_remove(ctx, this_val, argc, argv); JS_FreeCString(ctx, tk); return JS_NewBool(ctx, 0); }
    nb_cl_add(ctx, this_val, argc, argv);
    JS_FreeCString(ctx, tk);
    return JS_NewBool(ctx, 1);
}
static JSValue nb_cl_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    int r = n && tok ? has_class(n, tok) : 0;
    JS_FreeCString(ctx, tk);
    return JS_NewBool(ctx, r);
}

#define IDMAPNAME "__nb_idmap"    /* global object: node index -> JS wrapper (identity) */
/* DOM class prototypes (2026-09-18): real bundles branch on
 * `instanceof Element` and touch `Element.prototype` at load time. The
 * per-context global object `__nb_protos` holds the constructor-chain
 * prototypes for element/text/document wrappers built by install_dom_classes. */
#define PROTONAME "__nb_protos"
#define PROTO_EL 0   /* element wrappers ride HTMLElement.prototype */
#define PROTO_TEXT 1 /* text nodes ride Text.prototype */
#define PROTO_DOC 2  /* the document object rides Document.prototype */

/* Build a JS element object wrapping a C NbNode. */
static JSValue push_node(JSContext *ctx, NbNode *n) {
    int nidx = node_index(n);
    /* wrapper identity: one JS object per C node, held in the global
     * __nb_idmap object (the context is fresh per page). */
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue map = JS_GetPropertyStr(ctx, g, IDMAPNAME);
    if (!JS_IsObject(map)) {
        JS_FreeValue(ctx, map);
        map = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, g, IDMAPNAME, JS_DupValue(ctx, map));
    }
    JS_FreeValue(ctx, g);
    JSValue ex = JS_GetPropertyUint32(ctx, map, (uint32_t)nidx);
    if (JS_IsObject(ex)) {           /* existing wrapper */
        JS_FreeValue(ctx, map);
        return ex;
    }
    JS_FreeValue(ctx, ex);
    JSValue el = JS_NewObject(ctx);
    /* DOM class prototype (install_dom_classes) so `el instanceof Element`
     * and Element.prototype.* resolve like a browser; falls back to a plain
     * object when the holder is absent (node mode / pre-install). */
    {
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue holder = JS_GetPropertyStr(ctx, g2, PROTONAME);
        JS_FreeValue(ctx, g2);
        if (JS_IsObject(holder)) {
            int which = (n->tag && n->tag[0]) ? PROTO_EL : PROTO_TEXT;
            JSValue p = JS_GetPropertyUint32(ctx, holder, (uint32_t)which);
            if (JS_IsObject(p)) JS_SetPrototype(ctx, el, p);
            JS_FreeValue(ctx, p);
        }
        JS_FreeValue(ctx, holder);
    }
    JS_SetPropertyStr(ctx, el, NODEKEY, JS_NewInt32(ctx, nidx));
    {
        const char *label = (n->tag && n->tag[0]) ? n->tag : "#text";
        JS_SetPropertyStr(ctx, el, "nodeName", JS_NewString(ctx, label));
        if (n->tag && n->tag[0])
            JS_SetPropertyStr(ctx, el, "tagName", JS_NewString(ctx, label));
    }

    JS_SetPropertyStr(ctx, el, "getAttribute", JS_NewCFunction(ctx, nb_el_getAttribute, "getAttribute", 1));
    JS_SetPropertyStr(ctx, el, "setAttribute", JS_NewCFunction(ctx, nb_el_setAttribute, "setAttribute", 2));
    JS_SetPropertyStr(ctx, el, "removeAttribute", JS_NewCFunction(ctx, nb_el_removeAttribute, "removeAttribute", 1));
    JS_SetPropertyStr(ctx, el, "hasAttribute", JS_NewCFunction(ctx, nb_el_hasAttribute, "hasAttribute", 1));
    JS_SetPropertyStr(ctx, el, "appendChild", JS_NewCFunction(ctx, nb_el_appendChild, "appendChild", 1));
    JS_SetPropertyStr(ctx, el, "removeChild", JS_NewCFunction(ctx, nb_el_removeChild, "removeChild", 1));
    JS_SetPropertyStr(ctx, el, "insertBefore", JS_NewCFunction(ctx, nb_el_insertBefore, "insertBefore", 2));
    JS_SetPropertyStr(ctx, el, "replaceChild", JS_NewCFunction(ctx, nb_el_replaceChild, "replaceChild", 2));
    JS_SetPropertyStr(ctx, el, "addEventListener", JS_NewCFunction(ctx, nb_el_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, el, "removeEventListener", JS_NewCFunction(ctx, nb_el_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, el, "dispatchEvent", JS_NewCFunction(ctx, nb_el_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, el, "click", JS_NewCFunction(ctx, nb_el_click, "click", 0));
    /* resource/URL attributes real bundles read directly off the element:
     * closure's module loader does `D = O.src ? O.src : O.getAttribute("href")`
     * on the <script id="base-js"> / <link> it found by id. Expose the raw
     * attribute (already absolute in fetched pages; callers absolutize). */
    if (n->tag) {
        const char *sv = nb_attr_get(n, "src");
        if (sv && sv[0]) JS_SetPropertyStr(ctx, el, "src", JS_NewString(ctx, sv));
        const char *hv = nb_attr_get(n, "href");
        if (hv && hv[0]) JS_SetPropertyStr(ctx, el, "href", JS_NewString(ctx, hv));
    }
    /* canvas 2D (2026-09-18): real bundles probe <canvas> via
     * createElementNS('...','canvas') and immediately call getContext('2d')
     * through a fillStyle/color parse. Minimal context: geometry/measure
     * stubs that return well-formed objects so parser/feature paths don't
     * throw; rasterization is out of scope (see NB-JS-ENGINE-ROADMAP). */
    if (n->tag && strcmp(n->tag, "canvas") == 0) {
        JS_SetPropertyStr(ctx, el, "getContext", JS_NewCFunction(ctx, nb_el_getContext, "getContext", 1));
        JS_SetPropertyStr(ctx, el, "toDataURL", JS_NewCFunction(ctx, nb_canvas_toDataURL, "toDataURL", 0));
    }
    if (n->tag && strcmp(n->tag, "template") == 0) {
        /* <template>.content is a DocumentFragment (prelude __nb_docfrag) */
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue mf = JS_GetPropertyStr(ctx, g2, "__nb_docfrag");
        JS_FreeValue(ctx, g2);
        if (JS_IsFunction(ctx, mf)) {
            JSValue frag = JS_Call(ctx, mf, JS_UNDEFINED, 0, NULL);
            if (JS_IsException(frag)) JS_FreeValue(ctx, JS_GetException(ctx));
            else JS_SetPropertyStr(ctx, el, "content", frag);
        }
        JS_FreeValue(ctx, mf);
    }

    /* el.on<type> = cb accessors; native magic carries the ONPROPS index
     * (QuickJS accessors carry no property name — the magic is the index). */
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom nm = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_el_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* read-only accessor properties: children, childNodes, parentNode, firstChild, nextSibling */
    {
        JSAtom nm = JS_NewAtom(ctx, "children");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_children, "children", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childNodes");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_childNodes, "childNodes", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentNode");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_parentNode, "parentNode", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "firstChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_firstChild, "firstChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_nextSibling, "nextSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* accessors: id, className, textContent, innerHTML */
    {
        JSAtom nm = JS_NewAtom(ctx, "id");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_id_get, "get id", 0), JS_NewCFunction(ctx, nb_el_id_set, "set id", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "className");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_className_get, "get className", 0), JS_NewCFunction(ctx, nb_el_className_set, "set className", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_textContent_get, "get textContent", 0), JS_NewCFunction(ctx, nb_el_textContent_set, "set textContent", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_innerHTML_get, "get innerHTML", 0), JS_NewCFunction(ctx, nb_el_innerHTML_set, "set innerHTML", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung-2 remainder + rung 7: el.style — identity-cached snapshot of
     * the inline style attribute (reads mirror inline decls; JS writes
     * persist on the snapshot; not fed back into layout metrics). */
    {
        JSAtom nm = JS_NewAtom(ctx, "style");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_style_get, "get style", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung 7: layout-intent metrics (display:none-aware, CSS px from the
     * cascade, else 0 — see NB-JS-ENGINE-ROADMAP rung 7). */
    {
        JSAtom nm = JS_NewAtom(ctx, "offsetWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetParent");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_offset_parent, "get offsetParent", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, el, "getBoundingClientRect",
                      JS_NewCFunction(ctx, nb_el_getBoundingClientRect, "getBoundingClientRect", 0));

    /* rung-2 remainder: el.value get/set for form fields. */
    if (is_form_field(n->tag)) {
        JSAtom nm = JS_NewAtom(ctx, "value");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_value_get, "get value", 0), JS_NewCFunction(ctx, nb_el_value_set, "set value", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* classList */
    {
        JSValue cl = JS_NewObject(ctx);                /* classList */
        JS_SetPropertyStr(ctx, cl, NODEKEY, JS_NewInt32(ctx, nidx));
        JS_SetPropertyStr(ctx, cl, "add", JS_NewCFunction(ctx, nb_cl_add, "add", 1));
        JS_SetPropertyStr(ctx, cl, "remove", JS_NewCFunction(ctx, nb_cl_remove, "remove", 1));
        JS_SetPropertyStr(ctx, cl, "toggle", JS_NewCFunction(ctx, nb_cl_toggle, "toggle", 1));
        JS_SetPropertyStr(ctx, cl, "contains", JS_NewCFunction(ctx, nb_cl_contains, "contains", 1));
        JS_SetPropertyStr(ctx, el, "classList", cl);
    }

    /* cache wrapper in the identity map so later push_node calls return the
     * same object (map originally FIXED a self-storing bug: the old code
     * stored the wrapper into itself -> identity broke). */
    JS_SetPropertyUint32(ctx, map, (uint32_t)nidx, JS_DupValue(ctx, el));
    JS_FreeValue(ctx, map);
    return el;
}

/* ============================= rung 6: file-backed document.cookie jar =====
 * document.cookie getter/setter as C natives (the prelude in nb_host.h leaves
 * a configurable stub; install_dom redefines it with these). The jar lives on
 * disk at $NB_COOKIES_FILE (fallback $HOME/.config/nbjs/nb_cookies.txt), so
 * cookies survive across LOADs — each LOAD runs in a fresh engine instance, so
 * the file is the only persistence. Jar line format (TAB-separated legend):
 *   host<TAB>path<TAB>name<TAB>value<TAB>expires_epoch<TAB>secure
 * host "*" = set from a URI with no host. expires 0 = session cookie.
 * RFC 6265 subset: name=value + Domain/Path/Expires/Max-Age/Secure.
 * Reads tolerate damage: junk lines are skipped, not fatal. */
#define COOKIE_MAX_ENT 512

typedef struct {
    char host[128];
    char path[256];
    char name[128];
    char value[1024];
    time_t expires;
    int secure;
} CookieEnt;

static char g_cookie_path[PATH_MAX];
static int  g_cookie_path_set = 0;

static void mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void cookie_jar_init(void) {
    g_cookie_path_set = 1;
    const char *env = getenv("NB_COOKIES_FILE");
    if (env && env[0]) { snprintf(g_cookie_path, sizeof(g_cookie_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_cookie_path, sizeof(g_cookie_path), "%s/.config/nbjs/nb_cookies.txt", home);
    else
        g_cookie_path[0] = 0;   /* no writable location: reads '', writes no-op */
}

/* Split the current g_href into host + request path (bare host/port dropped).
 * Port numbers are skipped (cookie scoping ignores ports, RFC 6265 §1). */
static int href_parts(char *hostb, size_t hl, char *pathb, size_t pl) {
    const char *p = g_href;
    const char *a = strstr(p, "://");
    const char *s = a ? a + 3 : p;
    const char *q = s;
    while (*q) {
        if (*q == '/' || *q == '?' || *q == '#') break;
        q++;
    }
    size_t hn = (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    /* RFC 6265 §1: cookie scope is host-only — cut a ":port" suffix if the
     * colon slice between scheme and path is entirely digits. */
    for (char *c = hostb; *c; c++) if (*c == ':') { *c = 0; break; }
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *ph = q;
    while (*ph && *ph != '?' && *ph != '#') ph++;
    size_t pn = (size_t)(ph - q);
    if (pn >= pl) pn = pl - 1;
    memcpy(pathb, q, pn); pathb[pn] = 0;
    if (!pathb[0]) snprintf(pathb, pl, "/");
    return 1;
}

/* RFC 6265 §5.1.4 default-path for a Set-Cookie with no explicit Path. */
static void default_cookie_path(const char *rp, char *out, size_t olen) {
    if (!rp || rp[0] != '/') { snprintf(out, olen, "/"); return; }
    const char *r = strrchr(rp, '/');
    if (!r || r == rp) { snprintf(out, olen, "/"); return; }
    size_t n = (size_t)(r - rp);
    if (n >= olen) n = olen - 1;
    memcpy(out, rp, n); out[n] = 0;
    if (!out[0]) snprintf(out, olen, "/");
}

/* RFC 6265 §5.1.4 path-match: `cp` is the cookie path, `rp` the request path. */
static int cookie_path_match(const char *cp, const char *rp) {
    if (!cp || !rp) return 0;
    if (!strcmp(cp, rp)) return 1;
    size_t n = strlen(cp);
    if (n == 0) return 1;
    if (strncmp(rp, cp, n) != 0) return 0;
    if (cp[n - 1] == '/') return 1;
    return rp[n] == '/';
}

static char *trim_c(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
    return s;
}

static void sanitize_cookie_value(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 1 < olen; c++) {
        if (*c < 0x20 || *c == 0x7f) continue;   /* drop CR/LF/controls (line-injection) */
        out[o++] = (char)*c;
    }
    out[o] = 0;
}

/* days-from-civil -> UNIX epoch (no TZ dependence; glibc timegm macro-gated). */
static time_t epoch_from_ymd(int y, int m, int d, int hh, int mi, int ss) {
    if (m < 3) { m += 12; y--; }
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (unsigned)(m > 2 ? m - 3 : m + 9) + 2) / 5 + (unsigned)d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    long days = (long)(era * 146097) + (long)doe - 719468L;
    return (time_t)days * 86400L + hh * 3600L + mi * 60L + ss;
}

static const char *const COOKIE_MONTHS[12] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* IMF-fixdate ("Sun, 06 Nov 1994 08:49:37 GMT"); (time_t)-1 = unparseable.
 * Note a real "01 Jan 1970" parses to epoch 0 (a valid instant, NOT the
 * "no expiry" sentinel — that distinction is handled in the setter). */
static time_t cookie_datetime(const char *s) {
    if (!s || !*s) return (time_t)-1;
    int d = 0, y = 0, hh = 0, mi = 0, ss = 0, mon = -1;
    char monname[8] = {0};
    if (sscanf(s, "%*[^,], %d %7s %d %d:%d:%d",
               &d, monname, &y, &hh, &mi, &ss) == 6) {
        for (int i = 0; i < 12 && mon < 0; i++)
            if (!strncasecmp(COOKIE_MONTHS[i], monname, 3)) mon = i;
        if (mon >= 0 && y >= 1970 && y <= 9999)
            return epoch_from_ymd(y, mon + 1, d, hh, mi, ss);
    }
    return (time_t)-1;
}

static int cookie_parse_line(char *line, CookieEnt *e) {
    memset(e, 0, sizeof(*e));
    char *f[6];
    int nf = 0;
    char *q = line;
    while (nf < 6 && *q) {
        f[nf] = q;
        char *t = strchr(q, '\t');
        if (t) { *t = 0; q = t + 1; }
        else { q += strlen(q); }
        nf++;
    }
    if (nf < 5) return 0;
    snprintf(e->host, sizeof(e->host), "%s", f[0]);
    snprintf(e->path, sizeof(e->path), "%s", f[1]);
    snprintf(e->name, sizeof(e->name), "%s", f[2]);
    snprintf(e->value, sizeof(e->value), "%s", f[3]);
    e->expires = (time_t)atol(f[4]);
    if (nf >= 6) e->secure = atoi(f[5]) ? 1 : 0;
    return 1;
}

static int cookie_load_file(CookieEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(g_cookie_path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) { *nl = 0; }
        if (cookie_parse_line(p, &ents[n])) n++;
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}

static void cookie_save_file(const CookieEnt *ents, int n) {
    if (!g_cookie_path[0]) return;
    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", g_cookie_path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) { *slash = 0; if (slash != dirbuf) mkdir_p(dirbuf); }
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        sb_put(&b, ents[i].host); sb_put(&b, "\t");
        sb_put(&b, ents[i].path); sb_put(&b, "\t");
        sb_put(&b, ents[i].name); sb_put(&b, "\t");
        sb_put(&b, ents[i].value);
        char tail[64];
        snprintf(tail, sizeof(tail), "\t%ld\t%d\n",
                 (long)ents[i].expires, ents[i].secure ? 1 : 0);
        sb_put(&b, tail);
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_cookie_path);
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (b.s && b.len) fwrite(b.s, 1, b.len, f);
        fclose(f);
        rename(tmp, g_cookie_path);
    }
    free(b.s);
}

/* ======================== rung 6 seam: unified cookie store =============
 * ONE authoritative jar shared by document.cookie AND the network layer
 * (nb_fetch_sync).  Chromium-parity: the old two-store split (NB_COOKIES_FILE
 * for document.cookie, NB_CURL_COOKIES_FILE for curl -b/-c) is eliminated;
 * all Set-Cookie ingress and Cookie egress goes through our jar.  The manager
 * may still set NB_CURL_COOKIES_FILE for its own curls; the worker ignores it
 * from here forward.
 *
 * cookie_header_for_url()  — scope-match + emit Cookie: for outgoing request
 * cookie_set_from_wire()   — parse response Set-Cookie into the jar
 */

/* Extract host and path from an arbitrary URL (scheme://host[:port]/path).
 * Port is stripped for cookie scoping (RFC 6265 section 1). */
static int url_host_path(const char *url, char *hostb, size_t hl,
                         char *pathb, size_t pl) {
    hostb[0] = 0; pathb[0] = 0;
    const char *a = strstr(url, "://");
    if (!a) return 0;
    const char *s = a + 3;
    const char *q = s;
    while (*q && *q != '/' && *q != '?') q++;
    const char *col = NULL;
    for (const char *p = s; p < q; p++) if (*p == ':') { col = p; break; }
    size_t hn = col ? (size_t)(col - s) : (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *pp = (*q == '/') ? q : "/";
    snprintf(pathb, pl, "%s", pp);
    return 1;
}

/* Scope-match cookies from the jar for an outgoing request URL.
 * Writes "Cookie: n1=v1; n2=v2" into out.  Respects Domain/Path/Secure. */
static void cookie_header_for_url(const char *url, char *out, size_t olen) {
    out[0] = 0;
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return;
    char host[128], rp[512];
    if (!url_host_path(url, host, sizeof(host), rp, sizeof(rp))) return;
    int is_https = (strncmp(url, "https://", 8) == 0);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;
        if (ents[i].secure && !is_https) continue;
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;
        if (!cookie_path_match(ents[i].path, rp)) continue;
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    if (b.len && b.len + 10 < olen) {
        snprintf(out, olen, "Cookie: %.*s", (int)b.len, b.s);
    }
    free(b.s);
}

/* Parse a raw Set-Cookie header value and upsert into the jar.
 * request_url provides the default Domain/Path when the header omits them. */
static void cookie_set_from_wire(const char *header_val, const char *request_url) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0] || !header_val || !*header_val) return;

    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", header_val);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) return;
    while (*tok == ' ') tok++;
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) return;
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) return;

    while ((tok = strtok(NULL, ";")) != NULL) {
        while (*tok == ' ') tok++;
        if (!strncasecmp(tok, "domain=", 7))
            snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "path=", 5))
            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "max-age=", 8)) maxage = atol(tok + 8);
        else if (!strncasecmp(tok, "expires=", 8))
            snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure")) secure = 1;
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            snprintf(scope_host, sizeof(scope_host), "%s", dh);
    }
    if (!scope_path[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            default_cookie_path(dp, scope_path, sizeof(scope_path));
        else
            snprintf(scope_path, sizeof(scope_path), "/");
    }

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;
        else if (exp <= nowt) delete = 1;
    }

    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcasecmp(ents[i].host, scope_host) != 0) continue;
        if (ents[i].path[0] && strcmp(ents[i].path, scope_path) != 0) continue;
        if (strcmp(ents[i].name, name) != 0) continue;
        found = i;
        break;
    }
    if (delete) {
        if (found >= 0) {
            for (int i = found; i + 1 < n; i++) ents[i] = ents[i + 1];
            n--;
        }
    } else {
        if (found >= 0) {
            snprintf(ents[found].value, sizeof(ents[found].value), "%s", value);
            ents[found].expires = exp;
            ents[found].secure = secure;
        } else if (n < COOKIE_MAX_ENT) {
            CookieEnt *e = &ents[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->host, sizeof(e->host), "%s", scope_host);
            snprintf(e->path, sizeof(e->path), "%s", scope_path);
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->value, sizeof(e->value), "%s", value);
            e->expires = exp;
            e->secure = secure;
        }
    }
    cookie_save_file(ents, n);
}

static JSValue nb_dom_cookie_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return JS_NewString(ctx, "");
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) return JS_NewString(ctx, "");
    for (char *c = host; *c; c++) *c = (char)tolower((unsigned char)*c);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;   /* expired */
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;          /* other host */
        if (!cookie_path_match(ents[i].path, rp)) continue;         /* other path */
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    JSValue r = JS_NewStringLen(ctx, b.s ? b.s : "", b.len);
    free(b.s);
    return r;
}

static JSValue nb_dom_cookie_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *spec_owned = NULL;
    const char *spec = argc > 0 ? (spec_owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!spec) return JS_UNDEFINED;   /* JS_ToCString already threw on coercion failure */
    if (!g_cookie_path_set) cookie_jar_init();
    if (!spec || !*spec || !g_cookie_path[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", spec);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    tok = trim_c(tok);
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    while ((tok = strtok(NULL, ";")) != NULL) {
        tok = trim_c(tok);
        if (!strncasecmp(tok, "path=", 5))            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "domain=", 7))     snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "max-age=", 8))    { maxage = atol(tok + 8); }
        else if (!strncasecmp(tok, "expires=", 8))    snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure"))          secure = 1;
        /* HttpOnly / SameSite / unknown attrs are accepted and ignored. */
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;              /* strip leading dots */
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) snprintf(scope_host, sizeof(scope_host), "%s", host);
    if (!scope_path[0]) default_cookie_path(rp, scope_path, sizeof(scope_path));

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;               /* max-age=0 -> remove */
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;            /* unparseable -> session cookie */
        else if (exp <= nowt) delete = 1;          /* expired date -> remove */
    }

    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcasecmp(ents[i].host, scope_host) != 0) continue;
        if (ents[i].path[0] && strcmp(ents[i].path, scope_path) != 0) continue;
        if (strcmp(ents[i].name, name) != 0) continue;
        found = i;
        break;
    }
    if (delete) {
        if (found >= 0) {
            for (int i = found; i + 1 < n; i++) ents[i] = ents[i + 1];
            n--;
        }
    } else {
        if (found >= 0) {
            snprintf(ents[found].host, sizeof(ents[found].host), "%s", scope_host);
            snprintf(ents[found].path, sizeof(ents[found].path), "%s", scope_path);
            snprintf(ents[found].value, sizeof(ents[found].value), "%s", value);
            ents[found].expires = exp;
            ents[found].secure = secure;
        } else if (n < COOKIE_MAX_ENT) {
            CookieEnt *e = &ents[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->host, sizeof(e->host), "%s", scope_host);
            snprintf(e->path, sizeof(e->path), "%s", scope_path);
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->value, sizeof(e->value), "%s", value);
            e->expires = exp;
            e->secure = secure;
        }
    }
    cookie_save_file(ents, n);
    JS_FreeCString(ctx, spec_owned);
    return JS_UNDEFINED;
}

/* ===================== rung 6: localStorage (disk jar) + sessionStorage (per-LOAD) =====
 * install_host leaves getItem/setItem/removeItem as no-op stubs; install_dom replaces
 * BOTH globals with real C-backed objects here. localStorage persists across LOADs via
 * a jar on disk at $NB_LOCALSTORAGE_FILE (fallback ~/.config/nbjs/nb_localstorage.txt);
 * sessionStorage lives in process memory and is cleared at the top of every run_page,
 * so each LOAD gets a fresh session (a fresh runtime/context could not carry JS state
 * anyway). Jar line format: <pct-encoded key>\t<pct-encoded value>\n — keys/values are
 * percent-encoded (RFC 3986 unreserved pass through, everything else %XX) so tabs,
 * newlines and control chars are safe inside a line. Reads tolerate damage. */
#define ST_MAX_ENT 256

typedef struct { char key[256]; char value[4096]; } StEnt;

static char g_ls_path[PATH_MAX];
static int  g_ls_path_set = 0;
static StEnt g_ls[ST_MAX_ENT];      /* reused load/save buffer (single-threaded) */
static StEnt g_ss[ST_MAX_ENT];      /* in-memory session map, cleared per LOAD */
static int  g_ss_count = 0;

static void ls_jar_init(void) {
    g_ls_path_set = 1;
    const char *env = getenv("NB_LOCALSTORAGE_FILE");
    if (env && env[0]) { snprintf(g_ls_path, sizeof(g_ls_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_ls_path, sizeof(g_ls_path), "%s/.config/nbjs/nb_localstorage.txt", home);
    else
        g_ls_path[0] = 0;   /* no writable location: reads null, writes no-op */
}

static int pct_is_safe(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
static void pct_encode(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 3 < olen; c++) {
        if (pct_is_safe(*c)) out[o++] = (char)*c;
        else { unsigned char uc = *c;
            static const char H[] = "0123456789ABCDEF";
            out[o++] = '%'; out[o++] = H[uc >> 4]; out[o++] = H[uc & 15];
        }
    }
    out[o] = 0;
}
static void pct_decode(char *s) {
    char *w = s;
    for (const char *r = s; *r;) {
        if (r[0] == '%' && r[1] && r[2]) {
            int hi, lo;
            char h1 = r[1], h2 = r[2];
            hi = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            lo = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) { *w++ = (char)((hi << 4) | lo); r += 3; continue; }
        }
        *w++ = *r++;
    }
    *w = 0;
}

static int st_load_file(const char *path, StEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *tab = strchr(p, '\t');
        if (tab) {
            *tab = 0;
            snprintf(ents[n].key, sizeof(ents[n].key), "%s", p);
            pct_decode(ents[n].key);
            snprintf(ents[n].value, sizeof(ents[n].value), "%s", tab + 1);
            pct_decode(ents[n].value);
            n++;
        }
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}

static void ls_save_file(const StEnt *ents, int n) {
    if (!g_ls_path[0]) return;
    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", g_ls_path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) { *slash = 0; if (slash != dirbuf) mkdir_p(dirbuf); }
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        char k[256 * 3 + 1], v[4096 * 3 + 1];
        pct_encode(ents[i].key, k, sizeof(k));
        pct_encode(ents[i].value, v, sizeof(v));
        sb_put(&b, k); sb_put(&b, "\t"); sb_put(&b, v); sb_put(&b, "\n");
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_ls_path);
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (b.s && b.len) fwrite(b.s, 1, b.len, f);
        fclose(f);
        rename(tmp, g_ls_path);
    }
    free(b.s);
}

static int ls_find(int n, const char *key) {
    for (int i = 0; i < n; i++) if (strcmp(g_ls[i].key, key) == 0) return i;
    return -1;
}
static JSValue nb_ls_getItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); return JS_NULL; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_NULL;
    return JS_NewString(ctx, g_ls[f].value);
}
static JSValue nb_ls_setItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL, *vo = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    const char *val = js_arg_str_any(ctx, argv, argc, 1, &vo);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo); return JS_UNDEFINED; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    if (f >= 0) {
        snprintf(g_ls[f].value, sizeof(g_ls[f].value), "%s", val);
    } else if (n < ST_MAX_ENT) {
        StEnt *e = &g_ls[n++];
        memset(e, 0, sizeof(*e));
        snprintf(e->key, sizeof(e->key), "%s", key);
        snprintf(e->value, sizeof(e->value), "%s", val);
    } else {
        JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
        return JS_UNDEFINED;
    }
    ls_save_file(g_ls, n);
    JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
    return JS_UNDEFINED;
}
static JSValue nb_ls_removeItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); return JS_UNDEFINED; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_UNDEFINED;
    for (int i = f; i + 1 < n; i++) g_ls[i] = g_ls[i + 1];
    ls_save_file(g_ls, n - 1);
    return JS_UNDEFINED;
}
static JSValue nb_ls_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_ls_path_set) ls_jar_init();
    if (g_ls_path[0]) ls_save_file(g_ls, 0);
    return JS_UNDEFINED;
}
static JSValue nb_ls_key(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int i = js_arg_index(ctx, argv, argc, 0);
    if (!g_ls_path_set) ls_jar_init();
    if (!g_ls_path[0] || i < 0) return JS_NULL;
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    if (i >= n) return JS_NULL;
    return JS_NewString(ctx, g_ls[i].key);
}
static JSValue nb_ls_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_ls_path_set) ls_jar_init();
    int n = g_ls_path[0] ? st_load_file(g_ls_path, g_ls, ST_MAX_ENT) : 0;
    return JS_NewInt32(ctx, n);
}

static int ss_find(const char *k) {
    for (int i = 0; i < g_ss_count; i++) if (strcmp(g_ss[i].key, k) == 0) return i;
    return -1;
}
static JSValue nb_ss_getItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!k) { JS_FreeCString(ctx, ko); return JS_NULL; }
    int f = ss_find(k);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_NULL;
    return JS_NewString(ctx, g_ss[f].value);
}
static JSValue nb_ss_setItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL, *vo = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    const char *v = js_arg_str_any(ctx, argv, argc, 1, &vo);
    if (!k) { JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo); return JS_UNDEFINED; }
    int f = ss_find(k);
    if (f >= 0) {
        snprintf(g_ss[f].value, sizeof(g_ss[f].value), "%s", v);
    } else if (g_ss_count < ST_MAX_ENT) {
        StEnt *e = &g_ss[g_ss_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->key, sizeof(e->key), "%s", k);
        snprintf(e->value, sizeof(e->value), "%s", v);
    }
    JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
    return JS_UNDEFINED;
}
static JSValue nb_ss_removeItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!k) { JS_FreeCString(ctx, ko); return JS_UNDEFINED; }
    int f = ss_find(k);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_UNDEFINED;
    for (int i = f; i + 1 < g_ss_count; i++) g_ss[i] = g_ss[i + 1];
    g_ss_count--;
    return JS_UNDEFINED;
}
static JSValue nb_ss_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { g_ss_count = 0; return JS_UNDEFINED; }
static JSValue nb_ss_key(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int i = js_arg_index(ctx, argv, argc, 0);
    if (i < 0 || i >= g_ss_count) return JS_NULL;
    return JS_NewString(ctx, g_ss[i].key);
}
static JSValue nb_ss_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return JS_NewInt32(ctx, g_ss_count); }

/* ---- DOM class hierarchy (2026-09-18) ----
 * Real bundles feature-detect via `instanceof Element/Node` and read
 * `Element.prototype` at load. We expose browser constructor globals
 * and chain their prototypes in the standard order
 * (EventTarget <- Node <- Element <- HTMLElement / SVGElement),
 * plus Text, Document and Animation. Wrappers produced by push_node()
 * ride those prototypes (keyed in the __nb_protos holder), and the
 * global `document` object rides Document.prototype with a minimal
 * timeline for web-animations feature code. Constructors are inert
 * here — illegal-constructor throws are out of scope for this step. */
static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* quickjs.c: C constructors receive new_target as this_val, so build the
     * instance off its .prototype (the fresh object already carries f.prototype)
     * — `new Element()` then yields an Element instance and instanceof works.
     * Generic method calls get a non-function receiver and return undefined. */
    if (JS_IsFunction(ctx, this_val)) {
        JSValue proto = JS_GetPropertyStr(ctx, this_val, "prototype");
        JSValue obj = JS_IsObject(proto) ? JS_NewObjectProto(ctx, proto) : JS_NewObject(ctx);
        JS_FreeValue(ctx, proto);
        return obj;
    }
    return JS_UNDEFINED;
}
/* whenDefined must return a thenable, so hand back a resolved Promise. */
static JSValue nb_promise_resolved(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue funcs[2];
    JSValue p = JS_NewPromiseCapability(ctx, funcs);
    if (JS_IsException(p)) return p;
    JS_Call(ctx, funcs[0], JS_UNDEFINED, 1, argv ? argv : &this_val);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    return p;
}
static JSValue nb_empty_array(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewArray(ctx);
}
static JSValue nb_empty_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewObject(ctx);
}
/* construct an inert constructor whose .prototype chains off `parent`
 * (or Object.prototype when parent is not an object). Returns an owned
 * function object; calling it yields undefined (see class_noop). */
static JSValue add_class(JSContext *ctx, const char *name, JSValueConst parent) {
    JSValue f = JS_NewCFunction2(ctx, class_noop, name, 1, JS_CFUNC_constructor, 0);
    JSValue p = JS_NewObject(ctx);
    if (JS_IsObject(parent)) JS_SetPrototype(ctx, p, parent);
    JS_SetPropertyStr(ctx, f, "prototype", p);
    return f;
}
/* register a constructor global only if the name is still free — the prelude
 * already defines Event (nb_el_click builds clicks through it) and URL-family
 * globals; clobbering them breaks dispatch and parsing. */
static void add_global_class(JSContext *ctx, JSValueConst g, const char *name, JSValueConst parent) {
    JSValue ex = JS_GetPropertyStr(ctx, g, name);
    int present = !JS_IsUndefined(ex);
    JS_FreeValue(ctx, ex);
    if (present) return;
    JS_SetPropertyStr(ctx, g, name, add_class(ctx, name, parent));
}
static void install_dom_classes(JSContext *ctx) {
    JSValue ETf = JS_NewCFunction2(ctx, class_noop, "EventTarget", 1, JS_CFUNC_constructor, 0);
    JSValue Nf = JS_NewCFunction2(ctx, class_noop, "Node", 1, JS_CFUNC_constructor, 0);
    JSValue Ef = JS_NewCFunction2(ctx, class_noop, "Element", 1, JS_CFUNC_constructor, 0);
    JSValue Hf = JS_NewCFunction2(ctx, class_noop, "HTMLElement", 1, JS_CFUNC_constructor, 0);
    JSValue Sf = JS_NewCFunction2(ctx, class_noop, "SVGElement", 1, JS_CFUNC_constructor, 0);
    JSValue Tf = JS_NewCFunction2(ctx, class_noop, "Text", 1, JS_CFUNC_constructor, 0);
    JSValue Df = JS_NewCFunction2(ctx, class_noop, "Document", 1, JS_CFUNC_constructor, 0);
    JSValue Af = JS_NewCFunction2(ctx, class_noop, "Animation", 1, JS_CFUNC_constructor, 0);

    JSValue ETp = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ETf, "prototype", JS_DupValue(ctx, ETp));
    JSValue Np = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Np, ETp);
    JS_SetPropertyStr(ctx, Nf, "prototype", JS_DupValue(ctx, Np));
    JSValue Ep = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Ep, Np);
    JS_SetPropertyStr(ctx, Ef, "prototype", JS_DupValue(ctx, Ep));
    JSValue Hp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Hp, Ep);
    JS_SetPropertyStr(ctx, Hf, "prototype", JS_DupValue(ctx, Hp));
    JSValue Sp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Sp, Ep);
    JS_SetPropertyStr(ctx, Sf, "prototype", JS_DupValue(ctx, Sp));
    JSValue Tp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Tp, Np);
    JS_SetPropertyStr(ctx, Tf, "prototype", JS_DupValue(ctx, Tp));
    JSValue Dp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Dp, Np);
    JS_SetPropertyStr(ctx, Df, "prototype", JS_DupValue(ctx, Dp));
    JSValue Ap = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, Af, "prototype", JS_DupValue(ctx, Ap));

    JSValue g = JS_GetGlobalObject(ctx);
    JSValue holder = JS_NewObject(ctx);
    JS_SetPropertyUint32(ctx, holder, PROTO_EL, JS_DupValue(ctx, Hp));
    JS_SetPropertyUint32(ctx, holder, PROTO_TEXT, JS_DupValue(ctx, Tp));
    JS_SetPropertyUint32(ctx, holder, PROTO_DOC, JS_DupValue(ctx, Dp));
    JS_SetPropertyStr(ctx, g, PROTONAME, holder);

    JS_SetPropertyStr(ctx, g, "EventTarget", ETf);
    JS_SetPropertyStr(ctx, g, "Node", Nf);
    JS_SetPropertyStr(ctx, g, "Element", Ef);
    JS_SetPropertyStr(ctx, g, "HTMLElement", Hf);
    JS_SetPropertyStr(ctx, g, "SVGElement", Sf);
    JS_SetPropertyStr(ctx, g, "Text", Tf);
    JS_SetPropertyStr(ctx, g, "Document", Df);
    JS_SetPropertyStr(ctx, g, "Animation", Af);

    /* the document object rides Document.prototype; web-animations reads
     * document.timeline at load, so give it a minimal one. */
    {
        JSValue doc = JS_GetPropertyStr(ctx, g, "document");
        if (JS_IsObject(doc)) {
            JS_SetPrototype(ctx, doc, Dp);
            JSValue tl = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, tl, "currentTime", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, tl, "getAnimations", JS_NewCFunction(ctx, class_noop, "getAnimations", 0));
            JS_SetPropertyStr(ctx, tl, "play", JS_NewCFunction(ctx, class_noop, "play", 1));
            JS_SetPropertyStr(ctx, tl, "reverse", JS_NewCFunction(ctx, class_noop, "reverse", 0));
            JS_SetPropertyStr(ctx, tl, "_play", JS_NewCFunction(ctx, class_noop, "_play", 1));
            JS_SetPropertyStr(ctx, doc, "timeline", tl);
        }
        JS_FreeValue(ctx, doc);
    }

    /* customElements + CSSStyleSheet: real bundles read
     * window.customElements.polyfillWrapFlushCallback and probe
     * CSSStyleSheet.prototype at load; both globals must exist even if
     * inert (feature detection then takes the non-native fallback). */
    {
        JSValue ce = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ce, "define", JS_NewCFunction(ctx, class_noop, "define", 2));
        JS_SetPropertyStr(ctx, ce, "get", JS_NewCFunction(ctx, class_noop, "get", 1));
        JS_SetPropertyStr(ctx, ce, "whenDefined", JS_NewCFunction(ctx, nb_promise_resolved, "whenDefined", 1));
        JS_SetPropertyStr(ctx, ce, "upgrade", JS_NewCFunction(ctx, class_noop, "upgrade", 1));
        JS_SetPropertyStr(ctx, g, "customElements", ce);

        JSValue CSf = JS_NewCFunction2(ctx, class_noop, "CSSStyleSheet", 1, JS_CFUNC_constructor, 0);
        JSValue CSp = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, CSp, "replaceSync", JS_NewCFunction(ctx, class_noop, "replaceSync", 1));
        JS_SetPropertyStr(ctx, CSp, "replace", JS_NewCFunction(ctx, nb_promise_resolved, "replace", 1));
        JS_SetPropertyStr(ctx, CSp, "insertRule", JS_NewCFunction(ctx, class_noop, "insertRule", 2));
        JS_SetPropertyStr(ctx, CSp, "deleteRule", JS_NewCFunction(ctx, class_noop, "deleteRule", 1));
        JS_SetPropertyStr(ctx, CSf, "prototype", CSp);
        JS_SetPropertyStr(ctx, g, "CSSStyleSheet", CSf);
    }

    /* standard element/event constructor globals: bundles load-time-read
     * X.prototype (Object.create / extends) and branch on instanceof for a
     * long list of names. All inert; prototypes chain onto the hierarchy. */
    {
        static const char *html_els[] = {
            "HTMLTemplateElement", "HTMLUnknownElement", "HTMLScriptElement",
            "HTMLStyleElement", "HTMLDivElement", "HTMLSpanElement",
            "HTMLAnchorElement", "HTMLImageElement", "HTMLInputElement",
            "HTMLButtonElement", "HTMLFormElement", "HTMLLinkElement",
            "HTMLMetaElement", "HTMLHeadElement", "HTMLBodyElement",
            "HTMLHtmlElement", "HTMLCanvasElement", "HTMLVideoElement",
            "HTMLAudioElement", "HTMLMediaElement", "HTMLIFrameElement",
            "HTMLSelectElement", "HTMLOptionElement", "HTMLTextAreaElement",
            "HTMLLabelElement", "HTMLUListElement", "HTMLLIElement",
            "HTMLTableElement", "HTMLParagraphElement", "HTMLHeadingElement",
            "HTMLTitleElement", "HTMLSlotElement", "HTMLPictureElement",
            "HTMLSourceElement", "HTMLTrackElement", "HTMLBRElement",
            "HTMLHRElement", "HTMLPreElement", "HTMLOListElement",
            "HTMLDListElement", "HTMLMenuElement", "HTMLDialogElement"
        };
        for (size_t i = 0; i < sizeof(html_els) / sizeof(html_els[0]); i++)
            add_global_class(ctx, g, html_els[i], Hp);
        static const char *svg_els[] = {
            "SVGSVGElement", "SVGGraphicsElement", "SVGGeometryElement",
            "SVGPathElement", "SVGUseElement", "SVGImageElement",
            "SVGTextElement", "SVGGElement", "SVGRectElement",
            "SVGCircleElement", "SVGDefsElement", "SVGSymbolElement"
        };
        for (size_t i = 0; i < sizeof(svg_els) / sizeof(svg_els[0]); i++)
            add_global_class(ctx, g, svg_els[i], Ep);
        static const char *node_els[] = {
            "DocumentFragment", "ShadowRoot", "CharacterData", "Comment",
            "ProcessingInstruction", "DocumentType", "Attr", "CDATASection"
        };
        for (size_t i = 0; i < sizeof(node_els) / sizeof(node_els[0]); i++)
            add_global_class(ctx, g, node_els[i], Np);
        static const char *plain[] = {
            "NodeList", "HTMLCollection", "DOMTokenList", "NamedNodeMap",
            "MutationObserver", "MutationRecord", "CustomElementRegistry",
            "DOMParser", "XMLSerializer", "Range", "Selection",
            "Event", "CustomEvent", "MouseEvent", "KeyboardEvent",
            "PointerEvent", "TouchEvent", "FocusEvent", "InputEvent",
            "WheelEvent", "UIEvent", "ProgressEvent", "ErrorEvent",
            "MessageEvent", "DragEvent", "AnimationEvent", "TransitionEvent"
        };
        for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); i++)
            add_global_class(ctx, g, plain[i], JS_UNDEFINED);
        /* MutationObserver / DOMParser are newable with instance methods */
        JSValue f = add_class(ctx, "MutationObserver", JS_UNDEFINED);
        JSValue p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "observe", JS_NewCFunction(ctx, class_noop, "observe", 2));
        JS_SetPropertyStr(ctx, p, "disconnect", JS_NewCFunction(ctx, class_noop, "disconnect", 0));
        JS_SetPropertyStr(ctx, p, "takeRecords", JS_NewCFunction(ctx, nb_empty_array, "takeRecords", 0));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "MutationObserver", f);
        f = add_class(ctx, "DOMParser", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "parseFromString", JS_NewCFunction(ctx, nb_empty_object, "parseFromString", 2));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "DOMParser", f);
    }

    JS_FreeValue(ctx, g);
    /* the constructor function refs were consumed by JS_SetPropertyStr on the
     * global; the prototype objects are still ours (dups chain/holder). */
    JS_FreeValue(ctx, Ap);
    JS_FreeValue(ctx, Dp);
    JS_FreeValue(ctx, Tp);
    JS_FreeValue(ctx, Sp);
    JS_FreeValue(ctx, Hp);
    JS_FreeValue(ctx, Ep);
    JS_FreeValue(ctx, Np);
    JS_FreeValue(ctx, ETp);
}

/* ---- canvas 2D ---- */
static JSValue nb_c2d_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return JS_UNDEFINED; }
static JSValue nb_c2d_getImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int w = js_arg_index(ctx, argv, argc, 0);
    int h = js_arg_index(ctx, argv, argc, 1);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    JSValue img = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, img, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, img, "height", JS_NewInt32(ctx, h));
    JSValue len = JS_NewInt32(ctx, 4 * w * h);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, g, "Uint8ClampedArray");
    JS_FreeValue(ctx, g);
    JSValue data = (JS_IsObject(ctor) && !JS_IsNull(ctor))
                       ? JS_CallConstructor(ctx, ctor, 1, &len)
                       : JS_NewArray(ctx);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(data)) { JS_FreeValue(ctx, JS_GetException(ctx)); data = JS_NewArray(ctx); }
    JS_SetPropertyStr(ctx, img, "data", data);
    JS_FreeValue(ctx, len);
    return img;
}
static JSValue nb_c2d_measureText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue m = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, m, "width", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, m, "actualBoundingBoxAscent", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, m, "actualBoundingBoxDescent", JS_NewFloat64(ctx, 0));
    return m;
}
static JSValue nb_c2d_gradient(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue g = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, g, "addColorStop", JS_NewCFunction(ctx, nb_c2d_noop, "addColorStop", 2));
    return g;
}
static JSValue nb_c2d_createPattern(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewObject(ctx);
}
static JSValue nb_c2d_lineDash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewArray(ctx);
}
static JSValue nb_el_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue cached = JS_GetPropertyStr(ctx, this_val, "__nb_ctx");
    if (JS_IsObject(cached)) return cached;
    JS_FreeValue(ctx, cached);
    JSValue c = JS_NewObject(ctx);
    const char *strprops[] = {
        "fillStyle", "strokeStyle", "filter", "font", "textAlign", "textBaseline",
        "lineCap", "lineJoin", "globalCompositeOperation", "shadowColor",
        "letterSpacing", "wordSpacing", "textTransform", "direction"
    };
    for (size_t i = 0; i < sizeof(strprops) / sizeof(strprops[0]); i++)
        JS_SetPropertyStr(ctx, c, strprops[i], JS_NewString(ctx, ""));
    const char *numprops[] = {
        "lineWidth", "globalAlpha", "shadowBlur", "shadowOffsetX", "shadowOffsetY",
        "miterLimit", "lineDashOffset"
    };
    for (size_t i = 0; i < sizeof(numprops) / sizeof(numprops[0]); i++)
        JS_SetPropertyStr(ctx, c, numprops[i], JS_NewInt32(ctx, 0));
    const char *noops[] = {
        "fillRect", "clearRect", "strokeRect", "save", "restore", "translate",
        "rotate", "scale", "setTransform", "transform", "resetTransform",
        "drawImage", "putImageData", "beginPath", "closePath", "moveTo",
        "lineTo", "rect", "arc", "arcTo", "bezierCurveTo", "quadraticCurveTo",
        "fill", "stroke", "clip", "fillText", "strokeText", "setLineDash",
        "reset", "isPointInPath", "setTransformMatrix", "drawFocusIfNeeded"
    };
    for (size_t i = 0; i < sizeof(noops) / sizeof(noops[0]); i++)
        JS_SetPropertyStr(ctx, c, noops[i], JS_NewCFunction(ctx, nb_c2d_noop, noops[i], 0));
    JS_SetPropertyStr(ctx, c, "getImageData", JS_NewCFunction(ctx, nb_c2d_getImageData, "getImageData", 4));
    JS_SetPropertyStr(ctx, c, "createImageData", JS_NewCFunction(ctx, nb_c2d_getImageData, "createImageData", 2));
    JS_SetPropertyStr(ctx, c, "getLineDash", JS_NewCFunction(ctx, nb_c2d_lineDash, "getLineDash", 0));
    JS_SetPropertyStr(ctx, c, "measureText", JS_NewCFunction(ctx, nb_c2d_measureText, "measureText", 1));
    JS_SetPropertyStr(ctx, c, "createLinearGradient", JS_NewCFunction(ctx, nb_c2d_gradient, "createLinearGradient", 4));
    JS_SetPropertyStr(ctx, c, "createRadialGradient", JS_NewCFunction(ctx, nb_c2d_gradient, "createRadialGradient", 6));
    JS_SetPropertyStr(ctx, c, "createPattern", JS_NewCFunction(ctx, nb_c2d_createPattern, "createPattern", 2));
    JS_SetPropertyStr(ctx, this_val, "__nb_ctx", JS_DupValue(ctx, c));
    return c;
}
static JSValue nb_canvas_toDataURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewString(ctx, "data:,");
}

/* ---- DOM class one JS object per C node (see PROTONAME) ---- */

/* Attach the DOM natives to the global `document` object. */
static void install_dom(JSContext *ctx) {
    install_dom_classes(ctx);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, g, "document");
    if (JS_IsObject(doc)) {
        JS_SetPropertyStr(ctx, doc, "getElementById", JS_NewCFunction(ctx, nb_dom_getElementById, "getElementById", 1));
        JS_SetPropertyStr(ctx, doc, "getElementsByTagName", JS_NewCFunction(ctx, nb_dom_getElementsByTagName, "getElementsByTagName", 1));
        JS_SetPropertyStr(ctx, doc, "querySelector", JS_NewCFunction(ctx, nb_dom_querySelector, "querySelector", 1));
        JS_SetPropertyStr(ctx, doc, "querySelectorAll", JS_NewCFunction(ctx, nb_dom_querySelectorAll, "querySelectorAll", 1));
        JS_SetPropertyStr(ctx, doc, "createElement", JS_NewCFunction(ctx, nb_dom_createElement, "createElement", 1));
        JS_SetPropertyStr(ctx, doc, "createElementNS", JS_NewCFunction(ctx, nb_dom_createElementNS, "createElementNS", 2));
        JS_SetPropertyStr(ctx, doc, "createTextNode", JS_NewCFunction(ctx, nb_dom_createTextNode, "createTextNode", 1));
        JS_SetPropertyStr(ctx, doc, "getElementsByClassName", JS_NewCFunction(ctx, nb_dom_getElementsByClassName, "getElementsByClassName", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "documentElement");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_documentElement, "get documentElement", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        {
            JSAtom nm = JS_NewAtom(ctx, "body");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_body, "get body", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        {
            JSAtom nm = JS_NewAtom(ctx, "head");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_head, "get head", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        /* rung-6: document.cookie — file-backed jar. The prelude's empty-jar
         * stub is replaced by real C natives (survive across LOADs because the
         * jar is on disk; each LOAD runs a fresh heap). */
        {
            JSAtom nm = JS_NewAtom(ctx, "cookie");
            JS_DefinePropertyGetSet(ctx, doc, nm,
                JS_NewCFunction(ctx, nb_dom_cookie_get, "get cookie", 0),
                JS_NewCFunction(ctx, nb_dom_cookie_set, "set cookie", 1),
                JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
    }
    JS_FreeValue(ctx, doc);

    /* rung 6: localStorage/sessionStorage — the prelude's twin no-op stubs
     * become two real objects: localStorage (disk jar, survives LOADs) and
     * sessionStorage (in-memory, cleared per LOAD). Fresh objects are created
     * here and replace the globals (redefining a non-configurable stub prop
     * would throw). Both share the shape getItem/setItem/removeItem/clear/key
     * + a length getter. */
    {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "getItem", JS_NewCFunction(ctx, nb_ls_getItem, "getItem", 1));
        JS_SetPropertyStr(ctx, obj, "setItem", JS_NewCFunction(ctx, nb_ls_setItem, "setItem", 2));
        JS_SetPropertyStr(ctx, obj, "removeItem", JS_NewCFunction(ctx, nb_ls_removeItem, "removeItem", 1));
        JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx, nb_ls_clear, "clear", 0));
        JS_SetPropertyStr(ctx, obj, "key", JS_NewCFunction(ctx, nb_ls_key, "key", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "length");
            JS_DefinePropertyGetSet(ctx, obj, nm, JS_NewCFunction(ctx, nb_ls_length, "get length", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, g, "localStorage", obj);
    }
    {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "getItem", JS_NewCFunction(ctx, nb_ss_getItem, "getItem", 1));
        JS_SetPropertyStr(ctx, obj, "setItem", JS_NewCFunction(ctx, nb_ss_setItem, "setItem", 2));
        JS_SetPropertyStr(ctx, obj, "removeItem", JS_NewCFunction(ctx, nb_ss_removeItem, "removeItem", 1));
        JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx, nb_ss_clear, "clear", 0));
        JS_SetPropertyStr(ctx, obj, "key", JS_NewCFunction(ctx, nb_ss_key, "key", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "length");
            JS_DefinePropertyGetSet(ctx, obj, nm, JS_NewCFunction(ctx, nb_ss_length, "get length", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, g, "sessionStorage", obj);
    }
    JS_FreeValue(ctx, g);
}

#define EVAL_BUDGET_SEC 2   /* plan step 5: watchdog for runaway page.js */
/* NB_EVAL_BUDGET env override (row-31): a real 10.8MB single-IIFE bundle
 * legitimately needs more than 2s to parse, so receipt runs can raise the
 * watchdog (NB_EVAL_BUDGET=120); production default stays 2s. */
static int nb_budget(void) {
    static int b = -1;
    if (b < 0) {
        const char *e = getenv("NB_EVAL_BUDGET");
        b = e ? atoi(e) : EVAL_BUDGET_SEC;
        if (b < 1) b = EVAL_BUDGET_SEC;
    }
    return b;
}
#define MAX_DRAIN_MS 800    /* commit 7: bounded wait so short timers fire pre-RENDER */
static void sigalrm(int sig);   /* used by run_event_loop below */

/* ===================== Phase 2 (commit 7): timers + microtasks + events ================ */

#define MAX_TIMERS 2048
#define MAX_EVENTS 4096
#define MAX_ONPROPS 1024
#define MAX_TIMER_INVOCATIONS 5000   /* plan §2 CPU safety */
#define MAX_RAF_FRAMES 120           /* plan §2: ~2s of rAF */
#define RAF_MS 16

typedef struct { int id, active; long interval; uint64_t due; int slot; JSValue cb; } Timer;
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; JSValue cb; } EvL;
/* el.on<type> handlers CANNOT live as data props on the wrapper objects:
 * push_node() creates a fresh JS object per wrap, so the C side must own the
 * callback (held as a JSValue, keyed by node+kind+type). */
typedef struct { int kind; NbNode *node; char type[48]; int slot, active; JSValue cb; } OnProp;

#define EVT_NODE 1
#define EVT_WIN  2
#define EVT_DOC  3

static Timer g_timers[MAX_TIMERS];
static int g_timer_count = 0;
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

/* Format + clear the current context exception into buf; returns buf. */
static const char *js_error_to_cstr(JSContext *ctx, char *buf, size_t bl) {
    JSValue e = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, e);
    if (!s) {
        JS_FreeValue(ctx, e);
        snprintf(buf, bl, "unknown error");
        return buf;
    }
    snprintf(buf, bl, "%s", s);
    JS_FreeCString(ctx, s);
    /* diagnosis aid: NB_STACK=1 appends the JS exception stack so a bare
     * "TypeError: not a function" in a real bundle is traceable. */
    if (getenv("NB_STACK")) {
        JSValue st = JS_GetPropertyStr(ctx, e, "stack");
        if (JS_IsString(st)) {
            const char *stc = JS_ToCString(ctx, st);
            if (stc) {
                snprintf(buf + strlen(buf), bl - strlen(buf), " @ %s", stc);
                JS_FreeCString(ctx, stc);
            }
        }
        JS_FreeValue(ctx, st);
    }
    JS_FreeValue(ctx, e);
    return buf;
}

/* Release every JS callback this file currently holds (timers/events/on-props).
 * Must run BEFORE the owning context is freed; idempotent (slots are set to
 * JS_UNDEFINED after releasing, so a second call / a later per-LOAD reset
 * cannot double-free). */
static void free_held_callbacks(JSContext *ctx) {
    for (int i = 0; i < g_timer_count; i++)
        if (!JS_IsUndefined(g_timers[i].cb) && JS_IsFunction(ctx, g_timers[i].cb)) {
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_evl_count; i++)
        if (!JS_IsUndefined(g_evl[i].cb) && JS_IsFunction(ctx, g_evl[i].cb)) {
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_onprop_count; i++)
        if (!JS_IsUndefined(g_onprop[i].cb) && JS_IsFunction(ctx, g_onprop[i].cb)) {
            JS_FreeValue(ctx, g_onprop[i].cb);
            g_onprop[i].cb = JS_UNDEFINED;
        }
}

/* Run the callback held in `cb` with this=globalThis, 0 args.
 * Returns 0 on success, 1 on thrown error (message captured, exception freed). */
static int invoke_cb0(JSContext *ctx, JSValue cb) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, cb, g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}

/* ---- timers ---- */
static int timer_schedule(JSContext *ctx, JSValueConst cb, long interval, int repeat) {
    if (!JS_IsFunction(ctx, cb) || g_timer_count >= MAX_TIMERS) return 0;
    int slot = g_timer_count++;
    g_timers[slot].id = g_next_id++;
    g_timers[slot].active = 1;
    g_timers[slot].interval = repeat ? (interval > 0 ? interval : 1) : 0;
    g_timers[slot].due = now_ms() + (uint64_t)(interval > 0 ? interval : 1);
    g_timers[slot].slot = slot;
    g_timers[slot].cb = JS_DupValue(ctx, cb);
    return g_timers[slot].id;
}
static JSValue nb_timer_setTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 0) ms = 0;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 0));
}
static JSValue nb_timer_setInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 1) ms = 1;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 1));
}
static void timer_clear(JSContext *ctx, int want_oneshot, JSValueConst idv) {
    if (!JS_IsNumber(idv)) return;
    double d;
    if (JS_ToFloat64(ctx, &d, idv) != 0) return;
    int id = (int)d;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active && g_timers[i].id == id &&
            (want_oneshot ? g_timers[i].interval == 0 : g_timers[i].interval > 0)) {
            g_timers[i].active = 0;
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
            break;
        }
}
static JSValue nb_timer_clearTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 1, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_timer_clearInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 0, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}

/* ---- microtasks + rAF ---- */
/* A queueMicrotask() callback, executed by the QuickJS native job queue
 * (JS_EnqueueJob) inside run_event_loop's drain; `this` = globalThis. */
static JSValue microtask_job(JSContext *ctx, int argc, JSValueConst *argv) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, argv[0], g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        JS_FreeValue(ctx, r);
    } else {
        JS_FreeValue(ctx, r);
    }
    return JS_UNDEFINED;
}
static JSValue nb_queueMicrotask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        JS_ThrowTypeError(ctx, "queueMicrotask requires a function argument");
        return JS_EXCEPTION;
    }
    if (JS_EnqueueJob(ctx, microtask_job, 1, argv) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}
static JSValue nb_raf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc > 0 && JS_IsFunction(ctx, argv[0]) && g_raf_fires < MAX_RAF_FRAMES) {
        g_raf_fires++;
        timer_schedule(ctx, argv[0], RAF_MS, 0);
    }
    return JS_UNDEFINED;
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

/* Resolve a fetch/XHR URL against the current document URL (g_href), the
 * same RFC 3986 §5-style merge the manager does for script srcs:
 *   already-schemed   -> as-is (file:, http:, https:, data:, ...)
 *   //host/[...]      -> scheme of g_href + the rest
 *   /abs/path         -> scheme://host of g_href + the rest
 *   rel/path          -> scheme://host + dirname(g_href's path) + the rest
 * g_href may itself be file:///... (file-based suites keep working). */
static void resolve_doc_url(const char *rel, char *out, size_t olen) {
    out[0] = 0;
    if (!rel || !rel[0]) return;
    const char *sc = strstr(g_href, "://");
    const char *rp = rel;
    while (*rp && *rp != ':' && *rp != '/' && *rp != '?' && *rp != '#') rp++;
    if (*rp == ':') { snprintf(out, olen, "%s", rel); return; }      /* has scheme */
    if (strncmp(rel, "//", 2) == 0 && sc) {
        snprintf(out, olen, "%.*s%s", (int)(sc - g_href + 3), g_href, rel);
        return;
    }
    const char *hp = sc ? sc + 3 : NULL;
    const char *hostslash = hp ? strchr(hp, '/') : NULL;
    if (rel[0] == '/') {
        if (sc && hostslash) snprintf(out, olen, "%.*s%s",
                                      (int)(hostslash - g_href), g_href, rel);
        else if (sc) snprintf(out, olen, "%s%s", g_href, rel);
        else snprintf(out, olen, "%s", rel);
        return;
    }
    if (sc && hp) {
        const char *dlast = strrchr(hp, '/');
        if (dlast && dlast > hp)
            snprintf(out, olen, "%.*s%s", (int)(dlast - g_href + 1), g_href, rel);
        else if (hostslash)
            snprintf(out, olen, "%.*s%s", (int)(hostslash - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s/%s", g_href, rel);
    } else {
        const char *lst = strrchr(g_href, '/');
        if (lst) snprintf(out, olen, "%.*s%s", (int)(lst - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s", rel);
    }
}

static JSValue nb_fetch_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *m_own = NULL, *u_own = NULL, *h_own = NULL, *b_own = NULL;
    const char *method = NULL, *url = NULL, *headers = NULL, *body = NULL;
    if (argc > 0) method = (m_own = JS_ToCString(ctx, argv[0]));
    if (argc > 1) url = (u_own = JS_ToCString(ctx, argv[1]));
    if (argc > 2) headers = (h_own = JS_ToCString(ctx, argv[2]));
    if (argc > 3) body = (b_own = JS_ToCString(ctx, argv[3]));
    if (!method || !url) {
        JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
        JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
        JS_ThrowTypeError(ctx, "nbFetchSync needs method and url strings");
        return JS_EXCEPTION;
    }
    if (!headers) headers = "";
    if (!body) body = "";

    char urlb[2300];
    resolve_doc_url(url, urlb, sizeof(urlb));
    url = urlb;

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
                /* unified cookie jar: attach matching cookies from our jar
                 * (Chromium-parity — one store, both directions). */
                {
                    char ckhdr[4096];
                    cookie_header_for_url(url, ckhdr, sizeof(ckhdr));
                    if (ckhdr[0]) cfg_line(cf, "header", ckhdr);
                }
                /* dump response headers so we can parse Set-Cookie into jar */
                char hdrpath[256] = "";
                {
                    char t3[] = "/tmp/nbfetchhdr.XXXXXX";
                    int fd3 = mkstemp(t3);
                    if (fd3 >= 0) { close(fd3); snprintf(hdrpath, sizeof(hdrpath), "%s", t3); }
                }
                if (hdrpath[0]) cfg_line(cf, "dump-header", hdrpath);
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
                    if (!rb) snprintf(errbuf, sizeof(errbuf),
                                      "curl rc=%d status=%d url=%s",
                                      rc, status, url);
                } else snprintf(errbuf, sizeof(errbuf), "popen curl failed");
                /* ingest Set-Cookie response headers into the unified jar */
                if (hdrpath[0] && status >= 200 && status < 400) {
                    char *hbuf = NULL; size_t hbn = 0;
                    if (read_file(hdrpath, &hbuf, &hbn)) {
                        char *hp = hbuf;
                        while (hp && *hp) {
                            char *nl = strchr(hp, '\n');
                            if (nl) *nl = 0;
                            char *cr = hp + strlen(hp);
                            if (cr > hp && cr[-1] == '\r') cr[-1] = 0;
                            if (strncasecmp(hp, "Set-Cookie:", 11) == 0) {
                                const char *val = hp + 11;
                                while (*val == ' ') val++;
                                cookie_set_from_wire(val, url);
                            }
                            hp = nl ? nl + 1 : NULL;
                        }
                        free(hbuf);
                    }
                }
                unlink(cfgpath);
                unlink(bodypath);
                if (hdrpath[0]) unlink(hdrpath);
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

    alarm(nb_budget());   /* re-arm the budget for the rest of the drain */

    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "ok", JS_NewBool(ctx, status >= 200 && status < 300 && rb != NULL));
    JS_SetPropertyStr(ctx, o, "status", JS_NewInt32(ctx, status));
    JS_SetPropertyStr(ctx, o, "body", JS_NewString(ctx, rb ? rb : ""));
    JS_SetPropertyStr(ctx, o, "error", JS_NewString(ctx, errbuf[0] ? errbuf : ""));
    free(rb);
    JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
    JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
    return o;
}

static uint64_t timer_min_due(void) {
    uint64_t m = 0; int have = 0;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active) { if (!have || g_timers[i].due < m) { m = g_timers[i].due; have = 1; } }
    return have ? m : 0;
}
/* Drain the native job queue (promise reactions + queueMicrotask callbacks). */
static void drain_jobs(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *jctx;
    while (JS_IsJobPending(rt)) {
        if (g_pending_err) return;
        if (JS_ExecutePendingJob(rt, &jctx) < 0) {
            if (jctx && !g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(jctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
        }
    }
}
static int run_due_timers(JSContext *ctx, uint64_t now) {
    int ran = 0;
    for (int i = 0; i < g_timer_count; i++) {
        if (!g_timers[i].active || g_timers[i].due > now) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        long iv = g_timers[i].interval;
        if (invoke_cb0(ctx, g_timers[i].cb)) { g_invocations++; break; }
        g_invocations++; ran = 1;
        if (iv > 0) g_timers[i].due = now + (uint64_t)iv;   /* repeating — re-arm */
        else {
            g_timers[i].active = 0;                          /* oneshot */
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    }
    return ran;
}

/* ---- events (EventTarget add/removeEventListener; dispatch is commit 8) ---- */
/* Fetch an attribute from the JS global object (returns owned value). */
static JSValue get_global_attr(JSContext *ctx, const char *name) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, g, name);
    JS_FreeValue(ctx, g);
    return v;
}
static void evl_add(JSContext *ctx, int kind, NbNode *n, JSValueConst typev, JSValueConst cb) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    if (!type || !type[0] || !JS_IsFunction(ctx, cb) || g_evl_count >= MAX_EVENTS) { JS_FreeCString(ctx, to); return; }
    int slot = g_evl_count++;
    g_evl[slot].kind = kind; g_evl[slot].node = n; g_evl[slot].slot = slot; g_evl[slot].active = 1;
    snprintf(g_evl[slot].type, sizeof(g_evl[slot].type), "%s", type);
    g_evl[slot].cb = JS_DupValue(ctx, cb);
    JS_FreeCString(ctx, to);
}
static void evl_del(JSContext *ctx, int kind, NbNode *n, JSValueConst typev) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    for (int i = 0; i < g_evl_count; i++)
        if (g_evl[i].active && g_evl[i].kind == kind && g_evl[i].node == n &&
            (!type || !type[0] || !strcmp(g_evl[i].type, type))) {
            g_evl[i].active = 0;
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
            break;
        }
    JS_FreeCString(ctx, to);
}
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_NODE, get_this(ctx, this_val),
            argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_NODE, get_this(ctx, this_val), argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_event_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue c;
    if (JS_IsObject(this_val) && !JS_IsException((c = JS_GetPropertyStr(ctx, this_val, "cancelable")))) {
        int can = JS_ToBool(ctx, c);
        if (can < 0) can = 0;
        JS_FreeValue(ctx, c);
        if (can) JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, 1));
    } else if (JS_IsObject(this_val)) JS_FreeValue(ctx, c);
    return JS_UNDEFINED;
}
static JSValue nb_event_stopPropagation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (JS_IsObject(this_val)) JS_SetPropertyStr(ctx, this_val, "propagationStopped", JS_NewBool(ctx, 1));
    return JS_UNDEFINED;
}
/* ---- on-* handlers: el.onclick = fn (C-side registry, per kind+node+type).
 * The wrapper objects carry a generic accessor per key; magic identifies the
 * ONPROPS key (native signature gains `int magic`). */
static int onprop_find(int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_onprop_count; i++) {
        if (!g_onprop[i].active) continue;
        if (g_onprop[i].kind != kind || g_onprop[i].node != n) continue;
        if (strcmp(g_onprop[i].type, type)) continue;
        return i;
    }
    return -1;
}
static const char *onprop_type_from_magic(int magic) {
    if (magic < 0 || !ONPROPS[magic]) return NULL;
    return ONPROPS[magic];
}
static int onprop_set_core(JSContext *ctx, int kind, NbNode *n, int magic, JSValueConst cb) {
    const char *type = onprop_type_from_magic(magic);
    if (!type || !JS_IsFunction(ctx, cb)) return 0;
    int i = onprop_find(kind, n, type);
    if (i < 0) {
        if (g_onprop_count >= MAX_ONPROPS) return 0;
        i = g_onprop_count++;
        g_onprop[i].kind = kind; g_onprop[i].node = n;
        snprintf(g_onprop[i].type, sizeof(g_onprop[i].type), "%s", type);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    } else {
        JS_FreeValue(ctx, g_onprop[i].cb);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    }
    g_onprop[i].active = 1;
    g_onprop[i].slot = i;
    return 0;
}
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_NODE, get_this(ctx, this_val), magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_DOC, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_WIN, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_onprop_get_core(JSContext *ctx, int kind, NbNode *n, int magic) {
    const char *type = onprop_type_from_magic(magic);
    if (!type) return JS_UNDEFINED;
    int i = onprop_find(kind, n, type);
    if (i < 0 || !g_onprop[i].active) return JS_UNDEFINED;
    return JS_DupValue(ctx, g_onprop[i].cb);
}
static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_NODE, get_this(ctx, this_val), magic);
}
static JSValue nb_doc_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_DOC, NULL, magic);
}
static JSValue nb_win_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_WIN, NULL, magic);
}
/* one dispatch level for a single node/window/document: exact evl registrations,
 * then the on-* callback. Each target scans the registers from scratch (an
 * entry's kind+node binds it to exactly one target), so bubbling reaches the
 * document/window levels without a shared cursor skipping their listeners. */
static void dispatch_level(JSContext *ctx, int kind, NbNode *node, JSValue ev,
                           const char *type, int *stopped) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != node) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != node) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
}
static int dispatch_event(JSContext *ctx, int kind, NbNode *node, JSValue ev, int bubbles) {
    JSValue tv = JS_GetPropertyStr(ctx, ev, "type");
    char *to = NULL;
    const char *type = JS_IsString(tv) ? (to = JS_ToCString(ctx, tv)) : NULL;
    JS_FreeValue(ctx, tv);
    if (!type) return 1;
    int stopped = 0;

    if (kind == EVT_NODE && node) {
        JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, node));
        JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, node));
        dispatch_level(ctx, EVT_NODE, node, ev, type, &stopped);
        if (!stopped && bubbles) {               /* bubble node->...->root->document->window */
            NbNode *a = node->parent;
            while (a) {
                if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
                JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, a));
                dispatch_level(ctx, EVT_NODE, a, ev, type, &stopped);
                if (stopped) break;
                a = a->parent;
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
                dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
                dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
            }
        }
    } else if (kind == EVT_DOC) {
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "document"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
        dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
        if (!stopped) {
            JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
            dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
        }
    } else {                                       /* EVT_WIN */
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "window"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
        dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
    }
    JS_FreeCString(ctx, to);
    JSValue dpv = JS_GetPropertyStr(ctx, ev, "defaultPrevented");
    int dp = JS_ToBool(ctx, dpv);
    if (dp < 0) dp = 0;
    JS_FreeValue(ctx, dpv);
    return !dp;
}
static JSValue nb_el_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewBool(ctx, 0);
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_NODE, n, ev, bubbles));
}
static JSValue nb_doc_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_DOC, NULL, ev, bubbles));
}
static JSValue nb_win_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_WIN, NULL, ev, bubbles));
}
static JSValue nb_el_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    JSValue Event = get_global_attr(ctx, "Event");
    if (!JS_IsFunction(ctx, Event)) { JS_FreeValue(ctx, Event); return JS_UNDEFINED; }
    JSValue opts = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, opts, "bubbles", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, opts, "cancelable", JS_NewBool(ctx, 1));
    JSValue cargv[2];
    cargv[0] = JS_NewString(ctx, "click");
    cargv[1] = opts;
    JSValue ev = JS_CallConstructor(ctx, Event, 2, cargv);
    JS_FreeValue(ctx, Event);
    JS_FreeValue(ctx, cargv[0]);
    JS_FreeValue(ctx, opts);
    if (JS_IsException(ev)) { JS_FreeValue(ctx, ev); return JS_UNDEFINED; }
    int r = dispatch_event(ctx, EVT_NODE, n, ev, 1);
    JS_FreeValue(ctx, ev);
    return JS_NewBool(ctx, r);
}
static void fire_event(JSContext *ctx, int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != n) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        if (kind == EVT_NODE && n) JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, n));
        else JS_SetPropertyStr(ctx, ev, "target",
             get_global_attr(ctx, kind == EVT_WIN ? "window" : "document"));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
    /* lifecycle on-* props: window.onload, document.onDOMContentLoaded (commit 8) */
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != n) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
}

/* ---- the loop: lifecycle -> job/timer drain until quiescent or budget ---- */
/* returns nonzero if an event-loop callback threw (caller -> STATUS err) */
static int run_event_loop(JSContext *ctx) {
    signal(SIGALRM, sigalrm);
    alarm(nb_budget());   /* phase-1 backstop also covers timer/job callbacks */
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_DOC, NULL, "DOMContentLoaded");
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_WIN, NULL, "load");
    drain_jobs(ctx);
    uint64_t start = now_ms();
    for (int guard = 0; guard < 100000 && !g_pending_err; guard++) {
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        if (now_ms() - start > MAX_DRAIN_MS) break;   /* bound page_load wait */
        uint64_t now = now_ms();
        int ran = run_due_timers(ctx, now);
        drain_jobs(ctx);
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

/* ==================== rung 6 seam: generic SHA-1 primitive =============
 * Row-34 login: Google's page JS signs SAPISIDHASH =
 *   <ts>_<base64(sha1(<ts> " " <SAPISID> " " <origin>))>
 * by itself (Chromium parity — browsers have no LOGIN op; the site's own
 * script does the crypto). But the page needs a SHA-1 primitive to do so.
 * This is the engine-generic one: `__nb_sha1(utf8)->base64(digest)`, exposed
 * to any page. Not youtube-specific; the page JS handles the SAPISIDHASH
 * composition purely in JS. Raw digest bytes can't round-trip through a JS
 * string, so the digest is pre-encoded ASCII base64 here — verified against openssl
 * in worker_sapisid_test).  Shared impl in nb_sha1.h so fixture servers
 * recompute the SAME digest when verifying a received signature. */
#include "nb_sha1.h"

/* __nb_sha1(str) -> base64 of the 20-byte SHA-1 digest (plain ASCII). */
static JSValue nb_sha1_native(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    size_t n = 0;
    char *own = NULL;
    const char *t = "";
    if (argc > 0) t = (own = JS_ToCStringLen(ctx, &n, argv[0]));
    uint8_t out[20];
    nbsha1((const uint8_t *)t, n, out);
    char b64[29];
    nbsha1_b64_20(out, b64);
    JS_FreeCString(ctx, own);
    return JS_NewString(ctx, b64);
}

static void install_events_timers(JSContext *ctx) {
    JSValue doc = get_global_attr(ctx, "document");
    JS_SetPropertyStr(ctx, doc, "addEventListener",
        JS_NewCFunction(ctx, nb_doc_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "removeEventListener",
        JS_NewCFunction(ctx, nb_doc_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "dispatchEvent",
        JS_NewCFunction(ctx, nb_doc_dispatchEvent, "dispatchEvent", 1));
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, doc, key,
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, doc);
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "__nb_sha1",   JS_NewCFunction(ctx, nb_sha1_native, "__nb_sha1", 1));
    JS_SetPropertyStr(ctx, g, "setTimeout",  JS_NewCFunction(ctx, nb_timer_setTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, g, "setInterval", JS_NewCFunction(ctx, nb_timer_setInterval, "setInterval", 2));
    JS_SetPropertyStr(ctx, g, "clearTimeout",  JS_NewCFunction(ctx, nb_timer_clearTimeout, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, g, "clearInterval", JS_NewCFunction(ctx, nb_timer_clearInterval, "clearInterval", 1));
    JS_SetPropertyStr(ctx, g, "queueMicrotask", JS_NewCFunction(ctx, nb_queueMicrotask, "queueMicrotask", 1));
    JS_SetPropertyStr(ctx, g, "requestAnimationFrame", JS_NewCFunction(ctx, nb_raf, "requestAnimationFrame", 1));
    JS_SetPropertyStr(ctx, g, "nbFetchSync", JS_NewCFunction(ctx, nb_fetch_sync, "nbFetchSync", 4));
    JS_SetPropertyStr(ctx, g, "addEventListener",    JS_NewCFunction(ctx, nb_win_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, g, "removeEventListener", JS_NewCFunction(ctx, nb_win_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, g, "dispatchEvent",       JS_NewCFunction(ctx, nb_win_dispatchEvent, "dispatchEvent", 1));
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, g, key,
            JS_NewCFunctionMagic(ctx, nb_win_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_win_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, g);
}

/* boot hygiene (2026-09-09): install_dom / install_events_timers ran under a
 * protected call so a throw was reported to stderr as WERR| and the
 * page still loaded. QuickJS property definitions don't throw on non-writable
 * targets (JS_DefinePropertyGetSet just returns FALSE), so the direct calls
 * are enough; keep the name for call-site clarity. */
static void boot_install_safe(JSContext *ctx) {
    install_dom(ctx);
    install_events_timers(ctx);
}

/* plan step 5: CPU budget for script eval. If page.js burns through
 * EVAL_BUDGET_SEC of CPU (while(true) {} and friends) SIGALRM fires while
 * the engine is running; the deadly default _exit kills the worker mid-eval, the
 * manager sees the socket close and respawns on the next LOAD. */
static void sigalrm(int sig) { _exit(128 + sig); }
/* Evaluate src with a SIGALRM CPU budget. Returns 0 on success; on error
 * returns 1 and, if errbuf is non-NULL, formats the exception message into
 * it (the exception is always consumed/cleared). The completion value is
 * discarded — use peval_budget_value() when the result is wanted. */
static int peval_budget(JSContext *ctx, const char *src, size_t src_n,
                        char *errbuf, size_t errlen) {
    signal(SIGALRM, sigalrm);
    alarm(nb_budget());
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    alarm(0);
    if (JS_IsException(r)) {
        if (errbuf && errlen) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            snprintf(errbuf, errlen, "%s", m);
        } else {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
/* Like peval_budget but returns the completion JSValue (owned; freed by the
 * caller — also on JS_EXCEPTION). No exception is consumed here. */
static JSValue peval_budget_value(JSContext *ctx, const char *src, size_t src_n) {
    signal(SIGALRM, sigalrm);
    alarm(nb_budget());
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    alarm(0);
    return r;
}
/* Format a value for console display (duk_safe_to_string semantics); clears
 * any exception a throwing toString() may have left behind. */
static const char *js_display_cstr(JSContext *ctx, JSValueConst v, char *buf, size_t bl) {
    if (JS_IsUndefined(v)) { snprintf(buf, bl, "undefined"); return buf; }
    char *s = JS_ToCString(ctx, v);
    if (!s) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
        snprintf(buf, bl, "undefined");
        return buf;
    }
    snprintf(buf, bl, "%s", s);
    JS_FreeCString(ctx, s);
    if (JS_HasException(ctx)) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
    }
    return buf;
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
    nb_css_free(g_css);
    g_css = NULL;
}

/* devtools console EVAL: tear down everything a previous LOAD left live
 * (resident QuickJS runtime + its DOM tree). run_page() keeps those alive on
 * the success path so an eval:<js> snippet can run against the page; this
 * helper is the disciplined cleanup for the next LOAD, errors and QUIT. */
static void live_teardown(void) {
    if (g_live_ctx) {
        free_held_callbacks(g_live_ctx);   /* release refs before the heap dies */
        JS_FreeContext(g_live_ctx);
        g_live_ctx = NULL;
    }
    if (g_live_rt) {
        JS_FreeRuntime(g_live_rt);
        g_live_rt = NULL;
    }
    dom_teardown();
}

/* phase-2 (2026-09-09): document-order script runs. The manager writes one
 * <script> (inline or src-fetched) per slice of page.js, separated by the
 * SCRIPT_BOUNDARY sentinel. Each slice is compiled and run as its own
 * program — browser classic-script parity: a syntax error in slice 2 does
 * not stop slices 1/3, top-level `var` still lands on the shared global,
 * and external <script src> executes at its DOM position. Per-slice
 * failures print to stderr (surfaced by the manager as WERR|/[worker]).
 * A legacy page.js without any sentinel is treated as one program. */
#define SCRIPT_BOUNDARY "/*nbjs-script-boundary*/"
static const char *find_script_boundary(const char *p, const char *end,
                                        const char **after) {
    const char *q = p;
    for (; q + sizeof(SCRIPT_BOUNDARY) - 1 <= end; q++) {
        if (q[0] == '/' && q[1] == '*' &&
            memcmp(q, SCRIPT_BOUNDARY, sizeof(SCRIPT_BOUNDARY) - 1) == 0) {
            *after = q + sizeof(SCRIPT_BOUNDARY) - 1;
            return q;
        }
    }
    return NULL;
}
static void run_scripts_slices(JSContext *ctx, char *src, size_t src_n) {
    const char *p = src, *end = src + src_n;
    const char *after = NULL;
    char errbuf[512];
    if (!find_script_boundary(p, end, &after)) {
        /* legacy single-program page.js */
        if (peval_budget(ctx, src, src_n, errbuf, sizeof(errbuf)) != 0)
            fprintf(stderr, "WERR| script 0: %s\n", errbuf[0] ? errbuf : "eval error");
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
            if (peval_budget(ctx, p, slice, errbuf, sizeof(errbuf)) != 0)
                fprintf(stderr, "WERR| script %d: %s\n", idx, errbuf[0] ? errbuf : "eval error");
        }
        idx++;
        if (!bn) break;
        p = next;
    }
}
static void run_page(void) {
    /* phase-2 (commit 7): per-page event/timer/microtask state. The previous
     * LOAD's held callbacks belong to the old heap — release them FIRST, then
     * reset the counters so live_teardown()'s free_held_callbacks is a no-op. */
    if (g_live_ctx) free_held_callbacks(g_live_ctx);
    g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
    g_next_id = 1; g_invocations = 0; g_raf_fires = 0;
    g_pending_err = 0; g_pending_errmsg[0] = 0;

    /* rung-6 slice 2: only the daemon (manager) can act on navigation. */
    g_nav_kind[0] = 0; g_nav_url[0] = 0; g_nav_count = 1;
    g_nav_emit = !g_cli;

    /* rung-6: sessionStorage is per-LOAD — fresh session for this page. */
    g_ss_count = 0;

    /* devtools console EVAL: a previous LOAD may still have its resident
     * heap + DOM alive; tear it down before building this page. */
    live_teardown();

    g_dom_root = NULL;
    g_orphans = NULL;
    g_css = NULL;
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

    /* rung 7: parse the LOAD-delivered stylesheet into a rule cache.
     * nb_css_parse accepts empty files (nr==0), so no special casing
     * needed when there are no inline <style> blocks. */
    if (g_style_css[0]) {
        char *sc = NULL; size_t sn = 0;
        if (read_file(g_style_css, &sc, &sn)) {
            g_css = nb_css_parse(sc, sn);
            free(sc);
        }
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) { live_teardown(); send_status("STATUS err:heap"); return; }
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) { JS_FreeRuntime(rt); live_teardown(); send_status("STATUS err:heap"); return; }
    g_live_rt = rt;
    g_live_ctx = ctx;   /* resident from here on — kept alive for EVAL */
    install_host(ctx);

    /* rung 7: the resident worker's CSS resolve wins over install_host's
     * minimal __nb_ges default (the prelude reads __nb_ges at call time). */
    JSValue ge = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, ge, "__nb_ges",
                      JS_NewCFunction(ctx, nb_ges_rich, "__nb_ges", 1));
    JS_FreeValue(ctx, ge);

    /* rung-6 prelude: swallow its own failure, page continues */
    (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);

    boot_install_safe(ctx);

    char *src = NULL;
    size_t src_n = 0;
    if (!read_file_big(g_page_js, &src, &src_n)) {
        send_status("STATUS err:cannot read page.js");
        live_teardown();
        return;
    }
    if (src_n == 0) { free(src); send_status("STATUS ok"); return; }

    run_scripts_slices(ctx, src, src_n);
    free(src);
    /* phase-2 (commit 7): lifecycle + event loop — timers/microtasks now fire */
    int ev_err = run_event_loop(ctx);
    if (ev_err) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s",
                 g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
        send_status(msg);
        live_teardown();
        return;
    }

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

    /* rung-6 slice 2: if the page asked to navigate, ship a NAV frame so the
     * manager follows it through its own fetch/stack machinery (manager
     * worker_load captures it, the main loop consumes it next tick). */
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')  /* BACK/FORWARD: step count */
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                          g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay)) {
            char *ppay = malloc((size_t)pn + 1);
            if (ppay) {
                memcpy(ppay, pay, (size_t)pn); ppay[pn] = 0;
                send_payload(ppay, (size_t)pn);
                free(ppay);
            }
        }
    }

    /* devtools console EVAL: keep the heap + DOM tree resident so an
     * eval:<js> snippet can run against this page context (see cmd_eval);
     * live_teardown() reclaims it at the next LOAD / QUIT / error. */
    send_status("STATUS ok");
}

/* devtools console EVAL: run a <js> snippet against the resident page heap
 * (g_live_ctx). Source + result (+ any console.* noise) echo to the console
 * capture (g_out / NBW_CONSOLE); post-eval DOM rows are re-emitted so JS
 * mutations show in the browser; an eval-triggered navigation ships a NAV
 * frame. Budget-guarded like page scripts (peval_budget / EVAL_BUDGET_SEC). */
static void cmd_eval(const char *js) {
    if (!g_live_ctx) { send_status("STATUS err:no page loaded"); return; }
    if (g_out) { fprintf(g_out, ">%s\n", js); fflush(g_out); }
    JSValue rv = peval_budget_value(g_live_ctx, js, strlen(js));
    if (JS_IsException(rv)) {
        char tmp[512];
        const char *m = js_error_to_cstr(g_live_ctx, tmp, sizeof(tmp));
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "eval error");
        if (g_out) { fprintf(g_out, "!>%s\n", m ? m : "eval error"); fflush(g_out); }
        JS_FreeValue(g_live_ctx, rv);
        send_status(msg);
        return;
    }
    char disp[512];
    const char *r = js_display_cstr(g_live_ctx, rv, disp, sizeof(disp));
    if (g_out) { fprintf(g_out, "=>%s\n", r ? r : ""); fflush(g_out); }
    JS_FreeValue(g_live_ctx, rv);

    /* re-emit post-eval RENDER rows so mutated DOM is reflected */
    SB rr = {0, 0, 0};
    dom_render_rows(&rr);
    if (rr.s && rr.s[0]) {
        size_t rn = strlen(rr.s);
        if (rn < RENDER_MAX) {
            char *pay = malloc(7 + rn + 1);
            if (pay) {
                memcpy(pay, "RENDER\n", 7);
                memcpy(pay + 7, rr.s, rn);
                pay[7 + rn] = 0;
                send_payload(pay, 7 + rn);
                free(pay);
            }
        }
    }
    free(rr.s);

    /* an eval can navigate too (e.g. location.href = "...") */
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                          g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay))
            send_payload(pay, (size_t)pn);
    }
    send_status("STATUS ok");
}

/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon. */
/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon, but
 * `duk -i` forces the REPL even when stdin is piped. */
/* CLI-2/CLI-3 native hook used by both the REPL and node mode. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void      nb_install_fs(JSContext *ctx);
static const char g_cjs_prelude[];
static int repl_main(void) {
    JSRuntime *rt = JS_NewRuntime();
    if (!rt) return 1;
    JSContext *ctx = rt ? JS_NewContext(rt) : NULL;
    if (!ctx) { if (rt) JS_FreeRuntime(rt); return 1; }
    g_cli = 1; g_cli_log = 1;
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);
    install_host(ctx);
    (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);
    /* CLI-2/CLI-3 in the REPL too: require() + fs work line-by-line,
     * sharing the browser prelude above (window/document still present). */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
    nb_install_fs(ctx);
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "err:%s\n", errbuf[0] ? errbuf : "loader error");
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    char cwd_buf[PATH_MAX];
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) snprintf(cwd_buf, sizeof(cwd_buf), ".");
    JSValue av[2];
    av[0] = JS_NewString(ctx, cwd_buf);
    av[1] = JS_NewString(ctx, "[repl]");
    JSValue g0 = JS_GetGlobalObject(ctx);
    signal(SIGALRM, sigalrm);
    alarm(nb_budget());
    JSValue rr = JS_Call(ctx, install, g0, 2, av);
    JS_FreeValue(ctx, g0);
    JS_FreeValue(ctx, install);
    alarm(0);
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "err:%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JS_FreeValue(ctx, rr);
    static const char empty_html[] = "<html><body></body></html>";
    g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    boot_install_safe(ctx);

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
        free_held_callbacks(ctx);
        g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
        g_invocations = 0; g_raf_fires = 0;
        g_pending_err = 0; g_pending_errmsg[0] = 0;

        /* CLI-4: single-line ESM (import/export) → CJS in the REPL too.
         * Multi-line import/export statements are out of scope here. */
        JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
        if (JS_IsFunction(ctx, prep)) {
            JSValue av2[1]; av2[0] = JS_NewString(ctx, line);
            JSValue g1 = JS_GetGlobalObject(ctx);
            JSValue rp = JS_Call(ctx, prep, g1, 1, av2);
            JS_FreeValue(ctx, g1);
            JS_FreeValue(ctx, av2[0]);
            if (!JS_IsException(rp)) {
                if (JS_IsString(rp)) {
                    size_t sl;
                    char *ps = JS_ToCStringLen(ctx, &sl, rp);
                    if (ps) {
                        if (sl < sizeof(line)) { memcpy(line, ps, sl); line[sl] = 0; }
                        JS_FreeCString(ctx, ps);
                    }
                }
            } else {
                JSValue e = JS_GetException(ctx);
                JS_FreeValue(ctx, e);
            }
            JS_FreeValue(ctx, rp);
        }
        JS_FreeValue(ctx, prep);

        JSValue rv = peval_budget_value(ctx, line, strlen(line));
        if (JS_IsException(rv)) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            printf("err:%s\n", m ? m : "script error");
        } else {
            if (!JS_IsUndefined(rv)) {
                char disp[512];
                const char *s = js_display_cstr(ctx, rv, disp, sizeof(disp));
                printf("%s\n", s ? s : "");
            }
        }
        JS_FreeValue(ctx, rv);
        fflush(stdout);

        /* drain jobs + timers (no lifecycle events), CPU-bounded */
        signal(SIGALRM, sigalrm);
        alarm(nb_budget());
        uint64_t start = now_ms();
        for (int guard = 0; guard < 10000 && !g_pending_err; guard++) {
            if (now_ms() - start > 200) break;
            uint64_t now = now_ms();
            int ran = run_due_timers(ctx, now);
            drain_jobs(ctx);
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
    free_held_callbacks(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    dom_teardown();
    return 0;
}

/* ---- CLI-1: node-like runner (nbjs file.js [args...]) ---- */
static JSValue nb_cli_stdout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (g_out) { fputs(s ? s : "", g_out); fflush(g_out); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static JSValue nb_cli_stderr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (s) { fputs(s, stderr); fflush(stderr); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
/* console.error -> stderr (node parity; log/info/warn stay on stdout) */
static JSValue nb_cli_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        char *own = NULL;
        const char *s = (own = JS_ToCString(ctx, argv[i]));
        if (s) fputs(s, stderr);
        JS_FreeCString(ctx, own);
        if (i + 1 < argc) fputs(" ", stderr);
    }
    fputs("\n", stderr);
    fflush(stderr);
    return JS_UNDEFINED;
}
/* CLI-2: read a module file for require(). Returns the source string, or
 * undefined if the path is missing/unreadable (loader turns that into a
 * "Cannot find module" error). Same 512 kB cap as the page loader. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    JSValue r;
    if (p && p[0] && read_file(p, &s, &n)) {
        r = JS_NewStringLen(ctx, s, n);
        free(s);
    } else {
        r = JS_UNDEFINED;
    }
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_cli_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int code = 0;
    if (argc > 0 && JS_IsNumber(argv[0])) {
        double d;
        if (JS_ToFloat64(ctx, &d, argv[0]) == 0) code = (int)d;
    }
    if (g_out) fflush(g_out);
    exit(code);
    return JS_UNDEFINED;
}
static JSValue nb_cli_cwd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) return JS_NewString(ctx, buf);
    return JS_NewString(ctx, "/");
}
/* ---- CLI-3: fs-lite natives (sync-only, over the same read_file cap).
 * String-only payloads (no Buffer type); an optional encoding arg is
 * accepted so node-style call sites keep working. */
static JSValue nb_fs_readfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    if (!p || !p[0] || !read_file(p, &s, &n)) {
        char eb[512];
        snprintf(eb, sizeof(eb), "ENOENT: cannot read '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    JSValue r = JS_NewStringLen(ctx, s, n);
    free(s);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_writefile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "wb");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot write '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_appendfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "ab");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot append '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue r = JS_NewBool(ctx, p && p[0] && access(p, F_OK) == 0);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    if (!p || !p[0]) {
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "EINVAL: empty mkdir path"));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", p);
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static void nb_install_fs(JSContext *ctx) {
    JSValue fs = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fs, "readFileSync",   JS_NewCFunction(ctx, nb_fs_readfile, "readFileSync", 1));
    JS_SetPropertyStr(ctx, fs, "writeFileSync",  JS_NewCFunction(ctx, nb_fs_writefile, "writeFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "appendFileSync", JS_NewCFunction(ctx, nb_fs_appendfile, "appendFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "existsSync",     JS_NewCFunction(ctx, nb_fs_exists, "existsSync", 1));
    JS_SetPropertyStr(ctx, fs, "mkdirSync",      JS_NewCFunction(ctx, nb_fs_mkdir, "mkdirSync", 1));
    JSValue fsg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, fsg, "__nb_fs", fs);
    JS_FreeValue(ctx, fsg);
}

/* build a JS object from the process environment (not the full sys env —
 * see getenv below). Env exposure is opt-in via require('os')-free helper;
 * CLI-1 keeps it simple: expose a snapshot under process.env. */
static void nb_cli_install(JSContext *ctx, int argc, char **argv) {
    JSValue process = JS_NewObject(ctx);
    /* argv — node convention: [interpreter, script, args...] */
    JSValue argv_arr = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++) {
        JS_SetPropertyUint32(ctx, argv_arr, (uint32_t)i, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, process, "argv", argv_arr);

    /* env (snapshot of environ, ENAME="value" pairs) */
    JSValue env_obj = JS_NewObject(ctx);
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq) continue;
        char *k = strndup(*e, (size_t)(eq - *e));
        if (k) { JS_SetPropertyStr(ctx, env_obj, k, JS_NewString(ctx, eq + 1)); free(k); }
    }
    JS_SetPropertyStr(ctx, process, "env", env_obj);
    JS_SetPropertyStr(ctx, process, "cwd", JS_NewCFunction(ctx, nb_cli_cwd, "cwd", 0));
    /* stdout / stderr — each a small object with write() */
    JSValue so = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, so, "write", JS_NewCFunction(ctx, nb_cli_stdout, "write", 1));
    JS_SetPropertyStr(ctx, process, "stdout", so);
    JSValue se = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, se, "write", JS_NewCFunction(ctx, nb_cli_stderr, "write", 1));
    JS_SetPropertyStr(ctx, process, "stderr", se);
    JS_SetPropertyStr(ctx, process, "exit", JS_NewCFunction(ctx, nb_cli_exit, "exit", 1));

    /* expose as global `process` */
    JSValue pg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, pg, "process", process);
    JS_FreeValue(ctx, pg);
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

/* CLI-2 CommonJS loader + CLI-4 source-level ESM transpiler. Written in
 * conservative ES5.1 (no arrows/let): relative/absolute
 * resolution, module/exports wrapper, JSON require, cycles, and a line-
 * based import/export → CJS rewrite. Exposes `require`/`__dirname`/`__filename`
 * plus `__nb_esm_prepare(src)` for the C entry hook and REPL. */
static const char g_cjs_prelude[] =
"/* NB-JS loader (CLI-2 CJS + CLI-4 ESM transpile). */\n"
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
"\n"
"/* --- CLI-4: ESM source-level transpile ---\n"
" * Heuristic: a line starting (after ws) with `import` or `export` triggers\n"
" * transpile. Handles: default/named/namespace/side-effect imports,\n"
" * function/variable/const/default/named/re-export/export-star-from.\n"
" * Output uses `var` (conservative). Does NOT support multi-line imports,\n"
" * decorators, type annotations, or dynamic `import()`. */\n"
"function esmLooks(s){\n"
"  return /(^|\\n)\\s*(import|export)\\b/.test(s);\n"
"}\n"
"\n"
"function esmTranspile(src){\n"
"  var lines = src.split('\\n');\n"
"  var out = [];\n"
"  var eq = [];     /* deferred: exports.X = expr; at end */\n"
"  var nsc = [];    /* export * from copy-loops */\n"
"  out.push(\"Object.defineProperty(exports,'__esModule',{value:true});\");\n"
"  function tr(s){ return s.replace(/^\\s+|\\s+$/g, ''); }\n"
"  for (var i = 0; i < lines.length; i++){\n"
"    var line = lines[i];\n"
"    var t = tr(line);\n"
"    var lead = line.slice(0, line.length - line.replace(/^\\s+/, '').length);\n"
"    var m;\n"
"    /* import * as ns from '...'; */\n"
"    m = t.match(/^import\\s*\\*\\s*as\\s+([A-Za-z_$][\\w$]*)\\s+from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){ out.push(lead+'var '+m[1]+' = require('+m[2]+m[3]+m[2]+');'); continue; }\n"
"    /* import { a, b as c } from '...'; */\n"
"    m = t.match(/^import\\s*\\{([^}]+)\\}\\s*from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad import spec at line '+(i+1));\n"
"        var loc = nm[1], al = am ? am[2] : nm[1];\n"
"        out.push(lead+'var '+al+' = require('+m[2]+m[3]+m[2]+').'+loc+';');\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* import X from '...'; — node interop: unwrap .default if __esModule */\n"
"    m = t.match(/^import\\s+([A-Za-z_$][\\w$]*)\\s+from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      out.push(lead+'var '+m[1]+' = (function(__nb_m){ return (__nb_m && __nb_m.__esModule) ? __nb_m.default : __nb_m; })(require('+m[2]+m[3]+m[2]+'));');\n"
"      continue;\n"
"    }\n"
"    /* import '...'; */\n"
"    m = t.match(/^import\\s+(['\"])([^'\"]+)\\1;?$/);\n"
"    if (m){ out.push(lead+'require('+m[1]+m[2]+m[1]+');'); continue; }\n"
"    /* export * from '...'; */\n"
"    m = t.match(/^export\\s*\\*\\s*from\\s+(['\"])([^'\"]+)\\1;?$/);\n"
"    if (m){\n"
"      var v = '__nb_ns_'+i;\n"
"      nsc.push({lead:lead,pkg:m[2],v:v});\n"
"      out.push(lead+'var '+v+' = require('+m[1]+m[2]+m[1]+');');\n"
"      continue;\n"
"    }\n"
"    /* export { x, y as z } from '...'; */\n"
"    m = t.match(/^export\\s*\\{([^}]+)\\}\\s*from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad re-export spec at line '+(i+1));\n"
"        var loc = nm[1], al = am ? am[2] : nm[1];\n"
"        eq.push({l:al, r:'require('+m[2]+m[3]+m[2]+').'+loc});\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* export { a, b as c }; */\n"
"    m = t.match(/^export\\s*\\{([^}]+)\\};?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad export spec at line '+(i+1));\n"
"        var al = am ? am[2] : nm[1], loc = nm[1];\n"
"        eq.push({l:al, r:loc});\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* export default function name(...) { ... } — keep the body flowing */\n"
"    m = t.match(/^export\\s+default\\s+function\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+default\\s+/, ''));\n"
"      eq.push({l:'default', r:m[1]});\n"
"      continue;\n"
"    }\n"
"    /* export default function (...) { ... } — anonymous, name it */\n"
"    m = t.match(/^export\\s+default\\s+function\\b/);\n"
"    if (m){\n"
"      var dname = '__nb_default_'+i;\n"
"      out.push(lead+line.replace(/^export\\s+default\\s+function/, 'function '+dname));\n"
"      eq.push({l:'default', r:dname});\n"
"      continue;\n"
"    }\n"
"    /* export default expr */\n"
"    m = t.match(/^export\\s+default\\s+(.+)$/);\n"
"    if (m){ eq.push({l:'default', r:'('+m[1]+')'}); continue; }\n"
"    /* export function f(...)... */\n"
"    m = t.match(/^export\\s+function\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+function/, 'function'));\n"
"      eq.push({l:m[1], r:m[1]});\n"
"      continue;\n"
"    }\n"
"    /* export var/const f = ... */\n"
"    m = t.match(/^export\\s+(var|const)\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+(var|const)/, '$1'));\n"
"      eq.push({l:m[2], r:m[2]});\n"
"      continue;\n"
"    }\n"
"    out.push(line);\n"
"  }\n"
"  for (var ei = 0; ei < eq.length; ei++)\n"
"    out.push('exports.'+eq[ei].l+' = '+eq[ei].r+';');\n"
"  for (var ci = 0; ci < nsc.length; ci++){\n"
"    var c = nsc[ci];\n"
"    out.push(c.lead+'var __nb_keys_'+ci+' = Object.keys('+c.v+');');\n"
"    out.push(c.lead+'for(var __nb_j_'+ci+'=0; __nb_j_'+ci+'<__nb_keys_'+ci+'.length; __nb_j_'+ci+'++){');\n"
"    out.push(c.lead+'  var __nb_k = __nb_keys_'+ci+'[__nb_j_'+ci+'];');\n"
"    out.push(c.lead+'  if(!(__nb_k in exports) && __nb_k !== \\'default\\' && __nb_k !== \\'__esModule\\') exports[__nb_k] = '+c.v+'[__nb_k];');\n"
"    out.push(c.lead+'}');\n"
"  }\n"
"  return out.join('\\n');\n"
"}\n"
"\n"
"function esmPrepare(src){\n"
"  if (esmLooks(src)) return esmTranspile(src);\n"
"  return src;\n"
"}\n"
"\n"
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
"    src = esmPrepare(src);\n"
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
"  this.module = { id: entryFile, filename: entryFile, exports: {} };\n"
"  this.exports = this.module.exports;\n"
"};\n"
"this.__nb_esm_prepare = esmPrepare;\n"
"})();\n";

static int cli_main(int argc, char **argv) {
    g_cli = 1; g_cli_log = 1;         /* bare console lines -> stdout */
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);

    /* parse: duk [--node] file.js|-- [args...] */
    int script_i = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
            fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
            fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
            return 2;
        }
        if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) continue;
        script_i = i; break;
    }
    if (script_i < 0) {
        fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
        fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
        fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
        return 2;
    }
    const char *pg = argv[script_i];
    if (strcmp(pg, "-") != 0 && access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) return 1;
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) { JS_FreeRuntime(rt); return 1; }

    /* node-like host: console/print only, plus process. No DOM, no events,
     * no timers, no browser prelude — window/document/location are absent. */
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue console = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, console, "log",   JS_NewCFunction(ctx, native_log, "log", 1));
        JS_SetPropertyStr(ctx, console, "info",  JS_NewCFunction(ctx, native_log, "info", 1));
        JS_SetPropertyStr(ctx, console, "warn",  JS_NewCFunction(ctx, native_log, "warn", 1));
        JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, nb_cli_error, "error", 1));
        JS_SetPropertyStr(ctx, g, "print",  JS_NewCFunction(ctx, native_log, "print", 1));
        JS_SetPropertyStr(ctx, g, "console", console);
        JS_FreeValue(ctx, g);
    }

    /* process.argv = [interp, script, args...] (node convention) */
    {
        int nargv = 1 + (argc - script_i);
        char **a = calloc((size_t)nargv, sizeof(char *));
        if (!a) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
        a[0] = argv[0];
        for (int i = 0; i + script_i < argc; i++) a[i + 1] = argv[script_i + i];
        nb_cli_install(ctx, nargv, a);
        free(a);
    }

    /* CLI-2 CommonJS + CLI-3 fs: the only host hook the JS loader needs is
     * a module file reader; it defines `require`/`__dirname`/`__filename`
     * itself. `require('fs')` resolves to the fs-lite natives below. */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
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
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "loader error");
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue av[2];
    av[0] = JS_NewString(ctx, entry_dir);
    av[1] = JS_NewString(ctx, entry_file);
    JSValue g1 = JS_GetGlobalObject(ctx);
    signal(SIGALRM, sigalrm);
    alarm(nb_budget());
    JSValue rr = JS_Call(ctx, install, g1, 2, av);
    JS_FreeValue(ctx, g1);
    JS_FreeValue(ctx, install);
    alarm(0);
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JS_FreeValue(ctx, rr);

    char *src = NULL; size_t n = 0;
    if (strcmp(pg, "-") == 0) {
        /* read the whole script from stdin */
        {
            size_t cap = 1 << 16, len = 0;
            char *b = malloc(cap);
            if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
            for (;;) {
                size_t got = fread(b + len, 1, cap - len, stdin);
                len += got;
                if (len == cap) { cap *= 2; b = realloc(b, cap); if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; } }
                if (feof(stdin) || got == 0) break;
            }
            b[len] = 0; src = b; n = len;
        }
    } else if (!read_file(pg, &src, &n)) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 2;
    }

    /* CLI-4: source-level ESM — ask the loader's __nb_esm_prepare for
     * transpiled source (or the original unchanged). The loader prelude
     * must have run already (__nb_install_cjs installs it). */
    JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
    if (JS_IsFunction(ctx, prep)) {
        JSValue av2[1]; av2[0] = JS_NewStringLen(ctx, src, n);
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue rp = JS_Call(ctx, prep, g2, 1, av2);
        JS_FreeValue(ctx, g2);
        if (JS_IsException(rp)) {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        } else if (JS_IsString(rp)) {
            size_t sl;
            char *ps = JS_ToCStringLen(ctx, &sl, rp);
            if (ps) {
                char *ns = malloc(sl + 1);
                if (ns) { memcpy(ns, ps, sl + 1); free(src); src = ns; n = sl; }
                JS_FreeCString(ctx, ps);
            }
        }
        JS_FreeValue(ctx, rp);
        JS_FreeValue(ctx, av2[0]);
    }
    JS_FreeValue(ctx, prep);

    /* Strip a leading shebang line (the page runner has the same rule), then
     * run the module body with a CPU budget. */
    if (n > 2 && src[0] == '#' && src[1] == '!') {
        char *nl = strchr(src, '\n');
        if (nl) { size_t off = (size_t)(nl + 1 - src); n -= off; memmove(src, nl + 1, n); src[n] = 0; }
    }
    if (peval_budget(ctx, src, n, errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "script error");
        free(src);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    free(src);
    if (g_out) fflush(g_out);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

int main(int argc, char **argv) {
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */
    {
        const char *nbw_out = getenv("NBW_CONSOLE");
        if (nbw_out && nbw_out[0]) {
            /* append ("ab") — the manager keeps a live console panel across
             * worker respawns and trims the file itself (trim_tail_file) */
            g_out = fopen(nbw_out, "ab");
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
            live_teardown();
            break;
        } else if (strcmp(cmd, "LOAD") == 0) {
            g_title[0] = 0; g_href[0] = 0;
            g_style_css[0] = 0;
            if (f[1]) snprintf(g_page_js, sizeof(g_page_js), "%s", f[1]);
            if (f[2]) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", f[2]);
            if (f[3]) snprintf(g_href, sizeof(g_href), "%s", f[3]);
            if (f[4]) snprintf(g_title, sizeof(g_title), "%s", f[4]);
            if (f[5]) snprintf(g_style_css, sizeof(g_style_css), "%s", f[5]);
            run_page();
        } else if (strcmp(cmd, "EVAL") == 0) {
            /* devtools console: eval:<js> — js is field 1 (address-bar single
             * line; embedded '\n' is split out by split_lines, fine for REPL) */
            cmd_eval(f[1] ? f[1] : "");
        } else {
            send_status("STATUS err:unknown command");
        }
    }
    return 0;
}
