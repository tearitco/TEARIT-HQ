/* nb_worker_boot_install_safe.c - boot install helper (mechanical extract, single-TU include) */
/* boot hygiene (2026-09-09): install_dom / install_events_timers ran under a
 * protected call so a throw was reported to stderr as WERR| and the
 * page still loaded. QuickJS property definitions don't throw on non-writable
 * targets (JS_DefinePropertyGetSet just returns FALSE), so the direct calls
 * are enough; keep the name for call-site clarity. */
static void boot_install_safe(JSContext *ctx) {
    install_dom(ctx);
    install_events_timers(ctx);
}
