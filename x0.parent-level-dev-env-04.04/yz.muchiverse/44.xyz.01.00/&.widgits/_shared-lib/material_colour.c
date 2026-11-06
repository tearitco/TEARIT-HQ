/* material_colour.c - the periodic-element colour (Drude-model flavoured), as a PURE text-include (house shape: pure logic with 2+ consumers is text-included, never header + link).
 * Consumers: &.widgits/palettes/ops/elements_palette_manager.c (the 118-tile element picker) and @.apps/generator-hq/ops/material_colour_op.c (the generator's material colour, D35).
 * Lifted VERBATIM from elements_palette_manager.c on 2026-11-05 (D35 step M0): the 118 colours must not change, and the harness case `material` pins all 118 hex values.
 * Spec: #.ref/menu/palletes/chem-viz.txt section 3. A physics-SHAPED heuristic, not a solid-state solver: plasma frequency from valence electrons per nucleon sets reflectivity;
 * a heavy-element term absorbs blue so Cu and Au come out warm while Fe and Ag stay grey; light nonmetals tint cool. It explains METALS; clay, wood, grass and ice need the pigment table (D35 section 3).
 * Prefix mc_. Needs <math.h> and <stdio.h> from the includer. No globals. */
#ifndef MATERIAL_COLOUR_C
#define MATERIAL_COLOUR_C
/* outer (valence-ish) electrons, crude: main-group by period position, d-block ~ 2, f-block ~ 2. Enough for a Drude density proxy. */
static int mc_outer_electrons(int z) {
    /* period boundaries (cumulative Z at end of each period): 2,10,18,36,54,86,118 */
    static const int endz[] = { 0, 2, 10, 18, 36, 54, 86, 118 };
    int p = 1; while (p < 7 && z > endz[p]) p++;
    int within = z - endz[p - 1];                 /* 1..len */
    if (p == 1) return within;                    /* H,He -> 1,2 */
    if (p == 2 || p == 3) return within;          /* 1..8 */
    /* periods 4/5: 1,2 | 3..12 d-block | 13..18 p-block */
    if (p == 4 || p == 5) {
        if (within <= 2) return within;
        if (within <= 12) return 2;               /* transition metal */
        return within - 10;                       /* 3..8 */
    }
    /* periods 6/7: 1,2 | 3 | 4..17 f-block | 18..27 d-block | 28..32 p-block */
    if (within <= 2) return within;
    if (within <= 17) return 2;                   /* La/Ac + lanthanide/actinide */
    if (within <= 27) return 2;                   /* d-block */
    return within - 25;                           /* 3..7 */
}
static int mc_clampi(double v) { int i = (int)(v + 0.5); return i < 0 ? 0 : i > 255 ? 255 : i; }
/* Element z (1..118) with its proton and neutron counts -> colour as 0..255 per channel; *refl gets the 0..1 reflectivity (the generator uses it as metal-ness). */
static void mc_drude(int z, int prot, int neut, int *rr, int *gg, int *bb, double *refl) {
    double nucleons = prot + neut;
    if (nucleons < 1) nucleons = z > 0 ? z : 1;
    int oe = mc_outer_electrons(z);
    double n_e = (double)oe / nucleons;                 /* electron density proxy */
    double wp  = sqrt(n_e * 4200.0);                    /* plasma frequency, scaled */
    double rf = wp / (wp + 1.0);                        /* 0..1 */
    double base = 70.0 + rf * 150.0;                    /* 70..220 grey */
    double r = base, g = base, b = base;
    if (z >= 26) {                                      /* relativistic blue absorption */
        double s = (z - 25) / 95.0;
        b *= (1.0 - 0.55 * s);
        g *= (1.0 - 0.20 * s);
        r *= (1.0 + 0.06 * s);
    }
    if (z == 29 || z == 79) { r *= 1.15; g *= 0.92; b *= 0.55; }  /* Cu, Au - named in chem-viz */
    if (z <= 18 && oe >= 5) { r = base - 25; g = base;  b = base + 25; }  /* light nonmetals: cool */
    if (z <= 2)             { r = base + 10; g = base + 20; b = base + 30; }
    *rr = mc_clampi(r); *gg = mc_clampi(g); *bb = mc_clampi(b); if (refl) *refl = rf;
}
#endif
