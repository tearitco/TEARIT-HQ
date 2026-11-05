/* xelector_look.c - ONE place that decides how the xelector (the green selector box) looks: bolder while it moves FREE, lighter and thinner while it CONTROLS an entity (owner 2026-11-04).
 * PURE text-include (house rule: pure code shared by 2+ consumers = text-include; no header, no link): used by board-viewer/ops/bv_render_2d.c and bv_render_3d.c. No file access, no globals: each renderer reads its own view.pdl and its own
 * xelector state, then asks `xel_look_pick`. The looks are DATA (rows of the board's view.pdl); the defaults below apply when a row is absent or malformed:
 *   VIEW | xelector_free_rgb | 40,255,80      VIEW | xelector_free_px | 6      the xelector with NO possessed entity (what it always looked like)
 *   VIEW | xelector_ctl_rgb  | 150,255,170    VIEW | xelector_ctl_px  | 3      the xelector while state.txt `possessed_id` names an entity
 * `px` is the border thickness in pixels (2D). The 3D wire box has no thickness knob: it uses the colour only. Spec: BOOK 18.pc-hq/VIEWS-LAYOUT-STUDIO-AND-XELECTOR-LOOK-2026-11-04.md part 4. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned char r, g, b; int px; } XelLook;
/* "r,g,b" with 0..255 each; returns 1 and fills out on success, 0 (out untouched) on anything else */
static int xel_look_rgb(const char *s, unsigned char out[3]) {
    int a, b, c; char junk; if (!s || !s[0] || sscanf(s, " %d , %d , %d %c", &a, &b, &c, &junk) != 3) return 0;
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255) return 0;
    out[0] = (unsigned char)a; out[1] = (unsigned char)b; out[2] = (unsigned char)c; return 1; }
/* 1..12 pixels; returns the default for empty, non-numeric or out-of-range text */
static int xel_look_px(const char *s, int def) { if (!s || !s[0]) return def; char *e; long v = strtol(s, &e, 10); if (*e || v < 1 || v > 12) return def; return (int)v; }
/* is this possessed_id a real entity? ("", "-" and "." mean none; the 2D page uses "." for none) */
static int xel_look_controlling(const char *possessed_id) { return possessed_id && possessed_id[0] && strcmp(possessed_id, "-") != 0 && strcmp(possessed_id, ".") != 0; }
static XelLook xel_look_pick(int controlling, const char *free_rgb, const char *free_px, const char *ctl_rgb, const char *ctl_px) {
    XelLook l = controlling ? (XelLook){ 150, 255, 170, 3 } : (XelLook){ 40, 255, 80, 6 }; unsigned char c[3];
    if (controlling) { if (xel_look_rgb(ctl_rgb, c)) { l.r = c[0]; l.g = c[1]; l.b = c[2]; } l.px = xel_look_px(ctl_px, l.px); }
    else { if (xel_look_rgb(free_rgb, c)) { l.r = c[0]; l.g = c[1]; l.b = c[2]; } l.px = xel_look_px(free_px, l.px); }
    return l; }
