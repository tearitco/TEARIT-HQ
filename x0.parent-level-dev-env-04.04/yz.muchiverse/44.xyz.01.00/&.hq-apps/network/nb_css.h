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
 * ignored. Sizes resolve to 0 when absent. This module has zero JS/Duktape
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