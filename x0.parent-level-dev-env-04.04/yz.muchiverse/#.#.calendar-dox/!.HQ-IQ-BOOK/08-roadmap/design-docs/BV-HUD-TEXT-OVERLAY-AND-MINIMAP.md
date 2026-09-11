# pc-hq board HUD: text overlay + 3D minimap + menu-tb toggles

*Design brainstorm, 2026-09-10. Scope: **piececraft-hq** board window.
Everything lands in the shared board-viewer engine (`bv_render_3d.c` /
`bv_render_2d.c` / `bv_compose_frame.c`), which is what the pc-hq board
renders through.*

*Fundamental direction (honored here): refactor & reuse, never rewrite.
Every item below extends a primitive that already exists on disk — no
new asset pipeline, no new renderer, no per-project C.*

*(piececraft-xyz is out of scope and slated for deprecation — not a
target here.)*

---

## 0. What the user asked for

1. **Text overlay on the map** — readable text drawn on the board view
   (2D and 3D).
2. **Minimap in 3D views** — small, colored blocks only, in a screen
   corner. ~**8 px per voxel column**, **top-right** by default,
   corner + size **changeable in the `.pdl`**.
3. **HUD toggles in the menu toolbar** — one entry per overlay element
   (coords / minimap / fps / …), each on|off, **defaults set by a
   per-game `.pdl`**.

---

## 1. The engine as it stands (what we reuse)

| Need | Already exists | Where |
|---|---|---|
| Bitmap font on disk | 8×16 ASCII glyphs, `#`/`.` grid, one dir per code | `pieces/registry/fonts/ascii/<ascii>/glyph.txt` |
| Glyph loader + mask | `load_one_digit_glyph()`, `digit_mask()` | `bv_render_3d.c` ~1237 (today: only team digits 1/2 on voxel tops) |
| Text-in-frame (2D/ascii) | `line()` / `rowbuf` writer, e.g. `"  Selected (%d,%d): %s"` | `bv_compose_frame.c` ~857 |
| Cell blit primitives | `blit_emoji()` (16×16 nearest), `blit_cjk()` (tinted coverage) | `bv_render_2d.c` ~189/212 |
| RGBA framebuffer + atomic write | `g_fbuf` (`g_fw×g_fh`), `write_file_atomic()` → `rgb_frame_3d_overlay.raw` + `.receipt.txt` | `bv_render_3d.c` `render_one_frame()` |
| Top-solid-voxel per column | empty-space-skip precompute (topmost/bottommost solid per col, once per frame) | `bv_render_3d.c` ~1633 |
| Per-voxel colour | `get_edge_color()`, terrain legend `glyph\|h\|r\|g\|b\|asset\|name` | `bv_render_3d.c` ~1210, `bv_compose_frame.c` ~413 |
| Live canvas px | khtpm writes `#.desktop/pchq_board_view.txt`; engine reads it every frame | `render_one_frame()` ~1585 |
| Pipe-PDL `OPT` reader | `read_pdl_opt(path, name, def)` | `bv_dispatch.c` ~149, `bv_menu_input.c` |
| PDL opt flip from a menu row | `pc_toggle_pdl_opt.sh` (flips `OPT \| name \| 0\|1`) | `@.apps/piececraft-hq/ops/` — already wired for `entities_bar` |
| Menu-tb dropdown rows | `tb-menu` `<item class="dropdown-child">` rows, action = a shell cmd | `@.apps/piececraft-hq/pchq-board.xhtpm` |
| Projector publishes menu state | reads `pchq.pdl` opts, emits `*_on` vars for row labels | `pchq_board_projector.c` (`entities_bar` today) |

---

## 2. Data contract — one file the game writes, the renderer reads

`pieces/display/hud.txt` — plain `key=value`, rewritten atomically each
tick by `pc_menu_input.c` (already owns `pieces/display/*`). Absent file
→ HUD draws nothing (safe default).

```
coords=12 4 7            # x y z of camera / possessed entity
zlevel=7                 # current_z (already clamped in render_one_frame)
facing=NE
possess=hero_01
tick=48210
fps=17                   # engine already profiles g_prof_* under BV_HAVE_GPU
pick=tree_small (12,5)   # pieces/display/pick.txt (milestone-D), reused verbatim
line1=...                # free-form extra rows, game's choice
line2=...
```

The renderer never parses game logic — it blits whatever lines are
enabled. New HUD content = the game writes another `lineN=` / known
key; zero renderer change.

---

## 3. Per-game policy — `pieces/system/hud.pdl`

Pipe-delimited, same shape as `keybinds.pdl` (so `read_pdl_opt` reads it
unchanged):

```
OPT | hud_enabled  | 1     # master switch
OPT | hud_coords   | 1
OPT | hud_minimap  | 1
OPT | hud_fps      | 0
OPT | hud_pick     | 1
OPT | hud_possess  | 1
KEY | hud_anchor      | top-right    # top-left|top-right|bottom-left|bottom-right
KEY | hud_scale       | 1            # integer glyph scale (1 = 8×16)
KEY | minimap_px_per_col | 8         # px per voxel column  ← the "8x per voxel"
KEY | minimap_max_px  | 160          # hard cap on minimap W/H (stays "small in corner")
```

Ships in-repo for pc-hq with these defaults. The menu-tb toggles just
flip the `OPT` lines; `render_one_frame()` re-reads the file every
frame (it already re-reads `keybinds.pdl` that way), so the change is
live with no restart.

---

## 4. Rendering

### 4a. Text blitter (generalise what's already there)

Promote `digit_mask` / `load_one_digit_glyph` in `bv_render_3d.c` to:

```c
static const Glyph8x16 *bv_glyph(const char *font_root, int ascii);   /* load-once cache, 95 printable */

static void bv_blit_text(unsigned char *fb, int W, int H,
                         int x, int y, const char *str,
                         const unsigned char fg[4], int scale, int box);
/* box: draw a 1px dark backing so text stays legible over sky/voxels */
```

Same function drops into `bv_render_2d.c` (same RGBA `px`/`W`
conventions). The ascii "View Board" list in `bv_compose_frame.c`
already emits text rows — those just gain `line()` calls gated on the
`hud_*` opts, no blitter needed there.

Call site: end of `render_one_frame()`, **after** the raymarch loop,
**before** `write_file_atomic()` — the HUD becomes part of the overlay
`chtpm_rgb_render` composites in. CPU-only, no GPU path.

### 4b. Minimap (3D views)

- Reuse the per-column top-solid-voxel scan the raymarcher already
  computes. Grid = `board_w × board_h` columns.
- Each column → one `minimap_px_per_col`-sized flat block (default 8×8
  px). Total minimap = `board_w*8 × board_h*8`, clamped to
  `minimap_max_px` (downsample columns if the chunk is large) so it
  always stays a small corner inset.
- Colour each block by the top voxel's colour (`get_edge_color()` /
  terrain legend). 1px border + ~40%-alpha dark backing for contrast.
- Camera / possessed-entity column = one inverted / bright-outline
  block; `facing` = a 1px tick on that block's edge.
- Anchored per `hud_anchor` (default `top-right`).
- No new geometry, no second raymarch — a 2D fill over data the frame
  already produced.

### 4c. Order / interaction

HUD + minimap are the **last** writes into `g_fbuf` — on top of the
raymarch, below nothing. `hud_enabled=0` (or missing `hud.pdl`) → the
whole block is skipped and the frame is byte-identical to today
(matters for the frame-history `only_on_change` digest).

---

## 5. Menu-tb toggles

`pchq-board.xhtpm` `tb-menu` dropdown gains a "HUD ▸" group of
`dropdown-child` rows, one per opt:

```
action = setsid sh '${HOUSE}/@.apps/piececraft-hq/ops/pc_toggle_pdl_opt.sh'
         '${HOUSE}/@.apps/piececraft-hq/pieces/system/hud.pdl' hud_coords
```

`pc_toggle_pdl_opt.sh` already does exactly this flip for
`entities_bar`. Row labels show live state via a projector var —
`pchq_board_projector.c` reads `hud.pdl` opts and emits `hud_<name>_on`
the same way it emits `entities_bar_on`. A `- cancel -` row per the
standing dropdown rule.

---

## 6. Milestones

| # | Slice | Lands in | Verify |
|---|---|---|---|
| **A** | `bv_blit_text()` + generic glyph cache in `bv_render_3d.c`; hard-coded `"HUD"` string top-right behind a `hud_enabled` read of a hand-written `hud.pdl` | `bv_render_3d.c` | png dump: text legible over sky + voxels |
| **B** | `pieces/display/hud.txt` contract; `pc_menu_input.c` writes coords/zlevel/tick/possess/pick each tick; renderer blits enabled `hud_*` lines with anchor + scale | `pc_menu_input.c`, `bv_render_3d.c` | hud.txt grows on disk; overlay shows live coords while moving |
| **C** | Minimap: per-column top-solid sample → coloured 8px blocks in the anchored corner (clamped) + camera block | `bv_render_3d.c` | png: minimap matches board top-down; player block tracks movement |
| **D** | 2D parity — `bv_blit_text` into `bv_render_2d.c`; `hud_*`-gated `line()` rows in `bv_compose_frame.c` | `bv_render_2d.c`, `bv_compose_frame.c` | 2D view shows same HUD rows; ascii "View Board" list too |
| **E** | Menu-tb "HUD ▸" dropdown group in `pchq-board.xhtpm` + projector `hud_*_on` vars + `pc_toggle_pdl_opt.sh` wiring | `pchq-board.xhtpm`, `pchq_board_projector.c` | toggle a row → hud.pdl flips → overlay updates next frame, no restart |
| **F** | Ship `pieces/system/hud.pdl` with defaults; doc in `BOARD-CONTROLS.md` + `10-user-docs/FEATURE-CATALOG.md` | `@.apps/piececraft-hq/pieces/system/` | fresh clone renders the default HUD |

A–C are the visible core; D–F are 2D parity + menu exposure.

---

## 7. Decisions locked (2026-09-10)

- Minimap: **8 px per voxel column**, capped small, **top-right**,
  corner + size overridable in `hud.pdl`.
- Default overlay lines (slice B): `coords`, `zlevel`, `possess`,
  `pick`. **FPS off by default.**
- Anchor default: `top-right` (shared by text + minimap; independently
  settable later if they need to split).
- pc-hq only. No xyz.

## 8. Note on "dead code" (checked, none removed)

The `@.apps/piececraft-xyz/system/*` fork (`orchestrator`,
`chtpm_rgb_render`, `gl_mirror`, `renderer.c`, …) is **not dead** — xyz's
own `button.sh run` actively launches it as xyz's game engine; only the
3D board window comes from board-viewer. Deleting it now would break
xyz. It goes away with the planned xyz deprecation, not as part of this
work.
