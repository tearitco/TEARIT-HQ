/* pet_sky_probe_op - harness probe for the day-night plug functions in &.widgits/_shared-lib/pet_sky.c (decision D75). It #includes the real pet_sky.c, so the case tests the shipped code.
 *   pet_sky_probe_op show   <plug|-> <hour>         one line: hour=H phase=P sky=S out=r,g,b in=r,g,b   (plug `-` = no plug file: the built-in defaults)
 *   pet_sky_probe_op parity <plug|->                compare all 24 hours against the LEGACY rule below (copied verbatim from the code before 2026-11-14): prints PARITY|ok|24 or PARITY|DIFF|<hour>|<what>
 *   pet_sky_probe_op phases <plug|->                one line per declared phase: PHASE|name|from  (nothing when the plug is OFF)
 * Build: gcc -std=gnu11 -O2 -Wall -o pet_sky_probe_op pet_sky_probe_op.c   Exit 0 ok, 1 a parity difference, 2 usage. */
#include "../../pet_sky.c"

/* LEGACY (before D75): the hard-coded hour rule and tint multipliers, kept here only as the reference for the parity check */
static const char *legacy_phase(int h) { return (h >= 19 || h < 6) ? "night" : h < 8 ? "dawn" : h >= 17 ? "dusk" : "day"; }
static const char *legacy_sky(const char *ph) { return !strcmp(ph, "night") ? "StarlitSky" : (!strcmp(ph, "dusk") || !strcmp(ph, "dawn")) ? "Sunset" : "CloudySky1"; }
static void legacy_tint(const char *phase, int od, int *r, int *g, int *b) {
    int mr = 256, mg = 256, mb = 256;
    if (!strcmp(phase, "night")) { if (od) { mr = 110; mg = 120; mb = 190; } else { mr = 200; mg = 205; mb = 235; } } else if (!strcmp(phase, "dusk")) { if (od) { mr = 256; mg = 200; mb = 160; } else { mr = 256; mg = 235; mb = 215; } } else if (!strcmp(phase, "dawn")) { if (od) { mr = 256; mg = 215; mb = 185; } else { mr = 256; mg = 240; mb = 225; } }
    *r = mr; *g = mg; *b = mb;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: pet_sky_probe_op show|parity|phases <plug|-> [hour]\n"); return 2; }
    snprintf(PET_PLUG_PATH, sizeof PET_PLUG_PATH, "%s", strcmp(argv[2], "-") ? argv[2] : "");
    if (!strcmp(argv[1], "show") && argc >= 4) {
        int h = atoi(argv[3]); const char *p = pet_phase_of_hour(h); int a, b, c, d, e, f; pet_phase_tint(p, 1, &a, &b, &c); pet_phase_tint(p, 0, &d, &e, &f);
        printf("hour=%d phase=%s sky=%s out=%d,%d,%d in=%d,%d,%d\n", h, p, pet_sky_sheet(p), a, b, c, d, e, f); return 0; }
    if (!strcmp(argv[1], "phases")) { const char *n; int fr; for (int i = 0; pet_plug_phase(i, &n, &fr); i++) printf("PHASE|%s|%d\n", n, fr); return 0; }
    if (!strcmp(argv[1], "parity")) {
        for (int h = 0; h < 24; h++) {
            const char *p = pet_phase_of_hour(h), *q = legacy_phase(h);
            if (strcmp(p, q)) { printf("PARITY|DIFF|%d|phase %s vs legacy %s\n", h, p, q); return 1; }
            if (strcmp(pet_sky_sheet(p), legacy_sky(q))) { printf("PARITY|DIFF|%d|sky\n", h); return 1; }
            for (int od = 0; od <= 1; od++) { int a, b, c, x, y, z; pet_phase_tint(p, od, &a, &b, &c); legacy_tint(q, od, &x, &y, &z); if (a != x || b != y || c != z) { printf("PARITY|DIFF|%d|tint od=%d %d,%d,%d vs %d,%d,%d\n", h, od, a, b, c, x, y, z); return 1; } } }
        printf("PARITY|ok|24\n"); return 0; }
    fprintf(stderr, "usage: pet_sky_probe_op show|parity|phases <plug|-> [hour]\n"); return 2;
}
