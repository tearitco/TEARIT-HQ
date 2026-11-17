/* nb_worker_crash_install.c - worker crash-signal installer (mechanical extract, single-TU include) */
static void nb_crash_install(void) {
    if (!getenv("NB_CRASH_BT")) return;
    signal(SIGSEGV, nb_crash_handler);
    signal(SIGABRT, nb_crash_handler);
    signal(SIGBUS, nb_crash_handler);
    signal(SIGFPE, nb_crash_handler);
}
