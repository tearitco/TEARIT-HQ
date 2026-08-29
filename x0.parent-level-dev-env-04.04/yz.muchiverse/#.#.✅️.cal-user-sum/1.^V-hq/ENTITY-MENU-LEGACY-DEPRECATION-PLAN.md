ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md
Started: 2026-08-28

============================================================
STATUS UPDATE 2026-08-29 — Phase 0/1/2 DONE, Phase 3 real finding:
the file does NOT archive whole
============================================================
**Phase 0 (live paint bug) - DONE, verified live.** Root cause found
via real PNG dump (relay 112/'p'), not guessed: `dbhq_paint_frame_
line()`'s pipe-delimited frame format broke when an item's own
onclick shell command contained literal "|" characters (book-stack's
"Read" action - `find ... | head -1`, twice). Fixed by anchoring the
5 front fields (tag/id/classes/label/sprite) from the START and the 6
trailing numeric fields (nav_index/active/x/y/w/h) from the END,
leaving onclick as "whatever's in the middle" - safe to contain any
number of real pipes. One self-correction along the way (first attempt
used `strlen(buf2)` after the buffer already had embedded NULs from
the front-parse, silently breaking EVERY entity's menu, not just
book-stack's - caught immediately, fixed same session, re-verified).

**Phase 1 (real, durable converter) - DONE, per user's explicit choice
of design A (generate a real file) over design B (read meta.pdl
live).** `meta_to_menu_chtpm.py` (`*.livedesk-taskbar/ops/`) - real,
permanent, mechanical METHOD-row -> `<item>` converter, replacing the
lost one-off script from 2026-08-16/18. Wired into `tp_place_
desktop.c` so every NEWLY placed entity gets a real `menu.chtpm` the
same moment its `meta.pdl` is written - not a manual step anyone has
to remember.

**Phase 2 (batch backfill) - DONE, all 3 real real-instance locations
covered, not just the one originally known about:**
- `xyzfs/users/<uid>/home/livedesk/{pals,sessions/*/entities}/*` - 14
  real instances (the one real, currently-live user; confirmed via
  `ps aux` that every currently-running `tp_desktop_window_rgb.+x`
  process points here).
- `#.desktop/entities/*` (dog/ava/chicken/cat/asa) - a second, real,
  separate convention (referenced by `@.apps/asa-&-ava/button.sh` and
  `$.crypts/ops/crypt_autostart.c`) this plan's original state-check
  missed - 5 more real instances, now converted.
- Entity TYPE templates under `*.monads/*.<project>/entities/<name>/`
  - 11 real templates (book-stack, cursword, self, 8 muchi-pet
  monsters) - converted too, so a FUTURE fresh copy of a template
  already carries a real menu.chtpm even before `tp_place_desktop.c`'s
  new hook would fire for it.
- **Total: 30 real menu.chtpm files now exist house-wide** (was 7).
  Zero skipped due to a real parse failure; every one produced a
  real, non-empty item list.

**Phase 3 (archive) - REAL FINDING, changes the plan: `tp_desktop_
window_rgb.c` does NOT get archived, whole or otherwise, as a file.**
Read its own header comment directly: this is a 3412-line binary whose
REAL, primary, ongoing job is "be a real, draggable, closeable OS
window that represents this one desktop package" - the actual desktop
sprite/tile render, drag-to-reposition (writes `desktop_pos.txt`),
and self-close-on-deletion polling. The popup-menu engine (`launch_
khtpm_menu()` + the built-in fallback popup code) is ONE feature
inside this much larger file, not the file's own real purpose. **6
real, currently-running desktop entities depend on this exact binary
staying correct right now** (confirmed via `ps aux`) - this is not a
file to delete or wholesale-archive under any real design this house
would recognize as safe.

**What CAN still happen, a real, separate, smaller, deferred
decision**: now that all 30 known real entities have a `menu.chtpm`,
`launch_khtpm_menu()`'s own `access(menu_chtpm_path, F_OK) == 0` check
means the OLD BUILT-IN popup code inside this file is dead code for
every currently-known entity - it would only ever run for some future
entity that somehow lacks a `menu.chtpm` (which, given Phase 1's hook,
should no longer happen for anything created through the real
placement path). Whether to actually DELETE that now-dead code out of
a 3412-line, 6-live-process-depended-on file is a real, separate,
higher-risk decision than anything else in this plan - **not done
without explicit go-ahead**, and even then, worth keeping as a real
defensive fallback (an entity with a missing/corrupt menu.chtpm still
gets SOME popup instead of none) rather than deleting outright. Direct
instruction ("archive it") assumed the whole file was the popup engine
- it isn't; recommend closing this plan's real, achievable goal (every
entity on the shared renderer) as DONE, and treating "delete the now-
dead legacy popup code from tp_desktop_window_rgb.c" as its own,
separate, optional follow-up task, not this plan's Phase 3.

Purpose: finish migrating EVERY entity's right-click context menu onto
the shared Elem/CSS renderer (`khtpm_entity_menu_render.c`, class=
"entity-menu") and archive the legacy per-entity popup engine built
into `tp_desktop_window_rgb.c`, per direct instruction: "we need to
actually switch entities to all use the same parser/renderer. they
should have been part of the earlier refactor. nothing should be
using that legacy code. we should be able to deprecate it to an
archive zip."

============================================================
REAL, CONFIRMED CURRENT STATE (verified by direct read, not assumed)
============================================================

**This migration already started once, 2026-08-16/18, and stalled
partway.** `khtpm_entity_menu_render.c`'s own header comment says so
literally: "ONE-ENTITY TEST CASE ONLY... Every OTHER entity still uses
tp_desktop_window_rgb.c's own built-in popup engine... until this is
proven live on ava first." A later comment inside book-stack's own
`menu.chtpm` confirms a real follow-up pass happened: "Stage 2c
ROLLOUT (2026-08-16, direct instruction 'yes we need to finish those')
- entity-context-menu rollout beyond ava."

**Real dispatcher already exists and is a real, honest bridge, not a
hack**: `tp_desktop_window_rgb.c`'s own `launch_khtpm_menu()`
(~line 258-286) checks `<package_dir>/menu.chtpm` - if it exists,
that specific entity's right-click launches
`khtpm_entity_menu_render.c` against it INSTEAD of the legacy built-in
engine. Confirmed at the two real call sites (~1854, ~2028-2032).
Every entity WITHOUT a `menu.chtpm` still falls through to the legacy
path, unchanged - this is why "book-stack, ava, asa, self,
m1_ninjadragon, m8_redhorned, m9_missingno" (confirmed: these 7 have a
real, live `menu.chtpm`) are already on the new renderer, and every
other entity is not.

**Real, current scope of what's left**: 45 real `meta.pdl` template
files exist across all entity TYPES in this house; only 7 real
per-INSTANCE `menu.chtpm` files exist today (all under
`xyzfs/users/<uid>/home/livedesk/pals/<name>/`). The other ~38+
entity types/instances are still 100% on the legacy engine.

**The conversion script that made those 7 no longer exists** -
book-stack's own comment calls it "scratchpad meta_to_chtpm.py," and
it's gone (confirmed via `find`, no hits). Those 7 were converted BY
HAND, once, as a one-off - not by a real, permanent, repeatable
mechanism. This is the real reason the rollout stalled: there was
never a durable way to keep producing `menu.chtpm` for new/changed
entities, only a one-time script somebody ran and then lost/deleted.

**A real, live bug exists in the migrated path right now** (found
this session, book-stack): first menu item invisible-but-clickable,
visually jumbled with the window's own header/title. This is NOT a
legacy-code bug - book-stack is already on the NEW renderer, so this
bug lives in the shared, current code everyone's being migrated onto.
Root cause not yet pinned down (candidates: `measure_context_popup_w`/
row-height-vs-hit-test-row mismatch in the popup chrome, or a title-
element positioning issue) - **must be fixed BEFORE mass-rollout**,
or the migration just spreads a visible bug from 7 entities to 45.

============================================================
REAL DESIGN DECISION NEEDED FIRST (pick one, don't drift into a third
option silently)
============================================================
**A. Keep the current shape: a real generator produces a static
`menu.chtpm` per entity instance, same format the 7 already use.**
Needs: a real, PERMANENT, compiled converter (not a throwaway script,
not bash `printf`-generated XML - that's the exact anti-pattern this
house already found and fixed for stats-hq/palettes/bookmarks, see
`TPMOS-COMPLIANCE-DEBT.md`) that reads `meta.pdl`'s real `STATE`/
`METHOD` rows and emits real `<page><item label=".." action="..".../>`
XML, called automatically whenever an entity gets placed on a desktop
(the same real moment `tp_place_desktop.c` already writes
`meta.pdl`/`glyph.txt` for that instance - see its own header comment,
"tp_place_desktop.c already writes a real package (glyph.txt +
meta.pdl)"). Regenerate-on-change would also need wiring (an entity's
`meta.pdl` can be hand-edited later, per that file's own convention
"edit meta.pdl and the next right-click re-reads it").
Real tradeoff: a generated file can drift/go stale if regeneration
isn't triggered on every real meta.pdl edit, same class of bug this
session already found and fixed elsewhere tonight (the RMMV terms.pdl
mtime-staleness bug, the palettes checksum bug).

**B. Skip the intermediate file: teach `khtpm_entity_menu_render.c`'s
entity-menu path to read `meta.pdl` DIRECTLY at popup-open time,
building the same Elem tree in memory, no `menu.chtpm` ever written.**
No generator to keep in sync, no staleness class of bug possible (the
live file IS the source, read fresh every open - matching
`tp_desktop_window_rgb.c`'s OWN existing real guarantee: "edit
meta.pdl and the next right-click re-reads it," which option A would
have to re-earn with extra machinery, option B gets for free).
Real tradeoff: a small, real, one-time C addition (a `meta.pdl` ->
`Elem` tree builder, reusing `elem_new()`/the same `<page>`/`<item>`
tag semantics the current `.chtpm` parse already produces) - more
code up front, zero recurring generator/staleness surface after.

**Recommendation: B.** It's the same real principle already applied
elsewhere tonight (read the live source directly, don't generate an
intermediate that can drift) and it directly removes the ONE reason
the 2026-08-16/18 rollout stalled (no durable generation mechanism) -
there's nothing to keep durable if there's no generated file. Also
matches the compliance debt doc's own real vocabulary: a bash-generated
XML file is exactly what got flagged as non-compliant elsewhere;
reading the real data source directly in C is the pattern that
replaced it every other time this session.

**This is a real fork, not a small style choice - confirm before
Phase 2 starts building either shape.**

============================================================
PHASE 0 - fix the live bug in the ALREADY-migrated path (blocks
mass-rollout, do this first regardless of A/B above)
============================================================
Book-stack's menu (first item invisible/jumbled with header) is a
real bug in the CURRENT shared renderer's entity-menu popup chrome -
not legacy code. Real repro: book-stack is one of only 2 entities
house-wide with `grab_keyboard=1` set (the other is `cursword`) -
check whether `cursword`'s converted menu.chtpm (already exists, one
of the 7) shows the same symptom; if yes, the bug is real and general
to any entity with more real METHOD rows /a specific grab-state
combination, not book-stack-specific. Read `khtpm_entity_menu_
render.c`'s actual popup chrome/header draw code (this session did
NOT find the exact off-by-one yet - `measure_context_popup_w`-style
sizing vs. the header/row-0 draw position is the lead, not confirmed)
and fix it, live-verified against book-stack AND cursword, before
touching any other entity.

============================================================
PHASE 1 - build the real, durable mechanism (per the A/B decision
above)
============================================================
If B (recommended): add a real `meta.pdl`-to-`Elem`-tree builder
function to `khtpm_entity_menu_render.c` (or, if genuinely reusable
beyond this one mode, `khtpm_render_core.c` - evaluate real
X11/global coupling first, same standard this session already applied
to every other "does this belong in shared-lib" decision tonight).
Real behavior: given a `package_dir`, read its `meta.pdl`, skip
`SECTION`/blank/comment lines, map every real `METHOD | <label> |
<action>` row to one `<item label=".." action="..">` equivalent Elem,
in file order, same real action-string convention (`CLOSE`/`void`/
shell command) `dispatch_action()` already handles - reuse that
dispatch, don't reinvent it. `STATE` rows (`menu_stay_open`/
`grab_pointer`/`grab_keyboard`/etc) get read the same way
`tp_desktop_window_rgb.c`'s own `read_menu_config()` already does -
port that logic, don't redesign it.

If A: build the real, compiled, permanent converter binary instead,
wire it into `tp_place_desktop.c`'s own real package-write moment plus
a real "meta.pdl changed, regenerate" trigger (mtime or content
checksum, matching this session's own established real-staleness-fix
pattern).

============================================================
PHASE 2 - real per-entity cutover, verified one at a time
============================================================
Do NOT flip all 45 entity types at once. Real order, lowest-risk
first:
1. The 7 already-converted instances (book-stack/ava/asa/self/
   m1_ninjadragon/m8_redhorned/m9_missingno) - if going with option B,
   these can DROP their static `menu.chtpm` entirely once the direct-
   read path is proven equivalent (real regression check: same real
   menu items, same real actions, same real STATE flags honored).
2. The simple, no-special-STATE-flags entities next (dog/cat/chicken/
   the `monster` kind entities without grab flags) - lowest risk,
   proves the generic path on the common case.
3. Entities with real, non-default STATE rows last (anything else
   setting `menu_stay_open`/`grab_pointer`/`grab_keyboard`, `asa`/
   `ava`'s multi-page-flavored METHOD sets if any exist) - higher risk,
   do after the generic path is proven solid.
Real proof per batch, not just "it compiled": right-click each
migrated entity for real (or via the house's own file-relay input
convention, same discipline as every other window family this
session), confirm every real METHOD row appears as a real, correctly-
labeled, correctly-clickable item, confirm STATE flags (grab/stay-open)
behave identically to the legacy path before calling that batch done.

============================================================
PHASE 3 - archive the legacy engine, only after Phase 2 is 100% done
============================================================
Real verification before archiving anything: `grep -r
"tp_desktop_window_rgb"` across launchers/button.sh files house-wide,
confirm ZERO real entity still depends on its BUILT-IN popup path
(the binary itself may still be needed for other real responsibilities
it has beyond the popup menu - e.g. actually rendering/dragging the
entity sprite on the desktop; read its own file top-to-bottom first to
confirm the popup-menu code is genuinely separable from whatever else
it does before assuming the WHOLE file can be archived, not just the
popup-engine portion of it).
If the popup-engine code is cleanly separable: remove that dead code
from `tp_desktop_window_rgb.c` directly (a real, targeted deletion,
not just "stop calling it") once nothing references it, OR if the
whole file's real remaining purpose (desktop sprite rendering/drag)
still needs it, only the popup-specific functions get removed, not the
whole file archived.
If the whole file is genuinely dead after this migration: move it (and
its own build script) to `archive/`, same real convention this house
already uses for retired binaries (`khtpm_hq_render.c`'s own planned
archival is the direct precedent - confirmed via `khtpm-merge-how2.md`),
update any doc that still names it as the current mechanism (same
append-only correction discipline used for tonight's other doc fixes),
and confirm via `grep -r "tp_desktop_window_rgb"` one more time that
nothing house-wide still launches it before the file physically moves.

============================================================
OPEN QUESTIONS FOR WHOEVER IMPLEMENTS (Grok when back online, or
Sonnet) - answer before starting Phase 1
============================================================
1. A or B (see design-decision section) - user/Sonnet/Grok should
   confirm before code starts. Recommendation stands: B.
2. Does `tp_desktop_window_rgb.c` have OTHER real responsibilities
   beyond the right-click popup menu (desktop sprite render/drag/
   footprint) that mean the file itself survives even after the popup
   engine inside it is fully retired? Read the file's own real scope
   before assuming "deprecate to archive zip" means the WHOLE file.
3. Phase 0's exact root cause (the book-stack/cursword menu bug) -
   not yet found, first real task for whoever picks this up.
