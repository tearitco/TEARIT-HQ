# CIV-VS-PIECE — divergence investigation & decision record

**Status: DECISIONS LOCKED, IMPLEMENTATION NOT STARTED.** Written
2026-08-03, directly after piececraft-hq's P1 clone was verified working
end-to-end (setup screen, board generation, board-viewer widget rendering
real terrain). This doc is the bridge between "a working civ-txt clone"
and "PIECECRAFT_XYZ_DESIGN.md's actual Phase 1" — it inventories every
piece of the current clone that's civ-shaped instead of Minecraft-shaped,
records the real decisions made to resolve each one, and gives a build
order. Read `PIECECRAFT_XYZ_DESIGN.md` first — this doc doesn't repeat
that design's own reasoning, only the concrete choices made against it.

---

## 1. Why this doc exists

The clone-first approach (see `mutant-clone.txt`) was correct: prove the
skeleton (real CHTPM nav, real ops dispatch, real board-viewer pairing)
works before touching game-specific logic. That's done and verified. But
the clone is *civ-txt's* skeleton wearing piececraft-hq's name — every
screen, option, and data file still thinks it's a strategy game. Before
writing any Phase 1 code (chunk storage, Z-levels, tick model), every
civ-shaped assumption needs a named replacement. Guessing at these one
file at a time mid-implementation is how the original mutation happened
(see `mutant-clone.txt` root cause) — deciding them all up front, in one
place, is the fix.

---

## 2. Decisions made this session (source of truth)

These were asked and answered directly — implementation should treat
these as settled, not re-litigate them:

| Decision | Answer |
|---|---|
| Divergence scope | Real Phase 1 (design §11), not just cosmetic reskin |
| Setup screen content | World seed + chunk size + active radius (replaces Victory/Map/Combat) |
| Turn/tick model | Both: movement ends a tick AND a manual "End Turn"-equivalent button stays available |
| Terrain vocabulary | Expand now to dirt/stone/grass/sand/water/wood/leaves/ore (not civ's 5 glyphs) |
| Chunk size (§1a) | 16×16 (matches real Minecraft's own convention) |
| Z-height range (§1a) | 32 layers |
| Jump key | Space bar (already fixed by design §10 Q3) |
| Mine key | `g` |
| Build key | `h` |
| Mine/Build input style | Raw keys (not the §3a numbered xelector context-menu) — direct instruction, overrides the design doc's own tentative §3a suggestion |

**Note on the Mine/Build decision:** PIECECRAFT_XYZ_DESIGN.md §3a proposed
a numbered context-menu (`1 Mine · 2 Place · 3 Inspect · 4 Inventory ·
5 Exit`), matching mutaclysm's real xelector precedent. That's now
superseded for Mine/Build specifically — direct instruction this session
was `g`/`h` as raw always-on keys instead, once the `f`-key collision
with board-viewer's own camera-reset binding was flagged. Inspect/
Inventory/Exit are NOT decided yet (see §5 open items below) — the
numbered-menu pattern may still be right for those, since they're not
raw movement-adjacent verbs the way Jump/Mine/Build are.

---

## 3. Side-by-side: what's civ-shaped vs what Phase 1 needs

### 3a. Setup screen (`new_game.chtpm` / `pc_menu_input.c`)

| civ-txt clone (current) | Phase 1 target |
|---|---|
| `SET_VICTORY:<conquest\|score_turnlimit\|tech_score>` | removed — no victory conditions in a sandbox voxel game |
| `SET_MAP_SIZE:<small\|medium>` | replaced by `SET_CHUNK_SIZE` (fixed at 16, per §2 decision — may not even need to be a setup option if it's fixed) |
| `SET_COMBAT:<abstract\|per_unit>` | removed — no P1 combat system yet (design §9 flagged simple direct-HP for later) |
| (nothing) | `SET_SEED:<int>` — world seed, real input needed for deterministic chunk regen (§6) |
| (nothing) | `SET_ACTIVE_RADIUS:<n>` — collected now even though Phase 1 doesn't use it yet (single always-active chunk, no compression per §11) — saved to config for Phase 3 to read later, not acted on yet |
| `treasury=50`, `city_count=1` in config.txt | removed — no economy/cities concept |
| `victory_condition=` field | removed |

### 3b. Main screen (`main.chtpm` / `pc_menu_input.c`)

| civ-txt clone (current) | Phase 1 target |
|---|---|
| Static "Turn: N / Treasury: N / Cities: N" readout | Replaced with world/hero readout: tick count, hero position (chunk + local X/Y/Z), current chunk's biome |
| `END_TURN` — manual turn-advance button | Kept as a manual "advance one tick" fallback (per §2 decision), but movement/mine/build now ALSO advance `tick` by 1 on success (design §5) — END_TURN becomes a debug/wait action, not the primary loop driver |
| `OPEN_BOARD_WIDGET` | Kept as-is — this mechanism is real and already verified working, no change needed |

### 3c. World storage

| civ-txt clone (current) | Phase 1 target |
|---|---|
| Single `pieces/system/board.txt`, one flat grid, generated once by CONFIRM_START | `pieces/world_01/chunks/chunk_0_0/state.txt` + `chunk_0_0_z0.txt` .. `chunk_0_0_z31.txt` (32 Z-levels, 16×16 each), per design §1/§1a |
| No Z-axis at all | Real Z-axis, hero has `pos_z` again (this time correctly — as part of `pieces/hero_01/state.txt`, NOT `pieces/system/config.txt` as the original mutation had it) |
| No world_state.txt | `pieces/world_01/state.txt` — seed, active flag, `tick=<N>` counter (design §5) |
| No chunking at all | Single always-active chunk only for Phase 1 (design §11 explicit: "no compression yet, single always-active chunk, prove the render/nav loop") — chunk directory structure exists but only one chunk is ever generated/read |

### 3d. Terrain vocabulary (`terrain_legend.txt`)

| civ-txt clone (current, just fixed) | Phase 1 target |
|---|---|
| `.` plains, `f` forest, `^` hills, `~` water, `C` capital | Real voxel vocabulary per §2 decision: `.` dirt, `#` stone, `,` grass, `:` sand, `~` water, `T` wood, `%` leaves, `*` ore (glyphs are placeholders, pick real ones during implementation — avoid clashing with `#` which board-viewer's own comment-line detection also special-cases, per PIECECRAFT_XYZ_DESIGN.md §0a's own live-caught bug) |
| 5 rows | 8 rows minimum, real height/color/asset_hex per glyph (board-viewer already reads this data-driven — zero board-viewer changes needed, confirmed working this session) |

### 3e. Controls (`pieces/system/keybinds.txt` — currently deleted, needs recreating)

| Design doc's open question | This session's answer |
|---|---|
| §10 Q3 vertical move key | Space = jump (already fixed by design) |
| §10 Q3 mine/build keys | `g` = Mine, `h` = Build (raw keys — see §2 note above) |
| Camera modes 1-4, q/e/r/t/c/v/f | Unchanged — reused wholesale from board-viewer, already verified working |

### 3f. Hero/entity state

| civ-txt clone (current) | Phase 1 target |
|---|---|
| No hero state file at all | `pieces/hero_01/state.txt` per PORTABLE_ENTITY_ARCHITECTURE.md's universal shape: `entity_type/hp/pos_x/pos_y/pos_z/owner_id`, PLUS piececraft-hq-specific `interact_mode`/`xlector_pos_x`/`xlector_pos_y` (design §3a pattern-reuse) |
| `pieces/system/entities.txt` empty (touched but never populated) | Populated with the real hero entry once hero state exists, same generic manifest format board-viewer already reads (confirmed working for terrain/capital rendering this session) |

---

## 4. What does NOT change (already correct, do not touch)

- Board-viewer pairing/discovery (ledger_append/ledger_peers, ONLINE/OFFLINE,
  bv_state.txt refocus) — real, verified working this session, zero changes
  needed.
- `board_widget_bridge.txt` + `widget_cmds/inbox.txt` command-bus mechanism
  — real, receiving side verified working (civ-txt/tactics-txt drain it
  every idle tick already) — piececraft-hq's OWN sending side (key press
  inside the widget → command in inbox) is still unbuilt, but the pipe
  itself doesn't need rework.
- Camera modes 1-4, `q/e/r/t/c/v/f` inside 3D mode — reused wholesale,
  confirmed working.
- `terrain_legend.txt`'s FORMAT (`glyph|height|r|g|b|asset_hex|name`) —
  only the CONTENT needs to grow, board-viewer's own parsing is already
  correct and data-driven (Phase 0, done and verified).
- The ops-dispatch/piece.pdl/CHTPM nav skeleton itself — this is exactly
  what the clone phase proved works; Phase 1 changes WHAT the ops do, not
  HOW dispatch happens.

---

## 5. Still open (not decided this session, flag before implementing)

1. **Inspect/Inventory/Exit key/menu** — design §3a's numbered
   context-menu convention still applies here (not superseded like Mine/
   Build was). Needs its own real key or menu-trigger decision before
   Phase 2 (§7, place/break + inventory) starts. Not blocking Phase 1
   (world storage + nav-only, no inventory yet).
2. **Exact terrain glyph characters** — §3d above lists placeholder
   glyphs. Must avoid `#` (board-viewer's comment-line/wall-glyph
   collision, already found and fixed once for tactics-txt per design
   §0a — don't reintroduce it here).
3. **Whether `SET_ACTIVE_RADIUS` on the setup screen actually gets USED
   in Phase 1** — decided to collect it now (§2), but design §11 is
   explicit that Phase 1 has no compression, so this value sits unused
   in config until Phase 3. Worth confirming that's really wanted vs.
   just deferring the setup-screen field to Phase 3 too, to avoid asking
   the player for a number that does nothing yet.
4. **Deterministic per-chunk seed hash** (design §6) — needs "a real
   seeded-per-coordinate hash, not raw srand(time())" per the design's
   own callout. Not decided this session, needs its own real pass before
   §6 terrain-gen is implemented (Phase 1 needs at least ONE biome
   working per §11, which needs this).

---

## 6a. Xelector vs hero - real precedent, a separate entity (added 2026-08-03)

Direct correction, caught while testing z/x Z-level movement: the first
pass conflated "which Z-slice board-viewer is displaying" (a per-widget
DISPLAY toggle, `bv_state.txt`'s own `current_z`) with "where the
xelector cursor actually is" (a real, persisted position) - z/x only
ever changed the display toggle, never touched any real entity, so
nothing was actually moving through 3D space; it just LOOKED like a
display bug when the real gap was architectural.

**Real precedent, not invented**: fuzz-op's own live fixture
(`1.TPMOS_c_+rmmp.0103.0001/projects/fuzz-op/pieces/xlector/state.txt`)
has the xlector as its own **separate piece**, with its own real
`pos_x`/`pos_y`/`pos_z` - NOT fields tacked onto the hero's own state
(PIECECRAFT_XYZ_DESIGN.md §3a's own sketch had suggested exactly that
simplification - `xlector_pos_x/y` fields on hero's own state.txt -
this session's own hero_01/state.txt still has those, see §6b below for
the correction). Mutaclysm's own real `possessed_id` mechanic
(confirmed via direct code research, see PORTABLE_ENTITY_ARCHITECTURE.md
§1) is the other real half: the xlector is a free-roaming cursor that
can **possess** an entity (mutaclysm: hero-only today, `"none"` or
`"hero"`) - once possessed, movement/actions apply to the POSSESSED
entity, subject to THAT entity's own real constraints, not the
xlector's own free-roam rules.

**Direct instruction, the real split for piececraft-hq specifically**:
- The **xelector** is a cursor, not the player. Its own `pos_z` moves
  freely through the full build-height range (0..31) via z/x - "even
  into the sky or underground" - unconstrained by gravity, collision,
  or any other entity rule. This is real 3D cursor navigation, matching
  mutaclysm's own real xlector precedent exactly.
- The **hero** (piececraft-hq's own Minecraft-clone player character)
  is what the xelector POSSESSES. Once possessed, real entity
  constraints activate - jumping, mining, building, gravity, collision -
  none of which apply to the free-roaming xelector itself.
- `board-viewer`'s own `current_z` (bv_state.txt) becomes a **mirror**
  of the xelector's own real `pos_z`, not an independent value - the
  displayed Z-slice always follows wherever the xelector actually is,
  never drifts from it.

## 6b. Real implementation (2026-08-03)

- `pieces/xelector_01/state.txt` - new, real, separate entity per the
  precedent above: `pos_x`/`pos_y`/`pos_z` (free cursor position),
  `possessed_id` (mutaclysm's own real field name/shape - `none` or a
  target entity's own piece_id, `hero_01` by default here), `chunk_x`/
  `chunk_y`. Created by `pc_generate_chunk.+x` alongside `hero_01` and
  `world_01`, starting at the same position (standing on the generated
  surface).
- `hero_01/state.txt`'s own `interact_mode`/`xlector_pos_x`/
  `xlector_pos_y` fields (written by the FIRST pass, before this
  correction) were dead/unused as soon as this correction landed - the
  real xlector state lives on its own piece, not hero's - and were
  removed from `pc_generate_chunk.c`'s own hero-writing block in the
  same edit, rather than left behind as harmless-but-confusing cruft.
- `board-viewer/ops/bv_menu_input.c`'s own z/x handler now writes the
  FOCUSED host's own real `pieces/xelector_01/state.txt` `pos_z` field
  directly (same cross-project-file-write pattern `OPEN_BOARD_WIDGET`'s
  own refocus logic already uses), clamped to the manifest's own real
  `z_count` range - then mirrors that same value into board-viewer's
  own local `bv_state.txt` `current_z` so the displayed slice always
  matches the xelector's own real position, never an independent value.
- **NOT built yet, real later work**: the actual possession-driven
  constraint system (jump/mine/build only working on the POSSESSED
  entity, subject to its own gravity/collision rules) - `possessed_id`
  exists as a real field now, but nothing reads or enforces it yet.
  This is genuinely Phase 2+ scope (design §7, place/break + inventory)
  - flagging the field's real existence now so Phase 2 has real data to
  build on, not inventing the mechanic itself yet.

## 6. Recommended build order for this Phase 1 pass

1. `pieces/hero_01/state.txt` + `pieces/world_01/state.txt` (world/hero
   state skeleton, no chunk logic yet — just the files existing with
   real fields).
2. `pc_generate_chunk`-equivalent op: seeded per-coordinate hash (open
   item 4 above) → writes ONE chunk (`chunk_0_0`, 16×16×32) with one
   biome's real terrain, using the expanded `terrain_legend.txt` glyphs.
3. Rewrite `new_game.chtpm`/`pc_menu_input.c` setup screen: seed input,
   remove Victory/Map/Combat, generate the single chunk on confirm.
4. Rewrite `main.chtpm`/`pc_menu_input.c` main screen: tick/position
   readout, keep END_TURN as manual-advance fallback.
5. Wire `keybinds.txt` (space/g/h) + board-viewer's own sending-side
   inbox writes (the real gap flagged in design §4a's own "still NOT
   built" note) so Jump/Mine/Build inside the widget's nav mode actually
   reach piececraft-hq's ops.
6. `pc_place_block`/`pc_break_block` — deferred to Phase 2 per design
   §11, but keybinds for them are already reserved (g/h) so Phase 2 can
   wire the ops without touching input plumbing again.

Steps 1-4 are pure Phase 1 scope (design §11: "world/chunk storage,
terrain gen for ONE biome, camera+xlector wired for 3D voxel nav
including Y, no compression yet, single always-active chunk, prove the
render/nav loop"). Step 5 bleeds slightly into wiring the verbs Phase 2
will use, but only the key *reservation*, not the mine/build op logic
itself — worth doing now while touching keybinds.txt anyway.
