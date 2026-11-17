/* nb_worker_ms_until.c - monotonic deadline helper (mechanical extract, single-TU include) */
static long ms_until(long deadline_ms) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long left = deadline_ms - (long)(now.tv_sec * 1000L + now.tv_nsec / 1000000L);
    return left < 0 ? 0 : left;
}
