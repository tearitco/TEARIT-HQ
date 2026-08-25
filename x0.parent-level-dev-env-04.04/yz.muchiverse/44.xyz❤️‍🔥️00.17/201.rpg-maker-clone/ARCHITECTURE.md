# ARCHITECTURE — 201.rpg-maker-clone

## Intent

One freeglut binary that is both **maker** (map + event editors) and **player**.  
Data shapes intentionally mirror house projects (mutaclysm / event-editor packages) so later wiring is possible without a format rewrite.

**Not** the CHTPM → rgb → `gl_mirror` path. This is the pure GLUT product path (same quality bar as `&.widgits/event-editor/gl_mock/ee_gl_mock.c`).

## Binary modules

| File | Role |
|------|------|
| `src/main.c` | Modes, input, play interpreter, all screens |
| `src/draw.c` | Panels, text, tile colors (chrome helpers) |
| `src/project.c` | Load/save `project.pdl`, `map.txt`, events, `switches.pdl` |
| `src/rpg.h` | Shared types / API |

Build: `gcc … main.c draw.c project.c -lGL -lGLU -lglut`

## Runtime modes

```text
MODE_TITLE ──► MODE_MAP ──► MODE_EVENT
     │              │            │
     └──────── MODE_PLAY ◄───────┘
```

- **Title**: continuous nav 1..7 (New/Load/Map/Event/Play/Save/Quit).
- **Map**: 20×15 grid, palette brush, event markers, tool rail 1..10.
- **Event**: RMMV chrome — methods 1–8, pages 9–12, fields 13–16, contents 17–28, footer 29–34.
- **Play**: walk + collision + action trigger + message box.

Mode switches: `F1`/`F2`/`F3`, Title menu, Map tools, Event footer Play.

## Data model

```text
Project
  root, name, start_map, start_x/y
  Map.cells[H][W]          # single-byte terrain
  Event[MAX]               # used, x,y, name, trigger, sprite, cmds[]
  Switch[MAX]              # name → 0/1 (int)
```

### Files (house-aligned)

| Path | Format |
|------|--------|
| `project.pdl` | PDL table: name, start_map, start_x/y |
| `switches.pdl` | `SWITCH \| key \| value` rows |
| `maps/<id>/map.txt` | `MAP_H` lines × `MAP_W` chars |
| `maps/<id>/events/ev_X_Y/state.txt` | `key=value` (name, trigger, x, y, sprite) |
| `…/event.pal` | one command per line (interpreter source) |
| `…/event.ir.pdl` | optional metadata stub |

Events are discovered by scanning `events/ev_*` directories.

### Command IR

```c
enum CmdType {
  CMD_SHOW_TEXT, CMD_SET_SWITCH, CMD_IF_SWITCH,
  CMD_END, CMD_TRANSFER, CMD_RET, CMD_COMMENT, CMD_EMPTY
};
```

`event.pal` is the dual of the RMMV “Commands” list; Scratch view is the same array, alternate labels (`{ show_text … }`).

## Play interpreter

```text
on Space/Enter (action):
  event = tile in facing direction, else current tile
  if trigger==action → run

on step (touch):
  if event on tile && trigger==touch → run

run:
  pc = 0
  loop:
    show_text → message box; wait key; pc++
    set_switch → memory + write switches.pdl; pc++
    if_switch → if false, skip to matching end; else pc++
    end → pc++
    transfer → set pos / optional map_id (events reload STUB)
    ret → stop
```

Message box is `PLAY_MSG` substate; Space/Enter/Esc advances then resumes the script.

### Door rule

Terrain `+` is walkable only when switch `door_open != 0`. Walls `#` and water `~` always solid.

## UI / nav model

Same continuous digit-accum model as CHTPM / `ee_gl_mock`:

- Global focus index `0..N-1` displayed as **#1..N** (never resets per panel).
- Multi-digit: type `1` then `7` → jump #17.
- Arrows move focus; Left/Right jump zones in Event Editor.
- Tab toggles Commands | Scratch.

Timer: `glutTimerFunc(16, …)` for blink + redisplay (~60fps).

## Control flow (save)

```text
Map paint → project.dirty
Save / OK / Apply / Title Save →
  project.pdl + map.txt + switches.pdl + each used event package
Play set_switch → switches.pdl immediately (survives quit)
```

## Deliberate STUBs

Documented in README. Design choice: **working Play + edit loop** over multi-map graph, choices, self-switches, battle.

## Relation to house

| Sibling | Relation |
|---------|----------|
| `&.widgits/event-editor/gl_mock` | Visual DNA (copied layout ideas, no runtime link) |
| `101.mutaclsym` | `map.txt` / PDL vocabulary kinship |
| CHTPM event-editor widget | Parallel product path (file-menu ops); this binary is independent |

## Extension points

1. Multi-map: `project_load_map` + re-scan events on `transfer`.
2. Type-in fields: capture keys when a field nav item is focused.
3. Command insert palette: modal list instead of type cycle.
4. Touch + autorun queues in play tick.
5. Optional export into `#.desktop/events/` package shape for muta import.

*End ARCHITECTURE.md*
