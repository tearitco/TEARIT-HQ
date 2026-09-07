/* elements_palette_manager.c - the <module> behind palettes-elements.xhtpm.
 *
 * The "Chemicals+Compounds" palette is now a PERIODIC-TABLE element
 * picker: 118 element tiles, each filled with a Drude-model colour
 * (#.ref/menu/palletes/chem-viz.txt sec.3), clicked to stamp that
 * element onto the desktop (palettes_menu.sh 'place' <symbol>).
 *
 * Forked by the shared khtpm_core_render.+x as
 *   elements_palette_manager.+x <house_root> <package_dir> [id]
 * publishes <pkg>/state/palettes-elements_ui.txt (consumed via the
 * xhtpm's vars=). Static - no action file to poll.
 *
 * Data: proton/neutron counts come from the house recipe file
 *   #.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt   (lines 8..125
 *   are Hydrogen..Oganesson in Z order). Symbols are the one thing that
 *   file lacks, so SYM[] below carries them (canonical, Z-indexed).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

/* portable platform glue (sleep / monotonic clock / mkdir -p / script
 * spawn / terminate signals) - so this manager carries no #ifdef _WIN32.
 * See _shared-lib/kh_plat.h + the CROSS-PLATFORM-SEAM design doc. */
#define KH_PLAT_IMPL
#include "../../_shared-lib/kh_plat.h"

#define PL 4096
#define NEL 118
#define RECIPE_FILE "#.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt"

static char house_root[PL];
static char pkg_dir[PL];

static const char *SYM[NEL + 1] = { "",
"H","He","Li","Be","B","C","N","O","F","Ne","Na","Mg","Al","Si","P","S","Cl","Ar",
"K","Ca","Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn","Ga","Ge","As","Se","Br","Kr",
"Rb","Sr","Y","Zr","Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn","Sb","Te","I","Xe",
"Cs","Ba","La","Ce","Pr","Nd","Pm","Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb","Lu",
"Hf","Ta","W","Re","Os","Ir","Pt","Au","Hg","Tl","Pb","Bi","Po","At","Rn",
"Fr","Ra","Ac","Th","Pa","U","Np","Pu","Am","Cm","Bk","Cf","Es","Fm","Md","No","Lr",
"Rf","Db","Sg","Bh","Hs","Mt","Ds","Rg","Cn","Nh","Fl","Mc","Lv","Ts","Og" };

static char  g_name[NEL + 1][32];
static int   g_prot[NEL + 1];
static int   g_neut[NEL + 1];

/* outer (valence-ish) electrons, crude: main-group by period position,
 * d-block ~ 2, f-block ~ 2. Enough for a Drude density proxy. */
static int outer_electrons(int z) {
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

static int clampi(double v) { int i = (int)(v + 0.5); return i < 0 ? 0 : i > 255 ? 255 : i; }

/* Drude-model-flavoured element colour (chem-viz.txt sec.3). Not a
 * literal solid-state solver - a physics-shaped heuristic: plasma
 * frequency omega_p ~ sqrt(n_e) sets reflectivity; a relativistic
 * heavy-proton term absorbs blue for the high-Z / coinage metals so
 * Au/Cu come out warm while Fe/Ag stay metallic grey. Tune later. */
static void drude_color(int z, char *out, size_t osz) {
    double nucleons = g_prot[z] + g_neut[z];
    if (nucleons < 1) nucleons = z > 0 ? z : 1;
    int oe = outer_electrons(z);
    double n_e = (double)oe / nucleons;                 /* electron density proxy */
    double wp  = sqrt(n_e * 4200.0);                    /* plasma frequency, scaled */
    double refl = wp / (wp + 1.0);                      /* 0..1 */
    double base = 70.0 + refl * 150.0;                  /* 70..220 grey */
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

    snprintf(out, osz, "#%02x%02x%02x", clampi(r), clampi(g), clampi(b));
}

/* pull Name / protons / neutrons for Z=1..118 from the recipe file */
static void load_recipe_pn(void) {
    char path[PL];
    snprintf(path, sizeof(path), "%s/%s", house_root, RECIPE_FILE);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "elements-palette: cannot open %s\n", path); return; }
    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        if (lineno < 8 || lineno > 125) continue;      /* Hydrogen..Oganesson */
        int z = lineno - 7;
        char nm[32]; int p, n, e, a, b;
        if (sscanf(line, "%31s %d %d %d %d %d", nm, &p, &n, &e, &a, &b) < 4) continue;
        snprintf(g_name[z], sizeof(g_name[z]), "%s", nm);
        g_prot[z] = p; g_neut[z] = n;
    }
    fclose(f);
}

/* period (1..7) and group (1..18, 0 = f-block/lanthanide-actinide) */
static int period_of(int z) {
    static const int endz[] = { 0, 2, 10, 18, 36, 54, 86, 118 };
    int p = 1; while (p < 7 && z > endz[p]) p++;
    return p;
}
static int group_of(int z) {
    static const int endz[] = { 0, 2, 10, 18, 36, 54, 86, 118 };
    int p = period_of(z);
    int within = z - endz[p - 1];                  /* 1.. */
    if (p == 1) return within == 1 ? 1 : 18;       /* H=1, He=18 */
    if (p == 2 || p == 3) return within <= 2 ? within : within + 10;   /* 1,2 then 13..18 */
    if (p == 4 || p == 5) return within;           /* 1..18 straight */
    /* p 6/7: 1,2 | La/Ac (3) | 14 f-block (group 0) | 3..18 */
    if (within <= 3) return within;
    if (within <= 17) return 0;                    /* lanthanide / actinide */
    return within - 14;                            /* 4..18 -> 4..18 */
}

static int   g_sel = 0;              /* selected Z, 0 = none */
static char  g_msg[96] = "";
static char  g_last_sym[8] = "";
static long long g_last_ms = 0;

static long long now_ms(void) { return kh_plat_mono_ms(); }

static int z_of_sym(const char *s) {
    for (int z = 1; z <= NEL; z++) if (strcmp(SYM[z], s) == 0) return z;
    return 0;
}

static void do_place(int z) {
    if (z < 1 || z > NEL) return;
    const char *args[2] = { "place", SYM[z] };
    int rc = kh_plat_run_house_script(house_root,
             "&.widgits/palettes/palettes_menu.sh", args, 2); (void)rc;
    snprintf(g_msg, sizeof(g_msg), "Placed %s (%s) on the desktop.",
             g_name[z][0] ? g_name[z] : SYM[z], SYM[z]);
}

static void write_ui(void) {
    char dir[PL], tmp[PL], dst[PL];
    snprintf(dir, sizeof(dir), "%s/state", pkg_dir);
    kh_plat_mkdir_p(dir);
    snprintf(dst, sizeof(dst), "%s/palettes-elements_ui.txt", dir);
    snprintf(tmp, sizeof(tmp), "%s/palettes-elements_ui.txt.tmp", dir);
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    fprintf(f, "n_tiles=%d\n", NEL);
    fprintf(f, "empty=\n");
    for (int z = 1; z <= NEL; z++) {
        char col[8];
        drude_color(z, col, sizeof(col));
        fprintf(f, "t_%d_sym=%s\n",   z - 1, SYM[z]);
        fprintf(f, "t_%d_z=%d\n",     z - 1, z);
        fprintf(f, "t_%d_name=%s\n",  z - 1, g_name[z][0] ? g_name[z] : SYM[z]);
        fprintf(f, "t_%d_color=%s\n", z - 1, col);
        fprintf(f, "t_%d_cls=%s\n",   z - 1, (z == g_sel) ? "sel" : "");
    }

    /* detail card */
    if (g_sel >= 1 && g_sel <= NEL) {
        int z = g_sel;
        char col[8]; drude_color(z, col, sizeof(col));
        int g = group_of(z);
        fprintf(f, "sel_show=1\nsel_empty=\n");
        fprintf(f, "sel_sym=%s\n",   SYM[z]);
        fprintf(f, "sel_name=%s\n",  g_name[z][0] ? g_name[z] : SYM[z]);
        fprintf(f, "sel_z=%d\n",     z);
        fprintf(f, "sel_pne=%d protons  /  %d neutrons  /  %d electrons\n", g_prot[z], g_neut[z], z);
        fprintf(f, "sel_mass=%d\n",  g_prot[z] + g_neut[z]);
        fprintf(f, "sel_period=%d\n", period_of(z));
        if (g) fprintf(f, "sel_group=%d\n", g);
        else   fprintf(f, "sel_group=f-block\n");
        fprintf(f, "sel_color=%s\n", col);
    } else {
        fprintf(f, "sel_show=\nsel_empty=1\nsel_sym=\nsel_name=\nsel_z=\nsel_pne=\n"
                   "sel_mass=\nsel_period=\nsel_group=\nsel_color=\n");
    }
    fprintf(f, "msg=%s\n", g_msg);

    fclose(f);
    rename(tmp, dst);
}

static void do_click(const char *sym) {
    int z = z_of_sym(sym);
    if (!z) return;
    long long t = now_ms();
    if (strcmp(sym, g_last_sym) == 0 && t - g_last_ms < 500) {
        do_place(z);                 /* double-click */
        g_last_sym[0] = '\0';
    } else {
        g_sel = z;                   /* single click -> inspect */
        g_msg[0] = '\0';
        snprintf(g_last_sym, sizeof(g_last_sym), "%s", sym);
        g_last_ms = t;
    }
}

static void poll_action(int *last_seq) {
    char p[PL];
    snprintf(p, sizeof(p), "%s/state/palettes-elements_action.txt", pkg_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char buf[PL]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f);
    buf[n] = '\0';
    int seq = 0; char cmd[64] = "";
    for (char *ls = buf; *ls; ) {
        char *le = strchr(ls, '\n');
        if (strncmp(ls, "seq=", 4) == 0) seq = atoi(ls + 4);
        else if (strncmp(ls, "cmd=", 4) == 0) {
            size_t cl = (le ? (size_t)(le - ls) : strlen(ls)) - 4;
            if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1;
            memcpy(cmd, ls + 4, cl); cmd[cl] = '\0';
        }
        if (!le) break; ls = le + 1;
    }
    if (seq > *last_seq && cmd[0]) {
        *last_seq = seq;
        if (strncmp(cmd, "CLICK:", 6) == 0) do_click(cmd + 6);
        write_ui();
    }
}

static void bye(int s) { (void)s; _exit(0); }

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <package_dir> [id]\n", argv[0]); return 1; }
    snprintf(house_root, sizeof(house_root), "%s", argv[1]);
    snprintf(pkg_dir,    sizeof(pkg_dir),    "%s", argv[2]);
    kh_plat_on_terminate(bye);

    load_recipe_pn();
    { char ap[PL]; snprintf(ap, sizeof(ap), "%s/state/palettes-elements_action.txt", pkg_dir);
      FILE *a = fopen(ap, "w"); if (a) { fputs("seq=0\ncmd=\n", a); fclose(a); } }
    write_ui();

    int last_seq = 0;
    for (;;) { kh_plat_sleep_ms(60); poll_action(&last_seq); }
    return 0;
}
