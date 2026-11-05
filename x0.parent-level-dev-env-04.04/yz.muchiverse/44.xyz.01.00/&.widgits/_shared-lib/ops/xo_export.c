/* xo_export - copy ONE doc or media file into the owner's XO folder, the way AGENTS.md allows it (owner 2026-11-05, audit decision D8 in HQ-IQ-BOOK 02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md).
 *   xo_export <source file> <XO subfolder, e.g. 31.Nov/nov-4> <destination name> [--force]
 * XO is the owner's folder for learnings exported to other AI tools: DOCS AND MEDIA ONLY, every item named <pre>.<name> (`9.grok-handoff-...md`), no code, scripts, binaries, game data, logs or scratch trees.
 * The book file stays the ONE authority; the XO file is a projection of it, refreshed with this op (it was a hand `cp` after every edit). The op REFUSES:

 *   - a .md/.html/.txt whose first 4 KB contain a NUL byte (a binary renamed). LIMIT: source code renamed to .md cannot be told from prose; the name is the rule, say honestly what you export;
 *   - an extension that is not .md .html .png .jpg .jpeg .pdf .mp3 .mp4 .txt (so .c .sh .py .pdl .pal .+x and extensionless files never go in);
 *   - a destination name without a `<pre>.` prefix (at least one character, a dot, then a name), or containing '/' or "..";
 *   - a destination outside the XO root (default /home/no/Desktop/github/work/XO, env XO_ROOT overrides it, used by the harness), a subfolder with ".." in it;
 *   - overwriting an existing file with DIFFERENT bytes unless --force (the owner may have edited it); an identical file is reported as already current.
 * The copy is temp + rename. Exit 0 copied or already current, 1 refused, 2 usage. Plain C. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
static int same_bytes(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb"); int same = 1; if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    unsigned char x[8192], y[8192]; size_t ka, kb; do { ka = fread(x, 1, sizeof x, fa); kb = fread(y, 1, sizeof y, fb); if (ka != kb || memcmp(x, y, ka)) { same = 0; break; } } while (ka > 0);
    fclose(fa); fclose(fb); return same;
}
static void mkdirs(const char *path) { char t[PATH_MAX]; snprintf(t, sizeof t, "%s", path); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; } mkdir(t, 0755); }
int main(int argc, char **argv) {
    if (argc < 4 || argc > 5 || (argc == 5 && strcmp(argv[4], "--force"))) { fprintf(stderr, "usage: xo_export <source file> <XO subfolder> <destination name> [--force]\n"); return 2; }
    const char *src = argv[1], *sub = argv[2], *name = argv[3]; int force = argc == 5; const char *root = getenv("XO_ROOT"); if (!root || !root[0]) root = "/home/no/Desktop/github/work/XO";
    struct stat st; if (stat(src, &st) != 0 || !S_ISREG(st.st_mode)) { fprintf(stderr, "xo_export: REFUSED: %s is not a file\n", src); return 1; }
    const char *dot = strrchr(name, '.'); static const char *ok[] = {".md", ".html", ".png", ".jpg", ".jpeg", ".pdf", ".mp3", ".mp4", ".txt"}; int good = 0;
    for (size_t i = 0; dot && i < sizeof ok / sizeof *ok; i++) if (!strcasecmp(dot, ok[i])) good = 1;
    if (!good) { fprintf(stderr, "xo_export: REFUSED: '%s' is not a doc or media extension (XO takes .md .html .png .jpg .pdf .mp3 .mp4 .txt only; never code, scripts, binaries, game data)\n", name); return 1; }
    if (!strcasecmp(dot, ".md") || !strcasecmp(dot, ".html") || !strcasecmp(dot, ".txt")) { FILE *bf = fopen(src, "rb"); unsigned char bb[4096]; size_t bk = bf ? fread(bb, 1, sizeof bb, bf) : 0; if (bf) fclose(bf); for (size_t i = 0; i < bk; i++) if (!bb[i]) { fprintf(stderr, "xo_export: REFUSED: %s contains NUL bytes: a binary renamed to a text name does not belong in XO\n", src); return 1; } }      /* a text name with binary content */
    if (strchr(name, '/') || strstr(name, "..")) { fprintf(stderr, "xo_export: REFUSED: the destination name must be a plain file name\n"); return 1; }
    { const char *fd = strchr(name, '.'); if (!fd || fd == name || fd == dot) { fprintf(stderr, "xo_export: REFUSED: XO items are named <pre>.<name> (e.g. 9.grok-handoff.md); '%s' has no prefix\n", name); return 1; } }
    if (strstr(sub, "..") || sub[0] == '/') { fprintf(stderr, "xo_export: REFUSED: the subfolder must be relative to the XO root and without '..'\n"); return 1; }
    char dir[PATH_MAX], dst[PATH_MAX + 300], tmp[PATH_MAX + 320]; snprintf(dir, sizeof dir, "%s/%s", root, sub); snprintf(dst, sizeof dst, "%s/%s", dir, name); snprintf(tmp, sizeof tmp, "%s.xo_tmp", dst);
    if (access(dst, F_OK) == 0) { if (same_bytes(src, dst)) { printf("already current: %s\n", dst); return 0; } if (!force) { fprintf(stderr, "xo_export: REFUSED: %s exists with different bytes (the owner may have edited it); use --force to replace\n", dst); return 1; } }
    mkdirs(dir); FILE *a = fopen(src, "rb"), *b = a ? fopen(tmp, "wb") : NULL; unsigned char buf[8192]; size_t k; if (!a || !b) { fprintf(stderr, "xo_export: cannot write %s\n", tmp); if (a) fclose(a); return 1; }
    while ((k = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, k, b) != k) { fclose(a); fclose(b); remove(tmp); return 1; }
    fclose(a); if (fclose(b) != 0 || rename(tmp, dst) != 0) { remove(tmp); fprintf(stderr, "xo_export: write failed\n"); return 1; }
    printf("copied: %s\n", dst); return 0;
}
