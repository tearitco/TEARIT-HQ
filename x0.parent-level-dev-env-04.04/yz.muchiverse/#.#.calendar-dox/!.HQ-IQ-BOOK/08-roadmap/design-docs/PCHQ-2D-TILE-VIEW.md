# PCHQ-2D-TILE-VIEW — the `0` view is a real flat tile grid, not a raymarch fallback

Status: **design** (2026-09-09). Not implemented.

Direct request: "the `0` flip to 2d mode … we don't actually want that
view it's showing. we want just the tiles — no second interact — just
2d (non-3d raymarcher, just 2d tiles like RPG Maker / Final Fantasy).
it should look just like desktop. can have a matrix-style grid in the
same grid space as desktop. that old version is a naive legacy port,
can probably be completely scrapped."

Supersedes the pc-hq `0`-toggle notes in `pchq-vs-muta.md` §6 / B1 and
the "chrome-free 2D pixel path" follow-up flagged there.

---

## 1. What's wrong today

`0` flips `render_mode` in `<bv_session>/pieces/system/bv_state.txt`.
`bv_render_3d.c` `main()` does `if (!render_mode) return 0;` and only
ever writes `rgb_frame_3d_overlay.raw`. So `pchq_board_projector.c` has
a workaround (`pchq-vs-muta.md` B1): `render_mode==0` → point
`canvas_raw` at **`rgb_frame.raw`**, which is `chtpm_rgb_render`'s full
compose — the board-viewer engine's own **text UI**: a terrain-glyph
legend, a status/inventory/message strip, Z-level readout, "press …"
hints. That whole ASCII-chtpm surface is the "view we don't want."

It is a naive port of `civ-txt`'s original single-plane text board. The
2D path in this doc replaces it wholesale; the 3D raymarch and the
board-viewer engine itself are untouched.

---

## 2. The three views + their toggles

| Key | From → to | Renderer |
|---|---|---|
| `0` | **3D raymarch ⇄ 2D tile grid** (this doc) | `bv_render_3d.c` ⇄ `bv_render_2d.c` (new) |
| `` ` `` | within 2D: **tile grid ⇄ emoji view** | `bv_render_2d.c` tile mode ⇄ emoji mode |

`0` owns *dimensionality* (`render_mode` 1/0). `` ` `` owns *2D style*
(new key `view_2d_style` in `bv_state.txt`: `tiles` | `emoji`, default
`tiles`). `` ` `` is a no-op while `render_mode==1`. The emoji view is
board-viewer's existing 2D emoji rendering, kept as-is but drawn onto
the same clean grid surface (§4) — no legend, no status text.

There is **no separate "2D interact"**. INTERACT is permanently on (see
§6). All three views are live pictures of the one board state; the keys
that move the xelector / hero / camera behave the same regardless of
which view is up (camera keys are simply inert in 2D, as now).

---

## 3. Renderer choice — `bv_render_2d.c` (a new board-viewer op)

**Decision: Option B.** A new flat painter in board-viewer, NOT the
desktop's own `tp_main` tile renderer and NOT `pc_compose_frame.c`.

Why:
- board-viewer is already the single engine the pc-hq board mirrors
  (`ledger_peers` → `bv_session` → `canvas_raw`). Keeping the 2D
  renderer here = one data path, one process lifecycle, one diamond
  loop. It already loads everything a 2D painter needs:
  `load_voxel_chunk()`, `load_terrain_legend()`, the phymoji/emoji
  feed, the z-manifest, `bv_state.txt` (`selector_x/y`, `current_z`,
  `possessed_id`, …).
- It slots into `bv_dispatch.c` (the P-5 diamond op) as a sibling of
  `bv_render_3d` — `bv_dispatch` picks the renderer by `render_mode`
  and (for 2D) `view_2d_style`, writes the `.raw` + receipt, appends
  the `frame_changed.txt` marker. `bv_compose_frame` is **not called**
  in 2D (that's where the chrome text comes from — see §5).
- "Looks just like the desktop" is met by sharing the desktop's
  **assets and blit primitives**, not its window mode:
  - emoji glyphs via the same `emoji_gen_atlas.+x` atlas the desktop
    entities + board-viewer's own emoji mode already use;
  - real tilesets via `tp_asset_to_sprite.+x` / the palette
    `<fam>_active.txt` binding (see §7);
  - cell size = the house grid (`#.desktop/desk_grid.pdl` `GRID |
    cell_px | N`, default **80**), read the same way
    `khtpm_core_render.c`'s `read_grid_cell_px()` does — so a cell in
    the board window is the same on-screen size as a cell on the desk.

Rejected:
- **A (reuse `tp_main`/`draw_sprite_rgb`)** — that path is a whole
  window mode bound to `#.desktop/` state and the cursword/entity
  machinery, not a callable "blit this cell array to a buffer". Making
  it reusable = surgery in the 15k-line shared `khtpm_core_render.c`,
  against the standing "no new per-project branch / keep the shared
  renderer generic" rule (`CENTROID_GOLD_STD.md`, khtpm-house-standards
  skill).
- **C (`pc_compose_frame.c`)** — it's the legacy ASCII/chrome composer
  we're moving away from, and it reads pc-hq's *own* `pieces/`, forking
  the data path away from the board-viewer session the window mirrors.

---

## 4. `bv_render_2d.c` — what it draws

Input: `<bv_session>` (its own dir), `bv_state.txt`, the chunk, the
active tileset/emoji binding. Output: `pieces/display/rgb_frame_2d.raw`
(+ `rgb_frame_2d.receipt.txt` with `frame_w=`/`frame_h=`).

Per frame:
1. **Board dims.** `load_voxel_chunk()` gives `board_w × board_h`
   (target a **38×38** chunk — a `pc_generate_chunk` param, §8). Frame
   size = `board_w·CELL × board_h·CELL`, `CELL` = house grid cell_px.
2. **Ground fill.** For each cell `(cx,cy)` at `current_z`: the top
   voxel's terrain glyph → its tile. `tiles` style → the bound
   tileset's sprite for that glyph; `emoji` style → the glyph's emoji
   from the atlas. Fallback: flat terrain-legend colour. Empty/air →
   the window background colour (matches the desk).
3. **Entities.** Each piece with `pos_x/y == cx,cy` and `pos_z` on or
   just above `current_z` → its `emoji`/sprite blitted over the ground
   tile (same phymoji/emoji feed 3D uses). Hero, animals, sun/moon are
   pieces too — but sun/moon are hidden in 2D (they're sky objects).
4. **Xelector.** A single-cell highlight box at `selector_x,selector_y`
   — a 2px inset border in the theme accent, same "distinct from an
   entity at the same cell" intent the 3D marker has.
5. **The grid.** Manually drawn — the desktop's grid is a wallpaper
   image, this one is rendered. 1px lines on every `CELL` boundary,
   full width/height. "Matrix style": faint accent-on-dark
   (`kh_shade_hex(bg, +18)`-ish), optionally a brighter line every Nth
   cell. Config: `bv_state.txt` / `arrow_config.txt`
   `grid_2d = on|off` (default on), `grid_2d_major = 0|N`.
6. **Nothing else.** No legend, no status strip, no z readout, no
   hints, no "press …". If a readout is wanted later it goes in the
   khtpm window's toolbar or a help menu, not baked into the pixels.

Camera/pan keys: inert in 2D (as today). `z`/`x` change `current_z` →
the frame re-renders the new slice (a simple, real "peek up/down a
floor", no raymarch).

---

## 5. Removing the on-screen text

The text lives in two places, both bypassed for 2D:
- `bv_compose_frame.c` — composes the legend + status/message strip
  into the final frame. `bv_dispatch` must **not** call it in 2D mode;
  `bv_render_2d` writes a complete frame on its own.
- `chtpm_rgb_render` (the `rgb_frame.raw` path the projector's B1 hack
  points at) — retire that pointer. `pchq_board_projector.c`:
  `render_mode==0` → `canvas_raw = <bv_session>/pieces/display/rgb_frame_2d.raw`
  (was `rgb_frame.raw`). The B1 comment block goes away.

**Keep**: the khtpm board window's own `<item>` toolbar in
`pchq-board.xhtpm` (`In:`, `File`, `Desk`, `Menu`, `Player`, clock) —
that's real chrome drawn by the shared renderer around the `<canvas>`,
not part of the board frame. ("board widget can stay for now.")

`chtpm_rgb_render` / `bv_compose_frame` stay in the tree for the legacy
`civ-txt` / `piececraft-xyz` text-board consumers (they still call
them). Only pc-hq's board stops using that path.

---

## 6. "No second interact" — INTERACT permanently on

Today: the `In:` toolbar item arms `g_interact_relay_on`; keys only
forward while armed; there's auto-disengage-on-blur machinery
(`pchq-vs-tpmos.md` D1/D4/D7).

Change: the board window forwards input to the engine **whenever it has
X focus** — no arm step, no disengage. Concretely:
- `khtpm_core_render.c` interact-forward: gate on `g_x11_window_focused`
  alone for this window class (keep the `13`/`27` → `keyboard/history.txt`
  vs everything-else → `interact_relay.txt` routing from `4e5af97f` —
  that's the double-arrow fix, unrelated to arming).
- `In:` stays as a **status indicator** (always "on" while focused),
  not a toggle. Or drop it — decide during implementation.
- The board_viewer.chtpm parser's INTERACT state machine
  (`active_gui_is_typing.txt`) is no longer the gate; it can stay for
  the Enter-injection path (`de5ef0be`) but nothing keys off "is
  interact armed" anymore.

This also kills a class of bugs (stuck-armed, Esc-doesn't-exit,
auto-exit-when-inactive-not-happening) that this house has chased
repeatedly — see `pchq-vs-tpmos.md` PR-2..4, which this partially
delivers.

---

## 7. Tileset / emoji binding (both are palettes)

`&.widgits/palettes/` families: `emojis`, `rmmv`, `cdda`, `tiled`,
`ohrrpgce`, `piececraft`. Each has `<fam>_active.txt` (`tileset=…`).

- **emoji view** (`` ` ``): the `emojis` family via the shared emoji
  atlas — the glyph → emoji the board's `terrain_legend.txt` already
  maps for board-viewer's own 2D emoji mode. No new binding.
- **tile view** (default): a per-map tileset binding. v1: read one row
  from the host's `pieces/system/board_config.txt`
  (`tile_family = rmmv`, `tile_set = SF_Inside`) → resolve sprites via
  `tp_asset_to_sprite.+x` + the palette `*-ASSET-SOURCE-LOCATION.pdl`
  pointer (never a hardcoded clone path — compact §7). Absent → fall
  back to emoji view so a map with no tileset still renders.

Terrain-glyph → tile-index mapping is `terrain_legend.txt` (already
data-driven, shared by 2D + 3D per `bv_compose_frame.c`'s comment).

---

## 8. Grid size (~38×38)

The board is whatever `load_voxel_chunk()` reports. Target a **38×38**
default: a `pc_generate_chunk.c` arg (it already takes `<seed> x y
[flat]`). 38×38 · 80px = 3040×3040 — bigger than the window, so the
board window needs a **scroll/pan viewport** in 2D (follow the
xelector, or arrow-pan when not possessing). That viewport is the
board window's job (`pchq_board_projector` publishes `view_x/view_y`;
`pchq-board.xhtpm` `<canvas>` shows a sub-rect) — or `bv_render_2d`
renders only the visible window around the xelector. Decide in
implementation; rendering the whole 38×38 every frame at 80px is
~37MB/frame, so **render the viewport only**.

---

## 9. Context menus (2D + 3D, one path)

Guidance for the later menu work — build it once, mode-agnostic:

- The menu is a function of **the cell the xelector is on** (or the
  cell/entity a click hit), never of `render_mode`.
- `open menu` verb → `pchq_board_action.sh cell-menu <x> <y> <z>` →
  the generic `open_context_menu()` popup the desktop already uses,
  populated from the piece(s) under that cell (its `menu.chtpm` /
  `piece.pdl` actions — Move / Mine / Build / Inspect / Possess…).
- Anchor: 2D → the cell's on-screen rect (window-relative, easy —
  `cell·CELL - view_xy`); 3D → the projected screen point, or
  screen-centre if projection is awkward. Anchor is the only
  mode-specific bit.
- Do **not** build a 2D menu and a 3D menu. Do not revive
  board-viewer's own INTERACT numbered-context-menu sketch — use the
  house `open_context_menu` / `menu.chtpm` convention (matches the
  desktop, matches every other entity).

---

## 10. Scrap / keep

**Scrap (pc-hq board only):**
- `pchq_board_projector.c` `render_mode==0 → rgb_frame.raw` (the B1
  hack) + its comment block.
- Any pc-hq reliance on `chtpm_rgb_render` / `bv_compose_frame` for the
  2D view.
- The `In:` arm/disengage semantics (→ always-on, §6).

**Keep:**
- `bv_render_3d.c` (3D raymarch) — untouched; `0` still toggles to it.
- The board-viewer engine (prisc VM, `bv_dispatch` diamond loop,
  `bv_menu_input`, camera/possession model).
- `chtpm_rgb_render` / `bv_compose_frame` in the tree — legacy
  `civ-txt` / `piececraft-xyz` text boards still call them.
- `pchq-board.xhtpm` toolbar chrome.

---

## 11. Files

| File | Change |
|---|---|
| `&.widgits/board-viewer/ops/bv_render_2d.c` | **NEW** — flat tile/emoji painter → `rgb_frame_2d.raw` (+ receipt). Reuses `emoji_gen_atlas.+x` / `tp_asset_to_sprite.+x`, `read_grid_cell_px`-style cell size, `load_voxel_chunk` / `load_terrain_legend` / z-manifest. Draws the grid. No chrome. |
| `&.widgits/board-viewer/ops/bv_dispatch.c` | pick renderer by `render_mode` + `view_2d_style`; 2D path calls `bv_render_2d` only (never `bv_compose_frame`). |
| `&.widgits/board-viewer/scripts/build.sh` | build `bv_render_2d.+x`. |
| `&.widgits/board-viewer/ops/bv_menu_input.c` | `` ` `` (96) → toggle `view_2d_style` in `bv_state.txt` (no-op if `render_mode==1`). `grid_2d` keys optional. |
| `@.apps/piececraft-hq/ops/pchq_board_projector.c` | `render_mode==0` → `canvas_raw = …/rgb_frame_2d.raw`; drop the B1 block; publish `view_x/view_y` if the viewport lives here. |
| `@.apps/piececraft-hq/pieces/system/keybinds.pdl` | `KEY | view_2d_style_toggle | 96` (`` ` ``); document `0` as 2D⇄3D. |
| `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` | interact-forward for the board window gates on focus alone (no arm); `In:` → indicator. |
| `@.apps/piececraft-hq/pieces/system/board_config.txt` | `tile_family` / `tile_set` rows for the tile-view binding. |
| `@.apps/piececraft-hq/ops/pc_generate_chunk.c` | 38×38 default (or a param). |
| `@.apps/piececraft-hq/BOARD-CONTROLS.md` | document `0` / `` ` `` / the 2D view. |

---

## 12. Phases

1. **P1 — the 2D frame.** `bv_render_2d.c` emoji-style only (reuse the
   existing atlas + legend), viewport = whole board (small test chunk,
   e.g. 12×12), grid lines, no chrome. `bv_dispatch` routes `0`.
   Projector points at `rgb_frame_2d.raw`. Verify: `0` shows a clean
   flat emoji grid, no legend/status text; `0` again → 3D unchanged.
2. **P2 — real tilesets + `` ` ``.** `view_2d_style` toggle, the
   `tile_family`/`tile_set` binding, `tp_asset_to_sprite` path, emoji
   fallback.
3. **P3 — 38×38 + viewport.** `pc_generate_chunk` size, render only the
   visible window, follow-xelector / arrow-pan.
4. **P4 — always-on interact.** Drop the arm step (§6); fold in
   `pchq-vs-tpmos.md` PR-2..4 as far as this reaches.
5. **P5 — shared context menu** (§9) — likely its own doc once the menu
   content model is settled.

Related: `pchq-vs-muta.md` (§6/B1 superseded), `pchq-vs-tpmos.md`
(PR-2..4), `pc-hq-INDEX.md`, `@.apps/piececraft-hq/BOARD-CONTROLS.md`,
compact `!.HQ-IQ-COMPACT` §16 (desk-as-map direction),
`[[khtpm-tp_main-globals-footgun]]`.
