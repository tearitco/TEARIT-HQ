# egg-pals — what's built, what's next

Read `egg-pals.txt` (one directory up, sibling to this project) first — it's
the original handoff and explains *why* each piece below exists and which
real reference code it's ported from. Note: `egg-pals.txt` describes an
earlier design where the faucet directly generated eggs — that changed by
direct instruction (see below); everything else in that document still
holds.

## The economy (as actually built, not as first drafted)

- **Faucet** = tokens only. `Claim Tokens` grants +10 tokens (no cooldown
  yet). `Coin Flip` stakes a fixed 10 tokens on a 50/50 double-or-nothing.
  The faucet never touches eggs directly.
- **Store** = spend tokens. `Buy Egg` costs 20 tokens and mints one
  (deducts tokens, then calls the existing `generate_egg` op to do the
  actual minting — reuses it rather than duplicating its logic).
- **Pets** = read-only catalog for now (lists owned eggs/pets with
  species/hatched status). Selecting an individual pet to open its GL
  window is not built yet — see step 2 below.

## What's built and verified working right now

Everything compiles clean (`./button.sh compile`, zero warnings). The whole
menu loop has been driven end-to-end via a pty test (navigate every screen,
claim tokens, lose a coin flip, buy an egg, see it appear in the catalog,
quit) and every number/message came out exactly as expected — this isn't
just "it builds," it's been actually exercised.

- **The menu loop itself**, same three-process shape as mutaclsym (no
  ncurses): `system/keyboard_input` (raw terminal, vendored unmodified —
  it's fully game-agnostic) + `system/prisc+x pal/main_loop.pal` (reads
  keys, dispatches to `menu_input` then `compose_menu` each turn) +
  `system/renderer` (vendored unmodified, polls and prints
  `current_frame.txt`, logs every frame to `frame_history.txt`). Run
  `./button.sh run` — `wasd`/arrows to move, `enter` to select, `b` to go
  back, `q` to quit.
- **`ops/menu_input.c`** — the router. Reads/writes
  `pieces/system/menu_state.txt` (`screen`, `cursor`, `last_message`).
  Handles cursor movement, screen transitions, and — on the actual action
  options — shells out to `claim_tokens`/`coin_flip`/`buy_egg` and stores
  their one-line result as `last_message`. Note the constraint that shaped
  this: prisc+x's generic custom-op dispatch only ever passes **one**
  argument to a handler (a register value OR a literal, never both — see
  `exec_custom_op()` in `prisc+x.c`), so the pal script can only hand this
  op the keycode; the owner piece id defaults to `"user_01"` inside the op
  itself. A real multi-user version needs a "current user" pointer read
  here instead of that hardcoded default.
- **`ops/compose_menu.c`** — the renderer op. Boxed-ASCII-panel look
  (`+===+` borders, letter-spaced centered titles, `>`-marked cursor rows)
  matching real TPMOS's `.chtpm` layout files
  (`pieces/chtpm/layouts/*.chtpm` in the 1.TPMOS reference tree) without
  adopting their markup engine — this is hand-printed text, same pattern as
  mutaclsym's `compose_frame.c`.
- **`ops/claim_tokens.c` / `ops/coin_flip.c` / `ops/buy_egg.c`** — each a
  self-contained single-verb op, each printing one result line to stdout
  for `menu_input` to relay. `buy_egg` is the one that calls another op
  (`generate_egg.+x`) via `popen` rather than reimplementing minting.
- **`ops/generate_egg.c`** — the underlying mint primitive (unchanged from
  before): weighted-random species pick from
  `pieces/registry/emoji_pool/common_emoji.txt` (common ~10x more likely
  than rare), a real incrementing serial number
  (`pieces/system/serial_counter.txt`), piece creation, inventory update.
  Still directly runnable standalone (`./button.sh demo`), but now also
  reachable through the Store in the real menu.
- **`system/emoji_gen_atlas.c` + `system/emoji_xtract.c`** — the emoji
  glyph → PNG → plain-text pixel CSV pipeline, vendored from 1.TPMOS and
  verified working (`./system/emoji_gen_atlas "🐸" /tmp/frog.png` → real
  64x64 RGBA PNG → `./system/emoji_xtract` → correct downsampled CSV). Not
  wired into hatching yet — see step 1.
- **`system/egg_window.c`** — vendored from `mutaclsym/!.shape=on.0.0Ⓜ️/
  shape-a0.c`. Compiles and opens a real, working, draggable, borderless,
  circularly-shaped GL window today (`./system/egg_window`) — confirmed it
  actually opens against this machine's live X display, not just that it
  compiles. Still a circle with a flat color fill, and not wired to the
  menu's Pets screen yet — see steps 2-3.

Run `./button.sh check` to confirm every binary is built,
`./button.sh run` to play the real menu, `./button.sh demo` for the
lower-level standalone mint.

## Step 4 — the tamagotchi behavior layer (built)

Per this doc's own step 4 and `egg-pals.txt` §5: an epoch/tick engine
(`ops/tick_pets.c`, modeled on Moke-Pet's `manager.c` pattern per
`mutaclsym/dox/01-cdda-architecture.md` §4), dispatched every ~100 loop
iterations from `pal/main_loop.pal` (registered in `default_op.txt`).
Every hatched pet now has real metabolism: `hunger`/`energy` tick every
epoch, poop accumulates on a timer, exhaustion (`energy` hitting 0) forces
sleep, and a small per-tick random walk (`move_dx`/`move_dy`) is what
makes `egg_window` visibly wander the desktop on its own (see below).

Four new action ops (`ops/feed_pet.c`, `ops/clean_pet.c`,
`ops/toggle_sleep.c`, `ops/train_pet.c`) are reachable from a new
`pet_detail` menu screen (`menu_input.c`/`compose_menu.c`) - selecting a
hatched pet in Pets now opens this screen instead of immediately spawning
the window. Training is trivial-reward by design (matches `egg-pals.txt`
§5): energy-gated XP that levels up HP/MP caps and unlocks new skills
from `pieces/registry/skills/skill_pool.txt`.

Every discrete state change (feed/clean/sleep/train/hatch/export/destroy)
appends one line to a new append-only `pieces/system/master_ledger.txt`
(the "master ledger audit trail" `mutaclsym/dox/01-cdda-architecture.md`
§3 describes, not built anywhere until now) - chosen specifically because
that shape is what a future lightweight blockchain layer would need.

`system/egg_window.c` no longer blocks forever on `XNextEvent`: it polls
the X connection with a timeout so it can also redraw/reposition on its
own between input events, reading the pet's `state.txt` each poll and
applying that tick's `move_dx`/`move_dy`, retinting by hunger/asleep
state, and overlaying small pre-rendered poop/sleep icons
(`pieces/registry/icons/`, generated once by `scripts/gen_icons.sh` via
the existing emoji pipeline). Still a dumb renderer per its own header
comment - all the deciding happens in `tick_pets.c`.

**Trading cards.** `ops/export_card.c` snapshots a pet's stats/moves into
a PNG (`exports/<card_id>.png`) styled like a trading card - sprite,
HP/level, moves resolved against the skill registry, and a plain
incrementing `card_id` (no QR code, dropped per direct instruction). It
refuses to mint a second card while one is `issued`; `ops/destroy_card.c`
is the local, offline stand-in for a future on-chain burn that clears the
gate. Text is drawn with a small hand-authored vendored font
(`system/lib/bitmap_font5x7.h`, same vendoring convention as
`stb_image.h`).

## Step 5 — invisible desktop grid, facing, sprite-shaped window (built)

Per `#.wussup🥚️.txt`'s modified direction (grid is invisible and mapped
onto the user's real desktop, not a terminal grid - same functional
shape either way): `ops/tick_pets.c` no longer proposes free pixel jitter,
it proposes a `grid_step_x`/`grid_step_y` in `{-1,0,1}` per tick, and sets
`facing` (`left`/`right`) whenever `grid_step_x != 0`. `system/egg_window.c`
owns the actual grid - it's the only process that knows the display size -
tracking its own `grid_x`/`grid_y` in cell units (`GRID_CELL_PX = 80`),
clamped to the screen's cell bounds, and converting to pixels for
`XMoveWindow`. Dragging with the mouse is still free-pixel while held,
then snaps to the nearest cell on release.

The window is no longer a circle: `build_shape_mask()` derives the X11
Shape Extension mask from the sprite's own alpha channel (upscaled
nearest-neighbor to the window's pixel size, same technique
`ops/export_card.c`'s `blit_sprite` uses) instead of `XFillArc`, and
rebuilds it (mirrored) whenever facing flips, so the window's clickable
silhouette always matches what's actually drawn - verified via the Shape
extension directly (`XShapeGetRectangles`): identical total area in both
facing states, with each rectangle's position an exact horizontal mirror
of the other. Falls back to the original circle only if no sprite loaded.

## Step 6 — x/y/z position, movement trait (built)

Position moved from `egg_window.c`'s own transient memory into
`ops/tick_pets.c`'s authoritative `grid_x`/`grid_y`/`z` fields on the
pet's own `state.txt` - the point being it's now always displayable in
the terminal (`pet_detail` screen shows `Position: (x,y,z) Movement:
<type>`) even with no window open, not just something the GL window
happened to be tracking. `tick_pets.c` clamps x/y to a fixed
`WORLD_GRID_W x WORLD_GRID_H` logical grid (independent of any real
display); `egg_window.c` re-clamps to the real screen's cell bounds when
converting to pixels, since it's the only process that knows the
display size. Dragging a window is the one exception to "egg_window
never writes pet state" - dropping it writes the new `grid_x`/`grid_y`
back (verified: `grid_x=16`→`x=1280px` exactly matches `16*80`), because
that's recording user input, not deciding pet behavior.

`z` is bounded by a new `movement` trait on the species registry
(`pieces/registry/emoji_pool/common_emoji.txt` gained a 5th column:
`ground`/`flying`/`digging`/`aquatic`, copied onto the pet at
`generate_egg.c` time) - ground pets stay pinned at z=0; flying roams
z∈[1,4]; digging/aquatic roams z∈[-4,0]. Verified across 40+ ticks each
for a flying pet (owl) and a digging pet (mouse) - z never left its
band. Rendered as a simple vertical pixel offset
(`grid_y*GRID_CELL_PX - z*Z_PIXEL_OFFSET`) - confirmed pixel-exact
against the terminal's reported z.

## Step 7 — independent, self-ticking pet processes (built)

Superseded the earlier "close the window when its terminal session ends"
design (`egg_window` briefly took a session-PID argument for this - now
removed) once actual usage clarified the opposite was wanted: a pet
should keep running - and recording data - as its own process once
opened, independent of the terminal, until something explicitly closes
it (right-click Close, a keypress, or `scripts/kill_pets.sh`). That
requires ticking itself to stop being terminal-bound, not just the
window's lifetime, or an outlived window would just sit frozen.

**Two tick modes, made compatible instead of exclusive.**
`pieces/system/tick_config.txt` (new) controls both: `world_tick=1` -
`pal/main_loop.pal`'s turn loop still bareword-dispatches `tick_pets`
with no args every ~100 iterations, sweeping every pet, exactly as
before; `self_tick=1` - each pet's own `egg_window`, once opened, now
also invokes `tick_pets.+x <pet_id>` on its own timer (every
`tick_interval_sec`, default 3), independent of any terminal - this is
what lets a pet keep ticking after its terminal session closes. Both can
be on at once safely: `ops/tick_pets.c` gained a `last_tick_ts` field per
pet, checked in `tick_one_pet()` regardless of which mode is asking - if
less than `tick_interval_sec` has passed since a pet's last tick, the
call is a no-op. Whichever of world-tick/self-tick reaches a pet first
in an interval wins it; the other doesn't double it. `tick_pets.+x` also
gained single-pet mode (`tick_pets.+x <pet_id>` ticks just that one,
skipping the directory scan) for self-tick to call without sweeping
everyone else too.

**The process registry / monitor**, per direct instruction ("a monitor
of all running processes... doesn't have to run... master record
ledger... independent for now, but could be coupled [for] fights/ item
interacting/ storage etc"): the per-pet `window.pid` marker
(`pieces/world_01/map_lobby/<pet_id>/window.pid`, already written by
`menu_input.c`'s `spawn_egg_window` for its duplicate-window guard) is
the source of truth for "what's alive right now" - a new `Processes`
screen on the terminal main menu (`ops/list_processes.c`, an on-demand
snapshot refreshed only when the screen is opened, not a background
daemon) scans those markers and reports pet_id/pid/alive status.
`egg_window.c`'s `append_window_ledger` logs `window_opened`/
`window_closed` events to the existing `pieces/system/master_ledger.txt`
alongside `tick_pets.c`'s own state-change entries, so that ledger is now
the continuous record of both state changes and process lifecycle.
Processes are deliberately kept independent for now - no messaging
between them - but the registry is the seam a later mode (fights, item
interaction, shared storage) would hook into for actual inter-process
coordination; not built now, noted below.

**Cross-platform bug found and fixed while verifying this**: `egg_1`'s
`state.txt` had CRLF (`\r\n`) line endings from being touched cross-
platform at some point, and `tick_pets.c`'s `type=pet` check compared
the raw value against a bare `"pet\n"` - which never matches `"pet\r\n"`,
so `tick_one_pet()` silently returned immediately every single time,
for *any* caller, world-tick or self-tick alike. Self-ticking looked
completely broken until this was traced down (confirmed via `cat -A`
showing `^M$` line endings on the affected file but not on pets created
purely on this machine). The same latent bug existed everywhere else in
the codebase that trimmed a line with `strcspn(x, "\n")` instead of
`strcspn(x, "\r\n")` - `compose_menu.c`'s `read_kv_str` (used for every
screen/field comparison in the whole menu), `export_card.c`'s
`card_status`/`skills` parsing (would have silently let a second card
be minted, since the "already issued" string comparison would never
match), `egg_window.c`'s `facing` parsing (sprite would never mirror),
and half a dozen more - all fixed to strip both characters. Only
`system/emoji_gen_atlas.c` already had this right (`strcspn(val,
"\n\r")`), from earlier work - worth using as the reference next time
this class of file-parsing code gets copied into a new op.

## What's NOT built yet (in the order to build it)

Roadmap only past this point - none of the below is built, this section
exists so none of it gets forgotten before its turn comes up. See also
`dox/02-editor-and-modularity.md` for a deeper, grounded exploration of
the map/tile-picker/event-editor/multi-project direction touched on in
items 5-7 below - kept as its own doc rather than expanding this list
further, since it's detail on *how*, not another ordered list of *what*.

1. **Poop as its own window/piece**, not an overlay icon. `#.wussup🥚️.txt`
   asks for pooping to "create new windows like poop" - i.e. poop should
   become a real `type=item` piece with its own directory/state sitting on
   a grid tile (spawning its own small window), the same way a dead
   creature becomes food via a type-flip in Moke-Pet's model, rather than
   the current `poop_count` overlay-icon-in-the-pet's-own-window approach.

2. **Farming: plant, grow, harvest.** Same underlying pattern as poop
   (above) - a planted crop is a `type=item` piece sitting on a grid tile
   with its own tick-driven growth state (a `growth_stage`/`ready_at`
   field advanced by `tick_pets.c` or a sibling epoch op the same way
   hunger/poop already are), harvestable into the owner's inventory once
   ready. Build right after poop-items since it's the same
   piece-on-a-tile mechanism applied to a second case, not a new one.

3. **Tile interaction / the fuller per-piece FSM engine.** "if it goes on
   another tile space (another pet or item, it may interact)" - needs a
   collision/interaction resolver, which is really the fuller per-piece
   scripted FSM engine (`bot::navigate`, `bot::interact`, etc. per
   `mutaclsym/dox/01-cdda-architecture.md` §4's "more general,
   not-yet-built design"), replacing tick_pets.c's simple random walk with
   real goal-directed behavior. This is also where **self-cleanup**
   belongs (a pet autonomously walking to and clearing its own poop tile
   using a "clean" skill/method instead of waiting for the user to click
   Clean) and where **learning to jump/traverse obstacles** (for
   platformer mode, see below) would eventually attach - both are this
   same "pet decides to walk somewhere and interact" capability applied to
   a specific goal, not separate engines. Don't build before poop/farm
   items (above) exist - there's nothing to interact with yet.

4. **Basic pet chat + vocabulary learning.** Prioritized ahead of the
   window-management items below since it's largely self-contained: a
   `speak`/`chat` op (the "says random words" behavior `egg-pals.txt` §5
   already recaps, extended into two-way) that lets the user type text at
   their pet and the pet responds, building its own learned-vocabulary
   list over time via a trivial RL reward/punishment signal - same
   "trivial reward first" spirit as `train_pet.c`'s XP (do a recognized
   word/phrase = positive, gibberish = neutral/negative), not a real
   language model. This is a **local, single-pet feature** - distinct
   from and much simpler than the multiplayer/P2P chat under Networking
   below. The reward/punishment control this introduces (a quick
   thumbs-up/down on the pet's last action) should be a standing UI
   element in every pet context window going forward, not just chat's -
   later features (combat, farming, obstacle-jumping) reuse the same
   feedback control rather than each growing its own. UI placement per
   `#.wussup🥚️.txt`: "a little dialog box window at bottom of users
   screen... more invisible even than wraith" - a small, unobtrusive
   desktop dialog, not a terminal prompt.

5. **Keyboard cursor + active-target + focus highlight.** A visible
   cursor - 🔪 as a placeholder glyph for now, own small window on the
   same invisible grid, its own piece (`type=cursor`, not attached to any
   one pet) moved by arrow keys - that can be moved onto a pet/item's tile
   to select it, after which "cursor methods" (feed/clean/train/interact/
   whatever's already invoked from `pet_detail` today, or a new
   cursor-specific verb set) act on whatever is currently selected. This
   is not a new idea to invent: it's the **xlector active-target pattern**
   `mutaclsym/dox/01-cdda-architecture.md` §3 already documents (found
   in `projects/fuzz-op/manager/fuzz-op_manager.c` in the 1.TPMOS
   reference tree) - one `active_target_id` value that all verb dispatch
   routes through, so pet management/combat targeting/examine-at-a-distance
   all share one input path instead of each growing bespoke targeting
   logic. Whatever piece is the current active_target should render a
   small focus indicator (highlight/glow/icon overlay) in its own window
   so it's visually obvious what's selected - this is the concrete answer
   to "if something has user focus (was clicked last), it should have a
   focus icon or be highlighted." Sits right before click-to-open context
   windows (below) on purpose: keyboard cursor and mouse click are two
   input directions onto the *same* active-target mechanism, and should
   share one implementation rather than growing two. Not blocked on
   poop/farm items existing, but gets more useful once there's more than
   pets to select. Also the same object used for tile-painting once a
   map editor exists - see `dox/02-editor-and-modularity.md` §3.

6. **Click-to-open context windows**, modeled on 1.TPMOS's `wraith_gl.c`:
   it renders semantic, hit-testable objects and dispatches clicks to
   receipt files rather than tracking only raw pixels - that's the pattern
   to port for "click a pet/item, get a context window with actions" per
   `#.wussup🥚️.txt`, not a from-scratch click-detection scheme. Shares the
   active-target mechanism from the cursor (above) - a click just sets
   active_target_id the same way moving the cursor onto a tile does.
   `egg_window.c`'s header already earmarks appending raw click events to
   a history file for prisc+x to read as the attachment point.

7. **Folder/storage windows.** Every pet/item here is already a
   directory-backed piece, so a "folder window" is a browser over a
   directory's children that spawns child icon-windows on open - a new
   class of window (a container browser), not a variant of `egg_window.c`.
   Depends on click-to-open (above) existing first. The tile/emoji
   picker window and multi-project save/load explored in
   `dox/02-editor-and-modularity.md` §2-3 are close cousins of this same
   "window over many small things" class - worth building together.

8. **Window manager niceties**: more than one window optional to open at
   once from the menu, and a "minimize whole thing to a tab at the bottom
   of the screen" affordance for when the user needs to focus on their
   actual desktop. Sequenced after context/folder windows (above) since
   there's more than one window *type* to manage by then, not just
   multiple instances of one.

9. **Battles** (and item interaction / shared storage generally). Pet-vs-
   pet combat - the skill registry (`pieces/registry/skills/skill_pool.txt`,
   `power`/`mp_cost` per move) already has the shape a battle resolver
   needs, it just isn't wired to one yet. Depends on tile interaction
   (item 3) - a battle is naturally one more thing that can happen when
   two pets share a tile, and the cursor (item 5) is a natural way to
   pick a battle target/opponent. This is also the first thing that would
   need actual inter-process communication between two independent
   self-ticking `egg_window` processes (Step 7) - today they don't talk
   to each other at all, only the `window.pid` registry knows both exist.
   Don't design the messaging mechanism until this is actually up next.

10. **"Platformer mode"** - an alternate mode where z stops being the
    ground/fly/dig/swim band from Step 6 and becomes literal jump height,
    levels are horizontally-scrolling obstacle courses, and there's a
    dedicated map/level window (2D, or 3D if that turns out feasible;
    physics-based or turn-based, undecided) for level control. Pets
    learning to jump/traverse obstacles is the FSM engine (item 3) plus RL
    applied to a platforming goal, not a new AI system. An **attention
    mechanism in the FSM** (the pet learning to weight competing signals -
    hunger vs. an obstacle vs. the user's input - rather than reacting to
    whichever tick_pets.c last touched) is the natural next step in
    sophistication once this and battles (above) both exist and there's
    enough competing behavior for "attention" to mean something concrete.
    The most exploratory/speculative item here - expect this to get
    re-scoped once it's actually up next.

11. **The import/export bridge to other games** (`egg-pals.txt` §6) -
    explicitly future work, and now that trading cards exist, the natural
    next question once at least one other game in this family exists: does
    a card's `card_id` become the join key a converter maps through? Don't
    design this until there's a second game to actually map to.

12. **Networking: the future blockchain/P2P/PvP trading/multiplayer chat
    layer.** Explicitly not started per direct instruction - what's built
    now (the append-only `master_ledger.txt`, the `card_id`/issue-destroy
    gate) is deliberately shaped to be compatible with it later, not an
    implementation of it. Distinct from item 4's local pet chat - this is
    player-to-player.

13. **Sound effects / TTY.** Explicitly sequenced after chat (item 4)
    matures further per direct instruction, not before.

## Conventions already established here — keep following them

- No shared headers. Every new `ops/*.c` copies the ~15-line
  `resolve_root()`/`PATH_BUF` boilerplate from `generate_egg.c` rather
  than factoring it out.
- Plain pipe-delimited or `key=value` text everywhere, including pixel
  data (the CSV format, not a binary blob) — matches "if it's not in a
  file, it's a lie."
- Registry data (`pieces/registry/*/`) is pure data files, never requires
  a code change to extend — `common_emoji.txt` is the template.
- Every op that needs a bounded string copy: use `snprintf(dest,
  sizeof(dest), "%s", src)`, not `strncpy` (the latter trips
  `-Wstringop-truncation` under `-std=c11 -Wall -Wextra` even when
  followed by manual null-termination — this was hit and fixed in
  mutaclsym's `system/prisc+x.c`, don't rediscover it here).
- A pal script needs an *unconditional* `compose_menu` + `hit_frame` call
  before its main loop starts, not only inside the per-keypress branch —
  otherwise a truly fresh boot (no leftover `current_frame.txt` from a
  prior run) shows nothing until the first keypress. Hit this bug and
  fixed it here (and retroactively in mutaclsym) — `pal/main_loop.pal`
  already has the fix, keep it there if the script gets rewritten.
- prisc+x's generic custom-op dispatch passes exactly one argument to a
  handler (see `menu_input.c`'s own header comment) — don't design a new
  op assuming it can receive two pal-supplied values at once; route
  through a hardcoded default or a pointer file instead.
- Keep `./button.sh compile` at **zero warnings**. It is right now —
  don't let that slip as new ops get added.
