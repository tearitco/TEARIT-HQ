/* nb_worker_css_state.c - CSS/layout-intent state and declarations (mechanical extract, single-TU include) */
/* el.on<name> accessor names (also parsed from property arg0 of the natives) */
static const char *const ONPROPS[] = {
    "click", "dblclick", "change", "input", "submit", "keydown", "keyup",
    "keypress", "mouseover", "mouseout", "mouseenter", "mouseleave",
    "mousedown", "mouseup", "mousemove", "focus", "blur", "load", "error",
    "resize", "scroll", "contextmenu", NULL
};

/* ---- rung 7 slice 1: CSS/layout-intent ---------------------------------
 * The page's stylesheet text (inline <style> + linked CSS, manager-side)
 * reaches the worker via the LOAD style file; g_css holds the parsed rule
 * cache for this page. We model exactly: display: none, visibility:
 * hidden/collapse, opacity, and px width/height — enough for
 * getComputedStyle, display:none-driven hiding, and the offset/rect
 * family to behave on real widgets. There is no real layout engine: x/y
 * are always 0, sizes come from the CSS declaration or 0. */

#define STYK "_nbsty"   /* per-wrapper style-snapshot cache key */

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static int css_hidden(const NbNode *n);
