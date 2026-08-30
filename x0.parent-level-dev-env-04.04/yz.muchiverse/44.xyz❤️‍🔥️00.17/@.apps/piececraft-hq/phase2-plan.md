# PHASE 2 PLAN — piececraft-hq, written 2026-08-03

**Status: PLANNING ONLY, nothing built yet.** Written after Phase 1 (real
chunk generation, true 3D voxel rendering, real xelector/hero entities,
performance fixes) was verified working live. Read `civ-vs-piece.md`
first (the Phase 1 decision record this doc builds on) and
`mc-speed-algos.md` (the perf investigation that makes the "bigger map,
more content" ambitions in this doc actually affordable). This doc
covers four things asked about in one sitting: texture sheets, hero/
xelector possession (real avatar-creation precedent), a bigger/more
realistic world (biomes, water-planet island map), and NPCs + day/night.

---

## 1. Where Phase 1 actually left off

Confirmed working, live-tested, this session:
- Real chunk generation (`pc_generate_chunk.c`) — seeded, deterministic,
  16×16×32, real grass/dirt/stone/air layering.
- True multi-layer voxel rendering in board-viewer (both 2D and 3D),
  with a real, verified-gap-free empty-space-skip optimization and
  OpenMP parallelization — ~35-40x faster than the naive version, zero
  visual regression.
- A real, separate xelector entity (`pieces/xelector_01/state.txt`),
  matching mutaclysm/fuzz-op's actual precedent — free camera-cursor
  movement (`z`/`x` for Z, arrows for X/Y), a real `possessed_id` field
  (currently `hero_01`, unused — nothing reads/enforces it yet).
- A real hero entity (`pieces/hero_01/state.txt`) — universal schema
  per `PORTABLE_ENTITY_ARCHITECTURE.md`, but genuinely minimal: `hp`,
  position, `owner_id`. No inventory, no real player identity, no DNA/
  appearance — it's a placeholder, not a real character yet.

Explicitly **not** built yet (from civ-vs-piece.md's own build order,
still true): the main-screen tick/position readout rewrite, real
`keybinds.txt` publication + place/break ops, Inspect/Inventory/Exit
input scheme, multiple biomes, and everything in this doc.

---

## 2. Texture sheets — real answer: not yet, and here's why

Direct question: "should we create minecraft like texture sheets to
read for the elements."

**Real precedent already in this house, confirmed by reading the actual
code (not assumed):** `chtpm_rgb_render.c`'s own **GENERIC ON-DEMAND
EMOJI GENERATION** system (`ensure_emoji_asset_generated()`, calling
`ops/+x/emoji_gen_atlas.+x`) already does real, working texture
generation — given a real UTF-8 emoji codepoint, it rasterizes that
emoji (via FreeType + `NotoColorEmoji.ttf`) into a real per-voxel
`voxels_16.csv` texture asset, on demand, the first time it's needed,
then reuses the generated file forever after (real caching, not
regenerated every call). This is the SAME mechanism that already makes
`terrain_legend.txt`'s own `asset_hex` column work — every glyph
(`grass`, `dirt`, `stone`, ...) already has a real emoji codepoint
(`1F33F`, `1F7EB`, `1FAA8`) wired to it, and the ON-DEMAND system
generates the actual texture the first time board-viewer's 2D path
needs it.

**What's real and what's a genuine gap, checked directly this session:**
- The 2D render path (`chtpm_rgb_render.c` itself) calls the on-demand
  generator — confirmed working (this is why 2D terrain looked correct
  even before any texture files existed on disk for piececraft-hq's
  own glyphs).
- The 3D raymarcher (`bv_render_3d.c`) does **NOT** call the generator
  itself — it only reads whatever `voxels_16.csv` files already exist
  via `get_voxel8_cached()`, falling back to a flat legend color if the
  file is missing/empty. This is why several of piececraft-hq's own
  glyphs (dirt, grass, sand, wood, leaves, ore) rendered as flat colors
  in 3D mode during this session's own testing, not real emoji-derived
  textures, even though the SAME glyphs would show real texture in 2D.

**Recommendation: don't build a hand-authored Minecraft-style texture
sheet.** The real, working, already-proven system in this house is
emoji-codepoint-driven on-demand generation, not static texture atlases
— building a separate sprite-sheet pipeline would duplicate a system
that already exists and already works for the 2D path. The real,
scoped fix is smaller and more valuable: **wire `bv_render_3d.c`'s own
texture lookup to call the SAME on-demand generator** (`emoji_gen_atlas.
+x`) on a cache miss, instead of silently falling back to a flat color
— this makes 3D mode automatically pick up real textures for EVERY
glyph any terrain_legend.txt ever declares, present or future, with zero
new asset-authoring work, matching the 2D path's own real behavior. Flag
this as a real, scoped Phase 2 task (see §6), not "build texture
sheets."

---

## 3. Hero = real avatar, xelector possesses it (real precedent, read directly)

Direct instruction: use `0.user-pal👤️/01.avatar-creation👤️`'s own
real player-creation format for the hero, with xelector possessing it
to unlock real "player abilities."

**Real, existing, working precedent — read directly from
`ops/generate_clone.c`, not guessed:**

```
uuid=<uuid>
name=<name>
type=avatar
age=18
gender=male
skin_index=1
skin_emoji=👨🏻
species_emoji=👨🏻
hair_color=brown
shirt_color=blue
pants_color=black
height=170
weight=70
energy=100
asleep=0
hunger=100
owner_user_id=<uid>
owner_user_uuid=<uuid>
created_at=<timestamp>
grid_x=2
grid_y=2
z=0
```

This is a REAL, already-built "Clone factory / avatar DNA editor" -
Faucet (tokens) → Store (free starter/buy clone) → Avatars (list,
customize DNA: name/age/gender/skin/hair/shirt/pants/height/weight).
Storage is real too: per-user `xyzfs/users/<user_uuid>/home/avatars/
<avatar_uuid>/` under login context (`00.login-signup/current_login.
txt`), with a local desktop-window mirror at `pieces/world_01/
map_lobby/<avatar_uuid>/state.txt`.

### 3a. Real integration shape (matching precedent, not inventing one)

- `hero_01`'s own real identity should come FROM a real avatar-creation
  record, not be piececraft-hq's own separately-invented player
  schema. Concretely: `pc_generate_chunk.c` (or a new, real
  `pc_spawn_hero` op) reads the CURRENTLY LOGGED-IN user's own active
  avatar (`current_login.txt` → `active_avatar_uuid`/`active_avatar_
  path`, the SAME fields `generate_clone.c` already writes), and seeds
  `hero_01/state.txt` from THAT real avatar record - `name`, `skin_
  emoji`, `hp` (derived from `energy`, or a new field, your call),
  `hunger` (avatar-creation already tracks this - reuse it directly,
  don't invent piececraft-hq's own separate hunger field), real
  appearance fields for future rendering (`hair_color`/`shirt_color`/
  `pants_color`).
- No login yet / no avatar created yet: graceful default, matching this
  house's own "login can be optional, use 'default'" convention (see
  board-viewer's own `ledger_append.c` for the exact real precedent of
  this same fallback shape) - piececraft-hq gets a real, generic
  placeholder hero (today's current shape) rather than failing.
- **Possession, matching mutaclysm's own real `possessed_id` mechanic**
  (confirmed via direct code research earlier this session, cited in
  `PORTABLE_ENTITY_ARCHITECTURE.md` §1): while `xelector_01`'s own
  `possessed_id` field equals `hero_01`, real player-ability inputs
  (jump/mine/build — the `g`/`h` keys decided in civ-vs-piece.md §2/
  §6a) should apply TO `hero_01`, subject to hero's own real
  constraints (gravity, collision — none of which exist yet). When
  `possessed_id` is `none` (a real, already-defined state, matching
  mutaclysm's own convention), the xelector stays a free, unconstrained
  camera cursor exactly like it is today - jump/mine/build are real
  no-ops in that state, since there's no possessed entity to apply them
  to.
- **Not built yet, and genuinely separate future work, not blocking
  this**: the actual possession-driven constraint system itself
  (reading `possessed_id`, enforcing gravity/collision only on the
  possessed entity) - `civ-vs-piece.md` §6b already flagged this as
  real Phase 2+ scope when the field was first added. This doc doesn't
  change that scoping, just gives the possessed entity a REAL identity
  to draw from once that system gets built.

---

## 4. Bigger, more realistic world — biomes, water-planet island map

Direct ask: "start doing more minecraft realistic space, such as
biomes, bigger map (island on water planet), etc, since we are running
much faster."

**Real enabling fact, not assumed**: `mc-speed-algos.md`'s own measured
numbers (~0.03s/frame after the perf fix, down from ~1.3s) genuinely
make a bigger, more varied world affordable to render - the empty-
space-skip optimization's own cost scales with the number of DISTINCT
columns visible, not raw voxel count, so a wider view distance or a
taller build range doesn't reintroduce the original slowdown the way it
would have before that fix landed.

### 4a. Biomes (design §6, real scope already sketched, never built)

`PIECECRAFT_XYZ_DESIGN.md` §6 already named a real starting biome set
(forest/plains/desert/mountains/ocean, confirmed answered "yes fine" in
that doc's own §10 Q5) and the real mechanism (a second, larger-scale
noise/hash pass over CHUNK coordinates, separate from the per-column
height hash `pc_generate_chunk.c` already has, selecting which glyph-
frequency table that chunk rolls against). Today's `pc_generate_chunk.c`
only implements ONE biome (plains-shaped: grass/dirt/stone, no real
frequency table at all, no sand/water/ore placement). Real next step:
a `pc_generate_chunk` extension that (1) hashes chunk coordinates into a
biome id, (2) uses biome-specific per-column surface rules (desert:
sand-topped, no grass; ocean: real water-filled columns, not just a
sunken-height plains column; mountains: taller surface variation range)
before generating each chunk's own Z-layer files.

### 4b. Bigger map / water-planet island shape (real scoping questions, not decided)

"Island on a water planet" is a real, specific world-shape idea - genuinely
different from "many similar land chunks extending in every direction."
This needs real decisions before building, not guessed defaults:
- Is the water an OCEAN BIOME chunk type (per §4a, chunks far from
  center roll ocean), or a hard WORLD-SHAPE rule (a real distance-from-
  origin falloff that forces ocean past some radius, guaranteeing an
  island shape regardless of biome-hash luck)? These are genuinely
  different mechanisms - biome-hash alone could accidentally tile
  multiple disconnected islands or an all-land world; a real falloff
  rule guarantees the water-planet shape you're describing.
- How many chunks make up "the map" - is this still Phase 1's single
  always-active chunk (§11: "no compression yet"), or does a real
  island shape require MULTIPLE simultaneously-relevant chunks (i.e.
  does Phase 3's compression/decompression system need to start now
  instead of staying deferred)? A real island needs real chunk
  boundaries to walk across, which is a genuinely bigger undertaking
  than one chunk's own biome variety.

**Decision locked 2026-08-03**: hard, deterministic distance-from-origin
falloff rule (not biome-hash luck) - guarantees the island shape every
time, regardless of seed, and is genuinely simpler to reason about/debug
than hoping enough ocean-biome chunks roll adjacent to each other.
Direct instruction: "id like to guarantee the island shape for now. we
can change that later bt i think it keeps things managable" - staying
within Phase 1's existing SINGLE-CHUNK scope for this pass (the island
falloff applies within one chunk's own 16x16 columns, water toward the
chunk's own edges) - NOT pulling Phase 3 multi-chunk compression forward
yet. A real multi-chunk island (walkable coastline extending across
chunk boundaries) is explicitly deferred, matching "we can change that
later" - this is the deliberately smaller, manageable first version.

---

## 5. NPCs + day/night cycle (real precedent already named, not built)

- **NPCs**: `PIECECRAFT_XYZ_DESIGN.md` §9 already names the real shape -
  mobs as entities under the active chunk's own directory (`pieces/
  world_01/chunks/chunk_X_Y/mobs/<id>/state.txt`, same universal
  schema hero/xelector already use), `pc_mob_tick` walking active-chunk
  mobs once per world tick, simple per-profession behavior. §9a further
  names REAL, already-working precedent to reuse for the actual AI
  logic - `014.wsr-pal…/ops/corp_decide.c`'s own `decision_mode` tier
  chassis (0=preset/1=weighted/2=rl-stub/3=llm/4=human), already ported
  once for `my-chara-txt`'s own pets. Real, scoped, not invented -
  genuinely buildable once `world_01/state.txt`'s own `tick` counter is
  actually being incremented by real player actions (design §5 - not
  fully wired yet, `END_TURN` still increments `config.txt`'s own
  `turn` field today, a leftover from the civ-txt clone, not
  `world_01/state.txt`'s real `tick`).
- **Day/night cycle**: not directly named anywhere in the existing
  design docs yet - genuinely new scope. Simplest real mechanism
  matching this house's own tick-based convention (design §5): a
  `world_01/state.txt` field (e.g. `time_of_day`, 0-23 or a real
  fraction) advanced by `pc_mob_tick`-equivalent logic alongside the
  real tick counter, read by board-viewer's own render path to tint
  the sky/lighting color in `terrain_legend.txt`-adjacent data (or a
  new small lighting-multiplier table) - a real, scoped feature, not
  yet designed in detail. Flagged as real future work, not decided
  today.

---

## 6. Recommended build order (this doc's own real proposal)

Given everything above, four independent-ish work streams now exist
(texture generation wiring, hero/avatar integration, biome/world-shape,
NPCs+day-night) - **not proposing to build all of them in one pass**.
Suggested real sequence, but this is a recommendation to confirm with
you, not a decision made unilaterally:

1. **Main-screen tick/position rewrite + real `keybinds.txt` publication**
   (civ-vs-piece.md's own still-open build-order items #4/#5) - real
   prerequisite for almost everything else here, since NPCs/day-night
   both need the real `tick` counter actually incrementing, and hero
   possession needs real jump/mine/build key delivery working end-to-end,
   neither of which exist yet.
2. **3D texture-generation wiring** (§2) - small, scoped, immediately
   improves what you already see in-game with no new content design
   needed - every existing glyph gets real emoji textures in 3D for free.
3. **Hero = real avatar** (§3) - real identity/appearance data flowing
   into the game, sets up (but doesn't yet enforce) the possession
   system's real data dependency.
4. **Biomes** (§4a) - the smaller, more scoped half of the "bigger
   world" ask; buildable within Phase 1's existing single-chunk scope,
   no compression-system dependency.
5. **Real decision point**: island/water-planet world SHAPE (§4b) and
   multi-chunk support (pulls Phase 3 compression earlier) - needs a
   real conversation before starting, flagged, not scheduled yet.
6. **NPCs + day/night** (§5) - depends on #1's real tick counter,
   otherwise independently buildable once that lands.

Nothing in this doc has been built yet - this is the plan, per direct
instruction, before any of it gets coded.
