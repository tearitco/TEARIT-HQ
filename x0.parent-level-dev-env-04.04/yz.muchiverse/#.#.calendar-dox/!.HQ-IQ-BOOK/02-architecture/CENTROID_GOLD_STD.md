# 🎯 CENTROID_GOLD_STD.md — the real, final rendering architecture, going forward

**Status: GOLD STANDARD, adopted 2026-08-31.** Every NEW UI/window/app
built in this house from this point forward follows this document.
Existing apps are NOT retroactively rewritten to match it (see
"Looking backward" at the end) — but any future touch to one of them
is a real, deliberate opportunity to migrate it onto this standard
instead of extending its old engine further.

> **MANDATORY, added 2026-09-01, direct instruction after a real,
> concrete incident** (an agent read this doc alone, then spent a full
> session rewriting the wrong file — a deprecated standalone renderer —
> while the real, current, compliant version of that exact app already
> existed, unused, in the same directory): **before touching ANY
> khtpm-family renderer/manager code, read
> `#.#.calendar-dox/1.^V-hq/INDEX.md`'s Tier 1 list in full**, not
> just this one document — specifically `xperiments/khtpm-generic-
> dispatch-design.md` (read the file itself, not a summary of it in
> another doc — it's a living, dated-status-update document and an
> older cross-reference can be stale) and `TPMOS-COMPLIANCE-DEBT.md`.
> See `.claude/skills/khtpm-house-standards/SKILL.md` for the full
> incident writeup and the concrete, currently-adopted answer
> (generic `<cli_io>`/live-reparse capabilities in the shared renderer,
> no per-app dispatch table, no linking).

This doc exists because the real path to this answer ran through
several genuine, real, working architectures first — each one solved
a real problem, each one had a real, specific flaw that the next
attempt existed to fix, and none of them were mistakes in the sense of
being careless. **We were always aiming at this destination; we just
didn't know yet that the earlier stops were experiments, not the
final form.** This document names that arc honestly — including
condemning the real, specific flaws of what came before — because a
future agent needs to know WHY the old patterns are no longer the
model to copy, not just that a newer one exists.

---

## 1. The core principle, stated once, precisely

**One real, parsed, laid-out, styled tree of positioned rectangles
(`Elem`, with real `x/y/w/h` and real `CssStyle`) is the single source
of truth for what a window looks like. Every real display target
(X11/RGB today, ASCII/terminal tomorrow) is a thin, symmetric renderer
function that walks that SAME tree and produces its own output format
— never a second, independently-composed representation of the same
UI, and never business logic living inside the shared renderer/parser
file itself.**

That's the whole rule. Everything below is the real history that led
here, and the real, concrete consequences of the rule.

---

## 2. The real history — four real stages, not one mistake and one fix

### Stage 1 — `chtpm_parser_pal.c` / the PAL-VM engine (real, ASCII-native, still running today)

A real, working, text-grid layout engine — `.chtpm` files parsed into
a plain text buffer (`current_frame.txt`), printed straight to a
terminal. This is the REAL origin of several ideas this standard
keeps: `nav_index`/digit-jump focus (ported into khtpm verbatim, per
`Elem.nav_index`'s own real header comment — "wraith-alpha-standard
index nav... Ported from `wraith_parser_alpha.c`'s own `digit_accum`/
`do_jump` convention"), the `<interact src="...">` declarative relay
model, and the whole file-based-state discipline (`SKILLS.md` §1) this
house runs on. **Real, specific flaw**: the layout model itself is a
plain character grid with **no box model at all** — no real x/y/w/h
per element, no real color/border concept, nothing a second renderer
could use to draw anything other than monospace characters. This was
never going to grow into a native-looking GUI, structurally, no matter
how much effort went into it — the data it produces simply doesn't
carry the information a styled rectangle needs.

### Stage 2 — `chtpm_rgb_render.c` + `gl_mirror.c`/`x11_mirror.c` (real, working, a real dead end for "native-looking")

The house's first real attempt to get a WINDOW out of an ASCII app,
without touching Stage 1's engine: **font-rasterize the already-
composed text, character by character, into an RGBA buffer**, then
blit that buffer in a GL or X11 window. Real, working, still in
production for several apps today. **Real, specific, confirmed flaw**
(its own header comment, read directly, not inferred): "Does NOT parse
`.chtpm`... does not recognize `<cli_io>`, `<button>`, `<text>` tags...
zero semantic awareness of buttons/panels — it just blits whatever
characters are already there." This was a real, honest, deliberate
choice at the time (get SOMETHING on screen, reuse Stage 1's real
composed output, touch nothing else) — but it was **structurally
incapable of ever looking like a real, styled GUI**, for the exact
same root reason as Stage 1's flaw: there was still no positioned,
styled rectangle data anywhere in the pipeline, just characters. This
stage is a real, legitimate **mirror** technique (screen-sharing a
text app into a window) and remains useful for exactly that narrow
job — it was never going to become more than that, and treating it as
a stepping-stone toward a native GUI (rather than its own, separate,
complete answer to a narrower question) was the real mistake in how it
was reached for.

### Stage 2b — the game-tile path (`ops/compose_frame.c` / `ops/compose_rgb_frame.c`) — condemned outright

A second, PARALLEL composition path for the actual game/world view,
built independently of Stage 1/2's text pipeline — one hand-written
composer for the terminal, a SECOND hand-written composer
independently re-deriving the same tile-to-pixel math for RGB, kept in
sync only by comments and a hand-maintained sentinel-byte convention.
**Real, documented, live-caught consequence** (`!.HOUSE_STDS.md` §E.1):
a real race condition when both composers try to own the same output
file at once. This is the clearest, most direct violation of this
whole document's core principle that exists anywhere in the house's
history — two independent representations of the same UI, drifting by
construction, not by accident. **Condemned outright, not carried
forward in any form.**

### Stage 3 — the khtpm family (raw Xlib/Xft, its own `Elem`/CSS model) — got the LOOK right, in isolation

Built to solve the real problem Stage 1/2 never could: a genuinely
native-looking, styled, interactive window (db-hq, chat-hai, palettes,
bookmarks, stats-hq, entity-menu popups — all one real, merged binary,
`khtpm_core_render.c`). Real, working, real per-element
`x/y/w/h` after a real `layout_pass()`, real `CssStyle` (background/
foreground/border color+width — `khtpm_css_parser.c`'s own real
`apply_style()`), real hit-testing, real nav badges. **This stage
solved the "make it look right" problem completely and correctly.**
**Real, specific flaw**: it was built in isolation from Stage 1's own
real lessons, and two real regressions crept in as a direct result:
1. It never gained a second renderer for its own `Elem` tree — the
   exact same positioned/styled data that makes it look right is
   ALSO everything a text-mode renderer would need, and nobody built
   one, so every khtpm app lost the real CLI/headless-mirroring
   property Stage 1 apps have for free.
2. Individual window modes started writing their OWN business logic
   directly into the shared parser/renderer file
   (`dbhq_load_actors()` — real PDL data, but loaded inline in
   `khtpm_core_render.c` itself, condemned in
   `TPMOS-COMPLIANCE-DEBT.md` §4 the same day this standard was
   written) instead of the real, already-proven-elsewhere manager
   pattern (`khtpm_hq_manager.c` for Common Events). **This is a real,
   avoidable compliance miss, not a necessary cost of Stage 3's real
   architecture** — the manager pattern already existed in the same
   house, the same week, for the same window. Condemned explicitly;
   see §4 below for the rule this produces.

### Stage 4 — TODAY's real discovery: Stage 3's own tree already IS the missing centroid

Investigating whether a "styled mirror" could be made to look like a
real khtpm window led to checking `Elem`'s own real fields directly:

```c
/* khtpm_render_core.c, real, unmodified */
int x, y, w, h;   /* REAL, computed pixel position/size (layout_pass()) */
CssStyle style;   /* REAL background-color/color/border-color/border-width */
```

This is, verbatim, the "renderer-agnostic frame of drawable
primitives" every earlier stage was reaching for and never quite
built — Stage 1 lacked it entirely (no box model), Stage 2 mirrored
text instead of primitives, Stage 2b hand-duplicated it per-renderer.
**It was sitting, complete and already correct, inside Stage 3's own
`Elem` struct the entire time**, just never given a second consumer.
The real, final architecture (§1's own rule) is: keep Stage 3's real
parse+layout+CSS pass exactly as-is, add ONE new, thin, symmetric
renderer (`ascii_draw_elem()`, not yet built) that walks the SAME real
tree and draws box-drawing characters instead of pixels. Two real
renderers, one real tree, zero duplicated composition logic, zero new
parser, zero new IR to invent.

---

## 3. The real rule for every NEW window/app, going forward

1. **Author one real `.chtpm` + one real `.css`, and actually PARSE
   them through the real layout pipeline** (`parse_chtpm()`-equivalent
   + `layout_pass()`/`css_layout_pass()` + CSS application — the same
   pipeline `khtpm_core_render.c` itself uses) to build the
   Elem tree. khtpm's own tag vocabulary (`window`/`sidebar`/`panel`/
   `button`/`text`/`row`) — not a new vocabulary, not chtpm_parser_
   pal's ASCII vocabulary. **CORRECTED 2026-08-31, direct instruction
   ("we still want to use chtpm+layout module, we always should no
   matter what")**: this file originally carried an exception for "a
   small, wholly data-driven view," citing `khtpm_choice_picker.c` as
   real house precedent for hand-building the Elem tree directly in C
   instead of parsing a `.chtpm`. That exception was wrong and is
   struck — `khtpm_choice_picker.c` being a real, existing case that
   skips this does not make skipping it acceptable going forward.
   There is no size/complexity threshold below which hand-building is
   fine: every app, however small, parses a real `.chtpm`+CSS. Dynamic/
   fetched content is injected into the PARSED tree afterward (via
   `reusable_slot()`/`elem_inject_loop()`, both already real, shared,
   generic functions), never used as a reason to skip parsing
   altogether. This also settles rule 4 below: the ASCII/CLI mirror
   renderer must walk the SAME parsed tree the X11 renderer built —
   not re-derive its own flat view straight from the manager's state
   file independent of that tree.
2. **Business logic lives in a real, separate, compiled manager
   process** (`<app>_manager.c`, matching `khtpm_hq_manager.c`'s own
   proven shape: poll loop, publish a plain-text state file, consume a
   plain-text action-request file) — **never inline in the shared
   parser/renderer file.** This is not optional polish; it is the
   direct, condemned-in-§2-Stage-3 lesson. See `TPMOS-COMPLIANCE-
   DEBT.md` for the full real inventory of what happens when this is
   skipped (dead, unwired tabs; a shared file nobody can safely touch;
   real, live user-visible breakage).
3. **Parse + layout + CSS run exactly once**, producing the real
   `Elem` tree (existing `khtpm_render_core.c`/`khtpm_css_parser.c` —
   shared, unmodified, do not fork a new copy for a new app without
   checking whether the existing one already does the job, per
   `SKILLS.md` §3 landmine #1).
4. **Every real display target is a thin renderer function consuming
   that SAME tree** — `draw_elem()`/`render_tree()` for RGB/X11
   (already exists, unchanged), `ascii_draw_elem()` for a real
   terminal/headless mirror (to be built, real design in
   `xperiments/chtpmx11-refactor.md` §8) — never a new, independently-
   composed representation. If a NEW display target is ever needed
   beyond these two, it is a third thin renderer of the same tree, not
   a third composer.
5. **Real nav-index/digit-jump semantics stay real for every renderer**
   — `Elem.nav_index` already carries this house-wide (`SKILLS.md` §2),
   and it is real, existing data every renderer (including the future
   ASCII one) should surface, not something to reinvent per backend.
6. **Real receipts for every real renderer's own output**, same
   pattern already proven for `rgb_frame.receipt.txt`/`gl_display.
   receipt.txt` — a renderer's own output dimensions/checksum belong
   in a real, adjacent receipt file, not assumed/hardcoded by whatever
   reads it next.
7. **The shared renderer file (`khtpm_core_render.c`) never
   gains a new `g_is_<project>` global or a new per-project `strcmp`
   branch at a dispatch site, ever again.** ADDED 2026-08-31, direct
   correction caught live while building this doc's own first proof
   case ("that's still hardcoding... why cant u use existing
   conventions... the parser/renderer should have no knowledge of new
   projects and be completely agnostic"): every existing mode (db-hq,
   events-hq, chat-hai, palettes, bookmarks, stats-hq, swatch-picker)
   already has this exact shape — a global flag checked at ~15
   scattered dispatch points — and it is real, self-acknowledged debt,
   not a pattern to copy for an 8th time. A new mode registers itself
   in a real, generic dispatch table instead (`g_khtpm_modes[]` —
   design in `xperiments/khtpm-generic-dispatch-design.md`, not yet
   implemented as of this correction). This is the SAME severity class
   as rule 2's `dbhq_load_actors()` condemnation — a real, structural
   violation of "the renderer has no business logic," just distributed
   across dispatch sites instead of concentrated in one function. See
   `TPMOS-COMPLIANCE-DEBT.md` §6 for the full real incident + the
   ordered migration plan (build the generic mechanism → network-
   browser becomes its first real user → migrate each existing mode
   one at a time, smallest first, live-verified after each → delete the
   old `g_is_X` branches only once every mode is migrated).
8. **Repaint discipline — marker/dirty model, one writer, never a
   full redraw per raw X event.** ADDED 2026-09-03 after a real,
   multi-day flicker incident on `db-hq-pal` (`forensic-report-
   flicker.md`). The tpmos reference render loop
   (`pieces/chtpm/plugins/chtpm_parser.c` ~3024-3155) **never** blits
   unless a `dirty` flag is set, and `dirty` is set **only** when an
   append-only marker file *grows* (`st.st_size > last_size`) — not on
   mtime, not on a hash, not per input event. Input handlers *write
   the marker*; they do not call the composer. This binary already
   does exactly that for the real `g_is_db_hq` window
   (`dbhq_marker_pilot()` / `mark_frame_changed()` /
   `consume_frame_changed()`), and now for the generic default-mode
   window too (`g_frame_dirty`, consumed once per event-loop tick).
   Concrete rules for any renderer path, new or touched:
   - **One writer per frame file.** The process that composes the
     frame owns its frame/serialization file (tpmos PITFALL #17). No
     second process `fopen("a")`s it. PID-scope the path if more than
     one window of the same mode can exist.
   - **Coalesce.** N repaint requests inside one event-loop iteration
     collapse to one `redraw()` at the tick boundary. `redraw()` is
     not a "request" — it is the blit.
   - **No full redraw on a bare X event.** `Expose` → drain the whole
     burst, repaint once. `FocusIn`/`FocusOut` → ignore
     `NotifyGrab`/`NotifyUngrab`/`NotifyWhileGrabbed` and
     `NotifyPointer`/`PointerRoot`/`Inferior`; only repaint when the
     thing you draw from focus state actually changed. Relayed
     `MOUSE_EVENT:` **moves** are not consumed input — only clicks and
     wheel notches dirty the frame (tpmos PITFALL #52).
   - **No corrective `XMoveWindow`/`XSync` on the hot path unless our
     *intended* geometry changed.** Compare root-translated coords
     (`XTranslateCoordinates`), never frame-relative
     `XGetWindowAttributes` `wa.x/wa.y` — a reparenting WM makes that
     comparison always-true and the move storm perturbs focus and
     stacking, feeding the two rules above.
   - **Atomic state-file publish.** A projector/manager writes its
     `state/*.txt` via tmp + rename so a reader never sees a partial
     frame. A reader that content-hashes a state file debounces
     (same new value on two consecutive polls) before acting.

---

## 4. Looking backward — what this means for existing apps

**No free retrofit is expected or owed to any existing chtpm-native
app** (IRC chat, forum, chain, mutaclysm, piececraft/board-viewer's
own real engine, and every other `chtpm_parser_pal`-driven project).
They are not deprecated today, and nothing about them is broken by
this standard's adoption. They were real, working, valuable scaffolding
— every real convention this standard keeps (nav-index, `<interact>`-
style relay forwarding, the receipt+checksum discipline, the whole
file-based-state philosophy) was learned from building and living with
them, not invented from nothing. Treat them with that respect, not as
a false start to be ashamed of — they were genuine steps on the real
path to this document, we just didn't know yet which step we were on.

**The real, standing posture going forward**: no active retrofit
campaign, but a **deliberate bias to migrate opportunistically** —
whenever a real feature request, bug, or rewrite already has a reason
to touch one of these apps' own display layer, that is the real moment
to ask "does this become a real khtpm `Elem`-tree app instead of
extending the old text-grid engine further," not a moment to just
patch the old engine again by default. Each such migration is a real,
one-app-at-a-time decision (matching `khtpm-merge-how2.md`'s own
already-proven "no flag-day, no forced rewrite" discipline for the
5-app khtpm merge) — never a mandate to migrate everything now.

---

## 5. Cross-references

- `xperiments/chtpmx11-refactor.md` — the full research trail this
  standard was distilled from: §1-3 the real history (mutaclysm's real
  mirror mechanism, the real chtpm/khtpm independence), §4-5 why the
  house never fully unified before and the real `dbhq_load_actors()`
  compliance finding, §6-8 the real, incremental path to this
  document's own §1 rule (culminating in §8's `Elem`-tree discovery).
- `TPMOS-COMPLIANCE-DEBT.md` §4 — the real, condemned `dbhq_load_
  actors()` case this standard's §3 rule 2 exists to prevent from
  happening again.
- `SKILLS.md` §2 — the real, existing shared-paint-layer/`nav_index`/
  `reusable_slot()` conventions this standard builds directly on top
  of, not around.
- `09-appendix/forensic-report-flicker.md` — the real, worked
  incident behind §3 rule 8: a generic khtpm window that repainted
  on grab-synthetic focus events, per-rectangle `Expose`, relayed
  mouse-moves, and a corrective-move feedback loop, because it never
  went through the marker/dirty gate the `g_is_db_hq` window already
  used. Full ruled-out list + verification recipe.
- `au-31/00-todo.md` / `01-manager-design.md` — the real, in-progress
  network-HQ-window work this standard now governs going forward
  (manager-first, per §3 rule 2, already the plan before this document
  existed).
