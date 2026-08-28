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

**Phase 2 (the real fix, large, needs its own dedicated design pass
per mode): make the frame-history file the thing render is DERIVED
FROM**, not a receipt written after the fact — likely mode-by-mode
(start with ONE window mode, e.g. palettes, since it's the one under
active work and has no drag/modal-overlay complexity events-hq/chat-hai
have), proving the pattern before extending to the other 6 modes.
Requires: (a) a real, flat, serializable frame-snapshot format
(wraith-alpha's `current_frame.txt` is the real reference shape); (b)
separating "compute layout + decide what changed" from "paint pixels"
into two real steps with the snapshot file as the boundary between
them; (c) a real marker-file dirty-gate per window instance (not
global, since 7 modes' windows are independent).

**Not recommended:** attempting all 7 modes in one pass. The reference
implementation this house already trusts is single-mode; matching that
same incremental, provable-per-instance discipline here is the safer
path, not a larger one-shot rewrite of the whole file.

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
