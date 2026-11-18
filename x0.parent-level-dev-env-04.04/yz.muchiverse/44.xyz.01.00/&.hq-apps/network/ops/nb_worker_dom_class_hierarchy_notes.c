/* nb_worker_dom_class_hierarchy_notes.c - DOM class hierarchy notes (mechanical extract, single-TU include) */

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
