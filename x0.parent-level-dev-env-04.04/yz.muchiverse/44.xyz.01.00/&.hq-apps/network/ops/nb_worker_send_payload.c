/* nb_worker_send_payload.c - worker wire frame sender (mechanical extract, single-TU include) */
static void send_payload(const char *payload, size_t n) {
    if (g_cli) {
        /* plain text: drop the "RENDER\n" frame header, then the rows */
        size_t off = (strncmp(payload, "RENDER\n", 7) == 0 && n >= 7) ? 7 : 0;
        (void)!write(STDOUT_FILENO, payload + off, n - off);
        if (n - off == 0 || payload[n - 1] != '\n')
            (void)!write(STDOUT_FILENO, "\n", 1);
        return;
    }
    char lenbuf[16];
    int ln = snprintf(lenbuf, sizeof(lenbuf), "%.6d\n", (int)n);
    (void)!write(STDOUT_FILENO, lenbuf, (size_t)ln);
    if (n) (void)!write(STDOUT_FILENO, payload, n);
    (void)!write(STDOUT_FILENO, "\n", 1);
}
