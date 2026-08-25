# 201.rpg-maker-clone — MZ-style one-page map editor + play

Target layout matches RPG Maker MZ (tileset + map tree + canvas on one page).
See `REFERENCE_rpg_maker_mz_expected.jpg` and `UI_EXPECTATION.md`.

## Run
```sh
sh button.sh compile
sh button.sh run
```

## Map Editor (default)
| Input | Action |
|-------|--------|
| Click tileset A–R | Select tile (tabs bottom-left; keys 1–5) |
| Click/drag map | Paint (active tool) |
| Toolbar `/ # F X` | Pencil / Rect / Fill / Erase (or P/R/F/E) |
| G / O | Ground / Objects layer |
| Map tree click | Switch map (`map_start`, `factory_2`) |
| Click event diamond | Select; click again → Event Editor |
| N | New event at cursor |
| S | Save |
| [ ] | Zoom 1x / 2x |
| H | Toggle grid |
| MMB drag | Pan camera |
| WASD / Z | Pan camera |
| Arrows | Move cursor |
| F2 / F3 / F4 | Event / Play / Database |
| Q | Quit (auto-saves if dirty) |

## Play
Arrows move, Space/Enter on events. Crystal unlocks door; warp `>` → Factory 2; return pad back.
Esc → map editor.

## Data
`projects/demo/maps/<id>/map.txt` + `map_obj.txt` + `events/ev_x_y/`
