/* nb_worker_send_worker_error.c - worker diagnostic ERROR frame responder (mechanical extract, single-TU include) */
/* Worker-side diagnostic frame (2026-10-13).
 *
 * The manager ALREADY handles an "ERROR|" frame: it logs it to the module
 * log and keeps reading until STATUS, explicitly so worker diagnostics do
 * not abort a load (network_browser_manager.c, "hygiene: worker-side
 * JS/boot diagnostics"). So this uses the house'"'"'s existing frame kind
 * rather than inventing a protocol nobody reads.
 *
 * It exists because a failed DOM load was completely SILENT: nb_dom_load()
 * returns NULL for anything that is not its own serializer stream (and for
 * a missing file, fopen just fails), g_dom_root stays NULL, and the page
 * renders as its title and nothing else with STATUS ok. Measured on
 * 2026-10-13 while building the G1 acceptance harness - four commits were
 * spent proving that an HTML fixture is not a DOM, and every one of those
 * runs said "ok" while rendering nothing.
 */
static void send_worker_error(const char *msg) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "ERROR|%s", msg);
    if (g_cli) {
        (void)!write(STDOUT_FILENO, buf, strlen(buf));
        (void)!write(STDOUT_FILENO, "\n", 1);
        return;
    }
    send_payload(buf, strlen(buf));
}
