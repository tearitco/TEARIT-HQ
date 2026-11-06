/* generator_level_op: draw a small test level from rows, reading every sheet from a ROOT folder. Draw it once with the original RPG Maker folder and once with good/ (and voxel output):
 * same layout, different art, so a replacement is judged in place. A sheet that does not exist under the root is an error (exit 2): a level never silently falls back to another folder.
 *   level <level.pdl> <root> <out.png> [<extraroot>]     a path starting with "x:" is read from <extraroot> instead of <root>
 * Rows: LEVEL | w | h (tiles of 48)   FILL | sheet | sx | sy   TILE | sheet | sx | sy | tx | ty   PIC | sheet | sx | sy | w | h | px | py (pixels)
 *       CHAR | sheet | index 0-7 | dir 0 down 1 left 2 right 3 up | frame 0-2 | tx | ty    (RPG Maker people sheet layout: 4 x 2 characters of 3 x 4 frames of 48) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"
typedef struct { char key[600]; unsigned char *px; int w, h; } Img;
static Img C[48]; static int nc;
static char ROOT[500], XROOT[500];
static unsigned char *O; static int OW, OH, ops;
static char *fld(char **p) { char *s = *p, *e; while (*s == ' ') s++; e = strchr(s, '|'); if (e) { *e = 0; *p = e + 1; } else { e = s + strlen(s); *p = e; } while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static Img *load(const char *sheet) {
    char path[1200]; int i, n;
    for (i = 0; i < nc; i++) if (!strcmp(C[i].key, sheet)) return &C[i];
    if (nc >= 48) return NULL;
    if (!strncmp(sheet, "x:", 2)) snprintf(path, sizeof path, "%s/%s", XROOT, sheet + 2); else snprintf(path, sizeof path, "%s/%s", ROOT, sheet);
    snprintf(C[nc].key, sizeof C[nc].key, "%s", sheet);
    C[nc].px = stbi_load(path, &C[nc].w, &C[nc].h, &n, 4);
    if (!C[nc].px) { fprintf(stderr, "missing sheet: %s\n", path); return NULL; }
    return &C[nc++];
}
static void blit(Img *s, int sx, int sy, int w, int h, int dx, int dy) {
    int x, y;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        const unsigned char *a; unsigned char *b; int al;
        if (sx + x < 0 || sy + y < 0 || sx + x >= s->w || sy + y >= s->h || dx + x < 0 || dy + y < 0 || dx + x >= OW || dy + y >= OH) continue;
        a = s->px + ((sy + y) * s->w + sx + x) * 4; b = O + ((dy + y) * OW + dx + x) * 4; al = a[3];
        if (al == 255) memcpy(b, a, 4); else if (al) { b[0] = (a[0] * al + b[0] * (255 - al)) / 255; b[1] = (a[1] * al + b[1] * (255 - al)) / 255; b[2] = (a[2] * al + b[2] * (255 - al)) / 255; b[3] = 255; }
    }
    ops++;
}
int main(int argc, char **argv) {
    FILE *f; char line[700]; int rc = 0;
    if (argc < 5 || strcmp(argv[1], "level")) { fprintf(stderr, "usage: level <level.pdl> <root> <out.png> [<extraroot>]\n"); return 2; }
    snprintf(ROOT, sizeof ROOT, "%s", argv[3]); snprintf(XROOT, sizeof XROOT, "%s", argc > 5 ? argv[5] : argv[3]);
    f = fopen(argv[2], "r"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    while (fgets(line, sizeof line, f) && !rc) {
        char *p = line, *k; Img *s; int a[7], i;
        if (line[0] == '#' || line[0] == '\n') continue;
        k = fld(&p);
        if (!strcmp(k, "LEVEL")) { OW = atoi(fld(&p)) * 48; OH = atoi(fld(&p)) * 48; if (OW < 48 || OH < 48 || OW > 4800 || OH > 4800) { fprintf(stderr, "bad LEVEL size\n"); rc = 2; break; } O = calloc((size_t)OW * OH, 4); if (!O) { rc = 2; break; } }
        else if (!strcmp(k, "FILL") || !strcmp(k, "TILE") || !strcmp(k, "PIC") || !strcmp(k, "CHAR")) {
            char *sh = fld(&p), sheet[300]; snprintf(sheet, sizeof sheet, "%s", sh);
            for (i = 0; i < 7; i++) a[i] = atoi(fld(&p));
            if (!O) { fprintf(stderr, "LEVEL row must come first\n"); rc = 2; break; }
            s = load(sheet); if (!s) { rc = 2; break; }
            if (!strcmp(k, "FILL")) { int x, y; for (y = 0; y < OH / 48; y++) for (x = 0; x < OW / 48; x++) blit(s, a[0], a[1], 48, 48, x * 48, y * 48); }
            else if (!strcmp(k, "TILE")) blit(s, a[0], a[1], 48, 48, a[2] * 48, a[3] * 48);
            else if (!strcmp(k, "PIC")) blit(s, a[0], a[1], a[2], a[3], a[4], a[5]);
            else blit(s, (a[0] % 4) * 144 + a[2] * 48, (a[0] / 4) * 192 + a[1] * 48, 48, 48, a[3] * 48, a[4] * 48);
        }
    }
    fclose(f);
    if (!rc && !O) { fprintf(stderr, "no LEVEL row\n"); rc = 2; }
    if (!rc && !stbi_write_png(argv[4], OW, OH, 4, O, OW * 4)) rc = 2;
    if (!rc) { long op = 0; int i; for (i = 0; i < OW * OH; i++) op += O[i * 4 + 3] != 0; printf("LEVEL %dx%d draws=%d sheets=%d opaque=%ld\n", OW, OH, ops, nc, op); }
    return rc;
}
