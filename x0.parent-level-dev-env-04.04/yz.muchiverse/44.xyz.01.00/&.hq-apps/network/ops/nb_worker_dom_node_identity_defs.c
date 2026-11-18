/* nb_worker_dom_node_identity_defs.c - DOM node identity/prototype defs and CE upgrade decl (mechanical extract, single-TU include) */

#define IDMAPNAME "__nb_idmap"    /* global object: node index -> JS wrapper (identity) */
/* DOM class prototypes (2026-09-18): real bundles branch on
 * `instanceof Element` and touch `Element.prototype` at load time. The
 * per-context global object `__nb_protos` holds the constructor-chain
 * prototypes for element/text/document wrappers built by install_dom_classes. */
#define PROTONAME "__nb_protos"
#define PROTO_EL 0   /* element wrappers ride HTMLElement.prototype */
#define PROTO_TEXT 1 /* text nodes ride Text.prototype */
#define PROTO_DOC 2  /* the document object rides Document.prototype */

/* upgrade a freshly built wrapper if a custom-element class is registered
 * for its tag (customElements upgrade-on-creation). Defined in the CE
 * section below (line ~3070). */
static void ce_upgrade_node_if_registered(JSContext *ctx, JSValue el, NbNode *n);
