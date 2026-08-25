# PROMPT — 201.dwarf-fortress (Dwarf Fortress–class clone)

**Self-contained freeglut + C.** Target: a **playable DF-inspired** fortress sim that *feels* like classic Dwarf Fortress (ASCII roots + modern tiles optional), not full Bay12 feature parity.

Same compromise style as `202.snes-civ` and `203.gb-pokemon`:
- Pure freeglut (agent-fast, good UI)
- House DNA where useful: save under `saves/<fort>/` as text/pdl-ish files; world map as layered grids
- **POSIX `sh` button.sh**: compile | run | kill | help
- ~60fps timer, **no busy glutIdleFunc**, dirty redraw when possible

---

## Design compromise (feature-rich but shippable)

### Must ship (MVP)
1. **Local map** — e.g. 48×48 or 64×64 tiles, multi-z optional (start with **1 z-level** if faster; 3 z-levels stretch).
2. **Terrain**: soil, rock, water, trees, open space; dig/channel designation.
3. **Dwarves** (3–7): idle → path to job → work; simple needs (hungry/thirsty optional stub).
4. **Jobs / designations**:
   - Mine (dig wall → floor + stone pile)
   - Cut tree
   - Stockpile (wood/stone)
   - Build simple wall / workshop (carpenter)
5. **Materials**: wood, stone piles on ground; workshop turns wood → beds/chairs (1–2 craft recipes).
6. **UI** (critical polish):
   - Main view: top-down tiles (colored, not pure raw `#` only — or hybrid ASCII-on-tiles)
   - Side/status panel: fort name, season/year, population, selected unit, designations mode
   - Mode keys: `d` dig, `t` cut tree, `b` build menu, `q` query, Esc cancel
   - Look as intentional as freeglut event-editor chrome (dark panels, readable text)
7. **Pause / unpause** space; speed 1x only is fine.
8. **Save/load** fort to `saves/default/` (map + dwarves + stock counts).
9. **Embarks**: generate a simple site (trees + soil + rock band) on New Fort.

### Stretch (if MVP solid)
- Multiple z-levels + stairs
- Migrants, animals, combat, moods, magma
- Full ASCII classic mode toggle
- Trade caravan stub

### Non-goals
- Full DF raws, legends, world gen centuries, multiplayer
- Perfect pathfinding A* on huge maps (greedy/BFS on small map OK)

---

## Layout

```text
201.dwarf-fortress/
  PROMPT.md
  README.md
  ARCHITECTURE.md     # map to house file-mediated state / future CHTPM
  button.sh           # POSIX: compile | run | kill | help
  src/
    main.c
    map.c / map.h
    unit.c / unit.h
    job.c / job.h
    render.c / render.h
    save.c / save.h
  saves/
```

Single binary e.g. `./dwarf_fortress` or `./df_clone`.

---

## Controls (document in HUD + README)

| Key | Action |
|-----|--------|
| Arrows / WASD | Move camera or cursor |
| Space | Pause/unpause |
| d | Dig designation mode |
| t | Tree-cut designation |
| b | Build menu (wall / workshop) |
| Enter | Confirm designation / select |
| Esc | Cancel mode |
| Ctrl+S | Save |
| Ctrl+L | Load |
| Q | Quit (or Shift+Q) |

---

## Visual bar
- Not unlit monochrome soup unless player toggles “classic ASCII”
- Default: colored tiles + dwarf glyphs + stockpile markers
- Status panels with `[>]` or clear mode indicator
- 60fps cap; idle should not peg CPU when paused and static

---

## Acceptance
- [ ] `sh button.sh compile && sh button.sh run`
- [ ] Can designate dig, dwarf mines, stockpile appears, craft one item
- [ ] Save/load restores map + dwarves
- [ ] Pause works; no idle spin

## Agent rules
- Stay inside `201.dwarf-fortress/` — **no** `find /home/no/Desktop`
- POSIX sh only (no bash arrays)
- Prefer working MVP over incomplete legendary features

*End PROMPT — dwarf-fortress*
