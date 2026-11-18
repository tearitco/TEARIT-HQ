/* nb_worker_rand_bytes.c - OS entropy helper (mechanical extract, single-TU include) */
/* OS entropy for the crypto surfaces (2026-11-02).
 *
 * WHY: getRandomValues used rand()&0xFF and NOTHING calls srand(), so glibc
 * seeded from 1 and every worker process produced the SAME bytes. Measured on
 * three separate processes: all returned 67c6697351ff4aec, and the same
 * randomUUID. That is a real correctness bug in a CSPRNG-shaped API, and a
 * loud bot signal - a browser whose "random" values repeat every session.
 *
 * Read the kernel CSPRNG via getrandom(2). rand() survives only as the
 * fallback for the case where the kernel call fails: degraded, but explicitly
 * so, rather than silently deterministic. (getrandom rather than
 * open("/dev/urandom") because this translation unit already has a `read`
 * symbol that collides with <unistd.h>'s.) */
static void nb_rand_bytes(unsigned char *out, size_t n){
    size_t got = 0;
    while (got < n) {
        ssize_t r = getrandom(out + got, n - got, 0);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; break; }
        got += (size_t)r;
    }
    if (got == n) return;
    for (size_t i = 0; i < n; i++) out[i] = (unsigned char)(rand() & 0xFF);
}
