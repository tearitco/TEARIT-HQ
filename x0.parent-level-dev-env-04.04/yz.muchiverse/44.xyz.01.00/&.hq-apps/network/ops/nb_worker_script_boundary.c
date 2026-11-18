/* nb_worker_script_boundary.c - script boundary marker and finder (mechanical extract, single-TU include) */
#define SCRIPT_BOUNDARY "/*nbjs-script-boundary*/"
static const char *find_script_boundary(const char *p, const char *end,
                                        const char **after) {
    const char *q = p;
    for (; q + sizeof(SCRIPT_BOUNDARY) - 1 <= end; q++) {
        if (q[0] == '/' && q[1] == '*' &&
            memcmp(q, SCRIPT_BOUNDARY, sizeof(SCRIPT_BOUNDARY) - 1) == 0) {
            *after = q + sizeof(SCRIPT_BOUNDARY) - 1;
            return q;
        }
    }
    return NULL;
}
