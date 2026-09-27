# 2026-09-26 — real Move + range-limited placing grid closed; book-stack position fix; two new compact docs

## What landed today

- **book-stack's Read picker now uses the real calling-window
  position** (was falling back to `desktop_pos.txt`'s saved tile
  position, only an approximation - "slightly off"). Threaded
  `${WIN_X}`/`${WIN_Y}` through as `KHTPM_WIN_X`/`KHTPM_WIN_Y` env vars
  across the whole `prisc+x` → `dispatch.sh` → `khtpm_show_choices.+x`
  exec chain (env inherited at every hop, no per-script passing
  needed). Commit `b0187294`.
- **Act menu's "malformed template" warning fixed** - pre-existing bug
  in `open_entity_act.sh`'s awk-generated template (backslash-quote
  instead of the real XML `&quot;` escape inside `action="..."`
  attributes), unrelated to the earlier position fix. Commit `b4315f0d`.
- **Move is now a real, reliable, single-entity relocation feature**
  (bug_bounty.md's "Act menu.../Move does nothing" entry fully closed,
  commit `de2170eb`) - the biggest piece of today:
  - `move_entity_on_desk.sh` (new): reuses `FE_PLACE_CLICK`, the same
    real short-circuit File Explorer's own drag-and-drop already uses,
    to get a target cell back with NO new-tile stamping, then rewrites
    the entity's own `desktop_pos.txt` and relaunches it there.
  - `move_entity_to.sh` (new): the NO-UI companion for an agent/
    automated caller that already knows the target reference px - no
    grid, no mouse, no keyboard delivery needed at all. This is the
    real path for AI-driven or scripted entity movement.
  - `tp_arm_placer_rmmv.c`: the placing grid is no longer full-screen -
    `TP_ORIGIN_X`/`TP_ORIGIN_Y` (an entity's own position) + a real
    range limit the overlay to a small box of cells around that
    origin. A single click now moves the target (same highlight arrows
    already control) instead of placing immediately; a second click on
    the same cell, or Enter, confirms.
  - `desk_grid.pdl`: new `move_view_range`/`place_confirm` keys - real,
    per-house, human-editable config, not just an env var.
  - Confirmed live (no code change needed): arrow-key movement already
    works with zero explicit focus/click, since `XQueryKeymap` polls
    raw hardware key state independent of window focus by design.
  - Pixel-verified end-to-end: a real 560x560 range-limited window (was
    2496x1664 full screen), a click moving a green target indicator
    without placing, a confirmed double-click writing a real new
    `desktop_pos.txt` and relaunching the entity there, and an
    arrow-key press moving the target with no focus/click first.
  - `attack` and `use` remain real, separate, not-yet-built follow-ups
    (attack is still its own hardcoded a1/b2/2/6 demo; use is a plain
    no-op) - not touched by this fix.
- **Two new `00-compact/` docs**:
  - `compact-fix-guide.md` - a per-FILE pointer index (not chronological
    like `HOUSE_CODE_PITFALLS.md`/`bug_bounty.md`) - one section per
    real code file, pointer-only, for "about to touch X, what should I
    know first."
  - `#.compact-handoff.md` - a dated, task-specific handoff for a small/
    low-reasoning agent, covering what needed live verification at the
    time it was written (now partly superseded by today's own fixes -
    check its own "What just happened" section's currency before
    trusting it).
- Everything pushed to `origin/claude`.

## Real, open next steps

1. `use` (`act_row.sh`) is still a plain "recorded" no-op - real,
   undesigned follow-up, needs its own real logic (parity with
   `attack`'s/the new `move`'s real mechanisms).
2. `attack`'s own real logic is still a fixed, hardcoded demo
   (`apply_range.sh a1 b2 2 6`, ignores which entity/who clicked it) -
   a real per-entity attack needs the SAME kind of real-position wiring
   `move`/`Cli-io`/`Act` all just got.
3. Battle screen's per-dev-set selection mechanism (GAME.md's own
   2026-09-24 decision: both turn-based and real-time wanted, per
   dev-set) - the *how* (a Troops-database flag? a `.pdl` key?) is
   still undecided.
4. DUSTOPIA-HACK ladder step 3 ("one live window, one key, stop") -
   steps 1-2 are met as of 2026-09-24; this one looks tractable.
5. `prisc+x`'s cosmetic missing-`default_op.txt` warning - low
   priority, commands run fine anyway.
6. Next real focus, per direct instruction: AI-driven agent/gameplay
   work - `move_entity_to.sh`'s no-UI direct-position injection is the
   intended real interface for that, not the interactive grid.
