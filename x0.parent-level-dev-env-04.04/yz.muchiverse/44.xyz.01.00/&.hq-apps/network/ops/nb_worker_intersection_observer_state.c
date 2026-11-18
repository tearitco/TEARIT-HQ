/* nb_worker_intersection_observer_state.c - IntersectionObserver notes and limits (mechanical extract, single-TU include) */

/* ---- IntersectionObserver (2026-11-08, extended 2026-11-09) ----
 * Real initial entries from the worker's box geometry, delivered as a
 * queued microtask and batched per task. DOM-driven geometry changes
 * (childList / attribute edits, textContent / innerHTML) re-evaluate
 * observed targets and deliver change entries. It intentionally does not
 * claim scroll delivery: the worker knows the LOAD viewport, not a live
 * pane-scroll channel. Thresholds are not yet modelled; a change entry
 * fires when isIntersecting or the ratio changes. */
#define NB_INT_MAX_OBS 32
