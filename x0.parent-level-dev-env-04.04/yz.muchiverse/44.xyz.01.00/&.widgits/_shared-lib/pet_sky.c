/* pet_sky.c - the day phase and the 3D/2D sky files of the pets-like boards (text-included by petslike_core.c and by rpg_edit.c; pure: it needs only stdio/stdlib/string/time).
 *
 * One rule for "which phase is it" and "which RPG Maker parallax is the sky" (night = StarlitSky, dusk and dawn = Sunset, day = CloudySky1), and one writer for the two files the engine reads
 * from maps/<app>/<room>/: parallax.rgba (raw RGBA8, the picture) and parallax.pdl (show, name, width, height, loop_x, stamp; the stamp row tells the 3D renderer to reload and re-upload).
 * The caller decides WHEN to rewrite (the core keys it on phase + screen tone, rpg-edit on the phase) and what the pixels are (the core applies the screen tone first). */
#ifndef PET_SKY_C
#define PET_SKY_C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* hour 0..23 of the game day -> "night" (19 to 6), "dawn" (to 8), "dusk" (from 17), else "day" */
static const char *pet_phase_of_hour(int h) { return (h >= 19 || h < 6) ? "night" : h < 8 ? "dawn" : h >= 17 ? "dusk" : "day"; }
/* the phase's sky picture: parallaxes/<name>.png under the RPG Maker image folder */
static const char *pet_sky_sheet(const char *ph) { return !strcmp(ph, "night") ? "StarlitSky" : (!strcmp(ph, "dusk") || !strcmp(ph, "dawn")) ? "Sunset" : "CloudySky1"; }
/* a picture the 3D sky can use: set and a sane size */
static int pet_sky_ok(const unsigned char *px, int w, int h) { return px && w >= 1 && h >= 1 && w <= 4096 && h <= 4096; }

static int pet_sky_write_file(const char *path, const void *data, size_t n, const char *mode) {
    char tmp[4200]; snprintf(tmp, sizeof tmp, "%s.tmp", path); FILE *f = fopen(tmp, mode); if (!f) return 0;
    size_t wn = fwrite(data, 1, n, f); if (fclose(f) != 0 || wn != n) { remove(tmp); return 0; }
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}
/* dir/parallax.rgba + dir/parallax.pdl for phase `ph` (the picture first, then the pdl that announces it) */
static void pet_write_parallax(const char *dir, const char *ph, const unsigned char *rgba, int w, int h, long stamp) {
    char pp[4096], rp[4096], pd[240]; snprintf(pp, sizeof pp, "%s/parallax.pdl", dir); snprintf(rp, sizeof rp, "%s/parallax.rgba", dir);
    pet_sky_write_file(rp, rgba, (size_t)w * h * 4, "wb");
    int po = snprintf(pd, sizeof pd, "PARALLAX | show | 1\nPARALLAX | name | %s\nPARALLAX | width | %d\nPARALLAX | height | %d\nPARALLAX | loop_x | 1\nPARALLAX | stamp | %ld\n", ph, w, h, stamp);
    pet_sky_write_file(pp, pd, (size_t)po, "w");
}
/* no sky for this room (an interior, or the picture is missing): the flat sky colour */
static void pet_write_parallax_off(const char *dir) {
    static const char off[] = "PARALLAX | show | 0\nPARALLAX | name | \n"; char pp[4096]; snprintf(pp, sizeof pp, "%s/parallax.pdl", dir);
    pet_sky_write_file(pp, off, sizeof off - 1, "w");
}
#endif
