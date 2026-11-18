/* nb_worker_custom_elements_notes.c - customElements/ShadowRoot/template notes (mechanical extract, single-TU include) */
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
