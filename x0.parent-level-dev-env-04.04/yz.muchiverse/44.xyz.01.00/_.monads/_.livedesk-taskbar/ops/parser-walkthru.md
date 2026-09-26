# parser-walkthru.md — `khtpm_core_render.c` post-refactor audit (2026-08-29)

Real answers, grounded in the actual current file (9,950 lines, 461
`static` functions), written after a direct user question: "why is
this 9000 lines... didn't we condense it by getting rid of duplication
of the 4 different loops? ... explain modes to me... consider this a
post refactor audit."

Short version up front: **the 4-loop collapse this session did was
real and stayed collapsed** (verified below) — `draw_elem()`/
`render_tree()` are each a single shared function now, not one copy
per mode. What's still separate per mode — layout, click, key, and nav
assignment — is separate **on purpose**, not leftover duplication: each
mode's UI has a genuinely different shape (sidebar+panel vs
toolbar+left+right+footer vs composer+feed), so a shared layout/click
function would need to branch internally on mode anyway, which is
exactly the kind of hidden-mode-switch code this house's own standards
warn against. The honest tech debt is smaller and different — see §4.

---

## 1. What "mode" means here, concretely

This is genuinely one binary serving many roles, decided once at
startup by reading the root `<window class="...">` attribute out of
whichever `.chtpm` file was passed on argv (`main()`, ~line 9239). One
`g_is_<mode>` flag gets set to 1, and stays that way for the rest of
the process's life:

| `.chtpm` root class | Flag set | Notes |
|---|---|---|
| `swatch-picker` | `g_is_swatch_picker` | taskbar-settings |
| `db-hq` | `g_is_db_hq` | |
| `events-hq-window` | `g_is_events_hq` | |
| `chat-window` | `g_is_chat_hai` | |
| `stats-hq` | `g_is_stats_hq` **and** `g_is_db_hq` | stats-hq reuses ALL of db-hq's rendering machinery, `g_is_stats_hq` only flips a few content differences |
| `palettes` | `g_is_palettes` **and** `g_is_db_hq` | same pattern |
| `bookmarks` | `g_is_bookmarks` **and** `g_is_db_hq` | same pattern |
| *(none of the above matched)* | *(all flags stay 0)* | falls through to the generic **popup** path — this is what entity-menu right-click context menus AND taskbar-settings' swatch picker both are |

So there are really only **4 top-level UI shapes**: db-hq-family
(db-hq/stats-hq/palettes/bookmarks share one shape), events-hq,
chat-hai, and the generic popup. Every other visible difference
(sidebar vs no sidebar, tabs vs no tabs) is a content decision made
*inside* the db-hq-family code based on the sub-flags, not a 5th
top-level mode.

**Direct answer to "tb still has 1 click only, is that a different
mode?"**: yes — taskbar-settings matches none of `db-hq`/
`events-hq-window`/`chat-window`, so it never sets any of those flags
and runs the **popup** path (`popup_handle_click()`, ~line 8957),
which was deliberately left as single-click-activates when the new
two-step convention landed — a transient right-click-style flyout
menu is a different UX shape than a persistent window (see that
function's own commit message), the same real distinction chat-hai's
composer text field already gets (click-to-focus-for-typing isn't
"select a menu item" either).

---

## 2. The three real layers, bottom to top

1. **Shared, X11-free tree/CSS engine** — `khtpm_render_core.c`
   (`Elem` struct, `hit_test()`, `find_by_id()`/`find_by_tag()`,
   `css_layout_pass()`) and `khtpm_css_parser.c` (real CSS-like
   stylesheet parsing, compound class selectors). Genuinely mode-
   agnostic; doesn't know what a "db-hq" is.
2. **Shared, X11-using draw layer** — `khtpm_draw_core.c`
   (`draw_elem()`, `render_tree()`, `font_for()`/`alloc_pixel()`/
   `xft_color()`, the sprite cache). **This is the layer the "4 loops"
   collapse this session actually fixed** (see §3) — every mode calls
   the SAME `draw_elem()`/`render_tree()` now.
3. **Per-mode layout/input/content layer** — lives directly in
   `khtpm_core_render.c`, one real family per mode
   (`dbhq_*`/`evhq_*`/`chai_*`), each owning its own `*_layout_pass()`,
   `*_handle_click()`, `*_handle_key()`, `*_activate_elem()`,
   `*_assign_nav_indices()`, `*_redraw_content()`. This is where the
   461 functions mostly live, and where the real 77/50/49 per-mode
   split (§4) comes from.

Real per-file line counts for context: this file's own 9,950 lines is
the WHOLE per-mode layer for 4 shapes plus all the shared infra it
`#include`s inline (`khtpm_draw_core.c`, `khtpm_css_parser.c`,
`khtpm_taskbar_manager.c`) — it is not one flat blob of unstructured
duplication, it's several real files concatenated at build time (see
`build_core_render.sh`'s own `cp`+`$CC` lines).

---

## 3. The "4 loops" — confirmed still collapsed, not regrown

Before Part A of the events-hq render unification (this session,
`EVENTS-HQ-RENDER-UNIFICATION-PLAN.md`), events-hq ran its OWN hand-
copied `evhq_draw_elem()`/`evhq_render_tree()` — a real, drifted
duplicate of the shared draw layer, missing 8 real capabilities the
shared version had already gained (sprite support, badge-font cache,
border-width-aware layout, etc.). That pair was deleted outright; every
call site now calls the shared `draw_elem()`/`render_tree()` directly.

Verified again just now, for this doc:

```
grep -c "^static void draw_elem\|^static void render_tree" → 1 each,
total across the whole file (defined once, in khtpm_draw_core.c,
included once)
```

No `evhq_draw_elem`, no `chai_draw_elem`, no `dbhq_draw_elem` exist
anywhere in the current file. **This part of the audit is clean.**

---

## 4. What IS still separate per mode, and why that's a real, deliberate design choice

Real function-count-by-prefix, taken directly from the current file:

```
77  dbhq_*
50  chai_*
49  evhq_*
13  nav_*      (shared: tab-cycle, ledger publish)
 6  hq_*       (shared: generic helpers)
 4  history_*  (shared: per-pid input relay, see §5)
 2  popup_*    (shared: the generic popup path)
 1  picker...  (the Add-Command overlay - embedded in evhq_* despite being called from both dbhq and evhq, see §4.3)
```

The big three (`dbhq_`/`chai_`/`evhq_`) each have their own real,
separate:

- `*_layout_pass(Elem *window)` — `dbhq_layout_pass` (~2009),
  `evhq_layout_pass` (~4287), `chai_layout_pass` (~6364)
- `*_handle_click(int px, int py)` — `dbhq_handle_click` (~3032),
  `evhq_handle_click` (~5115), `chai_handle_click` (~7614)
- `*_handle_key(KeySym ks, char ch)` — `dbhq_handle_key` (~3083),
  `evhq_handle_key` (~5202), `chai_handle_key` (~7684)
- `*_activate_elem(Elem *hit)` — `dbhq_activate_elem` (~2911),
  `evhq_activate_elem` (~5018), `chai_activate_elem` (~7449)
- `*_assign_nav_indices(Elem *window)` — `dbhq_assign_nav_indices`
  (~2230), `evhq_assign_nav_indices` (~4553) (chat-hai assigns nav
  inline in its own layout pass instead of a separate function - a
  real, minor inconsistency, not a bug)
- `*_redraw_content(void)` — `dbhq_redraw_content` (~2675),
  `evhq_redraw_content` (~4953), `chai_redraw` (~7260)

### 4.1 Why this ISN'T the same class of bug the draw-loop collapse fixed

The draw-loop duplicate (§3) was two functions doing the **identical
job** (walk an Elem tree, paint each node) with **incidentally
different, drifted feature sets** — a real bug, fixed by deleting one
and pointing everyone at the other.

The layout/click/key functions are NOT doing the identical job. Each
mode's own `*_layout_pass()` positions a **structurally different
tree**: db-hq's is tabbar+sidebar+panel, events-hq's is
toolbar+pagetabs+left+right+footer(+viewmode-stub), chat-hai's is
sidebar+composer+panel-feed. A shared `mode_layout_pass()` would need
either (a) a big internal `if(g_is_db_hq){...}else if(g_is_events_hq)
{...}else{...}` — which just re-creates 3 separate functions glued
together with worse locality, or (b) a fully generic, data-driven
layout system that doesn't exist yet and would be a real, large,
separate project (this is close to what `K.1`/`K.2` in
`!.HOUSE_STDS.md` gesture at as the long-term direction — "never
hardcode a UI" — but that's a bigger rewrite, not a refactor of what's
here). Keeping them separate is the honest, currently-correct choice,
not unaddressed debt.

### 4.2 Where real, generalized sharing DOES already happen correctly

When a genuinely identical *behavior* (not just similar shape) is
needed across modes, this file already does share it, as one real
function, not copy-pasted:

- `click_focus_then_activate(Elem *hit)` (added this session, ~line
  450) — the new two-step click rule. Called from all 3 modes' own
  `*_handle_click()` and both picker overlays' own click loops. One
  real function, one real behavior, no duplication.
- `generic_scroll_layout_pass()` — shared scroll/pagination math,
  used by db-hq's palette grid AND events-hq's command list AND the
  Add-Command picker's own type list.
- `reusable_slot()` — the shared dynamic-Elem-injection pattern every
  mode's content-building code uses instead of a fresh `elem_new()`
  per frame.
- `history_path()`/`poll_agent_history()`/`dispatch_relay_code()` —
  the shared input-relay mechanism (per-PID as of tonight, see
  `HQ-WINDOW-MAP-AND-AGENT-INPUT.md`), identical for every mode.
- `evhq_build_scratch_view()`/`evhq_handle_block_onclick()` — Part B
  of tonight's events-hq unification: Common Events (db-hq) calls
  these SAME functions for its own Scratch view instead of a third
  copy, exactly because that specific piece of UI genuinely is
  identical between the two modes (see `EVENTS-HQ-RENDER-
  UNIFICATION-PLAN.md`).

### 4.3 One real, honest piece of debt: the Add-Command picker

The picker overlay (`evhq_draw_picker_overlay()`,
`evhq_dispatch_picker_onclick()`, `evhq_handle_key()`'s own
`g_evhq_picker_open` branch — all `evhq_`-prefixed) is used by BOTH
events-hq directly AND db-hq's Common Events editor
(`g_dbhq_ce_editing && g_evhq_picker_open`, checked in
`dbhq_handle_click()`/`dbhq_draw_content()`). That sharing is real and
correct — but the picker itself is hand-drawn C
(`evhq_draw_picker_overlay()` computes row rects with manual `ty +=
row_spacing` math), not a real `.chtpm`-declared, generically-laid-out
element the way the rest of this house's UI works. `picker.chtpm`
exists but only supplies a default width/height — it doesn't declare
the real rows.

This is the piece flagged live tonight ("its working now but its not
supposed to be hardcoded... it should be a new created chtpm element
or something, like how +- works") and is real, acknowledged, **not
yet fixed** — a genuine follow-up task, not something this audit is
pretending is fine. Converting it into a real generic element type
(parsed from `.chtpm`, laid out by `css_layout_pass()` like everything
else, scrollable via the already-shared `generic_scroll_layout_pass()`)
is the correct direction; it just hasn't been done yet.

---

## 5. The input-relay layer (also shared, also touched tonight)

`history_path()`/`history_dir()`/`poll_agent_history()`/
`dispatch_relay_code()`/`history_unregister()` are the one real,
mode-agnostic input mechanism every mode's real X11 event handling AND
every external relay writer goes through. As of tonight these are
per-PID (`#.desktop/<mode>_history/<pid>.txt`), not per-mode-name flat
files — see `HQ-WINDOW-MAP-AND-AGENT-INPUT.md` §3 for the real
incident that forced this and the discovery mechanism
(`nav_master_current.txt` + `nav_tab/<pid>`) for finding a specific
window's PID.

---

## 6. If you're auditing this again later

Real, cheap checks that would catch a regression of what this audit
confirmed clean:

```sh
# Confirms the draw-loop collapse is still holding (should each be exactly 1)
grep -c "^static void draw_elem\b" khtpm_core_render.c khtpm_draw_core.c
grep -c "^static void render_tree\b" khtpm_core_render.c khtpm_draw_core.c

# Lists every per-mode function family and its current size, to catch
# unexpected growth in one mode relative to the others
grep -oE "^static [A-Za-z_ *]+ [a-z_]+\(" khtpm_core_render.c \
  | grep -oE "[a-z_]+\($" | sed 's/($//' | sed -E 's/^([a-z]+)_.*/\1/' \
  | sort | uniq -c | sort -rn

# Finds any NEW hand-copied draw/render pair before it drifts the way
# evhq_draw_elem() once did
grep -n "^static void [a-z]*_draw_elem\|^static void [a-z]*_render_tree" \
  khtpm_core_render.c
```
