/* auto_observe - read a window's state surface; change detection by MARKER (size), not mtime.   auto_observe --manifest FILE --cursor DIR [--house DIR]
 * Manifest rows used:  APP | name | pidfile ;  STATE | name | file | kind (append|rewrite).   Prints one `OBS | app | name | size | first|same|changed | hash` row per STATE; a missing file prints `OBS | app | name | unknown` and the exit code is 3 (never read as zero).
 * Cursor: DIR/<app>_<name>.cur holds the last size and hash. append kind: changed = size grew (shrinking is reported as changed too); rewrite kind: changed = hash differs (size alone is not enough for a rewritten file). */
#include "auto_common.c"
static unsigned long fnv(const char *path) { FILE *f = fopen(path, "rb"); if (!f) return 0; unsigned long h = 1469598103934665603UL; int c; while ((c = fgetc(f)) != EOF) { h ^= (unsigned char)c; h *= 1099511628211UL; } fclose(f); return h; }
int main(int argc, char **argv) {
    const char *mf = NULL, *cur = NULL; char house[PL]; if (!getcwd(house, sizeof house)) house[0] = 0; const char *eh = getenv("HOUSE"); if (eh) snprintf(house, sizeof house, "%s", eh);
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--manifest") && i + 1 < argc) mf = argv[++i]; else if (!strcmp(argv[i], "--cursor") && i + 1 < argc) cur = argv[++i]; else if (!strcmp(argv[i], "--house") && i + 1 < argc) snprintf(house, sizeof house, "%s", argv[++i]); else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; } }
    if (!mf || !cur) { fprintf(stderr, "usage: auto_observe --manifest FILE --cursor DIR [--house DIR]\n"); return 2; } mkdir(cur, 0700);
    FILE *m = fopen(mf, "r"); if (!m) { fprintf(stderr, "cannot read manifest\n"); return 2; } char ln[PL], app[128] = "?"; int unknown = 0;
    while (fgets(ln, sizeof ln, m)) { if (ln[0] == '#') continue; char *f[6]; int n = ac_split(ln, f, 6); if (n >= 2 && !strcmp(f[0], "APP")) snprintf(app, sizeof app, "%s", f[1]);
        else if (n >= 4 && !strcmp(f[0], "STATE")) { char p[PL], cp[PL]; ac_abs(house, f[2], p, sizeof p); long sz = ac_size(p); if (sz < 0) { printf("OBS | %s | %s | unknown\n", app, f[1]); unknown = 1; continue; }
            unsigned long h = fnv(p); snprintf(cp, sizeof cp, "%s/%s_%s.cur", cur, app, f[1]); long ls = -1; unsigned long lh = 0; FILE *c = fopen(cp, "r"); if (c) { if (fscanf(c, "%ld %lu", &ls, &lh) != 2) ls = -1; fclose(c); }
            const char *st = ls < 0 ? "first" : !strcmp(f[3], "append") ? (sz != ls ? "changed" : "same") : (h != lh ? "changed" : "same");
            printf("OBS | %s | %s | %ld | %s | %lx\n", app, f[1], sz, st, h); c = fopen(cp, "w"); if (c) { fprintf(c, "%ld %lu\n", sz, h); fclose(c); } } }
    fclose(m); return unknown ? 3 : 0;
}
