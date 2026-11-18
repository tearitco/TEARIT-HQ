/* nb_worker_parsed_doc_surface_decls.c - range/selection decls and parsed-doc/DOMParser surface comments (mechanical extract, single-TU include) */
static JSValue nb_dom_createRange(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_win_getSelection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* ---- DOMParser: HTML/XML snippet parsing (2026-11-09) ----
 * The previous parseFromString was the inert nb_empty_object: it returned a
 * plain {} with no document surface, so every library parsing a snippet
 * crashed on the first query. This parses with the house HTML parser
 * (nb_parse_html, a #document tree) and hands back a document wrapper.
 * Scoped own properties are installed on the parsed wrapper — getElementById
 * plus documentElement/body/head getters — because push_node gives it the
 * Element prototype (element-scoped queries already work) but no Document
 * members. Honest limits: one lenient HTML parser serves HTML, XHTML, XML
 * and SVG mime types (no strict XML error detection); an unknown mime
 * throws TypeError instead of pretending to parse it. Parsed trees live in
 * g_orphans until page teardown. */
/* ---- parsed-document surface (2026-11-11) ----
 * parseFromString returns a push_node() wrapper: it carries the Element
 * prototype (element-scoped queries work) but had no Document members. The
 * host installs document.title on the live global only, so a parsed
 * document's title read undefined and createElement/createTextNode/
 * createDocumentFragment were missing entirely.
 *
 * title reads the first <title> in the tree; on this parse path RCDATA is a
 * real #text child, so node_text_content sees it. The setter replaces that
 * element's content and, when the document has no title, creates one under
 * <head> (synthesizing <head> as the root element's first child when it is
 * absent, the way the HTML parser would). Parsed trees live in g_orphans;
 * mutation records are emitted the same way the textContent setter emits
 * them (the added node is reported, removed children are not). */
