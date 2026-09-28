/* khtpm_strip_layout.h — the real declarative-layout engine for the strip,
 * per khtpm-strip-parser-SCOPE.md. This is the non-Xlib half of what the
 * scope doc calls "the parser itself": tokenizer, generic attribute parser,
 * a flat tag tree with parent_index (matching CHTPM's own flat-array
 * approach, not a pointer tree), ${var} substitution, and the ACTIVATE
 * scope mechanism (is_navigable()/is_descendant()/active_index/focus_index)
 * — all ported directly from chtpm_parser.c's real functions of the same
 * name/purpose (tokenize(), parse_attributes(), substitute_vars_naked(),
 * substitute_vars(), is_navigable(), is_descendant()), read in full from
 * "101.ledger-player-npc-simple+3/system/chtpm_parser.c" before writing
 * this file — not paraphrased from the scope doc's summary of them.
 *
 * Locked 5-tag vocabulary only (see SCOPE.md): <panel> <text>
 * <button label=".." onClick=".."> <row> <cli_io target_id=".." label="..">.
 * No <scroller>/<canvas>/<module>/<link>/<br/>, no visibility/time_reactive
 * attributes — the strip's popup-vs-header visibility is handled ENTIRELY
 * by the ACTIVATE scope mechanism, matching the scope doc's explicit
 * simplification.
 *
 * Xlib drawing, hit-testing, and manager-code dispatch are NOT here — those
 * stay in khtpm_strip_parser.c per the scope doc's render-walk/dispatch-walk
 * split ("the ACTUAL drawing primitives are fine to keep, it's the per-cell
 * hardcoding around them that goes away"). This file only owns the tag tree
 * and the runtime focus_index/active_index ints — a real design correction
 * from today's earlier hardcoded pass, which baked the cursor prefix into
 * manager-formatted label strings instead of computing it here at render
 * time from the parser's own runtime state (see SCOPE.md's "Cursor/focus
 * rendering" section).
 */
#ifndef KHTPM_STRIP_LAYOUT_H
#define KHTPM_STRIP_LAYOUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAY_MAX_ELEMENTS   256
#define LAY_MAX_CHILDREN   64
#define LAY_MAX_TOKENS     (LAY_MAX_ELEMENTS * 4)
#define LAY_TYPE_LEN       16
#define LAY_LABEL_LEN      512   /* must hold a whole substituted ${var} fragment (many buttons) for row/text content */
#define LAY_ONCLICK_LEN    64
#define LAY_ATTR_LEN       64
#define LAY_TAGNAME_LEN    32
#define LAY_ATTRSTR_LEN    768
#define LAY_FILE_MAX       (64 * 1024)

/* Real per-tab sprite support (2026-08-11, "i want to put sprites back" —
 * see tp_taskbar.c's own tab_sprite()/blit_tab_sprite(), read in full
 * before adding this): a <button>'s optional "sprite" attribute carries the
 * ENTITY DIRECTORY (not the sprite.csv file itself — khtpm_strip_parser.c's
 * own tab_sprite() appends "/sprite.csv", matching legacy exactly) whose
 * sprite.csv, if present, should be drawn next to that tab's label. Sized
 * to comfortably hold a real absolute entity path in this house (confirmed
 * live PATH= values run up to ~222 bytes; KtbTab.path itself is
 * KTB_PATH_BUF=4352 in khtpm_taskbar_manager.h, but this file intentionally
 * doesn't include that header, so a smaller-but-generous constant is used
 * here instead — LAY_ATTRSTR_LEN=768 is the real ceiling anyway, since the
 * whole tag's attribute string must fit in that buffer). */
#define LAY_SPRITE_LEN     512

/* REAL, NEW 2026-08-16, direct correction ("the cells aren't supposed
 * to be hardcoded... that's an oversight"): a real, stable STRING
 * identity for header buttons, so C dispatch can match by real
 * NAME (e.g. "toys") instead of purely by strip POSITION
 * (ACTIVATE:<n>, `which = cell index + 1`) - the real bug class this
 * fixes: the position-only scheme has zero compiler/runtime check
 * tying a `.chtpm` button's real position to the matching `which == N`
 * case in khtpm_taskbar_manager.c's own dispatch chain, so a button
 * reorder in the `.chtpm` silently desyncs every case below it with no
 * error. Same real addition shape as `sprite`'s own 2026-08-11
 * precedent (see that field's own comment) - optional, "" if absent,
 * existing `which`-based dispatch stays as real, working fallback for
 * every cell not yet migrated to id-based matching. */
#define LAY_ID_LEN         64

typedef struct {
    char type[LAY_TYPE_LEN];        /* "panel" | "text" | "button" | "row" | "cli_io" */
    char label[LAY_LABEL_LEN];      /* RAW, pre-substitution (may contain literal ${var}) */
    char onClick[LAY_ONCLICK_LEN];  /* RAW attribute, e.g. "ACTIVATE:3", "TAB:0", "STRIP:2", "HQITEM:1", "BACK" */
    char target_id[LAY_ATTR_LEN];   /* cli_io only */
    char sprite[LAY_SPRITE_LEN];    /* button only, optional — entity dir for tab_sprite(), "" = none */
    char id[LAY_ID_LEN];            /* button only, optional real stable identity — "" if absent */
    int  parent_index;              /* -1 = root */
    int  children[LAY_MAX_CHILDREN];
    int  num_children;
} LayElement;

typedef struct {
    LayElement elements[LAY_MAX_ELEMENTS];
    int element_count;
    int active_index;   /* -1 = no ACTIVATE scope open (matches chtpm_parser.c's active_index) */
    int focus_index;    /* cursor position among navigable elements */
} LayDoc;

/* Host-supplied ${var} lookup — mirrors chtpm_parser.c's get_var(), but
 * as a callback so khtpm_strip_parser.c can back it with strip_state.txt's
 * KEY rows + the manager's pre-rendered markup-fragment files, without this
 * file knowing anything about that file format. Must return "" (never NULL)
 * for an unknown var, matching get_var()'s own contract. */
typedef const char *(*LayVarLookupFn)(const char *name, void *ctx);

/* Load + tokenize + build the tag tree from a layout file on disk. Safe to
 * call repeatedly (e.g. once per dirty redraw, mirroring chtpm_parser.c's
 * own parse_chtm() being re-run from compose_frame() whenever underlying
 * vars changed) — each call fully rebuilds elements[]/element_count from
 * scratch. Returns 1 on success, 0 if the file couldn't be read.
 *
 * IMPORTANT — index stability across reloads: because ${var} fragments
 * (e.g. a tab list) can change ELEMENT COUNT between reloads, an element's
 * numeric index is NOT guaranteed stable across two lay_load() calls of the
 * same file. Callers that hold onto active_index/focus_index across a
 * reload must re-resolve them by identity (e.g. by onClick string), not by
 * raw index — see lay_reload_preserving_scope() below, which does exactly
 * that and is the function callers should actually use for a live redraw
 * loop; lay_load() is the one-shot primitive it's built on. */
int lay_load(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx);

/* Reload path into doc, preserving the currently-open ACTIVATE scope (by
 * onClick identity, since raw indices can shift — see lay_load()'s own
 * comment) instead of resetting active_index/focus_index to closed. This is
 * what the render loop should call on every dirty redraw. */
int lay_reload_preserving_scope(LayDoc *doc, const char *path, LayVarLookupFn get_var, void *ctx);

/* is_interactive()/is_navigable()/is_descendant() — ported directly from
 * chtpm_parser.c's real functions of the same name. is_navigable() folds in
 * the ACTIVATE-scope check (only an active menu's own descendants — or the
 * active menu root itself — are navigable while a scope is open). */
int lay_is_interactive(const LayElement *el);
int lay_is_navigable(const LayDoc *doc, int idx);
int lay_is_descendant(const LayDoc *doc, int child_idx, int parent_idx);

/* True if el->onClick begins with "ACTIVATE" (our onClick strings carry a
 * ":<manager-code>" suffix the real CHTPM reference doesn't need, since
 * this layout's submenu CONTENT is manager-authored per-cell rather than
 * already present in the static tree — see khtpm-strip-parser-SCOPE.md
 * necessitation notes in the final report for why a prefix match, not
 * strcmp(onClick,"ACTIVATE")==0, is correct here). */
int lay_is_activate_marker(const char *onClick);

/* Local scope transitions — ACTIVATE toggles into idx's scope (mirrors
 * clicking an onClick="ACTIVATE.." button); BACK pops to the nearest
 * ACTIVATE ancestor of the current active_index (ported from
 * chtpm_parser.c's send_command()'s "BACK" branch, ~line 1667). Both are
 * pure LOCAL scope changes — no manager round-trip, matching the scope
 * doc's explicit call-out that ACTIVATE/BACK should be handled entirely in
 * the parser. */
void lay_activate(LayDoc *doc, int idx);
void lay_back(LayDoc *doc);

/* Move focus_index by +1/-1 among currently-navigable elements, wrapping.
 * Mirrors chtpm_parser.c's do_jump()/count_navigable() in spirit (a linear
 * walk over is_navigable() elements), simplified to a delta-step since the
 * strip has no digit-jump-by-number requirement at the layout-engine level
 * (khtpm_strip_codes.h's existing digit buffer stays a manager-side
 * concept, untouched). */
void lay_focus_delta(LayDoc *doc, int delta);

/* Cursor prefix for element idx — "[^]" if idx is the open active scope,
 * "[>]" if idx has focus (and is either top-level or itself navigable),
 * "[ ]" otherwise. Ported directly from render_element()'s own prefix logic
 * (chtpm_parser.c ~line 2254-2260) — this is the actual design-correction
 * piece SCOPE.md calls out: the PARSER computes this, nothing is baked into
 * any label string. */
const char *lay_cursor_prefix(const LayDoc *doc, int idx);

/* Substituted (${var}-resolved) label for element idx, written into out
 * (size outsz). Ported from substitute_vars(). */
void lay_get_label(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                    char *out, size_t outsz);

/* Substituted (${var}-resolved) sprite path for element idx — mirrors
 * lay_get_label() exactly. Real gap fix, 2026-08-11 ("user should have a
 * sprite at least"): needed because the header's static layout uses
 * sprite="${avatar_dir}", unlike tabs whose sprite path is already a
 * concrete string by the time it reaches the tokenizer. */
void lay_get_sprite(const LayDoc *doc, int idx, LayVarLookupFn get_var, void *ctx,
                     char *out, size_t outsz);

/* REAL, NEW 2026-08-16 - see LayElement's own `id` field comment. */
const char *lay_get_id(const LayDoc *doc, int idx);

#ifdef __cplusplus
}
#endif
#endif /* KHTPM_STRIP_LAYOUT_H */
