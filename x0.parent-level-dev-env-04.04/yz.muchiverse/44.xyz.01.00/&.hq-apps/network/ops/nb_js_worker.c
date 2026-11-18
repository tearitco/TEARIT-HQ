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
#include <execinfo.h>   /* NB_CRASH_BT: fatal-signal backtrace into the worker log */
#include <time.h>
#include <sys/random.h>  /* getrandom(2) for nb_rand_bytes (crypto entropy) */
#include <errno.h>       /* EINTR in nb_rand_bytes' getrandom loop */
#include "../nb_css.h"
#include "../nb_dom.h"

/* R4 minimal shims */
static unsigned char _nbbd(char c){ if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26; if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; if(c=='=')return 64; return 255; }
/* OS entropy helper extracted (Phase 1, single-TU include). */
#include "nb_worker_rand_bytes.c"
/* Base64/crypto/perf host shims extracted (Phase 1, single-TU include). */
#include "nb_worker_base64_crypto_shims.c"

#define STB_IMAGE_IMPLEMENTATION
#include "../js/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
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
static int g_module_page_loaded = 0;  /* set when a module page is loaded; worker exits after */
                                 /* per run_page) - NAV frames go to manager */

/* devtools console EVAL: the last LOAD's QuickJS context + DOM tree stay live
 * (g_live_ctx / g_dom_root) between LOAD and EVAL, so an eval:<js> snippet
 * runs against the real page context (document, window, globals, timer
 * bindings) and can mutate the DOM. Tear down only at the next LOAD or on
 * QUIT/error - see run_page() and live_teardown(). */
static JSContext *g_live_ctx = NULL;
static JSRuntime *g_live_rt  = NULL;

/* Worker wire frame sender extracted (Phase 1, single-TU include). */
#include "nb_worker_send_payload.c"

/* Worker diagnostic ERROR frame responder extracted (Phase 1, single-TU include). */
#include "nb_worker_send_worker_error.c"

/* Worker STATUS responder extracted (Phase 1, single-TU include). */
#include "nb_worker_send_status.c"

/* Read one length-prefixed payload from stdin into g_rbuf.
 * Returns 1 on success, 0 on EOF/shutdown. */
/* Monotonic deadline helper extracted (Phase 1, single-TU include). */
#include "nb_worker_ms_until.c"

/* Bounded frame read. budget_ms < 0 keeps the historical blocking
 * behaviour; >= 0 gives up once the budget is spent and returns 0.
 *
 * Why: recv_frame() used a bare blocking read(), so a driver that never
 * answered parked the worker in recv() FOREVER. The worker is a long-lived
 * resident process (see the "keep the resident browser worker alive through
 * long page loads" change), so one unanswered FETCH wedged it permanently
 * and SILENTLY - `make check` printed nothing and had to be killed. Callers
 * already treat 0 as "no frame" and fall back, so a bounded wait degrades
 * into the direct-curl path instead of hanging the house.
 */
/* Why recv_frame_budget() gave up. Needed because "the caller timed out"
 * and "the peer went away" were BOTH a bare 0, and the tick loop must
 * do opposite things with them: keep ticking versus shut down. */
#define NB_RECV_SHUTDOWN 0
#define NB_RECV_TIMEOUT  1

/* Frame-reader pushback byte helper extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_getc.c"

/* Bounded frame reader extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_frame_ex.c"

/* Frame reader convenience wrappers extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_frame_wrappers.c"

/* LOAD frame field splitter extracted (Phase 1, single-TU include). */
#include "nb_worker_split_lines.c"

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

/* Node handle index helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_index.c"

/* Forward decls for the customElements / ShadowRoot / template-content
 * section (2026-09-20). Defined AFTER js_error_to_cstr so lifecycle hooks
 * reuse the g_trace_cb dump channel. */
static JSValue ce_define(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_when_def(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_upgrade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_attachShadow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static NbNode *nb_template_content_frag(JSContext *ctx, JSValue el, NbNode *n);
static JSValue make_fragment_wrapper(JSContext *ctx, NbNode *frag, const char *nodeName);

/* Small string builder helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_sb.c"

/* Basic node tree helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_tree_helpers.c"

/* ---- text / html serialization ---- */
/* Node textContent helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_text_content.c"
/* HTML fragment serialization (innerHTML / outerHTML / XMLSerializer).
 * The old emitter wrote text verbatim, which is wrong for the three things
 * a page actually does with innerHTML:
 *   - ESCAPE text: '&' -> &amp;, '<' -> &lt;, '>' -> &gt;, U+00A0 -> &nbsp;.
 *     Verbatim text re-parses as markup or as an entity, so a round-trip
 *     (`el.innerHTML = el.innerHTML`) silently changed the tree.
 *   - Raw-text elements (script/style/xmp/iframe/noembed/noframes/
 *     plaintext/noscript) keep their data literally: the parser never
 *     treated it as markup, so escaping it would corrupt JS/CSS.
 *   - VOID elements have no end tag. Emitting `</br>` makes a round-trip
 *     add a second break, because the HTML parser reads `</br>` as `<br>`.
 * XML mode escapes the same three bytes but not U+00A0 (XML has no &nbsp;
 * entity) and keeps closing tags for void elements (XML is not HTML).
 * Attribute blobs are still emitted as stored: parser/load blobs are
 * already source markup, and setAttribute values are not re-escaped yet. */
/* HTML serializer tag/escape helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ser_escape_helpers.c"
/* Attribute escaping helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_attr_escaped.c"
/* Canonical attribute serialization helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_attrs_canonical.c"
/* Safe comment serialization helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_comment_data_safe.c"
/* Outer HTML serialization helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_outer_html.c"

/* ---- selector engine (subset: tag, #id, .class, tag.class, descendants) ---- */
/* Basic selector compound matching helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selector_compound.c"
/* Selector chain and query helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selector_query.c"
/* Basic DOM find helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_find_helpers.c"
/* DOM/script forward declarations extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_script_forward_decls.c"
/* Image src storage helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_img_src.c"

/* Script src storage and execution helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_helpers.c"

/* Script src getter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_get.c"

/* Script src setter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_set.c"
/* Decoded image cache helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_img_decoded.c"

/* ---- step 4: RENDER — serialize the (post-JS) DOM into page.state.txt
 * rows exactly as the manager's projector consumes them (TITLE/TEXT/LINK/IMG).
 * Only the worker's own tree is authoritative here, so JS mutations
 * (document.title, el.textContent, innerHTML=, appendChild) become visible. */
/* Render row/selection helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_render_row_helpers.c"

/* DOM walk/render serializer extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_walk_render.c"
/* Canvas rasterizer state/types extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_state.c"
/* Canvas state reset helper extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_reset.c"

/* DOM render rows serializer extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_render_rows.c"

/* ---- JS <-> C node binding ---- */
/* JS/C node binding helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_js_node_binding.c"
/* CSS/layout-intent state and declarations extracted (Phase 1, single-TU include). */
#include "nb_worker_css_state.c"
/* CSS computed style helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_css_computed_helpers.c"

/* el.style — identity-cached snapshot of the inline style attribute.
 * Reads mirror the declared inline props; JS writes persist on the
 * snapshot (browser-like authoring) but — no layout engine — do not feed
 * the metrics below. Non-enumerable cache prop keeps it per-wrapper. */
/* Element style getter extracted (Phase 1, single-TU include). */
#include "nb_worker_el_style_get.c"

/* offsetWidth/offsetHeight/clientWidth/clientHeight — magic 0/1 for the
 * offset pair, 2/3 for the client pair (client dims special-case the root
 * scrolling element, whose client box is the viewport, not a CSS width).
 * Hidden elements measure 0 (browser display:none semantics); visible
 * elements report the modeled CSS px size, else 0 (no layout engine). */
static double nb_client_dim(NbNode *n, int which);
static JSValue nb_el_scroll_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_dim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
/* Element offset/dimension helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_el_offset_helpers.c"

/* Phase 3 slice 2: simple block layout for getBoundingClientRect.
 * x is 0 for now (no inline flow), y is stacked: parent y + sum of
 * previous siblings' CSS heights. Hidden ancestors (display:none) give 0,0,0,0. */
/* Layout hidden ancestor helper extracted (Phase 1, single-TU include). */
#include "nb_worker_layout_hidden_anc.c"
/* Layout/text metrics, viewport/scroll/gutter state, fwd decls extracted (Phase 1, single-TU include). */
#include "nb_worker_layout_metrics_state.c"
/* Document root height helper extracted (Phase 1, single-TU include). */
#include "nb_worker_nb_doc_root_h.c"

/* The usable content column: the pane width, minus the scrollbar when the
 * document actually overflows it. */
/* Content column width helper extracted (Phase 1, single-TU include). */
#include "nb_worker_g_col_px.c"

/* The chrome the renderer puts ABOVE the first content row inside the
 * scrolled column. Both terms are measured off the renderer, not guessed:
 *
 *   - GENERIC_SBAR_ARROW_H (14, khtpm_core_render.c:5428). layout_scroll_region
 *     sets `content_y = y + (have_arrows ? GENERIC_SBAR_ARROW_H : 0)`, so a
 *     column that scrolls starts its first row 14px down, under the scrollbar's
 *     up-arrow. Only when it scrolls - have_arrows requires max_scroll > 0.
 *   - NB_ROW_PX (30) for the page-title row. The manager projects <head><title>
 *     into a `page-title` row as the FIRST child of the content scrolllist
 *     (network_browser_manager.c:5570), so it occupies a real row before any
 *     <p>. A DOM-only model cannot see it: <title> lives in <head> and takes no
 *     space in a browser's body, which is exactly why this offset was 44px
 *     (14 + 30) and looked like a mystery.
 *
 * This is the worker mirroring the house renderer on purpose. It is NOT
 * claiming <title> takes body space in some other browser - it is saying the
 * box tree must describe the pixels this renderer actually draws, which is
 * what G2.2 measures. */
/* Content inset helper extracted (Phase 1, single-TU include). */
#include "nb_worker_content_inset_top.c"

/* WIDTH of an element's border box - the half of the box tree that was
 * missing (G2.1, slice 1, 2026-10-14).
 *
 * Before this, getBoundingClientRect() reported `w = st.width`, the
 * DECLARED CSS width and nothing else, so every unstyled element on every
 * page reported 0. Measured on the G2-a fixture: box_d1=w0/h+, box_p1=w0
 * /h+, box_s1=w0/h+, box_p2=w0/h+ - heights were already right (nb_content_h
 * below), only widths were absent. A browser that reports width 0 for
 * ordinary markup is not reporting geometry, it is reporting an absence.
 *
 * Two rules, which is what block layout actually does:
 *
 *   - A BLOCK box fills its containing block. Declared width wins; else
 *     the parent's own box; the document root is the content column.
 *   - An INLINE box shrinks to fit its text - it does NOT stretch to the
 *     column, and reporting 640px for a three-word <span> would be its own
 *     kind of lie.
 *
 * Deliberately NOT here yet: padding/border/margin in the width sum, and
 * inline-vs-block detection by tag beyond the common set. Those are
 * slice-2 work and pretending otherwise is how a box tree starts lying.
 */
/* Block tag classifier extracted (Phase 1, single-TU include). */
#include "nb_worker_layout_is_block_tag.c"

/* Box width helper extracted (Phase 1, single-TU include). */
#include "nb_worker_nb_box_w.c"

/* Height of an element's content box. A declared CSS height wins (that is what
 * the offset/client metrics report). Otherwise a block sums its children and a
 * text node gets one line per full column of characters, so unstyled paragraphs
 * stack the way they do in a browser instead of all reporting 0. */
/* Content height helper extracted (Phase 1, single-TU include). */
#include "nb_worker_nb_content_h.c"

/* Margin + padding of one edge, resolved from the element's own style. */
/* Edge/outer box helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_edge_outer_helpers.c"

/* Layout position helper extracted (Phase 1, single-TU include). */
#include "nb_worker_layout_xy.c"

static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* DOMRect.toJSON helper extracted (Phase 1, single-TU include). */
#include "nb_worker_rect_tojson.c"

/* ---- IntersectionObserver (2026-11-08, extended 2026-11-09) ----
 * Real initial entries from the worker's box geometry, delivered as a
 * queued microtask and batched per task. DOM-driven geometry changes
 * (childList / attribute edits, textContent / innerHTML) re-evaluate
 * observed targets and deliver change entries. It intentionally does not
 * claim scroll delivery: the worker knows the LOAD viewport, not a live
 * pane-scroll channel. Thresholds are not yet modelled; a change entry
 * fires when isIntersecting or the ratio changes. */
#define NB_INT_MAX_OBS 32
/* IntersectionObserver helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_intersection_observer.c"
/* ResizeObserver helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_resize_observer.c"
/* matchMedia behavior notes extracted (Phase 1, single-TU include). */
#include "nb_worker_match_media_notes.c"
/* matchMedia helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_match_media.c"
/* getBoundingClientRect helper extracted (Phase 1, single-TU include). */
#include "nb_worker_gbcr.c"
/* DOM element/native forward declarations extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_element_forward_decls.c"
/* NodeList/HTMLCollection helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_collections.c"

static JSValue nb_doc_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_doc_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_doc_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_dom_createEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* ---- document natives ---- */
/* Document query helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_query_helpers.c"

/* TreeWalker/NodeFilter helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_treewalker.c"
/* document/DOM factory helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_document.c"
/* Element native forward declarations extracted (Phase 1, single-TU include). */
#include "nb_worker_element_native_forward_decls.c"
/* getAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_get_attribute.c"
/* element.attributes getter helper extracted (Phase 1, single-TU include). */
#include "nb_worker_attributes.c"
/* rung 8: hasAttribute — real bundles gate on documentElement.hasAttribute */
static int attrs_has(const NbNode *n, const char *name);
/* hasAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_has_attribute.c"
/* hasAttributes/getAttributeNames helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_attr_names.c"
static JSValue nb_el_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_removeAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* toggleAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_el_toggle_attribute.c"
/* Attribute set helper extracted (Phase 1, single-TU include). */
#include "nb_worker_attrs_set.c"
/* setAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_el_set_attribute.c"
/* rung-2 remainder: element.removeAttribute(name) — rebuild the raw attrs
 * blob without the named attribute (attrs_set's loop, skipping the match). */
/* attrs_has/attrs_del/removeAttribute helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_attr_remove.c"
/* id/className property helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_id_class.c"
/* textContent get/set helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_text_content.c"
/* CharacterData (Text/Comment) data+nodeValue (2026-11-09). Text nodes are
 * the only mutation source for characterData; setting data replaces the
 * text, notifies characterData observers with the old value, and
 * re-evaluates geometry. nodeValue mirrors data on text nodes and is null
 * elsewhere, as in a browser. */
/* Text data get/set helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_text_data.c"
/* CharacterData/Text helpers extracted for modularity (Phase 1 pilot, single-TU include). */
#include "nb_worker_text.c"
/* nodeValue helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_value.c"
/* ---- dataset (2026-11-09) ----
 * dataset is a DOMStringMap view over an element's data-* attributes.
 * Handlers are C traps on a Proxy so reads stay live and writes land in
 * the attribute store (prop `userName` <-> attribute `data-user-name`). */
static JSValue push_node(JSContext *ctx, NbNode *n);
/* push_node_or_null helper extracted (Phase 1, single-TU include). */
#include "nb_worker_push_node_or_null.c"
/* dataset node lookup helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_node.c"
/* dataset key-to-attribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_attr.c"
/* dataset get trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_get.c"
/* dataset set trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_set.c"
/* dataset delete trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_del.c"
/* dataset ownKeys trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_own_keys.c"
/* dataset getOwnPropertyDescriptor trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_desc.c"
/* dataset has trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_has.c"
/* dataset Proxy factory helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_make.c"
/* innerHTML get/set helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_inner_html.c"
/* ---- insertAdjacentHTML / Text / Element (2026-11-09) ----
 * Real DOM insertion at the four spec positions, built on the same parser
 * and tree helpers that innerHTML/appendChild already use. Positions are
 * ASCII case-insensitive; anything else is a SyntaxError. beforebegin and
 * afterend on a node with no parent are a no-op, exactly as in a browser
 * (#document is the top; there is no place beside it).
 *
 * ChildList MutationObserver records are emitted for each inserted node.
 * A parsed fragment contributes one record per node (the C record model
 * carries one addedNode); a browser would batch them into one record. Both
 * describe the same real insertions, which is what an observer reacts to.
 *
 * insertAdjacentElement moves an existing node and returns it, or null when
 * the insertion is a no-op. Script-execution on insertion is not claimed
 * here: appendChild's dynamic <script> path is separate and this slice does
 * not duplicate it. */
/* insertAdjacent position parse/error helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_parse.c"
/* insertAdjacent placement/attach helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_place.c"
/* insertAdjacent HTML insertion helper extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_html_helper.c"
/* Element.insertAdjacentHTML extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_html.c"
/* Element.insertAdjacentText extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_text.c"
/* Element.insertAdjacentElement extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_element.c"
/* children/childNodes getters extracted (Phase 1, single-TU include). */
#include "nb_worker_children_nodes.c"
/* parentNode/firstChild/nextSibling getters extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_basic.c"
/* lastChild/previousSibling/parentElement and helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_struct.c"
/* element-only relation getters/count extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_element.c"
/* DocumentFragment insertion/validation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_fragment_utils.c"
/* appendChild helper extracted (Phase 1, single-TU include). */
#include "nb_worker_append_child.c"
/* Node.removeChild extracted (Phase 1, single-TU include). */
#include "nb_worker_remove_child.c"
/* Node.insertBefore extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_before.c"
/* Node.replaceChild extracted (Phase 1, single-TU include). */
#include "nb_worker_replace_child.c"
/* form-field tag helper extracted (Phase 1, single-TU include). */
#include "nb_worker_is_form_field.c"
/* select/option selection primitives extracted (Phase 1, single-TU include). */
#include "nb_worker_select_primitives.c"
/* option selected state helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_option_selected.c"
/* option value/index helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_option_value_index.c"
/* select selection helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_select_selection.c"
/* select options/length/index accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_select_options.c"
/* element value get/set and fire_change decl extracted (Phase 1, single-TU include). */
#include "nb_worker_value_get_set.c"
/* select.selectedIndex and option.selected accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_select_index_selected.c"
/* classList native methods extracted (Phase 1, single-TU include). */
#include "nb_worker_classlist.c"
/* DOM node identity/prototype defs and CE upgrade decl extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_node_identity_defs.c"
/* Build a JS element object wrapping a C NbNode. */
/* DOM node wrapper builder extracted (Phase 1, single-TU include). */
#include "nb_worker_push_node.c"

/* Cookie jar state/types extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_state.c"
/* Recursive mkdir helper extracted (Phase 1, single-TU include). */
#include "nb_worker_mkdir_p.c"

/* Cookie jar init helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_jar_init.c"

/* Split the current g_href into host + request path (bare host/port dropped).
 * Port numbers are skipped (cookie scoping ignores ports, RFC 6265 §1). */
/* href host/path helper extracted (Phase 1, single-TU include). */
#include "nb_worker_href_parts.c"

/* RFC 6265 §5.1.4 default-path for a Set-Cookie with no explicit Path. */
/* Default cookie path helper extracted (Phase 1, single-TU include). */
#include "nb_worker_default_cookie_path.c"

/* RFC 6265 §5.1.4 path-match: `cp` is the cookie path, `rp` the request path. */
/* Cookie path match helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_path_match.c"

/* Cookie string trim helper extracted (Phase 1, single-TU include). */
#include "nb_worker_trim_c.c"

/* Cookie value sanitize helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sanitize_cookie_value.c"

/* days-from-civil -> UNIX epoch (no TZ dependence; glibc timegm macro-gated). */
/* Epoch from ymd helper extracted (Phase 1, single-TU include). */
#include "nb_worker_epoch_from_ymd.c"

/* Cookie datetime helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_datetime.c"

/* Cookie line parse helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_parse_line.c"

/* Cookie file load helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_load_file.c"

/* Cookie file save helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_save_file.c"
/* Unified cookie store seam comments extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_store_seam.c"
/* URL host/path helper extracted (Phase 1, single-TU include). */
#include "nb_worker_url_host_path.c"

/* Scope-match cookies from the jar for an outgoing request URL.
 * Writes "Cookie: n1=v1; n2=v2" into out.  Respects Domain/Path/Secure. */
/* Cookie header builder extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_header_for_url.c"

/* Parse a raw Set-Cookie header value and upsert into the jar.
 * request_url provides the default Domain/Path when the header omits them. */
/* Cookie Set-Cookie ingest helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cookie_set_from_wire.c"

/* document.cookie getter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_cookie_get.c"

/* document.cookie setter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_cookie_set.c"

/* localStorage/sessionStorage state extracted (Phase 1, single-TU include). */
#include "nb_worker_storage_state.c"
/* localStorage jar/path/encoding helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_storage_jar_helpers.c"

/* localStorage implementation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_localstorage.c"

/* sessionStorage implementation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_sessionstorage.c"

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
/* DOM class noop constructor helper extracted (Phase 1, single-TU include). */
#include "nb_worker_class_noop.c"
/* promise-resolved and empty-array helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_promise_empty.c"
/* XMLSerializer/outerHTML get serialization helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_serialize_get.c"
/* outerHTML setter extracted (Phase 1, single-TU include). */
#include "nb_worker_outerhtml_set.c"
/* parsed-document accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_parsed_doc_accessors.c"
/* Range/selection decls and parsed-doc/DOMParser surface comments extracted (Phase 1, single-TU include). */
#include "nb_worker_parsed_doc_surface_decls.c"
/* document.title getter helper extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_title_get.c"
/* document.title setter extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_title_set.c"
/* document surface/install helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_surface.c"
/* DOMParser.parseFromString extracted (Phase 1, single-TU include). */
#include "nb_worker_domparser_parse.c"
/* PerformanceObserver mark delivery extracted (Phase 1, single-TU include). */
#include "nb_worker_performance_observer.c"
/* Range/Selection text and endpoint helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_helpers.c"
/* Range core set/ctor/createRange extracted (Phase 1, single-TU include). */
#include "nb_worker_range_core.c"
/* selection access helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selection_access.c"
/* Range property/get and boundary helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_props_boundary.c"
/* Range select/collapse helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_select_collapse.c"
/* Range clone/detach/toString extracted (Phase 1, single-TU include). */
#include "nb_worker_range_clone_detach.c"
/* Range text trim/delete/prune helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_prune.c"
/* Range delete/extract core wrappers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_delete.c"
/* Range clone_contents implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_range_clone_contents.c"
/* Selection implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_selection.c"
/* MutationObserver core and notify helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_mutation_core.c"
/* MutationObserver public API methods extracted (Phase 1, single-TU include). */
#include "nb_worker_mutation_api.c"
/* FormData helpers and methods extracted (Phase 1, single-TU include). */
#include "nb_worker_formdata.c"
/* Blob implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_blob.c"
/* FileReader read and FormData/Blob/FileReader install extracted (Phase 1, single-TU include). */
#include "nb_worker_filereader_install.c"
/* DOM class constructor helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_class_helpers.c"
/* element insertion arg helper and mode macros extracted (Phase 1, single-TU include). */
#include "nb_worker_node_insert_arg.c"
/* element insert/remove/replace operations extracted (Phase 1, single-TU include). */
#include "nb_worker_node_insert_ops.c"

/* element localName/matches/closest extracted (Phase 1, single-TU include). */
#include "nb_worker_el_match_closest.c"
/* element query/collection methods extracted (Phase 1, single-TU include). */
#include "nb_worker_el_query.c"

/* cloneNode/nbi_clone helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_clone.c"
/* Node.isEqualNode/isSameNode helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_equality.c"

/* getClientRects helper extracted (Phase 1, single-TU include). */
#include "nb_worker_rects.c"
/* window scroll helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_scroll.c"
/* element/window scroll metrics helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_scroll_metrics.c"

/* Node root/ownership helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_root.c"

static void install_dom_classes(JSContext *ctx) {
    /* DOM class hierarchy base setup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_base_body.c"
    /* Element prototype methods install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_methods.c"
    /* Element structural navigation getters install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_nav.c"
    /* Element scroll properties install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_scroll.c"
    /* Element insertAdjacent/outerHTML installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_adjacent.c"
    /* Node prototype core methods/getters install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_core.c"
    /* Node equality/root/position installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_compare.c"
    /* Node isConnected/ownerDocument/nodeValue installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_state.c"
    /* Element.remove and HTMLElement/SVGElement/Text prototype bases extracted (Phase 1, single-TU include). */
#include "nb_worker_install_proto_bases.c"
    /* Text/CharacterData prototype installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_text_cdata.c"
    /* Document/Animation prototypes and global DOM exports extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_exports.c"

    /* Node static type constants extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_constants.c"

    /* document object and timeline setup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_document_object.c"

    /* customElements and CSSStyleSheet globals extracted (Phase 1, single-TU include). */
#include "nb_worker_install_ce_cssom.c"

    /* standard element/event constructor global stubs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_global_stubs.c"
        /* FormData/Blob/FileReader and MutationObserver installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_formdata_mutation.c"
        /* Intersection/Resize/Performance observer installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_observers.c"
        /* Range global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_range.c"
        /* Selection global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_selection.c"
        /* DOMParser/XMLSerializer global installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_parser_serializer.c"
        /* NodeList/HTMLCollection global installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_list_classes.c"
        /* DOMTokenList global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_domtokenlist.c"
        /* install_dom_classes tail and cleanup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_tail.c"

/* minimal TTF font state and table lookup extracted (Phase 1, single-TU include). */
#include "nb_worker_font_state.c"
/* minimal TTF font load and table parsing extracted (Phase 1, single-TU include). */
#include "nb_worker_font_load.c"
/* minimal TTF glyph index and advance helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_font_glyph.c"
/* minimal TTF glyph outline decoding extracted (Phase 1, single-TU include). */
#include "nb_worker_font_points.c"
/* minimal TTF glyph raster/blit extracted (Phase 1, single-TU include). */
#include "nb_worker_font_blit.c"
/* canvas font string parsing helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_font_parse.c"
/* canvas context string property helper extracted (Phase 1, single-TU include). */
#include "nb_worker_ctx_str.c"
/* ---- canvas 2D ---- */
static JSValue nb_c2d_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
static void nb_grad_sample(JSContext *ctx, JSValue grad, double t, unsigned char out[4]);

static double nb_ctx_num(JSContext *ctx, JSValueConst o, const char *k, double def);
/* canvas context lookup/resize sync helper extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_get.c"
/* canvas/CSS color parsing helper extracted (Phase 1, single-TU include). */
#include "nb_worker_color_parse.c"
/* canvas pixel blend/fill helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_px.c"
/* canvas context utility helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_ctx_utils.c"
/* canvas/image source pixel resolver extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_source.c"
/* canvas 2D fillRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_fillrect.c"
/* canvas 2D strokeRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_strokerect.c"
/* canvas 2D clearRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_clearrect.c"
/* canvas 2D save/restore implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_save_restore.c"
/* canvas 2D transform operations extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_transform_ops.c"
/* canvas 2D putImageData implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_putimagedata.c"
/* canvas 2D image data operations extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_imagedata_ops.c"
/* canvas 2D measureText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_measuretext.c"
/* canvas 2D fillText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_filltext.c"
/* canvas 2D strokeText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_stroketext.c"
/* canvas 2D gradient helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_gradient.c"
/* canvas 2D pattern/line-dash helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_pattern_linedash.c"
/* canvas WebGL (GLES2/EGL) implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_webgl.c"
/* canvas element getContext implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_el_getcontext.c"
/* canvas export helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_export.c"
/* ---- DOM class one JS object per C node (see PROTONAME) ---- */

/* Attach the DOM natives to the global `document` object. */
/* DOM installation helper extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom.c"
/* eval budget/watchdog and error helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_eval_watchdog.c"
/* ---- customElements registry + ShadowRoot / template-content fragments
 * (2026-09-20) ----
 * kevlar DEFERS element registration: the ytd-masthead class (with its
 * searchbox <template> string) is wired by
 * `_.f9({disableElementRegistration:!0,...})(ZW); _.OV(ZW,"ytd-masthead",...)`
 * only when `window.customElements` is present, and the masthead + search UI
 * then get created at upgrade time (createElement("template") + innerHTML +
 * attachShadow + stamp). A REAL define/get/whenDefined/upgrade + C-backed
 * ShadowRoot is the slice that starts that path; the registry is a plain
 * object customElements.__registry (plus an ordered ""__names"" array) so
 * C holds NO JSValues across calls (no GC rooting hazards). Upgraded
 * wrappers carry __nb_upgraded; the lifecycle runs best-effort (prototype
 * swap + _initializeProperties/ready/connectedCallback) with exceptions
 * dumped to the TRACE_CB channel so the bundle keeps bootstrapping. */
/* custom elements core helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_core.c"
/* custom elements upgrade helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_upgrade.c"
/* custom elements registry API extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_registry_api.c"
/* fragment query helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_query.c"
/* fragment clone helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_clone.c"
/* fragment wrapper construction extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_wrapper.c"
/* document.createDocumentFragment() / new DocumentFragment(): a detached
 * #document-fragment kept on the orphan list, wrapped with the element
 * insert/query natives. Appending it MOVES its children into the target and
 * leaves it empty (see nb_insert_fragment_children). */

/* document.createComment binding extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_create_comment.c"

/* document owner/DOM-exception helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_node_helpers.c"
/* document.importNode/adoptNode bindings extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_import_adopt.c"


/* DocumentFragment factory extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_createfragment.c"

/* template content fragment helper extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_template.c"

/* Element.prototype.attachShadow implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_el_attachshadow.c"

/* Release every JS callback this file currently holds (timers/events/on-props).
 * Must run BEFORE the owning context is freed; idempotent (slots are set to
 * JS_UNDEFINED after releasing, so a second call / a later per-LOAD reset
 * cannot double-free). */
/* JS callback/observer slot cleanup extracted (Phase 1, single-TU include). */
#include "nb_worker_free_held_callbacks.c"

/* Run the callback held in `cb` with this=globalThis, 0 args.
 * Returns 0 on success, 1 on thrown error (message captured, exception freed). */
/* 0-arg callback invocation helper extracted (Phase 1, single-TU include). */
#include "nb_worker_invoke_cb0.c"

/* ---- timers ---- */
/* Timer schedule/clear bindings extracted (Phase 1, single-TU include). */
#include "nb_worker_timers.c"

/* ---- microtasks + rAF ---- */
/* Microtask queue and rAF bindings extracted (Phase 1, single-TU include). */
#include "nb_worker_microtask_raf.c"
/* ---- rung 4: fetch / XHR transport — blocking curl child, Promise-shaped ---- */
/* The JS prelude (nb_host.h) wraps this in a Promise polyfill + fetch() +
 * XMLHttpRequest. Blocking is fine: the drain loop afterwards exhausts the
 * microtask queue, so .then() chains render before RENDER. The manager stays
 * parse-time network owner; the worker is JS-time network owner. */
/* Curl config line helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_curl_cfg_helpers.c"

/* Document-relative URL resolver extracted (Phase 1, single-TU include). */
#include "nb_worker_resolve_doc_url.c"

/* Manager RPC fetch helper extracted (Phase 1, single-TU include). */
#include "nb_worker_try_fetch_via_manager.c"

/* Synchronous fetch/XHR transport binding extracted (Phase 1, single-TU include). */
#include "nb_worker_fetch_sync.c"

/* Timer due/drain/run helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_timer_loop_helpers.c"

/* Event listener and basic event method bindings extracted (Phase 1, single-TU include). */
#include "nb_worker_event_listeners.c"
/* on-* property handler helpers and bindings extracted (Phase 1, single-TU include). */
#include "nb_worker_onprop_handlers.c"
/* Event dispatch helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_event_dispatch.c"

/* Base64 decode helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_b64_helpers.c"
/* Image dimension/complete getters extracted (Phase 1, single-TU include). */
#include "nb_worker_img_dimension_getters.c"
/* Image src get/set accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_img_src_accessors.c"
/* Image constructor helper extracted (Phase 1, single-TU include). */
#include "nb_worker_image_ctor.c"
/* element/document dispatchEvent helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_dispatch.c"
/* Node.contains (row-31): webcomponents-lite.js does
 * `document.contains ? document.contains.bind(document)
 *  : document.documentElement.contains.bind(...)` and Polymer/Closure
 * ask it during upgrades. Ancestry walk over the C tree. */
/* Node/Document.contains helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_contains.c"
/* window.dispatchEvent helper extracted (Phase 1, single-TU include). */
#include "nb_worker_win_dispatch.c"
/* Element click/change helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_click.c"
/* element.focus()/blur() (2026-09-21): typed-input path needs the search
 * input focusable (kevlar reads document.activeElement and toggles the
 * yt-searchbox focused classes) and focus/blur listeners come and go. */
/* Element focus/blur helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_focus.c"
/* Search-box Enter navigation proxy extracted (Phase 1, single-TU include). */
#include "nb_worker_search_proxy.c"

/* el.blur is wired to nb_el_blur (magic 1); keep the click/misc natives below. */
/* Legacy/simple event firing helper extracted (Phase 1, single-TU include). */
#include "nb_worker_fire_event.c"
/* Tick loop state and notes extracted (Phase 1, single-TU include). */
#include "nb_worker_tick_loop_state.c"
/* Tick loop frame/wait helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_tick_helpers.c"

/* Main event loop drain helper extracted (Phase 1, single-TU include). */
#include "nb_worker_run_event_loop.c"

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

/* Generic SHA-1 native helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sha1_native.c"

/* Global event/timer/install helper extracted (Phase 1, single-TU include). */
#include "nb_worker_install_events_timers.c"

/* Boot install helper extracted (Phase 1, single-TU include). */
#include "nb_worker_boot_install_safe.c"

/* Script eval budget helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_eval_budget.c"
/* JS value display string helper extracted (Phase 1, single-TU include). */
#include "nb_worker_js_display_cstr.c"

/* Run the page script; sends STATUS ok|err across the wire. */
/* DOM/runtime teardown helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_teardown.c"

/* phase-2 (2026-09-09): document-order script runs. The manager writes one
 * <script> (inline or src-fetched) per slice of page.js, separated by the
 * SCRIPT_BOUNDARY sentinel. Each slice is compiled and run as its own
 * program — browser classic-script parity: a syntax error in slice 2 does
 * not stop slices 1/3, top-level `var` still lands on the shared global,
 * and external <script src> executes at its DOM position. Per-slice
 * failures print to stderr (surfaced by the manager as WERR|/[worker]).
 * A legacy page.js without any sentinel is treated as one program. */
/* Script boundary marker and finder extracted (Phase 1, single-TU include). */
#include "nb_worker_script_boundary.c"
/* Debug bundle probe/patch helper extracted (Phase 1, single-TU include). */
#include "nb_worker_bundle_probe.c"
/* Document-order script slice executor extracted (Phase 1, single-TU include). */
#include "nb_worker_run_scripts_slices.c"
/* Page load/run helper extracted (Phase 1, single-TU include). */
#include "nb_worker_run_page.c"

/* Devtools EVAL command handler extracted (Phase 1, single-TU include). */
#include "nb_worker_cmd_eval.c"

/* EVENT RPC command handler extracted (Phase 1, single-TU include). */
#include "nb_worker_cmd_event.c"

/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon. */
/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon, but
 * `duk -i` forces the REPL even when stdin is piped. */
/* CLI-2/CLI-3 native hook used by both the REPL and node mode. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void      nb_install_fs(JSContext *ctx);
static const char g_cjs_prelude[];
/* Interactive REPL entry extracted (Phase 1, single-TU include). */
#include "nb_worker_repl_main.c"

/* CLI stdio helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_cli_stdio.c"
/* CJS module file reader extracted (Phase 1, single-TU include). */
#include "nb_worker_cjs_read_file.c"
/* CLI process helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_cli_process.c"
/* fs-lite natives and installer extracted (Phase 1, single-TU include). */
#include "nb_worker_fs_lite.c"

/* CLI process/global install helper extracted (Phase 1, single-TU include). */
#include "nb_worker_cli_install.c"

/* Browser CLI runner extracted (Phase 1, single-TU include). */
#include "nb_worker_browser_cli_main.c"

/* CJS/ESM loader prelude extracted (Phase 1, single-TU include). */
#include "nb_worker_cjs_prelude.c"

/* Node-mode CLI entry extracted (Phase 1, single-TU include). */
#include "nb_worker_cli_main.c"

/* ---- fatal-signal backtrace (2026-10-26) ----
 * A hostile or oversized page script can take the worker down in ways that
 * leave NOTHING in the log: no WERR, no STATUS, just a zombie the manager
 * never hears from again. Measured on Cloudflare's orchestrate/chl_page
 * script: the last line in the worker log is "resolved script URL", and the
 * process is then gone. Without gdb on the box, the only way to tell a
 * SIGSEGV from an abort() is to have the worker report it itself. Gated by
 * NB_CRASH_BT so normal runs stay quiet. backtrace_symbols_fd() is the
 * async-signal-safe half of execinfo, and stderr is already the per-house
 * worker log, so the trace lands next to the page errors. */
/* Worker fatal-signal handler extracted (Phase 1, single-TU include). */
#include "nb_worker_crash_handler.c"
/* Worker crash-signal installer extracted (Phase 1, single-TU include). */
#include "nb_worker_crash_install.c"

/* Worker main entry and command loop extracted (Phase 1, single-TU include). */
#include "nb_worker_main.c"

