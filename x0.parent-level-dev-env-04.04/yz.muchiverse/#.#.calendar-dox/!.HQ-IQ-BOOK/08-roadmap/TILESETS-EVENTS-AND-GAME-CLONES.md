# Tilesets, event guides, and the games they feed

**Written 2026-09-08.** This is the find-it-later map for palettes,
outside-zip assets, event-command registry, per-tile event guides, and
how those pieces attach to the clones we actually want (Minecraft /
Mineclonia, CDDA, Civ, GTA-shaped, RPG Maker). Pointers, not dumps.

If a path moves, update the matching `*.pdl` under
`#.#.calendar-dox/1.^V-hq/` first, then this file.

House root in the commands below is `44.xyz.01.00`.

---

## 1. Palettes (the picker UI)

Launcher: `&.widgits/palettes/button-pal.sh <category> "$HOUSE"`
(category **first**). Categories live in `&.widgits/palettes/pallets.pdl`.

| pallets.pdl key | window | DIR tab | TILESET tab | grid | launch |
|---|---|---|---|---|---|
| `rmmv` | `palettes-rmmv.xhtpm` | asset folder (`tilesets`, `characters`, …) | sheet (`SF_Inside`, `Actor1`, …) | 48px crops | `button-pal.sh rmmv "$HOUSE"` |
| `piececraft` | `palettes-piececraft.xhtpm` | Mineclonia pack (`ITEMS`, `ENTITIES`, …) | Luanti mod (`mcl_core`, `mcl_wool`, …) | 16px PNG thumbs | `button-pal.sh piececraft "$HOUSE"` |
| `cdda` | `palettes-cdda.xhtpm` | UltiCa `pngs_*` sheet | first-level folder (`terrain`, `furniture`, …) | PNG thumbs | `button-pal.sh cdda "$HOUSE"` |
| `emojis` | `palettes-emojis.xhtpm` | Unicode group | subgroup | fully-qualified glyphs (no skin-tone variants) | `button-pal.sh emojis "$HOUSE"` |
| `elements` | `palettes-elements.xhtpm` | — | — | chemistry swatches | `button-pal.sh elements "$HOUSE"` |
| `df` / `kenney` / `paint` / `generate` | stub | — | — | not built | stub window |
| `tiled` / `ohrrpgce` / `my-palettes` / `tile-editor` | **not built** | see design | see design | — | `design-docs/MY-PALETTES-TILED-OHR-TILE-EDITOR-DESIGN.md` |

Chooser CSS families (same for rmmv / piececraft / cdda / emojis):
`.pal-dir` / `.pal-tileset` / `.swatch`. Active files:
`<cat>_active.txt` + `<cat>_options.txt` next to the palettes widget.
Generated thumbs: `&.widgits/palettes/sprites/{rmmv,pc,cdda,emoji}/`
— **do not gitadd**.

Manager: `*.monads/*.livedesk-taskbar/ops/palettes_manager.c`.
Projector: `&.widgits/palettes/ops/palettes_projector.c` (chooser-grid
for rmmv|piececraft|cdda|emojis).

Older notes: `09-appendix/PROGRESS-palettes-xhtpm.md` (stale on
“rmmv not ported” — rmmv **is** ported; piececraft/cdda/emojis groups
landed 2026-09-08). Design: `08-roadmap/design-docs/TILE-SYSTEM-DESIGN.md`,
`RMMV-IMG-DIR-TABS-PLAN.md`.

---

## 2. Asset roots (outside the house zip)

Same rule as RMMV: PNGs do not live inside `44.xyz.01.00`. Pointers:

| What | PDL (`#.#.calendar-dox/1.^V-hq/`) | On-disk root |
|---|---|---|
| RPG Maker MV img | `RMMV-ASSET-SOURCE-LOCATION.pdl` `img_root` | `NNEST-11.17/#.NNEST_ASSETS/rmmv-www-img` (USB-sourced copy) |
| Mineclonia / MC-like | `MINECLONIA-ASSET-SOURCE-LOCATION.pdl` `img_root` | `x0.parent-level-dev-env-04.04/#.NNEST_ASSETS/mineclonia/mods` (Codeberg clone, Pixel Perfection, **not** Mojang) |
| CDDA UltiCa | `CDDA-ASSET-SOURCE-LOCATION.pdl` `img_root` | `x0.parent-level-dev-env-04.04/#.NNEST_ASSETS/cdda-tilesets/gfx/UltimateCataclysm` (sparse `I-am-Erk/CDDA-Tilesets`) |
| Unicode emoji 17.0 | `UNICODE-EMOJI-SOURCE-LOCATION.pdl` `source_file` | `x0.parent-level-dev-env-04.04/#.NNEST_ASSETS/unicode-emoji/emoji-test-17.0.txt` (official UTS #51; 14.0 copy is stale) |
| Video demo clip | `VIDEO-ASSET-SOURCE-LOCATION.pdl` `sample_mp4` | `NNEST-12.00/#.NNEST_ASSETS/video/sample-10s-vp9.mp4` (moved 2026-09-08 off `#.media-library`) |

Do not commit clones or generated sprite.csv. License: Mineclonia LEGAL.md;
CDDA CC-BY-SA 3.0 in the tileset repo.

---

## 3. Events — registry, runtime, per-tile guides

**Command list (source of truth):**
`44.xyz.01.00/#.ref/menu/event_commands.registry.pdl`

Do **not** trust `event.commands.remaining.txt` as current — it still
lists Show Choices / Control Switch as missing; those are live.

**Authoring files:**
- `event.ir.pdl` — `NODE | id=N type=<COMMAND> | k=v` (see `common_events/greet_player/`)
- `event.pal` — compiled; never hand-write
- Compiler: `&.widgits/events-hq/ops/khtpm_events_hq_manager.c` `compile_page()`
- Ops: `&.widgits/events-hq/ops/mr_*.+x` (`mr_show_text`, `mr_change_gold`, `mr_world`, …)

**Per-tile event guides (2026-09-08):**
`44.xyz.01.00/#.ref/menu/event-guides/`

| File | Contents |
|---|---|
| `SCHEMA.pdl` | TILE field meanings (tex, trigger, db, cmds, need) |
| `INDEX.pdl` | sheet list + honest gaps |
| `mineclonia/mcl_core.pdl` | dirt/stone/ores/water/lava/ladder |
| `mineclonia/interact.pdl` | chest, door, furnace, bed, tnt, sign, craft, hopper |
| `cdda/terrain.pdl` | doors, stairs, walls, floor |
| `cdda/furniture.pdl` | bed, crate, toilet, desk |
| `cdda/traps.pdl` | beartrap, landmine, tripwire, portal |
| `cdda/items.pdl` | sample pickups (aid kit, weapons) |
| `examples/mcl_chest/pages/page_1/event.ir.pdl` | worked chest page |
| `examples/cdda_door/pages/page_1/event.ir.pdl` | worked door page |

A TILE row is an RMMV map event: **graphic = tex**, **page trigger**,
**command list**. Guides are authoring data — a picker tile does not
auto-run until an `event_pkg` is attached.

**2026-09-08 registry adds** (`mr_world.+x`): control_self_switch,
control_timer, fadeout/fadein/tint/flash/shake, transfer_player,
scroll_map, set_move_route, shop_processing, battle_processing, play_se,
change_menu/save/encounter access. These **write kv files**
(`map_state.pdl`, `screen_state.pdl`, `shop_state.pdl`, `battle_state.pdl`,
`self_switches.txt`, `timer.txt`). They do **not** yet move the 3D
camera, open a shop UI, or start a fight.

Older architecture: `08-roadmap/design-docs/EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`,
`EVENTS_RUNTIME.md`, `sep-1-events-SOS.md` (Tier list is partly
superseded by `mr_world`).

---

## 4. Sample maps in piececraft-hq

`@.apps/piececraft-hq/pieces/system/maps/`

| id | files | load |
|---|---|---|
| `mineclonia_sample` | `map.txt` (16×16 glyphs) + `events.pdl` | board File → mineclonia_sample, or `pchq_board_action.sh "" load-map mineclonia_sample` |
| `cdda_sample` | same | `load-map cdda_sample` |

Load copies `map.txt` onto `pieces/system/chunks/chunk_0_0/chunk_0_0_z0.txt`
and writes `board_config.txt`. Glyphs: `terrain_legend.txt`.
Place stand-in (no drag-drop yet): `ops/pchq_place_cell.sh "$HOUSE" <x> <y> '<glyph>'`.

Board File also launches `&.widgits/file-explorer` (X11 file-hq).
Picking a file there does **not** auto-load a map.

Test log: `#.#.calendar-dox/1.^V-hq/PIECECRAFT-CDDA-MINECLONIA-BOARD-TEST.md`.

---

## 5. How this maps onto the games we want

| Game we want | Tileset / picker | Events / db | Board / world | Still blocking play |
|---|---|---|---|---|
| **RPG Maker** | `rmmv` picker + `RMMV-ASSET-SOURCE-LOCATION.pdl` | registry + events-hq + Common Events tab | desktop tiles / mutaclysm import | db-hq Actors/Items/… tabs placeholder; Transfer Player state-only; desk persistence gap (`TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt`) |
| **Minecraft clone** | `piececraft` picker + Mineclonia mods | `event-guides/mineclonia/*` (chest/door/furnace/bed/tnt) | piececraft-hq chunks + `mineclonia_sample` | no voxel hit-test → events-hq; no gravity/move-route; shop/craft UI missing |
| **CDDA clone** | `cdda` picker + UltiCa | `event-guides/cdda/*` (doors, traps, loot, sleep) | `cdda_sample` | battle_processing is kv only; no map transfer; monsters not on guide sheets yet |
| **Civ clone** | palettes stub; civ-txt / piececraft terrain glyphs | gold/switch/variable already exist | civ-txt + board-viewer | not wired to these pickers |
| **GTA-shaped** | none yet (cars/peds would be a new DIR/TILESET pack) | movement + transfer + play_se stubs | no city map | do not start without a tileset PDL |

db-hq: `&.hq-apps/db-hq-pal/dashboard.xhtpm` has all 15 RMMV tabs;
**only Common Events is an editable panel.** Ladder:
`44.xyz.01.00/#.ref/menu/db-tabs-remaining.txt`.

---

## 6. Known gaps (do not paper over)

1. Palettes `arm-pc` / `arm-cdda` write brush files; **no drop onto pchq canvas**. Mutaclysm Xdnd harness (`101.drag-drop-test=ON🀄️`) is a different window.
2. Click-voxel → edit event: **not built**.
3. Placed `#.desktop/tiles/` packages are **not** `DESK` rows — vanish on reset.
4. Transfer / shop / battle / fade: **state files**, not gameplay.
5. DF / Kenney palettes: stub.
6. `PROGRESS-palettes-xhtpm.md` header is stale (2026-09-03).

---

## 7. Grep seeds for a later agent

```
event-guides
MINECLONIA-ASSET-SOURCE-LOCATION
CDDA-ASSET-SOURCE-LOCATION
RMMV-ASSET-SOURCE-LOCATION
UNICODE-EMOJI-SOURCE-LOCATION
VIDEO-ASSET-SOURCE-LOCATION
event_commands.registry.pdl
button-pal.sh piececraft
maps/mineclonia_sample
mr_world.c
pchq_board_action.sh
```
