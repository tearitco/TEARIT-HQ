/* TRANSITIONAL: cross-binary text include; planned replacement = one published
 * ui_scale/reference value (file now, in-mem DB later). Do not add further
 * cross-binary #includes of .c/.h for this; shell writers use a small script/op.
 *
 * khtpm_ui_scale.c - the pure screen-scale math, text-included (no linking, no
 * globals, only <stdio.h>/<stdlib.h>/<string.h>) by khtpm_ui_common.c (HQ engine
 * + desktop pals) and by the tiny placer ops that turn a screen click into a
 * saved entity position. One canonical copy so every process agrees on the
 * formula (2026-09-20, BUG-LOG "UI does not scale to the monitor").
 *
 * REFERENCE SPACE. `desktop_pos.txt` x/y are stored in REFERENCE px: the
 * screen the layouts were tuned on (hq_ui.pdl ui_ref_width/ui_ref_height,
 * default 2496x1664 - the user's main machine, where auto = 100 and every
 * function below is the identity, so nothing changes there). A process that
 * puts a window on screen converts ref -> screen; a process that records a
 * screen click/drag converts screen -> ref. Constants like GRID_X*80 written
 * by launchers, offsets like "+80" and the event movers (mr_move_to_entity,
 * mr_transfer_desk) are already reference-space and need no conversion.
 *
 * The mapping uses the GRID CELL, not a free per-axis ratio, so grid alignment
 * survives rounding: screen = ref * scaled_cell / base_cell where
 * base_cell = desk_grid.pdl cell_px (default 80) and scaled_cell =
 * round(base_cell * auto / 100) (the same number kh_auto_px() gives every
 * entity's GRID_CELL_PX). A ref position that is k*base_cell therefore lands
 * on exactly k*scaled_cell, which is where a dragged entity snaps to on that
 * screen - exact-equality touch checks between entities keep working.
 *
 * auto = min(screen_w/ref_w, screen_h/ref_h) * 100, clamped 50..300, or the
 * ui_scale override (percent) when > 0. */
#ifndef KHTPM_UI_SCALE_C
#define KHTPM_UI_SCALE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define KPS_UNUSED __attribute__((unused))
#else
#define KPS_UNUSED
#endif

#define KPS_DEFAULT_REF_W 2496
#define KPS_DEFAULT_REF_H 1664
#define KPS_DEFAULT_CELL 80

static KPS_UNUSED int kps_auto_pct(int override_pct, int screen_w, int screen_h, int ref_w, int ref_h) {
    int a = 100;
    if (override_pct > 0) a = override_pct;
    else if (screen_w > 0 && screen_h > 0 && ref_w > 0 && ref_h > 0) {
        int rw = (int)((long)screen_w * 100 / ref_w);
        int rh = (int)((long)screen_h * 100 / ref_h);
        a = rw < rh ? rw : rh;
    }
    if (a < 50) a = 50;
    if (a > 300) a = 300;
    return a;
}

/* base * auto / 100, min 1 - identical to kh_auto_px(). */
static KPS_UNUSED int kps_scaled_cell(int base, int auto_pct) {
    if (auto_pct == 100) return base;
    int v = (base * auto_pct + 50) / 100;
    return (base > 0 && v < 1) ? 1 : v;
}

/* round-to-nearest n/d for d > 0, symmetric about zero */
static KPS_UNUSED int kps_div_round(long n, long d) {
    if (d <= 0) return (int)n;
    return (int)(n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d));
}

static KPS_UNUSED int kps_ref_to_screen(int ref, int base_cell, int auto_pct) {
    if (auto_pct == 100 || base_cell <= 0) return ref;
    return kps_div_round((long)ref * kps_scaled_cell(base_cell, auto_pct), base_cell);
}

static KPS_UNUSED int kps_screen_to_ref(int scr, int base_cell, int auto_pct) {
    if (auto_pct == 100 || base_cell <= 0) return scr;
    return kps_div_round((long)scr * base_cell, kps_scaled_cell(base_cell, auto_pct));
}

/* For tools that don't run the full UI loaders: read `ui_scale`/`ui_ref_*`
 * from <desktop_dir>/hq_ui.pdl and `GRID | cell_px | N` from
 * <desktop_dir>/desk_grid.pdl (desktop_dir = the house's "#.desktop" dir),
 * then fill the auto percent for this screen and the base grid cell. */
static KPS_UNUSED void kps_load_for_tool(const char *desktop_dir, int screen_w, int screen_h,
                              int *auto_pct_out, int *base_cell_out) {
    int override_pct = 0, rw = KPS_DEFAULT_REF_W, rh = KPS_DEFAULT_REF_H, cell = KPS_DEFAULT_CELL;
    char path[4352], line[512];
    snprintf(path, sizeof(path), "%s/hq_ui.pdl", desktop_dir);
    FILE *f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (!eq || line[0] == '#') continue;
            *eq = '\0';
            char *val = eq + 1;
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            if (strcmp(line, "ui_scale") == 0) {
                int p = (int)(atof(val) * 100.0 + 0.5);
                override_pct = p > 0 ? p : 0;
            } else if (strcmp(line, "ui_ref_width") == 0 && atoi(val) > 0) rw = atoi(val);
            else if (strcmp(line, "ui_ref_height") == 0 && atoi(val) > 0) rh = atoi(val);
        }
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s/desk_grid.pdl", desktop_dir);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "GRID", 4) != 0) continue;
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char *le = end;
            while (le > p && le[-1] == ' ') le--;
            if ((size_t)(le - p) != 7 || strncmp(p, "cell_px", 7) != 0) continue;
            int v = atoi(end + 1);
            if (v > 0) cell = v;
            break;
        }
        fclose(f);
    }
    if (auto_pct_out) *auto_pct_out = kps_auto_pct(override_pct, screen_w, screen_h, rw, rh);
    if (base_cell_out) *base_cell_out = cell;
}

#endif /* KHTPM_UI_SCALE_C */
