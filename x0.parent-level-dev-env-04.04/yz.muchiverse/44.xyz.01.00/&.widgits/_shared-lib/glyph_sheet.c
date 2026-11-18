/* glyph_sheet.c - the Chinese character of a sprite sheet (owner 2026-11-18: ASCII-view cells are ALWAYS Chinese characters, never the first letter of an English sheet name; D23).
 * Text-included helper (pure apart from reading ONE data file): gs_load(house) reads <house>/&.widgits/board-viewer/glyph_sheets.pdl (reloaded when its size changes), gs_find(sheet, out, n) returns
 * 1 and writes the UTF-8 character for the sheet, or the DEFAULT row's character when the sheet is not listed (0 only when the file is missing or has no DEFAULT row: the caller then uses its own
 * Chinese fallback). The stem of a sheet name drops a leading ! or $ and trailing digits and is compared case-insensitively. Data rows: SHEET | stem | char ; DEFAULT | char. */
#ifndef GLYPH_SHEET_C
#define GLYPH_SHEET_C
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#define GS_UNUSED __attribute__((unused))
#define GS_MAX 96
static char gs_stem[GS_MAX][40], gs_glyph[GS_MAX][8], gs_def[8]; static int gs_n = 0; static long gs_size = -1; static char gs_path[4400];
static GS_UNUSED void gs_norm(const char *in, char *out, size_t n) {
    if (*in == '!' || *in == '$') in++;
    size_t k = 0; for (; *in && k + 1 < n; in++) out[k++] = (char)tolower((unsigned char)*in); out[k] = 0;
    while (k > 0 && isdigit((unsigned char)out[k - 1])) out[--k] = 0;
}
static GS_UNUSED char *gs_trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static GS_UNUSED void gs_load(const char *house) {
    snprintf(gs_path, sizeof gs_path, "%s/&.widgits/board-viewer/glyph_sheets.pdl", house); struct stat st; long sz = stat(gs_path, &st) == 0 ? (long)st.st_size : -2;
    if (sz == gs_size) return;
    gs_size = sz; gs_n = 0; gs_def[0] = 0;
    if (sz < 0) return;
    FILE *f = fopen(gs_path, "r"); if (!f) return; char ln[256];
    while (fgets(ln, sizeof ln, f)) {
        char *p = gs_trim(ln); if (!*p || *p == '#') continue; char *b1 = strchr(p, '|'); if (!b1) continue; *b1 = 0; char *kind = gs_trim(p); char *rest = b1 + 1;
        if (!strcmp(kind, "DEFAULT")) { snprintf(gs_def, sizeof gs_def, "%s", gs_trim(rest)); continue; }
        if (!strcmp(kind, "SHEET") && gs_n < GS_MAX) { char *b2 = strchr(rest, '|'); if (!b2) continue; *b2 = 0; gs_norm(gs_trim(rest), gs_stem[gs_n], sizeof gs_stem[0]); snprintf(gs_glyph[gs_n], sizeof gs_glyph[0], "%s", gs_trim(b2 + 1)); if (gs_glyph[gs_n][0]) gs_n++; }
    }
    fclose(f);
}
static GS_UNUSED int gs_find(const char *sheet, char *out, size_t n) {
    char s[40]; gs_norm(sheet, s, sizeof s);
    for (int i = 0; i < gs_n; i++) if (!strcmp(gs_stem[i], s)) { snprintf(out, n, "%s", gs_glyph[i]); return 1; }
    if (gs_def[0]) { snprintf(out, n, "%s", gs_def); return 1; }
    return 0;
}
#endif
