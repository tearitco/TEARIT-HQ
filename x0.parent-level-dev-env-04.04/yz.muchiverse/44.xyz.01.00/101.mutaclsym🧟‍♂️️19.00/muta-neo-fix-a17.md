# muta-neo-fix-a17.md — Investigation Report

## Summary

The `+18.01` "neo" branch is a half-ported rewrite that lost two critical subsystems:
**interact mode** and the **starting map**. The old `+18.0G` map and controls were
functional but lacked z-levels; the neo branch attempted to steal camera/player
controls from `&.widgits/board-viewer` and add z-levels, but the port is
incomplete and the base project data was never carried over.

---

## 1. Interact Mode — BROKEN / MISSING

### Old (+18.0G) Architecture (WORKING)

```
keyboard_input.c
  └─ writes pieces/apps/player_app/history.txt          (decimal keycodes)
  └─ writes pieces/keyboard/history.txt                 ("KEY_PRESSED: N" for chtpm)

chtpm_parser_pal.c
  └─ reads pieces/keyboard/history.txt
  └─ on INTERACT keys → writes pieces/apps/player_app/interact_relay.txt

game_dispatch.c (ops/+x/game_dispatch.+x)
  └─ reads ALL keys from interact_relay.txt
  └─ dispatches each to:
       ├─ ops/+x/move_player.+x
       ├─ ops/+x/choice.+x
       └─ ops/+x/camera_control.+x    ← BOARD-VIEWER CAMERA/POSITION CONTROL
  └─ runs end_turn + tick_monsters when hero acted
  └─ composes frame

pal/main_loop_chtpm.pal
  └─ exec game_dispatch every 16.6ms (persistent loop)
```

**Key data file:** `pieces/world_01/map_start/hero/state.txt`
Contains: `interact_mode`, `render_mode`, `camera_mode`, `cam_pan_x/y/z`,
`cam_yaw`, `cam_pitch`, `cam_z_level`, `xlector_pos_x/y`, `facing`, `hero_z`,
`emoji_mode`, `action_cursor`, `digit_accum`, etc.

### New (+18.01) Architecture (INCOMPLETE)

```
keyboard_input.c
  └─ writes pieces/apps/player_app/history.txt
  └─ writes pieces/keyboard/history.txt

main_module.pal
  └─ read_history pieces/apps/player_app/history.txt
  └─ mua_menu_input x9
  └─ mua_compose_frame
  └─ hit_frame

mua_menu_input.c (ops/+x/mua_menu_input.+x)
  └─ handles ALL input in one monolithic op
  └─ built-in interact mode (lines 1202-1258):
       ├─ 'i' toggles interact_mode
       ├─ arrows move cursor_x/y
       └─ 0-4 change render_mode
```

**Problems:**
1. **`pieces/apps/player_app/` directory does not exist in base project.**
   The old project had `interact_relay.txt`, `history.txt`, `state_changed.txt`.
   The new project's `button.sh` creates these in the *session* dir only.
2. **`pieces/hero_01/state.txt` is missing critical fields.**
   The new generate op writes: `entity_type`, `hp`, `pos_x/y/z`, `chunk_x/y`,
   `hunger`, `thirst`, `stamina`. It does NOT write: `interact_mode`,
   `render_mode`, `camera_mode`, `cam_*`, `xlector_pos_*`, `facing`, `hero_z`,
   `emoji_mode`, `action_cursor`, `digit_accum`, `active_panel`, `panel_cursor`.
   Result: `mua_menu_input.c` reads `interact_mode` → defaults to 0 → interact
   mode is permanently OFF. Even if toggled on, there is no camera state to
   update.
3. **`interact_relay.txt` is dead code.**
   `main.chtpm` still declares `<interact src="pieces/apps/player_app/interact_relay.txt" />`
   but nothing writes to it anymore. The chtpm parser's INTERACT bridge is
   disconnected.
4. **No `game_dispatch.c`, no `camera_control.c`.**
   The old per-tick dispatch loop and the board-viewer camera control op are
   completely absent. `mua_menu_input.c` has a simplified cursor-only mode
   but no real camera system (yaw/pitch/pan/z-level).

---

## 2. Starting Map — MISSING / WRONG

### Old (+18.0G) Map (WORKING)

```
pieces/world_01/map_start/
  ├── state.txt          id=map_start, width=40, height=16, turn=2903
  ├── map.txt            40x16 ASCII tile map (walls, corridors, stairs +)
  ├── transitions.txt    x|y|dest_map_id|dest_x|dest_y
  ├── furniture.txt
  ├── hero/
  │   ├── state.txt      pos_x=28, pos_y=7, hero_z=14, camera_mode=2, ...
  │   └── inventory/
  ├── items/             item_water_01, item_rock_01, item_stick_01, etc.
  └── monsters/          zombie_01, zombie_02, zombie_child_01/02/03
```

The map is a hand-crafted 2D tile-based overworld with building interiors
(`building_01_gf`, `building_01_f2`) and a second map (`map_02`) linked via
 stair transitions.

### New (+18.01) Map (BROKEN)

**`pieces/world_01/map_start/` does not exist.**
The new branch replaced the 2D tile map with a **voxel sandbox generator**
(`ops/+x/mua_generate_chunk.+x` / `mua_generate_chunk.c`). On "Confirm & Start"
it runs:

```
mua_generate_chunk.<x> <seed> 0 0
```

This generates a 3D voxel world (chunks, terrain columns, etc.) and writes
`pieces/hero_01/state.txt` with voxel coordinates (`chunk_x=0, chunk_y=0,
pos_z=19`). There is no 2D `map.txt`, no `transitions.txt`, no `furniture.txt`,
no `monsters/` directory under a `map_start`.

**Result:** The "neo starting map" is a procedurally generated voxel column
instead of the intended hand-crafted 2D overworld. The user reports it "does
not look right" because the old 40x16 ASCII map with walls, corridors, and
buildings was never ported — only the new voxel generator was added.

---

## 3. Board-Viewer Camera/Player Controls — NOT PORTED

The user explicitly wanted to steal **camera/player controls** from
`&.widgits/board-viewer`.

### Board-Viewer Status

`&.widgits/board-viewer/` is **completely empty** (0 files). However,
`mua_menu_input.c` references:

- `&.widgits/board-viewer/button.sh` (line 656)
- `&.widgits/board-viewer/ops/+x/ledger_peers.+x` (line 665)
- `&.widgits/board-viewer/ops/+x/ledger_peers.+x` for widget focus

These binaries are missing.

### What Was Supposed to Be Ported

From the old `+18.0G` project (which DID have board-viewer controls):

| File | Purpose | Status in +18.01 |
|------|---------|-------------------|
| `ops/camera_control.c` | 3D camera movement (yaw/pitch/pan/z-level) in 4 modes | **MISSING** |
| `ops/muta_widget_cmds.c` | Widget bridge commands (OPEN_BOARD_WIDGET, etc.) | **MISSING** |
| `ops/move_player.c` | Hero/cursor movement + bump attack | **MISSING** |
| `ops/choice.c` | Menu choice / xlector cursor | **MISSING** |
| `ops/end_turn.c` | Turn advancement | **MISSING** |
| `ops/tick_monsters.c` | Monster AI | **MISSING** |
| `ops/game_dispatch.c` | Per-tick key dispatch loop | **MISSING** |

The new `mua_menu_input.c` has a **simplified substitute**: it handles 'i'
toggle + arrow cursor + 0-4 view mode directly, but it does NOT implement:
- 3D camera modes (first-person, third-person, free-roam, bird's-eye)
- Camera yaw/pitch/pan/z-level state
- Cross-project board-viewer widget focus
- The old xlector possession system
- Monster AI / bump combat / end-turn cycle

---

## 4. Root Cause Analysis

### What Happened

The `+18.01` branch appears to be a **parallel rewrite** inspired by
piececraft-xyz's session-isolation architecture, NOT a clean incremental
port of `+18.0G`. Evidence:

1. **New `button.sh`** creates session dirs with symlinks (piececraft pattern).
   Old `button.sh` just `exec`'d the orchestrator directly.
2. **New `orchestrator.c`** is a stripped-down launcher (no compile, no
   config write, no map dir creation). Old `orchestrator.c` compiled all
   binaries, wrote `config.txt` from hero state, wrote `master_ledger.txt`,
   and created `pieces/world_01/map_start/hero/inventory/`.
3. **New `build.sh`** builds completely different ops (`mua_*` prefix) and
   skips all the classic game ops (`move_player`, `camera_control`,
   `game_dispatch`, etc.).
4. **New `pal/main_module.pal`** reads `history.txt` directly and calls
   `mua_menu_input`. Old `pal/game_module.pal` ran `game_dispatch` in a loop.
5. **New `mua_menu_input.c`** is a monolithic 1500-line op that bundles
   menu dispatch + interact mode + world tick + monster AI + board-widget
   launch. Old architecture split these into separate ops.

### What Was Lost in the Rewrite

| Lost Feature | Where It Lived | Why It's Gone |
|--------------|----------------|---------------|
| 2D starting map (`map_start/map.txt`) | `pieces/world_01/map_start/` | Replaced by voxel generator; base dir never seeded |
| `interact_mode` state fields | `hero/state.txt` | New generator doesn't write them |
| Camera control op | `ops/camera_control.c` | Not ported; replaced by simplified cursor in `mua_menu_input` |
| Board-viewer widget binaries | `&.widgits/board-viewer/` | Directory empty; references in code are dangling |
| Game dispatch loop | `ops/game_dispatch.c` | Replaced by inline logic in `mua_menu_input` |
| `interact_relay.txt` pipeline | `interact_relay.txt` + `chtpm_parser_pal` bridge | Dead code; nothing writes to it |

---

## 5. Specific File Comparisons

### `button.sh`
- **Old:** `exec orchestrator` — orchestrator manages full lifecycle.
- **New:** Creates session dirs, symlinks, runs `orchestrator &`, then
  `keyboard_input` in foreground. More complex but session-isolated.

### `orchestrator.c`
- **Old:** Compiles binaries, writes `config.txt` from `map_start/hero/state.txt`,
  writes `master_ledger.txt`, creates map dirs, launches `keyboard_input`,
  `renderer`, `chtpm_parser_pal`, `gl_mirror`.
- **New:** No compile step, no config/ledger write, no map dir creation.
  Only launches `renderer` + `chtpm_parser_pal` (via PAL_LAYOUT env).

### `game.chtpm` → `main.chtpm`
- **Old (`game.chtpm`):** Full game HUD with "Control Hero" INTERACT button,
  craft/examine/help submenus, detailed help text explaining interact mode.
- **New (`main.chtpm`):** Minimal HUD. `interact_relay.txt` reference is stale.
  No INTERACT button. Help text gone.

### `pal/` modules
- **Old:** `game_module.pal` + `main_loop_chtpm.pal` — persistent process
  running `game_dispatch` every 16.6ms.
- **New:** `main_module.pal` — reads `history.txt` directly, calls
  `mua_menu_input`, `mua_compose_frame`. Single-shot per keypress.

---

## 6. Conclusion

The `+18.01` branch is **not a working upgrade** of `+18.0G`. It is a
partial rewrite that:

1. **Lost the starting map** — replaced with a voxel generator that doesn't
   produce the old 2D overworld.
2. **Broke interact mode** — the state fields and relay pipeline were removed;
   the replacement in `mua_menu_input.c` is incomplete without camera state.
3. **Did not port board-viewer controls** — `camera_control.c`,
   `muta_widget_cmds.c`, and the board-viewer binaries themselves are missing.

## 6. Chosen Fix Plan — OPTION A (Hybrid)

**Decision:** Restore `+18.0G`'s proven input relay architecture into `+18.01`,
while keeping the new voxel generator and session scaffolding. This preserves:
- 3D voxel map (from `mua_generate_chunk.c`)
- Mode selector 0–4 (render_mode → 2D emoji / 3D FP / 3D TP / 3D free-roam / 3D bird's-eye)
- Camera/player controls from `ops/camera_control.c`
- Interact mode via `interact_relay.txt` → `game_dispatch.c` pipeline

### Step 1 — Copy Missing Ops from +18.0G

Copy these source files from `+18.0G/ops/` to `+18.01/ops/`:

| Source (+18.0G) | Destination (+18.01) | Purpose |
|-----------------|----------------------|---------|
| `ops/game_dispatch.c` | `ops/game_dispatch.c` | Per-tick key dispatch loop |
| `ops/camera_control.c` | `ops/camera_control.c` | 3D camera modes 1-4 |
| `ops/move_player.c` | `ops/move_player.c` | Hero/cursor movement + bump attack |
| `ops/choice.c` | `ops/choice.c` | Menu/xlector cursor |
| `ops/end_turn.c` | `ops/end_turn.c` | Turn advancement |
| `ops/tick_monsters.c` | `ops/tick_monsters.c` | Monster AI |
| `ops/muta_widget_cmds.c` | `ops/muta_widget_cmds.c` | Board-widget bridge |
| `ops/pickup.c` | `ops/pickup.c` | Item pickup |
| `ops/drop.c` | `ops/drop.c` | Item drop |
| `ops/eat.c` | `ops/eat.c` | Eat/drink |
| `ops/craft.c` | `ops/craft.c` | Crafting |
| `ops/examine.c` | `ops/examine.c` | Examine tile |
| `ops/save_game.c` | `ops/save_game.c` | Save/load |
| `ops/toggle_emoji.c` | `ops/toggle_emoji.c` | Toggle emoji/ASCII |
| `ops/compose_frame.c` | `ops/compose_frame.c` | ASCII frame composer |
| `ops/compose_rgb_frame.c` | `ops/compose_rgb_frame.c` | RGB frame composer |
| `ops/pdl_reader.c` | `ops/pdl_reader.c` | PDL file reader |
| `ops/title_input.c` | `ops/title_input.c` | Title screen input |
| `ops/compose_title_frame.c` | `ops/compose_title_frame.c` | Title frame composer |
| `ops/generate_map.c` | `ops/generate_map.c` | Map generator (authoring tool) |

Also copy `ops/lib/stb_image.h` and `ops/lib/stb_image_write.h` if not present.

### Step 2 — Update build.sh

Add the classic ops to `+18.01/scripts/build.sh`:

```sh
$CC $CFLAGS -o "ops/+x/game_dispatch.+x" "ops/game_dispatch.c"
$CC $CFLAGS -o "ops/+x/camera_control.+x" "ops/camera_control.c"
$CC $CFLAGS -o "ops/+x/move_player.+x" "ops/move_player.c"
$CC $CFLAGS -o "ops/+x/choice.+x" "ops/choice.c"
$CC $CFLAGS -o "ops/+x/end_turn.+x" "ops/end_turn.c"
$CC $CFLAGS -o "ops/+x/tick_monsters.+x" "ops/tick_monsters.c"
$CC $CFLAGS -o "ops/+x/pickup.+x" "ops/pickup.c"
$CC $CFLAGS -o "ops/+x/drop.+x" "ops/drop.c"
$CC $CFLAGS -o "ops/+x/eat.+x" "ops/eat.c"
$CC $CFLAGS -o "ops/+x/craft.+x" "ops/craft.c"
$CC $CFLAGS -o "ops/+x/examine.+x" "ops/examine.c"
$CC $CFLAGS -o "ops/+x/save_game.+x" "ops/save_game.c"
$CC $CFLAGS -o "ops/+x/toggle_emoji.+x" "ops/toggle_emoji.c"
$CC $CFLAGS -o "ops/+x/compose_frame.+x" "ops/compose_frame.c"
$CC $CFLAGS -o "ops/+x/compose_rgb_frame.+x" "ops/compose_rgb_frame.c" -lm
$CC $CFLAGS -I"ops/lib" -o "ops/+x/dump_rgb_png.+x" "ops/dump_rgb_png.c" -lm
$CC $CFLAGS -o "ops/+x/pdl_reader.+x" "ops/pdl_reader.c"
$CC $CFLAGS -o "ops/+x/title_input.+x" "ops/title_input.c"
$CC $CFLAGS -o "ops/+x/compose_title_frame.+x" "ops/compose_title_frame.c"
$CC $CFLAGS -o "ops/+x/generate_map.+x" "ops/generate_map.c"
$CC $CFLAGS -o "ops/+x/muta_widget_cmds.+x" "ops/muta_widget_cmds.c"
```

Keep the existing `mua_*` ops build lines — they are used by the new module.

### Step 3 — Update button.sh check/verify

Add the classic ops to the `check` verb in `+18.01/button.sh`:

```sh
for b in ops/+x/move_player.+x ops/+x/end_turn.+x ops/+x/compose_frame.+x \
         ops/+x/pickup.+x ops/+x/drop.+x ops/+x/eat.+x \
         ops/+x/tick_monsters.+x ops/+x/craft.+x ops/+x/examine.+x \
         ops/+x/save_game.+x ops/+x/title_input.+x ops/+x/compose_title_frame.+x \
         ops/+x/pdl_reader.+x ops/+x/choice.+x ops/+x/compose_rgb_frame.+x \
         ops/+x/dump_rgb_png.+x ops/+x/generate_map.+x ops/+x/camera_control.+x \
         ops/+x/muta_widget_cmds.+x; do
```

### Step 4 — Copy Starting Map Data

Copy the entire `pieces/world_01/map_start/` tree from `+18.0G` to `+18.01`:

```
+18.0G/pieces/world_01/map_start/
  ├── state.txt
  ├── map.txt
  ├── transitions.txt
  ├── furniture.txt
  ├── hero/
  │   ├── state.txt
  │   └── inventory/
  ├── items/
  └── monsters/
```

Also copy `+18.0G/pieces/world_01/state.txt` (world-level state with `id=world_01`).

### Step 5 — Fix orchestrator.c

Update `+18.01/system/orchestrator.c` to restore the old initialization:

1. **Add compile step** for the new ops (call `compile_ops()` or inline compile).
2. **Write `config.txt`** from `pieces/world_01/map_start/hero/state.txt` if absent
   (seed `hero_start_x/y` from the hero's `pos_x/pos_y`).
3. **Write `master_ledger.txt`** header if absent.
4. **Create map directories** (`pieces/world_01/map_start/hero/inventory/`, etc.).
5. **Launch `keyboard_input`** as a separate process (currently missing!).
6. **Launch `gl_mirror`** if available.

### Step 6 — Restore game.chtpm

Rename or update `+18.01/pieces/chtpm/layouts/main.chtpm` to include:
- INTERACT button: `<button label="Control Hero" onClick="INTERACT" />`
- Craft/examine/help submenus with their ACTUATE/BACK structure
- Help text explaining interact mode, controls, and mode selector
- Reference to `pieces/apps/player_app/interact_relay.txt`

Keep the new `new_game.chtpm` for the title screen, but have it navigate to
`game.chtpm` (not `main.chtpm`) after CONFIRM_START.

### Step 7 — Restore pal modules

Replace `+18.01/pal/main_module.pal` with the old `game_module.pal` pattern:

```pal
li x1, 0

compose_frame
compose_rgb_frame
hit_frame

loop:
exec ./ops/+x/game_dispatch
sleep 16667
j loop
```

Keep `new_game_module.pal` for the title screen.

### Step 8 — Fix interact_relay.txt path

Ensure `+18.01/pieces/apps/player_app/interact_relay.txt` is created at
session start (add to `button.sh` run section) and cleared on each run.

### Step 9 — Fix hero_01 state.txt generation

Update `ops/mua_generate_chunk.c` (the voxel generator) to ALSO write the
legacy fields that `camera_control.c` and `mua_menu_input.c` expect into
`pieces/hero_01/state.txt`:

```c
fprintf(f, "entity_type=hero\n");
fprintf(f, "hp=100\n");
fprintf(f, "pos_x=%d\n", x);
fprintf(f, "pos_y=%d\n", y);
fprintf(f, "pos_z=%d\n", z);
fprintf(f, "chunk_x=0\n");
fprintf(f, "chunk_y=0\n");
fprintf(f, "hunger=0\n");
fprintf(f, "thirst=0\n");
fprintf(f, "stamina=100\n");
/* NEW: camera/interact defaults */
fprintf(f, "interact_mode=0\n");
fprintf(f, "render_mode=2\n");       /* 2 = 3D third-person (board-viewer default) */
fprintf(f, "camera_mode=2\n");       /* 2 = third person */
fprintf(f, "hero_z=%d\n", z);
fprintf(f, "emoji_mode=1\n");
fprintf(f, "xlector_pos_x=%d\n", x);
fprintf(f, "xlector_pos_y=%d\n", y);
fprintf(f, "facing=1002\n");         /* LEFT */
fprintf(f, "action_cursor=-1\n");
fprintf(f, "digit_accum=0\n");
fprintf(f, "active_panel=none\n");
fprintf(f, "panel_cursor=0\n");
fprintf(f, "panel_digit_accum=0\n");
/* camera defaults */
fprintf(f, "cam_pan_x=0.00\n");
fprintf(f, "cam_pan_y=0.00\n");
fprintf(f, "cam_pan_z=0.00\n");
fprintf(f, "cam_yaw=180.00\n");
fprintf(f, "cam_pitch=6.00\n");
fprintf(f, "cam_z_level=2\n");
```

### Step 10 — Validation

1. `./button.sh compile` — all binaries build
2. `./button.sh check` — all binaries present
3. `./button.sh run` — game starts on title screen
4. CONFIRM_START — voxel world generates, navigates to game screen
5. `i` key — interact mode toggles ON (message "Interact mode ON")
6. Arrow keys in interact mode — cursor moves, camera updates
7. Keys `0`-`4` — render mode switches (2D emoji → 3D modes)
8. `c`/`v` in 3D modes — z-level changes
9. `q`/`e`/`r`/`t`/`f` — camera yaw/pitch/reset
10. Board-viewer widget launches via OPEN_BOARD_WIDGET (if `&.widgits/board-viewer/` is populated)

---

## 7. Rollback Plan

If Option A fails midway:
1. `+18.0G` is untouched — always revertible.
2. `+18.01` has no critical data (no saves, no ledgers) before this fix.
3. Each step above is independently reversible by restoring from `+18.0G` backups.

---

## 8. SESSION ADDENDUM (2026-08-17, later) — Direction changed: voxel 3D with z-levels, not the 2D map restore

**Direct user instruction, supersedes §6/§7's "Option A hybrid" plan above**: skip restoring
`+18.0G`'s hand-crafted 2D `map_start` map entirely. Instead: (1) starting map config should be
external/.pdl-driven, not hardcoded; (2) steal `&.widgits/board-viewer`'s real 3D voxel rendering
(the thing board-viewer was ALWAYS meant to demonstrate) so mutaclysm's own map shows **multiple
z-levels of walls/trees stacked and visible on ONE screen** (not the old teleport-only z-level
system, which never rendered more than one level at a time — teleport between maps stays,
per-map-multi-Z-visible is the new part); (3) board-viewer has NO interact mode — mutaclysm's own
interact-mode system needs to be layered on top of board-viewer's stolen renderer, not the other
way around.

### 8.1 Real, confirmed architecture facts (read before touching rendering code again)

**Two completely different 3D renderers exist in this house, do not confuse them:**

| | `+18.0G`'s own `ops/compose_rgb_frame.c` (`render_3d_view()`) | `&.widgits/board-viewer/ops/bv_render_3d.c` |
|---|---|---|
| Technique | Flat rasterizer — projects boxed geometry through a camera, sorts back-to-front, fills solid-color polygons | Real per-pixel DDA raymarcher (Amanatides & Woo grid traversal), real voxel-texture sampling |
| Z-levels | Single level only — walls are flat boxes, no real stacking | Real multi-Z voxel stack, ALL levels the camera can see render simultaneously |
| Textures | None / flat color | Real PNG-derived voxel textures via `get_voxel8_cached()`/`sample_voxel8_pixel()` |
| Camera | Simple, known-buggy (confirmed live this session — see §8.2 RGB dump comparison) | Real `Camera` struct + `build_camera()`, already had 3 real camera bugs found+fixed in a PRIOR session (mode-2 pitch, mode-1 clipping, free-roam/bird's-eye anchor-height) — see `au11-hq/legacy-shared-fix.md` §2.6 and `au11-hq/opencode-mutafix-pie.md` (NOTE: user flagged that doc's own specific camera-swap claims as possibly stale/superseded by this session's work — don't trust its line-level claims without re-verifying against current file state) |
| Data source | `pieces/world_01/map_start/map.txt` (hand-crafted 2D ASCII) | `pieces/system/chunks/chunk_X_Y/chunk_X_Y_zN.txt` (one 16x16 glyph grid per Z level) + `pieces/system/board_manifest.txt` (`z_base=`/`z_count=`) + `pieces/system/terrain_legend.txt` (`glyph|height|r|g|b|asset_hex|name`) |
| Interact mode | None built-in — mutaclysm's own | None built-in — mutaclysm's own must be layered on |

**Confirmed live via RGB dump this session** (both dumped through `dump_rgb_png.+x`, which reads
`pieces/display/rgb_frame.raw` directly — no window-capture race risk, see §8.3):
- `+18.0G`'s own 3D render: flat single-level brick-textured boxes, entities as flat colored
  squares, simplistic top-down-ish camera angle. This IS "og muta's buggy cam" the user referenced.
- `board-viewer`'s 3D render (via piececraft-xyz, tested live): real stacked/textured voxel cubes,
  multiple Z-levels genuinely visible in one frame, proper perspective camera. **This is the
  target look for muta-neo's map rendering.**

### 8.2 `mua_generate_chunk.c` ALREADY writes board-viewer's exact chunk format

Confirmed by direct invocation this session: `./ops/+x/mua_generate_chunk.+x <seed> 0 0` writes
`pieces/system/chunks/chunk_0_0/chunk_0_0_z0.txt` through `_z31.txt` (32 real 16x16 glyph grids)
AND `pieces/system/board_manifest.txt` with `z_base=pieces/system/chunks/chunk_0_0/chunk_0_0_z` /
`z_count=32` — this is byte-for-byte the same manifest format `bv_render_3d.c`'s own
`bv3d_has_z_manifest()` expects. **No format conversion needed** — mutaclysm's existing voxel
generator is already compatible with board-viewer's renderer. The gap is purely on the render/
input-wiring side, not the data side.

### 8.3 Real, confirmed testing method for board-viewer-family renderers (NOT XTest, NOT raw window capture)

Per direct user correction this session ("wait when u run it it should popup on my desktop so i
can also see. always test like this"): **default to launching via the real `button.sh run` (or
project equivalent) so a REAL GL window appears on the user's live desktop** — do not test purely
headless-in-background unless in a genuine no-`/dev/tty` API sandbox (see house testing doc's own
Pitfall #11 for when headless actually applies). Confirm the window is visible by asking the user,
or by confirming the display-mirror process (`x11_mirror.+x` / `gl_mirror`) is alive via `ps aux`.

**`board-viewer`'s widget (`bv_menu_input.c`/`bv_render_3d.c`/`bv_compose_frame.c`) is part of the
SAME `chtpm_parser_pal`/PAL-VM family as the rest of this house** — confirmed by grepping
`bv_menu_input.c` for its own input source: it reads `pieces/apps/player_app/history.txt` and
relays interact-mode keys into `pieces/apps/player_app/interact_relay.txt`, exactly like
mutaclysm/piececraft's own main game loop. **It is NOT a raw-Xlib program needing an XTest
injector** (unlike `tp_taskbar.c` — see house testing doc's own SCOPE section) — the visible GL
window is a passive mirror of composited frame data, it doesn't capture input itself. File
injection into the SESSION's own `pieces/apps/player_app/interact_relay.txt` (bare decimal ASCII
per line, e.g. `113` for `q`) works correctly and was verified live: injecting `113` (q / yaw-left)
changed `yaw=180` → `yaw=170` in the rendered frame, confirmed via both the text frame diff AND a
real RGB dump with a fresh, different `checksum_fnv1a64`.

**Real live-tested flow to reach board-viewer's 3D view from a cold launch** (verified end-to-end
this session on piececraft-xyz, same menu pattern mutaclysm's own `mua_menu_input.c` already
implements via `OPEN_BOARD_WIDGET` — see §8.4):
1. Launch via `button.sh run` (real desktop window pops up).
2. Outer CHTPM-level menu nav uses `pieces/keyboard/history.txt` with the
   `[TIMESTAMP] KEY_PRESSED: <decimal>` format (NOT `pieces/apps/player_app/history.txt` — that one
   is read only by the PAL game module directly, confirmed by testing both: writes to
   `player_app/history.txt` produced zero frame change; writes to `pieces/keyboard/history.txt`
   worked immediately, visible as `Nav > 1_` echo in the frame).
3. Select "Confirm & Start (Seeded World)" (digit `49`='1' + Enter `13`) → real chunk data
   generates, game screen loads.
4. Select "View Board" (digit `50`='2' + Enter `13`) → spawns `board-viewer/button.sh run-widget`
   as a real child process with its own session dir, own `gl_mirror`, own X window.
5. That widget session's own `pieces/apps/player_app/interact_relay.txt` accepts real camera/
   selector keys (`q`/`e` yaw, `r`/`t` pitch, `wasd` pan, `c`/`v` z-level, arrows = selector move)
   — confirmed interact mode was ALREADY ON by default in this widget (`Interact mode: ON`).
6. `dump_rgb_png.+x` (mutaclysm's own copy works fine pointed at any session's `PRISC_PROJECT_ROOT`
   — it's generic, reads whatever `pieces/display/rgb_frame.raw` is live) produces a real PNG proof
   without any window-capture race risk (contrast with the `dump_frame_png_op.+x` shared XWD-style
   capturer, which the house testing doc's own 2026-08-12 addendum already flags as unreliable
   when a human may be using the desktop concurrently — prefer the raw-file-reading dump method).

### 8.4 `mua_menu_input.c` ALREADY has `OPEN_BOARD_WIDGET` wired (piececraft-xyz's model)

Confirmed by grep: `ops/mua_menu_input.c` (mutaclysm's real, currently-live menu input handler,
NOT the `muta_menu_input_3d.c` board-viewer copy made earlier this session — see §8.6 dead-end)
already has a full `spawn_board_widget()`-equivalent (~line 617-740) that shells out to
`&.widgits/board-viewer/button.sh run-widget <host_project_root>` exactly like piececraft-xyz's own
menu does. **This is piececraft-xyz's "manual View Board button" model — the user does NOT want
this for muta-neo** (see below, §8.5) but it's useful as a reference for the exact spawn mechanics
(ledger-scoped process dedup via `ledger_peers.+x`, house-relative path resolution, `setsid` detach)
if a manual/optional board view is ever wanted alongside the auto-launched primary one.

### 8.5 What muta-neo actually needs (direct instruction, this session)

**NOT** piececraft-xyz's model (3D view is an optional separate widget you manually open via a menu
click). **Instead**, old `+18.0G`'s own model: the 3D view IS the primary game display, auto-opened
at launch via the standard PAL render pipeline, with interact/nav coexisting exactly like
`+18.0G`'s `[^] 1. [Control Hero]` + numbered `2.`-`10.` action menu (nav works OUTSIDE interact
mode; interact mode is a toggle for direct hero/camera control, not a gate on the whole menu).

Confirmed live via `+18.0G`'s own real behavior this session: `button.sh run` auto-spawns its own
`gl_mirror` process immediately (no menu click required) because `pal/game_module.pal` calls
`compose_frame` → `compose_rgb_frame` → `hit_frame` directly at module start, THEN loops calling
`ops/+x/game_dispatch` every tick. `game_dispatch.c` (already copied into `+18.01/ops/` per §Step 1
of this doc) is the real per-tick engine: reads+drains `interact_relay.txt`, dispatches each
buffered keycode to `move_player`/`choice`/`camera_control`, recomposes via `compose_frame` +
`compose_rgb_frame` on any keypress, and runs `end_turn`/`tick_monsters` only when the hero
actually acted (not on pure cursor/camera moves).

**The concrete port target, not yet implemented**: insert a call to `muta_render_3d.+x` into this
SAME dispatch flow (before `compose_rgb_frame`, or replacing its role) so the existing, already-
correct interact/nav/movement/turn system produces board-viewer's real voxel render instead of
`+18.0G`'s own flat rasterizer — auto-launched, not a manual "View Board" click. Concretely:
1. Either modify `game_dispatch.c` directly to `run_op("./ops/+x/muta_render_3d", NULL)` before
   `compose_rgb_frame`, or make a new `game_dispatch_3d.c` (copy of `game_dispatch.c`) — prefer
   modifying in place unless a non-3D fallback mode is wanted (open question for the user).
2. `pal/main_module.pal` (or a real `pal/game_module.pal` restored per §Step 7 of this doc) needs
   its own initial `compose_frame` / `muta_render_3d` / `compose_rgb_frame` / `hit_frame` sequence
   at module start, matching `+18.0G`'s own pattern exactly.
3. Hero position needs a real chunk-relative Z mapping — `muta_render_3d.c`'s own
   `default_current_z()` reads `hero_01/state.txt`'s `pos_z` and subtracts 1 (ground surface is one
   below the air tile the hero stands in, per that function's own header comment) — confirm
   mutaclysm's own hero state write matches this same convention before wiring, don't assume.
4. `main.chtpm`'s `${piece_methods}` substitution is the source of the numbered nav items outside
   interact — confirm this already produces an old-mutaclysm-style `[^] 1. [Control Hero]` +
   `2.`-`N.` layout via `mua_menu_input.c`'s own interact_mode-related code (referenced in the
   ORIGINAL investigation, §1 "New (+18.01) Architecture", lines 57-63 of this doc — `mua_menu_
   input.c` lines 1202-1258 already have a BUILT-IN interact toggle + arrow cursor + render-mode
   switch, this may already be closer to what's wanted than assumed — verify by testing, don't
   re-derive from scratch).

**Camera quality target**: board-viewer's own `Camera` struct/`build_camera()` (already has 3 real,
previously-fixed bugs per `au11-hq/legacy-shared-fix.md` §2.6) is the reference for "better camera
controls and modes" — NOT `+18.0G`'s own `camera_control.c`/`compose_rgb_frame.c` camera code,
which is the "buggy cam" being replaced. Do not port `+18.0G`'s own camera math into muta-neo;
port board-viewer's.

### 8.6 Dead-end this session (documented so it isn't repeated)

Early this session, before the direction above was clarified, a parallel/separate pipeline was
built: `ops/muta_render_3d.c`, `ops/muta_compose_frame_3d.c`, `ops/muta_menu_input_3d.c` (verbatim
copies of `bv_render_3d.c`/`bv_compose_frame.c`/`bv_menu_input.c`) + `pal/game_module_3d.pal` (a
NEW, from-scratch PAL loop) + `pieces/system/arrow_config.txt`/`terrain_legend.txt`/`board.txt`
(hand-written test fixtures, NOT generated by `mua_generate_chunk.c`) + `button.sh`'s
`module_path` was pointed at `game_module_3d.pal`. This produced a build that compiled clean but
rendered ONLY the outer text menu, no 3D output — because it bypassed mutaclysm's own real
`game_dispatch`/`move_player`/`choice`/`camera_control` dispatch chain entirely, reinventing input
handling from board-viewer's copy instead of reusing mutaclysm's own already-working relay/dispatch
system. **`button.sh` has been reverted** to `module_path=system/prisc+x pal/new_game_module.pal`
(the original title-screen module) — the `muta_render_3d.+x`/`muta_compose_frame_3d.+x` BINARIES
are still real, compiled, and useful (they ARE board-viewer's real renderer, just needs wiring into
the REAL dispatch chain per §8.5 above, not a bespoke replacement of it). `muta_menu_input_3d.+x`
is likely NOT needed at all — `mua_menu_input.c`'s own existing interact-mode code (§8.5 point 4)
plus `game_dispatch.c`'s own existing relay-drain loop should be reused as-is; only the RENDER call
inside that chain needs to change from `compose_rgb_frame` to `muta_render_3d`.
`pieces/system/arrow_config.txt`/`terrain_legend.txt`/`board.txt` (the hand-written fixtures) can
be deleted/ignored once `mua_generate_chunk.+x` is wired to run at real game start (it writes its
own real `board_manifest.txt` pointing at `pieces/system/chunks/`, not the flat `board.txt` these
fixtures assumed).

### 8.7 Concrete next steps (not yet done as of this addendum)

1. Modify `game_dispatch.c` (or a new variant) to call `muta_render_3d.+x` before/instead of
   `compose_rgb_frame` in its "any_key → recompose" branch.
2. Wire `mua_generate_chunk.+x` to run automatically at real game start (title → confirm & start),
   writing real chunk data + `board_manifest.txt`, replacing the hand-written `board.txt` fixture.
3. Restore/adapt `pal/game_module.pal`-equivalent (auto compose+render at module start, then loop
   `game_dispatch` every tick) as the REAL game screen's module, not `new_game_module.pal` (title)
   or the abandoned `game_module_3d.pal` (§8.6).
4. Verify `mua_menu_input.c`'s existing interact-mode code (lines ~1202-1258, referenced in §1 of
   this doc) already produces the right `[^] Control Hero`-equivalent + numbered nav-outside-
   interact menu; only add new C code if it's genuinely missing something after live testing, not
   preemptively.
5. Test the FULL flow live via `button.sh run` (real desktop window) + real relay injection into
   `interact_relay.txt` (decimal keycodes, e.g. `113`=q yaw, `114`=r pitch, `119`=w pan) + real
   `dump_rgb_png.+x` PNG dump for visual proof at each checkpoint — per the house testing doc and
   this session's own §8.3 confirmed method. Don't declare a checkpoint done from text-frame output
   alone; always get a real pixel dump.
6. Once single-chunk (0,0) 3D rendering with real z-levels is confirmed working end-to-end, revisit
   whether/how the old teleport-between-maps system (kept per direct instruction: "as far as the
   teleport system. leave it") integrates with the new per-map multi-chunk voxel model — each
   destination map needs its own real chunk data, not just a `dest_map_id` pointing at nothing.

---

## 9. SESSION ADDENDUM 2 (2026-08-17, later still) — §8.7 steps 1-3 implemented; a real, unresolved GL-window keyboard-input gap found live

### 9.1 What was implemented (Haiku-delegated, verified by re-build)

Steps 1-3 of §8.7 were delegated to a Haiku subagent (well-scoped, mechanical, matched the house's
own "delegate scoped work" standing rule) and verified after return:

1. **`ops/game_dispatch.c`** — the `any_key` recompose branch now calls `muta_render_3d.+x` between
   `compose_frame` and `compose_rgb_frame`:
   ```c
   run_op("./ops/+x/compose_frame", NULL);
   /* Call board-viewer's real voxel renderer as an overlay pass */
   run_op("./ops/+x/muta_render_3d", NULL);
   run_op("./ops/+x/compose_rgb_frame", NULL);
   ```
2. **`mua_generate_chunk.+x`** — confirmed ALREADY wired at `CONFIRM_START` inside
   `ops/mua_menu_input.c` (~lines 1280-1330): generates a seed from `time(NULL) ^ getpid()`, invokes
   `mua_generate_chunk.+x`, logs "World generated", navigates to `main.chtpm`. No change needed —
   this was already correct before this session started.
3. **`pal/game_module.pal`** — NEW file created (this project had no file by this exact name before;
   do not confuse with the abandoned `game_module_3d.pal` from §8.6, which still exists but is
   unused/dead):
   ```pal
   li x1, 0

   compose_frame
   muta_render_3d
   compose_rgb_frame
   hit_frame

   loop:
   exec ./ops/+x/game_dispatch
   sleep 16667
   j loop
   ```
4. **`pieces/chtpm/layouts/main.chtpm`** — done by the SESSION agent (not the Haiku subagent) after
   the subagent returned, since it required cross-referencing `${game_map}`/`${piece_methods}`
   compatibility with the CLASSIC `compose_frame.c` (confirmed real via grep — both substitutions
   really are written by the classic composer, not just the neo one) and comparing against
   `+18.0G`'s own real `game.chtpm` for the missing `<button onClick="INTERACT">` wiring. Changed:
   - `<module>` line: `pal/main_module.pal` → `pal/game_module.pal`
   - Added `<button label="Control Hero" onClick="INTERACT" /><br/>` (was completely absent before —
     this is the real reason `interact_relay.txt` was "dead code" per §1 of this doc's ORIGINAL
     investigation; without this button, `chtpm_parser_pal.c`'s own INTERACT-branch key-relay logic
     has nothing to focus, so no keys were ever routed into the relay file regardless of anything
     else being correct)
   - Moved `${game_map}` substitution below `${piece_methods}` (cosmetic, matches old `game.chtpm`'s
     own layout order)

   `button.sh`'s own `state.txt` `module_path=` was deliberately LEFT UNCHANGED
   (`pal/new_game_module.pal` / `active_target_id=new_game`) — the title screen still launches
   first; `main.chtpm`'s own `<module>` tag is what determines which PAL module runs once
   `CONFIRM_START` navigates there, which is the correct, minimal-diff place to make this change
   (confirmed by testing — see §9.2).

Rebuild after all changes: `build ok`, zero new errors, only pre-existing warnings.

### 9.2 Live-tested this session: title→confirm-start flow reaches `main.chtpm`+`game_module.pal` correctly

Launched via real `button.sh run` (real desktop window, confirmed visible by direct user
observation, not just process-list inference — per §8.3's own testing standard). Relay-injected
via `pieces/keyboard/history.txt` (`[TIMESTAMP] KEY_PRESSED: <decimal>` format — the SAME format
confirmed working on piececraft-xyz earlier this session, §8.3):
- Digit `50`='2' (Confirm & Start Seeded World) → selection cursor moved to option 2, confirmed via
  frame diff (`[>]` marker moved).
- `current_layout.txt` confirmed reading `pieces/chtpm/layouts/main.chtpm` — the new module/button
  wiring IS live and active for this session, not stale.

**Not yet confirmed**: whether a second Enter after the digit actually fires `CONFIRM_START` (the
injection for this was interrupted mid-session by the real bug in §9.3 below, needs re-test).

### 9.3 REAL, LIVE, UNRESOLVED BUG — the real GL/X11 window does not reliably accept direct human keyboard input for testing

**Direct user report, live, this session**: "its not allow key input from gl window. thats bad cuz
i cant test it" — the user clicked into the real popped-up game window and typed, and it did not
respond as expected.

**What's structurally CONFIRMED about `&.widgits/_shared-lib/ops/x11_mirror.c` (the real display-
mirror binary this project's `button.sh run` launches)**:
1. It IS a normal WM-managed window (`override_redirect` stays OFF, confirmed via grep + the file's
   own header comment at line ~613-615) — this is the STRUCTURALLY CORRECT pattern per this house's
   own `!.HOUSE_STDS.md` §F-19 precedent (that bug was specifically about override-redirect windows
   with a bare `XSetInputFocus` call; `x11_mirror.c` has NO `XSetInputFocus` call at all, relying
   entirely on the window manager's own normal click-to-focus — the right approach for a real,
   normal window).
2. It DOES have a real `KeyPress` handler (`XNextEvent`/`XLookupString`/`append_key()`, lines
   ~662-682) that writes to `pieces/apps/player_app/history.txt`.
3. It DOES log every real KeyPress it receives to `pieces/display/gl_key_debug.log` via
   `log_key_debug()`.
4. Its event-polling cadence is a real `select()` on the X connection fd at 30fps (`{0, 33333}`
   timeval) — not a cadence/starvation bug, this is a normal, correct pattern (matches
   `gl_mirror.c`'s own timer, per the file's own comment).

**What was found live, this session, in `gl_key_debug.log`** (real evidence, not inference): only 4
total entries ever logged for this session — `raw=13 mapped=13`, `raw=92 mapped=92`, `raw=13
mapped=13`, `raw=13 mapped=13` (three Enters + one backslash). Timestamped via file mtime at
23:18:36, which is BEFORE this session's own relay-injected test digits (sent ~23:20) — meaning
**zero digit keypresses (`1`-`9`) were ever captured by `x11_mirror.c` from real human typing this
session**, only Enter (and once backslash) registered.

**Real, honest, UNRESOLVED diagnosis** — do not treat any of the following as confirmed root cause,
they are candidate explanations only, ranked by plausibility given the evidence:
1. **Most likely**: the window didn't have real X input focus at the moment the user typed digit
   keys specifically (e.g. focus was on a different window, or focus was lost between the Enter
   presses that DID register and the digit presses that didn't — X focus is per-keystroke-moment,
   not "sticky" unless the WM enforces it). This is a testing/environment issue, not necessarily a
   code bug — same CLASS of false-positive this house's own testing doc already warns about
   (SCOPE ADDENDUM 2026-08-11's own "one confusing result on a live, shared, desktop is not proof
   of a code bug" lesson) but NOT yet confirmed to actually be the explanation here — needs a
   real, controlled retest (user clicks window title bar first to force focus, THEN types a single
   digit, THEN immediately checks `gl_key_debug.log` for a new line) before either "it's a testing
   artifact" or "it's a real code bug" can be asserted as fact.
2. **Possible**: some window manager global keybinding is intercepting digit keys before X ever
   delivers a `KeyPress` event to this window at all — would explain digits never even reaching
   `x11_mirror.c`'s own event loop (not visible from inside this file's own code, needs checking the
   window manager's own config, out of scope for a code-level fix if true).
3. **Possible but less likely given the evidence**: something else entirely intercepts/consumes the
   digit `KeyPress` events at the X-server level (a global hotkey daemon, a desktop shortcut, etc.)
   — same practical effect as #2, different cause.
4. **Ruled out (not the cause)**: NOT a polling-cadence bug (30fps `select()` loop is normal and
   correct). NOT an override-redirect focus bug (window is normal WM-managed, no `XSetInputFocus`
   misuse). NOT a missing KeyPress handler (the handler is real and does log/write correctly for
   the keys it DID receive).

**Real, practical implication for testing going forward, until this is diagnosed further**: DO NOT
rely on directly typing into the real GL window as the test method for muta-neo verification —
per this session's own confirmed-working alternative (§8.3), **use file-relay injection instead**
(`pieces/keyboard/history.txt` for outer CHTPM nav, `pieces/apps/player_app/interact_relay.txt` for
in-game/camera/interact keys once the INTERACT button is focused) — this method was verified
working end-to-end on piececraft-xyz this session (real yaw-angle change, real RGB checksum change)
and does not depend on real X focus at all (it's a file write, not a synthetic input event). The
GL window remains valuable as the REAL VISUAL PROOF surface (screenshot/PNG-dump target) even while
its own direct-typing input path is unresolved — these are two separate concerns (rendering output
vs. keyboard input capture), don't conflate a rendering-verification blocker with an input-capture
bug or vice versa.

**Concrete next diagnostic step, not yet done**: relaunch fresh, have the user click the window's
own title bar first (forces WM focus, generates zero KeyPress events itself so it's a clean control
step), then type ONE single digit key, then immediately `tail -f` or re-read `gl_key_debug.log` to
see in real time whether that specific digit ever gets logged at all. If it never appears in the
log, the problem is upstream of `x11_mirror.c`'s own code (WM/X-server-level interception, point #2
or #3 above) — if it DOES appear in the log but the on-screen frame still doesn't update, the
problem is downstream (in `append_key()`'s own write target, or in the PAL module's own
`read_history` consumption of that file) and needs a different investigation entirely.

---

## 10. SESSION ADDENDUM 3 (2026-08-17, final this session) — the REAL 3D render pipeline, fully traced end-to-end, map now confirmed rendering; keyboard input is a SEPARATE, still-open issue

This section documents the complete, real chain of bugs found and fixed to get the voxel 3D view
actually appearing on screen, plus the new user-requested UX changes (skip Confirm & Start, real
action menu instead). **Read this section fully before touching rendering/input code again** — each
bug below was found by direct live testing (RGB dumps, not assumption), and re-breaking any one
step will silently reintroduce a blank screen with no obvious error.

### 10.1 New UX requirements implemented this pass (direct user instruction)

1. **No manual "Confirm & Start" step** — world generates automatically on first-ever launch.
2. **Real action menu shows immediately** (`[^] 1. Control Hero`, `End Turn`, `Eat`, `Pick Up`,
   `Drop`) instead of the title-screen's `Confirm & Start` options.
3. **"View Board"/"View Editor" menu items removed** — the 3D view is the PRIMARY display,
   auto-shown, not something opened via a separate menu click (per §8.5's own already-recorded
   direction).
4. **Interact mode (`Control Hero`) requires Enter to activate** while focused, and **ESC exits it**
   back to the menu — this is real, existing, unmodified `chtpm_parser_pal.c` behavior (confirmed
   by reading the code, not assumed): the `INTERACT` branch is inside the same `if (key == ENTER)`
   block as `ACTIVATE`/href navigation (~line 3444 `// Execute accumulated value on Enter` through
   ~line 3537), so simply having "Control Hero" as the default-focused menu item does NOT
   auto-activate interact mode — a real Enter press is required. ESC-exits-INTERACT is separately
   confirmed real at ~line 3748 (pre-existing, not written this session).
5. **Once in interact mode, menu nav items should NOT be reachable, only camera/movement** — this
   is the existing, standard `chtpm_parser_pal.c` INTERACT-branch contract (ALL non-ESC keys route
   into `interact_relay.txt` while the INTERACT element is `active_index`, per the ORIGINAL
   investigation's own §1 finding, line ~3234-ish region) — not something this session added, but
   confirmed still the real, intended design.

### 10.2 Files changed this pass, in the order the bugs were found

**`button.sh`** (multiple real, load-bearing additions — read the inline comments in the file
itself for full detail, this list is a summary):
1. Auto-invoke `mua_generate_chunk.+x` on first-ever launch (`if [ ! -d chunks/chunk_0_0 ]` guard,
   so world persists across relaunches and is never regenerated once it exists).
2. Symlink `board_manifest.txt`, `chunks/`, `hero_01/`, `world_01/`, `xelector_01/` from `SCRIPT_DIR`
   (persistent, real project root) into each fresh, disposable `SESSION_DIR` — **BUG #1**: these
   were completely missing before this pass. Without them, `muta_render_3d.+x` (run from inside the
   session dir, which is where `PRISC_PROJECT_ROOT` actually points during real gameplay) found NO
   `board_manifest.txt` at all and silently wrote nothing.
3. Write `pieces/system/bv_state.txt` with `focused_project_root=$SESSION_DIR` — **BUG #2**:
   `muta_render_3d.c` (verbatim board-viewer code) is a "focus any host project" widget by design;
   its own `main()` reads `bv_state.txt`'s `focused_project_root` key and returns immediately if
   empty (`if (!focused_project_root[0]) return 0;`, ~line 1385). Without this file, NOTHING it does
   downstream ever runs, regardless of any other fix.
4. Append `render_mode=1`/`camera_mode=2`/`cam_*` fields onto `hero_01/state.txt` right after
   `mua_generate_chunk.+x` runs — **BUG #3 (only half-real, see BUG #4)**: `mua_generate_chunk.c`
   writes only `entity_type`/`hp`/`pos_x/y/z`/`owner_id`/`chunk_x/y`, no camera/render fields at
   all, so `render_mode` always defaulted to 0 (2D) with no way to reach 3D.
5. **BUG #4, the REAL blocker that made BUG #3's fix look like it wasn't working**:
   `ops/compose_frame.c` does NOT read `pieces/hero_01/state.txt` at all — it hardcodes
   `pieces/world_01/map_start/hero/state.txt` (5 separate call sites, all from `+18.0G`'s original
   2D-map convention, never updated when the voxel generator was added) as its `hero_path`. This
   file never existed in `+18.01`, so EVERY `read_kv_int()` call against it (hp, render_mode,
   interact_mode, everything) silently returned its hardcoded default — meaning BUG #3's fix (which
   only wrote to `hero_01/state.txt`) was writing to a file `compose_frame.c` never even opened.
   **Fix applied (minimal-risk, zero C code changes)**: symlink the OLD path AT the NEW file —
   `ln -sf "$SCRIPT_DIR/pieces/hero_01/state.txt" "$SCRIPT_DIR/pieces/world_01/map_start/hero/state.txt"`
   (plus `mkdir -p .../hero/inventory` since `compose_frame.c` also reads an inventory dir at that
   same old location) — both paths now resolve to the identical real file, no compose_frame.c edits
   needed. **This was the fix that got the map to actually appear on screen**, confirmed by direct
   user observation ("ok i see map").

**`ops/game_dispatch.c`**: (Haiku-delegated, then corrected this pass)
- Original Haiku-delegated change added `muta_render_3d` between `compose_frame` and
  `compose_rgb_frame`. **BUG #5**: classic `compose_rgb_frame.c` is a COMPLETELY SEPARATE, OLD
  rendering mechanism (direct flat-rasterizer RGB generation from `map.txt`-format data) with ZERO
  knowledge of `rgb_frame_3d_overlay.raw` or the MAP3D_MARKER convention — calling it after
  `muta_render_3d` would silently clobber/ignore whatever was just written. **The REAL architecture**
  (confirmed by reading `ops/muta_compose_frame_3d.c`'s own header comments, board-viewer's real
  composer): `compose_frame.c` writes a single `0x01` (SOH) marker byte into `current_frame.txt`
  when `render_mode==1` (this is `compose_frame.c`'s OWN native, pre-existing feature —
  board-viewer's `bv_compose_frame.c` PORTED this FROM mutaclysm, not the other way around, per that
  file's own comment: "exact port of mutaclysm's own ops/compose_frame.c:1088"). A SEPARATE,
  PERSISTENT daemon (`system/chtpm_rgb_render`, already built by `build.sh`, already launched by
  `button.sh run`'s own GL-mirror section) watches `current_frame.txt` for that marker and
  composites `rgb_frame_3d_overlay.raw` into the real RGB output itself. **Fix**: removed the
  `compose_rgb_frame` call from `game_dispatch.c`'s `any_key` branch entirely — the daemon does this
  job continuously; classic `compose_rgb_frame.c` was never meant to run at all when this scheme
  is in effect. Also applied the corresponding revert in `pal/game_module.pal`'s own startup
  sequence (removed the same erroneous `compose_rgb_frame` opcode line).

**`default_op.txt`**: **BUG #6** — this project's own PAL-VM (`system/prisc+x.c`) does NOT hardcode
opcodes like `compose_frame`/`compose_rgb_frame`/`muta_render_3d` in its parser at all; they're
DATA-DRIVEN, looked up in `default_op.txt` (format: `name type handler {description}`) at runtime.
`+18.01`'s own `default_op.txt` only ever registered `mua_menu_input`/`mua_compose_frame` (the neo
ops) — `compose_frame`/`compose_rgb_frame`/`muta_render_3d` were NOT registered at all, meaning any
PAL script bareword-calling them (including the Haiku-delegated `pal/game_module.pal`) silently
no-op'd on those lines (only `hit_frame` is a true hardcoded VM opcode, everything else needs a
`default_op.txt` entry or an explicit `exec ./ops/+x/<name>` line). **Fix**: added
`compose_frame`/`compose_rgb_frame`/`muta_render_3d` entries pointing at their real `ops/+x/*.+x`
binaries (matching `+18.0G`'s own `default_op.txt` convention for the first two).

**`pal/game_module.pal`** (new file, corrected this pass): final real content —
```pal
li x1, 0

compose_frame
muta_render_3d
hit_frame

loop:
exec ./ops/+x/game_dispatch
sleep 16667
j loop
```
(no `compose_rgb_frame` — see BUG #5 above for why).

**`pieces/chtpm/layouts/main.chtpm`**: `<module>` switched to `pal/game_module.pal`; added the
missing `<button label="Control Hero" onClick="INTERACT" />` (this is the real reason
`interact_relay.txt` was "dead code" in the ORIGINAL investigation — without a real INTERACT-typed
element in the layout, `chtpm_parser_pal.c`'s own key-relay branch has nothing to ever focus/engage).

**`projects/mutaclysm/pieces/main/piece.pdl`**: removed the `View Board`/`View Editor` METHOD rows
(§10.1 point 3).

**`button.sh`'s own initial `pieces/apps/player_app/state.txt`**: `active_target_id` changed from
`new_game` to `main` — **BUG #7**: `main.chtpm`'s own `${piece_methods}` substitution resolves
against `active_target_id`, which was still `new_game` even once the LAYOUT itself was `main.chtpm`
(the layout is forced via `PAL_LAYOUT` env var, independently of `active_target_id` - these are two
genuinely separate mechanisms that were left disagreeing). This is why the menu kept showing
`Confirm & Start` options (from `projects/mutaclysm/pieces/new_game/piece.pdl`) even after
`main.chtpm` was already the active layout.

### 10.3 Confirmed working, live, this session (direct user observation: "ok i see map")

The full chain — auto-generate world → symlink persistent world data into session →
`focused_project_root` self-pointer → `render_mode=1` reaching `compose_frame.c` via the path-alias
symlink → MAP3D_MARKER byte written → `chtpm_rgb_render` daemon composites the overlay → real voxel
3D view appears in the GL window — is now confirmed working end-to-end.

### 10.4 STILL OPEN, NOT YET DIAGNOSED — keyboard input stopped responding after this pass's changes

**Direct, live user report, immediately after confirming the map renders**: "kbd input no longer
works (does it think were in interact mode? interact mode isnt yet 'active' '^'". This is a
DIFFERENT, NEW symptom from §9.3's earlier GL-window-focus gap (that one had NO keys registering at
all in `gl_key_debug.log`; this session's later report is specifically after the render pipeline
changes above, and needs to be re-diagnosed fresh, not assumed to be the same root cause).

**What's already ruled out by reading the code (§10.1 point 4 above)**: `chtpm_parser_pal.c`'s own
INTERACT-activation branch is real and gated correctly behind an Enter press — simply having
"Control Hero" as the default-focused element does NOT auto-activate interact mode or swallow other
keys. If input genuinely isn't working, the cause is elsewhere — candidates, NOT yet confirmed:
1. A real regression from one of this pass's own changes (most suspect: the `main.chtpm` edit
   adding the `<button onClick="INTERACT">` — verify `initialize_focus()`/`is_navigable()` still
   correctly treats this as just ONE navigable item among the numbered menu, not something that
   captures focus specially just by being present in the layout).
2. The `active_target_id=main` change (§10.2, BUG #7) interacting badly with something else that
   expected `new_game` — check `mua_menu_input.c`'s own CONFIRM_START-adjacent code for any
   remaining hardcoded assumption about `active_target_id`.
3. A real GL-window-focus issue (§9.3's original finding may still be independently true and
   compounding) — re-verify with the same `gl_key_debug.log` live-tail method before assuming a
   code-level regression.

**Concrete next step, NOT yet done**: same procedure as §9.3's own "concrete next diagnostic step" —
click the window's title bar first (forces WM focus), send ONE real keypress, immediately check
whether it appears in `gl_key_debug.log` AND whether `current_layout.txt`/`current_frame.txt`
change. This distinguishes "key never reached the window" from "key reached the window but the
app's own logic doesn't respond to it" — two genuinely different bugs needing different fixes. Do
NOT guess further without this live evidence — this session already produced one long, costly
misdiagnosis chain (§10.2's own BUG #1 through #7) precisely by fixing one plausible-looking gap at
a time without a single, clear reproduction test at each step; the keyboard issue deserves the same
rigor `dump_rgb_png.+x` gave the rendering bugs, not more guessing.

**Direct user caution, worth repeating verbatim for whoever reads this next**: "document all this
information... explain in documentation as well i will hand off if u keep messing up" — this
section exists specifically so a fresh agent (or the same one, next session) does not have to
re-derive any of §10.1-10.3 from scratch, and so the keyboard issue gets a clean, evidence-first
diagnosis rather than another round of speculative fixes.

---

*Report generated: 2026-08-17*
*Comparing: `+18.01` (broken neo) vs `+18.0G` (working classic)*
*Fix plan recorded: 2026-08-17 22:34 PDT*
*Direction changed + investigated live (voxel 3D w/ z-levels, board-viewer render engine): 2026-08-17 23:15 PDT*
*§8.7 steps 1-3 implemented (Haiku-delegated) + main.chtpm wired + real GL-window keyboard-input gap found live, documented, unresolved: 2026-08-17 23:25 PDT*
*7 real bugs found+fixed end-to-end to get the voxel 3D map actually rendering (confirmed live by direct user observation); keyboard input regressed/still-open, documented for careful live re-diagnosis: 2026-08-17 23:40 PDT*
*§8.7 steps 1-3 implemented (Haiku-delegated) + main.chtpm wired + real GL-window keyboard-input gap found live, documented, unresolved: 2026-08-17 23:25 PDT*
