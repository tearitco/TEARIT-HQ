/* dust_zoom - R2c of rpg-dust: zoom in / zoom out as EVENTS with an append-only ledger (BOOK 16.game/RPG-DUST-DESIGN.md sections 5 and 9: "every zoom is an event; the world is a function of (seed, ledger)").
 *   dust_zoom start    <region addr>        begin (or restart) at a region address. Writes a START row.
 *   dust_zoom zoom_in  <cx> <cy>            from the current region address to its cell cx,cy (0..15): appends ZOOM, prints the new address
 *   dust_zoom zoom_out                      from an area address back to its region: appends ZOOM, prints the new address
 *   dust_zoom where                         prints the current address (replayed from the ledger; writes nothing)
 * The only state is <app>/state/zoom_ledger.txt (append-only, pipe rows):
 *   START   | seq | addr
 *   ZOOM    | seq | zoom_in/zoom_out | addr_before | addr_after | cx | cy
 *   REFUSED | seq | verb | addr_before | reason       (a refused zoom changes nothing but is recorded; the exit code is 1)
 * The current address is ALWAYS replayed from the ledger (last START / ZOOM addr_after): nothing else is stored, so deleting any derived file loses nothing.
 * Arguments of a verb may also come in the environment variable PET_ARG (space separated), which is how an events-hq page node `verb=zoom_in|arg=3 4` hands them over (same convention as the pets games).
 * Exit: 0 done, 1 refused (logged), 2 usage / no ledger directory.
 * Build: sh ops/build_dust_check.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dust_pdl.c"
#include "dust_addr.c"
static char LEDGER[2 * PATH_MAX + 32];
static int ledger_path(const char *argv0) {
    const char *env = getenv("DUST_STATE"); if (env && *env) { snprintf(LEDGER, sizeof LEDGER, "%s/zoom_ledger.txt", env); return 1; }
    char rp[PATH_MAX]; if (!realpath(argv0, rp)) return 0;                 /* <app>/ops/+x/dust_zoom.+x -> <app>/state/ */
    for (int i = 0; i < 3; i++) { char *sl = strrchr(rp, '/'); if (!sl) return 0; *sl = 0; }
    char sd[PATH_MAX + 16]; snprintf(sd, sizeof sd, "%s/state", rp); mkdir(sd, 0755); snprintf(LEDGER, sizeof LEDGER, "%s/zoom_ledger.txt", sd); return 1;
}
/* replay: current address and the next sequence number. Returns 1 when there is a current address. */
static int replay(char *addr, size_t sz, long *next) {
    FILE *f = fopen(LEDGER, "r"); *next = 1; addr[0] = 0; if (!f) return 0; char ln[1024]; int have = 0;
    while (fgets(ln, sizeof ln, f)) {
        char *fl[8]; int n = split_bar(ln, fl, 8); if (n < 2) continue; long seq = atol(fl[1]); if (seq >= *next) *next = seq + 1;
        if (!strcmp(fl[0], "START") && n >= 3) { cp(addr, sz, fl[2]); have = 1; }
        else if (!strcmp(fl[0], "ZOOM") && n >= 5) { cp(addr, sz, fl[4]); have = 1; }
    }
    fclose(f); return have;
}
static int append(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static int append(const char *fmt, ...) { FILE *f = fopen(LEDGER, "a"); if (!f) { fprintf(stderr, "dust_zoom: cannot append to %s\n", LEDGER); return 0; } va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); return fclose(f) == 0; }
static int refuse(long seq, const char *verb, const char *addr, const char *why) { append("REFUSED | %ld | %s | %s | %s\n", seq, verb, addr, why); printf("refused: %s\n", why); return 1; }
int main(int argc, char **argv) {
    if (argc < 2 || !ledger_path(argv[0])) { fprintf(stderr, "usage: dust_zoom start <region addr> | zoom_in <cx> <cy> | zoom_out | where   (ledger: <app>/state/zoom_ledger.txt or $DUST_STATE/zoom_ledger.txt)\n"); return 2; }
    const char *v = argv[1]; char a1[200] = "", a2[200] = ""; int na = argc - 2; if (na >= 1) cp(a1, sizeof a1, argv[2]); if (na >= 2) cp(a2, sizeof a2, argv[3]);
    const char *pa = getenv("PET_ARG"); if (na == 0 && pa && *pa) { char b[200]; cp(b, sizeof b, pa); char *t = strtok(b, " "); if (t) { cp(a1, sizeof a1, t); na = 1; t = strtok(NULL, " "); if (t) { cp(a2, sizeof a2, t); na = 2; } } }
    char cur[400]; long seq; int have = replay(cur, sizeof cur, &seq);
    if (!strcmp(v, "where")) { if (!have) { fprintf(stderr, "dust_zoom: no START yet\n"); return 2; } printf("%s\n", cur); return 0; }
    if (!strcmp(v, "start")) {
        char p[400]; int cx, cy; if (na < 1 || !a1[0] || split_cell(a1, p, sizeof p, &cx, &cy) || strstr(a1, "cell:")) { return refuse(seq, v, have ? cur : "-", "start needs a region address (no cell: component)"); }
       
        if (!append("START | %ld | %s\n", seq, a1)) { return 2; }
        printf("%s\n", a1); return 0;
    }
    if (!strcmp(v, "zoom_in")) {
        if (!have) { return refuse(seq, v, "-", "no START yet"); }
        char p[400]; int c1, c2; if (split_cell(cur, p, sizeof p, &c1, &c2)) return refuse(seq, v, cur, "already at the area level: nothing below it is built");
        char *e1, *e2; long cx = na >= 1 ? strtol(a1, &e1, 10) : -1, cy = na >= 2 ? strtol(a2, &e2, 10) : -1;
        if (na < 2 || *e1 || *e2 || cx < 0 || cy < 0 || cx >= N || cy >= N) { return refuse(seq, v, cur, "zoom_in needs a cell cx cy, each 0..15"); }
        char next[500]; snprintf(next, sizeof next, "%s/cell:%ld,%ld", cur, cx, cy);
        if (!append("ZOOM | %ld | zoom_in | %s | %s | %ld | %ld\n", seq, cur, next, cx, cy)) { return 2; }
        printf("%s\n", next); return 0;
    }
    if (!strcmp(v, "zoom_out")) {
        if (!have) { return refuse(seq, v, "-", "no START yet"); }
        char p[400]; int cx, cy; if (!split_cell(cur, p, sizeof p, &cx, &cy)) return refuse(seq, v, cur, "already at the region level: nothing above it is built");
        if (!append("ZOOM | %ld | zoom_out | %s | %s | %d | %d\n", seq, cur, p, cx, cy)) { return 2; }
        printf("%s\n", p); return 0;
    }
    fprintf(stderr, "dust_zoom: unknown verb %s\n", v); return 2;
}
