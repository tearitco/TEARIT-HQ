/* nb_worker_treewalker.c - TreeWalker/NodeFilter helpers (mechanical extract, single-TU include) */
/* ---- TreeWalker / NodeFilter (row-31): webcomponents-lite.js builds two
 * walkers at module init (`M=document.createTreeWalker(document,
 * NodeFilter.SHOW_ALL,null,!1)`, `N=...SHOW_ELEMENT...`) and its ShadyDOM
 * layer drives Node.prototype accessors through them (M.currentNode=this;
 * M.parentNode()/firstChild()/...). Implemented against the real C tree. */
#define WALK_CUR  "__nbw_cur"
#define WALK_SHOW "__nbw_show"
#define WALK_FILT "__nbw_filt"

static int walker_accept(JSContext *ctx, JSValueConst walker, NbNode *n) {
    if (!n) return 0;
    JSValue sv = JS_GetPropertyStr(ctx, walker, WALK_SHOW);
    uint32_t show = 0xFFFFFFFFu;
    if (JS_IsNumber(sv)) { uint32_t v = 0xFFFFFFFFu; JS_ToUint32(ctx, &v, sv); show = v; }
    JS_FreeValue(ctx, sv);
    int bit = (n->tag && n->tag[0]) ? 1 : 4;   /* SHOW_ELEMENT vs SHOW_TEXT */
    if (!(show & (uint32_t)bit)) return 0;
    JSValue f = JS_GetPropertyStr(ctx, walker, WALK_FILT);
    int ok = 1;
    if (JS_IsFunction(ctx, f)) {
        JSValue node = push_node(ctx, n);
        JSValue r = JS_Call(ctx, f, walker, 1, (JSValueConst *)&node);
        JS_FreeValue(ctx, node);
        if (JS_IsException(r)) { JS_FreeValue(ctx, JS_GetException(ctx)); ok = 0; }
        else { int32_t rv = 0; JS_ToInt32(ctx, &rv, r); ok = (rv == 1); }
        JS_FreeValue(ctx, r);
    } else if (JS_IsObject(f)) {
        JSValue fn = JS_GetPropertyStr(ctx, f, "acceptNode");
        if (JS_IsFunction(ctx, fn)) {
            JSValue node = push_node(ctx, n);
            JSValue r = JS_Call(ctx, fn, f, 1, (JSValueConst *)&node);
            JS_FreeValue(ctx, node);
            if (JS_IsException(r)) { JS_FreeValue(ctx, JS_GetException(ctx)); ok = 0; }
            else { int32_t rv = 0; JS_ToInt32(ctx, &rv, r); ok = (rv == 1); }
            JS_FreeValue(ctx, r);
        }
        JS_FreeValue(ctx, fn);
    }
    JS_FreeValue(ctx, f);
    return ok;
}
static NbNode *walker_cur(JSContext *ctx, JSValueConst walker) {
    JSValue c = JS_GetPropertyStr(ctx, walker, WALK_CUR);
    NbNode *n = get_node(ctx, c);
    JS_FreeValue(ctx, c);
    return n;
}
static NbNode *walker_root(JSContext *ctx, JSValueConst walker) {
    JSValue r = JS_GetPropertyStr(ctx, walker, "root");
    NbNode *n = get_node(ctx, r);
    JS_FreeValue(ctx, r);
    return n;
}
static void walker_set_cur(JSContext *ctx, JSValueConst walker, NbNode *n) {
    JSValue w = push_node(ctx, n);
    JS_SetPropertyStr(ctx, walker, WALK_CUR, JS_DupValue(ctx, w));
    JS_SetPropertyStr(ctx, walker, "currentNode", w);
}
static NbNode *prev_sib(NbNode *n) {
    if (!n || !n->parent) return NULL;
    NbNode *p = n->parent->first_child;
    if (p == n) return NULL;
    while (p && p->next_sibling != n) p = p->next_sibling;
    return p;
}
static JSValue nb_walker_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = walker_root(ctx, this_val);
    NbNode *n = walker_cur(ctx, this_val);
    if (!root || !n) return JS_NULL;
    for (;;) {
        if (n->first_child) { n = n->first_child; }
        else {
            while (n != root && !n->next_sibling) n = n->parent;
            if (n == root) return JS_NULL;
            n = n->next_sibling;
        }
        if (walker_accept(ctx, this_val, n)) { walker_set_cur(ctx, this_val, n); return push_node(ctx, n); }
    }
}
static JSValue nb_walker_prev(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = walker_root(ctx, this_val);
    NbNode *n = walker_cur(ctx, this_val);
    if (!root || !n) return JS_NULL;
    while (n != root) {
        NbNode *ps = prev_sib(n);
        if (ps) {
            n = ps;
            while (n->last_child) n = n->last_child;
            if (walker_accept(ctx, this_val, n)) { walker_set_cur(ctx, this_val, n); return push_node(ctx, n); }
            continue;
        }
        n = n->parent;
        if (!n || n == root) return JS_NULL;
        if (walker_accept(ctx, this_val, n)) { walker_set_cur(ctx, this_val, n); return push_node(ctx, n); }
    }
    return JS_NULL;
}
static JSValue nb_walker_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = walker_root(ctx, this_val);
    NbNode *n = walker_cur(ctx, this_val);
    if (!root || !n) return JS_NULL;
    while (n && n != root) {
        n = n->parent;
        if (n && n != root && walker_accept(ctx, this_val, n)) { walker_set_cur(ctx, this_val, n); return push_node(ctx, n); }
    }
    return JS_NULL;
}
static JSValue nb_walker_first(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = walker_cur(ctx, this_val);
    if (!n) return JS_NULL;
    for (NbNode *c = n->first_child; c; c = c->next_sibling)
        if (walker_accept(ctx, this_val, c)) { walker_set_cur(ctx, this_val, c); return push_node(ctx, c); }
    return JS_NULL;
}
static JSValue nb_walker_last(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = walker_cur(ctx, this_val);
    if (!n) return JS_NULL;
    for (NbNode *c = n->last_child; c; c = prev_sib(c))
        if (walker_accept(ctx, this_val, c)) { walker_set_cur(ctx, this_val, c); return push_node(ctx, c); }
    return JS_NULL;
}
static JSValue nb_walker_nextsib(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = walker_root(ctx, this_val);
    NbNode *n = walker_cur(ctx, this_val);
    if (!root || !n || n == root) return JS_NULL;
    for (NbNode *c = n->next_sibling; c; c = c->next_sibling)
        if (walker_accept(ctx, this_val, c)) { walker_set_cur(ctx, this_val, c); return push_node(ctx, c); }
    return JS_NULL;
}
static JSValue nb_walker_prevsib(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = walker_root(ctx, this_val);
    NbNode *n = walker_cur(ctx, this_val);
    if (!root || !n || n == root || !n->parent) return JS_NULL;
    NbNode *res = NULL;
    for (NbNode *c = n->parent->first_child; c && c != n; c = c->next_sibling)
        if (walker_accept(ctx, this_val, c)) res = c;
    if (res) { walker_set_cur(ctx, this_val, res); return push_node(ctx, res); }
    return JS_NULL;
}
static JSValue nb_walker_get_cur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = walker_cur(ctx, this_val);
    return n ? push_node(ctx, n) : JS_NULL;
}
static JSValue nb_walker_set_cur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (n) JS_SetPropertyStr(ctx, this_val, WALK_CUR, JS_DupValue(ctx, argv[0]));
    return JS_UNDEFINED;
}
static JSValue nb_doc_createTreeWalker(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!root) root = g_dom_root;              /* createTreeWalker(document,...) */
    if (!root) return JS_NULL;
    uint32_t show = 0xFFFFFFFFu;
    if (argc > 1 && JS_IsNumber(argv[1])) JS_ToUint32(ctx, &show, argv[1]);
    JSValue w = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, w, "root", push_node(ctx, root));
    JS_SetPropertyStr(ctx, w, "whatToShow", JS_NewUint32(ctx, show));
    JS_SetPropertyStr(ctx, w, WALK_CUR, push_node(ctx, root));
    JS_SetPropertyStr(ctx, w, WALK_SHOW, JS_NewUint32(ctx, show));
    JS_SetPropertyStr(ctx, w, WALK_FILT, (argc > 2) ? JS_DupValue(ctx, argv[2]) : JS_NULL);
    JS_SetPropertyStr(ctx, w, "nextNode", JS_NewCFunction(ctx, nb_walker_next, "nextNode", 0));
    JS_SetPropertyStr(ctx, w, "previousNode", JS_NewCFunction(ctx, nb_walker_prev, "previousNode", 0));
    JS_SetPropertyStr(ctx, w, "parentNode", JS_NewCFunction(ctx, nb_walker_parent, "parentNode", 0));
    JS_SetPropertyStr(ctx, w, "firstChild", JS_NewCFunction(ctx, nb_walker_first, "firstChild", 0));
    JS_SetPropertyStr(ctx, w, "lastChild", JS_NewCFunction(ctx, nb_walker_last, "lastChild", 0));
    JS_SetPropertyStr(ctx, w, "nextSibling", JS_NewCFunction(ctx, nb_walker_nextsib, "nextSibling", 0));
    JS_SetPropertyStr(ctx, w, "previousSibling", JS_NewCFunction(ctx, nb_walker_prevsib, "previousSibling", 0));
    JSAtom ck = JS_NewAtom(ctx, "currentNode");
    JS_DefinePropertyGetSet(ctx, w, ck,
        JS_NewCFunction(ctx, nb_walker_get_cur, "get currentNode", 0),
        JS_NewCFunction(ctx, nb_walker_set_cur, "set currentNode", 1), 0);
    JS_FreeAtom(ctx, ck);
    return w;
}

