/* pet_footer.c - the footer picture of a pets-like board (text-included by petslike_core.c and by rpg_edit.c; pure: stdio/stdlib/string/sys-stat only).
 *
 * The board footer (pchq_board_projector, append_entity_sprites) draws <pchq>/pieces/<event id>/sprite.csv for every map event; an event's id is its line number (1-based) in the shown room's events.txt.
 * The footer re-reads and re-parses that file on every redraw, so it is kept small: 32 x 32 = 1024 rows (a 64 x 64 one cost the window 4x the CPU). The CALLER makes the 32 x 32 RGBA block (the core samples a
 * character sheet cell, rpg-edit box-filters its art); this file owns the format, the folder and the write. Rows are at most 16 bytes ("255,255,255,255\n"), the buffer is sized for that. */
#ifndef PET_FOOTER_C
#define PET_FOOTER_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define PET_FOOT 32

/* rgba: PET_FOOT x PET_FOOT x 4 bytes. Returns 1 when the file changed on disk, 0 when it already held this picture (or could not be written). */
static int pet_write_footer(const char *pchq, int event_id, const unsigned char *rgba) {
    char d[4096], f[4200]; snprintf(d, sizeof d, "%s/pieces/%d", pchq, event_id); mkdir(d, 0755); snprintf(f, sizeof f, "%s/sprite.csv", d);
    size_t cap = (size_t)PET_FOOT * PET_FOOT * 16 + 256; char *out = (char *)malloc(cap); if (!out) return 0;
    size_t oo = (size_t)snprintf(out, 256, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", PET_FOOT);
    for (int i = 0; i < PET_FOOT * PET_FOOT; i++) { const unsigned char *q = rgba + (size_t)i * 4; oo += (size_t)snprintf(out + oo, cap - oo, "%d,%d,%d,%d\n", q[0], q[1], q[2], q[3]); }
    int changed = 1;
    { FILE *o = fopen(f, "rb"); if (o) { char *old = (char *)malloc(oo + 1); if (old) { size_t n = fread(old, 1, oo + 1, o); if (n == oo && !memcmp(old, out, oo)) changed = 0; free(old); } fclose(o); } }
    if (changed) { char t[4300]; snprintf(t, sizeof t, "%s.tmp", f); FILE *o = fopen(t, "wb"); if (!o) { free(out); return 0; } size_t wn = fwrite(out, 1, oo, o); if (fclose(o) != 0 || wn != oo || rename(t, f) != 0) { remove(t); changed = 0; } }
    free(out); return changed;
}
#endif
