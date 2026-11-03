#define _POSIX_C_SOURCE 200809L
/* edit_prefs - the rpg-edit-hq preferences: @.apps/rpg-edit-hq/state/prefs.txt (key=value) and the label file board/state/prefs_ui.txt for its own "Settings" toolbar tab (inside the editor window, NOT the theme settings window).
 *   edit_prefs <house> show                 rewrite board/state/prefs_ui.txt from prefs.txt (call once; the window reads it via vars=)
 *   edit_prefs <house> cycle <key>          next allowed value of <key>, wrap around; then rewrite board/state/prefs_ui.txt
 * Keys (add a row to KEYS to add a preference): editor_open = last | own ; sidebar_start = last | pets | palettes | none. Unknown/missing value = the FIRST value (the default).
 * Both files are rewritten whole (temp + rename), a fixed size (< 1 KB): no growth. Readers: board/open_board.sh (editor_open), ops/pal_side.c (sidebar_start). Built by build_rpg_edit_hq.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static const struct { const char *key, *vals, *title; const char *words[5]; } KEYS[] = {
    { "editor_open",   "last,own",                 "editor opens",   { "the last project", "its own book" } },
    { "sidebar_start", "last,pets,palettes,none",  "editor sidebar", { "last used", "pets", "palettes", "none (both hidden)" } },
};
#define NK (int)(sizeof KEYS / sizeof KEYS[0])
static char PREF[PATH_MAX], UI[PATH_MAX];
static int nth(const char *vals, int n, char *out, size_t sz) { const char *p = vals; for (int i = 0; ; i++) { const char *c = strchr(p, ','); size_t l = c ? (size_t)(c - p) : strlen(p); if (i == n) { snprintf(out, sz, "%.*s", (int)l, p); return 1; } if (!c) return 0; p = c + 1; } }
static int count(const char *vals) { int n = 1; for (const char *p = vals; *p; p++) if (*p == ',') n++; return n; }
static int get(int k, char *out, size_t sz) { /* index of the stored value (0 = default) */
    char want[64] = ""; FILE *f = fopen(PREF, "r"); size_t kl = strlen(KEYS[k].key);
    if (f) { static char ln[512]; while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, KEYS[k].key, kl) && ln[kl] == '=') { snprintf(want, sizeof want, "%s", ln + kl + 1); want[strcspn(want, "\r\n")] = 0; } fclose(f); }
    for (int i = 0; i < count(KEYS[k].vals); i++) { char v[32]; nth(KEYS[k].vals, i, v, sizeof v); if (!strcmp(v, want)) { snprintf(out, sz, "%s", v); return i; } }
    nth(KEYS[k].vals, 0, out, sz); return 0;
}
static void write_prefs(const int *idx) {
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", PREF); FILE *o = fopen(tmp, "w"); if (!o) return;
    for (int k = 0; k < NK; k++) { char v[32]; nth(KEYS[k].vals, idx[k], v, sizeof v); fprintf(o, "%s=%s\n", KEYS[k].key, v); } fclose(o); rename(tmp, PREF);
}
static void write_ui(const int *idx, const char *self) {
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", UI); FILE *o = fopen(tmp, "w"); if (!o) return;
    (void)self; for (int k = 0; k < NK; k++) fprintf(o, "pref_%s_label=%s: %s\n", KEYS[k].key, KEYS[k].title, KEYS[k].words[idx[k]]); fclose(o); rename(tmp, UI);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: edit_prefs <house> show|cycle <key>\n"); return 2; }
    snprintf(PREF, sizeof PREF, "%s/@.apps/rpg-edit-hq/state/prefs.txt", argv[1]); snprintf(UI, sizeof UI, "%s/@.apps/rpg-edit-hq/board/state/prefs_ui.txt", argv[1]);
    char self[PATH_MAX]; if (!realpath(argv[0], self)) snprintf(self, sizeof self, "%s", argv[0]);
    int idx[8] = {0}; for (int k = 0; k < NK; k++) { char v[32]; idx[k] = get(k, v, sizeof v); }
    if (!strcmp(argv[2], "cycle")) { if (argc < 4) return 2; int hit = 0; for (int k = 0; k < NK; k++) if (!strcmp(argv[3], KEYS[k].key)) { idx[k] = (idx[k] + 1) % count(KEYS[k].vals); hit = 1; } if (!hit) { fprintf(stderr, "edit_prefs: unknown key %s\n", argv[3]); return 2; } write_prefs(idx); }
    else if (strcmp(argv[2], "show")) { fprintf(stderr, "edit_prefs: unknown verb %s\n", argv[2]); return 2; }
    write_ui(idx, self); return 0;
}
