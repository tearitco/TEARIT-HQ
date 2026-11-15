/* nb_worker_doc_surface.c - document textContent stubs and install helpers (mechanical extract, single-TU include) */
/* The live global `document` is a host-created plain object with no NODEKEY,
 * so the shared Node getter cannot see it (it reported undefined). Spec:
 * Document.textContent is null and assigning it is a no-op. */
static JSValue nb_live_doc_textContent_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_NULL;
}
static JSValue nb_live_doc_textContent_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}
/* Shared by the live document and every parsed document wrapper. */

static JSValue nb_doc_importNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_doc_adoptNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_dom_createComment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static void nb_install_doc_factories(JSContext *ctx, JSValue doc) {
    JS_SetPropertyStr(ctx, doc, "createElement", JS_NewCFunction(ctx, nb_dom_createElement, "createElement", 1));
    JS_SetPropertyStr(ctx, doc, "createElementNS", JS_NewCFunction(ctx, nb_dom_createElementNS, "createElementNS", 2));
    JS_SetPropertyStr(ctx, doc, "createTextNode", JS_NewCFunction(ctx, nb_dom_createTextNode, "createTextNode", 1));
    JS_SetPropertyStr(ctx, doc, "createComment", JS_NewCFunction(ctx, nb_dom_createComment, "createComment", 1));
    JS_SetPropertyStr(ctx, doc, "createDocumentFragment", JS_NewCFunction(ctx, nb_dom_createDocumentFragment, "createDocumentFragment", 0));
    JS_SetPropertyStr(ctx, doc, "importNode", JS_NewCFunction(ctx, nb_doc_importNode, "importNode", 2));
    JS_SetPropertyStr(ctx, doc, "adoptNode", JS_NewCFunction(ctx, nb_doc_adoptNode, "adoptNode", 1));
}
static JSValue nb_make_implementation(JSContext *ctx) {
    JSValue impl = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, impl, "createHTMLDocument", JS_NewCFunction(ctx, nb_impl_createHTMLDocument, "createHTMLDocument", 1));
    JS_SetPropertyStr(ctx, impl, "createDocument", JS_NewCFunction(ctx, nb_impl_createDocument, "createDocument", 3));
    JS_SetPropertyStr(ctx, impl, "createDocumentType", JS_NewCFunction(ctx, nb_impl_createDocumentType, "createDocumentType", 3));
    JS_SetPropertyStr(ctx, impl, "hasFeature", JS_NewCFunction(ctx, nb_impl_hasFeature, "hasFeature", 2));
    return impl;
}
static void nb_install_doc_metadata(JSContext *ctx, JSValue doc, const char *contentType, const char *url) {
    const char *ct = contentType ? contentType : "text/html";
    const char *u = (url && url[0]) ? url : "about:blank";
    JS_SetPropertyStr(ctx, doc, "contentType", JS_NewString(ctx, ct));
    JS_SetPropertyStr(ctx, doc, "URL", JS_NewString(ctx, u));
    JS_SetPropertyStr(ctx, doc, "documentURI", JS_NewString(ctx, u));
    JS_SetPropertyStr(ctx, doc, "compatMode", JS_NewString(ctx, "CSS1Compat"));
    JS_SetPropertyStr(ctx, doc, "characterSet", JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, doc, "charset", JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, doc, "inputEncoding", JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, doc, "doctype", JS_NULL);
    JS_SetPropertyStr(ctx, doc, "implementation", nb_make_implementation(ctx));
}
