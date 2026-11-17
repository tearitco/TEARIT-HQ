/* nb_worker_recv_getc.c - frame-reader pushback byte helper (mechanical extract, single-TU include) */
/* One-byte pushback for the frame reader (2026-10-13).
 *
 * This existed as a comment claiming the reader "buffered it back
 * conceptually by not over-reading". It did the opposite: it read one
 * byte unconditionally and THREW IT AWAY whenever that byte was not a
 * newline. Two ways that loses data, both silent:
 *
 *   - a sender that omits the trailing newline has its NEXT frame's
 *     first length digit eaten, and every frame after it is garbage;
 *   - the read itself is BLOCKING, so a sender that omits the newline
 *     and then goes quiet parks the resident worker in recv() forever.
 *
 * So: peek with a zero timeout, consume a newline if that is what is
 * there, and otherwise push the byte back for the next frame to read. */
static int g_pb = -1;              /* pushback byte, -1 = empty */

static int recv_getc(int *why) {
    if (g_pb >= 0) { int c = g_pb; g_pb = -1; return c; }
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO; pfd.events = POLLIN; pfd.revents = 0;
    int rc;
    do { rc = poll(&pfd, 1, 0); } while (rc < 0 && errno == EINTR);
    if (rc <= 0) return -1;                        /* nothing waiting */
    char c;
    ssize_t r = read(STDIN_FILENO, &c, 1);
    if (r <= 0) return -1;
    (void)why;
    return (unsigned char)c;
}
