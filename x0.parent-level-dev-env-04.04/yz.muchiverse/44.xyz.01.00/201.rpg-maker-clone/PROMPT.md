# PROMPT — 201.rpg-maker-clone (self-contained RPG Maker–class editor + player)

**House:** `44.xyz…`  
**Role:** Build a **self-contained** RPG Maker–like **editor + play** app that is  
(1) close in **data shapes** to mutaclysm / event-editor house projects,  
(2) **one project**, freeglut primary UI (same quality as `gl_mock` RMMV Event Editor),  
(3) **fastest path to a finished-feeling product** agents can complete.

**Compromise:**
- Pure freeglut + C (not full CHTPM/prisc) for speed and look.  
- Data: `map.txt` grids, `events/ev_*/state.txt` + `event.pal`, `switches.pdl` — house DNA.  
- Modes in one binary: **Title → Map Edit | Event Edit | Play**.  
- Reuse layout DNA from `&.widgits/event-editor/gl_mock/ee_gl_mock.c` (RMMV event editor chrome).

---

## Goal product

### Must ship (MVP)
1. **Title screen** — New Project / Load / Quit (numbered nav continuous).  
2. **Map Editor**  
   - Orthogonal tile map (e.g. 20×15 or 32×24)  
   - Palette of terrain glyphs/colors (grass, wall, water, floor)  
   - Paint with brush; save `project/maps/<id>/map.txt`  
3. **Event Editor** (RMMV-like layout from the “good” glut mock)  
   - List events on map; add event at cursor  
   - Fields: name, trigger (action/touch), page  
   - Contents: short command list (Show Text, Set Switch, Conditional, Transfer)  
   - Commands | Scratch toggle (visual dual of same list)  
   - Save to `project/maps/<id>/events/ev_x_y/`  
4. **Play mode**  
   - Load map; draw tiles + player  
   - Arrow move; wall collision  
   - Action (Space/Enter) runs event at tile if trigger=action  
   - Show Text → message box; Set Switch → file; Conditional branch simple  
5. **Project folder** self-contained under `projects/demo/`  
6. **`button.sh`**: `compile | run | kill | help` — **POSIX sh**  
7. **Polish:** continuous nav 1..N, readable HUD, RMMV-ish panels (not flat ugly boxes only)

### Stretch
- Multiple maps + transfer between them  
- Self-switches A–D  
- Show Choices  
- Parallel/autorun events  
- Actor party HP  

### Non-goals
- Full RMMV plugin API / Ruby/JS  
- Full CHTPM stack  
- Multiplayer  

---

## Data layout (house-aligned)

```text
201.rpg-maker-clone/
  PROMPT.md
  README.md
  ARCHITECTURE.md
  button.sh
  src/          # C sources
  projects/
    demo/
      project.pdl          # name, start_map
      switches.pdl         # global switches
      maps/
        map_start/
          map.txt          # rows of single-byte terrain codes
          events/
            ev_5_4/
              state.txt    # name, trigger, pos, sprite
              event.pal    # simple script lines
              event.ir.pdl # optional structured nodes
```

**map.txt:** one char per cell, e.g. `.` floor `#` wall `~` water  

**event.pal lines (interpreter mini):**
```text
show_text Hello traveler!
set_switch door_open 1
if_switch door_open 1
  show_text Door is open.
end
transfer map_02 3 3
ret
```

---

## UI modes

| Mode | Look |
|------|------|
| Title | Centered methods, dark chrome |
| Map Edit | Grid + palette + status; continuous nav for tools |
| Event Edit | **Same RMMV layout** as `gl_mock` Event Editor (left props, right contents, footer) |
| Play | Full map view, message box overlay |

F1/F2/F3 or menu to switch modes when not playing; Esc back.

---

## Visual quality bar

Match or exceed `&.widgits/event-editor/gl_mock` polish:
- Dark blue-gray panels, title bars, `[>]` focus  
- Continuous numbering never resets per section  
- Event editor must look “RMMV-ish” not a raw list only  
- Play mode: clear player glyph, readable message box  

---

## Acceptance

- [ ] `sh button.sh compile && sh button.sh run`  
- [ ] Can paint a map, place an event with Show Text, play and see the message  
- [ ] Switches persist in switches.pdl across event steps  
- [ ] Save project survives quit/reload  
- [ ] Idle CPU throttled (~60fps timer, not busy spin)  
- [ ] POSIX button.sh  

---

## Agent rules

- One freeglut binary; few C files.  
- May **copy layout ideas** from house `gl_mock/ee_gl_mock.c` but keep **self-contained** (no runtime dep on muta).  
- Prefer working Play loop over perfect editor.  
- Document STUBs clearly in README if any.  

*End PROMPT.md — rpg-maker-clone*
