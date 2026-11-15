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

/* THE DAY/NIGHT PLUG (decision D74/D75, 2026-11-14). A game does not need a day and a night, so the cycle is a PLUG: one visible file <app>/plugs/day-night.plug.pdl holds the switch
 * (ENABLED | 1/0) and the parameters (PHASE | name | from_hour | outdoor r,g,b | indoor r,g,b | sky). The core sets PET_PLUG_PATH to that file; when it is empty or the file is missing, the
 * defaults below (what was hard-coded before) apply, so a book without the plug behaves exactly as it did. ENABLED | 0 means: phase `day` always, no tint, the day sky.
 * The file is re-read when its size or mtime moves (the same rule as tuning.pdl), at most once per call to pet_plug_load(). */
#include <sys/stat.h>
static char PET_PLUG_PATH[4096] = "";
typedef struct { char name[16]; int from; int out[3], in[3]; char sky[40]; } PetPhaseRow;
static const PetPhaseRow PET_PHASE_DEFAULT[4] = {
    { "dawn", 6, { 256, 215, 185 }, { 256, 240, 225 }, "Sunset" }, { "day", 8, { 256, 256, 256 }, { 256, 256, 256 }, "CloudySky1" },
    { "dusk", 17, { 256, 200, 160 }, { 256, 235, 215 }, "Sunset" }, { "night", 19, { 110, 120, 190 }, { 200, 205, 235 }, "StarlitSky" } };
static PetPhaseRow g_pp[8]; static int g_ppn = 4, g_pp_on = 1; static long g_pp_sig = -2;
static int pet_pp_triple(const char *s, int *o) { return sscanf(s, " %d , %d , %d", &o[0], &o[1], &o[2]) == 3 && o[0] >= 0 && o[1] >= 0 && o[2] >= 0 && o[0] <= 512 && o[1] <= 512 && o[2] <= 512; }
static void pet_plug_load(void) {
    struct stat st; long sig = (PET_PLUG_PATH[0] && stat(PET_PLUG_PATH, &st) == 0) ? (long)st.st_size * 1000003L + (long)st.st_mtime : -1;
    if (sig == g_pp_sig) { return; } g_pp_sig = sig;
    memcpy(g_pp, PET_PHASE_DEFAULT, sizeof PET_PHASE_DEFAULT); g_ppn = 4; g_pp_on = 1;
    if (sig < 0) return;
    FILE *f = fopen(PET_PLUG_PATH, "r"); if (!f) return; char ln[300]; PetPhaseRow tmp[8]; int n = 0;
    while (fgets(ln, sizeof ln, f)) {
        char *p = ln; while (*p == ' ') p++;
        if (!strncmp(p, "ENABLED", 7)) { char *bar = strchr(p, '|'); if (bar) g_pp_on = atoi(bar + 1) == 1; continue; }
        if (strncmp(p, "PHASE", 5) || n >= 8) continue;
        char *fl[6]; int nf = 0; for (char *t = strtok(p, "|"); t && nf < 6; t = strtok(NULL, "|")) { while (*t == ' ') t++; char *e = t + strlen(t); while (e > t && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; fl[nf++] = t; }
        if (nf < 6) { continue; } PetPhaseRow r; memset(&r, 0, sizeof r); snprintf(r.name, sizeof r.name, "%s", fl[1]); r.from = atoi(fl[2]);
        if (!r.name[0] || r.from < 0 || r.from > 23 || !pet_pp_triple(fl[3], r.out) || !pet_pp_triple(fl[4], r.in) || !fl[5][0] || strchr(fl[5], '/')) continue;
        snprintf(r.sky, sizeof r.sky, "%s", fl[5]); tmp[n++] = r; }
    fclose(f);
    if (n > 0) { memcpy(g_pp, tmp, sizeof(PetPhaseRow) * (size_t)n); g_ppn = n; }      /* a plug file with no valid PHASE row keeps the defaults; its ENABLED row still counts */
}
static int pet_plug_enabled(void) { pet_plug_load(); return g_pp_on; }
/* hour 0..23 of the game day -> the phase whose from_hour is the latest one at or before it (wrapping past midnight). Defaults: night 19 to 6, dawn to 8, day to 17, dusk to 19 */
static const char *pet_phase_of_hour(int h) {
    pet_plug_load(); if (!g_pp_on) return "day";
    int best = -1, top = 0; for (int i = 0; i < g_ppn; i++) { if (g_pp[i].from > g_pp[top].from) top = i; if (g_pp[i].from <= h && (best < 0 || g_pp[i].from > g_pp[best].from)) best = i; }
    return g_pp[best < 0 ? top : best].name; }
/* the phase's sky picture: parallaxes/<name>.png under the RPG Maker image folder */
static const char *pet_sky_sheet(const char *ph) {
    pet_plug_load(); if (!g_pp_on) return "CloudySky1";
    for (int i = 0; i < g_ppn; i++) if (!strcmp(g_pp[i].name, ph)) return g_pp[i].sky;
    return "CloudySky1"; }
/* channel multipliers (out of 256) for a phase, outdoors or indoors; 256 everywhere when the plug is off or the phase is unknown */
static void pet_phase_tint(const char *ph, int outdoor, int *r, int *g, int *b) {
    *r = *g = *b = 256; pet_plug_load(); if (!g_pp_on) return;
    for (int i = 0; i < g_ppn; i++) if (!strcmp(g_pp[i].name, ph)) { const int *c = outdoor ? g_pp[i].out : g_pp[i].in; *r = c[0]; *g = c[1]; *b = c[2]; return; } }
/* the phases the plug declares (name and from_hour), for the clock installer; returns the count (0 when the plug is off) */
static int pet_plug_phase(int i, const char **name, int *from) { pet_plug_load(); if (!g_pp_on || i < 0 || i >= g_ppn) return 0; *name = g_pp[i].name; *from = g_pp[i].from; return 1; }
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
