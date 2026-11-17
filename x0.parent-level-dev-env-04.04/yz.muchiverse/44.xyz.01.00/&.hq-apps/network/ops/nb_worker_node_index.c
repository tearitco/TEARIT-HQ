/* nb_worker_node_index.c - node handle index helpers (mechanical extract, single-TU include) */
/* JS handles are a plain numeric index into g_nodeindex (index -> NbNode*),
 * which is more robust than round-tripping a JSValue pointer object and
 * survives across multiple lazily-created wrappers for the same node. */
static NbNode **g_nodeindex = NULL;
static int g_nodecount = 0, g_nodecap = 0;
#define NODE_HANDLE_CAP 250000   /* plan step 5: bound JS-created wrappers */
static int node_index(NbNode *n) {
    for (int i = 0; i < g_nodecount; i++) if (g_nodeindex[i] == n) return i;
    if (g_nodecount >= NODE_HANDLE_CAP) return -1;   /* cap: fail the wrapper */
    if (g_nodecount >= g_nodecap) {
        int nc = g_nodecap ? g_nodecap * 2 : 64;
        if (nc > NODE_HANDLE_CAP) nc = NODE_HANDLE_CAP;
        NbNode **na = realloc(g_nodeindex, (size_t)nc * sizeof(*na));
        if (!na) return -1;
        g_nodeindex = na; g_nodecap = nc;
    }
    g_nodeindex[g_nodecount++] = n;
    return g_nodecount - 1;
}
static void node_index_reset(void) {
    free(g_nodeindex);
    g_nodeindex = NULL;
    g_nodecount = 0;
    g_nodecap = 0;
}
