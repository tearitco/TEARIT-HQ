**SUPERSEDED, forward pointer (2026-08-16):** events-hq (and db-hq) have
since been merged into the shared `khtpm_entity_menu_render.c` binary —
Stage 5 of the khtpm merge, see `khtpm-merge-how2.md` §5d.10/§5d.11 for
current, real status. The managed-window/`_MOTIF_WM_HINTS` focus fix
and RGB Phase 0 findings below are still real history, just no longer
the current architecture (events-hq is no longer its own standalone
binary).

# events-hq + RGB refactor — handoff (2026-08-12, context full, new
agent picking this up)

Read `aug-12-END.txt` (house root) FIRST for the full session narrative
(db-hq's whole focus-fix journey, the real bugs found+fixed). This doc
covers what happened AFTER that one - events-hq going from throwaway
test to real, wired feature, plus the RGB Phase 0 result and what
refactoring db-hq/the taskbar to RGB would actually take.

---

## 1. events-hq: real, wired, working - confirmed

Built this session: `&.widgits/events-hq/` (own top-level widget,
mirrors `event-ez`'s own layout convention). Real, not a stub:

- `ops/khtpm_events_hq_render.c` - managed window + `_MOTIF_WM_HINTS`
  (see `!.HOUSE_STDS.md` #21, the actual fix for db-hq's focus struggle,
  reused verbatim here), wraith-alpha nav (#22 - including the "Add
  Command" popup, which initially DIDN'T use nav and had to be
  corrected), real sprite.csv portrait loading (#23, not an emoji
  font), non-fatal X error handler (#24).
- Reads/writes the EXACT SAME `event_pkg/pages/page_N/event.ir.pdl` +
  compiles the same `event.pal`/`cmd_N.sh` event-ez itself uses (studied
  `ez_menu_input.c` before writing this, not guessed) - both editors
  stay compatible on the same data.
- Real `METHOD`/`OBJECT` rows wired for two real entities:
  `m1_ninjadragon` and `m8_redhorned` (`xyzfs/users/<uuid>/home/
  livedesk/pals/<name>/{meta.pdl,objects.pdl}`). **Both `meta.pdl`'s
  `METHOD` row AND `objects.pdl`'s `OBJECT` row needed the entry** -
  `objects.pdl`, when present, completely REPLACES the menu `meta.pdl`
  would otherwise build (real, confirmed live - the first attempt only
  edited `meta.pdl` and the row silently never appeared). Real bug
  found+fixed in the dispatch value itself too: a bare relative path
  (`&.widgits/events-hq/button.sh`) resolves against the LAUNCHING
  PROCESS's own CWD, not house root (same fragility class
  `!.HOUSE_STDS.md` #20 already documents) - fixed to
  `sh -c 'exec "$1/&.widgits/events-hq/button.sh" "$0" "$1"'`, which
  uses the dispatcher's own reliably-passed `$1` (house_root) to build
  an absolute path at runtime instead of a hardcoded one or a
  CWD-dependent relative one. **If you add an `Events (hq)` row to a
  THIRD entity, use this exact `sh -c` form, not a bare path.**
- **Nav confirmed working for real** (direct user confirmation, "i see
  nav does work") - digit-jump/arrows/Enter, including inside the "Add
  Command" popup, with real physical keyboard input. This is genuinely
  proven, not just logically reasoned.
- Entities need a real restart to pick up `meta.pdl`/`objects.pdl`
  edits - both are read ONCE at process startup
  (`tp_desktop_window.c`'s `load_methods()`/`load_objects()`), not
  per-click. Use the taskbar's own `Player > Reset` (`nav.sh nav 8` then
  `nav.sh row 3`) rather than manually killing/relaunching - it's the
  real, already-built "close all entities, respawn fresh" path.

### Near-term: events-hq still needs real logic event-ez already has

events-hq today is a real VIEWER + basic 3-type command ADD (Change
Gold / Show Text / Show Choices) - it does NOT yet have everything
event-ez has. Concrete gaps to check/build next, roughly in priority
order:

1. **Page management** - events-hq reads existing `pages/page_N/`
   dirs and lets you switch between them (tab click/digit-jump), but
   has no UI to CREATE a new page. Check `ez_menu_input.c` /
   `HOW2_USER_GUIDE.md` for whether event-ez itself can create pages
   from its own UI, or whether that's always been a manual/external
   step - don't assume, verify first.
2. **Condition/trigger editing** - events-hq's left panel shows the
   current `condition.pdl` trigger READ-ONLY. Check whether event-ez
   can edit it, and if so, add real text-entry for it (events-hq
   already has a working text-entry mechanism from the "Add Command"
   picker - reuse that same keystroke-accumulation pattern, don't
   build a second one).
3. **"Play" test-run** - `ava`'s own `meta.pdl` has a separate `Play`
   METHOD row that runs `prisc+x` directly against `event.pal`
   (`EVENTS_RUNTIME.md` describes the real runtime path via
   `play_event.sh`). events-hq has no equivalent "test this event
   right now" button. Real, useful, and small to add: a footer button
   that shells out to `play_event.sh <event_pkg_dir>` the same way.
4. **Node delete/reorder** - NOT a gap, a deliberate parity choice:
   `event.ir.pdl` is append-only in event-ez itself today
   (`HOW2_USER_GUIDE.md`'s own words - "Save appends new NODEs, in-place
   edit not yet built"). events-hq matches that same limitation on
   purpose. If event-ez ever gains delete/reorder, events-hq should
   too, at the same time - don't get ahead of the format's own real
   capabilities.
5. **`&.hq-apps/events-hq-test/`** (the Phase-0 throwaway test dir) and
   `ops/khtpm_rgb_test.c` are still sitting in the tree - genuinely
   throwaway per the RGB doc's own "Phase 0" scope, safe to delete once
   nobody needs to re-run that comparison test again. Not urgent.

---

## 2. RGB Phase 0 - confirmed proven, real result

Full plan/reasoning: `!.khtpm-rgb-refactor.md` (house root) - read that
BEFORE touching this section, don't re-derive it.

**Result, confirmed two independent ways** (not just eyeballed): a
compose-first buffer (Xft drawn into an offscreen Pixmap, read back
ONCE via `XGetImage`) presented via `XPutImage` into a real window, then
read back a SECOND time from that live window, produced BYTE-IDENTICAL
pixels to the original compose buffer - `memcmp()` in the test binary
AND an independent PIL `ImageChops.difference()` check from outside
both returned "no difference." The two-stage "compose once, present
anywhere" split genuinely works for the Xft-into-buffer approach (the
lower-effort option from the doc's own "open questions" list - true
headless/software-font-rasterizer was NOT tested, still an open
question).

Test artifacts: `khtpm_rgb_test.c` (throwaway, see above),
`&.hq-apps/events-hq-test/` (also throwaway - a MINIMAL static screen,
NOT the real events-hq built afterward, don't confuse the two).

### How to actually refactor db-hq / the taskbar header to RGB (next steps, concrete)

**Step 2 (db-hq) is DONE, confirmed live (2026-08-12, same session,
direct instruction: "i think we should do db to rgb refactor. the need
being auditability... it has no real logic yet so it seems safe
enough").** `khtpm_hq_render.c`'s `redraw()` now presents via
`XPutImage` off an `XGetImage`-derived frame (with `XCopyArea` fallback
if capture ever fails) and populates a persistent `g_frame_rgb`/
`g_frame_w`/`g_frame_h` buffer each redraw; `dump_frame_png()` was
rewritten to just write THAT buffer directly instead of doing its own
separate capture - this is the actual auditability payoff. Rebuilt
clean, verified two ways: headless `--dump-and-exit` (1559x783 PNG,
chrome/tabs/nav/panels all correct, matches pre-refactor appearance)
and a live relaunch via `open_db_hq.sh` with real FocusIn, real
arrow-key KeyPress events, and a relay-triggered `'p'` dump all still
working normally. Steps 3-4 (taskbar, "don't convert anything else
preemptively") are UNCHANGED - still not started, still gated the same
way.

Real next steps below are Phase-0-era text, kept for history; only step
1 (the Xft-vs-headless-rasterizer decision) and step 2's own ordering
rationale are still live-relevant now that step 2 itself is done:

1. **Decide the open question Phase 0 deliberately deferred**:
   software font rasterizer (true headless, no `Display` connection
   needed at all for compose - more code, matches `chtpm_rgb_render.c`'s
   own approach) vs. keep Xft but only ever draw into a malloc'd/Pixmap
   buffer (way less code, proven working in Phase 0, but still needs a
   live `Display` connection open even if no window is ever mapped -
   NOT fully headless). Given events-hq/db-hq both already depend
   entirely on Xft for text (no bitmap font work exists in either), the
   PRAGMATIC recommendation is: stay with Xft-into-buffer for now
   (Phase 0's own proven path), and only build a true software
   rasterizer later if a REAL need for Display-less operation shows up
   (e.g. wanting to render a frame from a cron job or CI with no X
   server at all) - don't build it speculatively.
2. **db-hq first** (`khtpm_hq_render.c`), matching
   `!.khtpm-rgb-refactor.md`'s own "candidates" ordering (smallest,
   newest, already has a `dump_frame_png()` stub that's the natural
   thing to REPLACE with the Phase 0 pattern instead of its current
   `XGetImage`-after-the-fact approach). Concretely: change `redraw()`
   to draw into `buf` (already does this - no change there), but change
   the PRESENT step from `XCopyArea(buf, win, ...)` to: keep one
   persistent `XImage`/malloc'd buffer around (not re-`XGetImage`'d
   every frame - that would be wasteful; only re-derive it if you
   specifically need the portable-buffer property, e.g. for a NEW debug/
   screenshot/relay-based rendering feature), and use `XPutImage`
   instead of `XCopyArea` for the actual on-screen present step,
   confirmed in Phase 0 to be pixel-identical. This is a real, bounded
   change (one function), not a rewrite - db-hq's own tag/CSS/nav/
   layout code is untouched.
3. **Taskbar header/strip second, only after db-hq's own conversion is
   confirmed stable for a while** (`khtpm_strip_parser.c`) - per the RGB
   doc's own explicit risk note: "it's the ONE taskbar in daily use,"
   higher risk, do NOT attempt this until db-hq's own conversion has
   been running/tested for real, uninterrupted use, not just a
   same-session smoke test. When it's time: same core swap
   (`XCopyArea`→`XPutImage` off a retained buffer), but the taskbar has
   THREE windows (`win`/`hq_win`/`popup_win`) and its own two-process
   manager pairing to preserve - budget real time for this, it is not
   the same size of change as db-hq's own single-window case.
4. **Don't convert anything else preemptively.** Direct instruction
   this session, after weighing it explicitly: "not yet" on converting
   existing systems to RGB broadly - the new managed-window/nav/sprite
   standard (`!.HOUSE_STDS.md` #21-24) is the actual priority to keep
   stable right now, RGB is a separate, still-small-scale-proven idea.
   Revisit RGB conversion when there's a CONCRETE need (headless
   testing, a screenshot pipeline for db-hq/events-hq specifically) -
   not just "it would be nice."

---

## 3. Zip-size question, resolved (real investigation, not guessed)

Direct question this session: "the zip for this dir is 3 times bigger
than it was before the refactor... is that cause of the repeated
entities?" Investigated live: this session's own additions total
~230KB of new binaries (`khtpm_hq_render.+x` 80K,
`khtpm_events_hq_render.+x` 82K, throwaway `khtpm_rgb_test.+x` 68K) plus
a few hundred KB of source/docs - nowhere near enough to triple a zip
of a 179MB tree. Confirmed: no duplicate per-entity binary copies
(pals correctly share one `tp_desktop_window.+x` binary, invoked with
different argv per entity, not copied), no stray large files modified
today, not a git repo (no `.git` bloat possible). **Not resolved**:
what the actual OLD zip was / what tool made it - this needs the old
zip file (or the exact command that created both) to diff properly;
don't guess further without that.

---

## 4. Where to look next / what NOT to re-derive

- `aug-12-END.txt` (house root) - the db-hq focus-fix journey in full,
  read first.
- `!.HOUSE_STDS.md` #19-24 - the real, hard-won X11/Wayland/Mutter
  focus rules for ANY new khtpm/-hq window. Don't reinvent any of
  these; they're each tied to a real, live-caught bug.
- `!.khtpm-rgb-refactor.md` - RGB reasoning + Phase 0 result + Phase 1
  plan (section 2 above summarizes it, but the doc has the full
  reasoning on the headless-vs-Xft tradeoff).
- `HQML-DESIGN+PLANS.md`'s "Window Chrome Convention" section is
  OUTDATED (still says "every khtpm window is override_redirect") -
  flagged in `!.HOUSE_STDS.md` #21 as needing a follow-up correction,
  not yet done. Fix that file's text to match #21 before anyone reads
  it and copies the wrong pattern.
