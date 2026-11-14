/* nb_worker_text.c - CharacterData/Text helpers (mechanically extracted, single-TU include) */
static JSValue nb_cdata_length_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->text) return JS_NewInt32(ctx, 0);
    JSValue s = JS_NewString(ctx, n->text);
    JSValue lv = JS_GetPropertyStr(ctx, s, "length");
    JS_FreeValue(ctx, s);
    return lv;
}
static JSValue nb_cdata_substringData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    int32_t off = 0, cnt = 0x7fffffff;
    if (argc > 0) JS_ToInt32(ctx, &off, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &cnt, argv[1]);
    if (off < 0) off = 0;
    if (cnt < 0) cnt = 0;
    JSValue s = JS_NewString(ctx, n->text ? n->text : "");
    JSAtom aslice = JS_NewAtom(ctx, "slice");
    JSValue args2[2] = { JS_NewInt32(ctx, off), JS_NewInt32(ctx, off + cnt) };
    JSValue sub = JS_Invoke(ctx, s, aslice, 2, args2);
    JS_FreeAtom(ctx, aslice);
    JS_FreeValue(ctx, args2[0]);
    JS_FreeValue(ctx, args2[1]);
    JS_FreeValue(ctx, s);
    return sub;
}
static JSValue nb_cdata_appendData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *ad = NULL;
    const char *a = (argc > 0) ? (ad = JS_ToCString(ctx, argv[0])) : "";
    const char *cur = n->text ? n->text : "";
    size_t l1 = strlen(cur), l2 = strlen(a ? a : "");
    char *nv = (char *)malloc(l1 + l2 + 1);
    if (nv) { memcpy(nv, cur, l1); memcpy(nv + l1, a ? a : "", l2 + 1); }
    JS_FreeCString(ctx, ad);
    if (!nv) return JS_ThrowOutOfMemory(ctx);
    JSValue argv1[1]; argv1[0] = JS_NewString(ctx, nv);
    JSValue r = nb_text_data_set(ctx, this_val, 1, argv1);
    JS_FreeValue(ctx, argv1[0]); free(nv);
    return r;
}
static JSValue nb_cdata_replaceData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    int32_t off = 0, cnt = 0;
    if (argc > 0) JS_ToInt32(ctx, &off, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &cnt, argv[1]);
    if (off < 0) off = 0;
    if (cnt < 0) cnt = 0;
    char *ds = NULL; const char *d = (argc > 2) ? (ds = JS_ToCString(ctx, argv[2])) : "";
    JSValue s = JS_NewString(ctx, n->text ? n->text : "");
    JSValue dstr = JS_NewString(ctx, d ? d : "");
    JS_FreeCString(ctx, ds);

    JSAtom aslice = JS_NewAtom(ctx, "slice");
    JSAtom aconcat = JS_NewAtom(ctx, "concat");
    JSValue a1[2] = { JS_NewInt32(ctx, 0), JS_NewInt32(ctx, off) };
    JSValue s1 = JS_Invoke(ctx, s, aslice, 2, a1);
    JS_FreeValue(ctx, a1[0]); JS_FreeValue(ctx, a1[1]);
    JSValue a2[1] = { JS_NewInt32(ctx, off + cnt) };
    JSValue s2 = JS_Invoke(ctx, s, aslice, 1, a2);
    JS_FreeValue(ctx, a2[0]);
    JSValue tmp = JS_Invoke(ctx, s1, aconcat, 1, &dstr);
    JSValue res = JS_Invoke(ctx, tmp, aconcat, 1, &s2);

    JS_FreeAtom(ctx, aslice);
    JS_FreeAtom(ctx, aconcat);
    JS_FreeValue(ctx, s); JS_FreeValue(ctx, dstr);
    JS_FreeValue(ctx, s1); JS_FreeValue(ctx, s2); JS_FreeValue(ctx, tmp);

    if (JS_IsException(res)) return res;
    JSValue argv1[1] = { res };
    JSValue r = nb_text_data_set(ctx, this_val, 1, argv1);
    JS_FreeValue(ctx, res);
    return r;
}
static JSValue nb_cdata_insertData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue argv3[3];
    argv3[0] = (argc > 0) ? JS_DupValue(ctx, argv[0]) : JS_NewInt32(ctx, 0);
    argv3[1] = JS_NewInt32(ctx, 0);
    argv3[2] = (argc > 1) ? JS_DupValue(ctx, argv[1]) : JS_NewString(ctx, "");
    JSValue r = nb_cdata_replaceData(ctx, this_val, 3, argv3);
    JS_FreeValue(ctx, argv3[0]); JS_FreeValue(ctx, argv3[1]); JS_FreeValue(ctx, argv3[2]);
    return r;
}
static JSValue nb_cdata_deleteData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue argv3[3];
    argv3[0] = (argc > 0) ? JS_DupValue(ctx, argv[0]) : JS_NewInt32(ctx, 0);
    argv3[1] = (argc > 1) ? JS_DupValue(ctx, argv[1]) : JS_NewInt32(ctx, 0);
    argv3[2] = JS_NewString(ctx, "");
    JSValue r = nb_cdata_replaceData(ctx, this_val, 3, argv3);
    JS_FreeValue(ctx, argv3[0]); JS_FreeValue(ctx, argv3[1]); JS_FreeValue(ctx, argv3[2]);
    return r;
}
static int nb_is_text_node(const NbNode *n) {
    return n && (!n->tag || n->tag[0] == '\0');
}
static JSValue nb_text_wholeText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    if (!nb_is_text_node(n)) return JS_NewString(ctx, n->text ? n->text : "");
    NbNode *left = n;
    if (n->parent) {
        /* find leftmost of contiguous text run containing n */
        NbNode *run_start = NULL;
        for (NbNode *c = n->parent->first_child; c; c = c->next_sibling) {
            if (nb_is_text_node(c)) {
                if (!run_start) run_start = c;
                if (c == n) { left = run_start; break; }
            } else {
                run_start = NULL;
            }
        }
    }
    size_t total = 0;
    for (NbNode *p = left; p; p = p->next_sibling) {
        if (!nb_is_text_node(p)) break;
        total += p->text ? strlen(p->text) : 0;
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf) return JS_ThrowOutOfMemory(ctx);
    size_t off = 0;
    for (NbNode *p = left; p; p = p->next_sibling) {
        if (!nb_is_text_node(p)) break;
        if (p->text) {
            size_t l = strlen(p->text);
            memcpy(buf + off, p->text, l);
            off += l;
        }
    }
    buf[off] = '\0';
    JSValue r = JS_NewString(ctx, buf);
    free(buf);
    return r;
}
static JSValue nb_text_splitText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (!nb_is_text_node(n)) return JS_UNDEFINED;
    int32_t off = 0;
    if (argc > 0) JS_ToInt32(ctx, &off, argv[0]);
    JSValue s = JS_NewString(ctx, n->text ? n->text : "");
    JSValue lv = JS_GetPropertyStr(ctx, s, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, lv);
    JS_FreeValue(ctx, lv);
    if (off < 0 || off > len) {
        JS_FreeValue(ctx, s);
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "IndexSizeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "IndexSizeError: offset out of range"));
        return JS_Throw(ctx, er);
    }
    JSAtom aslice = JS_NewAtom(ctx, "slice");
    JSValue a1[2] = { JS_NewInt32(ctx, 0), JS_NewInt32(ctx, off) };
    JSValue left = JS_Invoke(ctx, s, aslice, 2, a1);
    JS_FreeValue(ctx, a1[0]); JS_FreeValue(ctx, a1[1]);
    JSValue a2[1] = { JS_NewInt32(ctx, off) };
    JSValue right = JS_Invoke(ctx, s, aslice, 1, a2);
    JS_FreeValue(ctx, a2[0]);
    JS_FreeAtom(ctx, aslice);
    JS_FreeValue(ctx, s);
    if (JS_IsException(left) || JS_IsException(right)) {
        JS_FreeValue(ctx, left); JS_FreeValue(ctx, right);
        return JS_EXCEPTION;
    }
    JSValue argvL[1] = { left };
    JSValue rs = nb_text_data_set(ctx, this_val, 1, argvL);
    JS_FreeValue(ctx, rs);
    JS_FreeValue(ctx, left);

    char *rstr = JS_ToCString(ctx, right);
    JS_FreeValue(ctx, right);
    NbNode *nn = calloc(1, sizeof(*nn));
    if (!nn) { JS_FreeCString(ctx, rstr); return JS_ThrowOutOfMemory(ctx); }
    nn->text = strdup(rstr ? rstr : "");
    JS_FreeCString(ctx, rstr);
    orphan_add(nn);
    JSValue w = push_node(ctx, nn);

    if (n->parent) {
        NbNode *ref = n->next_sibling;
        local_insert_before(n->parent, nn, ref);
        orphan_remove(nn);
        nb_mutation_notify_childlist(ctx, n->parent, nn, NULL, n, ref);
    }
    return w;
}
