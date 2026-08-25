/* khtpm_css_parser.h — minimal CSS subset for the HQML styling layer
 * (au11-hq/HQML-DESIGN+PLANS.md Phase 1), first proof for db-hq.
 *
 * Scope is deliberately small (au11-hq/rpg-maker-database.html's
 * load-bearing subset only, per the design doc's own "start small" note):
 * selectors = element, .class, .class.class, #id, :hover: properties =
 * background-color, color, border(-color/-width), position/top/left,
 * width/height (px or %), padding, font-family/size/weight, z-index.
 * No flex/grid/animations - out of scope until a section needs them. */
#ifndef KHTPM_CSS_PARSER_H
#define KHTPM_CSS_PARSER_H

#define CSS_MAX_RULES 256
#define CSS_MAX_CLASSES 8

typedef struct {
    int has_bg_color;      char bg_color[32];
    int has_fg_color;      char fg_color[32];
    int has_border_color;  char border_color[32];
    int has_border_width;  int border_width;
    int has_position;      int position_absolute; /* 0=relative/static, 1=absolute */
    int has_top;            int top;    /* px, signed */
    int has_left;           int left;   /* px, signed */
    int has_width;           int width;   int width_is_pct;
    int has_height;          int height;  int height_is_pct;
    int has_padding;         int padding;
    int has_font_family;     char font_family[64];
    int has_font_size;       int font_size;
    int has_font_weight;     int font_weight_bold;
    int has_z_index;         int z_index;
    /* REAL START 2026-08-16, Stage 3 (khtpm-merge-how2.md §5) - real,
     * inventory-confirmed fields only (see that doc's own §5.1b for the
     * exact evidence from all 3 real consumers' current layout_pass()
     * functions before this was added - db-hq/events-hq/chat-hai).
     * gap/justify-content/align-items deliberately NOT added - that
     * same real inventory confirmed none of the 3 apps' current layouts
     * need them, adding them speculatively would be untested surface
     * area. */
    int has_display;         int display_flex; /* 0=block (default), 1=flex */
    int has_flex_direction;  int flex_row;      /* 0=column, 1=row - only meaningful if display_flex */
    int has_flex_grow;       int flex_grow;     /* real weight; a child with this set consumes remaining space on the main axis */
    /* REAL 2026-08-16, added AFTER the first real live port (db-hq's
     * own tabbar) found a real, genuine gap in the original §5.1b
     * scope: `padding` (existing field above, already real/used
     * elsewhere for text-label inset - reused here, same real
     * "inset from box edge" meaning, not a new concept) now ALSO
     * insets flex children on the cross axis when set on the
     * container; `gap` is a real, new, additive main-axis space
     * between consecutive flex children - NOT in the original §5.1b
     * inventory (that inventory correctly found the 3 apps' own MAIN
     * patterns didn't need it, but missed 2 real per-app insets/gaps
     * db-hq's own tabbar needed - see khtpm-merge-how2.md's own real
     * step-4 writeup). */
    int has_gap;              int gap;
} CssStyle;

typedef struct {
    char selector[128];   /* raw selector text, e.g. ".tab.active", "#sidebar", "button:hover" */
    CssStyle style;
} CssRule;

typedef struct {
    CssRule rules[CSS_MAX_RULES];
    int n_rules;
} CssSheet;

void css_style_init(CssStyle *s);
/* loads path, appends parsed rules into sheet (sheet must be zero-inited by caller first) */
int css_load(const char *path, CssSheet *sheet);
/* computes the cascaded style for an element (tag/id/classes) into *out.
 * hover: pass 1 if the element is currently hovered (activates :hover rules). */
void css_compute_style(const CssSheet *sheet, const char *tag, const char *id,
                        char classes[][32], int n_classes, int hover, CssStyle *out);

#endif
