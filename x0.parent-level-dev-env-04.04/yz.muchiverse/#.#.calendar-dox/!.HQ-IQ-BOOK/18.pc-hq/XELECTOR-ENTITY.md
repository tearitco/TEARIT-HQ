# The xelector is an entity in pc-hq (not yet on the livedesk)

Written 2026-10-05, from a direct instruction: "the xelector is actually an
entity for pc-hq that we don't have yet on livedesk — want to include it on
the bottom tb for pc-hq, and we can figure it out in livedesk later. doc it."

## What it is in pc-hq today

- A real piece with its own folder: `@.apps/piececraft-hq/pieces/xelector_01/`.
  `state.txt` holds `pos_x`, `pos_y`, `pos_z`, `possessed_id`,
  `pre_possess_x/y/z`, and the chunk fields.
- It is the cursor of the board: arrows move it, `z`/`x` move its level, the
  camera follows it (modes 1 and 2), and the 3D scene draws it as the cyan
  block (modes other than first-person).
- **Possession:** when `possessed_id` names an entity (the hero is the usual
  case), the xelector's cell *is* that entity's cell, and the player-ability
  keys (jump/mine/build, `g`/`h`) act on that entity. With `possessed_id`
  empty it is a free, unconstrained cursor. See `phase2-plan.md` and
  `civ-vs-piece.md` §6a/§6b.
- When the page file is in use (`PAGE-FILE.md`), the first 2D frame seeds an
  `xelector_01` row on the open page: cell in the cell columns, `possessed_id`
  in the glyph field, z in the last field. `pieces/xelector_01/state.txt` is
  then the old private copy. (No page file in this house holds that row right
  now; it is only created when a 2D frame draws.)

## What changed (this session)

- The xelector now has a **button on the pc-hq bottom entities bar**, in both
  modes: right after `map` when a page is bound, and right after the hero in the
  private-list fallback. Built in `pchq_board_projector.c` `emit_xelector()`,
  cell read from `pieces/xelector_01/state.txt`. Kind `xelector`.
- Clicking it opens a small context menu from `pc_entity_ctx.sh`:
  **Inspect, Dir, Close, Cli-io** (Cli-io is the same shared field every
  entity menu has; Dir/Inspect go through the existing CTX verbs). It is not a
  desk menu: there is no desk entity directory for it, so it uses pc-hq's
  generated menu. Events and Inventory are left out on purpose until it is
  decided what an xelector owns.
- Verified live through the relay: the bar shows `xelector` after `map` with
  the page-bound toolbar (`book:pre-design`, `page:teru-test`); clicking it opens
  the menu above and logs `kind=xelector id=xelector_01 cell=13,9,31`.

## What is deliberately NOT done: the livedesk side

The desk has no xelector entity. Things to decide before it gets one:

1. **Is there a desk analogue already?** Cursword is the likeliest. It owns the
   desk's level traversal: its `c`/`v` keys set `#.desktop/desktop_active_z.txt`
   (`khtpm_entity.c` `cursword_handle_camera_key`), and entities not on that
   level are unmapped. That is close to "the cursor that decides which floor you
   are looking at". **Unconfirmed hypothesis, not read as a design.** If true,
   the xelector and cursword should share one definition rather than two.
2. **Does the xelector get a desk pal directory** (`pals/xelector/` with a
   `meta.pdl` and `menu.chtpm`) so it shows up in every page and gets the
   desk-style menu automatically (the rule in `MOVE-AND-TAKE.md`: desk entities
   open their own `menu.chtpm`)? Or does it stay pc-hq-only?
3. **Possession on the desk:** does possessing mean the same thing for a desk
   pal (take over its keys), or is it a pc-hq-only mechanic?
4. **Where its cell lives:** the page-file row (agreed, see `PAGE-FILE.md`)
   with z in its own `z=`, like any desk entity, per the Move roadmap R1.

Until those are answered, treat the xelector as a pc-hq entity that happens to
be listed next to desk entities, nothing more.

## Related

- `MOVE-AND-TAKE.md` — Move, the shared range library, desk-is-the-parent rule.
- `PAGE-FILE.md` — the page file as the shared source of truth.
- Why the bar looks different in page mode: the page-bound bar lists the page's
  own entity rows, and deliberately leaves out `hero_01`, `tree_small`,
  `chicken`, `camera_01`; `xelector_01` is excluded from those rows and added
  separately by `emit_xelector()`.
