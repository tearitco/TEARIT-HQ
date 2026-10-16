/* side_toggle <kv-file> <key> [show|hide|toggle]  - flip one visibility flag in a key=value file.
 * A pinned side bar (class ov-pin on a canvas-overlay-left/right row) is shown with show="${key}": the key is "1" when
 * visible and EMPTY when hidden (a "0" string would count as visible). The bottom bar's cell runs this with toggle, the bar's own
 * "_" runs it with hide (idempotent: two quick clicks must not cancel). Other keys in the file are kept; written atomically. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: side_toggle <kv-file> <key> [show|hide|toggle]\n"); return 2; }
    const char *path = argv[1], *key = argv[2], *mode = argc > 3 ? argv[3] : "toggle";
    static char lines[64][256]; int n = 0, found = -1; size_t kl = strlen(key);
    FILE *f = fopen(path, "r");
    if (f) { while (n < 64 && fgets(lines[n], sizeof lines[0], f)) { if (!strncmp(lines[n], key, kl) && lines[n][kl] == '=') found = n; n++; } fclose(f); }
    int cur = found >= 0 && lines[found][kl + 1] != '\n' && lines[found][kl + 1] != 0;
    int want = !strcmp(mode, "show") ? 1 : !strcmp(mode, "hide") ? 0 : !cur;
    char row[256]; snprintf(row, sizeof row, "%s=%s\n", key, want ? "1" : "");
    if (found >= 0) memcpy(lines[found], row, strlen(row) + 1); else if (n < 64) { memcpy(lines[n], row, strlen(row) + 1); n++; }
    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = fopen(tmp, "w"); if (!f) return 1;
    for (int i = 0; i < n; i++) fputs(lines[i], f);
    fclose(f); return rename(tmp, path) ? 1 : 0;
}
