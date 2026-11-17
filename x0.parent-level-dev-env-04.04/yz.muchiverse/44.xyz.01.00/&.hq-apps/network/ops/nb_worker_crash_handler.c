/* nb_worker_crash_handler.c - worker fatal-signal handler (mechanical extract, single-TU include) */
static void nb_crash_handler(int sig) {
    void *bt[64];
    static const char hdr[] = "CRASH| fatal signal in worker (see frames below)\n";
    ssize_t w = write(STDERR_FILENO, hdr, sizeof(hdr) - 1);
    (void)w;
    int n = backtrace(bt, 64);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    signal(sig, SIG_DFL);
    raise(sig);
}
