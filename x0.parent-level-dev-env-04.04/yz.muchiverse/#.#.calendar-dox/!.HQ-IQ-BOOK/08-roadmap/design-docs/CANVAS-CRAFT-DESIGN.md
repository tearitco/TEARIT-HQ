# Canvas-Craft — the house crafting system (design)

**Status:** design · **Date:** 2026-09-06 · **Nothing built yet**
**Owner request:** "this will be a very big aspect of the gameplay …
let's write a design document for this first."

Replaces the current `elements` palette category ("Chemicals+Compounds")
with a real 3-panel crafting bench modelled on Satisfactory's crafting
UI (`#.ref/menu/palletes/satisfactory-crafting.png`). Seeded with the
quark → subatomic → element → compound → bio recipe tree that already
exists as data (`#.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt`)
and has never had a renderer. The 3D-in-2D atom inspector comes from
`#.ref/menu/palletes/chem-viz.txt`.

---

## 0. Why now / what's broken

The `elements` picker today (`&.widgits/palettes/palettes-elements.xhtpm`
+ `palettes_manager.+x` + `palettes_projector.+x`) is a `<repeat>` of
`<item class="swatch">` tiles and it's visibly broken:

| symptom | cause |
|---|---|
| "not showing any render" | `sprite=""` for every row (sprites were never generated) → the swatch `<item>` has nothing to draw. |
| "no labels" | the xhtpm binds `label="${t.glyph}"` — the **emoji only**. The manager publishes the full `"🧪 Acetic Acid (CH₃COOH)"` label but nothing renders it. |
| "columns aren't spaced far enough apart" | `.pal-tile{width:48px}` in a 6-wide swatch grid; the `.pal-wide{width:240px}` patch doesn't take because the swatch-grid layout path ignores `min-width`/`width:auto` (see the CSS comment in `palettes-elements.css`). |

Canvas-Craft does not use the swatch grid at all, so all three go away.
Old `palettes-elements.*` stays as rollback (same convention as every
other palettes port).

---

## 1. The data

### 1.1 Recipe registry — `elements]new=RECIPEZ+]z2🏆.txt`

Whitespace-delimited, one recipe per line, **line order matters** (the
parent fields are 0-based indices into this same line list):

```
<Name>  <protons>  <neutrons>  <electrons>  <parentA_idx>  <parentB_idx>
```

- All five numbers `-1` → a **primitive** (no recipe): `Up_quark`,
  `Down_quark`, `Electron`. These are mined / spawned / given, not
  crafted.
- `parentA_idx` / `parentB_idx` are **0-based** indices into the file.
  Verify: `Hydrogen 1 0 1 5 2` → idx 5 = line 6 = `Proton`, idx 2 =
  line 3 = `Electron`. Hydrogen = Proton + Electron. ✓
  `Water 10 8 10 7 14` → idx 7 = `Hydrogen`, idx 14 = `Oxygen`. ✓
- The `protons/neutrons/electrons` triple is the **total** for the
  finished item (self-consistent up the tree: 2×H(1,0,1) + O(8,8,8) =
  Water(10,8,10)). It is what the quantity resolver (§3) uses.
- `Pair_up_quarks -1 -1 -1 0 0` / `Pair_down_quarks -1 -1 -1 1 1` are
  convenience groupings (2× a quark) used as the immediate parents of
  `Proton` / `Neutron`. Physics note: real proton = uud, neutron = udd;
  the data aggregates loosely above the quark tier. **Recommendation:**
  make the quark→nucleon tier physically exact (it's a teaching
  moment — proton = 2×Up + 1×Down) and keep the loose aggregation
  above it (an element just needs "N protons, M neutrons").

Coverage in the file: 3 quark primitives, 2 pairs, Proton, Neutron, all
**118 elements** Hydrogen→Oganesson, then ~120 compounds/materials/bio
rows (H2, Water, Methane, Benzene, Aspirin, DNA, Virus, Cell, Tissue,
Organ, Steel, Diamond, Graphene, Kevlar, Vaccine, StemCell, …).

### 1.2 Display metadata — `chemistry_tiles_expanded🏆.csv`

14 columns: `emoji, compound_name, formula, category, hint, color_hex,
state, melting_point, boiling_point, density, toxicity, reactivity,
icon_tile, animation_frames`. ~12 rows fully filled, the rest have
emoji+formula+hint only. Joined to the recipe registry by name for the
recipe card (formula text, colour swatch, tooltip: state / mp / bp /
toxicity / reactivity).

### 1.3 Proposed consolidation — `canvascraft_recipes.pdl`

The terse `-1 -1 -1` format is hard to extend. **Recommendation:**
generate a real PDL once from the two files above and make it the source
of truth going forward:

```
RECIPE | id | name | symbol | Z | protons | neutrons | electrons |
         parentA | parentB | parentC | yield | tier | emoji | color | state | toxicity | reactivity
```

- `parentC` lets Electron be an explicit ingredient so **ions** are
  craftable later (Gold vs Au⁺), and lets 3-input compounds exist.
- `tier` = one of `quark · subatomic · element · compound · material ·
  organic · bio · mega` — drives the left-panel grouping.
- `id` = stable slug (`gold`, `up_quark`, `water`), not a line index.

Phase 1 can parse the legacy `.txt` directly; the PDL migration is a
clean follow-up, not a blocker.

---

## 2. The window — Canvas-Craft

Renamed from the vague "palette" framing. `pallets.pdl`:
`elements` row → `LABEL "Canvas-Craft"`, `PICKER canvascraft`. (Keeping
the KEY `elements` avoids touching `palettes_menu.sh` dispatch; a rename
to `canvas-craft` is an optional tidy — open question §7.)

Static `&.widgits/canvas-craft/canvas-craft.xhtpm` + `.css`, rendered by
the shared `khtpm_core_render.+x` (CENTROID_GOLD_STD — no new per-app C
in the renderer). All generic tags.

### 2.1 Three vertical panels

```
┌── LEFT: RECIPES ──────┬── MIDDLE: BENCH ─────────────┬── RIGHT: INVENTORY ──┐
│ [search recipe…]      │  ╔═ Gold ══════════════╗     │ Relevant Items       │
│ ▸ Quarks              │  ║ output ⬛ Gold  ×1   ║     │  ⬛79 ⬛118            │
│ ▸ Subatomic           │  ╚═════════════════════╝     │                      │
│ ▾ Elements   ← POE tab│   0 / 79   Proton  ▓▓░░░     │ All Items            │
│    [>] Hydrogen       │   0 / 118  Neutron ▓░░░░     │  ⬛ ⬛ ⬛ ⬛ ⬛ ⬛      │
│    [ ] Helium         │  ┌ crafting bench ─────────┐ │  ⬛ ⬛ ⬛ ⬛ ⬛ ⬛      │
│    [ ] …  Gold        │  │ ·  ·  ·   ·  ·  ·        │ │  ⬛ ⬛ …               │
│ ▸ Compounds           │  └─────────────────────────┘ │                      │
│ ▸ Materials · Bio …   │        [  CRAFT  ]           │  [Sort]      [🗑]     │
└───────────────────────┴─────────────────────────────┴──────────────────────┘
```

**LEFT — `<sidebar>`**, two tabs:
- **Recipes**: a search box (`<cli_io>`), then a `<scrolllist>` of
  recipes grouped by `tier` with collapsible headers. Real thumb +
  wheel + PageUp/Down (the palettes VIEW SPECS scroll contract).
- **POE**: the 118 elements in true periodic-table geometry — period
  rows, group columns, the lanthanide/actinide strip pulled out below.
  Each cell = symbol + Z, background = Drude colour (§5). Click → selects
  that element in the middle panel. Non-nav geometry cells; only the
  focused row/cell gets a nav index.

**MIDDLE — `<panel>`**: the selected recipe.
- Output card: emoji/sprite + name + `yield` ("× 16" like Satisfactory).
- **Ingredient rows** at *highest-common-denominator* form (§3): one row
  per **direct** parent, `have / need  <name>` + a fill bar. Crafting
  Gold shows `0/79 Proton`, `0/118 Neutron` — **not** the expanded
  quark tree. Crafting Water shows `0/2 Hydrogen`, `0/1 Oxygen`.
- **Crafting bench**: a small fixed grid of slots, itself a nav target.
  Items you add from inventory land here; the ingredient rows fill as
  matching items arrive.
- **CRAFT** button — enabled only when every `have ≥ need`. Fires
  `CRAFT`.
- Optional **Inspect** toggle → the 3D-in-2D atom view (§4) in a
  `<canvas>` in this same panel, or a spawned inspector window.

**RIGHT — `<panel>`**: inventory.
- **Relevant Items**: the subset you hold that the selected recipe
  needs (Satisfactory's top strip).
- **All Items**: full inventory grid, `Sort`, trash.

**Layout-engine note.** `layout_sidebar_panel()` today lays out
`<sidebar>` + exactly **one** `<panel>`. Two clean options:
- **(a)** extend it to `<sidebar>` + N `<panel>` in a flex row — the
  proper fix, benefits every future 3-pane window.
- **(b)** keep one `<panel>` whose child is a flex `<row>` of two
  `<scrolllist>`s (middle + right). `css_layout_pass()` already recurses
  into nested flex rows, so **(b) needs zero engine change** — use it
  for Phase 1, do (a) when a second 3-pane window wants it.

### 2.2 Interaction (direct instruction)

| action | result |
|---|---|
| click a recipe (LEFT) / a POE cell | `SELECT_RECIPE:<id>` — middle panel repopulates |
| **double-click** an item in inventory (RIGHT) | selects that inventory slot |
| **double-click** the crafting-bench nav (MIDDLE) | `BENCH_ADD:<slot>` — moves 1 of the selected item onto the bench; ingredient rows refill |
| click a bench item, press **Backspace** | `BENCH_REMOVE:<slot>` — item returns to inventory |
| **CRAFT** (all rows satisfied) | consume the bench inputs, add `yield ×` output to inventory, clear the bench |
| Inspect | `INSPECT:<id>` — opens the atom view (§4) |

Keyboard-only path (for the terminal mirror / headless): arrows +
digits select; Enter on a bench slot = add focused inventory item;
Backspace = remove; a nav-numbered CRAFT row.

---

## 3. Quantity resolver — "highest common denominator"

For selected recipe **X** with direct parents **A**, **B** and totals
`(Xp, Xn, Xe)`:

1. **Element case** (parents are `Proton` & `Neutron`): need `Xp`
   Protons + `Xn` Neutrons. Electrons: `Xe` free `Electron`s (or an
   explicit `parentC` once the PDL migration lands — ions).
2. **General 2-parent case**: find non-negative integers `(a, b)`
   minimising `a + b` s.t.
   `a·Ap + b·Bp = Xp`, `a·An + b·Bn = Xn`, `a·Ae + b·Be = Xe`.
   Over-determined but consistent by construction (the file's totals
   add up). Water: `a·1 + b·8 = 10`, `a·0 + b·8 = 8` → `b=1, a=2`.
3. **Fallback** (no exact solution — dirty data): greedy on the
   limiting axis, `a = ceil(Xp / max(Ap,1))`, flag the row.

Displayed as `have / need  <parent name>`. The player does the
recursion by hand — if they lack Hydrogen they open Hydrogen's recipe
(1 Proton + 1 Electron) and craft that first. Never auto-expand to
primitives.

`canvascraft_manager.c` computes this once per `SELECT_RECIPE` and
publishes the rows; it recomputes `have` on every `BENCH_ADD/REMOVE`.

---

## 4. The 3D-in-2D atom inspector (`chem-viz.txt`)

An element rendered as a nested voxel structure with LOD-zoom depth,
shown 2D (isometric/orthographic projection into a `<canvas>`), with a
zoom slider that controls which depth layer is "unpacked":

| depth_layer | shows | detail |
|---|---|---|
| **0 — Element macro** | one voxel = the whole atom | colour from the Drude model (§5) |
| **1 — Subatomic** | Protons + Neutrons clustered in a central nucleus sphere; Electrons on outer shell coordinates | proton = neon red, neutron = grey, electron = electric blue |
| **2 — Quark** | inside each nucleon: proton = 2 Up + 1 Down, neutron = 1 Up + 2 Down, offset within that nucleon's sub-space | up-quark = magenta, down-quark = cyan |

**Voxel data**: a generator (C op or Python helper, run once per
element) emits `#.ref/menu/palletes/voxels/<symbol>.csv` with the
`chem-viz.txt` schema:
`x,y,z,depth_layer,element,symbol,sub_x,sub_y,sub_z,particle_type,quark_type,color`.
Nucleon count and valence come straight from the recipe's `protons /
neutrons / electrons`. The renderer just projects + depth-sorts the
rows for the active layer.

**Terminal-mirror / headless**: the inspector degrades to ASCII — a
labelled box for layer 0, a char grid (`p`/`n` nucleus, `·` electrons)
for layer 1, `u`/`d` for layer 2. Same data, no `<canvas>`.

This layer is **Phase 4** — the crafting bench works without it.

---

## 5. Drude-model element colour

Not hardcoded. A pure function `element_color(Z, protons, neutrons,
valence) -> #rrggbb`:

- atomic volume ≈ f(protons + neutrons); pseudo-electron density ≈
  f(valence); simplified Drude plasma frequency `ω_p ∝ √n` → visible-light
  cutoff → base hue.
- relativistic heavy-proton shift for Au/Cu (absorb blue/violet →
  lower the blue channel → warm gold/copper); Fe/Ag and light/balanced
  elements default to reflective metallic grey.

Lives in `canvascraft_manager.c` (or a shared `chem_color.c` op), used
by both the POE grid and the inspector. Particles use fixed neon
emissive hex (listed in §4).

---

## 6. Architecture (TPMOS-compliant)

Matches the pattern `palettes_manager.c` already uses and
`pallette-design.txt`'s own note ("a real manager owns the
combination-recipe lookup/crafting-state, not bash or renderer-side
logic").

```
&.widgits/canvas-craft/
  canvas-craft.xhtpm            static 3-panel shell, generic tags
  canvas-craft.css
  open_canvas_craft.sh          launcher (bookmarks pattern)
  ops/
    canvascraft_manager.c       <module> — the brain
    build_canvascraft_manager.sh
    chem_color.c                Drude colour (shared with the inspector)
    voxel_gen.c                 element -> voxels/<symbol>.csv (Phase 4)
```

**`canvascraft_manager.c`** owns:
- parse the recipe registry (legacy `.txt` in P1, `canvascraft_recipes.pdl` after)
- category/tier grouping + search filter
- the quantity resolver (§3)
- **bench state** — slots + contents
- **inventory** — see persistence below
- `CRAFT` execution — validate, consume, produce, append to inventory

Publishes `&.widgits/canvas-craft/state/canvas-craft_ui.txt`
(`key=value` + `<repeat>` rows: recipe list, selected-recipe ingredient
rows with `have`/`need`, bench slots, inventory grid). Polls
`state/canvas-craft_action.txt` for the §2.2 verbs.

**Inventory persistence** — open question §7. Default proposal:
per-user `xyzfs/users/<uuid>/…/canvascraft_inventory.txt`
(`item_id <TAB> count` lines), written atomically (tmp+rename). A shared
"world" inventory (like the chain ledger) is a later mode.

**Change signalling**: DIAMOND — the manager appends to a
`canvas-craft_frame_changed.txt` marker on every publish; the renderer
already consumes markers this way. No `mtime`.

---

## 7. Open questions

1. **`user-pallet` ("My Pallet")** — fold it into Canvas-Craft as a
   "favourites / saved builds" tab, or leave it a separate palette
   category? (Owner said "instead of my palette it should say
   canvas-craft" — leaning fold-in.)
2. **Inventory scope** — per-user save (proposed) vs. one shared world
   inventory vs. both (per-user in solo, shared in a hosted world).
3. **Recipe KEY rename** — keep `elements` (no dispatch churn) or move
   to `canvas-craft` in `pallets.pdl` + `palettes_menu.sh`.
4. **Recipe file** — parse the legacy `.txt` forever, or migrate to
   `canvascraft_recipes.pdl` (§1.3) in Phase 2. Proposed: migrate.
5. **Electrons in element recipes** — free `Electron`s vs. explicit
   `parentC`. Proposed: `parentC`, so ions become a mechanic.
6. **3-pane layout** — extend `layout_sidebar_panel()` to N panels
   (engine change, reusable) vs. flex-wrap two `<scrolllist>`s in one
   `<panel>` (zero engine change). Proposed: flex-wrap for P1, engine
   change when a 2nd 3-pane window needs it.
7. **Quark fidelity** — exact uud/udd at the nucleon tier (teaching) vs.
   the data's loose `Pair_*` aggregation. Proposed: exact at that one
   tier, loose above.
8. **Gating in gameplay** — creative mode = every recipe unlocked;
   survival = recipes gated by a discovered/skill/tool flag. Where does
   the unlock state live? (`canvascraft_unlocks.txt` per user, probably.)
9. **Drag-to-place** — a crafted item should drop onto the desk / a
   piececraft or mutaclysm map via the existing palette placement +
   XDND `drop_action` protocol (`pallette-design.txt` PLACEMENT
   PROTOCOL). Confirm the crafted-item → tile mapping.

---

## 8. Build order

| phase | deliverable |
|---|---|
| **1** | `canvascraft_manager.c` parses the legacy recipe file, groups by tier, runs the resolver. Static 3-panel xhtpm (flex-wrap layout). LEFT recipe list + MIDDLE recipe card with `have/need` rows. **Read-only** — no inventory yet, `have` hardcoded 0. Proves the data + layout + resolver. |
| **2** | Inventory model + bench state + `BENCH_ADD/REMOVE/CRAFT`. RIGHT panel (Relevant / All / Sort / trash). Seed a starter inventory (a stack of quarks, protons, neutrons, electrons + a few common elements). Per-user persistence. Craft loop end to end. |
| **3** | POE tab — periodic-table geometry grid, Drude colours, `POE_SELECT`. `chem_color.c`. |
| **4** | 3D-in-2D inspector — `voxel_gen.c` + `<canvas>` projection + zoom/LOD; ASCII fallback for the terminal mirror. |
| **5** | `canvascraft_recipes.pdl` migration (§1.3); `parentC`/ions; reactive pairs (Water+Sodium); gameplay gating (§7.8). |
| **6** | Drag-to-place a crafted item onto desk/map (§7.9). Retire `palettes-elements.*` (keep as rollback). |

---

## 9. References

- `#.ref/menu/palletes/elements]new=RECIPEZ+]z2🏆.txt` — the recipe tree (§1.1)
- `#.ref/menu/palletes/chemistry_tiles_expanded🏆.csv` — display metadata (§1.2)
- `#.ref/menu/palletes/chem-viz.txt` — the LOD atom-voxel spec (§4)
- `#.ref/menu/palletes/satisfactory-crafting.png` — the UI model (§2.1)
- `#.ref/menu/palletes/little-alchemy-mockup.html` — combination-craft feel
- `&.widgits/palettes/pallette-design.txt` — palettes system + placement protocol + the deferred-crafting note this doc fulfils
- `02-architecture/CENTROID_GOLD_STD.md` — renderer/manager rules
- `reference/TPMOS-DIAMOND-render-chain.md` — marker-not-mtime change signalling
