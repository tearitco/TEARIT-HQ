/* nb_worker_dom_runtime_state.c - DOM runtime state/defines (mechanical extract, single-TU include) */

/* ==================== rung 2 DOM (worker side) ====================
 * The manager serializes the DOM to fetch.dom (nb_dom.h/.c); the worker
 * rebuilds the NbNode tree here and exposes it to JS via native QuickJS
 * accessors (plan §7 step 3, roadmap §2 minimum API). The JS side only
 * holds opaque pointer handles; the tree is C-side. No shared memory. */


/* Viewport width the page's box tree is computed against (G2.2). Defined
 * next to NB_CONTENT_PX further down, forward-declared here because
 * window.innerWidth is exported from the globals block. */
static double g_viewport_px;
static NbNode *g_active_node = NULL;  /* element.focus()/blur() tracker */
static int g_search_wired = 0;  /* document-level Enter search proxy wired per page */
static NbNode *g_orphans = NULL;   /* detached createElement() nodes still to free */
#define NODEKEY "_nbnode"
