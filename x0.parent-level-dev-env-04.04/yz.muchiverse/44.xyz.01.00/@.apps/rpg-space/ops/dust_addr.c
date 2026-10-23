/* dust_addr.c - pure text-include shared by dust_gen and dust_zoom (two consumers, no state): the address grammar. A region address is path components joined by '/'; an AREA address is a region address plus a last component cell:cx,cy (0..N-1). Needs stdio/string. */
#ifndef DUST_ADDR_C
#define DUST_ADDR_C
#define N 16
/* 1 and fills parent / cx / cy when addr ends in a valid /cell:cx,cy; 0 otherwise (a region address, or a malformed cell) */
static __attribute__((unused)) int split_cell(const char *addr, char *parent, size_t psz, int *cx, int *cy) {
    const char *slash = strrchr(addr, '/'); const char *last = slash ? slash + 1 : addr; char junk;
    if (!slash || sscanf(last, "cell:%d,%d%c", cx, cy, &junk) != 2 || *cx < 0 || *cy < 0 || *cx >= N || *cy >= N) return 0;
    size_t l = (size_t)(slash - addr); if (l == 0 || l >= psz) return 0; memcpy(parent, addr, l); parent[l] = 0; return 1;
}
#endif
