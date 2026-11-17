/* nb_worker_recv_frame_ex.c - bounded frame reader (mechanical extract, single-TU include) */
static int recv_frame_ex(int budget_ms, int *why) {
    if (why) *why = NB_RECV_SHUTDOWN;
    long deadline = 0;
    if (budget_ms >= 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        deadline = (long)(now.tv_sec * 1000L + now.tv_nsec / 1000000L) + budget_ms;
    }
    /* 1 = readable, 0 = budget spent / error. budget_ms < 0 never waits. */
    struct pollfd nbpfd;
    int nbok;
    nbpfd.fd = STDIN_FILENO;
    nbpfd.events = POLLIN;
    nbpfd.revents = 0;
    #define NB_READY_WAIT()                                                \
        ({                                                                 \
            if (budget_ms < 0) { nbok = 1; }                               \
            else {                                                         \
                int nbrc;                                                  \
                do { nbrc = poll(&nbpfd, 1, (int)ms_until(deadline)); }   \
                while (nbrc < 0 && errno == EINTR);                        \
                nbok = (nbrc > 0);                                          \
            }                                                              \
            nbok;                                                          \
        })
    char lenbuf[16];
    size_t i = 0;
    for (;;) {
        char c;
        if (g_pb >= 0) { c = (char)g_pb; g_pb = -1; }
        else {
            if (!NB_READY_WAIT()) { if (why) *why = NB_RECV_TIMEOUT; return 0; }
            ssize_t r = read(STDIN_FILENO, &c, 1);
            if (r == 0) return 0;                 /* EOF */
            if (r < 0) {
                if (errno == EINTR) continue;
                return 0;
            }
        }
        if (c == '\n') break;
        if (i < sizeof(lenbuf) - 1) lenbuf[i++] = c;
    }
    lenbuf[i] = 0;
    long n = strtol(lenbuf, NULL, 10);
    if (n < 0 || n > MAX_MSG) return 0;
    size_t got = 0;
    while (got < (size_t)n) {
        if (!NB_READY_WAIT()) { if (why) *why = NB_RECV_TIMEOUT; return 0; }
        ssize_t r = read(STDIN_FILENO, g_rbuf + got, (size_t)n - got);
        if (r == 0) return 0;
        if (r < 0) { if (errno == EINTR) continue; return 0; }
        got += (size_t)r;
    }
    g_rbuf[got] = 0;
    g_rlen = got;
    /* The trailing '\n' is OPTIONAL and must never be stolen: peek with a
     * zero timeout, eat it if it is there, push it back if it is not. */
    {
        int c = recv_getc(why);
        if (c >= 0 && c != '\n') g_pb = c;
    }
    #undef NB_READY_WAIT
    return 1;
}
