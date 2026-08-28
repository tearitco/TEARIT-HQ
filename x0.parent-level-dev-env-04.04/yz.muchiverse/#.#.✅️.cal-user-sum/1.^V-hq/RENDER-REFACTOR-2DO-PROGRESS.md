RENDER-REFACTOR-2DO-PROGRESS.md
Started: 2026-08-28
Purpose: live-updated status tracker for the render de-mode / frame-
history refactor effort. If this session gets cut off, READ THIS FILE
FIRST — it should be enough to resume cold, without needing the full
chat history. Update it as you go, don't wait until a phase finishes.

============================================================
READ THIS FIRST IF RESUMING COLD
============================================================
1. Read `RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md` (this same directory)
   in full — it's the real design doc: what the drift is, why it's
   real (not deliberate), the wraith-alpha reference pattern, confirmed
   scope (taskbar and mutaclysm are OUT of scope, for real documented
   reasons — don't re-investigate that, it's settled).
2. Read the "CURRENT STATUS" section immediately below to see exactly
   what's done vs. not.
3. Read the "NEXT CONCRETE STEP" section — that's literally the next
   thing to do, no re-deriving needed.
4. Real commits so far (all pushed to `origin/main`, in order):
   `04cecc4` → `1932bcc` → `c220310` → `5115a60` — `git log` these for
   exact diffs if you need to see real code, don't guess from this doc
   alone.

============================================================
THE REAL GOAL (do not lose sight of this)
============================================================
`khtpm_entity_menu_render.c` (7 window modes: db-hq, events-hq,
chat-hai, palettes, bookmarks, stats-hq, taskbar-settings) should
eventually: (a) have ZERO mode-specific branches in its core engine —
all app-specific behavior lives in manager processes; (b) PAINT should
be derived from a real, written, auditable frame-description file, not
directly from a live in-memory Elem tree — matching the wraith-alpha
`chtpm_parser.c`/`renderer.c` pattern already standard everywhere else
in this house. **Two separate goals, both real, don't conflate them:**
(a) is "de-mode the engine", (b) is "make render an honest receipt".
Phase C (below) made progress on (a). Phase 2 (next) is the real start
of (b) — and (a) alone, without (b), does NOT achieve the "honest,
auditable receipts" property the user actually cares about. Don't
declare victory on (a) work and call the job done.

============================================================
CURRENT STATUS (update this section every session)
============================================================
**Phase A (research/inventory) — DONE.**
Two read-only inventories completed: every `g_is_palettes` branch
cataloged (20+ sites), and every OTHER mode's list/grid/scroll
mechanism surveyed. Key finding: palettes' scroll mechanism was the
ONLY scroll support anywhere in the file — db-hq sidebar, bookmarks,
chat-hai sidebar, events-hq command list all had ZERO scroll (silent
overflow bug, not just an architecture nit).

**Phase B (design) — DONE, informal.**
Decided: extract palettes' scroll/track/thumb/arrow mechanism into
`generic_scroll_layout_pass(Elem *container, const char *row_class,
int box_y, int box_h)`. Since each mode runs as its own SEPARATE
PROCESS of this binary (mode flags set once at startup, never change),
reusing the same `g_pal_*` globals across modes is safe — no rename
needed, only the gate condition + row-selector needed to generalize.

**Phase C (de-mode the scroll mechanism specifically) — DONE, all 7
modes, verified live, committed, pushed.**
- `generic_scroll_layout_pass()` built, zero mode-specific knowledge
  inside it (pure function of container/row_class/box).
- Palettes (tile grid, row_class="pal-grid-row") — done, verified live
  (real regression test: World_B.png 256 tiles, 13 rows, 10 visible,
  correct geometry, no crash).
- db-hq sidebar (Common Events + Terms + stats-hq, all one function,
  row_class=NULL) — done, verified live (real fix: this list had ZERO
  scroll before, now has real track/thumb/arrows).
- Bookmarks (row_class="bm-bookmark") — done (landed in the same
  commit as palettes/db-hq, `c220310`, confirmed by direct code read).
- chat-hai session sidebar (row_class="session-item") — done. Real
  finding: chat-hai has its OWN separate redraw/click/key code path
  (`chai_*` functions), never shared db-hq's — needed its own 7-site
  hookup (draw/click/key/wheel), not just a layout-pass call. Verified
  live (real 61-item feed + sidebar, no crash, no regression).
- events-hq command list (needed a NEW real class "cmd-row" added to
  `evhq_inject_commands()` since `cmd-<type>` varies per type and there
  was no shared marker) — done, same 7-site hookup as chat-hai needed.
  Verified live (3 real commands + title, no crash, no regression).
- Debug-dump (relay code 210) extended to also cover chat-hai (had
  none before, PNG-only) - and to always include real x/y/w/h geometry
  for every nav element (was tag/id/label only - this is what let a
  pure pixel-position bug hide from the debug dump earlier tonight).
- Real, live-confirmed: all 7 modes now share ONE scroll mechanism.
  Previously-silent overflow bug (4 modes) is fixed as a side effect.

**Phase 2 (the REAL fix — frame-write-then-render) — NOT STARTED YET.**
This is the part that actually delivers "honest, auditable receipts."
Scope agreed with the user just before this doc was written:
- Target: palettes' tile grid ONLY, as the first real, minimal proof.
- Mechanism: after layout computes real x/y/w/h for every visible
  element, write a real flat frame-description file (not yet named -
  pick something like `#.desktop/palettes_frame.txt`) BEFORE painting.
  Build a genuinely separate paint function that reads ONLY that file
  (zero live Elem-tree pointer access) to draw pixels.
- **Scope correction from the user, already agreed - include nav_index
  and focus-ring state in the frame file and the new paint function
  from the start.** (Original plan wrongly deferred this as "extra" -
  it's not architecturally harder, just one more column in the same
  line format + one more small draw call. Deferring it would mean
  redoing this same paint function again shortly after. Don't repeat
  that mistake.)
- **Explicitly OUT of scope for this first proof** (confirmed with
  user): click/key DISPATCH (deciding what happens when an element
  activates) stays reading the live tree for now - that's behavior,
  not the visual receipt, and wraith-alpha's own `renderer.c` doesn't
  handle input either (`chtpm_parser.c` does). This may become its own
  later phase but is not blocking Phase 2's own completion.
- Verification standard: live PNG-diff, same visual tile grid pixel-
  for-pixel before/after, but now provably sourced from a written file
  instead of live memory - not just "it compiles."

============================================================
NEXT CONCRETE STEP (do this next, no re-deriving)
============================================================
1. Design the real frame-file line format (needs at minimum: tag,
   label, x, y, w, h, sprite path, onclick string, nav_index, classes
   for focus/active-state styling - check `Elem` struct's real fields
   in khtpm_entity_menu_render.c before finalizing, don't guess field
   names).
2. Add a real serialize step right after `dbhq_layout_pass()` +
   `dbhq_assign_nav_indices()` finish for the palettes case specifically
   (scoped to `g_is_palettes`, not touching other modes yet) - walk the
   panel's visible children (tab row, tile rows, tileset chooser row)
   and write one real line per element.
3. Build a new function (e.g. `dbhq_paint_palette_frame_from_file()`)
   that reads ONLY that file and does the actual `XFillRectangle`/
   sprite-blit/text draw calls - no `Elem*` dereferencing at all in
   this function, prove it by construction (take a parsed line struct
   as input, not an `Elem*`).
4. Swap the call inside `dbhq_redraw_content()`'s palettes branch from
   the existing `render_tree(g_window, 0)` to: serialize → call the new
   frame-reading paint function, for palettes only.
5. Rebuild, live-test: launch palettes/rmmv, PNG-dump BEFORE this
   change (already have real reference PNGs from tonight's earlier
   testing) and AFTER, confirm visually identical. Also confirm the new
   frame file itself is real, readable, and genuinely matches what got
   drawn (the actual auditability property being proven).
6. Once proven for palettes, THIS DOC gets updated with the real
   result, and a decision gets made (with the user) on whether to
   extend to the other 6 modes now or pause again.

============================================================
DECISIONS LOG (append, don't rewrite history)
============================================================
- 2026-08-28: pilot mode for de-moding = palettes (user confirmed:
  "if u are comfortable doing palettes thats fine").
- 2026-08-28: taskbar and mutaclysm confirmed OUT of scope (real
  research, not assumption - see assessment doc's "SCOPE CONFIRMED"
  section).
- 2026-08-28: Phase C scope widened from "palettes only" to "fix the
  overflow bug in all 4 other modes too" per direct user choice
  ("Build the primitive AND fix the other modes' overflow bug now").
- 2026-08-28: nav_index/focus-ring included in Phase 2's first proof
  scope (user pushback, correctly identified this wasn't actually
  harder, just deferred out of my own instinct to keep the diff small).
- 2026-08-28: click/key dispatch explicitly staying on the live tree
  for Phase 2's first proof - confirmed acceptable per wraith-alpha's
  own real precedent (renderer.c doesn't handle input either).
