/* nb_worker_cookie_jar_init.c - cookie jar init helper (mechanical extract, single-TU include) */
static void cookie_jar_init(void) {
    g_cookie_path_set = 1;
    const char *env = getenv("NB_COOKIES_FILE");
    if (env && env[0]) { snprintf(g_cookie_path, sizeof(g_cookie_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_cookie_path, sizeof(g_cookie_path), "%s/.config/nbjs/nb_cookies.txt", home);
    else
        g_cookie_path[0] = 0;   /* no writable location: reads '', writes no-op */
}
