/* nb_worker_match_media_notes.c - matchMedia behavior notes (mechanical extract, single-TU include) */
/* ---- matchMedia (2026-11-09) ----
 * The prelude used to define matchMedia as a stub whose .matches was the
 * constant false. Every media query therefore took its "no" branch: a
 * 784px window still claimed to be narrower than nothing, and the mobile
 * detection in real bundles (window.matchMedia("(pointer:coarse)").matches)
 * silently answered false. This is a real evaluator over the viewport the
 * manager supplies on LOAD (fields 6 and 7) - the same g_viewport_px /
 * g_viewport_h that window.innerWidth / innerHeight report.
 *
 * Scope, stated honestly: .matches is recomputed on every read, so polling
 * code is correct. Change events are NOT delivered - the worker has no live
 * pane-resize channel (LOAD carries one viewport and the worker lives in it
 * until the next navigation), so a listener waiting for a change waits
 * forever. The listener surface itself is real (add/remove/dispatch), which
 * is what feature detection and framework wrappers touch.
 *
 * Unknown media features answer false: this worker refuses to claim a
 * capability the renderer does not have. (hover: hover) and (pointer: fine)
 * are true because this IS a mouse-driven window; (prefers-color-scheme:
 * light) is true because there is no OS preference channel to consult and
 * light is the platform default we can defend, not a guess about dark.
 */
