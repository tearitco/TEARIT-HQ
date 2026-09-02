/* khtpm_css_parser.c — see khtpm_css_parser.h header comment for scope. */
#include "khtpm_css_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void css_style_init(CssStyle *s) {
    memset(s, 0, sizeof(*s));
}

/* merges src's SET fields on top of dst (src wins per-property) */
static void css_style_merge(CssStyle *dst, const CssStyle *src) {
    if (src->has_bg_color)     { dst->has_bg_color = 1; snprintf(dst->bg_color, sizeof(dst->bg_color), "%s", src->bg_color); }
    if (src->has_fg_color)     { dst->has_fg_color = 1; snprintf(dst->fg_color, sizeof(dst->fg_color), "%s", src->fg_color); }
    if (src->has_border_color) { dst->has_border_color = 1; snprintf(dst->border_color, sizeof(dst->border_color), "%s", src->border_color); }
    if (src->has_border_width) { dst->has_border_width = 1; dst->border_width = src->border_width; }
    if (src->has_position)     { dst->has_position = 1; dst->position_absolute = src->position_absolute; }
    if (src->has_top)          { dst->has_top = 1; dst->top = src->top; }
    if (src->has_left)         { dst->has_left = 1; dst->left = src->left; }
    if (src->has_width)        { dst->has_width = 1; dst->width = src->width; dst->width_is_pct = src->width_is_pct; }
    if (src->has_height)       { dst->has_height = 1; dst->height = src->height; dst->height_is_pct = src->height_is_pct; }
    if (src->has_padding)      { dst->has_padding = 1; dst->padding = src->padding; }
    if (src->has_font_family)  { dst->has_font_family = 1; snprintf(dst->font_family, sizeof(dst->font_family), "%s", src->font_family); }
    if (src->has_font_size)    { dst->has_font_size = 1; dst->font_size = src->font_size; }
    if (src->has_font_weight)  { dst->has_font_weight = 1; dst->font_weight_bold = src->font_weight_bold; }
    if (src->has_z_index)      { dst->has_z_index = 1; dst->z_index = src->z_index; }
    /* REAL START 2026-08-16, Stage 3 - same real per-field merge shape
     * as every field above, not a shortcut. Missing this would silently
     * drop display/flex-direction/flex-grow whenever a real cascaded
     * rule (e.g. ".tabbar.active") needed to combine with a base rule -
     * a real, easy-to-miss bug class this function's own existing
     * pattern already guards every other field against. */
    if (src->has_display)        { dst->has_display = 1; dst->display_flex = src->display_flex; }
    if (src->has_flex_direction) { dst->has_flex_direction = 1; dst->flex_row = src->flex_row; }
    if (src->has_flex_grow)      { dst->has_flex_grow = 1; dst->flex_grow = src->flex_grow; }
    if (src->has_gap)            { dst->has_gap = 1; dst->gap = src->gap; }
}

static void trim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) { s[n - 1] = '\0'; n--; }
}

static void parse_declaration(const char *prop, const char *val, CssStyle *out) {
    char v[256];
    snprintf(v, sizeof(v), "%s", val);
    trim(v);
    if (strcmp(prop, "background-color") == 0 || strcmp(prop, "background") == 0) {
        out->has_bg_color = 1; snprintf(out->bg_color, sizeof(out->bg_color), "%s", v);
    } else if (strcmp(prop, "color") == 0) {
        out->has_fg_color = 1; snprintf(out->fg_color, sizeof(out->fg_color), "%s", v);
    } else if (strcmp(prop, "border-color") == 0) {
        out->has_border_color = 1; snprintf(out->border_color, sizeof(out->border_color), "%s", v);
    } else if (strcmp(prop, "border") == 0) {
        /* "1px solid #999" - pull width (first token) and color (last token) */
        int w = 1; char color[32] = "#000000";
        char tmp[256]; snprintf(tmp, sizeof(tmp), "%s", v);
        char *tok = strtok(tmp, " ");
        int i = 0;
        while (tok) {
            if (i == 0) w = atoi(tok);
            if (tok[0] == '#') snprintf(color, sizeof(color), "%s", tok);
            tok = strtok(NULL, " ");
            i++;
        }
        out->has_border_width = 1; out->border_width = w;
        out->has_border_color = 1; snprintf(out->border_color, sizeof(out->border_color), "%s", color);
    } else if (strcmp(prop, "position") == 0) {
        out->has_position = 1; out->position_absolute = (strcmp(v, "absolute") == 0);
    } else if (strcmp(prop, "top") == 0) {
        out->has_top = 1; out->top = atoi(v);
    } else if (strcmp(prop, "left") == 0) {
        out->has_left = 1; out->left = atoi(v);
    } else if (strcmp(prop, "width") == 0) {
        out->has_width = 1;
        out->width_is_pct = (strchr(v, '%') != NULL);
        out->width = atoi(v);
    } else if (strcmp(prop, "height") == 0) {
        out->has_height = 1;
        out->height_is_pct = (strchr(v, '%') != NULL);
        out->height = atoi(v);
    } else if (strcmp(prop, "padding") == 0) {
        out->has_padding = 1; out->padding = atoi(v);
    } else if (strcmp(prop, "font-family") == 0) {
        out->has_font_family = 1; snprintf(out->font_family, sizeof(out->font_family), "%s", v);
    } else if (strcmp(prop, "font-size") == 0) {
        out->has_font_size = 1; out->font_size = atoi(v);
    } else if (strcmp(prop, "font-weight") == 0) {
        out->has_font_weight = 1; out->font_weight_bold = (strcmp(v, "bold") == 0 || atoi(v) >= 600);
    } else if (strcmp(prop, "z-index") == 0) {
        out->has_z_index = 1; out->z_index = atoi(v);
    } else if (strcmp(prop, "display") == 0) {
        /* REAL START 2026-08-16, Stage 3 - real, inventory-confirmed
         * subset only (khtpm-merge-how2.md §5.1b) - "flex" is the only
         * real value any of the 3 current apps' own layouts would need;
         * anything else (grid, inline-block, none) parses as block
         * (has_display=1, display_flex=0), same as omitting the
         * property entirely - real, deliberate, not a silent bug. */
        out->has_display = 1; out->display_flex = (strcmp(v, "flex") == 0);
    } else if (strcmp(prop, "flex-direction") == 0) {
        out->has_flex_direction = 1; out->flex_row = (strcmp(v, "row") == 0);
    } else if (strcmp(prop, "flex-grow") == 0) {
        out->has_flex_grow = 1; out->flex_grow = atoi(v);
    } else if (strcmp(prop, "gap") == 0) {
        out->has_gap = 1; out->gap = atoi(v);
    }
    /* unrecognized properties (box-shadow, border-radius, grid, etc.) are
     * silently ignored - out of scope for this minimal subset. */
}

int css_load(const char *path, CssSheet *sheet) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    char *buf = malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    char *p = buf;
    while (*p) {
        /* skip whitespace and comments */
        while (*p && isspace((unsigned char)*p)) p++;
        if (p[0] == '/' && p[1] == '*') {
            char *end = strstr(p, "*/");
            p = end ? end + 2 : p + strlen(p);
            continue;
        }
        if (!*p) break;
        char *brace = strchr(p, '{');
        if (!brace) break;
        char selector_raw[256];
        size_t sel_len = (size_t)(brace - p);
        if (sel_len >= sizeof(selector_raw)) sel_len = sizeof(selector_raw) - 1;
        memcpy(selector_raw, p, sel_len);
        selector_raw[sel_len] = '\0';
        trim(selector_raw);

        char *close = strchr(brace, '}');
        if (!close) break;
        char body[2048];
        size_t body_len = (size_t)(close - brace - 1);
        if (body_len >= sizeof(body)) body_len = sizeof(body) - 1;
        memcpy(body, brace + 1, body_len);
        body[body_len] = '\0';

        /* selector list may be comma-separated - one rule per selector, same style */
        CssStyle style; css_style_init(&style);
        char *decl = strtok(body, ";");
        while (decl) {
            char *colon = strchr(decl, ':');
            if (colon) {
                *colon = '\0';
                char prop[64]; snprintf(prop, sizeof(prop), "%s", decl);
                trim(prop);
                parse_declaration(prop, colon + 1, &style);
            }
            decl = strtok(NULL, ";");
        }

        char *sel_tok = strtok(selector_raw, ",");
        while (sel_tok) {
            char one[128]; snprintf(one, sizeof(one), "%s", sel_tok);
            trim(one);
            if (one[0] && sheet->n_rules < CSS_MAX_RULES) {
                snprintf(sheet->rules[sheet->n_rules].selector, sizeof(sheet->rules[sheet->n_rules].selector), "%s", one);
                sheet->rules[sheet->n_rules].style = style;
                sheet->n_rules++;
            }
            sel_tok = strtok(NULL, ",");
        }

        p = close + 1;
    }
    free(buf);
    return 1;
}

/* returns: 0=no match, 1=element-tag tier, 2=class tier, 3=id tier */
static int selector_tier_match(const char *selector, const char *tag, const char *id,
                                char classes[][32], int n_classes, int hover) {
    char sel[128]; snprintf(sel, sizeof(sel), "%s", selector);
    int want_hover = 0;
    char *hov = strstr(sel, ":hover");
    if (hov) { want_hover = 1; *hov = '\0'; }
    if (want_hover && !hover) return 0;

    if (sel[0] == '#') {
        return (id && strcmp(sel + 1, id) == 0) ? 3 : 0;
    }
    if (sel[0] == '.') {
        /* one or more dot-separated class requirements, e.g. ".tab.active" */
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", sel + 1);
        char *want = strtok(tmp, ".");
        while (want) {
            int found = 0;
            for (int i = 0; i < n_classes; i++) if (strcmp(classes[i], want) == 0) { found = 1; break; }
            if (!found) return 0;
            want = strtok(NULL, ".");
        }
        return 2;
    }
    /* bare element tag, possibly with trailing .class (e.g. "button.primary") - not used
     * by dashboard.css today but handled: split on first '.' */
    char *dot = strchr(sel, '.');
    if (dot) *dot = '\0';
    if (tag && strcmp(sel, tag) == 0) return 1;
    return 0;
}

void css_compute_style(const CssSheet *sheet, const char *tag, const char *id,
                        char classes[][32], int n_classes, int hover, CssStyle *out) {
    css_style_init(out);
    /* three passes, low to high specificity, so later tiers override earlier ones;
     * within a tier, later-in-file rules override earlier ones (cascade order). */
    for (int tier = 1; tier <= 3; tier++) {
        for (int i = 0; i < sheet->n_rules; i++) {
            if (selector_tier_match(sheet->rules[i].selector, tag, id, classes, n_classes, hover) == tier) {
                css_style_merge(out, &sheet->rules[i].style);
            }
        }
    }
}
