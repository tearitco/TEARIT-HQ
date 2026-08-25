# Cross-game pet import/export - reference implementation and what's still missing

Built to answer a direct ask: "make functionality that all of our
games... can import and export pets by using drag and drop, and they
will go on living their lives." This doc is honest about scope: the
FILE-LEVEL mechanism below is real, working, and live-verified
end-to-end (export a pet out of a running game, re-import it, it's
fully controllable again with its stats intact). The actual DRAG-AND-
DROP GESTURE - dragging a visual pet window and dropping it somewhere
to trigger this - is a separate, real piece of engineering that is
NOT built yet. Read §3 before assuming this is a complete feature.

## 1. What's real and working right now

`ops/pet_export.c` / `ops/pet_import.c` (read both files' own header
comments for full detail) implement:

- **Zero-sum, physical directory move** (`rename()`), exactly like
  mutaclsym's own `pickup.c`/`drop.c` already do for hero inventory
  (see mutaclsym's dox/00-HANDOFF.md's own inventory writeup) - a
  piece's location IS which directory it's currently in, one level
  further out here: moving a piece OUT of any single game's world
  entirely, into a directory living ONE LEVEL ABOVE any individual
  game's own project root (`z0.egg-pals+plats-8.00/exchange/` for this
  family specifically - override via `PRISC_EXCHANGE_ROOT` if a
  different sibling layout is needed).
- **A neutral `trade_envelope.txt`** written into the pet's own
  directory before the move - the exact format mutaclsym's own
  `platform-vision.txt` §2 proposed but never built (`origin_game|
  kind|payload_id|display_name|...` pipe-delimited rows), now made
  real. This is what makes the export op genuinely agnostic: it never
  needs to know anything about whatever game eventually imports the
  pet - that's the IMPORTING game's own problem, on the other end.
- **Best-effort translation on import**: `pet_import.+x` ALWAYS writes
  a fresh, native `state.txt` + `piece.pdl` for the imported pet in
  zoo_0000's own shape (feed/pet/play/export/select methods), whatever
  game it actually came from - matching platform-vision.txt's own
  "translated as best-effort into that game's own schema" framing,
  made concrete instead of speculative.
- **Live-verified this session**: exported a real pet (Rex) mid-game,
  confirmed its directory physically left `pieces/world_01/map_zoo/`
  and landed in `exchange/pet_rex/` with a correct envelope, confirmed
  xlector's `active_target_id` self-healed back to `xlector` (so
  nothing was left pointing at a now-nonexistent piece), ran
  `pet_import.+x pet_rex` manually, confirmed it reappeared as a fully
  native, fully controllable zoo_0000 pet with its hunger/happiness/
  energy correctly carried over from the envelope, re-selected it via
  xlector, fed it again successfully. Zero corruption anywhere in the
  round trip.

## 2. "They will go on living their lives"

Once imported, a pet is a completely ordinary zoo_0000 pet - the same
`decision_mode=0` wander AI (`ops/tick_pets.c`) applies to it
identically to any pet that was always native. There is nothing
special about an imported pet's runtime behavior; the whole point of
always writing a fresh native `piece.pdl`/`state.txt` on import is that
it stops being "a guest" the instant import succeeds.

## 3. The visual layer - ARCHITECTURE SPLIT, corrected after direct feedback

**Important correction, read this before touching any window code
here.** An earlier pass this session built `system/zoo_window.c` (a
per-pet, egg_window.c-style SHAPED desktop window) and, separately, a
`system/gl_mirror.c` (a normal bordered window mirroring the whole
level). The user directly corrected the intended split after seeing
both: **zoo_0000 itself owns the LEVEL view only** (map + xlector +
pets, rendered flat, matching the ASCII view exactly - "one state,
multiple renderers", not a deliberately emptied level). **Per-pet
desktop-window dragging is being built independently in a SEPARATE
project, `z0.egg-pals+13`**, not inside zoo_0000. The two will
eventually talk to each other "thru a shared data file" (the user's own
words) - not decided/designed yet, but the existing `exchange/`
directory and `trade_envelope.txt` format from §1 above are the most
likely real building blocks once that design happens, since they
already ARE a shared-file communication channel between two
independent programs.

**What's real and built, as of this correction:**
- `ops/compose_rgb_frame.c` + `system/gl_mirror.c` - the mutaclsym-
  style GL level mirror. Direct port of mutaclsym's own pair of files
  (flat per-tile color, zero GL calls in the CPU-compute half, a
  single dedicated GL-calling file that just blits a texture - see
  either file's own header comment for the full port-notes). Renders
  the SAME map/pets/xlector the ASCII view shows (pets as flat colored
  blocks per species - see `species_to_rgb()` - not sprites; sprites
  are the OTHER project's job). Auto-launched by `./button.sh run`,
  exactly like mutaclsym's own `gl_mirror` - this was the actual,
  concrete ask ("i expected it to open when normal program was open
  like mutaclysm did") that the earlier per-pet-window build had
  missed. Live-verified via a real screenshot (`xwd`+`ffmpeg`):
  correct walls/floor colors, both pets at their real positions in
  their own species color, cyan xlector cursor, matching header/footer
  text via the same font-glyph asset pipeline mutaclsym's own
  `compose_rgb_frame.c` uses.
- `system/zoo_window.c` (the per-pet shaped window, X11 Shape
  Extension + GLX, drag-to-grid-snap) - **still exists and still
  works** (real sprite rendering, real drag simulated via XTest, real
  bidirectional `pos_x`/`pos_y` sync with the terminal, all
  live-verified - see this doc's git history / the file's own header
  comment for the technical detail), but is **NOT launched by `./button.sh
  run`** anymore and is not the direction being pursued for cross-game
  dragging specifically - kept only as a manually-invokable reference
  (`./button.sh window <piece_id>`) and as a working example of the
  underlying mechanism (X11 Shape masking, grid-snap, position write-
  back) that `z0.egg-pals+13`'s own from-scratch build may or may not
  end up resembling.

**Not decided, explicitly**: the exact shape of the "shared data file"
the two programs will use to hand a pet back and forth visually (is it
the existing `trade_envelope.txt`/`exchange/` pattern, extended with a
live position-update channel? A different file entirely, since §1's
own mechanism is a one-shot MOVE, not a live per-frame position sync
the way `zoo_window.c`'s own `pos_x`/`pos_y` reuse is?). This is real
design work for whoever picks up the `z0.egg-pals+13` side next -
flagged here so it isn't lost, not answered speculatively.

## 4. Open questions, not decided

- Where does an imported pet land on the map? Currently a fixed point
  `(2,2)` - real placement logic (avoid overlapping another piece,
  spawn near xlector, etc.) is a reasonable enhancement, not built.
- What happens if a pet is imported with a `species` this game's own
  `pieces/registry/pets/pet_types.txt` doesn't recognize (no glyph
  mapping)? Currently falls back to `'?'` in `compose_frame.c`'s own
  `species_glyph()` - functionally fine (the pet still exists and is
  controllable), just visually generic. A real cross-game species/
  glyph registry is future work, not started.
- User/ownership association (does an exported pet remember WHO
  exported it, for a future trading/auction platform) - see the
  sibling `z0.user/dox/00-exploration.md` for that open question, kept
  deliberately separate from this doc since it's a genuinely
  independent design axis.
