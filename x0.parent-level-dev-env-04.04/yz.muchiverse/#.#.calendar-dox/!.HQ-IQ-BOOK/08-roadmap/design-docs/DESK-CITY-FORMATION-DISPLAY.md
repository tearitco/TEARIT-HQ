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

## 3. Resolved, 2026-09-14 (direct clarification)

> "the city formations, they are made from emojis, and will occupy
> tiles. they will be row, empty row, row (to allow for navigation),
> columns of four then space then four to allow navigation (roads)
> like a city. they will be on left and right side per
> 'castle/country' with a space in between them showing that they
> clearly own either side of the screen."

- **Real entities, real positions - not a separate overview.** Each
  building IS a real, individually interactive deskpal at a real
  `DESK`-row/`desktop_pos.txt` position - not a second read-only
  legend layered on top. Resolved by the DSR build (§ below): every
  entity's own real glyph, own real window, own real grid tile.
- **One tile per real entity**, not one glyph per type - a game with 4
  banks shows 4 real 🏦 tiles, not one 🏦 standing in for all of them.
- **"Entity type" = per-pal**, each distinct pal (castle vs bank vs
  store) gets its own glyph; multiple entities of the SAME pal-type
  (e.g. 4 stores) each get their own real tile too.
- **The grid pattern**: alternating building/empty rows ("row, empty
  row, row") so there's always a real walkable street between rows;
  within a row, buildings cluster in blocks of 4 columns with a gap
  column as a road, same idea applied to columns as to rows.
- **Two sides, mirrored, with a real gap**: one city per "castle/
  country" (one on the left, one on the right of the board), a real
  empty-column gap between them so it's visually obvious the two
  territories are distinct and each owns its own side.
- **Interactivity**: real, same as every other desk entity (each
  building is a real, clickable/touchable pal, not decorative).
- **Scope**: per-game, not universal - DSR's own `dsr` desk is the
  first real build of this pattern (§4 below); whether/how it applies
  to other desks (civ-test, a future Dwarf Fortress desk, etc.) is
  each game's own later decision, not a forced house-wide rule.

## 4. Real build - DSR, 2026-09-14

First real implementation: the `dsr` desk's own 14 buildings (2
castles/4 banks/8 stores, per `TEST-GAMES-ROADMAP.md` §6's own real
DSR design). Full detail, including exact grid coordinates and live
verification, is in `xyzfs/.../home/projects/dsr/NOTES.md`'s own
"City formation - built" section - not duplicated here to avoid the
two docs drifting apart. Buildings are currently simple deskpals
(Events(hq)/Dir/Close/Cancel stubs) - no real government/bank/store
mechanic behind them yet, matching DSR's own display-first sequencing.

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

First real build live (DSR's `dsr` desk, §4). Design questions
resolved 2026-09-14. Next: decide, per-game, whether/how this pattern
extends to other desks.
