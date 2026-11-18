/* nb_worker_layout_metrics_state.c - layout/text metrics, viewport/scroll/gutter state, fwd decls (mechanical extract, single-TU include) */
/* Text metrics for unstyled content. The khtpm renderer draws 12px text on a
 * 14px baseline, so a line box is 14px tall and an average glyph advances
 * ~7px; content wraps at the renderer's 640px column. */
#define NB_FONT_PX     12
#define NB_LINE_PX     14
/* ROW height, which is NOT the line height (2026-10-14, G2.2).
 *
 * Measured off the renderer's own element dump on a 6-paragraph fixture:
 * every content row is painted at a CONSTANT height of 30, and y steps
 * 180, 210, 240, 270, 300, 330 - exactly +30 each. The worker's own
 * NB_LINE_PX (14, "12px text on a 14px baseline") is a LINE box: text
 * leading with no row padding around it. Using 14 for stack height made
 * the worker's y-step disagree with the renderer's by more than 2x, which
 * is the whole of G2.2's scale disagreement - two constants that meant
 * different words, not a disagreement about layout.
 *
 * So block stacking uses NB_ROW_PX. NB_LINE_PX stays where it belongs:
 * inside the wrap calculation, as the per-line text box. */
#define NB_ROW_PX     30
#define NB_CHAR_PX     7
#define NB_CONTENT_PX  640

/* VIEWPORT width, runtime (2026-10-14, G2.2).
 *
 * The worker's box tree assumed a 640px content column. Measured against
 * the renderer's own element dump the pane is 389px, so every width the
 * worker reports is wrong by ~250px - and no amount of box-model work
 * fixes it: the worker simply does not know how wide the pane it lands in
 * will be. This makes that width a VALUE rather than a compile-time
 * constant, so it can be supplied per page and seen by the page itself.
 *
 * Defaults to NB_CONTENT_PX, so behaviour is unchanged until something
 * supplies a real number - which is the point: the guess is now visible
 * and replaceable instead of baked in. */
static double g_viewport_px = NB_CONTENT_PX;

/* The scrollbar gutter (G2.2, 2026-10-15).
 *
 * The renderer paints a VISIBLE scrollbar for any scrolllist that OVERFLOWS
 * its viewport - track + thumb + arrow navs - and reserves GENERIC_SCROLLBAR_W
 * (8) of the column for it (khtpm_core_render.c:5383). Measured live on the
 * G2.2 fixture: the renderer publishes content_w=389 and the painted content
 * rows are 381. So without this the box tree reports every scrolling row 8px
 * wider than the renderer actually draws it, which is precisely the kind of
 * confident wrong number G2 exists to remove.
 *
 * The deduction is CONDITIONAL: a page that fits has no scrollbar and keeps
 * the full width. That means the decision needs the viewport HEIGHT, which
 * LOAD did not carry - it sent width only (field 6). Field 7 now carries
 * content_h. An absent or unusable height leaves the gutter OFF, because
 * guessing "it probably scrolls" would be the same class of lie in the
 * opposite direction.
 *
 * There is a genuine cycle here: wrapping (nb_content_h) depends on the
 * column width, and the column width depends on whether the wrapped height
 * overflows. It is broken by deciding ONCE per page - measure the document
 * height at the FULL width, cache the verdict - with g_measuring making that
 * inner measurement ignore the gutter. Deciding per call would make the
 * answer depend on which element happened to be measured first. */
#define NB_SCROLLBAR_W 8
static double g_viewport_h = 0.0;
static int    g_gutter_state = -1;   /* -1 undecided this page, 0 none, 1 present */
static int    g_measuring = 0;
/* Page-driven scroll offset (2026-11-09). The renderer owns the native pane
 * scroll and exposes no channel for it, so the worker tracks the offset the
 * PAGE drives: window.scrollTo/scrollBy, element.scrollIntoView. Rects are
 * viewport-relative (document y minus this offset), and changing the offset
 * re-evaluates IntersectionObserver targets. A page script cannot scroll
 * the native pane; its ISV and IO math see a consistent within-worker world. */
static double g_scroll_x = 0.0;
static double g_scroll_y = 0.0;

static double nb_content_h(NbNode *n);
/* 6.2/6.3 (2026-10-19): both are called from functions defined ABOVE their
 * own definitions (nb_box_w -> nb_edge_pad_h, nb_content_h -> nb_outer_h),
 * so forward-declare rather than reorder the file. */
static double nb_outer_h(NbNode *n);
static double nb_edge_pad_h(NbNode *n);

/* Total height of the document `from` belongs to. Walks to the topmost
 * ancestor, then sums its children - the same shape nb_content_h uses, so the
 * measurement is the one the box tree would report anyway. */
