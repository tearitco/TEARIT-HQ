# khtpm incremental reparse (real tree diff/patch) — design doc

**Status: LIVE, MERGED TO `claude` (2026-09-11).** Built on a
short-lived branch (`khtpm-incremental-reparse`, off `claude`) per
direct instruction ("lets commit them on a new branch tho, to be
safe") while confidence was still building; fast-forward merged back
into `claude` and the branch deleted once the user confirmed real-
hardware testing passed for the dock/taskbar, chat-hai, and network-
browser (including the address-bar content=/label= fix and the
history delete/clear-all feature built on top of it - see this doc's
own step 3 below and `04-bugs/BUG-LOG.md`'s "network-browser address
bar" entry, now closed). `incremental_reparse=1` is live in
`#.desktop/hq_ui.pdl` - every window in the house currently runs the
new path. Remaining, NOT yet done: individually exercising every
OTHER window type (open-hai, text-edit-hq, db-hq, events-hq, etc. -
they're all running the new path by virtue of the shared flag, but
nobody's specifically driven each one yet) and the final cleanup step
(§ rollout step 7: remove the flag/old path, delete the now-dead
`kh_text_areas_reload()`/`kh_cli_io_reload()`/`kh_find_input_by_key()`
bolt-ons) once that broader confidence exists.

Earlier status, preserved for the history: §0 (khtpm_reparse_diff.c) done,
headless-tested green (commit `bb3f24fe`). Wired into
`reparse_chtpm_if_changed()` behind `incremental_reparse=1` in
`hq_ui.pdl`, OFF by default (commit `d13c92ef`). Rollout step 1 (dock/
taskbar) DONE and PASSING, 2026-09-11: flipped the flag live (only the
dock running at the time, so no other window was affected), restarted
the strip, drove it via the real relay (not xdotool) - 6 real reparses
observed in `kh_focus_debug.log`, including one genuine structural
change (the "toys" dropdown opening: `removed=20`, correctly diffed
and patched), all `ok`, zero crash, zero X error attributable to this
change, dock re-dumped via PNG afterward and renders correctly (nav
badges intact, clock advanced, no visual corruption). Rollout step 2
(chat-hai) DONE and PASSING: launched during an ACTIVE, running
multi-persona autonomous conversation (a new message appended every
~6s - real, frequent reparse activity, not idle); typed into the
composer, waited through 2 genuine reparses (one a real structural
change, the new message being appended), content and armed state both
survived; 3 real Backspace keystrokes all registered correctly - the
exact symptom class this whole investigation started from.

**Rollout step 3 (network-browser, the actual motivating case) DONE
and PASSING, confirmed on REAL hardware (not just relay) - the user
tested directly**: "yes its finally fixed." Two real bugs surfaced and
were fixed along the way, both now folded into `CENTROID_GOLD_STD.md`
item 9 as house-wide lessons: (a) the address bar losing focus/
backspacing wrongly - the actual bug this whole design exists to fix -
gone; (b) a real, separate `content=` vs `label=` confusion (briefly
regressed into a visible double-echo before the real fix landed) -
`content=` seeds `input_buffer` (what typing/Backspace edit), `label=`
is always just display text/a prefix, never the same thing. A related,
unrelated-to-this-design crash
(`parse_element()` not null-checking `elem_new()`'s return when a
large loaded page exhausted `MAX_ELEMS`) was also found and fixed
during this same testing pass.

Original design notes below, superseded in status only - the design
itself held up unchanged through implementation and this first live
test. Direct instruction 2026-09-11,
after the network-browser address-bar focus bug got "fixed" 3 times
(each fix real, each verified via the house's own relay-driven test
methodology, each reported unchanged on real hardware): "how do we fix
this once and for all. im willing to refactor" → chose the full
structural fix over a narrower same-shape-reuse patch. Write the
design first, in full, before touching shared code — this is
house-wide, shared, blast-radius-wide code (`parse_chtpm()`,
`reparse_chtpm_if_changed()`), the exact class of change the
`override_redirect` incident (`03-pitfalls/X11-AND-SESSION-PITFALLS.md`
2026-09-04) already burned this house once for skipping this step on.

## The problem, precisely

`reparse_chtpm_if_changed()` (khtpm_core_render.c ~line 1740) does not
patch the page. On every real reparse trigger (a manager rewriting its
`.chtpm`/`vars=` file, gated by the DIAMOND-style content hash/marker
this house already uses correctly) it:

```c
g_n_elems = 0;
Elem *new_window = parse_chtpm(g_chtpm_path);
```

`g_pool[MAX_ELEMS]` (a flat, static `Elem g_pool[1024]` array,
`elem_new()` just bumps `g_n_elems` and returns `&g_pool[g_n_elems++]`)
is walked from index 0 again, EVERY element in the whole window gets a
brand new `Elem*` at a brand new pool slot, regardless of whether its
content actually changed. Any code holding a pointer into the OLD tree
(`g_default_input_elem`, `g_default_active_scope_root`,
`g_dbhq_active_scope_root`, `g_default_minimize_elem`/`_fullscreen_elem`
being the sole exceptions since those are separate statics, not pool
members) is now dangling.

The house's fix pattern so far has been reactive, per-consumer,
"capture a stable key before the rebuild, go find the new Elem by that
key afterward, copy state across": `kh_text_areas_reload()`
(2026-09-08), `kh_cli_io_reload()`/`kh_find_input_by_key()`
(2026-09-11, this same investigation). This works, but only as well as
"find by key in the freshly rebuilt tree" can be trusted, and every
consumer needs its own bolt-on. It is fundamentally a workaround for
the tree being destroyed, not a fix for the destruction itself.

**Real evidence the workaround pattern has a ceiling**: after both the
cli_io reparse-grab fix AND the addr_label live-echo fix landed
(verified via the relay test — click-arm, type, wait 3+ real manager
ticks, backspace, all correct), the user reports network-browser's
address bar is "the exact sae bg as we just fixed the last 3 times. no
difference" on real hardware. See `04-bugs/BUG-LOG.md`'s "network-
browser address bar" open entry for the full incident trail. Whether
this is a genuine residual gap in the find-by-key approach, a real
X11-level issue the relay test can't reproduce (see
[[relay-testing-may-mask-real-focus-bugs]] and the already-documented
override_redirect/xdotool masking precedent), or something else
entirely is NOT YET PROVEN - this design doc is the "fix the actual
foundation instead of guessing again" response regardless of which it
turns out to be, since the find-by-key pattern is fragile either way.

## What "once and for all" actually requires

An element that reparse doesn't need to touch should never be
destroyed. If `g_default_input_elem` (or any other cross-reparse
pointer) simply never becomes invalid in the first place, there is no
"go find it again" step to get wrong, race on, or need a bolt-on fix
for the next time some other element type needs the same guarantee
(this has already happened twice: text_area 2026-09-08, cli_io
2026-09-11 - `<grid>` and any future stateful element will hit the
exact same gap a third time if the foundation doesn't change).

## Proposed design

### 0. Modularity: a new file in `_shared-lib/`, not more code in `khtpm_core_render.c`

Direct instruction 2026-09-11: "is there a way to do this more
modularly, using ops/ functions etc so its not so volatile?" - real,
already-established house convention answers this directly, no new
pattern needed: `&.widgits/_shared-lib/` already holds
`khtpm_render_core.c`/`khtpm_draw_core.c`/`khtpm_css_parser.c` as
separate, focused, TEXT-INCLUDED files (`#include
"khtpm_render_core.c"`, resolved against the one canonical copy via a
build-time `-I` flag - see `SHARED-SOURCE-COMPILE-IN-PLACE.md`, landed
2026-09-09). No in-house `.h`, no `.so`/dynamic linking (house-wide
rule, `khtpm-house-standards` skill) - just a genuinely separate `.c`
file, one real translation unit, compiled alongside the others.

This whole design lands as a **new file**, `&.widgits/_shared-lib/
khtpm_reparse_diff.c`, `#include`d from `khtpm_core_render.c` the same
way the existing three already are - NOT inline code added to the
already-huge `khtpm_core_render.c`. Concretely:

- `khtpm_reparse_diff.c` owns: the diff/patch algorithm (§2), the
  keyed-match logic, the free-list (§3). It depends only on the `Elem`
  type (already shared via `khtpm_render_core.c`) - no `dpy`/X11, no
  `g_default_input_elem` or any other `khtpm_core_render.c` global
  referenced directly. Its real entry point is one function,
  something like `int kh_reparse_diff_patch(Elem *old_root, Elem
  *new_root, Elem **removed_out, int *removed_n)` - takes two trees,
  patches `old_root`'s subtree in place, returns what got removed so
  the CALLER (khtpm_core_render.c, which owns `g_default_input_elem`
  and every other cross-reparse pointer) decides what to do about a
  removed element being the armed one. The diff engine itself should
  not need to know cli_io/text_area/grid are special - preserving
  `input_buffer`/`cursor`/etc across a match is a generic "these
  fields are runtime state, not template output" rule the engine
  applies uniformly (a real, small, explicit list of field names/
  offsets it never overwrites on a match, documented in the file
  itself), not per-tag logic.
- This split is what makes §7's headless test harness realistic: a
  standalone test driver can `#include "khtpm_reparse_diff.c"` alone
  (plus `khtpm_render_core.c` for the `Elem` type it depends on),
  build two small `Elem` trees by hand or from fixture `.chtpm` files
  via the existing `parse_chtpm()`, call `kh_reparse_diff_patch()`,
  and assert on the result - zero X11, zero live window, zero
  `khtpm_core_render.c` globals involved. Write this test file
  alongside the implementation, not after.
- Build-script change: `build_core_render.sh` (and any other consumer
  that ends up needing this - the taskbar's own build script if the
  dock rollout step needs it compiled in) adds `khtpm_reparse_diff.c`
  to its `-I`'d text-include set, same one-line-per-consumer pattern
  the 2026-09-09 migration already established for the other three
  files - no new build mechanism to invent.

### 1. Two-pool parse: old (live) + new (candidate)

Reparse currently overwrites `g_pool[]` in place. Instead: parse the
new document into a SEPARATE scratch pool (`g_pool_next[MAX_ELEMS]`,
same size/shape as `g_pool`), leaving the live tree (`g_pool`,
everything every live pointer refers to) completely untouched during
parsing. Only after a successful parse does the diff/patch step (§2)
run, and only elements that step decides actually need it get written
into.

Memory cost: doubles `g_pool`'s static footprint. `Elem` is large
(struct fields total well over 8KB each per the current field list -
`label[256]` + `onclick[1536]` + `backspace_action[1536]` +
`text_area_buffer[4096]` + `children[MAX_CHILDREN]` pointers + several
smaller fields) × `MAX_ELEMS` (1024) - already multiple MB per
process today; a second scratch pool roughly doubles that. Worth
confirming this is fine (it should be - these are per-process,
not shared/mmap'd, and this house runs many khtpm processes
concurrently already) before committing, not assumed.

### 2. The diff/patch pass

Walk the OLD tree (`g_window`, rooted in `g_pool`) and the NEW tree
(rooted in `g_pool_next`) together, top-down, matching each new node
against an old node using a real key:

- **Primary key**: `target_id` if non-empty, else `id` if non-empty -
  same key every existing bolt-on fix (`kh_find_input_by_key()` etc.)
  already uses, for the exact same reason (stable, author-controlled,
  survives a `<repeat>` re-expansion as long as the template keys each
  row, e.g. `id="hist${h.#}"`).
- **Fallback key (no id/target_id)**: `(parent-key, tag, sibling-index
  among same-tag children)`. Covers the common case of unkeyed
  `<text>`/`<item>` rows inside a `<repeat>` - matches by position
  when nothing else identifies a node, same as most virtual-DOM
  differs' fallback. A row insertion/removal in the MIDDLE of an
  unkeyed list will misattribute state to the wrong row past that
  point (a real, known limitation of positional fallback keys, not
  unique to this design) - acceptable because every stateful element
  this design actually cares about preserving (cli_io, text_area,
  grid) is expected to always carry a real `target_id`/`id` in
  practice; document this as a REQUIREMENT (see §5) rather than
  silently tolerate a keyless stateful field.
- **Match found, same tag**: this is the "same element, content maybe
  changed" case. Copy every NEW field value into the OLD Elem* IN
  PLACE - `tag`/`classes`/`label`/`onclick`/`sprite`/`bg`/`rows`/etc -
  EXCEPT the fields that represent live user/runtime state, which are
  preserved from the OLD element rather than overwritten:
  `input_buffer`, `cursor`, `sel_anchor`, `text_area_buffer`,
  `grid_cur_row`/`grid_cur_col`/`grid_edit_mode`/`grid_jump_buffer`/
  `grid_cell_buffer`. (`content="${...}"`/`label="${...}"` seeding a
  cli_io/text_area's initial value only matters the FIRST time a key
  is ever seen - once live state exists for that key, the template's
  own attribute is provenance for the INITIAL value only, matching
  `kh_text_areas_reload()`'s already-established "prefer the saved
  state over content=" precedent, generalized.) The OLD `Elem*`
  itself never moves, never gets a new address - `g_default_input_elem`
  and every other cross-reparse pointer is trivially still valid, with
  zero find-by-key step required.
- **Match found, different tag**: treat as remove-old + add-new (a
  `<cli_io>` template that got replaced by an `<item>` under the same
  id is a real structural change, not a content update - don't try to
  reuse the slot).
- **No match (new key)**: allocate a fresh `Elem` (from a real
  free-list now, see §3) and copy the new node's fields in full - a
  genuinely new element, no old state to preserve.
- **Old node with no match in the new tree**: this element was
  removed. If it happens to be `g_default_input_elem` (or any other
  live cross-reparse pointer), that pointer must be explicitly
  cleared/disarmed HERE, in the diff pass itself - this is the one
  place "the field really is gone" should still be detected and
  handled (matching the existing `kh_find_input_by_key()` "not found ->
  ungrab" branch, but as a NATURAL byproduct of the diff, not a
  separate re-lookup). Free the old node's slot back to the free-list.
- **Children arrays**: diff each parent's children list by key (a
  standard keyed-list diff - match, reorder pointers if order changed
  without content changing, recurse into matched pairs, add/remove as
  above). `n_children`/`children[]` order matters for draw order and
  `assign_nav_and_layout()`'s own top-to-bottom nav numbering - a
  reorder without a real diff (just concatenating unmatched-then-
  matched) would corrupt both.

### 3. `g_pool` needs a real free-list

Today `g_pool[]` is append-only, "reused in place from index 0" only
on a FULL reparse (the old comment: "elem_new()'s own g_pool[MAX_ELEMS]
never recycles slots" - within one parse, not across). Once diff/patch
means most elements are NEVER reallocated across a reparse, the pool
stops naturally "resetting" the way a full rebuild used to. Elements
that ARE removed (§2, no-match-in-new-tree) must return their slot to
a real free-list (`int g_pool_free[MAX_ELEMS]; int g_pool_free_n;`) so
`elem_new()` (called for genuinely new elements) can reuse them instead
of monotonically growing until `MAX_ELEMS` is exhausted - a real risk
today for a long-lived, growing window (network-browser's own history/
bookmarks/content are exactly this shape) that a full-rebuild-per-
reparse coincidentally avoided (every reparse reset `g_n_elems = 0`,
silently masking any leak). This is a real, new invariant to get right
- a bug here (double-free a slot two live pointers still reference, or
never free a genuinely-removed one) is worse than the bug being fixed.

### 4. What does NOT need to change

- `assign_nav_and_layout()`, `css_layout_pass()`, `draw_elem()`,
  `redraw()`'s own serialize/paint round trip - none of these care
  HOW the tree got to its current shape, only that it's internally
  consistent at the moment they run. Layout is already idempotent
  (recomputed fresh every call) and should stay that way - the diff
  only changes elements are STABLE (SAME pointer identity), not that
  their `x/y/w/h`/`style` stay stale (those still recompute every
  layout pass exactly as today).
- `kh_text_areas_reload()`/`kh_cli_io_reload()`/
  `kh_find_input_by_key()` - become dead code once the diff pass makes
  them unnecessary (state is preserved in place, never lost, never
  needs re-finding), but should be DELETED only after the diff/patch
  path is proven correct end to end, not stripped preemptively.

### 5. New authoring requirement this design imposes

Any `<cli_io>`/`<text_area>`/`<grid>` (any element whose runtime state
must survive a reparse) MUST carry a real `id` or `target_id` -
positional fallback keys are not reliable enough for stateful
elements (§2). This is already true in practice for every real
consumer in the house today (grep confirms every existing cli_io/
text_area/grid in the codebase already sets one) - documenting it as a
hard requirement, not a new constraint.

## Rollout plan (mandatory order, per the override_redirect lesson)

This is shared code every khtpm window depends on. **Test the dock/
taskbar FIRST, before any other window**, with an explicit dedicated
check that its dropdowns/menus still work - inverted from the
override_redirect incident's own mistake (tested the target window
first via xdotool, got false confidence, broke the dock without
anyone noticing until the user reported it live). Concretely:

1. Write `&.widgits/_shared-lib/khtpm_reparse_diff.c` (§0) -
   `g_pool_next`/diff/patch/free-list logic, self-contained. Not
   wired into `khtpm_core_render.c` yet.
2. Write its standalone test driver alongside it (§0's own test-file
   point) - a small program that `#include`s only `khtpm_reparse_diff.c`
   + `khtpm_render_core.c`, builds/parses fixture `.chtpm` trees,
   mutates them in controlled ways (content-only change, add a row,
   remove a row, reorder rows, change a keyed field's tag), and
   asserts the diff/patch result - real Elem-identity checks (same
   pointer survives a content-only change), not just visual output.
   Fully headless, zero X11, zero live window. Get this green before
   touching `khtpm_core_render.c` at all.
3. Wire `khtpm_reparse_diff.c` into `khtpm_core_render.c`'s
   `reparse_chtpm_if_changed()` behind a compile-time or runtime flag,
   OFF by default - the existing full-rebuild path stays the real,
   live behavior until the new path is proven end to end.
4. Flip the flag ON for the dock/taskbar specifically first. Live-test
   every dropdown/menu interaction a human would do. Get explicit
   user confirmation before proceeding.
5. Flip it on for one simple window (text-edit-hq or chat-hai) next.
   Verify text_area/cli_io state survives a real reparse burst, same
   as today's relay tests, PLUS get real-hardware confirmation this
   time (see [[relay-testing-may-mask-real-focus-bugs]] - relay-only
   verification is not sufficient evidence any more for this bug
   class).
6. Only then flip it on for network-browser (the actual motivating
   case) and confirm the address bar bug is genuinely gone on real
   hardware.
7. Once every window is confirmed working on the new path, remove the
   flag and the old full-rebuild code path, and delete the now-dead
   `kh_text_areas_reload()`/`kh_cli_io_reload()`/
   `kh_find_input_by_key()` bolt-ons.

## Open questions / risks, not yet resolved

- **Memory**: doubling `g_pool`'s static size (§1) - needs a real
  number, not just "should be fine." `sizeof(Elem)` × `MAX_ELEMS` × 2,
  measured, not estimated, before committing.
- **`g_nav[]`/`g_n_nav`**: currently rebuilt fresh every
  `assign_nav_and_layout()` call regardless of reparse - unaffected by
  this design (nav numbering is a layout-time concern, not a parse-
  time one), but worth an explicit test since nav badges are exactly
  the kind of thing a subtle Elem-identity bug would show up in first
  (mis-numbered or duplicate badges).
- **Does anything rely on the old "full reparse resets everything"
  behavior as a FEATURE**, not just tolerate it as a side effect? (e.g.
  a stale scope/selection that today gets silently cleared by the
  wipe, which some window might be depending on without documenting
  it.) Needs an audit of every `g_default_*`/`g_dbhq_*` cross-reparse
  global, not just the ones already known to need preservation.
- **Positional fallback key stability** (§2) for a `<repeat>` whose
  bound list can shrink from the FRONT (not just append/remove from
  the tail) - network-browser's own history list is newest-first, so
  a new visit shifts every existing row's index by one. Positional
  keys would misattribute state across ALL rows on every new visit in
  this specific shape. None of those rows carry meaningful runtime
  state today (they're plain `<item>`s, not cli_io/text_area) so this
  is currently harmless in practice, but should be verified explicitly
  for network-browser's own templates specifically, not assumed safe
  by the general "stateful elements always have real keys" rule (§5)
  alone - a future template change could introduce a keyless stateful
  row in a front-shifting list without anyone noticing the interaction
  with this design.

## Related

- [[relay-testing-may-mask-real-focus-bugs]] (memory) - why the fixes
  attempted before this design doc all appeared to work but reportedly
  didn't on real hardware.
- `04-bugs/BUG-LOG.md` - "network-browser address bar" open entry,
  the full incident trail motivating this design.
- `03-pitfalls/X11-AND-SESSION-PITFALLS.md` 2026-09-04 entry - the
  override_redirect incident this design's rollout plan is deliberately
  modeled after (test the dock first, not last).
- `02-architecture/reference/TPMOS-DIAMOND-render-chain.md` - the
  house's own "don't do more work than the content actually changed"
  philosophy this design applies one layer deeper (element identity,
  not just redraw timing).
- `SHARED-SOURCE-COMPILE-IN-PLACE.md` - the existing modular-`.c`-
  files-in-`_shared-lib/`, text-included-via-`-I` convention §0 of
  this design follows for `khtpm_reparse_diff.c`, rather than adding
  more code to `khtpm_core_render.c` directly.
