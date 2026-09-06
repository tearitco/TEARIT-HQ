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
│ [ search recipe…  🔍] │  ╔═ Gold ══════════════╗     │ [ filter items…  🔍] │
│ ▸ Quarks              │  ║ output ⬛ Gold  ×1   ║     │ Relevant Items       │
│ ▸ Subatomic           │  ╚═════════════════════╝     │  ⬛79 ⬛118            │
│ ▾ Elements   ← POE tab│   0 / 79   Proton  ▓▓░░░     │ All Items            │
│    [>] Hydrogen       │   0 / 118  Neutron ▓░░░░     │  ⬛ ⬛ ⬛ ⬛ ⬛ ⬛      │
│    [ ] Helium         │  ┌ crafting bench ─────────┐ │  ⬛ ⬛ ⬛ ⬛ ⬛ ⬛      │
│    [ ] …  Gold        │  │ ·  ·  ·   ·  ·  ·        │ │  ⬛ ⬛ …               │
│ ▸ Compounds           │  └─────────────────────────┘ │                      │
│ ▸ Materials · Bio …   │  ƒ( oxygen*5 + tree*2      ) │  [Sort]      [🗑]     │
│                       │        [  CRAFT  ]           │                      │
└───────────────────────┴─────────────────────────────┴──────────────────────┘
```

**LEFT — `<sidebar>`**, two tabs:
- **Recipes**: a **search bar** (`<cli_io>`, armed on click / a nav
  digit) that filters the list live — substring on name, symbol,
  formula, or `tier`; empty = full list. Below it, a `<scrolllist>` of
  recipes grouped by `tier` with collapsible headers. Real thumb +
  wheel + PageUp/Down (the palettes VIEW SPECS scroll contract).
- **POE**: the 118 elements in true periodic-table geometry — period
  rows, group columns, the lanthanide/actinide strip pulled out below.
  Each cell = symbol + Z, background = Drude colour (§5). Click → selects
  that element in the middle panel. Non-nav geometry cells; only the
  focused row/cell gets a nav index. (The search bar also filters POE —
  a match highlights its cell and scrolls it into view.)

**MIDDLE — `<panel>`**: the selected recipe.
- Output card: emoji/sprite + name + `yield` ("× 16" like Satisfactory).
- **Ingredient rows** at *highest-common-denominator* form (§3): one row
  per **direct** parent, `have / need  <name>` + a fill bar. Crafting
  Gold shows `0/79 Proton`, `0/118 Neutron` — **not** the expanded
  quark tree. Crafting Water shows `0/2 Hydrogen`, `0/1 Oxygen`.
- **Crafting bench**: a small fixed grid of slots, itself a nav target.
  Items you add from inventory land here; the ingredient rows fill as
  matching items arrive.
- **Formula bar** (`ƒ(…)`): a `<cli_io>` for loading the bench by typed
  expression instead of double-clicking — see §2.3.
- **CRAFT** button — enabled only when every `have ≥ need`. Fires
  `CRAFT`.
- Optional **Inspect** toggle → the 3D-in-2D atom view (§4) in a
  `<canvas>` in this same panel, or a spawned inspector window.

**RIGHT — `<panel>`**: inventory.
- A **filter bar** (`<cli_io>`), same behaviour as the left search but
  over inventory — substring on item name / symbol; empty = show all.
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
| type in the LEFT search bar | `SEARCH_RECIPES:<text>` — recipe list + POE filter live |
| type in the RIGHT filter bar | `FILTER_INV:<text>` — inventory grid filters live |
| click a recipe (LEFT) / a POE cell | `SELECT_RECIPE:<id>` / `POE_SELECT:<Z>` — middle panel repopulates |
| **double-click** an item in inventory (RIGHT) | selects that inventory slot |
| **double-click** the crafting-bench nav (MIDDLE) | `BENCH_ADD:<slot>` — moves 1 of the selected item onto the bench; ingredient rows refill |
| click a bench item, press **Backspace** | `BENCH_REMOVE:<slot>` — item returns to inventory |
| type + Enter in the **formula bar** (MIDDLE) | `BENCH_FORMULA:<raw text>` — stages the whole expression onto the bench (§2.3) |
| **CRAFT** (all rows satisfied) | consume the bench inputs, add `yield ×` output to inventory, clear the bench |
| Inspect | `INSPECT:<id>` — opens the atom view (§4) |

Keyboard-only path (for the terminal mirror / headless): arrows +
digits select; Enter on a bench slot = add focused inventory item;
Backspace = remove; a nav-numbered CRAFT row.

### 2.3 The formula bar (center panel)

A `<cli_io>` under the bench for **loading the bench by typed
expression** — the fast path for anyone who knows what they want, and
the only practical path for large stacks (nobody double-clicks 118
times to build Gold).

**Grammar**

```
formula := term ( '+' term )*
term    := ref ( '*' count )?          ; count defaults to 1
ref     := name                        ; item name / id slug, case-insensitive,
                                       ;   spaces or '-' → '_'  ("Up quark" == up_quark)
         | '[]' int                    ; a stack's STABLE handle (§6.1) - NOT its
                                       ;   row position. Survives Sort / filter.
count   := int                         ; may be a bare number or  x5 / X5 / *5
```

Examples (all equivalent ways to stage the same bench):
```
oxygen * 5 + tree * 2
oxygen x5 + tree x2
[]1 * 5 + []100 * 2                     ; []1 and []100 are whichever stacks own
                                       ;   handle 1 and 100 - see §6.1
oxygen*5 + []100*2                      ; names and handles mix freely
```

> **`[]N` must be stable across a Sort.** `N` is a **stable per-stack
> handle** the manager assigns when a stack first enters the inventory,
> shown as a small `[]N` badge on the cell. Sort and the filter bar
> only reorder / hide *rows*; a stack keeps its handle for its whole
> life, and a freed handle is never immediately reused. So a formula
> you typed (or memorised) before sorting still resolves. Merging two
> stacks of the same item keeps the **lower** handle. See §6.1 for the
> inventory model.

**On submit (Enter):**
1. Parse to `[(item, count), …]`. Unknown name / empty slot / bad
   number → the whole line is rejected, the offending token echoed
   under the bar, bench unchanged.
2. For each term, move `count` of `item` from inventory to the bench
   (top up an existing bench stack of the same item). If inventory is
   short, stage what's available and report `oxygen: staged 3/5 (short 2)`.
3. Ingredient `have/need` rows and CRAFT-enabled recompute (§3).
4. The bar clears; the raw text is kept in a small history (Up-arrow to
   recall), so `+ nitrogen*2` style follow-ups are quick.

**`CLEAR` / `-`**: a leading `-` on a term (`-oxygen*2`) or the word
`clear` empties matching bench stacks back to inventory — the typed
inverse of the per-item Backspace.

**Verb:** `BENCH_FORMULA:<raw text>` → `canvascraft_manager.c` does the
parse + inventory math and republishes. The manager is the only thing
that understands the grammar; the renderer just ships the `<cli_io>`
buffer through on submit (same path every other `<cli_io>` uses).

**Not** a recipe-definition language (yet). It only fills the current
bench for the currently-selected recipe. Using it to define a *custom*
multi-output recipe is a later idea — noted in §7.

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
- category/tier grouping + the left/right **search-filter** strings
- the quantity resolver (§3)
- the **formula-bar grammar** (§2.3) — parse + inventory math
- **bench state** — slots + contents
- **inventory** — see persistence below
- `CRAFT` execution — validate, consume, produce, append to inventory

Publishes `&.widgits/canvas-craft/state/canvas-craft_ui.txt`
(`key=value` + `<repeat>` rows: recipe list, selected-recipe ingredient
rows with `have`/`need`, bench slots, inventory grid, `formula_error`).
Polls `state/canvas-craft_action.txt` for the verbs:
`SELECT_RECIPE:<id>` · `POE_SELECT:<Z>` · `SEARCH_RECIPES:<text>` ·
`FILTER_INV:<text>` · `BENCH_ADD:<inv_slot>` · `BENCH_REMOVE:<bench_slot>` ·
`BENCH_FORMULA:<raw text>` · `CRAFT` · `INSPECT:<id>` · `SORT_INV` ·
`TAB:<left_tab>`.

### 6.1 Inventory model

The inventory is a list of **stacks**. Each stack:

```
handle   int    stable id, assigned at first insert, shown as []N,
                never renumbered by Sort, not immediately reused when freed
item_id  slug   canonical recipe/element id  (gold, up_quark, water)
count    int
```

- **Insert** an item you don't hold → new stack, `handle = ++max_handle`.
- **Insert** an item you already hold → merge into the existing stack
  (its `count += n`); the **lower** of the two handles wins if a merge
  ever unifies two stacks.
- **Sort** (`SORT_INV`) reorders the *published rows* (by name / tier /
  count — cycles) and never touches `handle`.
- **Filter bar** hides rows; handles unaffected.
- A stack that hits `count == 0` is removed; its handle goes on a
  free-list and is only reissued once `max_handle` would otherwise
  exceed a cap, so a just-typed `[]N` doesn't silently point at a new
  item.

Persistence — open question §7.2. Default proposal: per-user
`xyzfs/users/<uuid>/…/canvascraft_inventory.txt`, one stack per line
`handle <TAB> item_id <TAB> count`, plus a `max_handle` header line,
written atomically (tmp + rename). A shared "world" inventory (like the
chain ledger) is a later mode.

**Bench slots** carry the same `{handle, item_id, count}` shape so an
item keeps its handle while staged and `BENCH_REMOVE` puts it back with
the same badge.

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
10. **Formula bar reach** (§2.3) — stays a bench-loader, or grows into
    a recipe-definition language (`define waterbatch = hydrogen*20 +
    oxygen*10` → a saved custom recipe / macro)? Proposed: bench-loader
    only for now; revisit once `user-pallet` fold-in (Q1) is decided —
    saved formulas are a natural fit for that "favourites / saved
    builds" tab.
11. **Name collisions** — `oxygen` vs `O2` vs `Ozone`; `tree` isn't in
    the current recipe file at all. The formula parser needs a
    canonical name/alias table and a clear "unknown item" error.
    Proposed: exact slug match first, then a small alias map
    (`o2 → dioxygen`, plurals), else reject the token.

---

## 8. Build order

| phase | deliverable |
|---|---|
| **1** | `canvascraft_manager.c` parses the legacy recipe file, groups by tier, runs the resolver. Static 3-panel xhtpm (flex-wrap layout). LEFT recipe list + MIDDLE recipe card with `have/need` rows. **Read-only** — no inventory yet, `have` hardcoded 0. Proves the data + layout + resolver. |
| **2** | Inventory model + bench state + `BENCH_ADD/REMOVE/CRAFT`. RIGHT panel (Relevant / All / Sort / trash). Seed a starter inventory (a stack of quarks, protons, neutrons, electrons + a few common elements). Per-user persistence. Craft loop end to end. The **left search bar + right filter bar** (`SEARCH_RECIPES` / `FILTER_INV`) land here — they're just a substring filter over the `<repeat>` the manager already builds. |
| **2.5** | The **formula bar** (§2.3): `BENCH_FORMULA` grammar + parser in the manager, `<cli_io>` + `formula_error` line in the middle panel, Up-arrow history. Small and self-contained once Phase 2's inventory math exists. |
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
