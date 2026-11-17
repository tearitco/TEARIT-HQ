/* nb_worker_send_status.c - worker STATUS frame responder (mechanical extract, single-TU include) */
static void send_status(const char *status) {
    if (g_cli) {
        g_cli_status_ok = (strncmp(status, "STATUS ok", 9) == 0);
        if (strncmp(status, "STATUS ", 7) == 0) status += 7;
        (void)!write(STDOUT_FILENO, status, strlen(status));
        (void)!write(STDOUT_FILENO, "\n", 1);
        return;
    }
    send_payload(status, strlen(status));
}
