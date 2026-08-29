# Maps / Tiles / Z-Levels — consolidated real spec (2026-08-29)

**CORRECTION, same session, found via a direct "past 3 days, mentions
xdnd" search after this doc was first written**:
`TILE-SYSTEM-DESIGN.md` (same directory, dated 2026-08-27) is the
REAL, primary, authoritative doc for how a placed 2D desktop tile
works — read it FIRST, before this doc. It resolves the single-tile
question this doc's own §2 left as "not built, not spec'd" with a real
answer (a tile is a real `tp_desktop_window_rgb.c` entity, not a
separate canvas), has real autotile math already ported and pixel-
verified, and cites the REAL 2D→3D bridge doc (`#.DOX/drag-drop-
how2.md` — not `101.drag-drop-test=ON🀄️`, which is a test HARNESS for
that same real mechanism, not the spec itself). This doc's own real,
still-standing value is the piececraft-xyz/mutaclysm/cursword/Z-level
material `TILE-SYSTEM-DESIGN.md` does NOT cover (chunked voxel worlds,
Z-level navigation, the piececraft-specific extension of the transfer
mechanism) — read both, `TILE-SYSTEM-DESIGN.md` for single-tile/
autotile/palette-picker mechanics, this doc for multi-tile chunked
maps and the 3-way desktop/piececraft/mutaclysm relationship.

**Why this doc exists**: direct user correction tonight — "previously
we had spec'ed out that we could have both maps, individual tiles, or
both... is there not documentation of this specific spec? maybe its
disparate?" Confirmed: yes, genuinely disparate. This session's own
`GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` framed "tile/map
authoring" as a from-scratch gap needing a fresh 2D RPG-Maker-style
design (walk-around map, Options A/B/C — that framing was **wrong**,
written and pushed before this doc existed, now retracted). The REAL,
much more developed spec already exists, just never cross-referenced
with the events/game-readiness lineage this session was working from.
This doc is the missing cross-reference — read it before touching
maps/tiles/z-levels/palettes/piececraft/xelector ever again, so this
doesn't get rediscovered painfully a third time.

---

## 1. The real, disparate source docs (all pre-existing, all real)

| Doc | What it actually covers |
|---|---|
| `@.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md` | **The real spec.** A tick-based voxel-roguelike map/world system: chunked, per-Z-level flat ASCII grid files, compression/decompression of distant chunks, xlector-pattern cursor reuse, board-viewer rendering (extended via data-driven `terrain_legend.txt` + `ops_bank.txt`, NOT forked). Has a real open-questions section **with your own real answers already recorded** (§10) and a real phase order (§11). Status: **Phase 0 partially done** (terrain-legend retrofit shipped 2026-08-03; `ops_bank.txt` verb-dispatch NOT done — see §4a for the real, separate "board-viewer discovery/pairing" mechanism that WAS shipped instead, and why it's a different problem than ops_bank). Phases 1-5 (world storage, place/break, compression, crafting, mobs) are **real plan only, nothing built**.
| `101.mutaclsym…/dox/xelector-context.md` | The real, live "walk a cursor over the world, open a numbered context menu, act on what's under it" mechanic already working in mutaclysm today (`interact_mode=1` + free `xlector_pos_x/y` cursor fields on hero state, separate from `possessed_id`; fixed numbered-row context menu convention `1 Event · 2 Copy · 3 Paste · 4 Delete · 5 Exit`). PIECECRAFT_XYZ_DESIGN.md §3a already cites this as the pattern to reuse (not the code — each project gets its own cursor fields, same shape) for both hero-select AND piececraft's own block-targeting.
| `CURSWORD-SOUL-VISION.md` §5 | Corrected an earlier, too-pessimistic reading of "0% built" for tile/palette groundwork — pointed at `pallets.pdl`'s real `rmmv` category, `tp_rmmv_character_extract.c`, and `RMMV_EVENT_EDITOR_GUIDE.md`. That last pointer turned out to be a **false lead for THIS spec** — `RMMV_EVENT_EDITOR_GUIDE.md` is a separate, superseded prototype for event-COMMAND-editing chrome (not map/tile placement), and events-hq (this session's own work) has already surpassed its real functionality. Corrected here so nobody chases that thread again for map work specifically.
| `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` §1 (gap #1) | The doc that started tonight's confusion — real, but scoped ONLY to "can a player walk an RPG-Maker-style event-triggering map," which is a genuinely different, narrower question than piececraft-xyz's full voxel/chunk/Z-level vision. Its own gap #1 should be read as "a special case of piececraft-xyz's Phase 1-2, not built yet," not as its own separate design problem — see §3 below for how the two actually relate.
| `&.widgits/board-viewer/` (code, not a doc) | The real, shared, focus-adaptive 3D rendering widget every one of the above reuses (civ-txt, tactics-txt, piececraft-xyz, mutaclysm's own camera work). Terrain rendering is now real, data-driven (`terrain_legend.txt` per host) — confirmed, not aspirational, per PIECECRAFT_XYZ_DESIGN.md §0a's own "STATUS 2026-08-03: DONE."
| `101.drag-drop-test=ON🀄️/README.txt` | **The real desktop↔mutaclysm transfer mechanism — built and tested, 2026-07-26, not a stub.** Real X11 Xdnd drag-and-drop, verified end-to-end: a real desktop-tile pet window (`muchi-pals egg_window`) dragged onto mutaclysm's own `gl_mirror` window, with real "pet imported" verification against a real `exchange/` directory (`dd_check_import.+x <exchange_dir> <pet_id>`). A full reusable ops-based test harness exists (`dd_set_positions`/`dd_find_window`/`dd_move_window`/`dd_drag_drop`/`dd_assert_file`/`dd_check_import`), each independently callable. This directly answers the "pulled out onto desktop" / "dropped into muta map" half of §2's new transfer-model question below — the real mechanism (Xdnd + an `exchange/` handoff directory) already exists for desktop↔mutaclysm; piececraft-xyz's own "drop a tile into piececraft" would reuse this SAME real mechanism, not invent a second one.

---

## 2. Your own new statements tonight — real, not yet written down anywhere before this doc

Direct quote, preserved verbatim so this doesn't drift a second time:

> "previously we had spec'ed out that we could have both maps,
> individual tiles, or both. that they would come from 'palettes' and
> its not just 2d. there are z levels as well... we will use
> 'cursword' to change z levels, and navigate the view of the map,
> just like 'xelector' is used in mutaclysm. maps will be able to be
> loaded into 'piececraft' or individual tiles will be able to be
> dropped into piececraft/muta map or pulled out onto desktop."

Breaking down what's **new** here versus what §1's docs already cover:

- **"both maps, individual tiles, or both"** — NEW, not in
  PIECECRAFT_XYZ_DESIGN.md as written. That doc assumes a whole
  chunked world; it doesn't discuss a standalone SINGLE-TILE object
  that can exist independent of a map (e.g. a tile placed loose on
  the desktop, not part of any chunk).
- **"they would come from 'palettes'"** — CONFIRMED, real, already
  true: `tileset_registry.pdl` + `pallets.pdl`'s `rmmv` category ARE
  the real source-of-truth for tile art today (§1 above). What's NOT
  yet spec'd is palettes as the source for PLACING tiles into a
  piececraft map/chunk grid — today palettes only feeds board-
  viewer's `terrain_legend.txt` (a rendering lookup table), not an
  actual "drag this specific tile onto a map cell" placement flow.
- **"cursword changes z-levels, navigates the map view, like
  xelector"** — NEW. PIECECRAFT_XYZ_DESIGN.md §3 flags "Y-axis
  (vertical) selector movement... Needs a key from you" as a real
  open question (never answered in that doc's own §10) — this
  message answers it, but with a different actor than that doc
  assumed: not just a keypress, but **cursword itself** (the user's
  SOUL entity, `CURSWORD-SOUL-VISION.md`) as the real agent driving
  z-level navigation and view control, xelector-pattern. This is a
  real, new integration point between two previously-separate docs
  (CURSWORD-SOUL-VISION.md and PIECECRAFT_XYZ_DESIGN.md) that neither
  one mentions the other for.
- **"maps loaded into piececraft, or tiles dropped into piececraft /
  muta map, or pulled out onto desktop"** — HALF real precedent, half
  genuinely new. The real mechanism for desktop↔mutaclysm transfer
  already exists and is tested: `101.drag-drop-test=ON🀄️/` (real X11
  Xdnd drag-drop + a real `exchange/` handoff directory, verified
  round-trip for pet import — see §1's new entry above). What's
  genuinely NOT yet spec'd: (a) the same mechanism extended to
  piececraft-xyz specifically (that harness only tests desktop↔
  mutaclysm today, not desktop↔piececraft or mutaclysm↔piececraft),
  and (b) whether "a tile" and "a map" use the SAME real Xdnd
  exchange-dir mechanism or need their own real format (a single tile
  is presumably a small, existing entity/pal-shaped payload; a whole
  MAP being dragged is a much bigger, chunk-shaped payload with real
  open questions of its own — does dropping a map onto piececraft
  import all its chunks at once, lazily, or just register a
  reference?).

---

## 3. How gap #1 (events-hq's own "can a player walk a map") actually relates

Tonight's real, shipped events work (Task 1, Flow Control, the whole
Scripting/Scratch picker) needs SOME kind of map for its own
Autorun/Parallel-triggered, touch-triggered events to make sense as
"a little game" — that's real and still true. But per this doc's own
finding, that's not a separate system to design from scratch — it's
the same real map/chunk/tile system PIECECRAFT_XYZ_DESIGN.md already
specs, just consumed from events-hq's own trigger mechanism once real
map data + player position exist. Concretely: an event page's
Autorun/Parallel trigger already polls a named switch (real, working
tonight); a real "touch trigger" is the same mechanism, gated on the
player's real chunk/cell position matching the event's own placed
position, once that position data is real.

**This means gap #1 should be RETIRED as its own separate design
question** — it's not gap #1 anymore, it's "PIECECRAFT_XYZ_DESIGN.md
Phase 1-2, with an events-hq trigger hookup as a real, small addition
once Phase 1-2 land." `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`
should be marked superseded by this doc for its own §1 specifically
(its §2/§0/§3 findings about event commands remain real and correct,
those are unrelated to maps).

---

## 4. Real, honest status — what's actually built today, across all of this

- Board-viewer's data-driven terrain rendering (`terrain_legend.txt`):
  **real, shipped, verified** (2026-08-03).
- Tileset asset registry (`tileset_registry.pdl`, RMMV extraction
  tooling): **real, shipped**.
- Xelector cursor pattern: **real, shipped, live in mutaclysm today**.
- Board-viewer discovery/pairing (one instance per host, ledger-based,
  like fm-widget): **real, shipped** (§4a of PIECECRAFT_XYZ_DESIGN.md).
- Board-viewer → host command bus, RECEIVING side (inbox drain on
  idle-tick): **real, shipped**.
- Board-viewer → host command bus, SENDING side (a key pressed inside
  the 3D widget → a real command): **NOT built** — blocked on real key
  decisions and a real verb list, per that doc's own §4a closing note.
- `ops_bank.txt` (generic verb→binary dispatch): **NOT built** —
  deferred to "Phase 1," not started.
- Any real chunk/world storage, terrain generation, place/break,
  compression: **NOT built** — Phase 1-3, plan only.
- Cursword-driven Z-level nav / xelector-pattern reuse for piececraft:
  **NOT built, not even fully spec'd yet** — this is the real, new
  gap this doc surfaces (§2 above), not previously written down.
- Desktop↔mutaclysm drag-drop transfer (Xdnd + `exchange/` handoff
  dir, real pet-import round-trip): **real, built, tested**
  (2026-07-26) — see `101.drag-drop-test=ON🀄️/`.
- That SAME mechanism extended to piececraft-xyz (desktop↔piececraft,
  mutaclysm↔piececraft), and a real map-sized (not just tile-sized)
  payload format: **NOT built, not spec'd**.
- Palette↔map tile placement (drag a specific tile from a palette
  onto a map cell): **NOT built, not spec'd**.

---

## 5. Real next steps (not started — this doc is the record, not the build)

1. **Retire gap #1** in `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`
   per §3 above — a doc edit, not code.
2. **Write the two genuinely new integration specs** this session's
   conversation surfaced (§2 above) as real additions to
   `PIECECRAFT_XYZ_DESIGN.md` (or a cross-referenced sibling doc) —
   NOT designed yet, needs the same real question-and-answer pass §10
   of that doc already got:
   - Cursword's real role in Z-level nav / map-view control — what
     does it actually control day-to-day (just Z-depth, or full
     camera), and how does that interact with piececraft's own
     existing camera_mode 1-4 scheme?
   - The real palette→map tile-placement flow.
   - Extending the ALREADY-REAL `101.drag-drop-test=ON🀄️/` Xdnd/
     exchange-dir mechanism to cover desktop↔piececraft and
     mutaclysm↔piececraft (not just desktop↔mutaclysm, which is the
     only pair that's actually tested today), plus a real answer on
     tile-sized vs map-sized payloads (see §2's own note above).
3. **Then**, and only then, pick up PIECECRAFT_XYZ_DESIGN.md's own
   real Phase 1 (world/chunk storage + terrain gen + camera/xlector
   wired for real 3D nav) — the actual first buildable slice, per
   that doc's own phase order, now correctly informed by the Z-level/
   cursword/transfer answers from step 2 instead of guessed.
