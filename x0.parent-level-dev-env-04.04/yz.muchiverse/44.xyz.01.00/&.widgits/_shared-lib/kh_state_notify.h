/* kh_state_notify.h - "did this state change?" without reading it (pure, header-only, text-included; no globals, no I/O except the three backends below).
 *
 * WHY (2026-10-12, branch `marker`; measured in 08-roadmap/design-docs/INMEM-DB-CPU-THEORY-REPORT.md): the daemons and windows re-read the same small state files 10-60 times a second (the dsr-rpg daemon made
 * 411 file operations per second while idle: every pet file 10x/s, flags, daylight, screen state, and a scan of every /proc/<pid>/cmdline looking for a window). House rule (CENTROID_GOLD_STD #8): act only
 * when an APPEND-ONLY MARKER grows (one stat, size only, never mtime/hash). Writers append one byte after they write; readers compare the size to what they last saw.
 *
 * STATE_BACKEND (environment, read once by sn_init; the on/off switch for profiling):
 *   file    (default) today's behaviour: sn_changed() always says "yes, reload", sn_bump() does nothing. Zero risk, zero gain.
 *   marker  one tiny append-only file per slot, <state dir>/.m<slot>: sn_bump() appends a byte, sn_changed() stats its size.
 *   shmem   the in-memory "DB" demo: a POSIX shared-memory VERSION BOARD (SN_SLOTS 64-bit counters, name /kh_state_<hash of the state dir>) bumped with an atomic add and read with an atomic load: no file is
 *           opened or stat'ed at all. The VALUES stay in the files (mirror mode of INMEM-DB-STATE-LAYER-PLAN.md); only the "did it change" signal lives in memory. A full key-value port of wraith-alpha's
 *           tpmos_share_kvp is the later step; this is the smallest real in-memory backend and runs the same A/B.
 * Writers that are not our code (events-hq ops, shell scripts) do not bump: for those files use sn_sig_changed() (one stat: size + mtime_ns, a documented fallback until their writers append a marker) and keep a
 * slow safety refresh. Own writes: sn_bump_own() advances the reader's "seen" value too, so a process does not reload what it just wrote itself. */
#ifndef KH_STATE_NOTIFY_H
#define KH_STATE_NOTIFY_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define SN_SLOTS 32
typedef enum { SN_FILE = 0, SN_MARKER = 1, SN_SHMEM = 2 } SnMode;
typedef struct {
    SnMode mode; char dir[PATH_MAX];
    unsigned long seen[SN_SLOTS]; unsigned char have[SN_SLOTS];
    unsigned long *shm;
} SnState;

static const char *sn_mode_name(SnMode m) { return m == SN_MARKER ? "marker" : m == SN_SHMEM ? "shmem" : "file"; }

static void sn_init(SnState *s, const char *dir) {
    memset(s, 0, sizeof *s); snprintf(s->dir, sizeof s->dir, "%s", dir);
    const char *e = getenv("STATE_BACKEND"); s->mode = SN_FILE;
    if (e && !strcmp(e, "marker")) s->mode = SN_MARKER; else if (e && (!strcmp(e, "shmem") || !strcmp(e, "db"))) s->mode = SN_SHMEM;
    if (s->mode == SN_SHMEM) {
        unsigned long h = 1469598103934665603UL; for (const char *p = dir; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211UL; }
        char nm[64]; snprintf(nm, sizeof nm, "/kh_state_%016lx", h);
        int fd = shm_open(nm, O_CREAT | O_RDWR, 0600);
        if (fd >= 0 && ftruncate(fd, (off_t)(SN_SLOTS * sizeof(unsigned long))) == 0) {
            void *m = mmap(NULL, SN_SLOTS * sizeof(unsigned long), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (m != MAP_FAILED) s->shm = (unsigned long *)m;
        }
        if (fd >= 0) close(fd);
        if (!s->shm) s->mode = SN_MARKER;      /* no shared memory here: fall back to the marker files */
    }
}
static int sn_active(const SnState *s) { return s->mode != SN_FILE; }

static unsigned long sn_current(const SnState *s, int slot) {
    if (slot < 0 || slot >= SN_SLOTS) return 0;
    if (s->mode == SN_SHMEM) return __atomic_load_n(&s->shm[slot], __ATOMIC_ACQUIRE);
    if (s->mode == SN_MARKER) { char p[PATH_MAX + 16]; struct stat st; snprintf(p, sizeof p, "%s/.m%d", s->dir, slot); return stat(p, &st) == 0 ? (unsigned long)st.st_size : 0; }
    return 0;
}
/* a writer calls this AFTER it wrote the state */
static void sn_bump(SnState *s, int slot) {
    if (slot < 0 || slot >= SN_SLOTS) return;
    if (s->mode == SN_SHMEM) { __atomic_add_fetch(&s->shm[slot], 1UL, __ATOMIC_RELEASE); return; }
    if (s->mode == SN_MARKER) { char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/.m%d", s->dir, slot); int fd = open(p, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd >= 0) { if (write(fd, "x", 1) != 1) { /* best effort */ } close(fd); } }
}
/* a writer that also holds the new state in memory: bump, and skip its own bump when nobody else moved the slot meanwhile */
static void sn_bump_own(SnState *s, int slot) {
    if (!sn_active(s) || slot < 0 || slot >= SN_SLOTS) return;
    unsigned long before = sn_current(s, slot); sn_bump(s, slot); unsigned long after = sn_current(s, slot);
    if (s->have[slot] && s->seen[slot] == before && after == before + 1) s->seen[slot] = after;
}
/* a reader asks: 1 = reload (changed since I last asked, or the first time), 0 = unchanged. In file mode always 1 (the caller keeps its old read-everything behaviour). */
static int sn_changed(SnState *s, int slot) {
    if (s->mode == SN_FILE || slot < 0 || slot >= SN_SLOTS) return 1;
    unsigned long c = sn_current(s, slot);
    if (!s->have[slot]) { s->have[slot] = 1; s->seen[slot] = c; return 1; }
    if (c != s->seen[slot]) { s->seen[slot] = c; return 1; }
    return 0;
}

/* signature of a file somebody else writes (not our code): one stat; changed = size or mtime (ns) differs from the last look, or the file appeared/vanished. */
typedef struct { long size; long sec; long nsec; unsigned char have; } SnSig;
static int sn_sig_changed(SnSig *g, const char *path) {
    struct stat st; long sz = -1, sc = 0, ns = 0;
    if (stat(path, &st) == 0) { sz = (long)st.st_size; sc = (long)st.st_mtim.tv_sec; ns = (long)st.st_mtim.tv_nsec; }
    if (g->have && g->size == sz && g->sec == sc && g->nsec == ns) return 0;
    g->have = 1; g->size = sz; g->sec = sc; g->nsec = ns; return 1;
}
#endif
