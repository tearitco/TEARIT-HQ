Civ-Test Desk, Door Transfer, and the Events-Only Civilization Clone
=====================================================================
Reference doc, 2026-09-14. VISION + IN-PROGRESS PHASE — step 1 is
real, imminent work; everything past it is staged, later work,
written down now per direct instruction ("so lets write that in
plan"). Companion to PLAY-MODE-ENTITY-HARNESS-DESIGN.md and
EVENT-TRIGGER-LAYER-PLAN.md, whose real, already-proven desktop-
entity trigger mechanism (cursword walking onto a target entity,
`master_ledger.txt`, the bridge watcher) is what this whole plan is
built on top of - not a new mechanism.

## 1. The real, immediate goal

A new desk, `civ-test`, hosting a 🚪 door entity. Touching it (the
exact same real trigger mechanism castle already proves - cursword
walking onto its cell) fires a real, coded, inspectable "transfer
player" Common Event whose effect switches the active desk, landing
the player on `civ-test`. This is the smallest real, provable proof of
desk-to-desk transfer via an in-game event, not a taskbar click -
matching this whole track's own "walk into it, something happens, no
manual step" bar.

## 2. Step 1 (build now) - the door + transfer event

- **New desk**: `civ-test`, created the same real way any desk is
  (`livedesk_switch_desk()`'s own desk-file convention,
  `#.desktop/.../desks/<name>.pdl` - real path to be confirmed against
  the live session's own real desks dir before writing anything, not
  assumed).
- **Door entity**: 🚪, placeholder glyph for now (see §3 for the real
  tileset/animation/sound follow-up) - built the exact same real way
  castle was (`emoji_gen_atlas.+x`/`tp_asset_to_sprite.+x`, real
  `meta.pdl`/`pal.pdl`/`desktop_pos.txt`), placed on whichever desk the
  player should walk through it FROM (not `civ-test` itself - a door
  that teleports you the moment you arrive somewhere is a real,
  confusing design bug, not this feature).
- **Real "transfer player" Common Event**: a new op, `mr_transfer_desk`
  (mirrors `mr_show_text.c`/`mr_move_to_entity.c`'s own real, minimal
  shape) - `mr_transfer_desk.+x <house_root> <session> <target_desk>`.
  **RESOLVED 2026-09-14** (direct instruction: "that is the sort of
  thing that doesn't need to live in manager once we have events, but
  u can make separate event for now then add it to live desk later.
  its just an op right? cant the manager run prisc event ops?") -
  confirmed the manager already runs arbitrary real ops/scripts via
  shell dispatch (the exact same way `play_event.sh`'s own `prisc+x`
  invocation already works, already proven for castle) - no NEW cross-
  process signaling into the running manager needed at all, since the
  trigger already fires through that same real `play_event.sh` ->
  `prisc+x` -> op path. `mr_transfer_desk` is a normal op like any
  other real event command; it gets its OWN small, honest, self-
  contained "switch desk" implementation for now (duplicating what
  `livedesk_switch_desk()` does directly - same real desk-state
  files/session-pointer/respawn steps, same "duplicate rather than
  share a header" convention this house already uses everywhere else
  for cross-binary logic), NOT a request-file/manager-polling scheme.
  Real, deliberate, explicitly NOT permanent: once desk-switching is
  more centrally event-driven, consolidate this standalone copy back
  into the real, single `livedesk_switch_desk()` implementation -
  flagged here so it isn't forgotten, not solved now.
- **Trigger wiring**: identical shape to castle's own `condition.pdl`
  (`COND | trigger | player-touch`) + `event.ir.pdl`/`event.pal`/
  `cmd_1.sh` calling `mr_transfer_desk`. No new trigger mechanism -
  `desktop_check_touch_trigger()`/`master_ledger.txt`/
  `khtpm_desktop_trigger_watcher.c` are already real, proven, and
  entity-agnostic (already fire on ANY registered touch, not
  castle-specific).

## 3. Later (written down now, not built) - the real tileset door

Direct instruction: "later we can use the door in rpg maker tileset,
with its animation, and sound effect... so lets write that in plan."
Real, staged follow-up once step 2 (§2 above) is proven:
- Replace the 🚪 emoji glyph with a real RPG Maker tileset door sprite
  (a real asset, not a placeholder) - needs a real sprite-sheet-to-
  atlas pipeline check (does `tp_asset_to_sprite.+x` already handle a
  multi-frame tileset sprite, or does animation need a separate, new
  mechanism? Not yet researched - real first question when this step
  starts).
  - Real animation on trigger (door opening) - likely a short, real
    frame-sequence swap driven by the SAME `mr_transfer_desk` op (or a
    small op it calls first) before the actual desk-switch fires, so
    the player sees the door open before being moved, not an instant
    cut.
  - Real sound effect on trigger - needs a real, house-standard audio
    playback mechanism check (does one already exist for entity
    events, or is this genuinely new plumbing? Not yet researched).

## 4. Later (written down now, not built) - the events-only Civ clone

Direct instruction: "we want to start coding a mini, civilization
(snes) clone using only events on that desk." Real, staged, large
follow-up:
- Lives entirely on `civ-test`, built ONLY from Common Events (no new
  C code for game logic itself - matching this whole track's own
  data-driven-event philosophy, EVENT-COMMAND-REGISTRY-ARCHITECTURE.md).
- Castle's own real Civ I city menu (City/Production/Armed Forces/
  View/Happy/Rename/Cancel, `menu_gameplay.chtpm`) is the real,
  already-built precedent this would extend - each currently-stub
  choice needs real behavior behind it, a genuinely large scope of its
  own, not detailed further here.
- Real, deliberate sequencing: prove step 2 (door transfer) first: a
  desk-to-desk teleport is real, separate infrastructure from "a
  playable city screen," and conflating the two risks neither landing
  cleanly.

## 5. Later (written down now, not built) - pc-hq playability

Direct instruction: "later we want to open it via pc-hq and make sure
its playable in there too by simply loading and pressing 'play on'."
Real, genuinely separate, unresolved architecture question - NOT
assumed to be simple: piececraft-hq is a completely different engine
(`chtpm_parser_pal`/`prisc+x`, its own renderer) from the khtpm
desktop-entity family `civ-test` would be built in. "Load a desk's
game in pc-hq" could mean at least two structurally different things:
(a) pc-hq gains a real loader that reads `civ-test`'s own desk .pdl
and re-renders its entities/events inside pc-hq's own board/tile
system, or (b) the Common Events themselves (already a shared,
engine-agnostic data format - `event_pkg`/`event.pal`) are the real
portable unit, and pc-hq just needs its OWN real trigger-watcher
(matching `pc_trigger_watcher.c`, already real and proven) pointed at
the same event packages, with desktop-specific concerns (window
position, `menu.chtpm`) simply not applying there. (b) is more
consistent with this track's own "one shared event/ledger mechanism,
many real front-ends" pattern so far - a real, informed guess, not a
decision; settling this is real work for whenever this step starts,
not before.

## 6. Later (written down now, not built) - save/load game menu

Direct instruction: "in order to actually show game menu += save/load
game slots i may add a 'menu' option after desks... does this seem
like a good idea or do u know a better way. tb is getting a bit
crowded but that maybe acceptable?"

**REVISED 2026-09-14** (direct live response: "the desks may actually
get more crowded than taskbar. also its really me debating whether or
not to show different tb during 'play' mode... i dont want to do it
yet for just 1 menu thing. i think we should just add it after desk
for now") - the conditional-row-inside-desks-dropdown idea above is
superseded: nesting it there risks crowding the DESKS dropdown instead
of the taskbar, just moving the problem. The real, bigger question
underneath this (should the whole taskbar look different in Play Mode
- a genuine "game UI" vs "desktop edit UI" distinction, matching
PLAY-MODE-ENTITY-HARNESS-DESIGN.md's own vision) is real and worth
solving eventually, but explicitly NOT for one menu item right now.
**Decided**: a real, new, plain top-level cell, positioned right AFTER
the existing `4.desk` cell - simplest, most direct option for now,
genuinely revisited once the bigger "different taskbar in Play Mode"
question gets its own real design pass (not forgotten - flagged here
as the real reason this specific placement is a deliberate, temporary
choice, not a final architectural stance).

## 7. Real build order (do not skip ahead)

1. **§2 - civ-test desk + door + transfer event.** Real next task.
2. §3 - real tileset/animation/sound (only after §2 is proven live).
3. §4 - the events-only Civ clone itself (large, its own real scope).
4. §5 - pc-hq playability (genuinely separate engine question).
5. §6 - the conditional save/load Menu row (needed once §4 has real
   save-worthy state, not before).

## Next step

Not started. Real first task: confirm the live session's own real
desks directory/`.pdl` path (don't assume), create `civ-test`, place a
placeholder 🚪 door entity on whichever desk the player currently
stands on, and build `mr_transfer_desk` - with its own real open
question (how a prisc+x-child op reaches `livedesk_switch_desk()`,
running inside a separate manager process) resolved as the actual
first sub-task, not guessed at here.
