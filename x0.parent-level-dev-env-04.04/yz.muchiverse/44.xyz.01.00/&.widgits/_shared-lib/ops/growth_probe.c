/* growth_probe - the runtime-file growth audit as an op (owner 2026-11-05, audit decision D9 in HQ-IQ-BOOK 02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md).
 *   growth_probe <house> --standard <RUNTIME-FILE-GROWTH-STANDARD.md> [--min-mb N (default 1)] [--live-min M (default 120)]
 * AGENTS.md: every file the house writes while it runs needs an enforced cap and a row in the standard. A grep over the C sources cannot find the writers (most appends go through a path variable built
 * elsewhere: 18 of 19 in board-viewer/ops on 2026-11-05), so this OBSERVES instead. It walks the house runtime folders (#.desktop, @.apps, &.hq-apps, &.widgits; never .git, xyzfs or a build output) and lists every
 * text-like file (.txt .log .jsonl .csv .tsv) over --min-mb with its age and whether its file name appears in the standard. The verdict that matters: a file that is LARGE, was WRITTEN in the last --live-min
 * minutes (a live writer) and is NOT named in the standard = a candidate uncapped writer. Exit 1 when there is one (the lines say which), 0 otherwise (large stale files are listed for information, e.g. a 5.8 MB
 * animation_queue.txt last written 2026-10-05 is history, not growth). Plain C, read-only, one walk when run by hand. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <limits.h>
#include <ftw.h>
#include <sys/stat.h>
static char *STD; static long MINB; static long LIVES; static int nlist = 0, nbad = 0; static time_t NOW;
static int texty(const char *n) { const char *d = strrchr(n, '.'); if (!d) return 0; static const char *e[] = {".txt", ".log", ".jsonl", ".csv", ".tsv"}; for (size_t i = 0; i < sizeof e / sizeof *e; i++) if (!strcasecmp(d, e[i])) return 1; return 0; }
static int cb(const char *path, const struct stat *st, int type, struct FTW *ftw) {
    (void)ftw; if (type != FTW_F || st->st_size < MINB) return 0; const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    if (!texty(base) || strstr(path, "/.git/") || strstr(path, "/xyzfs/") || strstr(path, "/node_modules/")) return 0;
    long age_min = (long)((NOW - st->st_mtime) / 60); int named = STD && strstr(STD, base) != NULL; int live = age_min <= LIVES; int bad = live && !named; nlist++; if (bad) nbad++;
    printf("%7.1f MB  %s  %s  %s%s\n", (double)st->st_size / 1048576.0, live ? "live" : "stale", named ? "in-standard" : "NOT-in-standard", path, bad ? "   <== candidate uncapped writer" : ""); return 0;
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: growth_probe <house> --standard <file> [--min-mb N] [--live-min M]\n"); return 2; }
    const char *house = argv[1], *std = NULL; long mb = 1; LIVES = 120;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--standard") && i + 1 < argc) std = argv[++i]; else if (!strcmp(argv[i], "--min-mb") && i + 1 < argc) mb = atol(argv[++i]); else if (!strcmp(argv[i], "--live-min") && i + 1 < argc) LIVES = atol(argv[++i]); else { fprintf(stderr, "growth_probe: unknown argument %s\n", argv[i]); return 2; } }
    if (!std) { fprintf(stderr, "growth_probe: --standard is required\n"); return 2; }
    FILE *f = fopen(std, "r"); if (!f) { fprintf(stderr, "growth_probe: cannot read the standard %s\n", std); return 2; } fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); STD = malloc((size_t)n + 1); if (!STD || fread(STD, 1, (size_t)n, f) != (size_t)n) return 2; STD[n] = 0; fclose(f);
    MINB = mb * 1048576L; NOW = time(NULL); static const char *roots[] = {"#.desktop", "@.apps", "&.hq-apps", "&.widgits"};
    for (size_t i = 0; i < sizeof roots / sizeof *roots; i++) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s", house, roots[i]); struct stat st; if (stat(p, &st) == 0) nftw(p, cb, 16, FTW_PHYS); }
    printf("%d large text-like file(s) over %ld MB; %d live (written in the last %ld min) and not named in the standard\n", nlist, mb, nbad, LIVES); return nbad ? 1 : 0;
}
