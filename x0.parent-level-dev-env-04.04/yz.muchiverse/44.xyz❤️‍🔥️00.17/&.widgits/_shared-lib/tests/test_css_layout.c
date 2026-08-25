/* Standalone proof for Stage 3's real css_layout_pass() (khtpm-merge-
 * how2.md §5.3 step 3) - real test cases covering every pattern in the
 * §5.1b inventory, checked by hand against expected numbers, NOT
 * against a live app yet (that's step 4+, db-hq first). */
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static void check(const char *label, int got, int want) {
    if (got != want) { printf("FAIL %s: got %d want %d\n", label, got, want); g_fail++; }
    else printf("ok   %s = %d\n", label, got);
}

static Elem g_pool[32];
static int g_n = 0;
static Elem *mk(void) { Elem *e = &g_pool[g_n++]; memset(e, 0, sizeof(*e)); return e; }

int main(void) {
    /* Test 1: pattern #1 - column stack of fixed-height bands +
     * pattern #3 - one flex-grow band eats the remainder. Mirrors
     * chrome(30)/tabbar(30)/content(flex:1)/footer(34) in a 200-tall window. */
    {
        Elem *win = mk(); win->style.has_display = 1; win->style.display_flex = 1;
        win->style.has_flex_direction = 1; win->style.flex_row = 0; /* column */
        Elem *chrome = mk(); chrome->style.has_height = 1; chrome->style.height = 30;
        Elem *tabbar = mk(); tabbar->style.has_height = 1; tabbar->style.height = 30;
        Elem *content = mk(); content->style.has_flex_grow = 1; content->style.flex_grow = 1;
        Elem *footer = mk(); footer->style.has_height = 1; footer->style.height = 34;
        win->children[0] = chrome; win->children[1] = tabbar;
        win->children[2] = content; win->children[3] = footer; win->n_children = 4;
        css_layout_pass(win, 0, 0, 720, 200);
        check("T1 win.h", win->h, 200);
        check("T1 chrome.y", chrome->y, 0);
        check("T1 tabbar.y", tabbar->y, 30);
        check("T1 content.y", content->y, 60);
        check("T1 content.h (flex remainder)", content->h, 200 - 30 - 30 - 34);
        check("T1 footer.y", footer->y, 200 - 34);
        check("T1 footer.h", footer->h, 34);
        /* real cross-axis check: column children should span full width */
        check("T1 content.w (cross-axis fill)", content->w, 720);
    }

    /* Test 2: pattern #2 - row of natural-width (pre-measured) children,
     * left-packed, matching tabbar's own real tab layout. */
    {
        Elem *row = mk(); row->style.has_display = 1; row->style.display_flex = 1;
        row->style.has_flex_direction = 1; row->style.flex_row = 1;
        Elem *tab1 = mk(); tab1->w = 60; /* pre-measured natural width, no explicit CSS width */
        Elem *tab2 = mk(); tab2->w = 90;
        Elem *tab3 = mk(); tab3->w = 45;
        row->children[0] = tab1; row->children[1] = tab2; row->children[2] = tab3; row->n_children = 3;
        css_layout_pass(row, 4, 0, 900, 30);
        check("T2 tab1.x", tab1->x, 4);
        check("T2 tab2.x", tab2->x, 4 + 60);
        check("T2 tab3.x", tab3->x, 4 + 60 + 90);
        check("T2 tab1.h (cross-axis fill)", tab1->h, 30);
    }

    /* Test 3: pattern #3 - fixed-width sidebar + flex:1 panel remainder
     * (db-hq/events-hq's own real sidebar+panel split). */
    {
        Elem *row = mk(); row->style.has_display = 1; row->style.display_flex = 1;
        row->style.has_flex_direction = 1; row->style.flex_row = 1;
        Elem *sidebar = mk(); sidebar->style.has_width = 1; sidebar->style.width = 210;
        Elem *panel = mk(); panel->style.has_flex_grow = 1; panel->style.flex_grow = 1;
        row->children[0] = sidebar; row->children[1] = panel; row->n_children = 2;
        css_layout_pass(row, 0, 60, 900, 400);
        check("T3 sidebar.w", sidebar->w, 210);
        check("T3 panel.x", panel->x, 210);
        check("T3 panel.w (flex remainder)", panel->w, 900 - 210);
    }

    /* Test 4: pattern #5 - position:absolute <title> stays OUT of flow,
     * positioned via parent-relative top/left, doesn't consume main-axis
     * space or shift its siblings. */
    {
        Elem *panel = mk(); panel->style.has_display = 1; panel->style.display_flex = 1;
        panel->style.has_flex_direction = 1; panel->style.flex_row = 0; /* column */
        Elem *title = mk();
        title->style.has_position = 1; title->style.position_absolute = 1;
        title->style.has_top = 1; title->style.top = -8;
        title->style.has_left = 1; title->style.left = 10;
        title->w = 40; title->h = 14; /* pre-measured */
        Elem *body = mk(); body->style.has_height = 1; body->style.height = 100;
        panel->children[0] = title; panel->children[1] = body; panel->n_children = 2;
        css_layout_pass(panel, 50, 100, 300, 200);
        check("T4 title.x (parent.x + left)", title->x, 50 + 10);
        check("T4 title.y (parent.y + top, negative)", title->y, 100 - 8);
        check("T4 body.y (NOT pushed by title)", body->y, 100);
    }

    /* Test 4b (NEW 2026-08-16, added after db-hq's own real live
     * tabbar port found this gap): real padding (both axes) + real
     * main-axis gap between flow children, position:absolute child
     * unaffected by either. */
    {
        Elem *row = mk(); row->style.has_display = 1; row->style.display_flex = 1;
        row->style.has_flex_direction = 1; row->style.flex_row = 1;
        row->style.has_padding = 1; row->style.padding = 5;
        row->style.has_gap = 1; row->style.gap = 2;
        Elem *a = mk(); a->w = 20;
        Elem *b = mk(); b->w = 30;
        Elem *floater = mk();
        floater->style.has_position = 1; floater->style.position_absolute = 1;
        floater->w = 8; floater->h = 8;
        row->children[0] = a; row->children[1] = floater; row->children[2] = b;
        row->n_children = 3;
        css_layout_pass(row, 100, 100, 200, 40);
        check("T4b a.x (row.x + padding)", a->x, 100 + 5);
        check("T4b a.y (row.y + padding, cross-axis)", a->y, 100 + 5);
        check("T4b a.h (row.h - 2*padding)", a->h, 40 - 10);
        check("T4b b.x (a.x + a.w + gap)", b->x, (100 + 5) + 20 + 2);
        check("T4b floater.x (unaffected by padding - parent raw x + left:0)", floater->x, 100);
    }

    /* Test 5: pattern #9 - block (non-flex) parent leaves children
     * completely untouched, matching chat-hai's own phantom-element
     * contract (caller positions them by hand, engine must not
     * interfere even if called on their parent). */
    {
        Elem *win = mk(); /* no display:flex set -> block, default */
        Elem *phantom = mk(); phantom->x = 999; phantom->y = 888; phantom->w = 77; phantom->h = 66;
        win->children[0] = phantom; win->n_children = 1;
        css_layout_pass(win, 0, 0, 900, 700);
        check("T5 phantom.x untouched", phantom->x, 999);
        check("T5 phantom.y untouched", phantom->y, 888);
    }

    /* Test 6: percentage width (real, existing-but-dead CssStyle
     * capability per §5.1b - confirm the engine actually honors it
     * even though no current live app sets it yet). */
    {
        Elem *e = mk();
        e->style.has_width = 1; e->style.width_is_pct = 1; e->style.width = 50;
        css_layout_pass(e, 0, 0, 800, 600);
        check("T6 50% of 800", e->w, 400);
    }

    printf(g_fail == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", g_fail);
    return g_fail ? 1 : 0;
}
