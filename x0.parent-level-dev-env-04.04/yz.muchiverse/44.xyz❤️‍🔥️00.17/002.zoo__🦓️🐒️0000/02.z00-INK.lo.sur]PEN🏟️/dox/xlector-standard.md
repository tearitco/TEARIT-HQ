# The xlector standard - reference implementation and porting guide

Built in `z0.zoo_0000` specifically so this pattern could be proven out
in isolation, before deciding whether/how to bring it into mutaclsym or
any other game in this family - per direct instruction ("we can make a
new project... to explore the import export of the pets, and even
xelector stuff without bothering with mutaclsym so yes, lets do that
now"). Read `ops/xlector_input.c` itself first - its own header comment
carries most of this detail with exact file:line citations into real
1.TPMOS's `fuzz-op_manager.c`. This doc is the porting guide: what to
change per-project vs. what to keep byte-identical.

## The core idea

There is no "player" by default. A single, always-present cursor piece
called `xlector` is the DEFAULT thing wasd/arrows and method hotkeys
act on - not a hero, not any specific entity, in any game that adopts
this standard. Some games may choose to start xlector permanently
bound to a single controllable piece (effectively "a player"); others
may use it to freely explore/switch between many controllable
entities. Both are valid uses of the SAME mechanism - which one a given
game gets is a content/design choice (which pieces have a `select`
METHOD row in their own piece.pdl), not a code fork.

- **Enter**, while controlling xlector, scans every piece on the
  current map for one sitting at xlector's own tile. The first
  selectable match becomes the new `active_target_id` - control
  transfers to it.
- **'9' or Escape** is checked FIRST, unconditionally, every single
  tick, before anything else: if not currently controlling xlector,
  snap back to it immediately, full stop. This ordering matters and is
  not incidental - real fuzz-op_manager.c's own `route_input()` (line
  ~550) checks this before anything else too.
- **The menu is whatever the currently-controlled piece's own
  piece.pdl says**, via `pdl_reader.+x <active_target_id> list_methods`
  - genuinely the same one lookup regardless of whether the active
  target is xlector itself (typically nothing beyond move/select - a
  real, empty menu) or a selected entity (its real methods appear).
  There is no "xlector menu" vs "entity menu" branch anywhere in the
  code - this is the entire mechanism behind "xlector shows a different
  menu than a selected entity," confirmed against real
  fuzz-op_manager.c's own method_key handler (line ~708-788), which
  works the exact same way.
- **Shadow xlector sync**: whenever the controlled entity moves,
  xlector's own position is updated to match, so relinquishing control
  later leaves the cursor exactly where you left off. Real
  fuzz-op_manager.c's own name for this (line ~813).

## What this project changed vs. real fuzz-op_manager.c, and why

1. **Selectability is piece.pdl-driven, not a hardcoded name pattern.**
   Real fuzz-op excludes zombies from selection via
   `strstr(entry->d_name, "zombie")` - a hardcoded string check any
   adopting game would have to remember to keep in sync by hand, and
   which doesn't generalize across different games with different
   naming conventions. This project instead asks: does the candidate
   piece's own piece.pdl have a `select` METHOD row? If yes, xlector
   may take control of it; if no (an AI-only creature, scenery, etc.),
   it's skipped - per direct instruction: "depending on if the game
   .pdl dictates that in this particular game xelector is allowed to
   control those entities." This is the one deliberate design
   improvement over the reference, not a simplification - keep it when
   porting elsewhere, don't revert to name-pattern matching.
2. **xlector's own movement has zero special-casing.** It goes through
   the exact same collision-checked `move_entity.+x` as any other
   piece - matching real fuzz-op's own behavior (its route_input()
   calls the identical generic mover for xlector as for anything else),
   NOT mutaclsym's own earlier, different adaptation of "xlector" (a
   free/uncollided look-around cursor, built in an earlier mutaclsym
   session before this real standard was researched properly - see
   `hand-off-muta-eggs.txt` for why mutaclsym's existing interact_mode
   is a DIFFERENT, narrower thing than this standard, not the same
   pattern under an old name).
3. **Single fixed map (`map_zoo`)** - real fuzz-op's own xlector
   inherits the selected entity's `map_id` on select, supporting
   cross-map following. Out of scope here on purpose (this sandbox has
   one map); the moment a second map exists anywhere adopting this
   standard, port that piece of `route_input()` too - it's a small,
   well-understood addition, not a redesign. Don't build it
   speculatively before there's a second map to actually test it
   against.
4. **One-op-per-tick structure, not a long-lived manager process.**
   Real fuzz-op_manager.c is one persistent process holding
   `active_target_id` in memory, with a background thread polling
   `history.txt`. This project family (mutaclsym, zoo_0000) instead
   runs a FRESH short-lived process per tick, dispatched from
   `pal/main_loop.pal` - so `active_target_id` has to live in a FILE
   (xlector's own `state.txt`) instead of a global variable. This is
   the same "every keypress is a fresh process, persist what needs to
   survive across ticks in a state.txt field" convention mutaclsym's
   own `ops/choice.c` already established for its digit accumulator -
   applied here to `active_target_id` too. Functionally identical
   behavior, different process lifetime model.
5. **No joystick keycode range** (real fuzz-op also accepts
   2000-2009/2100-2103 range codes for joystick buttons/d-pad) - this
   project's own `keyboard_input.c` never emits those, so there's
   nothing to handle yet. Add them the same way real fuzz-op does if a
   joystick reader is ever built for a project using this standard.

## What every field/piece needs, concretely

- Every controllable piece (xlector included) needs `digit_accum`,
  `action_cursor`, and `last_key` fields in its own `state.txt` - the
  method-hotkey accumulator's persisted state, read/written generically
  by whichever piece currently is `active_target_id`.
- xlector's own `state.txt` needs `active_target_id` (default
  `"xlector"`) - the one piece of truly global state this whole
  mechanism hangs off of.
- Any piece meant to be selectable needs a `select` METHOD row in its
  own piece.pdl (the handler path doesn't matter much - this project
  points it at `ops/+x/xlector_input.+x` itself, matching how real
  fuzz-op's own xlector "select" is really just a marker route_input()
  already knows how to interpret, not a separately-dispatched verb) -
  see `piece_is_selectable()` in `ops/xlector_input.c` for the exact
  `has_method select` check this depends on.

## Porting this into mutaclsym (or any other project)

1. Copy `ops/xlector_input.c` and `ops/move_entity.c` largely as-is -
   the only project-specific constant in either file is `MAP_ID`
   (single-map assumption, item 3 above) and the state-file path shape
   (`pieces/world_01/<map>/<piece_id>/state.txt` - already matches
   mutaclsym's own convention exactly, so this should need close to
   zero changes there specifically).
2. Give every piece meant to be xlector-controllable the extra
   `digit_accum`/`action_cursor`/`last_key`/`active_target_id` fields
   this standard depends on (mutaclsym's hero already has the first
   three via its own `interact_mode` work; would need the `select`
   METHOD row and to decide what plays the "xlector" role there).
3. **Decide whether mutaclsym even wants this**, since it already has
   its OWN, narrower `interact_mode`/xlector-cursor feature (built
   earlier the same day this standard was researched, before the user
   redirected to look at real fuzz-op specifically) - the two are
   genuinely different designs (see item 2 above), not the same thing
   with a different name. `hand-off-muta-eggs.txt` covers this
   decision in more detail.
