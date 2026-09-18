# Cursword File / Inventory / robot-chat — planning burst (2026-09-17)

**Status:** PLAN, answers landed 2026-09-18. Still no Inventory UI
code this burst — waiting on the grid-shape check-in below.
**Why:** faster bootstrap than a full LLM or event-chat on Cursword main.

## What we are NOT doing this pass

- Not a full transformer / backprop / famous-LLM training loop.
- Not "event-chat dropped into Cursword main" (previous naive theory).
- Not patching `nav.sh` parser-layer commands (still dead; use
  `hqcell`/`mgrcode` + per-PID `entity_menu_history/<pid>.txt`).
- Not rewriting kilo §4 WSR-CIV.

## Direction (user, 2026-09-17, after Claude weekly-limit disconnect)

1. **Cursword gets two new context-menu buttons: File, Inventory.**
   Existing rows stay: Dir, Chat, Bookmarks, Play, Close, Cancel
   (`pals/cursword/meta.pdl` → generated `menu.chtpm`).
2. **File mode:** Cursword itself can become a 📁 icon with its PNG
   drawn on top. That icon represents `cursword/inventory/` (not
   cursword main). Opened → grid file-explorer of that dir.
3. **Inventory button:** opens an x11-hq window, same grid of inventory
   items. Drag entities in/out the same way as File mode.
   Linux-FS equivalent: `mv <entity-dir> cursword/inventory/`.
4. **Robot chat is a separate entity** (🤖️ emoji), events modified on
   *that* pal — not Cursword main. Drag the robot into the Cursword
   folder (or out to desktop / another inventory). Chat happens from
   Inventory, not Cursword main.
5. **Gemma later, not first:** when the user types, gemma DESCRIBEs
   synonym/antonym banks per word; a second pass sets category
   "attention" by hand/cosine (no BP/FF); then a late famous-style
   word-prediction stage. Architecture-only until File+Inventory exist.
6. Palette drag-drop (RPG Maker tiles, one-way) is the existing
   precedent to reuse, not invent.

## Already on disk (do not re-derive)

- Cursword `inventory.txt` exists (`qolq=105`) — not a directory of
  entities yet; do not assume it is `inventory/`.
- `METHOD | Dir` already `xdg-open`s the pal dir — File/Inventory are
  **not** that. They target a nested inventory, in-house grid, not
  the host file manager.
- kilo §11 still says add "AI Chat (events)" next to Chat. **Superseded
  as the first new button.** Chat stays. File + Inventory go first.
- Relay: parser layer dead (live-probed). Window drive =
  `entity_menu_history/<pid>.txt`. PID targeting design =
  `08-roadmap/design-docs/RELAY-WINDOW-TARGETING-DESIGN.md` (design
  only).

## Answers (user, 2026-09-18)

1. **File can wait.** An experimental METHOD hook is fine (stub that
   does not yet turn Cursword into a 📁 sprite). Inventory is the
   real first button.
2. **New directory** `cursword/inventory/`. Move today's
   `inventory.txt` *into* that dir. Do not delete the old path until
   live readers are retargeted (slow migrate).
3. **New 🤖️ pal** — do not retarget an existing desktop pal.
4. Grid question was too compressed — restated below. User: swatch /
   file-hq "sounds ok" pending a real explanation.
5. **Pause WSR-CIV/DSR momentarily.** Todo lives in
   `12.calendar/2026-09-18/2do.md`.

### What currently uses `inventory.txt` (live, not just docs)

These still hardcode `<package_dir>/inventory.txt` and parse `qolq=`:

- `&.widgits/events-hq/ops/mr_change_gold.c`
- `&.widgits/events-hq/ops/mr_select_item.c`
- `&.widgits/events-hq/ops/play_event.sh` (ledger line + gold.txt sync)
- `*.monads/*.muchi-pet/ops/open_rp_menu.sh`

Unrelated `inventory.txt` files also exist under muchi-pals /
avatar-creation (`pieces/world_01/.../inventory.txt`,
`xyzfs/.../home/avatars/inventory.txt`) — different contract, leave
them alone this pass.

**Migrate rule:** after mkdir, keep a compatibility copy or symlink at
`cursword/inventory.txt` pointing at `cursword/inventory/inventory.txt`
until those four ops are retargeted. Do not break Play / CHANGE_GOLD
in the same burst as the menu buttons.

### Purity / russian-doll (keep in dox, do not implement 105 files)

Game items move as **real directories** (`mv entity_dir
cursword/inventory/`). True purity would make quantity itself a pile
of files (105 gold → 105 `qolq` files or dirs). That is allowed, and
overkill for v1.

v1 target:

```
cursword/inventory/           # the bag (movable, nestable)
  inventory.txt               # legacy kv bag — migrate away
  qolq/qolq.txt               # contents: 105   (one number, no key=)
  <robot pal dir>/            # later
```

`gold.txt` at pal root (if present) stays in the slow-migrate pile
with `inventory.txt`; do not invent a third gold source this pass.

## Question 4 restated — what "grid" means

This house already has **two** on-screen shapes that look like "a
folder of tiles," and they are not the same code path:

**A. Palettes swatch grid** (`class="swatch"` on `<item>` inside a
`khtpm_core_render` `.xhtpm` window). Used by RPG Maker tile pickers.
Squares wrap to window width, sprites drawn in cells, one-way drag
onto a map is already real. Closest visual to "inventory of icons."
Does **not** today mean "this cell is a nested pal directory you can
`mv` onto the desktop."

**B. file-hq / host folder** — `METHOD | Dir` already `xdg-open`s the
pal. That is the *host* file manager, not an in-house window, and it
shows the whole pal (events, harnesses, pngs), not just
`inventory/`.

**C. A new x11-hq window** whose cells are **entity pals** (each cell
= a subdirectory of `inventory/`, icon = that pal's png). Drag in/out
= `mv` on the linux fs. This is the actual product. A and B are
*parts* we can steal (swatch layout for the cells; maybe file-hq
patterns for listing a dir), not the product themselves.

So the short question was: **first visible Inventory window = steal
swatch layout (A) and point its cells at `inventory/` subdirs, or
stand up a new layout branch in the renderer (which house rules
forbid if A already fits)?** Recommendation: **A**, no new
`layout_*` in `khtpm_core_render.c`. Confirm before I wire it.

## WAIT (user 2026-09-18) — do not invent a second explorer

File Explorer **already has grid mode** (2026-09-15): toolbar
`FE_VIEWMODE` (`Grid View` / `List View`), manager `grid_view` flag,
`<item class="swatch">` only when `n_grid_entries>0` so list mode
does not trip the whole-page swatch layout. Default is **list**.
Own comment: breadcrumbs/close in GRID have **not** been proven live.

`start_dir` is already a `fe_request.txt` key (`fe-pick.sh`), not a
renderer argv. No new `layout_*`. No new inventory widget.

**Next burst, only if you say so:** prove grid live (open file-explorer,
relay-click Grid View, screenshot/state), then Inventory METHOD =
that same widget + `start_dir=$pal/inventory/` + maybe default
`grid_view=1`. Still no symlink. Still no robot pal. Still no
gold-ops retarget unless you ask.

## File-explorer GRID — WIP (user 2026-09-18)

**Live gaps (why grid looks empty):**

- Grid cells are `<item class="swatch" label="${icon} ${name}">` with
  **no `sprite=`**. Swatch layout is a square tile; without a sprite
  there is no picture. Palettes work because they set `sprite=` (and
  put a glyph in `label=`).
- Filename is stuffed into `label=` on a tile that mostly draws a
  **nav badge above the square**. Name/emoji often clip or sit in the
  wrong place. List mode still shows `icon name size` as a row; grid
  does not get that row chrome.
- **Left click** on a DIR is `FE_ENTRY` → `chdir` (open folder). That
  is list-browser semantics. It is **not** desk placement.

**Intended interaction (WIP, do not code until asked):**

1. Grid cell = **preview** (real image if the entry has one: pal
   `atlas.png` / anim frame / toy icon) + **filename** under or on the
   tile. Still not a new renderer layout branch — fill `sprite=` + a
   caption the swatch path can actually draw.
2. **Left click** stays mild: open dir / select file (picker). Does
   **not** start dragging onto the desk.
3. **Right click** → context menu (same `meta.pdl` / `menu.chtpm`
   family as pals) → **Move / Drag** arms "placement grid image"
   (the palettes `place` / desk-tile chain). Until that menu item is
   chosen, the cell is not a brush.

Palette left-click-to-place is the *old* one-way tile picker. Inventory
and file-explorer grid are the *bag*: look first, arm drag on purpose.

No code this note.

## Still open (new)

- Inventory window: steal swatch grid (A) — yes/no?
- Compat path: symlink `cursword/inventory.txt` →
  `inventory/inventory.txt` until ops retarget — yes/no?
- Robot spawn: create the new pal on the **desktop** first, then drag
  into `inventory/`, or create it already inside `inventory/`?
