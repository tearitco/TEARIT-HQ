Desk City-Formation Entity Display
====================================
Reference doc, 2026-09-14. VISION/DESIGN doc, written BEFORE any
implementation per direct instruction ("lets document this in dev log
before and while we do pls") - nothing here is built yet.

## 1. The direct instruction

"also in desks, we are going to show the different emojis for each
entity type on desk in a 'city' formation in the top center of the
'board'."

## 2. What this means, as best understood so far

A desk's own entities (each with a real glyph - cursword 🗡️, castle 🏰,
door_civ 🚪, etc.) get a visual grouped/clustered display, arranged in
a "city" formation (a skyline/cluster-of-buildings-style layout, not a
plain grid), positioned at the top-center of the desk/board area. This
is a real, new desk-wide DISPLAY convention, not specific to any one
game (Civilization, DSR, etc.) - applies to `civ-test`, `office`, and
every desk going forward.

## 3. Real questions, unresolved - document before deciding, not after

- **Is this a SEPARATE overview display, or does it change where
  entities actually live?** Two real, different things it could mean:
  (a) a fixed-position "skyline" legend/overview cluster at top-center,
  distinct from each entity's own real, interactive `desktop_pos.txt`
  position (entities stay walkable/touchable wherever they are; the
  city formation is a second, read-only representation), or
  (b) entities of a shared "type" actually get laid out/clustered into
  a city arrangement as their REAL position (replacing free placement
  for city-formation-eligible entities).
- **"different emojis for each entity type"** - does this mean one
  representative glyph per TYPE (e.g. one 🏰 icon standing in for
  "however many castle-type entities exist"), or every individual
  entity's own real glyph shown separately, just arranged city-style?
- **What counts as an "entity type"?** Per-pal (cursword vs castle vs
  door_civ are each their own type), per-category (buildings vs
  characters vs objects), or per-game (civilization's own entity
  roster vs office's own desktop-tool roster)?
- **Real vs decorative**: is the city formation clickable/interactive
  (each city-formation glyph a real, working shortcut to its entity,
  like a legend you can click to jump to/highlight the real entity),
  or purely a visual overview with no interaction?
- **Scope**: every desk, or only game-desks (civ-test/dsr/etc), not
  utility desks like `office`?

## 4. What already exists that this can probably be built on

- Every entity already has a real glyph (`glyph.txt`, `pal.pdl`'s own
  `PAL | glyph | <emoji>` row) - the raw material already exists, no
  new per-entity data needed.
- The real emoji->sprite pipeline (`emoji_gen_atlas.+x` +
  `tp_asset_to_sprite.+x`, resolution 64) already used for every
  simple deskpal - the same pipeline can render a city-formation
  cluster's own glyphs if it turns out to be a single composed
  image/sprite rather than N individual small windows.
  proven multi-`<panel>`/flex layout (`css_layout_pass`,
  `display:flex`/`flex-wrap`) - if the city formation ends up living
  inside a real `.chtpm` window (e.g. a desk-overview panel) rather
  than directly on the X11 desktop background, this is the same real
  primitive DSR's own layout work (see `dsr`/NOTES.md) already
  confirmed sufficient, no new renderer tag needed there either.
- Desk background/board rendering itself: need to check (not yet
  checked) whether the desktop background is a real drawable surface
  this house's own code already paints into (vs. bare X11 root/wm
  background), since "top center of the board" implies compositing
  onto that surface directly, not a separate window.

## 5. Real next step

Before writing any code: resolve §3's open questions with direct
instruction (this doc exists so that conversation has a fixed
reference point, not so the assistant guesses), then check §4's real
"where does desk background painting currently happen" question
against the actual code (`khtpm_core_render.c`'s `tp_main()` /
desktop-entity render path) before designing the mechanism. Update
this doc's own Status section as design decisions land and as
implementation starts, per the direct "document before AND while we
do" instruction.

## 6. Status

Not started. Open questions in §3 not yet resolved.
