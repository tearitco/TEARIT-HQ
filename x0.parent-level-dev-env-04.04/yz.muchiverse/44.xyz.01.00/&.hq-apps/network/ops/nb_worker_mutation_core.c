/* nb_worker_mutation_core.c - MutationObserver core and notify helpers (mechanical extract, single-TU include) */
/* ---- MutationObserver: child-list and attribute slice (2026-11-07) ----
 * The previous MutationObserver was an inert existence stub: pages could
 * feature-detect it, but dynamic DOM changes never reached callbacks. This
 * is the smallest honest slice: childList mutations, subtree
 * matching, and set/removeAttribute records are delivered to the observer's
 * callback as a queued microtask, batched per task. CharacterData records
 * arrive when a Text node's data changes (2026-11-09). */
#define NB_MUT_MAX_OBSERVERS 32
#define NB_MUT_MAX_TARGETS 64
#define NB_MUT_MAX_ATTRS 32
typedef struct {
    NbNode *target;
    int child_list, subtree, attributes, character_data;
    int attr_count;
    char attrs[NB_MUT_MAX_ATTRS][128];
} NbMutTarget;
typedef struct {
    JSValue self;
    JSValue callback;
    NbMutTarget targets[NB_MUT_MAX_TARGETS];
    int target_count;
    int scheduled; /* a microtask delivery job is already queued */
} NbMutObserver;
static NbMutObserver g_mut_obs[NB_MUT_MAX_OBSERVERS];
static int g_mut_obs_count = 0;

static int nb_mut_option_bool(JSContext *ctx, JSValueConst opts, const char *key) {
    if (!JS_IsObject(opts)) return 0;
    JSValue v = JS_GetPropertyStr(ctx, opts, key);
    int r = JS_ToBool(ctx, v);
    JS_FreeValue(ctx, v);
    return r;
}
static int nb_mut_find_observer(JSContext *ctx, JSValueConst self) {
    for (int i = 0; i < g_mut_obs_count; i++)
        if (JS_StrictEq(ctx, g_mut_obs[i].self, self)) return i;
    return -1;
}
static int nb_mut_node_is_ancestor(NbNode *root, NbNode *node) {
    for (NbNode *p = node; p; p = p->parent)
        if (p == root) return 1;
    return 0;
}
static JSValue nb_mut_node_value(JSContext *ctx, NbNode *n) {
    return n ? push_node(ctx, n) : JS_NULL;
}
static JSValue nb_mut_records_for(JSContext *ctx, JSValue self) {
    JSValue recs = JS_GetPropertyStr(ctx, self, "__nb_mut_records");
    if (!JS_IsArray(ctx, recs)) {
        JS_FreeValue(ctx, recs);
        recs = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, self, "__nb_mut_records", JS_DupValue(ctx, recs));
    }
    return recs;
}
static void nb_mut_flush(JSContext *ctx, NbMutObserver *o);
static JSValue nb_mut_flush_job(JSContext *ctx, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    int oi = nb_mut_find_observer(ctx, argv[0]);
    if (oi < 0) return JS_UNDEFINED; /* disconnected before delivery */
    g_mut_obs[oi].scheduled = 0;
    nb_mut_flush(ctx, &g_mut_obs[oi]);
    return JS_UNDEFINED;
}
static void nb_mut_flush(JSContext *ctx, NbMutObserver *o) {
    JSValue recs = nb_mut_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    if (n == 0 || !JS_IsFunction(ctx, o->callback)) {
        JS_FreeValue(ctx, recs);
        return;
    }
    JSValue recarg = JS_DupValue(ctx, recs);
    JSValue selfarg = JS_DupValue(ctx, o->self);
    JSValue av[2] = { recarg, selfarg };
    JSValue r = JS_Call(ctx, o->callback, o->self, 2, av);
    if (JS_IsException(r)) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, r);
    JS_FreeValue(ctx, recarg);
    JS_FreeValue(ctx, selfarg);
    JS_FreeValue(ctx, recs);
    JS_SetPropertyStr(ctx, o->self, "__nb_mut_records", JS_NewArray(ctx));
}
static void nb_mut_append_record(JSContext *ctx, NbMutObserver *o, JSValue rec) {
    JSValue recs = nb_mut_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, recs, n, rec);
    JS_FreeValue(ctx, recs);
    /* Spec: delivery is a queued microtask, not synchronous. Batched:
     * further mutations in the same task reuse the queued job. */
    if (!o->scheduled) {
        o->scheduled = 1;
        JSValue jv[1] = { o->self };
        if (JS_EnqueueJob(ctx, nb_mut_flush_job, 1, jv) < 0) {
            o->scheduled = 0;
            nb_mut_flush(ctx, o);
        }
    }
}
static JSValue nb_mut_record(JSContext *ctx, const char *type, NbNode *target,
                             NbNode *added, NbNode *removed, NbNode *prev, NbNode *next,
                             const char *attr, const char *old_value) {
    JSValue rec = JS_NewObject(ctx);
    nb_list_proto(ctx, rec, "MutationRecord");
    JS_SetPropertyStr(ctx, rec, "type", JS_NewString(ctx, type));
    JS_SetPropertyStr(ctx, rec, "target", nb_mut_node_value(ctx, target));
    JSValue added_arr = JS_NewArray(ctx);
    if (added) JS_SetPropertyUint32(ctx, added_arr, 0, nb_mut_node_value(ctx, added));
    JS_SetPropertyStr(ctx, rec, "addedNodes", added_arr);
    JSValue removed_arr = JS_NewArray(ctx);
    if (removed) JS_SetPropertyUint32(ctx, removed_arr, 0, nb_mut_node_value(ctx, removed));
    JS_SetPropertyStr(ctx, rec, "removedNodes", removed_arr);
    JS_SetPropertyStr(ctx, rec, "previousSibling", nb_mut_node_value(ctx, prev));
    JS_SetPropertyStr(ctx, rec, "nextSibling", nb_mut_node_value(ctx, next));
    JS_SetPropertyStr(ctx, rec, "attributeName", attr ? JS_NewString(ctx, attr) : JS_NULL);
    JS_SetPropertyStr(ctx, rec, "oldValue", old_value ? JS_NewString(ctx, old_value) : JS_NULL);
    return rec;
}
static void nb_mutation_notify_childlist(JSContext *ctx, NbNode *target, NbNode *added, NbNode *removed, NbNode *prev, NbNode *next) {
    if (!ctx || !target) return;
    NbNode *rprev = prev, *rnext = next;
    if (added) {
        if (!rprev && !rnext) {
            for (NbNode *c = target->first_child; c; c = c->next_sibling) {
                if (c == added) { rnext = c->next_sibling; break; }
                rprev = c;
            }
        } else if (!rprev) {
            for (NbNode *c = target->first_child; c; c = c->next_sibling) {
                if (c == added) break;
                rprev = c;
            }
        }
    }
    for (int oi = 0; oi < g_mut_obs_count; oi++) {
        NbMutObserver *o = &g_mut_obs[oi];
        if (JS_IsUndefined(o->self) || !JS_IsFunction(ctx, o->callback)) continue;
        for (int ti = 0; ti < o->target_count; ti++) {
            NbMutTarget *t = &o->targets[ti];
            int direct = (t->target == target);
            int nested = (t->subtree && t->child_list && nb_mut_node_is_ancestor(t->target, target));
            if (t->child_list && (direct || nested)) {
                nb_mut_append_record(ctx, o, nb_mut_record(ctx, "childList", target, added, removed, rprev, rnext, NULL, NULL));
                break;
            }
        }
    }
    nb_obs_recheck(ctx);
}
static void nb_mutation_notify_attribute(JSContext *ctx, NbNode *target, const char *name, const char *old_value) {
    if (!ctx || !target || !name) return;
    for (int oi = 0; oi < g_mut_obs_count; oi++) {
        NbMutObserver *o = &g_mut_obs[oi];
        if (JS_IsUndefined(o->self) || !JS_IsFunction(ctx, o->callback)) continue;
        for (int ti = 0; ti < o->target_count; ti++) {
            NbMutTarget *t = &o->targets[ti];
            int direct = (t->target == target);
            int nested = (t->subtree && t->attributes && nb_mut_node_is_ancestor(t->target, target));
            if (!t->attributes || !(direct || nested)) continue;
            int allowed = (t->attr_count == 0);
            for (int ai = 0; !allowed && ai < t->attr_count; ai++)
                allowed = !strcasecmp(t->attrs[ai], name);
            if (allowed) {
                nb_mut_append_record(ctx, o, nb_mut_record(ctx, "attributes", target, NULL, NULL, NULL, NULL, name, old_value));
                break;
            }
        }
    }
    nb_obs_recheck(ctx);
}
static void nb_mutation_notify_characterData(JSContext *ctx, NbNode *target, const char *old_value) {
    if (!ctx || !target) return;
    for (int oi = 0; oi < g_mut_obs_count; oi++) {
        NbMutObserver *o = &g_mut_obs[oi];
        if (JS_IsUndefined(o->self) || !JS_IsFunction(ctx, o->callback)) continue;
        for (int ti = 0; ti < o->target_count; ti++) {
            NbMutTarget *t = &o->targets[ti];
            int direct = (t->target == target);
            int nested = (t->subtree && t->character_data && nb_mut_node_is_ancestor(t->target, target));
            if (t->character_data && (direct || nested)) {
                nb_mut_append_record(ctx, o, nb_mut_record(ctx, "characterData", target, NULL, NULL, NULL, NULL, NULL, old_value));
                break;
            }
        }
    }
    nb_obs_recheck(ctx);
}
