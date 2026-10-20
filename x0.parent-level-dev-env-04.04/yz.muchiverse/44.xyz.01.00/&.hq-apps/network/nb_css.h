#ifndef NB_CSS_H
#define NB_CSS_H

#include "nb_dom.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* nb_css.c — rung 7 slice 1: a browser-flavoured *subset* of CSS for the
 * NB-JS worker. Parses stylesheet/`<style>` text into a flat rule list and
 * resolves a tiny computed-style surface per element:
 *   display, visibility, opacity, width, height (px/number only)
 * plus the layout-intent queries that hide/measure real widgets:
 *   display:none / visibility:hidden (self or any ancestor) => hidden.
 *
 * Deliberately NOT a full CSS engine. Selector matching covers element
 * (#id), explicit class (.cls), tag name, `*`, descendant combinator
 * (space) and comma lists; attribute selectors and pseudo-classes are
 * tolerated (stripped) rather than evaluated. @media/@supports/@keyframes
 * blocks and @import/@charset (head-only) are skipped wholesale (the
 * engine has no viewport, so a media query can never fire). Cascade:
 * source order with the standard id/class/type specificity triple —
 * matching declarations override only when the new rule is more specific
 * or appears later; !important bumps a rule above non-important ones.
 * Inline `style="..."` (passed separately) always wins. There is no
 * inheritance of any property. Units: px and bare numbers; %/em/rem are
 * ignored. Sizes resolve to 0 when absent. This module has zero JS/engine
 * deps so the headless suite can link it directly.
 */

typedef struct NbCssRule {
    char *selector;         /* raw selector text (may be a comma list) */
    char *decls;            /* raw declaration block text (may be empty) */
    int imp;                /* 1 if any declaration carries !important */
    int n_id, n_cls, n_type;/* specificity triple (summed over the list) */
} NbCssRule;

typedef struct NbCss {
    NbCssRule *r;
    size_t nr, cap;
} NbCss;

/* Resolved computed style for one element (the properties we model). */
typedef struct NbCssStyle {
    char display[16];       /* "none" if display:none, else "" (default shown) */
    char visibility[16];    /* "hidden" if visibility:hidden, else "" */
    char opacity[16];       /* normalized string (e.g. "0.5"), else "" (=1) */
    double width, height;   /* CSS px when declared, else 0 */
    int has_width, has_height;
    /* FLEX, G2.3 steps 6.2/6.3 (2026-10-19). Mirrors the renderer's own
     * shared CSS layer (khtpm_css_parser.h:42-45 has exactly these four,
     * with has_/value pairs and the same meanings) rather than inventing a
     * second vocabulary - CENTROID_GOLD_STD §1a: share the shape, do not
     * write a private copy of the idea. The worker originally had none of
     * these, which is why a flex div laid its children out vertically: the
     * model had no horizontal axis at all (layout_xy() ended `*out_x = 0`).
     *
     * has_display_flex is DELIBERATELY separate from display[16] above:
     * display keeps the raw keyword for getComputedStyle, while these are
     * the typed values layout reads. Same split the renderer makes. */
    int has_display_flex, display_flex;      /* 1 flex, 2 grid */
    int has_flex_direction, flex_row;        /* 1=row, 0=column. CSS default (like the
                                              * renderer's model) is ROW, so layout must
                                              * treat "no direction set" as row, NOT column:
                                              * only an explicit column is vertical. */
    int has_flex_grow; double flex_grow;     /* weight for remaining main-axis space */
    int has_grid_template_columns, grid_cols;
    int grid_col_w[8];
    /* Box model (2026-10-14, G2.1). These existed NOWHERE before: the
     * resolver had no fields for them, so a declared padding/margin was
     * silently dropped and layout_xy could not offset a child from its
     * parent. That is why the G2-a fixture's `#d1 { padding: 10px }` had
     * no effect and every y came back 0 - the walk was correct, the BOX
     * MODEL underneath it did not exist. Only the shorthand's first
     * (top) value is modelled, which is all a single-number declaration
     * carries; `padding: 1px 2px` parses its first component. */
    double pad_top, pad_bottom, mar_top, mar_bottom;
    int has_pad_top, has_pad_bottom, has_mar_top, has_mar_bottom;
} NbCssStyle;

/* Parse CSS text into a fresh NbCss (never NULL; rules may be 0). Free with
 * nb_css_free. `text` need not be NUL-terminated (len given). */
NbCss *nb_css_parse(const char *text, size_t len);

/* Resolve the computed style for `el`. `inline_style` is the element's
 * `style` attribute value (may be NULL/empty) and — matching browsers —
 * overrides every stylesheet rule. `css` may be NULL. */
void nb_css_resolve(const NbCss *css, const NbNode *el,
                    const char *inline_style, NbCssStyle *out);

/* True when `el` (or any ancestor) computes to display:none / hidden. */
int nb_css_hidden(const NbCss *css, const NbNode *el);

void nb_css_free(NbCss *css);

#ifdef __cplusplus
}
#endif

#endif /* NB_CSS_H */