# Piececraft board + CDDA/Mineclonia maps + events — 2026-09-08 test

Local run notes. Not a design doc.

## What was added this pass

- Missing event commands landed in `#.ref/menu/event_commands.registry.pdl` + `mr_world.+x` (self switch, timer, fade/tint/flash/shake, transfer, scroll, move route, shop, battle, play SE, menu/save/encounter flags). **State files only** — no camera, no battle UI, no shop UI, no fade blit.
- Sample maps: `@.apps/piececraft-hq/pieces/system/maps/{mineclonia_sample,cdda_sample}/` (`map.txt` + `events.pdl`).
- Board **File** dropdown now: Open File Explorer (`&.widgits/file-explorer`, the X11 file-hq), load `mineclonia_sample`, load `cdda_sample`, default-legacy. Load copies `map.txt` onto `chunks/chunk_0_0/chunk_0_0_z0.txt` and writes `board_config.txt`.
- `pchq_place_cell.sh` stamps a glyph into that chunk (picker drag-drop is a separate path).

## Test results (this session)

| Step | Result |
|---|---|
| Build `mr_world.+x` | see build log; smoke writes `self_switches.txt` / `map_state.pdl` |
| Build `pchq_board_projector.+x` | compile only |
| File → Open File Explorer | **wired** in `pchq_board_action.sh file-hq` → `file-explorer/button.sh`. Not proven live unless a board window is already up. |
| File → mineclonia_sample / cdda_sample | **file copy works** without the 3D viewer (`cp map.txt` + `board_config.txt`). Board reload still needs a live board-viewer session (`append_key 54`). |
| Drop tiles from palettes picker onto the 3D board | **does not work yet.** Palettes `arm-pc` / `arm-cdda` write `state/pc_brush.txt` / `cdda_brush.txt` only. No Xdnd path into pchq-board. Mutaclysm-neo drag-drop harness (`101.drag-drop-test=ON`) targets `mutaclsym RGB mirror` + egg windows, not khtpm palettes → piececraft canvas. Use `pchq_place_cell.sh` as the file-level stand-in. |
| Edit event from inside the board | **partial.** `open-events` can launch events-hq at a sample `event.ir.pdl`. There is no click-on-voxel → events-hq hit test on the canvas. |
| Desk persistence of placed tiles | still the 2026-08-29 gap (`#.desktop/tiles/` vs `desks/*.pdl`). |
| Transfer Player / Shop / Battle | commands compile and write kv; **do not** change the 3D camera, open a shop, or start a fight. |

## How to load a sample without the full game

```
HOUSE=.../44.xyz.01.00
sh "$HOUSE/@.apps/piececraft-hq/ops/pchq_board_action.sh" "" load-map mineclonia_sample
cat "$HOUSE/@.apps/piececraft-hq/pieces/system/board_config.txt"
head "$HOUSE/@.apps/piececraft-hq/pieces/system/chunks/chunk_0_0/chunk_0_0_z0.txt"
```

Then View Board from piececraft-hq so board-viewer rereads the chunk.

## Event smoke

```
TMP=/tmp/mr-world-smoke
mkdir -p "$TMP"
$HOUSE/&.widgits/events-hq/ops/+x/mr_world.+x "$TMP" self_switch A 1
$HOUSE/&.widgits/events-hq/ops/+x/mr_world.+x "$TMP" transfer mineclonia_sample 8,8
cat "$TMP/self_switches.txt" "$TMP/map_state.pdl"
```
