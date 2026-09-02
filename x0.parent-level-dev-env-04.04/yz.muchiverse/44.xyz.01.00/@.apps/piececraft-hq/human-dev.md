# 🧑‍💻 human-dev.md — piececraft-hq Developer Walkthrough

Welcome! This doc exists so **you** (a human, not an AI) can open the
code, find the thing you want to change, change it, rebuild, and see
it work — without needing to re-derive the whole architecture from
scratch every time. It's written from real, lived bug-fixing sessions
on this exact codebase, not theory.

---

## 📖 Table of Contents

1. [🗺️ The Big Picture](#1-️-the-big-picture)
2. [📁 Where Everything Lives](#2--where-everything-lives)
3. [🔨 Building & Running](#3--building--running)
4. [🎮 Common Tweaks (start here!)](#4--common-tweaks-start-here)
5. [🌳 Adding a New Phymoji Object (tree/rock/animal)](#5--adding-a-new-phymoji-object-treerockanimal)
6. [🧱 Adding a New Terrain Block](#6--adding-a-new-terrain-block)
7. [🗺️ World Generation (chunks, spawn points)](#7-️-world-generation-chunks-spawn-points)
8. [📷 Camera System](#8--camera-system)
9. [🐛 Debugging Like We Did (headless testing)](#9--debugging-like-we-did-headless-testing)
10. [⚠️ Gotchas That Bit Us](#10-️-gotchas-that-bit-us)
11. [🐔 Adding an NPC / Simple AI](#11--adding-an-npc--simple-ai)
12. [🤖 Is Our AI Loop Honest With the House Standard?](#12--is-our-ai-loop-honest-with-the-house-standard)
13. [📚 Glossary](#13--glossary)

---

## 1. 🗺️ The Big Picture

piececraft-hq is **two separate programs working together**:

```
┌─────────────────────────┐         ┌──────────────────────────────┐
│   piececraft-hq         │         │   &.widgits/board-viewer/     │
│   (the "game")            │ spawns  │   (a separate GL window       │
│   - menus                 │ ──────► │   widget that draws the       │
│   - world generation      │         │   actual 2D/3D map)           │
│   - hero/xelector state   │ ◄────── │   - reads piececraft's own    │
│                            │ inbox   │     pieces/ files directly    │
└─────────────────────────┘         └──────────────────────────────┘
```

🔑 **The key mental model**: piececraft-hq doesn't draw the map
itself. It generates real data files (`pieces/system/chunks/...`,
`pieces/hero_01/state.txt`, etc.) and launches board-viewer as a
*separate process* that reads those files and draws a window. When
you press an arrow key with the map window focused, board-viewer
handles it directly — piececraft-hq's own menu only handles its own
screen's buttons.

This means: **"why doesn't my map look right" bugs are almost always
in `&.widgits/board-viewer/ops/`, not in `@.apps/piececraft-hq/ops/`.**
🎯

---

## 2. 📁 Where Everything Lives

### piececraft-hq itself (`@.apps/piececraft-hq/`)

| Path | What it does |
|---|---|
| `ops/pc_generate_chunk.c` | 🌍 Builds a new world: terrain height, hero/xelector spawn position, tree placement |
| `ops/pc_menu_input.c` | 🖱️ Handles clicks on piececraft's OWN screen (setup screen, main menu, opening the board widget) |
| `ops/pc_compose_frame.c` | 🖼️ Draws piececraft's OWN screen (not the map — the menus/HUD around it) |
| `ops/pc_phymoji_gen.c` | 🎨 Turns an emoji (like 🌳) into a real 3D voxel model |
| `pieces/system/terrain_legend.txt` | 🎨 Data file: which glyph = which color/emoji/name for ground blocks |
| `pieces/system/keybinds.txt` | ⌨️ Data file: which raw key = which game verb (JUMP/MINE/BUILD) |
| `pieces/registry/phymoji_assets/<name>/voxels.csv` | 🧊 Generated 3D voxel data for one object (tree, hero, chicken) |
| `pieces/world_01/phymoji_entities.txt` | 📍 Where each tree/object is actually placed in the world |
| `scripts/build.sh` | 🔨 Compiles everything, copies shared binaries |

### The map widget (`&.widgits/board-viewer/`)

| Path | What it does |
|---|---|
| `ops/bv_render_3d.c` | 🕹️ **The big one.** The whole 3D raymarcher — camera, terrain rendering, phymoji rendering, everything you SEE in 3D |
| `ops/bv_compose_frame.c` | 📝 The 2D text/emoji-grid view + the status lines around both views |
| `ops/bv_menu_input.c` | ⌨️ Every keypress while the map window is focused: arrows, camera keys (0-4, wasd, qerty), possession (9/8) |
| `scripts/build.sh` | 🔨 Compiles board-viewer, copies `emoji_gen_atlas.+x`/`emoji_xtract.+x` from wsr-pal |

---

## 3. 🔨 Building & Running

```bash
# After changing anything in piececraft-hq's own ops/:
cd @.apps/piececraft-hq
bash scripts/build.sh

# After changing anything in board-viewer's own ops/ (camera, rendering, keys):
cd &.widgits/board-viewer
bash scripts/build.sh
```

🚨 **The #1 thing that will confuse you**: after rebuilding, you
**must fully quit and relaunch** the running game/widget. Some
processes (like `chtpm_rgb_render`) are **persistent daemons** that
keep running your OLD binary until fully restarted — a rebuild alone
does nothing visible until you relaunch. We hit this repeatedly in
development; if a fix "isn't working," restart everything before
assuming the fix is wrong. 🔁

---

## 4. 🎮 Common Tweaks (start here!)

### ⌨️ "I want to change what a key does"

Arrow keys, camera keys (`0`-`4`, `wasd`, `q`/`e`, `r`/`t`, `z`/`x`,
`9`, `8`) all live in **one file**:
`&.widgits/board-viewer/ops/bv_menu_input.c`, in `main()`. Search for
`if (key == '...')` blocks — each is self-contained and easy to copy
as a template for a new key.

```c
// Example: this is literally how arrow-left is currently mapped
if (key == ARROW_LEFT) dx = -1;
else if (key == ARROW_RIGHT) dx = 1;
```

Game-specific verbs (JUMP/MINE/BUILD) are NOT hardcoded here — they're
data-driven from `pieces/system/keybinds.txt`:

```
32=JUMP
103=MINE
104=BUILD
```

The left side is a **raw key code** (32 = spacebar), the right side is
the verb name sent to `ops/pc_menu_input.c`'s own handler. Want `j` to
mine instead of `g`? Just change the number on the left (ASCII table
or `xev`-derived) — no C code needed. ✏️

### 📷 "I want the camera to sit somewhere else / be higher / be closer"

All camera math lives in `build_camera()` in
`bv_render_3d.c`. Each `camera_mode` (1=first-person, 2=third-person,
3=free-roam, 4=bird's-eye) has its own few lines:

```c
if (camera_mode == 1) {
    cam.eye.x = anchor_x + 0.3 * sin(yaw);
    cam.eye.y = anchor_h + 1.5;   // 👈 bump this up to fly higher
    cam.eye.z = anchor_z + 0.3 * cos(yaw);
}
```

See [Chapter 8](#8--camera-system) for the full mental model before
touching this — there's a real trap here (see gotchas).

### 🎨 "I want to change a terrain color / add a new ground block"

Don't touch C code at all — see [Chapter 6](#6--adding-a-new-terrain-block).
It's a data file.

### 🐌 "The game feels slow"

First check: is it slow because of **phymoji objects** (trees, hero,
animals) on screen? Their rendering cost scales with **voxel count**,
which is controlled by two `#define`s at the top of
`pc_phymoji_gen.c`:

```c
#define TILE_N 16        // 🔍 the object's real X/Y detail (higher = more detail, MORE voxels)
#define DEFAULT_DEPTH 8   // 📏 how many layers deep the object is extruded
```

Max real voxel count ≈ `TILE_N × TILE_N × DEFAULT_DEPTH`. Lower
`TILE_N` for a real, direct speed win at a detail cost. After
changing, **regenerate every asset** (see Chapter 5) — old
`voxels.csv` files don't update themselves.

---

## 5. 🌳 Adding a New Phymoji Object (tree/rock/animal)

"Phymoji" = any object built by extruding a real emoji into a 3D voxel
model (as opposed to flat terrain blocks). Here's the full recipe:

### Step 1 — Generate the voxel asset
```bash
cd @.apps/piececraft-hq
./ops/+x/pc_phymoji_gen.+x "🐄" cow_small
# → writes pieces/registry/phymoji_assets/cow_small/voxels.csv
```

Any real Unicode emoji works — the pipeline rasterizes it with
FreeType, crops the transparent border, and extrudes it into a real
3D shape automatically. No manual voxel editing needed. ✨

### Step 2 — Place it in the world
Add a line to `pieces/world_01/phymoji_entities.txt`:
```
cow_small,6,9,17
```
Format: `entity_id,x,y,z` (world grid coordinates — `z` is the ground
height it stands on, usually `surface_height + 1`).

`pc_generate_chunk.c` is what writes this file today for the debug
map's own trees (search for `TREE_COUNT`/`tree_col`/`tree_row`) — copy
that pattern if you want new objects placed automatically during world
generation instead of by hand.

### Step 3 — Rebuild board-viewer and relaunch
That's it — `bv_render_3d.c`'s own `load_phymoji_world_entities()`
picks up any `entity_id` with a matching `voxels.csv` automatically. No
C code changes needed for a NEW object of an EXISTING kind (like a
second tree) — only for a genuinely new *category* of behavior.

---

## 6. 🧱 Adding a New Terrain Block

Terrain (flat ground blocks — dirt, stone, grass, water...) is fully
**data-driven**. Edit `pieces/system/terrain_legend.txt`:

```
# glyph|height|r|g|b|asset_hex|name
,|0.6|90|170|60|1F33F|grass
X|0.5|180|60|180|1F52E|crystal   ← add a new line like this
```

- `glyph` — a single character used in the chunk `.txt` files (avoid
  `#`, it collides with comment-line parsing)
- `height` — cosmetic only (legacy field, not load-bearing for the
  real voxel model)
- `r/g/b` — fallback flat color if the emoji texture can't load
- `asset_hex` — the emoji's Unicode codepoint in hex (e.g. `1F52E` =
  🔮) — this is what gets rendered as a real texture
- `name` — cosmetic label

Then use your new glyph `X` in `pc_generate_chunk.c`'s own terrain
loop (the `if (z > sh) glyph = '_';` chain) wherever you want it to
appear. No board-viewer rebuild needed — it reads this file fresh
every frame. 🔥

---

## 7. 🗺️ World Generation (chunks, spawn points)

All of this lives in `ops/pc_generate_chunk.c`, `main()`:

```
Usage: pc_generate_chunk.+x <seed> <chunk_x> <chunk_y> [flat]
```

- **Seeded mode** (no `flat` arg): real per-column height noise via
  `hash_coord()` — deterministic, same seed = same world every time.
- **`flat` mode**: the "debug map" — flat ground at `FLAT_SURFACE_Z`
  (currently 16), with 4 fixed trees at hardcoded `tree_col`/`tree_row`
  positions. Great for testing since nothing shifts between runs.

**Hero/xelector spawn**: written near the bottom of `main()`. Hero
spawns at `(8, 8, surface[8][8] + 1)` — the `+1` matters: it's the AIR
tile directly above the ground, not the ground block itself. If you
ever see the player "embedded in the floor" or "floating," this `+1`
math (or something reading the wrong one of the two) is almost always
why. 🕳️

---

## 8. 📷 Camera System

Board-viewer has **4 camera modes**, each with genuinely different
math in `build_camera()` (`bv_render_3d.c`):

| Mode | Name | Follows | Notes |
|---|---|---|---|
| 1 | 🚶 First-person | xelector/hero exactly | Eye height = `anchor_h + offset` |
| 2 | 🎥 Third-person | xelector/hero, offset back | Same `anchor_h` base |
| 3 | 🌐 Free-roam | Nothing (wasd/qe/rt controlled) | Height is `c`/`v` keys, NOT tied to player |
| 4 | 🐦 Bird's-eye | Nothing (absolute map coords) | Looks straight down by default |

**The trap we hit repeatedly**: `anchor_h` (the height reference for
modes 1/2) and `current_z` (which Z-layer the 2D view shows) are
**two different concepts that used to share one variable**. If you
change what `current_z` means (e.g. "ground level" vs "player's actual
air tile"), you WILL silently break first-person camera height unless
you check `anchor_h`'s own computation too. They are now intentionally
separate in the code — keep them that way. 🚫🔗

---

## 9. 🐛 Debugging Like We Did (headless testing)

You don't need to click through the actual GL window to test a
rendering change. Every op reads its state from plain files and can be
run directly:

```bash
mkdir -p /tmp/mytest/pieces/system /tmp/mytest/pieces/display

cat > /tmp/mytest/pieces/system/bv_state.txt << EOF
focused_project_id=piececraft-hq
focused_project_root=/absolute/path/to/@.apps/piececraft-hq
emoji_mode=1
render_mode=1
camera_mode=4
EOF

export PRISC_PROJECT_ROOT=/tmp/mytest
./ops/+x/bv_render_3d.+x
# → writes /tmp/mytest/pieces/display/rgb_frame_3d_overlay.raw (raw RGBA pixels)
```

Then inspect the pixels with Python:
```python
data = open('rgb_frame_3d_overlay.raw', 'rb').read()  # RGBA, 4 bytes/pixel
from collections import Counter
c = Counter(data[i:i+3] for i in range(0, len(data), 4))
print(c.most_common(10))  # if it's ALL one color, something's wrong
```

This is exactly how every rendering bug in this doc's own history got
found and verified — much faster than relaunching the whole app every
time. ⚡

---

## 10. ⚠️ Gotchas That Bit Us

A running list of real, specific mistakes made building this — read
before you repeat them. 😅

- **🧟 Stale daemons**: `chtpm_rgb_render` is a persistent process.
  Rebuilding its binary does nothing until the app is FULLY restarted.
- **📦 Stale cached assets**: on-demand emoji generation caches to
  `pieces/registry/emoji_assets/<hex>/voxels_16.csv` by checking if the
  file *exists*, not if it's *valid*. A failed generation (e.g. a
  missing helper binary) leaves a real 0-byte file that's cached
  FOREVER as "done." If a specific tile is stuck grey/broken, check the
  file size of its cache entry before assuming it's a code bug.
- **🪓 Two different `chtpm_rgb_render` copies**: piececraft-hq's own
  `system/chtpm_rgb_render.c` (compiled from local source) and
  board-viewer's own `system/chtpm_rgb_render` (a copied BINARY from
  wsr-pal) are genuinely different files. Fixing one does not fix the
  other.
- **🎯 anchor_h vs current_z**: see Chapter 8. Don't let display-only
  concerns (which Z-slice to show in 2D) and physical concerns (where
  the camera eye actually sits) share one variable again.
- **📐 TILE_N ≠ cube**: raising `TILE_N` in `pc_phymoji_gen.c` only
  changes the X/Y footprint. `DEFAULT_DEPTH` is separate. A "16×16×16"
  expectation needs BOTH numbers matched on purpose.
- **🧮 Per-pixel loops need a coarse test first**: any per-pixel hit
  test that loops over a real object's full voxel/column list should
  ALWAYS be gated behind one cheap bounding-box test first. Skipping
  this is a real, easy-to-miss performance cliff (it's fine at small
  scale, then falls over the moment an object gets more detailed).
- **↔️ "Swap the arrows" is rarely the real fix**: an apparent
  left/right inversion in 3D is usually a camera-angle/yaw effect, not
  a genuinely wrong `dx` sign — flipping the raw input can "fix" one
  camera mode while breaking a different, unrelated view (like the flat
  2D grid) that never had the problem to begin with.

---

## 11. 🐔 Adding an NPC / Simple AI

The chicken is the real, working template for "an entity that moves on
its own." Full recipe:

### Step 1 — Generate + place it (same as Chapter 5)
```bash
./ops/+x/pc_phymoji_gen.+x "🐄" cow
```
Then write it to a **separate** file from static decoration —
`pieces/world_01/animals.txt` (NOT `phymoji_entities.txt`, which is
for things that never move, like trees):
```
cow,10,8,17
```
`pc_generate_chunk.c` already does this for the chicken — copy that
pattern (search `animals_path`).

### Step 2 — Give it a tick function
In `ops/pc_menu_input.c`, `tick_animals()` is the real, working
example: reads `animals.txt`, picks a random `-1/0/+1` step per axis
per entity, clamps to map bounds, rewrites the file, and logs each
real move to `data/master_ledger.txt` with the entity's own real
`entity_id` as the actor (not `"player"`).

### Step 3 — Wire it to the tick
`tick_animals()` is called right after every `advance_tick()` call
site (MOVE/JUMP/MINE/BUILD/END_TURN) — the world only advances when the
player acts, and NPCs move exactly once per that same real step. No
new hook needed for a second/third animal — `tick_animals()` already
loops every line in `animals.txt`.

### Step 4 — Rendering
Nothing to do — `bv_render_3d.c`'s own `load_phymoji_world_entities()`
already reads BOTH `phymoji_entities.txt` and `animals.txt` generically.
Want a smarter shape than "random 1-unit cube"? See
`load_phymoji_world_entities`'s own world-size branch (search for
`"chicken"` in `bv_render_3d.c`) — add a similar `if (strcmp(...))`
for your new entity's own real proportions.

⚠️ **Known gap**: this only makes the NPC visible in **3D**. The 2D
emoji view has no entity-overlay system at all yet (see Chapter 10) —
your new NPC won't show up there until that's built.

---

## 12. 🤖 Is Our AI Loop Honest With the House Standard?

Direct question asked 2026-08-04: does the chicken's random-walk AI
actually follow the real ledger-driven AI precedent documented in
`101.lpns+map+4/ledger-4-agent-trace.md`? Read that doc in full before
touching AI code again — here's the honest, side-by-side answer. ⚖️

### ✅ What genuinely matches

- **"NPC moves are random — no pathfinding or strategy"** — that
  reference doc's own §11 item 7 states this as the real, documented
  limitation of its own NPC logic. The chicken's `tick_animals()`
  (`pc_menu_input.c`) does exactly this: `rand() % 3 - 1` per axis,
  clamped to map bounds, nothing smarter. Not a simplification we
  invented — a real, matching precedent.
- **The ledger is the real source of truth for movement** — the
  reference doc's §6 states "all positions are reconstructed by
  replaying `data/master_ledger.txt`... this means the ledger is the
  single source of truth." Our chicken's actual position lives in
  `pieces/world_01/animals.txt` (not reconstructed from the ledger),
  but every real step is still faithfully logged there — see the gap
  below for the honest distinction.

### ⚠️ Where it genuinely diverges (real gaps, not invented ones)

- **Ledger file format is different.** The reference standard's own
  header: `timestamp|epoch|player|turn|action_data|action_type` (6
  fields). Piececraft-xyz's own `master_ledger.txt` (a PRE-EXISTING
  convention here, not something this pass invented) is
  `timestamp|turn|actor|action_type|details` (5 fields, no `epoch` at
  all, and `action_type`/`details` swapped relative to the reference's
  own `action_data`/`action_type` order). A tool built to parse the
  reference standard's own ledger format would NOT parse
  piececraft-hq's file correctly without changes.
- **Position source of truth is different.** The reference standard
  reconstructs a player's CURRENT position by replaying every `move`
  action in the ledger from a config-file starting point — the ledger
  itself IS the state. Piececraft-xyz's chicken instead keeps its real
  current position directly in `pieces/world_01/animals.txt` (rewritten
  each tick) and ALSO logs to the ledger — the ledger here is an audit
  trail, not the actual state store. Real, working, but a genuinely
  different architecture, not a port of the reference's own replay
  model.
- **No separate "AI turn" phase.** The reference standard runs NPC
  moves from a persistent `game_manager.c`'s own `npc_auto_play()` loop
  — a real, distinct phase that runs after the human's action and
  before returning control (`§8`). Piececraft-xyz's chicken instead
  ticks INLINE, as a direct side effect of the player's own
  `advance_tick()` call (`tick_animals()` called right after) — no
  separate process, no distinct phase, no "all computer players go
  before the human moves again" loop (moot right now since there's
  only ever one animal, but would matter with several).

### 🎯 Honest verdict

The chicken's AI is **faithful to the reference standard's own real
BEHAVIORAL spec** (random-only movement, ledger-logged) but **not a
byte-for-byte architectural port** — the ledger file format and the
"where does current position actually live" model both differ in real,
documented ways above. If a future feature needs to READ piececraft's
ledger with tooling built against the `101.lpns+map+4` format
specifically, it will need a real adapter, not a direct reuse.

---

## 13. 📚 Glossary

| Term | Meaning |
|---|---|
| **xelector** | The free-moving cursor/camera-anchor entity. Can "possess" the hero. |
| **hero** | The actual player-character entity (position, HP, etc). |
| **possession** | Xelector following/controlling the hero (`9` key toggles it). |
| **phymoji** | An object (tree, animal, hero) built by extruding a real emoji into 3D voxels — as opposed to a flat terrain block. |
| **chunk** | A 16×16 column area of the world, with one file per Z-layer. |
| **current_z** | Which Z-layer the 2D view currently displays. |
| **anchor_h** | The camera's own real eye-height reference (first/third-person). |
| **coarse box test** | A cheap single bounding-box check done BEFORE an expensive per-voxel scan, to skip work for pixels nowhere near the real object. |

---

🎉 That's the whole map. When in doubt: find the file in the tables
above, search for the exact string you're looking at on screen (a
color, a key, a message), and follow it from there. Good luck, and
have fun building! 🚀
