# mutaclsym — CDDA feature architecture & roadmap

Handoff document. If you are picking this project up cold: read this file
first, then the three source docs listed in §1, then read the actual code
under `system/`, `ops/`, `pal/` — this document explains the *pattern* for
extending the game, it does not restate what the code already shows.

Primary goal right now, per direct instruction: **implement actual CDDA
(Cataclysm: Dark Days Ahead) game mechanics and content**, using the
established TPMOS / "russian doll" architecture. Multiplayer, mods, farming,
crafting-beyond-CDDA, and civilization features are real future goals but are
explicitly out of scope until CDDA-parity mechanics exist — see §6.

## 1. Current state (2026-07-14)

Phase 0 vertical slice is built and verified working end-to-end, and the I/O
layer has since been rebuilt to match real TPMOS: **there is no ncurses
anywhere in this project.** ncurses was tried first, called "a regressive
mistake" by direct instruction, and fully removed. The pipeline is now three
independent processes, no central manager binary:

- `system/keyboard_input` — owns the real terminal in raw termios mode
  directly (`tcgetattr`/`tcsetattr`, no curses), decodes `ESC [ A/B/C/D`
  itself into sentinel ints (`ARROW_LEFT=1000` etc.), and appends bare
  decimal keycodes to `pieces/apps/player_app/history.txt` — one int per
  line. Exits (restoring the terminal) on `q`, after writing it, and drops a
  byte in `pieces/system/quit_flag.txt` so the renderer knows to stop.
- `system/prisc+x pal/main_loop.pal` — reads `history.txt` via
  `read_history`, dispatches to `ops/move_player.+x` / `ops/end_turn.+x` /
  `ops/compose_frame.+x`, and halts itself (native `halt` opcode) when it
  sees the same `q` keycode — no external kill needed for the VM.
- `system/renderer` — a plain cooked-terminal process, no raw mode needed
  for output. Polls `pieces/display/frame_changed.txt`'s *size* (never
  mtime), and on growth, ANSI-clears the screen and prints
  `pieces/display/current_frame.txt`. Every frame it draws is also appended,
  timestamped, to `pieces/display/frame_history.txt` — a plain-text,
  append-only log of every frame the game has ever shown. **This is the big
  win of dropping ncurses**: rendering can be verified by reading a text
  file, no pty/pyte terminal emulation needed at all. Exits when
  `quit_flag.txt` becomes non-empty.

`button.sh run` is the orchestrator: it launches `renderer` and
`prisc+x`/pal in the background (tracked, killed on exit per
`cdda-tpm-std-fast.txt`'s "always track and be able to kill subprocess PIDs"
rule), then runs `keyboard_input` in the foreground since it's the one
process that needs the controlling tty.

Hero position, wall/door collision (via the terrain registry), and turn
counting all persist correctly across the process boundary through plain
state files, verified via a pty-driven test that sent `wasd` + arrow-key
input and then read `frame_history.txt`/state files directly (no terminal
emulator needed to check the result).

Mandatory source docs (read, do not duplicate their contents into new docs):

- `cdda-tpm-std-fast.txt` — no-shared-headers, one-process-manager +
  many-single-verb-ops doctrine, marker-file-size redraw triggering.
- `!.world_architecture+1=rusindol.txt` — world/map/piece nested directory
  structure and its renderer/engine resolution order.
- Real CDDA source, `catacylsm.DDA-0.F-dev🧟‍♀️️]ON]/0001/data/json/` — reference
  for what content categories and fields CDDA actually models. CC-BY-SA-3.0
  licensed; read only the specific category file you're porting, don't bulk
  read this tree.

## 2. The extension pattern (how every new feature gets added)

Everything new is either a **registry piece** or a **world/map instance
piece**. There is no third kind.

**Registry pieces** — global, non-located, static definitions. Live under
`pieces/registry/<category>/`, outside any `world_*/map_*/` container, per
the rusindol doc's rule for global pieces. Established example:
`pieces/registry/terrain/terrain_types.txt`. These are the mutaclsym
equivalent of a CDDA `data/json/<category>.json` file full of `"type"`
objects — same idea (a stable `id`, a set of fields, string
cross-references to other ids), different serialization (plain
pipe-delimited text, not JSON — see below for why).

**World/map instance pieces** — live, mutable state placed on a specific
map. Live under `pieces/world_<id>/map_<id>/<piece_id>/`, each with its own
`state.txt` (mutable fields) and `piece.pdl` (verb -> op-binary dispatch
table). Established example: `pieces/world_01/map_start/hero/`. Monsters,
NPCs, dropped items, vehicles, etc. all become sibling directories here (or
nested *inside* another piece's directory when they're logically contained
by it — e.g. an item inside a container inside the hero's inventory —
per the rusindol doc's "any piece can itself be a container" rule).

**Why plain text, not JSON:** every op is a small, self-contained C binary
with no shared headers and, by extension, no shared JSON parser dependency.
An op parses only the fields it needs off the pipe-delimited line, exactly
like `move_player.c`'s `terrain_walkable()` does today. Adding a JSON library
would mean either vendoring a parser into every single op or reintroducing a
shared header — both against the standing architecture decision. If a
specific piece of ported CDDA logic strongly wants JSON's nesting, convert it
to flat pipe-delimited fields at data-authoring time, the same way the
terrain registry already flattens `glyph|id|name|walkable`.

**Every new verb = one new op.** Harvesting, crafting, attacking, feeding,
wearing, examining — each becomes its own `ops/<verb>.c`, self-contained,
with the same ~10 lines of `resolve_root()` boilerplate every existing op
already has. It gets wired in two places: a `METHOD` line in the relevant
piece's `piece.pdl`, and/or a call from a `.pal` script (see
`pal/main_loop.pal` for the existing pattern of dispatching on read input).
Never factor the boilerplate into a shared `.h` — copy it, per doctrine.

## 3. Further TPMOS conventions worth stealing (pal-based, not manager-based)

Found by reading more of the real TPMOS reference tree (`~/Downloads/
1.TPMOS_c_+rmmp.0100.0110/`) while fixing the I/O layer. All three below are
**ops** — plain binaries a `.pal` script calls via `exec`/custom-op dispatch,
same as `move_player`/`end_turn`/`compose_frame` already are. None of them
require growing a central manager; that stays intentionally minimal per the
earlier "Prisc for everything" decision. Not implemented yet — noted here so
Phase 2+ doesn't reinvent them.

**A generic state read/write op**, modeled on TPMOS's
`pieces/master_ledger/plugins/piece_manager.c` (`piece_manager.+x <piece_id>
get-state <key>` / `set-state <key> <value>`). Right now `move_player.c` and
`end_turn.c` each hand-roll their own "read all lines, find the key, rewrite
the file" loop — fine for two ops, but CDDA's real scope means dozens more
(hunger tick, HP damage, inventory add/remove, morale, thirst...). Worth
building a single `ops/piece_state.c` (`get <piece_state_path> <key>` / `set
<piece_state_path> <key> <value>`) that every future verb-op shells out to
instead of re-deriving the read-modify-write loop each time. Still
self-contained, still no shared header — it's a dependency reached through
`exec`/`popen`, not a `.h` include.

**A generic method-lookup op**, modeled on TPMOS's `pieces/system/pdl/
pdl_reader.c` (`pdl_reader.+x <piece_id> get_method <verb>` / `list_methods`
/ `has_method <verb>`). Right now `pal/main_loop.pal` hardcodes the op path
for every verb it knows about (`move_player`, `end_turn`, `compose_frame`).
That's fine while there's only one piece type (the hero). Once monsters,
items, and NPCs each have their own `piece.pdl` METHOD table with different
handlers for the same verb name (e.g. every piece type having its own
`attack` handler), a pal script should look up "what op handles verb X for
piece Y" at runtime via a `pdl_reader`-style op, instead of the pal script
needing a hardcoded branch per piece type per verb. Adopt this once Phase 4
(monsters/NPCs) needs more than one piece type to respond to the same verb.

**A master ledger audit trail**, modeled on TPMOS's `pieces/master_ledger/
master_ledger.txt` — a single append-only, timestamped, project-wide log
(`[ts] StateChange: piece_id key value | Trigger: source`) that every
mutating op appends one line to, in addition to writing the actual
state file. This is a different, complementary thing from
`frame_history.txt` (which logs rendered *frames*, not state *deltas*).
Real TPMOS's `pieces/apps/playrm/ops/src/undo_action.c` builds an entire
undo system out of nothing but replaying this log backwards — worth keeping
in mind once mutaclsym needs undo/replay/debugging-"what changed and why"
for anything beyond the current turn. Not needed for Phase 1-2; revisit once
enough mutating ops exist that "what changed this turn" stops being
obvious from reading `git diff`-equivalent state file changes by eye.

**The xlector active-target pattern** (already listed as deferred in §6, but
now with the concrete mechanism, seen in `projects/fuzz-op/manager/
fuzz-op_manager.c`): one `active_target_id` value (defaults to a dedicated
selector piece, `xlector`) that all verb dispatch routes through. Walking
the selector onto another piece's tile and pressing a select key retargets
`active_target_id` to that piece; every subsequent verb keypress then acts
on whichever piece is currently targeted, including the hero itself when
`active_target_id == "xlector"`'s owner. This is *how* pet
management/combat targeting/examine-at-a-distance all share one input path
in TPMOS instead of needing bespoke targeting logic per feature. Still
deferred per §6 — noted here only so the eventual implementation follows
the real mechanism rather than inventing a parallel one.

## 4. Autonomous & scriptable behavior: the FSM layer

Found in a *separate* reference project from 1.TPMOS proper — "Moke-Pet", a
lizard-tank sim at `~/Desktop/🤖️🪤️🏠️/🚛.XO-TANK-00.00/` (paths like
`0x=mokepet-tank-3.0/`, `0x.moke.tank+wrai.*`). It answers a question that
came up directly: how does the world run *without* per-turn human input —
needed for monster/NPC AI (Phase 4), and, per direct instruction, also for
(a) externally-controlled/multiplayer or user-`.pal`-scripted
automation/autobattling, and (b) a future level editor's user-authored AI.
All three are the *same* mechanism at different attachment points — worth
building once, generically, not three times.

**The proven-working piece: the epoch/turn-rotation loop.** Moke-Pet's real
(not just planned) `manager/manager.c` runs one "epoch" per invocation:
- Scans the map container directory for every piece of a given `type`
  (e.g. `lizard`) — dynamic discovery, no hardcoded entity list, so newly
  spawned entities (from reproduction, or a monster that just wandered onto
  the map) join the rotation automatically next epoch.
- For each entity: dispatch a perception op (`scan` → writes
  `memory/observations.txt`), make a decision, dispatch an action op, log
  the turn to an append-only epoch file (matches the master-ledger
  convention in §3).
- Metabolism and death run unconditionally every turn regardless of the
  entity's own decision: a `breathe` op ticks hunger/HP down, a
  `check_death` op runs after every action and, on death, *renames* the
  entity's own directory and flips its `type` (e.g. `liz_char` →
  `food_char`), making a dead creature something else can eat — mortality
  and predation both fall out of the piece/registry model for free, no
  special-cased "corpse" system needed.
- It runs **one epoch and exits**; something external re-invokes it
  repeatedly for a continuously-running world. For mutaclsym this re-invoke
  loop is exactly what `pal/main_loop.pal`'s existing `j loop` already is —
  the epoch tick folds into our existing turn loop as one more dispatch per
  active NPC/monster piece, alongside the hero's `move_player`/`end_turn`.

**The more general, not-yet-built design: per-piece FSM scripts.** A fuller
plan doc in the same reference tree
(`#.dev-storage/#.plans/#.fsm+ai-training-plan-b1/fsm_bot_programmer.txt`)
replaces Moke-Pet's hardcoded C decision logic with a `.pal`/asm script per
entity, dispatched on a `current_state` field via `beq r0, N, state_label` —
**the exact same dispatch style `pal/main_loop.pal` already uses**, just
keyed on a piece's own state instead of a keycode. It defines a small set of
shared ops any FSM script can call (`bot::navigate`, `bot::interact`,
`bot::wait`, `bot::choose` for randomized decisions, `bot::assert` for
verification), and — this is the direct answer to the multiplayer/autobattle
and level-editor asks — **the doc explicitly designs this as "one system,
two uses": the identical FSM engine drives both automated *testing* bots and
*AI player* bots**, meaning a user-authored "autobattler" script and an
internal monster-AI script are the same kind of object, differing only in
which piece they're attached to and what its states/transitions say. This is
the version worth actually building; Moke-Pet's C-heuristic manager is the
prototype that proves the surrounding mechanism (discovery, dispatch,
mortality-via-registry) works, not the target end state.

**The level-editor/user-authoring piece already exists too.** 1.TPMOS has a
real (if early-stage) visual `.pal` script editor: `projects/op-ed/` ("OP-ED:
The Sovereign RPG Editor" — see its own `README.md`). It browses available
ops, walks the user through filling in parameters, and writes out the
resulting `.asm`/`.pal` script — i.e. a human builds a piece's FSM by
clicking through op choices, not by hand-writing `beq`/`li` text. Its own
README states a design split worth keeping: *"Mode 1 vs Mode 2: WASD
movement is realtime (Direct C); Interaction is scripted (PRISC)"* — matches
what mutaclsym already does (`move_player`/`end_turn` are direct ops called
every keypress; anything more elaborate — a monster's turn, a user's
autobattler, a level-editor NPC — is a `.pal` script instead).

**How the three use cases share one mechanism, once built:**
- *Internal/autonomous (Phase 4 monsters/NPCs):* `pal/main_loop.pal`'s turn
  loop dispatches each active monster/NPC piece's own FSM script once per
  turn, using the epoch-style dynamic discovery above. No player input
  involved at all.
- *External/multiplayer/user-scripted automation:* the *same* per-piece FSM
  attachment point, but the piece is a player's companion/bot/autobattler
  instead of a wild monster — the FSM script is user-authored (by hand or,
  later, via an op-ed-style tool) rather than shipped with mutaclsym.
  Nothing in the dispatch mechanism needs to know or care which case it is.
- *Level editor:* once mutaclsym has one, attaching a custom FSM script to a
  placed piece is the same "point `piece.pdl`'s relevant METHOD/tick entry
  at a `.pal` file" operation as everything else — the editor's job is
  authoring the script (ideally via an op-ed-style guided builder
  eventually, not requiring raw `.pal` syntax), not inventing a new
  execution mechanism.

Not implemented yet. This belongs after Phase 4 (§5) introduces monster/NPC
pieces to actually attach FSM scripts to — building the FSM layer before
there's anything to drive with it would be solving an abstract problem
instead of a concrete one.

## 5. Build order — CDDA mechanics first

Phases are meant to be done roughly in order, since each depends on state the
previous one introduces. Each phase should end the same way Phase 0 did:
flood-fill/consistency-check any new map or data before committing it, then
prove the feature with a pty capture, not just a claim.

**Phase 1 — World substance (done)**
- Terrain registry expanded past wall/floor/door: grass, dirt, pavement,
  rubble, shallow/deep water, tree, window, stairs up/down (11 types total,
  `pieces/registry/terrain/terrain_types.txt`).
- Furniture registry added (`pieces/registry/furniture/furniture_types.txt`:
  table, chair, bed, counter, sink, crate) and actually wired in as a real
  second per-tile layer — `map_start/furniture.txt`, parallel to `map.txt`,
  drawn on top of terrain in `compose_frame.c`, checked alongside terrain in
  `move_player.c`'s collision (walkable furniture like chairs vs. blocking
  furniture like tables, both verified in-game).
- Second map (`map_02`, an outdoor courtyard using the new terrain types)
  with a real transition mechanic: `transitions.txt` per map
  (`x|y|dest_map_id|dest_x|dest_y`), stairs tiles trigger it in
  `move_player.c`. `move_player.c`/`compose_frame.c`/`end_turn.c` all read
  the hero's `map_id` field dynamically now instead of hardcoding
  `map_start` — each map keeps its own independent turn counter. Verified
  round-trip (walked real path through both doors, including getting
  correctly blocked by a furniture obstacle mid-route) start → map_02 →
  back to the exact origin tile.

**Phase 2 — Items & inventory (pickup/drop/eat done; wear/wield/examine
and the category split still open)**
- Item registry so far is one flat file, not yet split by category:
  `pieces/registry/items/items.txt`, format
  `id|name|category(weapon/food/drink/tool/armor/container)|glyph|weight|power`
  (13 starter items). Splitting into `pieces/registry/items/<category>/`
  per CDDA's real `item_category` is still open — revisit once the flat
  file gets unwieldy, per the sizing note in §7.
- **Items physically move directories now — true russian-doll nesting,
  not a location field.** This was built once as a field-based shortcut
  (flat `pieces/world_01/items/<id>/` + a `location` field) and explicitly
  corrected by direct instruction: a piece's disposition must be encoded
  by which directory it's physically sitting in, so a piece can be
  dragged into a different environment's tree by hand, on disk, with
  nothing else to keep in sync. Ground items live under
  `pieces/world_01/<map_id>/items/<id>/`; picked-up items get `rename()`d
  (not copied) into `pieces/world_01/map_start/hero/inventory/<id>/`, and
  back out on drop, restamped with the hero's current `pos_x`/`pos_y`.
  Verified via `find` before/after, not just state-file content, in both
  directions.
- `ops/pickup.c` (`g`) / `ops/drop.c` (`v`) / `ops/eat.c` (`e`, consumes
  food/drink by category and restores hunger/thirst — the first op that
  outright deletes a piece, `remove()`+`rmdir()`, rather than moving one).
  `compose_frame.c` draws ground items on the map (layer above furniture,
  below monsters, below the hero) and lists carried items as an inventory
  line. All verified end-to-end, including exact tick-order math (`eat`
  applies before that same turn's own hunger/thirst increment).
- Not built yet: `wear`/`wield` (no equipped-item slot exists yet — a
  weapon's `power` isn't consulted by combat, which currently uses a flat
  `HERO_ATTACK_DAMAGE` in `move_player.c`), `examine`.

**Phase 3 — Player needs & stats (hunger/thirst/stamina done; pain and
per-body-part HP still open)**
- `hero/state.txt` has `hunger`/`thirst`/`stamina`/`pain` fields, ticked
  every turn by `end_turn.c`'s `tick_hero_metabolism()`: hunger +1/turn,
  thirst +2/turn, stamina regenerates +5/turn (capped 100), both capped at
  200. Crossing 100 on either costs HP every turn (starving −1,
  dehydrated −2) until food/drink brings the level back down — verified
  the exact math live (thirst 14 → 0 from drinking a water bottle, then
  +2 from that same turn's own tick = 2, matching code order precisely).
- `pain` exists as a field but nothing sets it yet (no injury-beyond-HP
  concept) — not built.
- Still flat `hp`, not CDDA's per-body-part HP (`body_parts.json`
  equivalent) — not built.

**Phase 4 — Monsters & NPCs (chase AI + melee combat done; ranged, corpses,
loot, and the full FSM layer from §4 still open)**
- Monster registry (`pieces/registry/monsters/monster_types.txt`, format
  `id|name|glyph|hp|damage`) — zombie, zombie_child so far.
- Monster instances are pieces physically nested under their map
  (`pieces/world_01/<map_id>/monsters/<id>/`), same real-directory pattern
  established in Phase 2 for items — a monster that migrates maps would
  need its directory moved the same way (not built yet, monsters are
  currently stationary-per-map).
- `ops/tick_monsters.c`, called from `pal/main_loop.pal` right after
  `end_turn` on *every* hero action (not just movement) — every monster
  piece on the hero's current map takes one step toward the hero each
  hero turn (diagonal allowed, blocked by the same terrain/furniture
  rules the hero obeys, and by other monsters), or attacks the hero
  instead of moving if the step would land on the hero's tile. Reads
  every monster's starting position in one pass before anyone moves, so
  monsters don't see each other's already-updated positions mid-tick.
- `move_player.c` now checks for a monster at the target tile *before*
  the transition/terrain checks: if one's there, the hero attacks instead
  of moving (flat `HERO_ATTACK_DAMAGE`, currently 5), and a killed
  monster's whole piece directory is deleted outright (same consume
  pattern as `eat.c`). Verified a full kill: 20 HP zombie died in exactly
  4 hits, directory confirmed gone via `find`, hero took realistic
  counter-damage from the 3 turns it survived first, and — same turn a
  hero attack lands — the target monster's own retaliation (via
  `tick_monsters.c` running right after) was confirmed to apply
  correctly, i.e. both sides of a melee exchange land in the same turn.
- This is the first real attachment point the FSM layer from §4
  anticipated — `tick_monsters.c`'s hardcoded chase-then-attack logic is
  the "Moke-Pet-style C heuristic" precursor described there, not the
  per-piece `.pal`-scripted FSM yet. Upgrading specific monster types to
  real FSM scripts (so monster behavior can vary by type, or later be
  user/level-editor-authored) is still open.
- Not built: ranged attacks, corpses/loot on death (a dead monster is
  just deleted, not converted into a lootable/edible piece the way CDDA
  turns a dead zombie into a corpse item), NPCs (non-hostile,
  dialog-capable pieces), monster spawning/population over time.

**Phase 5 — Crafting (done, including an overlay picker panel)**
- Recipe registry (`pieces/registry/recipes/recipes.txt`,
  `id|name|result_item_id|req1:qty1,req2:qty2,...`) with requirement/
  material references into the Phase 2 item registry.
- `ops/craft.c`: checks inventory against a recipe's requirements,
  consumes inputs (same `remove()`+`rmdir()` pattern `eat.c` already
  established for consuming pieces), produces the output item (same
  piece-creation pattern `generate_egg.c`-in-egg-pals and the item/
  monster instance files here already establish). Takes an optional
  `recipe_id` argv - with one, crafts exactly that recipe if still
  satisfiable; with none (legacy/CLI-testing path, no longer reachable
  from normal play), falls back to "first satisfiable recipe wins",
  same precedent as `pickup.c`/`eat.c`.
- **Recipe picker overlay panel**, added once real CDDA's own
  in-place (not full-screen-swap) menu convention was researched
  (`pieces/apps/gl_os/plugins/gl_desktop.c`'s `project_mirror` window
  type in the real 1.TPMOS tree: render the map, then draw the menu
  list directly over it in the same pass, no clear, no z-order math -
  later draws simply win). Pressing the craft choice no longer
  auto-picks a recipe; it opens a panel (`hero/state.txt`'s
  `active_panel="craft"`/`panel_cursor`/`panel_digit_accum` fields,
  same multi-digit-accumulator-then-Enter-to-commit model
  `ops/choice.c` already uses for the outer action bar - see the
  top-level `nav-refactor-2.txt` for the full numbered-choice design
  history, including the corrected-after-live-playtest accumulator
  mechanics) listing every recipe in file order, each
  marked `(ready)`/`(missing)` against current inventory, with a
  trailing `Cancel` row at `recipe_count+1` (same "Back always gets
  the next free slot" convention egg-pals uses). `compose_frame.c`
  draws the panel by overwriting a sub-rectangle of the already-built
  map character grid *after* the map/items/monsters/hero are drawn
  into it - the map stays visible around (and, if the hero happens to
  be standing under the panel's rows, behind) the box, exactly matching
  the researched `gl_desktop.c` pattern. `move_player.c` independently
  suspends all movement while `active_panel != "none"` (checks the
  same field, self-filters like every other op already does for keys/
  modes it doesn't own) - a real CDDA-style modal capture, not just
  "the input happens not to reach movement code."

**Inventory/examine overlay panel (done)** - second real consumer of
the overlay-panel mechanism above, proving it out as an actual
reusable pattern rather than a one-off for crafting. Pressing examine
(outer bar, dynamically the next method after craft) opens
`active_panel="inventory"` instead of running `ops/examine.c` directly
(same special-case pattern `choice.c` already used for craft) - a
read-only browse of `hero/inventory/`, each row showing the item's
name, category, and a relevant stat (`dmg+N` for weapons, `hunger-N`/
`thirst-N` for food/drink, `def+N` for armor). Unlike the craft panel,
Enter on ANY row (including the trailing `Close` row) just closes the
panel - there's nothing further to commit, since every row already
shows full detail rather than hiding it behind a selection.
`ops/choice.c`'s panel-mode branch and `compose_frame.c`'s box-drawing
loop (`draw_panel_box()`) are now genuinely shared between both panel
types, not just structurally similar - the second consumer justified
extracting that duplication for real, not preemptively.

**Persistent message log (done)** - real CDDA/1.TPMOS-parity gap found
and fixed while checking on sidebar/HUD parity: HP/hunger/thirst/
stamina were already shown every frame (that part was already at
parity), but "what just happened" was a single `hero/state.txt` `msg`
field, overwritten by whichever op ran last in a tick - a zombie's
attack message could silently erase an earlier pickup message from the
same turn. Every message-producing op (`pickup.c`, `drop.c`, `eat.c`,
`craft.c`, `move_player.c`, `tick_monsters.c`) now appends to
`pieces/display/message_log.txt` instead (a plain, unbounded,
append-only audit log - same "keep the whole trail, only ever
tail-display it" shape `frame_history.txt` already established) rather
than rewriting a single field; `compose_frame.c` reads and displays the
last 4 lines each render. Net simplification in every writer, not just
an addition - appending one line is simpler code than the old
read-all-lines/rewrite-preserving-unknown-fields dance that updating a
single field required.

**Minimap - still not built, but no longer blocked.** Real CDDA's
sidebar includes a small overview minimap of the area around the
player. At the time this paragraph was first written, every mutaclsym
map was *exactly* viewport-sized, so there was nothing off-screen for
a minimap to show - building one would have meant faking a feature
with nothing behind it. §5a's camera/viewport work is now done (real
maps can be bigger than the 40x16 viewport, verified against an 80x32
test map), so that specific blocker is gone - but no map actually
authored today is bigger than 40x16 yet either, so a minimap still has
nothing real to display until a genuinely bigger map exists. Build the
minimap once open-world map generation (or at least one bigger
hand-authored map) actually produces off-screen area worth showing.

**Title screen (New Game / Load) - tracked, not built.** User flagged
this as a near-future addition (a pre-game screen, matching real
CDDA's own title screen) but explicitly forward-looking ("we will also
add") rather than requested this pass. Likely needs its own small
screen/state-machine, closer in shape to egg-pals' `menu_state.txt`
screen system than to mutaclsym's own real-time action bar - probably
deserves its own focused pass rather than folding into the
numbered-choice work above. Not designed in detail yet.

**Phase 6 — Vehicles/automobiles (not built)**
- Not yet designed in this document. A vehicle is structurally a
  multi-tile piece (or a piece containing sub-pieces — parts) rather than
  a single-tile piece like an item or monster; the existing map-tile
  overlay model (terrain/furniture/items/monsters/hero, each a single
  glyph per tile) needs a design decision here before implementation
  starts: does a vehicle occupy multiple map cells directly (each cell a
  reference to the same vehicle piece), or does it get its own
  mini-map-like internal grid (closer to real CDDA's actual model)? Don't
  start building without resolving this first.

**Phase 7 — Save/load + title screen (done)**
- Modeled directly on real 1.TPMOS op-ed's own "Sovereign Architecture"
  save system, per direct instruction - researched and confirmed via
  `projects/op-ed/manager/op-ed_manager.c`'s `save_game_to_path()`/
  `load_game_from_path()` (`mkdir -p` + `cp -r` into a named folder
  under `games/`, `rm -rf` + `cp -r` back out to load - see
  `platform-vision.txt` for the fuller citation, including where op-ed's
  pattern *doesn't* map directly: it has no numbered load-picker, no
  save-name sanitization, and no pre-load manifest, all three of which
  this project deliberately does differently, not by accident).
- **`pieces/world_01_template/`**: a pristine, never-mutated New Game
  starting state (map files reused from the live map, hero/items/
  monsters freshly authored - NOT copied from a live/mutated save,
  since a template must stay pristine across every New Game reset).
- **`ops/save_game.c`**: new outer-bar action. Auto-names `save_N` via
  an incrementing counter (`pieces/system/save_serial_counter.txt`,
  same pattern as `item_serial_counter.txt`) rather than op-ed's raw
  user-typed path - this project has no free-text input mechanism
  anywhere, and op-ed's own naming had zero sanitization anyway (a real
  path-injection risk deliberately not copied). Copies
  `pieces/world_01/` into `pieces/saves/save_N/world_01/` (`cp -r`,
  same shell-out precedent as op-ed), plus a `save_meta.txt` (turn
  count, timestamp) op-ed itself doesn't have - its own metadata file
  is only read *after* loading, but a numbered picker needs something
  to show *before* commit.
- **Title screen** (`ops/title_input.c` + `ops/compose_title_frame.c`):
  New Game (row 1) + one row per existing save (rows 2..N+1), same
  bracket-cursor multi-digit-accumulator-then-Enter-commits model as
  every other numbered list in this project (own small accumulator, not
  reusing `ops/choice.c` - there's no hero `piece.pdl` to dispatch
  against yet at title time). Committing does `rm -rf` + `cp -r` (New:
  from the template; Load: from the chosen save) then hands off to
  gameplay.
- **Title→gameplay hand-off**: single process, no restart.
  `pal/main_loop.pal` now boots into a title-phase loop first; on
  commit, `title_input.c` appends a sentinel keycode (999, outside any
  real key's range) to `history.txt` - prisc+x has no opcode that can
  check an arbitrary file's existence by path (`read_pos`/
  `read_layout` are both hardcoded to their own specific files,
  confirmed by reading `system/prisc+x.c`'s parser directly), so this
  reuses the exact same synthetic-keycode-injection mechanism already
  proven out (and later reverted for its original use case) during the
  numbered-choice work - see `nav-refactor-2.txt`. The title loop picks
  it up via an ordinary `read_history` + `beq` on the very next tick and
  jumps straight into the existing gameplay loop's labels - the same
  `keyboard_input`/`renderer`/`prisc+x` trio keeps running the whole
  way through.
- Verified end-to-end against isolated copies (a real process-collision
  incident happened mid-testing here - two orphaned `prisc+x`
  background processes from an earlier, incompletely-killed test both
  writing to the same test directory's `hero/state.txt` concurrently,
  which is what produced a scary-looking but ultimately self-inflicted
  "corrupted state" symptom; resolved by verifying process death before
  re-testing, not a bug in the feature code - the standalone
  `title_input.+x` test that first isolated this ran clean throughout):
  title screen renders correctly (dynamic save list, correct default
  cursor); New Game resets turn/HP/hunger/thirst/inventory/position to
  the template's exact values; Save correctly copies the live world and
  writes accurate metadata; Load correctly overwrites a since-modified
  live state with the exact saved values; the full title→gameplay
  transition works through the real single `prisc+x` process with no
  restart.
- **A real, unrelated bug found and fixed while investigating a
  "player stuck after canceling a menu" report during this same pass**:
  a corrupted `hero/state.txt` line (`active_panel=nonepanel_digit_accum=0`
  - two fields glued together, missing newline, likely from a binary
  being rebuilt mid-edit while a live session was still running and
  mid-keypress) meant `move_player.c`'s exact-match
  `strcmp(active_panel,"none")` check was never true again, permanently
  blocking movement while `end_turn`/`tick_monsters` kept ticking
  in the background (not gated on panel state at all) - looked exactly
  like the game freezing, with hunger/thirst still visibly climbing.
  Fixed at two layers: `move_player.c` now only suspends movement for
  an *exact* `"craft"`/`"inventory"` match (fails open toward movement
  working, not closed toward being stuck, for any other/corrupted
  value); `ops/choice.c` (the only writer of `active_panel`) now
  self-heals any unrecognized value back to `"none"`. Also added, per
  direct request: a `"You can't go that way."` message when movement is
  blocked by terrain/furniture (previously silent - this was the other
  half of what made the specific incident confusing), and a
  `Last key: N ('c')` indicator on the HP line so a stalled-looking
  session can be diagnosed by confirming input really is being read.

**Phase 8 — Professions/scenarios, starting presets, basic day/night and
weather** — later CDDA-parity milestones, in roughly that order. Not
detailed further here; revisit this document when Phase 5 is done.

## 5a. Camera/viewport — done

Was "a big deal" / blocking prerequisite for any map bigger than 40x16;
now built and verified. `move_player.c`/`tick_monsters.c`/
`compose_frame.c` no longer trust a fixed `MAP_W`/`MAP_H` `#define` for
collision/rendering bounds - each reads the CURRENT map's real
`width`/`height` from its own `state.txt` at runtime (`read_int_field`/
`read_kv_int`, matching each file's existing helper naming), falling
back to 40x16 if absent. `MAX_MAP_W`/`MAX_MAP_H` (256 each) are now
purely compile-time buffer-size caps, not real dimensions - stack
buffers stay fixed-size (`char grid[MAX_MAP_H][MAX_MAP_W+1]`, no
dynamic allocation), just bounded by the map's real width/height in
every loop instead of the buffer's max capacity, exactly as planned.

`compose_frame.c` is the actual camera: builds the WHOLE current map in
memory at absolute coordinates (unchanged from before - terrain,
furniture, items, monsters, hero all still place themselves by real
`pos_x`/`pos_y`, no new coordinate system), then computes a clamped
camera origin (`cam_x = clamp(hero_x - VIEWPORT_W/2, 0, map_w -
VIEWPORT_W)`, same for y - floored at 0 when `map_w < VIEWPORT_W`,
which is still true for every map that exists today, so this is a
no-op for now) and slices out a `VIEWPORT_W`(40) x `VIEWPORT_H`(16)
window into a separate `viewport[][]` buffer. The overlay panels
(craft/inventory - see the Phase 5 entry above) now draw onto this
`viewport` buffer, not the full map grid, since a panel is glued to
the visible screen's top-left corner, not the map's absolute origin -
it has to stay put regardless of where the camera has scrolled to.
`draw_panel_box()`'s signature changed accordingly
(`char grid[][VIEWPORT_W+1]` instead of `MAP_W+1`).

Verified against an isolated 80x32 test map (never the live map, which
stays 40x16 for now): with the hero at the map's center, the rendered
viewport showed only a 40x16 slice of open floor around them, not the
whole 80x32 map; moving the hero to within a few tiles of each corner
correctly clamped the camera at that edge (confirmed both the
top-left and bottom-right corners render their actual wall/marker
tiles at the clamped position, not scrolled past the map's real
boundary); the existing 40x16 `map_start` still renders byte-identical
to before this change (camera is a true no-op when `map_w <=
VIEWPORT_W`). `move_player.c`'s collision against the bigger test
map's real 80-wide walls was also confirmed working (not clamped to
the old fixed 40).

This was the one prerequisite standing in the way of a real generated
open world, multi-Z buildings, and a real minimap (see the Phase 5
entry above, which explicitly deferred the minimap on this) - all
still separate, undesigned pieces of work, but none of them are
blocked on this anymore.

**Follow-up, proven against a real (not synthetic) map**: `map_02` was
expanded from the old 40x16 placeholder to a genuine 80x32 outdoor area
(tree-bordered forest, dirt path, a small building, a pond, a rubble
patch) specifically to exercise the camera in real play before
investing in procedural generation - see `00-HANDOFF.md`'s "Next
steps" for what's still open (no items/monsters placed on it yet, and
`tick_monsters.c` chase AI hasn't specifically been verified across a
map bigger than one screen, only collision and the two map_start↔map_02
transitions). Both transitions (door at (30,11) in `map_start` ↔
stairs at (4,4) in `map_02`) verified landing at the correct
coordinates in both directions.

## 6. Deferred — explicitly not now

Named here so scope doesn't creep into current phases, and so a future agent
knows these are planned, not forgotten:

- **Mods/ops packaging** — a way to load extra ops/registries as optional
  add-ons.
- **Multiplayer / online play.**
- **Save/load via named save-game file bundles** — distinct from the
  always-live `state.txt` files that exist now; a save is a snapshot you can
  name, store, and reload, not just "the current live directory tree."
- **"xlector"** — DONE (2026-07-17): real interact_mode cursor (free/
  uncollided movement, `ops/move_player.c`/`ops/choice.c`), examine-at-
  a-distance (`examine_at()`), thrown-weapon combat targeting
  (`throw_at()`), and a real v1 of possession (`try_possess_at()`,
  piece.pdl-driven `possessable` STATE field) - see xyzos-standards.txt
  §14 for the full research writeup and what's still real, deferred
  work: the FULL generic multi-entity possession mechanism (movement/
  dispatch genericized to take a piece_id, a real standalone xlector
  piece instead of two fields bolted onto hero/state.txt) has no
  target to exercise it against until a second player-controllable
  piece exists - see the very next line.
- **Pet/companion management** — also the real, natural next
  possession target once built (see xyzos-standards.txt §14's own "NOT
  YET BUILT" list) - a companion piece with its own `possessable`
  STATE field, exercising the full generic possession mechanism for
  real instead of only ever having hero to test it against.
- **Firearms/ammo** — real, direct scoping decision (2026-07-17):
  ranged combat this pass is THROWN weapons only (existing melee items
  reused via the xlector cursor, `ops/choice.c`'s own `throw_at()`) -
  no ammo, no reload, no magazine state, no gun-category items in the
  registry at all yet. A real CDDA-shaped firearms system (ammo items,
  a `ranged_power`/accuracy-shaped stat separate from melee `power`,
  reload as its own action, out-of-ammo handling) is a deliberately
  separate, larger build - not attempted this pass.
- **Farming** — plant/harvest crops, raise and kill animals, recipes from
  farmed goods, selling goods to other players or NPC businesses.
- **Crafting broadened past CDDA parity.**
- **Civilization features** — founding settlements/countries, trade between
  players or factions. This is explicitly "way down the road" per direct
  instruction.

Do not start scaffolding any of these until the CDDA-parity phases in §5 are
substantially done.

## 7. Open decisions

- **Registry granularity.** Terrain/furniture/items/monsters are all still
  one flat pipe-delimited file each, holding every type as a line. That's
  fine at small scale. Items in particular may outgrow a single flat file
  fast (CDDA has hundreds of item defs per category) — may need one
  piece-directory-per-item instead of one-line-per-item in a shared file.
  Not decided; pick whichever keeps individual ops simple to parse, and
  revisit if a flat file gets unwieldy.
- ~~Item location model~~ — **resolved.** Settled by direct instruction:
  physical directory nesting (`rename()` a piece's whole directory when
  its disposition changes), not a location field. See Phase 2 in §5 for
  what actually got built (items) and Phase 4 for where the same pattern
  still needs to be extended (monsters currently don't migrate maps, and
  would need the same directory-move treatment if that's ever added).
  Applies to every future movable piece — vehicles, corpses/loot, anything
  else that can change hands or location.

## 8. Handoff notes

- Build/run: `./button.sh {compile|run|kill|check}` from the `mutaclsym/`
  root.
- **No ncurses anywhere, by design** — see §1. Do not reintroduce it (not
  even for a single op or a debug view); if a rendering need seems to
  require it, extend `compose_frame`/`renderer` instead.
- Test harness: no `pty`/`pyte` terminal emulation is needed to verify
  rendering anymore — `pieces/display/frame_history.txt` is a plain-text,
  timestamped, append-only log of every frame the renderer has drawn, and
  `pieces/display/current_frame.txt` is always the latest one. Read them
  directly. A pty is still needed only to *drive input*, since
  `keyboard_input` requires a real tty for raw termios mode (`tcgetattr`
  fails on a plain pipe) — write raw bytes to the pty's master fd, then
  verify by reading the frame/state files, not by parsing terminal escape
  codes.
- Quit path: `keyboard_input` and the pal script both independently
  recognize keycode `113` (`'q'`) as the stop signal — `keyboard_input`
  exits and writes `pieces/system/quit_flag.txt`; the pal script hits a
  native `halt` opcode. `renderer` polls `quit_flag.txt` and exits when it's
  non-empty. `button.sh run` resets *both* `quit_flag.txt` and
  `pieces/apps/player_app/history.txt` to empty at the start of every
  session — a leftover non-empty `quit_flag.txt` would make the renderer
  exit instantly, and a leftover `history.txt` would replay a previous
  session's keys into the new one (`read_history`'s cursor always starts
  at byte offset 0).
- Ctrl+C: raw mode clears `ISIG`, so the tty line discipline stops turning
  Ctrl+C into `SIGINT` — it arrives as ordinary input, byte `3` (ETX), like
  any other keystroke. `keyboard_input` checks for byte `3` explicitly in
  its read loop and normalizes it to the same `'q'` sentinel everything
  else already checks for (matching real TPMOS's `input_capture.c`, which
  does the same `CTRL_KEY('c')`-as-data check rather than relying on a
  signal handler). The `SIGINT`/`SIGTERM`/`SIGHUP` handler in
  `keyboard_input.c` is a fallback for a *real* external signal (e.g.
  `kill -TERM`, `button.sh kill`), not the normal Ctrl+C path.
- Arrow-key sentinel values (`ARROW_LEFT=1000`, `ARROW_RIGHT=1001`,
  `ARROW_UP=1002`, `ARROW_DOWN=1003`, matching real TPMOS's own convention)
  are defined independently in both `system/keyboard_input.c` and
  `ops/move_player.c` — no shared header, per doctrine, so if a value ever
  changes it must be changed in both places by hand.
- Terminal writes must use `\r\n`, never bare `\n`. `keyboard_input` clears
  `OPOST` on the shared tty (needed for raw key reads), and `OPOST` is a
  per-terminal-*device* setting, not per-process — so it silently breaks
  every other process's auto-CRLF translation on that same terminal too.
  `renderer.c`'s `write_crlf()` translates embedded `\n` to `\r\n` itself
  rather than trusting the terminal to still do it; any future code that
  writes multi-line output straight to the terminal (not to a file) needs
  the same treatment.
- Compile warning-free is the standing bar, including in vendored
  `system/prisc+x.c` — widen path buffers (`MAX_PATH + N` headroom) so gcc
  can prove `snprintf` can't truncate, prefer `snprintf(dest, sizeof(dest),
  "%s", src)` over `strncpy` for bounded copies (`-Wstringop-truncation`
  fires on `strncpy` itself regardless of manual null-termination after
  it), and check/cast `asprintf`/`system` return values rather than
  discarding them.
