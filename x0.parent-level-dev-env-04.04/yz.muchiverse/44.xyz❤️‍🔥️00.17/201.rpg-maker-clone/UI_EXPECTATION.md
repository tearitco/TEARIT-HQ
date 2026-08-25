# RPG Maker clone — UI expectation vs implementation

## Expected (your screenshot — RPG Maker MZ)

Single **map editor page** containing:

| Region | Content |
|--------|---------|
| Top | Menu bar (File Edit Mode Draw Layer Scale Tools Game Help) |
| Top | Tool icons (pencil, layers, zoom, play) |
| **Left** | **Tileset picker** with tabs **A B C D R** + tile grid |
| **Bottom-left** | **Map tree** (World map / Factory 1 / …) |
| **Center** | **Map canvas** with painted tiles + events |
| Bottom | Status (map name, size, zoom) |

Event Editor / Database are **deeper modes**, not the only window.

Reference image: `REFERENCE_rpg_maker_mz_expected.jpg` (user-supplied)

## Implemented (v4)

- **Default mode = Map Editor** one page: tileset A–R, map tree, canvas, toolbar, menu strip
- **Procedural tileset** factory-style: metal plates, server racks (LED blink), pipes, hazard rails, water shimmer, doors/lights
- **Checkerboard** under palette cells (MZ transparent look)
- Tools: Pencil / Rect / Fill / Erase (toolbar icons + P/R/F/E)
- Layers: Ground / Objects (G/O); object glyphs C/K/P/I/L
- Map tree switches real maps (`map_start`, `factory_2`)
- Zoom 1x/2x (`[` `]` or toolbar)
- Grid toggle (H / toolbar), event diamond markers (MZ purple)
- Middle-mouse pan; WASD camera; arrows cursor (auto-scroll into view)
- Click event once = select; click again = Event Editor
- **F2** Event Editor · **F3** Play · **F4** Database
- Play: camera follow, object layer, touch triggers, **Transfer Player** between maps
- Demo: Factory 1 with guard / crystal / warp pad → Factory 2 return pad

## Still short of real MZ

- No PNG tileset atlas / 48×48 autotile bitmasks
- No full character walk sheets (player is still a blue “P” box)
- Database is template lists, not full RMMV tables
- Event command insert is still limited (type-cycle / pal lines)
- Toolbar is glyph squares, not full MZ icon set

## Run

```sh
cd 201.rpg-maker-clone
sh button.sh compile
sh button.sh run
```
