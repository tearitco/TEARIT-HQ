
# chtpm/khtpm/mutaclysm rendering-family research (read-only)

All paths below are relative to house root:
`44.xyz❤️‍🔥️00.17/` unless given in full. All claims are grounded in files actually
read this session; every section cites `file:line`.

---

## 1. Does `mutaclysm` (`101.mutaclsym🧟‍♂️️19.00/`) drive its CLI app and its X11/GL
app from the SAME layout file(s), or from two separate sets?

**Real project dir found**: `101.mutaclsym🧟‍♂️️19.00/` (there is also a separate,
much larger `101.mutaclsym🧟‍♂️️+18.0G/` sibling — not investigated, out of scope).

**Answer: one real layout file, but the "GUI" is a mirror of the CLI's rendered
TEXT, not a second renderer walking the same parsed layout tree.** There is no
shared abstract "frame of drawable primitives" struct that both a text-stringifier
and a rasterizer consume. What actually happens:

- `101.mutaclsym🧟‍♂️️19.00/system/orchestrator.c` (read in full) launches, from one
  `main()`, in this order: `system/renderer` (terminal path, line 295-296),
  optionally `system/chtpm_parser_pal` if `PAL_LAYOUT` is set (line 298-303), and
  `system/chtpm_rgb_render` if present (line 305-309) — i.e. the orchestrator is
  the one place that fans a single session out to multiple long-running
  processes, matching `SIMLINK_PITFALL.md`'s own description of "system/renderer.c
  and system/gl_mirror.c are already long-running side-processes."
- `system/chtpm_parser_pal.c` (header comment, lines 34-70) is a **tracked fork**
  of real `1.TPMOS_c_+rmmp.0103.0001`'s `pieces/chtpm/plugins/chtpm_parser.c` — it
  parses the real `.chtpm` layout and writes the fully composed ASCII text
  (chrome + substituted `${game_map}` etc.) to
  `pieces/display/current_frame.txt`.
- `system/chtpm_rgb_render.c` (header comment, lines 1-52) does **not** parse
  `.chtpm` and has **zero semantic awareness of tags** ("Does NOT parse .chtpm...
  does not recognize `<cli_io>`, `<button>`, `<text>` tags", quoting its own header
  citing real `wraith_rgb_daemon.c`). Its actual job: watch
  `pieces/display/frame_changed.txt`, read the **already-fully-composed ASCII
  text** in `current_frame.txt`, and font-rasterize *every character of it* into
  an RGBA32 buffer (`pieces/display/rgb_frame.raw` +
  `rgb_frame.receipt.txt`). This is confirmed independently in
  `!.HOUSE_STDS.md:150-168`'s own pipeline diagram (see §3 below).
- `system/gl_mirror.c` (header, lines 1-60) is, by its own header comment, "the
  ONLY file in mutaclsym allowed to call GL/GLUT primitives" — its entire job is
  to poll `rgb_frame.raw` and blit it as one textured GL quad. It does not parse
  layout or game state at all.

So the real shape is: **layout file → parsed once by `chtpm_parser_pal` → ASCII
text buffer → (a) printed directly by `system/renderer` for the terminal, (b)
font-rasterized character-by-character into pixels by `chtpm_rgb_render`, then
blitted by `gl_mirror`.** The X11/GL "app" is a picture of the terminal's own
text output, not an independent GUI walking a widget tree — there is no button
hit-testing, no layout-aware pixel widgets, in this path at all. `!.HOUSE_STDS.md`
states this outright: "chtpm_rgb_render is a genuinely project-agnostic
daemon — it has zero game-state awareness, it just rasterizes whatever text is
in current_frame.txt."

- **A second, genuinely separate composition path exists for the game
  *world/tile* view**, and it is NOT layout-file-driven at all:
  `ops/compose_frame.c` (header, lines 1-8) reads raw game-state files
  (map/furniture/items/hero) and writes plain text to `current_frame.txt`
  directly (bypassing `.chtpm` entirely for this content). `ops/compose_rgb_frame.c`
  (header, lines 1-42) reads the **same underlying state files** — "byte-for-byte
  copied" camera-clamp formula, its own header says — and independently
  re-implements tile-to-pixel composition, writing directly to
  `rgb_frame.raw`. This is **duplicated composition logic over shared state
  files**, not a single composed structure consumed twice. `!.HOUSE_STDS.md:236-242`
  documents the real, live-caught race this creates when both `chtpm_rgb_render`
  and a project's own 3D op try to own `rgb_frame.raw` at once, and the fix
  (a separate overlay file + a literal `0x01` sentinel byte convention in
  `current_frame.txt` that `chtpm_rgb_render` special-cases) — itself proof this
  is file-based choreography between independent composers, not a shared IR.

**`SIMLINK_PITFALL.md`** (read in full) does not describe the frame-sharing
architecture itself — it's a symlink-elimination bug-fix log — but it corroborates
the process topology above end to end (bug #2 is specifically about
`chtpm_rgb_render.c` writing its RGB files to the wrong root; bug #3 is about
`orchestrator.c` launching `chtpm_parser_pal` via a bad relative path) and
explicitly states the file-bucket rule ("Session-local / ephemeral display
state ... `current_frame.txt`, `frame_history.txt`, `rgb_frame.raw`,
`rgb_frame.receipt.txt`") that only makes sense if this pipeline is real.

---

## 2. Real relationship between `chtpm_parser_pal.c`'s model and
`khtpm_entity_menu_render.c`'s model

**Answer: independent reimplementations, confirmed independent by the house's
own internal audit docs — with one notable, partial exception.**

- `khtpm_entity_menu_render.c` (found at
  `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c`, header read in
  full, lines 1-45) is raw Xlib/Xft — includes `X11/Xlib.h`, `X11/Xft/Xft.h`
  directly — has its **own** tag vocabulary (`<window class="...">`,
  `<page name="...">`, `<item label="..." action="...">`), its own attribute
  entity-decoder, and shares only `khtpm_render_core.c`/`khtpm_css_parser.h`
  with 3 other khtpm-family apps (db-hq/events-hq/chat-hai) — explicitly "the
  4th consumer of that shared core," per its own header.
- `!.HOUSE_STDS.md:509` states this as house-documented fact: "The newer
  `khtpm_*` family — raw Xlib/Xft rendering (no PAL VM, no `prisc+x`), its own
  from-scratch CSS-subset parser (`khtpm_css_parser.c`), its own `.chtpm` XML
  tag vocabulary parser (per-app...)."
- `!.HOUSE_STDS.md:514-516` is the load-bearing evidence for "no shared
  behavior": `chtpm_parser_pal.c`'s layout model is "text-grid, no CSS box
  model at all" — "a DIFFERENT kind of layout altogether, not a subset/superset
  relationship" — and `khtpm_css_parser.c` doesn't implement flex/position at
  all despite being nominally CSS. Line 519 makes the house's own conclusion
  explicit: **"Do not casually assume the two families share behavior, bug
  fixes, or feature support just because they share a file extension and
  superficial tag names."**
- `!.HOUSE_STDS.md:523` states the two families have never been merged and
  spells out what a real merge would require: "(a) porting khtpm_*'s real,
  working feature set... into chtpm_parser_pal.c and retiring the newer
  family, or (b) building out khtpm_css_parser.c into a real box-model/flex
  engine... a real architectural decision for a future session."
- `khtpm-merge-how2.md` (read in full) documents a **narrower, already-executed**
  merge: 5 khtpm_* window apps (entity-menu, taskbar-settings, db-hq,
  events-hq, chat-hai) were consolidated into ONE compiled binary
  (`khtpm_entity_menu_render.c`, mode-selected via a `class=` attribute), per
  its "Stage 5 (literal single-binary merge) is DONE for all 5 window apps"
  status line. This is real, evidenced consolidation — but it is consolidation
  **within** the khtpm family, not between chtpm and khtpm.
- The house's own **decision rule** (khtpm-merge-how2.md lines 57-122,
  quoting a direct instruction) explicitly forbids the naive fix the team
  first tried: sharing logic via `#include`-ing one `.c` file into N binaries
  ("shared SOURCE, not shared BINARY... the exact same anti-pattern") — and
  gives a real 3-bucket rule (independent-lifecycle process → fork/exec;
  discrete one-shot action → standalone op binary invoked via `system()`;
  pure hot-path logic needing direct access to live memory → the one
  legitimate `#include` case). This rule is directly relevant to any future
  unification work (§4).

**The partial exception**: `khtpm-strip-parser-SCOPE.md` (read in full,
`*.monads/*.livedesk-taskbar/khtpm-strip-parser-SCOPE.md`) shows that when the
taskbar's own strip parser was *designed*, its author explicitly modeled the
tag vocabulary and the `ACTIVATE`/`active_index`/`focus_index` scope mechanism
directly on `chtpm_parser.c`'s real behavior ("Modeled directly on
`chtpm_parser.c`'s real tags (confirmed by reading the actual source... reduced
to exactly what a taskbar strip needs" — line 21; "ported directly" from
`chtpm_parser.c`'s `is_navigable()`/`active_index` handling — line 35). So
**tag-vocabulary and nav-scope semantics were consciously borrowed by design**
in at least this one khtpm file, even though the two families remain
separately-parsed, separately-compiled, non-interoperable code today. This is
a real point of conceptual (not code) kinship the report should not omit —
it complicates a flat "fully independent" answer.

`!.HOUSE_STDS.md:51-53` (from `khtpm-merge-how2.md`'s own summary at line
51) also records: "The taskbar's own `LayDoc`/`khtpm_strip_layout.h`
architecture stays intentionally separate from Elem/CSS (a confirmed,
deliberate stop, not unfinished work)" — i.e. even inside the khtpm family
there are two more parallel layout mechanisms (`LayDoc` vs `Elem`/CSS) that
were deliberately NOT merged. The chtpm/khtpm split is not the only
unresolved duplication in this house.

---

## 3. Does a real "parse once into a renderer-agnostic frame, output to both
ASCII and RGB pixels" pattern exist (wraith-alpha / mutaclysm)?

**Answer: partially real, but not in the shape the question describes.**
There is no shared abstract intermediate representation ("a frame of drawable
primitives") anywhere in the files read. What is real:

1. **A shared, load-bearing pipeline built on plain files, not an in-memory
   IR.** `!.HOUSE_STDS.md:150-162` (§B, "The rendering/display pipeline, end
   to end") gives the full, real chain, read in full:
   ```
   your compose op (writes text)
     → pieces/apps/player_app/view.txt
     → pieces/display/current_frame.txt   (chtpm_parser_pal's composed text)
     → system/renderer                    (ASCII terminal path)
     → system/chtpm_rgb_render            (font-rasterizes current_frame.txt)
         → pieces/display/rgb_frame.raw + rgb_frame.receipt.txt
     → system/gl_mirror                   (GLUT window, blits rgb_frame.raw)
         → pieces/display/gl_display.receipt.txt
   ```
   This genuinely is "one text buffer, two real display outputs" — but the
   thing that's shared between the ASCII output and the pixel output is a
   **rendered TEXT STRING** (`current_frame.txt`), not a structured,
   renderer-agnostic list of drawable primitives (rects/text-runs/colors)
   that each backend interprets independently. The RGB path's entire job is
   literally "screenshot the text as a font bitmap" — it re-derives pixels
   from ASCII characters, it does not receive shapes/positions/colors as
   data.
2. **The receipt+checksum pattern is real and confirmed, exactly as
   described.** `!.HOUSE_STDS.md:167` (read directly): "The receipt +
   checksum pattern (`rgb_frame.receipt.txt`'s `frame_w`/`frame_h`/
   `checksum_fnv1a64`, `gl_display.receipt.txt` for the GL-upload side) is
   real, working, and load-bearing — `gl_mirror.c` reads its OWN
   window/texture dimensions from the receipt dynamically rather than
   hardcoding them, specifically because two different renderers can write
   different-sized frames to the same path." Confirmed independently by
   reading `gl_mirror.c`'s own header (lines 43-60, "the receipt-writing
   pattern... write_gl_display_receipt(), called from the same three sites
   wraith_gl.c calls it from") and `chtpm_rgb_render.c`'s own header
   (explicitly explaining why FRAME_W/FRAME_H changed from 640x304 to
   640x768 and that the receipt lets `gl_mirror.c` size itself dynamically).
3. **A `gl_mirror.c`/x11-mirror-shaped shim is real** and is explicitly
   scoped to "the ONLY file... allowed to call GL/GLUT primitives" (its own
   header, line 1-6), a direct, confirmed instance of the "GL→X11
   display-shim migration" pattern referenced in the task brief — it is a
   near-verbatim, deliberately-stripped-down port of real `1.TPMOS`'s
   `wraith_gl.c` (from that same header comment).
4. **The game-tile 3D/2D view is a genuinely separate, second composition
   path that never touches `current_frame.txt` or the `.chtpm` layout at
   all** — `ops/compose_frame.c` and `ops/compose_rgb_frame.c` (§1 above)
   independently re-derive their output from raw game-state files, with
   duplicated (not shared) composition math. This is the part of the
   pattern that is NOT a clean "one parse, two renders" — it's two
   hand-written renderers of the same underlying game state, kept in sync
   by convention/comments, not by a shared function or struct.

**Honest verdict**: the *display transport* half of the claim (one process
produces a buffer, N mirror processes blit it, verified by receipts) is real,
documented, and working. The *composition* half (one abstract, renderer-
agnostic "frame of primitives" parsed once from a layout file and consumed by
two backends) is **not** real for the `.chtpm`-layout path — what's shared
there is already-rendered ASCII text, and the RGB backend is a text-to-bitmap
rasterizer, not a primitive-list interpreter. It is also not real for the
game-tile path — that path has two independently-written composers over
shared state files, explicitly flagged by the house's own docs as a source of
races (`!.HOUSE_STDS.md §E.1`) rather than a clean shared abstraction.

---

## 4. What would it take to unify the house on one layout format + parser?

Given §1-3, the premise needs to be narrowed before it can be answered
concretely: there are really **three** separate things that could be
"unified," not one, and they are at very different levels of readiness.

### 4a. Unifying "one text buffer feeds two display backends" (mutaclysm's
real pattern) — already essentially done, cheap to extend
This part of the pattern is real, working, and already documented as the
house standard for adding new renderers (`!.HOUSE_STDS.md:167`: "If you add a
new renderer, follow this pattern"). Any new backend that wants to mirror an
existing ASCII-driven app just needs to: watch the same two trigger files
(`frame_changed.txt`, `renderer_pulse.txt`), read `current_frame.txt`, and
write its own `<name>.receipt.txt`. This is genuinely low-cost, incremental,
app-by-app, and does not require touching `chtpm_parser_pal.c` or `khtpm_*`
at all. **This is the one piece of the user's premise that is solidly true
and already extensible with no design work needed.**

### 4b. Unifying `chtpm_parser_pal.c` and `khtpm_*` onto one parser/tag
vocabulary — the house has already scoped this and explicitly left it
undone
`!.HOUSE_STDS.md:523` names the two real options directly (quoted in §2
above) and calls the decision itself "a real architectural decision for a
future session, not something to attempt inside a feature-shipping task" —
i.e. the house's own position, as of the most recent doc read, is that this
is scoped but intentionally not started. Concretely, what would have to be
true for either direction to work:

- **Direction (a), port khtpm's features into `chtpm_parser_pal.c` and
  retire khtpm**: `chtpm_parser_pal.c`'s layout model is a text-grid with no
  box model (`!.HOUSE_STDS.md:514`) — it has no concept of pixel
  positioning, drag, WM-managed X11 windows with `_MOTIF_WM_HINTS` (see
  `!.HOUSE_STDS.md:398`, a real, hard-won X11-specific fix), XDND drop
  targets, or per-pixel entity sprites (`sprite.csv`, §23 at line 424) — all
  real, load-bearing khtpm features with no ASCII/text-grid equivalent at
  all. Porting these into a text-grid engine is not a parser change, it's
  building an entirely new rendering backend underneath the existing text
  semantics — functionally a rewrite of `chtpm_parser_pal.c`'s output stage,
  not an extension of it.
- **Direction (b), build out `khtpm_css_parser.c` into a real flex/box
  engine and formally deprecate `chtpm_parser_pal.c`'s pixel-math
  `layout_pass()`**: `khtpm_css_parser.c` currently doesn't implement
  `display`/`position`/`flex` at all (`!.HOUSE_STDS.md:514`) — every khtpm
  app hand-rolls pixel layout in its own `layout_pass()`. Chtpm's ASCII
  concepts do NOT lack an X11 equivalent as cleanly as this section
  originally claimed — **CORRECTED 2026-08-31, a real error caught by
  direct user challenge, not by re-reading on my own initiative**: both
  `<module>` AND `<interact src=...>` are real, live, already-working on
  the khtpm/X11 side too:
  - `<module src="...">` is used verbatim in `&.hq-apps/chat-hai/
    chat-hai.chtpm` (launches `chat_hai_loop.sh`) and in db-hq's own
    `dashboard.chtpm` (launches `khtpm_hq_manager.c` via
    `dbhq_launch_module()`, itself "ported verbatim from
    `wraith_parser_alpha.c`'s own `launch_module()`" per that function's
    own header comment) — the SAME real tag, same real fork+execv
    mechanism, independently implemented in the khtpm family, not
    absent from it.
  - `<interact src="..."/>` is real and live for piececraft-hq/
    board-viewer: `khtpm_entity_menu_render.c` (~line 10182-10214, its
    own header comment, citing a real design doc
    `PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md`) states
    outright: "board_viewer.chtpm has a real, declarative
    `<interact src="..."/>` ... the ENGINE (`chtpm_parser_pal.c`)
    handles ALL real nav/focus/arrow-relay/ESC natively" — and the
    khtpm side (`pchq_append_key()`, `pchq_write_click_kv()`,
    `pchq_map_special_key()`) is an explicit "direct port of
    `x11_mirror.c`'s own `append_key()`/`write_click_kv()`/
    `map_special_key()`" — i.e. the SAME real relay-forwarding
    mechanism as mutaclysm's own mirror pattern (§3 above), independently
    re-implemented for this one khtpm mode.
  **Real, corrected conclusion**: the semantic gap between the two
  families is genuinely smaller than originally reported — both
  `<module>` and `<interact>` have real, independently-coded khtpm-side
  equivalents already proven live for at least one app each. What's
  still real and unresolved: this is duplication (independently
  re-implemented per mode, not shared code), not a shared parser or a
  documented, generalized "any khtpm mode can declare `<interact
  src=...>`" contract - `pchq_append_key()` etc. are specific to the
  `g_is_pchq_board` mode, not a generic mechanism every khtpm window
  gets for free the way every chtpm-family app does. `cli_io`'s
  text-buffer-cursor model genuinely still has no khtpm equivalent -
  that specific claim stands uncorrected.
- Either direction is realistically a **flag-day per app**, not incremental:
  an app is either "a `.chtpm` file parsed by `chtpm_parser_pal`" or "a
  `.chtpm`-shaped file parsed by a khtpm-family binary" — the two parsers
  don't currently read each other's files at all (confirmed: khtpm's own
  `.chtpm` files use a completely different, incompatible tag set per
  `khtpm_entity_menu_render.c`'s own header, §2 above), so migrating one app
  means literally rewriting its layout file's tags and re-testing its whole
  input/nav model, not a config flag.

### 4c. The one thing that already generalizes safely: the house's own
3-bucket sharing rule
Regardless of which merge direction (if any) is chosen, `khtpm-merge-how2.md`
lines 97-122 (quoted in §2) already gives a real, house-approved rule for how
to share code without repeating the earlier `#include`-into-N-binaries
mistake: long-lived independent processes → `fork()`+`execv()`; one-shot
actions → a standalone op binary invoked via `system()`; pure hot-path logic
needing the caller's live memory → the one legitimate `#include`-a-`.c`-file
case. Any real unification plan should be built on this rule from the start
rather than reproducing the same "looks deduplicated, still N binaries"
mistake documented in that same section.

### Concrete, incremental plan (bounded to what the real evidence supports)
1. **Do nothing to unify chtpm and khtpm's tag vocabularies or parsers.**
   The house's own docs already scoped this and did not schedule it; the
   real, cited feature gaps in both directions (§4b) mean a clean unification
   is not a small project — it is at minimum two large, mostly-independent
   engineering efforts (a box-model engine for khtpm, or a pixel-window
   backend for chtpm), each risking regressing a real, currently-working
   family. Not worth doing house-wide on current evidence.
2. **If a specific app genuinely needs both a terminal and a window,
   copy mutaclysm's real §4a pattern as-is**: keep it as a `.chtpm`
   file parsed once by `chtpm_parser_pal`, add `chtpm_rgb_render`+`gl_mirror`
   (or the house's already-existing shared `x11_mirror.c`, per
   `khtpm-merge-how2.md:47` — "3 of 16 projects converted to the shared
   `x11_mirror.c` binary, 13 remain," tracked in `legacy-shared-fix.md`,
   not read this session). This is real, already proven, and does not touch
   khtpm at all.
3. **The already-in-flight, much narrower migration to actually finish** is
   the one named in `khtpm-merge-how2.md:47`: converting the remaining 13 of
   16 legacy GL-window projects onto the shared `x11_mirror.c` binary. That
   is real, scoped, already 3/16 done, and does not require inventing a new
   architecture — it's the same "one shared binary, many consumers" pattern
   already validated for the 5-app khtpm merge (Stage 5, done).
4. **Do not create new in-house `.h` headers or `#include`-shared `.c` files
   as a shortcut** for any future consolidation work — `khtpm-merge-how2.md:
   529-531` records this as an already-corrected mistake with a direct user
   instruction against it.

### What would contradict or complicate the premise (explicitly flagged)
- The user's framing describes mutaclysm as parsing a layout "once into an
  abstract frame... handed to two different output paths." The real
  mechanism is closer to "parse once into TEXT, blit the text as a picture
  of itself" — a meaningfully weaker and more special-cased pattern than a
  renderer-agnostic primitive-list IR. A house-wide unification premised on
  "we already have a clean IR, just need to reuse it" would be building on
  a pattern that does not exist yet.
- The game-tile/3D view (the part of mutaclysm most people would call "the
  actual game," not just chrome) does NOT go through this shared-text
  pipeline at all — it's two independently-hand-written composers
  (`compose_frame.c`, `compose_rgb_frame.c`) over the same state files, kept
  in sync by comments and a hand-maintained sentinel-byte convention, with a
  documented real race condition when both try to own the same output file.
  Calling this "one source of truth, two renderers" overstates how safely
  reusable the pattern is.
- khtpm is not monolithic either: even within the "already merged" family,
  `LayDoc`/`khtpm_strip_layout.h` (taskbar) was deliberately kept separate
  from `Elem`/CSS-based khtpm (`khtpm-merge-how2.md:51-53`), and
  `khtpm_open_hai_render.c` was deliberately kept out of the 5-app merge
  (`khtpm-merge-how2.md:26`). A "unify the whole house" plan needs to reckon
  with at least 3 living layout mechanisms today (chtpm-text-grid,
  khtpm-Elem/CSS, khtpm-LayDoc-strip), not 2.
- One genuine point of hope the report should not undersell: the taskbar
  strip parser's design doc (`khtpm-strip-parser-SCOPE.md`) shows that when
  someone deliberately set out to design a *new* khtpm-family parser, they
  chose to port `chtpm_parser.c`'s real tag vocabulary and nav-scope
  semantics (`ACTIVATE`/`active_index`) rather than invent something new.
  That is evidence that chtpm's *semantic model* (not its text-grid
  rendering) is considered good/portable house-wide, even by the people
  building the "separate" khtpm family — a future unification, if ever
  undertaken, has a real precedent to build the shared semantic layer from,
  even though no code sharing happened.

---

---

## 5. Should `dbhq_load_actors()`-style inline hardcoding be fixed FIRST,
before any of the above?

**Yes — direct instruction confirms this, and the reasoning holds up
independent of whether any unification work above ever happens.**

`dbhq_load_actors()` (in the shared `khtpm_entity_menu_render.c`)
loads real PDL data (`&.widgits/db-hq/data/actors.pdl`) but does so
INLINE in the shared "hard boundary" renderer file, instead of via a
real, separate manager publishing a projection - a real "Manager owns
projection" violation (TPMOS §11/§12), condemned in
`TPMOS-COMPLIANCE-DEBT.md` §4 (added same day as this report) and
`INDEX.md`. Not fixed yet.

**Why fixing this first is conducive to a deeper refactor later, not
just a compliance nicety**: any future attempt to touch, extend, or
reason about `khtpm_entity_menu_render.c`'s own real class-detection/
rendering core (exactly what §4 above would require, in EITHER
direction) has to carry along every inline business-logic function
already bolted onto that file as unavoidable baggage - `dbhq_load_
actors()` and any undiscovered siblings (Classes/Skills/Items/etc.,
not yet audited) make the file bigger, more coupled, and riskier to
touch with every mode added. Pulling per-mode data logic OUT into real,
separate manager processes (matching `khtpm_hq_manager.c`'s own already
-proven shape) shrinks the shared file back down to real "parse tags,
render tags, dispatch clicks" - the part that would actually need to
change in a real unification attempt - and removes noise that has
nothing to do with the parser/renderer itself. Getting managers into
"semi proper, modular form" this way is real progress toward §4's own
precondition even if no unification work ever happens: real, testable
Ops instead of inline logic is the house's own standing standard
(TPMOS §12) regardless of any refactor ambition, and remains real,
compliant, standalone value on its own.

**Real, scoped next step** (already recorded in `au-31/00-todo.md`
before this report, cross-referenced here): build the real per-mode
manager for `dbhq_load_actors()` (and audit for siblings) BEFORE
adding any new window mode to the shared file - not a blocking
prerequisite for every future change everywhere, but specifically for
touching `khtpm_entity_menu_render.c`'s own shared core again.

---

## 6. Headless/CLI mirroring: how to make the RGB compositor "pretty"/
CSS-aware, incrementally, without losing ASCII/CLI parity

**Real constraint stated directly**: headless/CLI functionality
mirroring (driving and verifying an app via plain text, no X11 needed)
is valued and must NOT be lost. The historical "frame → RGB composite"
approach's one real, named flaw was that it wasn't "pretty"/CSS-aware
(confirmed in §1/§3 above: `chtpm_rgb_render.c` is a blind
character-by-character font rasterizer, zero tag/box/color/border
awareness) - not that the architecture itself was wrong.

**Real principle for every step below**: `current_frame.txt` (the
plain ASCII text) stays the permanent, unconditional source of truth
for CLI/headless mirroring, forever, at every step - nothing proposed
here ever requires it to change shape or gain markup. Each step is
purely ADDITIVE to the RGB half of the pipeline, and each step is
independently a real, live-testable improvement - stop at any step
without needing the next.

**Step 0 (already true today, real baseline)**: plain terminal
printing of `current_frame.txt` and the existing `chtpm_rgb_render.c`
both already coexist off the SAME real text buffer, verified in §1/§3.
Nothing to build - this is the real floor everything below builds on.

**Step 1 - reserved marker characters, opt-in, backward compatible**:
have `chtpm_parser_pal.c` optionally emit a small, real, reserved
non-printable-range marker sequence (e.g. a private-use Unicode
codepoint or a `\x01`-style control byte, matching the SAME real
"sentinel byte convention" `!.HOUSE_STDS.md §E.1` already documents
`chtpm_rgb_render.c` special-casing for the 3D-overlay race fix - not
a new idea, extending a real, already-proven mechanism) around a
`<button>`/`<panel>`'s own composed text region, carrying just an
element id/class. A plain terminal (or anything ignorant of the
marker) either strips it before printing (one small, real, additive
change to `system/renderer.c`'s own print step) or - if left
unstripped - a real control byte a terminal already ignores/no-ops on,
so this is safe even before the strip step lands. `chtpm_rgb_render.c`
reads the SAME marker to know "these next N characters belong to
element id=X" without parsing `.chtpm` itself.
**Real, scoped win**: element boundaries become real, known pixel
rects during rasterization - the composite would already know WHERE
elements are, decoupled from styling them.

**Step 2 - reuse `khtpm_css_parser.c` (already real, already exists,
already house-standard) inside `chtpm_rgb_render.c`**: once step 1
gives real per-element rects, have `chtpm_rgb_render.c` optionally load
a real, per-project `.css` file (the SAME real CSS-subset parser the
khtpm family already uses, not a new one) keyed by the marker's own
class/id, and draw a real background-color fill + border BEHIND the
rasterized text for that rect, before blitting the characters on top
- real box/color/border styling, zero widget/hit-testing logic added,
zero change to text positions (the ASCII grid math is untouched).
**Real, scoped win**: buttons/panels get real visual boxes/colors
without `chtpm_rgb_render.c` ever becoming tag-aware - it only ever
consumes "rect + class name," the same "zero semantic awareness of
buttons" property from §1/§3 stays essentially true (it still doesn't
know what a button DOES, only how to paint the box a marker told it
about).

**Step 3 - a parallel, optional positional side-file (longer-term,
still incremental, still opt-in)**: `chtpm_parser_pal.c` optionally
writes a second, small, real file alongside `current_frame.txt` (e.g.
`current_frame.regions.txt`, real `id|class|x|y|w|h` rows, computed
from the SAME real per-character pixel math the text grid already
uses) instead of embedding markers in the text stream itself. This is
a real, cleaner decoupling once step 1/2 are proven - `chtpm_rgb_
render.c` reads text + regions as two plain files, still zero .chtpm
parsing, still zero widget logic, but no marker bytes riding inside
the text buffer at all (a real, valid concern if some future consumer
of `current_frame.txt` is byte-sensitive).

**Honest limits, stated plainly**: none of steps 1-3 turn `chtpm_rgb_
render.c` into a real box-model/flex layout engine, and none of them
add real widget interactivity to the RGB path itself (clicks/keys
still forward into the SAME real relay files the ASCII engine already
owns, per the mutaclysm/board-viewer mirror pattern in §1/§3 - the RGB
window stays a real, accurate MIRROR with styled paint, not an
independent GUI). If genuine sidebar+panel HQ-style widgets
(hover states, real hit-testing per element, not just per-character
click-forwarding) are wanted instead, that is real, separate khtpm-
family work (§4's own new-manager-plus-window-mode path), not an
extension of this pipeline - the two are complementary real options
for different real goals (a styled MIRROR of an existing CLI app vs. a
genuinely native HQ window), not two ways to reach the same result.

---

---

## 7. "Styled mirror" vs. "native widget GUI" - what this actually means
(added because §6's closing line was confusing without this)

These are two GENUINELY DIFFERENT products, not two implementations of
the same result. Concretely, using IRC chat's real "Send" button as
the example:

**Styled mirror** (§6's own subject - `chtpm_rgb_render.c` +
`x11_mirror.c`/`gl_mirror.c`): the X11 window is a real-time PICTURE of
the same text-mode app, never a second program that understands the
UI.
- The real "brain" - what the Send button does, which room is active,
  what Enter does - lives entirely in the one real, running
  `chtpm_parser_pal.c` process. That never changes, no matter how much
  §6's steps get built.
- The X11 window displays whatever image got composed (plain
  monospace today; real colored boxes/borders after §6's steps) and,
  on a click or keypress, does NOT figure out "the Send button was
  clicked" itself - it just writes the raw key or raw x/y coordinate
  into the SAME relay files `chtpm_parser_pal.c` already reads (the
  real, proven mechanism §1/§3 document for board-viewer/mutaclysm:
  `pchq_append_key()`/`pchq_write_click_kv()`, direct ports of
  `x11_mirror.c`'s own functions). `chtpm_parser_pal.c` is the one
  real process that decides what that click meant.
- **The X11 process itself has zero concept of "this rectangle is a
  button."** It is a screen plus an input forwarder, nothing more -
  even with full §6 styling applied.

**Native widget GUI** (what db-hq/chat-hai/palettes/bookmarks/
stats-hq actually are, via `khtpm_entity_menu_render.c`): the X11
process itself parses the layout into real Elem objects (a real
Button at a real known rectangle) and does its OWN real hit-testing -
"the mouse landed at (340,82), that's inside the Send button's own
rect, run its `onclick`" - directly, in the same process, using
`hit_test()`/`draw_elem()`/`render_tree()` (the shared paint layer,
§2/SKILLS.md §2). The real "brain" for THAT click lives inside the
X11 program itself, not in a separate text-mode engine it forwards
raw input to.

**Why these are not two roads to the same destination**:
- A styled mirror can look progressively better (§6's whole point) but
  will NEVER gain real per-element hover states, real nav-index
  badges, or interaction responsiveness independent of the ASCII
  engine's own poll/redraw cadence - because the X11 side genuinely
  never learns what a "button" is, by design (that's what keeps
  `chtpm_rgb_render.c` simple and keeps CLI/headless mirroring free -
  §6's own real tradeoff).
- A native widget GUI genuinely feels/behaves like every other real HQ
  window in this house, but is real, separate new work - a real
  manager + a real khtpm `.chtpm` layout (§4/§5's own subject) - and
  becomes a SECOND place that has to agree with the CLI app's own real
  business rules (in practice this usually means the manager shells
  out to the SAME real ops the CLI app already uses, rather than
  re-deriving logic - but it is still a second integration point to
  build and keep correct, not free).

**Practical decision rule this suggests**: pick styled-mirror when the
goal is "let me see/drive the existing CLI app from a window, ideally
looking nicer than raw monospace" (near-zero new code, real CLI parity
is automatic since it's the SAME engine). Pick native-widget-GUI when
the goal is "I want this to look and behave like every other HQ window
in this house" (real new work, real compliance discipline per §5,
but a genuinely native result). Neither one is a smaller version of
the other - they answer different real questions.

---

---

## 8. The real bridge: khtpm's OWN `Elem` tree already IS the shared
"cross-reusable centroid" - it just needs a second renderer

**Real correction/refinement of §6, direct instruction: "why cant it
be made to intentionally look like the x11 gui we currently have...
transcending the x11 primitives by being inspired by them but drawing
them ourselves using rgb or just ascii."** §6 was bolting pixel
styling ONTO the weaker of the two real models (`chtpm_parser_pal`'s
plain text-grid, no box model at all - confirmed §2/§4b). The user's
question points at the OPPOSITE, cleaner direction: khtpm's OWN `Elem`
tree is already the richer model, and it already has everything a
real "renderer-agnostic frame of drawable primitives" needs -
confirmed by direct read, not assumption:

```c
/* khtpm_render_core.c, real Elem struct, verbatim */
struct Elem *children[MAX_CHILDREN]; int n_children; struct Elem *parent;
int x, y, w, h;   /* REAL, computed pixel position/size - layout_pass() */
CssStyle style;   /* REAL background-color/color/border-color/border-width -
                     khtpm_css_parser.c genuinely parses these properties,
                     confirmed: apply_style()'s own real background-color/
                     color/border-color/border handling */
```

This is exactly the abstract, positioned, styled rectangle-tree the
user originally asked about in the very first question of this whole
report (§3) - it just already exists, on the khtpm side, not as
something to invent. Today exactly ONE renderer consumes it -
`draw_elem()`/`render_tree()` (real Xlib/Xft pixels, the shared paint
layer per `SKILLS.md` §2). Nothing else reads this tree.

**The real bridge, concretely**: add a SECOND renderer -
`ascii_draw_elem()`/`ascii_render_tree()` - that walks the SAME real
`Elem` tree (same `parse_chtpm()`/`layout_pass()`/CSS pass, zero
duplicate parsing) and, instead of Xlib pixel calls, does:
- scale each Elem's real `x/y/w/h` from pixels down to character
  cells (divide by a real, fixed cell-width/cell-height constant -
  the same real "measure_text_px" reasoning `SKILLS.md` landmine #7
  already established for pixel-accurate text boxes, just inverted),
- draw real box-drawing characters (`┌─┐│└┘` or plain `+-|` for a
  dumb-terminal fallback) for any Elem whose `style.has_border_width`/
  `has_bg_color` is set,
- place the Elem's own real `label` text at the right character cell.

**Why this is the real "pure, solid, cross-reusable centroid" the
user is asking for, not a rename of §6's idea**: the SAME real parse +
layout + CSS pass now drives BOTH outputs - no text-buffer
intermediary, no blind character rasterization, no duplicated
composition logic (the real §1/§3 complaint about the CURRENT
mutaclysm pipeline: two independently hand-written composers,
`compose_frame.c`/`compose_rgb_frame.c`, kept in sync by convention).
One real tree, one real layout pass, two thin, purely mechanical
renderer functions consuming identical data. This IS what "transcend
the X11 primitives, draw them ourselves in RGB or ASCII" concretely
means: `draw_elem()` and `ascii_draw_elem()` become two real,
symmetric interpreters of the exact same `Elem.style`/`Elem.x/y/w/h`
data, not one being a screenshot of the other.

**Real scope boundary, stated plainly (this is NOT a free retrofit of
existing chtpm-native apps)**: this bridge is real and buildable for
apps built ON khtpm's `Elem`/CSS model going forward - e.g. the
network HQ windows (§4/§5) would get a real native X11 GUI AND a real
ASCII/headless mirror from the SAME one manager+layout, for free, day
one. It is NOT a drop-in migration path for IRC/Forum/Chain as they
exist today - their real business logic (login flow, room state,
posting) lives in the mature `chtpm_parser_pal`/PAL-VM engine
(§2/§4b), and putting that logic under khtpm's `Elem` tree instead
means genuinely re-implementing it there (the real "native widget
GUI" work already scoped in §7), not a mechanical swap. The real,
honest value of this bridge for the EXISTING ASCII-native apps is
narrower: it doesn't help them directly, but it retroactively confirms
`chtpm_parser_pal`'s own text-grid model (§2/§4b) really is the
weaker of the two real models in this house, not an equally-valid
peer - a real, useful data point for §4's own eventual "which
direction" decision, even though nothing needs to change for those
apps today.

**Concrete, buildable next step, not started**: build
`ascii_draw_elem()`/`ascii_render_tree()` as a real, standalone
addition to `khtpm_render_core.c`'s own family (a new, thin renderer
function, not a new parser or a new Elem field), prove it live against
ONE already-existing khtpm window (e.g. run db-hq's own real,
already-parsed `Elem` tree through it and diff a real terminal dump
against a real screenshot of the same window) before wiring it into
any NEW app's manager - this validates the real "same tree, two
renders" claim on a real, already-correct tree before depending on it
for new work.

---

## File/line index (for a follow-up session)

- `101.mutaclsym🧟‍♂️️19.00/SIMLINK_PITFALL.md` — full file read
- `101.mutaclsym🧟‍♂️️19.00/system/orchestrator.c` — full file read (launch order, lines 295-317)
- `101.mutaclsym🧟‍♂️️19.00/system/chtpm_parser_pal.c` — header comment, lines 1-110
- `101.mutaclsym🧟‍♂️️19.00/system/chtpm_rgb_render.c` — header comment, lines 1-52
- `101.mutaclsym🧟‍♂️️19.00/system/gl_mirror.c` — header comment, lines 1-60
- `101.mutaclsym🧟‍♂️️19.00/ops/compose_frame.c` — header + top, lines 1-80
- `101.mutaclsym🧟‍♂️️19.00/ops/compose_rgb_frame.c` — header + top, lines 1-80
- `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c` — header, lines 1-79
- `*.monads/*.livedesk-taskbar/khtpm-strip-parser-SCOPE.md` — lines 1-40
- `44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md` — lines 130-242 (pipeline + 3D overlay race),
  lines 495-531 (khtpm-vs-chtpm family history and decision rule),
  line 398 (WM-managed X11 window fix), line 424 (sprite.csv portrait loading)
- `#.#.✅️.cal-user-sum/1.^V-hq/khtpm-merge-how2.md` — full file read (94 lines)
- `101.ledger-player-npc-simple+3/system/chtpm_parser.c` — top of file (confirmed real location of the non-PAL chtpm_parser.c referenced by house docs)
- `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c` — lines ~10182-10214 (real `<interact src="..."/>`/`<module>` X11-side equivalents, §4b correction), `dbhq_load_actors()` (§5, exact line not re-cited here, see `TPMOS-COMPLIANCE-DEBT.md` §4)
- `&.hq-apps/chat-hai/chat-hai.chtpm` — line 30 (`<module src="..."/>` real usage)
- `&.widgits/db-hq/data/actors.pdl` — real data file `dbhq_load_actors()` reads (§5)
- `TPMOS-COMPLIANCE-DEBT.md` §4 — the real, condemned `dbhq_load_actors()` finding, added same day as this report
- `!.HOUSE_STDS.md` §E.1 — the real sentinel-byte convention `chtpm_rgb_render.c` already special-cases (cited as real precedent for §6 step 1's marker-byte proposal)
- `&.hq-apps/chat-hai/ops/khtpm_render_core.c` — lines 73-100, the real `Elem` struct (`x,y,w,h`, `CssStyle style`) - §8's own real "already-existing centroid" evidence
- `&.hq-apps/chat-hai/ops/khtpm_css_parser.c` — lines ~14-70, real `background-color`/`color`/`border-color`/`border` property parsing (§8)

Not found / out of scope this session: a real `1.TPMOS_c_+rmmp.0103.0001/
projects/wraith-alpha/` directory was referenced repeatedly by the files above
but its own source was not located/read directly — all wraith-alpha claims in
this report are second-hand, drawn from other files' own header comments that
quote or describe it (`chtpm_rgb_render.c`'s header, `gl_mirror.c`'s header,
`khtpm-merge-how2.md`'s house-standard section). A future pass should locate
and directly read `wraith_rgb_daemon.c`/`wraith_gl.c`/`wraith_parser_alpha.c`
to confirm those second-hand descriptions.
