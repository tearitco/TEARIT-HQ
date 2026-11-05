/* nb_css_props_test.c - G2.6 gate: the recognized-property list is
 * complete and exact. Links nb_css.c directly (it has zero JS/engine deps
 * by design) and asserts, per element, the full NbCssStyle outcome for
 * every property the page projection consumes.
 *
 * WHY THIS EXISTS (2026-11-04)
 * ----------------------------
 * G2.6's contract: the set G2.1-G2.5 are built on is resolved here, in
 * nb_css.c, and NOTHING in that set may silently drop. Before this file,
 * the only proof the needed set parsed correctly was "the layout suites
 * happen to pass" - a dropped property would surface as a weird geometry,
 * not as a named failure. This file asserts every row of the list by
 * name, so a regression prints the property and the mismatch.
 *
 * Also locked here, deliberately: the NOT-recognized set parses without
 * error and affects NOTHING - "silently drop" means exactly that, never
 * "silently half-apply".
 *
 * Each check prints PASS/FAIL; exit 1 iff any FAIL.
 */
#define _GNU_SOURCE
#include "nb_css.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, fails;

static void ok(int cond, const char *what) {
    checks++;
    if (cond) { printf("PASS: %s\n", what); return; }
    fails++;
    printf("FAIL: %s\n", what);
}

static NbNode mk(const char *tag, const char *id, const char *cls) {
    NbNode n;
    memset(&n, 0, sizeof n);
    n.tag = strdup(tag);
    n.id  = id ? strdup(id) : NULL;
    n.cls = cls ? strdup(cls) : NULL;
    n.attrs = NULL;
    return n;
}

static void free_node(NbNode *n) {
    if (!n) return;
    free(n->tag); free(n->id); free(n->cls); free(n->attrs); free(n->text);
    memset(n, 0, sizeof *n);
}

static const char SHEET[] =
    ".d-none { display: none; }\n"
    ".d-flex { display: flex; }\n"
    ".d-grid { display: grid; }\n"
    ".d-block { display: block; }\n"
    ".w1 { width: 120px; height: 34px; }\n"
    ".w2 { width: 75; height: 22; }\n"
    ".w3 { width: 50%; height: 40%; }\n"          /* %: ignored, never applied */
    ".pad { padding: 10px; }\n"                   /* first component only */
    ".pad2 { padding-top: 12px; padding-bottom: 7px; }\n"
    ".mar { margin: 9px; }\n"                     /* first component only */
    ".mar2 { margin-top: 13px; margin-bottom: 5px; }\n"
    ".pos-fixed { position: fixed; top: -8px; left: 90px; z-index: 30; }\n"
    ".pos-abs { position: absolute; top: 40px; left: 12px; }\n"
    ".rel { position: relative; }\n"
    ".fdir { flex-direction: column; }\n"
    ".grow { flex-grow: 3; }\n"
    ".grid-rep { grid-template-columns: repeat(3, 1fr); }\n"
    ".grid-px { grid-template-columns: 50px 100px 2fr; }\n"
    ".grid-cap { grid-template-columns: repeat(12, 1fr); }\n"
    ".vis { visibility: hidden; }\n"
    ".op { opacity: 0.5; }\n"
    ".fam { font-family: TestHouse; }\n"
    ".clr { color: red; }\n"                      /* not recognized: dropped */
    ".gap { gap: 12px; }\n"                       /* not recognized: dropped */
    ".fw { font-weight: 700; }\n"                  /* @font-face-only: dropped as a decl */
    ".dropped { display: grid; gap: 99px; color: blue; width: 10px; }\n" /* mixed: keep the recognized, drop the rest */
    "@font-face { font-family: \"TestHouse\"; src: url(https://x.example/y.woff2); font-weight: 700; font-style: italic; }\n"
    ".mw { display: block; }\n"
    "@media (max-width: 767px) { .mw { display: none; } }\n"
    "@media (min-width: 768px) { .mw { display: block; } }\n";

static NbCss *S;   /* sheet parsed with no viewport (legacy semantics) */
static NbCss *SV700, *SV800;

/* Resolve via a fresh element with the wanted class. */
static void res(NbCss *css, const char *tag, const char *id, const char *cls,
                const char *inline_style, NbCssStyle *out) {
    NbNode el = mk(tag, id, cls);
    memset(out, 0, sizeof *out);
    nb_css_resolve(css, &el, inline_style, out);
    free_node(&el);
}

#define C(cond, what) ok((cond), what)

int main(void) {
    S    = nb_css_parse(SHEET, sizeof(SHEET) - 1);
    SV700 = nb_css_parse_vp(SHEET, sizeof(SHEET) - 1, 700);
    SV800 = nb_css_parse_vp(SHEET, sizeof(SHEET) - 1, 800);
    C(S && SV700 && SV800, "sheet parses (legacy + both viewports)");

    /* ---- display: the four keywords, typed flex/grid flag ---- */
    { NbCssStyle st; res(S, "div", NULL, "d-none", NULL, &st);
      C(!strcmp(st.display, "none"), "display:none computes to none"); 
      C(st.has_display_flex == 0, "display:none carries no flex flag"); }
    { NbCssStyle st; res(S, "div", NULL, "d-flex", NULL, &st);
      C(!strcmp(st.display, "flex"), "display:flex computes to flex");
      C(st.has_display_flex == 1 && st.display_flex == 1, "display:flex => display_flex=1"); }
    { NbCssStyle st; res(S, "div", NULL, "d-grid", NULL, &st);
      C(!strcmp(st.display, "grid"), "display:grid computes to grid");
      C(st.has_display_flex == 1 && st.display_flex == 2, "display:grid => display_flex=2"); }
    { NbCssStyle st; res(S, "div", NULL, "d-block", NULL, &st);
      C(!strcmp(st.display, "block"), "display:block computes to block");
      C(st.has_display_flex == 0, "display:block carries no flex flag"); }

    /* ---- width/height, px and bare numbers; % is ignored ---- */
    { NbCssStyle st; res(S, "div", NULL, "w1", NULL, &st);
      C(st.has_width && st.width == 120 && st.has_height && st.height == 34, "width/height: px"); }
    { NbCssStyle st; res(S, "div", NULL, "w2", NULL, &st);
      C(st.has_width && st.width == 75 && st.has_height && st.height == 22, "width/height: bare numbers"); }
    { NbCssStyle st; res(S, "div", NULL, "w3", NULL, &st);
      C(!st.has_width && !st.has_height, "width/height: % values are ignored"); }

    /* ---- box model: first-component-only shorthand, vertical only ---- */
    { NbCssStyle st; res(S, "div", NULL, "pad", NULL, &st);
      C(st.has_pad_top && st.pad_top == 10 && !st.has_pad_bottom,
        "padding shorthand sets top only (documented first-component rule)"); }
    { NbCssStyle st; res(S, "div", NULL, "pad2", NULL, &st);
      C(st.has_pad_top && st.pad_top == 12 && st.has_pad_bottom && st.pad_bottom == 7,
        "padding-top/padding-bottom longhands"); }
    { NbCssStyle st; res(S, "div", NULL, "mar", NULL, &st);
      C(st.has_mar_top && st.mar_top == 9 && !st.has_mar_bottom,
        "margin shorthand sets top only (documented first-component rule)"); }
    { NbCssStyle st; res(S, "div", NULL, "mar2", NULL, &st);
      C(st.has_mar_top && st.mar_top == 13 && st.has_mar_bottom && st.mar_bottom == 5,
        "margin-top/margin-bottom longhands"); }

    /* ---- position family ---- */
    { NbCssStyle st; res(S, "div", NULL, "pos-fixed", NULL, &st);
      C(st.has_position && st.position_absolute && st.position_fixed,
        "position:fixed => absolute+fixed");
      C(st.has_top && st.top == -8 && st.has_left && st.left == 90,
        "top/left signed px");
      C(st.has_z_index && st.z_index == 30, "z-index parsed (mirrored from renderer, currently unread)"); }
    { NbCssStyle st; res(S, "div", NULL, "pos-abs", NULL, &st);
      C(st.has_position && st.position_absolute && !st.position_fixed,
        "position:absolute => absolute, not fixed");
      C(st.has_top && st.top == 40 && st.has_left && st.left == 12,
        "position:absolute top/left"); }
    { NbCssStyle st; res(S, "div", NULL, "rel", NULL, &st);
      C(st.has_position && !st.position_absolute && !st.position_fixed,
        "position:relative => context, not absolute"); }

    /* ---- flex ---- */
    { NbCssStyle st; res(S, "div", NULL, "fdir", NULL, &st);
      C(st.has_flex_direction && st.flex_row == 0, "flex-direction:column => flex_row=0"); }
    { NbCssStyle st; res(S, "div", NULL, "grow", NULL, &st);
      C(st.has_flex_grow && st.flex_grow == 3, "flex-grow:3"); }

    /* ---- grid ---- */
    { NbCssStyle st; res(S, "div", NULL, "grid-rep", NULL, &st);
      C(st.has_grid_template_columns && st.grid_cols == 3, "grid repeat(3,1fr) => 3 cols");
      C(st.grid_col_w[0] == 0 && st.grid_col_w[1] == 0 && st.grid_col_w[2] == 0, "fr tracks carry 0 width"); }
    { NbCssStyle st; res(S, "div", NULL, "grid-px", NULL, &st);
      C(st.has_grid_template_columns && st.grid_cols == 3, "grid 3-track list => 3 cols");
      C(st.grid_col_w[0] == 50 && st.grid_col_w[1] == 100 && st.grid_col_w[2] == 0,
        "px tracks and fr track widths"); }
    { NbCssStyle st; res(S, "div", NULL, "grid-cap", NULL, &st);
      C(st.has_grid_template_columns && st.grid_cols == 8, "repeat(12,1fr) clamped to 8 cols"); }

    /* ---- visibility / opacity ---- */
    { NbCssStyle st; res(S, "div", NULL, "vis", NULL, &st);
      C(!strcmp(st.visibility, "hidden"), "visibility:hidden"); }
    { NbCssStyle st; res(S, "div", NULL, "op", NULL, &st);
      C(!strcmp(st.opacity, "0.5"), "opacity:0.5 stored normalized"); }

    /* ---- font-family (getComputedStyle surface) ---- */
    { NbCssStyle st; res(S, "div", NULL, "fam", NULL, &st);
      C(!strcmp(st.font_family, "TestHouse"), "font-family resolves"); }

    /* ---- the NOT-recognized set: dropped, never half-applied ---- */
    { NbCssStyle st; res(S, "div", NULL, "clr", NULL, &st);
      C(st.width == 0 && !st.has_width && st.display[0] == 0, "color: red drops silently"); }
    { NbCssStyle st; res(S, "div", NULL, "gap", NULL, &st);
      C(st.display_flex == 0 && st.width == 0 && st.grid_cols == 0, "gap: 12px drops silently"); }
    { NbCssStyle st; res(S, "div", NULL, "fw", NULL, &st);
      C(st.display[0] == 0 && st.width == 0, "font-weight as a declaration drops silently (@font-face-only)"); }
    { NbCssStyle st; res(S, "div", NULL, "dropped", NULL, &st);
      C(!strcmp(st.display, "grid") && st.width == 10 && st.grid_cols == 0,
        "mixed sheet keeps recognized props, drops the rest"); }

    /* ---- inline style beats the sheet ---- */
    { NbCssStyle st; res(S, "div", NULL, "d-flex", "display: none", &st);
      C(!strcmp(st.display, "none") && st.display_flex == 0, "inline style beats stylesheet"); }

    /* ---- @font-face parsed, capped, fields exact ---- */
    C(S->font_face_count == 1, "@font-face parsed once");
    if (S->font_face_count >= 1) {
        C(strncmp(S->font_faces[0].font_family, "TestHouse", 64) == 0, "@font-face family");
        C(S->font_faces[0].font_weight == 700, "@font-face font-weight");
        C(S->font_faces[0].font_style == 1, "@font-face font-style italic");
        C(strstr(S->font_faces[0].src, "y.woff2") != NULL, "@font-face src");
    }

    /* ---- @media: both sides, and legacy mode skips entirely ---- */
    { NbCssStyle st; res(SV700, "div", NULL, "mw", NULL, &st);
      C(!strcmp(st.display, "none"), "viewport 700 => max-width branch applies"); }
    { NbCssStyle st; res(SV800, "div", NULL, "mw", NULL, &st);
      C(!strcmp(st.display, "block"), "viewport 800 => min-width branch applies"); }
    { NbCssStyle st; res(S, "div", NULL, "mw", NULL, &st);
      C(!strcmp(st.display, "block"), "legacy parse (no viewport) skips @media entirely"); }

    nb_css_free(S); nb_css_free(SV700); nb_css_free(SV800);

    printf("--\nnb_css_props_test: %d checks, %d failures\n", checks, fails);
    if (fails) { printf("nb_css_props_test.c FAIL\n"); return 1; }
    printf("nb_css_props_test.c PASS\n");
    return 0;
}
