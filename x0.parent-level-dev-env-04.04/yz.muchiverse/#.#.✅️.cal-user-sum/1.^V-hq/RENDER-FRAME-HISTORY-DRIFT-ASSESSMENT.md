RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md
Date: 2026-08-28
Status: ASSESSMENT ONLY — no refactor code written yet, per direct
instruction ("we should probably document this refactor and the size
of its impact before we do any more coding"). This doc exists to size
the problem and the fix, not to decide the fix on its own — read and
confirm scope/approach before any implementation starts.

============================================================
WHAT PROMPTED THIS
============================================================
While chasing a live visual bug in the RPG Maker tile picker (a
scrollbar arrow that appeared duplicated in a screenshot but wasn't
duplicated in the Elem tree), the debug-dump tool (relay code 210,
`dbhq_dump_debug_state()`) turned out not to print element geometry
(x/y/w/h) — only tag/id/label. That gap got fixed (small, already
done, see khtpm_entity_menu_render.c's dbhq_dump_debug_state()). But
the user's follow-up question exposed a much bigger, real, unintended
architectural gap: **is render actually derived from the frame-history
file, the way this house's own wraith-alpha family does it?**

Direct answer, confirmed by reading the code: **no.** This is real,
unintentional drift, not a deliberate different design for a different
domain (my own first answer to the user was wrong on this point and
they corrected it directly — the record here reflects the corrected
understanding).

============================================================
THE REAL HOUSE STANDARD (wraith-alpha family) — verified by reading
source, not assumed
============================================================
Reference instance read in full: `101.ledger-player-npc-simple+3/
system/chtpm_parser.c` (3032 lines) + `.../system/renderer.c` (115
lines). The same `chtpm_parser`/`renderer` pair (some renamed
`chtpm_parser_pal.c`) is duplicated across roughly 60+ app instances in
this house — this is a house-WIDE convention, not a one-off.

**The real pattern, confirmed line-by-line:**
1. `chtpm_parser.c` owns all real game/app state and logic. It writes
   `pieces/display/current_frame.txt` (+ `current_layout.txt`) via
   `compose_frame()` — but **only when something changed**, not every
   tick. The gate (`chtpm_parser.c:3029`): `if (dirty || clear_nav_on_
   next) { compose_frame(); dirty = 0; }`. `dirty` is set only by real
   state-diff checks (a marker file growing, a process exiting) — never
   unconditionally per loop iteration. A load-bearing comment at lines
   2902-2916 codifies this as policy: *"compose_frame() ONLY fires when
   frame_changed.txt grows... The marker file IS the throttle... DO NOT
   set dirty=1 directly. That caused triple-rendering."*
2. `frame_changed.txt` is a real, content-agnostic dirty marker.
   Producers (keypress/click handlers) append one tag byte purely to
   grow the file's SIZE — the byte content itself is never read. This
   is the "only updates on a marker file" mechanism the user described
   — confirmed real, confirmed cheap (a `stat()`-only size check, no
   parse).
3. `renderer.c` (`render_frame()`, lines 37-66) draws **exclusively**
   from `current_frame.txt` — one `fopen`/`fread`, nothing else. If
   `current_frame.txt` were deleted, the renderer has ZERO fallback
   source of drawable state (`fopen` fails, function returns early).
   The renderer polls a `renderer_pulse.txt` marker every 33ms purely
   to decide WHEN to re-read the frame file, not to get content from.

**Why this matters (the user's real point, not just perf):** the frame
file IS the receipt. Anyone auditing "what did this program actually
show the user at time T" can read `current_frame.txt` (or its
appended-history sibling) and get the literal, complete truth — not a
best-effort summary written after the fact by a separate, possibly-
incomplete logging call. This is a HONESTY/AUDITABILITY property, not
a performance optimization that happens to also help perf.

============================================================
WHAT khtpm_entity_menu_render.c ACTUALLY DOES TODAY — confirmed by
reading the code, not assumed
============================================================
File: `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c`,
**7742 lines**, one compiled binary covering 7 window modes in a
single process (db-hq, events-hq, chat-hai, palettes, bookmarks,
stats-hq, taskbar-settings/swatch-picker) — see `khtpm-merge-how2.md`
for the real, deliberate, DIFFERENT decision this merge represents
(collapsing 5 separate BINARIES into one process, for real stated
reasons). That merge decision is not in question here — it's a
separate, already-settled architectural choice. What IS in question:
within this one process, is drawing derived from an auditable frame
file, or from the live in-memory Elem tree directly?

**Confirmed: drawing reads the live Elem tree directly, in the same
process, same memory — there is no separate frame-description file
that `draw_elem()`/`render_tree()` read from.** The tree (Elem structs:
tag, classes, computed layout x/y/w/h, sprite cache handles, CSS style,
parent/children pointers) is built once at window-open by parsing the
`.chtpm` XML, then MUTATED IN PLACE on every interaction (tab click,
scroll, manager state-file reload) and painted directly from that live,
mutable structure via X11 calls (`XFillRectangle`/`XftDrawString`/etc)
straight into an offscreen Pixmap, then blitted to the real window.

**The "frame history" files that already exist
(`db_hq_frame_history.txt`, `events_hq_frame_history.txt`,
`chat_hai_frame_history.txt`) are NOT what render reads from — they
are appended AFTER drawing, as a summary log.** Confirmed directly:
`dbhq_append_frame_history()` is called as the LAST line of
`dbhq_redraw_content()` (khtpm_entity_menu_render.c), strictly after
`render_tree()`/`draw_elem()` have already painted pixels. It writes a
handful of scalar fields (`seq`, `focus_nav`, `tab`, `selected_event`)
— not a full frame description, and specifically NOT geometry (until
today's small dump-tool fix, geometry wasn't recorded ANYWHERE outside
the live, ephemeral Elem tree).

**This is the real drift:** two different consumers of "what does this
window currently show" — the actual pixel-draw code, and the audit/
debug-dump tooling — read from two DIFFERENT sources (live tree vs. a
lagging post-hoc summary file) that happen to usually agree, but are
not guaranteed to, and aren't provably in sync the way wraith-alpha's
single-source design guarantees by construction. Tonight's bug (a
geometry question the debug dump couldn't answer at all) is a direct,
concrete symptom of this gap — not a one-off, a structural one.

============================================================
SIZE OF THE GAP (scope, for planning purposes)
============================================================
- Reference wraith-alpha pair: ~3147 lines total, ONE window mode.
- Target file: khtpm_entity_menu_render.c, **7742 lines, 7 window
  modes in one binary**, actively rebuilt multiple times per session
  during normal work (this session alone rebuilt it a dozen+ times).
- Each of the 7 modes has its own real, already-divergent per-mode
  state (db-hq's tabs/sidebar, events-hq's drag+modal overlay,
  chat-hai's session ledger, palettes' scroll/grid, bookmarks' list,
  etc) — a real marker-driven refactor would need PER-MODE dirty-
  tracking, not one global `dirty` bit like the single-mode reference,
  since e.g. a palettes scroll shouldn't dirty/re-render chat-hai's
  entirely separate window instance.
- The Elem struct itself currently conflates THREE concerns in one
  mutable structure: (a) parsed document shape (tag/classes/children),
  (b) computed layout (x/y/w/h from CSS), (c) paint-time handles
  (sprite cache pointers, XftDraw state). wraith-alpha's `current_
  frame.txt` is a flat, serializable TEXT format with no live pointers
  at all — a real port would need a genuine intermediate
  representation (a plain, serializable frame snapshot) distinct from
  today's Elem tree, not just "write the Elem tree to a file."

**Honest read: this is a large, invasive refactor of the single most
heavily-used, most-frequently-rebuilt file in this house, spanning 7
window modes, all in active daily/session use.** It is real and worth
doing for the stated reason (honest, auditable receipts — a value this
house clearly holds, given how many other systems already have it),
but it is not a small or low-risk change, and a mistake here has wide
blast radius (every khtpm/-hq window, not just palettes).

============================================================
THE REAL END-GOAL (revised 2026-08-28, direct user correction — this
supersedes the narrower "per-mode frame file" framing this doc
originally proposed)
============================================================
The "modes" themselves (`g_is_db_hq`/`g_is_events_hq`/`g_is_chat_hai`/
`g_is_palettes`/`g_is_bookmarks`/`g_is_stats_hq`/`g_is_swatch_picker`)
are not a deliberate design — they're a direct historical artifact of
Stage 5's own real scope (per `khtpm-merge-how2.md`): 5 previously-
SEPARATE standalone binaries were copy-pasted into one file as
parallel branches to cut process count, but their app-specific control
flow (per-mode layout math, tab handling, scroll behavior, click
routing) was preserved as-is, not redesigned into one shared engine.

**Direct instruction, the real target:** the renderer should be a
genuinely GENERIC engine — parse a layout description (`.chtpm`+CSS,
which already exists and is already mostly generic), paint it, forward
input events — with ZERO mode-specific branches. Every app-specific
behavior (what a Terms-tab click does, how db-hq's scroll math works,
what a chat-hai session sidebar looks like) belongs entirely in
separate MANAGER processes, the same way a JS library attaches
behavior to a generic DOM rather than the browser engine itself
knowing about any specific website. This is a strictly BIGGER, BETTER
end-state than this doc's original "make each mode's own rendering
derive from its own frame file" framing — that framing still kept
"modes" as a concept baked into the renderer; this one removes them
from the renderer entirely.

**What already proves half of this is achievable, today, for real:**
the generic `onclick` dispatch (`open:`/`exec:`/`input:` prefixes,
`dbhq_activate_elem()`'s own generic branch, checked BEFORE any mode-
specific tag handling) already works exactly like this — a manager-
side script gets invoked generically, with zero mode-specific code in
the dispatcher itself. Multiple real managers already exist and
already do the "manager owns behavior, renderer just displays"
job for STATE (`khtpm_hq_manager.c`, `khtpm_events_hq_manager.c`,
`palettes_manager.c`, `terms_hq_manager.c`, `bookmarks_manager.c`) —
they publish plain state files the renderer's generic sidebar/panel
injection (`dbhq_inject_sidebar_items()`, now proven mode-agnostic
across Common Events AND Terms) already consumes without caring which
manager wrote it. **The real gap is narrower than "rebuild everything
from scratch"**: it's the LAYOUT/RENDER-DECISION code specifically
(per-mode `*_layout_pass()`, `*_redraw_content()`, `*_handle_key()`
functions) that still hard-branches on mode, not the state-publishing
side, which is already largely generic.

============================================================
RECOMMENDED APPROACH (for discussion, not yet approved)
============================================================
Given the size, a phased approach seems lower-risk than a single big
rewrite:

**Phase 0 (already done tonight, small):** debug-dump now includes
real geometry (x/y/w/h) alongside tag/id/label — closes the immediate
"can't prove a pixel bug from text" gap without touching the render
path at all.

**Phase 1 (small, provable, no render change):** make the EXISTING
frame-history files (`db_hq_frame_history.txt` etc) genuinely
COMPLETE — a real, full snapshot of what got drawn (every visible
Elem's tag/label/x/y/w/h, not just 4 scalar fields), still written
AFTER drawing as today. This doesn't fix the "render isn't derived
from the file" gap, but it DOES fix "the receipt isn't honest/complete"
in the meantime, and is low-risk (append-only, no behavior change to
drawing itself).

**Phase 2 (the real fix, large, needs its own dedicated design pass):
eliminate mode branches from the renderer, one mode at a time**, not
just "make each mode's frame derive from a file." For each mode
migrated: (a) whatever mode-specific layout/redraw/key logic exists
today moves OUT into that mode's own manager (already real and
running for most modes) as published, generic layout DATA — not just
content data like today, but real computed positions/sizes/behavior
hints; (b) the renderer's generic Elem/CSS/paint engine consumes that
data the exact same way regardless of which manager produced it,
matching the already-proven generic `onclick` dispatch and generic
sidebar-injection pattern; (c) a real marker-file dirty-gate per
window instance drives repaint, matching wraith-alpha's own real
`frame_changed.txt` mechanism. Suggested pilot mode: palettes (see
open question 2 below — still worth confirming, since db-hq/events-hq
see more day-to-day use and may matter more for trust).

**Not recommended:** attempting all 7 modes in one pass, or assuming
this is a full rewrite from nothing — the state-publishing half of
this pattern (managers own data, renderer displays generically) is
ALREADY real and working for several modes today; the actual lift is
narrower than it first looks, concentrated in the per-mode layout/
redraw/key-handling functions specifically.

============================================================
SCOPE CONFIRMED (2026-08-28, research pass before starting Phase 2)
============================================================
Three real questions were checked before committing to a pilot mode:

1. **Does the taskbar (khtpm_taskbar_manager.c/khtpm_strip_parser.c) or
   desktop entities need this refactor too?** No — and this was already
   investigated once, with the conclusion REVERSED from what you'd
   expect. The original 2026-08-15 merge plan called the taskbar's
   separate `LayDoc`/`LayElement` architecture (`khtpm_strip_layout.h`)
   "a mistake to correct." Real reconnaissance the very next day
   reversed that: `LayDoc` was ported from the SAME wraith-alpha
   `chtpm_parser.c` lineage this doc cites as the standard, and already
   has real `${var}` substitution + activation-scoping that Elem/CSS
   still lacks. Written verdict (merge archive, ~line 2124): **"LayDoc
   is not behind the Elem model — it's ahead of it."** Direction
   reversed: Elem/CSS should borrow FROM LayDoc, not replace it with
   Elem/CSS. Standing status: no drift to correct here, confirmed
   twice now (2026-08-16 and again in this research pass).
2. **Does mutaclysm need this refactor?** No — it's a structurally
   different, already-compliant pipeline (PAL/prisc+x VM +
   `compose_frame.c`/`compose_rgb_frame.c` + `x11_mirror.c`), already
   in the wraith-alpha family. It never touches `khtpm_entity_menu_
   render.c` or Elem/CSS at all.
3. **Was this exact refactor attempted before and done wrong?** No
   written record of that anywhere. What DID happen: the original
   Stage 5 merge plan explicitly scoped a follow-up ("Stage 2/3: one
   shared parser for all X layouts") right after the binary-consolidation
   stage — then that follow-up was simply never started, not botched.
   This effort is picking up a previously-scoped, never-executed stage,
   not fixing a past mistake.

**Net scope, now confirmed real and unchanged from this doc's original
sizing:** window-family modes only — db-hq, events-hq, chat-hai,
palettes, bookmarks, stats-hq, taskbar-settings/swatch-picker, all
within `khtpm_entity_menu_render.c`. Taskbar and mutaclysm are both
real, confirmed-out-of-scope, for real documented reasons — not
oversights.

============================================================
OPEN QUESTIONS FOR THE USER BEFORE PHASE 2 STARTS
============================================================
1. Confirm Phase 1 (complete-but-still-post-hoc frame history) is
   worth doing now as a real interim improvement, or skip straight to
   scoping Phase 2 on one mode.
2. Which mode should be the real first Phase-2 target — palettes (most
   actively worked on right now, simplest per-mode state) is the
   default suggestion here, but db-hq/events-hq are more heavily used
   day-to-day, which may matter more for "the receipts we most need to
   trust."
3. Should this become a dedicated, tracked multi-session effort (like
   the khtpm-merge-how2.md consolidation was) rather than something
   folded into ongoing feature work on top of this same file?
