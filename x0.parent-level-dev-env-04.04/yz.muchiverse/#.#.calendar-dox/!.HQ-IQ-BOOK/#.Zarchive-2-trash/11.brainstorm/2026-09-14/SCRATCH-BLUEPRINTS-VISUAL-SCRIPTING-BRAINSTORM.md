# Brainstorm: reviving Scratch/Blueprints visual scripting for events

**Status: brainstorm only, not scheduled, no code written.** Started
2026-09-14, growing out of the events-hq harness-hardening work the
same day (see `12.calendar/2026-09-14/` - the harness investigation
that found the Scratch tab's own rendering code had been deleted).
Real current-state findings + a reacted-to plan, not yet a committed
design. Anchors to the existing `08-roadmap/design-docs/PAL-VISUAL-
SCRIPTING-PLAN.md` (2026-08-26, vision + compile-target policy,
"nothing built yet" for the actual rendering) - this doc is the
follow-up brainstorm on the rendering side specifically, prompted by
discovering that side's history is more complicated than "never
built": it WAS built (2026-08-29, Scratch view real in the pre-merge
renderer), then deleted (exact commit not yet identified) when
project-specific mode blocks got pulled out of the shared
`khtpm_core_render.c` to satisfy this house's own "no per-project C in
the shared renderer" rule.

## Why this came up

Investigating why a test harness's `view_mode` never reached `1`
(2026-09-14, `12.calendar/2026-09-13/` tracking doc), direct code
search for the identifiers `dashboard.chtpm`'s own header comment names
(`g_evhq_view_mode`, `evhq_layout_pass()`, `events_hq_view_mode.txt`)
turned up zero matches in the current renderer. Further digging found
the actual fossil: a real, detailed comment describing "events-hq's
own view_mode==1 branch... the SAME real Scratch block-palette/
placement view" sitting immediately above an unrelated function
(`kh_nonfatal_x_error()`), followed by a literal `/* ====== end
events-hq mode block ====== */` marker - the implementing code is
gone, only the comment survived. The very next comment in the file
explicitly says "the now-deleted chat-hai mode block," confirming this
wasn't an events-hq-specific accident - it reads like a deliberate
cleanup pass that removed several per-project mode blocks at once.

The backend half is still alive: `khtpm_events_hq_manager.c`'s
`publish_scratch_blocks()` (real, current, 2026-08-28) still scans
freshly-compiled `event.pal` and emits `SCRATCHBLOCK|<key>|<ON|OFF>`
rows - but for exactly ONE recognized instruction shape (Control
Switch: `li x15,7 / li x12,<V> / ecall "...switches.txt" "<key>"`).
Nothing reads those rows anymore on the render side.

## Priority question, asked directly: before or after the trigger-layer task? Luxury or synergy?

**Answer: after. This is a luxury relative to the trigger layer, not a
synergy - they're orthogonal, and the trigger layer is the real gap.**

Reasoning:
- The trigger layer (NIGHT_05's proposal - a real in-game trigger
  dispatching to a Common Event) is a genuine, currently-*missing*
  capability: nothing in-game can fire a Common Event yet. This was
  already flagged as the real open item in the Sept 11 handout's own
  game-dev status check ("Events + DB wiring: ~30-40%... the trigger
  layer doesn't exist yet").
- Scratch/Blueprints is an *authoring experience* upgrade for events
  that can **already** be fully authored and fired today, via the
  existing Scripting tab (the plain RPG-Maker-style command list) -
  that path is real, current, unaffected by any of this. Nothing about
  the trigger layer needs Scratch to exist; nothing about Scratch needs
  the trigger layer to exist. No technical dependency either
  direction.
- The only structural link is that both eventually touch
  `event_commands.registry.pdl`'s PAL-vs-TEMPLATE compile-target
  policy (`PAL-VISUAL-SCRIPTING-PLAN.md`'s own "prefer PAL so it's
  visually mappable later" rule) - but that policy is ALREADY decided
  and already in effect for new commands regardless of whether the
  Scratch renderer itself exists. Building the trigger layer first
  doesn't block or complicate Scratch later.

Net: build the trigger layer first (it's the real missing capability;
also, per this same week's own repeated lesson - `03-pitfalls/
HOUSE_CODE_PITFALLS.md` #19 and the taskbar-perf work - smaller, real,
load-bearing fixes before larger speculative features). Scratch is real
and worth doing, but it's polish on an authoring path that already
works, not a blocker for anything.

## Real findings from checking the actual asset pack (not speculative)

User pointed at a real, external Scratch 3.0 block-image library
(PNG + SVG, organized by category: Control/Events/Looks/Motion/
Operators/Sensing/Sound/Variables/My Blocks). Checked it directly:

- **Use the PNGs, not the SVGs.** This house's rendering stack has no
  SVG rasterizer anywhere (no cairo/librsvg/nanosvg found in the whole
  codebase). It DOES already have a real, proven PNG-loading pipeline:
  `palettes_manager.c` already uses `stbi_load()` (4-channel RGBA,
  cached) for real tile/atlas assets today - the exact mechanism a
  block-image renderer would reuse, not something speculative. If an
  SVG-only asset is ever needed at a size the PNG set doesn't have,
  that's a one-time offline rasterize (rsvg-convert/inkscape), never a
  runtime dependency.
- **Keep our own RPG-Maker-style event vocabulary; borrow Scratch's
  visual grammar, not its commands.** Not a binary choice. Two real
  buckets:
  - Free, near-exact shape matches already in the pack: Loop ->
    `repeat`/`forever`, Conditional Branch -> `if-then`/`if-then-else`,
    Wait -> `wait`. Reusable close to as-is.
  - No equivalent in the pack (Show Text, Show Choices, Change Gold,
    Call Common Event - genuine RPG-Maker/house-specific content):
    needs either a generic "black-box op" block (styled like Scratch's
    own plain pink "My Blocks" category) or new custom shapes designed
    later, matching the pack's own corner-radius/notch/shadow grammar
    so they don't look bolted-on.
- **The real remaining risk for "will it look right" is label/param
  text, not the block shapes.** Shape rendering itself is low-risk
  (real assets + a proven load pipeline). Text needs to render INSIDE
  each block image at the right spot - either blank-slot PNGs with our
  own Xft text drawn on top, or per-instance pre-rendering. Untried,
  flagged, not yet a blocker.
- **Confirms "naive/primitive last time" precisely**: the manager's own
  code comment says the old renderer showed "labeled, bordered blocks
  instead of the static 'coming soon' stub" - i.e. it never got past
  plain bordered rectangles with text, never real jigsaw block shapes
  at all. Using the real asset pack is a categorical fidelity upgrade,
  not incremental.

## Open design question, still unresolved (asked, not yet answered)

Is Scratch-style and Blueprints-style one shared graph/data model with
two different skins, or two independently built renderers? (Also
flagged in `PAL-VISUAL-SCRIPTING-PLAN.md` itself as explicitly
undecided.) Everything past step 0 below depends on this - don't start
building past a skeleton until it's settled.

## Reacted-to staged plan (not yet started, not yet approved past discussion)

0. Settle the shared-model-vs-two-renderers question above.
1. Standalone binary skeleton (own process, own `.chtpm` - real
   precedent: `khtpm_choice_picker.c`-style, a genuinely new small app
   that still parses real `.chtpm`+CSS through the real pipeline, kept
   OUT of the shared `khtpm_core_render.c` per the house rule that got
   the last version deleted) that can render a small FIXED set of real
   block PNGs + labels. Pure visual proof, no live backend data yet.
2. Wire it to the one real data source that already exists
   (`publish_scratch_blocks()`'s Control Switch rows).
3. Extend `publish_scratch_blocks()`'s pattern-matching to the other
   free-match commands (Loop, Conditional Branch, Wait) one at a time.
4. Design custom block shapes for the no-equivalent commands (Show
   Text, Show Choices, Change Gold, Call Common Event) - separate,
   later scope.
5. Bidirectional edit -> `.pal` sync - explicitly out of scope until
   rendering itself is proven, per `PAL-VISUAL-SCRIPTING-PLAN.md`'s own
   existing "what is explicitly NOT decided yet" section.

## Next step

Not started. Waiting on: (a) the trigger-layer build-out to actually
happen first (see `12.calendar/2026-09-13/sept-13-events-trigger-
buildout-tracking.md`), and (b) an answer to the open design question
above before any code gets written here.
