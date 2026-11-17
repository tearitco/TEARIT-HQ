/* nb_worker_split_lines.c - LOAD frame field splitter (mechanical extract, single-TU include) */
/* Split g_rbuf into the LOAD fields, copying each into a target buffer.
 * LOAD payload layout (newline-delimited, plan §4 + extra href/title):
 *   line 0 = "LOAD"
 *   line 1 = page.js path
 *   line 2 = fetch.dom path
 *   line 3 = href
 *   line 4 = title
 */
static void split_lines(char *fields[8]) {
    int fi = 0;
    char *p = g_rbuf;
    fields[0] = p;
    for (size_t i = 0; i < g_rlen; i++) {
        if (g_rbuf[i] == '\n') {
            g_rbuf[i] = 0;
            if (i + 1 < g_rlen && fi < 7) fields[++fi] = &g_rbuf[i + 1];
        }
    }
    for (int k = fi + 1; k < 8; k++) fields[k] = NULL;
}
